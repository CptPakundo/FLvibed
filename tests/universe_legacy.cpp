// The hand-written text parsers of core\game\space_universe.cpp as they were at 6b15f2a (29 Oct 2026), frozen here as
// the reference that tests\universe_test.cpp holds core\game\universe_data.cpp to: identical tables on every system of
// the HD Edition install. Only their inputs changed (a DATA folder and an id function instead of the game's DataPath
// and createId); every parsing line is the original. Do not "fix" this file: it is the before of a proof.
#include "game/universe_data.h"
#include "str.h"

#include <fstream>
#include <map>

namespace legacy
{
static std::string Trim(std::string s) { return str::Trim(str::StripComment(s)); }

std::string SystemFile(const std::string& data, u32 system, unidata::IdFn createId)
{
    std::string file;
    std::ifstream in(data + "UNIVERSE\\universe.ini");
    std::string line, nick, f;
    bool sys = false;
    auto done = [&]()
    {
        if (sys && !nick.empty() && !f.empty() && createId(nick.c_str()) == system)
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
    return file;
}

unidata::System ParseSystem(const std::string& data, const std::string& file, unidata::IdFn createId,
                            float lethalDamage, float strictDamage)
{
    unidata::System out;
    { // ZoneLevels
        std::ifstream in(data + "UNIVERSE\\" + file);
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
                zone = createId(v.c_str());
            else if (zone && _stricmp(k.c_str(), "encounter") == 0)
            {
                const size_t c1 = v.find(',');
                const int level = c1 == std::string::npos ? 0 : atoi(v.c_str() + c1 + 1);
                int& l = out.levels[zone];
                l = std::max(l, level);
            }
        }
    }
    { // StrictZones (+ the lethal spheres)
        std::unordered_map<u32, unidata::StrictZone>& zones = out.strict;
        std::vector<unidata::LethalSphere>& lethal = out.lethal;
        struct Field
        {
            std::string file, zone;
        };
        std::vector<Field> fields;
        {
            std::ifstream in(data + "UNIVERSE\\" + file);
            std::string line, section, nick, fieldFile, fieldZone, shape;
            float damage = 0, size = 0, pos[3] = {};
            bool havePos = false;
            auto flush = [&]()
            {
                if (section == "[zone]" && !nick.empty() && damage > strictDamage)
                {
                    const bool lethalSphere =
                        damage >= lethalDamage && _stricmp(shape.c_str(), "sphere") == 0 && havePos && size > 0;
                    if (lethalSphere)
                        lethal.push_back({{pos[0], pos[1], pos[2]}, size});
                    else
                        zones[createId(nick.c_str())].why = "damage";
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
                    size = static_cast<float>(atof(v.c_str()));
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
            std::ifstream in(data + f.file);
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
                    excl.push_back(createId(v.c_str()));
            }
            if (!why.empty())
                zones[createId(f.zone.c_str())] = {why, excl};
        }
    }
    { // PlanetDockRing
        std::ifstream in(data + "UNIVERSE\\" + file);
        std::string line, section, nick, base, dockWith;
        std::unordered_map<std::string, u32> planetByBase;
        std::vector<std::pair<std::string, u32>> ringsByBase;
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
                    ringsByBase.push_back({lower(dockWith), createId(nick.c_str())});
                else if (!base.empty())
                    planetByBase[lower(base)] = createId(nick.c_str());
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
                out.dockRings[p->second] = r.second;
        }
    }
    return out;
}
} // namespace legacy
