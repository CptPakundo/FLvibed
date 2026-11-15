// core\game\spawnsim.*: the spawn simulator's rules on hand-made worlds with known answers (docs\kb\spawning.md: the roll
// D3, the density cap D1/D2, the life counter X1/X2, the faction draw E4/E5, the density-0 block L1), and the ini loader on
// the test install's New York.
#include "test.h"

#include "game/spawnsim.h"
#include "game/spawnsim_ini.h"

#include <algorithm>
#include <cmath>

using namespace spawnsim;

namespace
{
// One big sphere zone around the origin, one encounter of one formation of `ships` ships (sc_fighters), factions by weight.
World OneZone(int density, double repop, int ships, unsigned arrival)
{
    World w;
    w.shipClasses["sc_fighters"] = {"class_fighter"};
    w.shipClasses["diff4"] = {"d4"};
    w.shipClasses["diff5"] = {"d5"};
    w.ships.push_back({"fits_d4", {"class_fighter", "d4"}});
    w.ships.push_back({"fits_d5", {"class_fighter", "d5"}});
    Faction a{"a", 0, {0}}, b{"b", 0, {0, 1}}, c{"c", 0, {1}};
    w.factions = {a, b, c};
    Encounter e;
    e.nick = "enc";
    Formation f;
    f.lines.push_back({ships, ships, "sc_fighters", -1}); // level 5 - 1 -> diff4
    f.arrival = arrival;
    f.zoneCreationDistance = 0;
    e.formations.push_back(f);
    w.encounters.push_back(e);
    Zone z;
    z.nick = "big";
    z.size[0] = z.size[1] = z.size[2] = 50000;
    z.populated = true;
    z.hasDensity = true;
    z.density = density;
    z.repop = repop;
    ZoneEncounter ze;
    ze.encounter = 0;
    ze.level = 5;
    ze.factions = {{0, 1}, {1, 3}, {2, 5}}; // c has no d4 ship: never flies it (E4)
    z.encounters.push_back(ze);
    w.zones.push_back(z);
    return w;
}
} // namespace

