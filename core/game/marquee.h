// FLvibed - the marquee: how a text that does not fit its room shows all of itself (HUD 2
// decision 1, modules\hud\docs\UI_HUD2.md 5.20: "a marquee, the world-standard public-display pattern, instead of a permanent '...'
// cut"). A text the fit rule (textfit.h) would cut shows its start with the trailing ellipsis (the cut form, exactly), holds
// a short pause, then scrolls slowly and continuously until its end is visible, holds a short pause, then resets to the
// start - and repeats. The reset is a fade (out at the end, in at the start), not a jump: HUD 2's motion rule "continuous
// motion, never stepped" (UI_HUD2_PRESETS.md 1.2). A text that fits does not move.
// Pure: the offset, the clip and the alphas as a function of the time since the text appeared (frame-rate independent:
// no per-frame state, any sampling gives the same picture at the same time; sub-pixel: the offset is continuous). The
// kit (core\ui\hud2_kit.cpp TextMarquee) draws the full text clipped to its room with these values.
// Host-free tests: tests\marquee_test.cpp.
//
// The values (Timing) and why:
//  - the holds 1.5 s: Android's TextView marquee waits 1.2 s before it starts and before each restart (MARQUEE_DELAY /
//    MARQUEE_RESTART_DELAY, [R, memory]); a HUD is read in glances between flying, so a little longer - the owner's
//    "~1.5 s" - lets a glance land on the start (and on the end) before anything moves;
//  - the speed 0.05 H per second at the text floor (0.024 H, where the fit rule leaves every cut text): ~2 em/s, about 5
//    of Agency FB's narrow capitals a second - well under silent reading speed, so the eye can follow the moving word;
//    Android's 30 dp/s is ~0.04 H/s on a ~760 dp tall phone screen [R, memory], the owner's range 0.04-0.06 H/s. The speed
//    scales with the drawn size (a larger text scrolls as many characters a second);
//  - the reset fade 0.25 s each way: the panels' ease (PRESETS 1.2 Continuous motion, tau 0.25 s).
// Reduced motion: HUD 2 has no reduced-motion setting yet (modules\hud\docs\UI_HUD2.md 5.20); Static() below is the cut form a
// future switch would draw (WCAG 2.2.2 "Pause, Stop, Hide" asks for one where motion lasts over 5 s).
#pragma once
#include <cmath>

