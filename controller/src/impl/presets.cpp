#include "presets.h"

#include "log.h"
#include "ui_meta.h" // the spec's binding tables

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

// ---------------------------------------------------------------- the tables
// Straight from the spec (docs\UI_SPEC.json `bindings`, generated into core\ui_meta.h kBind_*): until 29 Oct 2026 this
// file carried a hand transcription of them (identical then). ACE's triggers act as levers: LT throttle down and reverse
// at full pull, RT throttle up and afterburner at full pull (controller.cpp RunPreset). A name the dispatcher does not
// know fails loudly: logged once, that slot stays unbound.
static Action SpecAction(const char* set, const char* button, const char* name)
{
    const Action a = ActionFromName(name);
    if (a == Action::None && name && _stricmp(name, "none") != 0)
        Log("presets: the spec's %s %s action '%s' is unknown to the dispatcher - unbound", set, button, name);
    return a;
}

static Table FromSpec(const char* set, const uimeta::Bind* binds, int count)
{
    Table t;
    if (!binds)
        Log("presets: the spec has no binding table %s - every button unbound", set);
    for (int i = 0; binds && i < count; ++i)
    {
        const glyphs::Id id = glyphs::FromName(binds[i].button);
        if (id == glyphs::COUNT)
        {
            Log("presets: the spec's %s button '%s' is unknown - skipped", set, binds[i].button);
            continue;
        }
        t.b[id] = {SpecAction(set, binds[i].button, binds[i].tap), SpecAction(set, binds[i].button, binds[i].hold),
                   SpecAction(set, binds[i].button, binds[i].dbl)};
    }
    return t;
}

static Table FromSpecPreset(const char* name)
{
    for (const uimeta::BindSet& s : uimeta::kBindingPresets)
        if (_stricmp(s.name, name) == 0)
            return FromSpec(name, s.binds, s.count);
    return FromSpec(name, nullptr, 0);
}

const Table& Flight(int preset)
{
    static const Table freelancer = FromSpecPreset("FREELANCER"), ace = FromSpecPreset("ACE"),
                       assisted = FromSpecPreset("ASSISTED");
    switch (preset)
    {
    case ACE: return ace;
    case ASSISTED: return assisted;
    default: return freelancer;
    }
}

const Table& FlightAp()
{
    static const Table ap = FromSpec("common.flight_ap", uimeta::kBind_common_flight_ap,
                                     static_cast<int>(sizeof(uimeta::kBind_common_flight_ap) / sizeof(uimeta::Bind)));
    return ap;
}

Table Effective(int preset, int weaponGroups, bool swapY)
{
    Table t = Flight(preset);
    // FREELANCER's Y: tap next group, hold cruise (the owner, 5 Nov 2026; GitHub issue 15). y_swap gives the old order
    // back, and without group cycling cruise takes the tap (the hold's weapon_group then drops, below). Only where Y is
    // next group / cruise, so nothing moves in ACE and ASSISTED.
    if ((swapY || weaponGroups != 1) && t.b[glyphs::Y].tap == Action::WeaponGroup && t.b[glyphs::Y].hold == Action::Cruise)
        std::swap(t.b[glyphs::Y].tap, t.b[glyphs::Y].hold);
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
// (not Turret: since 26 Oct 2026 each turret hold toggles the view itself; a latch made every second hold a no-op)
static bool Latchable(Action a)
{
    return a == Action::Afterburner || a == Action::Reverse || a == Action::Rear;
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
        // a deferred held-kind tap's pulse ends (kTapPulseSec after its release)
        if (s.pulse > 0 && (s.pulse -= dt) <= 0 && s.heldTap != Action::None)
        {
            out.push_back({id, s.heldTap, Phase::HoldEnd});
            s.heldTap = Action::None;
        }
        const bool deferTap = IsHeldKind(b.tap) && b.hold != Action::None; // waits for the hold threshold
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
            else if (IsHeldKind(b.tap) && !deferTap)
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
            if (deferTap && !s.holdFired && !s.consumed)
            {
                // released before the hold: the tap now, as a short hold (the game reads held commands per frame)
                if (s.heldTap != Action::None) // a previous pulse still running: end it first
                    out.push_back({id, s.heldTap, Phase::HoldEnd});
                s.heldTap = Action::None;
                Start(b.tap, id, false, Phase::Tap, out);
                s.pulse = kTapPulseSec;
                s.lastWasTap = false;
                continue;
            }
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
    s.pulse = 0;
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
