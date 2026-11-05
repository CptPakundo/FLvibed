#include "controller.h"

#include "../dimouse.h"
#include "../events.h"
#include "aim.h"
#include "autopilot.h"
#include "barbutton.h"
#include "settings.h"
#include "../fl.h"
#include "game/ai.h"
#include "game/deckmode.h"
#include "windowtext.h"
#include "gametext.h"
#include "game/flight.h"
#include "game/gameclock.h"
#include "game/geom.h"
#include "game/mission.h" // ForgetGame at a game load (host v34)
#include "game/render.h" // OnDeviceLost: the shared renderer's state block before a Reset
#include "game/room.h"
#include "game/space.h"
#include "game/spawn.h"
#include "log.h"
#include "../shm.h"
#include "debug.h"
#include "device.h"
#include "gamepad.h"
#include "glyphs.h"
#include "haptics.h" // the pad's vibration: events -> effect table -> mixer -> the two motors (IDEAS U30)
#include "goals.h"  // phase D flight modals (the D-FLIGHT track): the goal panel,
#include "picker.h" // the target picker,
#include "wheel.h"  // the action wheel
#include "host.h"
#include "padhud.h"
#include "hud2.h" // HUD 2: FLvibed's own flight HUD (docs\UI_HUD2.md; the HUD2 track)
#include "mprules.h"
#include "flufaware.h" // FLUF's windows: keep-outs, the pad on them, the keys' yield (docs\MOD_COMPATIBILITY.md FLUF)
// D-MENUS (25 Oct 2026): the menu screens on the pad, the New Game prompt, speedrun mode (clock, splits, LiveSplit)
// and the HUD timer (padmenus.h / newgame.h / livesplit.h / runtimer.h)
#include "livesplit.h"
#include "maneuver_watch.h" // the maneuver watchdog's rule (0.2.1 hotfix)
#include "target_cycle.h"   // the targeting press's enemy rule (4 Nov 2026)
#include "newgame.h"
#include "padmenus.h"
#include "runtimer.h"
#include "padbases.h" // phase D bases / dealers / NeuroNet windows (the D-BASES track),
#include "padmap.h"   // the nav map,
#include "wgroups.h"  // the weapon group editor,
#include "game/hudinfo.h"
#include "stallrec.h" // the NPC stall recorder
#include "promptbar.h" // the shared prompt bar (D-MENUS)
#include "presets.h"
#include "press_gate.h" // press ownership between the pad's consumers (the attended run's B leak, 26 Oct 2026)
#include "reticlecue.h"
#include "shieldcue.h"
#include "options_page.h"
#include "ui.h"
#include "uiopt.h"
#include "uiskin.h"  // Extended HUD 2: the skin over the game's windows (the SKIN track)
#include "uistrip.h" // Extended HUD 2: the docked overlay (the STRIP track)

#include <deque>
#include <map>
#include <sstream>

