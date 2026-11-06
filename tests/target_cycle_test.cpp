// The targeting press's enemy rule (core\game\target_cycle.h, the owner, 4 Nov 2026): a cold press takes the
// nearest enemy, presses in a run walk the others once each, a target changed by anything else or a pause starts cold.
#include "../core/game/target_cycle.h"

#include "test.h"

void Test_target_cycle()
{
    using namespace tcycle;
    const Enemy three[3] = {{20, 900}, {10, 300}, {30, 1500}};
    // cold: the nearest, whatever is targeted
    {
        Chain c;
        CHECK(c.Press(0, three, 3, 0) == 10);
    }
    {
        Chain c;
        CHECK(c.Press(0, three, 3, 30) == 10); // a far enemy targeted by other means
    }
    // a cold press on the nearest goes on to the next
    {
        Chain c;
        CHECK(c.Press(0, three, 3, 10) == 20);
    }
    // a run: nearest, then each other one, then it starts over without repeating the current target
    {
        Chain c;
        CHECK(c.Press(0, three, 3, 0) == 10);
        CHECK(c.Press(1, three, 3, 10) == 20);
        CHECK(c.Press(2, three, 3, 20) == 30);
        CHECK(c.Press(3, three, 3, 30) == 10);
    }
    // a pause past the window: cold again
    {
        Chain c;
        CHECK(c.Press(0, three, 3, 0) == 10);
        CHECK(c.Press(1, three, 3, 10) == 20);
        CHECK(c.Press(1 + kChainSecs + 0.1f, three, 3, 20) == 10);
    }
    // the target changed by something else inside the window: cold
    {
        Chain c;
        CHECK(c.Press(0, three, 3, 0) == 10);
        CHECK(c.Press(1, three, 3, 30) == 10);
    }
    // the target died: the next press is the nearest left
    {
        const Enemy left[2] = {{20, 900}, {30, 1500}};
        Chain c;
        CHECK(c.Press(0, three, 3, 0) == 10);
        CHECK(c.Press(1, left, 2, 0) == 20);
    }
    // enemies moved during a run: a visited one that came closer is not taken again
    {
        const Enemy moved[3] = {{20, 900}, {10, 1200}, {30, 100}};
        Chain c;
        CHECK(c.Press(0, three, 3, 0) == 10);
        CHECK(c.Press(1, moved, 3, 10) == 30);
        const Enemy again[3] = {{20, 900}, {10, 50}, {30, 100}};
        CHECK(c.Press(2, again, 3, 30) == 20);
    }
    // the game refused the pick: the next press takes another, with or without a target
    {
        Chain c;
        CHECK(c.Press(0, three, 3, 0) == 10);
        c.Refused(0);
        CHECK(c.Press(1, three, 3, 0) == 20);
    }
    // more enemies than the run's list: presses keep moving
    {
        Enemy many[40];
        for (int i = 0; i < 40; ++i)
            many[i] = {static_cast<unsigned>(100 + i), 100.0f + i};
        Chain c;
        unsigned cur = 0;
        for (int i = 0; i < 80; ++i)
        {
            const unsigned next = c.Press(static_cast<float>(i), many, 40, cur);
            CHECK(next != 0 && next != cur);
            cur = next;
        }
    }
    // nothing to take
    {
        Chain c;
        CHECK(c.Press(0, nullptr, 0, 0) == 0);
        const Enemy one[1] = {{10, 300}};
        CHECK(c.Press(1, one, 1, 10) == 0); // the only enemy is the target
        CHECK(c.Press(2, one, 1, 0) == 10);
    }
}
