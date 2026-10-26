// The universe data of the player's system: the game's zone test, the system file, encounter levels, strict zones
// and lethal spheres, planet dock rings, the area readout and the zones along a segment. Split from space.cpp on
// 24 Oct 2026 (a file move: the functions are unchanged).
#include "game/space_internal.h"

#include "flexe.h"
#include "str.h"

#include <fstream>
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

// An ini line normalised: its ';' comment cut, then trimmed (core\str.h). The key / value substrings of such a line
// hold no ';' any more, so the same call serves them.
static std::string Trim(std::string s) { return str::Trim(str::StripComment(s)); }

// The system's file (DATA\UNIVERSE\...), from universe.ini [system] nickname + file; empty if unknown.
std::string SystemFile(u32 system)
{
    const Api& a = GetApi();
    std::string file;
    if (!a.createId)
        return file;
    {
        std::ifstream in(DataPath("UNIVERSE\\universe.ini"));
        std::string line, nick, f;
        bool sys = false;
        auto done = [&]()
        {
            if (sys && !nick.empty() && !f.empty() && a.createId(nick.c_str()) == system)
                file = f;
        };
        while (std::getline(in, line) && file.empty())
        {
            line = Trim(line);
            if (line.empty())
                continue;
            if (line[0] == '[')
            {
                done();
                sys = _stricmp(line.c_str(), "[system]") == 0;
                nick.clear();
                f.clear();
                continue;
            }
            const size_t eq = line.find('=');
            if (eq == std::string::npos)
                continue;
            const std::string k = Trim(line.substr(0, eq)), v = Trim(line.substr(eq + 1));
            if (_stricmp(k.c_str(), "nickname") == 0)
                nick = v;
            else if (_stricmp(k.c_str(), "file") == 0)
                f = v;
        }
        done();
    }
    return file;
}

// Zone id -> highest encounter level of that zone, for one system (by system id; parsed once).
static const std::unordered_map<u32, int>& ZoneLevels(u32 system)
{
    static std::unordered_map<u32, std::unordered_map<u32, int>> cache;
    auto found = cache.find(system);
    if (found != cache.end())
        return found->second;
    std::unordered_map<u32, int>& levels = cache[system];
    const Api& a = GetApi();
    const std::string file = SystemFile(system);
    if (file.empty())
        return levels;
    std::ifstream in(DataPath(("UNIVERSE\\" + file).c_str()));
    std::string line;
    u32 zone = 0;
    while (std::getline(in, line))
    {
        line = Trim(line);
        if (line.empty())
            continue;
        if (line[0] == '[')
        {
            zone = 0;
            continue;
        }
        const size_t eq = line.find('=');
        if (eq == std::string::npos)
            continue;
        const std::string k = Trim(line.substr(0, eq)), v = Trim(line.substr(eq + 1));
        if (_stricmp(k.c_str(), "nickname") == 0)
            zone = a.createId(v.c_str());
        else if (zone && _stricmp(k.c_str(), "encounter") == 0)
        {
            const size_t c1 = v.find(',');
            const int level = c1 == std::string::npos ? 0 : atoi(v.c_str() + c1 + 1);
            int& l = levels[zone];
            l = std::max(l, level);
        }
    }
    return levels;
}

// Strict zones of a system (parsed once): fields where contact is costly - the field file's [properties] flags
// mine_danger_objects (minefields), gas_danger_objects (explosive gas), object_density_high (extremely dense), DAMAGING
// - and zones with `damage` > 0 (ZoneRules::strictDamage; radiation, planet / sun cores). A field's [Exclusion Zones]
// (gate and lane corridors through it) are not part of it. Text inis only (a binary BINI field file is skipped).
struct StrictZone
{
    std::string why;
    std::vector<u32> exclusions;
};
// Lethal spheres of a system (filled with StrictZones): zones of shape SPHERE whose damage kills in moments - planet
// and sun cores (2000000 and 10000 per second in Li01; radiation fields deal 40-50 and stay crossable strict zones).
// They are obstacles: a straight leg through Pittsburgh's death zone killed the ship in M01b (19 Oct 2026). The
// threshold: ZoneRules::lethalDamage (space.h; 1000 per second, was the constant kLethalZoneDamage).
struct LethalSphere
{
    float pos[3];
    float radius;
};
// The parsed tables per rules (the defaults: one entry, as before the rules were parameters), each per system.
struct ZoneTables
{
    std::unordered_map<u32, std::unordered_map<u32, StrictZone>> strict;
    std::unordered_map<u32, std::vector<LethalSphere>> lethal;
};
static ZoneTables& Tables(const ZoneRules& rules)
{
    static std::map<std::pair<float, float>, ZoneTables> all; // map: the tables' addresses stay put
    return all[{rules.lethalDamage, rules.strictDamage}];
}