void Test_spawnsim()
{
    // D3: one heartbeat with nothing near: p = density / repop_time
    {
        const World w = OneZone(3, 90, 1, ArriveBuzz);
        Query q;
        q.seconds = 3;
        q.runs = 40000;
        const Result r = Simulate(w, q);
        CHECK_NEAR(r.firstSeen[0] + r.firstSeen[1] + r.firstSeen[2], 3.0 / 90, 0.004);
        // E4 / E5: weights 1 : 3 over the factions with a fitting ship
        CHECK(r.firstSeen[2] == 0);
        CHECK_NEAR(r.firstSeen[1] / (r.firstSeen[0] + r.firstSeen[1]), 0.75, 0.05);
    }
    // D1 / D2: repop 1 (p = 1), a 3-ship formation fills density 3: one formation in 30 s, nothing more
    {
        const World w = OneZone(3, 1, 3, ArriveCruise);
        Query q;
        q.seconds = 30;
        q.runs = 200;
        const Result r = Simulate(w, q);
        CHECK_NEAR(r.firstSeen[0] + r.firstSeen[1], 3.0, 1e-9);
        CHECK_NEAR(r.zoneSpawns[0], 1.0, 1e-9);
    }
    // X1 / X2: a still ship spawned 2500-2550 m out loses about 1.25 a second from 48 and dies near 39 s unless the
    // player sees it (70 degrees of 360 of the ring): in 60 s about 0.19 x 1 + 0.81 x 2 = 1.81 spawns per run
    {
        const World w = OneZone(1, 1, 1, ArriveCruise);
        Query q;
        q.seconds = 60;
        q.runs = 4000;
        q.facings = 1;
        const Result r = Simulate(w, q);
        CHECK_NEAR(r.zoneSpawns[0], 1.81, 0.06);
    }
    // L1: a density-0 zone at the ship blocks every spawn (it is the smallest zone there)
    {
        World w = OneZone(3, 1, 1, ArriveBuzz);
        Zone z0;
        z0.nick = "quiet";
        z0.size[0] = 1000;
        z0.populated = true;
        z0.hasDensity = true;
        z0.density = 0;
        w.zones.push_back(z0);
        Query q;
        q.seconds = 30;
        q.runs = 50;
        const Result r = Simulate(w, q);
        CHECK(r.zoneSpawns[0] == 0);
        CHECK(r.ticksBlocked >= 9);
    }
    // L2: a near zone raises the limit to its density when the limit is below 0.6 x it
    {
        World w = OneZone(3, 90, 1, ArriveBuzz);
        Zone near = w.zones[0];
        near.nick = "near";
        near.pos = {0, 0, 51000}; // its surface 1000 m from the player
        near.density = 9;
        w.zones.push_back(near);
        const Lists l = CollectLists(w, Engine(), Vec3{}, 0);
        CHECK(l.smallest == 0);
        CHECK(l.l2.size() == 1 && l.l2[0] == 1);
        CHECK_NEAR(l.limit, 9, 1e-9);
    }
    // R = Rz Ry Rx: a cylinder turned 90 degrees about x lies along z
    {
        Zone c;
        c.shape = Shape::Cylinder;
        c.size[0] = 100;
        c.size[1] = 10000;
        RotationMatrix(90, 0, 0, c.rot);
        CHECK(InZone(c, Vec3{0, 0, 4000}, 0));
        CHECK(!InZone(c, Vec3{0, 4000, 0}, 0));
    }

    // 12a wander: a leg of 1500-3000 m in one of 16 directions 22.5 degrees apart, inside the zone
    {
        Zone big;
        big.size[0] = 50000;
        const Engine e;
        uint32_t seed = 7;
        for (int i = 0; i < 50; ++i)
        {
            Flight f;
            f.minRange = e.wanderMin;
            f.maxRange = e.wanderMax;
            const Vec3 t = WanderTarget(big, e, Vec3{100, 0, 200}, f, seed);
            const double dx = t.x - 100, dz = t.z - 200, r = std::sqrt(dx * dx + dz * dz);
            CHECK(r >= 1500 - 1e-6 && r <= 3000 + 1e-6);
            const double k = std::atan2(dz, dx) / (3.14159265358979323846 / 8);
            CHECK_NEAR(k, std::round(k), 1e-6);
        }
        // a zone smaller than the leg: the ranges halve until a point fits, then reset to 1500 / 3000
        Zone small;
        small.size[0] = 1000;
        Flight f;
        f.minRange = e.wanderMin;
        f.maxRange = e.wanderMax;
        const Vec3 t = WanderTarget(small, e, Vec3{}, f, seed);
        CHECK(InZone(small, t, 0) && std::sqrt(t.x * t.x + t.z * t.z) >= 375 - 1e-6);
        CHECK(f.minRange == 1500 && f.maxRange == 3000);
        // outside its zone: 1000 m towards it
        const Vec3 back = WanderTarget(small, e, Vec3{20000, 0, 0}, f, seed);
        CHECK_NEAR(back.x, 19000, 1e-6);
    }
    // 12a flight: a trade leader docks when it reaches its target; a wanderer takes its next leg
    {
        Zone big;
        big.size[0] = 50000;
        const Engine e;
        uint32_t seed = 3;
        Flight trade;
        trade.behavior = Behavior::Trade;
        trade.target = {3000, 0, 0};
        trade.hasTarget = trade.docks = true;
        trade.speed = 300;
        Vec3 pos{};
        int steps = 0;
        while (FlyStep(big, e, pos, trade, 1, seed) && steps < 100)
            ++steps;
        CHECK(steps == 8); // 2500 m to the 500 m dock range at 300 m/s: docked in the 9th second
        Flight wander;
        wander.behavior = Behavior::Wander;
        wander.minRange = e.wanderMin;
        wander.maxRange = e.wanderMax;
        wander.target = {600, 0, 0};
        wander.hasTarget = true;
        wander.speed = 100;
        Vec3 wp{};
        CHECK(FlyStep(big, e, wp, wander, 1, seed));
        CHECK_NEAR(wp.x, 100, 1e-9);
        CHECK(wander.target.x != 600 || wander.target.z != 0);
    }
    // 12a in the run: path leaders at the cruise speed leave sooner than still ones, so the zone spawns more often
    {
        World w = OneZone(1, 1, 1, ArriveCruise);
        Zone& z = w.zones[0];
        z.shape = Shape::Cylinder;
        z.size[0] = 750;
        z.size[1] = 60000;
        z.path = true;
        w.encounters[0].formations[0].behavior = Behavior::Path;
        Query q;
        q.seconds = 60;
        q.runs = 400;
        q.facings = 1;
        const Result fast = Simulate(w, q);
        Assume still;
        still.pathSpeed = 0;
        const Result slow = Simulate(w, q, Engine(), still);
        CHECK(fast.zoneSpawns[0] > 1.5 * slow.zoneSpawns[0]);
        // F2b: a faction that always scans keeps its patrols on the player for scanHold, so the zone spawns less often
        for (Faction& f : w.factions)
            f.scanChance = 1;
        const Result scan = Simulate(w, q);
        CHECK(scan.zoneSpawns[0] < 0.8 * fast.zoneSpawns[0]);
        if (g_test.verbose)
            printf("  scanning patrols per 60 s: %.2f\n", scan.zoneSpawns[0]);
        if (g_test.verbose)
            printf("  path leaders per 60 s: cruise %.2f, still %.2f\n", fast.zoneSpawns[0], slow.zoneSpawns[0]);
    }

    // the loader on the install's New York
    if (!NeedInstall("spawnsim New York"))
        return;
    World li01;
    std::string err;
    CHECK(LoadSystem(g_test.install + "\\DATA\\", "li01", {}, li01, err));
    int populated = 0, paths = 0, lanes = 0;
    for (const Zone& z : li01.zones)
    {
        populated += z.populated;
        paths += z.path;
        lanes += z.tradelane;
    }
    CHECK(populated > 50 && paths > 5 && lanes > 5);
    CHECK(li01.encounters.size() > 5 && !li01.rings.empty() && !li01.docks.empty());
    const Prepared prep = Prepare(li01);
    int flyable = 0;
    for (const auto& z : prep.allowed)
        for (const auto& e : z)
            for (const auto& f : e)
                flyable += !f.empty();
    CHECK(flyable > 20);
    int withSpeed = 0;
    double maxSpeed = 0;
    for (const NpcShip& s : li01.ships)
    {
        withSpeed += s.speed > 0;
        maxSpeed = std::max(maxSpeed, s.speed);
    }
    CHECK(withSpeed * 2 > static_cast<int>(li01.ships.size())); // engine speeds from the loadouts (12a)
    CHECK(maxSpeed > 40 && maxSpeed < 400);
    if (g_test.verbose)
        printf("  li01: %d of %zu npc ships with an engine speed (max %.0f m/s), cruise speed %.0f (0 = the default)\n", withSpeed,
               li01.ships.size(), maxSpeed, li01.cruiseSpeed);
    if (g_test.verbose)
        printf("  li01: %zu zones (%d populated, %d paths, %d lanes), %zu encounters, %zu rings, %zu docks, %d flyable formations\n",
               li01.zones.size(), populated, paths, lanes, li01.encounters.size(), li01.rings.size(), li01.docks.size(), flyable);
}