namespace ctl
{
enum class Mode
{
    Menu,     // main menu, loading, multiplayer browser...
    Docked,   // on a base or planet
    Space,    // flying
    SpaceUi,  // in space with a NeuroNet window (nav map, inventory...) using the cursor
    Cutscene, // in-space cinematic or docking camera
};

static const char* ModeName(Mode m)
{
    switch (m)
    {
    case Mode::Menu: return "menu";
    case Mode::Docked: return "docked";
    case Mode::Space: return "space";
    case Mode::SpaceUi: return "space-ui";
    case Mode::Cutscene: return "cutscene";
    }
    return "?";
}

// ---------------------------------------------------------------- options
struct Options
{
    bool enabled = true;
    bool focusOnly = true;    // ignore the real pad while Freelancer is not the foreground window
    bool maneuverWatchdog = true; // a formation / goto / dock stalled far from its goal is issued again (maneuver_watch.h)
    float cursorSpeed = 1.1f; // screen heights per second at full stick deflection
    float wheelRate = 10.0f;  // wheel notches per second at full deflection
    float steerExpo = 1.5f;   // steering response curve exponent (1 = linear)
    // Full deflection = this offset in Freelancer's mouse-flight units (1 = cursor at the screen
    // edge). Measured: turn rate = offset * 1.3 * max rate (steering_torque / angular_drag), so
    // the ship reaches its max turn rate at ~0.77 and any further cursor offset is wasted.
    float steerScale = 0.77f;
    bool invertY = false;         // stick up = nose down
    float throttleRate = 0.9f;    // throttle change per second at full deflection
    bool aimCursor = true;        // right stick also moves the aim cursor (guns follow it)
    float aimAssist = 0.75f;      // 0 off .. 1: pull of the aim cursor onto a hostile target's lead cross
    float aimRadius = 0.6f;       // cursor pull radius around the lead cross, fraction of screen height
    float aimSlowRadius = 0.12f;  // steering slowdown radius around the lead cross (fraction of height)
    float aimSlowdown = 0.35f;    // steering slowdown at the cross (0 none .. 1 full stop)
    float aimRotation = 0.6f;     // rotational assist strength (0 off .. 1 closes the whole error)
    float aimRotationMax = 0.15f; // cap of the rotational correction (steer units; 0.15 ~ 20 deg/s)
    // 0: the aim cursor follows the stick (+ slowdown, magnetism, rotational assist).
    // 1: "gimbal" - the stick only steers; the aim cursor sits at the screen center and snaps onto a
    //    hostile target's lead cross whenever it is within aim_cone degrees of the view axis.
    int aimMode = 1;
    float aimCone = 90.0f;         // gimbal mode: auto-aim cone half-angle (degrees); 90 = whole screen
    float aimSmooth = 0.06f;       // aim cursor smoothing time constant (s); removes lead-cross jitter
    int autoTarget = 0;            // see kOptions (default: the player always picks targets)
    bool fireTarget = true;        // see kOptions
    float autopilotCancel = 0.35f; // see kOptions: stick deflection that hands the ship back (0 = never)
    bool runInBackground = false;  // see kOptions
    // The UI build (25 Oct 2026; docs\UI_SPEC.json, the defaults = the spec's `def`): rows the options page shows and
    // the device service / glyphs / pad HUD / presets / LiveSplit client read as they are built (the codings: kOptions).
    int inputMode = 0;             // 0 automatic, 1 controller, 2 keyboard and mouse
    int glyphStyle = 0;            // 0 automatic (by the pad's HID ids), 1 Xbox, 2 PlayStation, 3 Nintendo
    int confirmButton = 0;         // 0 the bottom face button (A / cross / B), 1 the right one (Nintendo)
    bool pauseOnDisconnect = true; // a pad unplugged in flight pauses the game
    int padPreset = 0;             // 0 FREELANCER, 1 ACE, 2 ASSISTED, 3 CUSTOM (ui_meta.h kControllerPresets)
    int weaponGroups = 1;          // 0 simple (one fire button), 1 cycle on Y hold, 2 two groups on the triggers
    bool swapY = false;            // y_swap: with weapon_groups 1, Y tap = cruise, Y hold = next group (the old order)
    bool holdsAsToggles = false;   // afterburner, reverse, drift, rear view as press-once / press-again
    int rumble = 60;               // vibration strength in percent (0 = off)
    int padCursor = 2;             // 0 off, 1 rooms and maps, 2 everywhere (the owner's decision 15, 25 Oct 2026)
    int cursorMagnet = 60;         // the pad cursor's slow-down / pull over clickable elements, percent
    int promptBar = 0;             // 0 contextual, 1 always, 2 off
    int borderless = 1;            // borderless: 0 off, 1 auto (only when nothing else provides it), 2 always; the host reads it at the start (winmode.h)
    int flufWindows = 2;           // fluf_windows: FLUF's windows 0 ignored, 1 player windows (keep-outs, the pad drives them), 2 all (default: the owner, 26 Oct 2026)
    int worldPrompts = 0;          // 0 contextual, 1 off
    bool apHints = true;           // the autopilot's prompts / status shown before it was ever used (on since 0.2.0)
    int textSize = 100;            // FLvibed's own UI text, percent (80..150)
    int deckMode = 0;              // Steam Deck mode: 0 automatic (at 1280x800), 1 on, 2 off (core\game\deckmode.h)
    int gameTextSize = 100;        // the game's own text, percent (100..150; gametext.h)
    bool windowText = true;        // the game's windows' text by role, HUD 2 only (windowtext.h)
    bool hudGlyphs = true;         // glyph badges on the HUD's own elements in pad mode
    int buttonLegend = 0;          // 0 on, 1 off (the contextual button legend)
    bool apStatusLine = true;      // the autopilot's one status line under the maneuver bar
    bool showAdvanced = false;     // the options page lists every registry row, raw
    bool speedrunTimer = true;     // speedrun mode: the timer / split on the HUD
    int livesplit = 1;             // 0 off, 1 LiveSplit Server (TCP)
    int livesplitPort = 16834;
    bool livesplitGametime = true; // pause LiveSplit's game time during loads
    std::string autopilotKey = "K"; // the keyboard key that toggles the autopilot (UI_PLAN.md 2: K is unbound)
    bool maneuverButton = true;    // the fifth maneuver-bar button
    // HUD 2 (docs\UI_HUD2.md 5.3; the codings: kOptions)
    int hudStyle = 2, hudFade = 1, hudOpacity = 100, hudScale = 100, hudNumbers = 0, hudContext = 0, hudPrompts = 1;
    bool hudThreats = true, hudAlerts = true, hudInfo = true, hudWireframes = true;
    int hudTransition = 120;
    int hudPreset = 0; // hud_preset: HUD 2's layout preset (UI_HUD2_PRESETS.md 6 step 4; registry-only until step 5)
    int uiStyle = 1; // ui_style: the non-flight windows' look, 0 CLASSIC, 1 HUD 2 (uiskin.*, uistrip.*); only with HUD 2 on
};
static Options g_opt;

static Gamepad g_pad;
// The menus' view of the pad: g_pad, with A and B exchanged when confirm_button is 1 (the right button accepts). Every
// menu-grammar consumer reads it (the free cursor, FLUF's windows, padbases / padmap, the flight modals' copy); the
// flight bindings read g_pad, positional (UI_BUILD.md 5, 29 Oct 2026). Refreshed right after g_pad.Update.
static Gamepad g_menuPad;
static void LoadConfig();
static Mode g_mode = Mode::Menu;
static u32 g_frame = 0;
static float g_wheelAccum = 0;
static u32 g_lastPadTick = 0;

// The pad's flight bindings (presets.*: the spec's FREELANCER / ACE / ASSISTED tables and the flight_ap situation,
// tap / hold / double-tap) and the state the bindings keep.
static presets::Dispatcher g_dispatch;
static presets::Table g_table;    // the effective table of the current situation (this frame)
static int g_weaponGroup = 1;     // the active weapon group as the pad last set it (1..3; weapon_groups 1 cycles it)
static bool g_turret = false;     // turret view toggled on by the pad (USER_AUTO_TURRET, the game's H)
static float g_turretAimX = 0, g_turretAimY = 0; // turret view: the look commands' duty accumulators (stick -> share of frames)
static bool g_apUsed = false;     // the player has used the autopilot this session (its prompts may show)

// ---------------------------------------------------------------- command helpers
// Commands currently held by the pad (so we can release them on mode change), with the
// time until the next emulated key repeat for commands declared "repeat".
struct HoldState
{
    bool down = false;
    float repeat = 0;
};
static std::map<int, HoldState> g_holding;
static float g_frameDt = 0;

// Send the events a key bound to `cmd` would produce (per its keymap.ini "state" mask).
static void Hold(int cmd, bool want)
{
    HoldState& h = g_holding[cmd];
    const u32 flags = fl::CommandFlags(cmd);
    if (want && h.down && (flags & fl::CMDF_REPEAT))
    {
        h.repeat -= g_frameDt;
        if (h.repeat <= 0)
        {
            fl::FireCommand(cmd, true);
            h.repeat = 1.0f / 15.0f;
        }
    }
    if (want == h.down)
        return;
    h.down = want;
    fl::SetHeld(cmd, want);
    if (want && (flags & fl::CMDF_KEYDOWN))
    {
        fl::FireCommand(cmd, true);
        h.repeat = 1.0f / 15.0f;
    }
    if (!want && (flags & fl::CMDF_KEYUP))
        fl::FireCommand(cmd, false);
}

// A group's trigger (weapon_groups 2). USER_FIRE_WEAPON_GROUPn is keymap state = keydown with no repeat, and its handler
// (0x4DCC60) fires once per keydown; only USER_FIRE_WEAPONS is polled as held (0x51D115). So a held trigger gave one shot
// a pull (GitHub issue 14). A keydown each frame matches fire-all's per-frame poll; the guns' own refire sets the rate.
static void HoldFireGroup(int cmd, bool want)
{
    const bool was = g_holding[cmd].down;
    Hold(cmd, want);
    if (want && was)
        fl::FireCommand(cmd, true);
}

static bool g_inTap = false; // a pad tap is being fired (OnKeyCmd skips it: Apply answered it)
static void Tap(int cmd)
{
    const u32 flags = fl::CommandFlags(cmd);
    g_inTap = true;
    if (flags & fl::CMDF_KEYDOWN)
        fl::FireCommand(cmd, true);
    if (flags & fl::CMDF_KEYUP)
        fl::FireCommand(cmd, false);
    g_inTap = false;
}

// The maneuver lock's answer (0.2.1; a player report: in M01, after joining Brandt's formation, the mission's
// Act_LockManeuvers made B / free flight do nothing, which looked like a malfunction): while the game's maneuvers are
// locked (IBehaviorManager +0xBC: space::Pilot::locked, `Get-FLPilot` Locked) a requested maneuver is logged and HUD 2's
// alert card says LOCKED BY THE MISSION. The request still goes to the game (it refuses it itself: no game logic changed).
// The log line at most once a second per action (a held key repeats).
static bool LockAnswer(const char* what)
{
    space::Pilot p;
    if (!space::ReadPilot(p) || !p.locked)
        return false;
    static const char* lastWhat = nullptr;
    static DWORD lastAt = 0;
    const DWORD now = GetTickCount();
    if (what != lastWhat || now - lastAt > 1000)
        events::Push("maneuvers locked: %s refused%s", what, hud2::LockNotice() ? "" : " (HUD 2 not shown)");
    else
        hud2::LockNotice();
    lastWhat = what, lastAt = now;
    return true;
}

void OnKeyCmd(int cmd, bool down)
{
    if (g_inTap || !down || g_mode != Mode::Space)
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
        hud2::NoteAction(hudwake::Maneuver);
    else if (cmd == fl::USER_CRUISE || cmd == fl::USER_MANEUVER_ENGINEKILL)
        hud2::NoteAction(hudwake::Cruise);
    else if (cmd == fl::USER_CLEAR_TARGET || cmd == fl::USER_NEXT_ENEMY || cmd == fl::USER_CLOSEST_ENEMY)
        hud2::NoteAction(hudwake::TargetChg);
}

// Freelancer's ship behaviour keeps the last steering direction it was given, and its own mouse-flight
// code only sends a new one while mouse flight is engaged. So before handing steering back, the
// override sends "no turn" for a few frames; otherwise the ship keeps spinning.
static int g_steerStopFrames = 0;
static bool g_steerActive = false;
static float g_steerX = 0, g_steerY = 0; // the last override sent (the maneuver watchdog's snapshot)

static void SetSteer(float x, float y)
{
    HostSteer(true, x, y);
    g_steerActive = true;
    g_steerStopFrames = 0;
    g_steerX = x, g_steerY = y;
}

static void ReleaseSteer()
{
    if (g_steerActive)
    {
        HostSteer(true, 0, 0);
        g_steerActive = false;
        g_steerStopFrames = 3;
        g_steerX = g_steerY = 0;
    }
}

// The maneuver watchdog (setting `maneuver_watchdog`; rule and the why: maneuver_watch.h). The owner's M01 report: a
// formation with the Brandt stalled at the mission's LockManeuvers (constant hard turn, 0 m/s, the convoy gone). While a
// game formation / goto / dock runs outside the Advanced Autopilot, a stall far from the goal logs one snapshot and
// issues the same maneuver again the game's way (target the leader / goal, the player's maneuver dispatcher
// ai::Maneuver = the maneuver bar's call, as the harness's Start-FLFormation), then puts the player's target back.
static void WatchManeuver()
{
    static mwatch::Watch watch;
    static int lastOp = -1;
    static u32 goal = 0;      // goto / dock: the target when the op began; formation: the ship the player formed on
    static float lastSteerAt = -100;
    const float t = static_cast<float>(GetTickCount()) * 0.001f;
    mwatch::Sample s;
    s.t = t;
    space::Pilot p;
    const bool flying = g_opt.maneuverWatchdog && space::ReadPilot(p);
    if (!flying)
    {
        lastOp = -1, goal = 0;
        watch.Step(s);
        return;
    }
    const bool kind = p.op == ai::OpGoto || p.op == ai::OpDock || p.op == ai::OpFormation;
    if (p.op != lastOp)
        goal = kind ? (p.op == ai::OpFormation && !p.target ? p.leader : p.target) : 0;
    lastOp = p.op;
    // the distance is to the formation's leader (the wing), else to the goal
    const u32 to = p.op == ai::OpFormation ? (p.leader ? p.leader : goal) : goal;
    float gp[3];
    const bool known = kind && to && !p.lane && !p.jumping && space::ObjectPos(to, gp);
    space::ShipMotion m;
    const PadState& pad = g_pad.Cur();
    const float stick = (std::max)((std::max)(fabsf(pad.lx), fabsf(pad.ly)), (std::max)(fabsf(pad.rx), fabsf(pad.ry)));
    const u32 mouseAt = dimouse::LastRealTick();
    if (stick > 0.2f || (mouseAt && GetTickCount() - mouseAt < 1000))
        lastSteerAt = t;
    s.maneuver = known && space::ShipMotionOf(space::PlayerShipId(), m) && m.ok;
    s.key = (static_cast<unsigned>(p.op) << 24) ^ to;
    s.autopilot = autopilot::Active();
    s.steering = t - lastSteerAt < 1.0f;
    if (s.maneuver)
    {
        s.speed = m.speed;
        const float d[3] = {gp[0] - p.pos[0], gp[1] - p.pos[1], gp[2] - p.pos[2]};
        s.dist = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    }
    const mwatch::Decision dec = watch.Step(s);
    const char* opName = p.op == ai::OpGoto ? "goto" : p.op == ai::OpDock ? "dock" : "formation";
    if (dec.snapshot)
    {
        const u32 now = GetTickCount();
        const u32 padAt = device::LastPadTick(), kbmAt = device::LastKbmTick();
        // the leader / goal as the game reports it (the owner's 4 Nov report: the stall began as the convoy docked with
        // a trade lane - a formation follower runs a copy of its leader's dock, docs\FREELANCER_RE.md "Formation")
        int lDock = -1, lLane = -1, lJump = -1;
        float lSpeed = -1;
        for (const space::Contact& c : space::Contacts())
            if (c.id == to)
            {
                lDock = c.dockPort ? 1 : 0, lLane = c.lane ? 1 : 0, lJump = c.jumping ? 1 : 0;
                break;
            }
        space::ShipMotion lm;
        if (space::ShipMotionOf(to, lm) && lm.ok)
            lSpeed = lm.speed;
        events::Push("event maneuver stalled: op=%s locked=%d leader=%u goal=%u target=%u dist=%.0f speed=%.1f "
                     "throttle lt=%.2f rt=%.2f steer active=%d x=%.2f y=%.2f device=%s last inputs pad=%dms kbm=%dms "
                     "mouse=%dms sticks l=%.2f,%.2f r=%.2f,%.2f buttons=0x%X cruise=%d | leader docking=%d lane=%d "
                     "jumping=%d speed=%.1f",
                     opName, p.locked ? 1 : 0, p.leader, goal, p.target, s.dist, s.speed, pad.lt, pad.rt,
                     g_steerActive ? 1 : 0, g_steerX, g_steerY, device::Pad() ? "pad" : "kbm",
                     padAt ? static_cast<int>(now - padAt) : -1, kbmAt ? static_cast<int>(now - kbmAt) : -1,
                     mouseAt ? static_cast<int>(now - mouseAt) : -1, pad.lx, pad.ly, pad.rx, pad.ry, pad.buttons,
                     p.cruise ? 1 : 0, lDock, lLane, lJump, lSpeed);
    }
    if (dec.retry)
    {
        // the ship to issue it on: the one the player formed on / the goal; a formation whose ship is gone -> the leader
        float tmp[3];
        u32 on = goal && space::ObjectPos(goal, tmp) ? goal : p.op == ai::OpFormation ? p.leader : 0;
        const u32 before = p.target;
        bool targeted = on && (before == on || space::SetTarget(on));
        // Left first, then issued again - the player's own way out (the owner's 4 Nov report: the same formation issued on
        // a ship still in it got reply -1 twice and the ship kept turning in place; free flight twice, then FORMATION,
        // got reply 0 and no stall followed). The op changes in the frame of the call (FREELANCER_RE "Maneuvers").
        if (targeted)
            ai::FreeFlight();
        const bool ok = targeted && ai::Maneuver(p.op, false);
        if (before && before != on && targeted)
            space::SetTarget(before); // the player's target as it was (the maneuver keeps its own)
        space::Pilot after;
        const bool read = space::ReadPilot(after);
        events::Push("maneuver watchdog: re-issue %s on %u try %d/%d: %s (op now %d, locked=%d, leader=%u)", opName,
                     on, dec.attempt, mwatch::kMaxTries,
                     !on ? "no goal" : !targeted ? "target refused" : ok ? "accepted" : "refused",
                     read ? after.op : -1, read && after.locked ? 1 : 0, read ? after.leader : 0);
    }
    if (dec.gaveUp)
        events::Push("maneuver watchdog: %s still stalled after %d tries (dist=%.0f speed=%.1f locked=%d): giving up "
                     "until it moves",
                     opName, mwatch::kMaxTries, s.dist, s.speed, p.locked ? 1 : 0);
}

// The player ship's state once a second, as an event when it changed (hull to 0.1 %, shield to 1 %, consumables):
// the run's own hull / shield / consumables track, comparable with the footage's HUD scans (hud1 / count1 csv) - per
// section of a mission, what each fight cost (user, 20 Oct 2026: hull is sacred; compare with the 2020 playthrough).
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

// Called once per frame: completes a release started by ReleaseSteer().
static void TickSteerRelease()
{
    if (!g_steerActive && g_steerStopFrames > 0 && --g_steerStopFrames == 0)
        HostSteer(false, 0, 0);
}

static void ReleaseAll()
{
    for (auto& h : g_holding)
    {
        if (h.second.down)
        {
            fl::SetHeld(h.first, false);
            if (fl::CommandFlags(h.first) & fl::CMDF_KEYUP)
                fl::FireCommand(h.first, false);
            h.second.down = false;
        }
    }
    ReleaseSteer();
    dimouse::Reset();
}

static std::vector<uintptr_t> g_hudRoots; // the flight HUD's windows (scanned while flying)

// Only flight HUD windows open (the arrow cursor then comes from the cursor resting on the HUD or an object, not
// from a NeuroNet window taking the mouse). Unknown HUD = false.
static bool OnlyHudOpen()
{
    if (g_hudRoots.empty())
        return false;
    const std::vector<uintptr_t> roots = ui::VisibleRoots();
    return std::all_of(roots.begin(), roots.end(), [](uintptr_t r)
                       { return std::find(g_hudRoots.begin(), g_hudRoots.end(), r) != g_hudRoots.end(); });
}

static Mode DetectMode()
{
    if (fl::PlayerBase())
        return Mode::Docked;
    if (fl::PlayerIObj())
    {
        const u32 cam = fl::CurrentCamera();
        if (exe::CamCutscene.is(cam) || exe::CamDocking.is(cam))
            return Mode::Cutscene;
        // Freelancer switches to the arrow cursor while a NeuroNet window takes the mouse - and also while the cursor
        // rests on the HUD, which is still flight.
        if (_stricmp(fl::CursorName(), "arrow") == 0 && !OnlyHudOpen())
            return Mode::SpaceUi;
        // (D-BASES) the NeuroNet windows hang under the HUD's own NeuroNet root: known by their root controls
        if (padbases::SpaceWindowOpen())
            return Mode::SpaceUi;
        return Mode::Space;
    }
    return Mode::Menu;
}

// ---------------------------------------------------------------- UI focus navigation
static float g_navRepeat = 0; // auto-repeat timer for held D-pad directions
static u32 g_navHeldDir = 0;
static float g_snapHold = 0; // keep driving the cursor to a snapped control briefly
static std::vector<uintptr_t> g_prevRoots;
// Windows that make up the flight HUD (recorded while flying). When another window opens over
// them (pause menu, nav map, dialogs) they are background: not new, not navigable.
static bool g_navPrimed = false; // roots from the first frame after loading are not "new"

// HUD hit-test suppression (host API v4): see HostApi::SetHudHitSuppression.
static bool g_hudSuppressed = false;
static bool g_hudRootsChanged = false;

static void SetHudSuppression(bool on)
{
    if (!HostAtLeast(4))
        return;
    if (on == g_hudSuppressed && !(on && g_hudRootsChanged))
        return;
    g_hudSuppressed = on;
    g_hudRootsChanged = false;
    // FLvibed's own click box (HUD 2's hotspots, the goal panel's rows: a top-level root of the host's) stays clickable
    // while the autopilot flies: the keyboard + mouse player's clicks on it are meant (the owner, 26 Oct 2026: the
    // autopilot identical on both devices - the goal panel's rows, HUD 2's autopilot icon to stop it)
    std::vector<uintptr_t> roots;
    for (uintptr_t r : g_hudRoots)
        if (_stricmp(ui::ControlName(r), "FLvibedHud2Hotspots") != 0)
            roots.push_back(r);
    g_host->SetHudHitSuppression(on, roots.data(), static_cast<int>(roots.size()));
    aim::SetHudTransparent(on && !g_hudRoots.empty());
}

static void SnapTo(const ui::Item& it)
{
    float x = it.x, y = it.y;
    // Enter lists at their first row: the control spans the whole list box, whose
    // middle or bottom rows are often empty.
    float rowH, firstRow;
    if (ui::ListRows(it.ctl, rowH, firstRow))
        y = firstRow;
    dimouse::MoveTo(x, y);
    g_snapHold = 0.12f;
    Log("ui: focus %s (%.0f,%.0f)", it.name, x, y);
}

static bool IsAffirmative(const char* name)
{
    char low[40];
    strncpy_s(low, name, _TRUNCATE);
    _strlwr_s(low);
    return strstr(low, "accept") || strstr(low, "yes") || strstr(low, "okbutton") || strstr(low, "confirm") ||
           strstr(low, "continue") || strstr(low, "loadautosave");
}

static void UpdateUiNav(float dt)
{
    if (g_snapHold > 0)
    {
        g_snapHold -= dt;
        if (g_snapHold <= 0)
            dimouse::ReleaseTarget();
    }

    // Full item discovery (control tree + hit-test probing) is not cheap (the probe: 2.5-7 ms at 4K): fresh when the
    // player navigates (a press, or a held direction's repeat step due this frame), else the shared cache that probes
    // again only when the tree's clickable set changed (ui::CollectAllCached). Was: every 50 ms plus "g_navRepeat <=
    // dt", which held from the session's start until its first D-pad step (the timer starts at 0) and after a release
    // that left it under a frame: a full probe every frame in every pad screen, OnFrame 2.5 ms in a room and 7 ms on a
    // dealer at 4K (perf 25 Oct 2026, research\experience\runs\perf_pad_2026-10-25).
    const bool navPressed = g_pad.Pressed(XINPUT_GAMEPAD_DPAD_UP) || g_pad.Pressed(XINPUT_GAMEPAD_DPAD_DOWN) ||
                            g_pad.Pressed(XINPUT_GAMEPAD_DPAD_LEFT) || g_pad.Pressed(XINPUT_GAMEPAD_DPAD_RIGHT);
    const bool repeatDue = g_navHeldDir && g_pad.Down(g_navHeldDir) && g_navRepeat <= dt;
    std::vector<ui::Item> items = navPressed || repeatDue ? ui::CollectAll() : ui::CollectAllCached();
    const float mx = static_cast<float>(fl::MouseX()), my = static_cast<float>(fl::MouseY());

    // A window holding keyboard focus (pause menu, save dialog, message boxes...) is modal:
    // keep D-pad focus inside it. So is a menu's modal loop without the keyboard focus (padmenus::ModalRoot: the
    // multiplayer LAN / INTERNET prompt).
    uintptr_t modal = ui::ModalRoot(items);
    if (!modal)
        modal = padmenus::ModalRoot();
    if (modal)
        items.erase(
            std::remove_if(items.begin(), items.end(), [modal](const ui::Item& it) { return it.root != modal; }),
            items.end());

    // In space, a window opened over the flight HUD (pause menu, nav map, load dialog...) takes
    // the D-pad; the HUD behind it is only navigable when nothing else is open.
    auto isHud = [](const ui::Item& it)
    { return std::find(g_hudRoots.begin(), g_hudRoots.end(), it.root) != g_hudRoots.end(); };
    if (g_mode == Mode::SpaceUi &&
        std::any_of(items.begin(), items.end(), [&](const ui::Item& it) { return !isHud(it); }))
        items.erase(std::remove_if(items.begin(), items.end(), isHud), items.end());

    // Auto-focus: when new windows appear and the cursor is not already on one of their
    // controls, put it on the most likely choice: an affirmative button (OK / Accept /
    // Continue) if there is one, otherwise the new control closest to the screen center.
    std::vector<uintptr_t> roots;
    for (const ui::Item& it : items)
        if (std::find(roots.begin(), roots.end(), it.root) == roots.end())
            roots.push_back(it.root);
    auto isNew = [](uintptr_t r) { return std::find(g_prevRoots.begin(), g_prevRoots.end(), r) == g_prevRoots.end(); };
    const bool anyNew = g_navPrimed && std::any_of(roots.begin(), roots.end(), isNew);
    // The left stick moves the cursor this frame: the player points, no auto-focus (the owner's attended run 26 Oct 2026:
    // the stick's cursor move zeroed the snap's hold every frame, so the new window stayed "new" and the snap fired again
    // each frame - `ui: focus ManeuverCruiseButton` ten times in 50 ms, the cursor pulled back against the stick)
    const PadState& ps = g_pad.Cur();
    const bool stickPoints = (ps.lx != 0 || ps.ly != 0) && padbases::CursorAllowed();
    // (While a snap is still travelling to its target the cursor position is stale.)
    if (anyNew && g_snapHold <= 0 && !stickPoints)
    {
        const int under = ui::FindAt(items, mx, my);
        if (under < 0 || !isNew(items[under].root))
        {
            const float cx = fl::ScreenWidth() * 0.5f, cy = fl::ScreenHeight() * 0.5f;
            int best = -1;
            float bestD = 1e30f;
            for (size_t i = 0; i < items.size(); ++i)
            {
                if (!isNew(items[i].root))
                    continue;
                float d = (items[i].x - cx) * (items[i].x - cx) + (items[i].y - cy) * (items[i].y - cy);
                if (IsAffirmative(items[i].name))
                    d *= 0.01f;
                if (d < bestD)
                {
                    bestD = d;
                    best = static_cast<int>(i);
                }
            }
            if (best >= 0)
                SnapTo(items[best]);
        }
    }
    if (!(anyNew && g_snapHold > 0) || stickPoints) // keep new windows "new" until we could look at them
        g_prevRoots = roots;
    g_navPrimed = true;

    // D-pad moves focus between clickable controls, with auto-repeat when held.
    u32 dir = 0;
    const u32 dirs[4] = {XINPUT_GAMEPAD_DPAD_UP, XINPUT_GAMEPAD_DPAD_DOWN, XINPUT_GAMEPAD_DPAD_LEFT,
                         XINPUT_GAMEPAD_DPAD_RIGHT};
    for (u32 d : dirs)
    {
        if (g_pad.Pressed(d) && !padbases::Owns(d)) // (D-BASES: the bar's character strip / the dealers' side take L / R)
        {
            dir = d;
            g_navHeldDir = d;
            g_navRepeat = 0.4f;
        }
    }
    if (!dir && g_navHeldDir && g_pad.Down(g_navHeldDir))
    {
        g_navRepeat -= dt;
        if (g_navRepeat <= 0)
        {
            dir = g_navHeldDir;
            g_navRepeat = 0.11f;
        }
    }
    if (dir)
    {
        float dx = 0, dy = 0;
        if (dir == XINPUT_GAMEPAD_DPAD_UP)
            dy = -1;
        if (dir == XINPUT_GAMEPAD_DPAD_DOWN)
            dy = 1;
        if (dir == XINPUT_GAMEPAD_DPAD_LEFT)
            dx = -1;
        if (dir == XINPUT_GAMEPAD_DPAD_RIGHT)
            dx = 1;
        const int cur = ui::FindAt(items, mx, my);
        // Inside a list, up/down step row by row until the list's edge.
        float rowH, firstRow;
        if (cur >= 0 && dy != 0 && ui::ListRows(items[cur].ctl, rowH, firstRow))
        {
            // Snap to row centers so repeated steps cannot drift: the row the cursor is on comes from the list itself
            // (ui::ListHoveredRow, the data index minus the scroll bar's first visible row) when it knows, else from the
            // geometry (25 Oct 2026: the front list's assumed pitch drifted a row after a dozen steps).
            const ui::Item& list = items[cur];
            int row = static_cast<int>(lroundf((my - firstRow) / rowH));
            const int hovered = ui::ListHoveredRow(list.ctl);
            if (hovered >= 0)
            {
                const u32 bar = uiclick::FindVisible(exe::VtFrontList.is(Mem<u32>(list.ctl)) ? "CharScroll" : "LoadSave_Scroll");
                const int first = bar && exe::ScrollBarClick.is(reinterpret_cast<u32>((*reinterpret_cast<void***>(bar))[0x4C / 4]))
                                      ? static_cast<int>(Mem<float>(bar + 0x3C8) + 0.5f)
                                      : 0;
                if (hovered >= first)
                    row = hovered - first;
            }
            row += static_cast<int>(dy);
            const float ny = firstRow + row * rowH;
            if (row >= 0 && ny < list.y + list.hh)
            {
                dimouse::MoveTo(mx, ny);
                g_snapHold = 0.12f;
                return;
            }
        }
        const uintptr_t exclude = cur >= 0 ? items[cur].ctl : 0;
        int next = ui::FindInDirection(items, mx, my, dx, dy, exclude);
        // Lists made of row controls with their own scroll bar (the dealers' ItemButton/ItemView rows + Inventory_Scroll,
        // all children of the dealer list): up/down past the edge row scrolls the list one row, natively
        // (uiclick::Scroll), and the cursor stays on the edge row, which now shows the next item; once the bar stops
        // moving, focus leaves the list as usual. Was: the D-pad could not reach dealer rows below the first five.
        if (cur >= 0 && dy != 0)
        {
            const u32 parent = Mem<u32>(items[cur].ctl + 4);
            const auto sibling = [&](int i) { return i >= 0 && Mem<u32>(items[i].ctl + 4) == parent; };
            const auto isBar = [&](int i)
            {
                return exe::ScrollBarClick.is(
                    reinterpret_cast<u32>((*reinterpret_cast<void***>(items[i].ctl))[0x4C / 4]));
            };
            int bar = -1;
            for (int i = 0; parent && i < static_cast<int>(items.size()) && bar < 0; ++i)
                if (i != cur && sibling(i) && isBar(i))
                    bar = i;
            // the edge row: no row of this list (a sibling that is not the bar) lies further in that direction (was: the
            // control spatial navigation picked next - going up that could be another control, and Up scrolled a
            // row early, 20 Oct 2026)
            bool edge = bar >= 0 && !isBar(cur);
            for (int i = 0; edge && i < static_cast<int>(items.size()); ++i)
                if (i != cur && sibling(i) && !isBar(i) && (items[i].y - my) * dy > 4.0f)
                    edge = false;
            if (edge)
            {
                const float before = Mem<float>(items[bar].ctl + 0x3C8);
                if (uiclick::Scroll(items[bar].ctl, dy, false, nullptr, nullptr) != before)
                    return;
            }
        }
        if (next < 0 && cur < 0 && !items.empty())
            next = 0; // nothing focused yet: start somewhere
        if (next >= 0)
        {
            SnapTo(items[next]);
        }
    }
}

// ---------------------------------------------------------------- press ownership (core\press_gate.h)
// A press belongs to the consumer that had the pad when it went down (the owner's attended run, 26 Oct 2026: the B that
// closed FLUF's window / palette was also `pad B free_flight tap`; likewise the A that closed a game popup, and an A held
// in flight that became a mouse button on the window that opened under it). One gate per consumer: the flight dispatch,
// the pad on FLUF's windows, the menu / window cursor. Inputs = the buttons in glyph order plus the sticks (LS / RS
// deflected past their dead zone).
static PressGate<glyphs::COUNT> g_gateFlight, g_gateFluf, g_gateUi;
static void PadInputs(bool in[glyphs::COUNT], const Gamepad& pad = g_pad);

// ---------------------------------------------------------------- free cursor (menus, bases, windows)
static void UpdateFreeCursor(float dt)
{
    bool in[glyphs::COUNT];
    PadInputs(in, g_menuPad);
    g_gateUi.Take(in); // a press another consumer owns (flight's held A, the B that closed a pad menu) is no click here
    const PadState& p = g_pad.Cur();
    const float mag = in[glyphs::LS] ? sqrtf(p.lx * p.lx + p.ly * p.ly) : 0.0f;
    if (mag > 0 && padbases::CursorAllowed()) // (D-BASES: pad_cursor 0 = focus only)
    {
        // Quadratic response: precise near the center, fast at full deflection.
        const float speed = g_opt.cursorSpeed * fl::ScreenHeight() * mag * mag;
        float dx = p.lx / mag * speed * dt, dy = -p.ly / mag * speed * dt;
        padbases::ShapeCursor(dx, dy); // cursor_magnet: slows over and pulls toward the nearest clickable element
        dimouse::MoveBy(dx, dy);
        g_snapHold = 0;
    }

    // (D-BASES: a button the pad screen consumes this frame is not also a mouse button / the Escape key)
    dimouse::SetButton(0, in[glyphs::A] && !padbases::Owns(XINPUT_GAMEPAD_A));
    dimouse::SetButton(1, in[glyphs::X] && !padbases::Owns(XINPUT_GAMEPAD_X));

    // Right stick vertical scrolls lists.
    if (fabsf(p.ry) > 0 && in[glyphs::RS])
    {
        g_wheelAccum += p.ry * g_opt.wheelRate * dt;
        const int notches = static_cast<int>(g_wheelAccum);
        if (notches)
        {
            dimouse::Wheel(notches);
            g_wheelAccum -= notches;
        }
    }
    else
        g_wheelAccum = 0;

    if (g_menuPad.Pressed(XINPUT_GAMEPAD_B) && !padbases::Owns(XINPUT_GAMEPAD_B))
        fl::TapKey(VK_ESCAPE);
}

// ---------------------------------------------------------------- FLUF's windows on the pad (flufaware.h)
// FLUF's ImGui reads the game's own cursor position and buttons (fluf.md "FLUF in play" B), so the pad drives its
// windows the way it drives the game's: the left stick moves the cursor (whatever pad_cursor says: FLUF's windows have no
// D-pad focus), A / X are the mouse buttons, the right stick the wheel, B the Escape key (FLUF closes its topmost window
// on it; the palette clears its text first). The palette is a list under a text box: the D-pad steps through it and A
// runs the selected command (its own keys, sent through the game's key handler, which FLUF feeds to ImGui); typing stays
// with the keyboard. The prompt bar says what the buttons do. Nothing of FLvibed's flight / menu dispatch runs meanwhile.
static bool g_flufPad = false;
// B on a window FLUF's Escape does not reach (a command's or a module's ImGui window: ImGui::Begin with a close button;
// FLUF's UiManager::ProcessEscapeKey closes only its own FlWindows - the owner's attended run 26 Oct 2026, a palette
// command's window stayed open under B): the game's cursor FLUF reads goes to the window's close button, the left button
// goes down and up there (ImGui's CloseButton acts on the release over it), the cursor stays. Every step capped in frames.
struct FlufClose
{
    int step = 0; // 0 idle, 1 moving, 2 down, 3 up (waiting for the window to go)
    int frames = 0;
    float x = 0, y = 0;
    std::string name;
};
static FlufClose g_flufClose;
static void FlufCloseStep()
{
    FlufClose& c = g_flufClose;
    if (!c.step)
        return;
    ++c.frames;
    switch (c.step)
    {
    case 1: // the cursor on the button (exact at the game's next mouse poll; ImGui sees it at FLUF's next frame)
        dimouse::MoveTo(c.x, c.y);
        if ((fabsf(fl::MouseX() - c.x) < 2 && fabsf(fl::MouseY() - c.y) < 2 && c.frames >= 2) || c.frames > 30)
            c.step = 2, c.frames = 0;
        break;
    case 2: // held two frames (one FLUF ImGui frame sees it down)
        dimouse::SetButton(0, true);
        if (c.frames >= 3)
            c.step = 3, c.frames = 0;
        break;
    case 3:
        dimouse::SetButton(0, false);
        if (!flufaware::WindowOpen(c.name) || c.frames > 30)
        {
            const bool closed = !flufaware::WindowOpen(c.name);
            Log("fluf: B closed '%s' by its close button: %s", c.name.c_str(), closed ? "closed" : "still open");
            events::Push("fluf close %s %s", c.name.c_str(), closed ? "ok" : "failed");
            dimouse::ReleaseTarget();
            c = FlufClose();
        }
        break;
    }
}
static void UpdateFlufPad(float dt)
{
    g_flufPad = true;
    bool in[glyphs::COUNT];
    PadInputs(in, g_menuPad);
    g_gateFluf.Take(in); // a press flight / a popup owns (A held for formation, the stick flying) is not FLUF's
    if (g_flufClose.step) // a close by the button runs: the pad waits for it (a few frames)
    {
        FlufCloseStep();
        return;
    }
    const PadState& p = g_pad.Cur();
    const float mag = in[glyphs::LS] ? sqrtf(p.lx * p.lx + p.ly * p.ly) : 0.0f;
    if (mag > 0)
    {
        const float speed = g_opt.cursorSpeed * fl::ScreenHeight() * mag * mag;
        dimouse::MoveBy(p.lx / mag * speed * dt, -p.ly / mag * speed * dt);
    }
    const bool palette = flufaware::PaletteOpen() && flufaware::TextInput();
    if (palette)
    {
        if (g_pad.Pressed(XINPUT_GAMEPAD_DPAD_UP))
            fl::TapKey(VK_UP);
        if (g_pad.Pressed(XINPUT_GAMEPAD_DPAD_DOWN))
            fl::TapKey(VK_DOWN);
        if (g_menuPad.Pressed(XINPUT_GAMEPAD_A))
            fl::TapKey(VK_RETURN);
        dimouse::SetButton(0, false);
    }
    else
        dimouse::SetButton(0, in[glyphs::A]);
    dimouse::SetButton(1, in[glyphs::X]);
    if (fabsf(p.ry) > 0 && in[glyphs::RS])
    {
        g_wheelAccum += p.ry * g_opt.wheelRate * dt;
        const int notches = static_cast<int>(g_wheelAccum);
        if (notches)
        {
            dimouse::Wheel(notches);
            g_wheelAccum -= notches;
        }
    }
    else
        g_wheelAccum = 0;
    if (g_menuPad.Pressed(XINPUT_GAMEPAD_B))
    {
        bool byEscape = true;
        float fx = -1, fy = -1;
        std::string name;
        if (flufaware::CloseTarget(byEscape, fx, fy, name) && !byEscape)
        {
            g_flufClose.step = 1, g_flufClose.frames = 0;
            g_flufClose.x = fx * fl::ScreenWidth(), g_flufClose.y = fy * fl::ScreenHeight();
            g_flufClose.name = name;
            dimouse::SetButton(0, false);
            FlufCloseStep();
        }
        else
            fl::TapKey(VK_ESCAPE); // FLUF's own windows: its Escape closes the topmost (the palette clears first)
    }
    if (palette)
    {
        const promptbar::Prompt items[] = {{glyphs::A, L"RUN"}, {glyphs::DU, L"SELECT", false, glyphs::DD},
                                           {glyphs::B, L"CLEAR / CLOSE"}};
        promptbar::Set("fluf.palette", items, 3, true, 10);
    }
    else
    {
        const promptbar::Prompt items[] = {{glyphs::A, L"CLICK"}, {glyphs::LS, L"CURSOR"}, {glyphs::B, L"CLOSE"}};
        promptbar::Set("fluf.window", items, 3, true, 10);
    }
}

// The pad leaves FLUF's windows: no mouse button stays down.
static void EndFlufPad()
{
    if (!g_flufPad)
        return;
    g_flufPad = false;
    dimouse::SetButton(0, false);
    dimouse::SetButton(1, false);
    if (g_flufClose.step) // the window went (its close took): the sequence ends here
    {
        Log("fluf: B closed '%s' by its close button: closed", g_flufClose.name.c_str());
        events::Push("fluf close %s ok", g_flufClose.name.c_str());
        dimouse::ReleaseTarget();
        g_flufClose = FlufClose();
    }
}

// ---------------------------------------------------------------- flight
static float Curve(float v, float expo)
{
    const float a = fabsf(v);
    return (v < 0 ? -1.0f : 1.0f) * powf(a, expo);
}

static float g_lastSteerX = 0, g_lastSteerY = 0;
float LastSteerX() { return g_lastSteerX; }
float LastSteerY() { return g_lastSteerY; }

// ---------------------------------------------------------------- pad aiming
// Right stick -> steering (Freelancer mouse-flight units) and aim cursor (pixels), including the
// aim assist. Shared by the real pad and the "sloppy pilot" test bot, so assists can be measured.
//
// Aim assist (hostile targets only), the three classic controller mechanisms:
//  - slowdown: steering sensitivity drops close to the target's lead cross;
//  - magnetism: the aim cursor (guns fire at it) is pulled onto the lead cross;
//  - rotational assist: while the player is actively steering towards the target, a small
//    capped steering correction closes the remaining error ("small corrections in a dogfight");
//    nothing at rest and nothing when steering away, so the player still has to keep up.
struct PadAimResult
{
    float sx = 0, sy = 0;     // steering
    float aimX = 0, aimY = 0; // aim cursor
};

static PadAimResult PadAimRaw(float rx, float ry);

// Low-pass filter on the aim cursor (frame-to-frame wobble of the lead cross at close range, and the
// jump between screen center and the cross in gimbal mode).
static bool g_aimFilterInit = false;
static float g_aimFX = 0, g_aimFY = 0;

static PadAimResult PadAim(float rx, float ry, float dt)
{
    PadAimResult r = PadAimRaw(rx, ry);
    if (!g_aimFilterInit || g_opt.aimSmooth <= 0)
    {
        g_aimFX = r.aimX;
        g_aimFY = r.aimY;
        g_aimFilterInit = true;
    }
    else
    {
        const float a = 1.0f - expf(-dt / g_opt.aimSmooth);
        g_aimFX += (r.aimX - g_aimFX) * a;
        g_aimFY += (r.aimY - g_aimFY) * a;
    }
    r.aimX = g_aimFX;
    r.aimY = g_aimFY;
    return r;
}

static PadAimResult PadAimRaw(float rx, float ry)
{
    PadAimResult r;
    const float w = static_cast<float>(fl::ScreenWidth()), h = static_cast<float>(fl::ScreenHeight());

    // Steering through Freelancer's own mouse-flight code. The response curve applies to the
    // stick's magnitude so diagonals keep their direction and strength.
    const float mag = sqrtf(rx * rx + ry * ry);
    if (mag > 0)
    {
        const float k = Curve(std::min(mag, 1.0f), g_opt.steerExpo) * g_opt.steerScale / mag;
        r.sx = rx * k;
        r.sy = ry * k;
    }
    if (g_opt.invertY)
        r.sy = -r.sy;

    if (g_opt.aimMode == 1)
    {
        // Gimbal mode: the stick only flies the ship. The aim cursor rests at the screen center and
        // snaps onto a hostile target's lead cross while it is inside the auto-aim cone; the guns'
        // own gimbal does the fine aiming. The player's job is keeping the enemy inside the cone.
        r.aimX = w * 0.5f;
        r.aimY = h * 0.5f;
        const aim::Target t = aim::Current();
        if (t.hostile && t.onScreen && aim::AngleFromCenter(t.screenX, t.screenY) <= g_opt.aimCone)
        {
            r.aimX = t.screenX;
            r.aimY = t.screenY;
        }
        aim::ClampToSafeArea(r.aimX, r.aimY);
        return r;
    }

    // Aim cursor: points where the stick steers, like mouse flight; full deflection reaches the
    // screen edge while the turn rate saturates at steer_scale. Centered at rest.
    r.aimX = w * 0.5f;
    r.aimY = h * 0.5f;
    if (g_opt.aimCursor && g_opt.steerScale > 0)
    {
        // The stick's range is a circle; stretch it to the screen rectangle so diagonals reach the
        // corners (radial stretch r / max(|x|,|y|); direction unchanged).
        float ux = r.sx / g_opt.steerScale, uy = r.sy / g_opt.steerScale;
        const float m = std::max(fabsf(ux), fabsf(uy));
        if (m > 1e-4f)
        {
            const float k = sqrtf(ux * ux + uy * uy) / m;
            ux = Clampf(ux * k, -1.0f, 1.0f);
            uy = Clampf(uy * k, -1.0f, 1.0f);
        }
        r.aimX += ux * w * 0.5f;
        r.aimY -= uy * h * 0.5f;
    }

    if (g_opt.aimAssist > 0 || g_opt.aimRotation > 0)
    {
        const aim::Target t = aim::Current();
        if (t.hostile && t.onScreen)
        {
            const float ex = t.screenX - r.aimX, ey = t.screenY - r.aimY; // cursor -> lead (pixels)
            const float d = sqrtf(ex * ex + ey * ey) / h;

            // Rotational assist: steering correction towards the lead cross, scaled by how actively
            // the player steers, capped to small corrections, none when steering away.
            if (g_opt.aimRotation > 0 && d < g_opt.aimRadius && mag > 0.1f)
            {
                // Steering offset that would move the cursor onto the lead cross.
                float cx = ex / (w * 0.5f) * g_opt.steerScale, cy = -ey / (h * 0.5f) * g_opt.steerScale;
                const float awayDot = r.sx * cx + r.sy * cy;
                const float steer = sqrtf(r.sx * r.sx + r.sy * r.sy);
                if (!(awayDot < 0 && steer > 0.2f))
                {
                    const float input = Clampf(mag / 0.35f, 0.0f, 1.0f);
                    cx *= g_opt.aimRotation * input;
                    cy *= g_opt.aimRotation * input;
                    const float len = sqrtf(cx * cx + cy * cy);
                    if (len > g_opt.aimRotationMax)
                    {
                        cx *= g_opt.aimRotationMax / len;
                        cy *= g_opt.aimRotationMax / len;
                    }
                    r.sx += cx;
                    r.sy += cy;
                }
            }
            // Slowdown only close to the cross, so a wide pull radius does not make the ship
            // sluggish whenever a hostile is on screen.
            if (g_opt.aimAssist > 0 && d < g_opt.aimSlowRadius)
            {
                const float slow = 1.0f - g_opt.aimSlowdown * (1.0f - d / g_opt.aimSlowRadius);
                r.sx *= slow;
                r.sy *= slow;
            }
            // Magnetism: the aim cursor is pulled onto the lead cross.
            if (g_opt.aimAssist > 0 && d < g_opt.aimRadius)
            {
                const float closeness = 1.0f - d / g_opt.aimRadius;
                const float pull = Clampf(closeness * 2.0f * g_opt.aimAssist, 0.0f, 1.0f);
                r.aimX += ex * pull;
                r.aimY += ey * pull;
            }
        }
    }
    aim::ClampToSafeArea(r.aimX, r.aimY);
    return r;
}

// Lead pursuit state (Pursue, below): shared by the test bot and the autopilot.
struct Pursuit
{
    bool reversing = false;  // reverse thrust engaged
    float reverseHold = 0;   // time before the forward/reverse state may switch again
    float lastOffset = 2.0f; // previous frame's lead offset (to see the target heading off screen)
    float phase = 0;         // corkscrew angle (weave)
};
struct PursuitOut
{
    float sx = 0, sy = 0; // steering (SetSteer)
    float ax = 0, ay = 0; // aim cursor (pixels)
    bool aligned = false; // the cursor sits on the lead cross near the nose: the guns hit
    float offset = 2.0f;  // lead cross distance from the screen centre (half-screen units); 2 = off screen
    bool reverse = false, burn = false;
    bool screenFrame = false; // sx / sy from the flight screen's offsets (its axes: fl::FlightScreenAxes), else the ship's
};

// ---------------------------------------------------------------- test bot
// Frame-rate combat bot for automated tests (debug command "bot fight|off|status"). Uses the
// same aim code as the pad's aim assist; only ever engages hostile targets.
struct Bot
{
    bool on = false;
    bool hadTarget = false;
    float retarget = 0;
    float time = 0, fireTime = 0, onTargetTime = 0, afterburnTime = 0, reverseTime = 0;
    float engagedTime = 0, distTime = 0; // average distance = distTime / engagedTime
    float outOfRange = 0;                // seconds the current target has been out of gun range
    Pursuit pursuit;                     // lead pursuit and range keeping (Pursue)
    bool sloppy = false;                 // "sloppy pilot" mode: steers through the pad pipeline (see UpdateBot)
    float stickX = 0, stickY = 0;
    struct Sample
    {
        float t, x, y;
    };
    std::deque<Sample> history; // lead offsets for the pilot's reaction delay
    u32 targetsLost = 0, targetsGivenUp = 0;
    std::map<uintptr_t, float> ignored; // given-up targets -> bot time when they may be engaged again
};
static Bot g_bot;

// Chase limits: targets that flee (e.g. in cruise) are given up instead of chased forever.
constexpr float kBotGiveUpOutOfRange = 8.0f;  // s out of gun range
constexpr float kBotGiveUpDistance = 2500.0f; // m
constexpr float kBotIgnoreTime = 30.0f;       // s before a given-up target may be engaged again

// ---------------------------------------------------------------- lead pursuit (bot + autopilot)
// Flies like a mouse player: the nose (and the aim cursor) on the lead cross; range keeping modelled on a human
// player (CombatTest.mp4, mission 1 vs Rogue fighters): the throttle is binary - full forward or full reverse,
// switched often - never partial. Fighters (radius ~10 m) are fought at ~40-110 m; reverse is held when close and
// through sharp turns (it tightens the turn), afterburner closes gaps beyond the close band when the target is ahead.
// weave (0..1): evasion blended into the attack - the nose circles the lead cross (a corkscrew flight path, hard to
// hit) while the reticle stays on the cross and the gimbaled guns keep firing; the circle stays inside the fire cone.
static PursuitOut Pursue(const aim::Target& t, float dt, Pursuit& p, float closeBand = 0, float fireCone = 0.3f,
                         float gunRange = 0, float weave = 0, float weaveRadius = 0.6f)
{
    PursuitOut o;
    const float w = static_cast<float>(fl::ScreenWidth()), h = static_cast<float>(fl::ScreenHeight());
    o.ax = w * 0.5f;
    o.ay = h * 0.5f;
    p.phase += dt * (3.0f + 3.0f * weave); // ~0.5-1 turn per second
    if (t.onScreen)
    {
        float ox = (t.screenX - w * 0.5f) / (w * 0.5f), oy = -(t.screenY - h * 0.5f) / (h * 0.5f);
        // Steer at a point circling the lead (radius up to weaveRadius of the fire cone): the guns' gimbal covers it.
        const float r = Clampf(weave, 0, 1) * fireCone * weaveRadius;
        const float steerX = ox + r * cosf(p.phase), steerY = oy + r * sinf(p.phase);
        o.sx = Clampf(steerX * 1.5f, -g_opt.steerScale, g_opt.steerScale);
        o.sy = Clampf(steerY * 1.5f, -g_opt.steerScale, g_opt.steerScale);
        o.screenFrame = true;
        o.ax = t.screenX;
        o.ay = t.screenY;
        aim::ClampToSafeArea(o.ax, o.ay);
        // The guns reach the cursor only within their gimbal range: fire near the nose.
        o.offset = sqrtf(ox * ox + oy * oy);
        o.aligned = fabsf(o.ax - t.screenX) < 2 && fabsf(o.ay - t.screenY) < 2 && o.offset < fireCone;
    }
    else
        aim::TurnTowards(t.world, g_opt.steerScale, o.sx, o.sy);
    float closeDist = closeBand > 0 ? closeBand : std::max(45.0f, t.radius * 2.5f + 20.0f);
    if (closeBand <= 0 && gunRange > 0)
        closeDist = std::min(closeDist, gunRange * 0.5f); // big targets: still well inside the guns' reach
    const float farDist = closeDist + 60.0f;
    // Reverse early when the target is about to leave the screen (lead cross far off-center and still moving
    // outwards), not only once it is gone: the tighter turn keeps it in view.
    const float offsetRate = dt > 0 ? (o.offset - p.lastOffset) / dt : 0.0f;
    p.lastOffset = o.offset;
    const bool leaving = o.offset > 0.4f && offsetRate > 0.2f;
    const bool sharpTurn = o.offset > 0.6f || leaving;
    // Big targets (radius > 50 m: stations, generators, capital ships) are braked for by the closing speed too - 1.2 s
    // of it beyond the band (M12 22 Oct 2026: the run at a Nomad shield generator closed at ~200 m/s, reverse began
    // 60 m out, the ship rammed it and died at full hull).
    // Fighters: half a second of a fast close (> 80 m/s) beyond the band (M13 22 Oct 2026: a head-on close on a Nomad
    // fighter at ~100 m, the ship died outright at 94 % hull as the fighter blew up - a collision).
    float brakeMargin = t.radius > 50.0f    ? std::max(0.0f, t.closing) * 1.2f
                        : t.closing > 80.0f ? t.closing * 0.5f
                                            : 0.0f;
    // ...by the flight model's own braking distance where it is known (s9m, M12 gen 3: 1.2 s of 170 m/s = 204 m - full
    // reverse took the ship to 20 m/s in 0.5 s, 200 m short; then the afterburner again: three brake-and-burn cycles in
    // the last 300 m to every generator): 1.5x the stop + 40 m, never less than the old 60 m of the 22 Oct ramming
    if (t.radius > 50.0f && t.closing > 0)
    {
        flight::Model fm;
        if (flight::ReadModel(fm))
            brakeMargin =
                std::min(brakeMargin, std::max(60.0f, flight::BrakeDistance(fm, t.closing, 0) * 1.5f + 40.0f));
    }
    bool reverse =
        t.dist < closeDist || (sharpTurn && t.dist < 250.0f) || (t.closing > 40.0f && t.dist < farDist + brakeMargin);
    // Minimum hold so the switching happens in bursts (the player toggles about once a second), not every frame.
    p.reverseHold -= dt;
    if (reverse != p.reversing && p.reverseHold <= 0)
    {
        p.reversing = reverse;
        p.reverseHold = 0.5f;
    }
    o.reverse = p.reversing;
    // Aggressive afterburner: burst whenever the target is beyond the close band and roughly ahead.
    o.burn = !o.reverse && o.offset < 0.45f && (t.dist > farDist || !t.inRange);
    return o;
}

static void UpdateBot(float dt)
{
    g_bot.time += dt;
    const float w = static_cast<float>(fl::ScreenWidth()), h = static_cast<float>(fl::ScreenHeight());
    const aim::Target t = aim::Current();
    const uintptr_t handle = fl::TargetHandle();
    for (auto it = g_bot.ignored.begin(); it != g_bot.ignored.end();)
        it = it->second <= g_bot.time ? g_bot.ignored.erase(it) : std::next(it);
    const bool ignored = handle && g_bot.ignored.count(handle);
    if (!t.valid || !t.hostile || ignored)
    {
        if (g_bot.hadTarget && !ignored)
        {
            ++g_bot.targetsLost;
            events::Push("bot target-lost (%u)", g_bot.targetsLost);
        }
        g_bot.hadTarget = false;
        g_bot.outOfRange = 0;
        Hold(fl::USER_FIRE_WEAPONS, false);
        Hold(fl::USER_AFTERBURN, false);
        Hold(fl::USER_MANEUVER_BRAKE_REVERSE, false);
        SetSteer(0, 0);
        dimouse::MoveTo(w * 0.5f, h * 0.5f);
        g_bot.retarget -= dt;
        if (g_bot.retarget <= 0)
        {
            if (t.valid && !t.hostile) // a friendly or neutral is selected: never shoot it
                Tap(fl::USER_CLEAR_TARGET);
            // Closest enemy first; if that one is being ignored, cycle through the others.
            Tap(ignored ? fl::USER_NEXT_ENEMY : fl::USER_CLOSEST_ENEMY);
            g_bot.retarget = ignored ? 0.15f : 0.5f;
        }
        return;
    }
    if (!g_bot.hadTarget)
        events::Push("bot target-acquired dist=%.0f attitude=%.2f radius=%.0f", t.dist, t.attitude, t.radius);
    g_bot.hadTarget = true;

    g_bot.outOfRange = t.inRange ? 0 : g_bot.outOfRange + dt;
    if (g_bot.outOfRange > kBotGiveUpOutOfRange || t.dist > kBotGiveUpDistance)
    {
        g_bot.ignored[handle] = g_bot.time + kBotIgnoreTime;
        ++g_bot.targetsGivenUp;
        events::Push("bot target-given-up dist=%.0f out-of-range=%.1fs", t.dist, g_bot.outOfRange);
        g_bot.hadTarget = false;
        g_bot.retarget = 0;
        return;
    }

    // Steer like a mouse player: aim cursor on the lead cross, ship turns towards the cursor (Pursue).
    const PursuitOut po = Pursue(t, dt, g_bot.pursuit);
    float sx = po.sx, sy = po.sy, ax = po.ax, ay = po.ay;
    bool aligned = po.aligned;
    if (g_bot.sloppy)
    {
        // "Sloppy pilot": steers through the pad pipeline (curve, cursor, aim assist) like a gentle
        // human - reacts 0.25 s late, under-corrects by 20 %, moves the stick at hand speed, with
        // noise - so the assists can be measured. Fires only when the aim cursor is on the cross.
        float ux = 0, uy = 0; // where the lead cross is, in cursor units (-1..1)
        if (t.onScreen)
        {
            ux = (t.screenX - w * 0.5f) / (w * 0.5f);
            uy = -(t.screenY - h * 0.5f) / (h * 0.5f);
        }
        else if (g_opt.steerScale > 0)
        {
            ux = sx / g_opt.steerScale; // full turn towards the target
            uy = sy / g_opt.steerScale;
        }
        g_bot.history.push_back({g_bot.time, ux, uy});
        while (g_bot.history.size() > 1 && g_bot.history[1].t <= g_bot.time - 0.25f)
            g_bot.history.pop_front();
        const float tx = g_bot.history.front().x * 0.8f, ty = g_bot.history.front().y * 0.8f;
        // Cursor offset -> stick deflection: |u| = curve(|stick|) = |stick|^expo.
        const float um = sqrtf(tx * tx + ty * ty);
        float stx = 0, sty = 0;
        if (um > 1e-4f)
        {
            const float sm = powf(std::min(um, 1.0f), 1.0f / g_opt.steerExpo);
            stx = tx / um * sm;
            sty = ty / um * sm;
        }
        if (g_opt.invertY)
            sty = -sty;
        const float step = 4.0f * dt; // hand speed: full stick in 0.25 s
        g_bot.stickX += Clampf(stx - g_bot.stickX, -step, step);
        g_bot.stickY += Clampf(sty - g_bot.stickY, -step, step);
        const float nx = (rand() / static_cast<float>(RAND_MAX) - 0.5f) * 0.08f;
        const float ny = (rand() / static_cast<float>(RAND_MAX) - 0.5f) * 0.08f;
        const PadAimResult a = PadAim(Clampf(g_bot.stickX + nx, -1, 1), Clampf(g_bot.stickY + ny, -1, 1), dt);
        sx = a.sx;
        sy = a.sy;
        ax = a.aimX;
        ay = a.aimY;
        aligned =
            t.onScreen && sqrtf((ax - t.screenX) * (ax - t.screenX) + (ay - t.screenY) * (ay - t.screenY)) < 0.02f * h;
    }
    SetSteer(sx, sy);
    g_lastSteerX = sx;
    g_lastSteerY = sy;
    dimouse::MoveTo(ax, ay);

    // Fire discipline: only when the cursor sits on the lead cross and the target is in range.
    const bool fire = aligned && t.inRange;
    Hold(fl::USER_FIRE_WEAPONS, fire);
    if (fire)
        g_bot.fireTime += dt;
    if (aligned)
        g_bot.onTargetTime += dt;
    // Range keeping (Pursue): binary throttle, reverse when close / turning sharply, afterburner to close gaps.
    const bool reverse = po.reverse, burn = po.burn;
    Hold(fl::USER_MANEUVER_BRAKE_REVERSE, reverse);
    Hold(fl::USER_AFTERBURN, burn);
    if (!reverse)
        fl::SetThrottle(1.0f);
    if (burn)
        g_bot.afterburnTime += dt;
    if (reverse)
        g_bot.reverseTime += dt;
    g_bot.distTime += t.dist * dt;
    g_bot.engagedTime += dt;
}

static std::string BotStats()
{
    char buf[300];
    sprintf_s(buf,
              "time=%.1f on-target=%.1f firing=%.1f afterburn=%.1f reverse=%.1f avg-dist=%.0f targets-lost=%u "
              "given-up=%u",
              g_bot.time, g_bot.onTargetTime, g_bot.fireTime, g_bot.afterburnTime, g_bot.reverseTime,
              g_bot.engagedTime > 0 ? g_bot.distTime / g_bot.engagedTime : 0.0f, g_bot.targetsLost,
              g_bot.targetsGivenUp);
    return buf;
}

static void StopBot(const char* why)
{
    if (!g_bot.on)
        return;
    g_bot.on = false;
    ReleaseAll();
    events::Push("bot stopped (%s) %s", why, BotStats().c_str());
}

std::string BotCommand(std::istream& in)
{
    std::string what;
    in >> what;
    if (what == "fight" || what == "sloppy")
    {
        g_bot = Bot();
        g_bot.on = true;
        g_bot.sloppy = what == "sloppy";
        events::Push("bot started (%s)", what.c_str());
        return "bot " + what;
    }
    if (what == "off")
    {
        StopBot("command");
        return "bot off";
    }
    return std::string("bot ") + (g_bot.on ? "on " : "off ") + BotStats();
}

// ---------------------------------------------------------------- auto targeting (pad flight)
// auto_target 1: when the hostile target is destroyed (or lost), select the closest enemy;
// auto_target 2: also pick one up whenever nothing is targeted. A deliberate selection of a
// non-hostile (station, trade lane, friend) is never replaced; clearing the target with the D-pad
// pauses the automatic pick-up for 10 s.
struct AutoTarget
{
    bool hadHostile = false;
    float afterKill = -1;  // countdown to re-target after the hostile target vanished
    float acquire = 0;     // countdown to the next pick-up attempt with nothing targeted
    float manualPause = 0; // pick-up paused after the player cleared the target
};
static AutoTarget g_auto;

static void UpdateAutoTarget(float dt)
{
    if (g_opt.autoTarget <= 0)
        return;
    g_auto.manualPause -= dt;
    const aim::Target t = aim::Current();
    if (t.valid)
    {
        g_auto.hadHostile = t.hostile;
        g_auto.afterKill = -1;
        return;
    }
    if (g_auto.hadHostile)
    {
        g_auto.hadHostile = false;
        g_auto.afterKill = 0.4f; // let the explosion play
    }
    if (g_auto.afterKill > 0)
    {
        g_auto.afterKill -= dt;
        if (g_auto.afterKill <= 0)
        {
            Tap(fl::USER_CLOSEST_ENEMY);
            events::Push("auto-target after target lost");
        }
        return;
    }
    if (g_opt.autoTarget >= 2 && g_auto.manualPause <= 0)
    {
        g_auto.acquire -= dt;
        if (g_auto.acquire <= 0)
        {
            g_auto.acquire = 1.0f;
            Tap(fl::USER_CLOSEST_ENEMY);
        }
    }
}

// ---------------------------------------------------------------- select by firing
// fire_target: with nothing targeted, firing while the reticle is over a hostile or neutral ship
// selects that ship - through Freelancer's own click selection (one short left click; the cursor
// shape tells what is under the reticle: hostile/fire, neutral/fire_neutral; friendlies excluded).
static int g_selectClick = 0; // 1 = button pressed this frame, release next frame
static float g_selectCooldown = 0;

static void UpdateFireTarget(float dt, bool firing)
{
    g_selectCooldown -= dt;
    if (g_selectClick)
    {
        dimouse::SetButton(0, false);
        g_selectClick = 0;
        return;
    }
    if (!g_opt.fireTarget || !firing || g_selectCooldown > 0 || fl::TargetHandle())
        return;
    const char* c = fl::CursorName();
    if (_stricmp(c, "hostile") && _stricmp(c, "fire") && _stricmp(c, "neutral") && _stricmp(c, "fire_neutral"))
        return;
    dimouse::SetButton(0, true);
    g_selectClick = 1;
    g_selectCooldown = 0.5f;
    events::Push("fire-target select (%s under reticle)", c);
}

// ---------------------------------------------------------------- advanced autopilot
// Gimbal aim without the stick: the reticle locks onto a hostile target's lead cross inside the cone (the same rule
// as aim_mode 1), else rests at the screen centre.
static void GimbalAim(float& x, float& y)
{
    x = fl::ScreenWidth() * 0.5f;
    y = fl::ScreenHeight() * 0.5f;
    const aim::Target t = aim::Current();
    if (t.hostile && t.onScreen && aim::AngleFromCenter(t.screenX, t.screenY) <= g_opt.aimCone)
    {
        x = t.screenX;
        y = t.screenY;
    }
    aim::ClampToSafeArea(x, y);
}

// The torpedo hold fires what the torpedo hardpoint carries (UI_PLAN 6: "hold torpedo or cruise disruptor"; GitHub issue
// 7). The game launches the two by different commands: USER_LAUNCH_TORPEDOS passes over a disruptor (0x4DD4E0), so a
// ship with a disruptor and no torpedo gets the disruptor's command. A torpedo wins, as on HUD 2's row.
static bool DisruptorOnly()
{
    bool disruptor = false;
    for (const space::Weapon& w : space::PlayerWeapons())
    {
        if (w.cruiseDisruptor)
            disruptor = true;
        else if (_strnicmp(w.hardpoint, "HpTorpedo", 9) == 0)
            return false;
    }
    return disruptor;
}

static void HoldTorpedoSlot()
{
    const bool slot = g_dispatch.Held(presets::Action::Torpedo);
    const bool disruptor = slot && DisruptorOnly();
    Hold(fl::USER_LAUNCH_TORPEDOS, slot && !disruptor);
    Hold(fl::USER_LAUNCH_CRUISE_DISRUPTORS, disruptor || g_dispatch.Held(presets::Action::CruiseDisruptor));
}

// A stick's vertical value as what it points with: the camera and the cursor, which no Y inversion reaches by itself.
// The stick's steering is flipped by `invert_y` (PadAim) and by the game's own option (its steering code), so both
// count here and the two cancel as they do in flight (GitHub issue 6: turret view ignored Invert Y).
static float LookY(float y) { return g_opt.invertY != fl::GameInvertY() ? -y : y; }

// Runs the autopilot (autopilot.cpp decides, this applies its outputs). Returns true while it flies the ship.
// Turret view's look (the owner, 26 Oct 2026), the same whether the player or the autopilot flies: the reticle stays at
// the screen centre - gimbal mode's lock-on there as in flight (aim_mode 1: onto a hostile's lead cross inside the cone)
// - and the right stick turns the camera itself through the game's own look commands (USER_LOOK_ROTATE_CAMERA_*, held =
// the turret camera turns at its rate; 0x51A7D0 turns it by the cursor only while mouse flight is engaged, else by
// these - 0x51AC20 sets its yaw / pitch input). The commands are on / off: the stick's deflection is the share of frames
// they are held (an accumulator per axis), the camera's own smoothing (0x51AA47) evens it out. Out of turret view the
// look commands are let go (no-ops when not held). True = turret view (the cursor is placed). The steering is the
// caller's (the player's flight lets it go, the autopilot keeps steering).
static bool TurretLook(const PadState& p)
{
    static const int kLook[] = {fl::USER_LOOK_ROTATE_CAMERA_LEFT, fl::USER_LOOK_ROTATE_CAMERA_RIGHT,
                                fl::USER_LOOK_ROTATE_CAMERA_UP, fl::USER_LOOK_ROTATE_CAMERA_DOWN};
    if (!g_turret)
    {
        for (int c : kLook)
            Hold(c, false);
        return false;
    }
    auto pulse = [](float& acc, float v)
    {
        acc += fabsf(v);
        if (acc < 1)
            return false;
        acc -= 1;
        return true;
    };
    const bool onX = pulse(g_turretAimX, p.rx), onY = pulse(g_turretAimY, p.ry);
    if (p.rx == 0)
        g_turretAimX = 0;
    if (p.ry == 0)
        g_turretAimY = 0;
    Hold(fl::USER_LOOK_ROTATE_CAMERA_LEFT, onX && p.rx < 0);
    Hold(fl::USER_LOOK_ROTATE_CAMERA_RIGHT, onX && p.rx > 0);
    const float lookY = LookY(p.ry);
    Hold(fl::USER_LOOK_ROTATE_CAMERA_UP, onY && lookY > 0);
    Hold(fl::USER_LOOK_ROTATE_CAMERA_DOWN, onY && lookY < 0);
    float x = fl::ScreenWidth() * 0.5f, y = fl::ScreenHeight() * 0.5f;
    if (g_opt.aimMode == 1)
        GimbalAim(x, y);
    dimouse::MoveTo(x, y);
    return true;
}

static bool UpdateAutopilot(float dt)
{
    // When it stops, everything it held goes back first (was: pad flight re-holds only its own commands, so a slide
    // up / down or a missile hold of the last autopilot frame stayed held, 18 Oct 2026).
    static bool flew = false, wasFiring = false;
    const auto handBack = [&]
    {
        if (flew)
        {
            ReleaseAll(); // (the hand-back pulse: haptics.cpp, on the autopilot's end in flight)
            if (HostAtLeast(28) && g_host->SetGunAim)
                g_host->SetGunAim(false, nullptr); // the guns back to the player's cursor
        }
        flew = wasFiring = false;
        return false;
    };
    if (!autopilot::Active())
    {
        autopilot::TracePlayer(dt); // (the player's own flight in the same trace: the reference runs)
        return handBack();
    }
    PadState p = g_pad.Cur();
    if (g_gateFlight.Hidden(glyphs::LS)) // a stick still held from FLUF's cursor / a popup is not the player's steering
        p.lx = p.ly = 0;
    if (g_gateFlight.Hidden(glyphs::RS))
        p.rx = p.ry = 0;
    // The player's own steering takes the ship back at once (never re-issued by the autopilot): any stick past
    // autopilot_cancel - except the right stick in turret view or the ASSISTED preset, where it is the aim (UI_PLAN 6.4).
    const bool rightAims = g_turret || uiopt::Get().padPreset == presets::ASSISTED ||
                           wheel::IsOpen(); // (the open action wheel picks with the right stick: no hand-back by it)
    const float stick = std::max(rightAims ? 0.0f : sqrtf(p.rx * p.rx + p.ry * p.ry), sqrtf(p.lx * p.lx + p.ly * p.ly));
    if (g_opt.autopilotCancel > 0 && stick > g_opt.autopilotCancel)
    {
        autopilot::Stop("player input");
        return handBack();
    }
    autopilot::Out o;
    autopilot::Update(dt, o);
    if (!o.active)
        return handBack();
    flew = true;
    static Pursuit pursuit;
    static float fireHold = 0; // keeps the guns going through a one-frame loss of alignment
    bool fire = o.fire, reverse = false;
    // The coast (`big_coast`; the owner, 29 Oct 2026: "Killing the engine when in cruise engine only works when NO
    // additional engine-related input is sent ... no stopping, no throttle action. Just engine kill"): the steering
    // alone - no throttle, reverse, afterburner, slide or cruise toggle while it lasts (all released below)
    if (o.coast)
    {
        pursuit = Pursuit();
        float sx = 0, sy = 0;
        aim::SteerTowards(o.steerPos, g_opt.steerScale, 0.35f, sx, sy);
        float cam[9];
        if (o.steerLevel)
            aim::LevelSteer(sx, sy, nullptr, o.steerLevel == 2 && fl::FlightScreenAxes(cam) ? cam : nullptr);
        SetSteer(sx, sy);
        dimouse::MoveTo(fl::ScreenWidth() * 0.5f, fl::ScreenHeight() * 0.5f);
        o.afterburn = false, o.slide = 0, o.dodgeSlide = 0, o.cruise = -1;
    }
    else if (o.pursue)
    {
        // Lead pursuit (the bot's flight): the nose and the reticle on the lead cross; the guns fire on it. In the ship's
        // own frame (aim::Current(true)): the same flight whatever camera the player looks through.
        const aim::Target t = aim::Current(true);
        // the head-on pass's predictive turn: the nose at the point ahead of the target (the guns keep the lead
        // through the autopilot's gunAim / SetGunAim)
        aim::Target ts = t;
        float sp[3];
        if (o.steerAhead && fl::WorldToFlightScreen(o.steerAheadPos, sp))
        {
            ts.screenX = sp[0], ts.screenY = sp[1];
            ts.onScreen = sp[0] >= 0 && sp[1] >= 0 && sp[0] < fl::ScreenWidth() && sp[1] < fl::ScreenHeight();
            memcpy(ts.world, o.steerAheadPos, sizeof(ts.world));
        }
        PursuitOut po = Pursue(ts, dt, pursuit, o.pursueBand, o.fireCone, o.gunRange, o.weave, o.weaveRadius);
        float axes[9];
        const bool haveAxes = fl::FlightScreenAxes(axes);
        // (Pursue steers along the flight screen's axes - the camera's, or the ship's off screen; restated into the frame the
        // game turns in: `steer_level` 1 the level frame, 2 the camera's - measured in fights: the turn followed the screen)
        if (o.steerLevel)
            aim::LevelSteer(po.sx, po.sy, po.screenFrame && haveAxes ? axes : nullptr,
                            o.steerLevel == 2 && haveAxes ? axes : nullptr);
        // (debug `steer_check`: where the lead sits round the nose in the ship's frame vs where the nose turns, every
        // 0.25 s while it is 5+ deg off - the steering's frame measured in a fight)
        static DWORD checkAt = 0;
        fl::Kinematics kc = {};
        if (o.steerCheck && haveAxes && gameclock::NowMs() >= checkAt && fl::PlayerKinematics(kc))
        {
            checkAt = gameclock::NowMs() + 250;
            const float w[3] = {ts.world[0] - kc.pos[0], ts.world[1] - kc.pos[1], ts.world[2] - kc.pos[2]};
            float d[3];
            for (int i = 0; i < 3; ++i)
                d[i] = kc.rot[0 * 3 + i] * w[0] + kc.rot[1 * 3 + i] * w[1] + kc.rot[2 * 3 + i] * w[2];
            const float len = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
            const float off = len > 1 ? acosf(Clampf(-d[2] / len, -1, 1)) * 57.3f : 0;
            float spin[3] = {};
            if (off > 5 && space::PlayerSpin(spin))
            {
                // the camera's up in the ship's frame: its roll against the ship's
                const float cu[3] = {axes[0 * 3 + 1], axes[1 * 3 + 1], axes[2 * 3 + 1]};
                const float cx = kc.rot[0] * cu[0] + kc.rot[3] * cu[1] + kc.rot[6] * cu[2];
                const float cy = kc.rot[1] * cu[0] + kc.rot[4] * cu[1] + kc.rot[7] * cu[2];
                events::Push("autopilot steer-check off=%.0f phi=%.0f turn=%.0f w=%.2f bank=%.0f camroll=%.0f screen=%d level=%d", off,
                        atan2f(d[0], d[1]) * 57.3f, atan2f(-spin[1], spin[0]) * 57.3f,
                        sqrtf(spin[0] * spin[0] + spin[1] * spin[1]), geom::RollDeg(kc.rot), atan2f(cx, cy) * 57.3f,
                        po.screenFrame ? 1 : 0, o.steerLevel);
            }
        }
        SetSteer(po.sx, po.sy);
        g_lastSteerX = po.sx;
        g_lastSteerY = po.sy;
        dimouse::MoveTo(po.ax, po.ay);
        fireHold = po.aligned && t.inRange ? 0.15f : fireHold - dt;
        fire = fire && fireHold > 0;
        reverse = po.reverse;
        o.afterburn = o.afterburn || po.burn;
        if (o.turnManeuver) // the autopilot's turn maneuver decides the throttle (autopilot.cpp, turn_evade)
        {
            reverse = o.turnManeuver == 2;
            o.afterburn = !reverse;
        }
        if (!reverse)
            fl::SetThrottle(1.0f);
        fl::Kinematics k = {};
        if (o.station && fl::PlayerKinematics(k))
        {
            // Station keeping: a velocity toward the point that shrinks with the distance left (40 % of it per second,
            // at most 60 m/s along the nose, 40 sideways), reached by partial throttle / reverse along the nose and the
            // slide keys across it (the larger sideways error); no afterburner.
            const float err[3] = {o.stationPos[0] - k.pos[0], o.stationPos[1] - k.pos[1], o.stationPos[2] - k.pos[2]};
            auto axis = [&](int c, float sign, const float* v)
            { return sign * (v[0] * k.rot[0 * 3 + c] + v[1] * k.rot[1 * 3 + c] + v[2] * k.rot[2 * 3 + c]); };
            const float along = axis(2, -1, err), vAlong = axis(2, -1, k.vel);
            const float want = Clampf(along * 0.4f, -60.0f, 60.0f);
            reverse = want - vAlong < -5.0f;
            fl::SetThrottle(reverse ? 0.0f : Clampf((want - vAlong) / 40.0f, 0.0f, 1.0f));
            o.afterburn = false;
            const float right = axis(0, 1, err), vRight = axis(0, 1, k.vel);
            const float up = axis(1, 1, err), vUp = axis(1, 1, k.vel);
            const float dr = Clampf(right * 0.4f, -40.0f, 40.0f) - vRight, du = Clampf(up * 0.4f, -40.0f, 40.0f) - vUp;
            if (!o.dodgeSlide && !o.slide) // (a slide already set - the autopilot's wall slide - wins)
                o.slide = std::max(fabsf(dr), fabsf(du)) < 8.0f ? 0
                          : fabsf(dr) >= fabsf(du)              ? (dr > 0 ? 2 : 1)
                                                                : (du > 0 ? 3 : 4);
        }
    }
    else if (o.steerTo)
    {
        // Between targets: keep flying, nose towards the next one (never coast in a fight).
        pursuit = Pursuit();
        fireHold = 0;
        float sx = 0, sy = 0;
        if (o.steerFullAngle > 0)
            aim::SteerTowards(o.steerPos, g_opt.steerScale, o.steerFullAngle, sx, sy); // navigation: calm
        else
            aim::TurnTowards(o.steerPos, g_opt.steerScale, sx, sy);
        float cam[9];
        if (o.steerLevel) // (the ship's frame restated into the game's turn frame)
            aim::LevelSteer(sx, sy, nullptr, o.steerLevel == 2 && fl::FlightScreenAxes(cam) ? cam : nullptr);
        SetSteer(sx, sy);
        dimouse::MoveTo(fl::ScreenWidth() * 0.5f, fl::ScreenHeight() * 0.5f);
        fl::SetThrottle(o.steerThrottle);
        reverse = o.steerReverse;
    }
    else if (o.brake)
    {
        // Hold position (e.g. inside the tractor's reach): throttle 0, reverse while still moving.
        pursuit = Pursuit();
        fireHold = 0;
        SetSteer(0, 0);
        dimouse::MoveTo(fl::ScreenWidth() * 0.5f, fl::ScreenHeight() * 0.5f);
        fl::SetThrottle(0.0f);
        fl::Kinematics k = {};
        if (fl::PlayerKinematics(k))
            reverse = sqrtf(k.vel[0] * k.vel[0] + k.vel[1] * k.vel[1] + k.vel[2] * k.vel[2]) > 5.0f;
    }
    else
    {
        pursuit = Pursuit();
        fireHold = 0;
        ReleaseSteer(); // the game's behaviour steers
        float ax, ay;
        GimbalAim(ax, ay);
        dimouse::MoveTo(ax, ay);
    }
    // Turret view while the autopilot flies: the right stick turns the camera and the reticle stays centred, as in the
    // player's own flight (was: the cursor followed the stick - the turret block sat after this function's early
    // return, so the camera never turned and a look command held at the start stayed held). ASSISTED: the right stick
    // is the player's aim (the reticle follows the stick from the screen centre; the autopilot's gimbal aim yields
    // while the stick is deflected).
    const bool turret = TurretLook(p);
    // The guns in turret view (the owner, 28 Oct 2026: the player may look around while the autopilot keeps full
    // control): the game aims every gun along the cursor's ray through the current camera, which is now the player's -
    // so the autopilot hands the guns its own world point instead (host v28): the target's lead cross (aim::Current: the
    // game's lead when in range), else straight ahead of the nose. Out of turret view the game's own aim, by the cursor.
    if (HostAtLeast(28) && g_host->SetGunAim)
    {
        if (turret)
        {
            const aim::Target t = aim::Current(true);
            fl::Kinematics k = {};
            float ahead[3];
            const float* world = nullptr;
            if (t.valid && t.inRange)
                world = t.world;
            else if (fl::PlayerKinematics(k))
            {
                for (int i = 0; i < 3; ++i)
                    ahead[i] = k.pos[i] - k.rot[i * 3 + 2] * 1000.0f; // -z = forward (the rot's third column)
                world = ahead;
            }
            g_host->SetGunAim(world != nullptr, world);
        }
        else if (o.gunAim && fire) // the autopilot fires off the nose at a point some gun can reach (`fire_covering`)
            g_host->SetGunAim(true, o.gunAimPos);
        else
            g_host->SetGunAim(false, nullptr);
    }
    if (!turret && rightAims && (p.rx != 0 || p.ry != 0))
    {
        float ax = fl::ScreenWidth() * 0.5f + p.rx * fl::ScreenWidth() * 0.5f;
        float ay = fl::ScreenHeight() * 0.5f - LookY(p.ry) * fl::ScreenHeight() * 0.5f;
        aim::ClampToSafeArea(ax, ay);
        dimouse::MoveTo(ax, ay);
    }
    // Staggered fire (the player's trick, user 17 Oct 2026), for any loadout: a burst starts with the guns one by one,
    // evenly over the refire cycle (N active non-homing guns, the slowest refire r: gun k alone at k*r/N, by its
    // FIRE_WEAPON slot = its place in the ship's weapon list), then fire-all - the guns stay out of phase for the
    // whole burst, a steady stream of shots instead of volleys.
    static float staggerTime = 0, staggerStep = 0;
    static int staggerNext = 0, staggerCount = 0;
    static int staggerSlots[10];
    bool fireAll = fire;
    if (o.stagger && fire)
    {
        if (!wasFiring)
        {
            staggerTime = 0;
            staggerNext = 0;
            staggerCount = 0;
            float r = 0;
            const std::vector<space::Weapon> ws = space::PlayerWeapons();
            // By the game's fire slot (the HUD weapon list), never the list index: the index tapped the Javelin's slot
            // at every burst start (M02 jailbreak stray, traced 18 Oct 2026). A gun without a known slot is not staggered.
            for (size_t i = 0; i < ws.size() && staggerCount < 10; ++i)
                if (ws[i].active && !ws[i].homing && ws[i].refire > 0 && ws[i].fireSlot >= 1 && ws[i].fireSlot <= 10)
                {
                    staggerSlots[staggerCount++] = ws[i].fireSlot - 1;
                    r = std::max(r, ws[i].refire);
                }
            staggerStep = staggerCount > 1 ? r / staggerCount : 0.0f;
        }
        staggerTime += dt;
        if (staggerStep > 0 && staggerNext < staggerCount - 1)
        {
            while (staggerNext < staggerCount - 1 && staggerTime >= staggerNext * staggerStep)
                Tap(static_cast<fl::Cmd>(fl::USER_FIRE_WEAPON1 + staggerSlots[staggerNext++]));
            fireAll = false;
        }
        else
            fireAll = staggerTime >= (staggerCount - 1) * staggerStep;
    }
    wasFiring = fire;
    // The autopilot's "fire all" holds each gun's own fire slot, never the game's fire-all: that fires every weapon of
    // the active group, missile launchers included (measured 18 Oct 2026: 3 s of USER_FIRE_WEAPONS launched a Javelin -
    // the strays of M02's jailbreak fight). Only when a gun's slot is unknown does it fall back to fire-all.
    static unsigned heldSlots = 0;
    unsigned wantSlots = 0;
    bool slotsKnown = false;
    if (fireAll && !o.fireAllCmd)
    {
        slotsKnown = true;
        for (const space::Weapon& w : space::PlayerWeapons())
            if (!w.homing && w.ammo < 0 && w.refire > 0)
            {
                if (w.fireSlot < 1 || w.fireSlot > 10)
                    slotsKnown = false;
                else
                    wantSlots |= 1u << (w.fireSlot - 1);
            }
        if (!slotsKnown)
            wantSlots = 0;
    }
    for (int s = 0; s < 10; ++s)
        if ((heldSlots | wantSlots) & (1u << s))
            Hold(static_cast<fl::Cmd>(fl::USER_FIRE_WEAPON1 + s), (wantSlots & (1u << s)) != 0);
    heldSlots = wantSlots;
    // Manual weapons stay available: the player's fire and missile buttons (the flight_ap bindings) as always.
    Hold(fl::USER_FIRE_WEAPONS, (fireAll && !slotsKnown) || g_dispatch.Held(presets::Action::Fire));
    Hold(fl::USER_LAUNCH_MISSILES, g_dispatch.Held(presets::Action::Missile));
    HoldTorpedoSlot();
    if (o.missile)
        Tap(fl::USER_LAUNCH_MISSILES);
    if (o.torpedo)
        Tap(fl::USER_LAUNCH_TORPEDOS);
    if (o.mine)
        Tap(fl::USER_LAUNCH_MINES);
    if (o.engineKill)
        Tap(fl::USER_MANEUVER_ENGINEKILL);
    if (o.cm)
        Tap(fl::USER_LAUNCH_COUNTERMEASURES);
    if (o.closestEnemy)
        Tap(fl::USER_CLOSEST_ENEMY);
    if (o.repairHull)
        Tap(fl::USER_REPAIR_HEALTH); // the game's own repair command (the HUD's nanobot button)
    if (o.repairShield)
        Tap(fl::USER_REPAIR_SHIELD); // shield battery
    // Cruise by the engine's real state (CEEngine::IsCruiseEngaged: true from the request, charging or cruising): the
    // toggle only when it differs from what the autopilot wants, and not again before the game has run a few frames
    // since the last toggle (it shows in the engine's state by then).
    static u32 cruiseTapFrame = 0;
    if (o.cruise >= 0 && !o.coast && g_frame >= cruiseTapFrame + 3)
    {
        flight::Model fm;
        if (flight::ReadModel(fm) && fm.cruiseEngaged != (o.cruise == 1))
        {
            Tap(fl::USER_CRUISE);
            cruiseTapFrame = g_frame;
        }
    }
    if (o.collectAll)
        Tap(fl::USER_COLLECT_LOOT); // tractor beam, all items in reach
    if (o.dodgeSlide)
    {
        o.slide = o.dodgeSlide; // a missile dodge wins over the evasion's slide bursts
        o.afterburn = true;
    }
    Hold(fl::USER_AFTERBURN, o.afterburn || g_dispatch.Held(presets::Action::Afterburner));
    Hold(fl::USER_MANEUVER_BRAKE_REVERSE, reverse);
    Hold(fl::USER_REARVIEW_CAMERA_MODE, g_dispatch.Held(presets::Action::Rear) || wheel::RearView());
    Hold(fl::USER_MANEUVER_SLIDE_EVADE_LEFT, o.slide == 1);
    Hold(fl::USER_MANEUVER_SLIDE_EVADE_RIGHT, o.slide == 2);
    Hold(fl::USER_MANEUVER_SLIDE_EVADE_UP, o.slide == 3);
    Hold(fl::USER_MANEUVER_SLIDE_EVADE_DOWN, o.slide == 4);
    return true;
}

// ---------------------------------------------------------------- pad flight: the preset runner
// The pad's buttons in glyphs::Id order, as the dispatcher wants them. The triggers count as buttons past the pad's
// trigger threshold (PAD_LT / PAD_RT); the sticks are axes, never buttons.
static void PadButtons(bool down[glyphs::COUNT])
{
    memset(down, 0, glyphs::COUNT);
    down[glyphs::A] = g_pad.Down(XINPUT_GAMEPAD_A);
    down[glyphs::B] = g_pad.Down(XINPUT_GAMEPAD_B);
    down[glyphs::X] = g_pad.Down(XINPUT_GAMEPAD_X);
    down[glyphs::Y] = g_pad.Down(XINPUT_GAMEPAD_Y);
    down[glyphs::LB] = g_pad.Down(XINPUT_GAMEPAD_LEFT_SHOULDER);
    down[glyphs::RB] = g_pad.Down(XINPUT_GAMEPAD_RIGHT_SHOULDER);
    down[glyphs::LT] = g_pad.Down(PAD_LT);
    down[glyphs::RT] = g_pad.Down(PAD_RT);
    down[glyphs::L3] = g_pad.Down(XINPUT_GAMEPAD_LEFT_THUMB);
    down[glyphs::R3] = g_pad.Down(XINPUT_GAMEPAD_RIGHT_THUMB);
    down[glyphs::DU] = g_pad.Down(XINPUT_GAMEPAD_DPAD_UP);
    down[glyphs::DD] = g_pad.Down(XINPUT_GAMEPAD_DPAD_DOWN);
    down[glyphs::DL] = g_pad.Down(XINPUT_GAMEPAD_DPAD_LEFT);
    down[glyphs::DR] = g_pad.Down(XINPUT_GAMEPAD_DPAD_RIGHT);
    down[glyphs::VIEW] = g_pad.Down(XINPUT_GAMEPAD_BACK);
    down[glyphs::MENU] = g_pad.Down(XINPUT_GAMEPAD_START);
}

// The press gates' inputs: the buttons, and the sticks as "down" while deflected (gamepad.cpp's dead zone gives 0).
// `pad`: g_pad (positional: flight), or g_menuPad (the menus' view: A / B as accept / back).
static void PadInputs(bool in[glyphs::COUNT], const Gamepad& pad)
{
    PadButtons(in);
    in[glyphs::A] = pad.Down(XINPUT_GAMEPAD_A);
    in[glyphs::B] = pad.Down(XINPUT_GAMEPAD_B);
    const PadState& p = g_pad.Cur();
    in[glyphs::LS] = p.lx != 0 || p.ly != 0;
    in[glyphs::RS] = p.rx != 0 || p.ry != 0;
}

// The stick an axis action sits on: +1 right stick, -1 left stick, 0 unbound.
static int AxisStick(const presets::Table& t, presets::Action a)
{
    if (t.b[glyphs::RS].tap == a)
        return 1;
    if (t.b[glyphs::LS].tap == a)
        return -1;
    return 0;
}

// The phase D flight modals closed (a mode change, the device switching away, shutdown): the picker keeps the target it
// previewed (the game's own selection stands; nothing is restored behind the player's back).
static void CloseModals()
{
    picker::Close(true);
    wheel::Cancel();
    goals::Close();
    wgroups::Close();
}

static void ToggleAutopilot(const char* why)
{
    if (autopilot::Active())
        autopilot::Stop(why);
    else
    {
        autopilot::StartSmart(); // the plain start of every device (objective, waypoints, target)
        g_apUsed = true;
    }
}

void MarkAutopilotUsed() { g_apUsed = true; }
float StickDeadzone() { return g_pad.stickDeadzone; }
int PadBackend() { return g_pad.padBackend; }
bool FocusOnly() { return g_opt.focusOnly; }

// `context` (A): whatever applies to the target - dock / lane / jump when it is dockable (the game's dock maneuver,
// a toggle: a second press cancels a running dock), tractor ONE item when it is loot in the tractor's reach, with no
// target the nearest loot in reach (targeted, then tractored: the game's "Tractor Beam (Selected)"), else fly to it
// (goto; loot out of reach too). X tractors everything in reach (USER_COLLECT_LOOT). The HUDs name the item and its
// range (the owner, 4 Nov 2026: "Prompting should clarify which prompt does what (tractor specific, tractor all, etc)";
// until then A and X both tractored everything). The ASSISTED preset hands the whole thing to the autopilot.
static void ContextAction()
{
    if (uiopt::Get().padPreset == presets::ASSISTED)
    {
        if (!autopilot::Active())
            ToggleAutopilot("pad");
        return;
    }
    const u32 tid = fl::TargetId();
    ai::Self self;
    const bool haveSelf = ai::ReadSelf(self);
    const float reach = haveSelf ? self.tractorRange : 0;
    bool targetLoot = false, targetInReach = false, dockable = false;
    u32 nearestLoot = 0;
    float nearestD = 0;
    for (const space::Contact& c : space::Contacts())
    {
        // (in reach = the tractor can take it: in range and the hold has room for it)
        const bool loot = (c.type & space::kLoot) != 0, inReach = loot && reach > 0 && c.distance <= reach && c.lootFits != 0;
        if (c.id == tid && tid)
        {
            dockable = c.dock || (c.type & (space::kTradelaneRing | space::kJumpGate | space::kJumpHole)) != 0;
            targetLoot = loot, targetInReach = inReach;
        }
        if (inReach && (!nearestLoot || c.distance < nearestD))
            nearestLoot = c.id, nearestD = c.distance;
    }
    if (tid && dockable)
    {
        LockAnswer("dock");
        Tap(fl::USER_MANEUVER_DOCK);
    }
    else if (targetLoot && targetInReach)
        Tap(fl::USER_TRACTOR_BEAM);
    else if (!tid && nearestLoot)
    {
        const bool set = space::SetTarget(nearestLoot);
        Tap(set ? fl::USER_TRACTOR_BEAM : fl::USER_COLLECT_LOOT);
        events::Push("pad context: tractor the nearest loot %u (%.0f m)%s", nearestLoot, nearestD,
                     set ? "" : ": target refused, all loot instead");
    }
    else if (tid)
    {
        LockAnswer("goto");
        Tap(fl::USER_MANEUVER_GOTO);
    }
    else
        events::Push("pad context: nothing applies (no target, no loot in reach)");
}

// `target_smart` (RS tap): enemies first (the owner, 4 Nov 2026: "right stick should default to target nearest enemy
// for a first press, and only consecutive presses should be 'next target'") - with a hostile ship within
// tcycle::kReach, a cold press takes the nearest, presses in a run the next ones (target_cycle.h), set natively
// (space::SetTarget, filter-independent: the game's NEXT / CLOSEST ENEMY walk the contact list under its filter). With
// no enemy near, "TARGET AHEAD": the object nearest the screen centre inside HUD 2's reticle ring (the cone, the owner,
// 26 Oct 2026) - ships, loot, bases, rings, anything targetable; again = the next one ahead (the current target is
// skipped). Nothing in the ring: the game's nearest / next enemy. The double tap (clear) is the preset's.
// (Was: the game's click selection when the cursor's shape named an object exactly under the reticle - in calm flight
// that almost never held and the fallback found no enemy: "target ahead just doesn't work at all", the owner, 26 Oct
// 2026; then 26 Oct - 4 Nov the ring first, enemies only when the ring was empty.)
bool NearbyEnemy(const space::Contact& c)
{
    return c.ship && c.id && !c.player && c.health != 0 && c.attitudeAsked && c.attitude <= aim::kHostileAttitude &&
           c.distance < tcycle::kReach;
}

static void SmartTarget()
{
    static tcycle::Chain chain;
    const u32 cur = fl::TargetId();
    {
        tcycle::Enemy e[64];
        int n = 0;
        for (const space::Contact& c : space::Contacts())
            if (n < 64 && NearbyEnemy(c))
                e[n++] = {c.id, c.distance};
        if (n > 0)
        {
            const u32 pick = chain.Press(static_cast<float>(GetTickCount()) * 0.001f, e, n, cur);
            if (!pick)
                events::Push("pad target_smart: the only enemy near is the target");
            else if (space::SetTarget(pick))
                events::Push("pad target_smart: enemy %u (%d near)", pick, n);
            else
            {
                chain.Refused(cur);
                events::Push("pad target_smart: enemy %u refused", pick);
            }
            return;
        }
    }
    const float cx = fl::ScreenWidth() * 0.5f, cy = fl::ScreenHeight() * 0.5f, r = hud2::RingRadius();
    u32 best = 0;
    float bestD = r * r;
    for (const space::Contact& c : space::Contacts())
    {
        if (!c.id || c.id == cur || c.health == 0 || !(c.targetable || c.ship || c.dock || (c.type & space::kLoot)))
            continue;
        float s[3];
        if (!fl::WorldToScreen(c.pos, s) || s[2] <= 0)
            continue;
        const float d = (s[0] - cx) * (s[0] - cx) + (s[1] - cy) * (s[1] - cy);
        if (d < bestD)
            bestD = d, best = c.id;
    }
    if (best && space::SetTarget(best))
    {
        events::Push("pad target_smart: ahead %u (%.0f px from the centre, ring %.0f)", best, sqrtf(bestD), r);
        return;
    }
    const bool had = fl::TargetHandle() != 0;
    Tap(had ? fl::USER_NEXT_ENEMY : fl::USER_CLOSEST_ENEMY);
    events::Push("pad target_smart: %s", had ? "next enemy" : "closest enemy");
}

// One dispatched action in the game (the primitives: Tap / Hold with fl::USER_*, the autopilot API, the modals of
// phase D as events for now). Held-kind actions are read by the frame below through g_dispatch.Held().
static void Apply(const presets::Event& e)
{
    using presets::Action;
    using presets::Phase;
    const bool start = e.phase == Phase::Tap || e.phase == Phase::Dbl || e.phase == Phase::HoldStart;
    // hud_prompts (0.2.1): the context lanes show after a player action (core\game\promptshow.h)
    if (start)
        switch (e.action)
        {
        case Action::Context: case Action::FreeFlight: case Action::Formation: case Action::Goto:
        case Action::Autopilot: case Action::Handback: hud2::NoteAction(hudwake::Maneuver); break;
        case Action::EngineKill: case Action::Cruise: hud2::NoteAction(hudwake::Cruise); break;
        case Action::TargetSmart: case Action::TargetNearest: case Action::NextEnemy: case Action::ClearTarget:
            hud2::NoteAction(hudwake::TargetChg);
            break;
        case Action::Tractor: hud2::NoteAction(hudwake::Fire); break;
        default: break;
        }
    switch (e.action)
    {
    case Action::Context:
        if (start)
            ContextAction();
        break;
    case Action::FreeFlight: // the cancel button: free flight, and the cruise dropped (the game's free flight alone
        if (start)           // leaves the cruise engine engaged - verified 25 Oct 2026)
        {
            LockAnswer("free flight");
            Tap(fl::USER_MANEUVER_FREE_FLIGHT);
            flight::Model fm;
            if (flight::ReadModel(fm) && fm.cruiseEngaged)
                Tap(fl::USER_CRUISE);
        }
        break;
    case Action::Tractor:
        if (start)
            Tap(fl::USER_COLLECT_LOOT);
        break;
    case Action::Formation:
        if (start)
        {
            LockAnswer("formation");
            Tap(fl::USER_MANEUVER_FORMATION);
        }
        break;
    case Action::Goto:
        if (start)
        {
            LockAnswer("goto");
            Tap(fl::USER_MANEUVER_GOTO);
        }
        break;
    case Action::EngineKill:
        if (start)
            Tap(fl::USER_MANEUVER_ENGINEKILL);
        break;
    case Action::Cruise:
        if (start)
            Tap(fl::USER_CRUISE);
        break;
    case Action::Scan:
        if (start)
            Tap(fl::USER_SCAN_CARGO);
        break;
    case Action::TargetSmart:
        if (start)
            SmartTarget();
        break;
    case Action::TargetNearest:
        if (start)
            Tap(fl::USER_CLOSEST_ENEMY);
        break;
    case Action::NextEnemy:
        if (start)
            Tap(fl::USER_NEXT_ENEMY);
        break;
    case Action::ClearTarget:
        if (start)
        {
            Tap(fl::USER_CLEAR_TARGET);
            g_auto.manualPause = 10.0f;
            g_auto.hadHostile = false;
        }
        break;
    case Action::Autopilot:
        if (start)
            ToggleAutopilot("pad");
        break;
    case Action::Handback:
        if (start)
        {
            if (autopilot::Active())
                autopilot::Stop("pad");
            else
            {
                LockAnswer("free flight");
                Tap(fl::USER_MANEUVER_FREE_FLIGHT); // nothing to hand back: the cancel button's own meaning
            }
        }
        break;
    case Action::Hub:
        if (start)
            Tap(fl::USER_NAV_MAP); // the NeuroNet hub opens map first (its pad screen is phase D)
        break;
    case Action::Pause:
        if (start)
            Tap(fl::USER_PAUSE);
        break;
    case Action::Nanobot:
        if (start)
            Tap(fl::USER_REPAIR_HEALTH);
        break;
    case Action::Battery:
        if (start)
            Tap(fl::USER_REPAIR_SHIELD);
        break;
    case Action::Cm:
        if (start)
            Tap(fl::USER_LAUNCH_COUNTERMEASURES);
        break;
    case Action::Mine:
        if (start)
            Tap(fl::USER_LAUNCH_MINES);
        break;
    case Action::WeaponGroup: // weapon_groups 1: Y flips to the next of the three HUD groups that have guns
        if (start)
        {
            // (from the game's active group, not a count of our own: a load or a key may have changed it)
            const int active = hudinfo::ActiveWeaponGroup();
            const int next = wgroups::NextGroup(wgroups::GroupsWithGuns(), active >= 1 ? active : g_weaponGroup);
            if (next == active)
            {
                events::Push("pad weapon group: no other group has guns");
                break;
            }
            g_weaponGroup = next;
            Tap(static_cast<fl::Cmd>(fl::USER_WEAPON_GROUP1 + g_weaponGroup - 1));
            events::Push("pad weapon group %d", g_weaponGroup);
        }
        break;
    case Action::Turret: // the game's turret view (USER_AUTO_TURRET, H): each hold toggles it, it stays after the release
        if (e.phase == Phase::HoldStart) // (was on at the hold, off at the release: the owner, 26 Oct 2026, wants it to stay)
        {
            // the game's camera says whether it is on (OnFrame's sync, exe::CamTurret); without the symbol: our own flag
            if (!exe::CamTurret.ok())
                g_turret = !g_turret;
            g_turretAimX = g_turretAimY = 0;
            for (int c : {fl::USER_LOOK_ROTATE_CAMERA_LEFT, fl::USER_LOOK_ROTATE_CAMERA_RIGHT,
                          fl::USER_LOOK_ROTATE_CAMERA_UP, fl::USER_LOOK_ROTATE_CAMERA_DOWN})
            {
                fl::SetHeld(c, false); // the turret camera starts (and ends) without a turn input
                g_holding[c].down = false;
                fl::FireCommand(c, false);
            }
            Tap(fl::USER_AUTO_TURRET);
            events::Push("pad turret view toggle (was %s)", g_turret ? "on" : "off");
        }
        break;
    // phase D modals (picker.*, wheel.*, goals.*): LB tap the target picker, LB held the action wheel (its release does
    // the picked action; what needs this file's state - the weapon group - comes back as an action to apply), X the goal
    // panel while the autopilot flies
    case Action::TargetPicker:
        if (start)
            picker::Open();
        break;
    case Action::ActionWheel:
        if (e.phase == Phase::HoldStart)
            wheel::Open();
        else if (e.phase == Phase::HoldEnd)
        {
            const Action a = wheel::Commit();
            if (a != Action::None)
                Apply({e.btn, a, Phase::Tap});
        }
        break;
    case Action::GoalPanel:
        if (start)
            goals::Toggle();
        break;
    case Action::Accept:
        if (start)
            events::Push("ui accept (phase D: no suggestion to accept yet)");
        break;
    default: break; // held-kind actions (fire, missile, torpedo, afterburner, reverse, rear, cruise disruptor, groups)
    }
}

static void UpdateSpace(float dt)
{
    if (g_bot.on)
    {
        UpdateBot(dt);
        return;
    }
    const uiopt::UiOptions& o = uiopt::Get();
    const int preset = o.padPreset == presets::CUSTOM ? presets::FREELANCER : o.padPreset;
    g_dispatch.toggles = o.holdsAsToggles;
    // The situation's table: the preset's flight table, or the shared flight_ap one while the autopilot flies.
    const bool apFlying = autopilot::Active();
    g_table = apFlying ? presets::FlightAp() : presets::Effective(preset, o.weaponGroups, o.swapY);
    // Press ownership: what another consumer had when it went down (the B that closed FLUF's window, the A that closed a
    // popup, the stick that moved FLUF's cursor) stays hidden from the flight dispatch until released; a hold of the
    // dispatcher's own interrupted by another consumer is cancelled (no tap on its release).
    bool down[glyphs::COUNT], fresh[glyphs::COUNT];
    PadInputs(down);
    g_gateFlight.Take(down, fresh);
    for (int i = 0; i < glyphs::COUNT; ++i)
        if (fresh[i])
            g_dispatch.Cancel(static_cast<glyphs::Id>(i));
    PadState p = g_pad.Cur();
    if (g_gateFlight.Hidden(glyphs::LS))
        p.lx = p.ly = 0;
    if (g_gateFlight.Hidden(glyphs::RS))
        p.rx = p.ry = 0;
    if (g_gateFlight.Hidden(glyphs::LT))
        p.lt = 0;
    if (g_gateFlight.Hidden(glyphs::RT))
        p.rt = 0;
    down[glyphs::LS] = down[glyphs::RS] = false; // the sticks are axes, not the dispatcher's buttons
    // The phase D modals (picker / wheel / goal panel) read the buttons they own this frame; those are then hidden from
    // the flight dispatch until released (a B that closed the picker is not also free flight). The sticks and the
    // triggers stay live: the left stick flies, RT fires, the wheel owns the right stick (its steering is zeroed below).
    {
        static bool swallow[glyphs::COUNT] = {};
        bool owned[glyphs::COUNT];
        for (int i = 0; i < glyphs::COUNT; ++i)
        {
            const glyphs::Id id = static_cast<glyphs::Id>(i);
            owned[i] = picker::OwnsButton(id) || wheel::OwnsButton(id) || goals::OwnsButton(id) || wgroups::OwnsButton(id);
        }
        // the modals are menus: A / B as accept / back (confirm_button; both are owned alike, so the swallow below
        // stays positional and right)
        bool menu[glyphs::COUNT];
        memcpy(menu, down, sizeof(menu));
        menu[glyphs::A] = down[glyphs::Menu(glyphs::A)], menu[glyphs::B] = down[glyphs::Menu(glyphs::B)];
        picker::Update(dt, menu);
        wheel::Update(dt, menu, p.rx, p.ry);
        goals::Update(dt, menu);
        wgroups::Update(dt, menu);
        for (int i = 0; i < glyphs::COUNT; ++i)
        {
            if (down[i] && owned[i])
                swallow[i] = true;
            if (!down[i])
                swallow[i] = false;
            if (swallow[i])
                down[i] = false;
        }
    }
    // ACE: the triggers are the throttle lever (RT up, afterburner at full pull; LT down, reverse at full pull): axes,
    // not buttons
    const bool aceTriggers = g_table.b[glyphs::RT].tap == presets::Action::Throttle;
    bool aceBurn = false, aceReverse = false;
    if (aceTriggers)
    {
        down[glyphs::RT] = down[glyphs::LT] = false;
        if (p.rt > 0 || p.lt > 0)
            fl::SetThrottle(Clampf(fl::GetThrottle() + (p.rt - p.lt) * g_opt.throttleRate * dt, 0.0f, 1.0f));
        aceBurn = p.rt > 0.95f;
        aceReverse = p.lt > 0.95f;
    }
    static std::vector<presets::Event> events;
    events.clear();
    g_dispatch.Update(dt, g_table, down, events);
    for (const presets::Event& e : events)
    {
        if (e.action != presets::Action::Fire && e.action != presets::Action::Missile)
            Log("pad %s %s %s", glyphs::Name(e.btn), presets::ActionName(e.action),
                e.phase == presets::Phase::Tap         ? "tap"
                : e.phase == presets::Phase::Dbl       ? "dbl"
                : e.phase == presets::Phase::HoldStart ? "hold"
                                                       : "release");
        Apply(e);
    }

    if (UpdateAutopilot(dt))
        return;

    UpdateAutoTarget(dt);
    // Steering: the stick the preset steers with (RS in FREELANCER, LS in ACE; none in ASSISTED). In turret view the
    // right stick aims the camera's reticle instead (the ship flies straight on).
    const int steerStick = AxisStick(g_table, presets::Action::Steer);
    const int aimStick = AxisStick(g_table, presets::Action::Aim);
    const bool rsOwned = wheel::IsOpen(); // the open action wheel owns the right stick (it picks the slot)
    const float srx = steerStick > 0 ? (rsOwned ? 0.0f : p.rx) : steerStick < 0 ? p.lx : 0.0f;
    const float sry = steerStick > 0 ? (rsOwned ? 0.0f : p.ry) : steerStick < 0 ? p.ly : 0.0f;
    if (TurretLook(p)) // turret view: the camera turns, the reticle stays centred, the ship flies straight on
    {
        ReleaseSteer();
        g_lastSteerX = g_lastSteerY = 0;
    }
    else if (steerStick == 0)
    {
        ReleaseSteer();
        const float ax = aimStick < 0 ? p.lx : (aimStick > 0 || g_turret) ? p.rx : 0.0f;
        const float ay = aimStick < 0 ? p.ly : (aimStick > 0 || g_turret) ? p.ry : 0.0f;
        float x = fl::ScreenWidth() * 0.5f + ax * fl::ScreenWidth() * 0.5f;
        float y = fl::ScreenHeight() * 0.5f - LookY(ay) * fl::ScreenHeight() * 0.5f;
        aim::ClampToSafeArea(x, y);
        dimouse::MoveTo(x, y);
        g_lastSteerX = g_lastSteerY = 0;
    }
    else
    {
        const PadAimResult a = PadAim(srx, sry, dt);
        SetSteer(a.sx, a.sy);
        g_lastSteerX = a.sx;
        g_lastSteerY = a.sy;
        dimouse::MoveTo(a.aimX, a.aimY);
    }

    // The throttle lever on a stick's vertical axis (FREELANCER: LS), its horizontal axis the strafe.
    const int throttleStick = AxisStick(g_table, presets::Action::Throttle);
    const int strafeStick = AxisStick(g_table, presets::Action::StrafeUd);
    float strafeX = 0, strafeY = 0;
    if (throttleStick != 0)
    {
        const float ty = throttleStick > 0 ? p.ry : p.ly;
        if (ty != 0)
            fl::SetThrottle(Clampf(fl::GetThrottle() + ty * g_opt.throttleRate * dt, 0.0f, 1.0f));
        strafeX = throttleStick > 0 ? p.rx : p.lx;
    }
    if (strafeStick != 0)
    {
        strafeX = strafeStick > 0 ? p.rx : p.lx;
        strafeY = strafeStick > 0 ? p.ry : p.ly;
    }
    Hold(fl::USER_MANEUVER_SLIDE_EVADE_LEFT, strafeX < -0.6f);
    Hold(fl::USER_MANEUVER_SLIDE_EVADE_RIGHT, strafeX > 0.6f);
    Hold(fl::USER_MANEUVER_SLIDE_EVADE_UP, strafeY > 0.6f);
    Hold(fl::USER_MANEUVER_SLIDE_EVADE_DOWN, strafeY < -0.6f);

    // Held-kind actions as commands held
    const bool fire = g_dispatch.Held(presets::Action::Fire);
    Hold(fl::USER_FIRE_WEAPONS, fire);
    UpdateFireTarget(dt, fire || g_dispatch.Held(presets::Action::FireGroup1) ||
                             g_dispatch.Held(presets::Action::FireGroup2));
    HoldFireGroup(fl::USER_FIRE_WEAPON_GROUP1, g_dispatch.Held(presets::Action::FireGroup1));
    HoldFireGroup(fl::USER_FIRE_WEAPON_GROUP2, g_dispatch.Held(presets::Action::FireGroup2));
    Hold(fl::USER_LAUNCH_MISSILES, g_dispatch.Held(presets::Action::Missile));
    HoldTorpedoSlot();
    Hold(fl::USER_AFTERBURN, g_dispatch.Held(presets::Action::Afterburner) || aceBurn);
    Hold(fl::USER_MANEUVER_BRAKE_REVERSE, g_dispatch.Held(presets::Action::Reverse) || aceReverse);
    Hold(fl::USER_REARVIEW_CAMERA_MODE, g_dispatch.Held(presets::Action::Rear) || wheel::RearView());
}

// ---------------------------------------------------------------- frame
void Init()
{
    g_pad.Init();
    ai::SetEvadeGuard(HostGuardEvade);
    LoadConfig();
    padmenus::Init(), newgame::Init(), livesplit::Init(), runtimer::Init(); // (D-MENUS)
}

void Shutdown()
{
    SetHudSuppression(false);
    CloseModals();
    ReleaseAll();
    haptics::Stop(g_pad); // the motors off (a hot reload mid-effect)
    // render.cpp is compiled into this logic: its D3D textures go with it (every hot reload leaked the previous
    // logic's objects until 24 Oct 2026)
    shieldcue::Shutdown();
    reticlecue::Shutdown();
    hud2::Shutdown(); // the game's panels back, the textures freed
    uiskin::Shutdown(); // the windows' art and colours back
    uistrip::Shutdown();
    padhud::Shutdown();
    padbases::Shutdown(); // (D-BASES: the bar's hovered character released, the prompt bar's state)
    runtimer::Shutdown(), livesplit::Shutdown(), newgame::Shutdown(), padmenus::Shutdown(); // (D-MENUS)
}

// The frame of the last under-HUD pass: the shield cue draws there (user, 19 Oct 2026: screen-edge effects under the
// HUD, as Destiny and Apex Legends layer theirs); without that pass (hook refused) it falls back to the overlay.
static u32 g_underHudFrame = 0;
static bool g_menuOver = false; // a modal menu over the flight under ui_style 1 (this frame's under-HUD pass)

void OnRenderUnderHud()
{
    perf::Scope perfPass("OnRenderUnderHud");
    space::EndFrame(); // (normally done: a fault inside OnFrame skips its destructors) - the passes scan fresh
    g_underHudFrame = g_frame;
    // Extended HUD 2's modal menus in space (ui_style 1: pause, save / load, Options, confirm): the whole flight HUD -
    // the game's, HUD 2, the pad HUD layer, the cues - goes under the scrim, back the frame the menu closes (uiskin's
    // registry: false under ui_style 0, so today's behaviour exactly)
    g_menuOver = g_opt.enabled && (g_mode == Mode::Space || g_mode == Mode::SpaceUi) && uiskin::ModalOpen();
    hud2::SetMenuOver(g_menuOver);
    // the native HUD switch and the game's 3D views in HUD 2's windows: before the UI pass draws the controls (any mode:
    // restores)
    hud2::RenderUnderHud(g_host ? g_host->GetD3DDevice() : nullptr);
    // the hosted FLVIBED options page's opaque base under the Options window (before its glass / art)
    options_page::RenderUnder(g_host ? g_host->GetD3DDevice() : nullptr);
    // Extended HUD 2: the windows' art hidden / restored and their glass under the UI pass (any mode: restores)
    uiskin::RenderUnder(g_host ? g_host->GetD3DDevice() : nullptr);
    if (g_opt.enabled && (g_mode == Mode::Space || g_mode == Mode::SpaceUi) && !g_menuOver)
        shieldcue::Render(g_host ? g_host->GetD3DDevice() : nullptr);
}

void OnRenderOverlay()
{
    perf::Scope perfPass("OnRenderOverlay");
    space::EndFrame(); // as OnRenderUnderHud
    if (g_opt.enabled && (g_mode == Mode::Space || g_mode == Mode::SpaceUi))
    {
        void* device = g_host ? g_host->GetD3DDevice() : nullptr;
        // (under an Extended HUD 2 modal menu: the cues, HUD 2 (its own gate) and the pad HUD layer stay under the scrim)
        const bool menuOver = g_menuOver && g_frame - g_underHudFrame <= 2;
        if (g_frame - g_underHudFrame > 2)
            shieldcue::Render(device);
        if (!menuOver)
            reticlecue::Render(device);
        if (g_opt.enabled)
            hud2::Render(device); // HUD 2 (its panels; the flight modals draw over it)
        if (!menuOver)
            padhud::Render(device); // the pad HUD layer (badges, legend, status line, world prompts) over the HUD
        picker::Render(device); // the phase D flight modals over it (one open at a time by their buttons)
        wheel::Render(device);
        goals::Render(device);
        wgroups::Render(device);
    }
    if (g_opt.enabled && (g_mode == Mode::Docked || g_mode == Mode::SpaceUi)) // (D-BASES) the bases' strip and the
    {                                                                         // dealers' line, the nav map's list
        void* device = g_host ? g_host->GetD3DDevice() : nullptr;
        padbases::Draw(device);
        padmap::Draw(device);
    }
    padmenus::Draw();     // (D-MENUS) the New Game prompt, the load screens' tags, the OSK over the game's menu screens
    runtimer::Draw();     // (D-MENUS) speedrun mode's HUD timer (pad and keyboard alike)
    uistrip::Draw(g_host ? g_host->GetD3DDevice() : nullptr); // Extended HUD 2's docked overlay (the STRIP track)
    promptbar::Draw();    // the shared prompt bar (D-MENUS's component; every pad screen sets it per frame)
    options_page::Draw(); // the FLVIBED OPTIONS page over everything while it is open (the PAGE track)
    barbutton::Draw();    // the bar's autopilot button glyph (the HOST track)
}
// Before every device Reset (the host's Reset hook): every state block this logic holds goes - HUD 2's batcher's AND the
// shared renderer's (core\game\render.cpp: the bar button, glyphs, prompt bar, pages draw through it). Was: only HUD 2's;
// the renderer's state block outlived the Reset (26 Oct 2026, the Alt+Enter freeze audit): under d3d8to9 (HDE) a D3D9
// state block left alive makes Reset fail D3DERR_INVALIDCALL. Textures are MANAGED (they survive), no DEFAULT pool objects.
void OnDeviceLost()
{
    hud2::OnDeviceLost();
    render::OnDeviceLost();
}
void OnDeviceReset() {}

// A frame-rate cap for test games (the owner, 29 Oct 2026: "limit each instance to no more than 60 fps for now" - three
// background games at ~140 fps each): debug command `fpscap <fps>` (0 = off, the default; never a player setting). Waits
// at the start of the frame until 1 / fps has passed since the last one - a high-resolution waitable timer where Windows
// has one (Windows 10 1803+), else Sleep(0) spins on the performance counter.
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

void OnFrame(double dtd)
{
    // The game clock (core\game\gameclock.h): the frame's game dt as the game passes it, before this function's own
    // clamp - 0 while paused (the dt getter returns 0 then), 1/fps under the debug `fixeddt`; first, so every reader
    // this frame sees this frame's time. (The host's modal-loop frames pass a wall dt of at most 0.25 s.)
    gameclock::Advance(dtd);
    CapFrame();
    perf::NewFrame();
    perf::Scope perfFrame("OnFrame");
    barbutton::Update(); // the bar's autopilot button state to the host (the HOST track)
    // The frame's memo of the space scans (space.h BeginFrame): on for this function's run only - the game does not
    // simulate inside it; render passes and debug commands outside it scan fresh.
    space::BeginFrame();
    struct EndMemo
    {
        ~EndMemo() { space::EndFrame(); }
    } endMemo;
    ++g_frame;
    // A new game (host v34: a save load, New Game, the load after a death - each loads a fresh Content.dll): mission.cpp's
    // memory of the last script / objective and the shown objectives was the game before's (A37 S1: M07's stood in for
    // M12's "Awaiting" after SaveS208 -> SaveS182). Before anything this frame asks it. A logic (re)start forgets nothing:
    // its mission.cpp memory starts empty.
    {
        static bool seen = false;
        static u32 lastLoads = 0;
        const u32 loads = HostGameLoads();
        if (seen && loads != lastLoads)
        {
            mission::ForgetGame();
            autopilot::ArbiterForgetGame(); // (A37 S7: the goal queue's story memory is the game's too)
            events::Push("game-load %u: mission memory forgotten", loads);
        }
        seen = true;
        lastLoads = loads;
    }
    const float dt = Clampf(static_cast<float>(dtd), 0.0f, 0.25f);
    g_frameDt = dt;

    // focus_only: the physical pad acts only while the game is in front (the user may be using it in another program) -
    // the one gate the menus' reader asks too (focus_gate.h); everything else (modes, the bot, the autopilot, the
    // virtual pad) goes on out of focus.
    g_pad.ignoreReal = Gamepad::IgnoreRealNow(g_opt.focusOnly);
    g_pad.Update(dt);
    g_menuPad = uiopt::Get().confirmButton == 1 ? g_pad.SwappedAB() : g_pad;
    {
        // each gate in its consumer's view (the menu gates see A / B as accept / back)
        bool in[glyphs::COUNT], menu[glyphs::COUNT];
        PadInputs(in);
        PadInputs(menu, g_menuPad);
        g_gateFlight.Observe(in), g_gateFluf.Observe(menu), g_gateUi.Observe(menu);
    }
    TickSteerRelease();
    mprules::Update(); // multiplayer: the server's word on the autopilot (new chat lines only)
    ai::Update();
    spawn::CheckClearAt(ai::PlayerHealth()); // `spawn clearat`: test spawns go at a hull threshold (no harness lag)
    space::InvalidateFrame();                // that may have despawned ships (spawn.cpp knows nothing of the memo)
    TrackSelf();
    if (g_mode == Mode::Space || g_mode == Mode::SpaceUi)
        WatchManeuver(); // a stalled formation / goto / dock is issued again (setting maneuver_watchdog)
    stallrec::Update(static_cast<float>(dtd), ModeName(g_mode), fl::GamePaused());
    flight::RecordFrame();
    debug::Frame();
    options_page::Update(dt); // the options page's input and hosting state, before the pad flight / menu code (PAGE)
    flufaware::Update(dt, g_opt.flufWindows); // FLUF's windows from the HUD bus (no FLUF: nothing), before every reader
    shieldcue::Update(dt, g_mode == Mode::Space || g_mode == Mode::SpaceUi);
    reticlecue::Update(dt, g_mode == Mode::Space || g_mode == Mode::SpaceUi, autopilot::Active());
    {
        // the pad's vibration (haptics.h): before the enabled check below, so turning the module off silences it
        haptics::Frame hf;
        hf.where = g_mode == Mode::Space || g_mode == Mode::SpaceUi ? haptics::Where::Flight
                   : g_mode == Mode::Cutscene                       ? haptics::Where::Cutscene
                   : g_mode == Mode::Docked                         ? haptics::Where::Docked
                                                                    : haptics::Where::Menu;
        hf.enabled = g_opt.enabled;
        hf.paused = fl::GamePaused();
        hf.autopilot = autopilot::Active();
        haptics::Update(hf, g_pad);
    }
    if (g_mode == Mode::Space || g_mode == Mode::SpaceUi)
        space::TrackRounds(); // learns which incoming rounds hit (the damage model's hit chance)
    HostSetKeepRunning(g_opt.runInBackground);
    if (!g_opt.enabled)
    {
        HostCursorHide(kCursorHideIdle, false); // (the idle cursor's fail-safe: a disabled logic never leaves it hidden)
        return;
    }

    // Whoever was used last (pad, or the physical mouse / keyboard) owns the cursor, the steering and the UI: the
    // input-device service (device.cpp; was: the pad's tick against the mouse's, without the keyboard).
    const u32 now = GetTickCount();
    if (g_pad.AnyInput())
        g_lastPadTick = now;
    // The controller UI's settings as its modules read them (uiopt.h): the registry's values, copied each frame.
    {
        uiopt::UiOptions& u = uiopt::Get();
        u.inputMode = g_opt.inputMode, u.glyphStyle = g_opt.glyphStyle, u.confirmButton = g_opt.confirmButton;
        u.pauseOnDisconnect = g_opt.pauseOnDisconnect, u.padPreset = g_opt.padPreset;
        u.weaponGroups = g_opt.weaponGroups, u.swapY = g_opt.swapY, u.holdsAsToggles = g_opt.holdsAsToggles;
        u.rumble = g_opt.rumble, u.padCursor = g_opt.padCursor;
        u.cursorMagnet = g_opt.cursorMagnet, u.promptBar = g_opt.promptBar, u.worldPrompts = g_opt.worldPrompts;
        // (Steam Deck mode: the screen says, deck_mode overrides; the player's own text_size stays theirs)
        deckmode::g_on = deckmode::Active(g_opt.deckMode, static_cast<float>(fl::ScreenWidth()), static_cast<float>(fl::ScreenHeight()));
        u.apHints = g_opt.apHints, u.textSize = deckmode::TextSize(deckmode::g_on, g_opt.textSize), u.hudGlyphs = g_opt.hudGlyphs;
        gametext::Frame(g_opt.gameTextSize, g_mode == Mode::Menu);
        u.buttonLegend = g_opt.buttonLegend, u.apStatusLine = g_opt.apStatusLine;
        u.hudStyle = g_opt.hudStyle, u.hudFade = g_opt.hudFade, u.hudOpacity = g_opt.hudOpacity;
        u.hudScale = g_opt.hudScale, u.hudNumbers = g_opt.hudNumbers, u.hudThreats = g_opt.hudThreats;
        u.hudContext = g_opt.hudContext, u.hudPrompts = g_opt.hudPrompts, u.hudAlerts = g_opt.hudAlerts;
        u.hudInfo = g_opt.hudInfo;
        u.hudWireframes = g_opt.hudWireframes, u.hudTransition = g_opt.hudTransition, u.hudPreset = g_opt.hudPreset;
    }
    device::Update(dt, g_pad, g_mode == Mode::Space);
    // A new flight - the player ship appeared or changed: a launch, or a save load, also one started from inside a
    // flight that never passes the main menu (where the objective is re-read below): the objective is re-read from the
    // game's own copy (the hooked one was the previous save's: after SaveC705 -> SaveS008 HUD 2's headline and place
    // line stayed empty; verified fixed in game 26 Oct 2026, runs\m03_load_check_2026-10-26). (The game clears the
    // target itself on a load - the "stale target" seen was only `hud2 state`'s name.)
    {
        static u32 lastShip = 0;
        const u32 ship = space::PlayerShipId();
        if (ship != lastShip)
        {
            if (ship)
                HostObjectiveReread();
            lastShip = ship;
        }
    }
    // Turret view as the game has it: its current camera is the turret camera (exe::CamTurret). A window, a save load, a
    // death or the player's own H key change it without FLvibed (a spec recapture, 26 Oct 2026: TURRET VIEW kept after
    // a load). Without the symbol FLvibed's own flag stands.
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
    const bool padOwns = device::Pad();
    if (device::TakeDisconnectPause() && !fl::GamePaused())
        Tap(fl::USER_PAUSE); // pause_on_disconnect: the game's own pause menu

    // In windowed modes Freelancer hands the mouse back to Windows when the cursor leaves the
    // window; using the pad takes it back (like clicking into the window would).
    // Only while Freelancer is the foreground window (capturing fails otherwise), and rate
    // limited so a failing capture cannot run every frame.
    static u32 lastCaptureTry = 0;
    const bool focused = GetForegroundWindow() == fl::MainWindow(); // the main window itself (not an owned popup)
    if (g_pad.AnyInput() && !dimouse::IsCaptured() && focused && now - lastCaptureTry > 500)
    {
        lastCaptureTry = now;
        dimouse::Capture();
    }

    // Debounce: the cursor name flickers for a frame while windows open and close, and every
    // mode change releases held inputs. Accept a new mode once it has held for 3 frames.
    static Mode candidate = Mode::Menu;
    static int candidateFrames = 0;
    // Player death (Freelancer switches to its death camera).
    {
        static bool dead = false;
        const bool nowDead = fl::CamDeath() && fl::CurrentCamera() == fl::CamDeath();
        if (nowDead && !dead)
        {
            events::Push("player-death");
            StopBot("player-death");
            // the autopilot's own hull-0 stop normally came a frame earlier (autopilot_frame.cpp); this one ends the
            // objective mode on a death whatever the hull read (a no-op when it is off already)
            autopilot::Stop("player died");
        }
        dead = nowDead;
    }
    // The game's sequences (dock, launch, jump, lane) lock the player out by the ship's control exclusion; a sequence cut
    // short leaves it set (M13 22 Oct 2026: letterbox and no steering inside the Dyson sphere for 30 min). Every change
    // is an event, with the camera.
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
    const Mode detected = DetectMode();
    candidateFrames = detected == candidate ? candidateFrames + 1 : 1;
    candidate = detected;
    const Mode mode = candidateFrames >= 3 ? detected : g_mode;
    if (mode != g_mode)
    {
        Log("mode %s -> %s", ModeName(g_mode), ModeName(mode));
        events::Push("mode %s -> %s", ModeName(g_mode), ModeName(mode));
        if (mode == Mode::Menu)
            autopilot::NoteMenu();
        // out of the menus = a game loaded, started or launched into: the objective the load restored (no objective event
        // comes with it - a mission save loaded after a free-roam start, 25 Oct 2026) read once from the game (host v25).
        // Only here: inside a session the hooked events are newer than the client copy.
        if (g_mode == Mode::Menu)
            HostObjectiveReread();
        ReleaseAll();
        g_dispatch.Reset(); // the pad's holds and latches end with the mode
        // Turret view is the game's toggle: a window over the flight (space <-> space-ui) leaves it on, so FLvibed's flag
        // stays with it (was: reset at every mode change - the next X hold then toggled the game's view the other way,
        // the owner, 26 Oct 2026); leaving space ends it
        if (mode != Mode::Space && mode != Mode::SpaceUi)
            g_turret = false;
        // ...and the turret camera's look input: a key-up sent while a window had the cursor can be lost (the game
        // drops flight commands there), which left the camera turning for good. Back in flight, every look command's
        // key-up again (the handler 0x51AC20 zeroes its input on a key-up, whatever came before).
        if (mode == Mode::Space)
            for (int c : {fl::USER_LOOK_ROTATE_CAMERA_LEFT, fl::USER_LOOK_ROTATE_CAMERA_RIGHT,
                          fl::USER_LOOK_ROTATE_CAMERA_UP, fl::USER_LOOK_ROTATE_CAMERA_DOWN})
                fl::FireCommand(c, false);
        CloseModals(); // the phase D modals end with the flight mode
        // Windows are reused (e.g. one tutorial popup object for every message): whatever is
        // open when we enter a cursor mode counts as newly opened, so it gets auto-focus -
        // except the flight HUD, which was already there.
        if (g_mode == Mode::Space && mode == Mode::SpaceUi)
            g_prevRoots = g_hudRoots;
        else
            g_prevRoots.clear();
        if (mode != Mode::Space && mode != Mode::SpaceUi)
            g_hudRoots.clear();
        if (mode == Mode::Docked || mode == Mode::Menu)
            autopilot::Stop(mode == Mode::Docked ? "docked" : "left space");
        g_mode = mode;
    }
    // The autopilot's own dock: its docking / landing cutscene skipped the way a player's Escape skips it (user, 21 Oct
    // 2026: "the planet landing animation could be skipped just the same as the launch animation"; the harness did it
    // only in some of its calls - M03's Willard played 14 s, docks 5-6 s, skipped ones ~1 s). Only on the docking
    // camera (exe::CamDocking: story cutscenes in space are not skippable and are never sent USER_CANCEL), once per
    // cutscene, a second try after 1 s if it still runs.
    {
        static float onDockCam = -1;
        static int tries = 0;
        // ...only a dock that leads into a base (its CEqObj dock target): an airlock's transit into another system
        // (M12's Nomad lair) cut short left the ship pinned against the core's wall with the HUD hidden (22 Oct 2026)
        bool toBase = false;
        if (g_mode == Mode::Cutscene && exe::CamDocking.is(fl::CurrentCamera()) && autopilot::OwnDock())
        {
            const u32 tid = fl::TargetId();
            for (const space::Contact& c : space::Contacts())
                if (c.id == tid)
                    toBase = c.base != 0;
        }
        if (toBase)
        {
            onDockCam = onDockCam < 0 ? 0 : onDockCam + dt;
            if ((tries == 0 && onDockCam >= 0.1f) || (tries == 1 && onDockCam >= 1.1f))
            {
                Tap(fl::USER_CANCEL);
                ++tries;
                events::Push("autopilot dock-cutscene skipped (try %d)", tries);
            }
        }
        else
            onDockCam = -1, tries = 0;
    }

    // (D-MENUS) the session's mode per save (the sidecar, the loads), speedrun mode's clock / split detector / LiveSplit
    // client and the HUD timer's bests: every frame, whatever the device
    newgame::Update(dt, ModeName(g_mode));
    livesplit::Update(dt, ModeName(g_mode));
    runtimer::Update(dt);
    // HUD 2 (docs\UI_HUD2.md): its state and data, the native HUD switch's decision, the HUD bus - every frame
    {
        hud2::Context hc;
        hc.space = g_mode == Mode::Space;
        hc.spaceUi = g_mode == Mode::SpaceUi;
        hc.cutscene = g_mode == Mode::Cutscene;
        hc.pad = padOwns;
        hc.bot = g_bot.on;
        hc.table = g_table;
        hc.autopilot = autopilot::Active();
        hc.turret = g_turret;
        hc.weaponGroup = g_weaponGroup;
        hc.apKnown = g_apUsed || uiopt::Get().apHints || newgame::Session() == newgame::ASSISTED;
        hc.modal = picker::IsOpen() || wheel::IsOpen() || goals::IsOpen() || wgroups::IsOpen();
        hc.steer = sqrtf(g_lastSteerX * g_lastSteerX + g_lastSteerY * g_lastSteerY); // (CONTEXTUAL's triggers, 0.2.1)
        hc.firing = padOwns && (g_dispatch.Held(presets::Action::Fire) || g_dispatch.Held(presets::Action::Missile));
        // any fire input's PRESS wakes the whole HUD under hud_fade 1 (0.2.1 round 6, the owner: the press itself, even
        // with the weapons locked / nothing able to fire; core\game\hudwake.h FireInput) - the pad's here, the keyboard
        // and mouse's in hud2.cpp (the game's keymap)
        {
            using A = presets::Action;
            static bool fireWas = false;
            const bool fireNow = padOwns && (g_dispatch.Held(A::Fire) || g_dispatch.Held(A::Missile) ||
                                             g_dispatch.Held(A::Torpedo) || g_dispatch.Held(A::Cm) ||
                                             g_dispatch.Held(A::Mine) || g_dispatch.Held(A::FireGroup1) ||
                                             g_dispatch.Held(A::FireGroup2) || g_dispatch.Held(A::CruiseDisruptor));
            if (fireNow && !fireWas)
                hud2::NoteAction(hudwake::FireInput);
            fireWas = fireNow;
        }
        hud2::Update(dt, hc);
        // The game's mouse cursor under hud_fade 1 (0.2.1, core\game\hudwake.h CursorStep): in flight with the keyboard
        // and mouse it hides after 3 s without mouse movement or a button and shows again the frame either comes; shown
        // in every other context (a menu, the map, a dialog / popup = any non-HUD window, the options page, a FLUF window,
        // the pad's cursor, mouse flight engaged) and whenever a symbol it reads is unresolved.
        {
            static hudwake::CursorState cs;
            static int mxWas = -1, myWas = -1;
            static u32 bWas = 0;
            const bool known = exe::MouseX.ok() && exe::MouseY.ok() && exe::MouseButtons.ok() && exe::MouseFlightEngaged.ok();
            const int mx = known ? fl::MouseX() : 0, my = known ? fl::MouseY() : 0;
            const u32 b = known ? (fl::MouseButtons() & 7u) : 0;
            // the pad in control (0.2.1 round 6, the owner: the cursor did not hide with the pad): the pad's own aim moves
            // the game's cursor and its steering engages mouse flight, so neither counts - a real mouse hands the device
            // back to the keyboard and mouse (padOwns false: the K&M rule at once)
            const bool activity = padOwns ? !known
                                          : !known || mx != mxWas || my != myWas || b != 0 || b != bWas ||
                                                Mem<u8>(exe::MouseFlightEngaged) != 0;
            mxWas = mx, myWas = my, bWas = b;
            // (the fail-safe, 0.2.1 round 5: every non-flight state shows it the same frame - the pause menu by the
            // game's own paused flag too, a mode change at once, before the mode's 3-frame debounce settles)
            const bool eligible = g_opt.enabled && g_mode == Mode::Space && detected == Mode::Space &&
                                  !fl::GamePaused() && !hc.modal &&
                                  hud2::CursorIdleEligible() && OnlyHudOpen() && !options_page::IsOpen() &&
                                  !flufaware::InputWindowOpen();
            const int fade = uiopt::Get().hudFade <= 0 ? 0 : 1;
            const bool hide = hudwake::CursorStep(cs, fade, eligible, activity, dt);
            HostCursorHide(kCursorHideIdle, hide);
            hud2::NoteCursorHidden(hide);
        }
    }
    // The goal panel, every device (its keyboard + mouse form's keys, clicks and click box; after HUD 2, which stood its
    // own click box down while the panel is open)
    goals::Tick(dt);
    // HUD 2 over the menus and rooms (formerly "Extended HUD 2", docs\UI_HUD2_EXTENDED.md 10; part of HUD 2 since
    // 0.2.0): the skin's switch and the docked overlay's - every frame. Classic (hud_style 0) is classic everywhere.
    {
        const int uiStyle = g_opt.hudStyle != 0 ? g_opt.uiStyle : 0;
        uiskin::Context sc;
        sc.style = uiStyle;
        sc.pad = padOwns;
        sc.enabled = g_opt.enabled;
        uiskin::Update(dt, sc);
        uistrip::Update(dt, g_opt.enabled && uiStyle == 1 && g_mode == Mode::Docked);
        windowtext::Frame(g_opt.windowText && g_opt.enabled && uiStyle == 1 && g_opt.gameTextSize == 100);
    }
    // The open options page owns the pad and the keys: no flight / menu pad dispatch. Its ownership outlasts its close
    // until every button is up: the press that closed it (B, A on CANCEL / CLOSE) reached the menu and base code in
    // the same frame and again as a fresh edge of padmenus' own pad reader (release verify, 25 Oct 2026: B closed the
    // page AND resumed the pause menu; A on CANCEL also talked to the bar's focused character).
    static bool pageLatch = false;
    if (options_page::WantsInput())
    {
        pageLatch = true;
        return;
    }
    if (pageLatch)
    {
        if (g_pad.Down(XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_B | XINPUT_GAMEPAD_X | XINPUT_GAMEPAD_Y |
                       XINPUT_GAMEPAD_LEFT_SHOULDER | XINPUT_GAMEPAD_RIGHT_SHOULDER | XINPUT_GAMEPAD_START |
                       XINPUT_GAMEPAD_BACK))
            return;
        pageLatch = false;
    }
    // Start opens/closes the game menu (Freelancer's pause menu, F1 by default); in flight the preset's MENU binding
    // does it (a tap). Not while a D-MENUS screen owns the pad (the
    // OSK's Menu = done, the New Game prompt).
    if (g_mode != Mode::Space && g_pad.Pressed(XINPUT_GAMEPAD_START) && !padmenus::OwnsInput())
        Tap(fl::USER_PAUSE);

    // The pad HUD layer's view of the frame (padhud.cpp draws it in OnRenderOverlay)
    {
        padhud::Context pc;
        pc.active = padOwns && (g_mode == Mode::Space || g_mode == Mode::SpaceUi) && !g_bot.on;
        pc.flying = g_mode == Mode::Space;
        pc.table = g_table;
        pc.preset = uiopt::Get().padPreset == presets::CUSTOM ? presets::FREELANCER : uiopt::Get().padPreset;
        pc.autopilot = autopilot::Active();
        pc.turret = g_turret;
        pc.weaponGroup = g_weaponGroup;
        pc.apKnown = g_apUsed || uiopt::Get().apHints || newgame::Session() == newgame::ASSISTED; // (D-MENUS)
        padhud::Update(dt, pc);
        // a modal's own prompt bar: the legend steps aside (FLUF's windows on the pad too: A3, 26 Oct 2026)
        padhud::SetModal(picker::IsOpen() || wheel::IsOpen() || goals::IsOpen() || wgroups::IsOpen() || flufaware::OwnsPad());
    }

    // While the pad (or the test bot) flies, the HUD is transparent to the cursor: aiming can cross
    // it in every direction. The physical mouse keeps normal HUD access.
    const bool autoFlies = g_bot.on || autopilot::Active();
    static bool autoFlew = false;
    if (autoFlew && !autoFlies && padOwns && (g_mode == Mode::Space || g_mode == Mode::SpaceUi)) // (never the player's mouse)
        dimouse::MoveTo(fl::ScreenWidth() * 0.5f, fl::ScreenHeight() * 0.5f); // aim cursor off the HUD when done
    autoFlew = autoFlies;
    // ... but an open NeuroNet window is the cursor's, whoever flies: the windows and the NeuroNet box's buttons hang
    // under the HUD's root NN_MainButton, so the suppression also took the map's selection and the buttons to the
    // inventory, log and player info from the cursor (GitHub issue 2, 3 Nov 2026). By the window, not by Mode::SpaceUi:
    // that mode also means an arrow cursor over the HUD, which the suppression keeps from latching
    SetHudSuppression((g_mode == Mode::Space || g_mode == Mode::SpaceUi) && (padOwns || autoFlies) &&
                      !padbases::SpaceWindowOpen());

    // The flight HUD's windows, scanned while flying (whoever flies) and only while no window has the mouse (a window
    // opening switches the cursor to the arrow at once): DetectMode tells the HUD from NeuroNet windows by them.
    static float hudScan = 0;
    hudScan -= dt;
    if (g_mode == Mode::Space && hudScan <= 0 && _stricmp(fl::CursorName(), "arrow") != 0)
    {
        g_hudRoots = ui::VisibleRoots();
        g_hudRootsChanged = true;
        hudScan = 0.25f;
    }

    // The test bot and the autopilot fly whoever touched the mouse or pad last.
    if (autoFlies && g_mode == Mode::Space)
    {
        UpdateSpace(dt);
        return;
    }

    if (!padOwns)
    {
        // The physical mouse/keyboard is in use: stay out of its way (the motion trace still records the flight).
        if (g_mode == Mode::Space)
        {
            ReleaseAll();
            g_dispatch.Reset();
            picker::Close(true); // the pad's modals end with the pad - the goal panel has its keyboard + mouse form
            wheel::Cancel();
            wgroups::Close();
            if (g_turret && device::Switched()) // the pad's turret view ends with the pad (the player's H key is theirs:
                                                // g_turret follows the game's camera, so only at the switch)
            {
                g_turret = false;
                Tap(fl::USER_AUTO_TURRET);
            }
            autopilot::TracePlayer(dt);
        }
        return;
    }

    // FLUF (flufaware.h, fluf_windows 1 / 2): one of its input windows is open - the pad drives it this frame (the
    // palette by its keys, the others by the cursor); FLvibed's own dispatch steps aside until it closes
    if (flufaware::OwnsPad())
    {
        ReleaseSteer();
        UpdateFlufPad(dt);
        padmenus::Idle(dt);
        return;
    }
    EndFlufPad();
    // (D-MENUS) the game's menu screens on the pad (main menu + New Game prompt, load, save + OSK, pause, death, the
    // chat's field in space); while one of them owns the pad this frame the dispatch below stays out
    padmenus::Update(dt, ModeName(g_mode));
    if (padmenus::OwnsInput())
        return;
    switch (g_mode)
    {
    case Mode::Menu:
    case Mode::Docked:
    case Mode::SpaceUi:
        ReleaseSteer();
        // (D-BASES) the pad screens of bases, dealers, NeuroNet windows and the nav map: they take their buttons first
        // (padbases::Owns gates the generic focus / cursor code below)
        padbases::Update(dt, g_menuPad,
                         g_mode == Mode::Docked    ? padbases::Where::Docked
                         : g_mode == Mode::SpaceUi ? padbases::Where::SpaceUi
                                                   : padbases::Where::Menu);
        UpdateUiNav(dt);
        UpdateFreeCursor(dt);
        if (g_mode == Mode::SpaceUi && g_pad.Pressed(XINPUT_GAMEPAD_BACK) && !padbases::Owns(XINPUT_GAMEPAD_BACK))
            fl::TapKey(VK_ESCAPE); // close the window
        break;
    case Mode::Space: UpdateSpace(dt); break;
    case Mode::Cutscene:
        ReleaseSteer();
        if (g_pad.Pressed(XINPUT_GAMEPAD_A) || g_pad.Pressed(XINPUT_GAMEPAD_B))
            fl::TapKey(VK_ESCAPE);
        break;
    }
}

// Started by deploy.ps1 -Background (FLVIBED_BACKGROUND=1 in the game's environment): runs out of focus, muted.
static bool BackgroundTestStart()
{
    char v[8] = {};
    return GetEnvironmentVariableA("FLVIBED_BACKGROUND", v, sizeof(v)) && v[0] == '1';
}

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
      << " background_run=" << ((g_opt.runInBackground || BackgroundTestStart()) && HostAtLeast(7))
      << " lastPad=" << g_lastPadTick << " lastMouse=" << dimouse::LastRealTick() << "\n";
    o << device::Status() << "\n";
    o << "preset=" << presets::PresetName(uiopt::Get().padPreset) << " group=" << g_weaponGroup
      << " turret=" << g_turret << " " << g_dispatch.Status() << "\n";
    o << "held:";
    for (auto& h : g_holding)
        if (h.second.down)
            o << " " << fl::CmdName(h.first);
    o << "\n";
    return o.str();
}

