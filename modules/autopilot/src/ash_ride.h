// ASH-RIDE (switch `ash_ride`, with `spawn_charge`, `spawn_stage` and `chase_kill`; docs\research\runners\
// M02_ashcroft_datapass.md "The ONE proposal", adapted to its P0 fact: in HDE an engine kill at cruise ends cruise - the
// speed holds ~300 m/s while killed, the engine back on drops to ~90 m/s within 0.5 s and a new ~8.5 s charge is needed,
// so the runners' short engine-kill taps do not hold cruise speed here). The pure half: facts in, verdict out - no game
// call, no statics - so tests\ash_ride_test.cpp drives it. The impure adapter: autopilot_ashride.cpp.
// The four rules, each from the recordings (the data pass's table, R1-R3):
//  1. leave the last escort at shield 0 / hull <= 0.6 (the runners: 0.36-0.59) to the NPCs (King / the police killed it
//     23-30 s later); back to killing it when it still lives 35 s on, or a leash / nag of the fight would fire;
//  2. be at cruise speed near his spawn when he appears (every runner at 247-299 m/s; ours 0-96 in six probes): a cruise
//     loop round a centre on his run line behind the spawn, inside every leash;
//  3. at his spawn, intercept at cruise to a station behind him on his line, timed for his slow ramp (~75-80 m/s for
//     ~5 s, ~300 only at +10-12 s): arrive as he reaches cruise; past him, pass and turn at cruise (R1's head-on pass and
//     U-turn);
//  4. the coast (engine kill) only when he is at >= 295 m/s, we are at cruise and the gap is 90-220 m, stable >= 1 s.
// The owner's correction (binding; the data pass's "R3's taps keep cruise in vanilla" was an inference from
// OCR'd speeds, not a fact): an engine kill out of cruise never redirects the velocity - the ship coasts straight on its
// velocity vector (turning the nose changes only where the guns point) and cruise is lost (HDE P0: engine back on -> ~90
// m/s, a new 8.5 s charge). So: no taps anywhere; a coast is a straight line - it starts only with him dead ahead on our
// VELOCITY line and travelling parallel to us (the gate below), the nose aims the guns / missiles meanwhile; the loop and
// the U-turn of an overrun turn only under the cruise engine (the ride never leaves cruise); and leaving cruise costs the
// 8.5 s charge, which the intercept counts (our way in the time he still needs comes from our charge profile).
#pragma once
#include "common.h"
#include "lookahead.h"

#include <string>
#include <vector>

