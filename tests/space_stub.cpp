#include "space_stub.h"

#include <cmath>

namespace stub
{
static std::vector<space::Obstacle> g_solids;
static std::vector<Ring> g_rings;
static int g_rays = 0;

void SetSolids(const std::vector<space::Obstacle>& solids) { g_solids = solids; }
void SetRings(const std::vector<Ring>& rings) { g_rings = rings; }
int RayCount() { return g_rays; }
void ResetRayCount() { g_rays = 0; }

static bool Ignored(u32 id, const u32* ignore, int n)
{
    for (int i = 0; i < n; ++i)
        if (ignore && ignore[i] == id)
            return true;
    return false;
}

// The nearest sphere hit on the segment, -1 when clear; `hit` (optional) the solid hit.
static float Cast(const float from[3], const float to[3], const u32* ignore, int nIgnore, const space::Obstacle** hit)
{
    ++g_rays;
    const float d[3] = {to[0] - from[0], to[1] - from[1], to[2] - from[2]};
    const float len = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    if (len < 1e-6f)
        return -1;
    const float u[3] = {d[0] / len, d[1] / len, d[2] / len};
    float best = -1;
    for (const space::Obstacle& o : g_solids)
    {
        if (o.id && Ignored(o.id, ignore, nIgnore))
            continue;
        const float q[3] = {o.pos[0] - from[0], o.pos[1] - from[1], o.pos[2] - from[2]};
        const float t = q[0] * u[0] + q[1] * u[1] + q[2] * u[2];
        const float lat2 = q[0] * q[0] + q[1] * q[1] + q[2] * q[2] - t * t;
        const float r2 = o.radius * o.radius;
        if (lat2 > r2)
            continue;
        const float h = std::sqrt(r2 - lat2);
        float t0 = t - h;
        if (t0 < 0 && t + h > 0)
            t0 = 0; // starting inside: a hit at once
        if (t0 < 0 || t0 > len)
            continue;
        if (best < 0 || t0 < best)
        {
            best = t0;
            if (hit)
                *hit = &o;
        }
    }
    return best;
}
} // namespace stub

namespace space
{
float RayHitDistance(const float from[3], const float to[3], const u32* ignore, int nIgnore)
{
    return stub::Cast(from, to, ignore, nIgnore, nullptr);
}

u32 SightBlocker(const float from[3], const float to[3], u32 ignore, u32* blockerId)
{
    const Obstacle* hit = nullptr;
    const float d = stub::Cast(from, to, &ignore, 1, &hit);
    if (blockerId)
        *blockerId = hit ? hit->id : 0;
    return d >= 0 ? kBlockerSolar : 0;
}

std::string RayHitsRaw(const float*, const float*) { return ""; }

std::vector<Obstacle> Obstacles(const float center[3], float range)
{
    std::vector<Obstacle> out;
    for (const Obstacle& o : stub::g_solids)
    {
        const float d[3] = {o.pos[0] - center[0], o.pos[1] - center[1], o.pos[2] - center[2]};
        if (std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]) - o.radius <= range)
            out.push_back(o);
    }
    return out;
}

std::vector<Obstacle> LethalZones(const float*, float) { return {}; }
std::string StrictAt(const float*) { return ""; }

bool ObjectPos(u32 id, float pos[3])
{
    for (const stub::Ring& r : stub::g_rings)
        if (r.id == id)
        {
            pos[0] = r.pos[0], pos[1] = r.pos[1], pos[2] = r.pos[2];
            return true;
        }
    return false;
}

bool LaneLinks(u32 ring, u32& next, u32& prev)
{
    for (const stub::Ring& r : stub::g_rings)
        if (r.id == ring)
        {
            next = r.next, prev = r.prev;
            return true;
        }
    return false;
}

int LaneOpen(u32, u32) { return 1; }
float JumpSeconds(u32) { return 0; }
std::vector<Contact> Contacts() { return {}; }
bool ReadPilot(Pilot&) { return false; }
float PlayerRadius() { return 10; }
u32 PlanetDockRing(u32) { return 0; }
bool AttitudeToPlayer(u32, float&) { return false; }
std::vector<Waypoint> Waypoints() { return {}; }
bool TargetLaneStart(float*, float*, u32&) { return false; }
int TargetLaneSide() { return -1; }
} // namespace space
