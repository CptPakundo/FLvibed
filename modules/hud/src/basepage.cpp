// The docked windows as pages: basepage.h. The plan and the facts: docs\BASE_ROOMS_PLAN.md.
#include "basepage.h"
#include "uistrip.h"
#include "nnstrip.h"

#include "game/baseinfo.h"
#include "game/hostcalls.h"
#include "game/room.h"
#include "hudhost.h"
#include "nnpage_kit.h"
#include "nnpage_rules.h"
#include "text.h"
#include "ui/hud2_kit.h"
#include "ui/hud_rows.h"

#include <sstream>
#include <unordered_map>

namespace basepage
{
using namespace nnkit;
using nnrules::List;

enum class Kind
{
    None,
    Commodity,
    Equipment,
    Ships,
    Jobs,
    News,
    Repair,
    Final,  // the ship dealer's final stage: NN_Dealer in its mode 2
    Prices, // the game's commodity list by base
};
static Kind g_kind = Kind::None;
static u32 g_dealer = 0, g_inv = 0;
static bool g_pad = false;
static float g_read = 0, g_tradeWait = 0;

// The windows' own fields (docs\research\neuronet-windows-facts.md, FREELANCER_RE "Equipment dealer")
constexpr u32 kFilter = 0x32C, kRow = 0x3A4, kFirst = 0x3C4, kDesc = 0x4F4, kDealerKind = 0x978;
constexpr float kHoldSec = 0.6f; // the pad's hold on a purchase (controller's dealer_rules.h): a tap on a row never spends

enum Side
{
    Yours,
    Theirs
};
struct Row
{
    u32 arch = 0, desc = 0;
    int row = -1, count = 1, hp = -1;
    u16 id = 0;
    bool mounted = false;
    float health = 1;
    baseinfo::Deal deal = baseinfo::DealUnknown;
    std::wstring name, slot;
    bool noSlot = false;
};
static std::vector<Row> g_rows[2];
static List g_list[2];
static int g_side = Theirs;
static std::vector<equiprules::Hp> g_free;
static std::wstring g_filterWord[5];
static const char* const kFilterControl[5] = {"Filter_Weapons", "Filter_Ammo", "Filter_Equipment", "Filter_CargoBay", "Filter_Commodity"};
static bool Final() { return g_kind == Kind::Final; }
static int Filters() { return Final() ? 5 : 4; } // (the final stage has the inventory's five, the equipment dealer's window four)
static bool g_buyStop = false; // the final stage's middle stop: the focus on BUY SHIP

// The card: the game's description, wrapped; RS and the wheel move it.
static u32 g_cardArch = 0;
static std::wstring g_cardText;
static int g_cardFirst = 0, g_cardLines = 0, g_cardRoom = 0;
static float g_cardStep = 0;

static bool OnRow() { return g_list[g_side].count > 0 && g_list[g_side].sel < static_cast<int>(g_rows[g_side].size()); }
static const Row* Focus() { return OnRow() ? &g_rows[g_side][g_list[g_side].sel] : nullptr; }
static u32 WindowOf(int side) { return side == Yours ? g_inv : g_dealer; }

static std::wstring TextIn(const char* name)
{
    std::wstring s;
    return winplates::TextOf(uiclick::FindVisible(name), s) ? s : std::wstring();
}

static bool g_pagesOn = true; // base_pages and the skin as they stood when no page was up

// The game's window stands off the screen under the page: no hidden button under a click, no tooltip, nothing to move out of the way. Its calls do not care where it stands.
static void Away(u32 root)
{
    ui::Item it;
    float pos[3] = {};
    if (!root || !ui::ControlRect(root, it) || it.x > 1.5f * winplates::g_W || !safemem::Read(root + 0x60, pos, sizeof(pos)))
        return;
    const u32 set = R32(R32(root) + 0x14);
    if (!exe::ControlSetPosition.is(set) && !exe::InventorySetPosition.is(set))
        return;
    pos[0] += 4;
    hk::Call<void(__thiscall*)(u32, const float*)>(exe::ControlSetPosition.is(set) ? exe::ControlSetPosition : exe::InventorySetPosition, root, pos);
}


// ---- the rows: each list's whole length, by the windows' own answers (the game shows five)
// (at the final stage both windows answer so: each holds its own copy of the list, the new ship's left, what will not go on it right; a row of an empty hardpoint answers 0)
static void ReadRows(int side)
{
    if (!exe::InvSelected.ok())
        return;
    const u32 w = WindowOf(side), row = R32(w + kRow), desc = R32(w + kDesc);
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
        u8 m = 0;
        safemem::Read(d + 0xC, &m, 1);
        r.mounted = m != 0;
        r.health = RF(d + 0x10);
        r.count = static_cast<int>(R32(d + 0x14));
        r.name = hudinfo::EquipName(r.arch);
        if (g_kind == Kind::Commodity)
            r.deal = baseinfo::DealFor(r.arch, true);
        if (const equiprules::Spec* s = gamedata::Equip(r.arch))
            r.slot = equiprules::SlotWord(equiprules::SlotOf(s->type));
        if (!r.name.empty())
            rows.push_back(r);
    }
    W32(w + kRow, row);
    W32(w + kDesc, desc);
    g_free.clear();
    if (g_kind == Kind::Equipment)
    {
        const int filter = static_cast<int>(R32(w + kFilter));
        if (filter == 0 || filter == 2)
            g_free = gamedata::FreeHardpoints(filter);
    }
    for (size_t i = 0; i < g_free.size(); ++i)
    {
        Row r;
        r.hp = static_cast<int>(i);
        r.name = L"EMPTY";
        r.slot = equiprules::SlotWord(equiprules::BestOf(g_free[i]));
        rows.push_back(r);
    }
    const u16 keep = OnRow() && g_side == side ? g_rows[side][g_list[side].sel].id : 0;
    g_rows[side].swap(rows);
    g_list[side].count = static_cast<int>(g_rows[side].size());
    for (int i = 0; keep && i < g_list[side].count; ++i)
        if (g_rows[side][i].id == keep)
            g_list[side].sel = i;
    g_list[side].Fit();
}

static void ReadTheirs()
{
    if (Final())
    {
        ReadRows(Theirs);
        return;
    }
    if (!exe::DealerSelectedGood.ok())
        return;
    const u32 w = g_dealer, row = R32(w + kRow);
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
        if (g_kind == Kind::Commodity)
            r.deal = baseinfo::DealFor(good, false);
        else if (const equiprules::Spec* s = gamedata::Equip(good))
        {
            r.slot = equiprules::SlotWord(equiprules::SlotOf(s->type));
            if (!s->type.empty())
                r.noSlot = !gamedata::Against(good).fits;
        }
        rows.push_back(r);
    }
    W32(w + kRow, row);
    g_rows[Theirs].swap(rows);
    g_list[Theirs].count = static_cast<int>(g_rows[Theirs].size());
    g_list[Theirs].Fit();
}

// The game's selection follows the page's: its transfer panel, its price, its button.
static void Select()
{
    const Row* r = Focus();
    if (!r || r->row < 0 || !exe::InvSelectRow.ok())
        return;
    const u32 w = WindowOf(g_side);
    hk::Call<void(__thiscall*)(u32, int, int)>(exe::InvSelectRow, w, r->row - static_cast<int>(R32(w + kFirst)), 1);
}
static bool Selected()
{
    const Row* r = Focus();
    return r && r->row >= 0 && static_cast<int>(R32(WindowOf(g_side) + kRow)) == r->row;
}
static void Moved()
{
    g_cardFirst = 0;
    Select();
}

static u32 Slider() { return uiclick::FindVisible("QuantitySlider"); }
static int Qty()
{
    const u32 s = Slider();
    return s ? static_cast<int>(RF(s + 0x3C8) + 0.5f) : 1;
}
static int QtyMost()
{
    const u32 s = Slider();
    return s ? static_cast<int>(RF(s + 0x3C4) + 0.5f) - 1 : 1;
}
static void Quantity(int by)
{
    if (const u32 s = Slider())
        uiclick::Scroll(s, static_cast<float>(by), false, nullptr, nullptr);
}
static bool CanTrade() { return Selected() && uiclick::FindVisible("BuySellTradeButton") != 0; }
static void Trade()
{
    if (!CanTrade() || room::NNWindowBusy())
        return;
    uiclick::Click(uiclick::FindVisible("BuySellTradeButton"));
    g_tradeWait = 0.35f; // (the rows move a moment later)
}
// a mounted item the store compares with (issue 44): your side of the equipment dealer, one with numbers
static bool CanTag()
{
    const Row* r = Focus();
    const equiprules::Spec* s = r ? gamedata::Equip(r->arch) : nullptr;
    return g_kind == Kind::Equipment && g_side == Yours && r && r->mounted && r->hp < 0 && s && !s->stats.empty();
}
static bool Tagged() { return CanTag() && Focus()->arch == gamedata::TaggedCompare(); }
static void ToggleTag()
{
    if (CanTag())
        gamedata::TagCompare(Tagged() ? 0 : Focus()->arch);
}
static bool Mountable(const Row* r) { return g_kind == Kind::Equipment && g_side == Yours && r && r->hp < 0 && !r->slot.empty(); }
static void Mount()
{
    const Row* r = Focus();
    if (!Mountable(r) || !exe::InvMount.ok())
        return;
    hk::Call<void(__thiscall*)(u32, u32, int)>(exe::InvMount, g_inv, r->desc, r->mounted ? 1 : 0);
    g_tradeWait = 0.15f;
}
static void ShipPick(int i);
static void ShipBuy();
static void BoardPick(int i);
static void BoardAccept();
static void MendPick(int i);
static void MendAll();
static void MendGo();
static bool CanBuyFinal() { return uiclick::FindVisible("ContinueButton") != 0; }
static void FinalBuy()
{
    if (CanBuyFinal() && !room::NNWindowBusy())
        uiclick::Click(uiclick::FindVisible("ContinueButton"));
}
static void FinalBack()
{
    if (const u32 c = uiclick::FindVisible("BackToShipDealer"))
        if (!room::NNWindowBusy())
            uiclick::Click(c);
}
static void Filter(int i)
{
    if (const u32 c = uiclick::FindVisible(kFilterControl[i]))
        uiclick::Click(c);
    g_list[0].sel = g_list[0].first = g_list[1].sel = g_list[1].first = 0;
    g_tradeWait = 0.1f, g_cardFirst = 0;
}
static int NowFilter() { return static_cast<int>(R32(g_inv + kFilter)); }

// ---- the clicks
enum Chip
{
    kBuy,
    kMax,
    kLess,
    kMore,
    kMount,
    kTag,
    kClose,
    kShipBuy,
    kAccept,
    kFinalBuy,
    kFinalBack,
    kMendAll,
    kMendGo,
    kRoutes,
};
static void RowClick(int arg)
{
    if (g_kind == Kind::Repair)
    {
        MendPick(arg);
        return;
    }
    if (g_kind == Kind::Ships)
    {
        ShipPick(arg);
        return;
    }
    if (g_kind == Kind::Jobs || g_kind == Kind::News)
    {
        BoardPick(arg);
        return;
    }
    g_side = arg >= 1000 ? Theirs : Yours;
    g_list[g_side].sel = arg % 1000;
    g_list[g_side].Fit();
    Moved();
}
static void ChipClick(int chip)
{
    switch (chip)
    {
    case kBuy: Trade(); break;
    case kMax: Quantity(1000); break;
    case kLess: Quantity(-1); break;
    case kMore: Quantity(1); break;
    case kMount: Mount(); break;
    case kTag: ToggleTag(); break;
    case kClose:
        if (const u32 c = uiclick::FindVisible(g_kind == Kind::Ships ? "ShipTraderCancelButton" : g_kind == Kind::Jobs ? "MissionVendorCloseButton" : g_kind == Kind::News ? "NewsCloseButton" : g_kind == Kind::Repair ? "RepairCancelButton" : g_kind == Kind::Prices ? "CommodityByBaseCloseButton" : "Inventory_Close"))
            uiclick::Click(c);
        break;
    case kShipBuy: ShipBuy(); break;
    case kAccept: BoardAccept(); break;
    case kFinalBuy: FinalBuy(); break;
    case kFinalBack: FinalBack(); break;
    case kMendAll: MendAll(); break;
    case kMendGo: MendGo(); break;
    case kRoutes:
        if (const u32 c = uiclick::FindVisible("TradeRouteButton"))
            uiclick::Click(c);
        break;
    default: break;
    }
}
static void TabClick(int i) { Filter(i); }
static void Nothing(int) {}

// ---- the page
static Holes g_holes, g_nextHoles; // this frame's and the last's: the ground goes first, the rows say where
static float g_wheelSide = -1; // the list the mouse stands on, -1 the card
// What the dealer asks that the player does not have, at a glance on its row (Shockwave, 7 Nov 2026): the base's market line (gamedata Need) against the player's level
// and standing with the base's owners - the game's refusal says it only for the selected item. "" when nothing is missing or it cannot be known.
static std::wstring NeedWord(u32 arch)
{
    const gamedata::BaseFacts& b = gamedata::Base(fl::PlayerBase());
    for (const gamedata::Need& n : b.needs)
        if (n.arch == arch)
        {
            const int level = baseinfo::PlayerLevel();
            if (n.level > 0 && level >= 0 && level < n.level)
                return L"NEEDS LEVEL " + std::to_wstring(n.level);
            float standing = 0;
            if (n.rep > -1.0f && !b.owner.empty() && baseinfo::Standing(b.owner.c_str(), standing) && standing < n.rep)
                return L"NEEDS REP " + std::to_wstring(static_cast<int>(n.rep * 100 + (n.rep >= 0 ? 0.5f : -0.5f))) + L"%";
            return L"";
        }
    return L"";
}

static const wchar_t* DealWord(baseinfo::Deal d, bool selling)
{
    switch (d)
    {
    case baseinfo::DealGood: return selling ? L"GOOD OFFER" : L"GOOD PRICE";
    case baseinfo::DealFair: return L"FAIR";
    case baseinfo::DealHigh: return L"HIGH PRICE";
    case baseinfo::DealLow: return L"LOW OFFER";
    default: return L"";
    }
}
static u32 DealRgb(baseinfo::Deal d) { return d == baseinfo::DealGood ? kFriend : d == baseinfo::DealFair ? kAmber : kHostile; }

