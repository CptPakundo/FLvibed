// FLvibed - the pages' draw kit (the HUD 2 page framework: audit 70b 2.4): what a view may draw with, and nothing that
// reads or works the game. HUD 2's draw kit (core\ui\hud2_kit.h: text, rectangles, colours), the skin's primitives
// (skinkit.h: the scroll bar) and a chip's look (chiplook.h), and the pages' own: the ground and its holes, the frame and
// its title, chips whose click box is a record (page_view.h Hit), text wrapping and the words of money, and the compare
// block drawn from what was read for it. What reads the game for these (the frame's place, a hole round the game's text,
// the compare's numbers) is nnpage_kit.h's.
#pragma once
#include "ui/hud2_kit.h"
#include "../chiplook.h"
#include "../skinkit.h"
#include "page_view.h"
#include "game/screens/compare_model.h"
#include "game/screens/places_model.h"
#include "game/screens/status_rules.h"

#include <algorithm>
#include <cwchar>
#include <string>
#include <vector>

namespace pagekit
{
using namespace hud2kit;

// ---- the ground: HUD 2's page over everything but its holes, where the game's text or 3D shows through
// A rectangle that grows to take in each one added (pixels); `ok` once anything was added.
struct Box
{
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    bool ok = false;
    void AddRect(float a, float b, float c, float d)
    {
        x0 = ok ? std::min(x0, a) : a, y0 = ok ? std::min(y0, b) : b, x1 = ok ? std::max(x1, c) : c, y1 = ok ? std::max(y1, d) : d;
        ok = true;
    }
};
inline void Ground(float x0, float y0, float x1, float y1, float alpha = 1)
{
    if (x1 > x0 && y1 > y0)
        Rect(x0, y0, x1 - x0, y1 - y0, Argb(0x060716, alpha));
}
// The page's ground, but for `hole`.
inline void Cover(float W, float H, const Box& hole, float alpha = 1)
{
    if (!hole.ok)
    {
        Ground(0, 0, W, H, alpha);
        return;
    }
    Ground(0, 0, W, hole.y0, alpha);
    Ground(0, hole.y1, W, H, alpha);
    Ground(0, hole.y0, hole.x0, hole.y1, alpha);
    Ground(hole.x1, hole.y0, W, hole.y1, alpha);
}

// Several holes in one ground: the game's 3D icons stand in them (page_view.h: a view gives its holes back).
using page::Holes;
inline void GroundBut(float x0, float y0, float x1, float y1, const Holes& h)
{
    float ys[34];
    int m = 0;
    ys[m++] = y0, ys[m++] = y1;
    for (int i = 0; i < h.n; ++i)
        ys[m++] = std::max(y0, std::min(y1, h.r[i][1])), ys[m++] = std::max(y0, std::min(y1, h.r[i][3]));
    std::sort(ys, ys + m);
    for (int b = 0; b + 1 < m; ++b)
    {
        const float a = ys[b], c = ys[b + 1];
        if (c <= a)
            continue;
        int order[16], k = 0;
        for (int i = 0; i < h.n; ++i)
            if (h.r[i][1] <= a && h.r[i][3] >= c)
                order[k++] = i;
        std::sort(order, order + k, [&](int p, int q) { return h.r[p][0] < h.r[q][0]; });
        float x = x0;
        for (int i = 0; i < k; ++i)
        {
            Ground(x, a, h.r[order[i]][0], c);
            x = std::max(x, h.r[order[i]][2]);
        }
        Ground(x, a, x1, c);
    }
}

// ---- the frame: the screen, the content box between the side columns, the title's line (pixels; nnpage_kit.h Bounds)
struct Frame
{
    float W, H, left, right, top, bottom, titleY;
};
inline void Title(const Frame& f, const std::wstring& word) { Text(f.left, f.titleY, word, 0.044f * f.H, Argb(kInk, 1)); }

// ---- chips
// One chip at (x, y), `h` high, its word's width: its click box `action` / `arg` recorded in `out` while it is `on` (a
// disabled chip has none); lit while `in.hover` names that box. The x after it.
inline float ChipAt(const page::ViewIn& in, page::ViewOut& out, float x, float y, float h, const std::wstring& word, int action, int arg, bool on = true)
{
    const float size = skinrules::kChipWord * h / skinrules::kChipLow;
    const navlayout::Rect r{x, y, x + TextWidth(word, size) + 0.03f * in.H, y + h};
    int box = -1;
    if (on && out.nHits < page::ViewOut::kHits)
        box = out.nHits, out.hits[out.nHits++] = {r.x0, r.y0, r.x1, r.y1, action, arg};
    skinchip::Chip(r, word, size, !on ? skinchip::Look::Disabled : box >= 0 && box == in.hover ? skinchip::Look::Hover : skinchip::Look::Plain);
    return r.x1 + 0.012f * in.H;
}

// A box the mouse can click that draws nothing itself (a row; `action` -1: one that only keeps the mouse off what lies under
// it, a card): recorded in `out`. Its index (ViewIn::hover names it when the mouse is on it), -1 when `out` is full.
inline int HitAt(page::ViewOut& out, float x0, float y0, float x1, float y1, int action, int arg)
{
    if (out.nHits >= page::ViewOut::kHits)
        return -1;
    out.hits[out.nHits] = {x0, y0, x1, y1, action, arg};
    return out.nHits++;
}

// ---- words
// `text` in lines that fit `width`: its own newlines kept, by whole words.
inline std::vector<std::wstring> Wrap(const std::wstring& text, float size, float width)
{
    std::vector<std::wstring> lines;
    size_t i = 0;
    while (i <= text.size())
    {
        size_t e = text.find(L'\n', i);
        if (e == std::wstring::npos)
            e = text.size();
        std::wstring line;
        size_t w = i;
        while (w < e)
        {
            size_t sp = text.find(L' ', w);
            if (sp == std::wstring::npos || sp > e)
                sp = e;
            const std::wstring word = text.substr(w, sp - w);
            const std::wstring next = line.empty() ? word : line + L" " + word;
            if (!line.empty() && TextWidth(next, size) > width)
            {
                lines.push_back(line);
                line = word;
            }
            else
                line = next;
            w = sp + 1;
        }
        lines.push_back(line);
        i = e + 1;
    }
    return lines;
}

using screens::Upper; // (core\game\screens\screen_rules.h: a screen's read says it the same way)

inline std::wstring Money(long long v)
{
    std::wstring s = std::to_wstring(v < 0 ? -v : v);
    for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3)
        s.insert(static_cast<size_t>(i), L",");
    return (v < 0 ? L"-$" : L"$") + s;
}

