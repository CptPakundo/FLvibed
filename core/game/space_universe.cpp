// The universe data of the player's system: the game's zone test, the system file, encounter levels, strict zones
// and lethal spheres, planet dock rings, the area readout and the zones along a segment. Split from space.cpp on
// 24 Oct 2026 (a file move: the functions are unchanged). Since 29 Oct 2026 the tables come from
// core\game\universe_data (ini::Read: HD Edition's text inis and vanilla 1.1's BINI alike; identical tables on HDE,
// tests\universe_test.cpp); this file caches them per system and answers the game-side questions.
#include "game/space_internal.h"

#include "flexe.h"
#include "game/ini.h"
#include "game/universe_data.h"

#include <map>
#include <unordered_map>

namespace space
{
// ---- the danger of where the player is: the encounter levels of the zones at the ship (the system file's
// `encounter = <name>, <level>, <chance>` lines; the zones by the game's own zone test) against the player's rank.
static int ZonesAt(u32 system, const float pos[3], u32* ids, u32 max)
{
    static auto inZones = reinterpret_cast<int(__cdecl*)(u32, const float*, float, u32*, u32, u32&)>(
        GetProcAddress(GetModuleHandleA("server.dll"), "?InZones@System@pub@@YAHIABVVector@@MQAIIAAI@Z"));
    if (!inZones)
        return -1;
    u32 count = 0;
    __try
    {
        inZones(system, pos, 0.0f, ids, max, count);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return -1;
    }
    return static_cast<int>(count < max ? count : max);
}

// The system's file (DATA\UNIVERSE\...), from universe.ini [system] nickname + file; empty if unknown.
std::string SystemFile(u32 system)
{
    const Api& a = GetApi();
    if (!a.createId)
        return std::string();
    static const std::vector<ini::Section> universe = ini::Read(DataPath("UNIVERSE\\universe.ini")); // read once
    return unidata::SystemFile(universe, system, a.createId);
}

// A system's tables (core\game\universe_data.h: encounter levels, strict zones - minefields, explosive gas, extremely
// dense or damaging fields, damage zones -, lethal spheres - planet / sun cores: 2000000 and 10000 per second in Li01,
// the M01b death of 19 Oct 2026 was a straight leg through Pittsburgh's -, planet dock rings), parsed once per system
// and zone rules (the defaults: one entry, as before the rules were parameters). References stay valid (node maps).
static const unidata::System& Parsed(u32 system, const ZoneRules& rules)
{
    static std::map<std::pair<float, float>, std::unordered_map<u32, unidata::System>> all;
    std::unordered_map<u32, unidata::System>& bySystem = all[{rules.lethalDamage, rules.strictDamage}];
    const auto found = bySystem.find(system);
    if (found != bySystem.end())
        return found->second;
    unidata::System& s = bySystem[system];
    const Api& a = GetApi();
    const std::string file = SystemFile(system);
    if (!file.empty() && a.createId)
        s = unidata::ParseSystem(DataPath(""), file, a.createId, rules.lethalDamage, rules.strictDamage);
    return s;
}

// Zone id -> highest encounter level of that zone, for one system.
static const std::unordered_map<u32, int>& ZoneLevels(u32 system) { return Parsed(system, ZoneRules{}).levels; }

// Strict zones of a system (a field's [Exclusion Zones] - gate and lane corridors through it - are not part of it).
static const std::unordered_map<u32, unidata::StrictZone>& StrictZones(u32 system, const ZoneRules& rules)
{
    return Parsed(system, rules).strict;
}

// A planet's docking ring, from the system file: the object whose `dock_with` names the planet's `base` (Li02:
// Li02_02 base = Li02_02_base, Li02_02_dock_ring dock_with = Li02_02_base). 0 if none.
u32 PlanetDockRing(u32 planet)
{
    if (!InSpace() || !planet)
        return 0;
    const std::unordered_map<u32, u32>& rings = Parsed(Mem<u32>(exe::PlayerSystem), ZoneRules{}).dockRings;
    const auto r = rings.find(planet);
    return r != rings.end() ? r->second : 0;
}

bool IsDockRing(u32 object)
{
    if (!InSpace() || !object)
        return false;
    for (const auto& r : Parsed(Mem<u32>(exe::PlayerSystem), ZoneRules{}).dockRings)
        if (r.second == object)
            return true;
    return false;
}

// The system's lethal spheres (zones of shape SPHERE whose damage kills in moments, ZoneRules::lethalDamage: planet and
// sun cores; radiation fields deal 40-50 and stay crossable strict zones) within `range` of `center`: obstacles.
void AppendLethalZones(u32 system, const float center[3], float range, std::vector<Obstacle>& out,
                       const ZoneRules& rules)
{
    for (const unidata::LethalSphere& z : Parsed(system, rules).lethal)
    {
        const float d[3] = {z.pos[0] - center[0], z.pos[1] - center[1], z.pos[2] - center[2]};
        if (std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]) - z.radius > range)
            continue;
        Obstacle ob;
        memcpy(ob.pos, z.pos, sizeof(ob.pos));
        ob.radius = z.radius;
        ob.zone = true;
        out.push_back(ob);
    }
}

