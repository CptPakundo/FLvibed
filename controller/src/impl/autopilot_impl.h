// Advanced Autopilot: the shared state and declarations of its parts (autopilot.cpp = core: modes, start / stop,
// globals; autopilot_objective.cpp, _behaviours.cpp, _target.cpp, _combat.cpp, _dock.cpp, _geo.cpp (the geometry
// planners), _hold.cpp (the Hold mode), _travel.cpp, _frame.cpp, _settings.cpp). Split from one 5,700-line
// autopilot.cpp on 21 Oct 2026 along its section markers (_geo and _hold split out on 24 Oct 2026); the
// behaviour is unchanged. Everything here is internal to the autopilot (the module API is autopilot.h).
#pragma once
#include "autopilot.h"

#include "../events.h"
#include "../fl.h"
#include "aim.h"
#include "autopilot_util.h"
#include "game/ai.h"
#include "game/avoid.h"
#include "game/flight.h"
#include "game/geom.h"
#include "game/mission.h"
#include "game/route.h"
#include "game/space.h"
#include "host.h"
#include "log.h"
#include <deque>
#include <map>
#include <set>
#include <sstream>

namespace autopilot
{

enum class Tactic
{
    Engage, // trail the target (the game's Trail behaviour at the personality's distance)
    Evade,  // dodge: the game's Evade behaviour (the personality's dodge styles); slide bursts when it is refused
    Break,  // break off: the game's goto beside and behind the attack axis with afterburner
    Flee,   // leave the fight
    Seek,   // no target: look for the next enemy
    Rejoin, // escort: back to the escorted ship
    Loot,   // the fight is over: collect the loot nearby (the game's Loot behaviour)
    Extend, // under sustained fire: afterburner away from the attackers, jinking, until the hits stop (the player's way)
};

// The objective mode's sub-action (ObjectiveStart; a char[16] string until 24 Oct 2026). ActionName gives the old
// literal: events, FLMotion.tsv's action column and `ap status` print the same text. None = "-" (no action yet).
enum class Action
{
    None,
    Leash,
    Airlock,
    Jump,
    ToMarks,
    ScriptKill,
    Dock,
    ScriptGoal,
    PreStage,
    PreAggro,
    Survive,
    Escort,
    Fight,
    Formation,
    Follow,
    LaneWait,
    Lane,
    SurviveHold,
    SurviveStay,
    MissionForm,
    Pass,
    Rest,
    Route,
    Arrive,
    Via,
    Navmarker,
    DockWait,
    LeadStall,
};
const char* ActionName(Action a);

// Why a mode stopped (Stop / StopImpl; a string protocol until 24 Oct 2026). StopText gives the old literal, which the
// stop event and the log print; a reason formatted with numbers (Hostile) carries its text beside it. Other: a text from
// the module API (autopilot::Stop(const char*)) that is none of these.
enum class StopReason
{
    None, // "" (no stop yet / consumed)
    Restart,
    Command,
    PlayerInput,
    Pad,
    PlayerDied,
    Docked,
    Docking,
    LeftSpace,
    MissionControls,
    GameDocks,
    ManeuverEnded,
    LeaderGone,
    EscortedGone,
    TargetDisabled,
    TargetDestroyed,
    NoEnemiesLeft,
    Fled,
    Yield,
    Arrived,
    PodsDone,
    TooManyReplans,
    NoBestPath,
    NoWay,
    NoPlan,
    Hostile,
    DockRefused,
    Other,
};
const char* StopText(StopReason r);
StopReason StopReasonOf(const char* text); // the reason whose StopText this is, else Other

// hold: 0 run to the point (our steering), 1 stopping (the game's Idle), 2 turning the nose, 3 resting
enum class HoldPhase
{
    Run = 0,
    Stopping = 1,
    Turning = 2,
    Resting = 3,
};
// the flee's phase: 0 break contact, 1 charge the cruise engine, 2 cruising away
enum class RetreatPhase
{
    Break = 0,
    Charge = 1,
    Cruise = 2,
};
// attack passes at a static target (StaticPass): 0 in, 1 firing window, 2 out to passExit
enum class PassPhase
{
    In = 0,
    Firing = 1,
    Out = 2,
};
// the slash run: 0 the pass, 1 the extend
enum class SlashPhase
{
    Pass = 0,
    Extend = 1,
};
// The fast dock's kind of entrance (FastApproach; three bools fastRing / fastAirlock / fastLane until 24 Oct 2026 -
// never more than one set): Bay a station's bay, Ring a planet's docking ring (type 6: straight in, early request),
// Airlock an airlock (type 8, airlock_fly_through: flown through like a player, no dock request), Lane a lane ring
// (bayP2 = where the ride starts, bayAxis = against the lane). None: no entrance picked yet.
enum class DockKind
{
    None,
    Bay,
    Ring,
    Airlock,
    Lane,
};

struct Settings
{
    std::string pilot = "pilot_military_hard";    // the game's [Pilot] whose personality the autopilot flies by
    std::map<std::string, std::string> overrides; // personality fields by ini name ("p.trail_distance = 200")
    int engage = 1;          // combat: 0 = only the chosen target, 1 = then the closest enemy until none is left
    bool autoFire = true;    // fire the guns (gimbal aim)
    bool missiles = true;    // launch missiles by the personality's interval
    int countermeasures = 1; // 0 never; 1 cruise disruptors outside a fight + missiles whose damage matters; 2 every
                             // missile homing on us (see Countermeasures)
    float cmDistance = 700;  // drop CMs once a missile homing on us is this close (the CM's range is 1000 m)
    // a reference flight's line through a structure followed where one exists (RefLineAim: speedrun knowledge, the user's
    // M12 try 3 through the core, rotated to this run's turbine crossing); 0 = the rejoin / passage rules
    bool refLine = false;
    // a destroy goal only the last objective stands in for ("Awaiting"), none of whose objects the mission marks yet, is
    // not flown to (a later step's: M09's generators at the Tohoku entry, session 11); 0 = flown to. On by default since
    // session 11 (M09 6 / 7 PASS vs 0 in five gates; M13's city unaffected; the gate after it)
    bool staleDestroyWait = true;
    float cmShieldFraction = 0.5f; // in a fight: counter only when what homes on us would cost this much of the shield
    float dodgeWindow = 0.6f;    // no countermeasure possible: dodge by slide + afterburner this long before impact (s)
                                 // (16-run benchmark: 0.6 s 0 deaths, 3.78 kills/min vs no dodge 1 death, 3.01)
    bool evade = true;           // dodge and break by the personality (off: pure pursuit)
    int dodge = 1;               // how to dodge: 1 = slide bursts while the nose stays on the target (keeps tracking),
                                 // 2 = the game's Evade behaviour (NPC style: turns away, loses the aim for 2-3 s)
    float fireCone = 0.5f;       // fire while the lead cross is within this offset from the screen centre (half-screen)
    float followDistance = 1500; // follow / escort: the game's follow max distance (m)
    float followOffset[3] = {0, 50, 250}; // follow / escort: offset from the leader (x right, y up, z behind)
    float escortRadius = 2500;            // escort: fight hostiles within this distance
    float holdRadius = 300;    // hold: the leash - farther from the hold point, fly back (missions' StayInRange)
    float flee = -1;           // leave combat below this hull fraction (-1 = the personality's flee value)
    int steer = 0;             // combat steering: 0 = lead pursuit (most lethal), 1 = the game's Trail (NPC style)
    bool bursts = false;       // fire in the personality's bursts (NPC style) instead of whenever on target
    float engageDistance = 0;  // pursuit close distance (m); 0 = by the target's size (fighters ~45-110 m)
    float engageRadius = 3000; // pick hostiles within this distance (escort: of the escorted ship)
    bool bigShips = false;     // also pick gunboats, cruisers, capital ships and transports while fighters are around
    int caution = 1;           // 0 = never leave a fight early; 1 = leave when clearly outmatched and the hull is below
                               // 60 %; 2 = leave sooner (outmatched at even odds, hull below 80 %)
    float retreatOdds = 0.05f; // free roam: leave a fight whose measured race (FightOdds) is lost this badly (0.05 =
                               // 20:1; the loadout prior alone at 0.4 x it; also the outmatched line); 0 = off
    int repair = 1; // nanobots / shield batteries: 0 never; 1 only when needed (the player's way: a battery when
                    // the shield is low and still falling, nanobots when the hull is low and being hit, or
                    // critical); 2 at the personality's damage thresholds (NPC style)
    float repairShieldAt = 0.25f; // repair 1: cap - never a battery above this shield fraction
    float batteryHorizon = 0.6f;  // repair 1: battery when the shield left is below the loss rate x this (s) or the
                                  // largest recent single hit
    float repairHullAt = 0.6f;    // repair 1: nanobots below this hull fraction while being hit (always below 0.35)
    bool loot = true;         // collect loot near the fight once no enemy is left (the game's Loot behaviour): what the
                              // hold takes, the most valuable first
    float lootMinValue = 0;   // leave loot worth less than this (credits at the base price, goods.ini)
    bool extend = false;      // extend under sustained fire (afterburner away, jinking) and resume when the hits stop
                              // (off: 16-run benchmark 17 Oct 2026 - kills/min 2.47 vs 4.10, on target 58 vs 80 %,
                              // no less damage per kill; needs a smarter trigger)
    float extendQuiet = 1.5f; // extend: resume after this long without a hit (s)
    float extendMax = 8.0f;   // extend: at most this long (s)
    float extendJink = 40.0f; // extend: jink offset from straight away (degrees)
    float extendPatience = 3.0f; // extend: still hit after this long = futile, fight instead (s)
    bool stagger = true;      // staggered gun fire (the guns started one by one over their refire cycle, then fire-all;
                              // benchmark: dps 85 vs 82, lost per kill 0.21 vs 0.28 - small samples, user: benefit of
                              // the doubt)
    float evadeScale = 0.06f; // evasion: this fraction of shield + hull points lost in 2 s = full evasion
    float weaveRadius = 0.75f; // evasion: corkscrew radius at full evasion, as a fraction of fire_cone (tuned 17 Oct
                               // 2026, 43 runs: 0.75 = 0.13 ships lost per kill vs 0.20 at 0.6, same kill rate)
    float slideTime = 0.5f;    // evasion: length of each slide burst (s); 0 = no slide bursts (weave only)
    bool turnEvade = true;     // a turn onto a target far off the nose while attacked is itself a dodge: slide bursts,
                               // afterburner (reverse when the target is behind and close) - see UpdateCombat
    bool blindSpot = true; // a big target with turrets is fought from a spot its guns cannot reach (CEGun::CanPointAt)
                           // when one is in our gun range - see BlindSpot in autopilot_combat.cpp
    bool adaptive = true;  // ship-adaptive duel: close turn / wide turn / slash by agility and the measured exchange
                           // (DuelChoose); 0 = the close turn always (the pre-21 Oct 2026 pursuit)
    int duel = -1;         // force a duel option (A/B measurement): 0 close turn, 1 wide turn, 2 slash; -1 adaptive
    bool skipDockScene = true; // the autopilot's own dock: its docking / landing cutscene skipped (Escape) - OwnDock
    // The plain start (K, the bar button, View held - every device alike; the owner, 26 Oct 2026): 1 = the game's live
    // state first - an objective -> the objective mode, else the waypoint list -> travel along it, else the target;
    // 0 = the target alone (the start before 26 Oct 2026). StartSmart, autopilot.cpp.
    int startSmart = 1;
    // Fast dock (user, 18 Oct 2026: the player's shortcut when time or survival matter): fly player-style - full
    // throttle, afterburner when lined up, braking late - to the outer approach point (p2) of the bay the game will
    // assign, and request the dock just before it, so the docking cutscene starts at once instead of after the game's
    // slow approach (and queue). Geometry from the game (space::TargetDockBays).
    bool fastDock = false;
    // a fast dock's request dropped when the distance to the target keeps growing 4 s after it (the game's dock losing
    // a moving target: M06's Hood, session 11) - the approach lines up again; 0 = the game's dock as it goes (off until
    // tested: the game's dock may swing away from a big station's centre to a far bay)
    bool dockRegain = false;
    // Fast dock: where to request the dock, along the bay's axis from p2 (m; 0 = at p2, negative = past it towards the
    // mount). Only the position at the request matters (user, 18 Oct 2026): requested earlier, the game's dock takes
    // the ship and slows it (Fort Bush at full afterburner: 120 m out 4.2 s to the cutscene, 40 m 0.8 s, 5 m 0.4 s).
    float dockRequestDistance = 5;
    float dockArriveSpeed = 0; // fast dock: brake to this speed before the request point (m/s); 0 = no braking
    // fast dock at a planet's docking ring, straight in: the request this far from p2 (m) at full speed,
    // as at stations; 0 = the request ~1.4 s / at least 400 m out (the game's dock then flies the rest: 6.6-7.0 s from
    // the request in 80+ archived landings, whatever the speed or angle; the top-3 runners reach the ring at 299 m/s,
    // HUD up, the landing scene at the ring - M12 Toledo, 24 Oct 2026)
    float ringRequest = 0;
    // fast dock at an airlock (dock type 8): flown through like a player, no dock request (the game's Dock maneuver
    // there leaves the player locked out and letterboxed - FastApproach). 0 = request as at a gate.
    bool airlockFlyThrough = true;
    // an airlock run's cruise charge timed so this share of it is done at the transit (the user, 24 Oct 2026: 0.5), kept
    // through the turn-in once begun; 0 = the charge 0.5 s before the transit (4c133b7)
    float airlockChargeAt = 0;
    // an airlock run with the nose off its aim slows only to the speed whose turning circle fits the turn left; 0 = to
    // 120 m/s within 1.2 km of the door whenever the nose is 45 deg+ off
    bool airlockBurn = false;
    // a trip waiting for the mission's waypoint list (WaitForMissionList, up to 1.5 s) flies on at its destination
    // meanwhile; 0 = no steering while waiting (the ship coasts)
    bool waitFlies = false;
    // a fly-through's rejoin of its line under the cruise lock aims at the nearest point the turning circle reaches
    // (then farther ones); 0 = the farthest point in plain sight, 1.5 km at most
    bool rejoinNearest = false;
    // a hop's started cruise charge is kept unless the afterburner wins by more than 1 s; 0 = re-decided every frame
    bool hopCruiseHold = false;
    // marked loot tractored in passing under the cruise lock: the trip's own airlock (the exit) is the way on at once;
    // 0 = the trip goes on until the pickup's next step
    bool tractorExitRun = false;
    float tractorExitRange = 600; // ...only within this of the airlock door's outer point (m)
    // no explosive round while another solid lies beside the ship within the round's blast reach (rays across and 45 deg
    // forward of the nose; M09 s11o9-i2: a torpedo burst on the generator's rock 15 m beside us - dead at full hull)
    bool blastFlank = true;
    // pre-aggro (speedrun knowledge, source: the mission script - IDEAS A6): the mission ships the step's script is about
    // to turn hostile (mission::PendingHostileShips: exact ids by spawn position), neutral within this range (m), count
    // as hostile and are fought; 0 = off
    float preAggro = 0;
    // a passage beside a too-narrow gap looks for the opening's centre first (a grid in the obstruction's plane, the most
    // room round it); 0 = the 8 directions at 50-120 m only
    bool passageWindow = false;
    // a hop's charge reset under fire burns at once for 8 s; 0 = only after 8 s without completing
    bool hopResetBurn = false;
    // an arrival with no clear line and no way found yet climbs out (45 / 30 / 60 deg toward the destination's heading)
    // along a line clear for 400 m until the planner finds its way; 0 = straight at the point meanwhile
    bool nowayClimb = false;
    bool wheelTiming = true; // a rotating solar part on the way is crossed through a timed gap (avoid::WheelScan)
    // a static target (generator, platform with < 4 guns) is fought on an orbit round it, never from a hover (user, 22 Oct
    // 2026: "M12 had the player ship HOLD STILL as it was shooting target generators"): StaticOrbit
    bool staticOrbit = true;
    // ...by attack passes (the fallback of the hold; the orbit is the last): in fast, a short firing window close to the
    // hull, out to a covered exit, again - the 2020 M13 generators (StaticPass)
    bool staticPass = true;
    // ...by hold-and-salvo (default; the top-3 speedruns, every generator / satellite / platform in all three runs: in
    // fast, brake short, hold still 30-80 m off the hull firing everything, leave on the kill - StaticHold); hull lost at
    // the hold hands over to the passes for a while
    bool staticHold = true;
    // the hold point's distance from the target's real hull (m). The user (22 Oct 2026, speedrun #1's M12 / M13
    // generators) saw "a near-contact-but-not-exactly-contact holding position" at HUD ~60 m; the HUD distance is to the
    // SURFACE (0-M against a cruiser's hull, run 1 v7970), so the runner held 58-100 m off the surface, drifting out
    // 1-2 m/s until each kill (reference\calib\CALIBRATION.md #4) - the screen-filling look is the generators' size
    // Session 7 (user, 23 Oct 2026, M12): "a pure lateral run along the hull is even better than what I did" (try 3:
    // ~40 m/s at gaps of ~0-70 m, reverse taps only for the pad layout's convenience) - near the hull, sliding along it,
    // mines dropped on the way: 20 m (was 60, the runners' HUD reading, CALIBRATION #4)
    float staticHoldGap = 20;
    // ...slid round the target at this rate (deg/s), one way per hold, the slide thrusters doing it while the nose and
    // guns stay on (the user: "light orbit movements"; measured in run 1 / M09 / M13: ~5 deg/s, no reversals); 0 = still
    float staticHoldSlide = 5;
    // ...or, when > 0, at this speed along the hull (m/s; the lateral run - session 7), reversing where geometry blocks it
    float staticHoldLateral = 35;
    // drop mines at the hold every 1.5 s (the user, session 5: "all weapons, all missiles, all mines" - mines add DPS on a
    // generator; session 7, 23 Oct 2026: kept on - "the problem was the ship was not firing its weapons and was
    // ill-positioned to have its mines hit the generator": the hold's position must suit the mines too, open)
    bool staticHoldMines = true;
    // ...mines only within this hull gap (m; session 11: the hold keeps the torpedoes' blast need + 6 m, ~50 m at M12, so
    // at 40 no mine ever dropped - mine01: seek_dist 400, owner_safe_time 4 s, blast 16 m, 326 hull)
    float staticHoldMineGap = 40;
    bool staticHop = true; // the way to a static target's hold through real geometry, by cruise when faster (HopFly)
    float targetDwell = 6; // a live combat target is kept at least this long before the ranking may switch (s)
    // A step's flying condition from the mission script (mission::AdvanceGoals; Source::Hidden): travel there instead of
    // fighting or flying to a waypoint that does not advance the step - "objectives are triggers" (IDEAS A21)
    bool scriptGoals = true;
    // Knowledge scope (user, 22 Oct 2026; AUTOPILOT_UX.md section 2a): 1 = everything (hidden game data, the speedrun
    // preset; the default until the UI build of 25 Oct 2026 - the legs apply `ap preset speedrun`), 0 = what the player
    // was exposed to (seen in play, plus general skill; the COMPETENT preset and the spec's default, UI_SPEC.json
    // `knowledge`): behaviours that need hidden data (Source::Hidden) or community tricks (Source::Meta) stay off.
    int knowledge = 0;
    // close-quarter flight over every mode: the way ahead tested against real hulls (GeometryGuard, avoid::RayFan)
    bool geometryGuard = true;
    bool motionTrace = true; // FLMotion.tsv: 10 rows / s of the flight while the autopilot flies (autopilot_frame.cpp)
    // blind spots that also shelter us: a spot the other hostiles have no clear line to wins among those the target's
    // turrets cannot reach (user: the Rheinland battleship's bow trench) - BlindSpot
    bool blindShelter = true;
    // (a dock in a hurry needs none)
    float dockStage = 250; // fast dock: coming from the side or behind, line up this far out on the bay's axis
    // Fast lane entry: request when the game's stop would leave the ship within this distance of the aim point next to
    // the lane's start (m); the ride starts once the ship rests within ~15 m of the start.
    float laneRequestDistance = 15;
    int fastThrust = 0;  // fast dock / lane run: 0 as planned, 1 burner, 2 cruise (verification runs)
    u32 laneToward = 0;  // fast lane entry: the ring the ride should head to (0 = by the ship's side)
    bool avoid = true;   // the autopilot's own obstacle avoidance on its runs (core\game\avoid)
    float disableAt = 0; // combat on a chosen target: stop once its hull is at or below this (0 = destroy)
};

// A missile launch's geometry (MissileGeometry): the target's speed, turn rate, strafe, aspect (deg between its
// velocity and the line from it to us: 0 coming at us, 180 running away), crossing speed, and the escape ratio
struct MissileGeom
{
    float tSpeed = -1, tTurn = 0, tStrafe = 0, aspect = 0, crossing = 0, escape = -1;
    u32 tArch = 0; // the target's ship type (the run-in's per-type gate)
};

// A way through real geometry (avoid::RayPath) being flown: its points (x, y, z, ...; the destination last), the one
// steered at, when it was planned and the next point last looked for.
struct GeoPath
{
    std::vector<float> pts;
    float dest[3] = {};
    float start[3] = {}; // where the ship was at the plan (the first leg's start)
    size_t idx = 0;      // the leg flown: start/pts[idx-1] -> pts[idx]
    float plannedAt = -10, sightAt = -10;
    float skipAt = -10;   // the last test whether the next corner is in plain sight (a leg end skipped, 4x a second)
    bool arc = false;     // the way is an arc round a structure (ArcAround): no corner skips, no centring
    float elbowAt = -10;  // the last cheap elbow-only try while no way is known (each second)
    float centre[3] = {}; // the centring offset applied to the carrot (smoothed)
    bool planesFirst = false;       // RayPath's planesFirst (the hops: no 3D lattice)
    Progress bestRest = {1e30f, 0}; // the least way left so far and when (no progress -> a new plan)
};

struct State
{
    // Fields grouped by subsystem (24 Oct 2026, audit step 6): the nested groups below; what several subsystems share
    // stays here. A group drops its own name's prefix from its fields (g.hold.pos was g.holdPos).
    Mode mode = Mode::Off;
    Tactic tactic = Tactic::Engage;
    u32 target = 0;   // combat target / goto / dock / follow / formation target
    u32 escorted = 0; // escort: the ship we protect
    bool hasPos = false;
    float pos[3] = {};
    float tacticTime = 0; // seconds in the current tactic
    float time = 0;
    float tacticWindow = 0; // loop guard: start of the current 1 s window, changes in it, loop reported
    int tacticChanges = 0;
    bool tacticLoop = false;
    float issueTimer = 0;   // follow: polling interval of the leader check
    bool started = false;   // travel modes: the maneuver has started (op seen)
    float tractorTimer = 0; // next tractor-all (the fight's loot and travel's destination)
    // Game state instead of timers (user, 18 Oct 2026): frames run, the frame of our last submitted behaviour (the game
    // has acted on it once it ran kFramesToAct more frames), and the assessment count (each Assess: combat.assessments)
    // - the fight is over when an assessment taken after the last kill or loss of the target shows nothing to fight.
    u32 frames = 0, submitFrame = 0;

