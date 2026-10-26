// Obstacle avoidance for the autopilot's own steering (user, 18 Oct 2026: better than the game's auto avoidance,
// deterministic from the ship's size and maneuvering, costing as little time as possible against straight flight, one
// winding path around every obstacle ahead at once). Input: the client's own objects near the ship (space::Obstacles:
// field rocks and mines, solars, ships) with their physical radii, the ship's radius, speed and top turn rate
// (core\game\flight).
// The path ("elastic band"): points every 25 m over the next ~5 s of travel (the game creates field rocks only ~1.5 km
// around the ship), starting at the ship along its velocity and ending back on the straight line to the aim. Iterated:
// every point inside an obstacle's clearance (its radius + the ship's + a margin) is pushed out radially, then the chain
// is pulled straight towards its neighbours (shortest, least bending) - together a smooth curve around all of them with
// the least detour. Each frame starts from the last path (stable sides). The bend between points is checked against the
// ship's turning circle (speed / turn rate): too sharp = slow down in a strict field (mines), else keep the speed (light
// contact is fine there - user). Steering looks ahead along the path. Shared by every module (core\game).
#pragma once
#include "common.h"
#include "game/space.h"

#include <vector>

namespace avoid
{
// The planners' tuning (core audit 24 Oct 2026, audit_core 1.1 a): policy numbers owned by the caller, passed to each
// function as `const Params& = {}`. The defaults are the values tuned so far, their provenance beside each.
struct Params
{
    // ---- Steer: the elastic band
    float step = 25;     // m between path points
    int iterations = 30; // push-out + smoothing rounds

    // ---- WheelScanStep / WheelAim: the rotating part's scans (both must get the same values: the mask is `bins` wide)
    int scanN = 33;         // grid cells per side
    float scanStep = 25.0f; // m
    int bins = 60;          // 6 deg angle bins round the pivot

