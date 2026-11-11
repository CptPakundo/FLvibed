// The logic's frame and draw passes (coreframe.h). The order is the one docs\module_map.tsv's frame rows record: nothing here may move without the
// fingerprint's frame order saying so. A module's place is a view (core\ui\views.h); with the pad off, the frame is docs\MODULES_PLAN.md
// "P7's priority table" with no pad rows: the page's gate, then the flight or the device's row.
#include "coreframe.h"

#include "ai.h"
#include "aim.h"
#include "debugverbs.h"
#include "deckmode.h"
#include "handheld.h"
#include "hudsize.h"
#include "dimouse.h"
#include "enabled_gate.h"
#include "events.h"
#include "fl.h"
#include "flexe.h"
#include "flight.h"
#include "flightio.h"
#include "gameclock.h"
#include "hostcalls.h"
#include "hudwake.h"
#include "input_rules.h"
#include "log.h"
#include "maneuver_watchdog.h"
#include "mission.h"
#include "modslots.h"
#include "mprules.h"
#include "render.h"
#include "session.h"
#include "room.h"
#include "space.h"
#include "spawn.h"
#include "ui/flufaware.h"
#include "ui/hud_rows.h"
#include "ui/options_page.h"
#include "ui/ui.h"
#include "ui/uiopt.h"
#include "ui/views.h"
#include "ui/winplates.h"
#include "ui_buttons.h"

