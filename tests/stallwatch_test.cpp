// modules\controller\src\impl\stallwatch.h (pure): a ship that stays put for 3 s of game time stalls, once; slow drift inside 2 m
// is a stall too, a steady 1 m/s is not; leaving, or being gone, ends it; a gap (a load, a pause) starts over.
#include "test.h"

#include "../modules/npc/src/stallwatch.h"

using namespace stallwatch;

static Sample At(unsigned id, float x)
{
    Sample s;
    s.id = id;
    s.pos[0] = x;
    return s;
}

void Test_stallwatch()
{
    {
        Watch w;
        std::vector<Event> ev;
        for (int i = 0; i <= 5; ++i) // still for 2.5 s: nothing
            w.Update(i * 0.5, {At(1, 100)}, ev);
        CHECK(ev.empty());
        w.Update(3.0, {At(1, 100.5f)}, ev); // 3 s, moved under 2 m: stalled
        CHECK(ev.size() == 1 && ev[0].began && ev[0].id == 1 && ev[0].seconds >= kStallSec);
        ev.clear();
        w.Update(3.5, {At(1, 100)}, ev); // reported once
        CHECK(ev.empty());
        w.Update(4.0, {At(1, 130)}, ev); // it left
        CHECK(ev.size() == 1 && !ev[0].began && !ev[0].gone && ev[0].seconds >= 3.9);
    }
    {
        Watch w; // 1 m/s: past 2 m every 2.x s, never 3 s at one point
        std::vector<Event> ev;
        for (int i = 0; i < 40; ++i)
            w.Update(i * 0.5, {At(1, i * 0.5f)}, ev);
        CHECK(ev.empty());
    }
    {
        Watch w; // destroyed while stalled: over, gone; a ship that never stalled ends silently
        std::vector<Event> ev;
        for (int i = 0; i <= 7; ++i)
            w.Update(i * 0.5, {At(1, 0), At(2, i * 50.0f)}, ev);
        CHECK(ev.size() == 1 && ev[0].id == 1 && ev[0].began);
        ev.clear();
        w.Update(4.0, {}, ev);
        CHECK(ev.size() == 1 && ev[0].id == 1 && !ev[0].began && ev[0].gone);
    }
    {
        Watch w; // a long gap starts over: no stall out of the time a load or pause took
        std::vector<Event> ev;
        w.Update(0, {At(1, 0)}, ev);
        w.Update(10, {At(1, 0)}, ev);
        CHECK(ev.empty());
        w.Update(10.5, {At(1, 0)}, ev);
        CHECK(ev.empty());
    }
}