    // The fight: target bookkeeping, evasion, dodge, extend, retreat, the duel, the target's shield, guns' line of
    // fire, the big-target tactics (flank, blind spot, orbit), the stall
    struct Combat
    {
        int oddsLogged = 0;     // combat: the fight's odds (FightOdds) reported - 1 the prior, 2 measured
        float outgunnedFor = 0; // combat: seconds the odds have been below retreat_odds without a break
        float evadeCooldown = 0, breakCooldown = 0, seekTimer = 0, burst = 0;
        bool lostEnemy = false; // no enemy selected since the last frame that had one (the fight may be over)
        u32 trailTarget = 0;    // the target the running Trail was issued for
        bool gameEvade = false; // the dodge runs the game's Evade behaviour (else slide bursts)
        int dodgeDir = 0;       // current slide evade direction (Out::slide)
        float dodgeTimer = 0;
        std::deque<std::pair<float, float>> hull; // (time, hull) over the last seconds
        u32 kills = 0;
        bool chosen = false;    // the player chose the target (start with a target): kept until destroyed
        float pickTimer = 0;    // next target evaluation
        float outOfRange = 0;   // seconds the target has been out of gun range
        u32 progressTarget = 0; // no-progress rule: the target watched, its hull and our on-target time then
        float progressHull = 0, progressOnTarget = 0;
        std::map<u32, float> ignored;                     // given-up targets -> time they may be picked again
        float hullRepairTimer = 0, shieldRepairTimer = 0; // the personality's repair delays
        int nanobotsBefore = -1, batteriesBefore = -1;    // counts when the last repair was used (did it work?)
        int nanobotsMissed = 0, batteriesMissed = 0;      // presses in a row that used nothing
        u32 lootTarget = 0;                               // loot being collected (one item at a time: hold nearly full)
        float lootTime = 0;                               // seconds collecting
        std::deque<std::pair<float, float>> hp;           // (time, shield + hull points) over the last seconds
        float evasion = 0;                                // 0..1: how much the pursuit evades (from the damage taken)
        float onTargetInReach = 0;                        // on target within gun reach (the no-progress rule's clock)
        float lost = 0;    // shield + hull points lost during the run, in units of our total (1 = a full ship's worth)
        float totalHp = 0; // our shield + hull points at full (last read)
        // Extend: seconds of sustained evasion, seconds since the last hit, cooldown, jink (direction offset) state;
        // (statistics: stats.extends, stats.extendTime)
        float hotTime = 0, quiet = 0, extendCooldown = 0, jinkTimer = 0, jinkPhi = 0;
        // the flee's phase: 0 break contact, 1 charge the cruise engine, 2 cruising away
        RetreatPhase retreat = RetreatPhase::Break;
        float retreatPhaseTime = 0;
        float lastNearest = -1,
              nearestRate = 0; // the nearest hostile's distance last frame, its rate (m/s, > 0 opening)
        // Retreat from fixed guns after a fight (see UpdateCombat): where it began (bounded: never far from the fight).
        bool gunRetreat = false;
        float gunRetreatFrom[3] = {};
        // Turn maneuver (turn_evade): running, its slide timer, since when; (statistics: stats.turns, stats.turnTime)
        bool turning = false;
        float turnSlideTimer = 0, turnStart = 0, turnCooldown = 0;
        float dealt = 0, dealtHp = 0; // hull points taken off our targets; the current target's points last frame
        u32 dealtTarget = 0;
        // The duel's geometry, sampled at 10 Hz while an enemy is engaged (the tactics model's data): seconds sampled, with
        // its nose on us (within 15 deg, inside 900 m: its guns bear), ours on it, both; the turn-rate ratio (ours / its)
        float duelT = 0, duelTheyOn = 0, duelWeOn = 0, duelBoth = 0, duelTurnRatio = 0, duelSampleAt = 0;
        // The duel option (DuelChoose): 0 close turn, 1 wide turn, 2 slash; since when, what each option measured this fight
        // (seconds, target points taken as a fraction of its total, ours lost as a fraction of ours), the slash's phase
        int duelOpt = -1;
        float duelOptSince = 0, duelDecideAt = 0, optT[3] = {}, optDealt[3] = {}, optLost[3] = {};
        float duelDealtSeen = -1, duelLostSeen = 0;
        unsigned long long duelKey = 0;
        SlashPhase slashPhase = SlashPhase::Pass; // 0 the pass, 1 the extend
        float slashSince = 0;
        // The target's shield (the burst's data): when it was predicted to fail (first prediction under 2 s), when it went
        // down, its state last frame
        u32 shTarget = 0;
        float shPredAt = -1, shPredFor = -1, shDownAt = -1;
        bool shWasUp = true;
        bool wantFire = false;              // last frame's out.fire
        float lineCheckAt = -10;            // guns' line-of-fire check: last time (g.time)
        bool lineBlocked = false;           // ... a friendly / neutral ship is in it
        u32 lineBlocker = 0;                // ... the solar last found on the line (logged once per change)
        u32 assessments = 0, quietFrom = 0; // quietFrom: the first assessment that counts for "the fight is over"
        bool flanking = false;              // out of a big target's bow arc (its main guns): running to its flank
        // a big target's turret blind spot (BlindSpot): held in its own frame while the guns work; cover = its guns that
        // reach the spot (CEGun::CanPointAt), re-read every second
        bool blindOn = false;
        u32 blindTarget = 0;
        float blindLocal[3] = {}, blindNext = 0;
        int blindCover = -1, blindGuns = 0;
        float blindLostAt = 0; // g.combat.lost at the last reading: what a held spot costs per second
        int blindHits = 0;     // readings in a row at the spot with hull / shield lost
        float targetSince = 0; // when the combat target was last switched (target_dwell)
        // the orbit round a static target (StaticOrbit): its plane (radial r0 at the start, tangent t0), the angle, the
        // direction and when it flips next
        u32 orbitTarget = 0;
        float orbitR0[3] = {}, orbitT0[3] = {}, orbitAngle = 0, orbitDir = 1, orbitFlipAt = 0;
        // combat stall: where we were and what we had dealt at the last check; a break-out run until breakUntil
        float stallAt = -1, stallPos[3] = {}, stallDealt = 0, breakUntil = -1, breakPos[3] = {};
        bool approaching = false; // combat: cruising in to a far target (fight entry)
    } combat;

