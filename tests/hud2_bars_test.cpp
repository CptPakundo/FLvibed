// modules\hud\src\hud2_bars.h (pure): the failing bars. The flash limit first: the breath never crosses 3 Hz, measured from the glow itself; the rate's numbers;
// the hysteresis; a rate change does not jump the glow; the quiet state; the bloom's stages, its limits, and skin_motion's two calmer forms.
#include "test.h"

#include "../modules/hud/src/hud2_bars.h"

#include <vector>

using namespace hud2bars;

namespace
{
// Rising crossings of 0.5 of the glow's peak in `seconds`, at value v: the rate as a viewer sees it.
float MeasuredHz(float v, float seconds)
{
    Breath b;
    const float dt = 1.0f / 240;
    float was = 0, peak = 0;
    int rises = 0;
    for (float t = 0; t < seconds; t += dt)
    {
        const float g = b.Step(dt, v, false, 0);
        peak = g > peak ? g : peak;
        if (t > 0.5f && was < 0.5f && g >= 0.5f)
            ++rises;
        was = g;
        b.quiet = 0; // (the measure holds the full depth)
    }
    return rises / (seconds - 0.5f);
}
} // namespace

void Test_hud2_bars()
{
    CHECK_NEAR(Rate(0.25f), 1.25, 1e-4);
    CHECK_NEAR(Rate(0.15f), 1.75, 0.02);
    CHECK_NEAR(Rate(0.05f), 2.45, 0.02);
    CHECK_NEAR(Rate(0), 2.9, 1e-4);
    CHECK_NEAR(Rate(-1), 2.9, 1e-4);
    CHECK_NEAR(Rate(0.9f), 1.25, 1e-4);
    // the flash limit: under 3 Hz at every value, the rate itself and the glow measured
    for (float v = 0; v <= 0.25f; v += 0.01f)
    {
        CHECK(Rate(v) < 3.0f);
        CHECK(MeasuredHz(v, 20) < 3.0f);
    }
    CHECK_NEAR(MeasuredHz(0, 40), 2.9, 0.1);
    // starts at 25 %, stops above 30 %
    {
        Breath b;
        b.Step(0.016f, 0.27f, false, 0);
        CHECK(!b.on);
        b.Step(0.016f, 0.25f, false, 0);
        CHECK(b.on);
        b.Step(0.016f, 0.29f, false, 0);
        CHECK(b.on);
        b.Step(0.016f, 0.31f, false, 0);
        CHECK(!b.on);
    }
    // the value's jump does not jump the glow: one frame moves it little
    {
        Breath b;
        float g = 0;
        for (int i = 0; i < 300; ++i)
            g = b.Step(1 / 120.0f, 0.24f, false, 0);
        const float next = b.Step(1 / 120.0f, 0.02f, false, 0);
        CHECK(fabsf(next - g) < 0.25f);
    }
    // the quiet state: no further loss for 6 s, depth to 40 % over 1 s, the rate kept; a new loss restores it at once
    {
        Breath b;
        for (int i = 0; i < 60 * 8; ++i)
            b.Step(1 / 60.0f, 0.1f, false, 0);
        CHECK_NEAR(b.depth, kQuietDepth, 1e-3);
        CHECK_NEAR(b.rate, Rate(0.1f), 0.01);
        b.Step(1 / 60.0f, 0.09f, false, 0);
        CHECK_NEAR(b.depth, 1, 1e-5);
    }
    // a failed shield does not breathe; an unknown value neither
    {
        Breath b;
        CHECK(b.Step(0.016f, 0.1f, true, 0) == 0);
        CHECK(b.Step(0.016f, -1, false, 0) == 0);
    }
    // FADES and NONE: steady, rising as the value falls
    {
        Breath b;
        CHECK_NEAR(b.Step(0.016f, 0.25f, false, 1), 0.10, 1e-4);
        const float a = b.Step(0.016f, 0, false, 2), c = b.Step(0.5f, 0, false, 2);
        CHECK_NEAR(a, 0.28, 1e-4);
        CHECK_NEAR(a, c, 1e-6);
    }
    // the bloom's stages
    CHECK(!BloomAt(-0.01f, 0).on);
    CHECK_NEAR(BloomAt(0.025f, 0).light, 0.5, 1e-4);
    CHECK_NEAR(BloomAt(0.09f, 0).reach, 0.5, 1e-3);
    CHECK_NEAR(BloomAt(0.45f - 1e-4f, 0).reach, 1 + kBloomCreep, 1e-2);
    CHECK_NEAR(BloomAt(0.525f, 0).pips, 0.5, 1e-3);
    CHECK(!BloomAt(0.6f, 0).on);
    CHECK_NEAR(BloomAt(0.1f, 1).light, 1, 1e-4);
    CHECK(!BloomAt(0.4f, 1).on);
    CHECK(!BloomAt(0.1f, 2).on);
    // the same bar at most once a second; a target's waits behind the player's; two at once
    {
        Bloom b;
        CHECK(b.Start());
        b.Step(0.5f);
        CHECK(!b.Start());
        b.Step(0.6f);
        CHECK(b.Start());
        Bloom t;
        CHECK(t.Start(kTargetDelay));
        CHECK(!t.Now(0).on);
        t.Step(kTargetDelay + 0.01f);
        CHECK(t.Now(0).on);
        Bloom third;
        const Bloom* all[3] = {&b, &t, &third};
        CHECK(!BloomFree(all, 3));
        b.Step(0.7f), t.Step(0.7f);
        CHECK(BloomFree(all, 3));
    }
    // the glow: full over the lit pips, short roll-off past them, nothing beyond
    CHECK_NEAR(GlowAt(0.1f, 0.2f), 1, 1e-5);
    CHECK_NEAR(GlowAt(0.25f, 0.2f), 0.5, 1e-4);
    CHECK_NEAR(GlowAt(0.31f, 0.2f), 0, 1e-5);
    CHECK_NEAR(GlowAt(0.1f, 0), 0, 1e-5);
}
