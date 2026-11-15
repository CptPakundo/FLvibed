// The save lists under HUD 2: the rows that fit their panel (skinlists.h; facts docs\FREELANCER_RE.md "The save lists' rows").
#include "skinlists.h"

#include "game/menu.h"
#include "hook.h"
#include "skin_rules.h"

#include <sstream>

namespace skinlists
{
namespace
{
// A column's box as the game made it and as this file trimmed it: a height that is not the trimmed one is the game's (a new list,
// or the game wrote it again), so nothing is assumed across windows.
struct Kept
{
    u32 list = 0;
    float gameHeight = 0, myHeight = 0;
    int fit = 0;
};
Kept g_kept[2];
int g_writes = 0;
std::string g_last = "none";

// ListBox fields (Freelancer-Decomp list_box.h): +0x88C the first row drawn, +0x89C the box's height, +0x8A8 the rows drawn.
// UIScroll (ui_scroll.h): +0x6C flags (bits 0-1: live, shown), +0x3C0 / +0x3C4 the range, +0x3C8 the value, +0x3CC the page,
// +0x428 the page its range command restores; slot 42 (vt+0xA8) command 0x38 sets the value, clamps and lays the bar out again.
template <typename T> bool Put(u32 addr, T v)
{
    __try
    {
        if (Mem<T>(addr) == v)
            return false;
        Mem<T>(addr) = v;
        ++g_writes;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}
template <typename T> T Get(u32 addr, T def)
{
    __try
    {
        return Mem<T>(addr);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return def;
    }
}
bool BarValue(u32 bar, int value)
{
    typedef int(__thiscall * PropFn)(u32, u32, u32, u32);
    __try
    {
        reinterpret_cast<PropFn>((*reinterpret_cast<void***>(bar))[0xA8 / 4])(bar, 0x38, static_cast<u32>(value), 0);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

Kept* KeptFor(u32 list, float height)
{
    for (Kept& k : g_kept)
        if (k.list == list)
        {
            if (k.myHeight != height)
                k.gameHeight = height, k.myHeight = 0;
            return &k;
        }
    for (Kept& k : g_kept)
        if (!k.list)
        {
            k = {list, height, 0};
            return &k;
        }
    return nullptr;
}
bool Ours(const Kept& k, const menu::SaveLists& s) { return k.list && (k.list == s.names.list || k.list == s.dates.list); }

// The bar for `rows` of `count`: page = rows, range 0..count, the value inside it; shown with the list when the saves outnumber
// the rows (the game's own rule against its own count). The lists' first row follows the bar's value, as its move tells them (0x3E).
void Bar(const menu::SaveLists& s, int rows, bool show)
{
    const int count = s.names.count;
    if (!s.scroll)
        return;
    bool moved = Put<float>(s.scroll + 0x3CC, static_cast<float>(rows));
    moved |= Put<float>(s.scroll + 0x428, static_cast<float>(rows));
    if (count > rows)
    {
        moved |= Put<float>(s.scroll + 0x3C0, 0.0f);
        moved |= Put<float>(s.scroll + 0x3C4, static_cast<float>(count));
    }
    if (show)
    {
        const u8 flags = Get<u8>(s.scroll + 0x6C, 0), live = static_cast<u8>(Get<u8>(s.names.list + 0x6C, 3) & 3);
        Put<u8>(s.scroll + 0x6C, static_cast<u8>((flags & ~3) | live));
    }
    const float value = Get<float>(s.scroll + 0x3C8, 0.0f);
    const int want = std::min(static_cast<int>(value), skinrules::LastFirstRow(count, rows));
    if (moved || static_cast<int>(value) != want)
        BarValue(s.scroll, std::max(0, want));
    const int first = count > rows ? static_cast<int>(Get<float>(s.scroll + 0x3C8, 0.0f)) : 0;
    Put<int>(s.names.list + 0x88C, first);
    Put<int>(s.dates.list + 0x88C, first);
}
} // namespace

void Frame(bool on)
{
    menu::SaveLists s;
    if (!on || !menu::OpenSaveLists(s))
    {
        g_kept[0] = g_kept[1] = {};
        g_last = "none";
        return;
    }
    for (Kept& kept : g_kept) // another window's columns: forgotten
        if (!Ours(kept, s))
            kept = {};
    Kept* k[2] = {KeptFor(s.names.list, s.names.height), KeptFor(s.dates.list, s.dates.height)};
    const menu::SaveColumn* c[2] = {&s.names, &s.dates};
    int fit = s.gameRows;
    float room[2] = {};
    for (int i = 0; i < 2; ++i)
    {
        if (!k[i] || !(c[i]->height > 0))
            return;
        // the game's box in pixels (the trimmed one scales back), the panel's room the smaller of it and the bar
        const float box = c[i]->boxPx * k[i]->gameHeight / c[i]->height;
        room[i] = s.scrollPx > 0 ? std::min(box, s.scrollPx) : box;
        fit = std::min(fit, skinrules::RowsThatFit(room[i], c[i]->linePx, s.gameRows));
    }
    for (int i = 0; i < 2; ++i)
    {
        Put<int>(c[i]->list + 0x8A8, fit);
        // the box trimmed to the rows drawn, so the hover (rows of the line height inside the box) reaches no row below them
        // (once per fit: the box read back through the projection differs in its last bits frame to frame)
        if (k[i]->myHeight > 0 && k[i]->fit == fit)
            continue;
        const float box = c[i]->boxPx * k[i]->gameHeight / c[i]->height;
        const float h = fit * c[i]->linePx < box ? k[i]->gameHeight * (fit * c[i]->linePx) / box : k[i]->gameHeight;
        k[i]->myHeight = h;
        k[i]->fit = fit;
        Put<float>(c[i]->list + 0x89C, h);
    }
    if (s.names.count > fit)
        Bar(s, fit, true);
    else // every save drawn: from the first (the game's bar stays as it made it, hidden)
    {
        Put<int>(s.names.list + 0x88C, 0);
        Put<int>(s.dates.list + 0x88C, 0);
    }
    std::ostringstream o;
    o << (s.front ? "front" : "game") << " fit " << fit << " of " << s.gameRows << " saves " << s.names.count << " room " << room[0];
    g_last = o.str();
}

void GiveBack()
{
    menu::SaveLists s;
    if (menu::OpenSaveLists(s))
    {
        for (const Kept& k : g_kept)
            if (Ours(k, s) && k.myHeight > 0 && Get<float>(k.list + 0x89C, 0.0f) == k.myHeight)
            {
                Put<float>(k.list + 0x89C, k.gameHeight);
                Put<int>(k.list + 0x8A8, s.gameRows);
            }
        if (g_kept[0].list)
        {
            Bar(s, s.gameRows, false);
            if (s.scroll && s.names.count <= s.gameRows) // the game's own rule: no bar for saves that fit its rows
                Put<u8>(s.scroll + 0x6C, static_cast<u8>(Get<u8>(s.scroll + 0x6C, 0) & ~3));
        }
    }
    g_kept[0] = g_kept[1] = {};
    g_last = "given back";
}

std::string Status()
{
    std::ostringstream o;
    o << "skinlists " << g_last << " writes " << g_writes;
    return o.str();
}
} // namespace skinlists