    // Statistics (status, stop event)
    struct Stats
    {
        // Combat statistics (status, stop event): seconds engaged / with the lead in the fire cone / firing, distance
        // integral (average = distTime / engaged).
        float engaged = 0, onTarget = 0, firing = 0, distTime = 0;
        // Evasion statistics: seconds under fire (points lost in the last 2 s), seconds evading (evasion > 0.05) and of
        // those without hits (the fade), seconds evading with the lead in the fire cone; last pressure / shield (status).
        float hitTime = 0, evadeTime = 0, evadeNoHit = 0, evadeOnTarget = 0;
        float pressure = 0, shield = -1, lossRate = 0, maxHit = 0;
        // Extend statistics: extends started, seconds extending.
        int extends = 0;
        float extendTime = 0;
        // Turn maneuver statistics: turns, seconds turning.
        int turns = 0;
        float turnTime = 0;
        // Gun statistics, aggregated (no event per shot: the guns fire many times a second): shots (a gun's refire timer
        // restarting), the shots possible while we wanted fire (sum of 1 / refire), the damage the shots carried.
        int gunShots = 0;
        float gunMaxShots = 0, gunShotDamage = 0;
        std::map<u16, float> gunElapsed;
    } stats;

    // Missile outcome statistics: munitions homing on us (id -> damage, our points last seen), the ones gone awaiting
    // their verdict, the ones we dodged (missiles.watch / gone / dodged).
    struct Gone
    {
        float time, damage, points;
        u32 id;
    };
    // Our missiles in flight, for their outcome (hit / miss with the launch conditions: the planner's feedback).
    struct OwnMissile
    {
        u32 target = 0;
        float launched = 0, targetHp = -1, damage = 0, blast = 0, closest = 1e9f, launchDist = 0, flightPred = 0,
              closing = 0;
        bool planned = false; // launched by the planner (else: some other source - fire-all, the player)
        MissileGeom geom;     // at the launch
    };
    struct OwnGone
    {
        u32 id;
        OwnMissile m;
        float gone;
    };
    // Missiles and countermeasures: incoming munitions (dodge, countermeasures, outcomes), our launches (the planner)
    struct Missiles
    {
        float timer = 0, cmTimer = 0;
        std::map<u32, int> cmDrops; // countermeasures dropped per incoming munition
        u32 dodgeMissile = 0;       // the munition being dodged by maneuver
        int slide = 0;              // its slide direction
        // Missile outcome statistics: munitions homing on us (id -> damage, our points last seen), the ones gone awaiting
        // their verdict, the ones we dodged.
        std::map<u32, std::pair<float, float>> watch;
        std::vector<Gone> gone;
        std::set<u32> dodged;
        int hits = 0, misses = 0;
        int ammo = -1; // homing rounds left last frame (-1 unread)
        float run = 0; // seconds into a missile run (closing in for a short flight), 0 none
        // Our missiles in flight, for their outcome (hit / miss with the launch conditions: the planner's feedback).
        std::map<u32, OwnMissile> ownMissiles;
        std::vector<OwnGone> ownGone;
        float lastLaunchDist = 0, lastLaunchFlight = 0, lastLaunchClosing = 0;
        MissileGeom lastLaunchGeom;
        float prevFlight = 0,
              prevFlightAt = -1; // the run-in: last frame's predicted flight (is the run still shortening it?)
        int ownMissileHits = 0, ownMissileMisses = 0;
        float ownMissileDealt = 0;
        float runSeen = -10;  // when the run's conditions last held (g.time)
        u32 tapFrame = 0;     // the frame our last missile tap was asked
        u32 shieldTarget = 0; // the target whose shield is sampled (missile planner: shield down at impact)
        std::deque<std::pair<float, float>> shieldHist; // (time, shield points)
    } missiles;

