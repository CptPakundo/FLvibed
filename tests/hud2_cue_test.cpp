// modules\hud\src\hud2_cue.h (pure): the weapon group cue's test, its hold under 3 Hz, its row.
#include "test.h"

#include "../modules/hud/src/hud2_cue.h"

using namespace hud2cue;

void Test_hud2_cue()
{
    const Gun far1 = {600, Ok, -1}, near1 = {300, Ok, -1};
    const Gun mixed[2] = {far1, near1};
    CHECK(Judge(mixed, 2, true, 500) == Lit);    // the group's longest gun, as asked
    CHECK(Judge(mixed, 2, true, 700) == Unlit);
    CHECK(Judge(mixed, 2, false, 500) == Unlit); // not in the centre
    {
        const Gun g[1] = {{600, Refire, -1}};
        CHECK(Judge(g, 1, true, 500) == Lit); // refire is ignored
    }
    {
        const Gun g[1] = {{600, Angle, -1}};
        CHECK(Judge(g, 1, true, 500) == Unlit);
    }
    {
        const Gun g[2] = {{600, Power, -1}, {600, Destroyed, -1}};
        CHECK(Judge(g, 2, true, 500) == Amber); // every weapon of it cannot
    }
    {
        const Gun g[2] = {{600, Power, -1}, {600, Ok, -1}};
        CHECK(Judge(g, 2, true, 500) == Lit);
    }
    {
        const Gun g[1] = {{1000, Ok, 0}};
        CHECK(Judge(g, 1, true, 500) == Amber); // no rounds
    }
    {
        const Gun g[1] = {{600, Cruise, -1}};
        CHECK(Judge(g, 1, true, 500) == Hide);
        const Gun l[1] = {{600, Lane, -1}};
        CHECK(Judge(l, 1, true, 500) == Hide);
    }
    {
        const Gun g[1] = {{0, Ok, -1}};
        CHECK(Judge(g, 1, true, 0) == Unlit); // an unread reach shows the cell, never lit
    }
    CHECK(Judge(nullptr, 0, true, 0) == Unlit);
    // the centre: the lead, or any part of the target
    CHECK(InCentre(100, 0, true, 900, 900, 0, true, 0, 0, 216));
    CHECK(!InCentre(500, 0, true, 900, 0, 10, true, 0, 0, 216));
    CHECK(InCentre(500, 0, false, 500, 0, 300, true, 0, 0, 216));
    // the hold: however fast the wish flips, the state changes at most once per 0.17 s, so a full cycle stays under 3 Hz
    {
        Hold h;
        int changes = 0;
        bool was = false;
        const float dt = 1 / 120.0f;
        for (int i = 0; i < 1200; ++i)
        {
            h.Step((i / 3) % 2 == 0, dt);
            changes += h.lit != was;
            was = h.lit;
        }
        const float hz = changes / 2.0f / (1200 * dt);
        CHECK(hz < 3.0f);
        CHECK(changes > 2);
    }
    // eased, not a step
    {
        Hold h;
        const float first = h.Step(true, 1 / 120.0f);
        CHECK(first > 0 && first < 0.5f);
        for (int i = 0; i < 60; ++i)
            h.Step(true, 1 / 120.0f);
        CHECK(h.shown > 0.99f);
    }
    // the row: centred, the letters a small gap after the digits, five marks within the design's half width
    {
        float xs[kMaxMarks];
        const float half = Layout(3, 2, 1, xs);
        CHECK_NEAR(half, (5 * 0.020 + 3 * 0.003 + 0.009) / 2, 1e-5);
        CHECK_NEAR(xs[0], -xs[4], 1e-5);
        CHECK_NEAR(xs[3] - xs[2], 0.020 + 0.009, 1e-5);
        CHECK_NEAR(xs[1] - xs[0], 0.023, 1e-5);
        CHECK(half <= 0.059f);
        CHECK_NEAR(Layout(0, 0, 1, xs), 0, 1e-6);
        CHECK_NEAR(Layout(1, 0, 1, xs), 0.010, 1e-6);
        CHECK_NEAR(xs[0], 0, 1e-6);
    }
    // the breath rests under fades and none; 1 Hz under full
    CHECK(Breath(0.2f, 1) == 1 && Breath(0.2f, 2) == 1);
    CHECK(Breath(0.5f, 0) > 0.99f && Breath(1.0f, 0) < 0.01f);
}
