// subtitles\src\impl\stack_fit.h (pure): the stack fit's per-block targets - the newest block always 1, the older ones one
// common scale (tallest - newest) / older, clamped to [floor, 1]; all 1 below two blocks or with the setting at 1 - and its
// ease (exponential at kTau 0.5 s, instant when fresh, snapped near the target, never past it).
#include "test.h"

#include "../subtitles/src/impl/stack_fit.h"

void Test_stack_fit()
{
    using stackfit::Ease;
    using stackfit::kTau;
    using stackfit::Targets;
    using V = std::vector<float>;
    // fewer than two blocks: unscaled
    CHECK(Targets({}, 0, 0.7f).empty());
    CHECK(Targets({100}, 0, 0.7f) == V({1.0f}));
    // two equal blocks: the newest 1, the older to the floor (no room under the tallest)
    {
        const V t = Targets({100, 100}, 1, 0.7f);
        CHECK(t[1] == 1.0f);
        CHECK_NEAR(t[0], 0.7f, 1e-6);
        CHECK_NEAR(Targets({100, 100}, 1, 0.5f)[0], 0.5f, 1e-6);
    }
    // the newest is never scaled, whatever its place in the stack
    {
        const V t = Targets({100, 100, 100}, 0, 0.6f);
        CHECK(t[0] == 1.0f);
        CHECK_NEAR(t[1], 0.6f, 1e-6);
        CHECK_NEAR(t[2], 0.6f, 1e-6);
    }
    // a tall older block over a short newest one: (300 - 40) / 300 = 0.867 (above the floor), the newest 1
    {
        const V t = Targets({300, 40}, 1, 0.7f);
        CHECK_NEAR(t[0], 260.0f / 300.0f, 1e-5);
        CHECK(t[1] == 1.0f);
    }
    // three blocks, the newest short: (200 - 50) / (200 + 100) = 0.5 -> above a 0.4 floor, at a 0.6 floor the floor
    CHECK_NEAR(Targets({200, 100, 50}, 2, 0.4f)[0], 0.5f, 1e-5);
    CHECK_NEAR(Targets({200, 100, 50}, 2, 0.4f)[1], 0.5f, 1e-5);
    CHECK_NEAR(Targets({200, 100, 50}, 2, 0.6f)[1], 0.6f, 1e-6);
    // never above 1 (a newest taller than... itself: room never exceeds the older sum's share of 1)
    for (const float s : Targets({10, 400}, 0, 0.5f))
        CHECK(s <= 1.0f && s >= 0.5f);
    // the setting at 1 = off; a floor above 1 is off too
    CHECK(Targets({100, 100}, 1, 1.0f) == V({1.0f, 1.0f}));
    CHECK(Targets({100, 100}, 1, 1.5f) == V({1.0f, 1.0f}));
    // a newest out of range, degenerate heights: unscaled, never NaN
    CHECK(Targets({100, 100}, 5, 0.7f) == V({1.0f, 1.0f}));
    CHECK(Targets({0, 0}, 1, 0.7f) == V({1.0f, 1.0f}));
    CHECK(Targets({-5, -5}, 1, 0.7f) == V({1.0f, 1.0f}));
    CHECK(Targets({0, 100}, 1, 0.7f) == V({1.0f, 1.0f})); // an empty older block: nothing to shrink

    // the ease: gentle (half a second), instant when fresh / motion off, or from no value yet
    CHECK(kTau >= 0.4f && kTau <= 0.6f);
    CHECK(Ease(1.0f, 0.7f, 0.016f, kTau, true) == 0.7f);
    CHECK(Ease(0.0f, 0.7f, 0.016f, kTau, false) == 0.7f);
    // one frame at 60 fps: part of the way, toward the target, not past it
    {
        const float s = Ease(1.0f, 0.7f, 1.0f / 60, kTau, false);
        CHECK(s < 1.0f && s > 0.7f);
        CHECK_NEAR(s, 1.0f - 0.3f * (1.0f - std::exp(-(1.0f / 60) / kTau)), 1e-5);
    }
    // not sudden: after 0.2 s less than half the way; ~86 % after 1 s; there after 4 s; growing back the same
    {
        float s = 1.0f;
        for (int i = 0; i < 12; ++i)
            s = Ease(s, 0.7f, 1.0f / 60, kTau, false);
        CHECK(s > 0.7f + 0.3f * 0.5f);
        for (int i = 0; i < 48; ++i)
            s = Ease(s, 0.7f, 1.0f / 60, kTau, false);
        CHECK(s > 0.7f && s < 0.7f + 0.3f * 0.2f);
        for (int i = 0; i < 180; ++i)
            s = Ease(s, 0.7f, 1.0f / 60, kTau, false);
        CHECK(s == 0.7f); // snapped
        for (int i = 0; i < 240; ++i)
            s = Ease(s, 1.0f, 1.0f / 60, kTau, false);
        CHECK(s == 1.0f); // back exactly unscaled (sharp text)
    }
    // a frame gap of zero changes nothing; a huge dt lands on the target
    CHECK(Ease(0.8f, 0.7f, 0.0f, kTau, false) == 0.8f);
    CHECK(Ease(0.8f, 0.7f, 100.0f, kTau, false) == 0.7f);
}
