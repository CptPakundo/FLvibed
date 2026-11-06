// modules\controller\src\impl\padmap_zones.h (pure): the map's two zones: one press, one meaning; every chip reachable; nothing stuck.
#include "test.h"

#include "../modules/controller/src/impl/padmap_zones.h"

using namespace padzones;

namespace
{
// the screen's layout: two view chips on the left, a 2 x 2 block and a stepper on the right, a map between them
const Cell kCells[] = {
    {100, 300, 200, 340, 0}, {210, 300, 310, 340, 1},          // SYSTEM, SIRIUS
    {1000, 200, 1100, 240, 4}, {1110, 200, 1210, 240, 5},      // SET WAYPOINT, BEST PATH
    {1000, 250, 1100, 290, 6}, {1110, 250, 1210, 290, 8},      // ZOOM, CLEAR
    {1000, 330, 1210, 370, 23, true, true},                    // the layers' line
    {1000, 380, 1100, 420, 19, false},                         // disabled
};
constexpr int kN = sizeof(kCells) / sizeof(kCells[0]);

In Pad(Dir d) { In i; i.dpad = d; return i; }
} // namespace

void Test_padmap_zones()
{
    Zones z;
    // no chips: one zone, as without the skin, whatever is pressed
    CHECK(z.Step(Pad(Right), nullptr, 0).press == -1 && z.zone == Zone::Canvas);

    // on the canvas the D-pad enters, and A, B and the stick stay the canvas's
    In a;
    a.a = true;
    Out o = z.Step(a, kCells, kN);
    CHECK(!o.ownsA && o.press == -1 && z.zone == Zone::Canvas && z.focus == -1);
    z.Step(Pad(Right), kCells, kN);
    CHECK(z.zone == Zone::Panel && z.focus == 4);
    z.Reset();
    z.Step(Pad(Left), kCells, kN);
    CHECK(z.zone == Zone::Panel && z.focus == 0);

    // the panel: the D-pad walks, A presses the focused chip and is the panel's
    z.Step(Pad(Right), kCells, kN);
    CHECK(z.focus == 1);
    o = z.Step(a, kCells, kN);
    CHECK(o.press == 1 && o.ownsA);
    z.Step(Pad(Right), kCells, kN); // across the map
    CHECK(z.focus == 6 || z.focus == 4);

    // every chip can be reached by the D-pad alone
    bool seen[kN] = {};
    int todo[kN * 4] = {Enter(kCells, kN, Left)}, nTodo = 1;
    while (nTodo)
    {
        const int at = todo[--nTodo];
        if (seen[at])
            continue;
        seen[at] = true;
        for (Dir d : {Up, Down, Left, Right})
        {
            if (kCells[at].stepper && (d == Left || d == Right))
                continue;
            const int next = Walk(kCells, kN, at, d);
            if (next >= 0 && !seen[next])
                todo[nTodo++] = next;
        }
    }
    for (int i = 0; i < kN; ++i)
        CHECK(seen[i]);

    // a stepper takes left and right, up and down leave it
    z.zone = Zone::Panel, z.focus = 23;
    o = z.Step(Pad(Right), kCells, kN);
    CHECK(o.stepId == 23 && o.stepDir == 1 && z.focus == 23);
    o = z.Step(Pad(Left), kCells, kN);
    CHECK(o.stepDir == -1);
    z.Step(Pad(Up), kCells, kN);
    CHECK(z.focus != 23);

    // a disabled chip takes the focus and presses nothing
    z.focus = 19;
    o = z.Step(a, kCells, kN);
    CHECK(o.press == -1 && o.ownsA);

    // the left stick returns, B returns, and the stick's frame gives A back to the canvas
    In st;
    st.stick = true, st.a = true;
    o = z.Step(st, kCells, kN);
    CHECK(z.zone == Zone::Canvas && z.focus == -1 && !o.ownsA && o.press == -1);
    z.Step(Pad(Right), kCells, kN);
    In b;
    b.b = true;
    o = z.Step(b, kCells, kN);
    CHECK(z.zone == Zone::Canvas && o.ownsB);

    // the focused chip leaves with the selection: the nearest takes over, no focus on nothing
    z.Step(Pad(Right), kCells, kN);
    z.focus = 8;
    z.lastX = 1160, z.lastY = 270;
    Cell fewer[kN];
    int n = 0;
    for (const Cell& c : kCells)
        if (c.id != 8)
            fewer[n++] = c;
    z.Step(In(), fewer, n);
    CHECK(z.focus == 5 || z.focus == 6);
    CHECK(Find(fewer, n, z.focus) >= 0);

    // the chips all leave: back on the canvas
    z.Step(In(), nullptr, 0);
    CHECK(z.zone == Zone::Canvas && z.focus == -1);
}
