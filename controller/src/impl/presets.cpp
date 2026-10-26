#include "presets.h"

#include <sstream>

namespace presets
{
static const char* kActionNames[static_cast<int>(Action::COUNT)] = {
    "none",         "fire",        "missile",     "torpedo",        "cruise",         "afterburner", "reverse",
    "steer",        "throttle",    "strafe_ud",   "aim",            "context",        "free_flight", "tractor",
    "turret",       "formation",   "goto",        "engine_kill",    "target_picker",  "action_wheel",
    "target_smart", "target_nearest", "next_enemy", "clear_target", "rear",           "autopilot",   "hub",
    "pause",        "nanobot",     "battery",     "cm",             "mine",           "weapon_group", "handback",
    "goal_panel",   "accept",      "cruise_disruptor", "scan",      "fire_group1",    "fire_group2"};

const char* ActionName(Action a)
{
    const int i = static_cast<int>(a);
    return i >= 0 && i < static_cast<int>(Action::COUNT) ? kActionNames[i] : "?";
}

Action ActionFromName(const char* name)
{
    for (int i = 0; name && i < static_cast<int>(Action::COUNT); ++i)
        if (_stricmp(kActionNames[i], name) == 0)
            return static_cast<Action>(i);
    return Action::None;
}

bool IsHeldKind(Action a)
{
    switch (a)
    {
    case Action::Fire:
    case Action::Missile:
    case Action::Torpedo:
    case Action::CruiseDisruptor:
    case Action::Afterburner:
    case Action::Reverse:
    case Action::Rear:
    case Action::Turret:
    case Action::ActionWheel:
    case Action::FireGroup1:
    case Action::FireGroup2: return true;
    default: return false;
    }
}

const char* PresetName(int preset)
{
    switch (preset)
    {
    case FREELANCER: return "FREELANCER";
    case ACE: return "ACE";
    case ASSISTED: return "ASSISTED";
    case CUSTOM: return "CUSTOM";
    }
    return "?";
}

// ---------------------------------------------------------------- the tables (docs\UI_SPEC.json bindings, v2)
using A = Action;
#define BIND(id, tap, hold, dbl) t.b[glyphs::id] = {A::tap, A::hold, A::dbl}

static Table MakeFreelancer()
{
    Table t;
    BIND(A, Context, Formation, None);
    BIND(B, FreeFlight, Reverse, None);
    BIND(X, Tractor, Turret, None);
    BIND(Y, Cruise, WeaponGroup, None);
    BIND(LB, TargetPicker, ActionWheel, None);
    BIND(RB, Missile, Torpedo, None);
    BIND(LT, Afterburner, None, None);
    BIND(RT, Fire, None, None);
    BIND(LS, Throttle, None, None);
    BIND(RS, Steer, None, None);
    BIND(L3, EngineKill, None, None);
    BIND(R3, TargetSmart, Rear, ClearTarget);
    BIND(DU, Nanobot, None, None);
    BIND(DD, Battery, None, None);
    BIND(DL, Cm, None, None);
    BIND(DR, Mine, None, None);
    BIND(VIEW, Hub, Autopilot, None);
    BIND(MENU, Pause, None, None);
    return t;
}

static Table MakeAce()
{
    Table t;
    BIND(A, Fire, None, None);
    BIND(B, Missile, None, None);
    BIND(X, Context, Goto, None);
    BIND(Y, TargetNearest, TargetPicker, None);
    BIND(LB, Cm, None, None);
    BIND(RB, NextEnemy, None, None);
    BIND(LT, Reverse, None, None);     // the trigger as a lever: throttle down, reverse at full pull
    BIND(RT, Throttle, Afterburner, None); // throttle up, afterburner at full pull
    BIND(LS, Steer, None, None);
    BIND(RS, StrafeUd, None, None);
    BIND(L3, EngineKill, None, None);
    BIND(R3, NextEnemy, Rear, None);
    BIND(DU, Nanobot, None, None);
    BIND(DD, Battery, None, None);
    BIND(DL, Cruise, None, None);
    BIND(DR, Mine, None, None);
    BIND(VIEW, Hub, Autopilot, None);
    BIND(MENU, Pause, None, None);
    return t;
}

static Table MakeAssisted()
{
    Table t;
    BIND(A, Context, None, None);
    BIND(B, Handback, None, None);
    BIND(X, GoalPanel, None, None);
    BIND(Y, Cruise, ActionWheel, None);
    BIND(LB, None, None, None);
    BIND(RB, Missile, None, None);
    BIND(LT, TargetPicker, None, None);
    BIND(RT, Fire, None, None);
    BIND(LS, None, None, None);
    BIND(RS, Aim, None, None);
    BIND(L3, None, None, None);
    BIND(R3, TargetNearest, Rear, NextEnemy);
    BIND(DU, Nanobot, None, None);
    BIND(DD, Battery, None, None);
    BIND(DL, Cm, None, None);
    BIND(DR, Mine, None, None);
    BIND(VIEW, Hub, Autopilot, None);
    BIND(MENU, Pause, None, None);
    return t;
}

static Table MakeFlightAp()
{
    Table t;
    BIND(A, Accept, None, None);
    BIND(B, Handback, None, None);
    BIND(X, GoalPanel, None, None);
    BIND(Y, Cruise, None, None);
    BIND(LB, TargetPicker, ActionWheel, None);
    BIND(RB, Missile, None, None);
    BIND(LT, Afterburner, None, None);
    BIND(RT, Fire, None, None);
    BIND(LS, Handback, None, None);
    BIND(RS, Handback, None, None);
    BIND(L3, None, None, None);
    BIND(R3, TargetSmart, Rear, ClearTarget);
    BIND(DU, Nanobot, None, None);
    BIND(DD, Battery, None, None);
    BIND(DL, Cm, None, None);
    BIND(DR, Mine, None, None);
    BIND(VIEW, Hub, Autopilot, None);
    BIND(MENU, Pause, None, None);
    return t;
}
#undef BIND

const Table& Flight(int preset)
{
    static const Table freelancer = MakeFreelancer(), ace = MakeAce(), assisted = MakeAssisted();
    switch (preset)
    {
    case ACE: return ace;
    case ASSISTED: return assisted;
    default: return freelancer;
    }
}

const Table& FlightAp()
{
    static const Table ap = MakeFlightAp();
    return ap;
}

Table Effective(int preset, int autopilotPad, int weaponGroups)
{
    Table t = Flight(preset);
    if (autopilotPad == 1) // L3 + R3: a chord the caller detects; no hold
        t.b[glyphs::VIEW].hold = Action::None;
    else if (autopilotPad == 2)
    {
        t.b[glyphs::VIEW].hold = Action::None;
        t.b[glyphs::MENU].hold = Action::Autopilot;
    }
    if (weaponGroups == 0)
    {
        if (t.b[glyphs::Y].hold == Action::WeaponGroup)
            t.b[glyphs::Y].hold = Action::None;
    }
    else if (weaponGroups == 2)
    {
        // the triggers fire one group each (the Armored Core / MechWarrior pattern); the afterburner moves to L3, engine
        // kill goes into the action wheel (phase D: unbound until then)
        for (int i = 0; i < glyphs::COUNT; ++i)
        {
            if (t.b[i].tap == Action::Fire)
                t.b[i].tap = Action::FireGroup1;
            if (t.b[i].hold == Action::WeaponGroup)
                t.b[i].hold = Action::None;
        }
        if (preset == ACE) // A / B fire the groups (UI_PLAN 6.4)
        {
            t.b[glyphs::A].tap = Action::FireGroup1;
            t.b[glyphs::B].tap = Action::FireGroup2;
        }
        else
        {
            t.b[glyphs::LT].tap = Action::FireGroup2;
            t.b[glyphs::L3].tap = Action::Afterburner;
        }
    }
    return t;
}

bool Find(const Table& t, Action a, glyphs::Id& btn, int& slot)
{
    for (int i = 0; i < glyphs::COUNT; ++i)
    {
        if (t.b[i].tap == a)
            return btn = static_cast<glyphs::Id>(i), slot = 0, true;
        if (t.b[i].hold == a)
            return btn = static_cast<glyphs::Id>(i), slot = 1, true;
        if (t.b[i].dbl == a)
            return btn = static_cast<glyphs::Id>(i), slot = 2, true;
    }
    return false;
}

// ---------------------------------------------------------------- the dispatcher
static bool Latchable(Action a)
{
    return a == Action::Afterburner || a == Action::Reverse || a == Action::Rear || a == Action::Turret;
}

void Dispatcher::Start(Action a, glyphs::Id btn, bool holdSlot, Phase phase, std::vector<Event>& out)
{
    if (a == Action::None)
        return;
    if (IsHeldKind(a))
    {
        if (toggles && Latchable(a))
        {
            bool& l = latch_[static_cast<int>(a)];
            l = !l;
            out.push_back({btn, a, l ? Phase::HoldStart : Phase::HoldEnd});
            return;
        }
        (holdSlot ? s_[btn].heldHold : s_[btn].heldTap) = a;
        out.push_back({btn, a, Phase::HoldStart});
        return;
    }
    out.push_back({btn, a, phase});
}

void Dispatcher::Update(float dt, const Table& t, const bool down[glyphs::COUNT], std::vector<Event>& out)
{
    for (int i = 0; i < glyphs::COUNT; ++i)
    {
        BtnState& s = s_[i];
        const Binding& b = t.b[i];
        const glyphs::Id id = static_cast<glyphs::Id>(i);
        const bool wasDown = s.down;
        s.down = down[i];
        if (!wasDown)
            s.sinceRelease += dt;
        // a pending tap: delivered when the double-tap window closes without a second press
        if (s.tapPending && !s.down && s.sinceRelease > kDblSec)
        {
            s.tapPending = false;
            out.push_back({id, b.tap, Phase::Tap});
        }
        if (s.down && !wasDown) // press
        {
            s.heldFor = 0;
            s.holdFired = false;
            s.consumed = false;
            if (b.dbl != Action::None && s.lastWasTap && s.sinceRelease <= kDblSec)
            {
                s.tapPending = false;
                s.consumed = true;
                s.lastWasTap = false;
                out.push_back({id, b.dbl, Phase::Dbl});
            }
            else if (IsHeldKind(b.tap))
                Start(b.tap, id, false, Phase::Tap, out);
            else if (b.tap != Action::None && b.hold == Action::None && b.dbl == Action::None)
            {
                // nothing else can come of this button: act on the press (as the pad did before the presets)
                out.push_back({id, b.tap, Phase::Tap});
                s.consumed = true;
            }
        }
        else if (s.down) // held
        {
            s.heldFor += dt;
            if (!s.holdFired && !s.consumed && b.hold != Action::None && s.heldFor >= kHoldSec)
            {
                s.holdFired = true;
                s.tapPending = false;
                Start(b.hold, id, true, Phase::HoldStart, out);
            }
        }
        else if (wasDown) // release
        {
            s.sinceRelease = 0;
            if (s.heldTap != Action::None)
            {
                out.push_back({id, s.heldTap, Phase::HoldEnd});
                s.heldTap = Action::None;
            }
            if (s.heldHold != Action::None)
            {
                out.push_back({id, s.heldHold, Phase::HoldEnd});
                s.heldHold = Action::None;
            }
            if (s.holdFired || s.consumed || IsHeldKind(b.tap) || b.tap == Action::None)
                s.lastWasTap = false;
            else
            {
                s.lastWasTap = true;
                if (b.dbl != Action::None)
                    s.tapPending = true; // the window decides: a tap, or the first half of a double
                else
                    out.push_back({id, b.tap, Phase::Tap});
            }
        }
    }
}

bool Dispatcher::Held(Action a) const
{
    if (a == Action::None)
        return false;
    if (latch_[static_cast<int>(a)])
        return true;
    for (const BtnState& s : s_)
        if (s.heldTap == a || s.heldHold == a)
            return true;
    return false;
}

void Dispatcher::Cancel(glyphs::Id btn)
{
    BtnState& s = s_[btn];
    s.consumed = true;
    s.tapPending = false;
    s.lastWasTap = false;
    s.heldTap = s.heldHold = Action::None;
}

void Dispatcher::Reset()
{
    for (BtnState& s : s_)
        s = BtnState();
    memset(latch_, 0, sizeof(latch_));
}

float Dispatcher::HeldFor(glyphs::Id btn) const { return s_[btn].down ? s_[btn].heldFor : 0.0f; }

std::string Dispatcher::Status() const
{
    std::ostringstream o;
    o << "held:";
    for (int i = 0; i < glyphs::COUNT; ++i)
    {
        if (s_[i].heldTap != Action::None)
            o << " " << glyphs::Name(static_cast<glyphs::Id>(i)) << "=" << ActionName(s_[i].heldTap);
        if (s_[i].heldHold != Action::None)
            o << " " << glyphs::Name(static_cast<glyphs::Id>(i)) << "+=" << ActionName(s_[i].heldHold);
    }
    o << " latched:";
    for (int i = 0; i < static_cast<int>(Action::COUNT); ++i)
        if (latch_[i])
            o << " " << ActionName(static_cast<Action>(i));
    return o.str();
}
} // namespace presets
