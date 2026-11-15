// FLvibed - the dealers' proving experiment (audit 70b work order W4, results audit_2026-11-12\78_dealers_probe.md): the debug
// verb `dealprobe`, test tooling only, reached through the debug channel (which exists only with the test switch,
// core\common.h TestSwitch) and refusing anyway without it.
//
// It answers, for the commodity trader and the equipment dealer, from the client's own data and with NO dealer window,
// what the game's dealer window would list, charge, credit and refuse, and it buys and sells by the same calls, in the
// same order and with the same arguments as the window's handlers (NN_Dealer 0x4822A0 -> buy 0x47EAB0 / sell 0x47EEA0,
// read in Ghidra: audit 78a, the checks 78b). With a dealer window open it reads the window's own answers beside them,
// so the two can be compared line by line. It is the harness the dealers page family is checked with.
//
//   dealprobe state [good]                   cash, level, frame, the client's equipment list (and a good's stock)
//   dealprobe snap                           the visit's snapshot: the item ids held now (the window's +0x3A8)
//   dealprobe list trader|equip [filter]     the rows from data: good, name, stock, price, level / standing asked,
//                                            the "may I" (reason, IDS, quantity); then the player's items and credits
//   dealprobe window                         the open dealer window's own rows, prices and selected row's refusal
//   dealprobe compare                        list vs window for the open window's mode and filter: EQUAL or the lines
//   dealprobe select <good> | selectheld <id>  the open window's own row selection (InvSelectRow), as a row click;
//                                            the window can put its selection back on row 0 by itself in its first
//                                            seconds (seen up to ~60 frames after a select): check `window` just
//                                            before a click (audit 78_)
//   dealprobe may trader|equip <good>      own refusal for buying `good` (the window's, for its selected row: `window`)
//   dealprobe maysell trader|equip <id>      own refusal for selling item `id`
//   dealprobe buy trader|equip <good> [n]    the direct purchase (refused before any send when the window would refuse)
//   dealprobe sell trader|equip <id> [n]     the direct sale
// Single player only (multiplayer stays on the game's own button until a test-server run proves the route).
#include "common.h"
#include "flexe.h"
#include "hook.h"
#include "log.h"
#include "safemem.h"
#include "game/baseinfo.h"
#include "game/coreframe.h"
#include "game/gamedata.h"
#include "game/hudinfo.h"
#include "game/room.h"
#include "game/space.h"
#include "text.h"

#include <algorithm>
#include <sstream>
#include <type_traits>
#include <vector>