namespace marquee
{
struct Timing
{
    float startHoldS = 1.5f; // the start (its cut form) before it moves
    float speedH = 0.05f;    // H per second for a text at the floor size (the caller: Speed below)
    float endHoldS = 1.5f;   // the end shown before the reset
    float fadeS = 0.25f;     // the reset: the end fades out, then the start fades in
};

// The picture at one moment, in px from the room's left edge.
struct Frame
{
    bool moving = false; // false: the text fits its room (drawn as it is, never moves)
    float offset = 0;    // how far the text is moved left (0 .. overflow)
    float clip = 0;      // the text shows from 0 to `clip` (and never left of 0): <= the room
    float ellipsis = 0;  // the trailing ellipsis' alpha 0..1, drawn at `clip` (clipped to the room)
    float alpha = 1;     // the whole text's alpha (the reset's fade)
    int phase = 0;       // 0 the start held, 1 scrolling, 2 the end held, 3 fading out, 4 fading in
};

// The speed in px per second for a text drawn `size` px tall on a screen `floorPx` px floor (0.024 H, never under 14 px).
inline float Speed(const Timing& t, float screenH, float size, float floorPx)
{
    return floorPx > 0 && size > 0 ? t.speedH * screenH * (size / floorPx) : t.speedH * screenH;
}

// The cycle's length in seconds: fade in, the start held, the scroll, the end held, fade out.
inline float Period(const Timing& t, float overflow, float pxPerS)
{
    const float scroll = pxPerS > 0 && overflow > 0 ? overflow / pxPerS : 0.0f;
    return t.fadeS + t.startHoldS + scroll + t.endHoldS + t.fadeS;
}

inline float Clamp01(float v) { return v < 0 ? 0.0f : (v > 1 ? 1.0f : v); }

// A text only just over its room is no marquee: it stands whole, a little past the room's edge (Shockwave's video, 1280x800, the owner 7 Nov 2026: "Anubis" looped between "Anub" and
// "Anubis", a scroll of two letters that says nothing). The overflow may be an ellipsis and a half, or 8 % of the room: what the cut form gives up for its own ellipsis.
inline bool JustOver(float textW, float room, float ellW)
{
    const float a = 1.5f * ellW, b = 0.08f * room;
    return textW > room && textW - room <= (a > b ? a : b);
}

// The marquee's picture `offset` px into the scroll: the clip opens from the cut form's prefix (prefixW: the whole
// characters the fit rule kept before its ellipsis) towards the room - at least as fast as the text moves, so it reaches
// the room by the end - and keeps room for the ellipsis while more than an ellipsis' width is still hidden; the ellipsis
// fades over the last ellipsis' width of travel. Continuous in `offset`: at 0 it IS the cut form (the prefix, then the
// ellipsis at its end), at the overflow it is the text's end right-aligned in the room, no ellipsis.
inline Frame AtOffset(float textW, float room, float prefixW, float ellW, float offset)
{
    Frame f;
    if (!(textW > room) || room <= 0)
    {
        f.clip = textW < room ? textW : room;
        return f;
    }
    f.moving = true;
    const float overflow = textW - room;
    const float o = offset < 0 ? 0.0f : (offset > overflow ? overflow : offset);
    const float p = prefixW < 0 ? 0.0f : (prefixW > room ? room : prefixW);
    const float e = ellW > 0 ? Clamp01((overflow - o) / ellW) : 0.0f;
    const float rate = (room - p) / overflow > 1.0f ? (room - p) / overflow : 1.0f; // (reaches the room by the end)
    const float open = p + o * rate, keep = room - (ellW > 0 ? ellW : 0.0f) * e;
    f.offset = o;
    f.clip = open < keep ? open : keep;
    if (f.clip < p && o <= 0) // (the start: exactly the cut form, even where the prefix is wider than room - ellipsis)
        f.clip = p;
    f.clip = f.clip > room ? room : f.clip;
    f.ellipsis = e;
    return f;
}

// The picture `t` seconds after the text first showed (t < 0 = 0). The first cycle starts on the held start at full
// alpha (the text appears with its panel's own fade); every later cycle fades in from the reset.
inline Frame At(const Timing& tm, float textW, float room, float prefixW, float ellW, float pxPerS, double t)
{
    Frame f = AtOffset(textW, room, prefixW, ellW, 0);
    if (!f.moving || pxPerS <= 0)
        return f; // (fits, or no speed: the cut form, still)
    const float overflow = textW - room;
    const double period = Period(tm, overflow, pxPerS);
    if (!(period > 0))
        return f;
    const double u = std::fmod((t < 0 ? 0.0 : t) + tm.fadeS, period);
    const double a = tm.fadeS, b = a + tm.startHoldS, c = b + overflow / pxPerS, d = c + tm.endHoldS;
    if (u < a)
    {
        f.phase = 4;
        f.alpha = tm.fadeS > 0 ? Clamp01(static_cast<float>(u / tm.fadeS)) : 1.0f;
    }
    else if (u < b)
        f.phase = 0;
    else if (u < c)
    {
        f = AtOffset(textW, room, prefixW, ellW, static_cast<float>((u - b) * pxPerS));
        f.phase = 1;
    }
    else
    {
        f = AtOffset(textW, room, prefixW, ellW, overflow);
        f.phase = u < d ? 2 : 3;
        if (u >= d)
            f.alpha = tm.fadeS > 0 ? Clamp01(static_cast<float>(1.0 - (u - d) / tm.fadeS)) : 0.0f;
    }
    return f;
}

// The still picture (no motion): the cut form. For a future reduced-motion switch.
inline Frame Static(float textW, float room, float prefixW, float ellW) { return AtOffset(textW, room, prefixW, ellW, 0); }
} // namespace marquee