// ---------------------------------------------------------------- options table / FLController.ini
// Every tunable lives here once (the registry type core\settings.h; the autopilot's rows are autopilot_settings.cpp
// kDefs): `set <name> <value>` (live, not saved), `config save` (writes all current values to FLController.ini next to
// the DLL, a help line per setting), `config reload`, `config settings` (every setting of both sections with its help).
// The file is created with defaults and comments the first time.
static const settings::Def kOptions[] = {
    {"aim_mode", settings::Int, &g_opt.aimMode, 0, 1,
     "1 = gimbal: the right stick only steers, the reticle locks onto the selected hostile target's lead "
     "cross while it is on screen; 0 = the reticle follows the right stick (with the aim assists below)"},
    {"aim_cone", settings::Float, &g_opt.aimCone, 0, 90,
     "gimbal: lock-on cone half-angle in degrees (90 = whole screen)"},
    {"aim_smooth", settings::Float, &g_opt.aimSmooth, 0, 1, "reticle smoothing time constant in seconds (0 = off)"},
    {"auto_target", settings::Int, &g_opt.autoTarget, 0, 2,
     "0 = off (default): you pick targets (the target button: what is under the reticle, else the closest enemy) "
     "and a selected target is never "
     "changed; 1 = select the closest enemy when the current one is destroyed; 2 = also when nothing is "
     "targeted (paused for 10 s after you clear the target yourself)"},
    {"fire_target", settings::Bool, &g_opt.fireTarget, 0, 1,
     "1 = with nothing targeted, firing at a hostile or neutral ship under the reticle selects it"},
    {"steer_expo", settings::Float, &g_opt.steerExpo, 1, 4, "steering response curve exponent (1 = linear)"},
    {"steer_scale", settings::Float, &g_opt.steerScale, 0.1f, 1,
     "full stick = this mouse-flight offset; 0.77 = the ship's maximum turn rate"},
    {"invert_y", settings::Bool, &g_opt.invertY, 0, 1, "1 = stick up turns the nose down"},
    {"stick_deadzone", settings::Float, &g_pad.stickDeadzone, 0, 0.9f, "radial stick deadzone (0..1)"},
    {"trigger_threshold", settings::Float, &g_pad.triggerThreshold, 0, 0.9f, "trigger deadzone (0..1)"},
    {"pad_backend", settings::Int, &g_pad.padBackend, 0, 1,
     "how the controller is read: 0 XInput (default; Xbox pads and pads that act as one), 1 SDL3 (experimental, not "
     "yet verified with real PlayStation or Switch pads: meant to read them too, the button prompts from the pad's "
     "type; starts on its own thread in a few milliseconds, XInput reads the pad meanwhile and if SDL cannot start)"},
    {"throttle_rate", settings::Float, &g_opt.throttleRate, 0.1f, 5,
     "throttle lever change per second at full left stick"},
    {"cursor_speed", settings::Float, &g_opt.cursorSpeed, 0.1f, 5, "menu cursor speed, screen heights per second"},
    {"wheel_rate", settings::Float, &g_opt.wheelRate, 1, 50, "right stick scrolling, notches per second"},
    {"aim_cursor", settings::Bool, &g_opt.aimCursor, 0, 1, "aim_mode 0: the reticle follows the right stick"},
    {"aim_assist", settings::Float, &g_opt.aimAssist, 0, 1,
     "aim_mode 0: pull of the reticle onto a hostile's lead cross"},
    {"aim_radius", settings::Float, &g_opt.aimRadius, 0.01f, 2, "aim_mode 0: pull radius, fraction of screen height"},
    {"aim_slow_radius", settings::Float, &g_opt.aimSlowRadius, 0.01f, 2, "aim_mode 0: steering slowdown radius"},
    {"aim_slowdown", settings::Float, &g_opt.aimSlowdown, 0, 1, "aim_mode 0: steering slowdown at the cross"},
    {"aim_rotation", settings::Float, &g_opt.aimRotation, 0, 1, "aim_mode 0: rotational assist strength"},
    {"aim_rotation_max", settings::Float, &g_opt.aimRotationMax, 0, 1, "aim_mode 0: rotational assist cap"},
    {"focus_only", settings::Bool, &g_opt.focusOnly, 0, 1,
     "ignore the controller while the game is in the background, in flight and menus alike (a button held when the "
     "game comes back is ignored until it is released)"},
    {"maneuver_watchdog", settings::Bool, &g_opt.maneuverWatchdog, 0, 1,
     "1 = if your ship stalls during a formation, goto or dock (nearly stopped for 3 s, its goal more than 150 m away "
     "and not getting closer, you not steering), FLvibed leaves it and issues it again (up to 3 times, 10 s apart) and "
     "logs what happened"},
    {"run_in_background", settings::Bool, &g_opt.runInBackground, 0, 1,
     "1 = the game keeps running while its window is not in focus (Freelancer pauses otherwise); the mouse goes back "
     "to Windows meanwhile"},
    {"npc_stall_log", settings::Bool, &stallrec::Get().on, 0, 1,
     "writes EXE\\FLvibed_npc_stalls.log when a hostile ship near you stops moving: what the game says about it, for bug "
     "reports; read only, it changes nothing in the game"},
    {"shield_cue", settings::Bool, &shieldcue::Get().on, 0, 1,
     "screen-edge cue made of your shield's own hit effect when it is hit (tinted by the weapon), a flash when it "
     "fails; HUD 2's shield meter flashes red when low, and its danger alerts glow and pulse"},
    {"shield_cue_intensity", settings::Float, &shieldcue::Get().intensity, 0, 2,
     "shield_cue strength (alpha and size)"},
    {"hull_cue", settings::Bool, &shieldcue::Get().hull, 0, 1,
     "with shield_cue: hull hits show your ship's own hull hit effect at the screen edge, and while the hull is "
     "damaged the edges carry your ship's damage effects (sparks, fire, smoke) as the game lights them; HUD 2's hull "
     "meter flashes bright red when low"},
    {"hull_cue_pattern", settings::Int, &shieldcue::Get().pattern, 0, 2,
     "pattern of the hull damage edge, so it never reads as a shield hit: 0 none, 1 armour plates, 2 hexagons "
     "(default)"},
    {"reticle_cue", settings::Bool, &reticlecue::Get().on, 0, 1,
     "a ring pulse at the crosshair: autopilot on (cyan, outward), handed back (amber, inward), nanobots (green) / "
     "batteries (blue) used (double ring)"},
    {"autopilot_cancel", settings::Float, &g_opt.autopilotCancel, 0, 1,
     "Advanced Autopilot (the pad hold, the key or the bar button starts it): stick deflection that hands the ship "
     "back to you (0 = never). Its own settings (pilot, engage rules, weapons, personality fields p.<name>) are in "
     "the [Autopilot] section"},
    // The UI build's rows (docs\UI_SPEC.json; the page shows them with their words, core\ui_meta.h)
    {"input_mode", settings::Int, &g_opt.inputMode, 0, 2,
     "input device: 0 automatic (the last device used wins, the UI follows in one frame), 1 controller, 2 keyboard "
     "and mouse"},
    {"glyph_style", settings::Int, &g_opt.glyphStyle, 0, 4,
     "button prompts: 0 automatic (the connected pad's family), 1 Xbox, 2 PlayStation, 3 Nintendo, 4 Steam (Steam Deck / "
     "Steam Controller: Xbox's face buttons, L1 / L2 / R1 / R2)"},
    {"glyph_look", settings::Int, &uiopt::Get().glyphLook, 0, 2,
     "how the button prompts look: 0 automatic (HUD 2 draws them in its own colour, the classic HUD and the menus in "
     "the buttons' colours), 1 coloured everywhere, 2 HUD 2 style everywhere; sizes stay as each screen has them"},
    {"confirm_button", settings::Int, &g_opt.confirmButton, 0, 1,
     "which face button accepts in menus: 0 the bottom one (A / cross / B), 1 the right one (Nintendo)"},
    {"pause_on_disconnect", settings::Bool, &g_opt.pauseOnDisconnect, 0, 1,
     "a pad that drops out mid-flight pauses the game; in menus the UI switches to the keyboard"},
    {"pad_preset", settings::Int, &g_opt.padPreset, 0, 3,
     "the flight layout: 0 Freelancer (targets and weapons on the fingers, consumables on the D-pad), 1 Ace (arcade "
     "flight: the left stick steers, the triggers are the throttle), 2 Assisted (the autopilot flies, you aim and "
     "decide), 3 Custom"},
    {"weapon_groups", settings::Int, &g_opt.weaponGroups, 0, 2,
     "0 simple: RT fires the active group, Y is cruise; 1 cycle: Y tap flips to the next group, Y held is cruise; 2 two "
     "groups: RT fires group 1 and LT group 2 (afterburner moves to the LS click), Y is cruise. Launchers fire only on "
     "their own buttons"},
    {"y_swap", settings::Bool, &g_opt.swapY, 0, 1,
     "the Freelancer layout with weapon_groups 1: Y tap is cruise and Y held flips to the next group, instead of the "
     "other way round"},
    {"holds_as_toggles", settings::Bool, &g_opt.holdsAsToggles, 0, 1,
     "afterburner, reverse, drift and rear view become press-once / press-again instead of held buttons"},
    {"rumble", settings::Int, &g_opt.rumble, 0, 100,
     "vibration strength in percent (0 = off): hits on your ship by their weight, your shield failing, the "
     "autopilot's hand-back pulse and the game's boxes that wait for your answer; only while the controller is in use "
     "and the game is in front, never in cutscenes or the menus, and the world's effects never while paused"},
    {"pad_cursor", settings::Int, &g_opt.padCursor, 0, 2,
     "a cursor on the left stick where pointing is natural: 0 off, 1 rooms and maps, 2 everywhere; it slows over "
     "targets, the D-pad hides it"},
    {"cursor_magnet", settings::Int, &g_opt.cursorMagnet, 0, 100,
     "how strongly the pad cursor slows over and pulls toward a clickable element, percent"},
    {"prompt_bar", settings::Int, &g_opt.promptBar, 0, 2,
     "the bottom-right row of button prompts on controller screens: 0 contextual (a pending decision, a new screen), "
     "1 always, 2 off"},
    {"fluf_windows", settings::Int, &g_opt.flufWindows, 0, 2,
     "with FLUF installed: 0 ignore its windows (co-residency only; FLvibed's keys still pass to FLUF while its "
     "palette is open or its text box has the keys), 1 "
     "its player windows (palette, options, map, toasts) are keep-outs: FLvibed's HUD parts and strip fade under "
     "them, prompts and subtitles step away, the pad drives them, 2 (default) also its developer windows (the Debug "
     "Window); FLUF's windows stay on top; no effect without FLUF"},
    {"borderless", settings::Int, &g_opt.borderless, 0, 2,
     "borderless windowed, read at the game's start: 0 off (the game's own window mode), 1 (default) only when nothing "
     "else provides it (not with HDE: its exe and its Direct3D wrapper do), 2 always; while FLvibed provides it the "
     "game runs as a window without frame covering its monitor at the monitor's resolution (never exclusive "
     "fullscreen; sharp at high DPI scaling only when the exe is DPI-aware), and FLUF's Alt+Enter toggles it with the "
     "bordered window"},
    {"world_prompts", settings::Int, &g_opt.worldPrompts, 0, 1,
     "a prompt by the target when something unusual is possible now (dock in range, loot in reach): 0 contextual, 1 "
     "off"},
    {"ap_hints", settings::Bool, &g_opt.apHints, 0, 1,
     "show the autopilot's prompts and status before it was ever used; 0 = nothing about it shows until you switch "
     "it on once"},
    {"text_size", settings::Int, &g_opt.textSize, 80, 150,
     "size of FLvibed's own UI text (prompts, panels, the status line) in percent; the subtitles have their own"},
    {"deck_mode", settings::Int, &g_opt.deckMode, 0, 2,
     "Steam Deck mode, larger text and no safe margins at the screen's edge: 0 automatic (on at 1280x800), 1 on, 2 off"},
    {"game_text_size", settings::Int, &g_opt.gameTextSize, 100, 150,
     "size of the game's own text in every window, in percent; takes effect at the main menu"},
    {"window_text", settings::Bool, &g_opt.windowText, 0, 1,
     "HUD 2: the text in the game's windows takes the room it has, the prices most, labels less, headings and buttons not at all"},
    {"hud_glyphs", settings::Bool, &g_opt.hudGlyphs, 0, 1,
     "in pad mode the HUD's own buttons carry the glyph of the button that acts on them"},
    {"button_legend", settings::Int, &g_opt.buttonLegend, 0, 1,
     "the contextual list of the actions available now with their buttons, right-middle of the screen: 0 = on, "
     "1 = off (the row's order: ON / OFF)"},
    {"ap_status_line", settings::Bool, &g_opt.apStatusLine, 0, 1,
     "the one line under the maneuver bar while the autopilot flies, cruise charges or turret view is on"},
    {"show_advanced", settings::Bool, &g_opt.showAdvanced, 0, 1,
     "the options page adds the developer and tuning keys to every tab, as they are named in the ini files"},
    {"speedrun_timer", settings::Bool, &g_opt.speedrunTimer, 0, 1,
     "speedrun mode: the run time, the current split and the delta against your best on the HUD"},
    {"livesplit", settings::Int, &g_opt.livesplit, 0, 1,
     "send the timer and splits to LiveSplit on this computer: 0 off, 1 LiveSplit Server (TCP; Control > Start TCP "
     "Server in LiveSplit 1.8.29 or newer)"},
    {"livesplit_port", settings::Int, &g_opt.livesplitPort, 1024, 65535, "LiveSplit Server's port (default 16834)"},
    {"livesplit_gametime", settings::Bool, &g_opt.livesplitGametime, 0, 1,
     "pause LiveSplit's game time during loads (a loadless comparison); real time keeps running"},
    {"autopilot_key", settings::String, &g_opt.autopilotKey, 0, 0,
     "the keyboard key that toggles the autopilot (a key name: K, F10, Insert); a key the game binds is refused"},
    // HUD 2 (docs\UI_HUD2.md 5.3; the options page's CONTROLLER tab, section HUD)
    {"hud_style", settings::Int, &g_opt.hudStyle, 0, 2,
     "the HUD: 0 classic (the game's HUD and screens; on the pad with the glyph badges and the legend), 1 HUD 2 on the "
     "pad (keyboard and mouse keep the game's HUD), 2 HUD 2 always (default); HUD 2 includes its look over the menus "
     "and rooms (ui_style)"},
    {"hud_fade", settings::Int, &g_opt.hudFade, 0, 1,
     "HUD 2 in flight: 0 full (always shown), 1 contextual (fades out in calm flying; each part comes back for 4 s when "
     "it matters: steering, boost, a target, firing, a maneuver, a new objective; alerts, low shield / hull, missiles "
     "and fights always show)"},
    {"hud_opacity", settings::Int, &g_opt.hudOpacity, 40, 100, "HUD 2's overall opacity in percent"},
    {"hud_scale", settings::Int, &g_opt.hudScale, 80, 130, "HUD 2's size in percent"},
    {"hud_numbers", settings::Int, &g_opt.hudNumbers, 0, 2,
     "the numbers at the ring's shield and hull arcs: 0 only when low, 1 always, 2 never"},
    {"hud_threats", settings::Bool, &g_opt.hudThreats, 0, 1,
     "HUD 2 lists the next hostiles under the target card during a fight"},
    {"hud_context", settings::Int, &g_opt.hudContext, 0, 1,
     "the buttons for what you can do now, under your ship: 0 contextual, 1 off"},
    {"hud_prompts", settings::Int, &g_opt.hudPrompts, 0, 1,
     "when the context prompts show in flight: 0 always, 1 when they change or right after you act (dock, cruise, "
     "a target, a maneuver), then they fade out after 4 seconds"},
    {"hud_alerts", settings::Bool, &g_opt.hudAlerts, 0, 1,
     "short warning cards at the top (shield failing, missile incoming, cargo hold full) with the buttons that answer them"},
    {"hud_info", settings::Bool, &g_opt.hudInfo, 0, 1,
     "extra information when it matters: docking queue, jump countdown, mission timers, formation, tractor"},
    {"hud_wireframes", settings::Bool, &g_opt.hudWireframes, 0, 1,
     "the game's wireframe views in the target card and the ship panel where they can be kept (else flat icons)"},
    {"hud_transition", settings::Int, &g_opt.hudTransition, 0, 400,
     "advanced: how long HUD 2 takes to fade in when you switch to the controller, in milliseconds"},
    // HUD 2's layout presets (docs\UI_HUD2_PRESETS.md 6 step 4, the plumbing): not in UI_SPEC.json's rows yet, so the
    // options page lists it only with show_advanced (raw, CONTROLLER tab) until the presets are released (the owner's
    // frames per preset first: 6 step 8). 0.2.0 caps it at 1: Speedrun (2) and Guided (3) are not built and drew as
    // Standard; raise the cap as each preset's layout lands (hud2_preset.h keeps their ids)
    {"hud_preset", settings::Int, &g_opt.hudPreset, 0, 1,
     "experimental: HUD 2's layout preset - 0 Standard (default: today's HUD 2), 1 Focus (a calmer HUD that shows less "
     "until it matters); Speedrun and Guided are planned"},
    {"maneuver_button", settings::Bool, &g_opt.maneuverButton, 0, 1,
     "the fifth button on the maneuver bar (keyboard and mouse HUD) toggles the autopilot"},
    // HUD 2 over the menus (formerly Extended HUD 2, docs\UI_HUD2_EXTENDED.md 9-10; folded into HUD 2 in 0.2.0): 1 HUD 2
    // (default), 0 CLASSIC; applies only while hud_style is not 0
    {"ui_style", settings::Int, &g_opt.uiStyle, 0, 1,
     "The look of the menus, rooms and dialogs while HUD 2 is on: HUD 2 = its skin over the same screens (default; the "
     "game's controls stay as they are); Classic = the game's own"},
};