    // ---- RayPath (planesFirst and narrowGain stay per-call arguments)
    // The berths tried in order, each a half width max(floor, shipRadius x radii) (session 7, M12: a wide berth first - a
    // line 11-27 m off the trench walls set off the wall slide and the guard, which ended the cruise -, the ship's own
    // width only when no wide way exists; the third was max(5, shipRadius) x 1.5, the same number)
    struct Berth
    {
        float floor, radii;
    };
    Berth berths[3] = {{80.0f, 5.0f}, {25.0f, 2.5f}, {7.5f, 1.5f}};
    // the solid-centre plane is not tried for a berth this wide or wider (the wide berth: the level and vertical planes
    // and the lattice only)
    float noCentrePlaneBerth = 80.0f;
    // (ray budget 80 000 in all: 12 000 measured 28 ms, session 7)
    int rayBudget = 80000;
    // the 3D lattice, per attempt (15 000: at 30 000 the wide berth's search from inside the M12 trench used up the whole
    // budget - 80 000 rays, 167 ms, nothing found - before the narrower berths ran)
    int latticeBudget = 15000;
    int planeBudget = 4000; // per plane and berth (the caller tries up to six)
    // the plane's polar grid (13 rings x 72 directions: the M12 trench is ~200 m wide - 7 x 48 left nodes ~250 m apart and
    // the line struck its outer wall, session 7)
    int rings = 13, angles = 72;
};

struct Input
{
    float pos[3] = {}, vel[3] = {};
    float aim[3] = {};       // where the ship wants to go
    float shipRadius = 10;   // m (space::PlayerRadius)
    float turnRate = 1.6f;   // rad/s (flight::Model::turnRate)
    std::vector<u32> ignore; // objects not to avoid (the one being docked with / entered)
    bool strict = false;     // in (or about to enter) a field where contact is costly (space::StrictAt)
};

struct Result
{
    bool blocked = false;    // the path leaves the straight line (something is in the way)
    float steer[3] = {};     // where to steer now (the aim when clear)
    float maxSpeed = 1e9f;   // fly no faster than this to follow the path's bends (m/s)
    float extra = 0;         // the path's extra length over the straight line (m)
    int checked = 0;         // obstacles considered
    int debrisIgnored = 0;   // small breakable debris among them, not avoided (it breaks harmlessly)
    float clearAhead = 1e9f; // how far the straight line to the aim is clear (m)
    bool strict = false;     // a field where contact is costly (mines in view): slowing allowed; elsewhere light
                             // contact is fine and the speed is kept (user, 18 Oct 2026)
    bool imminent = false;   // strict and a bend too sharp within ~0.4 s of travel
    float residual = 0;      // the deepest the final path still cuts into an obstacle's radii (m; > 0: contact)
};

// The last path (warm start: stable sides between frames).
struct Memory
{
    bool active = false;
    std::vector<float> path; // x y z per point
};

Result Steer(const Input& in, const std::vector<space::Obstacle>& obstacles, Memory& mem, const Params& prm = {});

// Close-quarter flight through real geometry (user, 22 Oct 2026: M12's lair trench and core, M13's Dyson sphere - "close
// quarter flight decisions that are highly aware of nearby geometry"; Steer sees bounding spheres, which a lattice or a
// trench is not). The game's own ray test (space::RayHitDistance: real hulls) along the way ahead, the ship's width as
// three parallel rays: the wanted direction if it is clear for `look` m, else the clear direction nearest to it from a
// fan of rings at 12-85 degrees round it (8 each), else the one clear the longest (brake: nothing is clear).
struct FanResult
{
    bool blocked = false; // the wanted direction is not clear: steer along `dir`
    bool brake = false;   // no direction is clear for `look`: stop (reverse) while turning to `dir`
    float dir[3] = {};    // unit direction to fly
    float clear = 0;      // how far `dir` is clear (m; `look` when fully clear)
    int rays = 0;         // ray tests made
};
FanResult RayFan(const float pos[3], const float wanted[3], float look, float shipRadius, const u32* ignore,
                 int nIgnore);

// A way through real geometry from `from` to `to` (session 7, 23 Oct 2026, the user watching M12: "the ship tried to engage
// another generator without cruising its way through the tight trench at the lair's perimeter", "poor navigation between
// generators"; the user's try 3 hops follow the trench's arc at 300 m/s): straight when that is clear for the ship's
// width, else the shortest chain over a polar grid in the plane through both ends and the centre of the biggest solid
// the straight line crosses (Params::rings x angles; lazy Dijkstra, each leg ray-tested - space::RayHitDistance, five
// parallel rays), string-pulled to the fewest legs in plain sight. The points after `from`, `to` last, flattened
// (x, y, z, ...); empty when no way was found within the ray budget. `raysOut`: ray tests made.
// `planesFirst`: the first plane with a way wins and the 3D lattice is not used (the hops between static targets: the
// lattice's straight chords stalled twice in the M12 trench, session 7). `narrowGain` > 0: after the wide berth's way the
// next, narrower berth is tried too and kept when shorter by that fraction (0.12 = 12 %). Berths and budgets: `prm`.
std::vector<float> RayPath(const float from[3], const float to[3], float shipRadius, const u32* ignore, int nIgnore,
                           int* raysOut = nullptr, bool planesFirst = false, float narrowGain = 0.0f,
                           const Params& prm = {});
// How the last RayPath found its way ("level plane, berth 37 m", "none").
const char* RayPathMode();

// Crossing a solar's rotating part (session 7, 23 Oct 2026; the M12 core's reactor wheel - 6 spokes at 36 deg/s; the user's
// try 3 crossed at ~190 m off its pivot through a gap at 297 m/s, try 1 struck a spoke; the user: "the ship flew too close
// to the turbine's center, thus it couldn't avoid it in time"): two scans 0.25 s apart, each a 31 x 31 grid (20 m) of rays
// parallel to the line of flight through the part's plane (the game's ray test; only the part's hits, class 0x1, counted)
// -> its cross-section; the centroid = the pivot, the best angular shift between the scans = the spin. Then the crossing
// ring (the radius with the most free angle outside the hub) and, every frame from the mask alone, the aim: at the
// predicted arrival time, the middle of the free window nearest our own angle on that ring.
struct WheelScan
{
    int phase = 0;                // 0 none, 1 first scan taken, 2 ready (pivot, spin, ring known), -1 given up
    u32 part = 0;                 // the moving part's object id (the ray test's hit)
    float plane[3] = {};          // a point of the part's plane (its first hit)
    float dir[3] = {};            // the line of flight (unit; the plane's normal)
    float e1[3] = {}, e2[3] = {}; // the plane's axes
    float pivot[3] = {};
    float spin = 0;                          // rad/s (+ from e1 toward e2)
    float ring = 0;                          // the crossing radius (m)
    float t0 = 0;                            // the clock of the second scan (s; the caller's)
    std::vector<unsigned char> mask0, mask1; // 31 x 31 occupancy at the two scans
    float ringFree = 0;                      // free fraction of the ring
    std::vector<unsigned char> ringMask;     // occupancy round the ring at t0, 1 deg bins (360)
};
// Start (phase 0 -> 1) or finish (1 -> 2) the scans at clock `now` (the part's plane at `hit`, flight along `dir`).
// False if there is nothing to time (no moving part in the scans: phase -1).
// The grid and the angle bins: `prm` (scanN, scanStep, bins).
bool WheelScanStep(WheelScan& w, const float hit[3], const float dir[3], u32 part, float now, std::string* info,
                   const Params& prm = {});
// The point in the part's plane to fly through, arriving `tArrive` s after clock `now`, starting from `from` (phase 2;
// `prm` as given to WheelScanStep).
bool WheelAim(const WheelScan& w, const float from[3], float now, float tArrive, float aim[3], const Params& prm = {});

// The length of a run from `from` to `to` the way the steering flies it: straight, or round every lethal zone (with the
// berth Steer keeps at `speed`), planet and big station (radius >= 300 m) in the way - straight until the clearance is
// inside Steer's look-ahead window, then tangent, arc, tangent; a body inside another's clearance counts once; field
// rocks and ships are left out (small detours on the way). For plans and predictions (21 Oct 2026: Planet Leeds' ring lies
// behind the planet seen from the Iw03 hole, the straight-line plan said 23.5 s and the run round the zone took 28.3).
float PathLength(const float from[3], const float to[3], const std::vector<space::Obstacle>& bodies, float shipRadius,
                 float speed, float turnRate, u32 ignore = 0);
} // namespace avoid