// The nth of a row's mesh: the dealer's list stands right of the player's on the screen, where both stand off it.
// (a control's window by its parents, +4: at the final stage the two lists' controls do not stand in one order, and a list with no rows has no place to compare)
static bool Under(u32 ctl, u32 window)
{
    for (int i = 0; ctl && i < 8; ++i, ctl = R32(ctl + 4))
        if (ctl == window)
            return true;
    return false;
}
static u32 OnSide(const char* name, int side)
{
    for (int n = 1; n <= 2; ++n)
    {
        const u32 c = baseinfo::Find(name, n);
        if (c && Under(c, WindowOf(side)))
            return c;
    }
    return 0;
}
static int MeshNth(int side)
{
    for (int n = 1; n <= 2; ++n)
        if (Under(baseinfo::Find("ItemMesh1", n), WindowOf(side)))
            return n;
    for (int n = 1; n <= 2; ++n)
        if (Under(baseinfo::Find("ItemMesh1", n), WindowOf(1 - side)))
            return 0; // (the list has no rows: what is there is the other's, not to be moved away)
    ui::Item a, b;
    const u32 one = baseinfo::Find("ItemMesh1", 1), two = baseinfo::Find("ItemMesh1", 2);
    if (!one || !two || !ui::ControlRect(one, a) || !ui::ControlRect(two, b))
        return 1;
    const int theirs = a.x > b.x ? 1 : 2;
    return side == Theirs ? theirs : 3 - theirs;
}
static void DrawList(const Frame& f, int side, float x0, float x1, float y, float foot)
{
    const float H = f.H;
    List& l = g_list[side];
    const std::vector<Row>& rows = g_rows[side];
    // five rows, as the game's: the game's own icons stand in them
    const float pitch = std::min(0.124f * H, (foot - y) / 5), cell = std::min(0.092f * H, pitch);
    l.rows = 5;
    l.Fit();
    const int hover = skinchip::Hovered();
    Icon icons[5];
    const int nativeFirst = static_cast<int>(R32(WindowOf(side) + kFirst));
    // what the mounted launchers fire: their ammo is tagged on both sides (Shockwave, 7 Nov 2026)
    u32 fired[16];
    int nFired = 0;
    for (const baseinfo::Item& e : baseinfo::Equipment())
        if (e.mounted && nFired < 16)
            if (const u32 a = gamedata::Fires(e.arch))
                fired[nFired++] = a;
    auto forMounted = [&](u32 arch) {
        for (int k = 0; k < nFired; ++k)
            if (fired[k] == arch)
                return true;
        return false;
    };
    if (rows.empty())
        Text(x0, y + pitch / 2, Final() ? (side == Yours ? L"NOTHING OF THIS KIND" : L"ALL OF IT GOES ON THE NEW SHIP") : side == Yours ? L"NOTHING TO SELL HERE" : L"NOTHING ON SALE HERE", 0.028f * H, Argb(kDim, 1));
    for (int i = 0; i < l.rows && l.first + i < l.count; ++i)
    {
        const int at = l.first + i;
        const Row& r = rows[at];
        const float top = y + i * pitch, y1 = top + pitch * 0.36f, y2 = top + pitch * 0.76f;
        const bool on = at == l.sel && side == g_side, chosen = at == l.sel;
        const int slot = skinchip::Hot({x0, top, x1, top + pitch}, RowClick, side * 1000 + at);
        const int mesh = r.row - nativeFirst;
        if (r.row >= 0 && mesh >= 0 && mesh < 5)
        {
            icons[mesh] = {x0 + cell / 2, top + pitch / 2, true};
            g_nextHoles.Add(x0 + 1, top + (pitch - cell) / 2, x0 + cell - 1, top + (pitch + cell) / 2);
        }
        if (on)
            skinkit::Focus(x0 + cell, top + 0.002f * H, x1, top + pitch - 0.002f * H);
        else if (slot >= 0 && slot == hover)
            Rect(x0 + cell, top, x1 - x0 - cell, pitch, Argb(kChrome, 0.10f));
        else if (chosen)
            Rect(x0 + cell, top, x1 - x0 - cell, pitch, Argb(kChrome, 0.06f));
        if (slot >= 0 && slot == hover)
            g_wheelSide = static_cast<float>(side);
        const float pad = 0.012f * H;
        std::wstring right, under;
        u32 underRgb = kChrome;
        const bool tagged = side == Yours && r.hp < 0 && r.mounted && r.arch == gamedata::TaggedCompare();
        const std::wstring slotWord = tagged ? std::wstring(L"TAGGED TO COMPARE") : r.slot; // (Shockwave, 7 Nov: in the class's place, clear of the row's word)
        if (r.hp >= 0)
            right = r.slot;
        else if (mesh >= 0 && mesh < 5) // (the game's own number: a launcher's price, a damaged item's, a sale's)
        {
            char name[24];
            sprintf_s(name, "ItemPrice%02d", mesh + 1);
            winplates::TextOf(OnSide(name, side), right);
        }
        if (r.hp < 0)
        {
            if (side == Yours)
            {
                under = r.mounted ? L"MOUNTED" : r.count > 1 ? L"x" + std::to_wstring(r.count) : L"IN HOLD";
                if (r.mounted)
                    underRgb = kFriend; // (green, as the inventory's)
            }
            else
                under = r.slot;
            if (r.noSlot)
                under = L"NO HARDPOINT TAKES IT", underRgb = kAmber;
            if (r.deal != baseinfo::DealUnknown)
                under = (side == Yours ? L"x" + std::to_wstring(r.count) + L"   " : L"") + DealWord(r.deal, side == Yours), underRgb = DealRgb(r.deal);
        }
        const float rw = TextWidth(right, 0.028f * H), tx = x0 + cell + pad;
        TextMarquee(tx, y1, r.name, 0.032f * H, x1 - tx - rw - 2 * pad, Argb(r.hp >= 0 ? kDim : kInk, 1));
        Text(x1 - pad, y1, right, r.hp >= 0 ? 0.024f * H : 0.028f * H, Argb(r.hp >= 0 ? kChrome : on ? kInk : kActive, 1), Right);
        if (!under.empty())
            Text(tx, y2, under, 0.024f * H, Argb(on && underRgb == kChrome ? kInk : underRgb, 0.9f));
        if (r.hp < 0 && !slotWord.empty() && side == Yours)
            Text(x1 - pad, y2, slotWord, 0.022f * H, Argb(on ? kInk : tagged ? kActive : kChrome, tagged ? 1.0f : 0.8f), Right);
        if (r.hp < 0 && !Final())
        {
            const std::wstring need = side == Theirs ? NeedWord(r.arch) : std::wstring();
            if (!need.empty())
                Text(x1 - pad, y2, need, 0.022f * H, Argb(kAmber, 1), Right);
            else if (!r.mounted && forMounted(r.arch) && (side == Theirs || r.slot.empty()))
                Text(x1 - pad, y2, L"FOR MOUNTED", 0.022f * H, Argb(on ? kInk : kFriend, 1), Right); // (ammo for a launcher you carry)
        }
        if (r.mounted && r.health > 0 && r.health < 0.995f) // (beside the row's word, clear of the slot's)
        {
            const float px = tx + TextWidth(under, 0.024f * H) + pad, pe = x1 - pad - (side == Yours ? TextWidth(slotWord, 0.022f * H) + pad : 0);
            if (pe - px > 0.03f * H)
                skinkit::Pips(px, y2 - 0.003f * H, std::min(0.13f * H, pe - px), 0.006f * H, r.health, 10, r.health < 0.35f ? kHostile : kAmber);
        }
    }
    if (l.count > l.rows)
        skinkit::ScrollBar(x1 + 0.006f * H, y, y + l.rows * pitch, static_cast<float>(l.first) / l.count, static_cast<float>(l.rows) / l.count);
    if (const int nth = MeshNth(side))
        PlaceIcons("ItemMesh%d", 1, 5, nth, icons);
}

static void Heading(float x0, float x1, float y, float H, const std::wstring& word, const std::wstring& tag)
{
    Text(x0, y, word, 0.030f * H, Argb(kChrome, 1));
    Text(x1, y, tag, 0.026f * H, Argb(kDim, 1), Right);
    skinkit::Rule(x0, x1, y + 0.024f * H);
}

// The game's description, as many lines as stand in the room, from where it was scrolled to.
static float CardOf(float x0, float x1, float y, float foot, float H, u32 key, u32 info)
{
    if (key != g_cardArch)
    {
        g_cardArch = key, g_cardFirst = 0;
        g_cardText = text::InfoCard(info);
    }
    if (g_cardText.empty())
        return y;
    const float size = 0.025f * H, pitch = 0.031f * H;
    const std::vector<std::wstring> lines = Wrap(g_cardText, size, x1 - x0 - 0.016f * H);
    g_cardLines = static_cast<int>(lines.size());
    g_cardRoom = std::max(1, static_cast<int>((foot - y) / pitch));
    g_cardFirst = std::max(0, std::min(g_cardFirst, g_cardLines - g_cardRoom));
    for (int i = 0; i < g_cardRoom && g_cardFirst + i < g_cardLines; ++i)
        Text(x0, y + pitch * (i + 0.5f), lines[g_cardFirst + i], size, Argb(kInk, 0.9f));
    if (g_cardLines > g_cardRoom)
        skinkit::ScrollBar(x1 - 0.004f * H, y, foot, static_cast<float>(g_cardFirst) / g_cardLines, static_cast<float>(g_cardRoom) / g_cardLines);
    return foot;
}
static float Card(float x0, float x1, float y, float foot, float H, u32 arch) { return CardOf(x0, x1, y, foot, H, arch, gamedata::EquipInfo(arch)); }

// The final stage's middle head: the game's price, what the old ship brings, what the rows at the dealer's bring, what is left to pay; BUY SHIP, or the game's own refusal. The y after it.
static float DrawDeal(float x0, float x1, float y, float H)
{
    const float rowH = 0.034f * H;
    // (the game's SHIP PRICE already stands less what went to the dealer's side: read in game, 17,020 became 16,900 at a sale of 120)
    const long long net = nnrules::Number(TextIn("ShipDealerShipPrice")), tradeIn = static_cast<int>(R32(g_dealer + 0x9FC));
    long long sold = 0;
    bool whole = g_rows[Theirs].size() <= 5;
    const int first = static_cast<int>(R32(g_dealer + kFirst));
    for (const Row& r : g_rows[Theirs])
    {
        const int mesh = r.row - first;
        char name[24];
        sprintf_s(name, "ItemPrice%02d", mesh + 1);
        std::wstring t;
        if (mesh < 0 || mesh >= 5 || !winplates::TextOf(OnSide(name, Theirs), t))
            whole = false;
        sold += nnrules::Number(t);
    }
    if (whole)
    {
        skinkit::Row(x0, x1, y, rowH, L"New ship", Money(net + sold));
        y += rowH;
    }
    skinkit::Row(x0, x1, y, rowH, L"Your ship traded in", Money(-tradeIn));
    y += rowH;
    if (whole && sold > 0)
    {
        skinkit::Row(x0, x1, y, rowH, L"Sold to the dealer", Money(-sold));
        y += rowH;
    }
    skinkit::Row(x0, x1, y, rowH, net - tradeIn < 0 ? L"You receive" : L"You pay", Money(std::llabs(net - tradeIn)));
    y += rowH + 0.006f * H;
    if (CanBuyFinal())
    {
        const float axis = (x0 + x1) / 2, ch = 0.048f * H, w = TextWidth(L"BUY SHIP", skinrules::kChipWord * ch / skinrules::kChipLow) + 0.03f * H;
        if (g_buyStop && g_pad)
            skinkit::Focus(axis - w / 2 - 0.01f * H, y - 0.004f * H, axis + w / 2 + 0.01f * H, y + ch + 0.004f * H);
        ChipAt(axis - w / 2, y, ch, L"BUY SHIP", ChipClick, kFinalBuy);
        y += ch + 0.010f * H;
    }
    else
    {
        const std::wstring why = TextIn("ShipBuyProblem");
        if (!why.empty())
            y = skinkit::Body(x0, y, x1 - x0, why, 2, kAmber) + 0.006f * H;
    }
    skinkit::Rule(x0, x1, y);
    return y + 0.010f * H;
}

