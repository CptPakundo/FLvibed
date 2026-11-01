// Which XInput slot the pad is read from (controller\src\impl\pad_slots.h, T36 slice 1): the first slot that answers,
// kept while it answers; an unplug rescans at once, then every kRescan seconds; a replug on another slot.
#include "../controller/src/impl/pad_slots.h"

#include "test.h"

void Test_pad_slots()
{
    using namespace padslots;
    bool plugged[kSlots] = {};
    int polls[kSlots] = {};
    auto get = [&](int i) {
        ++polls[i];
        return plugged[i];
    };
    auto clearPolls = [&] {
        for (int& p : polls)
            p = 0;
    };
    Scan s;
    Event ev, ev2;
    const float dt = 1.0f / 60;

    // nothing plugged: the first frame scans every slot, then nothing is polled until the rescan
    CHECK(!s.Read(get, dt, ev, ev2) && ev == Event::None && s.Index() == -1);
    CHECK(polls[0] == 1 && polls[3] == 1);
    clearPolls();
    for (int f = 0; f < 60; ++f)
        s.Read(get, dt, ev, ev2);
    CHECK(polls[0] == 0 && polls[2] == 0); // one second: no polling of empty slots

    // a pad plugged into slot 2 is found at the next scan (<= kRescan), alone on its slot from then on
    plugged[2] = true;
    float waited = 0;
    bool found = false;
    while (waited < kRescan + 0.1f && !found)
    {
        found = s.Read(get, dt, ev, ev2);
        waited += dt;
    }
    CHECK(found && ev == Event::Connected && s.Index() == 2 && waited <= kRescan + dt);
    clearPolls();
    CHECK(s.Read(get, dt, ev, ev2) && ev == Event::None && polls[2] == 1 && polls[0] == 0);

    // a second pad on slot 0 does not take over while slot 2 answers
    plugged[0] = true;
    CHECK(s.Read(get, dt, ev, ev2) && s.Index() == 2);

    // slot 2 unplugged: disconnected and, in the same call, slot 0 found
    plugged[2] = false;
    CHECK(s.Read(get, dt, ev, ev2) && ev == Event::Disconnected && ev2 == Event::Connected && s.Lost() == 2 &&
          s.Index() == 0);

    // every pad unplugged: disconnected, the immediate rescan finds none, then the rescan waits again
    plugged[0] = false;
    clearPolls();
    CHECK(!s.Read(get, dt, ev, ev2) && ev == Event::Disconnected && ev2 == Event::None && s.Index() == -1);
    CHECK(polls[0] == 2 && polls[1] == 1); // the lost slot once, then the scan over all four
    clearPolls();
    CHECK(!s.Read(get, dt, ev, ev2) && polls[0] == 0);

    // replugged into the same slot: found again at the next scan
    plugged[0] = true;
    waited = 0, found = false;
    while (waited < kRescan + 0.1f && !found)
    {
        found = s.Read(get, dt, ev, ev2);
        waited += dt;
    }
    CHECK(found && ev == Event::Connected && s.Index() == 0);
}