namespace coreframe
{
using gamemode::Mode;

static const char* ModeName(Mode m) { return gamemode::Name(m); }

// The maneuver lock's answer (a mission's Act_LockManeuvers made B / free flight do nothing, which looked like a malfunction): while the game's
// maneuvers are locked (IBehaviorManager +0xBC: space::Pilot::locked) a requested maneuver is logged and HUD 2's alert card says LOCKED BY THE
// MISSION. The request still goes to the game, which refuses it itself. The log line at most once a second per action (a held key repeats).
bool LockAnswer(const char* what)
{
    space::Pilot p;
    if (!space::ReadPilot(p) || !p.locked)
        return false;
    static const char* lastWhat = nullptr;
    static DWORD lastAt = 0;
    const DWORD now = GetTickCount();
    if (what != lastWhat || now - lastAt > 1000)
        events::Push("maneuvers locked: %s refused%s", what, views::HudLockNotice() ? "" : " (HUD 2 not shown)");
    else
        views::HudLockNotice();
    lastWhat = what, lastAt = now;
    return true;
}

void OnKeyCmd(int cmd, bool down)
{
    if (flightio::InTap() || !down || g_mode != Mode::Space)
        return;
    const char* what = cmd == fl::USER_MANEUVER_FREE_FLIGHT ? "free flight"
                       : cmd == fl::USER_MANEUVER_FORMATION ? "formation"
                       : cmd == fl::USER_MANEUVER_DOCK      ? "dock"
                       : cmd == fl::USER_MANEUVER_GOTO      ? "goto"
                                                            : nullptr;
    if (what)
        LockAnswer(what);
    // (hud_prompts: the lanes show after the player acts; hud_fade CONTEXTUAL: what the action touches wakes)
    if (what)
        views::HudNoteAction(hudwake::Maneuver);
    else if (cmd == fl::USER_CRUISE || cmd == fl::USER_MANEUVER_ENGINEKILL)
        views::HudNoteAction(hudwake::Cruise);
    else if (cmd == fl::USER_CLEAR_TARGET || cmd == fl::USER_NEXT_ENEMY || cmd == fl::USER_CLOSEST_ENEMY)
        views::HudNoteAction(hudwake::TargetChg);
}

// The player ship's state once a second, as an event when it changed (hull to 0.1 %, shield to 1 %, consumables): the run's own track, comparable
// with the footage's HUD scans, per section of a mission.
int ApplySize()
{
    static handheld::Facts facts;
    static handheld::Verdict verdict;
    static bool read = false;
    const float W = static_cast<float>(fl::ScreenWidth()), H = static_cast<float>(fl::ScreenHeight());
    if (!read)
        facts = handheld::Read(), read = true, facts.W = -1;
    if (W != facts.W || H != facts.H)
    {
        facts.W = W, facts.H = H;
        verdict = handheld::Decide(facts);
        Log("hud_size: automatic reads %s at %.0fx%.0f (%s; model '%s', board '%s')", verdict.preset == hudsize::Large ? "large" : "normal",
            W, H, verdict.why, facts.systemProduct.c_str(), facts.boardProduct.c_str());
    }
    return hudsize::Apply(hudsize::Resolve(opts.hudSize, verdict.preset), H, opts.textSize);
}

static void TrackSelf()
{
    static DWORD next = 0;
    static char last[96] = {};
    const DWORD now = GetTickCount();
    if (now < next)
        return;
    next = now + 1000;
    ai::Self s;
    if (!ai::ReadSelf(s) || s.hull <= 0) // (docking / docked reads hull 0; a death has its own event)
        return;
    char line[96];
    sprintf_s(line, "self hull=%.3f shield=%.2f nano=%d batt=%d cm=%d", s.hull, s.shield, s.nanobots, s.batteries,
              space::PlayerCountermeasures());
    if (strcmp(line, last) == 0)
        return;
    strcpy_s(last, line);
    events::Push("%s", line);
}

// Only flight HUD windows open (the arrow cursor then comes from the cursor resting on the HUD or an object, not from a NeuroNet window taking the
// mouse). Unknown HUD = false.
static bool OnlyHudOpen()
{
    if (g_hudRoots.empty())
        return false;
    const std::vector<uintptr_t> roots = ui::VisibleRoots();
    return std::all_of(roots.begin(), roots.end(), [](uintptr_t r)
                       { return std::find(g_hudRoots.begin(), g_hudRoots.end(), r) != g_hudRoots.end(); });
}

// The flight view for the mode: only the flight HUD's windows and boxes over the flight are open. The cargo demand is one: the game switches to the
// arrow for it, but neither pauses nor hides its own HUD, and the box closes by itself 20 s on while the player may be under fire - so it is flight,
// not a NeuroNet window (the flight HUD, the pad's flight). OnlyHudOpen stays the cursor's test: the box keeps the cursor shown.
static bool FlightViewOnly()
{
    if (g_hudRoots.empty())
        return false;
    const std::vector<uintptr_t> roots = ui::VisibleRoots();
    return std::all_of(roots.begin(), roots.end(), [](uintptr_t r)
                       { return std::find(g_hudRoots.begin(), g_hudRoots.end(), r) != g_hudRoots.end() || winplates::CargoDemand(static_cast<u32>(r)); });
}

static bool CargoDemandUp()
{
    const std::vector<uintptr_t> roots = ui::VisibleRoots();
    return std::any_of(roots.begin(), roots.end(), [](uintptr_t r) { return winplates::CargoDemand(static_cast<u32>(r)); });
}

// HUD hit-test suppression (HostApi::SetHudHitSuppression).
static bool g_hudSuppressed = false;
static bool g_hudRootsChanged = false;

static void SetHudSuppression(bool on)
{
    if (!hostcalls::g.SetHudHitSuppression)
        return;
    if (on == g_hudSuppressed && !(on && g_hudRootsChanged))
        return;
    g_hudSuppressed = on;
    g_hudRootsChanged = false;
    // FLvibed's own click box (HUD 2's hotspots, the goal panel's rows: a top-level root of the host's) stays clickable while the autopilot flies: the
    // keyboard and mouse player's clicks on it are meant, the autopilot being identical on both devices
    std::vector<uintptr_t> roots;
    for (uintptr_t r : g_hudRoots)
        if (_stricmp(ui::ControlName(r), "FLvibedHud2Hotspots") != 0)
            roots.push_back(r);
    hostcalls::g.SetHudHitSuppression(on, roots.data(), static_cast<int>(roots.size()));
    aim::SetHudTransparent(on && !g_hudRoots.empty());
}

void Stop() { SetHudSuppression(false); }

// The frame of the last under-HUD pass: the shield cue draws there (screen-edge effects under the HUD, as Destiny and Apex Legends layer theirs);
// without that pass (hook refused) it falls back to the overlay.
static u32 g_underHudFrame = 0;
static bool g_menuOver = false; // a modal menu over the flight under ui_style 1 (this frame's under-HUD pass)
static bool g_padOwns = false;  // the pad is the device, as this frame's OnFrame found it

static void* Device() { return hostcalls::g.GetD3DDevice ? hostcalls::g.GetD3DDevice() : nullptr; }
static bool InFlight() { return g_mode == Mode::Space || g_mode == Mode::SpaceUi; }

void OnRenderUnderHud()
{
    perf::Scope perfPass("OnRenderUnderHud");
    space::EndFrame(); // (normally done: a fault inside OnFrame skips its destructors) - the passes scan fresh
    g_underHudFrame = g_frame;
    // With the pad's cursor hidden the game's hover sits on one control and FLvibed's hit test on another, and each rewrites the one ToolTip: on a still job
    // board "Kill Ships" and "Accept Job." swapped on screen, read frame by frame. Before the UI draws, so neither reaches the screen.
    if (views::padframe.NoHoverHelp && views::padframe.NoHoverHelp())
        ui::HideHoverHelp();
    void* device = Device();
    // Extended HUD 2's modal menus in space (ui_style 1: pause, save / load, Options, confirm): the whole flight HUD - the game's, HUD 2, the pad HUD
    // layer, the cues - goes under the scrim, back the frame the menu closes. False under ui_style 0 and without the HUD
    g_menuOver = views::HudUnderBegin(device, opts.enabled && InFlight());
    // the hosted FLVIBED options page's opaque base under the Options window (before its glass / art)
    options_page::RenderUnder(device);
    // Extended HUD 2: the windows' art hidden / restored and their glass under the UI pass (any mode: restores); without it the prompt lane's geometry
    if (views::hud.UnderEnd)
        views::hud.UnderEnd(device, opts.enabled && InFlight() && !g_menuOver);
    else
        winplates::ClassicPass(device, opts.enabled, g_padOwns, true);
}

static void FaultNow(int where);

void OnRenderOverlay()
{
    FaultNow(2);
    perf::Scope perfPass("OnRenderOverlay");
    space::EndFrame(); // as OnRenderUnderHud
    const views::PadFrameView& pad = views::padframe;
    if (!pad.Read)
        render::SetDevice(Device());
    if (opts.enabled && InFlight())
    {
        void* device = Device();
        // (under an Extended HUD 2 modal menu: the cues, HUD 2 (its own gate) and the pad HUD layer stay under the scrim)
        const bool menuOver = g_menuOver && g_frame - g_underHudFrame <= 2;
        if (views::hud.Overlay)
            views::hud.Overlay(device, menuOver, g_frame - g_underHudFrame > 2);
        if (pad.DrawFlight)
            pad.DrawFlight(device, menuOver); // the pad HUD layer over the HUD, then the flight modals (one open at a time by their buttons)
        views::PanelRender(device);
        if (pad.DrawGroups)
            pad.DrawGroups(device);
    }
    if (opts.enabled && (g_mode == Mode::Docked || g_mode == Mode::SpaceUi) && pad.DrawBases) // the bases' strip and the dealers' line, the nav map's list
        pad.DrawBases(Device());
    if (pad.DrawMenus)
        pad.DrawMenus(); // the New Game prompt, the load screens' tags, the OSK over the game's menu screens
    modslots::Draw(modslots::kSpeedrun); // speedrun mode's HUD timer (pad and keyboard alike)
    if (views::hud.Strip)
        views::hud.Strip(Device()); // Extended HUD 2's docked overlay
    if (pad.DrawPrompts)
        pad.DrawPrompts(); // the shared prompt bar (every pad screen sets it per frame)
    options_page::Draw(); // the FLVIBED OPTIONS page over everything while it is open
    views::BarDraw();     // the bar's autopilot button glyph
    if (views::hud.Top)
        views::hud.Top(Device()); // HUD 2's cursor and tooltips, over all of it
}

// Before every device Reset (the host's Reset hook): every state block this logic holds goes - HUD 2's batcher's and the shared renderer's. Under
// d3d8to9 (HDE) a D3D9 state block left alive makes Reset fail D3DERR_INVALIDCALL. Textures are MANAGED (they survive), no DEFAULT pool objects.
void OnDeviceLost()
{
    if (views::hud.DeviceLost)
        views::hud.DeviceLost();
    render::OnDeviceLost();
}

// A frame-rate cap for test games: debug command `fpscap <fps>` (0 = off, the default; never a player setting). Waits at the start of the frame until
// 1 / fps has passed since the last one - a high-resolution waitable timer where Windows has one (Windows 10 1803+), else Sleep(0) spins on the
// performance counter.
static int g_fpsCap = 0;
int FpsCap(int fps)
{
    if (fps >= 0)
        g_fpsCap = fps > 0 ? std::max(10, std::min(fps, 1000)) : 0;
    return g_fpsCap;
}
static void CapFrame()
{
    static LARGE_INTEGER freq = {}, last = {};
    if (!freq.QuadPart)
        QueryPerformanceFrequency(&freq);
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    if (g_fpsCap > 0 && last.QuadPart)
    {
        const LONGLONG period = freq.QuadPart / g_fpsCap;
        const LONGLONG due = last.QuadPart + period;
        static HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, 0x00000002 /* HIGH_RESOLUTION */, TIMER_ALL_ACCESS);
        const LONGLONG left = due - now.QuadPart;
        if (left > 0 && timer)
        {
            // (the timer in 100 ns units, negative = relative; 0.5 ms short, the rest spun)
            LARGE_INTEGER rel;
            rel.QuadPart = -std::max<LONGLONG>(0, left * 10000000 / freq.QuadPart - 5000);
            if (rel.QuadPart < 0 && SetWaitableTimer(timer, &rel, 0, nullptr, nullptr, FALSE))
                WaitForSingleObject(timer, 100);
        }
        for (QueryPerformanceCounter(&now); now.QuadPart < due; QueryPerformanceCounter(&now))
            Sleep(0);
        // (a frame that ran long resets the schedule: no catching up with short frames)
        last.QuadPart = now.QuadPart - due > period ? now.QuadPart : due;
        return;
    }
    last = now;
}