namespace ashride
{
// ---- 1. the leave rule
constexpr float kLeaveShield = 0.01f; // the escort's shield fraction counted as down (the runners leave at shield 0)
constexpr float kLeaveHull = 0.6f;    // ...and its hull at most this (R1 0.36, R2 0.59, R3 0.56-0.58 at the cruise press)
constexpr float kLeftS = 45;          // the escort still alive this long after we left: back to killing it (the proposal's
                                      // fallback; frame-verified 31 Oct 2026: the escort died ~24.8 s (R1) / ~38.3 s (R3)
                                      // after the press - 35 s would have turned back in R3)
constexpr float kLeashOver = 0.04f;   // beyond (the leash's frac + this) x its radius from its anchor ...
constexpr float kLeashOutSpeed = 20;  // ...and moving away from it faster than this (m/s): back to the fight - the nag or
                                      // the supervisor's 80 % trip rule would fire next (frac 0.75 / 0.8 -> 0.79 / 0.84)
constexpr float kSpawnWaitS = 10;     // the escort dead and his contact not seen this long: the ride ends (spawn_charge)
// The escort may be left: its shield down (fraction known, <= kLeaveShield) and its hull known, > 0 and <= kLeaveHull.
// An unknown shield (-1) or hull keeps the fight (fail closed).
bool Leave(float shieldFrac, float hull);
struct FallbackResult
{
    bool fall = false;
    std::string why; // "35 s", "leash <name> <d>/<limit> m"
};
// After the leave: back to the fight when the escort still lives kLeftS after it, or we are beyond a leash's
// (frac + kLeashOver) x radius and moving outward (> kLeashOutSpeed). A dead escort never falls back (the step moves on).
FallbackResult LeftFallback(float leftS, bool escortAlive, const float me[3], const float vel[3],
                            const std::vector<lookahead::StageLeash>& leashes);

// ---- 2. the loop
// How the loop's radius is chosen (measured, 31 Oct 2026, tests\ash_ride_test.cpp states it):
//  - the game's top turn rate is the flight model's (steering torque / angular drag, the slower of pitch and yaw:
//    Starflier 25000 / 15000 = 1.667 rad/s, flight::Model::turnRate);
//  - the velocity of a cruising ship follows the nose at a lower rate: the FLMotion traces of every leg flown 30-31 Oct
//    2026 (120k two-second windows at >= 280 m/s, level flight) top out at 1.23-1.33 rad/s (M02 sc4-c_i1 1.228 rad/s =
//    a 239 m radius; all legs 1.331 = 215 m) - kTurnUse = 1.23 / 1.667 = 0.74 of the model's rate;
//  - the controller's proportional steering (full rate only beyond 0.35 rad off) flew M02's cruise turns at 0.6-0.8
//    rad/s (radius 360-445 m: as0-as4, sc3, sc4, look0) - a loop asking for half the top rate stays inside what it flies
//    and leaves the other half to correct: radius = kLoopMargin (2) x the turn radius at cruise = 2 x 300 / (0.74 x
//    1.667) = 486 m for the Starflier, one lap ~10 s - so at any moment of his spawn we are <= 1.2-1.7 km from it;
//  - clamped to kLoopMinR..kLoopMaxR (a model read wrong cannot ask for a 100 m or a 2 km loop).
constexpr float kTurnUse = 0.74f;      // the share of the model's top turn rate a cruising ship's velocity follows
constexpr float kTurnMeasured = 1.23f; // rad/s: the measured top when the model gives none
constexpr float kLoopMargin = 2.0f;
constexpr float kLoopMinR = 400, kLoopMaxR = 700;
constexpr float kSpawnClear = 200;   // the loop passes no closer than this to the spawn (no ram at his appearance)
constexpr float kBackMin = 400, kBackMax = 800; // the centre this far behind the spawn on his line (the proposal)
constexpr float kMaxFromSpawn = 2000; // no point of the loop farther than this from the spawn (the brief's 1-2 km)
constexpr float kLeadS = 1.0f;        // the loop's carrot: this many seconds of flight ahead along the circle
// The turn radius at cruise (m): cruiseSpeed / (kTurnUse x turnRate); turnRate <= 0: cruiseSpeed / kTurnMeasured.
float CruiseTurnRadius(float cruiseSpeed, float turnRate);
float LoopRadius(float turnRadius); // kLoopMargin x turnRadius, clamped
struct Loop
{
    bool ok = false;
    float c[3] = {};     // the centre (the spawn's height)
    float u[3] = {};     // his run direction (horizontal unit)
    float r = 0;
    float back = 0;      // the wanted centre's distance behind the spawn (before any pull)
    float dSpawn = 0;    // the centre's distance from the spawn
    float farthest = 0;  // the loop's farthest point from the spawn (dSpawn + r)
    bool clamped = false;
    std::string why;   // "free", "clamped", or the refusal: "no-line", "leash <name> tighter than the loop",
                       // "no-loop-in-leashes", "far <d> m"
    std::string bound; // the leashes that pulled it (comma separated)
};
// The loop: centre = spawn - back x u, back from r + kSpawnClear (clamped to kBackMin..kBackMax) then the other 100 m
// steps of that band; each pulled into every leash shrunk by r (frac x radius - r round its anchor: the whole circle inside),
// by alternating projections; the candidate with the least pull wins; refused when none fits or its farthest point lies
// beyond kMaxFromSpawn.
Loop PlanLoop(const float spawn[3], const float runTo[3], float r, const std::vector<lookahead::StageLeash>& leashes);
// The way round: +1 when our motion turns counter-clockwise round the centre in (x, z) (the angle atan2(z, x) grows),
// -1 else - the direction we already turn, so the entry is a smooth turn.
int LoopDir(const float me[3], const float vel[3], const float c[3]);
// The carrot on the circle: our angle round the centre plus (outside the circle) the tangent's angle acos(r / d) plus the
// lead (speed x kLeadS / r rad, 0.25..1.0) in the loop's direction, at the centre's height.
void LoopCarrot(const float me[3], const Loop& l, int dir, float speed, float out[3]);

// ---- 3. the intercept
constexpr float kGap = 160;        // the station this far behind him on his line (in the runners' 87-213 m, R2 114 / R1 213)
constexpr float kSlack = 150;      // go when our way to the station is at least the way he still needs minus this (m): an
                                   // early arrival costs ~30 m of gap per second (his last second averages ~270 m/s),
                                   // a late one its whole lateness (both at 300 m/s then: nothing closes)
constexpr float kHisTop = 295;     // he counts as at cruise (the coast's bar, the runners' 296-299)
constexpr float kReadyS = 0.5f;    // ...or this close to it by the profile
constexpr float kOverrunAlong = -90; // closer behind him than this (along his line; 0 = beside him): passing him
constexpr float kTrackMax = 600;   // tracking him: behind him within this along his line ...
constexpr float kTrackLat = 200;   // ...and within this of his line
constexpr float kJoinMax = 1200;   // the join point lies at most this far behind the station (a ~45 deg join)
// The intercept's steering turns at the full rate beyond this angle off (rad; SteerTo's fullAngle) - the way estimate
// counts turns at the cruise turn radius (CruiseTurnRadius, the measured top), so the steering must fly them so: in the
// closed-loop run (tests\ash_ride_test.cpp, M02) 31 of 32 start phases reach the coast window at the measured 1.23 rad/s
// with a 0.12 rad band, 15 of 32 with the fight's 0.35 band at 1.0 rad/s (late: a slow turn costs more way than counted)
constexpr float kSteerFullAngle = 0.12f;
constexpr float kLoopFullAngle = 0.35f; // the loop: calm proportional turns (it asks only half the top rate)
// His charge as the profile has it (lookahead::kHisCharge: 75 m/s for 5.5 s, 300 at 9.5 s - the LOOK probe in HDE) and as
// his speed reads now: s to his cruise speed (0 when at kHisTop), and the metres he flies in the next T s.
float HisTimeToTop(float hisSpeed, float tSince);
float HisAdvance(float hisSpeed, float tSince, float T);
// Ours (the owner's point 4: leaving cruise costs the charge): the metres we fly in the next T s - `chargeLeftS` s more at
// the engine's speed (lookahead::kOurCharge.slow, 80 m/s; 0 when the charge is done or we cruise), then the ramp at
// flight::Model's 60 m/s^2 (kOurRamp) from our speed (at least the engine's) to kOurCharge.top, then that speed.
constexpr float kOurRamp = 60; // m/s^2 (EngineEquipConsts CRUISING_SPEED / CRUISE_ACCEL_TIME, flight.h)
float OurAdvance(float mySpeed, float chargeLeftS, float T);
// The extra way a turn of `angle` rad costs at radius R over the chord: R (angle - sin angle) (a U-turn: pi R).
float TurnLen(float angle, float R);
enum class Verdict
{
    Wait,    // early: keep looping (the adapter steers the loop's carrot)
    Go,      // fly to the station he reaches cruise at (by the join point), at cruise
    Overrun, // beside / ahead of him while he is slower: pass and turn at cruise onto the station behind him
    Track    // behind him on his line, he at cruise: the nose on him (the coast's gate decides the engine kill)
};
const char* VerdictName(Verdict v);
struct InterceptFacts
{
    float me[3] = {}, vel[3] = {};
    float mySpeed = 0;
    bool cruising = false;  // the cruise engine engaged (charging or cruising)
    float chargeLeftS = 0;  // s of our charge still to run (0 done / cruising; not engaged: the whole charge)
    float his[3] = {};
    float hisSpeed = 0;
    float u[3] = {};      // his run direction (horizontal unit: spawn -> run point)
    float tSince = 0;     // s since his spawn
    float turnR = 0;      // our turn radius at cruise (CruiseTurnRadius)
    bool committed = false; // a Go was given already (no wait after it)
};
struct InterceptResult
{
    Verdict v = Verdict::Wait;
    float steer[3] = {};   // where to steer (Wait: none - the loop)
    float station[3] = {}; // the station when he reaches cruise
    float along = 0;       // our position along his line relative to him (< 0 behind)
    float lat = 0;         // ...and off it
    float path = 0;        // our way to the station (m)
    float need = 0;        // the way we fly in his time to cruise (OurAdvance: the charge counted)
    float tReady = 0;      // his time to cruise (s)
    const char* why = "";
};
InterceptResult Intercept(const InterceptFacts& f);

// ---- 4. the coast gate
constexpr float kCoastMin = 90, kCoastMax = 220; // the gap (the runners: R3 87-96, R2 114, R1 213)
constexpr float kCoastHis = 295;                 // his speed at least
constexpr float kCoastMe = 290;                  // ours, in cruise (the runners' 299; R1's turn dip 273 is excluded)
constexpr float kStableBand = 10;                // the gap within this of where it settled (the data pass: +-10 m) ...
constexpr float kStableS = 1.0f;                 // ...for this long (the runners: 2 s / 7 s / 3 s)
// The coast is a straight line: he must lie on our velocity line and run parallel to it. 6 deg: at the gate's
// 220 m his offset from our line is <= 23 m (a fighter's ~10-15 m half span + the guns' gimbal take the rest); with his
// course within 6 deg of ours the offset grows at most 300 x sin 6 deg = 31 m/s, and the stable-gap second before it
// (both ~300 m/s, the gap +-10 m) already shows the courses parallel; the coast ends by its own rules (past 0.9 x gun
// range, our shield, 30 s)
constexpr float kCoastVelDeg = 6;      // the line to him within this of our velocity
constexpr float kCoastParallelDeg = 6; // his velocity within this of ours
struct CoastGate
{
    float since = -1; // when the gap settled (s; -1 not settled)
    float ref = 0;    // ...at this gap
};
struct CoastFacts
{
    float now = 0; // s
    float gap = 0, his = 0, me = 0;
    float velDeg = 180;      // the line to him off our velocity
    float parallelDeg = 180; // his velocity off ours
    bool cruising = false;
    bool shieldOut = false; // our shield almost down: no coast (ChaseStep's kChaseShieldOut)
};
// The angle (deg) between two vectors (180 when either is ~0).
float AngleDeg(const float a[3], const float b[3]);
// True when the coast may start: every condition holds and the gap stayed within kStableBand of where it settled for
// kStableS. A failed condition forgets the settling; `stableS` (optional): how long it has held.
bool CoastReady(CoastGate& g, const CoastFacts& f, float* stableS = nullptr);
} // namespace ashride
