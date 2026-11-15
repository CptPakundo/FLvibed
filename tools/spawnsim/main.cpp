// flspawnsim.exe (build.cmd spawnsim): the offline spawn simulator (GitHub #61, core\game\spawnsim.*) over an install's data.
//   flspawnsim <DATA dir> <points.tsv> [<save.fl>] [runs] [seed] [assumptions]
// assumptions: "hostileSpeed=0,pathSpeed=50,wanderSpeed=0,holdRange=1000,placementTries=16" overrides spawnsim::Assume
// (sensitivity runs; a speed < 0 = from the data, the default).
// points.tsv: one point per line "system <tab> name <tab> x <tab> y <tab> z <tab> seconds" (a player held there that long,
// arrived by teleport: no arrival fill). Writes TSV to stdout:
//   pt    system point faction firstSeen meanInDisc   (means over runs; see spawnsim::Result)
//   cell  system point cx cz faction firstSeen
//   diag  system point limit blocked L1 L2 L3 spawns-per-zone
// The player's standings come from the save's `house` lines (none: every faction neutral).
//   flspawnsim field <DATA dir> <system> [<save.fl>] [runs]: HUD 2 map's influence cells (core\game\influence.h) for one
//   system; writes "field spacing cells signatures ms" then "c x z strengthF strengthN strengthH lead second nZones" per cell.
#include "game/influence.h"
#include "game/spawnsim_ini.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

static int Field(int argc, char** argv)
{
    std::string data = argv[2];
    if (!data.empty() && data.back() != '\\' && data.back() != '/')
        data += '\\';
    const std::map<std::string, double> standing = argc > 4 ? spawnsim::ReadStandings(argv[4]) : std::map<std::string, double>();
    spawnsim::World w;
    std::string err;
    const auto t0 = std::chrono::steady_clock::now();
    if (!spawnsim::LoadSystem(data, argv[3], standing, w, err))
    {
        fprintf(stderr, "%s\n", err.c_str());
        return 1;
    }
    const auto t1 = std::chrono::steady_clock::now();
    influence::Options o;
    if (argc > 5)
        o.runs = atoi(argv[5]);
    const influence::Field f = influence::Compute(w, o);
    const auto t2 = std::chrono::steady_clock::now();
    printf("field\t%.0f\t%zu\t%d\tload_ms %lld\tcompute_ms %lld\n", f.spacing, f.cells.size(), f.signatures,
           static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count()),
           static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count()));
    for (const auto& c : f.cells)
        printf("c\t%.0f\t%.0f\t%.3f\t%.3f\t%.3f\t%d\t%d\t%d\n", c.x, c.z, c.strength[0], c.strength[1], c.strength[2], c.lead, c.second, c.nZones);
    return 0;
}

int main(int argc, char** argv)
{
    if (argc >= 4 && strcmp(argv[1], "field") == 0)
        return Field(argc, argv);
    if (argc < 3)
    {
        fprintf(stderr, "usage: flspawnsim <DATA dir> <points.tsv> [<save.fl>] [runs] [seed] [assumptions]\n");
        return 2;
    }
    std::string data = argv[1];
    if (!data.empty() && data.back() != '\\' && data.back() != '/')
        data += '\\';
    const std::map<std::string, double> standing = argc > 3 ? spawnsim::ReadStandings(argv[3]) : std::map<std::string, double>();
    spawnsim::Query base;
    if (argc > 4)
        base.runs = atoi(argv[4]);
    if (argc > 5)
        base.seed = static_cast<uint32_t>(strtoul(argv[5], nullptr, 10));
    spawnsim::Assume assume;
    if (argc > 6)
    {
        std::istringstream as(argv[6]);
        std::string kv;
        while (std::getline(as, kv, ','))
        {
            const size_t eq = kv.find('=');
            const std::string k = kv.substr(0, eq);
            const double v = eq == std::string::npos ? 0 : atof(kv.c_str() + eq + 1);
            if (k == "hostileSpeed")
                assume.hostileSpeed = v;
            else if (k == "pathSpeed")
                assume.pathSpeed = v;
            else if (k == "wanderSpeed")
                assume.wanderSpeed = v;
            else if (k == "holdRange")
                assume.holdRange = v;
            else if (k == "laneAccept")
                assume.laneAccept = v;
            else if (k == "placementTries")
                assume.placementTries = static_cast<int>(v);
            else
            {
                fprintf(stderr, "unknown assumption %s\n", k.c_str());
                return 2;
            }
        }
    }

    std::map<std::string, spawnsim::World> worlds;
    std::ifstream in(argv[2]);
    std::string line;
    while (std::getline(in, line))
    {
        std::istringstream ss(line);
        std::string sys, name;
        spawnsim::Query q = base;
        if (!(ss >> sys >> name >> q.player.x >> q.player.y >> q.player.z >> q.seconds))
            continue;
        if (!worlds.count(sys))
        {
            std::string err;
            if (!spawnsim::LoadSystem(data, sys, standing, worlds[sys], err))
            {
                fprintf(stderr, "%s\n", err.c_str());
                return 1;
            }
        }
        const spawnsim::World& w = worlds[sys];
        const spawnsim::Result r = spawnsim::Simulate(w, q, spawnsim::Engine(), assume);
        for (size_t f = 0; f < w.factions.size(); ++f)
            if (r.firstSeen[f] > 0 || r.meanInDisc[f] > 0)
                printf("pt\t%s\t%s\t%s\t%.4f\t%.4f\n", sys.c_str(), name.c_str(), w.factions[f].nick.c_str(), r.firstSeen[f], r.meanInDisc[f]);
        for (const auto& c : r.cells)
            for (size_t f = 0; f < w.factions.size(); ++f)
                if (c.second[f] > 0)
                    printf("cell\t%s\t%s\t%d\t%d\t%s\t%.4f\n", sys.c_str(), name.c_str(), c.first.first, c.first.second,
                           w.factions[f].nick.c_str(), c.second[f]);
        const spawnsim::Lists l = spawnsim::CollectLists(w, spawnsim::Engine(), q.player, 0);
        std::string l1, l2, l3, sp;
        for (int i : l.paths)
            l1 += w.zones[i].nick + " ";
        if (l.smallest >= 0)
            l1 += w.zones[l.smallest].nick;
        for (int i : l.l2)
            l2 += w.zones[i].nick + " ";
        for (int i : l.l3)
            l3 += w.zones[i].nick + " ";
        for (size_t i = 0; i < w.zones.size(); ++i)
            if (r.zoneSpawns[i] > 0.05)
            {
                char b[96];
                snprintf(b, sizeof b, "%s:%.2f ", w.zones[i].nick.c_str(), r.zoneSpawns[i]);
                sp += b;
            }
        printf("diag\t%s\t%s\t%.1f\t%d\t%s\t%s\t%s\t%s\tcombatRefused:%.1f\n", sys.c_str(), name.c_str(), l.limit, l.blocked ? 1 : 0,
               l1.c_str(), l2.c_str(), l3.c_str(), sp.c_str(), r.ticksCombatRefused);
        fflush(stdout);
    }
    return 0;
}
