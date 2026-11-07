// What the controller's files share: the options, the test bot, the pad's state and the names they call across. Private to controller*.cpp.
#pragma once

namespace ctl
{
using gamemode::Mode;

static const char* ModeName(Mode m) { return gamemode::Name(m); }
// the frame is core's (core\game\coreframe.h): its mode, its count, the flight HUD's windows
static const Mode& g_mode = coreframe::g_mode;
static const u32& g_frame = coreframe::g_frame;
static std::vector<uintptr_t>& g_hudRoots = coreframe::g_hudRoots;
// ---------------------------------------------------------------- command helpers (core\game\flightio.h)
using flightio::Claim;
using flightio::Hold;
using flightio::ReleaseAll;
using flightio::ReleaseSteer;
using flightio::SetSteer;
using flightio::Tap;
using flightio::TickSteerRelease;
using leadchase::Pursue;
using leadchase::Pursuit;
using leadchase::PursuitOut;
struct Options : sharedrows::Values
{
    bool focusOnly = true;    // ignore the real pad while Freelancer is not the foreground window
    float cursorSpeed = 1.1f; // screen heights per second at full stick deflection
    float wheelRate = 10.0f;  // wheel notches per second at full deflection
    float steerExpo = 1.5f;   // steering response curve exponent (1 = linear)
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
    float aimSmooth = 0.06f;       // aim cursor smoothing time constant (s); removes lead-cross jitter
    int autoTarget = 0;            // see kOptions (default: the player always picks targets)
    bool fireTarget = true;        // see kOptions
    // The UI build (docs\UI_SPEC.json, the defaults = the spec's `def`): rows the options page shows and
    // the device service / glyphs / pad HUD / presets / LiveSplit client read as they are built (the codings: kOptions).
    int inputMode = 0;             // 0 automatic, 1 controller, 2 keyboard and mouse
    int glyphStyle = 0;            // 0 automatic (by the pad's HID ids), 1 Xbox, 2 PlayStation, 3 Nintendo
    int confirmButton = 0;         // 0 the bottom face button (A / cross / B), 1 the right one (Nintendo)
    bool pauseOnDisconnect = true; // a pad unplugged in flight pauses the game
    int padPreset = 0;             // 0 FREELANCER, 1 ACE, 2 ASSISTED, 3 CUSTOM (ui_meta.h kControllerPresets)
    int weaponGroups = 1;          // 0 simple (one fire button), 1 cycle on Y hold, 2 two groups on the triggers
    bool swapY = false;            // y_swap: with weapon_groups 1, Y tap = cruise, Y hold = next group (the old order)
    bool holdsAsToggles = false;   // afterburner, reverse, drift, rear view as press-once / press-again
    int rumble = 100;              // vibration strength in percent (0 = off)
    int triggerEffects = 2;        // 0 off, 1 light, 2 full (docs\HAPTICS.md 10)
    int triggerStrength = 100;     // percent
    int hapticsMode = 0;           // 0 automatic, 1 motors, 2 waveform
    int hapticsClass[5] = {100, 100, 100, 100, 100}; // ship, weapons, danger, set pieces, prompts: percent
    int hapticsDevice = 1;         // 0: the recording alone
    int padCursor = 2;             // 0 off, 1 rooms and maps, 2 everywhere (the owner's decision 15)
    int cursorMagnet = 60;         // the pad cursor's slow-down / pull over clickable elements, percent
    bool mapTouchpad = true;       // the touchpad's gestures on the nav map
    int promptBar = 0;             // 0 contextual, 1 always, 2 off
    int worldPrompts = 0;          // 0 contextual, 1 off
    bool hudGlyphs = true;         // glyph badges on the HUD's own elements in pad mode
    int buttonLegend = 0;          // 0 on, 1 off (the contextual button legend)
    bool apStatusLine = true;      // the autopilot's one status line under the maneuver bar
};
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
// controller.cpp: the pad's state and the frame's gates
extern Options g_opt;
extern Gamepad g_pad;
extern Gamepad g_menuPad; // the menus' view of the pad (controller.cpp)
extern presets::Dispatcher g_dispatch;
extern presets::Table g_table;
extern int g_weaponGroup;
extern bool g_turret;
extern bool g_apUsed;
extern PressGate<glyphs::COUNT> g_gateFlight, g_gateFluf, g_gateUi;

// controller_flight.cpp
void PadInputs(bool in[glyphs::COUNT], const Gamepad& pad = g_pad);
extern float& g_lastSteerX;
extern float& g_lastSteerY;
extern Bot g_bot;
void StopBot(const char* why);
void CloseModals();
void UpdateSpace(float dt);

// controller_menus.cpp
extern std::vector<uintptr_t> g_prevRoots;
void UpdateUiNav(float dt);
void UpdateFreeCursor(float dt);
void UpdateFlufPad(float dt);
void EndFlufPad();

// controller_settings.cpp
void LoadConfig();
} // namespace ctl