static void DrawMiddle(const Frame& f, float x0, float x1, float y)
{
    const float H = f.H;
    if (Final())
        y = DrawDeal(x0, x1, y, H);
    const Row* r = Focus();
    if (!r)
    {
        if (const u32 mesh = baseinfo::Find("TransferItemMesh", 1))
            PlaceAt(mesh, -winplates::g_W, 0);
        Text((x0 + x1) / 2, y + 0.06f * H, g_pad ? L"CHOOSE AN ITEM" : L"CLICK AN ITEM", 0.030f * H, Argb(kDim, 1), Center);
        return;
    }
    if (r->hp >= 0)
    {
        const equiprules::Hp& hp = g_free[r->hp];
        Text(x0, y + 0.02f * H, L"EMPTY HARDPOINT", 0.036f * H, Argb(kInk, 1));
        Text(x0, y + 0.066f * H, r->slot, 0.028f * H, Argb(kActive, 1));
        float cy = y + 0.12f * H;
        const std::vector<u32> fit = gamedata::HoldItemsFor(hp);
        Text(x0, cy, fit.empty() ? L"NOTHING IN YOUR HOLD FITS IT" : L"IN YOUR HOLD, IT TAKES", 0.026f * H, Argb(kChrome, 1));
        cy += 0.036f * H;
        for (size_t i = 0; i < fit.size() && cy < f.bottom; ++i, cy += 0.036f * H)
            TextMarquee(x0, cy, hudinfo::EquipName(fit[i]), 0.028f * H, x1 - x0, Argb(kInk, 1));
        return;
    }
    // the game's own mesh of the selected item, at the column's head
    const float big = 0.104f * H, axis = (x0 + x1) / 2;
    {
        const u32 mesh = baseinfo::Find("TransferItemMesh", 1);
        if (mesh && CanTrade())
        {
            PlaceAt(mesh, axis, y + big / 2);
            g_nextHoles.Add(axis - big / 2, y, axis + big / 2, y + big);
            g_holes.Add(axis - big / 2, y, axis + big / 2, y + big);
            y += big + 0.004f * H;
        }
        else if (mesh)
            PlaceAt(mesh, -winplates::g_W, 0);
    }
    TextFit(axis, y + 0.02f * H, r->name, 0.038f * H, x1 - x0, Argb(kInk, 1), Center);
    y += 0.050f * H;
    // the trade: the game's own numbers, or its own refusal
    const bool selling = g_side == Yours;
    if (CanTrade())
    {
        const std::wstring price = TextIn("TransferPriceText"), total = TextIn("QuantityTotalText");
        const int qty = Qty(), most = QtyMost();
        wchar_t b[64];
        const u32 s = Slider();
        if (s)
            swprintf_s(b, L"%d OF %d", qty, most);
        else
            swprintf_s(b, L"1");
        const float cx = x0, cw = x1 - x0;
        // one line on the column's axis: the deal, then what it comes to
        const std::wstring word = Final() ? (selling ? L"SELL TO DEALER" : L"TAKE BACK") : selling ? L"SELL" : L"BUY", sum = total.empty() ? price : total;
        const float gap = 0.024f * H, ww = TextWidth(word, 0.026f * H), qw = TextWidth(b, 0.030f * H), sw = TextWidth(sum, 0.034f * H);
        float lx = axis - (ww + qw + sw + 2 * gap) / 2;
        Text(lx, y + 0.016f * H, word, 0.026f * H, Argb(kChrome, 1));
        lx += ww + gap;
        Text(lx, y + 0.016f * H, b, 0.030f * H, Argb(kInk, 1));
        lx += qw + gap;
        Text(lx, y + 0.016f * H, sum, 0.034f * H, Argb(selling ? kFriend : kActive, 1));
        y += 0.040f * H;
        if (s && most > 0)
        {
            skinkit::Pips(cx, y, cw, 0.010f * H, static_cast<float>(qty) / most, 24, kActive);
            y += 0.020f * H;
        }
        if (!g_pad)
        {
            const float ch = 0.044f * H, pad = 0.03f * H + 0.012f * H, cs = skinrules::kChipWord * ch / skinrules::kChipLow;
            float rowW = TextWidth(word, cs) + pad;
            if (s)
                rowW += TextWidth(L"-", cs) + TextWidth(L"+", cs) + TextWidth(L"MAX", cs) + 3 * pad;
            if (Mountable(r))
                rowW += TextWidth(r->mounted ? L"UNMOUNT" : L"MOUNT", cs) + pad;
            float bx = ChipAt(axis - rowW / 2, y, ch, word, ChipClick, kBuy);
            if (s)
            {
                bx = ChipAt(bx, y, ch, L"-", ChipClick, kLess);
                bx = ChipAt(bx, y, ch, L"+", ChipClick, kMore);
                bx = ChipAt(bx, y, ch, L"MAX", ChipClick, kMax);
            }
            if (Mountable(r))
                ChipAt(bx, y, ch, r->mounted ? L"UNMOUNT" : L"MOUNT", ChipClick, kMount);
            y += ch + 0.012f * H;
        }
    }
    else
    {
        const std::wstring why = TextIn("ErrorHelp");
        if (!why.empty() && Selected())
            y = skinkit::Body(x0, y, x1 - x0, why, 2, kAmber) + 0.008f * H;
        if (!g_pad && Mountable(r))
        {
            ChipAt(x0, y, 0.044f * H, r->mounted ? L"UNMOUNT" : L"MOUNT", ChipClick, kMount);
            y += 0.056f * H;
        }
    }
    // the compare tag (issue 44), on either card: the mouse's chip; the pad's click in words (the lane is full at 1280 and drops it - ITEM, side, SELL,
    // FILTER, UNMOUNT)
    if (CanTag() && !g_pad)
    {
        ChipAt(x0, y, 0.044f * H, Tagged() ? L"UNTAG" : L"COMPARE THE STORE WITH THIS", ChipClick, kTag);
        y += 0.056f * H;
    }
    else if (CanTag())
    {
        const float gh = 0.028f * H, gw = GlyphWidth(glyphs::R3, gh);
        Glyph(glyphs::R3, x0 + gw / 2, y + 0.018f * H, gh, Argb(kInk, 1));
        Text(x0 + gw + 0.010f * H, y + 0.018f * H, Tagged() ? L"CLICK: UNTAG" : L"CLICK: COMPARE THE STORE WITH THIS", 0.024f * H, Argb(Tagged() ? kActive : kChrome, 1));
        y += 0.046f * H;
    }
    skinkit::Rule(x0, x1, y);
    y += 0.008f * H;
    if (g_kind == Kind::Equipment || (Final() && gamedata::Equip(r->arch)))
        y = Compare(x0, x1, y, r->arch, r->mounted, H);
    else
    {
        wchar_t b[64];
        const float vol = baseinfo::VolumeOf(r->arch), base = baseinfo::BasePrice(r->arch);
        if (vol >= 0)
        {
            swprintf_s(b, L"%.0f", vol);
            skinkit::Row(x0, x1, y, 0.036f * H, L"Cargo space each", b);
            y += 0.036f * H;
        }
        if (base >= 0)
        {
            skinkit::Row(x0, x1, y, 0.036f * H, L"Usual price", Money(static_cast<long long>(base + 0.5f)));
            y += 0.036f * H;
        }
        for (const Row& mine : g_rows[Yours])
            if (mine.arch == r->arch && !selling)
            {
                skinkit::Row(x0, x1, y, 0.036f * H, L"In your hold", std::to_wstring(mine.count));
                y += 0.036f * H;
            }
        y += 0.008f * H;
    }
    const float cardTop = y;
    const int slot = skinchip::Hot({x0, cardTop, x1, f.bottom}, Nothing, 0);
    if (slot >= 0 && slot == skinchip::Hovered())
        g_wheelSide = -1;
    Card(x0, x1, cardTop, f.bottom, H, r->arch);
}

// The page's ground starts under the strip and the room's words: a strip of three lines (1280x800, some rooms) stood cut by a fixed line. The fixed line stays the least,
// so the game's own views, which stand where the game has them, keep their place.
static float PageTop(float H) { return std::max(0.165f * H, std::max(uistrip::StripBottom() * H, nnstrip::RoomFoot()) + 0.006f * H); }

static void DrawDealer(const Frame& f)
{
    const float H = f.H, W = f.W;
    // the strip of the base and the room buttons stand: the player goes on from here as from the game's window
    const float top = PageTop(H);
    GroundBut(0, top, W, H, g_holes);
    g_nextHoles = Holes();
    const float ly0 = f.left, ly1 = 0.335f * W, cx0 = 0.358f * W, cx1 = 0.642f * W, ry0 = 0.665f * W, ry1 = f.right;
    float y = top + 0.024f * H;
    int used = 0, cap = 0;
    std::wstring tag;
    if (baseinfo::Cargo(used, cap) && cap > 0)
        tag = L"CARGO " + std::to_wstring(used) + L" / " + std::to_wstring(cap);
    if (Final())
    {
        Heading(ly0, ly1, y, H, L"ON THE NEW SHIP", L"");
        Heading(ry0, ry1, y, H, L"GOES TO THE DEALER", g_list[Theirs].count ? std::to_wstring(g_list[Theirs].count) + L" ITEMS" : L"");
    }
    else
    {
        Heading(ly0, ly1, y, H, L"YOUR SHIP", tag);
        Heading(ry0, ry1, y, H, L"DEALER", std::to_wstring(g_list[Theirs].count) + L" ON SALE");
    }
    y += 0.036f * H;
    float ty = y;
    if (g_kind == Kind::Equipment || Final())
    {
        // the game's filters, in its own words
        float tx = ly0;
        const float size = 0.026f * H, tabH = 0.040f * H;
        const int filter = NowFilter(), hover = skinchip::Hovered();
        for (int i = 0; i < Filters(); ++i)
        {
            const float w = TextWidth(g_filterWord[i], size) + 0.020f * H;
            const navlayout::Rect r{tx, y, tx + w, y + tabH};
            const int slot = skinchip::Hot(r, TabClick, i);
            skinchip::Tab(r, g_filterWord[i], size, i == filter, slot >= 0 && slot == hover);
            tx += w + 0.004f * H;
        }
        ty = y + tabH + 0.006f * H;
    }
    g_wheelSide = -2;
    // the left column's foot: what the ship does now, under its list
    float foot = f.bottom;
    if (g_kind == Kind::Equipment)
    {
        foot -= 0.076f * H;
        skinkit::Rule(ly0, ly1, foot + 0.006f * H);
        int guns = 0;
        float hull = 0, shield = 0, energy = 0;
        for (const baseinfo::Item& it : baseinfo::Equipment())
        {
            baseinfo::Gun g;
            if (it.mounted && baseinfo::GunOf(it.arch, g) && g.hullDps >= 0)
                ++guns, hull += g.hullDps, shield += std::max(0.0f, g.shieldDps), energy += std::max(0.0f, g.energyPs);
        }
        wchar_t b[96];
        swprintf_s(b, L"%d GUNS", guns);
        Text(ly0, foot + 0.026f * H, L"FIREPOWER", 0.024f * H, Argb(kChrome, 1));
        Text(ly1, foot + 0.026f * H, b, 0.024f * H, Argb(kInk, 1), Right);
        swprintf_s(b, L"HULL %.0f/S   SHIELD %.0f/S   ENERGY %.0f/S", hull, shield, energy);
        TextFit((ly0 + ly1) / 2, foot + 0.056f * H, b, 0.024f * H, ly1 - ly0, Argb(kInk, 1), Center);
    }
    else if (cap > 0)
    {
        foot -= 0.050f * H;
        skinkit::Rule(ly0, ly1, foot + 0.006f * H);
        Text(ly0, foot + 0.030f * H, L"HOLD", 0.024f * H, Argb(kChrome, 1));
        skinkit::Pips(ly0 + 0.07f * H, foot + 0.024f * H, ly1 - ly0 - 0.07f * H, 0.012f * H, static_cast<float>(used) / cap, 20, used >= cap ? kWarn : kChrome);
    }
    DrawList(f, Yours, ly0, ly1, ty, foot);
    DrawList(f, Theirs, ry0, ry1, y, f.bottom);
    g_holes = g_nextHoles;
    DrawMiddle(f, cx0, cx1, top + 0.024f * H);
    if (!g_pad)
    {
        float cx = ChipAt(ly0, LaneY(), LaneChipH(), L"CLOSE", ChipClick, kClose);
        if (Final() && uiclick::FindVisible("BackToShipDealer"))
            cx = ChipAt(cx, LaneY(), LaneChipH(), L"BACK TO SHIPS", ChipClick, kFinalBack);
        if (g_kind == Kind::Commodity && uiclick::FindVisible("TradeRouteButton"))
            cx = ChipAt(cx, LaneY(), LaneChipH(), L"BEST PRICES", ChipClick, kRoutes);
        Text(std::max(0.358f * W, cx), (deckmode::LaneTopH() + deckmode::LaneBottomH()) / 2 * H, L"CLICK AN ITEM, WHEEL SCROLLS", 0.026f * H, Argb(kChrome, 1));
    }
}

// ---- the repair window: the game's stands off the screen and does the work: its ticks, its sum, its OK. Its rows are its nine; the bars are its own.
struct Mend
{
    int slot = 0;
    bool ticked = false, hull = false;
    float health = -1;
    u32 arch = 0;
    long long price = 0;
    std::wstring name;
};
static std::vector<Mend> g_mends;
static int g_mendSel = 0;
constexpr int kMendRows = 9;
constexpr u32 kTickState = 0x3A0; // (the toggle's state, 0 ticked, 1 not: read in game, FREELANCER_RE "The docked pages")

static u32 MendCtl(const char* format, int i)
{
    char n[40];
    sprintf_s(n, format, i);
    return uiclick::FindVisible(n);
}
static void ReadMends()
{
    std::vector<Mend> rows;
    std::vector<baseinfo::Item> gear = baseinfo::Equipment();
    for (int i = 0; i < kMendRows; ++i)
    {
        const u32 box = MendCtl("RepairCheckBox%d", i);
        if (!box)
            continue;
        Mend m;
        m.slot = i;
        m.ticked = R32(box + kTickState) == 0;
        winplates::TextOf(MendCtl("RepairItemName%d", i), m.name);
        std::wstring price;
        winplates::TextOf(MendCtl("RepairItemPrice%d", i), price);
        m.price = nnrules::Number(price);
        // the rows stand in the list's order, the damaged ones only: each takes the first damaged item of its name, so two of a kind keep their own health (the game's bar holds no share to read)
        for (baseinfo::Item& it : gear)
            if (it.arch && it.health >= 0 && it.health < 1 && hudinfo::EquipName(it.arch) == m.name)
            {
                m.arch = it.arch, m.health = it.health;
                it.arch = 0;
                break;
            }
        m.hull = !m.arch;
        rows.push_back(m);
    }
    g_mends.swap(rows);
    g_mendSel = std::max(0, std::min(g_mendSel, static_cast<int>(g_mends.size()) - 1));
}
static void MendTick()
{
    if (g_mendSel < static_cast<int>(g_mends.size()))
        if (const u32 box = MendCtl("RepairCheckBox%d", g_mends[g_mendSel].slot))
            uiclick::Click(box), g_read = 0.05f;
}
// A click takes the row, and on the row in focus ticks it; no click here spends.
static void MendPick(int i)
{
    const bool again = i == g_mendSel;
    g_mendSel = i, g_cardFirst = 0;
    if (again || !g_pad)
        MendTick();
}
static void MendAll()
{
    if (const u32 c = uiclick::FindVisible("RepairToggleChecks"))
        uiclick::Click(c), g_read = 0.05f;
}
static bool CanMend() { return uiclick::FindVisible("RepairOkButton") != 0 && nnrules::Number(TextIn("RepairTotalCost")) > 0; }
static void MendGo()
{
    if (CanMend() && !room::NNWindowBusy())
        uiclick::Click(uiclick::FindVisible("RepairOkButton")), g_read = 0.25f;
}