static const size_t kOptionCount = sizeof(kOptions) / sizeof(kOptions[0]);

// ---------------------------------------------------------------- the settings bus (the PAGE track, 25 Oct 2026)
// core\settings_bus.h (HostApi v18): the [Controller] and [Autopilot] registries registered with the bus BEFORE the ini
// loads (impl.cpp ImplInit: RegisterBus, then Init -> LoadConfig), so the bus's default snapshot = the struct
// initialisers; the options page lists, changes, resets and saves them through it. Unregistered at Shutdown (the
// pointers die with the logic DLL). Without the bus (an older host) nothing changes: `set` / `ap set` and the ini stay.
static void SaveConfig();
static int g_busController = 0, g_busAutopilot = 0;
static void BusSave(void*) { SaveConfig(); }
static void BusAppliedController(void*, const settings::Def*) {} // `set` has no reactions beyond the value itself
static void BusAppliedAutopilot(void*, const settings::Def* d) { autopilot::OnSettingApplied(d); }
static SettingsBusSection g_busSections[2];
static const SettingsBus* g_bus = nullptr; // the host's, or the page's local one (an older host)

void RegisterBus()
{
    const SettingsBus* bus = HostAtLeast(18) && g_host->GetSettingsBus ? g_host->GetSettingsBus() : nullptr;
    if (!bus)
        bus = options_page::LocalBus();
    g_bus = bus;
    size_t n = 0;
    const settings::Def* apDefs = autopilot::Defs(n);
    g_busSections[0] = {"controller", "Controller", kOptions, kOptionCount, nullptr, BusAppliedController, BusSave,
                        nullptr};
    g_busSections[1] = {"controller", "Autopilot", apDefs, n, nullptr, BusAppliedAutopilot, BusSave, nullptr};
    g_busController = bus->Register(&g_busSections[0]);
    g_busAutopilot = bus->Register(&g_busSections[1]);
    Log("settings bus: Controller %d (%u rows), Autopilot %d (%u rows)", g_busController,
        static_cast<unsigned>(kOptionCount), g_busAutopilot, static_cast<unsigned>(n));
}