    // attack passes at a static target (StaticPass): 0 in, 1 firing window, 2 out to pass.exit; since when
    struct Pass
    {
        u32 target = 0;
        PassPhase phase = PassPhase::In;
        float since = 0, exit[3] = {};
    } pass;

    // The Hold mode (_hold.cpp) and the hold-and-salvo at a static target (StaticHold)
    struct Hold
    {
        // hold: 0 run to the point (our steering), 1 stopping (the game's Idle), 2 turning the nose, 3 resting
        HoldPhase phase = HoldPhase::Run;
        bool scenicTried = false; // hold: the scenic point beside the last fight's subject was considered
        bool scenic = false;      // hold: may move to a scenic point (only the rest after a fight - not a wait between
                                  // actions: M01b's first hold flew beside Epsilon 1 just before "Enter formation")
        float look[3] = {};       // hold: where the nose points
        char lookSrc = 0;         // hold: why (HoldLook), 0 = nowhere in particular
        float pick = -1;          // hold: when the look point was last chosen (g.time)
        float trace = -1;         // hold: when the per-second state line was last logged (g.time)
        u32 system = 0;           // hold: the system the hold point belongs to (a jump starts the hold over)
        // hold-and-salvo at a static target (StaticHold): the held point (world), on it since when, the hull samples that
        // judge it, the passes' turn after a bad hold, the sides found bad (unit directions from the target, 3 floats each)
        u32 target = 0;
        bool on = false, at = false;
        float pos[3] = {}, dir[3] = {}, hullT = 0, hull = -1, offUntil = -1;
        float orbitDir = 1, mineAt = -10; // the hold's slide direction and its mine drops
        float flipAt = -10;               // the lateral run's last reversal (g.time)
        float evadeUntil = -1;            // exposed at the hold: the slide runs three times as fast until then
        float slideT = 0;                 // the slide's last update (s)
        std::vector<float> badDirs;
    } hold;

