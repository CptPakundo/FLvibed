// modules\hud\src\navmap_layout.h (pure): the strip and the map's solver over the display matrix: nothing leaves the screen, nothing meets.
#include "test.h"

#include "../modules/hud/src/navmap_layout.h"

#include <cwchar>

using namespace navlayout;

namespace
{
// Agency FB's capitals run about 0.46 em; the tab words of the design.
const int kChars[kMaxTabs] = {3, 9, 3, 6, 4, 4, 5};
struct Measure
{
    float operator()(int i, float size) const { return kChars[i] * 0.46f * size; }
};
struct Display
{
    float W, H;
    float side, laneTop, laneBottom;
};
// Normal's margins; Large's where the row says so
const Display kMatrix[] = {
    {3840, 2160, 0.05f, 0.91f, 0.95f},     {1920, 1080, 0.05f, 0.91f, 0.95f},  {2560, 1440, 0.05f, 0.91f, 0.95f},
    {1280, 800, 0.05f, 0.91f, 0.95f},      {1280, 800, 0.0125f, 0.945f, 0.985f}, {1920, 1200, 0.05f, 0.91f, 0.95f},
    {2560, 1080, 0.05f, 0.91f, 0.95f},     {3440, 1440, 0.05f, 0.91f, 0.95f},  {5120, 1440, 0.05f, 0.91f, 0.95f},
    {1024, 768, 0.05f, 0.91f, 0.95f},      {1600, 1200, 0.05f, 0.91f, 0.95f},  {1280, 1024, 0.05f, 0.91f, 0.95f},
};
} // namespace

void Test_navmap_layout()
{
    // the ruled numbers: 16:9 keeps 0.785 H between panels of about 0.39 H; 16:10 is 0.764 H with panels of 0.38 H
    {
        const MapOut o = Solve(3840, 2160, 0.05f);
        CHECK_NEAR(o.size, 0.785, 1e-4);
        CHECK_NEAR(o.panel, 0.39, 0.002);
        CHECK_NEAR(o.map.y0 / 2160, 0.115, 1e-4);
        CHECK_NEAR(o.map.y1 / 2160, 0.900, 1e-4);
        CHECK(!o.column);
    }
    {
        const MapOut o = Solve(1280, 800, 0.05f);
        CHECK_NEAR(o.size, 0.764, 1e-3);
        CHECK_NEAR(o.panel, 0.38, 1e-4);
        CHECK_NEAR(o.side, kGutterW, 1e-4);
    }
    // 21:9: the panels gain, the map keeps its height
    {
        const MapOut o = Solve(3440, 1440, 0.05f);
        CHECK_NEAR(o.size, 0.785, 1e-4);
        CHECK(o.panel > 0.40f);
    }
    // 4:3: the map at 0.70 H, one column of tabs for the left panel
    {
        const MapOut o = Solve(1024, 768, 0.05f);
        CHECK(o.column);
        CHECK_NEAR(o.size, kColumnMapH, 1e-4);
        CHECK(o.left.W() / 768 >= kColumnMin - 1e-4);
    }
    for (const Display& d : kMatrix)
    {
        // the map: on the screen, square, clear of both panels, above the lane
        const MapOut o = Solve(d.W, d.H, d.side);
        CHECK_NEAR(o.map.W(), o.map.H(), 0.5);
        CHECK(o.size <= kMapH + 1e-4f);
        CHECK(o.left.x0 >= -0.5f && o.right.x1 <= d.W + 0.5f);
        CHECK(!o.map.Overlaps(o.left) && !o.map.Overlaps(o.right));
        CHECK(o.map.y1 <= d.laneTop * d.H);
        CHECK(o.panel >= kPanelMin - 1e-4f);

        // the panels beside the game's own 0.49 H map, wherever it stands
        const float s = 0.49f * d.H, cx = d.W / 2, top = 0.30f * d.H;
        const MapOut a = Around(d.W, d.H, d.side, {cx - s / 2, top, cx + s / 2, top + s});
        CHECK(a.left.x0 >= -0.5f && a.right.x1 <= d.W + 0.5f);
        CHECK(!a.map.Overlaps(a.left) && !a.map.Overlaps(a.right));

        // a panel's chips stay in the panel; a tall panel moves up, it never runs into the lane
        Rect chip[8];
        const float bottom = Grid(a.right, d.H, a.right.y0, 8, 2, chip);
        for (int i = 0; i < 8; ++i)
            CHECK(chip[i].x0 >= a.right.x0 && chip[i].x1 <= a.right.x1 + 0.5f);
        CHECK(!chip[0].Overlaps(chip[1]) && !chip[0].Overlaps(chip[2]));
        const float height = bottom - a.right.y0;
        CHECK(PanelTop(a.right.y0, height, d.laneTop * d.H) + height <= d.laneTop * d.H + 0.5f);

        // the strip, five words and seven, K&M and pad
        for (int tabs : {5, 7})
            for (bool pad : {false, true})
            {
                StripIn in;
                in.W = d.W, in.H = d.H, in.side = d.side, in.laneTop = d.laneTop, in.laneBottom = d.laneBottom, in.tabs = tabs, in.bumpers = pad;
                in.bumperW = 0.05f * d.H;
                const StripOut st = Strip(in, Measure(), 5 * 0.46f * kWordH * d.H);
                // (5:4 in multiplayer alone runs past the middle: the words stand whole and the pills yield to the keep-out)
                if (tabs == 5 || d.W / d.H > 1.3f)
                {
                    CHECK(st.fits);
                    CHECK(st.box.x1 <= d.W * 0.5f + 0.5f);
                }
                CHECK_NEAR(st.box.x0, d.side * d.W, 0.5);
                CHECK(st.size >= in.floorH * d.H - 0.01f);
                for (int i = 0; i + 1 < tabs; ++i)
                    CHECK(st.word[i].x1 <= st.word[i + 1].x0 + 0.5f);
                CHECK(st.word[0].x0 >= st.box.x0 - 0.5f && st.word[tabs - 1].x1 <= st.box.x1 + 0.5f);
                CHECK(st.close.x1 <= d.W && !st.close.Overlaps(st.box));
            }
    }
    // a screen too narrow for its words: the size stops at the floor and says so; a word is never cut
    {
        StripIn in;
        in.W = 500, in.H = 800, in.tabs = 7;
        const StripOut st = Strip(in, Measure(), 40);
        CHECK(!st.fits);
        CHECK_NEAR(st.size, in.floorH * in.H, 0.01);
    }
}