static void DrawRepair(const Frame& f)
{
    const float H = f.H, W = f.W, top = PageTop(H);
    GroundBut(0, top, W, H, g_holes);
    g_nextHoles = Holes();
    const float x0 = f.left, x1 = 0.60f * W, rx0 = 0.625f * W, rx1 = f.right;
    float y = top + 0.024f * H;
    const int n = static_cast<int>(g_mends.size()), hover = skinchip::Hovered();
    Heading(x0, x1, y, H, L"DAMAGED ITEMS", n ? std::to_wstring(n) + (n == 1 ? L" ITEM" : L" ITEMS") : L"");
    y += 0.036f * H;
    // the sum stands under the rows, the rows take the room there is
    const float foot = f.bottom - 0.116f * H, pitch = n ? std::min(0.104f * H, (foot - y) / n) : 0, cell = std::min(0.066f * H, pitch);
    Icon icons[kMendRows];
    if (!n)
        Text((x0 + x1) / 2, y + 0.10f * H, L"NOTHING ON THIS SHIP NEEDS REPAIR", 0.032f * H, Argb(kDim, 1), Center);
    for (int i = 0; i < n; ++i)
    {
        const Mend& m = g_mends[i];
        const float rowTop = y + i * pitch, box = 0.030f * H, tx = x0 + box + 0.014f * H + cell + 0.012f * H;
        const bool on = i == g_mendSel;
        const int slot = skinchip::Hot({x0, rowTop, x1, rowTop + pitch}, RowClick, i);
        if (on)
            skinkit::Focus(x0, rowTop + 0.002f * H, x1, rowTop + pitch - 0.002f * H);
        else if (slot >= 0 && slot == hover)
            Rect(x0, rowTop, x1 - x0, pitch, Argb(kChrome, 0.10f));
        // the tick: a box, filled while the row is in the sum
        const float bx = x0 + 0.008f * H, by = rowTop + (pitch - box) / 2;
        Rect(bx, by, box, box, Argb(kChrome, 0.25f));
        Rect(bx + 0.002f * H, by + 0.002f * H, box - 0.004f * H, box - 0.004f * H, Argb(0x060716, 1));
        if (m.ticked)
            Rect(bx + 0.006f * H, by + 0.006f * H, box - 0.012f * H, box - 0.012f * H, Argb(kFriend, 1));
        const float ix = x0 + box + 0.014f * H;
        if (!m.hull) // (the hull has no icon of the game's)
        {
            icons[m.slot] = {ix + cell / 2, rowTop + pitch / 2, true};
            g_nextHoles.Add(ix + 1, rowTop + (pitch - cell) / 2, ix + cell - 1, rowTop + (pitch + cell) / 2);
        }
        const std::wstring price = Money(m.price);
        const float pad = 0.012f * H;
        TextMarquee(tx, rowTop + pitch * 0.34f, m.name, 0.030f * H, x1 - tx - pad * 2 - TextWidth(price, 0.030f * H), Argb(kInk, 1));
        Text(x1 - pad, rowTop + pitch * 0.34f, price, 0.030f * H, Argb(on ? kInk : m.ticked ? kActive : kDim, 1), Right);
        if (m.health >= 0)
        {
            wchar_t b[16];
            swprintf_s(b, L"%.0f %%", m.health * 100);
            skinkit::Pips(tx, rowTop + pitch * 0.74f - 0.005f * H, x1 - tx - 0.09f * H, 0.010f * H, m.health, 20, m.health < 0.35f ? kHostile : kAmber);
            Text(x1 - pad, rowTop + pitch * 0.74f, b, 0.024f * H, Argb(on ? kInk : kChrome, 1), Right);
        }
    }
    PlaceIcons("RepairSmallIcons%d", 0, kMendRows, 1, icons);
    g_holes = g_nextHoles;
    // the sum and what is left
    const long long total = nnrules::Number(TextIn("RepairTotalCost")), credits = baseinfo::Credits();
    float by = std::min(foot, y + n * pitch) + 0.010f * H;
    skinkit::Rule(x0, x1, by);
    by += 0.006f * H;
    skinkit::Row(x0, x1, by, 0.036f * H, L"Total cost", Money(total));
    by += 0.036f * H;
    skinkit::Row(x0, x1, by, 0.036f * H, L"Credits after", Money(credits - total));
    by += 0.040f * H;
    if (!g_pad && n)
    {
        float cx = ChipAt(x0, by, 0.048f * H, L"TICK ALL / NONE", ChipClick, kMendAll);
        ChipAt(cx, by, 0.048f * H, L"REPAIR", ChipClick, kMendGo, CanMend());
    }
    // the right: what is in focus, what it is and what it can do
    float ry = top + 0.024f * H;
    const gamedata::ShipFacts ship = gamedata::Ship(baseinfo::ShipArch());
    const int slotCard = skinchip::Hot({rx0, ry, rx1, f.bottom}, Nothing, 0);
    (void)slotCard;
    if (n && !g_mends[g_mendSel].hull)
    {
        const Mend& m = g_mends[g_mendSel];
        TextFit((rx0 + rx1) / 2, ry + 0.012f * H, m.name, 0.038f * H, rx1 - rx0, Argb(kInk, 1), Center);
        ry += 0.040f * H;
        skinkit::Rule(rx0, rx1, ry);
        ry = Compare(rx0, rx1, ry + 0.008f * H, m.arch, true, H);
        CardOf(rx0, rx1, ry, f.bottom, H, m.arch, gamedata::EquipInfo(m.arch));
    }
    else
    {
        TextFit((rx0 + rx1) / 2, ry + 0.012f * H, n ? g_mends[g_mendSel].name : baseinfo::ShipName(), 0.038f * H, rx1 - rx0, Argb(kInk, 1), Center);
        ry += 0.040f * H;
        skinkit::Rule(rx0, rx1, ry);
        ry += 0.008f * H;
        skinkit::Row(rx0, rx1, ry, 0.036f * H, L"Ship", baseinfo::ShipName());
        ry += 0.036f * H;
        if (ship.ok)
        {
            skinkit::Row(rx0, rx1, ry, 0.036f * H, L"Armor when whole", equiprules::ValueWord(ship.hull)); // (docked the hull's share reads nowhere yet: the game's own bar holds none)
            ry += 0.044f * H;
            CardOf(rx0, rx1, ry, f.bottom, H, ship.arch, ship.infoIds[1]);
        }
    }
    if (!g_pad)
    {
        ChipAt(x0, LaneY(), LaneChipH(), L"CLOSE", ChipClick, kClose);
        Text(0.358f * W, (deckmode::LaneTopH() + deckmode::LaneBottomH()) / 2 * H, L"CLICK A ROW TO TICK IT", 0.026f * H, Argb(kChrome, 1));
    }
}

// ---- the game's commodity list by base, as a page: its rows in the order that answers the question, the highest first, and the two answers named. It lists the bases the player has been to.
static std::vector<nnrules::Quote> g_quotes;
static std::wstring g_quoteTitle;
static void ReadQuotes()
{
    std::vector<nnrules::Quote> rows;
    for (int i = 0; i < 64; ++i)
    {
        nnrules::Quote q;
        std::wstring price, sells;
        if (!winplates::TextOf(MendCtl("CommodityByBasePrice%d", i), price))
            break;
        q.price = nnrules::Number(price);
        winplates::TextOf(MendCtl("CommodityByBuySellPrice%d", i), sells);
        winplates::TextOf(MendCtl("CommodityByBaseBase%d", i), q.base);
        winplates::TextOf(MendCtl("CommodityByBaseSystem%d", i), q.system);
        q.sells = sells;
        q.buyHere = !sells.empty() && towupper(sells[0]) == L'Y';
        rows.push_back(q);
    }
    nnrules::SortQuotes(rows);
    g_quotes.swap(rows);
    g_quoteTitle = Upper(TextIn("CommodityByBaseTitle"));
    g_list[0].count = static_cast<int>(g_quotes.size());
    g_list[0].Fit();
}

static void DrawPrices(const Frame& f)
{
    const float H = f.H, W = f.W, top = baseinfo::Docked() ? PageTop(H) : 0;
    GroundBut(0, top, W, H, Holes());
    g_holes = Holes();
    if (!top)
        Title(f, L"BEST PRICES");
    const float x0 = f.left, x1 = 0.66f * W, rx0 = 0.685f * W, rx1 = f.right;
    float y = (top ? top : f.top) + 0.024f * H;
    Heading(x0, x1, y, H, g_quoteTitle, std::to_wstring(g_quotes.size()) + (g_quotes.size() == 1 ? L" BASE KNOWN" : L" BASES KNOWN"));
    y += 0.040f * H;
    const float c1 = x0 + (x1 - x0) * 0.46f, c2 = x0 + (x1 - x0) * 0.78f;
    Text(x0, y, TextIn("CommodityByBaseBaseTitle"), 0.024f * H, Argb(kChrome, 1));
    Text(c1, y, TextIn("CommodityByBaseSystemTitle"), 0.024f * H, Argb(kChrome, 1));
    Text(c2, y, TextIn("CommodityByBaseBuySellTitle"), 0.024f * H, Argb(kChrome, 1));
    Text(x1 - 0.012f * H, y, TextIn("CommodityByBasePriceTitle"), 0.024f * H, Argb(kChrome, 1), Right);
    y += 0.020f * H;
    const float foot = f.bottom, pitch = 0.052f * H;
    List& l = g_list[0];
    l.rows = std::max(1, static_cast<int>((foot - y) / pitch));
    l.Fit();
    const std::wstring here = baseinfo::BaseName();
    const nnrules::Picks picks = nnrules::Pick(g_quotes);
    for (int i = 0; i < l.rows && l.first + i < l.count; ++i)
    {
        const nnrules::Quote& q = g_quotes[l.first + i];
        const float rowTop = y + i * pitch, cy = rowTop + pitch / 2;
        const bool on = l.first + i == l.sel && g_pad;
        if (on)
            skinkit::Focus(x0, rowTop + 0.002f * H, x1, rowTop + pitch - 0.002f * H);
        const u32 ink = l.first + i == picks.sell ? kFriend : q.base == here ? kActive : kInk;
        TextMarquee(x0 + 0.012f * H, cy, q.base, 0.030f * H, c1 - x0 - 0.024f * H, Argb(on ? kInk : ink, 1));
        TextMarquee(c1, cy, q.system, 0.028f * H, c2 - c1 - 0.012f * H, Argb(on ? kInk : kChrome, 1));
        Text(c2, cy, q.sells, 0.028f * H, Argb(on ? kInk : q.buyHere ? kInk : kDim, 1));
        Text(x1 - 0.012f * H, cy, Money(q.price), 0.030f * H, Argb(on ? kInk : ink, 1), Right);
    }
    if (l.count > l.rows)
        skinkit::ScrollBar(x1 + 0.006f * H, y, y + l.rows * pitch, static_cast<float>(l.first) / l.count, static_cast<float>(l.rows) / l.count);
    // the answers
    float ry = (top ? top : f.top) + 0.024f * H;
    Heading(rx0, rx1, ry, H, L"BEST DEALS", L"");
    ry += 0.046f * H;
    auto block = [&](const wchar_t* word, int at, u32 rgb)
    {
        Text(rx0, ry, word, 0.026f * H, Argb(kChrome, 1));
        ry += 0.034f * H;
        if (at < 0)
        {
            Text(rx0, ry, L"NO BASE KNOWN YET", 0.028f * H, Argb(kDim, 1));
            ry += 0.052f * H;
            return;
        }
        const nnrules::Quote& q = g_quotes[at];
        const std::wstring m = Money(q.price);
        TextMarquee(rx0, ry, q.base, 0.032f * H, rx1 - rx0 - TextWidth(m, 0.034f * H) - 0.012f * H, Argb(kInk, 1));
        Text(rx1, ry, m, 0.034f * H, Argb(rgb, 1), Right);
        ry += 0.030f * H;
        Text(rx0, ry, q.system, 0.024f * H, Argb(kChrome, 1));
        ry += 0.050f * H;
    };
    block(L"SELL IT HIGHEST AT", picks.sell, kFriend);
    block(L"BUY IT CHEAPEST AT", picks.buy, kActive);
    if (picks.sell >= 0 && picks.buy >= 0 && g_quotes[picks.sell].price > g_quotes[picks.buy].price)
    {
        int used = 0, cap = 0;
        const long long each = g_quotes[picks.sell].price - g_quotes[picks.buy].price;
        skinkit::Rule(rx0, rx1, ry - 0.012f * H);
        skinkit::Row(rx0, rx1, ry, 0.036f * H, L"Profit each", Money(each));
        ry += 0.036f * H;
        if (baseinfo::Cargo(used, cap) && cap > 0)
            skinkit::Row(rx0, rx1, ry, 0.036f * H, L"Your hold full", Money(each * cap));
    }
    if (!g_pad)
    {
        ChipAt(x0, LaneY(), LaneChipH(), L"CLOSE", ChipClick, kClose);
        if (l.count > l.rows)
            Text(0.358f * W, (deckmode::LaneTopH() + deckmode::LaneBottomH()) / 2 * H, L"WHEEL SCROLLS", 0.026f * H, Argb(kChrome, 1));
    }
}

// ---- the ship dealer
struct Offer
{
    int price = 0, refusal = 0, level = 0;
    std::wstring name, kind;
};
static std::vector<Offer> g_offers;
static int g_shipSel = -1;
static int g_shipFocus = 0;    // the pad's side of the ship dealer: 0 the list, 1 the ship (the stick turns it)
static int g_rsWas = 0;        // the stick's step direction last frame, and the time to its next repeat
static float g_rsRepeat = 0;
static u32 g_ship = 0; // NN_ShipTrader
static float g_view[4] = {};

static void ReadShips()
{
    std::vector<Offer> offers;
    const int n = std::min(3, static_cast<int>(R32(g_ship + 0x3CC)));
    for (int i = 0; i < n; ++i)
    {
        Offer o;
        o.price = static_cast<int>(R32(g_ship + 0x3A8 + 4 * i));
        const int status = static_cast<int>(R32(g_ship + 0x3FC + 4 * i));
        o.refusal = status == 1 ? 1 : status == 2 ? 2 : o.price > static_cast<int>(R32(g_ship + 0x3A4)) ? 3 : 0;
        o.level = static_cast<int>(R32(g_ship + 0x408 + 4 * i));
        char name[40];
        sprintf_s(name, "ShipTraderShipName%d", i);
        winplates::TextOf(uiclick::FindVisible(name), o.name);
        sprintf_s(name, "ShipTraderShipClass%d", i);
        winplates::TextOf(uiclick::FindVisible(name), o.kind);
        offers.push_back(o);
    }
    g_offers.swap(offers);
    g_shipSel = static_cast<int>(R32(g_ship + 0x3D0));
    if (g_shipSel >= static_cast<int>(g_offers.size()))
        g_shipSel = -1;
}
static void ShipPick(int i)
{
    char name[40];
    sprintf_s(name, "ShipTraderShipNameFrame%d", i);
    if (const u32 c = uiclick::FindVisible(name))
        uiclick::Click(c);
    g_cardFirst = 0, g_read = 0.05f;
}
static bool CanBuyShip() { return g_shipSel >= 0 && uiclick::FindVisible("ShipTraderBuyButton") != 0 && g_offers[g_shipSel].refusal == 0; }
static void ShipBuy()
{
    if (CanBuyShip() && !room::NNWindowBusy())
        uiclick::Click(uiclick::FindVisible("ShipTraderBuyButton"));
}

// The ship's hole in the page, fractions of H (the view's 0.165 + 0.115 .. 0.42): the wheel over it is the view's (Update)
constexpr float kShipHoleHalfW = 0.19f, kShipHoleTop = 0.28f, kShipHoleFoot = 0.585f;
static bool g_wheelHeld = false;
static void HoldWheel(bool on)
{
    if (on != g_wheelHeld && hostcalls::g.HoldRealWheel)
        hostcalls::g.HoldRealWheel(on), g_wheelHeld = on;
}