namespace debug
{
namespace
{
// ---- guarded reads
u32 R32(uintptr_t a)
{
    u32 v = 0;
    return a && safemem::Read(a, &v, 4) ? v : 0;
}
float RF(uintptr_t a)
{
    float v = 0;
    return a && safemem::Read(a, &v, 4) ? v : 0.0f;
}
u8 R8(uintptr_t a)
{
    u8 v = 0;
    return a && safemem::Read(a, &v, 1) ? v : 0;
}
u16 R16(uintptr_t a)
{
    u16 v = 0;
    return a && safemem::Read(a, &v, 2) ? v : 0;
}

// ---- Common.dll: the functions the dealer window imports (by export name: the same functions)
struct Common
{
    const u8*(__cdecl* findById)(u32) = nullptr;            // GoodList::find_by_id: GoodInfo (+0x4C equipment flag, +0x50 arch, +0xB8 / +0xBC free ammo)
    u32(__cdecl* good2arch)(u32) = nullptr;
    u32(__cdecl* arch2good)(u32) = nullptr;
    const u8*(__cdecl* getEquip)(u32) = nullptr;            // Archetype::GetEquipment (+0x5C volume; vt +0x10 its type)
    const u8*(__cdecl* getShip)(u32) = nullptr;             // Archetype::GetShip (+0x100 hold, +0x140 / +0x144 nanobot / battery limits)
    bool(__cdecl* combinable)(u32) = nullptr;               // arch_is_combinable
    const int* maxAmmo = nullptr;                           // MAX_PLAYER_AMMO
    const u8*(__thiscall* matchingCargo)(const void*, u32, int, float) = nullptr; // EquipDescList::find_matching_cargo
    const u8*(__thiscall* findItem)(const void*, u16) = nullptr;                  // EquipDescList::find_equipment_item (const)
    float(__thiscall* occupied)(const void*) = nullptr;                           // EquipDescList::get_cargo_space_occupied
    u32(__cdecl* createId)(const char*) = nullptr;
    bool(__cdecl* singlePlayer)() = nullptr;
    void(__thiscall* cacheClear)(void*) = nullptr;         // CacheString::clear
    char*(__cdecl* stringAlloc)(const char*, bool) = nullptr; // StringAlloc
    bool ok = false;
};
const Common& C()
{
    static Common c = []
    {
        Common r;
        HMODULE m = GetModuleHandleA("common.dll");
        if (!m)
            return r;
        auto get = [m](auto& p, const char* n) { p = reinterpret_cast<std::remove_reference_t<decltype(p)>>(GetProcAddress(m, n)); };
        get(r.findById, "?find_by_id@GoodList@@YAPBUGoodInfo@@I@Z");
        get(r.good2arch, "?Good2Arch@@YAII@Z");
        get(r.arch2good, "?Arch2Good@@YAII@Z");
        get(r.getEquip, "?GetEquipment@Archetype@@YAPAUEquipment@1@I@Z");
        get(r.getShip, "?GetShip@Archetype@@YAPAUShip@1@I@Z");
        get(r.combinable, "?arch_is_combinable@@YA_NI@Z");
        get(r.maxAmmo, "?MAX_PLAYER_AMMO@@3HA");
        get(r.matchingCargo, "?find_matching_cargo@EquipDescList@@QBEPBUEquipDesc@@IHM@Z");
        get(r.findItem, "?find_equipment_item@EquipDescList@@QBEPBUEquipDesc@@G@Z");
        get(r.occupied, "?get_cargo_space_occupied@EquipDescList@@QBEMXZ");
        get(r.createId, "?CreateID@@YAIPBD@Z");
        get(r.singlePlayer, "?SinglePlayer@@YA_NXZ");
        get(r.cacheClear, "?clear@CacheString@@QAEXXZ");
        get(r.stringAlloc, "?StringAlloc@@YAPADPBD_N@Z");
        r.ok = r.findById && r.good2arch && r.arch2good && r.getEquip && r.getShip && r.combinable && r.maxAmmo &&
               r.matchingCargo && r.findItem && r.occupied && r.createId && r.singlePlayer && r.cacheClear && r.stringAlloc;
        return r;
    }();
    return c;
}

// Equipment's type (its vtable +0x10: 0xC engine, 0x11 launcher, 0x1F munition, ...), -1 none.
int EquipType(const u8* eq)
{
    if (!eq)
        return -1;
    const u32 fn = R32(R32(reinterpret_cast<uintptr_t>(eq)) + 0x10);
    return fn ? reinterpret_cast<int(__thiscall*)(const void*)>(fn)(eq) : -1;
}
float Volume(u32 arch)
{
    const u8* eq = arch ? C().getEquip(arch) : nullptr;
    return eq ? RF(reinterpret_cast<uintptr_t>(eq) + 0x5C) : 0.0f;
}

// ---- the client's stores
const void* ClientList() { return exe::PlayerEquipList.ok() ? reinterpret_cast<const void*>(exe::PlayerEquipList.get() - 4) : nullptr; }
struct Desc
{
    u16 id = 0;
    u32 arch = 0;
    std::string hp;
    bool mounted = false;
    float health = 0;
    int count = 0;
    u8 mission = 0;
    uintptr_t at = 0; // the EquipDesc
};
Desc ReadDesc(uintptr_t d)
{
    Desc e;
    e.at = d;
    e.id = R16(d + 2);
    e.arch = R32(d + 4);
    if (const u32 s = R32(d + 8))
    {
        char buf[64] = {};
        safemem::Read(s, buf, sizeof(buf) - 1);
        e.hp = buf;
    }
    e.mounted = R8(d + 0xC) != 0;
    e.health = RF(d + 0x10);
    e.count = static_cast<int>(R32(d + 0x14));
    e.mission = R8(d + 0x18);
    return e;
}
// An MSVCP60 list of EquipDesc: [list + 4] its head node, node +0 next, +8 the EquipDesc.
std::vector<Desc> ListItems(uintptr_t list)
{
    std::vector<Desc> out;
    const u32 head = R32(list + 4);
    u32 n = R32(head);
    for (int i = 0; head && n && n != head && i < 512; ++i, n = R32(n))
        out.push_back(ReadDesc(n + 8));
    return out;
}
std::vector<Desc> Held() { return ClientList() ? ListItems(reinterpret_cast<uintptr_t>(ClientList())) : std::vector<Desc>(); }
int HeldCargo(u32 arch) // find_matching_cargo(arch, 0, 1.0): the first stack of that arch that is not mission cargo
{
    const u8* d = arch && ClientList() ? C().matchingCargo(ClientList(), arch, 0, 1.0f) : nullptr;
    return d ? static_cast<int>(R32(reinterpret_cast<uintptr_t>(d) + 0x14)) : 0;
}
u32 Cash() { return Mem<u32>(exe::PlayerCash); }
u32 ShipArch() { return Mem<u32>(exe::PlayerShipArch); }
const u8* ShipData() { return ShipArch() ? C().getShip(ShipArch()) : nullptr; }
float HoldCap() { const u8* s = ShipData(); return s ? RF(reinterpret_cast<uintptr_t>(s) + 0x100) : 0.0f; } // 0x4C4610
float HoldUsed() { return ClientList() ? C().occupied(ClientList()) : 0.0f; }                                  // 0x4C4640
u32 Base() { return Mem<u32>(exe::PlayerBase); }

// ---- the market (the client's: MarketOf on the docked base; the dealers lower and raise its stock themselves)
u32 Market()
{
    const u32 base = Base();
    if (!base || !exe::MarketOf.ok() || !exe::RoomInfoOwner.ok())
        return 0;
    return hk::Call<u32(__thiscall*)(u32, u32)>(exe::MarketOf, static_cast<u32>(exe::RoomInfoOwner.get()), base);
}
uintptr_t Mgi(u32 market, u32 good) { return market && good ? hk::Call<u32(__thiscall*)(u32, u32)>(exe::MarketGood, market, good) : 0; }
int Stock(uintptr_t mgi) { return mgi ? static_cast<int>(R32(mgi + 0x1C)) : 0; }
int UnitPrice(u32 market, u32 good) // ftol(0x438060): the base's price, no ammo
{
    return market && good ? static_cast<int>(hk::Call<float(__thiscall*)(u32, u32)>(exe::MarketGoodPrice, market, good)) : 0;
}
// The market's goods in its map order (ascending good id): a DFS of the MSVCP60 tree (market +4 the head, head +4 the root;
// a node +0 left, +8 right, +0xC the key, +0x10 the MarketGoodInfo; the shared nil node's left is 0).
std::vector<std::pair<u32, uintptr_t>> MarketGoods(u32 market)
{
    std::vector<std::pair<u32, uintptr_t>> out;
    const u32 head = R32(market + 4);
    std::vector<u32> stack;
    if (head && R32(head + 4))
        stack.push_back(R32(head + 4));
    while (!stack.empty() && out.size() < 4096)
    {
        const u32 n = stack.back();
        stack.pop_back();
        if (!n || n == head || R32(n) == 0)
            continue;
        out.emplace_back(R32(n + 0xC), n + 0x10);
        stack.push_back(R32(n));
        stack.push_back(R32(n + 8));
    }
    std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    return out;
}

// ---- the price (0x47F040 without the window: its window reads are the client list it copies)
int BuyPrice(u32 market, u32 good)
{
    int p = UnitPrice(market, good);
    const u8* gi = C().findById(good);
    if (!gi)
        return p;
    const uintptr_t g = reinterpret_cast<uintptr_t>(gi);
    const u32 ammoArch = R32(g + 0xB8);
    const int ammoCount = static_cast<int>(R32(g + 0xBC));
    if (!ammoArch || ammoCount <= 0)
        return p;
    const u32 ammoGood = C().arch2good(ammoArch);
    const int pa = UnitPrice(market, ammoGood);
    const uintptr_t m = Mgi(market, ammoGood);
    int n = !m || Stock(m) <= 0 ? 0 : std::min(ammoCount, Stock(m));
    const float vL = Volume(C().good2arch(good)), vA = Volume(C().good2arch(ammoGood));
    if (vL > 0 && vA > 0)
        n = std::min(n, std::max(0, static_cast<int>((HoldCap() - HoldUsed() - vL) / vA)));
    n = std::min(n, *C().maxAmmo - HeldCargo(ammoArch));
    return p + n * pa;
}

// ---- the "may I" (0x4837A0 then 0x47FEF0's buy branch, from data)
const int kIds[12] = {0, 1150, 1151, 1152, 1168, 1563, 458782, 1591, 1599, 932, 933, 975};
int IdsOf(int reason) { return reason > 0 && reason < 12 ? kIds[reason] : 0; }
struct May
{
    int qty = 0, reason = 0;
};
// The level with no window: the server's rank (pub::Player::GetRank, single player). Not the client's copy the window
// asks for (ClientRank): seen in game, a fresh start on a rank-2 save read 0 there until the first dealer window opened,
// which set it to 2. The client's copy only where the server's is unreadable.
int Level()
{
    const int s = baseinfo::PlayerLevel();
    return s >= 0 ? s : Mem<int>(exe::ClientRank);
}
float Standing() { return exe::BaseOwnerStanding.ok() ? hk::Call<float(__cdecl*)()>(exe::BaseOwnerStanding) : 1.0f; }
May MayBuy(u32 market, u32 good, int filter, float standing, int level)
{
    May r;
    if (!ShipArch())
        return r.reason = 3, r;
    const uintptr_t mgi = Mgi(market, good);
    const int stock = Stock(mgi);
    if (!good || !mgi || stock < 1)
        return r;
    const int price = BuyPrice(market, good);
    const u32 q = price > 0 ? Cash() / static_cast<u32>(price) : 0;
    if (static_cast<int>(q) < 1)
        r.reason = 2, r.qty = 0;
    const u32 arch = C().good2arch(good);
    const gamedata::Versus fit = gamedata::Against(arch);
    bool viaHardpoint = false;
    if (r.reason == 0)
    {
        if ((filter == 0 || filter == 2) && fit.freeHp)
            viaHardpoint = true, r.qty = std::min(static_cast<int>(q), std::min(stock, 1));
        else
        {
            const float vol = Volume(arch);
            const int room = vol != 0.0f ? static_cast<int>((HoldCap() - HoldUsed()) / vol) : stock;
            int qty = std::max(0, std::min({room, static_cast<int>(q), stock}));
            const u32 nano = Mem<u32>(exe::NanobotArch), batt = Mem<u32>(exe::BatteryArch);
            const bool limited = filter == 3 && (arch == nano || arch == batt);
            if ((filter == 0 || filter == 2 || filter == 3) && !limited && qty > 1)
                qty = 1;
            if (limited && ShipData())
            {
                const int lim = static_cast<int>(R32(reinterpret_cast<uintptr_t>(ShipData()) + (arch == nano ? 0x140 : 0x144)));
                const int avail = lim - HeldCargo(arch);
                if (avail < qty && lim >= 0)
                    qty = avail;
                if (qty <= 0)
                    r.reason = arch == nano ? 9 : 10, qty = 0;
            }
            if (filter == 1)
            {
                const int lim = *C().maxAmmo - HeldCargo(arch);
                if (lim < qty)
                    qty = lim;
                if (qty < 1)
                    r.reason = 0xB, qty = 0;
            }
            if (r.reason == 0 && room < 1)
                r.reason = 1, qty = 0;
            r.qty = r.reason ? 0 : qty;
        }
    }
    (void)viaHardpoint;
    // 0x47FEF0: level, then standing, only after reasons 0 / 2; then the hardpoint for filters 0 / 2
    if (r.reason == 0 || r.reason == 2)
    {
        const float reqLevel = RF(mgi + 0x14), reqRep = RF(mgi + 0x18);
        if (C().singlePlayer() && level >= 0 && level < 999 && static_cast<float>(level) < reqLevel)
            r.reason = 5, r.qty = 0;
        else if (standing < reqRep)
            r.reason = 6, r.qty = 0;
        if (filter == 0 || filter == 2)
        {
            if (!fit.fits)
                r.reason = 7, r.qty = 0;
            else if (r.qty > 0 && !fit.freeHp)
                r.reason = 8, r.qty = 0;
        }
    }
    return r;
}
// The sale's refusal (0x47FEF0's sell branch): 1397 mission cargo, then 1324 an engine (the later write wins); 0 = may.
int MaySell(const Desc& d, bool trader)
{
    const int cls = hk::Call<int(__cdecl*)(u32)>(exe::DealerClassOf, d.arch);
    if (cls == 5 || (trader && cls != 4) || d.count < 1)
        return -1; // not listed: nothing to sell
    int ids = 0;
    if (d.mission)
        ids = 1397;
    if (EquipType(C().getEquip(d.arch)) == 0xC)
        ids = 1324;
    return ids;
}

// ---- the visit's snapshot (the window takes the client list's copy when it opens: NN_Inventory +0x3A8)
struct Snap
{
    u32 base = 0;
    std::vector<u16> ids;
} g_snap;
void TakeSnap()
{
    g_snap.base = Base();
    g_snap.ids.clear();
    for (const Desc& d : Held())
        g_snap.ids.push_back(d.id);
}
bool InSnap(u16 id)
{
    if (g_snap.base != Base())
        TakeSnap();
    return std::find(g_snap.ids.begin(), g_snap.ids.end(), id) != g_snap.ids.end();
}
// The sale's credit (0x47EEA0): full for a commodity, a combinable good or an item not held when the visit's snapshot was
// taken; else (0.3 x price + 0.499999) x health x count, each step in float as the game's x87 code, truncated.
int Credit(u32 market, const Desc& d, int count, bool reducedTest)
{
    const u32 good = C().arch2good(d.arch);
    const float p = market ? hk::Call<float(__thiscall*)(u32, u32)>(exe::MarketGoodPrice, market, good) : 0.0f;
    const int cls = hk::Call<int(__cdecl*)(u32)>(exe::DealerClassOf, d.arch);
    if (cls == 4 || C().combinable(d.arch) || !reducedTest)
        return static_cast<int>(p * static_cast<float>(count));
    const float unit = p * 0.3f + 0.499999f;
    return static_cast<int>(unit * (d.health * static_cast<float>(count)));
}

// ---- the open dealer window: the control named NN_Dealer and its NN_Inventory partner (+0x974)
struct Win
{
    u32 dealer = 0, inv = 0;
    int mode = 0; // +0x978: 1 commodity trader, 3 equipment dealer, 2 the ship dealer's final stage
};
Win OpenWindow()
{
    Win w;
    u32 d = uiclick::FindVisible("NN_Dealer");
    if (!d)
        d = uiclick::Find("NN_Dealer");
    if (d && exe::VtDealer.is(R32(d)) && exe::VtInventory.is(R32(R32(d + 0x974))))
        w.dealer = d;
    if (w.dealer)
        w.inv = R32(w.dealer + 0x974), w.mode = static_cast<int>(R32(w.dealer + 0x978));
    return w;
}

// ---- rows
struct Row
{
    u32 good = 0;
    std::wstring name;
    int stock = 0, price = 0, avail = 1;
    float level = 0, rep = 0;
};
// The window's mode-3 middle keys (0x483230): the equipment's type, then a flag (launchers with special ammo, some munitions).
int TypeKey(u32 good) { return EquipType(C().getEquip(C().good2arch(good))); }
int FlagKey(u32 good)
{
    const u8* eq = C().getEquip(C().good2arch(good));
    const uintptr_t e = reinterpret_cast<uintptr_t>(eq);
    const int t = EquipType(eq);
    if (t == 0x11)
    {
        if (R8(e + 0xBA))
            return 1;
        const u32 ammoName = R32(e + 0x98); // the launcher's projectile archetype id (ID_String)
        const u8* am = ammoName ? C().getEquip(ammoName) : nullptr;
        return am && EquipType(am) == 0x1F && R32(reinterpret_cast<uintptr_t>(am) + 0x90) ? 1 : 0;
    }
    if (t == 0x1F)
    {
        const u32 k = R32(e + 0xA8);
        return k == 10 || k == 0x23 ? 1 : 0;
    }
    return 0;
}
// The ammunition row's availability (0x482D80, run on the player's list for filter 1): an item held (mounted or not) that is a
// launcher, mine dropper or countermeasure dropper (type 0x11 / 0x12 / 0x13) whose projectile (+0x98) is this ammunition.
bool LauncherHeldFor(u32 ammoArch)
{
    for (const Desc& d : Held())
    {
        const u8* eq = C().getEquip(d.arch);
        const int t = EquipType(eq);
        if (t != 0x11 && t != 0x12 && t != 0x13)
            continue;
        const u32 ammoName = R32(reinterpret_cast<uintptr_t>(eq) + 0x98);
        const u8* am = ammoName ? C().getEquip(ammoName) : nullptr;
        if (am && R32(reinterpret_cast<uintptr_t>(am) + 8) == ammoArch)
            return true;
    }
    return false;
}
// The dealer's rows from data (0x437F70 + the window's class filter + its sort), no window.
std::vector<Row> DataRows(bool trader, int filter, float standing, int level)
{
    std::vector<Row> rows;
    const u32 market = Market();
    for (const auto& [good, mgi] : MarketGoods(market))
    {
        const u8* gi = C().findById(good);
        if (Stock(mgi) <= 0 || !gi || R32(reinterpret_cast<uintptr_t>(gi) + 0x4C) != (trader ? 0u : 1u))
            continue;
        const u32 arch = R32(reinterpret_cast<uintptr_t>(gi) + 0x50);
        if (!trader && hk::Call<int(__cdecl*)(u32)>(exe::DealerClassOf, arch) != filter)
            continue;
        Row r;
        r.good = good, r.name = hudinfo::EquipName(arch), r.stock = Stock(mgi), r.price = BuyPrice(market, good);
        r.level = RF(mgi + 0x14), r.rep = RF(mgi + 0x18);
        if (!trader) // 0x482E00: not available when the standing or the level falls short, no hardpoint takes it, or (ammunition)
        {            // no held launcher fires it (0x482D80)
            const bool lvl = C().singlePlayer() && level >= 0 && level < 999 && static_cast<float>(level) < r.level;
            const bool hp = (filter == 0 || filter == 2) && !gamedata::Against(arch).fits;
            const bool noLauncher = filter == 1 && !LauncherHeldFor(arch);
            r.avail = standing < r.rep || lvl || hp || noLauncher ? 0 : 1;
        }
        rows.push_back(r);
    }
    // bubble sorts with strict compares keep ties in map order: a stable sort on the same keys
    std::stable_sort(rows.begin(), rows.end(), [trader](const Row& a, const Row& b)
    {
        if (a.avail != b.avail)
            return a.avail > b.avail;
        if (trader)
            return _wcsicmp(a.name.c_str(), b.name.c_str()) < 0;
        const int ta = TypeKey(a.good), tb = TypeKey(b.good);
        if (ta != tb)
            return ta < tb;
        const int fa = FlagKey(a.good), fb = FlagKey(b.good);
        if (fa != fb)
            return fa < fb;
        return a.price < b.price;
    });
    return rows;
}
// The window's rows: its own selection function for each row index, its own price function.
std::vector<Row> WindowRows(const Win& w)
{
    std::vector<Row> rows;
    if (!w.dealer || !exe::DealerSelectedGood.ok() || !exe::DealerPrice.ok())
        return rows;
    const u32 market = Market(), keep = R32(w.dealer + 0x3A4);
    for (u32 k = 0; k < 256; ++k)
    {
        Mem<u32>(w.dealer + 0x3A4) = k;
        const u32 good = hk::Call<u32(__thiscall*)(u32)>(exe::DealerSelectedGood, w.dealer);
        if (!good)
            break;
        Row r;
        const uintptr_t mgi = Mgi(market, good);
        r.good = good, r.name = hudinfo::EquipName(C().good2arch(good)), r.stock = Stock(mgi);
        r.price = hk::Call<int(__thiscall*)(u32, u32)>(exe::DealerPrice, w.dealer, good);
        r.level = RF(mgi + 0x14), r.rep = RF(mgi + 0x18), r.avail = R8(mgi + 0x24);
        rows.push_back(r);
    }
    Mem<u32>(w.dealer + 0x3A4) = keep;
    return rows;
}
// The window's verdict on its selected dealer row: 0x4837A0, then 0x47FEF0's tail on the window's own fields.
May WindowMay(const Win& w, u32* goodOut)
{
    May r;
    if (!w.dealer || !exe::DealerBuyMost.ok())
        return r;
    const u32 good = R32(w.dealer + 0x3A4) == 0xFFFFFFFF ? 0 : hk::Call<u32(__thiscall*)(u32)>(exe::DealerSelectedGood, w.dealer);
    if (goodOut)
        *goodOut = good;
    if (!good)
        return r;
    r.qty = hk::Call<int(__thiscall*)(u32, u32, int*)>(exe::DealerBuyMost, w.dealer, good, &r.reason);
    const uintptr_t mgi = Mgi(Market(), good);
    if ((r.reason == 0 || r.reason == 2) && mgi)
    {
        const int l = Mem<int>(exe::ClientRank);
        if (C().singlePlayer() && l >= 0 && l < 999 && static_cast<float>(l) < RF(mgi + 0x14))
            r.reason = 5, r.qty = 0;
        else if (RF(w.dealer + 0x990) < RF(mgi + 0x18))
            r.reason = 6, r.qty = 0;
        const int f = static_cast<int>(R32(w.inv + 0x32C));
        if ((f == 0 || f == 2) && exe::DealerHardpointFor.ok())
        {
            const u32 viewer = R32(w.inv + 0x330);
            if (!hk::Call<u32(__thiscall*)(u32, u32, bool)>(exe::DealerHardpointFor, viewer, good, true))
                r.reason = 7, r.qty = 0;
            else if (r.qty > 0 && !hk::Call<u32(__thiscall*)(u32, u32, bool)>(exe::DealerHardpointFor, viewer, good, false))
                r.reason = 8, r.qty = 0;
        }
    }
    return r;
}

std::string Narrow(const std::wstring& w) { return text::Utf8(w); }
void PutRow(std::ostringstream& o, int i, const Row& r)
{
    char b[256];
    sprintf_s(b, "  row %d good=0x%08X stock=%d price=%d level=%g rep=%g avail=%d \"", i, r.good, r.stock, r.price, r.level, r.rep, r.avail);
    o << b << Narrow(r.name) << "\"\n";
}
void PutState(std::ostringstream& o, u32 good)
{
    o << "frame=" << coreframe::g_frame << " cash=" << Cash() << " rank=" << Mem<int>(exe::ClientRank) << " level=" << Level()
      << " standing=" << Standing() << " hold=" << HoldUsed() << "/" << HoldCap();
    if (good)
    {
        const uintptr_t m = Mgi(Market(), good);
        char b[200];
        sprintf_s(b, " | MGI +0x04=%g +0x10=%d +0x14=%g +0x18=%g +0x20=%g +0x24=%u price(0x438060)=%d", RF(m + 4), static_cast<int>(R32(m + 0x10)),
                  RF(m + 0x14), RF(m + 0x18), RF(m + 0x20), R8(m + 0x24), UnitPrice(Market(), good));
        o << " stock(0x" << std::hex << good << std::dec << ")=" << Stock(m) << b;
    }
    o << "\n";
    for (const Desc& d : Held())
    {
        char b[200];
        sprintf_s(b, "  item id=%u arch=0x%08X count=%d health=%.4f mounted=%d mission=%d hp=%s\n", d.id, d.arch, d.count, d.health,
                  d.mounted ? 1 : 0, d.mission, d.hp.c_str());
        o << b;
    }
}
const Desc* FindHeld(const std::vector<Desc>& held, u16 id)
{
    for (const Desc& d : held)
        if (d.id == id)
            return &d;
    return nullptr;
}

// The direct purchase: 0x47EAB0's calls with no window. Only the cargo-bay routes (commodities, ammunition, nanobots,
// batteries): an item the window mounts or installs on a hardpoint is refused here (its hardpoint choice is the window's).
std::string Buy(bool trader, u32 good, int count)
{
    std::ostringstream o;
    const u32 market = Market();
    const u8* gi = good ? C().findById(good) : nullptr;
    if (!market || !gi)
        return "dealprobe buy: no market or no such good";
    const u32 arch = R32(reinterpret_cast<uintptr_t>(gi) + 0x50);
    const int cls = hk::Call<int(__cdecl*)(u32)>(exe::DealerClassOf, arch);
    const u32 nano = Mem<u32>(exe::NanobotArch), batt = Mem<u32>(exe::BatteryArch);
    if (trader ? cls != 4 : !(cls == 1 || (cls == 3 && (arch == nano || arch == batt))))
        return "dealprobe buy: not a cargo-bay purchase at this dealer (class " + std::to_string(cls) + "): not in this probe";
    if (R32(reinterpret_cast<uintptr_t>(gi) + 0xB8))
        return "dealprobe buy: a good with free ammo: not in this probe";
    const May may = MayBuy(market, good, trader ? 4 : cls, Standing(), Level());
    if (may.reason)
        return "dealprobe buy: REFUSED before any send: reason " + std::to_string(may.reason) + " IDS " + std::to_string(IdsOf(may.reason));
    count = std::min(count, may.qty);
    if (count < 1)
        return "dealprobe buy: quantity 0";
    o << "before ";
    PutState(o, good);
    const uintptr_t mgi = Mgi(market, good);
    count = std::min(count, Stock(mgi));                                               // 1. the stock cap, the stock lowered
    Mem<u32>(mgi + 0x1C) = static_cast<u32>(Stock(mgi) - count);
    hk::Call<void(__stdcall*)(u32, u32, int)>(exe::SendGoodBuy, Base(), good, count);  // 2. GFGoodBuy
    hk::Call<void(__cdecl*)(u32, int, float)>(exe::SendAddCargo, arch, count, 1.0f);   // 3. ReqAddItem to the cargo bay
    if (!trader)                                                                        //    the equipment dealer's line
        hk::Call<void(__cdecl*)(u32, u32)>(exe::QueueNN, C().createId("loaded_into_cargo_hold"), 0u);
    const int price = BuyPrice(market, good);                                           // 4. ReqChangeCash(-price x count)
    hk::Call<void(__cdecl*)(int)>(exe::SendCashSub, price * count);
    o << "sent GFGoodBuy(base 0x" << std::hex << Base() << ", good 0x" << good << std::dec << ", " << count << "), ReqAddItem(0x"
      << std::hex << arch << std::dec << ", cargo bay, " << count << ", 1.0), " << (trader ? "" : "QueueNN(loaded_into_cargo_hold), ")
      << "ReqChangeCash(-" << price * count << ")\nafter ";
    PutState(o, good);
    Log("dealprobe buy: good 0x%08X x%d price %d", good, count, price);
    return o.str();
}
// The direct sale: 0x47EEA0's calls with no window (weapons excluded: their sale also edits the HUD's weapon groups).
std::string Sell(bool trader, u16 id, int count)
{
    std::ostringstream o;
    const u32 market = Market();
    const std::vector<Desc> held = Held();
    const Desc* d = FindHeld(held, id);
    if (!market || !d)
        return "dealprobe sell: no market or no such item";
    const int cls = hk::Call<int(__cdecl*)(u32)>(exe::DealerClassOf, d->arch);
    if (cls == 0)
        return "dealprobe sell: a weapon (its sale edits the weapon groups, 0x4F6380): not in this probe";
    const int refuse = MaySell(*d, trader);
    if (refuse)
        return "dealprobe sell: REFUSED before any send: IDS " + std::to_string(refuse);
    const bool reduced = InSnap(d->id);
    const u32 good = C().arch2good(d->arch);
    const uintptr_t mgi = Mgi(market, good);
    if (!mgi)
        return "dealprobe sell: the base does not trade it";
    o << "before ";
    PutState(o, good);
    count = std::min(count, d->count);                                                  // 1. capped at the stack, stock raised
    Mem<u32>(mgi + 0x1C) = static_cast<u32>(Stock(mgi) + count);
    const int credit = Credit(market, *d, count, reduced);
    hk::Call<void(__cdecl*)(int)>(exe::SendCashAdd, credit);                            // 2. ReqChangeCash(+credit)
    hk::Call<void(__stdcall*)(u32, u32, int)>(exe::SendGoodSell, Base(), good, count);  // 3. GFGoodSell
    const bool removed = hk::Call<bool(__cdecl*)(u16, int)>(exe::SendRemoveItem, id, count); // 4. ReqRemoveItem (+ the client list)
    o << "sent ReqChangeCash(+" << credit << (reduced ? ", used: held at the visit's start" : ", full") << "), GFGoodSell(good 0x" << std::hex
      << good << std::dec << ", " << count << "), ReqRemoveItem(" << id << ", " << count << ") = " << removed << "\nafter ";
    PutState(o, good);
    Log("dealprobe sell: item %u x%d credit %d", id, count, credit);
    return o.str();
}
} // namespace
} // namespace debug

