// The pad's logic: options, UI navigation, free cursor, pad aim, test bot, auto targeting, flight presets, frame places.
#include "controller.h"

#include "dimouse.h"
#include "game/events.h"
#include "game/aim.h"
#include "game/flightio.h"
#include "game/pursuit.h"
#include "settings.h"
#include "game/fl.h"
#include "game/ai.h"
#include "game/deckmode.h"
#include "game/flight.h"
#include "game/gameclock.h"
#include "game/geom.h"
#include "game/mission.h" // ForgetGame at a game load (host v34)
#include "game/render.h" // OnDeviceLost: the shared renderer's state block before a Reset
#include "game/room.h"
#include "game/nnwin.h"
#include "game/space.h"
#include "game/spawn.h"
#include "log.h"
#include "../shm.h"
#include "device.h"
#include "gamepad.h"
#include "ui/glyphs.h"
#include "haptics.h" // the pad's vibration: events -> effect table -> mixer -> the two motors (IDEAS U30)
// phase D flight modals (the D-FLIGHT track): the goal panel is the Autopilot module's (core\ui\views.h),
#include "picker.h" // the target picker,
#include "wheel.h"  // the action wheel
#include "host.h"
#include "padhud.h"
#include "enabled_gate.h"
#include "game/coreframe.h"
#include "game/gamemode.h"
#include "game/mprules.h"
#include "ui/flufaware.h" // FLUF's windows: keep-outs, the pad on them, the keys' yield (docs\MOD_COMPATIBILITY.md FLUF)
// D-MENUS: the menu screens on the pad, the New Game prompt, speedrun mode (clock, splits, LiveSplit)
// and the HUD timer (padmenus.h / newgame.h; the clock and the timer: modules\speedrun)
#include "game/target_cycle.h"   // the targeting press's enemy rule
#include "game/session.h"
#include "newgame.h"
#include "padmenus.h"
#include "padbases.h" // phase D bases / dealers / NeuroNet windows (the D-BASES track),
#include "padmap.h"   // the nav map,
#include "wgroups.h"  // the weapon group editor,
#include "game/hudinfo.h"
#include "game/modslots.h" // the cut modules' slots
#include "promptbar.h" // the shared prompt bar (D-MENUS)
#include "ui/presets.h"
#include "press_gate.h" // press ownership between the pad's consumers (the attended run's B leak)
#include "ui/options_page.h"
#include "ui/ui.h"
#include "ui/uiopt.h"
#include "ui/hud_rows.h"
#include "ui/views.h"

#include <deque>
#include <map>
#include <sstream>

#include "controller_internal.h"