std::vector<Obstacle> LethalZones(const float center[3], float range, const ZoneRules& rules)
{
    std::vector<Obstacle> out;
    if (InSpace())
        AppendLethalZones(Mem<u32>(exe::PlayerSystem), center, range, out, rules);
    return out;
}

std::vector<Obstacle> LethalZones(const float center[3], float range)
{
    return LethalZones(center, range, ZoneRules{});
}

std::string StrictAt(const float pos[3]) { return StrictAt(pos, ZoneRules{}); }

int FieldAt(const float pos[3])
{
    if (!InSpace())
        return 0;
    const u32 system = Mem<u32>(exe::PlayerSystem);
    const auto& fields = Parsed(system, ZoneRules{}).fieldKinds;
    if (fields.empty())
        return 0;
    u32 ids[32];
    const int n = ZonesAt(system, pos, ids, 32);
    for (int i = 0; i < n; ++i)
        if (const auto f = fields.find(ids[i]); f != fields.end())
            return f->second;
    return 0;
}

std::string StrictAt(const float pos[3], const ZoneRules& rules)
{
    if (!InSpace())
        return "";
    const u32 system = Mem<u32>(exe::PlayerSystem);
    const auto& strict = StrictZones(system, rules);
    if (strict.empty())
        return "";
    u32 ids[32];
    const int n = ZonesAt(system, pos, ids, 32);
    for (int i = 0; i < n; ++i)
    {
        const auto z = strict.find(ids[i]);
        if (z == strict.end())
            continue;
        bool excluded = false;
        for (u32 e : z->second.exclusions)
            for (int j = 0; j < n && !excluded; ++j)
                excluded = ids[j] == e;
        if (!excluded)
            return z->second.why;
    }
    return "";
}

bool ReadArea(Area& out)
{
    out = Area();
    const Api& a = GetApi();
    const u32 player = InSpace() ? Mem<u32>(exe::PlayerShipId) : 0;
    float me[3];
    if (!a.ok || !player || !Location(a, player, me))
        return false;
    static auto getRank = reinterpret_cast<int(__cdecl*)(const u32&, int&)>(
        GetProcAddress(GetModuleHandleA("server.dll"), "?GetRank@Player@pub@@YAHABIAAH@Z"));
    if (getRank)
    {
        const u32 client = 1; // single player
        int rank = -1;
        __try
        {
            if (getRank(client, rank) == 0)
                out.rank = rank;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
        }
    }
    const u32 system = Mem<u32>(exe::PlayerSystem);
    const std::unordered_map<u32, int>& levels = ZoneLevels(system);
    u32 ids[64];
    const int n = ZonesAt(system, me, ids, 64);
    for (int i = 0; i < n; ++i)
    {
        const auto it = levels.find(ids[i]);
        if (it != levels.end() && it->second > out.level)
        {
            out.level = it->second;
            out.zone = ids[i];
        }
    }
    return true;
}

bool ZonesAlong(const float a[3], const float b[3], float step, int& samples, std::vector<ZoneSpan>& out)
{
    const u32 system = Mem<u32>(exe::PlayerSystem);
    const float d[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
    const float len = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    samples = static_cast<int>(len / step) + 1;
    if (samples > 4000)
        samples = 4000;
    for (int i = 0; i <= samples; ++i)
    {
        const float t = samples ? static_cast<float>(i) / samples : 0;
        const float p[3] = {a[0] + d[0] * t, a[1] + d[1] * t, a[2] + d[2] * t};
        u32 ids[64];
        const int n = ZonesAt(system, p, ids, 64);
        if (n < 0)
            return false;
        for (int k = 0; k < n; ++k)
        {
            auto it = std::find_if(out.begin(), out.end(), [&](const ZoneSpan& z) { return z.zone == ids[k]; });
            if (it == out.end())
            {
                out.push_back({ids[k], {}});
                it = out.end() - 1;
            }
            if (!it->ranges.empty() && it->ranges.back().second == i - 1)
                it->ranges.back().second = i;
            else
                it->ranges.push_back({i, i});
        }
    }
    return true;
}
} // namespace space