static int g_testFault = 0;
void TestFault(int where) { g_testFault = where; }
std::string FaultCommand(std::istream& in)
{
    std::string what, where;
    in >> what >> where;
    std::string reply;
    if (modslots::FaultVerb(what, where, reply))
        return reply;
    TestFault(what == "draw" ? 2 : 1);
    return "fault armed";
}
static void FaultNow(int where)
{
    if (g_testFault != where)
        return;
    g_testFault = 0;
    *static_cast<volatile int*>(nullptr) = 0;
}

static void ReleaseHeld(int) { flightio::ReleaseAll(); }

void OnFault()
{
    modslots::GiveBack(); // a module stopped in the frame the entry faulted: no next frame would do it
    for (int phase = 0; phase < 2; ++phase)
    {
        if (const modslots::Module* hud = modslots::g[modslots::kHud]; hud && hud->Fault)
            modslots::Try(hud->Fault, phase);
        if (views::padframe.Fault)
            modslots::Try(views::padframe.Fault, phase);
        for (int s = 0; s < modslots::kSlots; ++s)
            if (s != modslots::kHud && modslots::g[s] && modslots::g[s]->Fault)
                modslots::Try(modslots::g[s]->Fault, phase);
    }
    modslots::Try(ReleaseHeld, 0);
}

