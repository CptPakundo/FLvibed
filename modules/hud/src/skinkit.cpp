// HUD 2's skin primitives - see skinkit.h. Nothing here reads the game: what a primitive shows is the caller's.
#include "skinkit.h"

#include "game/deckmode.h"

#include <algorithm>

namespace skinkit
{
using namespace hud2kit;
using namespace skinrules;

namespace
{
float g_W = 0, g_H = 0;
}

void SetScreen(float W, float H) { g_W = W, g_H = H; }
float Px(float f) { return f * g_H; }

void Rim(float x0, float y0, float x1, float y1, float c, int corners, float a)
{
    const float tl = (corners & kTL) ? c : 0, tr = (corners & kTR) ? c : 0, br = (corners & kBR) ? c : 0, bl = (corners & kBL) ? c : 0;
    const float pts[16] = {x0 + tl, y0, x1 - tr, y0, x1, y0 + tr, x1, y1 - br, x1 - br, y1, x0 + bl, y1, x0, y1 - bl, x0, y0 + tl};
    Outline(pts, 8, std::max(2.0f, Px(0.0013f)), Argb(kChrome, a));
}

void Plate(float x0, float y0, float x1, float y1, int corners, float fill, float rimAlpha)
{
    if (x1 - x0 < 4 || y1 - y0 < 4)
        return;
    const float c = Px(kChamfer);
    hud2kit::Plate(x0, y0, x1, y1, c, corners, fill, kChrome, 0);
    Rim(x0, y0, x1, y1, c, corners, rimAlpha);
}

void AccentEdge(float x0, float y0, float x1, float y1, Edge edge, u32 rgb)
{
    const float t = std::max(2.0f, Px(0.004f));
    if (edge == Edge::Top)
        Rect(x0, y0, x1 - x0, t, Argb(rgb, 1));
    else
        Rect(edge == Edge::Left ? x0 : x1 - t, y0, t, y1 - y0, Argb(rgb, 1));
}

void Rule(float x0, float x1, float y, float a) { Line(x0 + Px(0.020f), y, x1 - Px(0.020f), y, std::max(2.0f, Px(kHeaderRule)), Argb(kChrome, a)); }

float Header(float x0, float x1, float y, const std::wstring& title, const std::wstring& tag)
{
    const float size = Px(kHeaderTitle), left = x0 + Px(0.020f), cy = y + size / 2;
    const float w = TextMarquee(left, cy, title, size, x1 - left - Px(0.020f), Argb(kInk, 1));
    if (!tag.empty())
        Text(left + w + Px(0.016f), cy, tag, Px(kHeaderTag), Argb(kChrome, 1));
    const float under = y + size + Px(0.010f);
    Rule(x0, x1, under);
    return under + Px(kHeaderRule);
}

void Section(float x, float y, const std::wstring& text) { Text(x, y, text, Px(kSection), Argb(kChrome, 1), Left, 0.04f); }

float Body(float x, float y, float w, const std::wstring& text, int maxLines, u32 rgb, Align align)
{
    std::wstring lines[8];
    float size = Px(kBodyLow);
    const int n = FitLines(text, size, w, 0, lines, std::min(8, std::max(1, maxLines)), size);
    const float pitch = size * 1.15f;
    for (int i = 0; i < n; ++i)
        Text(align == Center ? x + w / 2 : x, y + pitch * (static_cast<float>(i) + 0.5f), lines[i], size, Argb(rgb, 1), align);
    return y + pitch * static_cast<float>(n);
}

void Row(float x0, float x1, float y, float h, const std::wstring& name, const std::wstring& value)
{
    const float rowH = std::max(h, Px(kRowMin)), cy = y + rowH / 2, size = Px(kBodyLow);
    const float valueW = value.empty() ? 0 : Text(x1, cy, value, size, Argb(kInk, 1), Right);
    TextMarquee(x0, cy, name, size, x1 - x0 - valueW - Px(0.012f), Argb(kInk, 1));
}

void ChipGlass(float x0, float y0, float x1, float y1)
{
    const float c = Px(0.010f);
    hud2kit::Plate(x0, y0, x1, y1, c, kTL | kBR, 220 / 255.0f, kChrome, 0);
    Rim(x0, y0, x1, y1, c, kTL | kBR, 120 / 255.0f);
}

void Chip(float x0, float y0, float x1, float y1, const std::wstring& word, ChipState s)
{
    ChipGlass(x0, y0, x1, y1);
    if (s.focused)
        Focus(x0, y0, x1, y1);
    const u32 rgb = s.disabled ? kDim : s.focused ? kInk : kChrome;
    TextFit((x0 + x1) / 2, (y0 + y1) / 2, word, Px(kChipWord), x1 - x0 - Px(0.016f), Argb(rgb, 1), Center);
}

void Tab(float x0, float y0, float x1, float y1, const std::wstring& word, bool current)
{
    ChipGlass(x0, y0, x1, y1);
    if (current)
        Line(x0 + Px(0.012f), y1 - Px(0.004f), x1 - Px(0.012f), y1 - Px(0.004f), Px(0.004f), Argb(kActive, 1));
    TextFit((x0 + x1) / 2, (y0 + y1) / 2, word, Px(kChipWord), x1 - x0 - Px(0.016f), Argb(current ? kActive : kChrome, 1), Center);
}

// The fill alone measured olive (86, 85, 49) over the navy glass and half-read "disabled": the ACTIVE edges carry it.
void Focus(float x0, float y0, float x1, float y1, float fill)
{
    const float g = Px(kFocusGap), bw = Px(kFocusBar), e = std::max(1.0f, Px(kFocusEdge));
    Rect(x0, y0, x1 - x0, y1 - y0, Argb(kActive, fill));
    Rect(x0, y0, x1 - x0, e, Argb(kActive, 0.85f));
    Rect(x0, y1 - e, x1 - x0, e, Argb(kActive, 0.85f));
    Rect(x0 - g - bw, y0, bw, y1 - y0, Argb(kActive, 1));
}

void ScrollBar(float x, float y0, float y1, float first, float span)
{
    const float w = Px(kScrollTrack), h = y1 - y0;
    Rect(x, y0, w, h, Argb(kDim, 0.5f));
    const float a = std::min(1.0f, std::max(0.0f, first)), s = std::min(1.0f - a, std::max(0.05f, span));
    Rect(x, y0 + h * a, w, h * s, Argb(kChrome, 1));
}

void Scrim(float peak, float level)
{
    if (level > 0)
        Rect(0, 0, g_W, g_H, Argb(0x060716, peak * level));
}

void Dialog(const DialogBox& b, const std::wstring& title, const std::wstring& cost, bool amber)
{
    Plate(b.x0, b.y0, b.x1, b.y1, kTL | kBR, kDialogFill);
    if (amber)
        AccentEdge(b.x0, b.y0, b.x1, b.y1, Edge::Top, kAmber);
    const float cx = (b.x0 + b.x1) / 2, wide = b.x1 - b.x0 - Px(0.040f);
    TextFit(cx, b.titleY, title, Px(kHeaderTitle), wide, Argb(kInk, 1), Center);
    if (!cost.empty())
        Body(b.x0 + Px(0.020f), b.costY - Px(kBodyHigh) / 2, wide, cost, 3, kChrome, Center);
}

bool Tooltip(float x, float y, const std::wstring& text)
{
    if (text.empty() || g_W <= 0)
        return false;
    const float size = Px(kTooltipBody), pad = Px(0.010f), side = g_W * deckmode::SideW(), edge = g_H * deckmode::EdgeH();
    std::wstring lines[4];
    float drawn = size;
    const float room = std::min(g_W * 0.4f, g_W - 2 * side - 2 * pad);
    int n = 0;
    for (size_t at = 0; at <= text.size() && n < 4;) // (the game's own line breaks first: a character's card is three lines)
    {
        size_t end = text.find(L'\n', at);
        if (end == std::wstring::npos)
            end = text.size();
        n += FitLines(text.substr(at, end - at), size, room, 0, lines + n, 4 - n, drawn);
        at = end + 1;
    }
    if (n <= 0)
        return false;
    float w = 0;
    for (int i = 0; i < n; ++i)
        w = std::max(w, TextWidth(lines[i], drawn));
    const float pitch = drawn * 1.15f, h = pitch * static_cast<float>(n) + 2 * pad;
    const float x0 = std::min(std::max(x, side), g_W - side - w - 2 * pad), y0 = std::min(std::max(y, edge), g_H - edge - h);
    Plate(x0, y0, x0 + w + 2 * pad, y0 + h, kTL | kBR);
    for (int i = 0; i < n; ++i)
        Text(x0 + pad, y0 + pad + pitch * (static_cast<float>(i) + 0.5f), lines[i], drawn, Argb(kInk, 1));
    return true;
}

void Pips(float x, float y, float w, float h, float share, int n, u32 rgb)
{
    const float lit = n > 0 ? static_cast<float>(PipsLit(share, n)) / static_cast<float>(n) : 0;
    SegBar(x, y, w, h, lit, rgb, n);
}

void HoldRing(float cx, float cy, float r, float share, u32 rgb)
{
    const float width = std::max(2.0f, Px(0.003f));
    if (share > 0)
        Arc(cx, cy, r, -90, -90 + 360 * std::min(1.0f, share), width, Argb(rgb, 1));
}
} // namespace skinkit
