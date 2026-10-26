#include "uidraw.h"

#include "render.h"

#include <algorithm>
#include <cmath>

namespace uidraw
{
u32 Fade(u32 argb, float a)
{
    return (static_cast<u32>(static_cast<float>(argb >> 24) * std::clamp(a, 0.0f, 1.0f) + 0.5f) << 24) |
           (argb & 0xFFFFFF);
}

// The space HUD's panel look (user, 21 Oct 2026: "identical to the game's space HUD ... that scanline animated
// background"). Measured on the HUD's empty panel at 4K, every frame for 5.6 s (research\experience\runs\
// hud_scanline_1021): a translucent violet-navy base (13, 11, 26 over space); an inner edge glow (45, 56, 73) fading over
// ~12 px, a brighter top line; and the scanline - hud_shipinfo.cmp's ANIM_HUD_scanline material, refreshtest2.tga (pale
// blue 160, 209, 255, alpha 50 at its leading edge fading to 0 over a quarter of it), scrolled by anim_hud_scanline 0.25
// per second: a horizontal line with a glow trailing above it, sweeping down the panel's full height every 4 s; measured
// +26 / +35 / +41 RGB at the line (~6 px at 4K), the trail ~20 % of the sweep.
void HudPanel(float x0, float y0, float w, float h, float alpha, float sh, double now)
{
    auto argb = [&](u32 rgb, float a)
    { return (static_cast<u32>(std::clamp(a * alpha, 0.0f, 1.0f) * 255.0f) << 24) | rgb; };
    render::Box(x0, y0, w, h, argb(0x0D0B1A, 1.0f));
    // inner edge glow: each edge a gradient from the glow colour to nothing
    const float e = std::max(2.0f, sh * 0.0056f); // ~12 px at 2160
    const u32 glow = argb(0x2D3849, 0.9f), none = argb(0x2D3849, 0.0f);
    render::Gradient(x0, y0, w, e, glow, glow, none, none);         // top
    render::Gradient(x0, y0 + h - e, w, e, none, none, glow, glow); // bottom
    render::Gradient(x0, y0, e, h, glow, none, none, glow);         // left
    render::Gradient(x0 + w - e, y0, e, h, none, glow, glow, none); // right
    const float line = std::max(1.0f, sh / 2160.0f);
    render::Box(x0, y0, w, line, argb(0x7B8FA6, 0.8f)); // the brighter top line
    // the scanline: the leading line at y, its trail above it, clipped to the panel
    const float period = 4.0f, trail = 0.22f * h, thick = std::max(2.0f, sh * 0.0028f);
    const float phase = static_cast<float>(std::fmod(now, static_cast<double>(period)) / period);
    const float y = y0 + phase * (h + trail); // the line enters at the top and leaves with its trail below
    const u32 band = 0xA0D1FF;                // refreshtest2.tga's colour
    const float peak = 50.0f / 255.0f * 1.6f; // its alpha at the edge, over the panel's own opacity
    const float t0 = std::max(y0, y - trail), t1 = std::min(y0 + h, y);
    if (t1 > t0)
    {
        // the gradient runs 0 at y - trail to 0.35 x peak at y (the trail), interpolated at the clipped ends
        auto at = [&](float yy) { return argb(band, peak * 0.35f * (yy - (y - trail)) / trail); };
        render::Gradient(x0, t0, w, t1 - t0, at(t0), at(t0), at(t1), at(t1));
    }
    const float l0 = std::max(y0, y - thick * 0.5f), l1 = std::min(y0 + h, y + thick * 0.5f);
    if (l1 > l0)
        render::Box(x0, l0, w, l1 - l0, argb(band, peak));
}

// The subtitles' backing: one soft panel in the HUD's dark blue (its panels are translucent navy), feathered over a few
// pixels instead of hard boxes (display.cpp, 19 Oct 2026).
void SoftPanel(float x0, float y0, float w, float h, float alpha, float feather)
{
    const u32 navy = 0x00081422, base = static_cast<u32>(std::clamp(alpha, 0.0f, 1.0f) * 255.0f);
    const float f = std::max(2.0f, feather);
    for (int ring = 3; ring >= 1; --ring)
    {
        const float e = f * ring / 3.0f;
        render::Box(x0 - e, y0 - e, w + 2 * e, h + 2 * e, ((base * (4 - ring) / 8) << 24) | navy);
    }
    render::Box(x0, y0, w, h, (base << 24) | navy);
}

void Frame(float x, float y, float w, float h, float t, u32 argb)
{
    t = std::max(1.0f, t);
    render::Box(x, y, w, t, argb);
    render::Box(x, y + h - t, w, t, argb);
    render::Box(x, y + t, t, std::max(0.0f, h - 2 * t), argb);
    render::Box(x + w - t, y + t, t, std::max(0.0f, h - 2 * t), argb);
}

void FocusFrame(float x, float y, float w, float h, float sh, u32 argb)
{
    const float t = std::max(1.0f, sh * 0.0012f);
    Frame(x, y, w, h, t, argb);
    const float g = std::max(2.0f, sh * 0.004f); // a soft inner glow
    const u32 in = Fade(argb, 0.25f), none = argb & 0xFFFFFF;
    render::Gradient(x + t, y + t, w - 2 * t, g, in, in, none, none);
    render::Gradient(x + t, y + h - t - g, w - 2 * t, g, none, none, in, in);
}

void Dot(float cx, float cy, float r, u32 argb)
{
    const int steps = std::max(2, static_cast<int>(r));
    for (int i = 0; i < steps; ++i)
    {
        const float yy = (i + 0.5f) / steps * r;
        const float half = std::sqrt(std::max(0.0f, r * r - yy * yy));
        const float rowH = r / steps;
        render::Box(cx - half, cy - yy - rowH * 0.5f, 2 * half, rowH, argb);
        render::Box(cx - half, cy + yy - rowH * 0.5f, 2 * half, rowH, argb);
    }
}

void Triangle(float cx, float cy, float size, int dir, u32 argb)
{
    const int steps = std::max(3, static_cast<int>(size * 0.5f));
    const float height = size * 0.85f;
    for (int i = 0; i < steps; ++i)
    {
        const float t = (i + 0.5f) / steps;        // 0 at the tip, 1 at the base
        const float half = size * 0.5f * t;        // the half width at this step
        const float d = height * (t - 0.5f);       // offset along the axis from the centre
        const float step = height / steps;
        switch (dir)
        {
        case 0: render::Box(cx - half, cy + d - step * 0.5f, 2 * half, step, argb); break; // up
        case 1: render::Box(cx - half, cy - d - step * 0.5f, 2 * half, step, argb); break; // down
        case 2: render::Box(cx + d - step * 0.5f, cy - half, step, 2 * half, argb); break; // left
        default: render::Box(cx - d - step * 0.5f, cy - half, step, 2 * half, argb); break; // right
        }
    }
}

// The game's slider (opt_scroll: half 351 x 27 at 4K): a thin track line, a thumb box ~0.037 of the track wide and the
// track's full height, an arrow outside each end.
void SliderTrack(float x, float y, float w, float sh, float t, bool enabled)
{
    const u32 track = enabled ? kTrack : kDisabled, thumb = enabled ? kThumb : kDisabled,
              arrow = enabled ? kText : kDisabled;
    const float line = std::max(1.0f, sh * 0.0014f);
    const float h = std::max(4.0f, sh * 0.012f); // the thumb's height
    render::Box(x, y - line * 0.5f, w, line, track);
    const float tw = std::max(6.0f, w * 0.1f), tx = x + std::clamp(t, 0.0f, 1.0f) * (w - tw);
    render::Box(tx, y - h * 0.5f, tw, h, Fade(thumb, 0.85f));
    Frame(tx, y - h * 0.5f, tw, h, line, thumb);
    const float a = std::max(6.0f, sh * 0.009f);
    Triangle(x - a, y, a, 2, arrow);
    Triangle(x + w + a, y, a, 3, arrow);
}

// The game's checkbox (opt_toggle: half 42 at 4K): a black square with a blue frame, a green tick.
void Checkbox(float x, float y, float size, bool on, bool enabled)
{
    const float t = std::max(1.0f, size * 0.04f);
    render::Box(x, y, size, size, kBoxFill);
    Frame(x, y, size, size, t, enabled ? kBoxFrame : kDisabled);
    if (!on)
        return;
    // the tick: a short stroke down-right from the left, a long one up-right to the top-right corner
    const u32 c = enabled ? kCheck : kDisabled;
    const float w = std::max(2.0f, size * 0.14f);
    const int n = std::max(4, static_cast<int>(size * 0.5f));
    for (int i = 0; i < n; ++i)
    {
        const float f = (i + 0.5f) / n;
        if (f < 0.35f)
        {
            const float g = f / 0.35f;
            render::Box(x + size * (0.18f + 0.22f * g) - w * 0.5f, y + size * (0.5f + 0.25f * g) - w * 0.5f, w, w, c);
        }
        else
        {
            const float g = (f - 0.35f) / 0.65f;
            render::Box(x + size * (0.40f + 0.45f * g) - w * 0.5f, y + size * (0.75f - 0.6f * g) - w * 0.5f, w, w, c);
        }
    }
}

// The resolution picker's box (x 2458..3140 at 4K, the arrow cell ~44 px wide at its right end).
float PickerBox(float x, float y, float w, float h, float sh, bool enabled)
{
    const u32 frame = enabled ? kFrame : kDisabled;
    const float t = std::max(1.0f, sh * 0.0014f);
    render::Box(x, y, w, h, 0x40000000);
    Frame(x, y, w, h, t, frame);
    const float cell = std::max(10.0f, h * 0.6f);
    render::Box(x + w - cell, y, t, h, frame); // the cell's divider
    const float a = std::max(5.0f, h * 0.28f);
    Triangle(x + w - cell * 0.5f, y + h * 0.3f, a, 0, enabled ? kText : kDisabled);
    Triangle(x + w - cell * 0.5f, y + h * 0.7f, a, 1, enabled ? kText : kDisabled);
    return cell;
}

void TextShadow(float x, float y, const std::wstring& s, u32 argb, float off)
{
    if (off > 0)
        render::Text(x + off, y + off, s, Fade(kShadow, static_cast<float>(argb >> 24) / 255.0f));
    render::Text(x, y, s, argb);
}

textfit::Result FitText(const render::Font& f, const std::wstring& s, float maxW, float floorPx)
{
    static textfit::Cache cache;
    const float size = static_cast<float>(f.px);
    // (the font's identity: its atlas texture, size and weight - a rebuilt font is a new key)
    const unsigned id = static_cast<unsigned>(reinterpret_cast<uintptr_t>(f.texture)) ^ (static_cast<unsigned>(f.px) << 20) ^
                        static_cast<unsigned>(f.weight);
    cache.Bound();
    if (const textfit::Result* r = cache.Find(s, size, floorPx, maxW, 0, id))
        return *r;
    static std::vector<float> adv;
    adv.resize(s.size());
    auto advance = [&](wchar_t ch)
    {
        auto it = f.glyphs.find(ch);
        if (it == f.glyphs.end())
            it = f.glyphs.find(L'?');
        return it != f.glyphs.end() ? static_cast<float>(it->second.advance) : 0.0f;
    };
    for (size_t i = 0; i < s.size(); ++i)
        adv[i] = advance(s[i]);
    return cache.Put(textfit::Fit(s, adv, advance(textfit::kEllipsis), size, size, floorPx, maxW, 0));
}

void TextShadowFit(float x, float y, const render::Font& f, const textfit::Result& r, u32 argb, float off)
{
    if (r.text.empty())
        return;
    const float scale = f.px > 0 ? r.size / static_cast<float>(f.px) : 1.0f;
    if (scale > 0.995f)
    {
        TextShadow(x, y, r.text, argb, off);
        return;
    }
    const float yy = y + f.lineHeight * (1.0f - scale) * 0.5f; // the same centre line
    if (off > 0)
        render::TextScaled(x + off, yy + off, r.text, Fade(kShadow, static_cast<float>(argb >> 24) / 255.0f), scale);
    render::TextScaled(x, yy, r.text, argb, scale);
}
} // namespace uidraw