// The pad's stick turns the game's ship view as its mouse drag does (ShipViewControl, exe::VtShipView; drag 0x584C80, decompiled 7 Nov 2026): yaw +0x520 and
// pitch +0x524 in degrees, wrapped to +-360, yaw's sense flipped past 180 of pitch; copied to the live YPR +0x37C / +0x380; the matrix +0x358 it draws by
// rebuilt the same way (Ry(yaw), then Rx(pitch); roll stays 0). No easing pair: what is written stands, and a later mouse drag goes on from it.
static void ShipTurn(float rx, float ry, float dt)
{
    constexpr float kDead = 0.2f, kYawRate = 180.0f, kPitchRate = 120.0f, kDeg = 3.14159265f / 180.0f;
    const float sx = fabsf(rx) > kDead ? rx : 0, sy = fabsf(ry) > kDead ? ry : 0;
    const u32 view = uiclick::Find("ShipTrader3DShip");
    if ((sx == 0 && sy == 0) || !view || !exe::VtShipView.is(R32(view)))
        return;
    float yp[2] = {};
    if (!safemem::Read(view + 0x520, yp, sizeof(yp)) || !std::isfinite(yp[0]) || !std::isfinite(yp[1]))
        return;
    const auto wrap = [](float a) { while (a > 360) a -= 360; while (a < -360) a += 360; return a; };
    const float yaw = wrap(yp[0] + (fabsf(yp[1]) > 180 ? -1.0f : 1.0f) * sx * kYawRate * dt), pitch = wrap(yp[1] - sy * kPitchRate * dt); // (stick down as a drag down)
    float m[9] = {};
    const float cy = cosf(yaw * kDeg), syw = sinf(yaw * kDeg), cp = cosf(pitch * kDeg), sp = sinf(pitch * kDeg);
    m[4] = 1, m[0] = m[8] = cy, m[2] = syw, m[6] = -syw;
    for (int k = 0; k < 3; ++k)
    {
        const float a = m[3 + k], b = m[6 + k];
        m[3 + k] = cp * a - sp * b, m[6 + k] = a * sp + cp * b;
    }
    const auto put = [view](u32 off, float v) { *reinterpret_cast<float*>(static_cast<uintptr_t>(view + off)) = v; }; // (the view's own fields, read just above)
    put(0x520, yaw), put(0x524, pitch), put(0x37C, yaw), put(0x380, pitch);
    for (u32 i = 0; i < 9; ++i)
        put(0x358 + i * 4, m[i]);
}

// The ship's zoom is HDE's, not the game's (flplusplus: its handler in the view's wheel slot vt[0x48] adds speed x notches to +0x3EC, negative = farther,
// clamped by its ini's min / max; research\repos\flplusplus src\shippreviewscroll.cpp). The triggers move that field each frame, smoothly (the owner, 7 Nov 2026:
// the wheel's notches were stepped). No such plugin in the slot: no zoom, no prompt.
static u32 ShipViewCtl()
{
    const u32 view = uiclick::Find("ShipTrader3DShip");
    return view && exe::VtShipView.is(R32(view)) ? view : 0;
}
static bool ShipZoomable()
{
    const u32 view = ShipViewCtl();
    HMODULE in = nullptr;
    return view && GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                      reinterpret_cast<LPCSTR>(static_cast<uintptr_t>(R32(R32(view) + 0x48))), &in) &&
           in != GetModuleHandleA(nullptr);
}
static void ShipZoom(float lt, float rt, float dt)
{
    static bool read = false;
    static float speed = 3, nearest = 0, farthest = 100;
    if (!read)
    {
        read = true;
        char path[MAX_PATH] = {}, v[32] = {};
        GetModuleFileNameA(nullptr, path, MAX_PATH);
        if (char* slash = strrchr(path, '\\'))
            strcpy_s(slash + 1, path + MAX_PATH - (slash + 1), "flplusplus.ini");
        const auto num = [&](const char* key, float def)
        {
            GetPrivateProfileStringA("flplusplus", key, "", v, sizeof(v), path);
            return v[0] ? static_cast<float>(atof(v)) : def;
        };
        speed = num("ship_preview_scrolling_speed", 3), nearest = num("ship_preview_scrolling_min_distance", 0), farthest = std::max(nearest, num("ship_preview_scrolling_max_distance", 100));
        GetPrivateProfileStringA("flplusplus", "ship_preview_scrolling_inverse", "", v, sizeof(v), path);
        if (!_stricmp(v, "true") || !strcmp(v, "1"))
            speed = -speed;
    }
    const float t = (rt > 0.1f ? rt : 0) - (lt > 0.1f ? lt : 0); // RT in (the field toward 0), LT out
    const u32 view = ShipViewCtl();
    float z = 0;
    if (t == 0 || !ShipZoomable() || !safemem::Read(view + 0x3EC, &z, 4) || !std::isfinite(z))
        return;
    z = std::max(-farthest, std::min(-nearest, z + t * speed * 8.0f * dt)); // (8 notches' worth a second at a full trigger)
    *reinterpret_cast<float*>(static_cast<uintptr_t>(view + 0x3EC)) = z;
}

// The game's 3D view stays the game's: the window goes off the screen, the view comes back to the page's place, a hole in the ground over it.
// (it has no rectangle: it goes where the window goes, by its own position call, so it is called back)
static void ViewBack(u32 view)
{
    float pos[3] = {};
    if (!view || !safemem::Read(view + 0x60, pos, sizeof(pos)) || fabsf(pos[0]) < 0.01f)
        return;
    pos[0] = 0;
    reinterpret_cast<void(__thiscall*)(u32, const float*)>(R32(R32(view) + 0x14))(view, pos);
}

static float StatRow(float x0, float x1, float y, float H, const wchar_t* label, float mine, float theirs, bool show)
{
    const float pitch = 0.036f * H, c1 = x0 + (x1 - x0) * 0.62f;
    Text(x0, y + pitch / 2, label, 0.025f * H, Argb(kChrome, 1));
    Text(c1, y + pitch / 2, equiprules::ValueWord(mine), 0.028f * H, Argb(kInk, 1), Right);
    if (show)
    {
        const equiprules::Stat s{label, mine, 1, L""};
        const equiprules::Verdict j = equiprules::Judge(s, theirs);
        wchar_t b[24];
        if (j == equiprules::Verdict::Same)
            swprintf_s(b, L"=");
        else
            swprintf_s(b, L"%+.0f", mine - theirs);
        Text(x1, y + pitch / 2, b, 0.026f * H, Argb(j == equiprules::Verdict::Better ? kFriend : j == equiprules::Verdict::Worse ? kHostile : kChrome, 1), Right);
    }
    return y + pitch;
}

static u32 g_shipCardArch = 0;
static void DrawShips(const Frame& f)
{
    const float H = f.H, W = f.W, top = PageTop(H), view = 0.165f * H; // (view: the game's 3D ship stands where the game has it)
    const float ly0 = f.left, ly1 = 0.30f * W, cx0 = 0.325f * W, cx1 = 0.675f * W, ry0 = 0.70f * W, ry1 = f.right;
    // the view stands where the game has it, in the screen's middle
    g_nextHoles = Holes();
    g_nextHoles.Add(W / 2 - kShipHoleHalfW * H, kShipHoleTop * H, W / 2 + kShipHoleHalfW * H, kShipHoleFoot * H);
    (void)view;
    GroundBut(0, top, W, H, g_holes);
    float y = top + 0.024f * H;
    Heading(ly0, ly1, y, H, L"SHIPS HERE", std::to_wstring(g_offers.size()) + L" ON SALE");
    y += 0.036f * H;
    const float pitch = 0.100f * H, cell = 0.092f * H;
    const int hover = skinchip::Hovered();
    // the pad on the ship's side: its view framed as the focus, the stick's glyph and TURN under it (the owner: say plainly the stick turns the ship now)
    const bool shipSide = g_shipFocus == 1 && views::DevicePad() && g_shipSel >= 0;
    if (shipSide)
    {
        const float hx0 = W / 2 - kShipHoleHalfW * H, hy0 = kShipHoleTop * H, hx1 = W / 2 + kShipHoleHalfW * H, hy1 = kShipHoleFoot * H, gh = 0.030f * H;
        const float e = std::max(1.0f, 0.003f * H); // (a whole rectangle in the focus's yellow: the list's style left it three lines - the owner)
        Rect(hx0, hy0, hx1 - hx0, e, Argb(kActive, 0.85f));
        Rect(hx0, hy1 - e, hx1 - hx0, e, Argb(kActive, 0.85f));
        Rect(hx0, hy0, e, hy1 - hy0, Argb(kActive, 0.85f));
        Rect(hx1 - e, hy0, e, hy1 - hy0, Argb(kActive, 0.85f));
        const std::wstring word = L"TURN";
        const float tw = TextWidth(word, 0.026f * H), gw = GlyphWidth(glyphs::RS, gh), x = W / 2 - (gw + 0.008f * H + tw) / 2, ty = hy1 - 0.026f * H;
        Glyph(glyphs::RS, x + gw / 2, ty, gh, Argb(kInk, 1));
        Text(x + gw + 0.008f * H, ty, word, 0.026f * H, Argb(kInk, 1));
    }
    Icon icons[3];
    for (size_t i = 0; i < g_offers.size(); ++i, y += pitch)
    {
        const Offer& o = g_offers[i];
        const bool on = static_cast<int>(i) == g_shipSel;
        const int slot = skinchip::Hot({ly0, y, ly1, y + pitch}, RowClick, static_cast<int>(i));
        icons[i] = {ly0 + cell / 2, y + pitch / 2, true};
        g_nextHoles.Add(ly0 + 1, y + (pitch - cell) / 2, ly0 + cell - 1, y + (pitch + cell) / 2);
        if (on && !shipSide)
            skinkit::Focus(ly0 + cell, y + 0.002f * H, ly1, y + pitch - 0.002f * H);
        else if (on)
            Rect(ly0 + cell, y, ly1 - ly0 - cell, pitch, Argb(kChrome, 0.06f)); // (chosen: the focus is on the ship)
        else if (slot >= 0 && slot == hover)
            Rect(ly0 + cell, y, ly1 - ly0 - cell, pitch, Argb(kChrome, 0.10f));
        const float pad = 0.012f * H, tx = ly0 + cell + pad;
        const std::wstring price = Money(o.price);
        TextMarquee(tx, y + pitch * 0.32f, o.name, 0.034f * H, ly1 - tx - 2 * pad - TextWidth(price, 0.030f * H), Argb(kInk, 1));
        Text(ly1 - pad, y + pitch * 0.32f, price, 0.030f * H, Argb(on ? kInk : kActive, 1), Right);
        Text(tx, y + pitch * 0.72f, o.kind, 0.024f * H, Argb(on ? kInk : kChrome, 0.9f));
        if (o.refusal)
        {
            wchar_t b[32];
            if (o.refusal == 1)
                swprintf_s(b, L"NEEDS LEVEL %d", o.level);
            else
                swprintf_s(b, o.refusal == 2 ? L"REFUSED" : L"TOO DEAR");
            Text(ly1 - pad, y + pitch * 0.72f, b, 0.024f * H, Argb(on ? kInk : kAmber, 1), Right);
        }
    }
    PlaceIcons("ShipTraderShipMesh%d", 0, 3, 1, icons);
    g_holes = g_nextHoles;
    // the deal
    y += 0.014f * H;
    skinkit::Rule(ly0, ly1, y);
    y += 0.008f * H;
    const long long credits = baseinfo::Credits(), budget = static_cast<int>(R32(g_ship + 0x3A4));
    const long long tradeIn = budget - credits;
    const float rowH = 0.038f * H;
    std::wstring own = baseinfo::ShipName();
    skinkit::Row(ly0, ly1, y, rowH, L"Your ship", own);
    y += rowH;
    skinkit::Row(ly0, ly1, y, rowH, L"Trade-in, with its equipment", Money(tradeIn));
    y += rowH;
    if (g_shipSel >= 0)
    {
        const Offer& o = g_offers[g_shipSel];
        const long long net = o.price - tradeIn;
        skinkit::Row(ly0, ly1, y, rowH, L"Costs you", Money(net));
        y += rowH;
        skinkit::Row(ly0, ly1, y, rowH, L"Credits after", Money(credits - net));
        y += rowH;
        if (!g_pad)
            ChipAt(ly0, y + 0.012f * H, 0.048f * H, L"BUY THIS SHIP", ChipClick, kShipBuy, CanBuyShip());
    }
    // the middle: the ship and its card
    if (g_shipSel >= 0)
    {
        const Offer& o = g_offers[g_shipSel];
        Text((cx0 + cx1) / 2, top + 0.040f * H, o.name, 0.044f * H, Argb(kInk, 1), Center);
        Text((cx0 + cx1) / 2, top + 0.082f * H, o.kind, 0.028f * H, Argb(kChrome, 1), Center);
        const gamedata::ShipFacts s = gamedata::ShipNamed(o.name), m = gamedata::Ship(baseinfo::ShipArch());
        if (s.arch != g_shipCardArch)
        {
            g_shipCardArch = s.arch, g_cardFirst = 0, g_cardArch = 0;
            g_cardText = text::InfoCard(s.infoIds[1]);
        }
        const float cardTop = view + 0.47f * H, size = 0.025f * H, cp = 0.031f * H;
        const std::vector<std::wstring> lines = Wrap(g_cardText, size, cx1 - cx0 - 0.016f * H);
        g_cardLines = static_cast<int>(lines.size());
        g_cardRoom = std::max(1, static_cast<int>((f.bottom - cardTop) / cp));
        g_cardFirst = std::max(0, std::min(g_cardFirst, g_cardLines - g_cardRoom));
        for (int i = 0; i < g_cardRoom && g_cardFirst + i < g_cardLines; ++i)
            Text(cx0, cardTop + cp * (i + 0.5f), lines[g_cardFirst + i], size, Argb(kInk, 0.9f));
        if (g_cardLines > g_cardRoom)
            skinkit::ScrollBar(cx1 - 0.004f * H, cardTop, f.bottom, static_cast<float>(g_cardFirst) / g_cardLines, static_cast<float>(g_cardRoom) / g_cardLines);
        // the numbers against the player's
        float ry = top + 0.024f * H;
        Heading(ry0, ry1, ry, H, L"AGAINST YOUR SHIP", own);
        ry += 0.040f * H;
        const bool c = m.ok;
        ry = StatRow(ry0, ry1, ry, H, L"ARMOR", s.hull, m.hull, c);
        ry = StatRow(ry0, ry1, ry, H, L"CARGO SPACE", s.hold, m.hold, c);
        ry = StatRow(ry0, ry1, ry, H, L"GUN MOUNTS", static_cast<float>(s.guns), static_cast<float>(m.guns), c);
        ry = StatRow(ry0, ry1, ry, H, L"TURRET MOUNTS", static_cast<float>(s.turrets), static_cast<float>(m.turrets), c);
        ry = StatRow(ry0, ry1, ry, H, L"BEST GUN CLASS", static_cast<float>(s.gunClass), static_cast<float>(m.gunClass), c);
        ry = StatRow(ry0, ry1, ry, H, L"BEST SHIELD CLASS", static_cast<float>(s.shieldClass), static_cast<float>(m.shieldClass), c);
        ry = StatRow(ry0, ry1, ry, H, L"NANOBOTS", s.nanobots, m.nanobots, c);
        ry = StatRow(ry0, ry1, ry, H, L"SHIELD BATTERIES", s.batteries, m.batteries, c);
        ry = StatRow(ry0, ry1, ry, H, L"TORPEDO MOUNTS", static_cast<float>(s.torpedoes), static_cast<float>(m.torpedoes), c);
    }
    else
        Text((cx0 + cx1) / 2, view + 0.27f * H, g_pad ? L"CHOOSE A SHIP" : L"CLICK A SHIP", 0.032f * H, Argb(kDim, 1), Center);
    if (!g_pad)
        ChipAt(ly0, LaneY(), LaneChipH(), L"CLOSE", ChipClick, kClose);
}

