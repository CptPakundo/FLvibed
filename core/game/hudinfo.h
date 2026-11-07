// FLvibed - what HUD 2 (modules\hud\docs\UI_HUD2.md) needs to know that the other readers do not give yet (UI_HUD2.md 5.6): the
// weapon groups, display names, the target's faction / class / level, the engine's states, credits, the tractor, the
// trade lane run, jumps, docking, cruise disruption, the missile lock, the formation, a mission timer. Every call is
// cheap (cached internally where it touches strings or lists; names cached by id) and fails closed: an unknown value
// is reported as unknown (-1, "", false), never guessed. Owner: the READERS track; the HUD2 track only calls these.
// Facts behind each: docs\FREELANCER_RE.md "HUD 2 readers". Compiled into each module part that uses it (core\game).
#pragma once
#include "common.h"

#include <string>
#include <vector>

namespace hudinfo
{
// ---- weapons
// The active weapon group as the game keeps it (1..6), -1 unknown.
int ActiveWeaponGroup();
// A mounted weapon (the player's, in the HUD weapon list's order) with its display name and group membership.
struct WeaponInfo
{
    u32 arch = 0;          // equipment archetype id
    std::wstring name;     // display name (the equipment's ids_name), "" unknown
    u32 groups = 0;        // bit g-1 set = member of weapon group g (1..6); 0 = none / unknown
    bool launcher = false; // missile / torpedo / mine / countermeasure launcher
    int ammo = -1;         // -1 unlimited / unknown
    bool on = false;       // switched on now (the active group's guns)
    int slot = 0;          // its USER_TOGGLE_WEAPON<n> number in the game's list, 0 unknown
};
std::vector<WeaponInfo> Weapons();
// A ship by object id with a reference; ReleaseShip() it after use. Null when it is not a ship.
void* AcquireShip(u32 id);
void ReleaseShip(void* ship);
// Any equipment's display name by archetype id (cached), "" unknown.
std::wstring EquipName(u32 arch);

// ---- names
// The player's system and the zone at the ship (display names, cached by id), "" unknown.
std::wstring SystemName();
std::wstring ZoneName();
// A ship's affiliation (the reputation group's short name: "Rogues", "Liberty Police"), "" unknown.
std::wstring FactionName(u32 objId);
// A ship's class as a word ("FIGHTER", "FREIGHTER", "TRANSPORT", "GUNBOAT", "CRUISER", "BATTLESHIP", ...) or a solar's kind
// ("STATION", "PLANET", "TRADE LANE", "JUMP GATE", "JUMP HOLE", "LOOT", "WEAPONS PLATFORM", ...); "" unknown.
std::wstring ClassName(u32 objId);
// An object's physical radius (CSimple::get_physical_radius_r), 0 unknown: what the game's own label leaves out of a distance.
float ObjectRadius(u32 id);
// An NPC ship's level (its loadout / rank as the game shows "LEVEL n"), -1 unknown.
int Level(u32 objId);
// An object's name exactly as the game's target panel / contact list shows it now: "Unknown Jump Gate" (Moon, Planet,
// Sun, Trade Lane Ring, Station, Object) for a solar the player has not visited / discovered (the game's own test,
// exe::ObjectUnknown), else its name (space::SpeakerName). Cached per id, re-checked every second (a visit changes it).
std::wstring DisplayName(u32 objId);
// The game calls this solar unknown now: not visited, not discovered (that same test). False when it cannot tell.
bool Undiscovered(u32 objId);
// A trade lane ring. The test above never says unknown for one, whatever its lane's ends are: all 78 of New York's in SaveC706, Norfolk Shipyard not found (7 Nov 2026).
bool IsLaneRing(u32 objId);
// An object's name in a route's list, seen from anywhere on the map: as DisplayName, but a ring's lane name only when the player has found both its ends (the owner, 7 Nov 2026), else
// "Trade Lane". An end that names no place of this system counts as not found.
std::wstring RouteName(u32 objId);
// A trade lane ring's lane as the game's target panel names it ("Planet Manhattan->Fort Bush": the end on the viewer's
// side first, Freelancer.exe 0x4E8D30); "" unknown. DisplayName uses it for a known ring.
std::wstring LaneName(u32 ringId);

// ---- the player's ship
bool AfterburnerOn();
bool AfterburnerKnown(); // the reader resolved: AfterburnerOn's false means off
bool EngineKilled();
// The afterburner's gauge: CShip::get_thrust_power_ratio, 0..1, -1 unknown (docs\FREELANCER_RE.md "thruster / afterburner energy gauge").
float ThrustGauge();
// The mounted engine's archetype id (its nickname's hash), 0 unknown.
u32 EngineArch();
// Credits, -1 unknown.
long long Credits();
// The tractor beam: active now, and how many loot objects are within its reach.
bool TractorActive();
int LootInReach();

// ---- travel
// The trade lane run: on a lane, the rings left to the last ring, the object the lane leads to (its exit ring / the
// destination's name: the last ring's tradelane_space_name), seconds to the last ring at the ride's speed. False when
// not on a lane. ringsTotal / progress: the whole ride (from the entry ring), for a bar that fills over the ride.
struct LaneRun
{
    int ringsLeft = -1;
    std::wstring toward;
    float eta = -1;
    int ringsTotal = -1;  // the ride's rings after the entry ring, -1 unknown
    float progress = -1;  // 0..1 along the ride's path (entry ring -> last ring), -1 unknown
};
bool Lane(LaneRun& out);
// A jump in progress (gate / hole): the seconds until the jump, the destination system's name. False when none.
struct Jump
{
    float secondsLeft = -1;
    std::wstring toward;
};
bool Jumping(Jump& out);
// The system a jump gate / hole leads to (its display name), "" when the object is none or unknown.
std::wstring JumpToward(u32 objId);
// The game's system arrival line for the player's system as NN_ArrivalText words it: "New York System, Liberty Space."
// (IDS 1374 with the house by the game's own table, exe::ArrivalHouse) or "Omega-3 System." (1373 without a house);
// cached per system, "" unknown.
std::wstring ArrivalLine();
// Docking: the state of the player's dock request / approach (0 none, 1 requested, 2 queued, 3 granted / approaching,
// 4 in the docking sequence), the queue position (-1 unknown), the bay number (-1 unknown).
struct Dock
{
    int state = 0;
    int queue = -1;
    int bay = -1;
};
bool Docking(Dock& out);
// The server's dock answers, which only the controller host sees (host API v14 DockReply: sequence, kind, response): the
// HUD2 track calls this every flight frame with what HostDockReply returns (kind 0 = dock; response 3 queued, 4 granted).
// Without it Docking() still reports 1 requested / 3 approaching (port held) / 4 sequence, never 2 queued.
void SetDockReply(u32 seq, u32 kind, u32 response);
// The cruise engine was disrupted (a cruise disruptor / a lane disruption) within the last `withinSeconds`.
bool CruiseDisrupted(float withinSeconds = 3.0f);
// What is left of a disruption's wait, 1..0: the charge clock on its way up from -CRUISE_DISRUPT_TIME. 0 = the engine can charge.
float CruiseDisruptLeft();

// ---- combat
// The player's missile lock on the current target: 0..1 progress, -1 when the game keeps none / no launcher.
float MissileLock();
// The player's formation: the leader (0 none) and the members (ids), the player included where the game lists it.
struct Formation
{
    u32 leader = 0;
    std::vector<u32> members;
};
bool PlayerFormation(Formation& out);

// ---- missions
// A countdown the mission shows (seconds left), -1 none.
float MissionTimer();

// Debug: a one-line-per-reader status for `hudinfo` (controller debug command).
// The objective line as the game's banner shows it; "" when there is none. Never raw: an unfilled format code is dropped.
std::wstring ObjectiveLine(u32 textId);
std::string Status();
// Debug: research dumps behind the readers (`hudinfo wg|engine|lock|tractor|obj <id>|sys [id]|equip <arch>`).
std::string Research(const std::string& what);
// Debug (`hudinfo probe <mode> [a b c d]` / `hudinfo probe off`): a research request the controller's native HUD switch
// (hudvis) acts on each frame while HUD 2 shows - the view experiments of 25 Oct 2026. mode 0 = off.
struct Probe
{
    int mode = 0;
    float v[4] = {};
};
Probe& DebugProbe();
} // namespace hudinfo
