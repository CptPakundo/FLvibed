// The goal queue (IDEAS A37 / A38, modules\autopilot\docs\A37_PLAN.md slice S6): pure, host-free - no game, no autopilot state.
// A GoalRecord is one thing to achieve as data (A38: `docs\research\autopilot-planning.md` "Minimum goal schema"): a
// closed type, its origin (the story, the player, a job), its subject (an object, a point, a system), typed parameters,
// done / fail / when conditions in a small closed condition language over the Situation (mirroring the game's Cnd_*),
// a policy, the display string ids, the step key it stands for and the entry memory that dies with its step. The queue
// is the only memory the arbiter keeps (A37_PLAN 1 "The queue is the only memory"): every entry is re-proven at every
// assess (valid / achieved / failed / blocked), a head already done is popped before anything acts (achieve semantics),
// the story entries are rebuilt from the live state alone (the A39 live table first, the ini view the fallback) and the
// player's own entries are kept while valid; D10's re-proof on resume is a pure function of the paused queue, the
// situation and the pause's facts.
// The executor memory S3-S5's `situation::Queue` carries today is classed here field by field (MemoryFields: derived
// each time / entry memory dying with the step / story memory / mission scope / executor-only); ToRulesQueue builds the
// rules' Queue from a goal queue + the executor's own state, FromRulesQueue classes a recorded one. The step key is
// written only by Advance, after Decide, when the new entry replaces the old one (4c4fb6f's class cannot recur).
// S6 is behaviour-neutral: nothing in the game decides by it (S7's executor and switch `situation_arbiter` will).
// Compiled into the controller's logic and, plain, into build\test\fltest.exe (tests\goalqueue_test.cpp).
#pragma once
#include "situation.h"
#include "situation_rules.h"

#include <string>
#include <vector>

