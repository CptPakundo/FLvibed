// The situation assessor (IDEAS A37, controller\docs\A37_PLAN.md slice S1): a pure function Snapshot -> Situation.
// A Snapshot is every fact one look at the game gives the objective mode (the objective, the story script's derived
// goals and the live trigger table, the place, the pilot, the holds, the contacts, waypoints, lanes, spawns, the
// settings the rules read and the executor's own state) - plain values, ids and numbers, written by the one impure
// adapter (situation_snap.cpp, SnapshotNow) and kept as text by situation_io.h (`snap v2`). A Situation is what the
// facts mean: the step (its key from the snapshot alone), the placeholder and its stand-in, the step's kind (A41) and
// where that kind came from (A40 job text, the script's NNObjective type + Cnd_, the words), the subject, what holds
// the ship, the threats, the constraints, the script goals with their live state and whether they are achieved, the
// affordances, a hash.
// S1 / S2 are behaviour-neutral: nothing decides by it yet (debug `ap situation [snap|hash]`; the arbiter is S3-S7).
// S2: the story source is the live trigger table when it answers (goals and leashes of the armed triggers, the live
// script as a placeholder's stand-in, the kind by the armed row that sets the next objective), the ini derivation the
// fallback, both views' difference printed.
// Compiled into the controller's logic and, plain, into build\test\fltest.exe: no game, no autopilot state.
#pragma once
#include "common.h"
#include "objective_kinds.h"

#include <string>
#include <vector>