    // the hop to a static target's hold point through real geometry (GeoPathAim)
    struct Hop
    {
        GeoPath path; // (was the GeoPath g.hop)
        u32 target = 0;
        bool cruise = false; // the hop's cruise wish (logged on change)
        float cruiseAt =
            -10; // when that wish last turned on (the charge's own clock: the game's flag reads 0 while charging)
        float chargePrev = 0; // the charge's elapsed time last frame (hop_reset_burn: a reset under fire)
        float chargeSince = -1, noCruiseUntil = -10; // a charge not completing -> the afterburner for a while
        float slowSince = -1, backUntil = -10;       // stuck on a hop -> backing off until
        int backSlide = 0;
        u32 frame = 0; // the last frame HopFly flew the ship (g.frames)
    } hop;

    // Docking: the fast dock (bay, approach, request), the queue shortcut, lane entry, the passage, dock_regain
    struct Dock
    {
        // Fast dock: the bay flown to (-1 none yet / no bays: the game's own dock), lined up on its axis, request sent.
        int fastBay = -1;
        int fastBays = 0; // player-size bays of the target (the game's dock points; 1 = queue if someone is there)
        bool formationTried =
            false;                // the user's queue shortcut tried at this bay (formation on the ship docking there)
        float formationAt = 0;    // when (g.time)
        float shortcutSince = -1; // the entrance approach for the shortcut began (g.time; -1 none)
        bool formationTakenIn = false;       // ... and the game took us in (dock port / jump / lane ride)
        u32 replySeen = 0;                   // the last server answer read (host API v14 DockReply sequence)
        float queuedSince = -1;              // the server queued our request (g.time; -1 not queued)
        int queueTries = 0;                  // shortcut tries while queued
        float formationEndAt = -100;         // the last try ended (g.time)
        float bayLen = 0;                    // the bay's approach line p2 -> mount (m; lanes: 60)
        std::vector<space::DockBay> allBays; // every dock point of the target (the lock test looks at all of them)
        u32 lockedIn = 0;                    // a ship in its docking animation at our bay (EnteringShip), 0 none
        float aheadEta = 1e9f;               // the soonest a ship going in reaches the entrance (s; 0 = locked in now)
        bool rushing = false;                // the shortcut's rush to the entrance is on
        bool parkForQueue = false; // the server queued us: back to the entrance for the shortcut (once per approach)
        float parkedSince = -1;    // stopped at the entrance waiting for a ship to lock in (g.time)
        bool portLogged = false;   // the game's assigned dock point logged for this approach
        // the entrance's kind (was fastRing: the bay is a planet's docking ring (type 6): straight in, early request;
        // fastAirlock: the bay is an airlock (type 8): flown through like a player, no dock request; fastLane: the target is
        // a lane ring: bayP2 = where the ride starts, bayAxis = against the lane)
        DockKind fastKind = DockKind::None;
        float bayMount[3] = {};   // the bay's mount point (an airlock's docking sphere centre)
        float airlockPastAt = -1; // an airlock run reached its mount without a transit (g.time; -1 not yet)
        float airlockAlong =
            1e9f; // the airlock run's last distance along the door's axis (a pass beside the door = a miss)
        // PassageVia: a point beside a narrow hole the run flies through instead (on until past its plane), last test (g.time)
        bool passOn = false;
        float passVia[3] = {}, passDir[3] = {}, passOff[3] = {}, passPt[3] = {},
              passCheckAt = -10;              // (via = 80 m past the plate;
                                              // off = the shift; pt = at it)
        float fastStill = 0;                  // seconds at rest while lined up and not yet requested (failsafe)
        float bayP2[3] = {}, bayAxis[3] = {}; // the bay's outer approach point and its outward axis (mount -> p2)
        bool fastLined = false, fastRequested = false, fastTried = false, fastBurn = false;
        Progress fastBest = {1e30f, 0}; // the approach's progress: nearest to the bay so far, and when
        float ringNear = 1e30f;         // a ring run requested at p2 (ring_request): the nearest to p2 so far
        // (fastLane: in fastKind, DockKind::Lane)
        u32 laneNext = 0;         // the next ring of that ride
        int laneSide = -1;        // the game's direction test (space::TargetLaneSide) must read this for that ride
        bool laneCross = false;   // the ship starts on the ring's other side: it flies through the ring first
        bool fastForced = false;  // the fast approach whatever fast_dock says (the objective mode's lane entry)
        int fastBurnSwitches = 0; // afterburner on/off changes during the approach (jitter check)
        bool fastBrakeCommit = false, fastPlanned = false; // braking begun (never re-armed); the plan logged
        float fastPredicted = 0; // the plan's time to the request point (s, from the approach start): checked vs real
        float fastDetour = 0,
              fastDetourAt = -1;                // the way round a planet's zone beyond the straight line (m), when read
        float fastStart = 0, fastRequestAt = 0; // statistics: time the approach started / the request went out
        float losingFor = 0, gainDist = -1;     // dock_regain: the distance growing after a request, for how long
        float grantAt = -1, grantCrawlFor = 0;  // dock_regain: the last dock grant (reply 4), the crawl after it (s)
        bool grantHanded = false;               // ... handed to the game's dock once for that grant
        float restFor = 0, circleUntil = -10;   // the game's dock holding us at rest under fire; circling until
        u32 laneTowardPlan = 0; // fast lane entry: the ring the ride must head to (travel's plan; 0 = the setting)
    } dock;

