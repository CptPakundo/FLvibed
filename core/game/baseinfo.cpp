// FLvibed - Extended HUD 2's docked readers (baseinfo.h). Sources: Freelancer.exe's UI objects (fields pinned by the
// signed code sites of core\flexe.h "Extended HUD 2 readers": a site that does not match turns its reader off), server.dll
// pub:: exports (base, location, ship, hold, market price, reputation), Common.dll (archetypes), the game's string
// resources and initialworld.ini (faction names). Every object read is guarded (__try in POD-only helpers). Facts:
// docs\FREELANCER_RE.md "Extended HUD 2 readers".
#include "game/baseinfo.h"

#include "flexe.h"
#include "game/hudinfo.h"
#include "game/ini.h"
#include "game/room.h"
#include "game/shipname.h"
#include "log.h"
#include "str.h"
#include "text.h"

#include <sstream>
#include <unordered_map>

namespace baseinfo
{
namespace
{
constexpr u32 kClient = 1; // single player: the one client

struct Fns
{
    bool tried = false;
    // server.dll
    int(__cdecl* getBaseStrid)(u32&, const u32&) = nullptr;                // pub::GetBaseStridName
    u32(__cdecl* baseNick)(char*, u32, const u32&) = nullptr;               // pub::GetBaseNickname
    u32(__cdecl* locNick)(char*, u32, const u32&) = nullptr;                // pub::GetLocationNickname
    int(__cdecl* location)(const u32&, u32&) = nullptr;                     // pub::Player::GetLocation
    int(__cdecl* shipId)(const u32&, u32&) = nullptr;                       // pub::Player::GetShipID
    int(__cdecl* holdLeft)(const u32&, float&) = nullptr;                   // pub::Player::GetRemainingHoldSize
    int(__cdecl* price)(u32, u32, float&) = nullptr;                        // pub::Market::GetPrice
    int(__cdecl* playerRep)(const u32&, int&) = nullptr;                    // pub::Player::GetRep
    int(__cdecl* repGroup)(u32&, const char*) = nullptr;                    // pub::Reputation::GetReputationGroup
    int(__cdecl* feeling)(const int&, const u32&, float&) = nullptr;        // pub::Reputation::GetReputation(rep, group)
    const u8* enumCargo = nullptr;                                          // pub::Player::EnumerateCargo (its code)
    // Common.dll
    const u8*(__cdecl* getSystem)(u32) = nullptr;     // Universe::get_system
    const void*(__cdecl* getShip)(u32) = nullptr;     // Archetype::GetShip
    const void*(__cdecl* getEquip)(u32) = nullptr;    // Archetype::GetEquipment
    u32(__thiscall* rootId)(const void*) = nullptr;   // Archetype::Root::get_id
    const void*(__cdecl* getExplosion)(const u32*) = nullptr; // Archetype::GetExplosion(ID_String)
};

template <class T> void Get(T& p, HMODULE m, const char* name) { p = reinterpret_cast<T>(GetProcAddress(m, name)); }

const Fns& F()
{
    static Fns f;
    if (f.tried)
        return f;
    HMODULE c = GetModuleHandleA("common.dll"), s = GetModuleHandleA("server.dll");
    if (!c || !s)
        return f;
    f.tried = true;
    Get(f.getBaseStrid, s, "?GetBaseStridName@pub@@YAHAAIABI@Z");
    Get(f.baseNick, s, "?GetBaseNickname@pub@@YAIPADIABI@Z");
    Get(f.locNick, s, "?GetLocationNickname@pub@@YAIPADIABI@Z");
    Get(f.location, s, "?GetLocation@Player@pub@@YAHABIAAI@Z");
    Get(f.shipId, s, "?GetShipID@Player@pub@@YAHABIAAI@Z");
    Get(f.holdLeft, s, "?GetRemainingHoldSize@Player@pub@@YAHABIAAM@Z");
    Get(f.price, s, "?GetPrice@Market@pub@@YAHIIAAM@Z");
    Get(f.playerRep, s, "?GetRep@Player@pub@@YAHABIAAH@Z");
    Get(f.repGroup, s, "?GetReputationGroup@Reputation@pub@@YAHAAIPBD@Z");
    Get(f.feeling, s, "?GetReputation@Reputation@pub@@YAHABHABIAAM@Z");
    f.enumCargo = reinterpret_cast<const u8*>(GetProcAddress(s, "?EnumerateCargo@Player@pub@@YAHABIAAUCargoEnumerator@2@@Z"));
    Get(f.getSystem, c, "?get_system@Universe@@YAPBUISystem@1@I@Z");
    Get(f.getShip, c, "?GetShip@Archetype@@YAPAUShip@1@I@Z");
    Get(f.getEquip, c, "?GetEquipment@Archetype@@YAPAUEquipment@1@I@Z");
    Get(f.rootId, c, "?get_id@Root@Archetype@@QBEIXZ");
    Get(f.getExplosion, c, "?GetExplosion@Archetype@@YAPAUExplosion@1@ABUID_String@@@Z");
    Log("baseinfo: strid=%p loc=%p ship=%p hold=%p price=%p rep=%p cargo=%p sites deal=%d/%d hint=%d ship=%d/%d/%d char=%d",
        f.getBaseStrid, f.location, f.shipId, f.holdLeft, f.price, f.feeling, f.enumCargo, exe::DealMarkBuy.ok(),
        exe::DealMarkSell.ok(), exe::ControlHintSet.ok(), exe::ShipTraderStatus.ok(), exe::ShipTraderLevel.ok(),
        exe::ShipTraderAfford.ok(), exe::CharTooltipText.ok());
    return f;
}

// ---- guarded reads (POD only)
bool Read(uintptr_t a, void* out, size_t n)
{
    __try
    {
        memcpy(out, reinterpret_cast<const void*>(a), n);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}
u32 U32(uintptr_t a, u32 fail = 0)
{
    u32 v = fail;
    return Read(a, &v, 4) ? v : fail;
}
float F32(uintptr_t a, float fail = -1)
{
    float v = fail;
    return Read(a, &v, 4) ? v : fail;
}
// An archetype by id with its id checked (Root::get_id), 0 when unknown.
uintptr_t Arch(const void*(__cdecl* get)(u32), u32 id)
{
    const Fns& f = F();
    if (!get || !f.rootId || !id)
        return 0;
    __try
    {
        const void* a = get(id);
        return a && f.rootId(a) == id ? reinterpret_cast<uintptr_t>(a) : 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
}
int CallU(int(__cdecl* fn)(const u32&, u32&), u32& out)
{
    if (!fn)
        return -1;
    __try
    {
        const u32 c = kClient;
        return fn(c, out);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return -1;
    }
}
bool Nick(u32(__cdecl* fn)(char*, u32, const u32&), u32 id, char* out, u32 n)
{
    out[0] = 0;
    if (!fn || !id)
        return false;
    __try
    {
        fn(out, n, id);
        out[n - 1] = 0;
        return out[0] != 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        out[0] = 0;
        return false;
    }
}

std::wstring TrimW(std::wstring t)
{
    while (!t.empty() && iswspace(t.back()))
        t.pop_back();
    while (!t.empty() && iswspace(t.front()))
        t.erase(t.begin());
    return t;
}

// ---- the control snapshot
std::vector<Ctl> g_visible;

// ---- factions (initialworld.ini: display name -> reputation group), loaded once
std::unordered_map<std::wstring, std::vector<std::string>> g_factions; // display name -> its groups
bool g_factionsLoaded = false;
void LoadFactions()
{
    if (g_factionsLoaded)
        return;
    g_factionsLoaded = true;
    for (const ini::Section& s : ini::Read(ini::DataPath("initialworld.ini")))
    {
        if (s.type != "group")
            continue;
        const std::string* nick = s.Get("nickname");
        const std::string* ids = s.Get("ids_name");
        if (!nick || !ids)
            continue;
        const std::wstring name = TrimW(text::Ids(static_cast<u32>(strtoul(ids->c_str(), nullptr, 10))));
        if (!name.empty())
            g_factions[name].push_back(*nick); // several groups may share a name (li_n_grp / fc_ln_grp: Liberty Navy)
    }
    Log("baseinfo: %u factions from initialworld.ini", static_cast<unsigned>(g_factions.size()));
}

int FeelingFor(const char* nick)
{
    const Fns& f = F();
    if (!f.repGroup || !f.feeling || !f.playerRep)
        return -2;
    __try
    {
        u32 group = 0;
        int rep = 0;
        const u32 c = kClient;
        if (f.playerRep(c, rep) != 0 || !rep || f.repGroup(group, nick) != 0 || !group)
            return -2;
        float feel = 0;
        if (f.feeling(rep, group, feel) != 0)
            return -2;
        // the game's own thresholds for hostile / friendly (FREELANCER_RE "Extended HUD 2 readers")
        return feel <= -0.6f ? -1 : (feel >= 0.6f ? 1 : 0);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return -2;
    }
}

int Attitude(const std::wstring& faction)
{
    LoadFactions();
    const auto it = g_factions.find(faction);
    if (it == g_factions.end())
        return -2;
    // a name several groups share counts only when they all agree (else unknown: fails closed)
    int a = FeelingFor(it->second.front().c_str());
    for (const std::string& n : it->second)
        if (FeelingFor(n.c_str()) != a)
            return -2;
    return a;
}

u32 BaseStrid(u32 base)
{
    const Fns& f = F();
    if (!f.getBaseStrid)
        return 0;
    __try
    {
        u32 strid = 0;
        return f.getBaseStrid(strid, base) == 0 ? strid : 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
}

u32 SystemStrid(u32 sys)
{
    const Fns& f = F();
    if (!f.getSystem)
        return 0;
    __try
    {
        const u8* s = f.getSystem(sys);
        // ISystem: id +0x4C, strid_name +0x68 (FREELANCER_RE "HUD 2 readers")
        return s && *reinterpret_cast<const u32*>(s + 0x4C) == sys ? *reinterpret_cast<const u32*>(s + 0x68) : 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
}

float HoldLeft()
{
    const Fns& f = F();
    if (!f.holdLeft)
        return -1;
    __try
    {
        float left = -1;
        const u32 c = kClient;
        return f.holdLeft(c, left) == 0 ? left : -1;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return -1;
    }
}

float MarketPrice(u32 base, u32 good)
{
    const Fns& f = F();
    if (!f.price || !base || !good)
        return -1;
    __try
    {
        float p = -1;
        return f.price(base, good, p) == 0 && p >= 0 ? p : -1;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return -1;
    }
}

float ExplosionDamage(u32 id)
{
    const Fns& f = F();
    if (!id || !f.getExplosion)
        return 0;
    __try
    {
        const void* e = f.getExplosion(&id);
        return e ? *reinterpret_cast<const float*>(reinterpret_cast<uintptr_t>(e) + 0xC) : 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
}
} // namespace

// ================= the context strip
bool Docked() { return exe::PlayerBase.ok() && Mem<u32>(exe::PlayerBase) != 0; }

std::wstring BaseName()
{
    static std::unordered_map<u32, std::wstring> cache;
    const u32 base = Docked() ? Mem<u32>(exe::PlayerBase) : 0;
    if (!base)
        return L"";
    if (auto it = cache.find(base); it != cache.end())
        return it->second;
    const u32 strid = BaseStrid(base);
    return cache[base] = strid ? TrimW(text::Ids(strid)) : L"";
}

std::wstring RoomName()
{
    static std::unordered_map<u32, std::wstring> cache;
    if (!Docked())
        return L"";
    const Fns& f = F();
    u32 loc = 0;
    if (CallU(f.location, loc) != 0 || !loc)
        return L"";
    if (auto it = cache.find(loc); it != cache.end())
        return it->second;
    char room[128], base[128];
    if (!Nick(f.locNick, loc, room, sizeof(room)) || !Nick(f.baseNick, Mem<u32>(exe::PlayerBase), base, sizeof(base)))
        return L"";
    // the room's nickname is the base's plus "_<Room>" (Li01_01_ShipDealer)
    std::string r = room;
    const size_t bl = strlen(base);
    if (r.size() > bl + 1 && _strnicmp(r.c_str(), base, bl) == 0 && r[bl] == '_')
        r = r.substr(bl + 1);
    else if (const size_t us = r.rfind('_'); us != std::string::npos)
        r = r.substr(us + 1);
    // the game's own name for it: the room bar button whose hotspot switches to this room (its record's label is the
    // room's name, "ShipDealer") carries it as its hint ("Ship Dealer", "Commodity Trader", "Launch Pad")
    for (const room::Hotspot& h : room::Hotspots())
        if (h.behaviour == 1 && _stricmp(h.label.c_str(), r.c_str()) == 0)
        {
            const std::wstring t = HintText(h.ctl);
            if (!t.empty())
            {
                std::wstring u;
                for (wchar_t ch : t)
                    u += static_cast<wchar_t>(towupper(ch));
                return cache[loc] = u;
            }
        }
    // the NavBar not built yet (a room's entry): the nickname's word, CamelCase split - not cached
    std::wstring w;
    for (size_t i = 0; i < r.size(); ++i)
    {
        const char ch = r[i];
        if (i > 0 && isupper(static_cast<u8>(ch)) && islower(static_cast<u8>(r[i - 1])))
            w += L' ';
        w += static_cast<wchar_t>(toupper(static_cast<u8>(ch)));
    }
    return w;
}

std::wstring SystemName()
{
    static std::unordered_map<u32, std::wstring> cache;
    const u32 sys = exe::PlayerSystem.ok() ? Mem<u32>(exe::PlayerSystem) : 0;
    if (!sys)
        return L"";
    if (auto it = cache.find(sys); it != cache.end())
        return it->second;
    const u32 strid = SystemStrid(sys);
    return cache[sys] = strid ? TrimW(text::Ids(strid)) : L"";
}

std::wstring ShipName()
{
    u32 arch = 0;
    return CallU(F().shipId, arch) == 0 && arch ? shipname::Name(arch) : L"";
}

long long Credits() { return hudinfo::Credits(); }

bool Cargo(int& used, int& capacity)
{
    used = capacity = -1;
    const Fns& f = F();
    u32 arch = 0;
    if (CallU(f.shipId, arch) != 0 || !arch || !f.holdLeft)
        return false;
    const uintptr_t ship = Arch(f.getShip, arch);
    if (!ship)
        return false;
    const float cap = F32(ship + 0x100); // Archetype::Ship hold_size (core\game\ai.cpp kArchHold)
    const float left = HoldLeft();
    if (!(cap > 0) || left < 0 || left > cap + 0.5f)
        return false;
    capacity = static_cast<int>(cap + 0.5f);
    used = static_cast<int>(cap - left + 0.5f);
    return true;
}

// ================= controls
void Refresh()
{
    g_visible.clear();
    if (!exe::UiControls.ok())
        return;
    __try
    {
        const u32 head = Mem<u32>(exe::UiControls);
        u32 node = head ? Mem<u32>(head + 4) : 0;
        for (int guard = 0; node && node != head && guard < 6000; ++guard, node = Mem<u32>(node + 4))
        {
            const u32 ctl = Mem<u32>(node + 8);
            if (!ctl || !(Mem<u8>(ctl + 0x6C) & 1))
                continue;
            Ctl c;
            c.ctl = ctl;
            c.vt = Mem<u32>(ctl);
            memcpy(c.name, reinterpret_cast<const void*>(ctl + 0x18), 32);
            c.name[32] = 0;
            g_visible.push_back(c);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        g_visible.clear();
    }
}

const std::vector<Ctl>& Visible() { return g_visible; }

u32 Find(const char* name, int nth)
{
    for (const Ctl& c : g_visible)
        if (strcmp(c.name, name) == 0 && --nth <= 0)
            return c.ctl;
    return 0;
}

std::wstring HintText(u32 ctl)
{
    if (!ctl || !exe::ControlHintSet.ok())
        return L"";
    const u32 ids = U32(ctl + 0xCC);
    return ids ? TrimW(text::Ids(ids)) : L"";
}

// ================= dealers
namespace
{
// The dealer window (NN_Dealer, vt checked) among the visible controls, 0 none.
u32 DealerWindow()
{
    const u32 d = Find("NN_Dealer");
    return d && exe::VtDealer.ok() && exe::VtDealer.is(U32(d)) ? d : 0;
}
} // namespace

bool PendingPurchase(Transfer& out)
{
    out = Transfer();
    const u32 dealer = DealerWindow();
    if (!dealer || !exe::DealerSelectedGood.ok())
        return false;
    // the dealer's own list: +0x360 = 0 (the player's list points at the dealer there), +0x3A4 the selected row,
    // +0x978 its kind (FREELANCER_RE "Equipment dealer")
    if (U32(dealer + 0x360) != 0 || static_cast<int>(U32(dealer + 0x3A4, 0xFFFFFFFF)) < 0)
        return false;
    if (!Find("BuySellTradeButton"))
        return false; // the game offers no purchase (nothing fits / affordable)
    // a row of the player's list selected = a sale, not a purchase (NN_Inventory's own +0x3A4)
    if (const u32 inv = Find("NN_Inventory"); inv && static_cast<int>(U32(inv + 0x3A4, 0)) >= 0)
        return false;
    // the quantity: the slider's position (range 1 .. n + 1); a single item (equipment) has no slider: 1
    if (const u32 slider = Find("QuantitySlider"))
    {
        float range[2] = {}, pos = -1;
        if (!Read(slider + 0x3C0, range, 8) || !Read(slider + 0x3C8, &pos, 4) || pos < 1 || pos > range[1])
            return false;
        out.qty = static_cast<int>(pos + 0.5f);
    }
    else
        out.qty = 1;
    out.side = static_cast<int>(U32(dealer + 0x978));
    out.good = hk::Call<u32(__thiscall*)(u32)>(exe::DealerSelectedGood, dealer);
    if (!out.good)
        return false;
    const Fns& f = F();
    out.unitPrice = MarketPrice(Mem<u32>(exe::PlayerBase), out.good);
    // the good's equipment archetype (same nickname, so the same id): its volume (Archetype::Equipment +0x5C)
    if (const uintptr_t e = Arch(f.getEquip, out.good))
        out.volume = F32(e + 0x5C);
    return true;
}

Deal DealOf(u32 mark)
{
    if (!mark || !exe::ControlHintSet.ok() || !exe::DealMarkBuy.ok() || !exe::DealMarkSell.ok())
        return DealUnknown;
    switch (U32(mark + 0xCC))
    {
    case 1047: return DealGood; // green square
    case 1049: return DealFair; // yellow square
    case 1048: return DealHigh; // red square
    case 1050: return DealGood; // green circle (selling)
    case 1052: return DealFair; // yellow circle
    case 1051: return DealLow;  // red circle
    default: return DealUnknown;
    }
}

// A munition's energy_damage (weapon_equip.ini) and constants.ini [ShieldEquipConsts] HULL_DAMAGE_FACTOR, read once
// (the game keeps no energy field it is known to read back here: core\game\space_rounds.cpp reads the same ini).
static bool EnergyDamage(u32 munition, float& energy, float& factor)
{
    static bool loaded = false;
    static std::unordered_map<u32, float> energyOf;
    static float hullFactor = 0.5f;
    if (!loaded)
    {
        loaded = true;
        HMODULE common = GetModuleHandleA("common.dll");
        auto createId = common ? reinterpret_cast<u32(__cdecl*)(const char*)>(GetProcAddress(common, "?CreateID@@YAIPBD@Z"))
                               : nullptr;
        if (createId)
        {
            for (const auto& s : ini::Read(ini::DataPath("EQUIPMENT\\weapon_equip.ini")))
                if (s.type == "munition")
                    if (const auto* n = s.Get("nickname"))
                        if (const auto* e = s.Get("energy_damage"))
                            energyOf[createId(n->c_str())] = static_cast<float>(atof(e->c_str()));
            for (const auto& s : ini::Read(ini::DataPath("constants.ini")))
                if (s.type == "shieldequipconsts")
                    if (const auto* f = s.Get("hull_damage_factor"))
                        hullFactor = static_cast<float>(atof(f->c_str()));
        }
    }
    factor = hullFactor;
    const auto it = energyOf.find(munition);
    energy = it != energyOf.end() ? it->second : 0.0f; // (no energy_damage line = none)
    return !energyOf.empty();
}

bool GunOf(u32 arch, Gun& out)
{
    out = Gun();
    const Fns& f = F();
    const uintptr_t g = Arch(f.getEquip, arch);
    if (!g || !hk::IsExportedClass(reinterpret_cast<const void*>(g), "common.dll", "??_7Gun@Archetype@@6B@"))
        return false;
    out.arch = arch;
    out.name = hudinfo::EquipName(arch);
    // Archetype::Launcher: refire_delay +0x90, muzzle_velocity +0x94 (Common.dll's ini reader 0x62F6150..), projectile id
    // +0x98 (CELauncher's ctor 0x62992B0 looks it up); Gun: hp_gun_type vector begin +0xC0 / end +0xC4 of u32 (read in game 25 Oct 2026: Justice Mk I = {15})
    const float refire = F32(g + 0x90), muzzle = F32(g + 0x94), power = F32(g + 0x8C); // power_usage +0x8C (space_internal.h)
    const u32 proj = U32(g + 0x98);
    out.rate = refire > 0.001f ? 1.0f / refire : -1;
    out.speed = muzzle > 0 ? muzzle : -1;
    if (out.rate > 0 && power >= 0 && power < 1e6f)
        out.energyPs = power * out.rate;
    if (const uintptr_t m = Arch(f.getEquip, proj);
        m && hk::IsExportedClass(reinterpret_cast<const void*>(m), "common.dll", "??_7Munition@Archetype@@6B@"))
    {
        // Munition hull_damage + its explosion's (core\game\space_rounds.cpp MunitionDamage)
        out.damage = F32(m + 0x80) + ExplosionDamage(U32(m + 0x24));
        out.homing = U32(m + 0x90) == 2; // Munition seeker LOCK (core\game\space_internal.h kMunitionSeeker)
        const float life = F32(m + 0x70); // Projectile lifetime
        out.range = muzzle > 0 && life > 0 ? muzzle * life : -1;
        if (out.rate > 0 && out.damage >= 0)
        {
            out.hullDps = out.damage * out.rate;
            float energy = 0, factor = 0.5f;
            if (EnergyDamage(proj, energy, factor))
                out.shieldDps = (out.damage * factor + energy) * out.rate;
        }
    }
    const u32 b = U32(g + 0xC0), e = U32(g + 0xC4);
    if (b && e >= b && (e - b) % 4 == 0 && (e - b) / 4 <= 32)
        for (u32 p = b; p < e; p += 4)
            out.hpTypes.push_back(U32(p));
    return true;
}

std::vector<Item> Equipment()
{
    std::vector<Item> out;
    const Fns& f = F();
    const u8* fn = f.enumCargo;
    // pub::Player::EnumerateCargo's own code says where the list is (core\game\space_debug.cpp EquipmentList, verified
    // 20 Oct 2026): the instructions it relies on are checked first
    static const struct
    {
        int at;
        const char* bytes;
        int n;
    } sig[] = {{0x11, "\x8B\x35", 2},
               {0x1E, "\x8B\x35", 2},
               {0x24, "\x69\xD2\x18\x04\x00\x00", 6},
               {0x2A, "\x8B\xBC\x32\x48\x03\x00\x00", 7},
               {0x3B, "\x8D\x8C\x31\x60\xFE\xFF\xFF", 7},
               {0x43, "\x8B\x59\x04", 3},
               {0x57, "\x8A\x46\x14", 3},
               {0x5E, "\x8B\x46\x0C", 3},
               {0x64, "\x8B\x4E\x1C", 3},
               {0x6F, "\x66\x8B\x46\x0A", 4}};
    if (!fn)
        return out;
    u8 code[0x80];
    if (!Read(reinterpret_cast<uintptr_t>(fn), code, sizeof(code)))
        return out;
    for (const auto& s : sig)
        if (memcmp(code + s.at, s.bytes, s.n) != 0)
            return out;
    const u32 count = U32(U32(reinterpret_cast<uintptr_t>(fn) + 0x13));
    const u32 arr = U32(U32(reinterpret_cast<uintptr_t>(fn) + 0x20));
    if (!arr || kClient - 1 >= count || !U32(arr + (kClient - 1) * 0x418 + 0x348))
        return out;
    const u32 head = U32(arr + kClient * 0x418 - 0x1A0 + 4);
    int n = 0;
    for (u32 node = U32(head); node && node != head && n < 256; node = U32(node), ++n)
    {
        Item it;
        it.arch = U32(node + 0x0C);
        it.count = static_cast<int>(U32(node + 0x1C));
        u8 m = 0;
        Read(node + 0x14, &m, 1);
        it.mounted = m != 0;
        char hp[33] = {};
        if (const u32 p = U32(node + 0x10))
            if (Read(p, hp, 32))
                hp[32] = 0, it.hardpoint = hp;
        out.push_back(it);
    }
    return out;
}

bool CompareSelected(Gun& selected, Gun& mounted)
{
    selected = Gun(), mounted = Gun();
    const u32 dealer = DealerWindow();
    if (!dealer || !exe::DealerSelectedGood.ok() || U32(dealer + 0x360) != 0 ||
        static_cast<int>(U32(dealer + 0x3A4, 0xFFFFFFFF)) < 0)
        return false;
    const u32 good = hk::Call<u32(__thiscall*)(u32)>(exe::DealerSelectedGood, dealer);
    if (!GunOf(good, selected))
        return false;
    // the mounted gun it competes with: one sharing a hardpoint type, else the first of the same kind (gun or missile
    // launcher: the munition's seeker) - a class 2 gun goes on a class 2 hardpoint that holds a class 1 gun now
    bool kindMatch = false;
    for (const Item& it : Equipment())
    {
        Gun g;
        if (!it.mounted || !GunOf(it.arch, g))
            continue;
        for (u32 t : g.hpTypes)
            if (std::find(selected.hpTypes.begin(), selected.hpTypes.end(), t) != selected.hpTypes.end())
            {
                mounted = g;
                return true;
            }
        if (!kindMatch && g.homing == selected.homing)
            mounted = g, kindMatch = true;
    }
    return kindMatch;
}

// ================= the ship dealer
bool SelectedShip(ShipOffer& out)
{
    out = ShipOffer();
    const u32 st = Find("NN_ShipTrader");
    if (!st || !exe::VtShipTrader.ok() || !exe::VtShipTrader.is(U32(st)) || !exe::ShipTraderStatus.ok() ||
        !exe::ShipTraderLevel.ok() || !exe::ShipTraderAfford.ok())
        return false;
    const int sel = static_cast<int>(U32(st + 0x3D0, 0xFFFFFFFF));
    const int n = static_cast<int>(U32(st + 0x3CC)); // the offered ships (3 at Manhattan)
    if (sel < 0 || sel >= 3 || sel >= n)
        return false;
    out.selected = sel;
    out.price = static_cast<int>(U32(st + 0x3A8 + 4 * sel));
    out.budget = static_cast<int>(U32(st + 0x3A4));
    out.credits = Credits();
    const u32 status = U32(st + 0x3FC + 4 * sel);
    if (status == 1)
        out.refusal = 1, out.level = static_cast<int>(U32(st + 0x408 + 4 * sel));
    else if (status == 2)
        out.refusal = 2;
    else if (out.price > out.budget)
        out.refusal = 3;
    return true;
}

// ================= room characters
namespace
{
bool IsCharacter(void* obj) { return obj && exe::VtCharacter.ok() && exe::VtCharacter.is(U32(reinterpret_cast<uintptr_t>(obj))); }
} // namespace

bool CharacterHovered(void* obj)
{
    u8 v = 0;
    return IsCharacter(obj) && Read(reinterpret_cast<uintptr_t>(obj) + 0x2F8, &v, 1) && v;
}

bool CharacterTalking(void* obj)
{
    u8 v = 0;
    return IsCharacter(obj) && Read(reinterpret_cast<uintptr_t>(obj) + 0x36C, &v, 1) && v;
}

bool CharacterScreenRect(void* obj, float& x, float& y, float& w, float& h)
{
    if (!IsCharacter(obj))
        return false;
    // +0x2F4 -> the character's pick box: +4 back to the character, +0x10 int x, y, w, h in screen pixels (top-left
    // origin), kept by the game's per-frame update; the native card's box starts at its x (0x444C09..)
    const uintptr_t o = reinterpret_cast<uintptr_t>(obj);
    const u32 box = U32(o + 0x2F4);
    i32 r[4] = {};
    if (!box || U32(box + 4) != static_cast<u32>(o) || !Read(box + 0x10, r, sizeof(r)) || r[2] <= 0 || r[3] <= 0 ||
        r[2] > 20000 || r[3] > 20000)
        return false;
    x = static_cast<float>(r[0]), y = static_cast<float>(r[1]), w = static_cast<float>(r[2]), h = static_cast<float>(r[3]);
    return true;
}

bool CharacterCard(void* obj, Card& out)
{
    out = Card();
    if (!IsCharacter(obj) || !exe::CharTooltipText.ok())
        return false;
    const uintptr_t o = reinterpret_cast<uintptr_t>(obj);
    const u32 p = U32(o + 0x2FC);
    wchar_t buf[256] = {};
    if (!p || !Read(p, buf, sizeof(buf) - 2))
        return false;
    buf[255] = 0;
    std::vector<std::wstring> lines;
    std::wstring cur;
    for (const wchar_t* c = buf; *c; ++c)
        if (*c == L'\n')
            lines.push_back(TrimW(cur)), cur.clear();
        else
            cur += *c;
    lines.push_back(TrimW(cur));
    lines.erase(std::remove(lines.begin(), lines.end(), std::wstring()), lines.end());
    if (lines.empty())
        return false;
    out.name = lines.back();
    if (lines.size() >= 2)
    {
        out.faction = lines.front();
        for (size_t i = 1; i + 1 < lines.size(); ++i)
            out.role += (out.role.empty() ? L"" : L" ") + lines[i];
        out.attitude = Attitude(out.faction);
    }
    return true;
}

// ================= debug
std::string Status()
{
    std::ostringstream o;
    o << "docked " << Docked() << " base \"" << text::Utf8(BaseName()) << "\" room \"" << text::Utf8(RoomName())
      << "\" system \"" << text::Utf8(SystemName()) << "\" ship \"" << text::Utf8(ShipName()) << "\" credits "
      << Credits();
    int used = -1, cap = -1;
    Cargo(used, cap);
    o << " cargo " << used << "/" << cap << "\n";
    Transfer t;
    if (PendingPurchase(t))
        o << "purchase good=" << t.good << " qty=" << t.qty << " price=" << t.unitPrice << " volume=" << t.volume
          << " side=" << t.side << "\n";
    else
        o << "purchase -\n";
    Gun s, m;
    if (CompareSelected(s, m))
        o << "compare " << text::Utf8(s.name) << " dmg=" << s.damage << " rate=" << s.rate << " range=" << s.range
          << " vs " << text::Utf8(m.name) << " dmg=" << m.damage << " rate=" << m.rate << " range=" << m.range << "\n";
    ShipOffer so;
    if (SelectedShip(so))
        o << "ship sel=" << so.selected << " refusal=" << so.refusal << " level=" << so.level << " price=" << so.price
          << " budget=" << so.budget << " credits=" << so.credits << "\n";
    return o.str();
}

std::string Research(const std::string& what)
{
    std::istringstream in(what);
    std::string op;
    in >> op;
    std::ostringstream o;
    if (op == "ids") // ids <n>: a string id's text
    {
        u32 id = 0;
        in >> id;
        o << id << " \"" << text::IdsUtf8(id) << "\"";
    }
    else if (op == "equip") // the player's equipment list (server side)
    {
        for (const Item& it : Equipment())
        {
            Gun g;
            const bool gun = GunOf(it.arch, g);
            o << "arch=" << it.arch << " count=" << it.count << " mounted=" << it.mounted << " hp=" << it.hardpoint;
            if (gun)
            {
                o << " gun \"" << text::Utf8(g.name) << "\" dmg=" << g.damage << " rate=" << g.rate << " range=" << g.range
                  << " hptypes=";
                for (u32 t : g.hpTypes)
                    o << t << ",";
            }
            o << "\n";
        }
    }
    else if (op == "gun") // gun <arch>
    {
        u32 a = 0;
        in >> a;
        Gun g;
        if (GunOf(a, g))
        {
            o << "\"" << text::Utf8(g.name) << "\" dmg=" << g.damage << " rate=" << g.rate << " range=" << g.range << " hptypes=";
            for (u32 t : g.hpTypes)
                o << t << ",";
        }
        else
            o << "not a gun";
    }
    else if (op == "feel") // feel <group nickname>: the player's standing with it (raw)
    {
        std::string nick;
        in >> nick;
        const Fns& f = F();
        int rep = 0;
        u32 group = 0;
        float feel = -9;
        const u32 c = kClient;
        const int r1 = f.playerRep ? f.playerRep(c, rep) : -1;
        const int r2 = f.repGroup ? f.repGroup(group, nick.c_str()) : -1;
        const int r3 = f.feeling && rep && group ? f.feeling(rep, group, feel) : -1;
        o << "rep=" << rep << " (" << r1 << ") group=" << group << " (" << r2 << ") feel=" << feel << " (" << r3
          << ") attitude=" << FeelingFor(nick.c_str());
    }
    else if (op == "arch") // arch <id>: the equipment archetype's address (for peek)
    {
        u32 a = 0;
        in >> a;
        char b[32];
        sprintf_s(b, "0x%08X", static_cast<unsigned>(Arch(F().getEquip, a)));
        o << b;
    }
    else if (op == "hints") // every visible control with a hint IDS
    {
        Refresh();
        for (const Ctl& c : g_visible)
            if (const u32 ids = U32(c.ctl + 0xCC); ids && ids < 0x200000)
                o << c.name << " " << ids << " \"" << text::IdsUtf8(ids) << "\"\n";
    }
    else
        o << "baseinfo research: ids <n> | equip | gun <arch> | hints";
    return o.str();
}
} // namespace baseinfo
