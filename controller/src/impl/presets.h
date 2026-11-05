// The pad's flight bindings as data (docs\UI_SPEC.json `bindings`: the presets FREELANCER / ACE / ASSISTED and the
// shared `flight_ap` situation), in the spec's own action vocabulary, and the dispatcher that turns button states into
// tap / hold / double-tap actions (UI_PLAN.md 6.4; UI_BUILD.md HUD track). The tables are read from the spec's
// generated core\ui_meta.h (since 29 Oct 2026; tests\presets_test.cpp). What each action does in the game is controller.cpp's
// (RunPreset): this file knows buttons, timing and the action names only.
#pragma once
#include "common.h"
#include "glyphs.h"

#include <string>
#include <vector>

namespace presets
{
enum class Action : u8
{
    None,
    Fire,
    Missile,
    Torpedo,
    Cruise,
    Afterburner,
    Reverse,
    Steer,    // an axis: the stick that steers
    Throttle, // an axis: the throttle lever (a stick's vertical axis, or the triggers in ACE)
    StrafeUd, // an axis: strafe up / down (and left / right) on a stick
    Aim,      // an axis: the reticle without steering (ASSISTED, turret view)
    Context,  // dock / lane / jump / fly to the target, loot in reach: whatever applies
    FreeFlight,
    Tractor,
    Turret,
    Formation,
    Goto,
    EngineKill,
    TargetPicker, // phase D modal
    ActionWheel,  // phase D modal
    TargetSmart,  // the object under the reticle, else the nearest enemy; again = next; (dbl clear, hold rear)
    TargetNearest,
    NextEnemy,
    ClearTarget,
    Rear,
    Autopilot,
    Hub,
    Pause,
    Nanobot,
    Battery,
    Cm,
    Mine,
    WeaponGroup,
    Handback,
    GoalPanel, // phase D modal
    Accept,
    CruiseDisruptor,
    Scan,
    FireGroup1, // weapon_groups 2: the triggers fire one group each
    FireGroup2,
    COUNT
};
const char* ActionName(Action a); // the spec's string ("target_smart")
Action ActionFromName(const char* name);
// Held-kind actions: active while the button is down (a tap slot) or from the hold threshold to the release (a hold
// slot); the others are one-shot taps.
bool IsHeldKind(Action a);

enum Preset : int
{
    FREELANCER = 0,
    ACE = 1,
    ASSISTED = 2,
    CUSTOM = 3, // = FREELANCER until the bindings editor exists (phase D)
};
const char* PresetName(int preset);

struct Binding
{
    Action tap = Action::None, hold = Action::None, dbl = Action::None;
};
struct Table
{
    Binding b[glyphs::COUNT];
};
// The spec's tables as written.
const Table& Flight(int preset);
const Table& FlightAp(); // common.flight_ap: while the autopilot flies (every preset)
// The flight table with the layout option applied: `weaponGroups` (0 simple: Y is cruise on the tap, nothing held; 2 two groups:
// RT / LT fire group 1 / 2, afterburner moves to the LS click and engine kill is unbound). The autopilot is View held in
// every preset (the alternatives - a stick-click chord, hold Menu - removed 28 Oct 2026: never the owner's choice).
// `swapY` (y_swap): with weapon_groups 1, FREELANCER's Y back to the old order: tap cruise, hold next group.
Table Effective(int preset, int weaponGroups, bool swapY = false);
// The button an action sits on in a table (the first match; slot 0 tap, 1 hold, 2 dbl); false when unbound.
bool Find(const Table& t, Action a, glyphs::Id& btn, int& slot);

// Timing (UI_PLAN 6.4): a hold fires at kHoldSec; a second press within kDblSec of a tap's release is a double tap; a
// tap on a button that has a double-tap action is delivered when the window closes (or dropped for the double).
// A held-kind tap on a button that also has a hold action (FREELANCER's RB: missile / hold torpedo) waits for the
// threshold: released before it = the tap, held for kTapPulseSec (the game reads held commands per frame); reaching it =
// the hold alone. Until 29 Oct 2026 the tap started on the press, so every torpedo also launched a missile (seen in
// game: SaveC706, RB held 0.8 s - torpedo 20 -> 19 and missile 17 -> 16).
constexpr float kHoldSec = 0.4f;
constexpr float kDblSec = 0.3f;
constexpr float kTapPulseSec = 0.1f;

enum class Phase
{
    Tap,
    Dbl,
    HoldStart, // a hold slot reached kHoldSec (one-shot actions act now; held-kind ones start)
    HoldEnd,   // a held-kind action ends (the button released; nothing with holds_as_toggles)
};
struct Event
{
    glyphs::Id btn;
    Action action;
    Phase phase;
};

class Dispatcher
{
  public:
    bool toggles = false; // holds_as_toggles: afterburner, reverse, rear view, turret latch press-once / press-again
    // Once per frame with every button's down state (glyphs::Id order). Events in delivery order.
    void Update(float dt, const Table& t, const bool down[glyphs::COUNT], std::vector<Event>& out);
    // A held-kind action active now (a button down in its slot, or latched).
    bool Held(Action a) const;
    // Drops the pending tap / the hold of a button (a chord consumed it): its release then yields nothing.
    void Cancel(glyphs::Id btn);
    // Releases every hold and latch (a mode change, the device switching away).
    void Reset();
    float HeldFor(glyphs::Id btn) const; // seconds the button has been down (0 up)
    std::string Status() const;

  private:
    struct BtnState
    {
        bool down = false;
        float heldFor = 0;
        bool holdFired = false;
        bool consumed = false; // this press was a double tap's second press (or cancelled): no tap on release
        bool tapPending = false;
        float sinceRelease = 1e9f;
        bool lastWasTap = false;
        Action heldTap = Action::None, heldHold = Action::None; // held-kind actions active from this button
        float pulse = 0; // a deferred held-kind tap still held after its release (kTapPulseSec)
    } s_[glyphs::COUNT];
    bool latch_[static_cast<int>(Action::COUNT)] = {};
    void Start(Action a, glyphs::Id btn, bool holdSlot, Phase phase, std::vector<Event>& out);
};
} // namespace presets