// ---- a heading: the word on the left, a tag on the right, a rule under them (the docked pages' columns)
inline void Heading(float x0, float x1, float y, float H, const std::wstring& word, const std::wstring& tag)
{
    Text(x0, y, word, 0.030f * H, Argb(kChrome, 1));
    Text(x1, y, tag, 0.026f * H, Argb(kDim, 1), Right);
    skinkit::Rule(x0, x1, y + 0.024f * H);
}

// ---- a card: the game's description, wrapped, as many lines as stand in the room from `first` (clamped here); a scroll bar
// when it is longer. `lines` / `room` what it laid out. The y it ends at (`foot`), `y` when there is no text.
inline float Card(float x0, float x1, float y, float foot, float H, const std::wstring& text, int& first, int& lines, int& room)
{
    if (text.empty())
        return y;
    const float size = 0.025f * H, pitch = 0.031f * H;
    const std::vector<std::wstring> wrapped = Wrap(text, size, x1 - x0 - 0.016f * H);
    lines = static_cast<int>(wrapped.size());
    room = std::max(1, static_cast<int>((foot - y) / pitch));
    first = std::max(0, std::min(first, lines - room));
    for (int i = 0; i < room && first + i < lines; ++i)
        Text(x0, y + pitch * (i + 0.5f), wrapped[first + i], size, Argb(kInk, 0.9f));
    if (lines > room)
        skinkit::ScrollBar(x1 - 0.004f * H, y, foot, static_cast<float>(first) / lines, static_cast<float>(room) / lines);
    return foot;
}

