// core\game\universe_data.cpp (the universe tables the autopilot steers by, on ini::Read since 29 Oct 2026):
// 1. the proof that HD Edition is unchanged: on every [system] of the install's universe.ini the new reader gives
//    exactly the tables of the text parsers it replaced (tests\universe_legacy.cpp, frozen) - system file, encounter
//    levels, strict zones (why + exclusions), lethal spheres (same floats, same order), planet dock rings - under the
//    default zone rules and a second set that moves both thresholds;
// 2. vanilla 1.1 (BINI inis; research\Vanilla when present): the old parsers saw nothing there, the new one sees the
//    systems' lethal spheres, strict fields and dock rings (SWEEP_2026-10-29 #5).
#include "test.h"

#include "game/flhash.h"
#include "game/ini.h"
#include "game/universe_data.h"

#include <windows.h>

namespace legacy
{
std::string SystemFile(const std::string& data, u32 system, unidata::IdFn createId);
unidata::System ParseSystem(const std::string& data, const std::string& file, unidata::IdFn createId,
                            float lethalDamage, float strictDamage);
} // namespace legacy

static u32 Id(const char* nickname) { return flhash::CreateId(nickname); }

static bool Same(const unidata::System& a, const unidata::System& b)
{
    if (a.levels != b.levels || a.dockRings != b.dockRings || a.strict.size() != b.strict.size() ||
        a.lethal.size() != b.lethal.size())
        return false;
    for (const auto& z : a.strict)
    {
        const auto o = b.strict.find(z.first);
        if (o == b.strict.end() || o->second.why != z.second.why || o->second.exclusions != z.second.exclusions)
            return false;
    }
    for (size_t i = 0; i < a.lethal.size(); ++i)
        if (memcmp(&a.lethal[i], &b.lethal[i], sizeof(unidata::LethalSphere)) != 0)
            return false;
    return true;
}

struct Totals
{
    int systems = 0, levels = 0, strict = 0, lethal = 0, rings = 0;
    void Add(const unidata::System& s)
    {
        ++systems, levels += static_cast<int>(s.levels.size()), strict += static_cast<int>(s.strict.size());
        lethal += static_cast<int>(s.lethal.size()), rings += static_cast<int>(s.dockRings.size());
    }
};

static std::vector<std::string> SystemNicks(const std::string& data)
{
    std::vector<std::string> out;
    for (const ini::Section& s : ini::Read(data + "UNIVERSE\\universe.ini"))
        if (s.type == "system")
            if (const std::string* n = s.Get("nickname"))
                out.push_back(*n);
    return out;
}

void Test_universe()
{
    if (!NeedInstall("the universe tables (HDE data)"))
        return;
    const std::string data = g_test.install + "\\DATA\\";
    const std::vector<ini::Section> universe = ini::Read(data + "UNIVERSE\\universe.ini");
    const std::vector<std::string> nicks = SystemNicks(data);
    CHECK(nicks.size() > 40);
    const float rules[2][2] = {{1000, 0}, {100, 50}}; // {lethalDamage, strictDamage}: the defaults, then both moved
    Totals t;
    int differ = 0;
    for (const std::string& n : nicks)
    {
        const u32 id = Id(n.c_str());
        const std::string file = unidata::SystemFile(universe, id, Id);
        CHECK(file == legacy::SystemFile(data, id, Id));
        if (file.empty())
            continue;
        for (const auto& r : rules)
        {
            const unidata::System now = unidata::ParseSystem(data, file, Id, r[0], r[1]);
            const bool same = Same(now, legacy::ParseSystem(data, file, Id, r[0], r[1]));
            CHECK(same);
            if (!same)
                ++differ, fprintf(stderr, "  universe: %s (%s) differs from the text parsers (rules %g / %g)\n",
                                  n.c_str(), file.c_str(), r[0], r[1]);
            if (r == rules[0])
                t.Add(now);
        }
    }
    // the tables are not empty by accident: HDE's Li01 has Planet Manhattan's core, its dock ring and minefields
    CHECK(t.lethal > 50 && t.strict > 50 && t.rings > 20 && t.levels > 100);
    if (g_test.verbose || differ)
        printf("  universe HDE: %d systems, identical to the text parsers; %d zone levels, %d strict zones, %d lethal "
               "spheres, %d dock rings\n",
               t.systems, t.levels, t.strict, t.lethal, t.rings);

    // vanilla 1.1: BINI. The old parsers read nothing there; the new reader reads the same kinds of tables.
    const std::string vanilla = g_test.install + "\\..\\..\\research\\Vanilla\\Freelancer\\DATA\\";
    if (GetFileAttributesA((vanilla + "UNIVERSE\\universe.ini").c_str()) == INVALID_FILE_ATTRIBUTES)
        return;
    const std::vector<ini::Section> vu = ini::Read(vanilla + "UNIVERSE\\universe.ini");
    Totals v, old;
    for (const std::string& n : SystemNicks(vanilla))
    {
        const u32 id = Id(n.c_str());
        const std::string file = unidata::SystemFile(vu, id, Id);
        if (file.empty())
            continue;
        v.Add(unidata::ParseSystem(vanilla, file, Id, 1000, 0));
        const std::string oldFile = legacy::SystemFile(vanilla, id, Id);
        if (!oldFile.empty())
            old.Add(legacy::ParseSystem(vanilla, oldFile, Id, 1000, 0));
    }
    CHECK(v.systems > 40 && v.lethal > 50 && v.strict > 50 && v.rings > 20);
    CHECK(old.lethal == 0 && old.strict == 0 && old.rings == 0); // what the autopilot had on vanilla until now
    if (g_test.verbose)
        printf("  universe vanilla: %d systems; %d zone levels, %d strict zones, %d lethal spheres, %d dock rings (the "
               "text parsers: %d systems found)\n",
               v.systems, v.levels, v.strict, v.lethal, v.rings, old.systems);
}