namespace goalqueue
{
using situation::Situation;

constexpr int kRecordVersion = 2; // the GoalRecord schema (2: the seen lists left the entry's memory for the story's)
constexpr int kQueueVersion = 2;  // the queue's text (2: seen_alive / seen_listed in the header)

// ---------------------------------------------------------------- the record (A38)
// The goal types (closed; AUTOPILOT_UX.md 3.2). MissionStep = the objective the game shows (its A41 kind in params).
enum class Type
{
    MissionStep,
    FlyTo,
    Dock,
    TradeLane,
    Jump,
    Formation,
    Follow,
    Escort,
    Attack,
    ClearArea,
    Defend,
    Loot,
    Hold,
    Repair,
    Wait,
    BaseAction,
    COUNT
};
enum class State
{
    Pending,
    Active, // the head
    Suspended,
    Done,
    Failed,
    Cancelled,
    COUNT
};
enum class Origin
{
    Story,  // the story mission's objective and its script goals (rebuilt from the live state at every rebuild)
    Player, // the player's own (kept while valid)
    Job,    // a job's objective (rebuilt like the story's)
    COUNT
};
enum class Engage // the engagement policy (AUTOPILOT_UX.md 3.4)
{
    Auto,
    Avoid,
    Protect,
    Fight,
    COUNT
};
const char* TypeName(Type t);
const char* StateName(State s);
const char* OriginName(Origin o);
const char* EngageName(Engage e);

// The subject: a space object (CreateID), a point in a system, a system; `system` 0 = this one / unknown.
struct Ref
{
    enum class Kind
    {
        None,
        Object,
        Pos,
        System,
        COUNT
    } kind = Kind::None;
    u32 id = 0; // Object: the object; System: the system
    u32 system = 0;
    float pos[3] = {};
};
const char* RefKindName(Ref::Kind k);

// Typed parameters (units in the names)
struct Params
{
    float withinM = 0; // an arrival / reach distance (m)
    float radiusM = 0; // an area's radius (clear area, defend, loot; m)
    int count = 0;     // kills / repeats of the type's own unit
    u32 toward = 0;    // a lane ride's direction (the neighbour ring)
    objkinds::GoalType stepKind = objkinds::GoalType::None; // MissionStep: the step's A41 kind
};

// The condition language (closed): one kind and its arguments; a condition is Yes / No / Unknown over the situation
// (Unknown = a fact the snapshot does not hold: never "done", never "failed" - fail closed).
enum class CndKind
{
    True,       // always
    DistObj,    // within `m` of object `id` (cnd_distship)
    DistPos,    // within `m` of `pos` (cnd_distvec / distcircle / locenter)
    Destroyed,  // object `id` dead (cnd_destroyed of one object; Unknown when not among the contacts)
    Docked,     // docked at base `id` (0 = any base) (cnd_baseenter)
    InSystem,   // in system `id` (cnd_systementer / jumpincomplete)
    StepLeft,   // the game's step key is no longer `id` (the objective moved on: the story's own "done")
    LaneRide,   // riding a trade lane (cnd_tlentered)
    Leader,     // in formation with `id` (cnd_playermaneuver formation)
    HullBelow,  // hull fraction below `m`
    ScriptGoal, // the script condition `name` (a trigger) achieved by the situation, or complete in the live table
    COUNT
};
const char* CndKindName(CndKind k);
struct Cnd
{
    CndKind kind = CndKind::True;
    bool negate = false;
    u32 id = 0;
    float m = 0;
    float pos[3] = {};
    std::string name;
};
enum class Tri
{
    No,
    Yes,
    Unknown
};
Tri Eval(const Cnd& c, const Situation& sit);
// done / fail: any of them Yes; when: all of them Yes (empty = always)
Tri AnyOf(const std::vector<Cnd>& cs, const Situation& sit);
Tri AllOf(const std::vector<Cnd>& cs, const Situation& sit);

struct Policy
{
    int knowledge = 0; // the knowledge scope the goal needs: 0 what the player was exposed to, 1 all (the script's own)
    Engage engage = Engage::Auto;
    bool sideEffects = true; // may request docks / lanes / formation
};
struct Display // the game's own strings (ids), rebuilt for the read-back
{
    u32 title = 0, detail = 0;
};

// The entry memory: what today's ObjectiveRun keeps per step (A37_PLAN 1), dying with the step
struct EntryMemory
{
    u32 escortShip = 0;                 // the friendly ship the step last pointed at
    std::string badGoal;                // a script goal whose trip found no way
    std::vector<std::string> deadGoals; // script goals reached without the step advancing (sorted)
    bool arrived = false;               // the step's travel ended
    int arriveRetries = 0;              // the navmarker's retries ...
    bool viaNext = false;               // ... and its via point
    bool cruiseLock = false;
    bool stepAtKnown = false; // the step began at game clock `stepAt` (s)
    double stepAt = 0;
    std::vector<u32> scriptMarks;           // the step's destroy condition's objects counted as marks
    u32 missionLeader = 0;                  // the leader the mission named for a travel
};

enum class Proof // the re-proof of one entry
{
    Valid,
    Achieved,
    Failed,
    Blocked,
    COUNT
};
const char* ProofName(Proof p);
// Why a record is refused / failed / blocked (the reason codes)
enum class Reason
{
    Ok,
    BadVersion,       // a schema version this build does not know
    UnknownType,      // a type outside the closed enum
    UnknownField,     // a state / origin / engage / ref / condition kind outside its enum (parse: an unknown field)
    BadParam,         // a parameter out of range (negative, not finite)
    NoSubject,        // the type needs a subject
    Unresolved,       // the subject / a dependency / the parent names nothing known
    BadSubject,       // the subject cannot be that (a dock that is no base, a formation with a hostile, ...)
    KnowledgeScope,   // the goal needs knowledge the scope does not allow (a script goal under "exposed")
    GameLock,         // the game owns the ship (mission holds, its dock, no ship) or the lane is closed both ways
    NoRoute,          // the subject is in another system with no known route
    BrokenConstraint, // the goal would break a mission constraint (a leash, a never-attack step)
    DependsOn,        // a dependency is not done yet (blocked)
    When,             // its `when` does not hold (blocked)
    FailHolds,        // its fail condition holds
    COUNT
};
const char* ReasonName(Reason r);
struct Verdict
{
    Reason reason = Reason::Ok;
    std::string detail;
    bool ok() const { return reason == Reason::Ok; }
};

struct GoalRecord
{
    u32 id = 0;
    int version = kRecordVersion;
    Type type = Type::Hold;
    State state = State::Pending;
    Origin origin = Origin::Player;
    u32 parent = 0; // the entry this one is a step of (0 none)
    int step = 0;   // its index among the parent's steps
    std::vector<u32> dependsOn;
    Ref subject;
    Params params;
    std::vector<Cnd> when, done, fail;
    Policy policy;
    bool confirm = false; // needs the player's yes before it acts
    int repeat = 0;       // re-armed this many more times when achieved (-1 forever)
    std::string sourceText; // the player's words (natural language, A38 3)
    Display display;
    u32 stepKey = 0; // the game step it stands for (story / job entries)
    EntryMemory memory;
    // the last re-proof (derived at every assess: not part of the text)
    Proof proof = Proof::Valid;
    Reason reason = Reason::Ok;
};

// ---------------------------------------------------------------- the queue
struct StoryMemory
{
    u32 lastReal = 0; // the last objective text that was no placeholder (ObjectiveDecide's lastReal)
    // DestroyFired's objects seen alive / listed (sorted): game scope like the old path's statics - kept in the step's
    // entry they were lost at a step with none (a blank objective): gate s7arb2 M13, Cap1's destroy condition read as
    // unmet after the kill and pre-stage-next flew into NomadCap2's spawn
    std::vector<u32> seenAlive, seenListed;
};
struct MissionMemory // mission scope: the survival stance (autopilot.cpp g_survival*, ObjectiveRun survival)
{
    u32 survTitle = 0;
    bool survMission = false, survEnded = false;
    u32 survSystem = 0;
    bool survival = false;
};
struct GoalQueue
{
    int version = kQueueVersion;
    u32 nextId = 1;
    u32 stepKey = 0; // the step the story entries were last decided for (written by Advance only)
    std::vector<GoalRecord> entries; // ordered; [0] the head
    StoryMemory story;
    MissionMemory mission;
};
// The executor's own state (not the queue's: the running sub-mode, its travel; ObjectiveRun sub* / action, State)
struct ExecMemory
{
    std::string mode = "off";
    u32 target = 0;
    std::string sub = "off";
    u32 subTarget = 0;
    std::string action = "-";
    bool trHasDest = false;
    u32 trDestObject = 0;
    float trDest[3] = {};
    u32 lockId = 0;
    bool lockHave = false;
    float lockP2[3] = {};
    u32 trPassRing = 0;
    u32 escorted = 0;
};

// What the validator needs beyond the situation: the queue (dependencies, parents) and a route oracle (null = no route
// is known to any other system: fail closed; S7's executor answers it from core\game\route.*).
struct Env
{
    const GoalQueue* queue = nullptr;
    bool (*routeKnown)(u32 fromSystem, u32 toSystem) = nullptr;
};

// The schema alone (version, enums, parameters).
Verdict ValidateSchema(const GoalRecord& rec);
// The schema, then the record against the situation: the subject resolved and fit for the type, dependencies / parent
// known, the knowledge scope, a game lock, a route, the mission's constraints. Fails closed: the first refusal.
Verdict Validate(const GoalRecord& rec, const Situation& sit, const Env& env = Env());
// The re-proof: schema refused -> Failed; done -> Achieved; fail -> Failed; a game lock -> Blocked, any other refusal ->
// Failed; `when` not Yes or a dependency not done -> Blocked; else Valid. Writes rec.proof / rec.reason.
Verdict Prove(GoalRecord& rec, const Situation& sit, const Env& env = Env());
void ProveAll(GoalQueue& q, const Situation& sit, const Env& env = Env());
// Achieve semantics: re-proves every entry and pops the achieved head(s) before anything acts - a popped entry's steps
// (children) go with it, its id leaves every dependsOn; a repeat > 0 / -1 re-arms it at the back (its memory fresh),
// at most one pass over the queue. The new head is Active. Returns how many were popped.
int PopAchieved(GoalQueue& q, const Situation& sit, const Env& env = Env());
// The head (null: empty).
const GoalRecord* Head(const GoalQueue& q);

// The story's entries from the situation alone: the step (MissionStep; origin Job for a job's step; its subject, kind,
// text, step key, done = StepLeft) and its script goals as children (the live table's armed goals when the story source
// is live, else the ini view's: FlyTo / Attack, done = ScriptGoal, knowledge 1). None without an objective.
std::vector<GoalRecord> StoryEntries(const Situation& sit, u32& nextId);
// The queue rebuilt from the situation: the story entries rebuilt (nothing of `prev`'s story entries kept but a carried
// "Dock X" entry while valid), `prev`'s player entries kept while valid (re-proven: not Failed), its story / mission
// memory and step key kept. Without `prev`: the cold queue (the situation alone). The rebuilt step entry starts
// `arrived` when its trip has nowhere left to go (WithinArrivalReach: A37 S7, S6's `arrived` exception).
GoalQueue Rebuild(const Situation& sit, const GoalQueue* prev = nullptr, const Env& env = Env());
// The step's own arrival test (A37 S7): the step's trip is over when the ship is within kArrivalReachM of its arrival
// object - a planet's docking ring, else the step's target - for a step that neither docks (its words, an airlock) nor
// names a ship, a lane ring, a gate or a hole; or, for a navmarker step with nothing to go through at the marker (no
// airlock / gate / hole within 200 m: the nav-way's), within kNavReachM of the marker (the supervisor's own "there",
// autopilot_objective.cpp "navmarker still ... off"). Unknown facts (no pilot, the object not among the contacts): no.
constexpr float kArrivalReachM = 1000, kNavReachM = 600;
bool WithinArrivalReach(const Situation& sit);
// The executor's working memory (a rules' Queue: ObjectiveRun's step memory, lastDock, lastReal, the survival stance)
// written into the queue: the step entry for q.stepKey (none: nothing - no entry yet), the carried Dock entry (added /
// replaced / dropped by mem.lastDock), the story and mission memory. `clockSec`: the game clock now (the step's start
// from mem.stepAgeMs). The pause's (D10): the queue as the player's stop leaves it.
void CarryMemory(GoalQueue& q, const situation::Queue& mem, double clockSec);

// ---------------------------------------------------------------- today's executor memory, classed (A37_PLAN 1)
enum class MemClass
{
    Derived,  // derived each time from the situation (seq, the leash anchor)
    Entry,    // entry memory, dying with the step (the story step entry's EntryMemory)
    Story,    // the story's own memory between steps (lastReal; lastDock = a carried "Dock X" entry)
    Mission,  // mission scope (the survival stance)
    Executor, // executor-only (the running sub-mode, its travel, its escort)
    COUNT
};
const char* MemClassName(MemClass c);
struct FieldInfo
{
    const char* name; // situation::Queue's field (the snap text's `queue` key)
    MemClass cls;
    bool (*same)(const situation::Queue& a, const situation::Queue& b);
    void (*copy)(const situation::Queue& from, situation::Queue& to);
};
// Every field of situation::Queue, once, with its class (a test checks the table covers the struct's size).
const std::vector<FieldInfo>& MemoryFields();
// The fields where a and b differ (MemoryFields' order).
std::vector<const FieldInfo*> Differing(const situation::Queue& a, const situation::Queue& b);

// The leash anchor as derived from the situation alone: the last armed, not disarmed ini-view leash whose anchor is a
// ship contact (its object or the ship its pilot's name names) - ScriptBlock's named anchor; 0 none.
u32 DerivedLeashShip(const Situation& sit, float& dist);
// The rules' Queue from a goal queue: derived fields from the situation, the entry memory of the story step entry
// standing for this step (none: fresh), the carried Dock entry's object, the story / mission memory, the executor's.
situation::Queue ToRulesQueue(const GoalQueue& gq, const ExecMemory& ex, const Situation& sit);
// A recorded rules' Queue classed: the goal queue (the story entries rebuilt, the step entry holding the entry memory,
// a carried Dock entry for lastDock, the story / mission memory, step key = this step's) + the executor's state.
struct Classed
{
    GoalQueue queue;
    ExecMemory exec;
};
Classed FromRulesQueue(const Situation& sit, const situation::Queue& q);
ExecMemory ExecOf(const situation::Queue& q);

// After Decide: the decision's writes. A decision (not "held" / "none") for a step whose key the queue does not hold
// yet replaces the story entries (rebuilt, their entry memory fresh) and only then writes the step key; every decision
// keeps the story memory (lastReal) and the carried Dock entry (ObjectiveDecide's lastDock rules). True = a new step.
bool Advance(GoalQueue& q, const Situation& sit, const situation::Proposal& p);

// ---------------------------------------------------------------- D10: the re-proof on resume
struct PauseFacts
{
    double pausedMs = 0;        // how long the player had the ship
    bool systemChanged = false; // another system than at the pause
    bool docked = false;        // docked meanwhile
    bool jobTaken = false;      // a job accepted meanwhile
    float movedM = 0;           // how far the ship is from where it was paused
};
// The silent-resume thresholds: PLACEHOLDERS (A37_PLAN 4: S7 defines and tests them; S9 draws the choice).
struct ResumePolicy
{
    double silentMaxMs = 30000; // kResumeSilentMs: a longer pause asks
    float movedMaxM = 5000;     // kResumeMovedM: flying farther than this meanwhile asks
};
enum class ResumeChoice
{
    Nothing, // the queue is empty after the re-proof (the planner's / after_queue's)
    Silent,  // resume at once
    Ask,     // the goal panel shows the queue with RESUME / CLEAR
    COUNT
};
const char* ResumeChoiceName(ResumeChoice c);
// Silent when the head is valid, the pause short and nothing unrelated was done meanwhile; else Ask; Nothing when empty.
ResumeChoice ResumeChoiceOf(const GoalRecord* head, const PauseFacts& pause, const ResumePolicy& pol, std::string* why);
struct Resumed
{
    GoalQueue queue;
    ResumeChoice choice = ResumeChoice::Nothing;
    std::string why;
    int popped = 0;  // achieved entries popped
    int dropped = 0; // the player's entries no longer valid
};
// D10: every entry re-proven - the story rebuilt from the live state, the player's kept while valid, every achieved one
// popped (not only the head) - then the choice. A silent resume of the same step keeps that step's entry memory (the same step goes on);
// an Ask re-plans it fresh.
Resumed Resume(const GoalQueue& paused, const Situation& sit, const PauseFacts& pause,
               const ResumePolicy& pol = ResumePolicy(), const Env& env = Env());

// ---------------------------------------------------------------- text (`goal v1` records, `goalqueue v1`)
// One record per line, "goal <key>=<value> ..." in a fixed order; parsing fails closed (an unknown, missing, repeated or
// out-of-order field, an unknown enum name, a bad value).
std::string SerializeRecord(const GoalRecord& rec);
bool ParseRecord(const std::string& line, GoalRecord& out, std::string* err = nullptr);
std::string Serialize(const GoalQueue& q);
bool Parse(const std::string& text, GoalQueue& out, std::string* err = nullptr);
} // namespace goalqueue
