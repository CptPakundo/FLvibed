// 0.2.1 (the owner, 2 Nov 2026): two pure rules of HUD 2's context prompts.
// core\game\promptfit.h FitLabel - a prompt's label never scrolls: one line at the nominal size where it fits; too long:
// one line shrunk to the two-line size, else two lines at a smaller size >= the floor; still too long: the ellipsis at
// the floor (the last resort). LabelMinWidth = the narrowest room it reads whole in (the lanes' squeeze limit).
// core\game\promptshow.h Step - hud_prompts 1: the lanes show on a change of the prompt set or a player action, 4 s,
// then hide; 0 always shown; a waiting prompt never hidden.
#include "test.h"

#include "game/promptfit.h"
#include "game/promptshow.h"

namespace
{
// a monospace stand-in for the font: every character 0.5 em wide
float MonoW(const std::wstring& s, float size) { return 0.5f * size * static_cast<float>(s.size()); }
} // namespace

void Test_promptlabel()
{
    using namespace promptfit;
    const float nominal = 56, floor = 52; // 4K, k = 1: 0.026 H and the 0.024 H floor
    auto w = [](const std::wstring& s, float z) { return MonoW(s, z); };
    // (1) fits: one line, nominal
    LabelFit r = FitLabel(L"DOCK", nominal, floor, 200, w);
    CHECK(r.lines == 1 && r.size == nominal && r.line[0] == L"DOCK" && !r.cut);
    // (2) a little too long: one line shrunk, never under the two-line size
    r = FitLabel(L"CANCEL CRUISE", nominal, floor, 13 * 0.5f * 53, w); // fits at 53
    CHECK(r.lines == 1 && r.size < nominal && r.size >= WrapSize(nominal, floor) && !r.cut);
    CHECK(r.width <= 13 * 0.5f * 53 + 0.5f);
    // (3) too long for one line: two lines at a smaller size >= the floor, the split balanced, nothing cut
    r = FitLabel(L"LEAVE FORMATION", nominal, floor, 9 * 0.5f * 52.5f, w); // FORMATION (9) at the two-line size 52.5? -> >= floor
    CHECK(r.lines == 2 && r.size >= floor && r.size < nominal && !r.cut);
    CHECK(r.line[0] == L"LEAVE" && r.line[1] == L"FORMATION");
    CHECK(r.width <= 9 * 0.5f * 52.5f + 0.5f);
    // the larger scale: the two-line size 0.85 x nominal above the floor
    r = FitLabel(L"ABORT DOCK NOW", 100, floor, 8 * 0.5f * 85, w); // "DOCK NOW" (8) at 85 fits exactly
    CHECK(r.lines == 2 && r.size >= 84.9f && r.size <= 85.0f && r.line[0] == L"ABORT" && r.line[1] == L"DOCK NOW");
    // (4) still too long at the floor: the ellipsis at the floor, two lines
    r = FitLabel(L"DOCK AT PLANETMANHATTAN", nominal, floor, 6 * 0.5f * floor, w);
    CHECK(r.lines == 2 && r.size == floor && r.cut);
    CHECK(r.width <= 6 * 0.5f * floor + 0.5f);
    CHECK(r.line[1].back() == static_cast<wchar_t>(0x2026));
    // (4) one long word: one line at the floor, cut
    r = FitLabel(L"SUPERCALIFRAGILISTIC", nominal, floor, 8 * 0.5f * floor, w);
    CHECK(r.lines == 1 && r.size == floor && r.cut && r.width <= 8 * 0.5f * floor + 0.5f);
    // never under the floor, whatever the room
    for (float room : {1.0f, 30.0f, 100.0f, 200.0f, 400.0f, 1000.0f})
    {
        r = FitLabel(L"LEAVE FORMATION NOW PLEASE", nominal, floor, room, w);
        CHECK(r.size >= floor);
        CHECK(r.lines >= 1 && r.lines <= 2);
    }
    // the squeeze limit: the wider line of the best split at the floor; one word: its width at the floor
    CHECK_NEAR(LabelMinWidth(L"LEAVE FORMATION", nominal, floor, w), 9 * 0.5f * floor, 1e-3);
    CHECK_NEAR(LabelMinWidth(L"TURRET", nominal, floor, w), 6 * 0.5f * floor, 1e-3);
    // a room of LabelMinWidth always reads whole
    r = FitLabel(L"LEAVE FORMATION", nominal, floor, LabelMinWidth(L"LEAVE FORMATION", nominal, floor, w), w);
    CHECK(!r.cut && r.lines == 2);
    // the lanes: a squeezed prompt is marked wrap, and the row above its lane moves up by the wrapped plate's growth
    {
        const LaneGeom g = ContextLanes(3840, 2160, 1, 489, 3351);
        CHECK_NEAR(g.tall, kWrapTall * 2160, 1e-3);
        const float room = g.lx1 - g.lx0;
        const float wv[2] = {room + 100, 200}, mv[2] = {room - 50, 200};
        const int side[2] = {0, 0};
        const LanesPlaced o = PlaceLanes(wv, mv, side, 2, 30, g, 2160, 1);
        CHECK(o.n == 2 && o.wrap[0] && !o.wrap[1] && o.row[0] == 0 && o.row[1] == 1);
        CHECK_NEAR(o.y[1], g.y0 - g.pitch - g.tall, 1e-3);
        // the wrapped plate's top (y - 0.026 H - tall) stays below the row above's bottom (y1 + 0.026 H)
        CHECK(o.y[0] - 0.026f * 2160 - g.tall >= o.y[1] + 0.026f * 2160);
        // nothing wrapped: the old pitch
        const float wv2[2] = {200, 200};
        const LanesPlaced p = PlaceLanes(wv2, wv2, side, 2, 30, g, 2160, 1);
        CHECK(!p.wrap[0] && !p.wrap[1]);
        if (p.row[1] == 1)
            CHECK_NEAR(p.y[1], g.y0 - g.pitch, 1e-3);
    }
}