    // ArriveAt (travel's arrivals, also the fast dock's runs): clear lines, rejoin, the way plan, the wheel timing, the
    // cruise charge, unstick
    struct Arrive
    {
        bool staging = false;      // ArriveAt steers via a staging point this frame (not "no progress")
        bool lineClear = false;    // ArriveAt: the real line to the point is clear for the ship's width (LineClear)
        float lineClearAt = -10;   // ... when it was last tested (g.time)
        bool rejoinOk = false;     // ArriveAt: a fly-through's line of approach is clear from here and on (rejoin it)
        float rejoinAt = -10;      // ... when it was last tested (g.time)
        int rejoinMiss = 0;        // ... failed tests in a row
        bool nowayClimbOn = false; // ArriveAt (noway_climb): no way known yet, climbing out along a clear line
        float nowayAt = -10, nowayAim[3] = {};
        GeoPath path; // ArriveAt's way when the real line to its point is not clear
        bool pathOn = false;
        avoid::WheelScan wheel; // a rotating part ahead being timed (ArriveAt; avoid::WheelScanStep / WheelAim)
        float wheelCheckAt = -10, wheelDoneAt = -10;
        u32 wheelPart = 0, wheelFrame = 0; // the part the guard leaves to the timing, and the frame that was so
        float chargeWantSince = -1, chargeOffUntil = -1,
              chargePrev = 0; // ArriveAt: cruise wanted below cruise speed since; afterburner until
        float rejoinLead = 0; // ArriveAt (cruise lock): the line point's along chosen at the last test
        float stuckSince = -1,
              unstickUntil = -1; // ArriveAt: forward thrust without forward speed since; backing off until
        int unstickDir = 0;      // the slide side of the last unstick (1-4, rotates)
    } arrive;

