// What the pages over the game's windows share (nnpage.*, basepage.*): the holes the game's text shows through, the frame, the compare block. docs\NEURONET_PLAN.md.
#pragma once
#include "game/baseinfo.h"
#include "game/deckmode.h"
#include "game/equip_rules.h"
#include "game/gamedata.h"
#include "game/hudinfo.h"
#include "flexe.h"
#include "skin_rules.h"
#include "navmap_layout.h"
#include "nnstrip.h"
#include "safemem.h"
#include "skinchip.h"
#include "skinkit.h"
#include "ui/hud2_kit.h"
#include "ui/ui.h"
#include "ui/winplates.h"

namespace nnkit
{
using namespace hud2kit;

inline u32 R32(u32 a)
{
    u32 v = 0;
    safemem::Read(a, &v, 4);
    return v;
}
inline float RF(u32 a)
{
    float v = 0;
    safemem::Read(a, &v, 4);
    return v;
}
inline void W32(u32 a, u32 v)
{
    for (int i = 0; i < 4; ++i)
        safemem::Write(a + i, static_cast<u8>(v >> (8 * i)));
}

// ---- the holes: the game's text shows through
struct Box
{
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    bool ok = false;
    void Add(u32 ctl, float grow = 0)
    {
        ui::Item it;
        if (!ctl || !ui::ControlRect(ctl, it))
            return;
        float a = it.x - it.hw - grow, b = it.y - it.hh - grow, c = it.x + it.hw + grow, d = it.y + it.hh + grow;
        if (exe::VtText.is(R32(ctl))) // (a rich text answers its top left; its box is in pixels, its bar outside on the right)
            a = it.x - grow, b = it.y - grow, c = it.x + static_cast<float>(R32(ctl + 0x358)) + grow * 3, d = it.y + static_cast<float>(R32(ctl + 0x35C)) + grow;
        else if (it.hw <= 1)
            return;
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

// Several holes in one ground: the game's 3D icons stand in them.
struct Holes
{
    float r[16][4];
    int n = 0;
    void Add(float x0, float y0, float x1, float y1)
    {
        if (n < 16)
            r[n][0] = x0, r[n][1] = y0, r[n][2] = x1, r[n][3] = y1, ++n;
    }
};
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

// A control of the game's, centred on (cx, cy): its local position through its position property, as windowtext moves a text. A window off the screen keeps it where the page wants it.
using SetProp = void(__thiscall*)(u32, int, const float*, int);
// (a guard, not a rule: a window the pad closed this frame is gone before the page draws, 7 Nov 2026: the callers hold a fresh list, this stays for what still slips through)
inline bool SetPosition(u32 ctl, const float* pos)
{
    __try
    {
        reinterpret_cast<SetProp>(R32(R32(ctl) + 0xA8))(ctl, 6, pos, 0);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}
inline void PlaceAt(u32 ctl, float cx, float cy)
{
    ui::Item it;
    float pos[3] = {};
    if (!ctl || !ui::ControlRect(ctl, it) || !safemem::Read(ctl + 0xA8, pos, sizeof(pos)))
        return;
    if (fabsf(it.x - cx) < 1.5f && fabsf(it.y - cy) < 1.5f)
        return;
    const float H = winplates::g_H;
    pos[0] += (cx - it.x) / H, pos[1] += (it.y - cy) / H;
    SetPosition(ctl, pos);
}
// A row's icon: where the game's mesh of that row stands, or off the page.
struct Icon
{
    float cx = 0, cy = 0;
    bool on = false;
};
// The five meshes of a list (`format` by 1..5 or 0..), the `nth` of that name on the screen.
inline void PlaceIcons(const char* format, int base, int count, int nth, const Icon* icons)
{
    for (int i = 0; i < count; ++i)
    {
        char name[40];
        sprintf_s(name, format, base + i);
        PlaceAt(baseinfo::Find(name, nth), icons[i].on ? icons[i].cx : -winplates::g_W, icons[i].on ? icons[i].cy : 0);
    }
}
// The list of a window whose rows show: the game's own scrolled to `first`, so its five meshes are the page's.
inline void SyncList(u32 window, int first)
{
    const int now = static_cast<int>(R32(window + 0x3C4));
    if (first >= 0 && first != now)
        if (const u32 bar = R32(window + 0x3D0))
            uiclick::Scroll(bar, static_cast<float>(first - now), false, nullptr, nullptr);
}

struct Frame
{
    float W, H, left, right, top, bottom, titleY;
};
inline Frame Bounds()
{
    Frame f;
    f.W = winplates::g_W, f.H = winplates::g_H;
    f.left = deckmode::SideW() * f.W, f.right = f.W - f.left;
    float r[4] = {};
    f.titleY = nnstrip::Rect(r) ? (r[1] + r[3]) / 2 : 0.07f * f.H;
    f.top = 0.135f * f.H, f.bottom = 0.865f * f.H;
    return f;
}
inline void Title(const Frame& f, const std::wstring& word) { Text(f.left, f.titleY, word, 0.044f * f.H, Argb(kInk, 1)); }

inline std::wstring Money(long long v)
{
    std::wstring s = std::to_wstring(v < 0 ? -v : v);
    for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3)
        s.insert(static_cast<size_t>(i), L",");
    return (v < 0 ? L"-$" : L"$") + s;
}

// The mouse's chips stand in the prompts' lane, level with CLOSE.
inline float LaneY() { return deckmode::LaneTopH() * winplates::g_H; }
inline float LaneChipH() { return (deckmode::LaneBottomH() - deckmode::LaneTopH()) * winplates::g_H; }

// One chip at (x, y), its click; the x after it.
inline float ChipAt(float x, float y, float h, const std::wstring& word, skinchip::Action act, int arg, bool on = true)
{
    const float size = skinrules::kChipWord * h / skinrules::kChipLow;
    const navlayout::Rect r{x, y, x + TextWidth(word, size) + 0.03f * winplates::g_H, y + h};
    const int slot = on ? skinchip::Hot(r, act, arg) : -1;
    skinchip::Chip(r, word, size, !on ? skinchip::Look::Disabled : slot >= 0 && slot == skinchip::Hovered() ? skinchip::Look::Hover : skinchip::Look::Plain);
    return r.x1 + 0.012f * winplates::g_H;
}

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

inline std::wstring Upper(std::wstring s)
{
    for (wchar_t& c : s)
        c = towupper(c);
    return s;
}

// An item against what the ship carries: a heading, then its numbers, each beside the mounted item's difference. The y after it.
inline float Compare(float x0, float x1, float y, u32 arch, bool mounted, float H)
{
    const equiprules::Spec* me = gamedata::Equip(arch);
    if (!me || me->stats.empty())
        return y;
    using namespace equiprules;
    const gamedata::Versus v = mounted ? gamedata::Versus() : gamedata::Against(arch);
    const Spec* other = v.arch ? gamedata::Equip(v.arch) : nullptr;
    std::wstring head;
    u32 rgb = kChrome;
    if (mounted)
        head = L"MOUNTED";
    else if (v.hasSlot && !v.fits)
        head = L"NO HARDPOINT ON THIS SHIP TAKES IT", rgb = kAmber;
    else if (v.tagged && other) // (the player's choice first, a free hardpoint or not)
        head = L"VS " + Upper(hudinfo::EquipName(v.arch)) + L" (TAGGED)", rgb = kActive;
    else if (v.freeHp)
        head = L"FITS A FREE HARDPOINT", rgb = kFriend;
    else if (other)
        head = L"VS " + Upper(hudinfo::EquipName(v.arch));
    else
        head = L"NUMBERS";
    const std::wstring slot = SlotWord(SlotOf(me->type));
    const float sw = slot.empty() ? 0 : TextWidth(slot, 0.022f * H) + 0.012f * H;
    TextMarquee(x0, y + 0.014f * H, head, 0.026f * H, x1 - x0 - sw, Argb(rgb, 1));
    if (!slot.empty())
        Text(x1, y + 0.014f * H, slot, 0.022f * H, Argb(kChrome, 0.8f), Right);
    y += 0.034f * H;
    const float pitch = 0.032f * H, c1 = other ? x0 + (x1 - x0) * 0.68f : x1; // (no difference to show: the value stands at the column's edge)
    for (const Stat& s : me->stats)
    {
        Text(x0, y + pitch / 2, s.label, 0.024f * H, Argb(kChrome, 1));
        Text(c1, y + pitch / 2, ValueWord(s.value) + L" " + s.unit, 0.026f * H, Argb(kInk, 1), Right);
        float theirs = 0;
        if (other && Find(other->stats, s.label, theirs))
        {
            const Verdict j = Judge(s, theirs);
            Text(x1, y + pitch / 2, DeltaWord(s.value, theirs), 0.026f * H, Argb(j == Verdict::Better ? kFriend : j == Verdict::Worse ? kHostile : kChrome, 1), Right);
        }
        y += pitch;
    }
    return y + 0.008f * H;
}
} // namespace nnkit