void UnregisterBus()
{
    if (g_bus)
    {
        if (g_busController)
            g_bus->Unregister(g_busController);
        if (g_busAutopilot)
            g_bus->Unregister(g_busAutopilot);
    }
    g_busController = g_busAutopilot = 0;
    g_bus = nullptr;
}

static std::string ConfigPath()
{
    return std::string(g_host && g_host->dllDir ? g_host->dllDir : "") + "FLController.ini";
}

static void SaveConfig()
{
    // Rewrite the whole file (keeps the comments and the option order), both sections.
    FILE* f = nullptr;
    if (fopen_s(&f, ConfigPath().c_str(), "w") != 0 || !f)
    {
        Log("config: cannot write %s", ConfigPath().c_str());
        return;
    }
    fprintf(f, "; FLController - native gamepad support for Freelancer.\n");
    fprintf(f, "; Edit while the game is closed (or use the debug command \"config reload\").\n\n");
    settings::WriteCommented(f, "Controller", kOptions, kOptionCount);
    fprintf(f, "; Advanced Autopilot (controller\\docs\\AUTOPILOT.md; `ap settings` lists these in the game).\n");
    autopilot::WriteCommented(f);
    fclose(f);
    Log("config: saved %s", ConfigPath().c_str());
}

static void LoadConfig()
{
    const std::string path = ConfigPath();
    if (GetFileAttributesA(path.c_str()) == INVALID_FILE_ATTRIBUTES)
    {
        SaveConfig(); // first run: write the defaults with comments
        return;
    }
    settings::Load("Controller", path, kOptions, kOptionCount);
    autopilot::Load(path);
    Log("config: loaded %s", path.c_str());
}