static const std::unordered_map<u32, StrictZone>& StrictZones(u32 system, const ZoneRules& rules)
{
    ZoneTables& tables = Tables(rules);
    std::unordered_map<u32, std::unordered_map<u32, StrictZone>>& cache = tables.strict;
    auto found = cache.find(system);
    if (found != cache.end())
        return found->second;
    std::unordered_map<u32, StrictZone>& zones = cache[system];
    std::vector<LethalSphere>& lethal = tables.lethal[system];
    const Api& a = GetApi();
    const std::string file = SystemFile(system);
    if (file.empty() || !a.createId)
        return zones;
    struct Field
    {
        std::string file, zone;
    };
    std::vector<Field> fields;
    {
        std::ifstream in(DataPath(("UNIVERSE\\" + file).c_str()));
        std::string line, section, nick, fieldFile, fieldZone, shape;
        float damage = 0, size = 0, pos[3] = {};
        bool havePos = false;
        auto flush = [&]()
        {
            if (section == "[zone]" && !nick.empty() && damage > rules.strictDamage)
            {
                // A lethal sphere (a planet's / sun's death zone) is an avoidance obstacle with its berth, not a field
                // to fly through carefully: counting it as strict too made every planet dock's last kilometre strict
                // (the 3 s look-ahead reaches into it beside the ring) and the avoidance swung wide (user, 20 Oct
                // 2026, M04 Planet Leeds: 34 s vs 25 s planned). Strict stays for crossable damage (radiation etc.).
                const bool lethalSphere =
                    damage >= rules.lethalDamage && _stricmp(shape.c_str(), "sphere") == 0 && havePos && size > 0;
                if (lethalSphere)
                    lethal.push_back({{pos[0], pos[1], pos[2]}, size});
                else
                    zones[a.createId(nick.c_str())].why = "damage";
            }
            if ((section == "[asteroids]" || section == "[nebula]") && !fieldFile.empty() && !fieldZone.empty())
                fields.push_back({fieldFile, fieldZone});
            nick.clear(), fieldFile.clear(), fieldZone.clear(), shape.clear(), damage = size = 0, havePos = false;
        };
        while (std::getline(in, line))
        {
            line = Trim(line);
            if (line.empty())
                continue;
            if (line[0] == '[')
            {
                flush();
                section = line;
                for (char& c : section)
                    c = static_cast<char>(tolower(c));
                continue;
            }
            const size_t eq = line.find('=');
            if (eq == std::string::npos)
                continue;
            const std::string k = Trim(line.substr(0, eq)), v = Trim(line.substr(eq + 1));
            if (_stricmp(k.c_str(), "nickname") == 0)
                nick = v;
            else if (_stricmp(k.c_str(), "damage") == 0)
                damage = static_cast<float>(atof(v.c_str()));
            else if (_stricmp(k.c_str(), "shape") == 0)
                shape = v;
            else if (_stricmp(k.c_str(), "size") == 0)
                size = static_cast<float>(atof(v.c_str())); // a sphere's radius (first value)
            else if (_stricmp(k.c_str(), "pos") == 0)
                havePos = sscanf_s(v.c_str(), "%f , %f , %f", &pos[0], &pos[1], &pos[2]) == 3;
            else if (_stricmp(k.c_str(), "file") == 0)
                fieldFile = v;
            else if (_stricmp(k.c_str(), "zone") == 0)
                fieldZone = v;
        }
        flush();
    }
    for (const Field& f : fields)
    {
        std::ifstream in(DataPath(f.file.c_str()));
        std::string line, section, why;
        std::vector<u32> excl;
        while (std::getline(in, line))
        {
            if (line.compare(0, 4, "BINI") == 0)
                break; // binary ini: not read
            line = Trim(line);
            if (line.empty())
                continue;
            if (line[0] == '[')
            {
                section = line;
                for (char& c : section)
                    c = static_cast<char>(tolower(c));
                continue;
            }
            const size_t eq = line.find('=');
            if (eq == std::string::npos)
                continue;
            const std::string k = Trim(line.substr(0, eq));
            std::string v = Trim(line.substr(eq + 1));
            if (section == "[properties]" && _stricmp(k.c_str(), "flag") == 0)
            {
                for (char& c : v)
                    c = static_cast<char>(tolower(c));
                if (v == "mine_danger_objects" || v == "gas_danger_objects" || v == "object_density_high" ||
                    v == "damaging")
                    why += (why.empty() ? "" : ",") + v;
            }
            else if (section == "[exclusion zones]" && _stricmp(k.c_str(), "exclusion") == 0)
                excl.push_back(a.createId(v.c_str()));
        }
        if (!why.empty())
            zones[a.createId(f.zone.c_str())] = {why, excl};
    }
    return zones;
}

