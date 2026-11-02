// core\game\hudwake.h (pure): HUD 2's CONTEXTUAL mode (hud_fade 1, 0.2.1; the owner, 2 Nov 2026) - idle = nothing shown;
// each trigger wakes the elements it touches for 4 s; FULL / a fight / a modal = everything; low shield / hull and missiles
// keep the ring (and the ship panel) up.
#include "test.h"

#include "game/hudwake.h"

void Test_hudwake()
{
    using namespace hudwake;
    // what each trigger touches
    CHECK(Wakes(Steer) == Ring);
    CHECK(Wakes(Boost) == Ring);
    CHECK(Wakes(Fire) == (Loadout | Ring));
    CHECK(Wakes(TargetChg) == Target);
    CHECK(Wakes(Maneuver) == (Nav | Ship | Ring));
    CHECK(Wakes(Cruise) == (Ring | Ship));
    CHECK(Wakes(Objective) == Nav);
    CHECK(Wakes(Hit) == (Ring | Ship));
    CHECK(Wakes(FireInput) == All); // a fire input's press: the whole HUD (0.2.1 round 6)
    {
        State fs;
        In fi;
        fi.dt = 0.016f;
        CHECK(Step(fs, 1, fi) == 0);
        fi.trig = FireInput; // pressed with nothing able to fire: still everything
        CHECK(Step(fs, 1, fi) == All);
        fi.trig = 0, fi.dt = kLingerS - 0.1f;
        CHECK(Step(fs, 1, fi) == All);
        fi.dt = 0.2f;
        CHECK(Step(fs, 1, fi) == 0);
    }
    // idle in CONTEXTUAL: nothing
    State s;
    In in;
    in.dt = 0.016f;
    CHECK(Step(s, 1, in) == 0);
    // FULL: everything, always
    CHECK(Step(s, 0, in) == All);
    // a steer wakes the ring alone, 4 s, then it goes
    in.trig = Steer;
    CHECK(Step(s, 1, in) == Ring);
    in.trig = 0, in.dt = 3.9f;
    CHECK(Step(s, 1, in) == Ring);
    in.dt = 0.2f;
    CHECK(Step(s, 1, in) == 0);
    // a target switch: the card only; a fire during it adds the loadout and the ring, each with its own timer
    in.trig = TargetChg, in.dt = 0.016f;
    CHECK(Step(s, 1, in) == Target);
    in.trig = 0, in.dt = 2.0f;
    CHECK(Step(s, 1, in) == Target);
    in.trig = Fire, in.dt = 0.016f;
    CHECK(Step(s, 1, in) == (Target | Loadout | Ring));
    in.trig = 0, in.dt = 2.1f; // the card's 4 s are over, the fire's not
    CHECK(Step(s, 1, in) == (Loadout | Ring));
    in.dt = 2.0f;
    CHECK(Step(s, 1, in) == 0);
    // a retrigger restarts the linger
    in.trig = Objective, in.dt = 0.016f;
    CHECK(Step(s, 1, in) == Nav);
    in.trig = 0, in.dt = 3.0f;
    CHECK(Step(s, 1, in) == Nav);
    in.trig = Objective, in.dt = 0.016f;
    CHECK(Step(s, 1, in) == Nav);
    in.trig = 0, in.dt = 3.0f;
    CHECK(Step(s, 1, in) == Nav);
    in.dt = 1.5f;
    CHECK(Step(s, 1, in) == 0);
    // always shown: a fight, a modal (everything); low shield / hull (ring + ship); missiles (ring)
    in.combat = true;
    CHECK(Step(s, 1, in) == All);
    in.combat = false, in.modal = true;
    CHECK(Step(s, 1, in) == All);
    in.modal = false, in.lowShield = true;
    CHECK(Step(s, 1, in) == (Ring | Ship));
    in.lowShield = false, in.lowHull = true;
    CHECK(Step(s, 1, in) == (Ring | Ship));
    in.lowHull = false, in.missiles = true;
    CHECK(Step(s, 1, in) == Ring);
    in.missiles = false;
    CHECK(Step(s, 1, in) == 0);
    // any other mode value reads as CONTEXTUAL (the retired 2 MINIMAL)
    CHECK(Step(s, 2, in) == 0);

    // the mouse cursor: hides after kCursorIdleS of eligible flight without mouse activity, back on any activity;
    // never while not eligible (a menu, the map, the pad...) nor under hud_fade 0
    CursorState cs;
    CHECK(!CursorStep(cs, 1, true, false, 2.9f));
    CHECK(CursorStep(cs, 1, true, false, 0.2f));
    CHECK(CursorStep(cs, 1, true, false, 5.0f));
    CHECK(!CursorStep(cs, 1, true, true, 0.016f)); // a move: shown at once, the count restarts
    CHECK(!CursorStep(cs, 1, true, false, 2.0f));
    CHECK(CursorStep(cs, 1, true, false, 1.1f));
    CHECK(!CursorStep(cs, 1, false, false, 0.016f)); // a menu opens: shown, and the count restarts after it
    CHECK(!CursorStep(cs, 1, true, false, 2.9f));
    CHECK(!CursorStep(cs, 0, true, false, 10.0f)); // FULL: never hidden
    CHECK(!CursorStep(cs, 1, true, false, 2.9f));
    CHECK(CursorStep(cs, 2, true, false, 0.2f)); // other values read as CONTEXTUAL
    CHECK(!CursorStep(cs, 1, true, false, -1.0f)); // a bad dt: shown
}
