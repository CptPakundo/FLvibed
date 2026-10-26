// Space objects around the player and target selection, natively: the client's own object registry (Common.dll
// CObject::FindFirst/FindNext) instead of the HUD contact list, whose content depends on the filter the player picked,
// and the game's own target command (0x54BCB0, what a contact list click sends). Facts: docs/FREELANCER_RE.md
// "Targets and contacts". Shared by every module (core/game), compiled into each.
#pragma once
#include "common.h"

#include <string>
#include <vector>

namespace space
{
// Archetype type bits (CSimple::get_type, FLSDK ObjectType).
enum Type : u32
{
    kPlanet = 1 << 1,
    kDockingRing = 1 << 5,
    kJumpGate = 1 << 6,
    kTradelaneRing = 1 << 7,
    kStation = 1 << 8,
    kAirlockGate = 1 << 10, // AIRLOCK_GATE (the Dyson sphere door, the Nomad lair / core): FLHookSDK ObjectType
    kJumpHole = 1 << 11,
    kLoot = 1 << 28,
};

struct Contact
{
    u32 id = 0;      // space object id (pub::SpaceObj, IObjInspect get_id; solars: CreateID of the nickname)
    u32 type = 0;    // Type bits
    u32 nameIds = 0; // string id of the object's name (CEqObj::get_name), 0 if none
    u32 base = 0;    // the base a docking with it leads to (CEqObj::get_dock_target), 0 if none
    bool ship = false;
    bool dock = false;       // CEqObj::is_dock: the player can request docking with it (bases, rings, gates, lanes)
    bool targetable = false; // CEqObj::is_targetable
    float pos[3] = {};
    float distance = 0; // to the player's ship
    float health = -1;  // hull, 0..1 (CSimple::get_relative_health), -1 unknown
    float attitude = 0; // ships: the ship's attitude towards the player (-1 hostile .. 1 friendly; 0 unknown)
    // ships: both reputations were known (the ship's GetRep and the player's), so the game was asked for `attitude`
    // (pub::Reputation::GetAttitude; a failed answer leaves it 0); false: `attitude` is the default 0
    bool attitudeAsked = false;
    u32 targetId = 0;           // ships: the object the ship has targeted (CShip::get_target)
    bool targetsPlayer = false; // ships: it has the player targeted (an attacker)
    float maxHit = 0;           // ships: hull hit points at full health (CSimple::get_max_hit_pts): its toughness
    int rep = 0;                // reputation instance (pub::SpaceObj::GetRep), 0 none: what an objective's 'R' names
    // ships: in a docking sequence - its behaviour holds a dock port (IBehaviorManager::get_docking_port) - jumping
    // (CShip::is_jumping), riding a lane (CShip::is_using_tradelane)
    bool dockPort = false, jumping = false, lane = false;
    // loot: what it holds, how many, how many the player's hold can take, the value at the base price (goods.ini)
    u32 lootGood = 0;
    u32 lootUnits = 0;
    int lootFits = -1; // -1 unknown
    float lootValue = 0;
    // A caller's annotation, never set here (Contacts() reports what the game says): the autopilot's pre-aggro overlay
    // (autopilot::Contacts) marks a neutral ship the mission's script is about to turn hostile and reads its attitude
    // as hostile.
    bool preHostile = false;
};

// A reputation's display name as the game shows it (an NPC pilot's name; Contact::rep): "" when unknown.
std::wstring RepName(int rep);
// A space object's name as the game's comm caption names a speaker (decomp hud_comm.cpp Appearance): a ship's pilot (its
// reputation's name), else the object's own name (a base, station, planet, a named ship); "" when it has neither or is
// not in the player's system.
std::wstring SpeakerName(u32 id);

// Base price of a good / equipment by archetype id (DATA\EQUIPMENT\goods.ini), 0 unknown.
float Price(u32 archetype);
// A shield generator's offline_threshold (st_equip.ini [ShieldGenerator]: the capacity fraction at which it fails and
// goes offline to rebuild), -1 unknown.
float ShieldOfflineThreshold(u32 shieldArch);

// How dangerous it is where the player is: the highest encounter level of the zones at the ship (the system file's
// `encounter = <name>, <level>, <chance>`, the zones by the game's zone test), and the player's rank
// (pub::Player::GetRank). False when not flying.
struct Area
{
    int level = 0; // 0 = no encounter zone here
    int rank = -1; // the player's level, -1 unknown
    u32 zone = 0;  // the zone giving that level
};
bool ReadArea(Area& out);

// Solars and ships of the player's current system, nearest first (empty when not in space). Memoised per frame (below).
std::vector<Contact> Contacts();

// Per-frame memo of the full scans Contacts(), Obstacles() and PlayerWeapons() (audit 24 Oct 2026: 60+ / 20+ call sites,
// several per frame): between BeginFrame() and EndFrame() the first call scans, later ones get a copy of that result.
// The game does not simulate between the two (the controller brackets its OnFrame logic), so a copy equals a fresh
// scan; what this module itself changes inside the frame invalidates it - the waypoint calls (the HUD's waypoint
// object), ai::UseConsumable (the hold's free space: Contact::lootFits) - and so can
// any caller (InvalidateFrame()). Never begun (the subtitles logic), outside the bracket (render passes, debug
// commands): every call scans, as before. Per module.
void BeginFrame();
void EndFrame();
void InvalidateFrame();

// Guided missiles / torpedoes in flight in the player's system (CObject class CGUIDED_OBJECT 0xA7), nearest first:
// what they home on (CGuided::get_target), who fired them, and from the munition archetype whether one is a cruise
// disruptor and its hull damage. For countermeasure timing (a CM diverts missiles within its range by chance).
struct Missile
{
    u32 id = 0;
    float pos[3] = {};
    float distance = 0; // to the player's ship
    float closing = 0;  // m/s towards the player (from the previous call; 0 the first time)
    u32 targetId = 0;   // space id it homes on, 0 none
    bool targetsPlayer = false;
    u32 owner = 0;                // space id of the ship that fired it (CProjectile owner)
    bool cruiseDisruptor = false; // Munition cruise_disruptor
    float hullDamage = 0;         // hull damage of a hit: Munition hull_damage + its explosion's hull_damage
    float blastRadius = 0;        // its explosion's radius (m), 0 none
    bool homing = false;          // Munition seeker = SEEKER (a countermeasure can divert it; DUMB ones fly straight)
};
std::vector<Missile> Missiles();

// Gun rounds in flight toward the player (CObject class CBEAM_OBJECT 0x67; user, 19 Oct 2026: "track enemy
// projectiles in real time, model the damage deterministically"): each round's straight path relative to the player's
// motion, its closest approach and when. Only rounds of other ships that come within `radius` m (0: the ship's physical
// radius) inside `horizon` s; soonest first.
struct Round
{
    u32 owner = 0;        // the ship that fired it (CProjectile::get_owner)
    u32 munition = 0;     // its munition archetype id
    float hullDamage = 0; // its munition's hull damage per hit (+ explosion)
    float shieldMod = 1;  // weaponmoddb.ini factor of its weapon_type against the player's shield_type
    // damage to the shield while it is up: (hull_damage x constants.ini [ShieldEquipConsts] HULL_DAMAGE_FACTOR 0.5 +
    // energy_damage) x shieldMod (verified 19 Oct 2026: predicted / measured ~0.94 vs 2 police fighters)
    float shieldDamage = 0;
    float miss = 0; // closest approach to the player's centre (m)
    float tHit = 0; // seconds until that closest approach
};
// A hit = the closest approach inside the ship's own bounding box (CObject::get_surface_extents, ship axes) + 1 m;
// without extents, within `radius` (0: the physical radius). `shieldArch`: the player's shield generator (shieldMod).
std::vector<Round> IncomingRounds(float horizon, float radius = 0, u32 shieldArch = 0);
// Every frame in space (controller): follows each round predicted to hit until it vanishes (at its predicted time: a
// hit) or flies past (a miss) and learns the hit share per miss-distance band; RoundHitChance(miss) is that share
// (prior 0.5 until 10 rounds in the band). RoundStats() for the debug / docs.
void TrackRounds();
// The last round TrackRounds saw hit the player: its munition, the ship that fired it, its flight direction (world,
// unit) and when (QueryPerformanceCounter seconds). False if none yet.
bool LastRoundHit(u32& munition, u32& owner, float dir[3], double& when);
float RoundHitChance(float miss);
std::string RoundStats();

// The player ship's mounted guns and launchers, in the ship's equipment order (CEquipManager::Traverse over
// EquipmentClass Gun): live refire state (CELauncher refireDelayElapsed) and the launcher archetype's data, for
// staggered fire, launch planning and power budgeting. FLSDK CEquip / CELauncher / Archetype::Launcher.
struct Weapon
{
    u16 subObj = 0;      // CEquip SubObjId
    u32 archId = 0;      // the launcher's archetype id (nickname hash; Archetype::Root::get_id, 0 unknown)
    bool active = false; // CEquip isActive (mounted and powered)
    float refire = 0;    // refire_delay (s)
    float elapsed = 0;   // seconds since its last shot (CELauncher refireDelayElapsed)
    float power = 0;     // power_usage per shot
    float muzzleVelocity = 0;
    bool homing = false;   // its projectile is a homing munition (missile / torpedo launcher)
    float hullDamage = 0;  // the projectile's hull damage per hit (munition + its explosion)
    float blastRadius = 0; // the projectile's explosion radius (m), 0 none
    int ammo = -1;         // rounds left (CELauncher::GetAmmoCount)
    u32 ammoArch = 0;      // the projectile (munition) archetype id: its good's price is space::Price(ammoArch)
    // the munition's flight (homing launchers): lifetime (s), seeker range (m) / half-angle of view (deg) / max turn rate
    // (rad/s), motor delay / acceleration (m/s2) / burn time (s) - launch speed = ship velocity + muzzle velocity
    float lifetime = 0, seekerRange = 0, seekerFovDeg = 0, maxTurn = 0, motorDelay = 0, motorAccel = 0, motorTime = 0;
    uintptr_t projArch = 0;  // the munition archetype's address (diagnostics: debug `munition`)
    char hardpoint[32] = {}; // the hardpoint it is mounted on (CExternalEquip mountedHardpoint)
    int fireSlot = 0;        // its USER_FIRE_WEAPON<n> number (hardpoint name order; 0 unknown) - player only
};
std::vector<Weapon> PlayerWeapons();
// Another ship's guns and launchers (an NPC in the player's system, by space id): what it carries (ammo) - the loot a
// kill can drop.
std::vector<Weapon> ShipWeapons(u32 id);
// Turret reach (a ship's or a solar's guns, by space id): counts[i] = how many of its active guns can point at pts[i]
// now and reach it (the game's own CEGun::CanPointAt - joint limits widened by `tolerance` rad - and GetMunitionRange).
// Returns the number of guns, -1 unknown (counts then 0). For blind spots beside capital ships.
int GunsCovering(u32 id, const float (*pts)[3], int n, int* counts, float tolerance = 0);
// A ship's motion and agility (any ship in the system, the player's too): velocity (the physics'), nose, the turn rate
// it can hold (rad/s, steering torque / angular drag) and its strafe acceleration (m/s^2), from its archetype.
struct ShipMotion
{
    bool ok = false;
    float pos[3] = {}, vel[3] = {}, fwd[3] = {};
    float speed = 0, turnRate = 0, strafeAccel = 0, mass = 0;
    u32 arch = 0;
};
bool ShipMotionOf(u32 id, ShipMotion& m);
// The fraction of its shield at which a ship's shield fails (its generator's st_equip.ini offline_threshold), -1 unknown.
// A shield goes offline there, not at 0 (21 Oct 2026: predictions to 0 came true 1.0-1.5 s early, every time).
float ShipShieldThreshold(u32 id);
// How long a ship's failed shield stays down (its generator's offline_rebuild_time, s), -1 unknown. Measured 21 Oct 2026:
// 12.00 s in every case (the file's value), back at the offline threshold.
float ShipShieldRebuild(u32 id);
float ShieldRebuildTime(u32 shieldArch);

// The game's loot rule for one item a dying ship carries (Content.dll, docs\FREELANCER_RE.md "Loot drops"): the
// expected number dropped, given the player's net worth and how many the ship carries; 0 when the item has no
// [mLootProps] entry (DATA\MISSIONS\lootprops.ini, read once).
float ExpectedDrops(u32 archetype, float worth, int carried);
// The player's net worth as the story and the loot rule use it (pub::Player::GetAssetValue + cash); -1 unknown.
float PlayerWorth();
// The player ship's space id, 0 when not in space.
u32 PlayerShipId();
// The player ship's control exclusion mask (CEqObj +0x190, Common.dll set_control_exclusion / is_control_excluded):
// 0 = the player flies; a game sequence sets it - dock start at an airlock / gate port 4, launch 6, jump and landing 7,
// trade lane 3 - and its own end clears it. Bit 4 = the cinematic one: in space the letterbox and the player's lockout
// follow it (Freelancer-Decomp space_camera_game.cpp, 0x51D0B4). -1 when not flying.
int PlayerControlExclusion();
// Countermeasures the player can drop: the summed ammo of the active CM droppers, -1 when none is fitted.
int PlayerCountermeasures();
// Mines the player can drop: the summed ammo of the mounted mine droppers (EquipmentClass Mine), -1 when none is fitted.
int PlayerMines();

// The docking bays of the player's current target, from the game (IObjInspect::get_dock_hardpoints): each bay a
// straight approach line p2 (outermost) -> p1 -> mount; the docking cutscene starts when the docking ship reaches p2
// (docs\FREELANCER_RE.md "Docking geometry"). Type: 1 berth (player size), 4 / 5 larger moorings, 6 docking ring,
// others for gates / holes. Empty when the target has none (lane rings, ships).
struct DockBay
{
    int index = -1;
    int type = -1;
    float radius = 0;
    float mount[3] = {}, p1[3] = {}, p2[3] = {};
};
std::vector<DockBay> TargetDockBays();
// Where a lane ride from the targeted ring starts for the side the ship is on (IObjInspect::get_lane_start on the
// player ship: the ring's HpLeftLane / HpRightLane, 254 m below / 252 m above its centre on Li01, one per direction),
// the lane direction (unit) and the next ring that way. False when the target is not a lane ring.
bool TargetLaneStart(float start[3], float dir[3], u32& next);
// The same for a chosen direction: towards the ring `toward` (the next ring that way); the other direction's ride
// starts at the ring's other lane hardpoint, reversed. The game picks the direction by the side of the ring the ship
// is on when it decides (position; verified 18 Oct 2026: the nose does not matter). False if `toward` is neither way.
// `side`: the value TargetLaneSide must read for the ride to go that way.
bool TargetLaneStartToward(u32 toward, float start[3], float dir[3], int& side);
// The game's own direction test for the targeted ring at the ship's position now (get_lane_direction): 0 / 1, -1 unknown.
int TargetLaneSide();
// The target's physical radius and centre (IObjInspect::get_physical_radius), false if unknown.
bool TargetBody(float center[3], float& radius);
// A named hardpoint of the target in world coordinates (IObjInspect::get_hardpoint: a depot's HpCargo01..), false if
// the target has none by that name.
bool TargetHardpoint(const char* name, float pos[3]);

// Makes the object the player's target the way the game does for a contact list click: the action
// {0x1C USER_SET_TARGET, object id, SUBOBJ_ID_NONE} through 0x54BCB0. False if the game refused it.
bool SetTarget(u32 id);

// What the player ship's autopilot is doing, from the game's own state (docs/FREELANCER_RE.md "Maneuvers").
struct Pilot
{
    int op = -1;         // IBehaviorManager +0xC0: pub::AI::OpType of the running behaviour (Goto 2, Dock 7,
                         // InstantTradeLane 9, Formation 10, FreeFlight 19; -1 none)
    bool locked = false; // IBehaviorManager +0xBC: maneuvers locked (Act_LockManeuvers)
    bool lane = false;   // CShip::is_using_tradelane
    u32 laneEntry = 0, laneExit = 0; // CShip::get_tradelane_entrance_ring / _exit_ring
    bool jumping = false;            // CShip::is_jumping
    bool cruise = false;             // CShip::is_cruise_active
    u32 leader = 0;                  // formation leader's space id (IObjInspect::get_formation_leader), 0 if none
    u32 target = 0;                  // current target's space id (CShip::get_target), 0 if none
    u32 system = 0;                  // the player's system (0x673354)
    float pos[3] = {};
};

// False when not in space.
bool ReadPilot(Pilot& p);

// Puts a waypoint at a position of the player's system the way the nav map's "Set waypoint" does (the player's
// waypoint list, 0x4C4780), as the first and only one, and selects it as the target the way a click on its
// contact list row does (0x4CEEB0: the HUD creates the waypoint object). Returns the waypoint's space id, 0 if
// refused.
u32 SetWaypoint(const float pos[3]);
// Saves the player's waypoint list as it is now (before something replaces it: the autopilot's best-path route), for
// RestoreWaypoints.
void SaveWaypoints();
// Puts back the list SetWaypoint replaced (first replacement since the last restore), unless the mission has changed
// the list since (ownList: the caller knows the list is its own best-path route, just read). Returns the number of
// entries restored, -1 nothing saved, -2 not restored (list changed).
int RestoreWaypoints(bool ownList = false);
// Targets the first entry of the player's waypoint list as it is (a mission objective's navmarker, or the nav map's
// first waypoint), as a click on its contact list row does. Returns its space id, 0 if the list is empty.
u32 SelectFirstWaypoint();
// The farthest distance from which the game accepts a dock request (Common.dll; 0 if unknown).
float DockRange();
// Clears the player's waypoint list (0x4C46C0, as the nav map's "Clear all waypoints").
void ClearWaypoints();
// Drops one entry of the player's waypoint list (exe::RemoveWaypoint 0x4C46E0, the nav map's "Clear waypoint"; the later
// entries move down). False when not in space, the index is out of range or the symbol is unresolved.
bool RemoveWaypoint(int index);

// Things the ship can hit near a point, from the client's own objects (Common.dll CObject::FindFirst / FindNext):
// asteroids (CAsteroid, class 0x1003: the fields' rocks and mines, created around the player), dynamic asteroids
// (0x4001), solars (0x303: planets, stations, rings, gates) and ships other than the player's (0x503). Position by
// EngineObject::get_position, size by the physical radius (CSimple::get_physical_radius_r; EngineObject::get_radius for
// dynamic asteroids). For obstacle avoidance (autopilot), within `range` of `center`.
struct Obstacle
{
    float pos[3] = {};
    float radius = 0;
    u32 id = 0;   // space id (0 for client-only asteroids)
    u32 type = 0; // solars: Type bits
    bool asteroid = false, mine = false, ship = false;
    bool breakable = false; // a dynamic asteroid (class 0x4001): debris that breaks and can drop loot
    bool zone = false;      // a lethal zone's sphere (planet / sun core: damage >= 1000/s), no object (id 0)
};
std::vector<Obstacle> Obstacles(const float center[3], float range);
// How the system file's damage zones are classed (core audit 24 Oct 2026, audit_core 1.1 a: a planner's threshold, owned
// by the caller; the defaults are the values used so far). A zone with `damage` above `strictDamage` is a costly zone
// (StrictAt "damage"), unless it is a SPHERE whose damage is at least `lethalDamage` per second: a lethal sphere
// (Obstacle::zone) - planet and sun cores (2000000 and 10000 per second in Li01; radiation fields deal 40-50 and stay
// crossable strict zones; a straight leg through Pittsburgh's death zone killed the ship in M01b, 19 Oct 2026).
// Parsed once per system and rules. Obstacles() always uses the defaults.
struct ZoneRules
{
    float lethalDamage = 1000;
    float strictDamage = 0;
};
// The player's system's lethal spheres (planet / sun death zones, Obstacle::zone) within `range` of `center` - only those
// (cheap: no object walk).
std::vector<Obstacle> LethalZones(const float center[3], float range);
std::vector<Obstacle> LethalZones(const float center[3], float range, const ZoneRules& rules);
// Is the point in a field where contact is costly (minefield, explosive gas, extremely dense field, damaging / damage
// zone; not in the field's exclusion corridors)? The reason ("mine_danger_objects", "damage", ...), empty if not.
// From the system and field files plus the game's zone test (pub::System::InZones). (Overloads, not a default argument:
// the host-free tests' stand-in, tests\space_stub.cpp, defines the one-argument forms.)
std::string StrictAt(const float pos[3]);
std::string StrictAt(const float pos[3], const ZoneRules& rules);
// A planet's docking ring (system file: the object whose dock_with names the planet's base), 0 if it has none.
u32 PlanetDockRing(u32 planet);
// The space object an objective's 'R' entry names: FmtStr::append_rep_instance (Common.dll writes kind 'R'; 'O' is
// append_spaceobj_id), i.e. a reputation instance - small numbers for mission ships (M03 Walker = 8). Accepts a space
// id as well (M03's dock ring arrived as one). 0 when nothing in the system matches. byId / byRep: each match.
u32 ObjectiveObject(u32 ref, u32* byId = nullptr, u32* byRep = nullptr);
u32 ObjectiveObject(u32 ref, const std::vector<Contact>& all, u32* byId = nullptr, u32* byRep = nullptr);
// Seconds a jump through this gate / hole of the player's system takes once it starts: its jump_effect's jump_out +
// jump_out_tunnel + jump_in_tunnel + jump_in times (fx\jumpeffect.ini, the file freelancer.ini names). 0 unknown.
float JumpSeconds(u32 gate);
// The attitude of an object's faction (pub::SpaceObj::GetRep of a base, station, ship) towards the player (-1 hostile
// .. 1 friendly, pub::Reputation::GetAttitude). False if unknown.
bool AttitudeToPlayer(u32 id, float& att);
// The player ship's physical radius (m), 0 unknown.
float PlayerRadius();
// A space object's shield (server pub::SpaceObj::GetShieldHealth): current / max shield points, up. False if unknown.
bool ShieldOf(u32 id, float& current, float& max, bool& up);
// Invincible (the game's pub::SpaceObj::GetInvincible) and at its floor: guns cannot take more hull off it.
bool AtInvincibleFloor(u32 id, float health);
// Test aid: take the object's shield offline (the game's pub::SpaceObj::DrainShields).
bool DrainShields(u32 id);
// Line of sight in the player's system through the game's physics (PhySys::FindRayCollisions, real hulls): the object
// class of the first solid, unbreakable object between the points - 0x303 solar (station, base, planet, platform) or
// 0x1003 field rock - or 0 if the line is clear (ships, breakable debris and loot do not block; nor `ignore`, the
// object looked at); `blockerId` (optional) receives the blocking object's id (0 unknown).
u32 SightBlocker(const float from[3], const float to[3], u32 ignore = 0, u32* blockerId = nullptr);
// The same ray test for flying: the distance from `from` to the nearest solid, unbreakable object on the segment (real
// hulls: a lair's lattice, a trench wall, a station's arms), the objects in `ignore` skipped; -1 if the segment is clear.
float RayHitDistance(const float from[3], const float to[3], const u32* ignore = nullptr, int nIgnore = 0);
// Debug: every raw hit of that ray test (object, class, id, crc, distance), unfiltered.
std::string RayHitsRaw(const float from[3], const float to[3]);
constexpr u32 kBlockerSolar = 0x303, kBlockerRock = 0x1003;

// The position of an object of the player's system (server pub::SpaceObj::GetLocation), false if unknown.
bool ObjectPos(u32 id, float pos[3]);
// Position and orientation (3x3, rows; forward = -third column, as the player's kinematics) of an object of the
// player's system (pub::SpaceObj::GetLocation). False if unknown.
bool ObjectPose(u32 id, float pos[3], float rot[9]);
// A lane ring's neighbours (server pub::SpaceObj::GetTradelaneNextAndPrev: the system file's next_ring / prev_ring),
// 0 where the lane ends. False if not a lane ring (or unreadable).
bool LaneLinks(u32 ring, u32& next, u32& prev);
// Would the mission let the player ride the lane from `ring` toward `toward` (Act_PlayerCanTradelane state, the
// game's own check of a lane request)? 1 open, 0 closed (the request would be denied), -1 unknown.
int LaneOpen(u32 ring, u32 toward);

// The player's waypoint list as it is (24-byte entries: position, system, object), in order.
struct Waypoint
{
    float pos[3] = {};
    u32 system = 0, object = 0, seq = 0;
};
std::vector<Waypoint> Waypoints();
// The nav map's best path (Freelancer.exe exe::PlotBestPath): asks the server for the route from the ship to a point
// (and object) of a system; the answer replaces the waypoint list a few frames later (lane entry / exit rings, gates,
// the destination; empty when there is no way, e.g. locked gates). False if not in space.
bool PlotBestPath(const float end[3], u32 endObject, u32 endSystem);

// Debug command: contacts [dock|all] (nearest first). False if not one of them. `name` resolves string ids
// (each module has its own resource reader).
bool Command(const std::string& cmd, const std::string& rest, std::string* reply, std::string (*name)(u32 ids));
} // namespace space