static Holes g_cut; // the panels' under-ground leaves these
// ---- the job board and the news: a panel in the room, the room showing beside it. The game's window stays (these two answer no move of their own, and their picture stands by it) and does the
// work; its text is reflowed into the panel's column, its icons come to the rows, what could be clicked blind or read through a hole leaves. The rows' words are the game's own controls' text: their
// five are what its list shows, so an edge row scrolls the game's list.
static bool Jobs() { return g_kind == Kind::Jobs; }
static u32 BoardRoot() { return uiclick::FindVisible(Jobs() ? "NN_MissionVendor" : "NN_News"); }
static const char* BoardText() { return Jobs() ? "MissionVendorMissionFullText" : "NewsStoryText"; }
static u32 BoardBox(int i)
{
    char n[48];
    sprintf_s(n, Jobs() ? "MissionVendorSummaryBox%d" : "NewsHeadlineBox%d", i);
    return uiclick::FindVisible(n);
}
static bool RowSelected(u32 box) { return box && (R32(box + 0x35C) & 0x10) != 0; }
static int BoardSel()
{
    for (int i = 0; i < 5; ++i)
        if (RowSelected(BoardBox(i)))
            return i;
    return -1;
}
static void BoardPick(int i)
{
    if (const u32 c = BoardBox(i))
        uiclick::Click(c);
}
static void BoardStep(int by)
{
    const int sel = BoardSel(), next = sel + by;
    if (next >= 0 && BoardBox(next) && next < 5)
    {
        BoardPick(next);
        return;
    }
    // the list's edge: the game's list moves under the same row
    if (Jobs())
        if (const u32 bar = uiclick::FindVisible("MissionVendorSummaryScroll"))
        {
            uiclick::Scroll(bar, static_cast<float>(by), false, nullptr, nullptr);
            BoardPick(sel);
        }
}
static void BoardAccept()
{
    if (const u32 c = uiclick::FindVisible("MissionVendorAcceptButton"))
        if (!room::NNWindowBusy())
            uiclick::Click(c);
}
static u32 TextBar()
{
    const u32 t = uiclick::FindVisible(BoardText());
    return t ? R32(t + 0x3F0) : 0;
}
static void TextScroll(float px)
{
    if (const u32 bar = TextBar())
        uiclick::Scroll(bar, px, false, nullptr, nullptr);
}
// The pad's side of the board (issue 45: the stick scrolled the list, a long description could not be read): 0 the list, 1 the text - only while the text
// overflows (D-pad right goes over), as the dealers' sides
static int g_boardSide = 0;
static float g_textAcc = 0;
static bool BoardTextSide() { return g_boardSide == 1 && TextBar() != 0; }

struct Panel
{
    float x0, y0, x1, y1, mid, tx0, tx1, ty0, ty1, px0, py0, px1, py1;
};
static Panel BoardPanel(const Frame& f)
{
    Panel p;
    const float H = f.H, W = f.W;
    p.x0 = 0.07f * W, p.x1 = 0.93f * W, p.y0 = std::max(0.175f * H, PageTop(H) + 0.010f * H), p.y1 = 0.885f * H;
    p.mid = W / 2 - 0.01f * W;
    // the picture stays where the game draws it: it has no place of its own, only its window's
    p.px0 = W / 2 + 0.1525f * H, p.px1 = W / 2 + 0.479f * H;
    p.py0 = (Jobs() ? 0.219f : 0.259f) * H, p.py1 = p.py0 + 0.24f * H;
    p.tx0 = p.mid + 0.03f * H, p.tx1 = p.x1 - 0.05f * H;
    p.ty0 = p.py1 + 0.075f * H, p.ty1 = p.y1 - (Jobs() ? 0.02f : 0.075f) * H;
    return p;
}

static float g_tick = 0; // the ticker's offset, px
static void BoardPlace(const Frame& f)
{
    const Panel p = BoardPanel(f);
    const u32 box = uiclick::FindVisible(BoardText());
    // (a rich text keeps its own place: size +0x330, pixels +0x358, left and top +0x348, its layout position +0x47C: windowtext's FitBoxes)
    const float w = RF(box + 0x330);
    const int px = box ? static_cast<int>(R32(box + 0x358)) : 0;
    if (box && w > 0 && px > 0 && exe::RichSetSize.ok())
    {
        const float k = static_cast<float>(px) / w, wantW = p.tx1 - p.tx0, wantH = p.ty1 - p.ty0;
        float pos[3] = {};
        safemem::Read(box + 0x47C, pos, sizeof(pos));
        const float dx = p.tx0 - static_cast<float>(static_cast<int>(R32(box + 0x348))), dy = static_cast<float>(static_cast<int>(R32(box + 0x34C))) - p.ty0;
        if (fabsf(dx) >= 1.5f || fabsf(dy) >= 1.5f)
        {
            pos[0] += dx / k, pos[1] += dy / k;
            SetPosition(box, pos);
        }
        if (fabsf(static_cast<float>(px) - wantW) > 2 || fabsf(static_cast<float>(R32(box + 0x35C)) - wantH) > 2)
        {
            hk::Call<void(__thiscall*)(u32, float, float)>(exe::RichSetSize, box, wantW / k, wantH / k);
            if (exe::RichLayout.ok())
                hk::Call<u32(__thiscall*)(u32)>(exe::RichLayout, box);
        }
    }
    if (const u32 bar = TextBar())
        PlaceAt(bar, -f.W, 0);
    static const char* const kJobs[] = {"MissionVendorIcon", "MissionVendorTitle", "MissionVendorMissionReward", "MissionVendorMissionDest", "MissionVendorMissionTitle", "MissionVendorMissionReject", "MissionVendorAcceptButton", "MissionVendorAcceptText", "MissionVendorCloseButton"};
    static const char* const kNews[] = {"NewsStoryIcon", "NewsTitle", "NewsStoryHeadline", "NewsTicker", "NewsCloseButton"};
    if (Jobs())
        for (const char* n : kJobs)
            PlaceAt(uiclick::FindVisible(n), -f.W, 0);
    else
        for (const char* n : kNews)
            PlaceAt(uiclick::FindVisible(n), -f.W, 0);
}

static void DrawBoard(const Frame& f)
{
    const float H = f.H;
    const Panel p = BoardPanel(f);
    // the text and the picture show through, on the panel's ground under the game's pass
    Holes h;
    h.Add(p.px0, p.py0, p.px1, p.py1);
    h.Add(p.tx0 - 2, p.ty0 - 2, p.tx1 + 2, p.ty1 + 2);
    const float cell = 0.082f * H, list0 = p.y0 + 0.07f * H, pitch = std::min(0.112f * H, (p.y1 - list0 - 0.02f * H) / 5);
    Icon icons[5];
    const float x0 = p.x0 + 0.02f * H, x1 = p.mid - 0.01f * H;
    const int sel = BoardSel(), hover = skinchip::Hovered();
    for (int i = 0; i < 5; ++i)
        if (BoardBox(i))
        {
            const float top = list0 + i * pitch;
            icons[i] = {x0 + cell / 2, top + pitch / 2, true};
            h.Add(x0 + 1, top + (pitch - cell) / 2, x0 + cell - 1, top + (pitch + cell) / 2);
        }
    GroundBut(p.x0, p.y0, p.x1, p.y1, h);
    g_holes = Holes();
    g_holes.Add(p.x0, p.y0, p.x1, p.y1);
    g_cut = Holes();
    g_cut.Add(p.px0, p.py0, p.px1, p.py1);
    skinkit::Plate(p.x0, p.y0, p.x1, p.y1, kTL | kBR, 0.0f);
    Text(x0, p.y0 + 0.034f * H, Jobs() ? L"JOB BOARD" : L"NEWS", 0.036f * H, Argb(kInk, 1));
    skinkit::Rule(x0, x1, p.y0 + 0.060f * H);
    // each job's difficulty on its row, the number the game prints only in the chosen job's text ("Difficulty : n"; Shockwave, 7 Nov 2026: "without needing to
    // scroll"): the base's offers in the board's order from the host (core\host\events.cpp), lined up with the rows by their rewards. Rows that do not line up show
    // none: no number rather than another job's.
    int jobDiff[5] = {}, jobTotal = 0, jobFirst = 0, jobRows = 0;
    if (Jobs())
    {
        u32 rewards[64];
        int diffs[64];
        const int n = hostcalls::OfferDifficulties(fl::PlayerBase(), rewards, diffs, 64);
        long long rowReward[5];
        int rows = 0;
        for (int i = 0; i < 5; ++i)
        {
            char nm[48];
            sprintf_s(nm, "MissionVendorSummaryReward%d", i);
            std::wstring t;
            if (!BoardBox(i) || !winplates::TextOf(uiclick::FindVisible(nm), t))
                break;
            long long v = 0;
            for (const wchar_t ch : t)
                if (ch >= L'0' && ch <= L'9')
                    v = v * 10 + (ch - L'0');
            rowReward[rows++] = v;
        }
        for (int s = 0; rows > 0 && s + rows <= n; ++s)
        {
            bool match = true;
            for (int i = 0; i < rows && match; ++i)
                match = static_cast<long long>(rewards[s + i]) == rowReward[i];
            if (match)
            {
                for (int i = 0; i < rows; ++i)
                    jobDiff[i] = diffs[s + i];
                jobTotal = n, jobFirst = s, jobRows = rows;
                break;
            }
        }
    }
    for (int i = 0; i < 5; ++i)
    {
        const u32 box = BoardBox(i);
        if (!box)
            continue;
        const float top = list0 + i * pitch, a = x0 + cell + 0.012f * H;
        const int slot = skinchip::Hot({x0, top, x1, top + pitch}, RowClick, i);
        const bool on = i == sel;
        if (on && !(g_pad && BoardTextSide()))
            skinkit::Focus(a - 0.006f * H, top + 0.003f * H, x1, top + pitch - 0.003f * H);
        else if (on)
            Rect(a - 0.006f * H, top, x1 - a + 0.006f * H, pitch, Argb(kChrome, 0.06f)); // (chosen: the pad is on the text)
        else if (slot >= 0 && slot == hover)
            Rect(a - 0.006f * H, top, x1 - a + 0.006f * H, pitch, Argb(kChrome, 0.10f));
        char n[48];
        if (Jobs())
        {
            sprintf_s(n, "MissionVendorIconFrame%d", i);
            const std::wstring type = Upper(baseinfo::HintText(uiclick::FindVisible(n)));
            std::wstring reward, dest, title;
            sprintf_s(n, "MissionVendorSummaryReward%d", i);
            winplates::TextOf(uiclick::FindVisible(n), reward);
            sprintf_s(n, "MissionVendorSummaryDest%d", i);
            winplates::TextOf(uiclick::FindVisible(n), dest);
            sprintf_s(n, "MissionVendorSummaryTitle%d", i);
            winplates::TextOf(uiclick::FindVisible(n), title);
            const float rw = TextWidth(reward, 0.032f * H), pad = 0.012f * H;
            TextMarquee(a + pad, top + pitch * 0.34f, type.empty() ? title : type, 0.032f * H, x1 - a - rw - 3 * pad, Argb(kInk, 1));
            float dw = 0;
            if (jobDiff[i] > 0)
            {
                const std::wstring dword = L"DIFFICULTY " + std::to_wstring(jobDiff[i]);
                dw = TextWidth(dword, 0.024f * H) + 2 * pad;
                Text(x1 - pad, top + pitch * 0.72f, dword, 0.024f * H, Argb(kInk, 1), Right); // (white: what the row adds, beside the blue it repeats)
            }
            TextMarquee(a + pad, top + pitch * 0.72f, title + L"   " + dest, 0.024f * H, x1 - a - 2 * pad - dw, Argb(on ? kInk : kChrome, 1));
            Text(x1 - pad, top + pitch * 0.34f, reward, 0.032f * H, Argb(on ? kInk : kFriend, 1), Right);
        }
        else
        {
            std::wstring line;
            sprintf_s(n, "NewsHeadlineHeadline%d", i);
            winplates::TextOf(uiclick::FindVisible(n), line);
            TextMarquee(a + 0.012f * H, top + pitch / 2, line, 0.030f * H, x1 - a - 0.024f * H, Argb(on ? kInk : kChrome, 1));
        }
    }
    PlaceIcons(Jobs() ? "MissionVendorSummaryIcon%d" : "NewsHeadlineIcon%d", 0, 5, 1, icons);
    // more jobs than rows: a bar beside them says how many and where the rows stand (Shockwave, 7 Nov 2026: no sign of more)
    if (jobTotal > jobRows && jobRows > 0)
        skinkit::ScrollBar(x1 + 0.008f * H, list0, list0 + jobRows * pitch, static_cast<float>(jobFirst) / jobTotal, static_cast<float>(jobRows) / jobTotal);
    // the chosen one's head, over its text
    const float hy = p.ty0 - 0.036f * H;
    if (Jobs())
    {
        std::wstring reward, dest, title;
        winplates::TextOf(uiclick::FindVisible("MissionVendorMissionReward"), reward);
        winplates::TextOf(uiclick::FindVisible("MissionVendorMissionDest"), dest);
        winplates::TextOf(uiclick::FindVisible("MissionVendorMissionTitle"), title);
        if (!reward.empty())
        {
            const std::wstring type = sel >= 0 ? [&] { char n[48]; sprintf_s(n, "MissionVendorIconFrame%d", sel); return Upper(baseinfo::HintText(uiclick::FindVisible(n))); }() : std::wstring();
            const float rw = TextWidth(reward, 0.034f * H);
            TextMarquee(p.tx0, hy, type + L"   " + title + L"   " + dest, 0.028f * H, p.tx1 - p.tx0 - rw - 0.02f * H, Argb(kInk, 1));
            Text(p.tx1, hy, reward, 0.034f * H, Argb(kFriend, 1), Right);
        }
    }
    else
    {
        std::wstring head;
        winplates::TextOf(uiclick::FindVisible("NewsStoryHeadline"), head);
        TextMarquee(p.tx0, hy, head, 0.030f * H, p.tx1 - p.tx0, Argb(kInk, 1));
        // the market's ticker across the panel, the whole of what the base trades: the game's own control shows a slice of it
        static u32 tickBase = 0;
        static std::wstring tick;
        if (tickBase != fl::PlayerBase())
        {
            tickBase = fl::PlayerBase();
            tick.clear();
            for (const gamedata::Good& g : gamedata::Base(tickBase).goods)
                tick += Upper(hudinfo::EquipName(g.arch)) + L" " + std::to_wstring(static_cast<int>(g.price + 0.5f)) + L"   ---   ";
        }
        if (tick.empty())
            winplates::TextOf(uiclick::FindVisible("NewsTicker"), tick), tickBase = 0;
        if (!tick.empty())
        {
            const float size = 0.028f * H, a = p.x0 + 0.03f * H, b = p.x1 - 0.03f * H, w = std::max(TextWidth(tick, size), 1.0f), y = p.y1 - 0.034f * H;
            g_tick = fmodf(g_tick, w);
            for (float x = a - g_tick; x < b; x += w)
                TextClipped(x, y, tick, size, a, b, Argb(kChrome, 1));
        }
    }
    // its bar, the game's own state
    if (const u32 bar = TextBar())
    {
        float lo = 0, hi = 0;
        const float pos = uiclick::Scroll(bar, 0, false, &lo, &hi);
        if (hi > lo)
        {
            const float room = p.ty1 - p.ty0, whole = room + hi - lo;
            skinkit::ScrollBar(p.tx1 + 0.012f * H, p.ty0, p.ty1, (pos - lo) / whole, room / whole);
        }
    }
    if (g_pad && BoardTextSide()) // the pad on the text: framed as the ship dealer's ship is
    {
        const float e = std::max(1.0f, 0.003f * H), fx0 = p.tx0 - 0.010f * H, fy0 = p.ty0 - 0.008f * H, fx1 = p.tx1 + 0.026f * H, fy1 = p.ty1 + 0.008f * H;
        Rect(fx0, fy0, fx1 - fx0, e, Argb(kActive, 0.85f));
        Rect(fx0, fy1 - e, fx1 - fx0, e, Argb(kActive, 0.85f));
        Rect(fx0, fy0, e, fy1 - fy0, Argb(kActive, 0.85f));
        Rect(fx1 - e, fy0, e, fy1 - fy0, Argb(kActive, 0.85f));
    }
    if (!g_pad)
    {
        float cx = ChipAt(p.x0, LaneY(), LaneChipH(), L"CLOSE", ChipClick, kClose);
        if (Jobs() && uiclick::FindVisible("MissionVendorAcceptButton"))
            cx = ChipAt(cx, LaneY(), LaneChipH(), L"ACCEPT JOB", ChipClick, kAccept);
        Text(cx + 0.02f * H, (deckmode::LaneTopH() + deckmode::LaneBottomH()) / 2 * H, L"WHEEL SCROLLS THE TEXT", 0.026f * H, Argb(kChrome, 1));
    }
}

