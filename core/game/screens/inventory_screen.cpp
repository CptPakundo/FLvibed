// The INVENTORY page's reads and actions: inventory_screen.h. nnpage.cpp's ReadItems, SelectItem, SyncNow, SetFilter,
// ToggleMount, Jettison, Trade, the jettison panel's slider and buttons and the INVENTORY branch of its Update, moved unchanged
// (audit 70b F4 unit b2); its statics are the one Kept here, its clocks still sums of the frames' dt. The slider's four
// borrowed scrolls (the chips', the bar's, the wheel's) are one now (Slide).
#include "inventory_screen.h"

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
#include "log.h"
#include "safemem.h"

#include <algorithm>
#include <cwchar>
#include <cwctype>

namespace inventoryscreen
{
// The window's own fields (docs\research\neuronet-windows-facts.md)
constexpr u32 kInvFilter = 0x32C, kInvRow = 0x3A4, kInvFirst = 0x3C4, kInvDesc = 0x4F4; // the filter, the selected row, the list's first row, the row's EquipDesc
constexpr u32 kJetQty = 0x93C, kJetMost = 0x940;                                         // the jettison panel's count of how many

static Kept g_kept; // (the page's statics before: never reset but by Reset)
static u32 g_root = 0; // the window the page stands over (the actions' and the live answers')
static const char* const kFilterControl[5] = {"Filter_Weapons", "Filter_Ammo", "Filter_Equipment", "Filter_CargoBay", "Filter_Commodity"};
constexpr int kFilterCommodities = 4;

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

void Open(Model& m) { m.k = &g_kept; }
const Kept& State() { return g_kept; }
void Reset()
{
    g_kept.itemRead = 0;
    g_kept.itemSelected = false;
    g_kept.list = screens::List();
    g_kept.syncPending = false;
}
void Clock(float dt) { g_kept.stepAge += dt, g_kept.syncAge += dt; }

// The game's own refresh of a step - the selection (the card it shows), as STATUS's faction card (status_screen.cpp Follow) - is not per step: each new card
// is laid out and measured again (windowtext: up to 6.4 ms a step on this PC, issue 68 - a Deck several times that), which a held or tapped walk paid at 20 steps a
// second. The page's own focus moves at once; the game follows a lone tap at once too, a walk when it rests (0.1 s) or every 0.25 s. Anything that acts on the game's
// selection (mount, jettison, best prices) brings it up to date first (SyncNow).
static bool SyncDue() { return g_kept.stepAge >= 0.10f || g_kept.syncAge >= 0.25f; }

// A gun's row says what it does, its second line in the game's words for the card's numbers: the slot's class stood there in the game's window.
static const std::wstring& GunLine(u32 arch)
{
    const auto it = g_kept.gunLine.find(arch);
    if (it != g_kept.gunLine.end())
        return it->second;
    baseinfo::Gun g;
    std::wstring line;
    if (baseinfo::GunOf(arch, g) && g.hullDps >= 0)
    {
        wchar_t b[96];
        swprintf_s(b, L"HULL %.0f/S   SHIELD %.0f/S   %.0f M", g.hullDps, g.shieldDps, g.range);
        line = b;
    }
    return g_kept.gunLine[arch] = line;
}
static void ReadLoadout()
{
    Loadout& l = g_kept.load;
    l = Loadout();
    for (const baseinfo::Item& it : baseinfo::Equipment())
    {
        baseinfo::Gun g;
        if (!it.mounted || !baseinfo::GunOf(it.arch, g) || g.hullDps < 0)
            continue;
        ++l.guns, l.hull += g.hullDps, l.shield += std::max(0.0f, g.shieldDps), l.energy += std::max(0.0f, g.energyPs);
    }
}

int FilterOf(u32 root) { return static_cast<int>(R32(root + kInvFilter)); }
// Mounting is a dock's: the game's own window hides its mount toggles (EquipToggle1..5) in space and shows them docked (issue 79, its dumps).
static bool Mountable(int filter) { return fl::PlayerBase() && (filter == 0 || filter == 2 || filter == 3); }

// The window's own answer for each of its rows (0x47E790, its filter and order), asked row by row: the game's five rows are five of these. Its selection goes back after.
static void ReadItems(u32 root)
{
    perf::Scope ps("nn.ReadItems");
    if (!exe::InvSelected.ok())
        return;
    std::vector<Item>& have = g_kept.items;
    screens::List& list = g_kept.list;
    const u32 row = R32(root + kInvRow), desc = R32(root + kInvDesc);
    const u16 keep = list.sel < static_cast<int>(have.size()) ? have[list.sel].id : 0;
    std::vector<Item> items;
    int nothing = 0;
    for (int k = 0; k < 96 && nothing < 16; ++k)
    {
        W32(root + kInvRow, static_cast<u32>(k));
        W32(root + kInvDesc, 0);
        const u32 d = hk::Call<u32(__fastcall*)(u32)>(exe::InvSelected, root);
        if (!d)
        {
            ++nothing;
            continue;
        }
        nothing = 0;
        Item it;
        it.desc = d, it.row = k;
        safemem::Read(d + 2, &it.id, 2);
        it.arch = R32(d + 4);
        u8 m = 0;
        safemem::Read(d + 0xC, &m, 1);
        it.mounted = m != 0;
        it.health = RF(d + 0x10);
        it.count = static_cast<int>(R32(d + 0x14));
        it.name = hudinfo::EquipName(it.arch);
        if (!it.name.empty())
            items.push_back(it);
    }
    W32(root + kInvRow, row);
    W32(root + kInvDesc, desc);
    ReadLoadout();
    const int filter = FilterOf(root);
    g_kept.free.clear();
    if (filter == 0 || filter == 2) // every slot in its own place, EMPTY where nothing is mounted (issue 52)
        items = screens::InSlotOrder(items, filter, g_kept.free, [](const Item& it) { return it.mounted ? screens::DescHardpoint(it.desc) : std::string(); },
                                     [](int i)
                                     {
                                         Item it;
                                         it.hp = i, it.row = -1;
                                         it.name = L"EMPTY";
                                         return it;
                                     });
    have.swap(items);
    list.count = static_cast<int>(have.size());
    for (int i = 0; keep && i < list.count; ++i)
        if (have[i].id == keep)
            list.sel = i;
    list.Fit();
}
// The game's selection follows the page's: its card, its JETTISON.
static void SelectItem(u32 root)
{
    const screens::List& list = g_kept.list;
    if (list.count <= 0 || !exe::InvSelectRow.ok() || g_kept.items[list.sel].hp >= 0)
        return;
    const int first = static_cast<int>(R32(root + kInvFirst));
    perf::Scope ps("nn.InvSelect");
    hk::Call<void(__thiscall*)(u32, int, int)>(exe::InvSelectRow, root, g_kept.items[list.sel].row - first, 1);
    g_kept.itemSelected = true;
}
// The game's selection brought up to the page's focus now (a deferred step: SyncDue), before anything that acts on it.
static void SyncNow(u32 root)
{
    if (!g_kept.syncPending)
        return;
    g_kept.syncPending = false, g_kept.syncAge = 0;
    SelectItem(root);
}
static void SetFilter(u32 root, int filter)
{
    g_kept.syncPending = false; // (a new filter starts its own selection)
    if (!exe::InvSetFilter.ok())
        return;
    hk::Call<void(__thiscall*)(u32, int, int)>(exe::InvSetFilter, root, filter, 1);
    screens::List& list = g_kept.list;
    list.sel = 0, list.first = 0, g_kept.itemRead = 0, g_kept.itemSelected = false;
}
static bool OnEmpty()
{
    const screens::List& list = g_kept.list;
    return list.count > 0 && list.sel < static_cast<int>(g_kept.items.size()) && g_kept.items[list.sel].hp >= 0;
}
// What an empty hardpoint's A would mount: the first row of the hold it takes, null none.
static const Item* ForEmpty()
{
    if (!OnEmpty())
        return nullptr;
    const std::vector<u32> fit = gamedata::HoldItemsFor(g_kept.free[g_kept.items[g_kept.list.sel].hp]);
    for (const Item& it : g_kept.items)
        if (it.hp < 0 && !it.mounted && std::find(fit.begin(), fit.end(), it.arch) != fit.end())
            return &it;
    return nullptr;
}
static void ToggleMount(u32 root)
{
    SyncNow(root);
    const screens::List& list = g_kept.list;
    if (list.count <= 0 || !exe::InvMount.ok() || !Mountable(FilterOf(root)))
        return;
    if (OnEmpty())
    {
        if (const Item* it = ForEmpty())
        {
            hk::Call<void(__thiscall*)(u32, u32, int)>(exe::InvMount, root, it->desc, 0);
            g_kept.itemRead = 0.12f;
        }
        return;
    }
    const Item& it = g_kept.items[list.sel];
    hk::Call<void(__thiscall*)(u32, u32, int)>(exe::InvMount, root, it.desc, it.mounted ? 1 : 0);
    g_kept.itemRead = 0.12f; // (the list moves a moment later)
}
// A stack opens the game's panel, which asks how many. A single item goes at the game's first press: the page asks once more (XAG 115: review before a loss, by a press, never a hold).
static bool Jettison(u32 root)
{
    SyncNow(root);
    const u32 c = uiclick::FindVisible("JettisonButton");
    if (!c || g_kept.list.count <= 0 || OnEmpty())
        return false;
    const Item& it = g_kept.items[g_kept.list.sel];
    if (it.count <= 1 && g_kept.jettisonArmed != it.id)
    {
        g_kept.jettisonArmed = it.id;
        return true;
    }
    g_kept.jettisonArmed = 0;
    return uiclick::Click(c);
}
// The game's commodity list by base: where the selected good sells (it opens over the window; only a commodity's row has it). The game's button stays hidden: its
// TRADE ROUTES label stood in the card's text, and neither moving it nor its local position took (the game lays it out again; 7 Nov 2026). The page offers the list on
// a commodity's row (filter 4, as the game shows the button) and clicks the hidden button: the click runs its action, not a hit test.
bool TradeOffered(u32 root) { return root && FilterOf(root) == kFilterCommodities && g_kept.list.count > 0 && !OnEmpty() && uiclick::Find("TradeRouteButton"); }
static bool Trade(u32 root)
{
    SyncNow(root);
    const u32 c = uiclick::Find("TradeRouteButton");
    return c && TradeOffered(root) && uiclick::Click(c);
}
bool JettisonOpen() { return uiclick::FindVisible("JettisonAcceptButton") != 0; }
// The jettison panel's count, `by` notches on the game's own slider (the chips', the bar's and the wheel's one way).
static bool Slide(float by)
{
    uiclick::Scroll(uiclick::Find("JettisonQuantitySlider"), by, false, nullptr, nullptr);
    return true;
}

bool CanMount(u32 root) { return g_kept.list.count > 0 && Mountable(FilterOf(root)) && (!OnEmpty() || ForEmpty()); }
bool JettisonShown() { return uiclick::FindVisible("JettisonButton") && !OnEmpty(); }
// The compare tag (issue 44, as the equipment dealer's: Shockwave, the inventory tab too): RS's click or the card's chip on a mounted item makes it what the
// items in the hold compare with (gamedata::TagCompare: one tag for both pages). (Asked only while the page stands: its prompts, pad, clicks and draw.)
bool CanTag()
{
    const screens::List& list = g_kept.list;
    if (list.count <= 0 || list.sel >= static_cast<int>(g_kept.items.size()) || OnEmpty())
        return false;
    const Item& it = g_kept.items[list.sel];
    const equiprules::Spec* s = gamedata::Equip(it.arch);
    return it.mounted && it.hp < 0 && s && !s->stats.empty();
}
bool Tagged() { return CanTag() && g_kept.items[g_kept.list.sel].arch == gamedata::TaggedCompare(); }
void ToggleTag()
{
    if (CanTag())
        gamedata::TagCompare(Tagged() ? 0 : g_kept.items[g_kept.list.sel].arch);
}
u16 ShipViewPart()
{
    const screens::List& list = g_kept.list;
    return list.count > 0 && list.sel < static_cast<int>(g_kept.items.size()) && g_kept.items[list.sel].mounted ? g_kept.items[list.sel].id : 0;
}

void Update(Model& m, u32 root, float dt)
{
    m.root = g_root = root;
    screens::List& list = g_kept.list;
    if (g_kept.syncPending && SyncDue() && !room::NNWindowBusy())
        SyncNow(root); // (the game's selection catches up with a walk: SyncDue)
    // the game's list stands where the page's does: its five icons are the page's (RowIcons lends one to a shown row outside them)
    if (!room::NNWindowBusy())
    {
        int shown[screens::kMeshes], n = 0;
        for (; n < screens::kMeshes && list.first + n < static_cast<int>(g_kept.items.size()); ++n)
            shown[n] = g_kept.items[list.first + n].row;
        screens::SyncList(root, shown, n);
    }
    if ((g_kept.itemRead -= dt) <= 0)
    {
        ReadItems(root), g_kept.itemRead = 0.5f;
        for (int i = 0; i < 5; ++i)
            if (g_kept.filterWord[i].empty())
            {
                std::wstring& w = g_kept.filterWord[i];
                w = baseinfo::HintText(uiclick::Find(kFilterControl[i]));
                if (w.empty())
                {
                    static const wchar_t* const kWords[] = {L"WEAPONS", L"AMMO", L"EQUIPMENT", L"CARGO BAY", L"COMMODITIES"};
                    w = kWords[i];
                }
                for (wchar_t& c : w)
                    c = towupper(c);
            }
        // (a filter or a mount clears or moves the game's selection a moment later: its card and its JETTISON follow the page's row)
        if (list.count > 0 && !room::NNWindowBusy() && !OnEmpty() && static_cast<int>(R32(root + kInvRow)) != g_kept.items[list.sel].row)
            SelectItem(root);
    }
}

void Prepare(Model& m, u32 root)
{
    m.root = g_root = root;
    m.visible = uiclick::FindVisible(kWindowName) != 0;
    m.jettison = JettisonOpen();
    m.qty = static_cast<int>(R32(root + kJetQty)), m.most = static_cast<int>(R32(root + kJetMost));
    if (!m.visible || m.jettison)
        return; // (the rest is the list's: the jettison panel draws the item and its count only)
    m.filter = FilterOf(root);
    m.empty = OnEmpty();
    m.fits.clear();
    if (m.empty)
        for (u32 arch : gamedata::HoldItemsFor(g_kept.free[g_kept.items[g_kept.list.sel].hp]))
            m.fits.push_back(hudinfo::EquipName(arch));
    m.canMount = CanMount(root);
    m.jetButton = uiclick::FindVisible("JettisonButton") != 0;
    m.trade = TradeOffered(root);
    m.canTag = CanTag(), m.tagged = Tagged();
    m.taggedArch = gamedata::TaggedCompare();
    m.gunLine.resize(g_kept.items.size());
    for (size_t i = 0; i < g_kept.items.size(); ++i)
        m.gunLine[i] = GunLine(g_kept.items[i].arch);
    m.ship = baseinfo::ShipName();
    for (wchar_t& c : m.ship)
        c = towupper(c);
    m.inSpace = !fl::PlayerBase();
    m.shipViewOk = exe::FrameUpdate.ok();
    m.facts = ShipFacts();
    if (!m.inSpace)
    {
        const gamedata::ShipFacts s = gamedata::Ship(baseinfo::ShipArch());
        m.facts.ok = s.ok;
        m.facts.hull = s.hull, m.facts.hold = s.hold, m.facts.nanobots = s.nanobots, m.facts.batteries = s.batteries;
        m.facts.guns = s.guns, m.facts.turrets = s.turrets, m.facts.gunClass = s.gunClass, m.facts.shieldClass = s.shieldClass;
    }
    m.compare = g_kept.list.count > 0 && !m.empty && g_kept.list.sel < static_cast<int>(g_kept.items.size());
    if (m.compare)
        m.block = screens::ReadCompare(g_kept.items[g_kept.list.sel].arch, g_kept.items[g_kept.list.sel].mounted);
    m.credits = baseinfo::Credits();
    m.used = 0, m.cap = 0;
    m.cargo = baseinfo::Cargo(m.used, m.cap);
}

// ---- the actions: the window's own handlers and buttons
static bool Select(u32 root, int)
{
    SelectItem(root);
    return true;
}
static bool Filter(u32 root, int filter)
{
    SetFilter(root, filter);
    return true;
}
static bool Mount(u32 root, int)
{
    ToggleMount(root);
    return true;
}
static bool Throw(u32 root, int) { return Jettison(root); }
static bool Routes(u32 root, int) { return Trade(root); }
static bool SlideBy(u32, int notches) { return Slide(static_cast<float>(notches)); }
static bool Bar(u32 root, int cell)
{
    const int most = static_cast<int>(R32(root + kJetMost)), qty = static_cast<int>(R32(root + kJetQty));
    const int want = std::max(1, (most * (cell + 1) + kJetCells / 2) / kJetCells);
    return Slide(static_cast<float>(want - qty));
}
static bool Accept(u32, int) { return uiclick::Click(uiclick::Find("JettisonAcceptButton")); }
static bool Cancel(u32, int) { return uiclick::Click(uiclick::Find("JettisonCancelButton")); }

#define INVENTORY_ACTION(id, name, word, fn)                                                                                                    \
    {id, name, word, nullptr, nullptr, nullptr, fn, nullptr, screens::Route::Borrowed, screens::Route::Borrowed, nullptr, false, screens::Answer::Now, 0, \
     nullptr, nullptr}
const screens::ActionDesc kActions[] = {
    INVENTORY_ACTION(kActSelect, "select", L"SELECT", Select),
    INVENTORY_ACTION(kActFilter, "filter", L"FILTER", Filter),
    INVENTORY_ACTION(kActMount, "mount", L"MOUNT", Mount),
    INVENTORY_ACTION(kActJettison, "jettison", L"JETTISON", Throw),
    INVENTORY_ACTION(kActRoutes, "routes", L"BEST PRICES", Routes),
    INVENTORY_ACTION(kActSlide, "slide", L"HOW MANY", SlideBy),
    INVENTORY_ACTION(kActBar, "bar", L"HOW MANY", Bar),
    INVENTORY_ACTION(kActAccept, "accept", L"JETTISON", Accept),
    INVENTORY_ACTION(kActCancel, "cancel", L"CANCEL", Cancel),
};
#undef INVENTORY_ACTION
const int kActionCount = sizeof(kActions) / sizeof(kActions[0]);

const hk::Sym* const kWindowClass = &exe::VtInventory;
} // namespace inventoryscreen
