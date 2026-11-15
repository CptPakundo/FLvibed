// The BEST PRICES page's reads and actions: prices_screen.h. The reads are basepage.cpp's ReadQuotes and what its DrawPrices
// read as it drew, moved unchanged (audit 70b F3a).
#include "prices_screen.h"

#include "parked.h"
#include "game/baseinfo.h"
#include "game/room.h"
#include "ui/winplates.h"

namespace pricesscreen
{
static void ReadRows(Model& m)
{
    std::vector<screens::Quote> rows;
    for (int i = 0; i < 64; ++i)
    {
        screens::Quote q;
        std::wstring price, sells;
        if (!winplates::TextOf(screens::Ctl("CommodityByBasePrice%d", i), price))
            break;
        q.price = screens::Number(price);
        winplates::TextOf(screens::Ctl("CommodityByBuySellPrice%d", i), sells);
        winplates::TextOf(screens::Ctl("CommodityByBaseBase%d", i), q.base);
        winplates::TextOf(screens::Ctl("CommodityByBaseSystem%d", i), q.system);
        q.sells = sells;
        q.buyHere = !sells.empty() && towupper(sells[0]) == L'Y';
        rows.push_back(q);
    }
    screens::SortQuotes(rows);
    m.quotes.swap(rows);
    std::wstring title = screens::TextIn("CommodityByBaseTitle");
    for (wchar_t& c : title)
        c = towupper(c);
    m.title = title;
    m.list.count = static_cast<int>(m.quotes.size());
    m.list.Fit();
}

uint64_t Stamp()
{
    static uint64_t frames = 0;
    return ++frames;
}

bool Read(Model& m, u32)
{
    const u32 now = GetTickCount();
    bool rows = false;
    if (!room::NNWindowBusy() && (m.rowsDue == 0 || static_cast<int>(now - m.rowsDue) >= 0)) // (0: the open's first read, at once)
    {
        m.rowsDue = (now + 200) | 1;
        ReadRows(m);
        rows = true;
    }
    m.docked = baseinfo::Docked();
    m.colBase = screens::TextIn("CommodityByBaseBaseTitle"), m.colSystem = screens::TextIn("CommodityByBaseSystemTitle");
    m.colBuySell = screens::TextIn("CommodityByBaseBuySellTitle"), m.colPrice = screens::TextIn("CommodityByBasePriceTitle");
    m.here = baseinfo::BaseName();
    m.used = m.cap = 0;
    m.cargo = baseinfo::Cargo(m.used, m.cap);
    return rows;
}

// ---- the actions: the window's own buttons
static bool Close(u32, int) { return screens::PressClose(kCloseButton); }

const screens::ActionDesc kActions[] = {
    {kActClose, "close", L"CLOSE", nullptr, nullptr, nullptr, Close, nullptr, screens::Route::Borrowed, screens::Route::Borrowed, nullptr,
     false, screens::Answer::Now, 0, nullptr, nullptr},
};
const int kActionCount = sizeof(kActions) / sizeof(kActions[0]);
} // namespace pricesscreen
