// Obstacles and rays: the obstacle classes and their accessors, Obstacles(), the game's own ray test (PhySys
// FindRayCollisions: raw hits, sight blockers, hit distance) and the player's physical radius. Split from space.cpp
// on 24 Oct 2026 (a file move: the functions are unchanged).
#include "game/space_internal.h"

#include "flexe.h"

namespace space
{
// Obstacles: class ids (FLSDK CObject::Class) and the exported accessors that read them.
constexpr u32 kClassAsteroid = 0x1003, kClassDynamicAsteroid = 0x4001;
// A solar's moving part (a Rev joint driven by its animation script) answers the ray test as its own object of class 0x1
// (session 6, 23 Oct 2026, `ap rayraw`: the M12 core's rotating reactor - six spokes at 36 deg/s - hit as class 0x1,
// id 4284637536; filtered out, the core read as empty where the user's ship struck a spoke)
constexpr u32 kClassSolarPart = 0x1;

struct ObstacleApi
{
    const float*(__thiscall* pos)(const void* obj) = nullptr;           // EngineObject::get_position
    float(__thiscall* radius)(const void* obj) = nullptr;               // EngineObject::get_radius
    float(__thiscall* physRadius)(const void* obj, float* c) = nullptr; // CSimple::get_physical_radius_r
    bool(__thiscall* isMine)(void* obj) = nullptr;                      // CAsteroid::is_mine
    bool ok = false;
};

static const ObstacleApi& GetObstacleApi()
{
    static ObstacleApi a;
    static bool done = false;
    HMODULE c = GetModuleHandleA("common.dll");
    if (done || !c)
        return a;
    done = true;
    auto get = [c](const char* n) { return reinterpret_cast<void*>(GetProcAddress(c, n)); };
    a.pos = reinterpret_cast<decltype(a.pos)>(get("?get_position@EngineObject@@QBEABVVector@@XZ"));
    a.radius = reinterpret_cast<decltype(a.radius)>(get("?get_radius@EngineObject@@QBE?BMXZ"));
    a.physRadius = reinterpret_cast<decltype(a.physRadius)>(get("?get_physical_radius_r@CSimple@@UBEMAAVVector@@@Z"));
    a.isMine = reinterpret_cast<decltype(a.isMine)>(get("?is_mine@CAsteroid@@QAE_NXZ"));
    a.ok = a.pos && a.radius && a.physRadius;
    return a;
}

static bool ReadObstacle(const Api& a, const ObstacleApi& o, void* obj, u32 cls, Obstacle& out)
{
    __try
    {
        const float* p = o.pos(obj);
        memcpy(out.pos, p, sizeof(out.pos));
        if (cls == kClassDynamicAsteroid)
            out.radius = o.radius(obj);
        else
        {
            float c[3] = {};
            out.radius = o.physRadius(obj, c);
            // The physical sphere's centre (the radius is about it; it can sit off the object's origin).
            if (std::isfinite(c[0]) && (c[0] != 0 || c[1] != 0 || c[2] != 0))
                memcpy(out.pos, c, sizeof(out.pos));
        }
        out.asteroid = cls == kClassAsteroid || cls == kClassDynamicAsteroid;
        out.breakable = cls == kClassDynamicAsteroid; // debris that breaks on contact / fire (and drops loot)
        out.ship = cls == kClassShip;
        if (cls == kClassAsteroid && o.isMine)
            out.mine = o.isMine(obj);
        if (cls != kClassDynamicAsteroid)
        {
            out.id = *a.getId(obj, nullptr);
            out.type = a.getType(obj, nullptr);
        }
        return out.radius > 0 && std::isfinite(out.radius);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

// Every readable obstacle object of the player's system, in class then registry order (what Obstacles filters by its
// sphere); memoised per frame (space.h BeginFrame) - the walk and the reads do not depend on the caller's centre.
struct ObstacleScan
{
    bool inSpace = false; // the Api is complete and the player flies
    u32 system = 0;
    std::vector<Obstacle> all;
};
static FrameMemo<ObstacleScan> g_obstacleMemo;

static ObstacleScan ScanObstacles()
{
    ObstacleScan s;
    const Api& a = GetApi();
    const ObstacleApi& o = GetObstacleApi();
    if (!a.ok || !o.ok || !InSpace())
        return s;
    s.inSpace = true;
    s.system = Mem<u32>(exe::PlayerSystem);
    void* me = PlayerCShip();
    for (u32 cls : {kClassAsteroid, kClassDynamicAsteroid, kClassSolar, kClassShip})
    {
        std::vector<void*> objs;
        Collect(a, cls, s.system, objs);
        for (void* obj : objs)
        {
            if (obj == me)
                continue;
            Obstacle ob;
            if (ReadObstacle(a, o, obj, cls, ob))
                s.all.push_back(ob);
        }
    }
    return s;
}

std::vector<Obstacle> Obstacles(const float center[3], float range)
{
    std::vector<Obstacle> out;
    // memo off (stamp 0): this call's own scan, held in the memo but never reused
    const ObstacleScan& s = g_obstacleMemo.Valid() ? g_obstacleMemo.value : g_obstacleMemo.Fill(ScanObstacles());
    if (!s.inSpace)
        return out;
    for (const Obstacle& ob : s.all)
    {
        const float d[3] = {ob.pos[0] - center[0], ob.pos[1] - center[1], ob.pos[2] - center[2]};
        if (std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]) - ob.radius <= range)
            out.push_back(ob);
    }
    AppendLethalZones(s.system, center, range, out);
    return out;
}

// Line of sight through the game's own physics world: Common.dll PhySys::FindRayCollisions(system, from, to, RayHit*,
// n) tests the real collision hulls (FLSDK; FLUF uses it for explosion line of sight). RayHit = {CObject*, position,
// velocity, crc} (32 bytes); CObject +0x4C objectClass (FLSDK CObject.hpp).
struct RayHit
{
    void* object;
    float pos[3], vel[3];
    u32 crc;
};
static_assert(sizeof(RayHit) == 32, "PhySys::RayHit");
typedef int(__cdecl* FindRayCollisionsFn)(u32 system, const float* from, const float* to, RayHit* hits, int count);

// (the guarded part: plain data only)
static int RayHitsRawImpl(FindRayCollisionsFn find, const Api& a, const float from[3], const float to[3], RayHit* hits,
                          u32* cls, u32* ids)
{
    __try
    {
        const int n = find(Mem<u32>(exe::PlayerSystem), from, to, hits, 16);
        for (int i = 0; i < n && i < 16; ++i)
            if (hits[i].object)
            {
                cls[i] = *reinterpret_cast<const u32*>(static_cast<const char*>(hits[i].object) + 0x4C);
                if (a.getId)
                    if (const u32* p = a.getId(hits[i].object, nullptr))
                        ids[i] = *p;
            }
        return n;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return -1;
    }
}

// Every raw hit of the game's ray test on a segment, unfiltered (debug `ap rayraw`; session 6, 23 Oct 2026: the M12 core's
// rotating reactor was invisible to RayHitDistance - which kind of hit, if any, does it give?)
std::string RayHitsRaw(const float from[3], const float to[3])
{
    const Api& a = GetApi();
    static FindRayCollisionsFn find = reinterpret_cast<FindRayCollisionsFn>(
        GetProcAddress(GetModuleHandleA("common.dll"), "?FindRayCollisions@PhySys@@YAHIABVVector@@0PAURayHit@1@H@Z"));
    if (!find || !InSpace())
        return "rayraw: not in space";
    RayHit hits[16];
    u32 cls[16] = {}, ids[16] = {};
    const int n = RayHitsRawImpl(find, a, from, to, hits, cls, ids);
    if (n < 0)
        return "rayraw: fault";
    char line[200];
    sprintf_s(line, "rayraw hits=%d", n);
    std::string s = line;
    for (int i = 0; i < n && i < 16; ++i)
    {
        const float d[3] = {hits[i].pos[0] - from[0], hits[i].pos[1] - from[1], hits[i].pos[2] - from[2]};
        sprintf_s(line, "\n  obj=%p class=0x%X id=%u crc=0x%08X dist=%.1f at=(%.0f,%.0f,%.0f)", hits[i].object, cls[i],
                  ids[i], hits[i].crc, sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]), hits[i].pos[0], hits[i].pos[1],
                  hits[i].pos[2]);
        s += line;
    }
    return s;
}

