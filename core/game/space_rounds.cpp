// Munitions in flight: a munition's damage, guided missiles, gun rounds (CBeam), the incoming-rounds prediction with
// the shield damage model, and the round outcome learning (TrackRounds). Split from space.cpp on 24 Oct 2026 (a file
// move: the functions are unchanged).
#include "game/space_internal.h"

#include "fldll.h"
#include "flexe.h"
#include "game/ini.h"

#include <sstream>
#include <unordered_map>

namespace space
{
// A munition's damage per hit: its own hull_damage plus its explosion's (missiles carry theirs in the explosion:
// Archetype::Root explosionArchId +0x24 -> Archetype::GetExplosion -> Explosion radius +0x4, hull_damage +0xC).
// Not a Munition (e.g. a countermeasure's projectile): no damage.
void MunitionDamage(uintptr_t munition, float& damage, float& radius)
{
    static const auto getExplosion = reinterpret_cast<const void*(__cdecl*)(const u32*)>(
        GetProcAddress(GetModuleHandleA("common.dll"), "?GetExplosion@Archetype@@YAPAUExplosion@1@ABUID_String@@@Z"));
    damage = 0;
    radius = 0;
    if (!IsMunitionArch(reinterpret_cast<const void*>(munition)))
        return;
    damage = Mem<float>(munition + kMunitionHullDamage);
    const u32 explosion = Mem<u32>(munition + 0x24);
    if (explosion && getExplosion)
        if (const void* e = getExplosion(&explosion))
        {
            const uintptr_t ep = reinterpret_cast<uintptr_t>(e);
            radius = Mem<float>(ep + 0x4);
            damage += Mem<float>(ep + 0xC);
        }
}

static bool ReadMissile(const Api& a, void* o, void*(__fastcall* guidedTarget)(void*, void*), Missile& m)
{
    const Accessors& acc = Acc();
    if (!acc.getArchetype || !acc.projOwner)
        return false;
    __try
    {
        m.id = *a.getId(o, nullptr);
        const u32* owner = acc.projOwner(o);
        m.owner = owner ? *owner : 0;
        const uintptr_t arch = reinterpret_cast<uintptr_t>(acc.getArchetype(o));
        if (arch && IsMunitionArch(reinterpret_cast<const void*>(arch)))
        {
            MunitionDamage(arch, m.hullDamage, m.blastRadius);
            m.cruiseDisruptor = Mem<u8>(arch + kMunitionCruiseDisruptor) != 0;
            m.homing = Mem<u32>(arch + kMunitionSeeker) == 2; // SeekerType: 0 none, 1 dumb, 2 seeker
        }
        if (guidedTarget)
        {
            void* t = guidedTarget(o, nullptr); // IObjRW (its get_id)
            const auto getId = dll::ObjGetId.get<dll::ObjGetIdFn>(t);
            m.targetId = getId ? getId(t) : 0;
        }
        return m.id != 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

std::vector<Missile> Missiles()
{
    std::vector<Missile> out;
    const Api& a = GetApi();
    const u32 player = InSpace() ? Mem<u32>(exe::PlayerShipId) : 0;
    float me[3];
    if (!a.ok || !player || !Location(a, player, me))
        return out;
    static void*(__fastcall * guidedTarget)(void*, void*) = reinterpret_cast<void*(__fastcall*)(void*, void*)>(
        GetProcAddress(GetModuleHandleA("common.dll"), "?get_target@CGuided@@QBEPAUIObjRW@@XZ"));
    // Closing speed from an earlier sample at least 30 ms old (callers in quick succession share it safely); between
    // two such samples the last closing speed stands. Was: GetTickCount (~16 ms steps) and 0 whenever the sample was
    // too fresh - at high frame rates several frames in every 50 ms read 0, dropping the missile dodge (18 Oct 2026).
    struct Sample
    {
        float distance;
        double time;
        float closing;
    };
    static std::unordered_map<u32, Sample> lastDist;
    static LARGE_INTEGER freq = {};
    if (!freq.QuadPart)
        QueryPerformanceFrequency(&freq);
    LARGE_INTEGER qpc;
    QueryPerformanceCounter(&qpc);
    const double now = static_cast<double>(qpc.QuadPart) / static_cast<double>(freq.QuadPart);
    std::vector<void*> objs;
    Collect(a, kClassGuided, Mem<u32>(exe::PlayerSystem), objs);
    std::unordered_map<u32, Sample> seen;
    for (void* o : objs)
    {
        Missile m;
        if (!ReadMissile(a, o, guidedTarget, m) || !Location(a, m.id, m.pos))
            continue;
        const float dx = m.pos[0] - me[0], dy = m.pos[1] - me[1], dz = m.pos[2] - me[2];
        m.distance = std::sqrt(dx * dx + dy * dy + dz * dz);
        m.targetsPlayer = m.targetId == player;
        const auto it = lastDist.find(m.id);
        if (it == lastDist.end())
            seen[m.id] = {m.distance, now, 0.0f};
        else if (now - it->second.time >= 0.03)
        {
            m.closing = static_cast<float>((it->second.distance - m.distance) / (now - it->second.time));
            seen[m.id] = {m.distance, now, m.closing};
        }
        else
        {
            m.closing = it->second.closing;
            seen[m.id] = it->second;
        }
        out.push_back(m);
    }
    lastDist.swap(seen);
    std::sort(out.begin(), out.end(), [](const Missile& x, const Missile& y) { return x.distance < y.distance; });
    return out;
}

// Gun rounds: CBeam objects (class 0x67) through Common.dll's exported accessors, behind the CBeam class check.
typedef const float*(__fastcall* LastPosFn)(const void*, void*);
typedef float*(__fastcall* VelocityFn)(const void*, void*, float*); // returns the Vector by value (hidden ptr)
typedef const u32*(__fastcall* OwnerFn)(const void*, void*);
typedef const void*(__fastcall* MunitionFn)(const void*, void*);

static bool ReadBeam(const void* o, LastPosFn lastPos, VelocityFn velocity, OwnerFn owner, MunitionFn munition,
                     float p[3], float v[3], u32& own, const void*& arch)
{
    __try
    {
        const u32* ow = owner(o, nullptr);
        own = ow ? *ow : 0;
        const float* lp = lastPos(o, nullptr);
        if (!lp)
            return false;
        memcpy(p, lp, 12);
        velocity(o, nullptr, v);
        arch = munition(o, nullptr);
        return std::isfinite(p[0]) && std::isfinite(v[0]);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

// The ship's bounding box in its own axes (CObject::get_surface_extents, virtual: the CShip's own override by slot).
static hk::Virtual g_surfaceExtents{"common.dll", "??_7CShip@@6BCEqObj@@@",
                                    "?get_surface_extents@CObject@@UBE_NAAVVector@@0@Z"};
bool ShipExtents(void* ship, float mn[3], float mx[3])
{
    typedef bool(__fastcall * Fn)(void*, void*, float*, float*);
    const Fn fn = g_surfaceExtents.get<Fn>(ship);
    if (!fn || !IsClass(ship, "??_7CShip@@6BCEqObj@@@"))
        return false;
    __try
    {
        return fn(ship, nullptr, mn, mx) && std::isfinite(mn[0]) && mx[0] > mn[0] && mx[1] > mn[1] && mx[2] > mn[2];
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

// Shield damage factor of a munition against a shield generator, from the game's data: weapon_equip.ini [Munition]
// weapon_type, weaponmoddb.ini [WeaponType] shield_mod = <shield_type>, <factor>, st_equip.ini [ShieldGenerator]
// shield_type. 1 when unknown.
static std::unordered_map<u32, float> g_energyDamage; // munition id -> energy_damage (weapon_equip.ini)
static float g_hullDamageFactor = 0.5f;               // constants.ini [ShieldEquipConsts] HULL_DAMAGE_FACTOR
static float ShieldMod(u32 munitionArch, u32 shieldArch)
{
    static bool loaded = false;
    static std::unordered_map<u32, std::string> weaponType; // munition id -> weapon_type (lower case)
    static std::unordered_map<u32, std::string> shieldType; // shield generator id -> shield_type
    static std::unordered_map<std::string, float> mods;     // "weapon|shield" -> factor
    const Api& a = GetApi();
    if (!loaded && a.createId)
    {
        loaded = true;
        auto lower = [](std::string s)
        {
            std::transform(s.begin(), s.end(), s.begin(),
                           [](unsigned char c) { return static_cast<char>(tolower(c)); });
            return s;
        };
        for (const auto& s : ini::Read(ini::DataPath("EQUIPMENT\\weapon_equip.ini")))
            if (s.type == "munition")
                if (const auto* n = s.Get("nickname"))
                {
                    const u32 id = a.createId(n->c_str());
                    if (const auto* w = s.Get("weapon_type"))
                        weaponType[id] = lower(*w);
                    if (const auto* e = s.Get("energy_damage"))
                        g_energyDamage[id] = static_cast<float>(atof(e->c_str()));
                }
        for (const auto& s : ini::Read(ini::DataPath("constants.ini")))
            if (s.type == "shieldequipconsts")
                if (const auto* f = s.Get("hull_damage_factor"))
                    g_hullDamageFactor = static_cast<float>(atof(f->c_str()));
        for (const auto& s : ini::Read(ini::DataPath("EQUIPMENT\\st_equip.ini")))
            if (s.type == "shieldgenerator")
                if (const auto* n = s.Get("nickname"))
                    if (const auto* t = s.Get("shield_type"))
                        shieldType[a.createId(n->c_str())] = lower(*t);
        for (const auto& s : ini::Read(ini::DataPath("EQUIPMENT\\weaponmoddb.ini")))
            if (s.type == "weapontype")
                if (const auto* n = s.Get("nickname"))
                    for (const auto& m : s.All("shield_mod"))
                    {
                        const auto f = ini::Fields(m);
                        if (f.size() >= 2)
                            mods[lower(*n) + "|" + lower(f[0])] = static_cast<float>(atof(f[1].c_str()));
                    }
    }
    const auto w = weaponType.find(munitionArch);
    const auto s = shieldType.find(shieldArch);
    if (w == weaponType.end() || s == shieldType.end())
        return 1;
    const auto m = mods.find(w->second + "|" + s->second);
    return m == mods.end() ? 1.0f : m->second;
}

std::vector<Round> IncomingRounds(float horizon, float radius, u32 shieldArch)
{
    std::vector<Round> out;
    const Api& a = GetApi();
    const u32 player = InSpace() ? Mem<u32>(exe::PlayerShipId) : 0;
    float me[3];
    if (!a.ok || !player || !Location(a, player, me))
        return out;
    static const HMODULE common = GetModuleHandleA("common.dll");
    static const LastPosFn lastPos =
        reinterpret_cast<LastPosFn>(GetProcAddress(common, "?get_last_pos@CBeam@@QBEABVVector@@XZ"));
    static const VelocityFn velocity =
        reinterpret_cast<VelocityFn>(GetProcAddress(common, "?get_velocity@CBeam@@UBE?AVVector@@XZ"));
    static const OwnerFn owner = reinterpret_cast<OwnerFn>(GetProcAddress(common, "?get_owner@CProjectile@@QBEABIXZ"));
    static const MunitionFn munition =
        reinterpret_cast<MunitionFn>(GetProcAddress(common, "?munitionarch@CBeam@@QBEPBUMunition@Archetype@@XZ"));
    if (!lastPos || !velocity || !owner || !munition)
        return out;
    // the player's velocity from positions >= 30 ms apart (the last estimate stands in between)
    static float lastMe[3] = {}, vMe[3] = {};
    static double lastT = 0;
    static LARGE_INTEGER freq = {};
    if (!freq.QuadPart)
        QueryPerformanceFrequency(&freq);
    LARGE_INTEGER qpc;
    QueryPerformanceCounter(&qpc);
    const double now = static_cast<double>(qpc.QuadPart) / static_cast<double>(freq.QuadPart);
    if (lastT > 0 && now - lastT >= 0.03 && now - lastT < 1.0)
        for (int i = 0; i < 3; ++i)
            vMe[i] = static_cast<float>((me[i] - lastMe[i]) / (now - lastT));
    else if (now - lastT >= 1.0)
        vMe[0] = vMe[1] = vMe[2] = 0;
    if (now - lastT >= 0.03)
        memcpy(lastMe, me, sizeof(me)), lastT = now;
    const float r = radius > 0 ? radius : std::max(5.0f, PlayerRadius());
    // the hull's box in ship axes (rows of the pose: right = column 0, up = column 1, back = column 2)
    float mn[3] = {}, mx[3] = {}, pose[9] = {}, posePos[3];
    const bool box = radius <= 0 && ShipExtents(PlayerCShip(), mn, mx) && ObjectPose(player, posePos, pose);
    std::vector<void*> objs;
    Collect(a, kClassBeam, 0, objs);
    for (void* o : objs)
    {
        if (!IsClass(o, "??_7CBeam@@6B@"))
            continue;
        Round rd;
        float p[3], v[3];
        const void* arch = nullptr;
        if (!ReadBeam(o, lastPos, velocity, owner, munition, p, v, rd.owner, arch) || rd.owner == player)
            continue;
        if (arch && IsMunitionArch(arch))
        {
            float blast = 0;
            MunitionDamage(reinterpret_cast<uintptr_t>(arch), rd.hullDamage, blast);
            rd.munition = Mem<u32>(reinterpret_cast<uintptr_t>(arch) + 0x4); // Archetype::Root id
            rd.shieldMod = ShieldMod(rd.munition, shieldArch); // also loads the tables (1 without a shield)
            const auto e = g_energyDamage.find(rd.munition);
            rd.shieldDamage =
                (rd.hullDamage * g_hullDamageFactor + (e == g_energyDamage.end() ? 0.0f : e->second)) * rd.shieldMod;
        }
        // closest approach of the relative straight path
        const float d[3] = {p[0] - me[0], p[1] - me[1], p[2] - me[2]};
        const float w[3] = {v[0] - vMe[0], v[1] - vMe[1], v[2] - vMe[2]};
        const float ww = w[0] * w[0] + w[1] * w[1] + w[2] * w[2];
        if (!std::isfinite(ww) || ww < 1)
            continue;
        const float t = -(d[0] * w[0] + d[1] * w[1] + d[2] * w[2]) / ww;
        if (t < 0 || t > horizon)
            continue;
        const float c[3] = {d[0] + w[0] * t, d[1] + w[1] * t, d[2] + w[2] * t};
        rd.miss = std::sqrt(c[0] * c[0] + c[1] * c[1] + c[2] * c[2]);
        rd.tHit = t;
        bool hit = rd.miss <= r;
        if (box) // the closest point in ship axes inside the hull's box + 1 m (a round's own size, aim spread)
        {
            hit = true;
            for (int k = 0; k < 3 && hit; ++k)
            {
                const float local = c[0] * pose[k] + c[1] * pose[3 + k] + c[2] * pose[6 + k];
                hit = local >= mn[k] - 1.0f && local <= mx[k] + 1.0f;
            }
        }
        if (hit)
            out.push_back(rd);
    }
    std::sort(out.begin(), out.end(), [](const Round& x, const Round& y) { return x.tHit < y.tHit; });
    return out;
}

// Round outcome learning: bands of the closest approach (m): 0-1, 1-2, 2-3, 3-4, 4-6, 6+.
static const float kBandEdge[] = {1, 2, 3, 4, 6, 1e9f};
static int g_bandHit[6] = {}, g_bandMiss[6] = {};
static int Band(float miss)
{
    int b = 0;
    while (b < 5 && miss >= kBandEdge[b])
        ++b;
    return b;
}

float RoundHitChance(float miss)
{
    const int b = Band(miss), n = g_bandHit[b] + g_bandMiss[b];
    return n < 10 ? 0.5f : static_cast<float>(g_bandHit[b]) / n;
}

std::string RoundStats()
{
    std::ostringstream o;
    static const char* kName[] = {"0-1", "1-2", "2-3", "3-4", "4-6", "6+"};
    o << "round outcomes by closest approach (m): ";
    for (int b = 0; b < 6; ++b)
        o << kName[b] << " " << g_bandHit[b] << "/" << g_bandHit[b] + g_bandMiss[b] << "  ";
    return o.str();
}

static struct
{
    bool any = false;
    u32 munition = 0, owner = 0;
    float dir[3] = {};
    double when = 0;
} g_lastHit;

bool LastRoundHit(u32& munition, u32& owner, float dir[3], double& when)
{
    if (!g_lastHit.any)
        return false;
    munition = g_lastHit.munition, owner = g_lastHit.owner, when = g_lastHit.when;
    memcpy(dir, g_lastHit.dir, sizeof(g_lastHit.dir));
    return true;
}

void TrackRounds()
{
    struct Track
    {
        float miss;
        double tHit; // absolute time of the predicted closest approach
        double seen; // last time it was in the list
        u32 munition, owner;
        float dir[3]; // flight direction (unit)
    };
    static std::unordered_map<uintptr_t, Track> live; // by CBeam pointer
    static LARGE_INTEGER freq = {};
    if (!freq.QuadPart)
        QueryPerformanceFrequency(&freq);
    LARGE_INTEGER qpc;
    QueryPerformanceCounter(&qpc);
    const double now = static_cast<double>(qpc.QuadPart) / static_cast<double>(freq.QuadPart);
    const Api& a = GetApi();
    const u32 player = InSpace() ? Mem<u32>(exe::PlayerShipId) : 0;
    if (!a.ok || !player)
    {
        live.clear();
        return;
    }
    // which tracked rounds still exist
    std::vector<void*> objs;
    Collect(a, kClassBeam, 0, objs);
    std::unordered_map<uintptr_t, bool> present;
    for (void* o : objs)
        present[reinterpret_cast<uintptr_t>(o)] = true;
    for (auto it = live.begin(); it != live.end();)
    {
        const Track& t = it->second;
        const bool there = present.count(it->first) != 0;
        if (!there) // vanished: at its predicted time = a hit (else it ended otherwise: not counted)
        {
            if (std::fabs(now - t.tHit) < 0.08 || (t.seen <= t.tHit && now >= t.tHit - 0.02))
            {
                ++g_bandHit[Band(t.miss)];
                g_lastHit.any = true;
                g_lastHit.munition = t.munition, g_lastHit.owner = t.owner, g_lastHit.when = now;
                memcpy(g_lastHit.dir, t.dir, sizeof(t.dir));
            }
            it = live.erase(it);
        }
        else if (now > t.tHit + 0.15) // still flying well after its closest approach: a miss
        {
            ++g_bandMiss[Band(t.miss)];
            it = live.erase(it);
        }
        else
        {
            it->second.seen = now;
            ++it;
        }
    }
    // new rounds predicted to reach the hull's box within 1 s (the same test as IncomingRounds)
    float me[3];
    if (!Location(a, player, me))
        return;
    // pointers of the rounds IncomingRounds reports: match by re-reading each beam here
    static const HMODULE common = GetModuleHandleA("common.dll");
    static const LastPosFn lastPos =
        reinterpret_cast<LastPosFn>(GetProcAddress(common, "?get_last_pos@CBeam@@QBEABVVector@@XZ"));
    static const VelocityFn velocity =
        reinterpret_cast<VelocityFn>(GetProcAddress(common, "?get_velocity@CBeam@@UBE?AVVector@@XZ"));
    static const OwnerFn owner = reinterpret_cast<OwnerFn>(GetProcAddress(common, "?get_owner@CProjectile@@QBEABIXZ"));
    static const MunitionFn munition =
        reinterpret_cast<MunitionFn>(GetProcAddress(common, "?munitionarch@CBeam@@QBEPBUMunition@Archetype@@XZ"));
    if (!lastPos || !velocity || !owner || !munition)
        return;
    float mn[3] = {}, mx[3] = {}, pose[9] = {}, posePos[3];
    if (!ShipExtents(PlayerCShip(), mn, mx) || !ObjectPose(player, posePos, pose))
        return;
    for (void* o : objs)
    {
        const uintptr_t key = reinterpret_cast<uintptr_t>(o);
        if (live.count(key) || !IsClass(o, "??_7CBeam@@6B@"))
            continue;
        float p[3], v[3];
        u32 own = 0;
        const void* arch = nullptr;
        if (!ReadBeam(o, lastPos, velocity, owner, munition, p, v, own, arch) || own == player)
            continue;
        // the player's own motion is small against a round's (~600-900 m/s) over < 1 s: ignored here
        const float d[3] = {p[0] - me[0], p[1] - me[1], p[2] - me[2]};
        const float ww = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
        if (ww < 1)
            continue;
        const float t = -(d[0] * v[0] + d[1] * v[1] + d[2] * v[2]) / ww;
        if (t < 0.05f || t > 1.0f)
            continue;
        const float c[3] = {d[0] + v[0] * t, d[1] + v[1] * t, d[2] + v[2] * t};
        bool in = true;
        for (int k = 0; k < 3 && in; ++k)
        {
            const float local = c[0] * pose[k] + c[1] * pose[3 + k] + c[2] * pose[6 + k];
            in = local >= mn[k] - 1.0f && local <= mx[k] + 1.0f;
        }
        if (in)
        {
            const float vl = std::sqrt(ww);
            const u32 mun = arch && IsMunitionArch(arch) ? Mem<u32>(reinterpret_cast<uintptr_t>(arch) + 0x4) : 0;
            live[key] = {std::sqrt(c[0] * c[0] + c[1] * c[1] + c[2] * c[2]),
                         now + t,
                         now,
                         mun,
                         own,
                         {v[0] / vl, v[1] / vl, v[2] / vl}};
        }
    }
}
} // namespace space
