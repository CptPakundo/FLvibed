// The arbiter's rules (IDEAS A37, modules\autopilot\docs\A37_PLAN.md slices S3-S5): pure functions Situation + Queue ->
// a Proposal (what to start: the sub-mode, its target, the objective mode's action), each a port of one branch of
// autopilot_objective.cpp ObjectiveDecide with the same conditions and the same mode / target / action. S3 ports the
// branches before the pre-stage (the hold, the leash, marked loot to destroy, the tractor's exit run, a jump, a granted
// airlock, the script's destroy condition, the last dock, the script goal); S4 the pre-stage, the mark fights, pre-aggro,
// the survival stance, the threat fight, the escort and the ship target; S5 the rest (lanes, passes, routes, rest).
// Nothing decides by them yet: `situation_shadow` (default 0) runs them beside ObjectiveDecide at the exits they cover and
// logs `situation shadow agree|differ <rule> ...` (situation_snap.cpp ShadowCompare); S7 switches.
// The rules read the situation, its facts (Situation::facts: the snapshot) and the executor's memory (Queue: today's
// ObjectiveRun / State fields; S6's goal queue replaces it). Compiled into the controller's logic and, plain, into
// build\test\fltest.exe.
#pragma once
#include "situation.h"

#include <optional>
#include <string>
#include <vector>

