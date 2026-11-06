// Advanced Autopilot - what the objective's files share: the objective's verb, the stage plan and the names the four files call across.
// Private to autopilot_objective*.cpp; include it after autopilot_impl.h and lookahead.h.
#pragma once

namespace autopilot
{
// What the objective's words ask beyond "go there" (the words only refine what the target's kind decides: a dock, an
// escort, a fight; English text - other languages fall back to the kind alone).
enum class ObjVerb
{
    None,
    Dock,
    Escort,
    Fight,
    Survive // "Defend Yourself!", "Survive", "Escape": staying alive is the task, kills are not
};
// spawn_stage's facts now: the step's armed kill goal (AdvanceGoalsPeek with LOOK's kind 4; armed in the live table when
// it reads) with one ship alive and the others down, what its firing spawns (mission::SpawnAfterKill), PSN's exception
// (lookahead::FleeingSpawn), the fight's leash (the leaving nags this kill switches off, anchored on their object now) and
// the stage point (lookahead::StagePoint). Not ok: `why` of the last goal looked at.
struct StagePlan
{
    bool ok = false;
    std::string why, trigger, ship, anchorName;
    lookahead::StageResult st;
    float spawn[3] = {};
    u32 last = 0;        // the goal's one live ship
    float lastFromP = 0; // its distance from the stage point
    int alive = 0, down = 0, need = 0;
    float leash = 0;
    // `spawn_charge`: the spawn flees (FleeingSpawn passed: his run point known - the charge's facts), the point is the
    // charge's (StagePoint over every leash), the leashes it kept to, the ones that pulled it, and why the charge's point
    // was not used (a datum missing: the spawn_stage point as before; a refusal: no stage)
    bool fled = false, charge = false, chargeRefused = false;
    float run[3] = {};
    int chargeLeashes = 0;
    std::string chargeBound, chargeWhy;
    std::vector<lookahead::StageLeash> chargeLs; // (`ash_ride`: the leashes the loop keeps inside)
};
// autopilot_objective.cpp: the objective's helpers and the decision's actions
bool StartTravel(u32 object, const float* point, u32 system, bool noDock);
ObjVerb ObjectiveVerb(u32 textId);
bool HostileContact(const space::Contact& c);
u32 PreHostileNear(const std::vector<space::Contact>& all);
bool ObjectiveThreat(const std::vector<space::Contact>& all);
bool DestroyFired(const mission::Goal& sg, const std::vector<space::Contact>& all);
u32 SurvivalLead(const std::vector<space::Contact>& all);
bool StartOrKeep(Mode m, u32 target);
bool DecHoldAt(const float* q);
bool DecDock(u32 ring, u32 toward);
bool DecPass(u32 ring, float radius);
bool DecShoot(u32 id);
bool DecWaypoints(bool noDock);
void DecFly(bool ok, const float* dir, float radius, float ahead);
void ObjectiveStart(Mode m, u32 target, Action action, bool ok);
void ShadowAfter(const char* rule, Mode m, u32 target, Action action, bool keep = false);
void ShadowExit(const char* rule, Mode m, u32 target, Action action, bool ok, bool keep = false);
extern u32 s_lastReal; // ObjectiveDecide's `lastReal`
bool LeashTripRuns(const space::Contact& lc);
void LeashBack(const space::Contact& lc);

// autopilot_objective_look.cpp: the look-ahead's facts, the chase, the spawn stage, the lock trap
lookahead::TimerDestroyFacts TimerDestroyFactsNow(const ObjectiveStep& st, ObjVerb verb, const std::vector<space::Contact>& all);
std::string TimerDestroyNext();
lookahead::AllyKillFacts AllyKillFactsNow(const ObjectiveStep& st, const std::vector<space::Contact>& all);
void LookLog(const ObjectiveStep& st, ObjVerb verb, const std::vector<space::Contact>& all);
u32 ChaseCandidate(const ObjectiveStep& st, ObjVerb verb, const std::vector<space::Contact>& all, lookahead::LookFacts& f, lookahead::LookResult& r);
StagePlan SpawnStageNow(const ObjectiveStep& st, const std::vector<space::Contact>& all);
int StageVerdictNow(const StagePlan& sp, float* toGo = nullptr);
bool LockTrapDecide(const std::vector<space::Contact>& all);

// autopilot_objective_decide.cpp
std::vector<mission::Goal> FightAheadGoals(u32 textId, const mission::LiveTable& lt, bool live);
void DecideBody(const char* reason);

} // namespace autopilot
