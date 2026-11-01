// subtitles\src\impl\stack_fit.h (pure): the stack fit's target scale - 1 below two blocks or with the setting at 1, else
// the tallest block's share of the stack's height (gaps included) clamped to [floor, 1] - and its ease (exponential at the
// display's glide, instant when fresh, snapped near the target, never past it).
#include "test.h"

#include "../subtitles/src/impl/stack_fit.h"

void Test_stack_fit()
{
    using stackfit::Ease;
    using stackfit::Target;
    // fewer than two blocks: unscaled
    CHECK(Target({}, 10, 0.7f) == 1.0f);
    CHECK(Target({100}, 10, 0.7f) == 1.0f);
    // two equal blocks: 100 / 210 < floor -> the floor
    CHECK_NEAR(Target({100, 100}, 10, 0.7f), 0.7f, 1e-6);
    CHECK_NEAR(Target({100, 100}, 10, 0.5f), 0.5f, 1e-6);
    // a tall block and a short one: 300 / (300 + 40 + 10) = 0.857 (above the floor)
    CHECK_NEAR(Target({300, 40}, 10, 0.7f), 300.0f / 350.0f, 1e-5);
    // the order does not matter; three blocks: 100 / (100 + 50 + 50 + 2 * 10) = 0.4545 -> the floor 0.5
    CHECK_NEAR(Target({50, 100, 50}, 10, 0.5f), 0.5f, 1e-6);
    CHECK_NEAR(Target({50, 100, 50}, 10, 0.4f), 100.0f / 220.0f, 1e-5);
    // the setting at 1 = off; a floor above 1 is off too; never above 1
    CHECK(Target({100, 100}, 10, 1.0f) == 1.0f);
    CHECK(Target({100, 100}, 10, 1.5f) == 1.0f);
    CHECK(Target({100, 0}, 0, 0.5f) == 1.0f); // an empty second block: the tallest is the whole stack
    // degenerate heights: unscaled, never NaN
    CHECK(Target({0, 0}, 0, 0.7f) == 1.0f);
    CHECK(Target({-5, -5}, 0, 0.7f) == 1.0f);

    // the ease: instant when fresh / motion off, or from no value yet
    CHECK(Ease(1.0f, 0.7f, 0.016f, 0.07f, true) == 0.7f);
    CHECK(Ease(0.0f, 0.7f, 0.016f, 0.07f, false) == 0.7f);
    // one frame at 60 fps: part of the way, toward the target, not past it
    {
        const float s = Ease(1.0f, 0.7f, 1.0f / 60, 0.07f, false);
        CHECK(s < 1.0f && s > 0.7f);
        CHECK_NEAR(s, 1.0f - 0.3f * (1.0f - std::exp(-(1.0f / 60) / 0.07f)), 1e-5);
    }
    // ~95 % of the way in 0.2 s (kGlideTau 0.07: the display's glide), there after a second; growing back the same
    {
        float s = 1.0f;
        for (int i = 0; i < 12; ++i)
            s = Ease(s, 0.7f, 1.0f / 60, 0.07f, false);
        CHECK(s > 0.7f && s < 0.7f + 0.3f * 0.06f);
        for (int i = 0; i < 48; ++i)
            s = Ease(s, 0.7f, 1.0f / 60, 0.07f, false);
        CHECK(s == 0.7f); // snapped
        for (int i = 0; i < 60; ++i)
            s = Ease(s, 1.0f, 1.0f / 60, 0.07f, false);
        CHECK(s == 1.0f); // back exactly unscaled (sharp text)
    }
    // a frame gap of zero changes nothing; a huge dt lands on the target
    CHECK(Ease(0.8f, 0.7f, 0.0f, 0.07f, false) == 0.8f);
    CHECK(Ease(0.8f, 0.7f, 10.0f, 0.07f, false) == 0.7f);
}
