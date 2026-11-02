// FLvibed - HUD 2's CONTEXTUAL mode (0.2.1, the owner's decisions of 2 Nov 2026; docs\UI_HUD2.md 5.25). `hud_fade` has
// two modes: 0 FULL (every element always shown) and 1 CONTEXTUAL (the default; the old 1 "calm dims" and 2 "minimal"
// retired, 2 falls back to 1). CONTEXTUAL: in flight HUD 2 fades out while idle / in simple flying; each element wakes on
// its own triggers, lingers kLingerS after the last one, then fades (the caller's ease). Always shown whatever the
// timers: a fight (the whole combat set while hostiles are engaged), a modal / menu; low shield / hull and incoming
// missiles keep the ring and the ship panel up. (Alerts are their own element and always show; the context prompts follow
// hud_prompts: core\game\promptshow.h.) Pure: the caller passes the frame's triggers and dt. Test: tests\hudwake_test.cpp.
#pragma once

namespace hudwake
{
constexpr float kLingerS = 4.0f;

enum Elem : unsigned
{
    Ring = 1u << 0,    // the reticle ring + the speed / throttle / afterburner readout inside it
    Ship = 1u << 1,    // the ship panel (shield, hull, the maneuver's state, escorts)
    Loadout = 1u << 2, // the loadout (weapons, launcher, consumables)
    Target = 1u << 3,  // the target card + the threat list
    Nav = 1u << 4,     // the top-left nav / objectives readout
    All = (1u << 5) - 1
};

enum Trig : unsigned
{
    Steer = 1u << 0,     // steering / maneuvering input past a small threshold
    Boost = 1u << 1,     // afterburner / boost
    Fire = 1u << 2,      // weapons fire, a launcher, a consumable used (the loadout's counts changed)
    TargetChg = 1u << 3, // a target switch / clear
    Maneuver = 1u << 4,  // goto, dock, formation, free flight (the wheel's maneuvers too)
    Cruise = 1u << 5,    // cruise on / off, engine kill
    Objective = 1u << 6, // the objective / waypoint changed
    Hit = 1u << 7,       // the ship was hit (the shield / hull fell)
    FireInput = 1u << 8, // a fire input PRESSED (guns, a launcher, CM / mine / torpedo, fire-all, a group's trigger) -
                         // the press itself, whether or not anything fires (locked, empty): the whole HUD (0.2.1 round 6)
};

// What each trigger wakes (the owner: "the elements they touch").
inline unsigned Wakes(unsigned trig)
{
    unsigned m = 0;
    if (trig & Steer)
        m |= Ring;
    if (trig & Boost)
        m |= Ring;
    if (trig & Fire)
        m |= Loadout | Ring;
    if (trig & TargetChg)
        m |= Target;
    if (trig & Maneuver)
        m |= Nav | Ship | Ring;
    if (trig & Cruise)
        m |= Ring | Ship;
    if (trig & Objective)
        m |= Nav;
    if (trig & Hit)
        m |= Ring | Ship;
    if (trig & FireInput)
        m |= All;
    return m;
}

struct In
{
    unsigned trig = 0;      // this frame's triggers (Trig bits)
    bool combat = false;    // hostiles engaged: the combat set stays up while the fight lasts
    bool modal = false;     // a modal / menu over the flight
    bool lowShield = false; // low shield / hull: the ring and the ship panel stay up
    bool lowHull = false;
    bool missiles = false;  // incoming missiles: the ring stays up
    float dt = 0;
};

struct State
{
    float left[5] = {}; // seconds each element still shows (Ring .. Nav)
};

// One frame: mode = hud_fade (0 FULL, 1 CONTEXTUAL; anything else = 1). Returns the elements shown (Elem bits).
inline unsigned Step(State& s, int mode, const In& in)
{
    const unsigned woke = Wakes(in.trig);
    unsigned shown = 0;
    for (unsigned i = 0; i < 5; ++i)
    {
        if (woke & (1u << i))
            s.left[i] = kLingerS;
        else
            s.left[i] = s.left[i] > in.dt ? s.left[i] - in.dt : 0.0f;
        if (s.left[i] > 0)
            shown |= 1u << i;
    }
    if (mode == 0 || in.combat || in.modal)
        return All;
    if (in.lowShield || in.lowHull)
        shown |= Ring | Ship;
    if (in.missiles)
        shown |= Ring;
    return shown;
}

// The game's mouse cursor under CONTEXTUAL (0.2.1, the owner: "100% full contextual hiding and re-appearance"): in flight
// it hides after kCursorIdleS without mouse movement or a mouse button, and shows again the frame any comes. `eligible` =
// the caller's whole context check (flight, HUD 2 up, no menu / map / dialog / options page / FLUF window, no mouse
// flight engaged; the pad in control counts too since 0.2.1 round 6 - its activity is only a real mouse, which hands the
// device back to the keyboard and mouse): anything else shows it and restarts the count. Fails closed: hud_fade 0 (or
// below) never hides. Returns true = hide.
constexpr float kCursorIdleS = 3.0f;

struct CursorState
{
    float idle = 0; // seconds of eligible flight without mouse activity
};

inline bool CursorStep(CursorState& s, int mode, bool eligible, bool activity, float dt)
{
    if (mode <= 0 || !eligible || activity || !(dt >= 0))
    {
        s.idle = 0;
        return false;
    }
    s.idle += dt;
    return s.idle >= kCursorIdleS;
}
} // namespace hudwake
