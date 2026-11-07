// Failing bars (modules\hud\docs\UI_HUD2_SKIN.md 6.11; docs\research\bar-critical-feedback.md): the breath of a bar running low and the bloom of a bar that
// breaks, for the ring's arcs, the target's bars and the escorts' pips. A bar never changes colour: both effects stay in its own. Pure, no game call: the
// parts feed it each frame's value and read a glow, tests\hud2_bars_test.cpp holds the rate under the 3 Hz limit. `skin_motion` (0 full, 1 fades, 2 none)
// takes the motion out; pips, numbers and the alert card carry the state in all three.
#pragma once
#include <cmath>

namespace hud2bars
{
constexpr float kOnAt = 0.25f, kOffAt = 0.30f;       // breathing starts at, and stops above: a recharging bar does not flicker
constexpr float kRateTop = 1.25f, kRateMax = 2.9f;   // Hz at 25 % and at nothing: 2.9 stands just under the 3 Hz limit
constexpr float kRateTau = 0.2f;                     // the rate's low-pass: a change must not jump the glow
constexpr float kQuietAfter = 6.0f, kQuietFall = 1.0f, kQuietDepth = 0.4f; // no further loss: depth falls, the rate stays
constexpr float kEscortDepth = 0.5f;
constexpr float kRollOff = 0.10f;                    // the glow's short roll-off at both ends of what is left, of the bar's length
constexpr float kBloomSec = 0.6f;
constexpr float kBloomCatch = 0.05f, kBloomBurst = 0.13f, kBloomSpread = 0.45f; // its stages end here (seconds)
// (larger and brighter than first drawn: Shockwave saw no bloom on a Steam Deck's 1280x800, the owner asked for more scale, 7 Nov 2026)
constexpr float kBloomReachH = 0.08f, kBloomInwardH = 0.03f, kBloomCreep = 0.20f;
constexpr float kBloomHaloA = 0.10f;                 // each of the halo's four layers: 0.40 at the bar (0.22 before)
constexpr float kGlowLayerA = 0.40f;                 // the breath's two wide layers under the arc (0.28 before)
constexpr float kTargetDelay = 0.06f;                // a target's bloom waits behind the player's
constexpr int kMaxBlooms = 2;
constexpr float kBloomEvery = 1.0f;                  // the same bar at most once a second
constexpr float kPi = 3.14159265f;

// The rate at value v (0..1): f = 1.25 * (2.9 / 1.25) ^ (1 - v / 0.25): 1.25 at 25 %, 1.75 at 15 %, 2.45 at 5 %, 2.9 at nothing.
inline float Rate(float v)
{
    const float c = v < 0 ? 0 : v > kOnAt ? kOnAt : v;
    return kRateTop * powf(kRateMax / kRateTop, 1 - c / kOnAt);
}

// The steady glow of FADES and NONE: rising as the value falls, 0.10 at 25 % to 0.28 at nothing.
inline float SteadyGlow(float v)
{
    const float c = v < 0 ? 0 : v > kOnAt ? kOnAt : v;
    return 0.10f + (0.28f - 0.10f) * (1 - c / kOnAt);
}

// One bar's breath. The phase is integrated, never sin(2 pi f t): a rate change must not jump the glow.
struct Breath
{
    bool on = false;
    float phase = 0, rate = kRateTop, depth = 1, quiet = 0, last = -1;

