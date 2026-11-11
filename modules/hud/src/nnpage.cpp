// The NeuroNet's pages: nnpage.h. The plan and the game's facts: docs\NEURONET_PLAN.md, docs\research\neuronet-windows-facts.md; the rules: nnpage_rules.h.
#include "nnpage.h"

#include "game/baseinfo.h"
#include "game/deckmode.h"
#include "game/hostcalls.h"
#include "game/hudinfo.h"
#include "game/nntabs.h"
#include "game/nnwin.h"
#include "game/room.h"
#include "game/space.h"
#include "flexe.h"
#include "hudhost.h"
#include "hudvis.h"
#include "nnpage_kit.h"
#include "text.h"
#include "nnpage_rules.h"
#include "basepage.h"
#include "nnstrip.h"
#include "safemem.h"
#include "skinchip.h"
#include "skinkit.h"
#include "ui/hud2_kit.h"
#include "ui/hud_rows.h"
#include "ui/ui.h"
#include "ui/winplates.h"

#include <sstream>
#include <unordered_map>
#include <vector>

namespace nnpage
{
using namespace hud2kit;
using nnrules::List;
using namespace nnkit;

enum class Page
{
    None,
    Log,
    Inventory,
    Status,
    Info,
    Death // the log after a death or a failed mission, single player: its three load buttons (issue 50)
};
static Page g_page = Page::None;
static u32 g_root = 0;
static bool g_pad = false, g_paused = false;
static float g_dt = 0;

// The windows' own fields (docs\research\neuronet-windows-facts.md)
constexpr u32 kLogTab = 0x37C;                                  // 0 the LOG tab, 1 MISSION
constexpr u32 kInvFilter = 0x32C, kInvRow = 0x3A4, kInvFirst = 0x3C4, kInvDesc = 0x4F4; // the filter, the selected row, the list's first row, the row's EquipDesc
constexpr u32 kRepBegin = 0x410, kRepEnd = 0x414;               // {reputation id, float} pairs

static const int kFamStatus = winplates::FamilyIndex("playerinfo");

static u32 RootOf(int fam)
{
    for (const winplates::Win& w : winplates::g_wins)
        if (w.fam == fam && winplates::Shown(w.root))
            return w.root;
    return 0;
}

// What a click on a row does: the row takes the focus; a second click on it is its A.
static void RowClick(int row);
static void TabClick(int tab);
static void ChipClick(int chip);
enum Chip
{
    kChipPrimary,
    kChipCard,
    kChipJettison,
    kChipFilter,
    kChipMap,
    kChipTrade,
    kChipTag,
    kChipJetLess,
    kChipJetMore,
    kChipJetMax,
    kChipJetGo,
    kChipJetCancel,
    kChipJetBar, // .. + kJetCells: where on the bar the mouse put the quantity
};
constexpr int kJetCells = 20;

static void Mouse(const Frame& f, const wchar_t* line)
{
    if (!g_pad)
        Text(f.left, (deckmode::LaneTopH() + deckmode::LaneBottomH()) / 2 * f.H, line, 0.026f * f.H, Argb(kChrome, 1));
}
static float ChipAt(float x, float y, float h, const std::wstring& word, int chip) { return nnkit::ChipAt(x, y, h, word, ChipClick, chip); }

// ---- STATUS
struct Rep
{
    u32 id = 0;
    int index = 0;
    float v = 0;
    std::wstring name;
};
static std::vector<Rep> g_reps, g_repShown;
static nnrules::Filter g_filter = nnrules::All;
static bool g_byName = false;
static List g_repList;
static std::wstring g_head[13], g_stat[13];
static float g_statusRead = 0, g_placesRead = 0;
static bool g_card = false;

// The game's own refresh of a step - STATUS's faction card, INVENTORY's selection (the card it shows) - is not per step: each new card is laid out and
// measured again (windowtext: up to 6.4 ms a step on this PC, issue 68 - a Deck several times that), which a held or tapped walk paid at 20 steps a second.
// The page's own focus moves at once; the game follows a lone tap at once too, a walk when it rests (0.1 s) or every 0.25 s. Anything that acts on the game's
// selection (mount, jettison, best prices) brings it up to date first (SyncNow).
static float g_stepAge = 1e9f, g_syncAge = 1e9f;
static bool g_syncPending = false;
static bool SyncDue() { return g_stepAge >= 0.10f || g_syncAge >= 0.25f; }
static std::wstring g_cardName;

static void ShowReps()
{
    g_repShown.clear();
    for (const Rep& r : g_reps)
        if (nnrules::Passes(g_filter, r.v))
            g_repShown.push_back(r);
    std::stable_sort(g_repShown.begin(), g_repShown.end(), [](const Rep& a, const Rep& b) { return g_byName ? a.name < b.name : a.v < b.v; });
    g_repList.count = static_cast<int>(g_repShown.size());
    g_repList.Fit();
}

static void ReadStatus(u32 root)
{
    perf::Scope ps("nn.ReadStatus");
    const u32 begin = R32(root + kRepBegin), end = R32(root + kRepEnd);
    if (begin && end > begin && (end - begin) / 8 < 512)
    {
        std::vector<Rep> reps;
        for (u32 p = begin, i = 0; p < end; p += 8, ++i)
        {
            Rep r;
            r.id = R32(p), r.index = static_cast<int>(i), r.v = RF(p + 4);
            r.name = hudinfo::RepName(r.id);
            if (!r.name.empty())
                reps.push_back(r);
        }
        g_reps.swap(reps);
        ShowReps();
    }
    // (the first read of an open window may come before the server's answer.) The game hides its stats under the faction card, which stands as long as the page,
    // and goes on writing them there (Total Time counts on: issue 73): read hidden then
    const auto find = [](const char* n) { return g_card ? uiclick::Find(n) : uiclick::FindVisible(n); };
    for (int i = 0; i < 13; ++i)
    {
        char name[32];
        sprintf_s(name, "PlayerInfoHeading%d", i);
        std::wstring h, s;
        if (winplates::TextOf(find(name), h))
            g_head[i] = nnrules::Heading(h);
        else if (!g_card)
            g_head[i].clear();
        sprintf_s(name, "PlayerInfoStat%d", i);
        if (winplates::TextOf(find(name), s))
            g_stat[i] = s;
        else if (!g_card)
            g_stat[i].clear(); // (under the card a row the game does not answer keeps what was read)
    }
}

static const wchar_t* BandWord(nnrules::Band b)
{
    static const wchar_t* const kWords[] = {L"HOSTILE", L"UNFRIENDLY", L"NEUTRAL", L"FAVOURABLE", L"FRIENDLY"};
    return kWords[b];
}
static u32 BandRgb(nnrules::Band b)
{
    const u32 kRgb[] = {kHostile, kAmber, kNeutral, kChrome, kFriend};
    return kRgb[b];
}
static const wchar_t* FilterWord(nnrules::Filter f)
{
    static const wchar_t* const kWords[] = {L"ALL", L"ENEMIES", L"INDIFFERENT", L"FRIENDS"};
    return kWords[f];
}

// The focused faction's card stands beside the list, the game's own: no press to read it, where the game's window took one to open it and one to close it.
static int g_cardShown = -1;
static void ShowCard()
{
    if (g_repList.count <= 0 || !exe::StatusFactionCard.ok() || !g_root || room::NNWindowBusy() || g_stat[0].empty())
        return;
    const Rep& r = g_repShown[g_repList.sel];
    if (r.index == g_cardShown || !SyncDue())
        return;
    g_syncAge = 0;
    {
        perf::Scope ps("nn.StatusCard");
        hk::Call<void(__thiscall*)(u32, int)>(exe::StatusFactionCard, g_root, r.index);
    }
    g_cardShown = r.index, g_card = true, g_cardName = r.name;
    // (the card stands as long as the page: its close button has nothing to close, and it stood over the text)
    for (const char* n : {"NavMapCloseInfocardButton", "NavMapCloseInfocardText"})
        if (const u32 c = uiclick::Find(n))
        {
            u8 flags = 0;
            if (safemem::Read(c + 0x6C, &flags, 1))
                safemem::Write(c + 0x6C, static_cast<u8>(flags & 0xFC));
        }
}

static void DrawStatus(const Frame& f)
{
    const float H = f.H, W = f.W;
    Box hole;
    const u32 card = g_card ? uiclick::FindVisible("FactionInfoCard") : 0;
    if (g_card)
    {
        hole.Add(card, 0.012f * H);
        hole.y0 -= 0.05f * H; // (the card lays its heading out above its box's top)
    }
    Cover(W, H, OpenHole(hole, card));
    Title(f, L"PLAYER STATUS");
    // the pilot: three numbers large, the rest as rows
    const float x0 = f.left, x1 = 0.30f * W;
    float y = f.top + 0.03f * H;
    if (!g_stat[0].empty())
    {
        Text(x0, y, g_head[0], 0.026f * H, Argb(kChrome, 1));
        Text(x1, y, g_stat[0], 0.044f * H, Argb(kActive, 1), Right);
        y += 0.056f * H;
    }
    if (!g_stat[1].empty())
    {
        Text(x0, y, g_head[1], 0.026f * H, Argb(kChrome, 1));
        Text(x1, y, g_stat[1], 0.040f * H, Argb(kInk, 1), Right);
        y += 0.050f * H;
    }
    if (!g_stat[2].empty())
    {
        Text(x0, y, g_head[2], 0.026f * H, Argb(kChrome, 1));
        Text(x1, y, g_stat[2], 0.030f * H, Argb(kInk, 1), Right);
        y += 0.028f * H;
        if (nnrules::Number(g_stat[2]) > 0) // (the game may say a mission instead of a sum)
        {
            skinkit::Pips(x0, y, x1 - x0, 0.010f * H, nnrules::Share(nnrules::Number(g_stat[1]), nnrules::Number(g_stat[2])), 24, kActive);
            y += 0.03f * H;
        }
    }
    y += 0.012f * H;
    skinkit::Rule(x0, x1, y);
    y += 0.012f * H;
    const float rowH = std::min(0.056f * H, std::max(skinrules::kRowMin * H, (f.bottom - y) / 10));
    for (int i = 3; i < 13; ++i)
        if (!g_head[i].empty())
        {
            skinkit::Row(x0, x1, y, rowH, g_head[i], g_stat[i]);
            y += rowH;
        }
    // reputation: the list in the middle, the focused faction's standing under it, its card at the right
    const float rx0 = 0.32f * W, rx1 = (hole.ok ? hole.x0 : f.right) - 0.03f * H;
    float ry = f.top + 0.03f * H;
    Text(rx0, ry, L"REPUTATION", 0.030f * H, Argb(kChrome, 1));
    std::wstring tag = FilterWord(g_filter);
    tag += L"  ";
    tag += std::to_wstring(g_repList.count);
    tag += g_byName ? L"  BY NAME" : L"  BY STANDING";
    Text(rx1, ry, tag, 0.026f * H, Argb(kDim, 1), Right);
    ry += 0.028f * H;
    skinkit::Rule(rx0, rx1, ry);
    ry += 0.008f * H;
    const float foot = f.bottom - 0.066f * H; // the standing's two lines
    const float pitch = g_repShown.empty() ? 0.05f * H : std::min(0.066f * H, std::max(0.05f * H, (foot - ry) / g_repShown.size()));
    g_repList.rows = std::max(1, static_cast<int>((foot - ry) / pitch));
    g_repList.Fit();
    // from the right: the band's word, then the bar, the name in what is left (at 16:10 the bar ran under the word, 7 Nov 2026)
    const float wordW = TextWidth(L"UNFRIENDLY", 0.026f * H), half = std::min(0.09f * H, (rx1 - rx0) * 0.14f);
    const float cx = rx1 - 0.012f * H - wordW - 0.024f * H - half, nameW = cx - half - rx0 - 0.036f * H;
    if (g_repShown.empty())
        Text(rx0, ry + pitch / 2, g_reps.empty() ? L"READING THE NEURAL NET" : L"NOBODY IN THIS GROUP", 0.030f * H, Argb(kDim, 1));
    const int hover = skinchip::Hovered();
    for (int i = 0; i < g_repList.rows && g_repList.first + i < g_repList.count; ++i)
    {
        const int row = g_repList.first + i;
        const Rep& r = g_repShown[row];
        const float top = ry + i * pitch, cy = top + pitch / 2;
        const int slot = skinchip::Hot({rx0, top, rx1, top + pitch}, RowClick, row);
        if (row == g_repList.sel)
            skinkit::Focus(rx0, top + 0.002f * H, rx1, top + pitch - 0.002f * H);
        else if (slot >= 0 && slot == hover)
            Rect(rx0, top, rx1 - rx0, pitch, Argb(kChrome, 0.10f));
        TextMarquee(rx0 + 0.012f * H, cy, r.name, 0.030f * H, nameW, Argb(kInk, 1));
        // the bar: from the middle, the game's own marks at -0.6 and 0.6
        const nnrules::Band b = nnrules::BandOf(r.v);
        const float bh = 0.012f * H;
        Rect(cx - half, cy - bh / 2, 2 * half, bh, Argb(kDim, 0.30f));
        const float len = half * std::max(-1.0f, std::min(1.0f, r.v));
        Rect(len < 0 ? cx + len : cx, cy - bh / 2, fabsf(len), bh, Argb(BandRgb(b), 1));
        for (const float m : {-0.6f, 0.0f, 0.6f})
            Rect(cx + half * m - 1, cy - bh, 2, 2 * bh, Argb(kChrome, 0.7f));
        Text(rx1 - 0.012f * H, cy, BandWord(b), 0.026f * H, Argb(row == g_repList.sel ? kInk : BandRgb(b), 1), Right);
    }
    if (g_repList.count > g_repList.rows)
        skinkit::ScrollBar(rx1 + 0.008f * H, ry, ry + g_repList.rows * pitch, static_cast<float>(g_repList.first) / g_repList.count, static_cast<float>(g_repList.rows) / g_repList.count);
    if (g_repList.count > 0)
    {
        // what the number means: the game's own lines, how far they are
        const Rep& r = g_repShown[g_repList.sel];
        wchar_t line[96];
        swprintf_s(line, L"STANDING %+.2f", r.v);
        // two lines, the standing over what it means: on one line the meaning ran left over the pilot's rows at 1280x800 (Shockwave, 7 Nov 2026)
        const float ly = foot + 0.017f * H;
        skinkit::Rule(rx0, rx1, foot + 0.002f * H);
        Text(rx0, ly, line, 0.026f * H, Argb(BandRgb(nnrules::BandOf(r.v)), 1));
        if (r.v > -0.6f)
            swprintf_s(line, L"HOSTILE AT -0.60: %.2f AWAY", r.v + 0.6f);
        else
            swprintf_s(line, L"ITS SHIPS ATTACK, ITS BASES REFUSE YOU: %.2f TO GO", -0.6f - r.v);
        TextMarquee(rx0, ly + 0.030f * H, line, 0.024f * H, rx1 - rx0, Argb(kChrome, 1));
    }
    if (!g_pad)
    {
        const float cy0 = LaneY(), ch = LaneChipH();
        ChipAt(0.32f * W, cy0, ch, L"SHOW " + std::wstring(FilterWord(static_cast<nnrules::Filter>((g_filter + 1) % nnrules::kFilters))), kChipFilter);
        Mouse(f, L"CLICK A FACTION, WHEEL SCROLLS");
    }
}

static unsigned PadStatus(unsigned pressed)
{
    bool stepped = false;
    if (pressed & XINPUT_GAMEPAD_DPAD_UP)
        stepped |= g_repList.Step(-1);
    if (pressed & XINPUT_GAMEPAD_DPAD_DOWN)
        stepped |= g_repList.Step(1);
    if (pressed & XINPUT_GAMEPAD_DPAD_LEFT)
        stepped |= g_repList.Step(-g_repList.rows);
    if (pressed & XINPUT_GAMEPAD_DPAD_RIGHT)
        stepped |= g_repList.Step(g_repList.rows);
    if (stepped)
        g_stepAge = 0; // (the card follows: ShowCard, SyncDue)
    if (pressed & XINPUT_GAMEPAD_X)
        g_filter = static_cast<nnrules::Filter>((g_filter + 1) % nnrules::kFilters), g_repList.sel = 0, g_repList.first = 0, ShowReps();
    if (pressed & XINPUT_GAMEPAD_Y)
        g_byName = !g_byName, ShowReps();
    return XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_DPAD_DOWN | XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_DPAD_RIGHT | XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_X | XINPUT_GAMEPAD_Y;
}

// ---- INVENTORY
struct Item
{
    u32 desc = 0, arch = 0;
    int row = 0, count = 1;
    u16 id = 0;
    bool mounted = false;
    float health = 1;
    std::wstring name;
    int hp = -1; // an empty hardpoint's row: its place in g_free
};
static std::vector<Item> g_items;
static std::vector<equiprules::Hp> g_free; // the ship's empty hardpoints under this filter (the game's window lists only what the ship carries)
static List g_itemList;
static float g_itemRead = 0;
// The loadout as the game's data has it: what the mounted guns do a second, what they draw (baseinfo::GunOf).
struct Loadout
{
    int guns = 0;
    float hull = 0, shield = 0, energy = 0;
} g_load;
static std::unordered_map<u32, std::wstring> g_gunLine;
static bool g_itemSelected = false; // the game's selection is the page's focus
static std::wstring g_filterWord[5];
static const char* const kFilterControl[5] = {"Filter_Weapons", "Filter_Ammo", "Filter_Equipment", "Filter_CargoBay", "Filter_Commodity"};

// A gun's row says what it does, its second line in the game's words for the card's numbers: the slot's class stood there in the game's window.
static const std::wstring& GunLine(u32 arch)
{
    const auto it = g_gunLine.find(arch);
    if (it != g_gunLine.end())
        return it->second;
    baseinfo::Gun g;
    std::wstring line;
    if (baseinfo::GunOf(arch, g) && g.hullDps >= 0)
    {
        wchar_t b[96];
        swprintf_s(b, L"HULL %.0f/S   SHIELD %.0f/S   %.0f M", g.hullDps, g.shieldDps, g.range);
        line = b;
    }
    return g_gunLine[arch] = line;
}
static void ReadLoadout()
{
    g_load = Loadout();
    for (const baseinfo::Item& it : baseinfo::Equipment())
    {
        baseinfo::Gun g;
        if (!it.mounted || !baseinfo::GunOf(it.arch, g) || g.hullDps < 0)
            continue;
        ++g_load.guns, g_load.hull += g.hullDps, g_load.shield += std::max(0.0f, g.shieldDps), g_load.energy += std::max(0.0f, g.energyPs);
    }
}

static int Filter(u32 root) { return static_cast<int>(R32(root + kInvFilter)); }
// Mounting is a dock's: the game's own window hides its mount toggles (EquipToggle1..5) in space and shows them docked (issue 79, its dumps).
static bool Mountable(int filter) { return fl::PlayerBase() && (filter == 0 || filter == 2 || filter == 3); }

// The window's own answer for each of its rows (0x47E790, its filter and order), asked row by row: the game's five rows are five of these. Its selection goes back after.
static void ReadItems(u32 root)
{
    perf::Scope ps("nn.ReadItems");
    if (!exe::InvSelected.ok())
        return;
    const u32 row = R32(root + kInvRow), desc = R32(root + kInvDesc);
    const u16 keep = g_itemList.sel < static_cast<int>(g_items.size()) ? g_items[g_itemList.sel].id : 0;
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
    const int filter = Filter(root);
    g_free.clear();
    if (filter == 0 || filter == 2) // every slot in its own place, EMPTY where nothing is mounted (issue 52)
        items = InSlotOrder(items, filter, g_free, [](const Item& it) { return it.mounted ? DescHardpoint(it.desc) : std::string(); },
                            [](int i)
                            {
                                Item it;
                                it.hp = i, it.row = -1;
                                it.name = L"EMPTY";
                                return it;
                            });
    g_items.swap(items);
    g_itemList.count = static_cast<int>(g_items.size());
    for (int i = 0; keep && i < g_itemList.count; ++i)
        if (g_items[i].id == keep)
            g_itemList.sel = i;
    g_itemList.Fit();
}
// The game's selection follows the page's: its card, its JETTISON.
static void SelectItem(u32 root)
{
    if (g_itemList.count <= 0 || !exe::InvSelectRow.ok() || g_items[g_itemList.sel].hp >= 0)
        return;
    const int first = static_cast<int>(R32(root + kInvFirst));
    perf::Scope ps("nn.InvSelect");
    hk::Call<void(__thiscall*)(u32, int, int)>(exe::InvSelectRow, root, g_items[g_itemList.sel].row - first, 1);
    g_itemSelected = true;
}
// The game's selection brought up to the page's focus now (a deferred step: SyncDue), before anything that acts on it.
static void SyncNow(u32 root)
{
    if (!g_syncPending)
        return;
    g_syncPending = false, g_syncAge = 0;
    SelectItem(root);
}
static void SetFilter(u32 root, int filter)
{
    g_syncPending = false; // (a new filter starts its own selection)
    if (!exe::InvSetFilter.ok())
        return;
    hk::Call<void(__thiscall*)(u32, int, int)>(exe::InvSetFilter, root, filter, 1);
    g_itemList.sel = 0, g_itemList.first = 0, g_itemRead = 0, g_itemSelected = false;
}
static bool OnEmpty() { return g_itemList.count > 0 && g_itemList.sel < static_cast<int>(g_items.size()) && g_items[g_itemList.sel].hp >= 0; }
// What an empty hardpoint's A would mount: the first row of the hold it takes, null none.
static const Item* ForEmpty()
{
    if (!OnEmpty())
        return nullptr;
    const std::vector<u32> fit = gamedata::HoldItemsFor(g_free[g_items[g_itemList.sel].hp]);
    for (const Item& it : g_items)
        if (it.hp < 0 && !it.mounted && std::find(fit.begin(), fit.end(), it.arch) != fit.end())
            return &it;
    return nullptr;
}
static void ToggleMount(u32 root)
{
    SyncNow(root);
    if (g_itemList.count <= 0 || !exe::InvMount.ok() || !Mountable(Filter(root)))
        return;
    if (OnEmpty())
    {
        if (const Item* it = ForEmpty())
        {
            hk::Call<void(__thiscall*)(u32, u32, int)>(exe::InvMount, root, it->desc, 0);
            g_itemRead = 0.12f;
        }
        return;
    }
    const Item& it = g_items[g_itemList.sel];
    hk::Call<void(__thiscall*)(u32, u32, int)>(exe::InvMount, root, it.desc, it.mounted ? 1 : 0);
    g_itemRead = 0.12f; // (the list moves a moment later)
}
// A stack opens the game's panel, which asks how many. A single item goes at the game's first press: the page asks once more (XAG 115: review before a loss, by a press, never a hold).
static u16 g_jettisonArmed = 0;
static bool g_shipView = false;
static float g_shipRect[4] = {};
static bool Jettison()
{
    SyncNow(g_root);
    const u32 c = uiclick::FindVisible("JettisonButton");
    if (!c || g_itemList.count <= 0 || OnEmpty())
        return false;
    const Item& it = g_items[g_itemList.sel];
    if (it.count <= 1 && g_jettisonArmed != it.id)
    {
        g_jettisonArmed = it.id;
        return true;
    }
    g_jettisonArmed = 0;
    return uiclick::Click(c);
}
// The game's commodity list by base: where the selected good sells (it opens over the window; only a commodity's row has it). The game's button stays hidden: its
// TRADE ROUTES label stood in the card's text, and neither moving it nor its local position took (the game lays it out again; 7 Nov 2026). The page offers the list on
// a commodity's row (filter 4, as the game shows the button) and clicks the hidden button: the click runs its action, not a hit test.
constexpr int kFilterCommodities = 4;
static bool TradeOffered() { return g_root && Filter(g_root) == kFilterCommodities && g_itemList.count > 0 && !OnEmpty() && uiclick::Find("TradeRouteButton"); }
// The label is not the button's own (a text button, vt 0x5E614C, which PlaceAt moves off the page: its TRADE ROUTES stayed, 7 Nov 2026) but a title text of its own:
// its text goes blank through the text control's own setter (message 0x1D, as skinload blanks the loading word), right before the UI pass (Under).
static bool BlankText(u32 ctl)
{
    __try
    {
        const u32 fn = Mem<u32>(Mem<u32>(ctl) + 0xA8);
        if (!exe::TextCtlSetProperty.is(fn))
            return false;
        reinterpret_cast<u32(__thiscall*)(u32, u32, const void*, u32)>(fn)(ctl, 0x1D, L" ", 0);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}
static int g_tradeBlanked = 0; // (debug: times the label was blanked, `nnpage`)
static void HideTrade()
{
    // the label is its own control, TradeRouteText (a title text, vt 0x5E4F24; `uinames Trade`, 7 Nov 2026), not the button: blank, so it never stands over the card
    const u32 t = uiclick::Find("TradeRouteText");
    std::wstring now;
    if (t && winplates::TextOf(t, now) && now != L" " && BlankText(t))
        ++g_tradeBlanked;
}
static bool Trade()
{
    SyncNow(g_root);
    const u32 c = uiclick::Find("TradeRouteButton");
    return c && TradeOffered() && uiclick::Click(c);
}
static bool JettisonOpen() { return uiclick::FindVisible("JettisonAcceptButton") != 0; }
static Holes g_iconCells;
static ListIcons g_invIcons; // the rows the page drew, their models sent in Under
// Every row of the list as the icons need it: its game row, its item.
static std::vector<IconRow> InvRows()
{
    std::vector<IconRow> rows(g_items.size());
    for (size_t i = 0; i < g_items.size(); ++i)
        rows[i] = {g_items[i].row, g_items[i].arch, 0, 0};
    return rows;
}

// The jettison panel as the page's: the item with its mesh, the game's own count (+0x93C of +0x940) on a bar of the page's. The game's slider and buttons stand off the screen for as long as it
// stands and go on answering by name; its number field comes into the page, it can still be typed in.
static const char* const kJetOff[] = {"Inv_Jettison_Title", "JettisonQuantitySlider", "JettisonQuantityText", "JettisonCancelButton", "JettisonAcceptButton"};
static float g_jetHome[5][3];
static u32 g_jetMoved[5] = {};
static void JettisonControlsBack()
{
    for (int i = 0; i < 5; ++i)
        if (g_jetMoved[i])
        {
            if (uiclick::FindVisible(kJetOff[i]) == g_jetMoved[i])
                SetPosition(g_jetMoved[i], g_jetHome[i]);
            g_jetMoved[i] = 0;
        }
}
static void DrawJettison(const Frame& f, u32 root)
{
    const float H = f.H, W = f.W, axis = W / 2;
    for (int i = 0; i < 5; ++i)
    {
        const u32 c = uiclick::FindVisible(kJetOff[i]);
        if (c && !g_jetMoved[i] && safemem::Read(c + 0xA8, g_jetHome[i], sizeof(g_jetHome[i])))
            g_jetMoved[i] = c;
        PlaceAt(c, -W, 0);
    }
    const int qty = static_cast<int>(R32(root + 0x93C)), most = static_cast<int>(R32(root + 0x940));
    float y = f.top + 0.09f * H;
    // (the game's window stands behind the page here, so what shows through a hole stands below it: the item's mesh, and for the keyboard the game's number field)
    const float cell = 0.092f * H, shown = 0.715f * H;
    Holes h;
    const Item* it = g_itemList.sel < static_cast<int>(g_items.size()) ? &g_items[g_itemList.sel] : nullptr;
    int mesh = -1;
    if (it)
    {
        const IconRow one{it->row, it->arch, axis, shown + cell / 2};
        g_invIcons.Set(root, &one, 1, InvRows()); // (Under places the item's mesh and sends its model)
        if (const int* sent = g_invIcons.MeshesFor(root, 0))
            mesh = sent[0];
    }
    else
        g_invIcons = ListIcons();
    if (mesh >= 0)
        h.Add(axis - cell / 2, shown, axis + cell / 2, shown + cell);
    const u32 edit = uiclick::FindVisible("JettisonQuantityEdit");
    Box field;
    if (!g_pad)
    {
        PlaceAt(edit, axis + 0.16f * H, shown + cell / 2);
        field.Add(edit, 0.004f * H);
        if (field.ok)
            h.Add(field.x0, field.y0, field.x1, field.y1);
    }
    else if (edit)
        PlaceAt(edit, -W, 0);
    GroundBut(0, 0, W, H, h);
    Title(f, L"JETTISON");
    if (it)
        TextFit(axis, y, it->name, 0.044f * H, 0.6f * W, Argb(kInk, 1), Center);
    Text(axis, y + 0.044f * H, L"HOW MANY GO OVERBOARD", 0.026f * H, Argb(kChrome, 1), Center);
    y += 0.104f * H;
    Text(axis, y, std::to_wstring(qty), 0.066f * H, Argb(kActive, 1), Center);
    y += 0.056f * H;
    // the bar: the mouse puts the quantity where it clicks
    const float bw = 0.42f * W, bx = axis - bw / 2, bh = 0.022f * H;
    Rect(bx, y, bw, bh, Argb(kChrome, 0.18f));
    if (most > 0)
        Rect(bx, y, bw * std::min(1.0f, static_cast<float>(qty) / most), bh, Argb(kActive, 1));
    for (int i = 1; i < 10; ++i)
        Rect(bx + bw * i / 10, y, std::max(1.0f, 0.001f * H), bh, Argb(0x060716, 0.8f));
    for (int i = 0; i < kJetCells && !g_pad; ++i)
        skinchip::Hot({bx + bw * i / kJetCells, y - 0.012f * H, bx + bw * (i + 1) / kJetCells, y + bh + 0.012f * H}, ChipClick, kChipJetBar + i);
    Text(bx, y + bh + 0.022f * H, L"1", 0.026f * H, Argb(kChrome, 1));
    Text(bx + bw, y + bh + 0.022f * H, std::to_wstring(most), 0.026f * H, Argb(kChrome, 1), Right);
    y += bh + 0.052f * H;
    if (!g_pad)
    {
        const float ch = 0.052f * H, cs = skinrules::kChipWord * ch / skinrules::kChipLow, gap = 0.03f * H + 0.012f * H;
        const float rowW = TextWidth(L"-", cs) + TextWidth(L"+", cs) + TextWidth(L"MAX", cs) + TextWidth(L"JETTISON", cs) + TextWidth(L"CANCEL", cs) + 5 * gap - 0.012f * H;
        float cx = axis - rowW / 2;
        cx = ChipAt(cx, y, ch, L"-", kChipJetLess);
        cx = ChipAt(cx, y, ch, L"+", kChipJetMore);
        cx = ChipAt(cx, y, ch, L"MAX", kChipJetMax);
        cx = ChipAt(cx, y, ch, L"JETTISON", kChipJetGo);
        ChipAt(cx, y, ch, L"CANCEL", kChipJetCancel);
        Mouse(f, L"WHEEL, BAR OR CHIPS SET THE NUMBER");
    }
}

static bool CanTagInv(); // (the compare tag: below, by PadInventory)
static bool TaggedInv();
static void DrawInventory(const Frame& f, u32 root)
{
    const float H = f.H, W = f.W;
    baseinfo::Refresh(); // (the pad's close runs between the page's Update and this: the list must be this moment's)
    if (!uiclick::FindVisible("NN_Inventory"))
        return;
    if (JettisonOpen())
    {
        DrawJettison(f, root);
        return;
    }
    JettisonControlsBack();
    PlaceAt(uiclick::FindVisible("TradeRouteButton"), -W, 0); // (it stood in the card's hole: the page has its own word for it, Trade; its label is HideTrade's)
    HideTrade();
    Box hole;
    const u32 card = uiclick::FindVisible("InfoCard_Info");
    hole.Add(card, 0.012f * H);
    // (an empty hardpoint has no card of the game's: what stood there was the last row's)
    const bool empty = OnEmpty();
    {
        Holes h = g_iconCells;
        if (!empty && OpenHole(hole, card).ok)
            h.Add(hole.x0, hole.y0, hole.x1, hole.y1);
        GroundBut(0, 0, W, H, h);
    }
    if (empty && hole.ok)
    {
        const equiprules::Hp& hp = g_free[g_items[g_itemList.sel].hp];
        const float cx = hole.x0 + 0.02f * H, cw = hole.x1 - hole.x0 - 0.04f * H;
        float cy = hole.y0 + 0.03f * H;
        Text(cx, cy, L"EMPTY HARDPOINT", 0.034f * H, Argb(kInk, 1));
        cy += 0.046f * H;
        Text(cx, cy, equiprules::SlotWord(equiprules::BestOf(hp)), 0.028f * H, Argb(kActive, 1));
        cy += 0.040f * H;
        skinkit::Rule(cx, cx + cw, cy);
        cy += 0.030f * H;
        const std::vector<u32> fit = gamedata::HoldItemsFor(hp);
        Text(cx, cy, fit.empty() ? L"NOTHING IN YOUR HOLD FITS IT" : L"IN YOUR HOLD, IT TAKES", 0.026f * H, Argb(kChrome, 1));
        cy += 0.036f * H;
        for (size_t i = 0; i < fit.size() && cy < hole.y1 - 0.03f * H; ++i, cy += 0.036f * H)
            TextMarquee(cx, cy, hudinfo::EquipName(fit[i]), 0.028f * H, cw, Argb(kInk, 1));
    }
    std::wstring ship = baseinfo::ShipName();
    for (wchar_t& c : ship)
        c = towupper(c);
    Title(f, ship.empty() ? L"INVENTORY" : ship);
    const float x0 = f.left, x1 = 0.405f * W;
    // the filters: the game's five, in its words
    const int filter = Filter(root), hover = skinchip::Hovered();
    float tx = x0, ty = f.top;
    const float size = 0.030f * H, tabH = 0.046f * H;
    for (int i = 0; i < 5; ++i)
    {
        const float w = TextWidth(g_filterWord[i], size) + 0.024f * H;
        const navlayout::Rect r{tx, ty, tx + w, ty + tabH};
        const int slot = skinchip::Hot(r, TabClick, i);
        skinchip::Tab(r, g_filterWord[i], size, i == filter, slot >= 0 && slot == hover);
        tx += w + 0.008f * H;
    }
    // five rows, as the game's: its own icons stand in them, its list scrolled with the page's
    const float y = ty + tabH + 0.012f * H, foot = f.bottom, pitch = std::min(0.126f * H, (foot - y) / 5), cell = std::min(0.092f * H, pitch);
    g_itemList.rows = 5;
    g_itemList.Fit();
    Holes cells;
    const int n = std::min(g_itemList.rows, std::max(0, g_itemList.count - g_itemList.first));
    // the icons: Under places the meshes and sends their models from the list as it stands then (the frame of a step); a row gets a cell when Under gave
    // it its own model this frame
    const int* meshOf = g_invIcons.MeshesFor(root, g_itemList.first);
    g_invIcons.SetList(root, InvRows(), &g_itemList, x0 + cell / 2, y, pitch);
    if (g_items.empty())
        Text(x0, y + pitch / 2, L"NOTHING HERE", 0.030f * H, Argb(kDim, 1));
    for (int i = 0; i < n; ++i)
    {
        const int row = g_itemList.first + i;
        const Item& it = g_items[row];
        const float top = y + i * pitch, cy = top + pitch * 0.36f;
        const int slot = skinchip::Hot({x0, top, x1, top + pitch}, RowClick, row);
        if (meshOf && meshOf[i] >= 0)
            cells.Add(x0 + 1, top + (pitch - cell) / 2, x0 + cell - 1, top + (pitch + cell) / 2);
        if (row == g_itemList.sel)
            skinkit::Focus(x0 + cell, top + 0.002f * H, x1, top + pitch - 0.002f * H);
        else if (slot >= 0 && slot == hover)
            Rect(x0 + cell, top, x1 - x0 - cell, pitch, Argb(kChrome, 0.10f));
        const bool tagged = it.hp < 0 && it.mounted && it.arch == gamedata::TaggedCompare();
        const std::wstring right = it.hp >= 0 ? equiprules::SlotWord(equiprules::BestOf(g_free[it.hp])) : tagged ? L"MOUNTED, TAGGED" : it.mounted ? L"MOUNTED" : (it.count > 1 ? L"x" + std::to_wstring(it.count) : L"IN HOLD");
        const float rw = TextWidth(right, 0.026f * H);
        const float tx = x0 + cell + 0.012f * H;
        TextMarquee(tx, cy, it.name, 0.032f * H, x1 - tx - rw - 0.05f * H, Argb(it.hp >= 0 ? kDim : kInk, 1));
        Text(x1 - 0.012f * H, cy, right, 0.026f * H, Argb(row == g_itemList.sel ? kInk : tagged ? kActive : it.mounted ? kFriend : kChrome, 1), Right);
        const std::wstring& line = GunLine(it.arch);
        if (!line.empty())
            TextMarquee(tx, top + pitch * 0.76f, line, 0.024f * H, x1 - tx - 0.012f * H, Argb(row == g_itemList.sel ? kInk : kChrome, 0.9f));
        if (it.health > 0 && it.health < 0.995f && it.mounted)
            skinkit::Pips(x1 - 0.14f * H, top + pitch * 0.76f, 0.13f * H, 0.006f * H, it.health, 10, it.health < 0.35f ? kHostile : kAmber);
    }
    if (g_itemList.count > g_itemList.rows)
        skinkit::ScrollBar(x1 + 0.006f * H, y, y + g_itemList.rows * pitch, static_cast<float>(g_itemList.first) / g_itemList.count, static_cast<float>(g_itemList.rows) / g_itemList.count);
    g_iconCells = cells;
    // the middle: the ship, what it carries and what it can do
    const float mx0 = 0.425f * W, mx1 = (hole.ok ? hole.x0 : f.right) - 0.03f * H, inSpace = !fl::PlayerBase();
    float my = y + 0.008f * H;
    g_shipView = inSpace && exe::FrameUpdate.ok();
    if (g_shipView)
    {
        // no wider than tall: the view fits the model by its width, and a long ship (Shockwave's Anubis) ran out of a wide, low box at its nose (issue 46)
        g_shipRect[0] = (mx0 + mx1) / 2, g_shipRect[1] = my + 0.125f * H, g_shipRect[2] = std::min((mx1 - mx0) / 2, 0.125f * H), g_shipRect[3] = 0.125f * H;
        my += 0.262f * H;
    }
    wchar_t b[48];
    // docked no view of the ship stands (the status panel's lives in space; the window's own ShipView_Control is hidden by its builder, BASE_ROOMS_PLAN): the column carries what the ship is, two numbers a line
    if (!inSpace)
    {
        const gamedata::ShipFacts s = gamedata::Ship(baseinfo::ShipArch());
        if (s.ok)
        {
            const struct
            {
                const wchar_t* word;
                float value;
            } facts[] = {{L"Armor", s.hull}, {L"Cargo space", s.hold}, {L"Gun mounts", static_cast<float>(s.guns)}, {L"Turret mounts", static_cast<float>(s.turrets)}, {L"Best gun class", static_cast<float>(s.gunClass)}, {L"Best shield class", static_cast<float>(s.shieldClass)}, {L"Nanobots", s.nanobots}, {L"Shield batteries", s.batteries}};
            const float half = (mx1 - mx0 - 0.03f * H) / 2, pitch = 0.034f * H;
            for (int i = 0; i < 8; ++i)
            {
                const float a = i % 2 ? mx0 + half + 0.03f * H : mx0;
                skinkit::Row(a, a + half, my + (i / 2) * pitch, pitch, facts[i].word, equiprules::ValueWord(facts[i].value));
            }
            my += 4 * pitch + 0.012f * H;
            skinkit::Rule(mx0, mx1, my);
            my += 0.008f * H;
        }
    }
    if (g_load.guns > 0)
    {
        swprintf_s(b, L"%d GUNS", g_load.guns);
        Text(mx0, my + 0.014f * H, L"FIREPOWER", 0.026f * H, Argb(kChrome, 1));
        Text(mx1, my + 0.014f * H, b, 0.026f * H, Argb(kInk, 1), Right);
        my += 0.032f * H;
        swprintf_s(b, L"HULL %.0f/S   SHIELD %.0f/S   ENERGY %.0f/S", g_load.hull, g_load.shield, g_load.energy);
        TextFit((mx0 + mx1) / 2, my + 0.014f * H, b, 0.026f * H, mx1 - mx0, Argb(kInk, 1), Center);
        my += 0.040f * H;
    }
    // the selected item against what it would stand beside
    if (g_itemList.count > 0 && !empty)
    {
        skinkit::Rule(mx0, mx1, my);
        my = Compare(mx0, mx1, my + 0.008f * H, g_items[g_itemList.sel].arch, g_items[g_itemList.sel].mounted, H);
        if (g_pad && CanTagInv()) // the click in words under the numbers (the lane holds FILTER, ITEM, MOUNT, JETTISON, CARD already)
        {
            const float gh = 0.028f * H, gw = GlyphWidth(glyphs::R3, gh);
            Glyph(glyphs::R3, mx0 + gw / 2, my + 0.020f * H, gh, Argb(kInk, 1));
            Text(mx0 + gw + 0.010f * H, my + 0.020f * H, TaggedInv() ? L"CLICK: UNTAG" : L"CLICK: COMPARE THE OTHERS WITH THIS", 0.024f * H, Argb(TaggedInv() ? kActive : kChrome, 1));
            my += 0.044f * H;
        }
    }
    // the purse and the hold under the ship's name, in the docked strip's words and sizes (uistrip DrawStrip's second line, which stands down over a NeuroNet
    // window): at the column's foot the compare block pushed them under the prompts (issue 67)
    {
        float lx = f.left;
        const float ly = f.titleY + 0.043f * H; // (between the title and the filters: at 1280x800 0.050 H left the filters 8 px under it)
        const auto item = [&](const wchar_t* label, const std::wstring& value, u32 rgb)
        {
            lx += Text(lx, ly, label, 0.024f * H, Argb(kChrome, 1), Left, 0.06f) + 0.010f * H;
            lx += Text(lx, ly, value, 0.030f * H, Argb(rgb, 1)) + 0.032f * H;
        };
        item(L"CREDITS", Money(baseinfo::Credits()), kInk);
        int used = 0, cap = 0;
        if (baseinfo::Cargo(used, cap) && cap > 0)
            item(L"CARGO", std::to_wstring(used) + L" / " + std::to_wstring(cap), used >= cap ? kWarn : kInk);
    }
    if (!g_pad && g_itemList.count > 0)
    {
        float cx = 0.425f * W;
        const float y2 = LaneY(), ch = LaneChipH();
        if (Mountable(filter) && (!empty || ForEmpty()))
            cx = ChipAt(cx, y2, ch, g_items[g_itemList.sel].mounted ? L"UNMOUNT" : L"MOUNT", kChipPrimary);
        if (uiclick::FindVisible("JettisonButton") && !empty)
            cx = ChipAt(cx, y2, ch, g_jettisonArmed ? L"CLICK AGAIN TO JETTISON" : L"JETTISON", kChipJettison);
        if (TradeOffered())
            cx = ChipAt(cx, y2, ch, L"BEST PRICES", kChipTrade);
        if (CanTagInv()) // (the mouse's way to RS's click, issue 44)
            ChipAt(cx, y2, ch, TaggedInv() ? L"UNTAG" : L"COMPARE THE OTHERS WITH THIS", kChipTag);
        Mouse(f, L"CLICK AN ITEM, WHEEL SCROLLS");
    }
}

// The compare tag (issue 44, as the equipment dealer's: Shockwave, the inventory tab too): RS's click or the card's chip on a mounted item makes it what the
// items in the hold compare with (gamedata::TagCompare: one tag for both pages)
static bool CanTagInv()
{
    if (g_page != Page::Inventory || g_itemList.count <= 0 || g_itemList.sel >= static_cast<int>(g_items.size()) || OnEmpty())
        return false;
    const auto& it = g_items[g_itemList.sel];
    const equiprules::Spec* s = gamedata::Equip(it.arch);
    return it.mounted && it.hp < 0 && s && !s->stats.empty();
}
static bool TaggedInv() { return CanTagInv() && g_items[g_itemList.sel].arch == gamedata::TaggedCompare(); }
static void ToggleTagInv()
{
    if (CanTagInv())
        gamedata::TagCompare(TaggedInv() ? 0 : g_items[g_itemList.sel].arch);
}

static unsigned PadInventory(u32 root, unsigned pressed)
{
    const int filter = Filter(root);
    if (pressed & ~static_cast<unsigned>(XINPUT_GAMEPAD_Y))
        g_jettisonArmed = 0;
    // (the game's selection follows in Update when due: SyncDue)
    if ((pressed & XINPUT_GAMEPAD_DPAD_UP) && g_itemList.Step(-1))
        g_stepAge = 0, g_syncPending = true;
    if ((pressed & XINPUT_GAMEPAD_DPAD_DOWN) && g_itemList.Step(1))
        g_stepAge = 0, g_syncPending = true;
    if (pressed & XINPUT_GAMEPAD_DPAD_LEFT)
        SetFilter(root, (filter + 4) % 5);
    if (pressed & XINPUT_GAMEPAD_DPAD_RIGHT)
        SetFilter(root, (filter + 1) % 5);
    if (pressed & XINPUT_GAMEPAD_A)
        ToggleMount(root);
    if (pressed & XINPUT_GAMEPAD_Y)
        Jettison();
    if (pressed & XINPUT_GAMEPAD_X)
        Trade();
    if (pressed & XINPUT_GAMEPAD_RIGHT_THUMB)
        ToggleTagInv();
    return XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_DPAD_DOWN | XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_DPAD_RIGHT | XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_X | XINPUT_GAMEPAD_Y |
           XINPUT_GAMEPAD_RIGHT_THUMB;
}

// ---- LOG and INFO: the game's text in a column, HUD 2 round it
// The hole for the game's text starts a little below the text's top: the window's art (a tab's foot, a line) stands on the text's top left, and a hole grown up by
// 0.012 H like its other sides showed it above the first line (issue 63, Shockwave's Deck). At 4K the art's foot stood 8 px under the text's top, the first line's
// capitals 19 px: the hole starts between them, 13 px (0.006 H; the game's UI scales with the height).
constexpr float kTextTop = -0.006f;
static std::wstring g_objective;
static void LogTab(int mission)
{
    if (const u32 c = uiclick::FindVisible(mission ? "MissionButton" : "LogButton"))
        uiclick::Click(c);
}

// What stands in this system and the player has found, nearest first (the target menu's own test); the ships near, by what they think of the player.
struct Place
{
    std::wstring name, what;
    float distance = 0, attitude = 0;
};
static std::vector<Place> g_places, g_ships;
// A base in the words of the data: who runs it, what it offers.
static std::wstring Offers(const gamedata::BaseFacts& b)
{
    std::wstring s;
    const auto add = [&](bool on, const wchar_t* word)
    {
        if (on)
            s += (s.empty() ? L"" : L", "), s += word;
    };
    add(b.ships, L"SHIPS");
    add(b.equipment, L"EQUIPMENT");
    add(b.trader, L"TRADE");
    add(b.bar, L"BAR");
    return s;
}
static void DrawList(const Frame& f, float y, float x0, float x1, const wchar_t* title, const std::vector<Place>& list, bool ships)
{
    if (list.empty())
        return;
    const float H = f.H;
    skinkit::Rule(x0, x1, y);
    y += 0.024f * H;
    Text(x0, y, title, 0.024f * H, Argb(kChrome, 1));
    const float rowH = 0.058f * H;
    // a list cut at the page's foot says so, nearest first: "8 OF 20" beside its title (the owner, 7 Nov 2026: no sign there were more)
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
        const Place& p = list[i];
        wchar_t d[24];
        swprintf_s(d, L"%.1f K", p.distance / 1000);
        const float dw = TextWidth(d, 0.026f * H);
        const u32 rgb = ships ? BandRgb(nnrules::BandOf(p.attitude)) : kInk;
        TextMarquee(x0, y + rowH * 0.32f, p.name, 0.028f * H, x1 - x0 - dw - 0.02f * H, Argb(rgb, 1));
        Text(x1, y + rowH * 0.32f, d, 0.026f * H, Argb(kChrome, 1), Right);
        Text(x0, y + rowH * 0.76f, p.what, 0.024f * H, Argb(kChrome, 0.8f));
    }
}

// The left column of LOG and INFO: a label over its value.
static float Fact(const Frame& f, float y, float x1, const wchar_t* label, const std::wstring& value)
{
    if (value.empty())
        return y;
    Text(f.left, y, label, 0.024f * f.H, Argb(kChrome, 1));
    TextMarquee(f.left, y + 0.036f * f.H, value, 0.034f * f.H, x1 - f.left, Argb(kInk, 1));
    return y + 0.082f * f.H;
}
static float Here(const Frame& f, float y, float x1)
{
    std::wstring where = fl::PlayerBase() ? baseinfo::SystemName() : hudinfo::SystemName();
    const std::wstring base = fl::PlayerBase() ? baseinfo::BaseName() : hudinfo::ZoneName();
    y = Fact(f, y, x1, L"SYSTEM", where);
    return Fact(f, y, x1, fl::PlayerBase() ? L"BASE" : L"ZONE", base);
}

static void DrawLog(const Frame& f, u32 root)
{
    const float H = f.H;
    Box hole;
    const u32 text = uiclick::FindVisible("StoryStarHistoryText");
    hole.Add(text, 0.012f * H, kTextTop * H);
    const Box open = OpenHole(hole, text);
    Cover(f.W, H, open);
    Title(f, L"NEURAL NET LOG");
    u8 tab = 0;
    safemem::Read(root + kLogTab, &tab, 1);
    const int hover = skinchip::Hovered();
    float tx = f.left;
    const float size = 0.030f * H, tabH = 0.046f * H;
    for (int i = 0; i < 2; ++i)
    {
        const wchar_t* word = i == 0 ? L"MISSION" : L"JOURNAL";
        const float w = TextWidth(word, size) + 0.024f * H;
        const navlayout::Rect r{tx, f.top, tx + w, f.top + tabH};
        const int slot = skinchip::Hot(r, TabClick, i);
        skinchip::Tab(r, word, size, (i == 0) == (tab != 0), slot >= 0 && slot == hover);
        tx += w + 0.008f * H;
    }
    if (!g_pad)
    {
        if (!g_objective.empty())
            ChipAt(0.425f * f.W, LaneY(), LaneChipH(), L"SHOW ON MAP", kChipMap);
        Mouse(f, L"WHEEL SCROLLS THE TEXT");
    }
    // (the column comes with the text, in the frame it stands laid out where it stays: before, it followed it from the game's place)
    if (hole.ok && !open.ok)
        return;
    const float x1 = hole.ok ? hole.x0 - 0.03f * H : 0.34f * f.W, inner = 0.016f * H;
    float y = f.top + tabH + 0.02f * H;
    const float cardH = g_objective.empty() ? 0.10f * H : 0.21f * H;
    skinkit::Plate(f.left, y, x1, y + cardH, kTL | kBR);
    Text(f.left + inner, y + 0.03f * H, L"RIGHT NOW", 0.026f * H, Argb(kActive, 1));
    if (g_objective.empty())
        Text(f.left + inner, y + 0.066f * H, L"No objective", 0.030f * H, Argb(kDim, 1));
    else
        skinkit::Body(f.left + inner, y + 0.056f * H, x1 - f.left - 2 * inner, g_objective, 4);
    y += cardH + 0.03f * H;
    const float timer = hudinfo::MissionTimer();
    if (timer > 0)
    {
        wchar_t b[32];
        swprintf_s(b, L"%d:%02d", static_cast<int>(timer) / 60, static_cast<int>(timer) % 60);
        y = Fact(f, y, x1, L"TIME LEFT", b);
    }
    y = Here(f, y, x1);
    y = Fact(f, y, x1, L"SHIP", baseinfo::ShipName());
    DrawList(f, y, f.left, x1, L"NEAR YOU", g_places, false);
}

// The log after a death or a failed mission (FREELANCER_RE "The death / mission-failed dialog"): the game's text through its hole, the reason at its top;
// the three buttons stay the game's, where the game put them under the text, HUD 2 chips drawn on them (a click goes through to the game's own button).
// The pad's binds are padmenus' (Screen::Death): A, X, Y, its prompts in the lane.
static const char* const kDeathButton[3][2] = {{"LoadAutosaveButton", "LoadAutosaveText"}, {"MainMenuButton", "MainMenuText"}, {"LoadSavegameButton", "LoadSavegameText"}};
static void DrawDeath(const Frame& f)
{
    const float H = f.H;
    Box hole;
    const u32 text = uiclick::FindVisible("StoryStarHistoryText");
    hole.Add(text, 0.012f * H, kTextTop * H);
    Cover(f.W, H, OpenHole(hole, text));
    Title(f, L"NEURAL NET LOG");
    if (g_pad)
        return;
    const float mx = static_cast<float>(fl::MouseX()), my = static_cast<float>(fl::MouseY());
    for (const auto& b : kDeathButton)
    {
        ui::Item it;
        const u32 ctl = uiclick::FindVisible(b[0]);
        if (!ctl || !ui::ControlRect(ctl, it) || it.hw <= 1)
            continue;
        std::wstring word;
        winplates::TextOf(uiclick::Find(b[1]), word);
        if (word.empty())
            word = b[0][0] == 'M' ? L"MAIN MENU" : b[0][4] == 'A' ? L"LOAD AUTOSAVE" : L"LOAD GAME";
        for (wchar_t& c : word)
            c = towupper(c);
        const navlayout::Rect r{it.x - it.hw, it.y - it.hh, it.x + it.hw, it.y + it.hh};
        const bool over = mx >= r.x0 && mx <= r.x1 && my >= r.y0 && my <= r.y1;
        Ground(r.x0, r.y0, r.x1, r.y1); // (the game's art and caption under the chip)
        skinchip::Chip(r, word, std::min(0.030f * H, (r.y1 - r.y0) * 0.6f), over ? skinchip::Look::Hover : skinchip::Look::Plain);
    }
}
static bool DeathShown() { return !nnwin::Multiplayer() && uiclick::FindVisible("LoadAutosaveButton") && uiclick::FindVisible("StoryStarHistoryText"); }
// (windowtext puts the log in the middle on this screen: the page has nothing for its left - no ship, no place to read)

static void ReadPlaces()
{
    g_places.clear(), g_ships.clear();
    if (fl::PlayerBase())
        return;
    for (const space::Contact& c : space::Contacts())
    {
        if (c.ship && c.id && !c.player)
        {
            Place p;
            p.name = hudinfo::DisplayName(c.id);
            p.what = hudinfo::FactionName(c.id);
            p.distance = c.distance, p.attitude = c.attitude;
            if (!p.name.empty())
                g_ships.push_back(p);
        }
        if (c.ship || !c.dock || !c.id || hudinfo::IsLaneRing(c.id) || hudinfo::Undiscovered(c.id))
            continue;
        Place p;
        p.name = hudinfo::DisplayName(c.id);
        p.what = hudinfo::ClassName(c.id);
        p.distance = c.distance;
        // what the data says of a place the player has found: where a gate leads, what a base offers
        for (const gamedata::SolarFact& s : gamedata::System(fl::PlayerSystem()).solars)
            if (s.id == c.id)
            {
                if (!s.goesToName.empty())
                    p.what += L" TO " + Upper(s.goesToName);
                else if (s.base)
                {
                    const std::wstring o = Offers(gamedata::Base(s.base));
                    if (!o.empty())
                        p.what += L": " + o;
                }
            }
        if (!p.name.empty())
            g_places.push_back(p);
    }
    const auto nearest = [](const Place& a, const Place& b) { return a.distance < b.distance; };
    std::sort(g_places.begin(), g_places.end(), nearest);
    std::sort(g_ships.begin(), g_ships.end(), nearest);
}

// A list of factions in the colour of the player's standing, by weight: who flies here, who keeps an office.
static float DrawFactions(float x0, float x1, float y, float foot, float H, const wchar_t* title, const std::vector<gamedata::Faction>& list)
{
    if (list.empty())
        return y;
    Text(x0, y, title, 0.024f * H, Argb(kChrome, 1));
    {
        // cut at its foot: "7 OF 11" beside the title, the weightiest first (as DrawList)
        size_t named = 0;
        for (const gamedata::Faction& g : list)
            named += !text::Ids(g.nameIds).empty();
        const size_t fit = static_cast<size_t>(std::max(0.0f, (foot - y - 0.05f * H - 0.03f * H) / (0.034f * H) + 1));
        if (fit < named)
        {
            wchar_t of[32];
            swprintf_s(of, L"%u OF %u", static_cast<unsigned>(fit), static_cast<unsigned>(named));
            Text(x1, y, of, 0.024f * H, Argb(kInk, 1), Right);
        }
    }
    y += 0.026f * H;
    skinkit::Rule(x0, x1, y);
    y += 0.024f * H;
    float sum = 0;
    for (const gamedata::Faction& g : list)
        sum += g.weight;
    for (size_t i = 0; i < list.size() && y + 0.03f * H <= foot; ++i, y += 0.034f * H)
    {
        const gamedata::Faction& g = list[i];
        const std::wstring name = text::Ids(g.nameIds);
        if (name.empty())
        {
            y -= 0.034f * H;
            continue;
        }
        const nnrules::Band b = nnrules::BandOf(g.standing);
        wchar_t share[16];
        swprintf_s(share, L"%.0f %%", sum > 0 ? 100 * g.weight / sum : 0);
        TextMarquee(x0, y, name, 0.027f * H, x1 - x0 - 0.20f * H, Argb(BandRgb(b), 1));
        Text(x1 - 0.075f * H, y, BandWord(b), 0.022f * H, Argb(BandRgb(b), 0.9f), Right);
        Text(x1, y, share, 0.024f * H, Argb(kChrome, 1), Right);
    }
    return y;
}
// INFO's two columns, laid out by the game's text: the left one ends short of it, the ships stand under it
static void InfoColumns(const Frame& f, const Box& hole)
{
    const float H = f.H;
    const float x1 = hole.ok ? hole.x0 - 0.03f * H : 0.34f * f.W;
    float y = Here(f, f.top + 0.03f * H, x1);
    const u32 base = fl::PlayerBase();
    if (base)
    {
        // the left column, from the game's data: the card at the right lists what is sold
        const gamedata::BaseFacts& b = gamedata::Base(base);
        const std::wstring offers = Offers(b);
        if (!offers.empty())
            y = Fact(f, y, x1, L"HERE YOU FIND", offers);
        y = DrawFactions(f.left, x1, y + 0.01f * H, f.bottom - 0.03f * H, H, L"WHO KEEPS AN OFFICE HERE", b.factions);
        if (b.offersMax > 0)
        {
            wchar_t line[48];
            swprintf_s(line, L"THE JOB BOARD HOLDS %d TO %d OFFERS", b.offersMin, b.offersMax);
            Text(f.left, y + 0.012f * H, line, 0.024f * H, Argb(kChrome, 1));
        }
    }
    else
    {
        const float half = (y + f.bottom) / 2 + 0.03f * H;
        DrawFactions(f.left, x1, y + 0.01f * H, half - 0.03f * H, H, L"WHO FLIES IN THIS SYSTEM", gamedata::System(fl::PlayerSystem()).factions);
        DrawList(f, half, f.left, x1, L"FOUND IN THIS SYSTEM", g_places, false);
        // the game's text may be one line: the room under it says who is around
        const u32 text = uiclick::FindVisible("PlayerInfoText");
        const float used = text ? static_cast<float>(R32(text + 0x380)) : 1e6f, top = hole.y0 + (kTextTop - 0.012f) * H; // (from 0.012 H above the text, as before issue 63)
        if (hole.ok && top + used + 0.08f * H < f.bottom)
            DrawList(f, top + used + 0.05f * H, hole.x0 + 0.012f * H, f.right, L"SHIPS NEAR YOU", g_ships, true);
    }
}
static void DrawInfo(const Frame& f)
{
    const float H = f.H;
    Box hole;
    const u32 infoText = uiclick::FindVisible("PlayerInfoText");
    hole.Add(infoText, 0.012f * H, kTextTop * H);
    const Box open = OpenHole(hole, infoText);
    Cover(f.W, H, open);
    Title(f, L"CURRENT INFORMATION");
    // (the columns come with the text, in the frame it stands laid out where it stays: before, they followed it from the game's place and jumped twice)
    if (open.ok || !hole.ok)
        InfoColumns(f, hole);
    if (!g_pad)
    {
        ChipAt(0.425f * f.W, LaneY(), LaneChipH(), L"SHOW ON MAP", kChipMap);
        Mouse(f, L"WHEEL SCROLLS THE TEXT");
    }
}

// ---- the clicks
static void RowClick(int row)
{
    if (g_page == Page::Status)
        g_repList.sel = row, g_repList.Fit();
    else if (g_page == Page::Inventory)
    {
        g_itemList.sel = row, g_itemList.Fit();
        g_jettisonArmed = 0;
        SelectItem(g_root);
    }
}
static void TabClick(int tab)
{
    if (g_page == Page::Inventory)
        SetFilter(g_root, tab);
    else if (g_page == Page::Log)
        LogTab(tab == 0);
}
static void ChipClick(int chip)
{
    switch (chip)
    {
    case kChipPrimary: ToggleMount(g_root); break;
    case kChipJettison: Jettison(); break;
    case kChipFilter: PadStatus(XINPUT_GAMEPAD_X); break;
    case kChipMap: nnwin::Go(nntabs::kCmdMap); break;
    case kChipTrade: Trade(); break;
    case kChipTag: ToggleTagInv(); break;
    case kChipJetLess: uiclick::Scroll(uiclick::Find("JettisonQuantitySlider"), -1, false, nullptr, nullptr); break;
    case kChipJetMore: uiclick::Scroll(uiclick::Find("JettisonQuantitySlider"), 1, false, nullptr, nullptr); break;
    case kChipJetMax: uiclick::Scroll(uiclick::Find("JettisonQuantitySlider"), 100000, false, nullptr, nullptr); break;
    case kChipJetGo: uiclick::Click(uiclick::Find("JettisonAcceptButton")); break;
    case kChipJetCancel: uiclick::Click(uiclick::Find("JettisonCancelButton")); break;
    default:
        if (chip >= kChipJetBar && chip < kChipJetBar + kJetCells)
        {
            const int most = static_cast<int>(R32(g_root + 0x940)), qty = static_cast<int>(R32(g_root + 0x93C));
            const int want = std::max(1, (most * (chip - kChipJetBar + 1) + kJetCells / 2) / kJetCells);
            uiclick::Scroll(uiclick::Find("JettisonQuantitySlider"), static_cast<float>(want - qty), false, nullptr, nullptr);
        }
        break;
    }
}

// ---- the frame
void Update(float dt, bool pad)
{
    g_pad = pad;
    g_dt = dt;
    Page page = Page::None;
    u32 root = 0;
    if (winplates::g_on && !winplates::g_scene && nnwin::OpenTab() >= 0 && DeathShown()) // (the lookups only with a NeuroNet window open: issue 68) // (the log after a death or a failed mission: its own page, no tabs - the strip stands down)
    {
        root = RootOf(winplates::kFamLog);
        page = root ? Page::Death : Page::None;
    }
    else if (nnstrip::Wanted() && !uiclick::FindVisible("LoadAutosaveButton"))
    {
        const int cmd = nnwin::OpenCommand();
        root = cmd == nntabs::kCmdLog         ? RootOf(winplates::kFamLog)
               : cmd == nntabs::kCmdInventory ? RootOf(winplates::kFamInventory)
               : cmd == nntabs::kCmdStatus    ? RootOf(kFamStatus)
               : cmd == nntabs::kCmdInfo      ? RootOf(winplates::kFamHelp)
                                              : 0;
        if (root)
            page = cmd == nntabs::kCmdLog ? Page::Log : cmd == nntabs::kCmdInventory ? Page::Inventory : cmd == nntabs::kCmdStatus ? Page::Status : Page::Info;
    }
    if (page != g_page || root != g_root)
    {
        g_statusRead = g_itemRead = 0;
        g_card = false, g_itemSelected = false, g_cardShown = -1;
        g_itemList = List();
        g_reps.clear(), g_repShown.clear(), g_repList.count = 0;
        for (std::wstring& s : g_stat)
            s.clear();
        g_placesRead = 0;
    }
    if (page != g_page || root != g_root)
        g_syncPending = false;
    g_page = page, g_root = root;
    g_stepAge += dt, g_syncAge += dt;
    // the world stops in single player, in space, the map too; an autopilot under the window goes on flying (the owner, 5 Nov, issue 25)
    // (the status window's numbers and factions come from the in-process server, which the pause stops: the world goes on the moment until they are in)
    const bool waiting = page == Page::Status && (g_reps.empty() || g_stat[0].empty());
    // A press or click on a page may ask the game something (a jettison, a mount, a route) that the in-process server answers, and the pause stopped the server: the
    // request was lost (the map's route, Shockwave, 7 Nov 2026). The world stays paused; the host lets the server take the request on a frame with no time in it (input.cpp).
    {
        static bool s_down = false;
        const bool down = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0 && GetForegroundWindow() == fl::MainWindow();
        if (down && !s_down && nnstrip::Wanted())
            hostcalls::WorldCommand();
        s_down = down;
    }
    g_paused = nnstrip::Wanted() && hudrows::g.nnPause && !nnwin::Multiplayer() && !fl::PlayerBase() && !views::ApActive() && hostcalls::g.SetWorldPause && !waiting;
    if (g_paused)
        hostcalls::g.SetWorldPause();
    if (page == Page::None)
        return;
    if (page == Page::Inventory)
        HideTrade(); // (before the game's UI pass too: its label never shows a frame)
    // (the ship view pulses its selected part by the UI's blink clock, which the pause stops: the page runs it)
    if (g_paused && page == Page::Inventory && exe::UiBlinkPhase.ok())
    {
        const u32 fn = static_cast<u32>(exe::UiBlinkPhase.get()), clock = R32(fn + 2);
        const float period = RF(R32(fn + 8));
        if (period > 0)
        {
            float c = fmodf(RF(clock) + dt, period);
            u32 bits;
            memcpy(&bits, &c, 4);
            W32(clock, bits);
        }
    }
    if (hostcalls::g.SetFlightLock)
        hostcalls::g.SetFlightLock(); // (a full-screen window: the keys and the mouse fly nothing)
    int wheel = hostcalls::g.RealMouseWheel ? hostcalls::g.RealMouseWheel() : 0;
    if (page == Page::Status)
    {
        if ((g_statusRead -= dt) <= 0)
            ReadStatus(root), g_statusRead = 0.5f;
        if (wheel)
            g_repList.Scroll(-wheel);
        ShowCard();
    }
    else if (page == Page::Inventory)
    {
        if (g_syncPending && SyncDue() && !room::NNWindowBusy())
            SyncNow(root); // (the game's selection catches up with a walk: SyncDue)
        // the game's list stands where the page's does: its five icons are the page's (RowIcons lends one to a shown row outside them)
        if (!room::NNWindowBusy())
        {
            int shown[kListMeshCount], n = 0;
            for (; n < kListMeshCount && g_itemList.first + n < static_cast<int>(g_items.size()); ++n)
                shown[n] = g_items[g_itemList.first + n].row;
            SyncList(root, shown, n);
        }
        if ((g_itemRead -= dt) <= 0)
        {
            ReadItems(root), g_itemRead = 0.5f;
            for (int i = 0; i < 5; ++i)
                if (g_filterWord[i].empty())
                {
                    g_filterWord[i] = baseinfo::HintText(uiclick::Find(kFilterControl[i]));
                    if (g_filterWord[i].empty())
                    {
                        static const wchar_t* const kWords[] = {L"WEAPONS", L"AMMO", L"EQUIPMENT", L"CARGO BAY", L"COMMODITIES"};
                        g_filterWord[i] = kWords[i];
                    }
                    for (wchar_t& c : g_filterWord[i])
                        c = towupper(c);
                }
            // (a filter or a mount clears or moves the game's selection a moment later: its card and its JETTISON follow the page's row)
            if (g_itemList.count > 0 && !room::NNWindowBusy() && !OnEmpty() && static_cast<int>(R32(root + kInvRow)) != g_items[g_itemList.sel].row)
                SelectItem(root);
        }
        if (wheel && JettisonOpen())
            uiclick::Scroll(uiclick::Find("JettisonQuantitySlider"), static_cast<float>(wheel), false, nullptr, nullptr);
        else if (wheel)
            g_itemList.Scroll(-wheel);
    }
    else if (page == Page::Log)
    {
        wchar_t obj[256] = {};
        g_objective = HostObjectiveText(obj, 256) ? obj : L"";
    }
    if ((page == Page::Info || page == Page::Log) && (g_placesRead -= dt) <= 0)
        ReadPlaces(), g_placesRead = 2.0f;
}

void Draw()
{
    if (basepage::On())
    {
        basepage::Draw();
        return;
    }
    if (g_paused) // (never a hidden state: the word stands where the eye looks for the time)
    {
        const Frame f = Bounds();
        Text(f.right, f.titleY + (g_page == Page::None && !g_pad ? 0.06f * f.H : 0), L"PAUSED", 0.030f * f.H, Argb(kDim, 1), Right); // (under the map's LAYERS chip)
    }
    if (g_page == Page::None)
    {
        // CHAT and GROUP (multiplayer): no family knows them, the game's window stands on the same ground under the same title
        const int cmd = nnstrip::Wanted() ? nnwin::OpenCommand() : 0;
        if (cmd == nntabs::kCmdChat || cmd == nntabs::kCmdGroup)
            Title(Bounds(), cmd == nntabs::kCmdChat ? L"CHAT" : L"GROUP");
        // Between two windows (the old one's close, the new one's open: about a second) no root of a page is shown yet, and the room, the NeuroNet box and the
        // windows' slides stood under the scrim (issue 62): the page's ground and title stand for the tab already chosen. Not on the death screen (its own page).
        // The map has no page: its gap is the windows' slides (the map grows to its own ground the frame they end, navnew Grow), and its title waits for the map,
        // which may open on another view than the player's system.
        else if ((cmd == nntabs::kCmdLog || cmd == nntabs::kCmdInventory || cmd == nntabs::kCmdStatus || cmd == nntabs::kCmdInfo ||
                  (cmd == nntabs::kCmdMap && room::NNWindowBusy())) &&
                 !uiclick::FindVisible("LoadAutosaveButton"))
        {
            const Frame f = Bounds();
            Ground(0, 0, f.W, f.H);
            const float lane = deckmode::LaneTopH() * f.H; // (the pad's CLOSE stands at the right edge, where the page's prompts end: as below)
            winplates::g_keep.push_back({f.left, lane, f.left + 0.02f * f.H, lane + 0.02f * f.H});
            const std::wstring ship = cmd == nntabs::kCmdInventory ? Upper(baseinfo::ShipName()) : std::wstring();
            if (cmd != nntabs::kCmdMap)
                Title(f, cmd == nntabs::kCmdLog ? L"NEURAL NET LOG" : cmd == nntabs::kCmdStatus ? L"PLAYER STATUS" : cmd == nntabs::kCmdInfo ? L"CURRENT INFORMATION" : ship.empty() ? L"INVENTORY" : ship);
        }
        return;
    }
    const Frame f = Bounds();
    // (the prompts stand at the right edge, as on the map: something holds the lane's left)
    const float lane = deckmode::LaneTopH() * f.H;
    winplates::g_keep.push_back({f.left, lane, f.left + 0.02f * f.H, lane + 0.02f * f.H});
    switch (g_page)
    {
    case Page::Status: DrawStatus(f); break;
    case Page::Inventory: DrawInventory(f, g_root); break;
    case Page::Log: DrawLog(f, g_root); break;
    case Page::Info: DrawInfo(f); break;
    case Page::Death: DrawDeath(f); break;
    default: break;
    }
}

void Under()
{
    if (g_page == Page::Inventory)
        HideTrade();
    // the rows' item models, between the game's row fill and the UI pass that draws them (nnpage_kit.h RowIcons); a window gone since the page drew sends nothing
    if (g_page == Page::Inventory && g_root && uiclick::FindVisible("NN_Inventory") == g_root)
        g_invIcons.Send(g_root);
    else
        g_invIcons = ListIcons();
    basepage::IconsUnder();
    LentSweep(); // (a lent model no page sent this frame: its mesh's true name back)
    // STATUS: the card's window brings its X along (PlayerInfoCloseButton), cut at the page's top right (Shockwave's shots, the owner, 7 Nov 2026): off the page,
    // which closes by its own CLOSE. So does the window's own title, PlayerInfoMainTitle (IDS 931 PLAYER STATUS: its STATUS stood cut by the top edge of the
    // card's hole); the page's title says it. The card's heading, the faction's name, is the card's own and stays (docs\FREELANCER_RE.md "Base windows").
    if (g_page == Page::Status)
        for (const char* n : {"PlayerInfoCloseButton", "PlayerInfoMainTitle"})
            PlaceAt(uiclick::FindVisible(n), -static_cast<float>(fl::ScreenWidth()), 0);
}

void DrawAfter()
{
    basepage::DrawAfter();
    if (g_page == Page::Inventory && g_shipView && !JettisonOpen())
        hudvis::DrawShipAt(g_shipRect[0], g_shipRect[1], g_shipRect[2], g_shipRect[3], g_itemList.count > 0 && g_itemList.sel < static_cast<int>(g_items.size()) && g_items[g_itemList.sel].mounted ? g_items[g_itemList.sel].id : 0);
}

bool On() { return g_page != Page::None || basepage::On(); }

int Prompts(views::PagePrompt* out, int max)
{
    using namespace glyphs;
    if (basepage::On())
        return basepage::Prompts(out, max);
    int n = 0;
    auto add = [&](int id, const wchar_t* word, int id2 = -1)
    {
        if (n < max)
            out[n++] = {id, id2, word, false};
    };
    switch (g_page)
    {
    case Page::Status:
        add(DU, L"FACTION", DD); // (the right stick walks them too; the card has nothing to scroll)
        {
            static std::wstring next;
            next = L"SHOW ";
            next += FilterWord(static_cast<nnrules::Filter>((g_filter + 1) % nnrules::kFilters));
            add(X, next.c_str());
        }
        add(Y, g_byName ? L"SORT BY STANDING" : L"SORT BY NAME");
        break;
    case Page::Inventory:
    {
        add(DL, L"FILTER", DR);
        if (g_itemList.count > 1)
            add(DU, L"ITEM", DD);
        if (g_itemList.count > 0 && Mountable(Filter(g_root)) && (!OnEmpty() || ForEmpty()))
            add(A, g_items[g_itemList.sel].mounted ? L"UNMOUNT" : L"MOUNT");
        if (uiclick::FindVisible("JettisonButton") && !OnEmpty())
            add(Y, g_jettisonArmed ? L"AGAIN TO JETTISON" : L"JETTISON");
        if (TradeOffered())
            add(X, L"BEST PRICES");
        add(RS, L"SCROLL CARD");
        if (CanTagInv()) // (last: a full lane drops it, the card says it too)
            add(R3, TaggedInv() ? L"CLICK UNTAG" : L"CLICK COMPARE WITH");
        break;
    }
    case Page::Log:
        add(DL, L"MISSION / JOURNAL", DR);
        add(DU, L"SCROLL", DD);
        if (!g_objective.empty())
            add(Y, L"SHOW ON MAP");
        break;
    case Page::Info:
        add(DU, L"SCROLL", DD);
        add(Y, L"SHOW ON MAP");
        break;
    default: break;
    }
    return n;
}

unsigned Pad(unsigned pressed, unsigned held, float dt)
{
    if (basepage::On())
        return basepage::Pad(pressed, held, dt);
    if (pressed & (XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_B | XINPUT_GAMEPAD_X | XINPUT_GAMEPAD_Y))
        hostcalls::WorldCommand(); // (what the press asks of the game gets its frame: Update's note on the pause)
    // STATUS: the right stick walks the factions as the D-pad does - its card has no scroll of its own, the stick moved the game's list under the page (Shockwave, issue 68)
    if (g_page == Page::Status)
    {
        static unsigned rsWas = 0;
        const unsigned rs = (held & views::kPagePadRsUp) ? XINPUT_GAMEPAD_DPAD_UP : (held & views::kPagePadRsDown) ? XINPUT_GAMEPAD_DPAD_DOWN : 0;
        pressed |= rs & ~rsWas;
        held |= rs;
        rsWas = rs;
    }
    // a held D-pad walks a list: 0.3 s, then 20 rows a second (0.4 s / 12.5 a second: "could be a little faster", Shockwave, issue 68)
    static float repeat = 0;
    const unsigned walk = XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_DPAD_DOWN;
    if ((held & walk) && (g_page == Page::Status || g_page == Page::Inventory)) // (STATUS's card stands as long as the page: it blocked the repeat there, issue 68)
    {
        repeat += dt;
        if (repeat > 0.3f)
            pressed |= held & walk, repeat -= 0.05f;
    }
    else
        repeat = 0;
    switch (g_page)
    {
    case Page::Status: return PadStatus(pressed);
    case Page::Inventory: return JettisonOpen() ? 0 : PadInventory(g_root, pressed);
    case Page::Log:
        if (pressed & XINPUT_GAMEPAD_DPAD_LEFT)
            LogTab(1);
        if (pressed & XINPUT_GAMEPAD_DPAD_RIGHT)
            LogTab(0);
        if ((pressed & XINPUT_GAMEPAD_Y) && !g_objective.empty())
            nnwin::Go(nntabs::kCmdMap);
        return XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_DPAD_RIGHT | XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_X | XINPUT_GAMEPAD_Y;
    case Page::Info:
        if (pressed & XINPUT_GAMEPAD_Y)
            nnwin::Go(nntabs::kCmdMap);
        return XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_DPAD_RIGHT | XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_X | XINPUT_GAMEPAD_Y;
    default: return 0;
    }
}

void Shutdown()
{
    LentSweep(true); // (the game's own window must not keep a lent model under another name)
    g_page = Page::None;
    g_root = 0;
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
        } kMap[] = {{"a", XINPUT_GAMEPAD_A}, {"b", XINPUT_GAMEPAD_B}, {"x", XINPUT_GAMEPAD_X}, {"y", XINPUT_GAMEPAD_Y}, {"up", XINPUT_GAMEPAD_DPAD_UP}, {"down", XINPUT_GAMEPAD_DPAD_DOWN}, {"left", XINPUT_GAMEPAD_DPAD_LEFT}, {"right", XINPUT_GAMEPAD_DPAD_RIGHT}};
        for (const auto& k : kMap)
            if (b == k.n)
                return "nnpage: took 0x" + std::to_string(Pad(k.m, 0, 0));
        return "nnpage press <a|b|x|y|up|down|left|right>";
    }
    std::ostringstream o;
    o << "nnpage page=" << static_cast<int>(g_page) << " root=0x" << std::hex << g_root << std::dec << " pad=" << g_pad << " reps=" << g_reps.size() << " shown=" << g_repList.count << " sel=" << g_repList.sel
      << " paused=" << g_paused << " card=" << g_card << " cardFor=" << (g_repList.count > 0 && g_repList.sel < static_cast<int>(g_repShown.size()) ? static_cast<int>(g_repShown[g_repList.sel].index == g_cardShown) : -1) << " syncPending=" << g_syncPending << " places=" << g_places.size() << " guns=" << g_load.guns << " hullps=" << g_load.hull << " items=" << g_items.size() << " itemSel=" << g_itemList.sel;
    if (g_page == Page::Inventory && g_root)
        o << " filter=" << Filter(g_root) << " gameRow=" << static_cast<int>(R32(g_root + kInvRow)) << " tradeBlanked=" << g_tradeBlanked << " gameFirst=" << static_cast<int>(R32(g_root + kInvFirst));
    o << " shownRows=";
    for (int i = 0; i < g_invIcons.n; ++i)
        o << (i ? "," : "") << g_invIcons.shown[i].gameRow;
    o << " iconSends=" << g_iconSends << " iconLoads=" << g_iconLoads << " iconCatchUps=" << g_iconCatchUps << " iconHidden=" << g_iconHidden; // (both pages: nnpage_kit.h RowIcons)
    int lent = 0;
    for (const LentModel& l : g_lent)
        lent += l.file != nullptr;
    o << " lent=" << lent;
    for (int i = 0; i < 13; ++i)
        if (!g_stat[i].empty())
            o << " s" << i << "=" << g_stat[i].size();
    return o.str();
}
} // namespace nnpage