namespace situation
{
// A goal's lead (the trigger nickname its chain leads to) is a landing: a '_' token that starts with "land" (land,
// land_on_leeds, landing_on_osiris, force_land_Osiris, junilands no) or "forceland" - not "land" anywhere in the name
// (audit A1-4: the substring matched mlog_destroy_the_rheinland_forces, badlands_tether_30; DATA\MISSIONS nicknames).
inline bool LeadsToLanding(const std::string& leads)
{
    size_t start = 0;
    while (start <= leads.size())
    {
        size_t end = leads.find('_', start);
        if (end == std::string::npos)
            end = leads.size();
        const std::string tok = leads.substr(start, end - start);
        std::string low;
        for (const char c : tok)
            low += static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
        if (low.compare(0, 4, "land") == 0 || low == "forceland")
            return true;
        start = end + 1;
    }
    return false;
}

// 2 (A37 S2): the story source (goals / leashes from the live trigger table, the ini view beside them), each live row's
// lead. Version 1 snapshots still parse (S1's fixtures) and assess exactly as before (the ini view).
// 3 (A37 S3): the facts the arbiter's first rules read beyond S2's (the objective's resolved refs, the goals each ini goal
// arms, the destroy goals' object positions, the leashes' anchors and disarming, the system entry, the tractor's reach,
// tractor_exit_*), the executor's memory those rules read (`queue`) and the decision the old path made (`decided`). The
// situation (Describe, the hash) is unchanged: v1 / v2 texts hash as before.
// 4 (A37 S4): the facts the pre-stage, mark-fight, pre-aggro, survival and escort rules read (the goals each ini goal's
// step leads to - `after`, the next step's goals and places from the objective mode's last real objective - `prestage` /
// `realnext`, the title's escape words, the pilot's system, each pointed contact's obstacle radius and whether the step's
// text names it, the survival / big-coast settings) and the memory they read (`queue ... last_real= arrived= step_age=
// escorted= surv_*=`). The situation is unchanged again: every older text hashes as before.
// 5 (A37 S5): the facts the lane / pass / route / arrive / navmarker / rest rules read (an entry's first resolved ref's
// position - `ref_placed= ref_pos=`, a lane ring's neighbours' positions, a pointed / referenced solar's planet dock ring -
// `dock_ring=`, follow_distance) and the memory (`queue ... mission_leader= via_next= arrive_retries= tr_pass=`). A
// shadowed decision's waypoint list is the one as the decision began (a restart's stop restores the player's list before
// the snapshot). The situation is unchanged: every older text hashes as before.
constexpr int kSnapVersion = 5;

// ---------------------------------------------------------------- the snapshot (facts)
// The objective's words as a verb (the adapter reads the text, the snapshot keeps only the verb: ids and numbers).
enum class Verb
{
    None,
    Dock,
    Escort,
    Fight,
    Survive
};
const char* VerbName(Verb v);
Verb VerbFromName(const std::string& s);
// The objective mode's English keywords (autopilot_objective.cpp ObjectiveVerb, the same rules): lower-cased words at
// word boundaries. Other languages: None.
Verb VerbOf(const std::string& text);

struct SnapStep // one objective entry (HostObjectiveNow / HostObjectiveSteps)
{
    u32 textId = 0;
    u32 refs[4] = {};
    int refCount = 0;
    u32 refObj[4] = {}; // each ref resolved to a space id (space::ObjectiveObject over the contacts), 0 none
    bool hasNav = false;
    float nav[3] = {};
    u32 navSystem = 0;
    Verb verb = Verb::None; // VerbOf(the step's text)
    std::string job;        // jobs::KindOf(the text id): "" none, "ambiguous", or the job kind
    int jobRows = 0;        // vignetteparams.ini rows using the id
    // v5: the first resolved ref's position (space::ObjectPos; the route-lane rule's step 1), placed = the game answered
    bool refPlaced = false;
    float refPos[3] = {};
};

struct SnapObjDef // an [NNObjective] naming the step's text (mission::ObjectiveDefs)
{
    std::string nick, type, script; // script relative to DATA\MISSIONS ("M07\m07.ini")
};

struct SnapGoalObj
{
    u32 id = 0;
    bool placed = false; // the game still places it (space::ObjectPos)
    float pos[3] = {};   // v3: where (placed only)
};

struct SnapGoal // a script condition (mission::Goal), with the adapter's reads of its objects
{
    int kind = 0; // 1 within dist of a solar (cnd_distship), 2 of a point (cnd_distvec), 3 destroyed (cnd_destroyed)
    u32 object = 0;
    std::string objectName;
    float pos[3] = {};
    float dist = 0;
    int count = 0;
    std::string subject, trigger, leads, file; // file relative to DATA\MISSIONS
    bool byEntry = false;
    bool placed = false; // kind 1: the solar's position is known (objPos)
    float objPos[3] = {};
    std::vector<SnapGoalObj> objects; // kind 3
};

struct SnapLeash // mission::Leash
{
    u32 object = 0;
    std::string name;
    float dist = 0;
    u32 nameIds = 0;
    std::vector<u32> offComms;
    // v3 (the objective mode's leash anchor, ObjectiveDecide's script-goal block): disarmed - one of offComms heard in the
    // last 30 min (wall time, HostCommHeardAt); the ship its pilot's name names - searched only when the object is no ship
    // contact: the last ship contact with that name id or reputation name text, 0 none; the object's position
    bool off = false;
    u32 ship = 0;
    bool placed = false;
    float opos[3] = {};
};

// v3: a goal an ini-view goal's trigger arms (mission::GoalsArmedBy(by, its script)) - the objective mode's fly-through
// and fired-condition trips
struct SnapArmed
{
    std::string by, byFile; // the arming goal's trigger and script (relative, as SnapGoal::file)
    SnapGoal goal;
};

// v3 (A37 S3): the executor's memory the arbiter's ported rules read - today's ObjectiveRun / State fields, taken as
// ObjectiveDecide began (after its step-memory upkeep: the seq reset, a dead goal noted); S6's goal queue replaces it.
// Modes and actions by their names (autopilot ModeName / ActionName): the assessor knows no autopilot type.
struct Queue
{
    u32 seq = 0;
    std::string mode = "off"; // the running mode and its target
    u32 target = 0;
    std::string sub = "off"; // the objective mode's last start (ObjectiveRun sub / subTarget / action)
    u32 subTarget = 0;
    std::string action = "-";
    bool trHasDest = false; // the running travel: its destination, object, the airlock lock it keeps
    u32 trDestObject = 0;
    float trDest[3] = {};
    u32 lockId = 0;
    bool lockHave = false;
    float lockP2[3] = {};
    bool cruiseLock = false;
    u32 leashShip = 0; // an armed ship leash's anchor and its distance
    float leashDist = 0;
    u32 lastDock = 0;   // the last Dock objective's target, kept while only a placeholder follows it
    u32 escortShip = 0; // the friendly ship an objective last pointed at
    std::string badGoal;                // a script goal whose trip found no way this step
    std::vector<std::string> deadGoals; // script goals reached without the step advancing (sorted)
    std::vector<u32> scriptMarks;       // the step's destroy condition's live objects counted as marks (scriptMarkN)
    std::vector<u32> seenAlive, seenListed; // the destroy conditions' objects seen alive / in the contact list (sorted)
    // v4 (A37 S4): ObjectiveDecide's `lastReal` (the last objective text it saw that was no placeholder - a hidden static);
    // this step's travel ended (arrived); game ms since the step began (-1: no step start recorded); the escort's ship
    // (State::escorted); the mission-scope survival memory (g_survivalTitle / Mission / Ended / System) and the stance
    u32 lastReal = 0;
    bool arrived = false;
    double stepAgeMs = -1;
    u32 escorted = 0;
    u32 survTitle = 0;
    bool survMission = false, survEnded = false;
    u32 survSystem = 0;
    bool survival = false;
    // v5 (A37 S5): the leader the mission named for a travel (ObjectiveRun missionLeader), the navmarker's via point
    // retry (viaNext, arriveRetries), the ring the running travel flies through (State::Travel passRing)
    u32 missionLeader = 0;
    bool viaNext = false;
    int arriveRetries = 0;
    u32 trPassRing = 0;
};

// v4: the goals the step an ini-view goal leads to arms (mission::GoalsAfter(leads, its script)) - the pre-stage's
struct SnapAfter
{
    std::string leads, file; // the leading goal's `leads` and script (relative, as SnapGoal::file)
    SnapGoal goal;
};

// v4: the next step's facts from the objective mode's last real objective (pre-stage-next, survival_stage): lastReal
// after this decision's update (the step's own text when it is no placeholder), NextStepGoals' next objective (its goals
// are the `realnext` records), NextObjectivePlace from it (`next_*`) and from the survival stage's text with two
// objectives skipped (`stage_*`: the step's text, else lastReal)
struct SnapPrestage
{
    u32 lastReal = 0;
    std::string next;
    bool nextOk = false;
    float nextPos[3] = {};
    u32 nextText = 0;
    u32 stageFrom = 0;
    bool stageOk = false;
    float stagePos[3] = {};
    u32 stageText = 0;
};

// v3: the decision the old path (ObjectiveDecide) made - its sub-mode, target and action (ObjectiveRun, ModeName /
// ActionName); `rule` the branch that took it ("" = read off the objective run, not at a decision)
struct Decided
{
    std::string rule;
    std::string mode = "off";
    u32 target = 0;
    std::string action = "-";
    bool keep = false; // the branch kept the running sub-mode (no new start)
};

struct SnapNag // mission::TowardsNag
{
    bool has = false;
    bool pos = false;
    u32 object = 0;
    std::string name;
    float p[3] = {};
    float dist = 0;
};

struct SnapLive // one row of the live trigger table (mission::LiveTriggers)
{
    u32 id = 0;
    int state = -1; // 0 armed, 1 inactive, 2 complete
    std::string cond, nick, iniCond;
    std::string leads; // v2: what its chain leads to (mission::LeadsOfLive: an objective, SUCCEED, FAIL, "")
    int hops = -1;     // v2: the hop it was found at (0 its own actions)
};

struct SnapContact // space::Contact (+ the pre-aggro overlay) and, for non-ships, the reputation's attitude
{
    u32 id = 0, type = 0, nameIds = 0, base = 0;
    bool ship = false, dock = false, targetable = false;
    float pos[3] = {};
    float distance = 0, health = -1, attitude = 0;
    bool attitudeAsked = false;
    bool attKnown = false; // non-ships: space::AttitudeToPlayer answered (att)
    float att = 0;
    u32 targetId = 0;
    bool targetsPlayer = false;
    float maxHit = 0;
    int rep = 0;
    bool dockPort = false, jumping = false, lane = false, preHostile = false;
    u32 player = 0;
    // v4, for the contacts the mission points at (marks + resolved refs) only: a non-ship's obstacle radius
    // (space::Obstacles round it, 0 none / not read - big_coast's huge static) and, for a ship, whether a 4+ letter word of
    // its speaker name is a word of the step's text (the escort's named ship)
    float radius = 0;
    bool named = false;
    // v5, for the contacts the mission points at and the objective's entries' resolved refs: a solar's planet docking
    // ring (space::PlanetDockRing, 0 none / not read; the arrive rule's destination). (v5 also reads `radius` for the
    // entries' refs: the pass rule's ring.)
    u32 dockRing = 0;
};

struct SnapLane // a lane ring among the contacts: its links and the mission's permission (space::LaneLinks / LaneOpen)
{
    u32 ring = 0, next = 0, prev = 0;
    int openNext = -1, openPrev = -1; // 1 open, 0 closed, -1 unknown / no neighbour
    // v5: the neighbours' positions (space::ObjectPos; the route-lane rule), placed = the game answered
    bool nextPlaced = false, prevPlaced = false;
    float nextPos[3] = {}, prevPos[3] = {};
};

struct SnapWaypoint
{
    float pos[3] = {};
    u32 system = 0, object = 0;
};

struct SnapSpawn
{
    u32 id = 0, arch = 0;
    float pos[3] = {};
};

struct Snapshot
{
    int version = kSnapVersion;
    // the objective
    u32 objSeq = 0;
    SnapStep step;
    std::vector<SnapStep> entries; // every entry of the objective (route steps; a job's blank entry 0)
    u32 title = 0;
    std::vector<u32> marks; // the host's mission marks, newest first
    std::vector<u32> refsObj; // v3: the objective's 'R' values (HostObjectiveRefs) as space ids, in order, 0 dropped
    // the story script (mission.cpp's derived facts, read without writing its memory)
    std::vector<SnapObjDef> objDefs;
    // the story source (v2): true = `goals` / `leashes` are the live table's (mission::GoalsOfLive / ArmedLeashes; the
    // table ok with a script), `iniGoals` / `iniLeashes` the ini view beside them; false = `goals` / `leashes` are the ini
    // view (AdvanceGoalsPeek / Leashes: the table failed closed - multiplayer, another build, no story) and ini* empty
    bool goalsLive = false;
    std::vector<SnapGoal> goals;
    std::vector<SnapGoal> iniGoals;
    bool goalsFromLast = false, goalsByTitle = false; // (the ini view's stand-in, either source)
    u32 standInText = 0;
    std::string goalsScript; // the script the ini view's goals come from
    std::vector<SnapLeash> leashes;
    std::vector<SnapLeash> iniLeashes;
    SnapNag nag;                     // the step's towards nag
    std::vector<SnapGoal> nextGoals; // NextStepGoals
    std::string nextObjective;
    SnapNag nextNag;
    std::vector<u32> pendingHostile; // PendingHostileShips
    std::vector<SnapArmed> armed;    // v3: the goals each ini-view goal (kind 1 / 2) arms
    std::vector<SnapAfter> after;    // v4: the goals of the step each ini-view goal leads to (by leads + script, once)
    SnapPrestage prestage;           // v4
    std::vector<SnapGoal> realNext;  // v4: NextStepGoals(prestage.lastReal)
    bool titleEscape = false;        // v4: the title's text has "escape" / "flee" / "survive" (lower case)
    // the live trigger table (A39)
    bool liveOk = false;
    std::string liveReason, liveScript;
    u32 liveController = 0;
    float liveMs = 0; // its read time (ms; a measurement, not assessed)
    std::vector<SnapLive> live;
    // place and game state
    u32 system = 0, base = 0;
    bool inSpace = false;
    double clock = 0; // the game clock (s; not assessed)
    bool hasEntry = false; // v3: where the ship first showed in this system (SystemEntry: an airlock's or a jump's way in)
    float entry[3] = {};
    // pilot and ship
    bool pilotOk = false;
    int op = -1;
    bool locked = false, laneRide = false, jumping = false, cruise = false;
    u32 laneEntry = 0, laneExit = 0, leader = 0, target = 0;
    float pos[3] = {}, vel[3] = {};
    float hull = -1, shield = -1;
    float tractorRange = 0; // v3: the tractor's reach (ai::Self; 0 unknown)
    u32 pilotSystem = 0;    // v4: space::Pilot::system (the survival stance's system test)
    // holds and the game's answers
    bool missionHolds = false, dockInProgress = false;
    double sinceGrantMs = -1; // since a dock grant we did not request (a mission's Act_DockRequest); -1 none
    u32 dockReplySeq = 0, dockReplyShip = 0, dockReplyKind = 0, dockReplyResponse = 0;
    u32 resultSeq = 0, resultType = 0;
    // the world
    std::vector<SnapContact> contacts; // the 48 nearest + every marked / referenced / goal / leash object
    std::vector<SnapLane> lanes;
    std::vector<SnapWaypoint> waypoints;
    std::vector<SnapSpawn> spawns; // since the last menu, the newest 64
    int spawnCount = 0;
    // knowledge scope and the settings the rules read
    int knowledge = 0;
    bool scriptGoals = true, staleDestroyWait = true, prestageNext = true, airlockFlyThrough = true;
    float preAggro = 0;
    bool tractorExitRun = false; // v3: tractor_exit_run, tractor_exit_range
    float tractorExitRange = 600;
    bool bigCoast = true, survivalCalm = true, survivalStage = true, survivalUntilJump = true; // v4
    float followDistance = 1500; // v5: follow_distance (the survival stance's rest beside its lead)
    // the executor's own state (memory the arbiter will replace: reported, assessed only where named)
    int apMode = 0; // autopilot::Mode
    u32 apTarget = 0;
    bool objOn = false;
    bool survival = false; // the mission-scope survival stance (assessed: the threat rules)
    bool ourDock = false;  // our own dock request runs (assessed: a dock that is ours is no game dock)
    Queue queue;           // v3: the memory the rules read (not assessed: the rules take it beside the situation)
    Decided decided;       // v3: the old path's decision with this snapshot (not assessed)
};

// ---------------------------------------------------------------- the situation (meaning)
enum class Placeholder
{
    None,        // a real step
    NoObjective, // text 0
    Awaiting,    // 21660 "Awaiting Mission Objective"
    Blank        // text 1 (" "), or a text only "null" / "nn_blank" objectives name
};
enum class StandIn
{
    None,
    Live,    // the live trigger table's script (S2: the snapshot's story source is live)
    IniLast, // the last objective the script set (mission.cpp's memory)
    Title    // the mission's title (a mid-mission load)
};
enum class KindSource
{
    None,
    Job,    // the job's objective_text id (A40)
    Script, // the [NNObjective] type + the step's Cnd_
    Verb,   // the objective's words
    Unknown
};
enum class SubjectSource
{
    None,
    Ref,  // the objective's target ('R' value)
    Nav,  // its navmarker
    Mark, // no target, no navmarker: the newest live friendly mark (the words name it)
    Goal  // the script goal that stands in
};
enum class Held
{
    None,
    Docked,       // in a base
    NoShip,       // not in space and not docked (menus, loading)
    MissionHolds, // the mission flies the player (locked maneuvers / a priority directive)
    GameDock,     // the game docks the ship on a mission's request
    Lane,         // riding a lane
    Jump          // jumping
};
enum class ThreatWhy
{
    Pointed,   // a hostile the mission points at within 6 km
    Attacker,  // a hostile ship attacking us within 3 km (survival: 1.2 km)
    Battle,    // a hostile ship attacking our side within the battle's reach
    PreHostile // a ship the script is about to turn hostile (pre-aggro overlay)
};
enum class Achieved
{
    No,
    Yes,
    Unknown
};
const char* PlaceholderName(Placeholder p);
const char* StandInName(StandIn s);
const char* KindSourceName(KindSource k);
const char* SubjectSourceName(SubjectSource s);
const char* HeldName(Held h);
const char* ThreatWhyName(ThreatWhy t);
const char* AchievedName(Achieved a);

struct Threat
{
    u32 id = 0;
    ThreatWhy why = ThreatWhy::Pointed;
    float distance = 0;
};

struct Constraint
{
    enum class Kind
    {
        Leash,      // the player farther than dist from the object fails the mission
        Nag,        // not closing on the object / point fails it
        NeverAttack // a job's *_dont_kill_ships step
    } kind = Kind::Leash;
    u32 object = 0;
    std::string name;
    float dist = 0;
    int armed = -1; // -1 unknown (the ini view: mission.cpp does not track a leash's arming), 1 armed (live: state 0)
};

struct GoalView
{
    std::string source;  // "live" (GoalsOfLive), "ini" (AdvanceGoals) or "next" (the next step's, NextStepGoals)
    std::string trigger; // the trigger whose condition it is
    int kind = 0;
    std::string cnd; // the Cnd_ it is (cnd_distship / cnd_distvec / cnd_destroyed)
    std::string leads;
    int live = -2;  // its row in the live trigger table: 0 armed, 1 inactive, 2 complete, -1 absent, -2 no table
    Achieved achieved = Achieved::Unknown;
    float distance = -1; // to its sphere's centre / nearest live object (m), -1 unknown
};

struct Affordances
{
    bool dock = false;    // the subject can be docked with
    int lane = -2;        // the subject is a lane ring: 1 the mission lets the player ride it, 0 closed, -1 unknown; -2 no ring
    bool jump = false;    // a jump gate / hole
    bool airlock = false; // an airlock gate
};

struct Situation
{
    u32 stepKey = 0;  // the objective mode's step key (ObjectiveSupervise): text, the first ref, the navmarker
    u32 marksKey = 0; // the host's mission marks, sorted
    u32 step = 0;     // both: the step as the snapshot alone names it
    u32 textId = 0;
    Placeholder placeholder = Placeholder::None;
    bool known = false; // a script or the job table names the text
    StandIn standIn = StandIn::None;
    u32 standInText = 0;
    objkinds::GoalType kind = objkinds::GoalType::None;
    KindSource kindSource = KindSource::None;
    std::string kindDetail; // what decided the kind ("job guide_to_main_battle", "rep_inst+lane_ring", "verb dock")
    Verb verb = Verb::None;
    SubjectSource subjectSource = SubjectSource::None;
    u32 subject = 0;
    bool subjectHasPos = false;
    float subjectPos[3] = {};
    float subjectDist = -1;
    Held held = Held::None;
    std::vector<Threat> threats; // nearest first
    std::vector<Constraint> constraints;
    std::vector<GoalView> goals;
    int liveArmed = -1; // armed rows of the live table (-1 no table)
    // the story source (v2 snapshots; v1: version 1, nothing below printed): live or ini, why not live, and the live
    // view against the ini view - goals by script + trigger, leashes by object + distance ("live" / "ini" = only there)
    int snapVersion = kSnapVersion;
    bool storyLive = false;
    std::string storyReason;
    struct Diff
    {
        std::string what; // "goal" / "leash"
        std::string only; // "live" / "ini"
        std::string name; // goal: its trigger; leash: the object's name
        std::string detail; // goal: what it leads to; leash: the distance
    };
    std::vector<Diff> diff;
    int diffBoth = 0; // goals + leashes in both views
    Affordances afford;
    u32 hash = 0; // FNV-1a over Describe()'s lines after the first
    // the facts it was assessed from (A37 S3: the arbiter's rules read the contacts, goals and leashes through it; not
    // described, not hashed)
    Snapshot facts;
};

// The rules' constants (today's: autopilot_objective.cpp ObjectiveThreat, autopilot_frame.cpp GameDocksUs).
struct AssessPolicy
{
    float pointedReach = 6000;  // a hostile the mission points at
    float attackerReach = 3000; // a hostile ship attacking us
    float survivalReach = 1200; // ... under the survival stance (kSurvivalReach)
    float battleReach = 15000;  // hostiles attacking our side (kBattleReach)
    float airlockReach = 3000;  // a granted airlock is flown through, not waited at (airlock_fly_through)
    double grantDockMs = 4000;  // a mission's dock grant holds the ship this long (kGameGrantDockMs)
    float hostileAttitude = -0.6f;
};

Situation Assess(const Snapshot& s, const AssessPolicy& policy = AssessPolicy());
// The situation as text (`ap situation`): "situation v1 hash=0x........" then one line per part.
std::string Describe(const Situation& sit);
// The objective mode's step key, as ObjectiveSupervise computes it (the same expression: a cross-check in game).
u32 LegacyStepKey(const SnapStep& st);

// ---------------------------------------------------------------- the adapter (situation_snap.cpp, the game only)
// The snapshot now (the game thread; in space, docked or in menus) - reads only, writes no memory of the autopilot or
// of mission.cpp. `queue`: the executor memory to record (the shadow's, taken as the decision began); null = now's.
Snapshot SnapshotNow(const Queue* queue = nullptr);
// The executor memory the rules read, now (QueueNow: from the objective run and the running travel).
Queue QueueNow();
// Debug `ap situation [snap|hash|last]` (autopilot_settings.cpp): the situation, the snapshot text (with the in-game
// hash as its last comment line), the hash of both the live and the re-parsed snapshot, or the snapshot of the last
// shadowed decision (situation_shadow; its `decided` record = the old path's decision, its `#shadow` line the verdict).
std::string Command(const std::string& sub);
// The shadow (`situation_shadow`, A37 S3; the callers test the setting, so with it off nothing runs): ShadowEntry as
// ObjectiveDecide begins (the executor memory then, and the waypoint list); ShadowCompare at an exit the ported rules
// cover (S3-S5: Decide - every exit of ObjectiveDecide since S5), after its start - the old
// branch and its decision against the rules' proposal over a snapshot of now with that memory, one event
// `situation shadow agree|differ <branch> ...`; ShadowHeld at ObjectiveSupervise's holds (at most once a second, an
// event only when its line changes).
void ShadowEntry();
void ShadowCompare(const char* branch, const char* mode, u32 target, const char* action, bool keep, bool started);
void ShadowHeld(const char* why);
} // namespace situation