// A planet's docking ring, from the system file: the object whose `dock_with` names the planet's `base` (Li02:
// Li02_02 base = Li02_02_base, Li02_02_dock_ring dock_with = Li02_02_base). Parsed once per system. 0 if none.
u32 PlanetDockRing(u32 planet)
{
    static std::unordered_map<u32, std::unordered_map<u32, u32>> cache; // system -> planet id -> ring id
    if (!InSpace() || !planet)
        return 0;
    const u32 system = Mem<u32>(exe::PlayerSystem);
    auto found = cache.find(system);
    if (found == cache.end())
    {
        std::unordered_map<u32, u32>& rings = cache[system];
        const Api& a = GetApi();
        const std::string file = SystemFile(system);
        if (!file.empty() && a.createId)
        {
            std::ifstream in(DataPath(("UNIVERSE\\" + file).c_str()));
            std::string line, section, nick, base, dockWith;
            std::unordered_map<std::string, u32> planetByBase;    // base (lower) -> planet id
            std::vector<std::pair<std::string, u32>> ringsByBase; // dock_with (lower), ring id
            auto lower = [](std::string s)
            {
                for (char& c : s)
                    c = static_cast<char>(tolower(c));
                return s;
            };
            auto flush = [&]()
            {
                if (section == "[object]" && !nick.empty())
                {
                    if (!dockWith.empty())
                        ringsByBase.push_back({lower(dockWith), a.createId(nick.c_str())});
                    else if (!base.empty())
                        planetByBase[lower(base)] = a.createId(nick.c_str());
                }
                nick.clear(), base.clear(), dockWith.clear();
            };
            while (std::getline(in, line))
            {
                line = Trim(line);
                if (line.empty())
                    continue;
                if (line[0] == '[')
                {
                    flush();
                    section = lower(line);
                    continue;
                }
                const size_t eq = line.find('=');
                if (eq == std::string::npos)
                    continue;
                const std::string k = Trim(line.substr(0, eq)), v = Trim(line.substr(eq + 1));
                if (_stricmp(k.c_str(), "nickname") == 0)
                    nick = v;
                else if (_stricmp(k.c_str(), "base") == 0)
                    base = v;
                else if (_stricmp(k.c_str(), "dock_with") == 0)
                    dockWith = v;
            }
            flush();
            for (const auto& r : ringsByBase)
            {
                const auto p = planetByBase.find(r.first);
                if (p != planetByBase.end())
                    rings[p->second] = r.second;
            }
        }
        found = cache.find(system);
    }
    const auto r = found->second.find(planet);
    return r != found->second.end() ? r->second : 0;
}

void AppendLethalZones(u32 system, const float center[3], float range, std::vector<Obstacle>& out,
                       const ZoneRules& rules)
{
    StrictZones(system, rules); // parses the system once (fills the rules' lethal table)
    for (const LethalSphere& z : Tables(rules).lethal[system])
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