namespace situation
{
// How a proposal is started (A37 S7: the executor's input, situation_exec.h) - the start the old exit of the same branch
// makes: its kind, the object / point, the travel's options and fly-through, and the exit's own writes the supervisor and
// the travel read later (ObjectiveRun: travelObjective, goalTrigger, flyGoal, preStageTarget, inVia, the script marks).
// Never compared by the shadow (ProposalText leaves it out); the arbiter compares it with the old path's plan.
struct Plan
{
    enum class Kind
    {
        None,      // nothing new: the running sub-mode goes on (a keep, a hold verdict)
        Mode,      // StartOrKeep(mode, object): combat / escort / formation / follow / hold in place
        Travel,    // StartTravel(object, point, system, noDock) (+ the fly-through)
        HoldAt,    // Start(Hold, 0, point)
        Dock,      // Start(Dock, object) + the lane ride toward `toward` (the fast entry forced)
        Pass,      // StartPass(object, radius)
        Shoot,     // StartShootObject(object)
        Waypoints, // Start(Travel, 0) over the mission's waypoint list
        COUNT
    };
    Kind kind = Kind::None;
    u32 object = 0;
    bool hasPoint = false;
    float point[3] = {};
    u32 system = 0;      // the point's system (0 = this one)
    bool noDock = true;  // Travel / Waypoints: no dock at the end
    u32 toward = 0;      // Dock: the lane ride's next ring
    float radius = 0;    // Pass: the ring's radius
    bool fly = false;    // Travel: flown through (g.tr flyDir / flyRadius / flyAhead)
    float flyDir[3] = {};
    float flyRadius = 0, flyAhead = 0;
    // the exit's writes (applied by the executor when it plans the proposal itself)
    int travelObjective = -1; // -1 unchanged, else 0 / 1
    int fightObjective = -1;  // -1 unchanged, else 0 / 1 (with scriptMarks: the step's destroy condition's live objects)
    std::vector<u32> scriptMarks;
    bool setGoalTrigger = false;
    std::string goalTrigger;
    float flyGoal[3] = {};
    float flyGoalR = 0; // > 0: g_obj.flyGoal / flyGoalR written
    u32 preStageTarget = 0;
    bool inVia = false; // the via point: inVia on, viaNext off
};
const char* PlanKindName(Plan::Kind k);
// "<kind> object=<id> [point=x,y,z system=<id>] dock=<0|1> [toward=] [radius=] [fly=dx,dy,dz r= ahead=]"
std::string PlanText(const Plan& p);

struct Proposal
{
    std::string rule;         // the rule that holds ("held", "leash", ...; "none" = no ported rule: a later slice's)
    std::string mode = "off"; // the sub-mode it starts (autopilot ModeName)
    u32 target = 0;
    std::string action = "-"; // the objective mode's action (autopilot ActionName)
    bool keep = false;        // no new start: the running sub-mode goes on (mode / target / action = the running ones)
    std::string detail;       // why (the log; never compared)
    Plan plan;                // how it is started (A37 S7; never compared by ProposalText)
};
// "<rule> mode=<m> target=<id> action=<a> keep=<0|1>"
std::string ProposalText(const Proposal& p);

// ObjectiveSupervise's holds (the mission flies the ship, the game docks it on a mission's request, a lane ride, a jump):
// nothing is decided; detail = situation::HeldName. Docked / no ship hold nothing today (the objective mode decides).
std::optional<Proposal> Rule_Held(const Situation& sit, const Queue& q);
// An armed ship leash we are far out on (beyond 80 %): back toward its ship (the trip there keeps running) - never when
// the step docks or points at a jump gate / hole (by a ref or a navmarker within 2 km of one).
std::optional<Proposal> Rule_Leash(const Situation& sit, const Queue& q);
// Mission-marked loot the objective says to destroy, nothing attacking: shot.
std::optional<Proposal> Rule_MarkedLoot(const Situation& sit, const Queue& q);
// The mark fallback's target is loot within the tractor's reach while a travel runs: the trip goes on (the beam pulls it
// in), or under tractor_exit_run + the cruise lock within its range of the door: on to the trip's airlock.
std::optional<Proposal> Rule_TractorExit(const Situation& sit, const Queue& q);
// The step's target is a jump gate / hole: the way out.
std::optional<Proposal> Rule_Jump(const Situation& sit, const Queue& q);
// A mission's dock grant (< 20 s, no dock maneuver) with an airlock within 3 km: through it (not the way we came in).
std::optional<Proposal> Rule_Airlock(const Situation& sit, const Queue& q);
// The script's destroy condition for a step that names no target of its own (Source::Hidden): its live objects - far:
// a trip to 1.5 km short of the nearest, else their fight.
std::optional<Proposal> Rule_ScriptDestroy(const Situation& sit, const Queue& q);
// The last Dock objective's base / planet while only a text the script does not know follows it (no leash): docked with.
std::optional<Proposal> Rule_DockEntry(const Situation& sit, const Queue& q);
// The script's flying condition for the step (Source::Hidden): a trip into it (through it to a goal it arms inside), or
// with it fired, to the goal it arms.
std::optional<Proposal> Rule_ScriptGoal(const Situation& sit, const Queue& q);

// ---- S4 (ObjectiveDecide from the pre-stage to the formation / follow of a ship target)
// The script block's fired condition while the script still talks (a placeholder by the last objective, or a fight
// step's destroy condition met): the next step's distance goal (its near side) or the nearest object of its destroy
// condition (a trip, within 700 m a hold).
std::optional<Proposal> Rule_PreStage(const Situation& sit, const Queue& q);
// prestage_next: a placeholder after a step whose end fired nothing - the next step's own distance goal (NextStepGoals of
// the last real objective), else the place the script declares next (beyond 1 km); never across a leash.
std::optional<Proposal> Rule_PreStageNext(const Situation& sit, const Queue& q);
// A fight step: a huge static mark (big_coast) is the fight's own approach; a mark beyond 5 km a trip there first.
std::optional<Proposal> Rule_MarkFight(const Situation& sit, const Queue& q);
// pre_aggro: the nearest ship the script is about to turn hostile is fought now (not on a dock / escort / lane-escape
// step; under an armed ship leash only within 75 % of it).
std::optional<Proposal> Rule_PreAggro(const Situation& sit, const Queue& q);
// survival_stage: the calm survival stance waits at the script's next place (a fight there only within 1.5 km of it).
std::optional<Proposal> Rule_SurvivalStage(const Situation& sit, const Queue& q);
// The survival stance attacked: with the mission's lead (escort), else only the close attackers.
std::optional<Proposal> Rule_Survival(const Situation& sit, const Queue& q);
// A threat (or the step's own hostile target): the escort's ship / the mark fight with the escort / the objective's
// hostile ship (survival) / the fight.
std::optional<Proposal> Rule_ThreatFight(const Situation& sit, const Queue& q);
// An escort step with no ship target: formation (or the follow already running) with the ship it last pointed at, the
// ship its words name, else the nearest friendly mark.
std::optional<Proposal> Rule_Escort(const Situation& sit, const Queue& q);
// A ship target: formation for an escort step, else follow.
std::optional<Proposal> Rule_ShipTarget(const Situation& sit, const Queue& q);

// ---- S5 (the rest of ObjectiveDecide: every exit is ported)
// A Dock step at a lane ring: closed both ways -> wait beside it (a trip there beyond 1.5 km); else the ride - a trip
// first beyond 3 km (unless arrived), then the dock toward the open neighbour.
std::optional<Proposal> Rule_Lane(const Situation& sit, const Queue& q);
// A trip objective with the leader the mission named, alive and friendly: in its formation.
std::optional<Proposal> Rule_MissionForm(const Situation& sit, const Queue& q);
// A race ring (a satellite-class solar, no lane / dock, 30-300 m) as the step's target, no verb (or Survive): through it
// (the running travel through it kept).
std::optional<Proposal> Rule_Pass(const Situation& sit, const Queue& q);
// A route objective (the step = the last entry's text) whose target is a lane ring: the ride toward the neighbour nearer
// to the route's step 1.
std::optional<Proposal> Rule_RouteLane(const Situation& sit, const Queue& q);
// This step's travel is done: rest there (under the survival stance beside its lead).
std::optional<Proposal> Rule_Arrived(const Situation& sit, const Queue& q);
// A route objective: a trip to its last entry (object or navmarker).
std::optional<Proposal> Rule_Route(const Situation& sit, const Queue& q);
// A target the mission's waypoint list holds (an entry on it or within 1.5 km, not the last): the list flown.
std::optional<Proposal> Rule_Waypoints(const Situation& sit, const Queue& q);
// Any other target: a trip there (a planet's docking ring), docked with for a Dock step or an airlock.
std::optional<Proposal> Rule_Arrive(const Situation& sit, const Queue& q);
// A navmarker on an airlock / gate / hole (within 200 m), not hostile: through it.
std::optional<Proposal> Rule_NavWay(const Situation& sit, const Queue& q);
// A navmarker retry after a trip ended short of it: by a via point first.
std::optional<Proposal> Rule_Via(const Situation& sit, const Queue& q);
// A navmarker: the trip to it.
std::optional<Proposal> Rule_Navmarker(const Situation& sit, const Queue& q);
// Nothing else: rest (under the survival stance beside its lead).
std::optional<Proposal> Rule_Rest(const Situation& sit, const Queue& q);

// The S3 rules in ObjectiveDecide's order, the first that holds; else rule "none".
Proposal DecideS3(const Situation& sit, const Queue& q);
// Every ported rule (S3-S5) in ObjectiveDecide's order, the first that holds (the shadow's decider; since S5 the rest
// rule always holds, so "none" no longer comes back).
Proposal Decide(const Situation& sit, const Queue& q);
} // namespace situation