// The verb (debugverbs.cpp's dispatch declares it at its call). Test tooling: refuses without the test switch and in
// multiplayer.
std::string DealProbe(std::istream& in)
{
    using namespace debug;
    if (!TestSwitch())
        return "dealprobe: test games only";
    if (space::Multiplayer())
        return "dealprobe: single player only";
    if (!C().ok || !exe::MarketOf.ok() || !exe::MarketGood.ok() || !exe::MarketGoodPrice.ok() || !exe::DealerClassOf.ok())
        return "dealprobe: a symbol or a Common.dll export is unresolved";
    if (!baseinfo::Docked())
        return "dealprobe: not docked";
    std::string what, side;
    in >> what;
    std::ostringstream o;
    const Win w = OpenWindow();
    if (what == "state")
    {
        std::string g;
        in >> g;
        PutState(o, static_cast<u32>(strtoul(g.c_str(), nullptr, 0)));
        o << "window " << (w.dealer ? (w.mode == 1 ? "trader" : w.mode == 3 ? "equip" : "other") : "none");
        return o.str();
    }
    if (what == "snap")
    {
        TakeSnap();
        return "dealprobe snap: " + std::to_string(g_snap.ids.size()) + " items held at base " + std::to_string(g_snap.base);
    }
    if (what == "select" || what == "selectheld") // the window's own row selection (InvSelectRow), as a click on the row
    {
        std::string a;
        in >> a;
        const u32 want = static_cast<u32>(strtoul(a.c_str(), nullptr, 0));
        if (!w.dealer || !exe::InvSelectRow.ok())
            return "dealprobe select: no dealer window";
        const bool mine = what == "selectheld";
        const u32 win = mine ? w.inv : w.dealer, keepRow = R32(win + 0x3A4), keepDesc = R32(win + 0x4F4);
        int found = -1;
        for (int k = 0; k < 256 && found < 0; ++k)
        {
            Mem<u32>(win + 0x3A4) = static_cast<u32>(k);
            if (mine)
            {
                Mem<u32>(win + 0x4F4) = 0;
                const u32 dd = hk::Call<u32(__fastcall*)(u32)>(exe::InvSelected, win);
                if (dd && R16(dd + 2) == want)
                    found = k;
            }
            else
            {
                const u32 g = hk::Call<u32(__thiscall*)(u32)>(exe::DealerSelectedGood, win);
                if (!g)
                    break;
                if (g == want)
                    found = k;
            }
        }
        Mem<u32>(win + 0x3A4) = keepRow;
        Mem<u32>(win + 0x4F4) = keepDesc;
        if (found < 0)
            return "dealprobe select: not in the window's rows";
        hk::Call<void(__thiscall*)(u32, int, int)>(exe::InvSelectRow, win, found - static_cast<int>(R32(win + 0x3C4)), 1);
        if (!mine) // the rest of the dealer row click (0x482120, after its InvSelectRow): the quantity back to 1, the purchase's
        {          // hardpoint name (inventory +0x394) cleared and re-chosen for the new row (filters 0 / 2), the player's row
                   // deselected. Without it the name a weapon row left caps every later row at 1 (seen: ammo, nanobots).
            Mem<u32>(w.dealer + 0x988) = 1;
            C().cacheClear(reinterpret_cast<void*>(w.inv + 0x394));
            const u32 g = hk::Call<u32(__thiscall*)(u32)>(exe::DealerSelectedGood, w.dealer);
            const int f = static_cast<int>(R32(w.dealer + 0x32C));
            if (g && (f == 0 || f == 2) && exe::DealerHardpointFor.ok())
                if (const u32 hp = hk::Call<u32(__thiscall*)(u32, u32, bool)>(exe::DealerHardpointFor, R32(w.inv + 0x330), g, false))
                    Mem<u32>(w.inv + 0x394) = reinterpret_cast<u32>(C().stringAlloc(reinterpret_cast<const char*>(hp), false));
            Mem<u32>(w.inv + 0x3A4) = 0xFFFFFFFF;
            Mem<u32>(w.inv + 0x4F4) = 0;
        }
        return "dealprobe select: row " + std::to_string(found) + " -> selected row " + std::to_string(static_cast<int>(R32(win + 0x3A4)));
    }
    if (what == "window" || what == "compare")
    {
        if (!w.dealer || (w.mode != 1 && w.mode != 3))
            return "dealprobe " + what + ": no commodity trader or equipment dealer window open";
        const bool trader = w.mode == 1;
        const int filter = trader ? 4 : static_cast<int>(R32(w.dealer + 0x32C));
        const std::vector<Row> win = WindowRows(w);
        if (what == "window")
        {
            o << "window " << (trader ? "trader" : "equip") << " filter=" << filter << " standing(+0x990)=" << RF(w.dealer + 0x990)
              << " rank=" << Mem<int>(exe::ClientRank) << " rows=" << win.size() << "\n";
            for (size_t i = 0; i < win.size(); ++i)
                PutRow(o, static_cast<int>(i), win[i]);
            u32 g = 0;
            const May m = WindowMay(w, &g);
            o << "selected good=0x" << std::hex << g << std::dec << " window: reason=" << m.reason << " IDS " << IdsOf(m.reason) << " qty=" << m.qty
              << " button=" << (uiclick::FindVisible("BuySellTradeButton") ? 1 : 0) << " error=" << (uiclick::FindVisible("ErrorHelp") ? 1 : 0);
            if (g)
            {
                const May mine = MayBuy(Market(), g, filter, Standing(), Level());
                o << " | data: reason=" << mine.reason << " IDS " << IdsOf(mine.reason) << " qty=" << mine.qty
                  << ((mine.reason == 0) == (m.reason == 0) && mine.reason == m.reason && mine.qty == m.qty ? " EQUAL" : " DIFFER");
            }
            // the player's side: the window's rows (InvSelected per row) with its own snapshot test, beside the data's
            o << "\nplayer side (filter " << R32(w.inv + 0x32C) << "):\n";
            const u32 row = R32(w.inv + 0x3A4), desc = R32(w.inv + 0x4F4);
            int nothing = 0;
            for (int k = 0; k < 96 && nothing < 16; ++k)
            {
                Mem<u32>(w.inv + 0x3A4) = static_cast<u32>(k);
                Mem<u32>(w.inv + 0x4F4) = 0;
                const u32 dd = hk::Call<u32(__fastcall*)(u32)>(exe::InvSelected, w.inv);
                if (!dd)
                {
                    ++nothing;
                    continue;
                }
                nothing = 0;
                const Desc d = ReadDesc(dd);
                const bool winOld = C().findItem(reinterpret_cast<const void*>(w.inv + 0x3A8), d.id) != nullptr;
                const bool myOld = InSnap(d.id);
                char b[256];
                sprintf_s(b, "  row %d id=%u arch=0x%08X count=%d health=%.4f mounted=%d old(window)=%d old(data)=%d credit1(window rule)=%d credit1(data)=%d maysell(data)=%d \"",
                          k, d.id, d.arch, d.count, d.health, d.mounted ? 1 : 0, winOld ? 1 : 0, myOld ? 1 : 0, Credit(Market(), d, 1, winOld),
                          Credit(Market(), d, 1, myOld), MaySell(d, trader));
                o << b << Narrow(hudinfo::EquipName(d.arch)) << "\"\n";
            }
            Mem<u32>(w.inv + 0x3A4) = row;
            Mem<u32>(w.inv + 0x4F4) = desc;
            return o.str();
        }
        const float st = RF(w.dealer + 0x990);
        const std::vector<Row> mine = DataRows(trader, filter, st, Level());
        int diff = 0;
        for (size_t i = 0; i < std::max(win.size(), mine.size()); ++i)
        {
            const Row* a = i < mine.size() ? &mine[i] : nullptr;
            const Row* b = i < win.size() ? &win[i] : nullptr;
            if (a && b && a->good == b->good && a->price == b->price && a->stock == b->stock && a->level == b->level && a->rep == b->rep &&
                (trader || a->avail == b->avail))
                continue;
            ++diff;
            o << "DIFF row " << i << "\n data  ";
            if (a)
                PutRow(o, static_cast<int>(i), *a);
            o << " window";
            if (b)
                PutRow(o, static_cast<int>(i), *b);
            o << "\n";
        }
        o << (diff ? "compare: DIFFER " : "compare: EQUAL ") << mine.size() << " data rows, " << win.size() << " window rows ("
          << (trader ? "trader" : "equip") << " filter " << filter << ", standing data " << Standing() << " window " << st << ")";
        return o.str();
    }
    in >> side;
    const bool trader = side == "trader";
    if (!trader && side != "equip")
        return "dealprobe: trader|equip expected";
    if (what == "list")
    {
        int filter = trader ? 4 : 0;
        in >> filter;
        const float st = Standing();
        const int lv = Level();
        const std::vector<Row> rows = DataRows(trader, filter, st, lv);
        o << "data " << side << " filter=" << filter << " standing=" << st << " level=" << lv << " MAX_PLAYER_AMMO=" << *C().maxAmmo
          << " rows=" << rows.size() << "\n";
        for (size_t i = 0; i < rows.size(); ++i)
        {
            PutRow(o, static_cast<int>(i), rows[i]);
            const May m = MayBuy(Market(), rows[i].good, filter, st, lv);
            o << "    may: reason=" << m.reason << " IDS " << IdsOf(m.reason) << " qty=" << m.qty << "\n";
        }
        o << "held:\n";
        for (const Desc& d : Held())
        {
            const int cls = hk::Call<int(__cdecl*)(u32)>(exe::DealerClassOf, d.arch);
            if (trader ? cls != 4 : (cls == 4 || cls == 5))
                continue;
            char b[200];
            sprintf_s(b, "  id=%u arch=0x%08X class=%d count=%d health=%.4f old=%d credit1=%d maysell=%d \"", d.id, d.arch, cls, d.count, d.health,
                      InSnap(d.id) ? 1 : 0, Credit(Market(), d, 1, InSnap(d.id)), MaySell(d, trader));
            o << b << Narrow(hudinfo::EquipName(d.arch)) << "\"\n";
        }
        return o.str();
    }
    std::string a, n;
    in >> a >> n;
    const u32 arg = static_cast<u32>(strtoul(a.c_str(), nullptr, 0));
    const int count = n.empty() ? 1 : atoi(n.c_str());
    if (what == "may")
    {
        const u32 arch = C().good2arch(arg);
        const May m = MayBuy(Market(), arg, trader ? 4 : hk::Call<int(__cdecl*)(u32)>(exe::DealerClassOf, arch), Standing(), Level());
        o << "data: reason=" << m.reason << " IDS " << IdsOf(m.reason) << " qty=" << m.qty;
        return o.str();
    }
    if (what == "maysell")
    {
        const std::vector<Desc> held = Held();
        const Desc* d = FindHeld(held, static_cast<u16>(arg));
        return d ? "data: maysell IDS " + std::to_string(MaySell(*d, trader)) : "dealprobe maysell: no such item";
    }
    if (what == "buy" || what == "sell")
    {
        if (w.dealer)
            return "dealprobe " + what + ": a dealer window is open: the direct route runs with none";
        return what == "buy" ? Buy(trader, arg, count) : Sell(trader, static_cast<u16>(arg), count);
    }
    return "dealprobe: state | snap | list | window | compare | may | maysell | buy | sell";
}
