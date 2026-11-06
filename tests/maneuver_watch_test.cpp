// The maneuver watchdog's rule (core\game\maneuver_watch.h, 0.2.1 hotfix): stalled = slower than 5 m/s for
// 3 s while the leader / goal is > 150 m away and not getting closer; one snapshot, re-issues at most every 10 s, 3 per
// stall; a steering player stops the clock; the autopilot, no maneuver, moving or arrived reset it.
#include "../core/game/maneuver_watch.h"

#include "test.h"

void Test_maneuver_watch()
{
    using namespace mwatch;
    auto at = [](float t, float speed, float dist) {
        Sample s;
        s.t = t, s.maneuver = true, s.key = 10, s.speed = speed, s.dist = dist;
        return s;
    };
    // the player's M01 case: formation, stopped at 0 m/s, the convoy pulling away
    {
        Watch w;
        CHECK(!w.Step(at(0, 1, 200)).snapshot);
        CHECK(!w.Step(at(2.9f, 0, 260)).retry);
        Decision d = w.Step(at(3.0f, 0, 320));
        CHECK(d.snapshot && d.retry && d.attempt == 1);
        d = w.Step(at(5, 0, 400));
        CHECK(!d.snapshot && !d.retry); // < 10 s since the try
        d = w.Step(at(13, 0, 500));
        CHECK(d.retry && d.attempt == 2 && !d.snapshot);
        d = w.Step(at(23, 0, 600));
        CHECK(d.retry && d.attempt == 3);
        d = w.Step(at(30, 0, 700));
        CHECK(!d.retry && !d.gaveUp);
        d = w.Step(at(33, 0, 800));
        CHECK(!d.retry && d.gaveUp);
        CHECK(!w.Step(at(50, 0, 900)).gaveUp); // once
        // it moves again: the stall is over, a new one gets fresh tries
        CHECK(!w.Step(at(51, 40, 900)).retry && !w.Stalled());
        CHECK(!w.Step(at(52, 0, 900)).retry);
        CHECK(w.Step(at(55, 0, 900)).retry);
    }
    // a dock the game answered "wait": it holds the ship itself, so no stall, and the watch starts over when the wait ends
    {
        Watch w;
        Sample s = at(0, 0, 900);
        s.queued = true;
        for (float t = 0; t < 30; t += 1)
        {
            s.t = t;
            CHECK(!w.Step(s).snapshot && !w.Step(s).retry);
        }
        CHECK(!w.Stalled());
        CHECK(!w.Step(at(31, 0, 900)).retry);
        CHECK(w.Step(at(34, 0, 900)).retry);
    }
    // close to the leader (a stopped convoy, the ship in its slot): never a stall
    {
        Watch w;
        for (float t = 0; t < 20; t += 0.5f)
            CHECK(!w.Step(at(t, 0, 120)).retry);
    }
    // slow but closing in (a dock approach): no stall while the distance falls
    {
        Watch w;
        float dist = 1000;
        for (float t = 0; t < 20; t += 0.5f, dist -= 15)
            CHECK(!w.Step(at(t, 3, dist)).retry);
    }
    // the player steers: the clock stops, and starts afresh after
    {
        Watch w;
        w.Step(at(0, 0, 400));
        Sample s = at(2, 0, 400);
        s.steering = true;
        for (float t = 2; t < 10; t += 0.5f)
        {
            s.t = t;
            CHECK(!w.Step(s).retry);
        }
        CHECK(!w.Step(at(10, 0, 400)).retry);
        CHECK(!w.Step(at(12.9f, 0, 400)).retry);
        CHECK(w.Step(at(13, 0, 400)).retry);
    }
    // the autopilot flies, or no maneuver: nothing, and the watch resets
    {
        Watch w;
        Sample s = at(0, 0, 400);
        s.autopilot = true;
        for (float t = 0; t < 10; t += 0.5f)
        {
            s.t = t;
            CHECK(!w.Step(s).retry);
        }
        Sample n;
        n.t = 11;
        CHECK(!w.Step(n).retry);
        CHECK(!w.Step(at(12, 0, 400)).retry);
        CHECK(w.Step(at(15, 0, 400)).retry);
    }
    // another maneuver (new leader / goal): fresh clock
    {
        Watch w;
        w.Step(at(0, 0, 400));
        Sample s = at(2.5f, 0, 400);
        s.key = 11;
        CHECK(!w.Step(s).retry);
        s.t = 5;
        CHECK(!w.Step(s).retry);
        s.t = 5.5f;
        CHECK(w.Step(s).retry);
    }
}
