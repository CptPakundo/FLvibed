// Ships and goods: prices, shield thresholds and rebuild times, the loot drop rule, a ship's motion / agility, its
// shield generator, the player's worth and control exclusion. Split from space.cpp on 24 Oct 2026 (a file move:
// the functions are unchanged).
#include "game/space_internal.h"

#include "flexe.h"
#include "game/ini.h"

#include <fstream>
#include <unordered_map>

namespace space
{
// GoodInfo::price (+88, FLSDK Goods.hpp) of the good the game loaded for `archetype` (every goods file, mods and
// binary INIs included); -1 when the game has none.
float GamePrice(const Api& a, u32 archetype)
{
    if (!a.goodByArch)
        return -1;
    __try
    {
        const u8* info = static_cast<const u8*>(a.goodByArch(archetype));
        return info ? *reinterpret_cast<const float*>(info + 88) : -1.0f;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return -1;
    }
}

// The base price: the game's good list, else goods.ini [Good] nickname -> price, keyed by CreateID(nickname) (the
// archetype id), loaded once.
float Price(u32 archetype)
{
    static std::unordered_map<u32, float> prices;
    static bool loaded = false;
    const Api& a = GetApi();
    if (const float p = GamePrice(a, archetype); p >= 0)
        return p;
    if (!loaded && a.createId)
    {
        loaded = true;
        for (const ini::Section& s : ini::Read(DataPath("EQUIPMENT\\goods.ini"))) // text or BINI (SWEEP #5)
        {
            if (s.type != "good")
                continue;
            std::string nickname;
            for (const auto& kv : s.kv)
                if (kv.first == "nickname")
                    nickname = kv.second;
                else if (kv.first == "price" && !nickname.empty())
                    prices[a.createId(nickname.c_str())] = static_cast<float>(atof(kv.second.c_str()));
        }
    }
    const auto it = prices.find(archetype);
    return it == prices.end() ? 0.0f : it->second;
}

static std::unordered_map<u32, float> g_shieldRebuild; // [ShieldGenerator] offline_rebuild_time by archetype id
float ShieldOfflineThreshold(u32 shieldArch)
{
    static std::unordered_map<u32, float> thresholds;
    static bool loaded = false;
    const Api& a = GetApi();
    if (!loaded && a.createId)
    {
        loaded = true;
        for (const ini::Section& s : ini::Read(DataPath("EQUIPMENT\\st_equip.ini"))) // text or BINI (SWEEP #5)
        {
            if (s.type != "shieldgenerator")
                continue;
            std::string nickname;
            for (const auto& kv : s.kv)
                if (kv.first == "nickname")
                    nickname = kv.second;
                else if (kv.first == "offline_threshold" && !nickname.empty())
                    thresholds[a.createId(nickname.c_str())] = static_cast<float>(atof(kv.second.c_str()));
                else if (kv.first == "offline_rebuild_time" && !nickname.empty())
                    g_shieldRebuild[a.createId(nickname.c_str())] = static_cast<float>(atof(kv.second.c_str()));
        }
    }
    const auto it = thresholds.find(shieldArch);
    return it == thresholds.end() ? -1.0f : it->second;
}

float ShieldRebuildTime(u32 shieldArch)
{
    ShieldOfflineThreshold(shieldArch); // loads the file
    const auto it = g_shieldRebuild.find(shieldArch);
    return it == g_shieldRebuild.end() ? -1.0f : it->second;
}

// A space object's shield: server.dll pub::SpaceObj::GetShieldHealth(const uint& id, float& current, float& max,
// bool& up) (FLHookSDK). Current in shield points, false if unknown (no shield: max 0).
bool ShieldOf(u32 id, float& current, float& max, bool& up)
{
    typedef int(__cdecl * GetShieldFn)(const u32&, float&, float&, bool&);
    static GetShieldFn get = reinterpret_cast<GetShieldFn>(
        GetProcAddress(GetModuleHandleA("server.dll"), "?GetShieldHealth@SpaceObj@pub@@YAHABIAAM1AA_N@Z"));
    current = max = 0;
    up = false;
    if (!id || !InSpace())
        return false;
    __try
    {
        if (get && get(id, current, max, up) == 0 && std::isfinite(current) && std::isfinite(max))
            return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
    // multiplayer (server.dll holds no world): the client ship's own shield - CEqObj equipment manager +0xE4 ->
    // CEquipManager::FindFirst(1 << 8) = the CEShield -> GetHitPoints / GetMaxHitPoints (as ai.cpp reads the player's)
    typedef void*(__fastcall * FirstFn)(void*, void*, u32);
    typedef float(__fastcall * HpFn)(const void*, void*);
    static const HMODULE common = GetModuleHandleA("common.dll");
    static const auto first = reinterpret_cast<FirstFn>(
        common ? GetProcAddress(common, "?FindFirst@CEquipManager@@QAEPAVCEquip@@I@Z") : nullptr);
    static const auto hp =
        reinterpret_cast<HpFn>(common ? GetProcAddress(common, "?GetHitPoints@CEShield@@UBEMXZ") : nullptr);
    static const auto hpMax =
        reinterpret_cast<HpFn>(common ? GetProcAddress(common, "?GetMaxHitPoints@CEShield@@UBEMXZ") : nullptr);
    const Api& a = GetApi();
    void* o = first && hp && hpMax ? ClientFind(a, id, true) : nullptr; // (CEqObj only)
    if (!o)
        return false;
    bool ok = false;
    if (IsClass(o, "??_7CShip@@6BCEqObj@@@"))
        __try
        {
            void* shield = first(static_cast<u8*>(o) + kEqObjEquipManager, nullptr, 1u << 8);
            if (shield && IsClass(shield, "??_7CEShield@@6B@"))
            {
                current = hp(shield, nullptr);
                max = hpMax(shield, nullptr);
                up = current > 0;
                ok = std::isfinite(current) && std::isfinite(max);
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            ok = false;
        }
    ClientRelease(a, o);
    return ok;
}

// The game's invincibility of a space object (missions' Act_Invulnerable): server.dll
// pub::SpaceObj::GetInvincible(uint id, bool& invincible, bool& (a second flag), float& floor) - the hull fraction it
// cannot be damaged below. True when the object is invincible and already at that floor: guns cannot take more off it
// (M04 Benford: the cruisers' floor 0.4 - the autopilot held still firing at one).
bool Invincible(u32 id, bool& invincible, bool& second, float& floor)
{
    typedef int(__cdecl * GetInvincibleFn)(u32, bool&, bool&, float&);
    static GetInvincibleFn get = reinterpret_cast<GetInvincibleFn>(
        GetProcAddress(GetModuleHandleA("server.dll"), "?GetInvincible@SpaceObj@pub@@YAHIAA_N0AAM@Z"));
    invincible = second = false;
    floor = 0;
    if (!get || !id || !InSpace())
        return false;
    __try
    {
        return get(id, invincible, second, floor) == 0 && std::isfinite(floor);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool AtInvincibleFloor(u32 id, float health)
{
    bool inv = false, other = false;
    float floor = 0;
    if (!id || health < 0 || !Invincible(id, inv, other, floor) || !inv)
        return false;
    return health <= floor + 0.02f;
}

// Test aid (research captures): the game's pub::SpaceObj::DrainShields(uint id) (server.dll; adoxa's Console
// `draintgt` calls it) - the shield goes offline and rebuilds after its offline_rebuild_time.
bool DrainShields(u32 id)
{
    typedef int(__cdecl * DrainFn)(u32);
    static DrainFn drain =
        reinterpret_cast<DrainFn>(GetProcAddress(GetModuleHandleA("server.dll"), "?DrainShields@SpaceObj@pub@@YAHI@Z"));
    if (!drain || !id || !InSpace())
        return false;
    __try
    {
        return drain(id) == 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

// The player ship's archetype id (CreateID of its shiparch nickname: what it may dock with - mission_property - and its
// stats in the game data): exported CObject::get_archetype + Archetype::Root::get_id. 0 when not flying.
u32 PlayerShipArch()
{
    static const auto rootId = reinterpret_cast<u32(__thiscall*)(const void*)>(
        GetProcAddress(GetModuleHandleA("common.dll"), "?get_id@Root@Archetype@@QBEIXZ"));
    const void* ship = PlayerCShip();
    if (!ship || !Acc().getArchetype || !rootId)
        return 0;
    __try
    {
        const void* r = Acc().getArchetype(ship);
        return r ? rootId(r) : 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
}

// A ship's motion (the physics' own velocity, the nose) and agility from its archetype (Archetype::Ship, FLSDK field
// order, offsets beside the ones verified against shiparch.ini - hold 0x100, nanobots 0x140): mass +0x20, linear drag
// +0x104, angular drag +0x108 (x, y, z), steering torque +0x114, strafe force +0x124. The turn rate it can hold is
// torque / drag on the pitch / yaw axes (the smaller); the strafe acceleration force / mass.
static bool ReadShipMotion(const Api& a, void* o, u32 id, ShipMotion& m)
{
    __try
    {
        {
            a.velocity(m.vel, o);
            m.speed = std::sqrt(m.vel[0] * m.vel[0] + m.vel[1] * m.vel[1] + m.vel[2] * m.vel[2]);
            float rot[9];
            if (ObjectPose(id, m.pos, rot))
                m.fwd[0] = -rot[0 * 3 + 2], m.fwd[1] = -rot[1 * 3 + 2], m.fwd[2] = -rot[2 * 3 + 2];
            const u8* arch = a.shipArch(o, nullptr);
            if (arch && (hk::IsExportedClass(arch, "common.dll", "??_7Ship@Archetype@@6BEqObj@1@@") ||
                         hk::IsExportedClass(arch, "common.dll", "??_7Ship@Archetype@@6BCommReferrable@@@")))
            {
                m.mass = Mem<float>(reinterpret_cast<uintptr_t>(arch) + 0x20);
                const float* drag = reinterpret_cast<const float*>(arch + 0x108);
                const float* torque = reinterpret_cast<const float*>(arch + 0x114);
                for (int axis = 0; axis < 2; ++axis)
                    if (drag[axis] > 0 && torque[axis] > 0)
                        m.turnRate = m.turnRate > 0 ? std::min(m.turnRate, torque[axis] / drag[axis])
                                                    : torque[axis] / drag[axis];
                if (drag[2] > 0 && torque[2] > 0)
                    m.rollRate = torque[2] / drag[2]; // its own top roll rate (steering_torque z / angular_drag z)
                const float strafe = Mem<float>(reinterpret_cast<uintptr_t>(arch) + 0x124);
                if (m.mass > 0)
                    m.strafeAccel = strafe / m.mass;
                m.arch = a.archId ? a.archId(arch, nullptr) : 0;
            }
            m.ok = true;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        m.ok = false;
    }
    return m.ok;
}

bool PlayerSpin(float spin[3])
{
    using GetFn = float*(__cdecl*)(float*, const void*); // Vector returned through the hidden pointer
    static const GetFn get = reinterpret_cast<GetFn>(
        GetProcAddress(GetModuleHandleA("common.dll"), "?GetAngularVelocityOS@PhySys@@YA?AVVector@@PBUCObject@@@Z"));
    const void* ship = InSpace() ? PlayerCShip() : nullptr;
    if (!get || !ship)
        return false;
    __try
    {
        get(spin, ship);
        return std::isfinite(spin[0]) && std::isfinite(spin[1]) && std::isfinite(spin[2]);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool AddPlayerSpin(const float delta[3])
{
    using AddFn = void(__cdecl*)(void*, const float*);
    static const AddFn add = reinterpret_cast<AddFn>(
        GetProcAddress(GetModuleHandleA("common.dll"), "?AddToAngularVelocityOS@PhySys@@YAXPAUCObject@@ABVVector@@@Z"));
    void* ship = InSpace() ? PlayerCShip() : nullptr;
    if (!add || !ship)
        return false;
    __try
    {
        add(ship, delta);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

// A ship's shield generator archetype (the first CEquip of EquipmentClass ShieldGenerator, 1 << 9), 0 none.
static u32 ReadShieldGenArch(void* ship)
{
    using Ctor = void*(__fastcall*)(void*, void*, int);
    using Trav = void*(__fastcall*)(void*, void*, void*);
    static const HMODULE common = GetModuleHandleA("common.dll");
    static const Ctor ctor = reinterpret_cast<Ctor>(GetProcAddress(common, "??0CEquipTraverser@@QAE@H@Z"));
    static const Trav trav =
        reinterpret_cast<Trav>(GetProcAddress(common, "?Traverse@CEquipManager@@QAEPAVCEquip@@AAVCEquipTraverser@@@Z"));
    const Accessors& acc = Acc();
    const Api& a = GetApi();
    if (!ctor || !trav || !ship || !acc.equipArch || !a.archId || !IsClass(ship, "??_7CShip@@6BCEqObj@@@"))
        return 0;
    __try
    {
        alignas(8) unsigned char traverser[32] = {};
        ctor(traverser, nullptr, 1 << 9);
        void* mgr = reinterpret_cast<unsigned char*>(ship) + kEqObjEquipManager;
        if (void* e = trav(mgr, nullptr, traverser))
            if (const void* arch = acc.equipArch(e))
                return a.archId(arch, nullptr);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
    return 0;
}

static u32 ShipShieldArch(u32 id)
{
    static std::unordered_map<u32, u32> cache; // ship id -> generator archetype (it does not change mid-fight)
    if (const auto it = cache.find(id); it != cache.end())
        return it->second;
    const Api& a = GetApi();
    if (!a.ok || !id || !InSpace())
        return 0;
    std::vector<void*> objs;
    Collect(a, kClassShip, Mem<u32>(exe::PlayerSystem), objs);
    for (void* o : objs)
        if (o && ObjectId(a, o) == id)
        {
            if (cache.size() > 4096)
                cache.clear();
            return cache[id] = ReadShieldGenArch(o);
        }
    return 0;
}

float ShipShieldRebuild(u32 id)
{
    const u32 arch = ShipShieldArch(id);
    return arch ? ShieldRebuildTime(arch) : -1.0f;
}

float ShipShieldThreshold(u32 id)
{
    if (const u32 arch = ShipShieldArch(id))
        return ShieldOfflineThreshold(arch);
    return -1.0f;
}

bool ShipMotionOf(u32 id, ShipMotion& m)
{
    m = ShipMotion();
    const Api& a = GetApi();
    if (!a.ok || !a.velocity || !a.shipArch || !id || !InSpace())
        return false;
    std::vector<void*> objs;
    Collect(a, kClassShip, Mem<u32>(exe::PlayerSystem), objs);
    if (id == PlayerShipId())
        objs.push_back(PlayerCShip());
    for (void* o : objs)
        if (o && ObjectId(a, o) == id)
            return ReadShipMotion(a, o, id, m);
    return false;
}

// ---- loot: the game's drop rule (Content.dll 0x6F5D0F0, docs\FREELANCER_RE.md "Loot drops") over lootprops.ini.
const std::unordered_map<u32, LootProps>& LootTable()
{
    static std::unordered_map<u32, LootProps> table;
    static bool loaded = false;
    const Api& a = GetApi();
    if (loaded || !a.createId)
        return table;
    loaded = true;
    for (const ini::Section& s : ini::Read(DataPath("MISSIONS\\lootprops.ini"))) // text or BINI (SWEEP #5)
    {
        if (s.type != "mlootprops")
            continue;
        std::string nickname;
        for (const auto& kv : s.kv)
            if (kv.first == "nickname")
                nickname = kv.second;
            else if (kv.first == "drop_properties" && !nickname.empty())
            {
                LootProps p;
                float chance = 0;
                if (sscanf_s(kv.second.c_str(), "%f , %d , %d , %d , %d , %d", &chance, &p.minWorth, &p.worthMult,
                             &p.min, &p.max1, &p.max2) == 6)
                {
                    p.chance = chance * 0.01f;
                    table[a.createId(nickname.c_str())] = p;
                }
            }
    }
    return table;
}

float ExpectedDrops(u32 archetype, float worthF, int carried)
{
    const auto& t = LootTable();
    const auto it = t.find(archetype);
    if (it == t.end() || carried <= 0 || worthF < 0)
        return 0;
    const LootProps& p = it->second;
    const int worth = static_cast<int>(worthF);
    if (worth < p.minWorth)
        return 0;
    const int over = worth - p.minWorth;
    int n = over <= 0 ? 0 : p.worthMult ? std::min(over / p.worthMult, carried) : carried;
    n = std::min(std::min(n + p.min, p.max1), carried);
    return std::min(p.chance * static_cast<float>(n), static_cast<float>(p.max2));
}

int PlayerControlExclusion()
{
    const u8* ship = static_cast<const u8*>(PlayerCShip());
    if (!ship)
        return -1;
    __try
    {
        return *reinterpret_cast<const int*>(ship + 0x190);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return -1;
    }
}

float PlayerWorth()
{
    HMODULE srv = GetModuleHandleA("server.dll");
    static const auto assets = srv ? reinterpret_cast<int(__cdecl*)(const u32&, float&)>(
                                         GetProcAddress(srv, "?GetAssetValue@Player@pub@@YAHABIAAM@Z"))
                                   : nullptr;
    static const auto cash = srv ? reinterpret_cast<int(__cdecl*)(const u32&, int&)>(
                                       GetProcAddress(srv, "?InspectCash@Player@pub@@YAHABIAAH@Z"))
                                 : nullptr;
    if (!assets || !cash)
        return -1;
    const u32 client = 1; // single player
    float a = 0;
    int c = 0;
    return assets(client, a) == 0 && cash(client, c) == 0 ? a + static_cast<float>(c) : -1.0f;
}
} // namespace space