// ---- the frame
static Kind KindNow()
{
    if (uiclick::FindVisible("NN_CommodityByBase"))
        return Kind::Prices;
    if (uiclick::FindVisible("NN_Repair"))
        return Kind::Repair;
    if (uiclick::FindVisible("NN_ShipTrader"))
        return Kind::Ships;
    if (uiclick::FindVisible("NN_MissionVendor"))
        return Kind::Jobs;
    if (uiclick::FindVisible("NN_News"))
        return Kind::News;
    const u32 d = uiclick::FindVisible("NN_Dealer");
    if (!d || !uiclick::FindVisible("NN_Inventory"))
        return Kind::None;
    const u32 k = R32(d + kDealerKind);
    return k == 1 ? Kind::Commodity : k == 3 ? Kind::Equipment : k == 2 ? Kind::Final : Kind::None;
}

void Update(float dt, bool pad)
{
    g_pad = pad;
    // The switches take effect at the next opening: a page standing keeps its windows off the screen and its controls in its places until it closes.
    if (g_kind == Kind::None)
        g_pagesOn = hudrows::g.basePages && hudrows::g.hudStyle != 0 && hudrows::g.uiStyle == 1;
    const Kind kind = g_pagesOn ? KindNow() : Kind::None;
    const u32 d = kind == Kind::Ships ? uiclick::FindVisible("NN_ShipTrader") : kind == Kind::Jobs ? uiclick::FindVisible("NN_MissionVendor") : kind == Kind::News ? uiclick::FindVisible("NN_News") : kind == Kind::Repair ? uiclick::FindVisible("NN_Repair") : kind == Kind::Prices ? uiclick::FindVisible("NN_CommodityByBase") : kind != Kind::None ? uiclick::FindVisible("NN_Dealer") : 0;
    if (kind != g_kind || d != g_dealer)
    {
        g_offers.clear(), g_shipSel = -1, g_shipCardArch = 0, g_shipFocus = 0, g_boardSide = 0;
        g_read = 0, g_tradeWait = 0, g_cardArch = 0;
        g_rows[0].clear(), g_rows[1].clear();
        g_list[0] = g_list[1] = List();
        g_side = kind == Kind::Final ? Yours : Theirs;
        g_mends.clear(), g_mendSel = 0, g_quotes.clear();
        g_buyStop = kind == Kind::Final; // (the focus opens on BUY SHIP, as the pad's way always did: its hold needs a fresh press)
    }
    g_kind = kind, g_dealer = d;
    if (kind != Kind::Ships)
        HoldWheel(false);
    if (kind == Kind::None)
        return;
    const int wheelNow = kind == Kind::Repair || kind == Kind::Prices ? (hostcalls::g.RealMouseWheel ? hostcalls::g.RealMouseWheel() : 0) : 0;
    if (kind == Kind::Repair || kind == Kind::Prices)
    {
        if (!room::NNWindowBusy())
        {
            Away(d);
            if (kind == Kind::Prices)
                PlaceAt(uiclick::FindVisible("CommodityByBaseCloseButton"), -winplates::g_W, 0);
            if ((g_read -= dt) <= 0)
            {
                g_read = 0.2f;
                if (kind == Kind::Repair)
                    ReadMends();
                else
                    ReadQuotes();
            }
        }
        if (kind == Kind::Repair)
            g_cardFirst -= wheelNow;
        else
            g_list[0].Scroll(-wheelNow);
        return;
    }
    if (kind == Kind::Jobs || kind == Kind::News)
    {
        g_tick += dt * 0.06f * winplates::g_H;
        if (!room::NNWindowBusy())
        {
            BoardPlace(Bounds());
            if (BoardSel() < 0 && BoardBox(0))
                BoardPick(0);
        }
        const int wheel = hostcalls::g.RealMouseWheel ? hostcalls::g.RealMouseWheel() : 0;
        if (wheel)
            TextScroll(-0.08f * winplates::g_H * wheel);
        return;
    }
    if (kind == Kind::Ships)
    {
        g_ship = d;
        if (!room::NNWindowBusy())
        {
            Away(d);
            ViewBack(uiclick::Find("ShipTrader3DShip"));
            if ((g_read -= dt) <= 0)
            {
                g_read = 0.4f;
                ReadShips();
                if (g_shipSel < 0 && !g_offers.empty())
                    ShipPick(0);
            }
        }
        // the wheel over the ship is the game's 3D view's (its zoom), anywhere else the card's - and kept from the view: it zoomed as the card scrolled (issue 41)
        const float H = winplates::g_H, W = winplates::g_W, mx = static_cast<float>(fl::MouseX()), my = static_cast<float>(fl::MouseY());
        const bool overShip = fabsf(mx - W / 2) <= kShipHoleHalfW * H && my >= kShipHoleTop * H && my <= kShipHoleFoot * H;
        HoldWheel(!overShip);
        const int wheel = hostcalls::g.RealMouseWheel ? hostcalls::g.RealMouseWheel() : 0;
        if (!overShip)
            g_cardFirst -= wheel;
        return;
    }
    g_inv = uiclick::FindVisible("NN_Inventory");
    if (!room::NNWindowBusy())
    {
        Away(g_dealer);
        Away(g_inv);
    }
    if (g_tradeWait > 0 && (g_tradeWait -= dt) <= 0)
        g_read = 0;
    // the rows wait out the slide, then read at once: a read spent during the slide left the page without rows for up to 0.75 s over the game's window
    // (the owner: "frame-perfect", 7 Nov 2026)
    static bool s_busy = false;
    const bool busy = room::NNWindowBusy();
    if (s_busy && !busy)
        g_read = 0;
    s_busy = busy;
    if (!busy && (g_read -= dt) <= 0)
    {
        g_read = 0.75f;
        for (int i = 0; i < 5; ++i)
            if (g_filterWord[i].empty())
            {
                // the game's words, the last one off where the four must stand in one list's width ("EXTERNAL EQUIPMENT")
                g_filterWord[i] = Upper(baseinfo::HintText(uiclick::Find(kFilterControl[i])));
                const size_t sp = g_filterWord[i].find(L' ');
                if (sp != std::wstring::npos)
                    g_filterWord[i].resize(sp);
                if (g_filterWord[i].size() > 8)
                    g_filterWord[i].resize(4), g_filterWord[i] += L".";
                if (g_filterWord[i].empty())
                {
                    static const wchar_t* const kWords[] = {L"WEAPONS", L"AMMO", L"EXTERNAL", L"INTERNAL", L"GOODS"};
                    g_filterWord[i] = kWords[i];
                }
            }
        if (!room::NNWindowBusy())
        {
            ReadRows(Yours);
            ReadTheirs();
            if (g_buyStop && !CanBuyFinal())
                g_buyStop = false;
            if (OnRow() && !Selected() && Focus()->row >= 0) // (a sale clears the game's selection a moment later: the page's row stands)
                Select();
        }
    }
    // the game's lists stand where the page's do: their five icons are the page's five
    for (int side = 0; side < 2; ++side)
        if (!room::NNWindowBusy() && g_list[side].first < static_cast<int>(g_rows[side].size()) && g_rows[side][g_list[side].first].row >= 0)
            SyncList(WindowOf(side), g_rows[side][g_list[side].first].row);
    const int wheel = hostcalls::g.RealMouseWheel ? hostcalls::g.RealMouseWheel() : 0;
    if (wheel && g_wheelSide >= 0)
        g_list[static_cast<int>(g_wheelSide)].Scroll(-wheel);
    else if (wheel && g_wheelSide == -1)
        g_cardFirst -= wheel;
}

void Draw()
{
    if (g_kind == Kind::None)
    {
        g_holes = Holes();
        return;
    }
    baseinfo::Refresh(); // (the pad's close runs between the page's Update and this: the list must be this moment's)
    const Frame f = Bounds();
    const float lane = deckmode::LaneTopH() * f.H;
    winplates::g_keep.push_back({f.left, lane, f.left + 0.02f * f.H, lane + 0.02f * f.H});
    if (KindNow() != g_kind)
        return;
    if (g_kind == Kind::Jobs || g_kind == Kind::News)
        DrawBoard(f);
    else if (g_kind == Kind::Ships)
        DrawShips(f);
    else if (g_kind == Kind::Repair)
        DrawRepair(f);
    else if (g_kind == Kind::Prices)
        DrawPrices(f);
    else
        DrawDealer(f);
}
void DrawAfter() {}
// (under the game's UI: the icons' cells stand on the page's ground, not on the room)
void DrawUnder()
{
    for (int i = 0; i < g_holes.n; ++i)
        GroundBut(g_holes.r[i][0], g_holes.r[i][1], g_holes.r[i][2], g_holes.r[i][3], g_kind == Kind::Jobs || g_kind == Kind::News ? g_cut : Holes());
}
bool On() { return g_kind != Kind::None; }
bool Hides() { return g_kind != Kind::None; }

static float g_aHeld = 0, g_yHeld = 0;
static bool g_aFired = false, g_yFired = false;

int Prompts(views::PagePrompt* out, int max)
{
    using namespace glyphs;
    int n = 0;
    auto add = [&](int id, const wchar_t* word, int id2 = -1, bool hold = false, float progress = 0)
    {
        if (n < max)
            out[n++] = {id, id2, word, hold, progress};
    };
    if (g_kind == Kind::Jobs || g_kind == Kind::News)
    {
        // one side's words, only the D-pad direction that goes over (the dealers' rule)
        if (!BoardTextSide())
        {
            add(DU, g_kind == Kind::Jobs ? L"JOB" : L"STORY", DD);
            if (TextBar())
                add(DR, L"READ");
        }
        else
        {
            add(DL, g_kind == Kind::Jobs ? L"JOBS" : L"STORIES");
            add(RS, L"SCROLL");
        }
        if (g_kind == Kind::Jobs && uiclick::FindVisible("MissionVendorAcceptButton"))
            add(A, L"ACCEPT", -1, true, g_aFired ? 0 : std::min(1.0f, g_aHeld / kHoldSec));
        return n;
    }
    if (g_kind == Kind::Ships)
    {
        // one side's words at a time, and only the D-pad direction that goes over (the dealers' rule)
        if (g_shipFocus == 0)
        {
            add(DU, L"SHIP", DD);
            if (g_shipSel >= 0)
                add(DR, L"VIEW SHIP");
        }
        else
        {
            add(DL, L"SHIP LIST");
            add(RS, L"TURN");
            if (g_cardLines > g_cardRoom)
                add(DU, L"SCROLL CARD", DD);
        }
        if (ShipZoomable())
            add(LT, L"ZOOM", RT);
        if (CanBuyShip())
            add(A, L"BUY THIS SHIP", -1, true, g_aFired ? 0 : std::min(1.0f, g_aHeld / kHoldSec));
        return n;
    }
    const float progress = g_aFired ? 0 : std::min(1.0f, g_aHeld / kHoldSec);
    if (g_kind == Kind::Repair)
    {
        if (g_mends.size() > 1)
            add(DU, L"ITEM", DD);
        if (!g_mends.empty())
        {
            add(A, g_mends[g_mendSel].ticked ? L"UNTICK" : L"TICK");
            add(X, L"TICK ALL / NONE");
        }
        if (CanMend())
            add(Y, L"REPAIR", -1, true, g_yFired ? 0 : std::min(1.0f, g_yHeld / kHoldSec));
        if (g_cardLines > g_cardRoom)
            add(RS, L"SCROLL CARD");
        return n;
    }
    if (g_kind == Kind::Prices)
    {
        if (g_list[0].count > g_list[0].rows)
            add(DU, L"SCROLL", DD);
        return n;
    }
    const Row* r = Focus();
    if (Final() && g_buyStop)
    {
        add(DL, L"YOUR ITEMS", DR);
        add(A, L"BUY SHIP", -1, true, progress);
    }
    else
    {
        add(DU, L"ITEM", DD);
        // only the direction that goes over (the owner, 7 Nov 2026): your ship stands left, the dealer right
        add(g_side == Yours ? DR : DL, Final() ? (CanBuyFinal() ? L"BUY SHIP" : g_side == Yours ? L"DEALER" : L"NEW SHIP") : g_side == Yours ? L"DEALER" : L"YOUR SHIP");
        if (CanTrade())
            add(A, Final() ? (g_side == Yours ? L"SELL TO DEALER" : L"TAKE BACK") : g_side == Yours ? L"SELL" : L"BUY", -1, true, progress);
        if (CanTrade() && Slider())
        {
            add(LT, L"QUANTITY", RT);
            add(Y, L"MAX");
        }
    }
    if (g_kind == Kind::Equipment || Final())
        add(X, L"FILTER");
    if (g_kind == Kind::Commodity && uiclick::FindVisible("TradeRouteButton"))
        add(X, L"BEST PRICES");
    if (Mountable(r))
        add(LS, r->mounted ? L"UNMOUNT" : L"MOUNT");
    if (CanTag())
        add(R3, Tagged() ? L"CLICK UNTAG" : L"CLICK COMPARE WITH");
    if (Final() && uiclick::FindVisible("BackToShipDealer"))
        add(B, L"BACK TO SHIPS");
    return n;
}

