// What the screens read from and press on the game's parked window: parked.h.
#include "parked.h"

#include "screen_rules.h"
#include "game/baseinfo.h"
#include "game/room.h"
#include "safemem.h"
#include "ui/winplates.h"

#include <cstdio>

namespace screens
{
std::wstring TextIn(const char* name)
{
    std::wstring s;
    return winplates::TextOf(uiclick::FindVisible(name), s) ? s : std::wstring();
}

u32 Ctl(const char* format, int i)
{
    char n[48];
    sprintf_s(n, format, i);
    return uiclick::FindVisible(n);
}

bool PressClose(const char* button)
{
    const u32 c = uiclick::FindVisible(button);
    if (c)
        uiclick::Click(c);
    return c != 0;
}

static u32 R32(u32 a)
{
    u32 v = 0;
    safemem::Read(a, &v, 4);
    return v;
}

bool InWindow(u32 ctl, u32 window)
{
    for (int i = 0; ctl && i < 8; ++i, ctl = R32(ctl + 4))
        if (ctl == window)
            return true;
    return false;
}

u32 CtlIn(const char* name, u32 window)
{
    for (int n = 1; n <= 2; ++n)
    {
        const u32 c = baseinfo::Find(name, n);
        if (c && InWindow(c, window))
            return c;
    }
    return 0;
}

void SyncList(u32 window, int first)
{
    const int now = static_cast<int>(R32(window + 0x3C4)); // (the list's first shown row: nnpage_kit.h kListFirst)
    if (first >= 0 && first != now)
        if (const u32 bar = R32(window + 0x3D0))
            uiclick::Scroll(bar, static_cast<float>(first - now), false, nullptr, nullptr);
}
void SyncList(u32 window, const int* gameRow, int n) { SyncList(window, BestWindowStart(gameRow, n)); }
} // namespace screens
