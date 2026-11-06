// core\game\universe_data.h. The rules are those of the text parsers this replaced (space_universe.cpp before 29 Oct
// 2026), kept exactly - a later key in a section overrides an earlier one, a field's flags override a damage entry of
// the same zone - and tests\universe_test.cpp proves the tables identical to theirs on every system of the install.
#include "game/universe_data.h"

#include <cstdio>
#include <cstring>

namespace unidata
{
std::string SystemFile(const std::vector<ini::Section>& universe, u32 system, IdFn id)
{
    for (const ini::Section& s : universe)
    {
        if (s.type != "system")
            continue;
        std::string nick, file;
        for (const auto& kv : s.kv)
            if (kv.first == "nickname")
                nick = kv.second;
            else if (kv.first == "file")
                file = kv.second;
        if (!nick.empty() && !file.empty() && id(nick.c_str()) == system)
            return file;
    }
    return std::string();
}

static std::string Lower(std::string s)
{
    for (char& c : s)
        c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    return s;
}

System ParseSystem(const std::string& dataDir, const std::string& systemFile, IdFn id, float lethalDamage,
                   float strictDamage)
{
    System out;
    struct Field
    {
        std::string file, zone;
    };
    std::vector<Field> fields;
    std::unordered_map<std::string, u32> planetByBase;    // base (lower) -> planet id
    std::vector<std::pair<std::string, u32>> ringsByBase; // dock_with (lower), ring id
    for (const ini::Section& s : ini::Read(dataDir + "UNIVERSE\\" + systemFile))
    {
        std::string nick, fieldFile, fieldZone, shape, base, dockWith;
        float damage = 0, size = 0, pos[3] = {};
        bool havePos = false;
        u32 zone = 0; // the encounter levels: the nickname seen so far in this section
        for (const auto& kv : s.kv)
        {
            const std::string& k = kv.first;
            const std::string& v = kv.second;
            if (k == "nickname")
                nick = v, zone = id(v.c_str());
            else if (k == "encounter")
            {
                if (zone)
                {
                    const size_t c1 = v.find(',');
                    const int level = c1 == std::string::npos ? 0 : atoi(v.c_str() + c1 + 1);
                    int& l = out.levels[zone];
                    l = l > level ? l : level;
                }
            }
            else if (k == "damage")
                damage = static_cast<float>(atof(v.c_str()));
            else if (k == "shape")
                shape = v;
            else if (k == "size")
                size = static_cast<float>(atof(v.c_str())); // a sphere's radius (first value)
            else if (k == "pos")
                havePos = sscanf_s(v.c_str(), "%f , %f , %f", &pos[0], &pos[1], &pos[2]) == 3;
            else if (k == "file")
                fieldFile = v;
            else if (k == "zone")
                fieldZone = v;
            else if (k == "base")
                base = v;
            else if (k == "dock_with")
                dockWith = v;
        }
        if (s.type == "zone" && !nick.empty() && damage > strictDamage)
        {
            // A lethal sphere (a planet's / sun's death zone) is an avoidance obstacle with its berth, not a field to
            // fly through carefully (M04 Planet Leeds): strict stays for crossable damage.
            if (damage >= lethalDamage && _stricmp(shape.c_str(), "sphere") == 0 && havePos && size > 0)
                out.lethal.push_back({{pos[0], pos[1], pos[2]}, size});
            else
                out.strict[id(nick.c_str())].why = "damage";
        }
        if ((s.type == "asteroids" || s.type == "nebula") && !fieldFile.empty() && !fieldZone.empty())
        {
            fields.push_back({fieldFile, fieldZone});
            out.fieldKinds[id(fieldZone.c_str())] = s.type == "nebula" ? 2 : 1;
        }
        if (s.type == "object" && !nick.empty())
        {
            if (!dockWith.empty())
                ringsByBase.push_back({Lower(dockWith), id(nick.c_str())});
            else if (!base.empty())
                planetByBase[Lower(base)] = id(nick.c_str());
        }
    }
    for (const auto& r : ringsByBase)
    {
        const auto p = planetByBase.find(r.first);
        if (p != planetByBase.end())
            out.dockRings[p->second] = r.second;
    }
    for (const Field& f : fields)
    {
        std::string why;
        std::vector<u32> excl;
        for (const ini::Section& s : ini::Read(dataDir + f.file))
            for (const auto& kv : s.kv)
            {
                if (s.type == "properties" && kv.first == "flag")
                {
                    const std::string v = Lower(kv.second);
                    if (v == "mine_danger_objects" || v == "gas_danger_objects" || v == "object_density_high" ||
                        v == "damaging")
                        why += (why.empty() ? "" : ",") + v;
                }
                else if (s.type == "exclusion zones" && kv.first == "exclusion")
                    excl.push_back(id(kv.second.c_str()));
            }
        if (!why.empty())
            out.strict[id(f.zone.c_str())] = {why, excl};
    }
    return out;
}
} // namespace unidata