    // `v` 0..1 (under 0: unknown); `offline`: a failed shield climbs while it is down, so it does not breathe. Returns the glow, 0..1.
    float Step(float dt, float v, bool offline, int motion)
    {
        if (dt < 0)
            dt = 0;
        if (v < 0 || offline)
        {
            on = false, last = v;
            return 0;
        }
        on = on ? v <= kOffAt : v <= kOnAt;
        if (last >= 0 && v < last - 1e-4f)
            quiet = 0, depth = 1;
        else
            quiet += dt;
        last = v;
        if (!on)
            return 0;
        if (motion != 0)
            return SteadyGlow(v);
        rate += (Rate(v) - rate) * (dt >= kRateTau ? 1.0f : dt / kRateTau);
        phase += rate * dt;
        phase -= floorf(phase);
        if (quiet > kQuietAfter)
        {
            const float f = (quiet - kQuietAfter) / kQuietFall;
            depth = f >= 1 ? kQuietDepth : 1 - (1 - kQuietDepth) * f;
        }
        return depth * (0.5f - 0.5f * cosf(2 * kPi * phase));
    }
};

// What a bloom draws this frame.
struct BloomOut
{
    bool on = false;
    float light = 0;  // the whole bar lit in its own colour, 0..1
    float reach = 0;  // the halo's reach, 0..1 of kBloomReachH away from the reticle (inward 0.4 of it)
    float pips = 1;   // the empty pips coming up under it at the end, 0..1
};

// The curve of one bloom, by its age: catch, burst, spread, settle. `motion` 1 fades in place, 100 ms up and 300 ms down; 2 cuts.
inline BloomOut BloomAt(float t, int motion)
{
    BloomOut o;
    if (t < 0)
        return o;
    if (motion == 2)
        return o;
    if (motion == 1)
    {
        if (t >= 0.4f)
            return o;
        o.on = true;
        o.light = t < 0.1f ? t / 0.1f : 1 - (t - 0.1f) / 0.3f;
        o.pips = 0;
        return o;
    }
    if (t >= kBloomSec)
        return o;
    o.on = true;
    o.pips = 0;
    if (t < kBloomCatch)
        o.light = t / kBloomCatch;
    else if (t < kBloomBurst)
        o.light = 1, o.reach = (t - kBloomCatch) / (kBloomBurst - kBloomCatch);
    else if (t < kBloomSpread)
    {
        const float f = (t - kBloomBurst) / (kBloomSpread - kBloomBurst);
        o.light = 1 - f, o.reach = 1 + kBloomCreep * f;
    }
    else
    {
        o.reach = 1 + kBloomCreep;
        o.pips = (t - kBloomSpread) / (kBloomSec - kBloomSpread);
    }
    return o;
}

// One bar's bloom: started from the break's edge, limited to once a second.
struct Bloom
{
    float age = -1e9f, since = 1e9f, wait = 0; // (age -1 read as a bloom waiting: every bar "bloomed" in the first 1.6 s and blocked the real ones)

    // A break: `delay` holds a target's behind the player's. False: refused by the once-a-second limit.
    bool Start(float delay = 0)
    {
        if (since < kBloomEvery)
            return false;
        age = -delay, since = 0;
        return true;
    }
    void Step(float dt)
    {
        since += dt;
        if (age > -1e8f && (age < kBloomSec || age < 0))
            age += dt;
        if (age >= kBloomSec)
            age = -1e9f;
    }
    bool Active() const { return age > -1e8f; }
    BloomOut Now(int motion) const { return Active() ? BloomAt(age, motion) : BloomOut(); }
};

// At most kMaxBlooms full blooms at once: true while another may start.
inline bool BloomFree(const Bloom* const* all, int n)
{
    int active = 0;
    for (int i = 0; i < n; ++i)
        active += all[i] && all[i]->Active();
    return active < kMaxBlooms;
}

// The glow's strength along a bar at position p (0..1 of its length) with the lit part ending at `lit`: full over the lit pips, the same short roll-off at both
// ends, so the real value stays plain.
inline float GlowAt(float p, float lit)
{
    if (lit <= 0)
        return 0;
    const float r = kRollOff;
    if (p < 0 || p > lit + r)
        return 0;
    if (p <= lit - r || lit < r)
        return p <= lit ? 1.0f : 1 - (p - lit) / r;
    return p <= lit ? 1.0f : 1 - (p - lit) / r;
}
} // namespace hud2bars
