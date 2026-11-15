// The encounter spawn simulator (GitHub #61): the game's ambient / patrol / trade spawning (docs\kb\spawning.md, read from
// Content.dll) run over a plain data model, Monte Carlo with a fixed seed. Host-free: no game, no files. The model is filled
// by a loader: spawnsim_ini.* from the install's ini files (offline, the validation tool tools\spawnsim\), later one that
// reads the game's loaded data in memory. Nothing in here is vanilla data: every number is either an input of the model
// (densities, repop times, weights, factions, ship lines, levels, arrival flags, longevity, lanes, docks) or an engine
// constant of Content.dll (Engine below, each with its address in spawning.md), or a model assumption for a rule not
// read yet (Assume below, each naming its open rule of spawning.md section 14).
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace spawnsim
{
struct Vec3
{
    double x = 0, y = 0, z = 0;
};

enum class Shape
{
    Sphere,
    Ellipsoid,
    Box,
    Cylinder,
    Ring
};

// One `encounter = name, level, chance` line of a zone with the `faction = group, weight` lines after it.
struct ZoneEncounter
{
    int encounter = -1; // index into World::encounters
    int level = 0;      // the diff<level> class added to every ship line (E5)
    double chance = 1;  // relative weight among the zone's encounters (E1)
    std::vector<std::pair<int, double>> factions; // World::factions index, weight
};

struct Zone
{
    std::string nick;
    Shape shape = Shape::Sphere;
    Vec3 pos;
    double size[3] = {0, 0, 0}; // as the ini gives it: sphere r; ellipsoid radii; box full extents; cylinder r, length; ring outer, inner, height
    double rot[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1}; // local -> world, R = Rz Ry Rx of `rotate` (row major)
    bool populated = false; // any population key or path_label (section 2)
    bool path = false;      // path_label
    bool tradelane = false; // nickname holds "tradelane" and a box 10x longer than wide and high: radius 0 in the L1 pick
    bool hasDensity = false;
    int density = 0;
    double repop = 10;      // repop_time (default 10, <= 0 -> 1)
    bool additive = true;   // population_additive
    int maxBattle = 0;      // max_battle_size
    double relief = 15;     // relief_time
    std::vector<ZoneEncounter> encounters;
};

enum Arrival : unsigned
{
    ArriveCruise = 1,
    ArriveBuzz = 2,
    ArriveTradelane = 4,
    ArriveDockRing = 8,
    ArriveJumpGate = 16,
    ArriveStation = 32,
    ArriveCapital = 64,
    ArriveObjectAll = ArriveDockRing | ArriveJumpGate | ArriveStation | ArriveCapital,
    ArriveAll = ArriveCruise | ArriveBuzz | ArriveTradelane | ArriveObjectAll,
};

// The formation's `behavior` = the leader's guide (Content.dll ParseGuideBehavior; spawning.md 12a, O10).
enum class Behavior
{
    Wander, // GotoVec legs of 1500-3000 m inside its zone, no cruise (engine speed)
    Path,   // patrol_path: the path's points in turn, cruise allowed (cruise speed)
    Trade,  // trade: to a dock target (lanes, cruise), docks there
    Other
};

struct ShipLine
{
    int minCount = 1, maxCount = 1;
    std::string cls; // the line's class (lower case): a [ShipClass] nickname or a raw npc_class id
    int offset = 0;  // 4th value: diff<level + offset>, floor 1
};

struct Formation
{
    std::vector<ShipLine> lines;
    unsigned arrival = ArriveAll;
    Behavior behavior = Behavior::Wander;
    double zoneCreationDistance = 1500; // zone_creation_distance (default 1500)
    bool simultaneous = true;           // allow_simultaneous_creation
    int timesToCreate = -1;             // -1 infinite
    double longevity = 24;              // longevity (default 24)
};

struct Permutation
{
    std::vector<int> formations; // indices into Encounter::formations
    bool star = false;
    double weight = 1;
    double reroll = 0; // seconds; 0 = every try
};

struct Encounter
{
    std::string nick;
    std::vector<Formation> formations;
    std::vector<Permutation> permutations; // none in the file: one of all formations, weight 1 (E3)
};

struct NpcShip
{
    std::string nick;
    std::vector<std::string> classes; // npc_class, lower case
    double speed = 0; // engine top speed: engine max_force / (engine linear_drag + ship linear_drag) of its loadout; 0 unknown
};

struct Faction
{
    std::string nick;      // lower case group nickname
    double standing = 0;   // the player's reputation with it (G1, G2: <= -0.6 hostile)
    std::vector<int> ships; // faction_prop npc_ship -> World::ships
    double scanChance = 0;  // faction_prop scan_chance (patrol cargo scans, 12a F2b); 0 = never scans
    bool lawful = true;     // faction_prop legality (a player is scanned up to attitude 0.2 by unlawful, 1.0 by lawful ones)
};

struct DockObject
{
    Vec3 pos;
    unsigned kind = 0; // ArriveDockRing / ArriveJumpGate / ArriveStation / ArriveCapital
};

struct LaneRing
{
    Vec3 pos;
    Vec3 dir; // unit, towards the next ring (or from the previous one at a lane's end)
};

struct World
{
    std::vector<Zone> zones; // system-file order (O7)
    std::vector<Encounter> encounters;
    std::vector<Faction> factions;
    std::vector<NpcShip> ships;
    std::map<std::string, std::vector<std::string>> shipClasses; // [ShipClass] nickname -> members, lower case
    std::vector<DockObject> docks;
    std::vector<LaneRing> rings;
    double cruiseSpeed = 0; // constants.ini [EngineEquipConsts] CRUISING_SPEED; 0 = not set (Engine::cruiseSpeed, Common.dll's default)
};

// Engine constants of Content.dll (spawning.md, addresses there). A mod changes these only by patching Content.dll.
struct Engine
{
    double heartbeat = 3.0, lifeTick = 1.0;
    double atShip = 10, nearReach = 2000, ahead = 4000; // L1 / L2 / L3 reach (not `near`: a macro of windef.h in the game's build)
    double frustumH = 1.2217, frustumV = 0.9671;     // L3 view frustum, full angles (rad)
    double nearRaise = 0.6, aheadShare = 0.33;       // D1
    double countRadius = 3750, aheadPoint = 5000;    // D2
    double spawnRing = 2500, spawnRingWidth = 50;    // section 8
    double heightBand = 250, buzzHeight = 150;
    double pathStep = 200;
    double laneReach = 5000, dockReach = 2700;
    double laneSpeed = 2500;                         // Freelancer.exe's lane speed (docs\FREELANCER_RE.md "lane speed 2500 m/s")
    double hostile = -0.6, combatRadius = 2500;      // G1 / G2
    double refillRadius = 2500, seenRange = 4000, seenHalfAngle = 0.6109; // X1 (70 degrees full)
    double maxRate = 15, rateStep = 100;
    // flight after spawn (spawning.md 12a, O10 pinned): the guides' own numbers
    double cruiseSpeed = 300;                          // Common.dll EngineEquipConsts::CRUISING_SPEED default (World overrides)
    double wanderMin = 1500, wanderMax = 3000;         // FWanderGuide leg ranges (06fb8114 / 06fb8118)
    double wanderArrive = 500, wanderFloor = 100;      // its GotoVec range; ranges halve while MinRange > 100
    double wanderZoneStep = 1000;                      // no point found: 1000 m towards the zone (06fb856c)
    double tradeReach = 20000;                         // trade leader's dock target within 20 km (SpawnPlacement 0x469c4000)
    double dockArrive = 500;                           // a trade leader this close to its target docks (leaves the census)
    double scanReach = 2500, scanRange = 500;          // FCargoScanRoute: targets within 2500 m, trails at ScanRange 500
    double scanMaxUnlawful = 0.2, scanMaxLawful = 1.0; // the player's attitude ceiling for a scan
};

// Model assumptions for rules spawning.md has not pinned down; each names its open rule (section 14). A speed < 0 = from
// the data (O10 pinned 15 Nov 2026: engine speed of the formation's ships, cruise speed for path / trade leaders).
struct Assume
{
    double hostileSpeed = -1;   // hostiles fly at the player and hold at holdRange (god-mode census: they stay)
    double holdRange = 1000;
    double pathSpeed = -1;      // path / trade leaders (data: the cruise speed)
    double wanderSpeed = -1;    // wanderers (data: the engine speed)
    double scanHold = 60;       // O10b: seconds a scanning patrol trails the player from its roll (the scan's comm chain is not
                                // read; New York census C4 measured 45-85 s near the player)
    int placementTries = 16;    // O4: points tried on the 2500 m sphere before a placement fails
    double laneAccept = 1;      // O4 / T2: share of lane arrivals passing the refusals not modelled (a ship that way within
                                // 15 km, a lane end in the player's view)
};

struct Query
{
    Vec3 player;
    double seconds = 150;     // dwell
    double sampleFrom = 30;   // time-mean counts from here (the census skips the first 30 s)
    double sampleEvery = 3;   // the census samples every 3 s
    double disc = 5000;       // the census's cover radius
    double cell = 2500;       // first-sighting cells (x, z)
    int runs = 400;
    uint32_t seed = 61;
    int facings = 8;          // the player's facing is unknown: runs cycle through this many yaws (L3, X1 "seen")
};

struct Result
{
    // per World::factions index, means over runs
    std::vector<double> firstSeen;    // ships first seen inside the disc (the census's mix)
    std::vector<double> meanInDisc;   // time-mean ships in the disc that were first seen inside it (the census's count)
    std::map<std::pair<int, int>, std::vector<double>> cells; // (floor(x/cell), floor(z/cell)) -> first sightings per faction
    std::vector<double> zoneSpawns;   // formations spawned per run, per World::zones index
    double ticksBlocked = 0;          // heartbeats with a density-0 zone at the ship, per run
    double ticksCombatRefused = 0;    // tries refused by the combat gate (G2), per run
};

// The fixed parts of one zone encounter: per formation, the factions allowed to fly it (E4, E5) with their weights.
struct Prepared
{
    std::vector<std::vector<std::vector<std::vector<std::pair<int, double>>>>> allowed; // [zone][encounter][formation]
    // the same shape: mean engine speed of the faction's ships fitting the formation's leader line (0 = none known)
    std::vector<std::vector<std::vector<std::vector<double>>>> speed;
};
Prepared Prepare(const World& w);

// A formation in flight after it spawned (spawning.md 12a): its behaviour, speed and current destination.
struct Flight
{
    Behavior behavior = Behavior::Other;
    Vec3 vel;              // straight flight (path axis, lane, no destination)
    double speed = 0;      // m/s towards `target` when `hasTarget`
    Vec3 target;
    bool hasTarget = false;
    bool docks = false;    // trade: reaching `target` = docked (gone)
    double minRange = 0, maxRange = 0; // wander leg ranges (FWanderGuide +0x30 / +0x34)
    double engine = 0;     // engine top speed (closing in to scan)
    double scanChance = 0; // patrol_path: the faction's scan_chance when the player qualifies (12a F2b), else 0
    bool scanRolled = false;
    double scanLeft = 0;   // seconds of trailing the player left
};
// Picks the wanderer's next destination like FWanderGuide::Start: 16 ring directions (22.5 degrees apart) at a random
// range in [min, max] each, tried from a random start, the first inside the zone; none: ranges halve while min > 100,
// then a point up to 1000 m towards the zone. `seed`: the game-style rand() state, advanced.
Vec3 WanderTarget(const Zone& z, const Engine& e, const Vec3& from, Flight& f, uint32_t& seed);
// `dt` seconds of flight for a non-hostile formation; false when it docked (leaves the system's census).
bool FlyStep(const Zone& z, const Engine& e, Vec3& pos, Flight& f, double dt, uint32_t& seed);

// True when the NPC ship fits the line at `level` (E5).
bool ShipFits(const World& w, const NpcShip& s, const ShipLine& line, int level);

// Inside the zone grown by `margin` metres (the grown-axes approximation of the engine's distance test).
bool InZone(const Zone& z, const Vec3& p, double margin);
// The zone's radius for the smallest-zone pick (L1): a trade-lane zone 0, else its bounding radius.
double PickRadius(const Zone& z);
// R = Rz Ry Rx of `rotate = x, y, z` in degrees, local -> world.
void RotationMatrix(double rx, double ry, double rz, double out[9]);

// The candidate lists at the player for one facing (yaw, rad): L1 (paths + the smallest zone, random insert done per tick),
// L2, L3; `blocked` when a zone at the ship has density 0.
struct Lists
{
    std::vector<int> paths, l2, l3;
    int smallest = -1;
    bool blocked = false;
    double limit = 0; // D1 over L1 and L2
};
Lists CollectLists(const World& w, const Engine& e, const Vec3& p, double yaw);

Result Simulate(const World& w, const Query& q, const Engine& e = Engine(), const Assume& a = Assume());
// The same with the fixed parts prepared once (Prepare(w)), for many queries over one world (core\game\influence.h).
Result Simulate(const World& w, const Prepared& prep, const Query& q, const Engine& e = Engine(), const Assume& a = Assume());
} // namespace spawnsim
