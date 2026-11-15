// core\game\influence.*: HUD 2 map's influence cells (issue 61) on hand-made worlds: the relation of the strongest, the zones a cell lies in, a thin lane
// that misses every cell centre still read, cells outside every zone's reach empty, the look's scales and the contested alternation.
#include "test.h"

#include "game/flhash.h"
#include "game/influence.h"

#include <algorithm>

using namespace spawnsim;

namespace
{
// A sphere zone of radius r at (x, 0, z) whose one encounter (one fighter, cruise arrival) is flown by faction `faction`.
Zone Sphere(const char* nick, double x, double z, double r, int faction, int density)
{
    Zone s;
    s.nick = nick;
    s.pos = {x, 0, z};
    s.size[0] = s.size[1] = s.size[2] = r;
    s.populated = s.hasDensity = true;
    s.density = density;
    s.repop = 5;
    ZoneEncounter ze;
    ze.encounter = 0;
    ze.level = 1;
    ze.factions = {{faction, 1}};
    s.encounters.push_back(ze);
    return s;
}

World Base()
{
    World w;
    w.shipClasses["sc_f"] = {"cf"};
    w.shipClasses["diff1"] = {"d1"};
    w.ships.push_back({"ship", {"cf", "d1"}});
    Faction friendly{"friend", 0.9, {0}}, hostile{"foe", -0.9, {0}};
    w.factions = {friendly, hostile};
    Encounter e;
    e.nick = "enc";
    Formation f;
    f.lines.push_back({1, 1, "sc_f", 0});
    f.arrival = ArriveCruise;
    e.formations.push_back(f);
    w.encounters.push_back(e);
    return w;
}
} // namespace

void Test_influence()
{
    using namespace influence;
    CHECK(RelationOf(-0.6) == kHostile && RelationOf(0.6) == kFriendly && RelationOf(0.59) == kNeutral);
    CHECK(StrengthT(0) == 0 && StrengthT(kFullShips) == 1 && StrengthT(100) == 1 && StrengthT(1) > 0.2f && StrengthT(1) < 0.4f);
    CHECK(CellScale(0) < CellScale(1) && CellAlpha(0) < CellAlpha(1));
    {
        Cell c;
        c.lead = kFriendly, c.second = kHostile;
        c.col = 2, c.row = 4;
        CHECK(CellRelation(c) == kFriendly);
        c.col = 3;
        CHECK(CellRelation(c) == kHostile);
        c.second = -1;
        CHECK(CellRelation(c) == kFriendly);
    }
    // two zones 60 km apart: cells near each take its faction's relation and its id; far from both, no cell
    {
        World w = Base();
        w.zones.push_back(Sphere("west", -30000, 0, 8000, 0, 6));
        w.zones.push_back(Sphere("east", 30000, 0, 8000, 1, 6));
        Options o;
        o.spacing = 2000, o.runs = 4, o.seconds = 60;
        const Field f = Compute(w, o);
        CHECK(!f.cells.empty() && f.signatures > 0 && f.spacing == 2000);
        int west = 0, east = 0, wrong = 0, outside = 0;
        for (const Cell& c : f.cells)
        {
            if (c.x < -24000 && c.x > -36000 && std::abs(c.z) < 6000)
                c.lead == kFriendly && c.nZones == 1 && c.zones[0] == flhash::CreateId("west") ? ++west : ++wrong;
            if (c.x > 24000 && c.x < 36000 && std::abs(c.z) < 6000)
                c.lead == kHostile && c.nZones == 1 && c.zones[0] == flhash::CreateId("east") ? ++east : ++wrong;
            if (std::abs(c.x) < 14000) // beyond 8 km + the 4 km reach of both
                ++outside;
        }
        CHECK(west > 10 && east > 10 && wrong == 0 && outside == 0);
        // the relations flip with the standings
        w.factions[0].standing = -0.9;
        CHECK(Relations(w) == std::vector<int>({kHostile, kHostile}));
    }
    // a 200 m wide lane box of the friendly faction across a hostile background on a 3 km grid: the lane's cells read friendly although no centre is in it
    {
        World w = Base();
        Zone bg = Sphere("background", 0, 0, 30000, 1, 2);
        Zone lane = Sphere("zone_tradelane_test", 0, 1300, 0, 0, 12);
        lane.shape = Shape::Box;
        lane.size[0] = 40000, lane.size[1] = 200, lane.size[2] = 200; // along x, at z = 1300: between the rows at z = 0 and 2598
        lane.tradelane = true;
        lane.repop = 1;
        w.zones = {bg, lane};
        Options o;
        o.spacing = 3000, o.runs = 6, o.seconds = 60;
        const Field f = Compute(w, o);
        int onLane = 0, friendly = 0;
        for (const Cell& c : f.cells)
            if (std::abs(c.x) < 15000 && std::abs(c.z - 1300) < 1800)
                ++onLane, friendly += c.lead == kFriendly || c.second == kFriendly;
        CHECK(onLane > 4 && friendly * 2 >= onLane);
    }
}
