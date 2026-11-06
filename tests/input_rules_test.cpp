// core\game\input_rules.h (pure): each rule against the expression the frame used, written out here, over every case. A
// difference is a change of behaviour. The call sites are not seen from here: the legs and tests\input_owner.ps1 cover those.
#include "test.h"

#include <algorithm>
#include <cmath>

#include "../core/game/input_rules.h"

using namespace inputrules;

static bool OldTakesBack(float lx, float ly, float rx, float ry, bool rightAims, float cancel)
{
    const float stick = std::max(rightAims ? 0.0f : sqrtf(rx * rx + ry * ry), sqrtf(lx * lx + ly * ly));
    return cancel > 0 && stick > cancel;
}

void Test_input_rules()
{
    for (int a = 0; a < 2; ++a)
        for (int p = 0; p < 2; ++p)
        {
            const bool ap = a != 0, pad = p != 0;
            CHECK(Want(Channel::Fire, ap, pad) == (ap || pad));        // (fireAll && !slotsKnown) || Held(Fire)
            CHECK(Want(Channel::Afterburner, ap, pad) == (ap || pad)); // o.afterburn || Held(Afterburner)
            CHECK(Want(Channel::Reverse, ap, pad) == ap);              // reverse
            CHECK(Want(Channel::Slide, ap, pad) == ap);                // o.slide == n
            CHECK(Want(Channel::Missile, ap, pad) == pad);             // Held(Missile); the autopilot taps apart
            CHECK(Want(Channel::Rear, ap, pad) == pad);                // Held(Rear) || wheel::RearView()
            for (Channel c : {Channel::Fire, Channel::Missile, Channel::Afterburner, Channel::Reverse, Channel::Rear, Channel::Slide})
                CHECK(Want(c, ap, pad) == (ClaimedBy(c, ap, pad) != OwnerNone)); // whoever a command is held for owns it
        }
    CHECK(ClaimedBy(Channel::Fire, true, true) == OwnerAutopilot);
    CHECK(ClaimedBy(Channel::Fire, false, true) == OwnerPad);
    CHECK(ClaimedBy(Channel::Fire, false, false) == OwnerNone);
    CHECK(ClaimedBy(Channel::Reverse, false, true) == OwnerNone); // not heard
    CHECK(ClaimedBy(Channel::Missile, true, true) == OwnerPad);

    // the hand-back over a grid of sticks, the threshold's own edge and `autopilot_cancel` 0 included
    const float vals[] = {-1.0f, -0.7f, -0.3f, -0.25f, 0.0f, 0.1f, 0.25f, 0.3f, 0.6f, 1.0f};
    const float cancels[] = {0.0f, 0.1f, 0.25f, 0.3f, 0.6f, 1.0f, 1.5f};
    for (float lx : vals)
        for (float ly : vals)
            for (float rx : vals)
                for (float ry : vals)
                    for (int aims = 0; aims < 2; ++aims)
                        for (float cancel : cancels)
                            CHECK(TakesBack({lx, ly, rx, ry, aims != 0}, cancel) == OldTakesBack(lx, ly, rx, ry, aims != 0, cancel));
    CHECK(!TakesBack({0.25f, 0, 0, 0, false}, 0.25f)); // strictly past it
    CHECK(TakesBack({0, 0, 1, 0, false}, 0.5f));
    CHECK(!TakesBack({0, 0, 1, 0, true}, 0.5f));       // the right stick aims
    CHECK(!TakesBack({1, 0, 1, 0, false}, 0.0f));      // 0: never

    // the page's latch, as the two ifs stood
    for (int wants = 0; wants < 2; ++wants)
        for (int button = 0; button < 2; ++button)
            for (int was = 0; was < 2; ++was)
            {
                bool latch = was != 0, oldLatch = was != 0, oldStop = false;
                if (wants)
                {
                    oldLatch = true;
                    oldStop = true;
                }
                else if (oldLatch)
                {
                    if (button)
                        oldStop = true;
                    else
                        oldLatch = false;
                }
                CHECK(PageHolds(wants != 0, button != 0, latch) == oldStop);
                CHECK(latch == oldLatch);
            }

    // who gets the frame: `autoFlies && Space` first, then `!padOwns`
    for (int f = 0; f < 2; ++f)
        for (int s = 0; s < 2; ++s)
            for (int o = 0; o < 2; ++o)
            {
                Frame old = Frame::Pad; // the two ifs as they stood
                if (f && s)
                    old = Frame::Flight;
                else if (!o)
                    old = Frame::Device;
                CHECK(WhoHasTheFrame(f != 0, s != 0, o != 0, false) == old);
            }
    CHECK(WhoHasTheFrame(true, true, false, false) == Frame::Flight); // a keyboard start flies past the device's row
    CHECK(WhoHasTheFrame(true, false, false, false) == Frame::Device);
    CHECK(WhoHasTheFrame(true, false, true, true) == Frame::Window); // under a window, whoever owns the device
    CHECK(WhoHasTheFrame(true, false, false, true) == Frame::Window);

    // the autopilot keeps working if the game isn't paused: a window or the page never stops it, the pause always does
    CHECK(FliesUnderWindow(true, true, false, false, false));   // a NeuroNet window
    CHECK(FliesUnderWindow(true, false, true, true, false));    // the page over the flight
    CHECK(FliesUnderWindow(true, true, true, false, false));    // the page over a window
    CHECK(!FliesUnderWindow(true, true, false, false, true));   // paused
    CHECK(!FliesUnderWindow(true, false, true, true, true));
    CHECK(!FliesUnderWindow(false, true, false, false, false)); // nothing to fly
    CHECK(!FliesUnderWindow(true, false, true, false, false));  // the page in a base or a menu
    CHECK(!FliesUnderWindow(true, false, false, true, false));  // plain flight: the frame's own row

    CHECK(kKeyChain[0] == KeyStep::Fluf);
    CHECK(kKeyChain[1] == KeyStep::Page);
    CHECK(kKeyChain[2] == KeyStep::GoalPanel);
    CHECK(kKeyChain[3] == KeyStep::Modules);
    CHECK(kKeyChain[4] == KeyStep::BarButton);
}
