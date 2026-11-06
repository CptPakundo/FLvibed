// LOOK - act on the next step's goal while the current step only waits (modules\autopilot\docs\LOOK_PLAN.md; slice L0: log
// only, switch `look_log`, default 0). This is LOOK's pure half: facts in, verdict out - no game call, no statics - so
// tests\look_test.cpp drives it over synthetic and real scripts. The rule (LOOK_PLAN 1): (a) kill - a Cnd_Destroyed on a
// mission SHIP (Goal kind 4) armed now or about to be armed by the current step's chain, the ship alive, hostile to the
// player and a visible contact: fight it now (M02 "Follow Ashcroft"); (b) fly - else the least-cost armed / next-step
// distance goal instead of the nearest (M05: player_near_br04_01 vs dst_to_br04_tradelane). Every safety predicate that
// would block a candidate is named (the current step's towards-nag, the ship leashes, PSN: StageablePlace); tethers are
// parsed and reported only. The impure adapter that fills the facts, logs the verdict when it changes and answers
// `ap look`: autopilot_objective.cpp (LookFactsNow). Compiled into the controller's logic and, plain, into
// build\test\fltest.exe.
#pragma once
#include "common.h"
#include "game/mission.h"

#include <string>
#include <vector>

namespace lookahead
{
// One ship of a kill goal as the game shows it now (the adapter: by spawn position - mission::ShipIdsOf - else by its
// pilot's name).
struct KillShip
{
    std::string nick;        // [MsnShip] nickname (lower)
    u32 id = 0;              // its contact; 0 none (not spawned, or not found)
    bool alive = false;      // hull left (Contact::health != 0)
    bool hostile = false;    // the game's attitude to the player <= kHostile (not pre_aggro's overlay)
    bool preHostile = false; // only pre_aggro's overlay reads it hostile: the script has not turned it yet
    bool friendly = false;   // attitude > 0
    // CEqObj::is_targetable: a ship we cannot target we cannot fight (no cloak reader exists - space::Contact has none -
    // so this is the only "cloaked" stand-in L0 has)
    bool targetable = false;
    float pos[3] = {};
    float dist = 0;   // from the player
    float hull = -1;  // 0..1, -1 unknown
    bool spawnKnown = false; // where the script spawns it (Goal ShipRef): PSN's place while it is not spawned
    float spawn[3] = {};
    // the game's own invincibility (space::Invincible), read only when the script has an Act_Invulnerable on it
    bool invRead = false, inv = false, invSecond = false;
    float invFloor = 0;
};
struct KillGoal
{
    std::string trigger, leads, label, source; // source: "armed" (AdvanceGoals' view), "chain" (ChainGoals), "next"
    int count = 0, hops = 0;
    std::vector<KillShip> ships;
    float healthGate = 0; // the script's destroying health gate (Goal::healthGate), 0 none
    bool gateFails = false;
    bool scriptInv = false, scriptInvSecond = false; // Act_Invulnerable on it in the script (Goal::invulnerable)
    float scriptInvFloor = 0;
    std::string invTrigger;
};
struct FlyGoal
{
    std::string trigger, leads, what, source; // what: the object's nickname (kind 1) or "point"
    int kind = 1, hops = 0, leadsOrder = -1;
    bool byEntry = false;
    bool placed = false; // the position is known (kind 2 always; kind 1 where the game places the object)
    float pos[3] = {};
    float radius = 0;            // the condition's distance (m)
    std::string laneFrom;        // the script's lane ride after it (Goal::laneFrom), "" none
    bool lane = false;           // ... and the game's lane read: ride time and end
    float laneRideS = 0;         // riding from laneFrom to the lane's end (route::RideTime)
    float laneEnd[3] = {};       // the lane's last ring in that direction
};
// An armed ship leash (mission::Leashes as the decision reads them): its anchor now.
struct LeashAnchor
{
    std::string name;
    bool found = false;
    float pos[3] = {};
    float dist = 0;
};
// The current step's towards nag (mission::TowardsNagOf).
struct Nag
{
    bool has = false, placed = false;
    std::string name;
    float pos[3] = {};
    float radius = 0; // its distance, else 4000 (Content.dll's default, LOOK_PLAN 4a)
};
struct TetherFact
{
    std::string a, b, trigger;
    float dist = 0, secs = 0, inner = 0;
    bool fails = false;
    float gap = -1; // a's distance from b now (b the player), -1 unknown
};
struct LookFacts
{
    bool havePilot = false;
    float me[3] = {};
    float flySpeed = 300; // m/s: the ship's cruise speed (flight::Model; 300 = CRUISING_SPEED, FREELANCER_RE)
    u32 textId = 0;       // the step (or the last real one standing in)
    std::string verb;     // the objective's verb (none / dock / escort / fight / survive)
    std::vector<KillGoal> kills;
    std::vector<FlyGoal> flies;
    std::vector<LeashAnchor> leashes;
    Nag nag;
    std::vector<TetherFact> tethers;
    // the objective the script declares next with a point here (pre-stage-next's place) and whether it asks for a
    // destruction (M13: NomadCap2's "Destroy" point is its spawn - PSN)
    bool nextPlace = false, nextDestroy = false;
    float nextPos[3] = {};
    u32 nextText = 0;
};

enum class Kind
{
    None,
    Kill,
    Fly
};
const char* KindName(Kind k);

struct LookResult
{
    Kind kind = Kind::None;
    u32 target = 0;   // kill: the ship's contact id
    std::string ship; // kill: its nickname
    std::string trigger, leads, source;
    float point[3] = {}; // fly: the goal's near side (70 % of its distance on our side, as the decision aims)
    float dist = 0;      // kill: the ship's distance; fly: the way to the near side (m)
    float cost = 0;      // fly: PathCost (s)
    std::string nearest; // fly: the candidate with the shortest way to its sphere (today's choice) and its cost
    float nearestCost = 0;
    // every refused candidate: "<trigger>[/<ship>]:<code>[(<detail>)]" (codes: unspawned dead friendly pre-hostile
    // not-hostile untargetable gate-fails leash nag psn unplaced inside)
    std::vector<std::string> blockers;
    bool nextOk = true; // PSN on the declared next place (StageablePlace); nextWhy its reason
    std::string nextWhy;
    std::string key; // what makes the verdict: step, kind, target, trigger, the blockers' codes (the log's dedupe key)
};

// Policy numbers of L0 (named, each with its source; LOOK_PLAN 1):
constexpr float kHostile = -0.6f;    // the autopilot's hostile attitude (HostileContact)
constexpr float kLeashFrac = 0.85f;  // a trip's near side beyond 85 % of a leash from its anchor is refused (the decision's)
constexpr float kNagDefault = 4000;  // Act_NagDistTowards' default distance (Content.dll, LOOK_PLAN 4a)
// PSN's keep-out round a destroy objective's place / an unspawned hostile's spawn: an L0 choice, not measured (the M13
// Cap2 death: the ship staged at the spawn point itself); the slices that act on it tune it
constexpr float kPsnRadius = 3000;

// PSN (WORK_QUEUE "PSN", LOOK_PLAN 1): may the autopilot go to `p` ahead of the script? False (with `why`) within
// kPsnRadius of a kill goal ship's spawn while that ship has no contact (unspawned or not visible), or of the declared
// next objective's point when that objective is a destruction (its object's place - M13 NomadCap2).
bool StageablePlace(const float p[3], const LookFacts& f, std::string* why = nullptr);
// The time (s) to the furthest-advancing candidate `dest` by way of `g`: straight flight to g's sphere at flySpeed, then
// - when g leads to an earlier objective than dest - on to dest's sphere: by g's lane ride when it has one (ride +
// flight from the lane's end), else straight from g's near side. g == dest (or g advances as far): its own flight.
float PathCost(const FlyGoal& g, const LookFacts& f, const FlyGoal* dest);
// The verdict: the nearest unblocked kill, else the least-cost unblocked fly goal, else none.
LookResult LookTarget(const LookFacts& f);
// The event line after "autopilot look " (one line, < 480 characters) and `ap look`'s full text.
std::string Line(const LookResult& r, const LookFacts& f);
std::string Describe(const LookResult& r, const LookFacts& f);
// One line per kill goal ship under the script's Act_Invulnerable (after "autopilot look "): the script's flags and floor
// against the game's own GetInvincible and the ship's hull - the open M02 fact (LOOK_PLAN 4a, Ashcroft).
std::vector<std::string> InvincibleLines(const LookFacts& f);

// ---- TIMER-DESTROY (docs\WORK_QUEUE.md, switch `timer_destroy`): a "Destroy ..." step the
// script advances by itself (mission::StepAdvanceOf kind "timer": M13 "Destroy the Nomads" ends on cap_timer ->
// trent_go_back -> ether_one ~20 s after it shows, whatever is killed) is not fought: the ship stages toward the next
// step's place (PSN: never toward a destroy objective / the step's own target), rests, and defends itself only against
// what shoots at it there. Without the script's knowledge (not the speedrun preset) it still fights while that is safe.
struct TimerDestroyFacts
{
    std::string advance;   // StepAdvance::kind ("" unknown: today's decision)
    bool speedrun = false; // Knows(Source::Hidden): the script's knowledge (the next place) may be used
    float hull = -1;       // 0..1, -1 unknown
    int batteries = -1;    // aboard, -1 unknown
    bool attacked = false; // a hostile ship shooting at us within kSurvivalReach
    bool havePilot = false;
    float me[3] = {};
    bool nextPlace = false, nextDestroy = false; // the advancing path's next objective has a point here / asks a kill
    float nextPos[3] = {};
    bool stepPlace = false; // the step's own target place (its navmarker: the battleship's)
    float stepPos[3] = {};
    // M13-MIMIC item 2: the script takes the next objective's dock now (an armed Cnd_BaseEnter row leading on, mission::
    // ArmedBaseEnters) and the next objective names its object (go_home's St01_dock_ring_1)
    bool dockNow = false;
    u32 nextObject = 0;
};
enum class TdVerdict
{
    Today,  // not this rule's step (or no knowledge to act on): the decision as before
    Fight,  // the default preset while it is safe: the fight as before
    Stage,  // fly to `point` (the next place, kTdStandOff short of it on our side)
    Rest,   // hold where we are (or at the next place once there)
    Defend, // answer what shoots at us (combat, the defend-only target filter)
    Dock    // dock with `nextObject` now: the script already takes it (the runners dock on the unlock)
};
const char* TdVerdictName(TdVerdict v);
struct TdResult
{
    TdVerdict verdict = TdVerdict::Today;
    float point[3] = {};
    float dist = 0;  // Stage / Rest at the place: our distance to the next place (m)
    std::string why; // one word or two for the log: "safe", "unsafe(hull)", "psn", "no-place", ...
};
// Policy numbers (named, each with its source):
// a battery count under which a big fight is no longer safe: Q4's predictor - every M13 battleship death of 75 legs was
// one turret volley on a low shield, and the batteries left on reaching 300 m decided it (>= 7: 15 / 15 lived)
constexpr int kTdSafeBatteries = 7;
constexpr float kTdSafeHull = 0.6f; // under it the default preset stops a fight the step does not need (a policy choice)
constexpr float kTdStandOff = 300;  // short of the next place (survival_stage's: beside the ring, not in its mouth)
constexpr float kTdArrive = 700;    // within it of the stand-off point: rest there (survival_stage's)
TdResult TimerDestroy(const TimerDestroyFacts& f);

// ---- ALLY-KILL (switch `ally_kill`; docs\research\runners\M13_gate_allykill.md): a step the script ends on a mission
// solar's death by ANY killer (Cnd_Destroyed without a 4th field) whose kill unlocks the next step's dock (Act_LockDock =
// Player, <obj>, unlock on the kill path, <obj> = the next objective's object, the next step a Dock) is not fought: the
// ship waits at that dock, answers only what shoots at it and docks on the unlock (the next step's own decision) - M13's
// gate battleship: runner R2 sits at the hole while the allies kill NomadCap3 ~45 s after the ambush. While no friendly
// wears it down the fight comes back (fails closed).
struct AllyKillFacts
{
    bool qualifies = false; // the step's shape (StepAdvance::AllyKillShape + the next step's verb Dock)
    bool speedrun = false;  // Knows(Source::Hidden): never under knowledge "exposed"
    bool havePilot = false;
    float me[3] = {};
    bool dockKnown = false; // the next objective's object placed now (space::ObjectPos)
    float dockPos[3] = {};
    u32 target = 0;          // the kill's solar (its space id)
    bool targetAlive = false; // its contact found, hull left
    float targetHull = -1;    // 0..1, -1 unknown
    int allies = 0;           // friendly ships whose target is it
    float waitedS = 0;        // since the ship first came within kAkAtDock of the dock this step (0 not yet)
    float hullDrop = 0;       // the target's hull lost since then (0 unknown)
    bool attacked = false;    // a hostile ship shooting at us within kSurvivalReach
    float hull = -1;          // ours (the log's)
    int batteries = -1;       // ours (the log's)
    bool fellBack = false;    // the fallback fired already this step: it keeps the fight
};
// Policy numbers (named, each with its source):
constexpr float kAkAtDock = 300;       // within it of the dock: at the dock (the design's; R2 at 0 m)
constexpr float kAkStandOff = 150;     // the stage point this far short of the dock on our side (in its mouth, not in the
                                       // hole's mesh; a point trip with no dock - a locked dock answers dock_denied_locked)
constexpr float kAkDefendReach = 1500; // attacked on the stage trip within this of the dock: defend (TimerDestroy's)
constexpr float kAkWaitS = 20;         // at the dock this long with no ally on it ...
constexpr float kAkHullDrop = 0.05f;   // ... and less than this hull lost: the fight (the design's fallback)
// ...and with allies on it too, when they do not wear it down: at the dock this long and still under kAkHullDrop lost
// (probe ak1: 8 allies "on" it, its hull 1.000 for 3 min at the hole, 2 of 3 legs failed the segment limit;
// R2's allies killed it ~45 s after the ambush - real work shows well inside 30 s)
constexpr float kAkAllyStallS = 30;
// Today: not qualifying / no knowledge / no pilot / target dead or unseen / no dock place; Fight: the fallback; Stage: a
// point trip to the stand-off (Defend when attacked within kAkDefendReach); Rest at the dock (Defend when attacked).
TdResult AllyKill(const AllyKillFacts& f);

// ---- SPAWN-STAGE (switch `spawn_stage`, with `chase_kill`; modules\autopilot\docs\AUTOPILOT.md "spawn_stage / chase_kill";
// docs\research\runners\M02.md "## Plan" 1, ASH-START): while a fight step's last kill is pending and that kill spawns a
// later step's kill ship at a known place, the ship waits beside that spawn - the runners are cruising ~0.8-2 km from
// Ashcroft's spawn when the 4th escort dies. PSN (StageablePlace) forbids an unspawned hostile's spawn in general; its
// exception from data: the spawned ship FLEES - its first orders are a flight away (a goto to a point, nothing at the
// player) and nothing else the same chain spawns lands within kPsnRadius of the place.
struct SpawnFacts
{
    bool hasPos = false; // the spawn position is known
    float spawn[3] = {};
    bool ordersFound = false;                               // its first [ObjList] was found in the script
    std::vector<std::pair<std::string, std::string>> cmds;  // that list's commands (keys lower case)
    struct Other
    {
        std::string nick;
        bool hasPos = false;
        float pos[3] = {};
        bool friendly = false; // set friendly to the player by the same chain (anything else counts as hostile)
    };
    std::vector<Other> others; // what else the chain spawns
};
SpawnFacts SpawnFactsOf(const mission::SpawnAfter& a);
// True (and `runTo` = the first goto's point) when the spawn flees: the first moving command is a GotoVec / GotoSpline
// (SetPriority, BreakFormation, MakeNewFormation, SetLifeTime, Avoidance may come before it), no command of the list
// names the player, the run point lies at least kPsnRadius off the spawn, and no other non-friendly spawn of the chain is
// unplaced or within kPsnRadius of it. False with `why` otherwise ("no-spawn-pos", "no-orders", "<cmd> first",
// "at-player(<cmd>)", "no-goto", "short-run <d> m", "<nick> unplaced", "<nick> spawns <d> m off").
bool FleeingSpawn(const SpawnFacts& f, float runTo[3], std::string* why = nullptr);
// Policy numbers (named, each with its source):
constexpr float kStageBack = 200;       // the stage point lies this far behind the spawn on his line (the design's; ~1 s at
                                        // his charge's start speed: he passes us within gun reach while charging)
constexpr float kStageSide = 250;       // ...and this far beside it (not in his nose: no ram, a crossing shot)
constexpr float kStageLeashFrac = 0.8f; // inside 80 % of the fight's leaving-nag distance from its anchor (nag_battle3)
constexpr float kStageArrive = 1500;    // a trip to the point beyond this; within it the stage holds (survival_stage's)
constexpr float kStageReach = 1500;     // the last kill-goal ship within this of the point is fought there
constexpr float kStageTimeoutS = 25;    // at the point this long without the kill: today's fight (fail closed)
struct StageResult
{
    bool ok = false;
    float p[3] = {};   // where to wait
    float u[3] = {};   // his run direction (horizontal unit): the nose at the point
    float dSpawn = 0;  // the point's distance from the spawn
    float dAnchor = -1; // ...from the leash's anchor (-1: none)
    bool clamped = false;
    std::string why; // "free", "clamped", or the refusal ("no-line")
};
// P = spawn - kStageBack * u + kStageSide * n (u: the horizontal unit from the spawn to the run point, n: horizontal and
// perpendicular to it, on the anchor's side), pulled toward `anchor` (nullptr: none) until within kStageLeashFrac x
// `leash` of it (leash <= 0: no pull).
StageResult StagePoint(const float spawn[3], const float runTo[3], const float* anchor, float leash);

// ---- SPAWN-CHARGE (switch `spawn_charge`, with `spawn_stage` and `chase_kill`; docs\research\runners\
// M02_ashcroft_consensus.md "The ONE design"): the stage point close behind the spawn ON his line, inside EVERY leash, so
// the cruise charge pressed at his spawn locks ~150-300 m behind him (both charges take ~8.5-9 s) - the coast's window.
// One leash (or leaving nag) the point must keep inside: within frac x radius of the anchor.
struct StageLeash
{
    std::string name;
    float anchor[3] = {};
    float radius = 0; // the script's distance (m); <= 0: ignored
    float frac = 0;   // the share of it the point keeps inside (<= 0: ignored)
};
// Policy numbers (named, each with its source):
constexpr float kChargeBack = 250;   // the point this far behind the spawn on his line (the note's design 1: ~250 m)
constexpr float kChargeSide = 70;    // ...and this far beside it: a crossing margin, 60-80 m (design 1: at a 200 m gap
                                     // 250 m aside is 51 deg off the nose, outside the coast's 30 deg)
constexpr float kChargeLeashFrac = 0.75f; // inside 75 % of every armed ship leash (design 1: the supervisor's own 80 %
                                          // trip rule, autopilot_objective.cpp "leash: ... back toward it", never fires)
constexpr float kChargeNagFrac = kStageLeashFrac; // ...and 80 % of the fight's leaving nags (as spawn_stage)
constexpr float kChargeMaxFromSpawn = 600; // no point within the leashes this close to the spawn: no stage (design 1)
// P = spawn - back x u + side x n (u: the horizontal unit from the spawn to the run point; n: horizontal, perpendicular,
// on the side whose point needs the least pull - ties: the tightest leash's anchor side), pulled into every leash
// (alternating projections onto the balls, which converge on a point of their intersection when there is one). ok
// false with why "no-line", "no-point-in-leashes" or "far <d> m" (the point inside them all lies > kChargeMaxFromSpawn
// from the spawn); else why "free" / "clamped" and dAnchor = the bound leash's anchor distance (-1 no leash).
// `bound` (optional): the leash names that pulled it, comma separated ("" none).
StageResult StagePoint(const float spawn[3], const float runTo[3], const std::vector<StageLeash>& leashes,
                       std::string* bound = nullptr, float back = kChargeBack, float side = kChargeSide);
// Still fleeing: farther than kChargeRunDone from his run point (at it he drops cruise and fights beside his thugs:
// consensus "What the numbers say" 2 - the PB first targeted him 1.2 km from it, M02.md M02.pod).
constexpr float kChargeRunDone = 1500;
bool StillFleeing(const float his[3], const float runTo[3]);
// No missile at a fleeing ship beyond this (design 3: as3 / as4 wasted 1-2 Javelins at 1.1-1.2 km at his spawn,
// launched at 80 m/s at a target leaving at 300 - consensus "What the numbers say" 7)
constexpr float kChargeNoMissileBeyond = 600;
// The coast's start speed against a FLEEING ship: from the stage's gap the coast window (110-250 m) is reached during
// our charge's ramp (ChargeGap: ~200 m at our 150 m/s with him at ~110 m/s and rising), where a coast at 150 m/s is
// outrun at once - so it waits for cruise speed: 270 m/s (the note's P1 pass bar; the runners coast at 297 -> 283 m/s,
// d10_r2_ashcroft_ek.jpg; HDE holds 300 -> 287 m/s over 6.8 s, M13-ab12-b_i1 big_coast_own, FREELANCER_RE.md)
// 295 since probe sc3 (run-judge): from 270 the coast began before our lock against his 300 - he opened
// 34-48 m/s and the coast ended at 540 m after 10.2 s; the runners kill the engine at his top speed, ~296-297
constexpr float kChargeCoastSpeed = 295;
// The charge's speed profile (the note's design, "Pure half": his - 75 m/s for ~5.5 s, the ramp to 300 m/s at ~9.5 s
// (LOOK probe: 75 m/s at 0.8 s, 294 m/s at 9 s, LOOK_PLAN 138-146); ours - the engine's 80 m/s for 5 s, 300 m/s at 8.5 s
// (FREELANCER_RE.md 467-476; FLMotion as2-as4: 7.5-8.5 s)); linear in the ramp.
struct ChargeProfile
{
    float slow = 0, slowS = 0, top = 0, topAtS = 0;
};
constexpr ChargeProfile kHisCharge = {75, 5.5f, 300, 9.5f};
constexpr ChargeProfile kOurCharge = {80, 5.0f, 300, 8.5f};
float ChargeDist(const ChargeProfile& p, float t); // metres flown t s after the press (0 before it)
struct GapPrediction
{
    float lockGap = 0; // the gap when both are at cruise speed (m)
    float lockT = 0;   // ...that many s after his spawn
    float minGap = 0;  // the least gap on the way (m): under ~50 a ram / pass risk
};
// gap0: his distance at his spawn along his line (we behind him); ourLagS: our press after his start (s). Before our
// press we are counted at rest (the stage holds still or fights near P).
GapPrediction ChargeGap(float gap0, float ourLagS);
// The last escort fought from P (design 2): within kChargeKeep of P the fight runs free; beyond it the pursuit keeps
// station (nose and guns on the escort) at a point kChargeKeepAt from P toward the escort until back within
// kChargeKeepIn - no chasing it away from P.
constexpr float kChargeKeep = 400;   // the note's "within ~400 m of P"
// a charge stage's trip ends this close to P (spawn_stage's kStageArrive 1500 let probe sc1 "arrive" 1305 m short and
// press cruise from 493 m at 26 m/s - run-judge 31 Oct 2026; the point's 250 m behind the spawn is the design)
constexpr float kChargeArrive = 150;
// beyond this from the keep's station point it is flown to at full throttle (the station controller crawled at 24-33 m/s
// for 24 s in sc1), within it the station holds
constexpr float kChargeKeepFly = 300;
constexpr float kChargeKeepIn = 250; // hysteresis: the station holds until this close (a choice: no flip every frame)
constexpr float kChargeKeepAt = 200; // the station point: half the keep radius toward the escort (a choice)
struct KeepResult
{
    bool station = false;
    float pos[3] = {};
    float dP = 0; // our distance from P
};
KeepResult ChargeKeep(const float me[3], const float p[3], const float target[3], bool wasOn);

// ---- CHASE (switch `chase_kill`): an armed later-step kill ship that is alive, hostile and targetable now is fought now
// (LookTarget's kill, src=armed: M02 "Follow Ashcroft" - today a 25 km navmarker trip behind him). Against a target
// faster than our engine's top speed the fight cruises after him at once and opens fire by engine-kill coasts at cruise
// speed (the runners: 297 -> 283 m/s firing at ~120-150 m, frame d10_r2_ashcroft_ek.jpg), the engine back on and the
// cruise again when the gap opens. The step per frame:
enum class ChaseVerdict
{
    Fight,   // not a chase (he is no faster than our engine, or gone): the fight as before
    Charge,  // cruise after him now (no 3 km floor, no drop at gun range)
    Coast,   // kill the engine / keep coasting: guns on him at cruise speed
    EngineOn // end the coast: the engine back on, then the cruise again (no brake)
};
const char* ChaseVerdictName(ChaseVerdict v);
struct ChaseFacts
{
    float gap = 0;        // his distance (m)
    float bearingDeg = 0; // his angle off our nose
    float mySpeed = 0, hisSpeed = 0, engineTop = 0, gunRange = 0;
    bool cruising = false; // our cruise engaged
    bool coasting = false; // our engine killed by the chase's coast
    bool killed = false;   // he is dead / gone
    float shield = -1;     // ours 0..1, -1 unknown
    // `spawn_charge`: he just spawned and runs for his run point (the staged ship, StillFleeing): the charge starts at once
    // whatever his speed (not only once he outruns our engine: that waited 2-5.4 s, the 1.4 km gap - consensus "What the
    // numbers say" 1), and the coast starts only at kChargeCoastSpeed
    bool fleeing = false;
};
struct ChaseResult
{
    ChaseVerdict verdict = ChaseVerdict::Fight;
    const char* why = ""; // EngineOn: "gap" / "shield" / "gone"; Coast at its start: "start"
};
constexpr float kChaseCoastMin = 110, kChaseCoastMax = 250; // the gap the runners coast at (fights.tsv: 110-230 m)
constexpr float kChaseBearingDeg = 30;   // he is ahead within this of the nose
constexpr float kChaseCoastSpeed = 150;  // our speed at least this (CoastGuard's: a coast needs cruise speed)
constexpr float kChaseGunFrac = 0.9f;    // the coast ends beyond this x gun range (big_coast's weapons range)
constexpr float kChaseShieldOut = 0.15f; // ...or our shield under this (big_coast's "almost down")
constexpr float kChaseNoMissileHull = 0.5f; // no missile at him under this hull (HDE's full splash; 22355: a round hitting
                                            // the ejected pod fails the mission)
ChaseResult ChaseStep(const ChaseFacts& f);

// Mission goals as facts (the script's part; the adapter fills what the game knows)
KillGoal KillGoalOf(const mission::Goal& g, const char* source);
FlyGoal FlyGoalOf(const mission::Goal& g, const char* source);
} // namespace lookahead