namespace ctl
{

// ---------------------------------------------------------------- options
Options g_opt;

Gamepad g_pad;
// The menus' view of the pad: g_pad, with A and B exchanged when confirm_button is 1 (the right button accepts). Every
// menu-grammar consumer reads it (the free cursor, FLUF's windows, padbases / padmap, the flight modals' copy); the
// flight bindings read g_pad, positional (UI_BUILD.md 5, 29 Oct 2026). Refreshed right after g_pad.Update.
Gamepad g_menuPad;
static u32 g_lastPadTick = 0;

// The pad's flight bindings (presets.*: the spec's FREELANCER / ACE / ASSISTED tables and the flight_ap situation,
// tap / hold / double-tap) and the state the bindings keep.
presets::Dispatcher g_dispatch;
presets::Table g_table;    // the effective table of the current situation (this frame)
int g_weaponGroup = 1;     // the active weapon group as the pad last set it (1..3; weapon_groups 1 cycles it)
bool g_turret = false;     // turret view toggled on by the pad (USER_AUTO_TURRET, the game's H)
bool g_apUsed = false;     // the player has used the autopilot this session (its prompts may show)



// Freelancer's ship behaviour keeps the last steering direction it was given, and its own mouse-flight
// code only sends a new one while mouse flight is engaged. So before handing steering back, the
// override sends "no turn" for a few frames; otherwise the ship keeps spinning.
// ---------------------------------------------------------------- press ownership (core\press_gate.h)
// A press belongs to the consumer that had the pad when it went down (the B that
// closed FLUF's window / palette was also `pad B free_flight tap`; likewise the A that closed a game popup, and an A held
// in flight that became a mouse button on the window that opened under it). One gate per consumer: the flight dispatch,
// the pad on FLUF's windows, the menu / window cursor. Inputs = the buttons in glyph order plus the sticks (LS / RS
// deflected past their dead zone).
PressGate<glyphs::COUNT> g_gateFlight, g_gateFluf, g_gateUi;



// ---------------------------------------------------------------- frame
void Init()
{
    g_pad.primary = true;
    g_pad.Init();
    LoadConfig();
    padmenus::Init(), session::Init(), modslots::Init(); // (D-MENUS)
}

void Shutdown()
{
    coreframe::Stop();
    CloseModals();
    ReleaseAll();
    haptics::Stop(g_pad); // the motors off (a hot reload mid-effect)
    // render.cpp is compiled into this logic: its D3D textures go with it (every hot reload leaked the previous logic's objects until 24 Oct 2026)
    padhud::Shutdown();
    padbases::Shutdown(); // (the bar's hovered character released, the prompt bar's state)
    modslots::Shutdown(), session::Shutdown(), padmenus::Shutdown();
}

// ---------------------------------------------------------------- the pad's places in the frame (core\game\coreframe.cpp, views::PadFrameView)
static void PadRead(float dt)
{
    flightio::Get() = {g_opt.steerScale, g_opt.aimCone, g_opt.invertY, g_opt.autopilotCancel, g_opt.maneuverWatchdog};
    // focus_only: the physical pad acts only while the game is in front (the user may be using it in another program) - the one gate the menus' reader
    // asks too (focus_gate.h); everything else (modes, the bot, the autopilot, the virtual pad) goes on out of focus.
    g_pad.ignoreReal = Gamepad::IgnoreRealNow(g_opt.focusOnly);
    g_pad.Update(dt);
    g_menuPad = uiopt::Get().confirmButton == 1 ? g_pad.SwappedAB() : g_pad;
    // each gate in its consumer's view (the menu gates see A / B as accept / back)
    bool in[glyphs::COUNT], menu[glyphs::COUNT];
    PadInputs(in);
    PadInputs(menu, g_menuPad);
    g_gateFlight.Observe(in), g_gateFluf.Observe(menu), g_gateUi.Observe(menu);
}

// the pad's vibration (haptics.h)
static void PadFeel()
{
    haptics::Frame hf;
    hf.where = g_mode == Mode::Space || g_mode == Mode::SpaceUi ? haptics::Where::Flight
               : g_mode == Mode::Cutscene                       ? haptics::Where::Cutscene
               : g_mode == Mode::Docked                         ? haptics::Where::Docked
                                                                : haptics::Where::Menu;
    hf.enabled = coreframe::opts.enabled;
    hf.paused = fl::GamePaused();
    hf.autopilot = views::ApActive();
    hf.window = g_mode == Mode::SpaceUi;
    // the triggers' roles and the fire and launcher buttons, from the live table: a trigger follows its bind, not its side
    const auto roleOf = [](presets::Action a) {
        return a == presets::Action::Fire || a == presets::Action::FireGroup1 || a == presets::Action::FireGroup2 ? haptics::Role::Fire
               : a == presets::Action::Afterburner                                                                ? haptics::Role::Afterburner
               : a == presets::Action::Throttle                                                                   ? haptics::Role::Throttle
                                                                                                                  : haptics::Role::None;
    };
    hf.role[0] = roleOf(g_table.b[glyphs::LT].tap), hf.role[1] = roleOf(g_table.b[glyphs::RT].tap);
    hf.held[0] = g_pad.Down(PAD_LT), hf.held[1] = g_pad.Down(PAD_RT);
    bool down[glyphs::COUNT];
    PadInputs(down);
    static bool fireWas = false, launcherWas = false;
    bool fire = false, launcher = false;
    for (int i = 0; i < glyphs::COUNT; ++i)
    {
        const presets::Action tap = g_table.b[i].tap, hold = g_table.b[i].hold;
        fire = fire || (down[i] && roleOf(tap) == haptics::Role::Fire);
        launcher = launcher || (down[i] && (tap == presets::Action::Missile || tap == presets::Action::Torpedo || tap == presets::Action::CruiseDisruptor ||
                                            hold == presets::Action::Torpedo || hold == presets::Action::CruiseDisruptor));
    }
    hf.fireHeld = fire, hf.firePressed = fire && !fireWas, hf.launcherPressed = launcher && !launcherWas;
    fireWas = fire, launcherWas = launcher;
    haptics::Update(hf, g_pad);
}

// The controller UI's settings as its modules read them (uiopt.h): the registry's values, copied each frame.
static void PadOptions()
{
    // Whoever was used last (pad, or the physical mouse / keyboard) owns the cursor, the steering and the UI: the input-device service (device.cpp).
    if (g_pad.AnyInput())
        g_lastPadTick = GetTickCount();
    uiopt::UiOptions& u = uiopt::Get();
    u.inputMode = g_opt.inputMode, u.glyphStyle = g_opt.glyphStyle, u.confirmButton = g_opt.confirmButton;
    u.pauseOnDisconnect = g_opt.pauseOnDisconnect, // ASSISTED has no steering of its own: without the Autopilot module the pad is FREELANCER, for every reader
    u.padPreset = g_opt.padPreset == presets::ASSISTED && !views::ApPresent() ? presets::FREELANCER : g_opt.padPreset;
    u.weaponGroups = g_opt.weaponGroups, u.swapY = g_opt.swapY, u.holdsAsToggles = g_opt.holdsAsToggles;
    u.rumble = g_opt.rumble, u.padCursor = g_opt.padCursor;
    u.triggerEffects = g_opt.triggerEffects, u.triggerStrength = g_opt.triggerStrength, u.hapticsMode = g_opt.hapticsMode, u.hapticsDevice = g_opt.hapticsDevice;
    memcpy(u.hapticsClass, g_opt.hapticsClass, sizeof(u.hapticsClass));
    const haptics::Caps caps = haptics::CapsFor(g_pad.Facts());
    u.padTriggers = caps.adaptive || caps.triggerMotors;
    u.cursorMagnet = g_opt.cursorMagnet, u.mapTouchpad = g_opt.mapTouchpad, u.promptBar = g_opt.promptBar, u.worldPrompts = g_opt.worldPrompts;
    u.apHints = g_opt.apHints, u.hudGlyphs = g_opt.hudGlyphs;
    u.buttonLegend = g_opt.buttonLegend, u.apStatusLine = g_opt.apStatusLine;
}

static void PadDevice(float dt, bool space) { device::Update(dt, g_pad, space); }

static void PadWatch(u32 now)
{
    // Turret view as the game has it: its current camera is the turret camera (exe::CamTurret). A window, a save load, a death or the player's own H key
    // change it without FLvibed. Without the symbol FLvibed's own flag stands.
    if (exe::CamTurret.ok())
    {
        const bool on = exe::CamTurret.is(fl::CurrentCamera());
        if (on != g_turret)
        {
            g_turret = on;
            events::Push("turret view %s (the game's camera)", on ? "on" : "off");
        }
    }
    glyphs::Update(g_pad.HasRealController(), g_pad.Vendor());
    if (device::TakeDisconnectPause() && !fl::GamePaused())
        Tap(fl::USER_PAUSE); // pause_on_disconnect: the game's own pause menu

    // In windowed modes Freelancer hands the mouse back to Windows when the cursor leaves the window; using the pad takes it back (like clicking into
    // the window would). Only while Freelancer is the foreground window (capturing fails otherwise), and rate limited so a failing capture cannot run
    // every frame.
    static u32 lastCaptureTry = 0;
    const bool focused = GetForegroundWindow() == fl::MainWindow(); // the main window itself (not an owned popup)
    if (g_pad.AnyInput() && !dimouse::IsCaptured() && focused && now - lastCaptureTry > 500)
    {
        lastCaptureTry = now;
        dimouse::Capture();
    }
}

static void PadDied() { StopBot("player-death"); }

static void PadModeChange(Mode from, Mode mode)
{
    g_dispatch.Reset(); // the pad's holds and latches end with the mode
    wheel::DropRearView();
    // Turret view is the game's toggle: a window over the flight (space <-> space-ui) leaves it on, so FLvibed's flag stays with it; leaving space ends it
    if (mode != Mode::Space && mode != Mode::SpaceUi)
        g_turret = false;
    // ...and the turret camera's look input: a key-up sent while a window had the cursor can be lost (the game drops flight commands there), which left
    // the camera turning for good. Back in flight, every look command's key-up again (the handler 0x51AC20 zeroes its input on a key-up).
    if (mode == Mode::Space)
        for (int c : {fl::USER_LOOK_ROTATE_CAMERA_LEFT, fl::USER_LOOK_ROTATE_CAMERA_RIGHT, fl::USER_LOOK_ROTATE_CAMERA_UP,
                      fl::USER_LOOK_ROTATE_CAMERA_DOWN})
            fl::FireCommand(c, false);
    CloseModals(); // the phase D modals end with the flight mode
    // Windows are reused (e.g. one tutorial popup object for every message): whatever is open when we enter a cursor mode counts as newly opened, so it
    // gets auto-focus - except the flight HUD, which was already there.
    if (from == Mode::Space && mode == Mode::SpaceUi)
        g_prevRoots = g_hudRoots;
    else
        g_prevRoots.clear();
}


// What HUD 2 asks of the pad this frame.
static void PadFillHud(views::HudContext& hc)
{
    const bool padOwns = hc.pad;
    hc.bot = g_bot.on;
    hc.table = g_table;
    hc.turret = g_turret;
    hc.weaponGroup = g_weaponGroup;
    hc.apKnown = views::ApPresent() && (g_apUsed || uiopt::Get().apHints || session::Current() == session::ASSISTED);
    hc.modal = picker::IsOpen() || wheel::IsOpen() || views::PanelOpen() || wgroups::IsOpen();
    hc.steer = sqrtf(g_lastSteerX * g_lastSteerX + g_lastSteerY * g_lastSteerY); // (CONTEXTUAL's triggers)
    hc.firing = padOwns && (g_dispatch.Held(presets::Action::Fire) || g_dispatch.Held(presets::Action::Missile));
    // any fire input's PRESS wakes the whole HUD under hud_fade 1 (the press itself, even with the weapons locked / nothing able to fire;
    // core\game\hudwake.h FireInput) - the pad's here, the keyboard and mouse's in hud2.cpp (the game's keymap)
    using A = presets::Action;
    static bool fireWas = false;
    const bool fireNow = padOwns && (g_dispatch.Held(A::Fire) || g_dispatch.Held(A::Missile) || g_dispatch.Held(A::Torpedo) || g_dispatch.Held(A::Cm) ||
                                     g_dispatch.Held(A::Mine) || g_dispatch.Held(A::FireGroup1) || g_dispatch.Held(A::FireGroup2) ||
                                     g_dispatch.Held(A::CruiseDisruptor));
    if (fireNow && !fireWas)
        views::HudNoteAction(hudwake::FireInput);
    fireWas = fireNow;
}

static bool PadMenuButtonDown()
{
    return g_pad.Down(XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_B | XINPUT_GAMEPAD_X | XINPUT_GAMEPAD_Y | XINPUT_GAMEPAD_LEFT_SHOULDER |
                      XINPUT_GAMEPAD_RIGHT_SHOULDER | XINPUT_GAMEPAD_START | XINPUT_GAMEPAD_BACK);
}

static void PadPrompts(float dt, bool padOwns)
{
    // Start opens / closes the game menu (Freelancer's pause menu, F1 by default); in flight the preset's MENU binding does it (a tap). Not while a menu
    // screen owns the pad (the OSK's Menu = done, the New Game prompt).
    if (padOwns && g_mode != Mode::Space && g_pad.Pressed(XINPUT_GAMEPAD_START) && !padmenus::OwnsInput())
        Tap(fl::USER_PAUSE);
    // The pad HUD layer's view of the frame (padhud.cpp draws it in the overlay pass)
    padhud::Context pc;
    pc.active = padOwns && (g_mode == Mode::Space || g_mode == Mode::SpaceUi) && !g_bot.on;
    pc.flying = g_mode == Mode::Space;
    pc.table = g_table;
    pc.preset = uiopt::Get().padPreset == presets::CUSTOM ? presets::FREELANCER : uiopt::Get().padPreset;
    pc.autopilot = views::ApActive();
    pc.turret = g_turret;
    pc.weaponGroup = g_weaponGroup;
    pc.apKnown = views::ApPresent() && (g_apUsed || uiopt::Get().apHints || session::Current() == session::ASSISTED);
    padhud::Update(dt, pc);
    // a modal's own prompt bar: the legend steps aside (FLUF's windows on the pad too)
    padhud::SetModal(picker::IsOpen() || wheel::IsOpen() || views::PanelOpen() || wgroups::IsOpen() || flufaware::OwnsPad());
}

static bool PadBotOn() { return g_bot.on; }

// The physical mouse / keyboard took the device in flight: the pad's modals and holds end with the pad (the goal panel has its keyboard + mouse form).
static void PadDeviceRow()
{
    g_dispatch.Reset();
    picker::Close(true);
    wheel::Cancel();
    wgroups::Close();
    if (g_turret && device::Switched()) // the pad's turret view ends with the pad (the player's H key is theirs: g_turret follows the game's camera, so only
    {                                   // at the switch)
        g_turret = false;
        Tap(fl::USER_AUTO_TURRET);
    }
}

// The rest of the pad's frame: FLUF's windows, the menus, the mode's own screen.
static void PadRest(float dt)
{
    // FLUF (flufaware.h, fluf_windows 1 / 2): one of its input windows is open - the pad drives it this frame (the palette by its keys, the others by the
    // cursor); FLvibed's own dispatch steps aside until it closes
    if (flufaware::OwnsPad())
    {
        if (!flightio::UnderWindow())
            ReleaseSteer();
        UpdateFlufPad(dt);
        padmenus::Idle(dt);
        return;
    }
    EndFlufPad();
    // the game's menu screens on the pad (main menu + New Game prompt, load, save + OSK, pause, death, the chat's field in space); while one of them owns
    // the pad this frame the dispatch below stays out
    padmenus::Update(dt, ModeName(g_mode));
    if (padmenus::OwnsInput())
        return;
    switch (g_mode)
    {
    case Mode::Menu:
    case Mode::Docked:
    case Mode::SpaceUi:
        if (!flightio::UnderWindow()) // (the autopilot flying on under the window steers)
            ReleaseSteer();
        // the pad screens of bases, dealers, NeuroNet windows and the nav map: they take their buttons first (padbases::Owns gates the generic focus /
        // cursor code below)
        padbases::Update(dt, g_menuPad,
                         g_mode == Mode::Docked    ? padbases::Where::Docked
                         : g_mode == Mode::SpaceUi ? padbases::Where::SpaceUi
                                                   : padbases::Where::Menu);
        UpdateUiNav(dt);
        UpdateFreeCursor(dt);
        if (g_mode == Mode::SpaceUi && g_pad.Pressed(XINPUT_GAMEPAD_BACK) && !padbases::Owns(XINPUT_GAMEPAD_BACK))
            fl::TapKey(VK_ESCAPE); // close the window
        break;
    case Mode::Space:
        // between two NeuroNet windows (a bumper's tab waits to open, or a window still slides): no ship control - the owner, 7 Nov 2026: RB fired
        // missiles as the next tab loaded. The bumpers go on stepping the tabs
        if (nnwin::Held() || room::NNWindowBusy())
        {
            ReleaseSteer();
            if (g_pad.Pressed(XINPUT_GAMEPAD_LEFT_SHOULDER))
                nnwin::Step(-1);
            if (g_pad.Pressed(XINPUT_GAMEPAD_RIGHT_SHOULDER))
                nnwin::Step(+1);
            break;
        }
        UpdateSpace(dt);
        break;
    case Mode::Cutscene:
        ReleaseSteer();
        if (g_pad.Pressed(XINPUT_GAMEPAD_A) || g_pad.Pressed(XINPUT_GAMEPAD_B))
            fl::TapKey(VK_ESCAPE);
        break;
    }
}

static void PadDrawFlight(void* device, bool menuOver)
{
    if (!menuOver)
        padhud::Render(device); // the pad HUD layer (badges, legend, status line, world prompts) over the HUD
    picker::Render(device);
    wheel::Render(device);
}

static void* g_basesDevice = nullptr;
static void PadDrawBases(void* device)
{
    padbases::Draw(device);
    g_basesDevice = device;
}

// (the map's list stands over HUD 2's map and panels, which draw after the bases' pass, and under the prompts)
static void PadDrawPrompts()
{
    if (g_basesDevice)
        padmap::Draw(g_basesDevice);
    g_basesDevice = nullptr;
    promptbar::Draw();
}

static void PadFault(int phase)
{
    if (phase == 0)
        haptics::Stop(g_pad); // XInput's motors stay at their last value: the timer that ends them was the logic's
}

void PublishFrame()
{
    views::padframe = {PadRead, PadFeel, PadOptions, PadDevice, PadWatch, device::Pad, PadDied, PadModeChange, PadFillHud, PadMenuButtonDown, PadPrompts,
                       PadBotOn, UpdateSpace, PadDeviceRow, PadRest, Verb, PadDrawFlight, wgroups::Render, PadDrawBases, padmenus::Draw,
                       PadDrawPrompts, PadFault, padbases::NoHoverHelp};
}
void WithdrawFrame() { views::padframe = {}; }

// Started by deploy.ps1 -Background (FLVIBED_BACKGROUND=1 in the game's environment): runs out of focus, muted.
static bool BackgroundTestStart()
{
    char v[8] = {};
    return GetEnvironmentVariableA("FLVIBED_BACKGROUND", v, sizeof(v)) && v[0] == '1';
}

// Debug `owners`: who holds each command down.
std::string DescribeOwners() { return "owners:" + flightio::HeldOwners(); }

std::string DescribeState()
{
    std::ostringstream o;
    const PadState& p = g_pad.Cur();
    o << "mode=" << ModeName(g_mode) << " frame=" << g_frame << "\n";
    o << "screen=" << fl::ScreenWidth() << "x" << fl::ScreenHeight() << " mouse=(" << fl::MouseX() << ","
      << fl::MouseY() << ") buttons=0x" << std::hex << fl::MouseButtons() << std::dec << " cursor='" << fl::CursorName()
      << "'\n";
    o << "base=0x" << std::hex << fl::PlayerBase() << " system=0x" << fl::PlayerSystem() << " iobj=0x"
      << reinterpret_cast<uintptr_t>(fl::PlayerIObj()) << " camera=0x" << fl::CurrentCamera() << std::dec
      << " paused=" << fl::GamePaused() << " throttle=" << fl::GetThrottle() << "\n";
    o << "pad connected=" << p.connected << " real=" << g_pad.HasRealController() << " buttons=0x" << std::hex
      << p.buttons << std::dec << " L=(" << p.lx << "," << p.ly << ") R=(" << p.rx << "," << p.ry << ") LT=" << p.lt
      << " RT=" << p.rt << " slot=" << g_pad.ActiveIndex() << " dev=" << Gamepad::DevActive() << " vendor=0x"
      << std::hex << g_pad.Vendor() << std::dec << " ignore_real=" << g_pad.ignoreReal << "\n";
    if (const std::string b = g_pad.BackendStatus(); !b.empty()) // (pad_backend 0 with SDL never started: no line)
        o << b << "\n";
    o << "dimouse: " << dimouse::Stats() << "\n";
    o << "foreground=" << (GetForegroundWindow() == fl::MainWindow())
      << " background_run=" << ((coreframe::opts.runInBackground || BackgroundTestStart()) && HostAtLeast(7))
      << " lastPad=" << g_lastPadTick << " lastMouse=" << dimouse::LastRealTick() << "\n";
    o << device::Status() << "\n";
    o << "preset=" << presets::PresetName(uiopt::Get().padPreset) << " group=" << g_weaponGroup
      << " turret=" << g_turret << " " << g_dispatch.Status() << "\n";
    o << "held:" << flightio::HeldNames() << "\n";
    return o.str();
}

} // namespace ctl
