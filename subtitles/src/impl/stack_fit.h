// Stack fit (pure; tests\stack_fit_test.cpp; the owner, 1 Nov 2026, for 0.2.0): when two or more speakers' blocks are
// stacked, the whole stack shrinks together so its height stays about one block's - at 4:3 and in letterboxed story
// scenes (M01's Donau) a two-speaker stack covered a large part of the picture. display.cpp draws the stack through
// render::ZoomBegin / ZoomEnd at the eased scale; the layout (keep-outs, the crosshair limit, retiring) stays unscaled,
// and the scale only shrinks, so the drawn stack always lies inside its unscaled place. Setting `stack_fit` (the floor).
#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

namespace stackfit
{
// The target scale. heights = the main stack's block heights (px: not the Neural Net corner, not lines fading out or
// released), gap = between two blocks, floor = stack_fit (0.5..1; 1 = off). Fewer than two blocks: 1; else the tallest
// block's share of the stack's whole height, clamped to [floor, 1].
inline float Target(const std::vector<float>& heights, float gap, float floor)
{
    if (heights.size() < 2 || !(floor < 1.0f))
        return 1.0f;
    float total = 0, tallest = 0;
    for (const float h : heights)
        total += std::max(0.0f, h), tallest = std::max(tallest, h);
    total += std::max(0.0f, gap) * static_cast<float>(heights.size() - 1);
    if (!(total > 0.0f) || !(tallest > 0.0f))
        return 1.0f;
    return std::clamp(tallest / total, std::clamp(floor, 0.05f, 1.0f), 1.0f);
}

// One frame's step from `current` toward `target`: exponential with the time constant tau (s) over dt (s) - the display's
// glide; at once when `instant` (a stack starting afresh, a new context, motion off). Within 0.002 it snaps, so a stack
// back at 1 draws unscaled (sharp, point-filtered text) rather than at 0.9999.
inline float Ease(float current, float target, float dt, float tau, bool instant)
{
    if (!(current > 0.0f) || instant || !(tau > 0.0f))
        return target;
    const float k = 1.0f - std::exp(-std::max(0.0f, dt) / tau);
    const float next = current + (target - current) * k;
    return std::fabs(next - target) < 0.002f ? target : next;
}
} // namespace stackfit