// ---- the NeuroNet's left column (LOG's and INFO's; audit 70b F4a, from nnpage.cpp): a label over its value, where the
// player is, a list of places (core\game\screens\places_model.h)
// A label over its value at the frame's left, up to `x1`; nothing for an empty value. The y after it.
inline float Fact(const Frame& f, float y, float x1, const wchar_t* label, const std::wstring& value)
{
    if (value.empty())
        return y;
    Text(f.left, y, label, 0.024f * f.H, Argb(kChrome, 1));
    TextMarquee(f.left, y + 0.036f * f.H, value, 0.034f * f.H, x1 - f.left, Argb(kInk, 1));
    return y + 0.082f * f.H;
}
// The system, then the base (docked) or the zone. The y after them.
inline float HereFacts(const Frame& f, float y, float x1, const screens::Here& h)
{
    y = Fact(f, y, x1, L"SYSTEM", h.system);
    return Fact(f, y, x1, h.docked ? L"BASE" : L"ZONE", h.place);
}
// A list of places under a rule and its title, nearest first, a row each (its name in `rgbOf(place)`, its distance, what it
// is) down to the frame's foot; a list cut there says so beside its title ("8 OF 20": the owner, 7 Nov 2026). Nothing when empty.
template <class RgbOf>
inline void PlaceList(const Frame& f, float y, float x0, float x1, const wchar_t* title, const std::vector<screens::Place>& list, RgbOf rgbOf)
{
    if (list.empty())
        return;
    const float H = f.H;
    skinkit::Rule(x0, x1, y);
    y += 0.024f * H;
    Text(x0, y, title, 0.024f * H, Argb(kChrome, 1));
    const float rowH = 0.058f * H;
    const size_t fit = static_cast<size_t>(std::max(0.0f, (f.bottom - y - 0.024f * H) / rowH));
    if (fit < list.size())
    {
        wchar_t of[32];
        swprintf_s(of, L"%u OF %u", static_cast<unsigned>(fit), static_cast<unsigned>(list.size()));
        Text(x1, y, of, 0.024f * H, Argb(kInk, 1), Right);
    }
    y += 0.024f * H;
    for (size_t i = 0; i < list.size() && y + rowH <= f.bottom; ++i, y += rowH)
    {
        const screens::Place& p = list[i];
        wchar_t d[24];
        swprintf_s(d, L"%.1f K", p.distance / 1000);
        const float dw = TextWidth(d, 0.026f * H);
        TextMarquee(x0, y + rowH * 0.32f, p.name, 0.028f * H, x1 - x0 - dw - 0.02f * H, Argb(rgbOf(p), 1));
        Text(x1, y + rowH * 0.32f, d, 0.026f * H, Argb(kChrome, 1), Right);
        Text(x0, y + rowH * 0.76f, p.what, 0.024f * H, Argb(kChrome, 0.8f));
    }
}

// ---- a standing's colour (STATUS's factions, INFO's; its word: core\game\screens\status_rules.h BandWord; audit 70b F4b,
// from nnpage.cpp)
inline u32 BandRgb(statusrules::Band b)
{
    const u32 kRgb[] = {kHostile, kAmber, kNeutral, kChrome, kFriend};
    return kRgb[b];
}

// ---- the compare block: an item against what the ship carries (its data: core\game\screens\compare_model.h)
using screens::CompareBlock;
using screens::CompareRow;
// The block from `x0` to `x1` at `y`: a heading, then its numbers, each beside the other item's difference. The y after it.
inline float DrawCompare(float x0, float x1, float y, const CompareBlock& b, float H)
{
    if (!b.ok)
        return y;
    const float sw = b.slot.empty() ? 0 : TextWidth(b.slot, 0.022f * H) + 0.012f * H;
    TextMarquee(x0, y + 0.014f * H, b.head, 0.026f * H, x1 - x0 - sw, Argb(b.headRgb, 1));
    if (!b.slot.empty())
        Text(x1, y + 0.014f * H, b.slot, 0.022f * H, Argb(kChrome, 0.8f), Right);
    y += 0.034f * H;
    const float pitch = 0.032f * H, c1 = b.versus ? x0 + (x1 - x0) * 0.68f : x1; // (no difference to show: the value stands at the column's edge)
    for (const CompareRow& row : b.rows)
    {
        Text(x0, y + pitch / 2, row.label, 0.024f * H, Argb(kChrome, 1));
        Text(c1, y + pitch / 2, row.value, 0.026f * H, Argb(kInk, 1), Right);
        if (!row.delta.empty())
            Text(x1, y + pitch / 2, row.delta, 0.026f * H, Argb(row.deltaRgb, 1), Right);
        y += pitch;
    }
    return y + 0.008f * H;
}
} // namespace pagekit
