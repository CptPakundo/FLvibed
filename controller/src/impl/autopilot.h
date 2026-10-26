// Advanced Autopilot: the player ship flies with the game's own NPC behaviours (core\game\ai.*: goto, follow,
// formation, dock, face, trail, evade, break) chosen by a tactical loop that plays the part of an NPC state graph,
// tuned by a pilot personality (any of the game's [Pilot] definitions, every field overridable). Combat aiming is the
// controller's gimbal aim (the reticle locks onto the target's lead cross; the guns' gimbal does the rest); the
// autopilot decides when to fire, launch missiles and countermeasures. The player's own input takes over at once.
#pragma once
#include "common.h"
#include "settings.h"

#include <istream>
#include <string>

namespace autopilot
{
enum class Mode
{
    Off,
    Goto,      // fly to the target (or a point) with the game's goto
    Dock,      // dock / land / lane / jump with the target
    Follow,    // stay with a ship at a distance (pub::AI follow)
    Formation, // join the target's formation
    Face,      // turn to face the target and hold
    Escort,    // follow a friendly ship, fight what threatens it or the player, then rejoin
    Combat,    // fight the target, then (engage rules) the next hostile
    Travel, // the fastest way to a destination (or along the waypoint list): lanes, gates, fast dock (core\game\route)
    Hold,   // fly to a point (default: where the ship is) and keep station there until stopped
    Shoot,  // shoot named points of a target (a depot's cargo pods) from firing range, tractor what they drop
};
const char* ModeName(Mode m);

// What the controller must do this frame (it owns input: aim cursor, held commands, taps).
struct Out
{
    bool active = false;       // the autopilot flies: the controller does not steer
    bool gimbal = false;       // aim with the gimbal (reticle on the lead cross inside the cone)
    bool fire = false;         // hold the guns
    bool missile = false;      // tap missiles
    bool torpedo = false;      // tap torpedoes
    bool mine = false;         // tap mines (the game ignores it without a dropper / ammo)
    bool cm = false;           // tap countermeasures
    bool closestEnemy = false; // tap "closest enemy" (target selection)
    int slide = 0;             // hold a slide evade: 1 left, 2 right, 3 up, 4 down (the player's own dodge keys)
    int dodgeSlide = 0;        // missile dodge: overrides `slide` (same codes) and adds the afterburner
    bool afterburn = false;
    bool pursue = false;     // fly lead pursuit on the current target (the controller's Pursue: nose and reticle on the
                             // lead cross, binary throttle); the guns fire only when `fire` and the reticle is on it
    float pursueBand = 0;    // pursuit: close distance (m), 0 = by the target's size (at most half the gun range)
    float gunRange = 0;      // the ship's farthest gun range (m), 0 unknown
    float fireCone = 0.5f;   // pursuit: fire while the reticle (on the lead cross) is within this offset (half-screen)
    bool repairHull = false; // tap: use nanobots (the game's repair command)
    bool repairShield = false; // tap: use a shield battery
    bool collectAll = false;   // tap: tractor all loot in reach (USER_COLLECT_LOOT)
    bool brake = false;        // stop the ship (reverse to standstill), no steering
    float weave = 0;           // pursuit: corkscrew around the lead cross (0..1; evasion while tracking)
    float weaveRadius = 0.6f;  // pursuit: the corkscrew's radius at full weave, as a fraction of fireCone
    int turnManeuver = 0;      // pursuit, turning onto a target far off the nose under fire: 1 afterburner (never
                               // reverse), 2 reverse thrust (never afterburner) - overrides the pursuit's own throttle
    bool fireAllCmd = false;   // fire with the game's fire-all instead of each gun's slot (A/B measurement)
    bool stagger = false;      // staggered fire: each burst starts the guns one by one, evenly over their refire cycle,
                               // then fire-all (the guns refire out of phase: a steady stream of shots)
    bool steerTo = false;      // no target selected yet: fly towards steerPos at full throttle
    float steerPos[3] = {};
    float steerThrottle = 1;   // steerTo: throttle 0..1 (fast dock: 0 while braking)
    bool steerReverse = false; // steerTo: reverse thrust while steering (fast dock: braking before the bay)
    float steerFullAngle = 0;  // steerTo: 0 = full-rate turns (a fight); > 0 = proportional below this angle (rad)
    bool station = false;      // pursuit: hold stationPos with throttle and slides (the nose and guns stay on the
    float stationPos[3] = {};  // target): a capital ship's turret blind spot
    int cruise = -1;           // want the cruise engine: 1 on, 0 off, -1 leave it as it is (the controller toggles by
                               // the game's state: CEEngine::IsCruiseEngaged)
};

void Update(float dt, Out& out); // once per frame while in space
void TracePlayer(float dt);      // once per frame in space while the player flies: the motion trace (FLMotion.tsv)
void NoteMenu();            // the game is in its menu (a load passes through it): the spawns so far are another game's
void Stop(const char* why); // hands control back (free flight)
bool Active();
// The autopilot began the dock that is running now (its own request: stopped "docking" in the last 5 s, or the
// objective mode is on): its docking / landing cutscene is skipped as a player's Escape skips it.
bool OwnDock();
Mode Current();
// The target-based start: no target -> Combat on the closest enemy, hostile target -> Combat, friendly ship -> Escort,
// dockable -> Dock, else Goto.
void StartForTarget();
// The plain start of every device (the autopilot key, the bar button, View held, ASSISTED's A, `ap start smart`; the
// owner, 26 Oct 2026: identical on keyboard + mouse and the pad): with start_smart 1, from the game's live state - an
// objective now (the game's objective text) -> the objective mode; else a waypoint list (the nav map's: the player's
// plotted route or the mission's) -> travel along it (across systems); else StartForTarget. start_smart 0 =
// StartForTarget alone. Returns what it chose: "objective", "waypoints", "target", "refused".
const char* StartSmart();
// The objective mode on (as `ap start objective`); false with a host older than v11.
bool StartObjective();
// Travel along the waypoint list as it is (as `ap start travel` without a destination); false when refused / empty.
bool StartWaypoints();
void Load(const std::string& iniPath); // [Autopilot] section of FLController.ini
void Save(const std::string& iniPath); // the values only (`ap save`)
// The settings registry (autopilot_settings.cpp kDefs; core\settings.h): every [Autopilot] key but `knowledge` and
// `consumables`, which set several fields / take words. `WriteCommented` writes the whole section with a help line per
// setting (the controller's `config save`); `SettingsReference` lists everything with values and help (`ap settings`).
const settings::Def* Defs(size_t& count);
void WriteCommented(FILE* f);
std::string SettingsReference();
// The settings bus changed a row (the options page; core\settings_bus.h `applied`): the reactions `ap set` has.
void OnSettingApplied(const settings::Def* def);
std::string Command(std::istream& in, const std::string& iniPath); // debug: ap ...
} // namespace autopilot