void OnFrame(double dtd)
{
    FaultNow(1);
    modslots::GiveBack(); // a module stopped by a fault: what it changed goes back, outside the scene, no module on the stack
    // first, before this function's own clamp: every reader this frame sees this frame's time. The game passes its dt on through a pause (read in game: 1.5 s in
    // 1.5 s), and game time stands still there
    if (!fl::GamePaused())
        gameclock::Advance(dtd);
    CapFrame();
    perf::NewFrame();
    perf::Scope perfFrame("OnFrame");
    const views::PadFrameView& pad = views::padframe;
    views::BarUpdate(); // the bar's autopilot button state to the host
    // The frame's memo of the space scans (space.h BeginFrame): on for this function's run only - the game does not simulate inside it; render passes
    // and debug commands outside it scan fresh.
    space::BeginFrame();
    struct EndMemo
    {
        ~EndMemo() { space::EndFrame(); }
    } endMemo;
    ++g_frame;
    // A new game (a save load, New Game, the load after a death - each loads a fresh Content.dll): mission.cpp's memory of the last script / objective
    // and the shown objectives was the game before's. Before anything this frame asks it. A logic (re)start forgets nothing: its memory starts empty.
    {
        static bool seen = false;
        static u32 lastLoads = 0;
        const u32 loads = hostcalls::g.GameLoads ? hostcalls::g.GameLoads() : 0;
        if (seen && loads != lastLoads)
        {
            mission::ForgetGame();
            views::ApForgetGame(); // (the goal queue's story memory is the game's too)
            events::Push("game-load %u: mission memory forgotten", loads);
        }
        seen = true;
        lastLoads = loads;
    }
    const float dt = Clampf(static_cast<float>(dtd), 0.0f, 0.25f);
    flightio::SetFrameDt(dt);
    if (pad.Read)
        pad.Read(dt); // the flight options, the pad's read, the press gates
    flightio::TickSteerRelease();
    mprules::Update(); // multiplayer: the server's word on the autopilot (new chat lines only)
    ai::Update();
    spawn::CheckClearAt(ai::PlayerHealth()); // `spawn clearat`: test spawns go at a hull threshold (no harness lag)
    space::InvalidateFrame();                // that may have despawned ships (spawn.cpp knows nothing of the memo)
    TrackSelf();
    if (InFlight())
        mwatch::Frame(); // a stalled formation / goto / dock is issued again (setting maneuver_watchdog)
    modslots::Frame(modslots::kNpc, static_cast<float>(dtd), ModeName(g_mode), fl::GamePaused());
    flight::RecordFrame();
    debug::Frame();
    options_page::Update(dt); // the options page's input and hosting state, before the pad flight / menu code
    flufaware::Update(dt, opts.flufWindows); // FLUF's windows from the HUD bus (no FLUF: nothing), before every reader
    if (views::hud.Cues)
        views::hud.Cues(dt, InFlight(), views::ApActive());
    if (pad.Feel)
        pad.Feel(); // the pad's vibration: before the enabled check below, so turning the module off silences it
    if (InFlight())
        space::TrackRounds(); // learns which incoming rounds hit (the damage model's hit chance)
    if (hostcalls::g.SetKeepRunning)
        hostcalls::g.SetKeepRunning(opts.runInBackground);
    if (!enabled_gate::AnyRuns(enabled_gate::Read(opts.enabled)))
    {
        hostcalls::CursorHide(hostcalls::kCursorHideIdle, false); // (the idle cursor's fail-safe: a disabled logic never leaves it hidden)
        return;
    }

    const u32 now = GetTickCount();
    // The UI's settings as its modules read them (uiopt.h): the registry's values, copied each frame.
    {
        if (pad.Options)
            pad.Options();
        uiopt::UiOptions& u = uiopt::Get();
        u.textSize = ApplySize(); // (hud_size; the player's own text_size stays theirs)
        if (views::hud.GameText)
            views::hud.GameText(g_mode == Mode::Menu);
        const hudrows::Values& h = hudrows::g;
        u.hudStyle = h.hudStyle, u.hudFade = h.hudFade, u.hudOpacity = h.hudOpacity;
        u.hudScale = h.hudScale, u.hudNumbers = h.hudNumbers, u.hudThreats = h.hudThreats;
        u.hudContext = h.hudContext, u.hudPrompts = h.hudPrompts, u.hudAlerts = h.hudAlerts;
        u.hudInfo = h.hudInfo;
        u.hudWireframes = h.hudWireframes, u.hudTransition = h.hudTransition, u.hudPreset = h.hudPreset;
    }
    if (pad.Device)
        pad.Device(dt, g_mode == Mode::Space);
    // A new flight - the player ship appeared or changed: a launch, or a save load, also one started from inside a flight that never passes the main
    // menu: the objective is re-read from the game's own copy (the hooked one was the previous save's). (The game clears the target itself on a load.)
    {
        static u32 lastShip = 0;
        const u32 ship = space::PlayerShipId();
        if (ship != lastShip)
        {
            if (ship && hostcalls::g.ObjectiveReread)
                hostcalls::g.ObjectiveReread();
            lastShip = ship;
        }
    }
    if (pad.Watch)
        pad.Watch(now);
    const bool padOwns = pad.Owns && pad.Owns();
    g_padOwns = padOwns;

    // Debounce: the cursor name flickers for a frame while windows open and close, and every mode change releases held inputs. Accept a new mode once it
    // has held for 3 frames.
    static Mode candidate = Mode::Menu;
    static int candidateFrames = 0;
    // Player death (Freelancer switches to its death camera).
    {
        static bool dead = false;
        const bool nowDead = fl::CamDeath() && fl::CurrentCamera() == fl::CamDeath();
        if (nowDead && !dead)
        {
            events::Push("player-death");
            if (pad.Died)
                pad.Died();
            // the autopilot's own hull-0 stop normally came a frame earlier; this one ends the objective mode on a death whatever the hull read
            views::ApStop("player died");
        }
        dead = nowDead;
    }
    // The game's sequences (dock, launch, jump, lane) lock the player out by the ship's control exclusion; a sequence cut short leaves it set. Every
    // change is an event, with the camera.
    {
        static int lastExclusion = -2;
        const int exclusion = space::PlayerControlExclusion();
        if (exclusion != lastExclusion)
        {
            const u32 cam = fl::CurrentCamera();
            events::Push("control-exclusion %d -> %d camera=%s", lastExclusion, exclusion,
                         exe::CamDocking.is(cam)    ? "docking"
                         : exe::CamCutscene.is(cam) ? "cutscene"
                                                    : "other");
            lastExclusion = exclusion;
        }
    }
    const Mode detected = gamemode::Detect(FlightViewOnly, gamemode::SpaceWindowOpen);
    candidateFrames = detected == candidate ? candidateFrames + 1 : 1;
    candidate = detected;
    const Mode mode = candidateFrames >= 3 ? detected : g_mode;
    if (mode != g_mode)
    {
        Log("mode %s -> %s", ModeName(g_mode), ModeName(mode));
        events::Push("mode %s -> %s", ModeName(g_mode), ModeName(mode));
        if (mode == Mode::Menu)
            views::ApNoteMenu();
        // out of the menus = a game loaded, started or launched into: the objective the load restored read once from the game. Only here: inside a
        // session the hooked events are newer than the client copy.
        if (g_mode == Mode::Menu && hostcalls::g.ObjectiveReread)
            hostcalls::g.ObjectiveReread();
        flightio::ReleaseAll();
        if (pad.ModeChange)
            pad.ModeChange(g_mode, mode); // reads g_hudRoots: before they go below
        views::PanelClose(); // the goal panel, whoever plays: it takes Esc, Enter and the arrows from whatever comes next
        if (mode != Mode::Space && mode != Mode::SpaceUi)
            g_hudRoots.clear();
        if (mode == Mode::Docked || mode == Mode::Menu)
            views::ApStop(mode == Mode::Docked ? "docked" : "left space");
        g_mode = mode;
    }
    views::ApDockSkip(dt, g_mode == Mode::Cutscene);

    // the session's mode per save, speedrun mode's clock / split detector / LiveSplit client and the HUD timer's bests: every frame, whatever the device
    session::Update(dt, ModeName(g_mode));
    modslots::Frame(modslots::kSpeedrun, dt, ModeName(g_mode), fl::GamePaused());
    // HUD 2 (modules\hud\docs\UI_HUD2.md): its state and data, the native HUD switch's decision, the HUD bus - every frame
    {
        views::HudContext hc;
        hc.space = g_mode == Mode::Space;
        hc.spaceUi = g_mode == Mode::SpaceUi;
        hc.cutscene = g_mode == Mode::Cutscene;
        hc.pad = padOwns;
        hc.autopilot = views::ApActive();
        hc.apKnown = views::ApPresent() && (uiopt::Get().apHints || session::Current() == session::ASSISTED);
        // a box over the flight with the pad (the cargo demand: the game's arrow up, flight by FlightViewOnly): its prompt bar takes A and B, so the context
        // line steps aside as for the picker - its B / X holds were not true while the box stood
        const bool flightBox = padOwns && detected == Mode::Space && CargoDemandUp();
        hc.modal = views::PanelOpen();
        if (pad.FillHud)
            pad.FillHud(hc); // (sets modal from the pad's own windows)
        hc.modal |= flightBox;
        if (views::hud.Flight)
            views::hud.Flight(dt, hc);
        if (views::hud.Cursor)
            views::hud.Cursor(dt, padOwns, opts.enabled && g_mode == Mode::Space && detected == Mode::Space && !fl::GamePaused() && !hc.modal, OnlyHudOpen);
    }
    // The goal panel, every device (its keyboard + mouse form's keys, clicks and click box; after HUD 2, which stood its own click box down while the
    // panel is open)
    views::PanelTick(dt);
    // HUD 2 over the menus and rooms: the skin's switch and the docked overlay's - every frame. Classic (hud_style 0) is classic everywhere.
    if (views::hud.Skin)
        views::hud.Skin(dt, padOwns, opts.enabled, g_mode == Mode::Docked || detected == Mode::Docked); // (the raw signal too: the docked strip came 3 frames late)
    // The open options page owns the pad and the keys: no flight / menu pad dispatch. Its ownership outlasts its close until every button is up: the
    // press that closed it reached the menu and base code in the same frame and again as a fresh edge of padmenus' own pad reader.
    static bool pageLatch = false;
    const bool pageHolds = inputrules::PageHolds(options_page::WantsInput(), pad.MenuButtonDown && pad.MenuButtonDown(), pageLatch);
    // The autopilot keeps working if the game isn't paused: under a NeuroNet window or the page it flies, and they keep the pad and the cursor.
    // FLUF's windows are the same case, though the game's cursor and mode do not show them.
    const bool windowUp = g_mode == Mode::SpaceUi || (g_mode == Mode::Space && flufaware::OwnsPad());
    const bool underWindow = inputrules::FliesUnderWindow(views::ApActive(), windowUp, pageHolds, g_mode == Mode::Space, fl::GamePaused());
    // A stop under a window (the key, the bar): no row below calls the flight again, and its hand-back is what lets go of the holds. A pause over the window
    // ends `underWindow` for its length, so a stop during it counts too.
    static bool flewUnder = false;
    if (underWindow)
        flewUnder = true;
    if (flewUnder && !views::ApActive())
    {
        views::ApFly(dt);
        flewUnder = false;
    }
    else if (!windowUp && !pageHolds)
        flewUnder = false;
    flightio::SetUnderWindow(underWindow);
    if (hostcalls::g.SetSteerLift)
        hostcalls::g.SetSteerLift(underWindow, views::ApActive());
    if (pageHolds)
    {
        if (underWindow)
            views::ApFly(dt);
        return;
    }
    if (pad.Prompts)
        pad.Prompts(dt, padOwns); // Start, and the pad HUD layer's view of the frame

    // While the pad (or the test bot) flies, the HUD is transparent to the cursor: aiming can cross it in every direction. The physical mouse keeps normal
    // HUD access.
    const bool autoFlies = (pad.BotOn && pad.BotOn()) || views::ApActive();
    static bool autoFlew = false;
    if (autoFlew && !autoFlies && padOwns && g_mode == Mode::Space) // (never the player's mouse, never a window's cursor)
        dimouse::MoveTo(fl::ScreenWidth() * 0.5f, fl::ScreenHeight() * 0.5f); // aim cursor off the HUD when done
    autoFlew = autoFlies;
    // ... but an open NeuroNet window is the cursor's, whoever flies: the windows and the NeuroNet box's buttons hang under the HUD's root NN_MainButton, so
    // the suppression also took the map's selection and the buttons to the inventory, log and player info from the cursor. By the window, not by
    // Mode::SpaceUi: that mode also means an arrow cursor over the HUD, which the suppression keeps from latching
    SetHudSuppression(InFlight() && (padOwns || autoFlies) && !gamemode::SpaceWindowOpen());

    // The flight HUD's windows, scanned while flying (whoever flies) and only while no window has the mouse (a window opening switches the cursor to the
    // arrow at once): the mode detection tells the HUD from NeuroNet windows by them.
    static float hudScan = 0;
    hudScan -= dt;
    if (g_mode == Mode::Space && hudScan <= 0 && _stricmp(fl::CursorName(), "arrow") != 0)
    {
        g_hudRoots = ui::VisibleRoots();
        g_hudRootsChanged = true;
        hudScan = 0.25f;
    }

    // The test bot and the autopilot fly whoever touched the mouse or pad last.
    const inputrules::Frame owner = inputrules::WhoHasTheFrame(autoFlies, g_mode == Mode::Space, padOwns, underWindow);
    if (owner == inputrules::Frame::Window)
    {
        views::ApFly(dt);
        if (padOwns && pad.Rest)
            pad.Rest(dt); // the window's own pad screen
        return;
    }
    if (owner == inputrules::Frame::Flight)
    {
        if (pad.Fly)
            pad.Fly(dt);
        else
            views::ApFly(dt);
        return;
    }
    if (owner == inputrules::Frame::Device)
    {
        // The physical mouse / keyboard is in use: stay out of its way (the motion trace still records the flight).
        if (g_mode == Mode::Space)
        {
            flightio::ReleaseAll();
            if (pad.DeviceRow)
                pad.DeviceRow();
            views::ApTracePlayer(dt);
        }
        return;
    }
    if (pad.Rest)
        pad.Rest(dt); // FLUF's windows, the menus and the mode's own pad screen
}

