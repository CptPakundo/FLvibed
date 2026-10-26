// Route planning for travel (user, 18 Oct 2026: the fastest mix of flight and trade lanes, the way an experienced
// player travels): the game's own best path (the nav map's / an objective's waypoint list: lane entry and exit rings,
// gates, the destination) is the skeleton; for each lane on it the planner decides deterministically, from the ship's
// flight model (core\game\flight) and the game's lane ride law, whether riding beats flying, which ring to enter at
// and where to leave the lane early (free flight) when the next point lies along it. One system at a time: after a
// jump the route is planned again in the new system. Facts: docs\FREELANCER_RE.md "Lane ride timing", "Lane entry
// geometry and the fast entry", "Leaving a lane early", "Best path". Shared by every module (core\game).
#pragma once
#include "common.h"
#include "game/flight.h"
#include "game/space.h"

#include <string>
#include <vector>

namespace route
{
// The lane ride law: Common.dll's LaneAction constants (found by signature in its code; the measured values when not
// found) and the measured overheads.
struct LaneLaw
{
    float speed = 2500;      // ride speed (m/s)
    float accel = 312.5f;    // entry ramp (m/s^2): 0.125 of the speed per second
    float endSpeed = 400;    // the last segment slows linearly with distance to this (m/s)
    float exitRate = 2500;   // leaving by free flight: linear slowdown (m/s^2; 2500 -> 0 in 1.0 s, ~1.25 km)
    float endDwell = 0.87f;  // after the last ring until free flight (s) ...
    float endCarry = 213;    // ... and how far past the ring (m)
    float entryDelay = 0.8f; // fast entry: request -> ride (s)
    bool fromGame = false;   // speed / ramp / end speed read from Common.dll
};
const LaneLaw& Law();

// The rings of one lane from `from` to `to` in ride order (following the game's next / prev ring links), their
// positions and distances from `from`.
struct Lane
{
    std::vector<u32> rings;
    std::vector<float> x, y, z; // ring positions
    std::vector<float> at;      // distance of each ring from the first along the lane
    float speed = 0;            // this lane's ride speed, measured on a ride (0: the game's law - LaneLaw::speed)
    float length() const { return at.empty() ? 0 : at.back(); }
    void PointAt(float s, float out[3]) const; // the point s metres along (clamped to the lane)
};
bool LaneBetween(u32 from, u32 to, Lane& out);
// What a ride really ran at (the autopilot measures every ride: lane mods such as adoxa's Lane Speed set a speed per
// lane): stored for every ring of the ride's lane, used by every later plan over that lane. 0 = not measured.
void LearnLaneSpeed(u32 ring, float speed);
float LearnedLaneSpeed(u32 ring);

// Riding from `fromS` to `toS` (metres along the lane): seconds, and the speed at `toS`. The ride starts from rest at
// fromS (ramp), the lane's last segment slows to endSpeed.
float RideTime(const LaneLaw& law, const Lane& lane, float fromS, float toS);
float RideSpeed(const LaneLaw& law, const Lane& lane, float fromS, float s);

enum class Kind
{
    Lane,   // fast entry at `object` heading to `toward`, ride; leave at exitAt (free flight) or at the lane's end
    Jump,   // fast approach and dock with the gate / hole `object` (then plan again in the new system)
    Dock,   // fast dock with `object` (the destination)
    Arrive, // fly to `pos` and stop
};
struct Step
{
    Kind kind = Kind::Arrive;
    u32 object = 0;
    u32 toward = 0;       // lane: the next ring in the ride's direction
    u32 endRing = 0;      // lane: the lane's last ring on this ride
    float laneLength = 0; // lane: from the entry ring to the end ring (m)
    float exitAt = -1;    // lane: distance from the entry ring where free flight goes out (-1: ride to the end)
    float pos[3] = {};    // the step's point: entry ring / gate / base / destination
    float eta = 0;        // planned seconds from the plan's start to the end of this step
};
struct Plan
{
    bool ok = false;
    std::vector<Step> steps;
    float time = 0;   // the plan's total (s)
    float direct = 0; // flying straight to the system's last point without lanes (s), for comparison
    std::string text; // one line per step (events / debug)
};
// The planner's tuning (core audit 24 Oct 2026, audit_core 1.1 a): policy numbers owned by the caller, passed as
// `const Params& = {}`. The defaults are the values used so far, their provenance beside each.
struct Params
{
    // the lane-vs-fly rule: a ride's exits (leaving the lane by free flight) are weighed every `exitEvery` m along it
    // and at the lane's end
    float exitEvery = 500;
    // an arrival's stand-off (StandOff): a point just outside a lethal zone is as bad as one inside (21 Oct 2026, M01: the
    // mission's point after the Donau lay 129 m off Manhattan's 4100 m death zone - the avoidance's berth left a sliver,
    // the ship swerved at the atmosphere for 22 s and stopped 206 m short): out to the zone's surface + `standOff`; and the
    // arrival beside an object / out of a costly zone that far past its radius
    float standOff = 300;
    // the walk out of a costly zone (space::StrictAt) toward the start: `strictStep` m steps, at most `strictSteps` (20 km)
    float strictStep = 100;
    int strictSteps = 200;
    // a travel at a planet goes to its docking ring unless the ring's faction is hostile: attitude above this
    float ringAttitude = -0.6f;
};

// The fastest way through the waypoints of `system` (the list as the game has it; entries in other systems are
// beyond the gate), starting at `pos` with `speed`.
// `closed`: lane rings not to use (a mission can disable lanes for the player - the ride then never starts).
Plan Make(const flight::Model& m, const float pos[3], float speed, const std::vector<space::Waypoint>& wps, u32 system,
          const std::vector<u32>& closed = {}, const Params& prm = {});
// The plan from where the player ship is now through the waypoint list as it is (`closed`: rings closed this trip).
Plan MakeNow(const std::vector<u32>& closed = {}, const Params& prm = {});
// A plan's last dock becomes an arrival beside that object (outside its body and any costly zone, + 300 m:
// Params::standOff), for "fly to X" objectives that point at a dockable object before the mission asks to dock (M03: the
// docking ring).
void ArriveInstead(Plan& p, const float from[3], const Params& prm = {});

// Debug: `route [ring ...]` - MakeNow's plan (those rings closed); `lanelaw`. False if not one of them.
bool Command(const std::string& cmd, const std::string& rest, std::string* reply);
} // namespace route
