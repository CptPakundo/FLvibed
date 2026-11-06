// core\ui\winfit.h (pure): panels fitted to what they hold, in the states the windows really take.
#include "test.h"

#include "../core/ui/winfit.h"

using namespace winfit;

namespace
{
const Rules kR = {0.018f, 0.012f, 0.05f, 0.12f, 0.15f};

Panel Make(float x0, float y0, float x1, float y1, Kind k)
{
    Panel p;
    p.plate = {x0, y0, x1, y1};
    p.kind = k;
    return p;
}
bool Near(float a, float b) { return a - b < 0.0001f && b - a < 0.0001f; }
}

void Test_winfit()
{
    { // three panels side by side, a little uneven: they end level, under the tallest content
        std::vector<Panel> ps = {Make(0, .2f, 1, .66f, Kind::Hug), Make(1.1f, .2f, 2, .68f, Kind::Hug), Make(2.1f, .2f, 3, .63f, Kind::Hug)};
        Add(ps, {.1f, .3f, .9f, .62f}, kR.reach);
        Add(ps, {1.2f, .3f, 1.9f, .64f}, kR.reach);
        Add(ps, {2.2f, .3f, 2.9f, .60f}, kR.reach);
        Solve(ps, kR);
        CHECK(Near(ps[0].plate.y1, .658f) && Near(ps[1].plate.y1, .658f) && Near(ps[2].plate.y1, .658f));
    }
    { // far apart: each ends at its own content, no panel is stretched empty
        std::vector<Panel> ps = {Make(0, .2f, 1, .66f, Kind::Hug), Make(1.1f, .2f, 2, .66f, Kind::Hug)};
        Add(ps, {.1f, .25f, .9f, .30f}, kR.reach);
        Add(ps, {1.2f, .25f, 1.9f, .60f}, kR.reach);
        Solve(ps, kR);
        CHECK(Near(ps[0].plate.y1, .318f) && Near(ps[1].plate.y1, .618f));
    }
    { // a panel with nothing measured keeps what the table asks
        std::vector<Panel> ps = {Make(0, .2f, 1, .5f, Kind::Hug)};
        Solve(ps, kR);
        CHECK(Near(ps[0].plate.y1, .5f));
    }
    { // a button under the panel is the panel's: it grows round it, and the panel under moves down
        std::vector<Panel> ps = {Make(0, .2f, 1, .6f, Kind::Hug), Make(0, .75f, 1, 1.0f, Kind::Under)};
        Add(ps, {.1f, .3f, .9f, .59f}, kR.reach);
        Add(ps, {.4f, .62f, .6f, .68f}, kR.reach);
        Solve(ps, kR);
        CHECK(Near(ps[0].plate.y1, .698f));
        CHECK(Near(ps[1].plate.y0, .71f) && Near(ps[1].plate.y1, 1.0f));
    }
    { // the panel under takes the room left, keeps its foot, and never closes up
        std::vector<Panel> ps = {Make(0, .2f, 1, .6f, Kind::Hug), Make(0, .62f, 1, .8f, Kind::Under)};
        Add(ps, {.1f, .3f, .9f, .40f}, kR.reach);
        Solve(ps, kR);
        CHECK(Near(ps[1].plate.y0, .43f));
        std::vector<Panel> qs = {Make(0, .2f, 1, .6f, Kind::Hug), Make(0, .62f, 1, .8f, Kind::Under)};
        Add(qs, {.1f, .3f, .9f, .75f}, kR.reach);
        Solve(qs, kR);
        CHECK(Near(qs[1].plate.y0, .65f));
    }
    { // a kept panel (a 3D view): its neighbours end level with it when close, and it never moves
        std::vector<Panel> ps = {Make(0, .2f, 1, .68f, Kind::Level), Make(1.1f, .2f, 2, .6f, Kind::Hug)};
        Add(ps, {.1f, .25f, .9f, .3f}, kR.reach);
        Add(ps, {1.2f, .25f, 1.9f, .63f}, kR.reach);
        Solve(ps, kR);
        CHECK(Near(ps[0].plate.y1, .68f) && Near(ps[1].plate.y1, .68f));
    }
    { // a collapsed panel stays out of the row
        std::vector<Panel> ps = {Make(0, .2f, 1, .3f, Kind::Keep), Make(1.1f, .2f, 2, .6f, Kind::Hug)};
        Add(ps, {1.2f, .25f, 1.9f, .58f}, kR.reach);
        Solve(ps, kR);
        CHECK(Near(ps[0].plate.y1, .3f) && Near(ps[1].plate.y1, .598f));
    }
    { // the 3D view's panel ends with the tallest list, up or down, and the panel under follows
        std::vector<Panel> ps = {Make(0, .2f, 1, .4f, Kind::Hug), Make(1.1f, .2f, 2, .8f, Kind::Match), Make(2.1f, .2f, 3, .5f, Kind::Hug), Make(0, .85f, 3, 1.0f, Kind::Under)};
        Add(ps, {.1f, .25f, .9f, .30f}, kR.reach);
        Add(ps, {2.2f, .25f, 2.9f, .60f}, kR.reach);
        Solve(ps, kR);
        CHECK(Near(ps[1].plate.y1, .618f) && Near(ps[2].plate.y1, .618f) && Near(ps[0].plate.y1, .318f));
        CHECK(Near(ps[3].plate.y0, .63f));
    }
    { // a row that ends just short of the panel under it does not nudge it
        std::vector<Panel> ps = {Make(0, .2f, 1, .6f, Kind::Hug), Make(0, .689f, 1, .9f, Kind::Under)};
        Add(ps, {.1f, .3f, .9f, .662f}, kR.reach);
        Solve(ps, kR);
        CHECK(Near(ps[0].plate.y1, .68f) && Near(ps[1].plate.y0, .689f));
    }
    { // the button at the foot of the 3D view's panel: the panel holds it, the list beside it ends level
        std::vector<Panel> ps = {Make(0, .2f, 1, .8f, Kind::Match), Make(1.1f, .2f, 2, .5f, Kind::Hug)};
        Add(ps, {.3f, .614f, .7f, .666f}, kR.reach);
        Add(ps, {1.2f, .25f, 1.9f, .62f}, kR.reach);
        Solve(ps, kR);
        CHECK(Near(ps[0].plate.y1, .684f) && Near(ps[1].plate.y1, .684f));
    }
    { // a panel that holds only its close button still holds its heading
        std::vector<Panel> ps = {Make(0, .2f, 1, .6f, Kind::Hug)};
        ps[0].least = .316f;
        Add(ps, {.8f, .21f, .9f, .25f}, kR.reach);
        Solve(ps, kR);
        CHECK(Near(ps[0].plate.y1, .316f));
    }
    // ownership: inside first, then the nearest above in the column, and nothing past the reach or in another column
    {
        std::vector<Panel> ps = {Make(0, .2f, 1, .6f, Kind::Hug), Make(0, .65f, 1, .9f, Kind::Under)};
        CHECK(Owner(ps, .5f, .7f, kR.reach) == 1);
        CHECK(Owner(ps, .5f, .62f, kR.reach) == 0);
        CHECK(Owner(ps, 1.5f, .5f, kR.reach) == -1);
        CHECK(Owner(ps, .5f, 1.1f, kR.reach) == -1);
    }
    // a short content never leaves a panel thinner than its margins
    {
        std::vector<Panel> ps = {Make(0, .5f, 1, .9f, Kind::Hug)};
        Add(ps, {.1f, .505f, .9f, .50f}, kR.reach);
        Solve(ps, kR);
        CHECK(ps[0].plate.y1 >= .5f + kR.air * 2 - 0.0001f);
    }
    CHECK(Near(Toward(.5f, .6f, .03f), .53f) && Near(Toward(.5f, .4f, .03f), .47f) && Near(Toward(.5f, .51f, .03f), .51f));
}
