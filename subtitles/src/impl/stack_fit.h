// Stack fit (pure; tests\stack_fit_test.cpp; the owner, 1 Nov 2026, for 0.2.0; reworked for 0.2.1 from the owner's play,
// 2 Nov 2026: "too fast and sudden", shrink the older lines, never the newest): when two or more speakers' blocks are
// stacked, the newest block (the most recently started line) stays at scale 1 and the older ones shrink to one common
// scale, so the stack's height stays about its tallest block's (at 4:3 and in letterboxed story scenes - M01's Donau - a
// two-speaker stack covered a large part of the picture). display.cpp draws each older block through render::ZoomBegin /
// ZoomEnd about its own anchor at its eased scale, the slots following the eased heights; the layout (keep-outs, the
// crosshair limit, retiring) stays unscaled, and a scale only shrinks, so the drawn stack always lies inside its unscaled
// place. Setting `stack_fit` (the floor).
#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

namespace stackfit
{
// The scale's time constant (s): gentle - the owner found the display's glide (0.07 s) too sudden for a size change.
constexpr float kTau = 0.5f;

// The target scales, one per block. heights = the main stack's block heights (px: not the Neural Net corner, not lines
// fading out or released), newest = the index of the most recently started one, floor = stack_fit (0.5..1; 1 = off).
// The newest: 1. The others: one common scale s = (tallest - newest's height) / (the older blocks' heights), clamped to
// [floor, 1] - the budget is the tallest block plus the gaps (the gaps stay unscaled, so they cancel): with the newest the
// tallest the older go to the floor; under a much taller older block they shrink only as far as needed. Fewer than two
// blocks, the setting at 1, a newest out of range or degenerate heights: all 1.
inline std::vector<float> Targets(const std::vector<float>& heights, size_t newest, float floor)
{
    std::vector<float> out(heights.size(), 1.0f);
    if (heights.size() < 2 || !(floor < 1.0f) || newest >= heights.size())
        return out;
    float older = 0, tallest = 0;
    for (size_t i = 0; i < heights.size(); ++i)
    {
        const float h = std::max(0.0f, heights[i]);
        tallest = std::max(tallest, h);
        if (i != newest)
            older += h;
    }
    if (!(older > 0.0f) || !(tallest > 0.0f))
        return out;
    const float room = tallest - std::max(0.0f, heights[newest]);
    const float s = std::clamp(room / older, std::clamp(floor, 0.05f, 1.0f), 1.0f);
    for (size_t i = 0; i < out.size(); ++i)
        if (i != newest)
            out[i] = s;
    return out;
}

// One frame's step from `current` toward `target`: exponential with the time constant tau (s) over dt (s); at once when
// `instant` (a stack starting afresh, a new context, motion off). Within 0.002 it snaps, so a block back at 1 draws
// unscaled (sharp, point-filtered text) rather than at 0.9999.
inline float Ease(float current, float target, float dt, float tau, bool instant)
{
    if (!(current > 0.0f) || instant || !(tau > 0.0f))
        return target;
    const float k = 1.0f - std::exp(-std::max(0.0f, dt) / tau);
    const float next = current + (target - current) * k;
    return std::fabs(next - target) < 0.002f ? target : next;
}
} // namespace stackfit