unsigned Pad(unsigned pressed, unsigned held, float dt)
{
    if (g_kind == Kind::None)
        return 0;
    static float repeat = 0;
    const unsigned walk = XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_DPAD_DOWN;
    if (held & walk)
    {
        repeat += dt;
        if (repeat > 0.4f)
            pressed |= held & walk, repeat -= 0.08f;
    }
    else
        repeat = 0;
    const unsigned page = XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_DPAD_DOWN | XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_DPAD_RIGHT | XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_X | XINPUT_GAMEPAD_Y;
    if (g_kind == Kind::Repair)
    {
        const int n = static_cast<int>(g_mends.size());
        if ((pressed & XINPUT_GAMEPAD_DPAD_UP) && g_mendSel > 0)
            --g_mendSel, g_cardFirst = 0;
        if ((pressed & XINPUT_GAMEPAD_DPAD_DOWN) && g_mendSel + 1 < n)
            ++g_mendSel, g_cardFirst = 0;
        if (pressed & XINPUT_GAMEPAD_A)
            MendTick();
        if (pressed & XINPUT_GAMEPAD_X)
            MendAll();
        // a repair spends: a hold, with a fresh press (dealer_rules.h); A only ticks
        const bool hold = (held & XINPUT_GAMEPAD_Y) != 0;
        g_yHeld = hold ? g_yHeld + dt : 0;
        if (!hold)
            g_yFired = false;
        if (hold && !g_yFired && g_yHeld >= kHoldSec)
            g_yFired = true, MendGo();
        g_cardFirst += (held & views::kPagePadRsDown) && g_cardFirst < g_cardLines ? 1 : 0;
        g_cardFirst -= (held & views::kPagePadRsUp) ? 1 : 0;
        return page;
    }
    if (g_kind == Kind::Prices)
    {
        if (pressed & XINPUT_GAMEPAD_DPAD_UP)
            g_list[0].Scroll(-1);
        if (pressed & XINPUT_GAMEPAD_DPAD_DOWN)
            g_list[0].Scroll(1);
        return page;
    }
    if (g_kind == Kind::Jobs || g_kind == Kind::News)
    {
        // two sides (issue 45, the owner): D-pad left the list - the stick and D-pad up / down step it; D-pad right the text, while it overflows - the stick
        // and D-pad up / down scroll it
        const bool canText = TextBar() != 0;
        if (pressed & XINPUT_GAMEPAD_DPAD_LEFT)
            g_boardSide = 0;
        if ((pressed & XINPUT_GAMEPAD_DPAD_RIGHT) && canText)
            g_boardSide = 1;
        const int rs = (held & views::kPagePadRsUp) ? -1 : (held & views::kPagePadRsDown) ? 1 : 0;
        if (!BoardTextSide())
        {
            int step = (pressed & XINPUT_GAMEPAD_DPAD_UP) ? -1 : (pressed & XINPUT_GAMEPAD_DPAD_DOWN) ? 1 : 0;
            if (rs != g_rsWas)
                step = rs, g_rsRepeat = 0.35f;
            else if (rs && (g_rsRepeat -= dt) <= 0)
                step = rs, g_rsRepeat = 0.12f;
            if (step)
                BoardStep(step);
            g_textAcc = 0;
        }
        else
        {
            const float H = winplates::g_H;
            if (pressed & (XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_DPAD_DOWN))
                TextScroll(((pressed & XINPUT_GAMEPAD_DPAD_UP) ? -0.11f : 0.11f) * H);
            g_textAcc = rs ? g_textAcc + rs * 0.9f * H * dt : 0;
            if (fabsf(g_textAcc) >= 0.02f * H)
                TextScroll(g_textAcc), g_textAcc = 0;
        }
        g_rsWas = rs;
        const bool hold = (held & XINPUT_GAMEPAD_A) != 0;
        g_aHeld = hold ? g_aHeld + dt : 0;
        if (!hold)
            g_aFired = false;
        if (hold && !g_aFired && g_aHeld >= kHoldSec && g_kind == Kind::Jobs)
            g_aFired = true, BoardAccept();
        return XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_DPAD_DOWN | XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_DPAD_RIGHT | XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_X | XINPUT_GAMEPAD_Y;
    }
    if (g_kind == Kind::Ships)
    {
        // two sides, as the dealers' (the owner, 7 Nov 2026, issue 41): D-pad left the list, where the stick and D-pad up / down step the ships; D-pad right
        // the ship, where the stick turns it and D-pad up / down scroll its card. The triggers zoom it on either side (padbases)
        if (pressed & XINPUT_GAMEPAD_DPAD_LEFT)
            g_shipFocus = 0;
        if ((pressed & XINPUT_GAMEPAD_DPAD_RIGHT) && g_shipSel >= 0)
            g_shipFocus = 1;
        const int n = static_cast<int>(g_offers.size());
        const int rs = (held & views::kPagePadRsUp) ? -1 : (held & views::kPagePadRsDown) ? 1 : 0;
        if (g_shipFocus == 0)
        {
            int step = (pressed & XINPUT_GAMEPAD_DPAD_UP) ? -1 : (pressed & XINPUT_GAMEPAD_DPAD_DOWN) ? 1 : 0;
            if (rs != g_rsWas) // (a ship a push, then repeats while it is held, as a list's stick does)
                step = rs, g_rsRepeat = 0.35f;
            else if (rs && (g_rsRepeat -= dt) <= 0)
                step = rs, g_rsRepeat = 0.12f;
            if (step < 0 && g_shipSel > 0)
                ShipPick(g_shipSel - 1);
            if (step > 0 && g_shipSel + 1 < n)
                ShipPick(g_shipSel + 1);
        }
        else
        {
            if (pressed & XINPUT_GAMEPAD_DPAD_DOWN)
                g_cardFirst = std::min(g_cardFirst + 3, g_cardLines);
            if (pressed & XINPUT_GAMEPAD_DPAD_UP)
                g_cardFirst = std::max(g_cardFirst - 3, 0);
        }
        float raw[6] = {};
        u32 buttons = 0;
        if (views::padflight.Raw)
            views::padflight.Raw(raw, &buttons);
        if (g_shipFocus == 1)
            ShipTurn(raw[2], raw[3], dt);
        ShipZoom(raw[4], raw[5], dt);
        g_rsWas = rs;
        const bool hold = (held & XINPUT_GAMEPAD_A) != 0;
        g_aHeld = hold ? g_aHeld + dt : 0;
        if (!hold)
            g_aFired = false;
        if (hold && !g_aFired && g_aHeld >= kHoldSec && CanBuyShip())
            g_aFired = true, ShipBuy();
        return XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_DPAD_DOWN | XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_DPAD_RIGHT | XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_X | XINPUT_GAMEPAD_Y;
    }
    if (Final())
    {
        // left and right walk the three stops as they stand: the new ship, BUY SHIP, the dealer (dealer_rules.h NextStop, the pad's way before the page)
        const int was = g_buyStop ? 1 : g_side == Yours ? 0 : 2, by = (pressed & (XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_LEFT_SHOULDER)) ? -1 : (pressed & (XINPUT_GAMEPAD_DPAD_RIGHT | XINPUT_GAMEPAD_RIGHT_SHOULDER)) ? 1 : 0;
        int to = std::max(0, std::min(2, was + by));
        if (to == 1 && !CanBuyFinal())
            to = was + 2 * by;
        if (by && to != was && to >= 0 && to <= 2)
        {
            g_buyStop = to == 1;
            if (to != 1)
                g_side = to == 0 ? Yours : Theirs, Moved();
        }
    }
    if (!g_buyStop)
    {
        if (pressed & XINPUT_GAMEPAD_DPAD_UP)
            if (g_list[g_side].Step(-1))
                Moved();
        if (pressed & XINPUT_GAMEPAD_DPAD_DOWN)
            if (g_list[g_side].Step(1))
                Moved();
    }
    if (!Final() && (pressed & (XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_LEFT_SHOULDER)) && g_side != Yours)
        g_side = Yours, Moved();
    if (!Final() && (pressed & (XINPUT_GAMEPAD_DPAD_RIGHT | XINPUT_GAMEPAD_RIGHT_SHOULDER)) && g_side != Theirs)
        g_side = Theirs, Moved();
    if ((pressed & XINPUT_GAMEPAD_X) && (g_kind == Kind::Equipment || Final()))
        Filter((NowFilter() + 1) % Filters());
    if ((pressed & XINPUT_GAMEPAD_X) && g_kind == Kind::Commodity)
        ChipClick(kRoutes);
    if ((pressed & XINPUT_GAMEPAD_Y) && !g_buyStop)
        Quantity(1000);
    if (pressed & XINPUT_GAMEPAD_LEFT_THUMB)
        Mount();
    if (pressed & XINPUT_GAMEPAD_RIGHT_THUMB) // RS's click on a mounted item: what the store's items compare with; again: back to the weakest that would go
        ToggleTag();
    // a held A trades, as a job or a ship is taken (dealer_rules.h): a tap on a row never spends
    const bool down = (held & XINPUT_GAMEPAD_A) != 0;
    g_aHeld = down ? g_aHeld + dt : 0;
    if (!down)
        g_aFired = false;
    if (down && !g_aFired && g_aHeld >= kHoldSec && g_buyStop)
        g_aFired = true, FinalBuy();
    else if (down && !g_aFired && g_aHeld >= kHoldSec && CanTrade())
        g_aFired = true, Trade();
    // the right stick walks the card
    g_cardStep += ((held & views::kPagePadRsDown) ? dt : (held & views::kPagePadRsUp) ? -dt : 0) * 8;
    if (!(held & (views::kPagePadRsDown | views::kPagePadRsUp)))
        g_cardStep = 0;
    if (fabsf(g_cardStep) >= 1)
    {
        const int lines = static_cast<int>(g_cardStep);
        g_cardFirst += lines, g_cardStep -= static_cast<float>(lines);
    }
    return XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_DPAD_DOWN | XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_DPAD_RIGHT | XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_X | XINPUT_GAMEPAD_Y | XINPUT_GAMEPAD_LEFT_SHOULDER | XINPUT_GAMEPAD_RIGHT_SHOULDER | XINPUT_GAMEPAD_LEFT_THUMB | XINPUT_GAMEPAD_RIGHT_THUMB;
}

void Shutdown()
{
    HoldWheel(false);
    g_kind = Kind::None;
    g_dealer = g_inv = 0;
}

std::string Command(std::istream& in)
{
    std::string what;
    in >> what;
    if (what == "press")
    {
        std::string b;
        in >> b;
        const struct
        {
            const char* n;
            unsigned m;
        } kMap[] = {{"x", XINPUT_GAMEPAD_X}, {"y", XINPUT_GAMEPAD_Y}, {"up", XINPUT_GAMEPAD_DPAD_UP}, {"down", XINPUT_GAMEPAD_DPAD_DOWN}, {"left", XINPUT_GAMEPAD_DPAD_LEFT}, {"right", XINPUT_GAMEPAD_DPAD_RIGHT}, {"ls", XINPUT_GAMEPAD_LEFT_THUMB}};
        for (const auto& k : kMap)
            if (b == k.n)
                return "basepage: took 0x" + std::to_string(Pad(k.m, 0, 0));
        if (b == "trade")
        {
            Trade();
            return "basepage: traded";
        }
        if (b == "a")
        {
            Pad(XINPUT_GAMEPAD_A, 0, 0);
            return "basepage: a";
        }
        return "basepage press <a|x|y|up|down|left|right|ls|trade>";
    }
    std::ostringstream o;
    if (g_kind == Kind::Repair)
    {
        o << "basepage repair sel=" << g_mendSel << " can=" << CanMend() << " total=" << text::Utf8(TextIn("RepairTotalCost"));
        for (const Mend& m : g_mends)
            o << " [" << m.slot << " " << text::Utf8(m.name) << " $" << m.price << " tick=" << m.ticked << " health=" << m.health << " arch=" << m.arch << "]";
        return o.str();
    }
    if (g_kind == Kind::Prices)
    {
        o << "basepage prices n=" << g_quotes.size();
        for (const nnrules::Quote& q : g_quotes)
            o << " [" << text::Utf8(q.base) << " $" << q.price << " " << text::Utf8(q.sells) << "]";
        return o.str();
    }
    if (g_kind == Kind::Final)
        o << "basepage final stop=" << g_buyStop << " can=" << CanBuyFinal() << " back=" << (uiclick::FindVisible("BackToShipDealer") != 0) << " why=" << text::Utf8(TextIn("ShipBuyProblem")) << " tradein=" << static_cast<int>(R32(g_dealer + 0x9FC)) << " ";
    if (g_kind == Kind::Ships)
    {
        o << "basepage ships sel=" << g_shipSel << " can=" << CanBuyShip() << " side=" << g_shipFocus << " zoomable=" << ShipZoomable();
        if (const u32 v = ShipViewCtl())
        {
            float f[3] = {};
            safemem::Read(v + 0x3EC, &f[0], 4), safemem::Read(v + 0x520, &f[1], 8);
            o << " zoom=" << f[0] << " yaw=" << f[1] << " pitch=" << f[2];
        }
        for (const Offer& s : g_offers)
            o << " [" << text::Utf8(s.name) << " $" << s.price << " ref=" << s.refusal << " lvl=" << s.level << " facts=" << gamedata::ShipNamed(s.name).ok << "]";
        return o.str();
    }
    if (g_kind == Kind::Jobs || g_kind == Kind::News)
    {
        o << "basepage board sel=" << BoardSel() << " bar=" << TextBar() << " side=" << g_boardSide;
        if (const u32 bar = TextBar())
            o << " textAt=" << uiclick::Scroll(bar, 0, false, nullptr, nullptr);
        return o.str();
    }
    o << "basepage kind=" << static_cast<int>(g_kind) << " side=" << g_side << " yours=" << g_rows[Yours].size() << " theirs=" << g_rows[Theirs].size() << " sel=" << g_list[g_side].sel;
    if (g_kind != Kind::None)
    {
        o << " filter=" << NowFilter() << " rowY=" << static_cast<int>(R32(g_inv + kRow)) << " rowT=" << static_cast<int>(R32(g_dealer + kRow)) << " canTrade=" << CanTrade() << " qty=" << Qty() << "/" << QtyMost() << " credits=" << baseinfo::Credits();
        if (const Row* r = Focus())
        {
            o << " item=" << text::Utf8(r->name) << " card=" << g_cardText.size() << " mounted=" << r->mounted << " tagged=" << gamedata::TaggedCompare();
            if (!r->mounted)
            {
                const gamedata::Versus v = gamedata::Against(r->arch);
                o << " vs=" << text::Utf8(v.arch ? hudinfo::EquipName(v.arch) : std::wstring(L"-")) << " vsTagged=" << v.tagged << " free=" << v.freeHp;
            }
        }
    }
    return o.str();
}
} // namespace basepage
