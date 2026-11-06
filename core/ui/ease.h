// FLvibed - the panels' width and height easing, shared by HUD 2's panels and the pad's picker. Pure.
#pragma once
#include <algorithm>

namespace uiease
{
// A narrower content shrinks the panel only past this fraction of H (x k): a value ticking a digit does not breathe the panel.
constexpr float kContentHyst = 0.012f;

// One panel's eased width (pixels) toward `target`: grows at 14 / s (never clipped for long), shrinks at 5 / s and only when the target is the hysteresis
// below. cur 0 = no width yet: straight to the target.
inline float EaseContent(float cur, float target, float dt, float hystPx)
{
    if (target <= 0)
        return cur;
    if (cur <= 0)
        return target;
    if (target > cur)
    {
        const float n = cur + (target - cur) * std::min(1.0f, 14.0f * dt) + 0.5f; // (+ half a pixel: it arrives)
        return n > target ? target : n;
    }
    if (target < cur - hystPx)
        return cur + (target - cur) * std::min(1.0f, 5.0f * dt);
    return cur;
}
} // namespace uiease
