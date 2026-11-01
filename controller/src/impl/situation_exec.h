// The arbiter's executor (IDEAS A37, controller\docs\A37_PLAN.md slice S7; setting `situation_arbiter`, default 0): the
// decision comes from Decide(Assess(snapshot), queue) and the goal queue; the executor turns the proposal into today's
// ObjectiveStart calls (the same mode / target / action vocabulary), every side effect checking the game first.
// This header is the executor's pure half (host-free, tests\situation_exec_test.cpp): the side-effect gate and its event
// line, the plan comparison (the old path's start against the rules' plan), the ObjectiveStart arguments of a proposal,
// the pause facts and what a resume restores (D10). The impure half: autopilot_objective.cpp (plan mode, the executor:
// ArbiterExecute) and situation_snap.cpp (the snapshot, the queue across the pause: Arbitrate / ArbiterResume / ...).
// Compiled into the controller's logic and, plain, into build\test\fltest.exe.
#pragma once
#include "goalqueue.h"
#include "situation_rules.h"

#include <string>

namespace situation
{
// ---------------------------------------------------------------- side effects check the game first (A37_PLAN 1)
// What the gate reads of the game before a start (the executor fills it: autopilot_objective.cpp GateFactsNow).
struct GateFacts
{
    bool inSpace = true;               // a pilot to read (not docked, not in the menus)
    bool lane = false, jumping = false; // riding a trade lane / in a jump
    bool gameDocks = false;            // GameDocksUs: a mission's dock grant flies the ship
    bool dockInProgress = false;       // the game holds a docking port for us (ai::DockInProgress) ...
    u32 dockPortObject = 0;            // ... of this object (ai::DockPort)
    std::string mode = "off";          // the autopilot's running mode (ModeName) and its target
    u32 target = 0;
    bool dockReplyPending = false;     // our Dock mode's request is out, no server answer since (DockReply sequence)
    u32 leader = 0;                    // the formation leader the game has us under (0 none)
};
enum class GateVerdict
{
    Go,     // start it
    Keep,   // the same thing already runs (no new request): the decision stands, nothing is started
    Refuse, // not now (a refused start: the supervisor decides again)
    Adopt,  // the game already flies it (formation under this leader): our mode takes it over without a request
    COUNT
};
const char* GateVerdictName(GateVerdict v);
struct Gate
{
    GateVerdict verdict = GateVerdict::Go;
    std::string kind; // SideEffectKind
    std::string why;  // the refusal's / keep's reason ("" for Go)
};
// The side effect's kind: travel (a trip with no dock at its end) / dock-trip (a trip that docks, jumps or passes an
// airlock at its end) / dock (the lane ride's or a dock request) / pass / shoot / hold (in place or at a point) /
// waypoints / the mode's name (combat, escort, formation, follow); "" = nothing is started (Plan::Kind::None).
std::string SideEffectKind(const Plan& p, const std::string& mode);
// The gate, before any start: nothing while not in space, riding a lane or jumping; a dock request (a Dock plan or a
// trip that docks at its end) never while the game docks us, never a second one to the object the game's dock already
// holds (keep while our own mode runs, else refuse), never a second one while our Dock mode to it runs (keep), never
// another while our own request awaits its answer; no formation request while the game already has us under that leader
// (adopt). Cruise: untouched (as today: autopilot.cpp StopImpl keeps it across a restart).
Gate CheckSideEffect(const GateFacts& f, const Plan& p, const std::string& mode);
// "side-effect <kind> target=<id> step=0x........ rule=<rule>" + " <verdict> why=<why>" unless Go (the fuzz oracle, S8)
std::string SideEffectLine(const Gate& g, u32 target, u32 step, const std::string& rule);

// Two plans the same start? "" = agree, else the differing parts ("kind travel/hold-at", "point 37 m", "fly", ...).
// Points within 1 m, the fly direction within 0.01, radii within 1 m are the same.
std::string PlanDiff(const Plan& a, const Plan& b);

// ---------------------------------------------------------------- the decision -> ObjectiveStart
// The ObjectiveStart arguments a proposal stands for (the old exit's: ObjectiveStart(mode, target, action, ok)); a held
// verdict and the leash's running trip back (a keep) make no ObjectiveStart call (objectiveStart false).
struct StartArgs
{
    std::string mode = "off";
    u32 target = 0;
    std::string action = "-";
    bool objectiveStart = false;
};
StartArgs StartArgsOf(const Proposal& p);

// ---------------------------------------------------------------- D10: the pause and the resume
// Where and when the player's stop paused the queue (situation_snap.cpp ArbiterPause), and what happened meanwhile.
struct PauseMark
{
    double atMs = 0; // game clock (ms)
    u32 system = 0;
    bool posKnown = false;
    float pos[3] = {};
    u32 stepKey = 0;     // the paused queue's step
    bool docked = false; // docked / left space meanwhile (Stop "docked" / "left space" while paused)
};
// The pause's facts at the resume: how long (a clock gone back - a load - reads as long), another system, docked
// meanwhile, a job taken (the step now is a job's and another than the paused one), how far the ship moved (another
// system / no position: not compared).
goalqueue::PauseFacts PauseFactsOf(const PauseMark& m, const Situation& now, double nowMs);
// What a resume writes back into the executor's working memory (ObjectiveRun): the step entry's memory (silent and the
// same step: the paused one, all of it; else the rebuilt entry's - fresh, `arrived` by the step's own arrival test),
// the carried Dock entry's object and the story's last real text.
struct Restore
{
    goalqueue::ResumeChoice choice = goalqueue::ResumeChoice::Nothing;
    bool cold = false;     // no paused queue (the first start in this game: after a load / a fresh process)
    bool sameStep = false; // the paused step goes on with its memory
    goalqueue::EntryMemory memory;
    u32 lastDock = 0, lastReal = 0;
    std::string why;
};
// From D10's Resume over the paused queue (Resume itself: goalqueue.h), or cold (`paused` null: the queue rebuilt from
// the situation alone). `queue` receives the queue the executor goes on with.
Restore ResumeOf(const goalqueue::GoalQueue* paused, const Situation& sit, const goalqueue::PauseFacts& pause,
                 const goalqueue::ResumePolicy& pol, goalqueue::GoalQueue& queue);

// ---------------------------------------------------------------- the impure half (situation_snap.cpp; the game only)
// The arbiter's decision at an exit of ObjectiveDecide's body (run in plan mode: nothing started yet): the snapshot now
// with the memory as the decision began (ShadowEntry: the executor's working memory, classed into the queue - lossless,
// S6), Decide over the queue's rules' Queue, then Advance (the step key written after Decide). `old` = the old body's
// decision at that exit. `have` false: no entry memory (the switch went on during the decision).
struct Arbitration
{
    bool have = false;
    Proposal p;
    bool agree = false; // p's rule / mode / target / action / keep = the old exit's
    u32 step = 0;       // the situation's step (sit.step)
    double ms = 0;
};
Arbitration Arbitrate(const Decided& old);
// An activation (ap start objective / arm, the smart start): the next decision resumes the paused queue first.
void ArbiterActivated();
// The first decision after an activation (before its step memory upkeep): D10 - the paused queue re-proven (Resume), or
// the cold rebuild; the restore written into ObjectiveRun (silent and the same step: its seq kept, so the step's memory
// survives the decision's step upkeep); one event `situation resume <choice> ...`.
void ArbiterResume();
// The player's stop (ap stop, the pad, player input): the queue as it stands, with the working memory, kept (D10).
void ArbiterPause();
// Docked / left space while paused (a pause fact).
void ArbiterNoteDocked();
// A new game (a load, New Game, a death's reload): the queue and its pause forgotten (the load loses the story memory:
// A37 S6's last_real finding, decided for S7).
void ArbiterForgetGame();
// The queue's text for `ap situation queue` (the queue, the pause mark).
std::string ArbiterQueueText();
// The queue's story memory (the last real objective text): ObjectiveDecide's lastReal before each decision.
u32 ArbiterLastReal();
} // namespace situation