bool OnRawKey(int vk, bool down, bool textField)
{
    for (const inputrules::KeyStep step : inputrules::kKeyChain)
        switch (step)
        {
        case inputrules::KeyStep::Fluf:
            // FLUF's text box has the keyboard or its command palette is open, focused or not, as a chat box: every key is FLUF's (FLvibed's hook sees keys
            // before FLUF's; FLUF withholds them from the game itself), and every FLvibed key stands down
            if (flufaware::HoldsKeys())
                return false;
            break;
        case inputrules::KeyStep::Page:
            if (options_page::OnRawKey(vk, down, textField))
                return true;
            break;
        case inputrules::KeyStep::GoalPanel: // the open goal panel's keys (its keyboard + mouse form)
            if (views::PanelKey(vk, down, textField))
                return true;
            break;
        case inputrules::KeyStep::Modules: // a module's own screen over the game's (the New Game list by keyboard)
            if (modslots::Key(vk, down))
                return true;
            break;
        case inputrules::KeyStep::BarButton:
            return views::BarKey(vk, down, textField);
        }
    return false;
}

bool OnUiButton(int id)
{
    if (id == kUiBtnBar)
    {
        views::BarClick();
        return true;
    }
    if (id >= kUiBtnHud2 && id < kUiBtnHud2 + kUiBtnHud2Count) // HUD 2's keyboard-and-mouse hotspots
    {
        if (views::PanelOwnsHotspots()) // the open goal panel's keyboard + mouse form holds the click box (HUD 2's set is off)
            views::PanelHotspot(id - kUiBtnHud2);
        else
            views::HudHotspot(id - kUiBtnHud2);
        return true;
    }
    return false;
}
} // namespace coreframe
