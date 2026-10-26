// Ships and goods: prices, shield thresholds and rebuild times, the loot drop rule, a ship's motion / agility, its
// shield generator, the player's worth and control exclusion. Split from space.cpp on 24 Oct 2026 (a file move:
// the functions are unchanged).
#include "game/space_internal.h"

#include "flexe.h"

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
        char path[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, path, MAX_PATH); // EXE\Freelancer.exe -> DATA\EQUIPMENT\goods.ini
        if (char* slash = strrchr(path, '\\'))
            strcpy_s(slash + 1, MAX_PATH - (slash + 1 - path), "..\\DATA\\EQUIPMENT\\goods.ini");
        std::ifstream in(path);
        std::string line, nickname;
        bool good = false;
        while (std::getline(in, line))
        {
            const size_t semi = line.find(';');
            if (semi != std::string::npos)
                line.erase(semi);
            auto trim = [](std::string s)
            {
                s.erase(0, s.find_first_not_of(" \t\r"));
                s.erase(s.find_last_not_of(" \t\r") + 1);
                return s;
            };
            line = trim(line);
            if (line.empty())
                continue;
            if (line[0] == '[')
            {
                good = _stricmp(line.c_str(), "[Good]") == 0;
                nickname.clear();
                continue;
            }
            const size_t eq = line.find('=');
            if (!good || eq == std::string::npos)
                continue;
            const std::string k = trim(line.substr(0, eq)), v = trim(line.substr(eq + 1));
            if (_stricmp(k.c_str(), "nickname") == 0)
                nickname = v;
            else if (_stricmp(k.c_str(), "price") == 0 && !nickname.empty())
                prices[a.createId(nickname.c_str())] = static_cast<float>(atof(v.c_str()));
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
        char path[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, path, MAX_PATH); // EXE\Freelancer.exe -> DATA\EQUIPMENT\st_equip.ini
        if (char* slash = strrchr(path, '\\'))
            strcpy_s(slash + 1, MAX_PATH - (slash + 1 - path), "..\\DATA\\EQUIPMENT\\st_equip.ini");
        std::ifstream in(path);
        std::string line, nickname;
        bool shield = false;
        while (std::getline(in, line))
        {
            if (const size_t semi = line.find(';'); semi != std::string::npos)
                line.erase(semi);
            line.erase(0, line.find_first_not_of(" \t\r"));
            line.erase(line.find_last_not_of(" \t\r") + 1);
            if (line.empty())
                continue;
            if (line[0] == '[')
            {
                shield = _stricmp(line.c_str(), "[ShieldGenerator]") == 0;
                nickname.clear();
                continue;
            }
            const size_t eq = line.find('=');
            if (!shield || eq == std::string::npos)
                continue;
            std::string k = line.substr(0, eq), v = line.substr(eq + 1);
            k.erase(k.find_last_not_of(" \t") + 1);
            v.erase(0, v.find_first_not_of(" \t"));
            if (_stricmp(k.c_str(), "nickname") == 0)
                nickname = v;
            else if (_stricmp(k.c_str(), "offline_threshold") == 0 && !nickname.empty())
                thresholds[a.createId(nickname.c_str())] = static_cast<float>(atof(v.c_str()));
            else if (_stricmp(k.c_str(), "offline_rebuild_time") == 0 && !nickname.empty())
                g_shieldRebuild[a.createId(nickname.c_str())] = static_cast<float>(atof(v.c_str()));
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
    if (!get || !id || !InSpace())
        return false;
    __try
    {
        return get(id, current, max, up) == 0 && std::isfinite(current) && std::isfinite(max);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

// The game's invincibility of a space object (missions' Act_Invulnerable): server.dll
// pub::SpaceObj::GetInvincible(uint id, bool& invincible, bool& (a second flag), float& floor) - the hull fraction it
// cannot be damaged below. True when the object is invincible and already at that floor: guns cannot take more off it
// (M04 Benford: the cruisers' floor 0.4 - the autopilot held still firing at one, 20 Oct 2026).
bool AtInvincibleFloor(u32 id, float health)
{
    typedef int(__cdecl * GetInvincibleFn)(u32, bool&, bool&, float&);
    static GetInvincibleFn get = reinterpret_cast<GetInvincibleFn>(
        GetProcAddress(GetModuleHandleA("server.dll"), "?GetInvincible@SpaceObj@pub@@YAHIAA_N0AAM@Z"));
    if (!get || !id || health < 0 || !InSpace())
        return false;
    __try
    {
        bool inv = false, other = false;
        float floor = 0;
        if (get(id, inv, other, floor) != 0 || !inv || !std::isfinite(floor))
            return false;
        return health <= floor + 0.02f;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
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
    std::ifstream in(DataPath("MISSIONS\\lootprops.ini"));
    std::string line, nickname;
    bool block = false;
    while (std::getline(in, line))
    {
        if (const size_t semi = line.find(';'); semi != std::string::npos)
            line.erase(semi);
        line.erase(0, line.find_first_not_of(" \t\r"));
        line.erase(line.find_last_not_of(" \t\r") + 1);
        if (line.empty())
            continue;
        if (line[0] == '[')
        {
            block = _stricmp(line.c_str(), "[mLootProps]") == 0;
            nickname.clear();
            continue;
        }
        const size_t eq = line.find('=');
        if (!block || eq == std::string::npos)
            continue;
        std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        k.erase(k.find_last_not_of(" \t") + 1);
        v.erase(0, v.find_first_not_of(" \t"));
        if (_stricmp(k.c_str(), "nickname") == 0)
            nickname = v;
        else if (_stricmp(k.c_str(), "drop_properties") == 0 && !nickname.empty())
        {
            LootProps p;
            float chance = 0;
            if (sscanf_s(v.c_str(), "%f , %d , %d , %d , %d , %d", &chance, &p.minWorth, &p.worthMult, &p.min, &p.max1,
                         &p.max2) == 6)
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
