// The dealer page's reads and actions: market_screen.h. The reads are basepage.cpp's ReadRows / ReadTheirs, the dealer's part of
// its Update and what its DrawDealer / DrawList / DrawMiddle / DrawDeal read as they drew; the actions its Trade, Quantity, Mount,
// ToggleTag, Filter, FinalBuy / FinalBack, Select and the dealer's chips, moved unchanged (audit 70b F3b).
#include "market_screen.h"

#include "compare_screen.h"
#include "parked.h"
#include "slot_order.h"
#include "game/baseinfo.h"
#include "game/fl.h"
#include "game/gamedata.h"
#include "game/hudinfo.h"
#include "game/room.h"
#include "flexe.h"
#include "hook.h"
#include "safemem.h"
#include "text.h"
#include "ui/winplates.h"

#include <cwctype>

namespace marketscreen
{
// The windows' own fields (docs\research\neuronet-windows-facts.md, FREELANCER_RE "Equipment dealer")
constexpr u32 kFilter = 0x32C, kRow = 0x3A4, kFirst = 0x3C4, kDesc = 0x4F4, kDealerKind = 0x978;
static const char* const kFilterControl[5] = {"Filter_Weapons", "Filter_Ammo", "Filter_Equipment", "Filter_CargoBay", "Filter_Commodity"};
static std::wstring g_filterWord[5]; // the filters' words, read once each (they stand for the session, as the old page's did)
static u32 g_window[2];              // the two windows as the last read found them: the actions' windows
static int g_soonMs = -1;            // an action asks the rows to be read again this soon (ms), -1 none

static_assert(kDealUnknown == baseinfo::DealUnknown && kDealGood == baseinfo::DealGood && kDealFair == baseinfo::DealFair &&
                  kDealHigh == baseinfo::DealHigh && kDealLow == baseinfo::DealLow,
              "the model's Deal is baseinfo::Deal");

static u32 R32(u32 a)
{
    u32 v = 0;
    safemem::Read(a, &v, 4);
    return v;
}
static float RF(u32 a)
{
    float v = 0;
    safemem::Read(a, &v, 4);
    return v;
}
static void W32(u32 a, u32 v)
{
    for (int i = 0; i < 4; ++i)
        safemem::Write(a + i, static_cast<u8>(v >> (8 * i)));
}
static std::wstring Upper(std::wstring s) // (the pages' kit's: towupper)
{
    for (wchar_t& c : s)
        c = towupper(c);
    return s;
}
static u32 WindowOf(const Model& m, int side) { return side == kYours ? m.inv : m.dealer; }

// A new reading of a list keeps each row's price from the last (issue 93): the same row as it was (its game row, item, id, health, count, mount) keeps it
// fresh; any other row keeps the old number only until it is read again (the window is steered over it: Read).
static void KeepPrices(const std::vector<Row>& old, std::vector<Row>& rows)
{
    for (Row& r : rows)
        for (const Row& o : old)
            if ((o.priceRead || o.priceFresh) && o.row == r.row && o.arch == r.arch && o.id == r.id)
            {
                r.price = o.price, r.priceRead = o.priceRead;
                r.priceFresh = o.priceFresh && o.health == r.health && o.count == r.count && o.mounted == r.mounted;
                break;
            }
}

// ---- the rows: each list's whole length, by the windows' own answers (the game shows five)
// (at the final stage both windows answer so: each holds its own copy of the list, the new ship's left, what will not go on it right; a row of an empty hardpoint answers 0)
static void ReadRows(Model& m, int side)
{
    if (!exe::InvSelected.ok())
        return;
    const u32 w = WindowOf(m, side), row = R32(w + kRow), desc = R32(w + kDesc);
    std::vector<Row> rows;
    int nothing = 0;
    for (int k = 0; k < 96 && nothing < 16; ++k)
    {
        W32(w + kRow, static_cast<u32>(k));
        W32(w + kDesc, 0);
        const u32 d = hk::Call<u32(__fastcall*)(u32)>(exe::InvSelected, w);
        if (!d)
        {
            ++nothing;
            continue;
        }
        nothing = 0;
        Row r;
        r.desc = d, r.row = k;
        safemem::Read(d + 2, &r.id, 2);
        r.arch = R32(d + 4);
        u8 mounted = 0;
        safemem::Read(d + 0xC, &mounted, 1);
        r.mounted = mounted != 0;
        r.health = RF(d + 0x10);
        r.count = static_cast<int>(R32(d + 0x14));
        r.name = hudinfo::EquipName(r.arch);
        if (m.mode == kCommodity)
            r.deal = static_cast<int>(baseinfo::DealFor(r.arch, true));
        if (const equiprules::Spec* s = gamedata::Equip(r.arch))
            r.slot = equiprules::SlotWord(equiprules::SlotOf(s->type));
        if (!r.name.empty())
            rows.push_back(r);
    }
    W32(w + kRow, row);
    W32(w + kDesc, desc);
    m.free.clear();
    const int filter = m.mode == kEquipment ? static_cast<int>(R32(w + kFilter)) : -1;
    if (filter == 0 || filter == 2) // every slot in its own place, EMPTY where nothing is mounted (issue 52)
        rows = screens::InSlotOrder(rows, filter, m.free, [](const Row& r) { return r.mounted ? screens::DescHardpoint(r.desc) : std::string(); },
                                    [&m](int i)
                                    {
                                        Row r;
                                        r.hp = i;
                                        r.name = L"EMPTY";
                                        r.slot = equiprules::SlotWord(equiprules::BestOf(m.free[i]));
                                        return r;
                                    });
    const u16 keep = m.OnRow() && m.side == side ? m.rows[side][m.list[side].sel].id : 0;
    KeepPrices(m.rows[side], rows);
    m.rows[side].swap(rows);
    m.list[side].count = static_cast<int>(m.rows[side].size());
    for (int i = 0; keep && i < m.list[side].count; ++i)
        if (m.rows[side][i].id == keep)
            m.list[side].sel = i;
    m.list[side].Fit();
}

static void ReadTheirs(Model& m)
{
    if (m.Final())
    {
        ReadRows(m, kTheirs);
        return;
    }
    if (!exe::DealerSelectedGood.ok())
        return;
    const u32 w = m.dealer, row = R32(w + kRow);
    std::vector<Row> rows;
    for (u32 k = 0; k < 128; ++k)
    {
        W32(w + kRow, k);
        const u32 good = hk::Call<u32(__thiscall*)(u32)>(exe::DealerSelectedGood, w);
        if (!good)
            break;
        Row r;
        r.arch = good, r.row = static_cast<int>(k);
        r.name = hudinfo::EquipName(good);
        if (m.mode == kCommodity)
            r.deal = static_cast<int>(baseinfo::DealFor(good, false));
        else if (const equiprules::Spec* s = gamedata::Equip(good))
        {
            r.slot = equiprules::SlotWord(equiprules::SlotOf(s->type));
            if (!s->type.empty())
                r.noSlot = !gamedata::Against(good).fits;
        }
        rows.push_back(r);
    }
    W32(w + kRow, row);
    KeepPrices(m.rows[kTheirs], rows);
    m.rows[kTheirs].swap(rows);
    m.list[kTheirs].count = static_cast<int>(m.rows[kTheirs].size());
    m.list[kTheirs].Fit();
}

// The game's selection follows the page's: its transfer panel, its price, its button.
static void SelectFocus(const Model& m)
{
    const Row* r = m.Focus();
    if (!r || r->row < 0 || !exe::InvSelectRow.ok())
        return;
    const u32 w = WindowOf(m, m.side);
    hk::Call<void(__thiscall*)(u32, int, int)>(exe::InvSelectRow, w, r->row - static_cast<int>(R32(w + kFirst)), 1);
}
static bool Selected(const Model& m)
{
    const Row* r = m.Focus();
    return r && r->row >= 0 && static_cast<int>(R32(WindowOf(m, m.side) + kRow)) == r->row;
}

bool Slider() { return uiclick::FindVisible("QuantitySlider") != 0; }
int Qty()
{
    const u32 s = uiclick::FindVisible("QuantitySlider");
    return s ? static_cast<int>(RF(s + 0x3C8) + 0.5f) : 1;
}
int QtyMost()
{
    const u32 s = uiclick::FindVisible("QuantitySlider");
    return s ? static_cast<int>(RF(s + 0x3C4) + 0.5f) - 1 : 1;
}
bool CanTrade(const Model& m) { return Selected(m) && uiclick::FindVisible("BuySellTradeButton") != 0; }
bool CanBuyFinal() { return uiclick::FindVisible("ContinueButton") != 0; }
bool RoutesStand() { return uiclick::FindVisible("TradeRouteButton") != 0; }
bool BackStands() { return uiclick::FindVisible("BackToShipDealer") != 0; }
// a mounted item the store compares with (issue 44): your side of the equipment dealer, one with numbers
bool CanTag(const Model& m)
{
    const Row* r = m.Focus();
    const equiprules::Spec* s = r ? gamedata::Equip(r->arch) : nullptr;
    return m.mode == kEquipment && m.side == kYours && r && r->mounted && r->hp < 0 && s && !s->stats.empty();
}
bool Tagged(const Model& m) { return CanTag(m) && m.Focus()->arch == gamedata::TaggedCompare(); }
int NowFilter(const Model& m) { return static_cast<int>(R32(m.inv + kFilter)); }

// What the dealer asks that the player does not have, at a glance on its row (Shockwave, 7 Nov 2026): the base's market line (gamedata Need) against the player's level
// and standing with the base's owners - the game's refusal says it only for the selected item. "" when nothing is missing or it cannot be known.
// yours, when asked: the player's own value in the need's units ("YOU: LEVEL 4", "YOU: REP 5%"), for the card (issue 56).
static std::wstring NeedWord(u32 arch, std::wstring* yours = nullptr)
{
    const gamedata::BaseFacts& b = gamedata::Base(fl::PlayerBase());
    for (const gamedata::Need& n : b.needs)
        if (n.arch == arch)
        {
            const int level = baseinfo::PlayerLevel();
            if (n.level > 0 && level >= 0 && level < n.level)
            {
                if (yours)
                    *yours = L"YOU: LEVEL " + std::to_wstring(level);
                return L"NEEDS LEVEL " + std::to_wstring(n.level);
            }
            float standing = 0;
            if (n.rep > -1.0f && !b.owner.empty() && baseinfo::Standing(b.owner.c_str(), standing) && standing < n.rep)
            {
                if (yours)
                    *yours = L"YOU: REP " + screens::RepWord(standing);
                return L"NEEDS REP " + screens::RepWord(n.rep);
            }
            return L"";
        }
    return L"";
}

uint64_t Stamp()
{
    static uint64_t frames = 0;
    return ++frames;
}

bool Read(Model& m, u32 root)
{
    if (m.mode == kNone) // (the open's first read: the model was just made; the focus opens as the old page's did)
    {
        const u32 k = R32(root + kDealerKind);
        m.mode = k == 1 ? kCommodity : k == 3 ? kEquipment : k == 2 ? kFinal : kNone;
        m.side = m.Final() ? kYours : kTheirs;
        m.buyStop = m.Final(); // (the focus opens on BUY SHIP, as the pad's way always did: its hold needs a fresh press)
        g_soonMs = -1;
    }
    m.dealer = root, m.inv = uiclick::FindVisible(kPartnerName);
    g_window[kYours] = m.inv, g_window[kTheirs] = m.dealer;
    const u32 now = GetTickCount();
    if (g_soonMs >= 0)
        m.tradeDue = (now + static_cast<u32>(g_soonMs)) | 1, g_soonMs = -1;
    if (m.tradeDue && static_cast<int>(now - m.tradeDue) >= 0) // (the rows move a moment after a trade, a mount, a filter)
        m.rowsDue = 0, m.tradeDue = 0;
    // the rows wait out the slide, then read at once: a read spent during the slide left the page without rows for up to 0.75 s over the game's window
    // (the owner: "frame-perfect", 7 Nov 2026)
    const bool busy = room::NNWindowBusy();
    if (m.wasBusy && !busy)
        m.rowsDue = 0;
    m.wasBusy = busy;
    bool read = false;
    if (!busy && (m.rowsDue == 0 || static_cast<int>(now - m.rowsDue) >= 0)) // (0: at once)
    {
        m.rowsDue = (now + 750) | 1;
        for (int i = 0; i < 5; ++i)
            if (g_filterWord[i].empty())
            {
                // the game's words, the last one off where the four must stand in one list's width ("EXTERNAL EQUIPMENT")
                static const wchar_t* const kWords[] = {L"WEAPONS", L"AMMO", L"EXTERNAL", L"INTERNAL", L"GOODS"};
                g_filterWord[i] = Upper(baseinfo::HintText(uiclick::Find(kFilterControl[i])));
                const size_t sp = g_filterWord[i].find(L' ');
                if (sp != std::wstring::npos)
                    g_filterWord[i].resize(sp);
                // a long word: the short one where the game's is that word in English (AMMUNITION -> AMMO: "AMMU." read as cut, Shockwave), else its first four and a stop
                if (g_filterWord[i].size() > 8)
                {
                    if (g_filterWord[i].compare(0, 3, kWords[i], 3) == 0)
                        g_filterWord[i] = kWords[i];
                    else
                        g_filterWord[i].resize(4), g_filterWord[i] += L".";
                }
                if (g_filterWord[i].empty())
                    g_filterWord[i] = kWords[i];
            }
        if (!room::NNWindowBusy())
        {
            ReadRows(m, kYours);
            ReadTheirs(m);
            read = true;
            if (m.buyStop && !CanBuyFinal())
                m.buyStop = false;
            if (m.OnRow() && !Selected(m) && m.Focus()->row >= 0) // (a sale clears the game's selection a moment later: the page's row stands)
                SelectFocus(m);
        }
    }
    // the game's lists stand where the page's do: their five icons are the page's (the runtime's RowIcons lends one to a shown row outside them); while a
    // shown row's price is not read yet, the window starts at that row instead until ReadFrame has read it (issue 93: its ItemPriceNN is the only source)
    for (int side = 0; side < 2 && !room::NNWindowBusy(); ++side)
    {
        int shown[screens::kMeshes], n = 0, unread = -1;
        for (; n < screens::kMeshes && m.list[side].first + n < static_cast<int>(m.rows[side].size()); ++n)
        {
            const Row& r = m.rows[side][m.list[side].first + n];
            shown[n] = r.row;
            if (unread < 0 && r.row >= 0 && !r.priceFresh)
                unread = r.row;
        }
        const int best = screens::BestWindowStart(shown, n);
        const bool covered = unread < 0 || (best >= 0 && unread >= best && unread < best + screens::kMeshes);
        screens::SyncList(WindowOf(m, side), covered ? best : unread);
    }
    return read;
}

void ReadFrame(Model& m)
{
    for (int i = 0; i < 5; ++i)
        m.filterWord[i] = g_filterWord[i];
    m.used = m.cap = 0;
    m.cargoRead = baseinfo::Cargo(m.used, m.cap);
    m.filterNow = NowFilter(m);
    m.guns = 0, m.hull = m.shield = m.energy = 0;
    if (m.mode == kEquipment)
        for (const baseinfo::Item& it : baseinfo::Equipment())
        {
            baseinfo::Gun g;
            if (it.mounted && baseinfo::GunOf(it.arch, g) && g.hullDps >= 0)
                ++m.guns, m.hull += g.hullDps, m.shield += std::max(0.0f, g.shieldDps), m.energy += std::max(0.0f, g.energyPs);
        }
    m.tagged = gamedata::TaggedCompare();
    // what the mounted launchers fire: their ammo is tagged on both sides (Shockwave, 7 Nov 2026)
    u32 fired[16];
    int nFired = 0;
    for (const baseinfo::Item& e : baseinfo::Equipment())
        if (e.mounted && nFired < 16)
            if (const u32 a = gamedata::Fires(e.arch))
                fired[nFired++] = a;
    for (int side = 0; side < 2; ++side)
    {
        const u32 w = WindowOf(m, side);
        const int nativeFirst = static_cast<int>(R32(w + kFirst)); // (the game's own row of five: its prices)
        const bool settled = nativeFirst == m.priceFirst[side];   // (a window just moved still shows its old rows' numbers until the game's update)
        m.priceFirst[side] = nativeFirst;
        for (Row& r : m.rows[side])
        {
            r.forMounted = false;
            for (int k = 0; k < nFired && !r.forMounted; ++k)
                r.forMounted = fired[k] == r.arch;
            const int mesh = r.row - nativeFirst;
            if (settled && mesh >= 0 && mesh < 5) // (the game's own number: a launcher's price, a damaged item's, a sale's; off its five the last one read)
            {
                char name[24];
                sprintf_s(name, "ItemPrice%02d", mesh + 1);
                r.priceRead = winplates::TextOf(screens::CtlIn(name, w), r.price); // ("" read: the game shows no number on that row)
                r.priceFresh = true;
            }
            r.need = side == kTheirs && r.hp < 0 && !m.Final() ? NeedWord(r.arch) : std::wstring();
        }
    }
    // the focus's column
    const Row* r = m.Focus();
    m.transferMesh = baseinfo::Find("TransferItemMesh", 1) != 0;
    m.selected = Selected(m), m.canTrade = CanTrade(m);
    m.priceText.clear(), m.totalText.clear(), m.errorHelp.clear(), m.need.clear(), m.yours.clear();
    m.slider = false, m.qty = 1, m.most = 1;
    m.canTag = CanTag(m), m.tagOn = Tagged(m);
    m.compare = false, m.block = screens::CompareBlock(), m.volume = m.basePrice = -1;
    m.fits.clear();
    if (r && r->hp >= 0)
    {
        if (r->hp < static_cast<int>(m.free.size()))
            for (const u32 a : gamedata::HoldItemsFor(m.free[r->hp]))
                m.fits.push_back(hudinfo::EquipName(a));
    }
    else if (r)
    {
        if (m.canTrade)
        {
            m.priceText = screens::TextIn("TransferPriceText"), m.totalText = screens::TextIn("QuantityTotalText");
            m.qty = Qty(), m.most = QtyMost(), m.slider = Slider();
        }
        else
        {
            m.errorHelp = screens::TextIn("ErrorHelp");
            // the need against the player's own value, under the game's refusal: that names the need at most (or another reason, a full ship), never the player's own value (issue 56)
            if (m.side != kYours && !m.Final() && m.selected)
                m.need = NeedWord(r->arch, &m.yours);
        }
        m.compare = m.mode == kEquipment || (m.Final() && gamedata::Equip(r->arch));
        if (m.compare)
            m.block = screens::ReadCompare(r->arch, r->mounted);
        else
            m.volume = baseinfo::VolumeOf(r->arch), m.basePrice = baseinfo::BasePrice(r->arch);
        // the card: a new item's from its top
        if (r->arch != m.cardKey)
        {
            m.cardKey = r->arch, m.cardFirst = 0;
            m.cardText = text::InfoCard(gamedata::EquipInfo(r->arch));
        }
    }
    // the final stage's deal (the game's SHIP PRICE already stands less what went to the dealer's side: read in game, 17,020 became 16,900 at a sale of 120)
    m.shipNet = m.Final() ? screens::Number(screens::TextIn("ShipDealerShipPrice")) : 0;
    m.tradeIn = m.Final() ? static_cast<int>(R32(m.dealer + 0x9FC)) : 0;
    m.canBuyFinal = CanBuyFinal();
    m.why = m.Final() && !m.canBuyFinal ? screens::TextIn("ShipBuyProblem") : std::wstring();
    m.back = BackStands(), m.routes = RoutesStand();
}

void Filtered(Model& m)
{
    m.list[0].sel = m.list[0].first = m.list[1].sel = m.list[1].first = 0;
    m.cardFirst = 0;
    g_soonMs = 100;
}

const hk::Sym* const kWindowClass = &exe::VtDealer;
const hk::Sym* const kPartnerClass = &exe::VtInventory;

// ---- the actions: the windows' own buttons and handlers (an arg of side * 1000 + row names a list's game row)
static bool Select(u32, int arg)
{
    const int side = arg / 1000, row = arg % 1000;
    const u32 w = arg >= 0 && side < 2 ? g_window[side] : 0;
    if (!w || !exe::InvSelectRow.ok())
        return false;
    hk::Call<void(__thiscall*)(u32, int, int)>(exe::InvSelectRow, w, row - static_cast<int>(R32(w + kFirst)), 1);
    return true;
}
static bool Trade(u32, int arg)
{
    const int side = arg / 1000, row = arg % 1000;
    const u32 w = arg >= 0 && side < 2 ? g_window[side] : 0;
    const bool can = w && static_cast<int>(R32(w + kRow)) == row && uiclick::FindVisible("BuySellTradeButton") != 0; // (CanTrade)
    if (!can || room::NNWindowBusy())
        return false;
    uiclick::Click(uiclick::FindVisible("BuySellTradeButton"));
    g_soonMs = 350; // (the rows move a moment later)
    return true;
}
static bool Quantity(int by)
{
    const u32 s = uiclick::FindVisible("QuantitySlider");
    if (s)
        uiclick::Scroll(s, static_cast<float>(by), false, nullptr, nullptr);
    return s != 0;
}
static bool Less(u32, int) { return Quantity(-1); }
static bool More(u32, int) { return Quantity(1); }
static bool Max(u32, int) { return Quantity(1000); }
static bool MountAs(int desc, int unmount)
{
    if (!desc || !exe::InvMount.ok())
        return false;
    hk::Call<void(__thiscall*)(u32, u32, int)>(exe::InvMount, g_window[kYours], static_cast<u32>(desc), unmount);
    g_soonMs = 150;
    return true;
}
static bool Mount(u32, int desc) { return MountAs(desc, 0); }
static bool Unmount(u32, int desc) { return MountAs(desc, 1); }
static bool Tag(u32, int arch)
{
    gamedata::TagCompare(gamedata::TaggedCompare() == static_cast<u32>(arch) ? 0 : static_cast<u32>(arch));
    return true;
}
static bool Filter(u32, int i)
{
    const u32 c = i >= 0 && i < 5 ? uiclick::FindVisible(kFilterControl[i]) : 0;
    if (c)
        uiclick::Click(c);
    return c != 0;
}
static bool Routes(u32, int)
{
    const u32 c = uiclick::FindVisible("TradeRouteButton");
    if (c)
        uiclick::Click(c);
    return c != 0;
}
static bool FinalBuy(u32, int)
{
    if (!CanBuyFinal() || room::NNWindowBusy())
        return false;
    uiclick::Click(uiclick::FindVisible("ContinueButton"));
    return true;
}
static bool FinalBack(u32, int)
{
    const u32 c = uiclick::FindVisible("BackToShipDealer");
    if (!c || room::NNWindowBusy())
        return false;
    uiclick::Click(c);
    return true;
}
static bool Close(u32, int) { return screens::PressClose(kCloseButton); }

#define MARKET_ACTION(id, name, word, fn)                                                                                                       \
    {id, name, word, nullptr, nullptr, nullptr, fn, nullptr, screens::Route::Borrowed, screens::Route::Borrowed, nullptr, false, screens::Answer::Now, 0, \
     nullptr, nullptr}
const screens::ActionDesc kActions[] = {
    MARKET_ACTION(kActSelect, "select", L"SELECT", Select),
    MARKET_ACTION(kActTrade, "trade", L"BUY / SELL", Trade),
    MARKET_ACTION(kActLess, "less", L"-", Less),
    MARKET_ACTION(kActMore, "more", L"+", More),
    MARKET_ACTION(kActMax, "max", L"MAX", Max),
    MARKET_ACTION(kActMount, "mount", L"MOUNT", Mount),
    MARKET_ACTION(kActUnmount, "unmount", L"UNMOUNT", Unmount),
    MARKET_ACTION(kActTag, "tag", L"COMPARE WITH", Tag),
    MARKET_ACTION(kActFilter, "filter", L"FILTER", Filter),
    MARKET_ACTION(kActRoutes, "routes", L"BEST PRICES", Routes),
    MARKET_ACTION(kActFinalBuy, "buyship", L"BUY SHIP", FinalBuy),
    MARKET_ACTION(kActFinalBack, "back", L"BACK TO SHIPS", FinalBack),
    MARKET_ACTION(kActClose, "close", L"CLOSE", Close),
};
#undef MARKET_ACTION
const int kActionCount = sizeof(kActions) / sizeof(kActions[0]);
} // namespace marketscreen