std::string SetOption(std::istream& in)
{
    std::string name, value;
    in >> name >> value;
    if (name == "enabled")
    {
        g_opt.enabled = atof(value.c_str()) != 0;
        if (!g_opt.enabled)
            ReleaseAll(); // hand steering, held commands and the cursor back to the game
        return "ok enabled=" + value;
    }
    const settings::Def* d = settings::Find(kOptions, kOptionCount, name);
    if (!d)
        return "unknown option " + name;
    if (value.empty())
        return name + "=" + settings::Value(*d);
    if (!settings::Apply(*d, value))
        return "bad value for " + name + ": " + value + " (kept " + settings::Value(*d) + ")";
    return "ok " + name + "=" + settings::Value(*d) + " (live; 'config save' to keep)";
}

std::string AutopilotCommand(std::istream& in) { return autopilot::Command(in, ConfigPath()); }

std::string ConfigCommand(std::istream& in)
{
    std::string what;
    in >> what;
    if (what == "save")
        SaveConfig();
    else if (what == "reload")
        LoadConfig();
    else if (what == "settings") // every setting of both sections with its value and help (the registry)
        return "[Controller]\n" + settings::Reference(kOptions, kOptionCount) + "\n[Autopilot]\n" +
               autopilot::SettingsReference();
    return ConfigPath() + "\n" + settings::Describe(kOptions, kOptionCount, " = ", "\n") + "\n";
}
} // namespace ctl
