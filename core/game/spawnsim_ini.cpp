// The spawn simulator's offline loader (spawnsim_ini.h): the install's ini files -> spawnsim::World.
#include "game/spawnsim_ini.h"

#include "game/flhash.h"
#include "game/ini.h"
#include "str.h"

#include <cstdlib>
#include <fstream>

namespace spawnsim
{
namespace
{
double Num(const std::string& s, double def)
{
    if (s.empty())
        return def;
    char* end = nullptr;
    const double v = std::strtod(s.c_str(), &end);
    return end == s.c_str() ? def : v;
}

std::vector<double> Nums(const std::string* v)
{
    std::vector<double> out;
    if (v)
        for (const auto& f : ini::Fields(*v))
            out.push_back(Num(f, 0));
    return out;
}

bool Yes(const std::string& v) { return v == "yes" || v == "true" || v == "1"; }

// The first file of an EXE\freelancer.ini [Data] key, relative to DATA ("" if absent).
std::string DataKey(const std::string& dataDir, const char* key)
{
    for (const auto& s : ini::Read(dataDir + "..\\EXE\\freelancer.ini"))
        if (s.type == "data")
            if (const std::string* v = s.Get(key))
                return *v;
    return "";
}

std::vector<std::string> DataKeys(const std::string& dataDir, const char* key)
{
    std::vector<std::string> out;
    for (const auto& s : ini::Read(dataDir + "..\\EXE\\freelancer.ini"))
        if (s.type == "data")
            for (const auto& v : s.All(key))
                out.push_back(v);
    return out;
}

unsigned ArrivalFlags(const std::string* v)
{
    if (!v)
        return ArriveAll; // [inferred] no `arrival` line: every method
    unsigned flags = 0;
    for (std::string f : ini::Fields(*v))
    {
        f = str::Lower(f);
        const bool clear = !f.empty() && f[0] == '-';
        if (clear)
            f = f.substr(1);
        unsigned m = 0;
        if (f == "all")
            m = ArriveAll;
        else if (f == "cruise")
            m = ArriveCruise;
        else if (f == "buzz")
            m = ArriveBuzz;
        else if (f == "tradelane")
            m = ArriveTradelane;
        else if (f == "object_all")
            m = ArriveObjectAll;
        else if (f == "object_docking_ring")
            m = ArriveDockRing;
        else if (f == "object_jump_gate")
            m = ArriveJumpGate;
        else if (f == "object_station")
            m = ArriveStation;
        else if (f == "object_capital")
            m = ArriveCapital;
        flags = clear ? (flags & ~m) : (flags | m);
    }
    return flags;
}

bool ReadEncounter(const std::string& path, Encounter& enc)
{
    const auto secs = ini::Read(path);
    if (secs.empty())
        return false;
    for (const auto& s : secs)
    {
        if (s.type == "encounterformation")
        {
            Formation f;
            for (const auto& line : s.All("ship_by_class"))
            {
                const auto v = ini::Fields(line);
                if (v.size() < 3)
                    continue;
                ShipLine l;
                l.minCount = static_cast<int>(Num(v[0], 1));
                l.maxCount = std::max(l.minCount, static_cast<int>(Num(v[1], 1)));
                l.cls = str::Lower(v[2]);
                l.offset = v.size() > 3 ? static_cast<int>(Num(v[3], 0)) : 0;
                f.lines.push_back(l);
            }
            f.arrival = ArrivalFlags(s.Get("arrival"));
            if (const std::string* b = s.Get("behavior"))
            {
                const std::string v = str::Lower(*b);
                f.behavior = v == "wander" ? Behavior::Wander : v == "patrol_path" ? Behavior::Path : v == "trade" ? Behavior::Trade : Behavior::Other;
            }
            if (const std::string* v = s.Get("zone_creation_distance"))
                f.zoneCreationDistance = Num(*v, 1500);
            if (const std::string* v = s.Get("allow_simultaneous_creation"))
                f.simultaneous = Yes(str::Lower(*v));
            if (const std::string* v = s.Get("times_to_create"))
                f.timesToCreate = str::Lower(*v) == "infinite" ? -1 : static_cast<int>(Num(*v, -1));
            if (const std::string* v = s.Get("longevity"))
                f.longevity = Num(*v, 24);
            enc.formations.push_back(f);
        }
        else if (s.type == "creation")
            for (const auto& line : s.All("permutation"))
            {
                const auto v = ini::Fields(line);
                if (v.empty())
                    continue;
                Permutation p;
                std::string first = v[0];
                p.star = first.find('*') != std::string::npos;
                size_t at = 0;
                while (at < first.size())
                {
                    const size_t plus = first.find('+', at);
                    const std::string part = first.substr(at, plus == std::string::npos ? std::string::npos : plus - at);
                    p.formations.push_back(static_cast<int>(Num(str::Trim(part), -1)));
                    if (plus == std::string::npos)
                        break;
                    at = plus + 1;
                }
                p.weight = v.size() > 1 ? Num(v[1], 1) : 1;
                p.reroll = v.size() > 2 ? Num(v[2], 0) : 0;
                enc.permutations.push_back(p);
            }
    }
    return true;
}

Vec3 Pos(const std::string* v)
{
    const auto n = Nums(v);
    Vec3 p;
    if (n.size() > 0)
        p.x = n[0];
    if (n.size() > 1)
        p.y = n[1];
    if (n.size() > 2)
        p.z = n[2];
    return p;
}

int FactionIndex(World& w, std::map<std::string, int>& index, const std::string& nick,
                 const std::map<std::string, double>& standing)
{
    const std::string g = str::Lower(nick);
    const auto it = index.find(g);
    if (it != index.end())
        return it->second;
    Faction f;
    f.nick = g;
    const auto s = standing.find(g);
    f.standing = s == standing.end() ? 0 : s->second;
    w.factions.push_back(f);
    return index[g] = static_cast<int>(w.factions.size()) - 1;
}
} // namespace

std::map<std::string, double> ReadStandings(const std::string& savePath)
{
    std::map<std::string, double> out;
    std::ifstream f(savePath);
    std::string line;
    while (std::getline(f, line))
    {
        const std::string l = str::Trim(line);
        if (str::Lower(l.substr(0, 5)) != "house")
            continue;
        const size_t eq = l.find('=');
        if (eq == std::string::npos)
            continue;
        const auto v = ini::Fields(l.substr(eq + 1));
        if (v.size() >= 2)
            out[str::Lower(v[1])] = Num(v[0], 0);
    }
    return out;
}

std::string SystemNickname(const std::string& dataDir, uint32_t id)
{
    std::string universe = DataKey(dataDir, "universe");
    if (universe.empty())
        universe = "universe\\universe.ini";
    for (const auto& s : ini::Read(dataDir + universe))
        if (s.type == "system")
            if (const std::string* n = s.Get("nickname"))
                if (flhash::CreateId(*n) == id)
                    return *n;
    return "";
}

bool LoadSystem(const std::string& dataDir, const std::string& system, const std::map<std::string, double>& standing,
                World& w, std::string& error)
{
    w = World();
    std::string universe = DataKey(dataDir, "universe");
    if (universe.empty())
        universe = "universe\\universe.ini";
    const size_t slash = universe.find_last_of('\\');
    const std::string universeDir = slash == std::string::npos ? "" : universe.substr(0, slash + 1);
    std::string sysFile;
    for (const auto& s : ini::Read(dataDir + universe))
        if (s.type == "system")
        {
            const std::string* n = s.Get("nickname");
            const std::string* f = s.Get("file");
            if (n && f && str::Lower(*n) == str::Lower(system))
                sysFile = *f;
        }
    if (sysFile.empty())
    {
        error = "system " + system + " not in " + universe;
        return false;
    }
    const auto sys = ini::Read(dataDir + universeDir + sysFile);
    if (sys.empty())
    {
        error = "cannot read " + sysFile;
        return false;
    }

    // ship classes, NPC ships, faction ship lists (the fixed DATA\MISSIONS files Content.dll opens)
    for (const auto& s : ini::Read(dataDir + "MISSIONS\\shipclasses.ini"))
        if (s.type == "shipclass")
            if (const std::string* n = s.Get("nickname"))
            {
                auto& m = w.shipClasses[str::Lower(*n)];
                for (const auto& v : s.All("member"))
                    m.push_back(str::Lower(v));
            }
    // flight after spawn (spawning.md 12a): engine top speed = max_force / (engine linear_drag + ship linear_drag) of the NPC
    // ship's loadout (docs\FREELANCER_RE.md "top speed F / d"), the cruise speed from constants.ini
    std::map<std::string, std::pair<double, double>> engines; // engine nickname -> max_force, linear_drag
    for (const auto& file : DataKeys(dataDir, "equipment"))
        for (const auto& s : ini::Read(dataDir + file))
            if (s.type == "engine")
                if (const std::string* n = s.Get("nickname"))
                    engines[str::Lower(*n)] = {Num(s.Get("max_force") ? *s.Get("max_force") : "", 0),
                                               Num(s.Get("linear_drag") ? *s.Get("linear_drag") : "", 0)};
    std::map<std::string, double> shipDrag; // ship archetype -> linear_drag
    for (const auto& file : DataKeys(dataDir, "ships"))
        for (const auto& s : ini::Read(dataDir + file))
            if (s.type == "ship")
                if (const std::string* n = s.Get("nickname"))
                    shipDrag[str::Lower(*n)] = Num(s.Get("linear_drag") ? *s.Get("linear_drag") : "", 0);
    std::map<std::string, std::string> loadoutEngine; // loadout -> its engine (the first equip line naming an [Engine])
    for (const auto& file : DataKeys(dataDir, "loadouts"))
        for (const auto& s : ini::Read(dataDir + file))
            if (s.type == "loadout")
                if (const std::string* n = s.Get("nickname"))
                    for (const auto& e : s.All("equip"))
                    {
                        const auto fl = ini::Fields(e);
                        if (!fl.empty() && engines.count(str::Lower(fl[0])))
                        {
                            loadoutEngine.emplace(str::Lower(*n), str::Lower(fl[0]));
                            break;
                        }
                    }
    const std::string constants = DataKey(dataDir, "constants");
    for (const auto& s : ini::Read(dataDir + (constants.empty() ? "constants.ini" : constants)))
        if (s.type == "engineequipconsts")
            if (const std::string* v = s.Get("cruising_speed"))
                w.cruiseSpeed = Num(*v, 0);
    std::map<std::string, int> shipIndex;
    for (const auto& s : ini::Read(dataDir + "MISSIONS\\npcships.ini"))
        if (s.type == "npcshiparch")
            if (const std::string* n = s.Get("nickname"))
            {
                NpcShip ship;
                ship.nick = str::Lower(*n);
                for (const auto& line : s.All("npc_class"))
                    for (const auto& c : ini::Fields(line))
                        ship.classes.push_back(str::Lower(c));
                const std::string* lo = s.Get("loadout");
                const std::string* arch = s.Get("ship_archetype");
                const auto le = lo ? loadoutEngine.find(str::Lower(*lo)) : loadoutEngine.end();
                if (le != loadoutEngine.end())
                {
                    const auto& eng = engines[le->second];
                    const auto sd = arch ? shipDrag.find(str::Lower(*arch)) : shipDrag.end();
                    const double drag = eng.second + (sd != shipDrag.end() ? sd->second : 0);
                    if (eng.first > 0 && drag > 0)
                        ship.speed = eng.first / drag;
                }
                shipIndex[ship.nick] = static_cast<int>(w.ships.size());
                w.ships.push_back(ship);
            }
    std::map<std::string, int> facIndex;
    for (const auto& s : ini::Read(dataDir + "MISSIONS\\faction_prop.ini"))
        if (s.type == "factionprops")
            if (const std::string* a = s.Get("affiliation"))
            {
                const int fi = FactionIndex(w, facIndex, *a, standing);
                if (const std::string* sc = s.Get("scan_chance"))
                    w.factions[fi].scanChance = Num(*sc, 0);
                if (const std::string* lg = s.Get("legality"))
                    w.factions[fi].lawful = str::Lower(*lg) != "unlawful";
                for (const auto& v : s.All("npc_ship"))
                {
                    const auto it = shipIndex.find(str::Lower(v));
                    if (it != shipIndex.end())
                        w.factions[fi].ships.push_back(it->second);
                }
            }
    if (w.ships.empty() || w.factions.empty())
    {
        error = "npcships.ini / faction_prop.ini missing or empty";
        return false;
    }

    // solar archetype types (dock kinds, trade-lane rings)
    std::map<std::string, std::string> solarType;
    for (const auto& file : DataKeys(dataDir, "solar"))
        for (const auto& s : ini::Read(dataDir + file))
            if (s.type == "solar")
            {
                const std::string* n = s.Get("nickname");
                const std::string* t = s.Get("type");
                if (n && t)
                    solarType[str::Lower(*n)] = str::Lower(*t);
            }

    std::map<std::string, std::string> encFile;
    for (const auto& s : sys)
        if (s.type == "encounterparameters")
        {
            const std::string* n = s.Get("nickname");
            const std::string* f = s.Get("filename");
            if (n && f)
                encFile[str::Lower(*n)] = *f;
        }
    std::map<std::string, int> encIndex;
    auto encounterOf = [&](const std::string& nick) -> int
    {
        const std::string n = str::Lower(nick);
        const auto it = encIndex.find(n);
        if (it != encIndex.end())
            return it->second;
        int at = -1;
        const auto f = encFile.find(n);
        Encounter enc;
        enc.nick = n;
        if (f != encFile.end() && ReadEncounter(dataDir + f->second, enc))
        {
            at = static_cast<int>(w.encounters.size());
            w.encounters.push_back(enc);
        }
        return encIndex[n] = at;
    };

    std::map<std::string, Vec3> objPos;
    std::vector<std::pair<Vec3, std::string>> ringNext; // ring pos, next_ring / prev_ring nickname (sign in the string)
    for (const auto& s : sys)
    {
        if (s.type == "object")
        {
            const std::string* n = s.Get("nickname");
            const std::string* arch = s.Get("archetype");
            if (!n || !s.Get("pos"))
                continue;
            const Vec3 p = Pos(s.Get("pos"));
            objPos[str::Lower(*n)] = p;
            const std::string t = arch ? solarType[str::Lower(*arch)] : "";
            DockObject d;
            d.pos = p;
            d.kind = t == "docking_ring" ? ArriveDockRing : (t == "jump_gate" || t == "jump_hole") ? ArriveJumpGate
                                                         : t == "station" ? ArriveStation : 0u;
            if (d.kind)
                w.docks.push_back(d);
            if (t == "tradelane_ring")
            {
                if (const std::string* nx = s.Get("next_ring"))
                    ringNext.push_back({p, "+" + str::Lower(*nx)});
                else if (const std::string* pv = s.Get("prev_ring"))
                    ringNext.push_back({p, "-" + str::Lower(*pv)});
            }
            continue;
        }
        if (s.type != "zone")
            continue;
        Zone z;
        const std::string* nick = s.Get("nickname");
        z.nick = nick ? *nick : "";
        const std::string shape = s.Get("shape") ? str::Lower(*s.Get("shape")) : "sphere";
        z.shape = shape == "box" ? Shape::Box : shape == "cylinder" ? Shape::Cylinder : shape == "ellipsoid" ? Shape::Ellipsoid
                : shape == "ring" ? Shape::Ring : Shape::Sphere;
        z.pos = Pos(s.Get("pos"));
        const auto size = Nums(s.Get("size"));
        for (int i = 0; i < 3; ++i)
            z.size[i] = size.empty() ? 0 : size[std::min<size_t>(i, size.size() - 1)];
        if (z.shape == Shape::Cylinder && size.size() < 2)
            z.size[1] = 2 * z.size[0];
        const auto rot = Nums(s.Get("rotate"));
        RotationMatrix(rot.size() > 0 ? rot[0] : 0, rot.size() > 1 ? rot[1] : 0, rot.size() > 2 ? rot[2] : 0, z.rot);
        ZoneEncounter* cur = nullptr;
        for (const auto& kv : s.kv)
        {
            const std::string& k = kv.first;
            if (k == "encounter" || k == "density_restriction" || k == "population_additive" || k == "tradelane_attack" ||
                k == "max_battle_size" || k == "relief_time" || k == "path_label")
                z.populated = true;
            if (k == "encounter")
            {
                const auto v = ini::Fields(kv.second);
                ZoneEncounter ze;
                ze.encounter = v.empty() ? -1 : encounterOf(v[0]);
                ze.level = v.size() > 1 ? static_cast<int>(Num(v[1], 0)) : 0;
                ze.chance = v.size() > 2 ? Num(v[2], 1) : 1;
                z.encounters.push_back(ze);
                cur = &z.encounters.back();
            }
            else if (k == "faction")
            {
                if (!cur) // a faction line anywhere but right after an encounter is ignored
                    continue;
                const auto v = ini::Fields(kv.second);
                if (!v.empty())
                    cur->factions.push_back({FactionIndex(w, facIndex, v[0], standing), v.size() > 1 ? Num(v[1], 1) : 1});
            }
            else
            {
                cur = nullptr;
                if (k == "density")
                {
                    z.hasDensity = true;
                    z.density = static_cast<int>(Num(kv.second, 0));
                }
                else if (k == "repop_time")
                {
                    z.repop = Num(kv.second, 10);
                    if (z.repop <= 0)
                        z.repop = 1;
                }
                else if (k == "population_additive")
                    z.additive = Yes(str::Lower(kv.second));
                else if (k == "max_battle_size")
                    z.maxBattle = static_cast<int>(Num(kv.second, 0));
                else if (k == "relief_time")
                    z.relief = Num(kv.second, 15);
                else if (k == "path_label")
                    z.path = true;
            }
        }
        const std::string lower = str::Lower(z.nick);
        if (lower.find("tradelane") != std::string::npos && z.shape == Shape::Box)
        {
            double s3[3] = {z.size[0], z.size[1], z.size[2]};
            std::sort(s3, s3 + 3);
            z.tradelane = s3[2] >= 10 * s3[1];
        }
        w.zones.push_back(z);
    }
    for (const auto& r : ringNext)
    {
        const auto it = objPos.find(r.second.substr(1));
        if (it == objPos.end())
            continue;
        Vec3 d{it->second.x - r.first.x, it->second.y - r.first.y, it->second.z - r.first.z};
        if (r.second[0] == '-')
            d = {-d.x, -d.y, -d.z};
        const double len = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
        if (len <= 0)
            continue;
        w.rings.push_back({r.first, {d.x / len, d.y / len, d.z / len}});
    }
    return true;
}
} // namespace spawnsim