void Test_promptshow()
{
    using namespace promptshow;
    const unsigned a = Mix(Mix(kSeed, 1), std::wstring(L"CRUISE")), b = Mix(Mix(kSeed, 1), std::wstring(L"CANCEL CRUISE"));
    CHECK(a != b);
    CHECK(Mix(Mix(kSeed, std::wstring(L"AB")), std::wstring(L"C")) != Mix(Mix(kSeed, std::wstring(L"A")), std::wstring(L"BC")));
    // mode 1: shown on arrival (the first signature), 4 s, then hidden
    State s;
    CHECK(Step(s, 1, a, false, false, 0.016f));
    float t = 0;
    while (t < kHoldS - 0.15f)
    {
        CHECK(Step(s, 1, a, false, false, 0.1f));
        t += 0.1f;
    }
    CHECK(!Step(s, 1, a, false, false, 0.2f));
    CHECK(!Step(s, 1, a, false, false, 5.0f)); // calm: stays hidden
    // a change of the set: shown again, for 4 s
    CHECK(Step(s, 1, b, false, false, 0.016f));
    CHECK(Step(s, 1, b, false, false, 3.9f));
    CHECK(!Step(s, 1, b, false, false, 0.2f));
    // a player action with the same set: shown again
    CHECK(Step(s, 1, b, true, false, 0.016f));
    CHECK(Step(s, 1, b, false, false, 3.0f));
    // an action while shown restarts the 4 s
    CHECK(Step(s, 1, b, true, false, 0.016f));
    CHECK(Step(s, 1, b, false, false, 3.9f));
    CHECK(!Step(s, 1, b, false, false, 0.2f));
    // a prompt waiting on the player: never hidden
    CHECK(Step(s, 1, b, false, true, 10.0f));
    // mode 0: always shown (0.2.0), the timer still kept
    State z;
    CHECK(Step(z, 0, a, false, false, 0.016f));
    CHECK(Step(z, 0, a, false, false, 100.0f));
    CHECK(z.left == 0);
}