    // Obstacle avoidance (ApplyAvoid) and the geometry guard (GeometryGuard)
    struct Avoid
    {
        int wallSlide =
            0; // GeometryGuard: slide away from a wall closer than 35 m beside the way (Out::slide code), 0 none
        float wallNear = 0;    // ...its distance (m)
        float pathAim[3] = {}; // the planned way's point steered at this frame (GeoPathAim), for the geometry guard
        u32 pathAimFrame = 0;  // ... the frame it was set (g.frames)
        avoid::Memory mem;     // obstacle avoidance: the side chosen
        int detours = 0;       // statistics: detours started, frames with the straight line blocked
        u32 frames = 0, cruiseDrops = 0;
        bool strict = false; // in a strict field (event on change)
    } avoid;
    // Travel: the destination (a point / object of a system; none = follow the waypoint list as it is), the phase, the
    // plan and the step being flown.
    struct Travel
    {
        int airlockNear = -1; // the destination lies within 1.5 km of an airlock (-1 not read yet): no cruise into it
        // that airlock's door line (AirlockStage: targeted once, its bay read): id (~0u none), asked at, read, p2, outward axis
        u32 lockId = 0;
        float lockAskAt = -10;
        bool lockHave = false;
        float lockP2[3] = {}, lockAxis[3] = {};
        enum Phase
        {
            Plot,  // ask the game for its best path to the destination
            Await, // the answer replaces the waypoint list
            Plan,  // plan this system from the list
            Fly,   // fly the plan's steps
        } phase = Plot;
        bool hasDest = false;
        bool noDock = false; // arrive beside the destination object, do not dock (ap start travel <id> nodock)
        float dest[3] = {};
        u32 destObject = 0, destSystem = 0;
        std::vector<u32> listBefore; // the list's sequence numbers when the best path was asked for
        bool plotted = false;        // our route replaced the player's list (restored at the stop)
        u32 plotFrame = 0;
        float listWaitSince = -1; // g.time the list was first found empty (-1: not waiting)
        route::Plan plan;
        size_t step = 0;
        bool stepBegun = false, riding = false, exitSent = false;
        // The ride's real speed (measured, learned per lane: route::LearnLaneSpeed): distance along the lane over
        // one-second windows; the top window is the lane's speed.
        float rideWinStart = -1, rideWinAlong = 0, rideTop = 0, rideBegan = 0;
        float laneOrigin[3] = {}, laneDir[3] = {}; // the entry ring and the ride's direction
        float stepStart = 0, planStart = 0;
        u32 system = 0;
        int replans = 0;
        int jumpInsBefore = 0;        // the host's jump-in count when the jump step began
        std::vector<u32> closedRings; // lanes the mission has closed to the player this trip (never started a ride)
        float restAt = -1;            // lane step: since when the ship rests at the start point after its request
        float arriveBest = 1e30f;     // arrive step: the closest it got, and when (no progress = arrived close by)
        float arriveBestAt = 0;
        // fly-through: the step's script condition is "within flyRadius of a point" holding the marker - aimed beyond
        // the marker along flyDir (the line of approach), no braking; 0 = an ordinary arrival
        float flyRadius = 0, flyDir[3] = {};
        float flyAhead = 0;      // how far beyond the point the aim lies (m); 0 = max(1000, flyRadius)
        bool changeSeen = false; // the system id changed (the ship is still at its old coordinates for a few seconds)
        float changePos[3] = {}; // ... where it was then
        // A ring to fly through, not to arrive at (M06 race: track rings, Cnd_DistCircle - 22 Oct 2026): flown at full
        // speed through its centre along its axis; never "arrived" (the next objective restarts the travel).
        u32 passRing = 0;
        float passRadius = 0;
        int passSide = 0; // the side of the ring's plane the ship started on (+1 / -1)
        bool passed = false;
        u32 passNext = 0; // the course's next ring after this one (predicted), 0 none
        bool passNextKnown = false;
        bool passBraking = false; // the turning circle says brake (logged on change)
        float passTraceAt = -10;
    } tr;
    // Shoot mode: the points (a depot's cargo pods, world coordinates), the one being shot and since when
    struct Shoot
    {
        std::vector<float> pts; // x, y, z per point
        size_t i = 0;
        float since = -1;                    // g.time the current point came into firing range (-1: not yet)
        u32 lootBefore = 0;                  // loot contacts near the point when its shooting began
        float att0 = 2, att = 2, attAt = -1; // the depot's attitude to us: at the start, last read, when
        u32 obj = 0;                         // StartShootObject: the object shot (no pods, no tractor)
    } sh;
};

// The ship class bits of a contact's type (the game's ObjectType)
constexpr u32 kFighterBit = 1u << 16;
constexpr u32 kFreighterBit = 1u << 17, kGunboatBit = 1u << 18, kCruiserBit = 1u << 19, kTransportBit = 1u << 20,
              kCapitalBit = 1u << 21;

// Objective mode (ap start objective; user, 19 Oct 2026: "Harness is one thing. How would the autopilot handle this
// successfully?"): a supervisor above the modes that does what the current objective asks, from the player's own
// information (the objective's text, its target and navmarker, the HUD's marks) - ObjectiveDecide below. It stays on
// across docking and the modes it starts; the player's input, `ap stop` or our death end it.
struct ObjectiveRun
{
    bool on = false;
    u32 seq = 0;          // the objective step decided for
    Mode sub = Mode::Off; // what it started last
    u32 subTarget = 0;
    bool arrived = false;         // this step's travel ended: rest there (Hold) instead of travelling again
    bool travelObjective = false; // the objective sends the player to a place: its trip outranks fights on the way
    u32 preStageTarget = 0;       // "pre-stage": the next step's target waited at (the rest's subject: the nose on it)
    bool tractorExitPending = false; // tractor_exit_run: loot pulled in passing, the door not yet within range
    float flyGoal[3] = {};           // the script condition's sphere a trip of this decision flies into (fly-through,
    float flyGoalR = 0;              // pre-stage); 0 none - a trip ending outside it is not "arrived"
    u32 escortShip = 0; // the friendly ship an objective last pointed at (the convoy's lead): kept through the escort
    u32 laneRing = 0;   // a lane objective's ring still closed: watched until the mission opens it (M04 West Point)
    bool survival = false; // a Survive objective was given in this run: no free fights from then on (kSurvivalReach)
    // The leader the MISSION put the player in formation with (Act_GiveObjList Player, <formation op>: a priority
    // directive, seen while ai::MissionHolds): M09 "Fly to Tekagi's Arch" completes only when Ozu nears the arch - a
    // travel of our own left him behind and the nag failed the mission (22 Oct 2026). Travel objectives keep that
    // formation while the leader lives; 0 none.
    u32 missionLeader = 0;
    bool fightObjective = false; // the objective's words ask for a fight ("Destroy ..."): its marks outrank everything
    // The script's own destroy condition for this step (mission::Goal kind 3, Source::Hidden): its live objects count as
    // the mission's marks and the step as a fight (M13 "Head to the Barrier" advances when the 4 generators die)
    u32 scriptMarks[8] = {};
    int scriptMarkN = 0;
    std::string goalTrigger; // the script goal the running trip flies to
    std::string badGoal;     // a script goal whose trip found no way: skipped for the rest of the step
    std::set<std::string>
        deadGoals;        // script goals reached without the step advancing (not armed): skipped for the step
    float leave[27] = {}; // where the ship goes after the current kill: the destroy goal's live objects and
    int leaveN = 0;       // ... the next step's place (StaticHold's side); leaveAt: when read (GetTickCount64)
    ULONGLONG leaveAt = 0;
    bool cruiseLock = false; // cruise kept come what may (steering only) from an airlock transit to the next jump /
                             // dock (user, 23 Oct 2026: M12 core entrance -> Omicron Minor hole, "must not drop")
    u32 lastDock = 0;        // the last Dock objective's target, kept while only a placeholder text follows it
                             // (cleared by a dock / leaving space and by any objective the script knows)
    u32 leashShip = 0;       // an armed ship leash's anchor (mission::Leashes): every trip keeps within 75 % of it
    float leashDist = 0;
    u32 stepKey = 0; // the objective step decided for (text, target, navmarker): a re-issued copy is no new step
    std::vector<u32>
        lastMarks; // the host's mission marks when the step was last seen (a step swallowed by a placeholder)
    ULONGLONG missionFormSince = 0;      // the mission ordered a formation (op 10) whose leader is not readable yet
    bool gameDock = false;               // the game docks the ship on a mission's request (GameDocksUs): waiting
    bool formRefused = false;            // the running follow stands in for a formation the game refused (too far)
    ULONGLONG arrivedAt = 0;             // when this step's travel ended "arrived"
    ULONGLONG leadStillSince = 0;        // the followed lead and we both at rest since
    ULONGLONG stepAt = 0;                // when the current objective step began
    bool viaNext = false, inVia = false; // the next navmarker travel goes by a via point / is on its way to one
    int arriveRetries = 0;               // travels again because the step's navmarker was still far (per step)
    ULONGLONG formRetryAt = 0;
    char lastStop[48] = ""; // why the last sub-mode stopped (StopImpl): "travel: dock refused" -> retry
    StopReason lastStopReason = StopReason::None; // ...as a reason (the retries test this; lastStop is its text)
    ULONGLONG dockRetryAt = 0; // a refused (locked) dock the objective still asks for: retried at this tick
    int dockNoWayTries = 0;    // a dock trip's "no way" retries this step (at most 15)
    ULONGLONG nextLook = 0, subSince = 0, windowStart = 0;
    int decisions = 0;
    Action action = Action::None; // the running sub-action (ActionName: its text)
};
extern ObjectiveRun g_obj; // the objective run (autopilot.cpp)

constexpr float kSurvivalReach = 1200; // m

constexpr float kBattleReach = 15000; // m: hostiles fighting our side count as the player's battle within this

// The fight as a pilot reads it (Assess, with the target choice): hostile strength around us in fighter units
// (class weight x attacker x hull left), where it is, how close, how many.
struct Situation
{
    u32 best = 0;     // the target choice
    float threat = 0; // fighter = 1, freighter / transport 0.5, gunboat 3, cruiser 6, capital 10; x1.5 attacking
    int hostiles = 0, attackers = 0;
    u32 protectedId = 0; // the friendly ship the hostiles target most (the fight's subject: where to look after)
    int hidden = 0;      // hostiles out of sight behind something solid (space::SightBlocker): not targets
    int hiddenNear = 0;  // of them, within 3 km: the fight is not over (they come out)
    std::vector<u32> behindStation; // of them, hidden by a station / base / planet: not even flown towards
    float nearest = 1e9f;           // nearest hostile ship (m)
    float center[3] = {};           // threat-weighted position of the hostiles
    std::vector<u32> loot;          // loot worth taking within reach, best first
    int lootInReach = 0;            // of it, within the tractor's reach (tractor-all takes those)
    float lootNearest = 1e9f, lootNearestPos[3] = {}; // the nearest of it
    int areaLevel = 0;                                // encounter level of the zones at the ship (0 none)
    int rank = -1;                                    // the player's rank
};

constexpr u32 kLootBit = 1u << 28;

constexpr float kTheirHitPrior = 0.02f;
struct Odds
{
    float odds = 1e9f, ourPot = 0, theirPot = 0, ourHp = 0, theirHp = 0, ourDps = 0, theirDps = 0;
    int ships = 0;
    bool measured = false;
};

struct MissileEconomy
{
    float price = -1; // -1: no loaded homing launcher
    int stock = 0;
    float refill = 0, worth = -1;
    bool cheap = false;
};

// Globals (defined in autopilot.cpp / their section file)
extern Settings g_set;

// Where a behaviour's knowledge comes from (user, 22 Oct 2026; AUTOPILOT_UX.md section 2a). Every behaviour that is more
// than plain flying names its source; the knowledge scope (Settings::knowledge, set by presets) decides which may run.
// Seen: what play shows the player (objective text, comms, HUD, contacts, scans, places visited - the game's own visit
// record). Skill: general piloting a skilled player finds alone (hold-and-salvo, braking short, staggered fire). Hidden:
// game data a player never sees (turret reach from the guns' joint limits, script trigger conditions - core\game\mission's
// facts, used only under the knowledge scope "all", AUTOPILOT_UX.md 2a.2 - wreck contents, unvisited markets). Meta: community knowledge of the game's quirks (speedrun tricks, docs\research\speedrun.md).
enum class Source
{
    Seen,
    Skill,
    Hidden,
    Meta,
};
inline bool Knows(Source s) { return g_set.knowledge >= 1 || s == Source::Seen || s == Source::Skill; }
extern ai::Personality g_p;
extern bool g_pReady;
extern std::map<u32, float> g_missileGate;
extern State g;
extern u32 g_survivalTitle;
extern bool g_survivalMission;
extern Situation g_sit;
extern u32 g_subjectId;
extern ULONGLONG g_subjectTick; // last seen as the subject (GetTickCount64)

// Functions used across the parts (defined in their section file)
Mode ModeFromName(const std::string& s);
void BuildPersonality();
float Pf(float v, float lo, float hi, float fallback);
const char* TacticName(Tactic t);
float MissileGate(u32 arch);
float Dot3(const float* a, const float* b);
float Len3(const float* a);
bool SubmitSettled();
void MarkSubmit();
int SubmitNow(const ai::Directive& d);
void FreeFlightNow();
std::string CombatStats();
bool ObjectiveOn();
// text: the reason as formatted with its numbers (Hostile) or the module API's own text; nullptr = StopText(why)
void StopImpl(StopReason why, bool cancel, const char* text = nullptr);
void Stop(StopReason why, const char* text = nullptr); // Stop(const char*) (autopilot.h) for the autopilot's own stops
// The autopilot's view of the contacts: space::Contacts() (what the game says) with pre_aggro's overlay - the ships
// SetPreHostile names (mission::PendingHostileShips), neutral now (attitude -0.6 .. 0.6, asked of the game) and within
// its range, read as hostile (attitude -0.65, Contact::preHostile) so every hostile test takes them. Every autopilot
// site reads contacts through this (FindContact too); empty list = no overlay.
std::vector<space::Contact> Contacts();
void SetPreHostile(const std::vector<u32>& ships, float range);
space::Contact FindContact(u32 id);
int MissionObjectIds(u32* out, int max, const std::vector<space::Contact>& all);
bool Start(Mode m, u32 target, const float* pos);
bool FriendlyContact(const space::Contact& c);
// The objective asks for a destruction (a Fight verb in any entry); the nearest mission-marked loot it then names to
// destroy (0 none) - shot by the Shoot mode, never tractored (autopilot_objective.cpp; the frame drops a tractor-all
// while one is within the beam).
bool ObjectiveAsksDestroy();
u32 DestroyMarkedLoot(const std::vector<space::Contact>& all, float* dist);
void ObjectiveDecide(const char* reason);
void ObjectiveSupervise();
// The game docks the ship on its own: a dock port held with no request of ours (a mission's Act_DockRequest).
bool GameDocksUs();
ULONGLONG SinceGameGrant(); // ms since the last dock grant we did not request (a mission's Act_DockRequest); ~0 none
// GameDocksUs: a mission's grant counts as the game docking us for 4 s after it, until the game's own Dock maneuver
// holds the dock port (M12 core exit: the grant's 4 s ran out before the port was held, our travel took over again
// and the ship died in the core's radiation - autopilot_frame.cpp)
constexpr ULONGLONG kGameGrantDockMs = 4000;
// ObjectiveDecide: a mission's grant with no dock maneuver after it and an airlock within 3 km is the story's way in
// for 20 s after it - flown through by the fast approach's airlock run (M12 22 Oct 2026, the lair entry -
// autopilot_objective.cpp)
constexpr ULONGLONG kGameGrantWayInMs = 20000;
bool SystemEntry(float pos[3]); // where the ship first showed in its current system (an airlock's or a jump's way in)
ULONGLONG SystemEntryTick();    // when the system last changed (GetTickCount64), 0 none seen
// the last airlock door line read (any trip), when its p2 lies within `within` of `near` and the door is in this system
bool LastAirlockDoor(const float near[3], float within, float p2[3], float axis[3]);
// ref_line: the reference flight's aim for a trip to `dest` (autopilot_travel.cpp)
bool RefLineAim(const fl::Kinematics& k, const float dest[3], float aim[3]);
bool DockRegain(float dt, u32 target); // dock_regain: a granted dock losing ground dropped (autopilot_dock.cpp)
void SetTactic(Tactic t);
int Op();
void Trail(u32 target);
void Follow(u32 target);
bool GameEvade(u32 threat);
int DodgeDirection();
void BreakAway(const aim::Target& t);
void ResetSituation();
void Reassess();
u32 PickTarget();
float OurHullDps();
Situation Assess();
float Strength(const ai::Self& s);
const Odds& FightOdds(const ai::Self& self);
float RecentDamage(float hull);
const MissileEconomy& Economy();
void TrackOwnMissiles();
bool LineOfFireBlocked(u32 target, const float tpos[3], float radius, float past, float pastRadius);
float MissileFlightTime(const space::Weapon& w, float dist, float closing0);
MissileGeom MissileGeometry(u32 target, const space::Weapon& w, float flight, float targetRadius);
int DuelChoose(unsigned long long key, float ratio, int attackers);
void DuelLog();
void DamageAndRepairs(float dt, Out& out, ai::Self& self);
void UpdateCombat(float dt, Out& out);
void Countermeasures(float dt, Out& out);
// solidsClear: the real line ahead is ray-clear - no solid's bounding sphere is avoided (zones, rocks, ships still are)
void ApplyAvoid(const fl::Kinematics& k, Out& out, u32 ignore, bool solidsClear = false);
// a gap a ray passes but the ship at speed does not (the M12 core's plate): the passage beside it into aim (autopilot_geo)
bool PassageVia(const fl::Kinematics& k, float aim[3]);
// The straight line from `a` to `b` clear of real hulls for a ship of `radius` (centre + 4 side rays, the game's ray test).
bool LineClear(const float a[3], const float b[3], float radius);
// Fly `p` toward `dest` through real geometry: planned (avoid::RayPath; again when the destination moves > 100 m, or 3 s
// after no way was found), on to the farthest point in plain sight. True with `aim` (the point to steer at) and `rest`
// (the way left along it); false when no way is known (the caller steers straight). `ignore`: the target (0 none);
// `who` names it in the events.
bool GeoPathAim(GeoPath& p, const fl::Kinematics& k, const float dest[3], u32 ignore, float aim[3], float& rest,
                const char* who);
// A ring to fly through (a race course's track ring): its physical radius, 0 if the contact is none (autopilot_objective.cpp).
float PassRingRadius(const space::Contact& c);
// Starts a travel through that ring (and on along its course: the look-ahead); false if refused.
bool StartPass(u32 ring, float radius);
u32 EnteringShip(float dist);
void QueuedShortcut();
bool FastApproach(float dt, Out& out);
char HoldLook(const float at[3], float look[3]);
u32 MissionSubject();
u32 RestSubject(char& why);
bool ScenicPoint(const float at[3], float out[3]);
bool ArriveAt(float dt, const float pos[3], Out& out, bool calm = false);
bool WheelTiming(const fl::Kinematics& k, const float pos[3], float aim[3]);
void UpdateTravel(float dt, Out& out);
void UpdateShoot(float dt, Out& out);
// The Hold mode's frame (autopilot_hold.cpp); `op` = the ship's AI op UpdateModes read at the top of the frame (Op())
void UpdateHold(float dt, Out& out, int op);
// Shoot mode on a depot: its cargo pods (HpCargo01..08 of the target), each shot from firing range until it drops its
// cargo, the loot tractored; false if the target has no pods.
bool StartShootPods(u32 depot);
// Shoot mode on one object to destroy (a marked cargo pod): followed live, shot from firing range with the nose on it,
// nothing tractored; ends "target destroyed" when it is gone.
bool StartShootObject(u32 id);
} // namespace autopilot