u32 SightBlocker(const float from[3], const float to[3], u32 ignore, u32* blockerId)
{
    if (blockerId)
        *blockerId = 0;
    const Api& a = GetApi();
    static FindRayCollisionsFn find = reinterpret_cast<FindRayCollisionsFn>(
        GetProcAddress(GetModuleHandleA("common.dll"), "?FindRayCollisions@PhySys@@YAHIABVVector@@0PAURayHit@1@H@Z"));
    if (!find || !InSpace())
        return 0;
    RayHit hits[16];
    __try
    {
        const int n = find(Mem<u32>(exe::PlayerSystem), from, to, hits, 16);
        for (int i = 0; i < n && i < 16; ++i)
        {
            if (!hits[i].object)
                continue;
            const u32 cls = *reinterpret_cast<const u32*>(static_cast<const char*>(hits[i].object) + 0x4C);
            // Solid and unbreakable: solars (stations, bases, planets, platforms) and field rocks. Ships (the target,
            // us, the others) move; dynamic asteroids break; loot, munitions and the like do not stop a ship.
            if (cls != kClassSolar && cls != kClassAsteroid && cls != kClassSolarPart)
                continue;
            const u32* id = a.getId ? a.getId(hits[i].object, nullptr) : nullptr;
            if (ignore && id && *id == ignore)
                continue; // the target itself (a platform's own hull ends the ray)
            if (blockerId && id)
                *blockerId = *id;
            return cls;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
    return 0;
}

float RayHitDistance(const float from[3], const float to[3], const u32* ignore, int nIgnore)
{
    const Api& a = GetApi();
    static FindRayCollisionsFn find = reinterpret_cast<FindRayCollisionsFn>(
        GetProcAddress(GetModuleHandleA("common.dll"), "?FindRayCollisions@PhySys@@YAHIABVVector@@0PAURayHit@1@H@Z"));
    if (!find || !InSpace())
        return -1;
    RayHit hits[16];
    float best = -1;
    const float sd[3] = {to[0] - from[0], to[1] - from[1], to[2] - from[2]};
    const float segLen = sqrtf(sd[0] * sd[0] + sd[1] * sd[1] + sd[2] * sd[2]);
    __try
    {
        const int n = find(Mem<u32>(exe::PlayerSystem), from, to, hits, 16);
        for (int i = 0; i < n && i < 16; ++i)
        {
            if (!hits[i].object)
                continue;
            const u32 cls = *reinterpret_cast<const u32*>(static_cast<const char*>(hits[i].object) + 0x4C);
            if (cls != kClassSolar && cls != kClassAsteroid && cls != kClassSolarPart)
                continue;
            if (nIgnore > 0 && a.getId)
            {
                const u32* id = a.getId(hits[i].object, nullptr);
                bool skip = false;
                for (int j = 0; id && j < nIgnore; ++j)
                    skip = skip || *id == ignore[j];
                if (skip)
                    continue;
            }
            const float d[3] = {hits[i].pos[0] - from[0], hits[i].pos[1] - from[1], hits[i].pos[2] - from[2]};
            const float dist = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
            // a "hit" at the segment's very end is the test's own end point, not a surface (22 Oct 2026: every guard
            // event read hit = the full look-ahead, clear again 0.1 s later)
            if (dist >= 0.97f * segLen)
                continue;
            if (best < 0 || dist < best)
                best = dist;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return -1;
    }
    return best;
}

float PlayerRadius()
{
    const ObstacleApi& o = GetObstacleApi();
    void* me = PlayerCShip();
    if (!o.ok || !me)
        return 0;
    float r = 0, c[3];
    __try
    {
        r = o.physRadius(me, c);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
    return std::isfinite(r) ? r : 0;
}
} // namespace space
