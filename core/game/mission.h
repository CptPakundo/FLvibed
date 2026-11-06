// Story mission scripts (DATA\MISSIONS\Mxx\*.ini, plain text): what the current step's armed triggers really wait for -
// the game's own conditions read from the install, never a copy (the top-3 speedruns: "an
// objective is a trigger, not an order" - M12's "Destroy all of the Nomad Ships" advances the moment the player is within
// 8 km of Toledo, M08's "Fly to the Waypoint" when Ozu, who follows the player, is within 9 km of the jump hole;
// docs\research\speedrun.md V3, IDEAS A21). These are script-derived facts; which caller uses them under which setting
// is the caller's business.
//
// The script's runtime state (which triggers are armed) is not read from the game; it is derived from the data: the
// triggers armed by the current objective's setter, by the trigger that armed the setter, and by the triggers that fire
// on entering the player's system (with their Cnd_True / Cnd_Timer children), restricted to the player's system.
// The game's own list of armed triggers is readable: mission_live.h (single player). GoalsOfLive /
// ArmedLeashes / LeadsOfLive below read the goals from it instead (A37 S2: the situation assessor's story source; the
// autopilot's AdvanceGoals path does not use them yet).
#pragma once
#include "common.h"

#include <string>
#include <vector>

namespace mission
{
// A mission ship a kind 4 goal names (LOOK): its [MsnShip] nickname, its pilot's name and where the script spawns it.
struct ShipRef
{
    std::string nick;   // [MsnShip] nickname (lower)
    u32 nameIds = 0;    // its NPC's individual_name (the pilot's name as the game shows it), 0 none
    bool hasPos = false; // the spawn position is known: the [MsnShip]'s, else its [MsnFormation]'s
    float pos[3] = {};
};

struct Goal
{
    int kind = 0;             // 1: within `dist` of the solar `object`; 2: within `dist` of the point `pos`;
                              // 3: `count` of the mission solars `objects` destroyed;
                              // 4 (LOOK, only through a reader's `look` argument - every other reader leaves it out, the
                              // decision sweep unchanged): `count` of the mission SHIPS `ships` destroyed (Cnd_Destroyed by
                              // an [MsnShip] label or nickname; count -1 = all of them, Content.dll: LOOK_PLAN 4a)
    u32 object = 0;           // kind 1: the solar's id (CreateID of its nickname)
    std::string objectName;   // kind 1: its nickname as the script writes it; kinds 3, 4: the label / nickname
    float pos[3] = {};        // kind 2
    float dist = 0;           // m
    std::vector<u32> objects; // kind 3: the [MsnSolar]s named or labelled so (space id = CreateID of the nickname)
    int count = 0;            // kinds 3, 4
    std::string subject;      // "player", or the ship (that follows the player) the condition names
    std::string trigger;      // the trigger whose condition this is
    std::string leads;        // what it leads to: an objective nickname, or "SUCCEED"
    std::string file;         // the script (full path)
    bool byEntry = false;     // armed only through a system-entry trigger, not the step's own objective chain (whether
                              // it is live during the step is unknown: M05 / M13 fights, session 8)
    // ---- LOOK's extras: filled only by a reader called with `look` (modules\autopilot\docs\LOOK_PLAN.md, L0)
    std::vector<ShipRef> ships; // kind 4: the ships the label / nickname names (runtime ids: ShipIdsOf)
    // kind 4: the script's health gate - the highest Cnd_HealthDec fraction on one of its ships (or the label) whose
    // immediate chain (Cnd_True hops) destroys it (M02: ashcroft_health_35 -> destroy_him -> Act_Destroy = Ashcroft); 0
    // none. `gateFails`: such a gate's immediate chain fails the mission.
    float healthGate = 0;
    bool gateFails = false;
    // kind 4: an Act_Invulnerable = <ship | label>, true, [b], [f] anywhere in the script (the first): its second flag and
    // its 4th field (f, read by FLvibed as a hull floor - not verified, LOOK_PLAN 4a), and the trigger that sets it
    bool invulnerable = false, invSecond = false;
    float invFloor = 0;
    std::string invTrigger;
    // kinds 1, 2: a lane ride the trigger's chain waits for next (Cnd_TLEntered = Player, <from>, <toward> within 4 hops:
    // M05's dst_to_br04_tradelane -> tle_Br04_3); "" none
    std::string laneFrom, laneToward;
    int leadsOrder = -1; // `leads`' index among the script's [NNObjective]s (the mission's flow); SUCCEED: their count
    int hops = 0;        // ChainGoals: the hop from the step's setter that arms it (1: the setter itself)
    // kinds 3, 4: the Cnd_Destroyed's 4th field - the killer it counts (lower: "player"), "" any killer (M13 check_cap =
    // NomadCap3, 1, ALL: the allies' kill counts; M06 d_dexter = dexter, 1, EXPLODE, Player)
    std::string killer;
};

// The flying conditions that advance the current step: armed triggers (see above) with a single condition - the player
// (or a ship that follows the player) within a distance of a solar or a point, or N of the mission's solars destroyed
// (Cnd_Destroyed by label or nickname; M13's barrier generators advance "Head to the Barrier") - whose action chain (up
// to 4 hops of Act_ActTrig) sets a new objective or succeeds the mission, and never fails it (a destroy condition: a
// failure only through triggers that fire at once - one the chain merely arms, M13's "outside 18 km -> FAIL", is a
// watcher). `textId` = the current objective's text
// (an id no script knows - "Awaiting Mission Objective" - means the last one found this mission); `system` = the player's
// system id. `fromLast` (optional) is set when the last known objective stood in for textId.
// `titleId` (optional): the mission's title id - when no objective is known yet (a save loaded mid-mission), the script
// that sets that title, from its system-entry triggers alone.
std::vector<Goal> AdvanceGoals(u32 textId, u32 system, bool* fromLast = nullptr, u32 titleId = 0);
// What AdvanceGoals would answer now, read only (the situation snapshot, modules\autopilot\src\situation_snap.cpp, A37 S1):
// mission.cpp's memory (the last script and text found, the objectives shown) is read, never written - the autopilot's
// next AdvanceGoals call answers exactly as it would have without this one.
struct AdvanceInfo
{
    bool fromLast = false;  // the last known objective (or the title) stood in for the text
    bool byTitle = false;   // the stand-in is the mission's title (no objective known yet: a mid-mission load)
    u32 standInText = 0;    // the last known objective's text when it stood in (0 otherwise)
    std::string script;     // the script the goals come from (full path), "" none
};
// `look` (LOOK L0): also kind 4 and the look extras (Goal).
std::vector<Goal> AdvanceGoalsPeek(u32 textId, u32 system, u32 titleId, AdvanceInfo& info, bool look = false);
// A new game (a save load, New Game, the load after a death; the controller logic calls it when HostApi v34 GameLoads
// moves): AdvanceGoals' memory - the last script and text found (the placeholder's stand-in, Leashes) and the objectives
// each script has shown - belongs to the game before and is forgotten (A37 S1: after SaveS208 -> SaveS182
// M07's last objective could stand in for M12's "Awaiting").
void ForgetGame();
// Tests only: the scripts of `missionsDir` (a MISSIONS folder; "" none) instead of the install's, memory forgotten.
void UseScriptsFrom(const std::string& missionsDir);
// The [NNObjective]s whose type names text `textId` (any script): nickname (lower), type ("navmarker", "rep_inst",
// "ids"; lower) and script (full path). Empty when no script knows the text.
struct ObjectiveDef
{
    std::string nick, type, script;
};
std::vector<ObjectiveDef> ObjectiveDefs(u32 textId);
// The goals of the step a goal leads to (`objective` = its Goal::leads nickname, `file` = its Goal::file), as they will be
// armed once that objective is set: the look-ahead for a step whose own condition has fired while the script still
// talks (M12: "within 2.7 km of the lair" -> ~31 s of dialogue -> "Destroy all the Shield Generators"; the runners wait
// at generator 1 and fire the second the banner shows).
std::vector<Goal> GoalsAfter(const std::string& objective, const std::string& file, u32 system, bool look = false);
// The goals a fired trigger (`trigger`, of `file`) arms - the next level of the chain (M12 Omicron Minor:
// "within 8 km of Toledo" fired, its chain arms "within 3 km of the ring" -> "Dock with Planet Toledo"; the ship stood by
// Juni with nothing to fly to).
std::vector<Goal> GoalsArmedBy(const std::string& trigger, const std::string& file, u32 system, bool look = false);
// LOOK L0: the goals the current step's chain arms now or is about to arm - forward from the setters of the objective with
// text `textId` (not past a trigger that sets a new objective), up to 4 hops whatever their conditions (comms complete,
// distances close: they come), in `system`, each with `hops` and the look extras (kind 4 included). M05 "mlog_22890":
// player_near_br04_01 (hop 1) and dst_to_br04_tradelane (hop 3, after two comm lines). A placeholder text: none.
std::vector<Goal> ChainGoals(u32 textId, u32 system);
// A leash: the player more than `dist` from the ship `object` (its space id; `name` as the script writes it) fails the
// mission (Cnd_DistShip = outside, Player, <ship>, d with Act_ChangeState FAIL in its chain).
struct Leash
{
    u32 object = 0;
    std::string name;
    float dist = 0;
    u32 nameIds = 0; // a mission ship: its NPC's individual_name (the pilot's name as the game shows it); 0 otherwise
    std::vector<u32> offComms; // comm lines (CreateID) whose Cnd_CommComplete trigger deactivates it (M10 botzler_dies)
};
// The leashes of the current mission's script (the one AdvanceGoals last found) in `system` (M10:
// "Awaiting" -> a script goal 17.6 km off, more than 6 km from Von Claussen -> "You fled the battle"). Whether one is armed
// is not tracked: the caller keeps to all of them.
std::vector<Leash> Leashes(u32 system);
// A tether (Cnd_TetherBroke = <shipA>, <shipB>, <dist>, <secs>[, <inner>]; Content.dll, LOOK_PLAN 4a - decompiled, not
// verified in game): while shipA is outside `dist` of shipB the mission clock accrues, cumulatively (never reset on
// rejoin); inside `inner` (0 none given) it accrues only while not closing; the trigger fires at `secs`. Kept beside the
// leashes, never merged into them (LOOK_PLAN 2: merging would change M04 for everyone): L0 only reports them.
struct Tether
{
    std::string a, b;   // as the script writes them (M04: "juni", "Player")
    u32 aId = 0, bId = 0; // CreateID of each (a mission ship's runtime id is not its nickname's: aNameIds)
    u32 aNameIds = 0;   // shipA a mission ship: its NPC's individual_name; 0 otherwise
    float dist = 0, secs = 0, inner = 0;
    std::string trigger; // the [Trigger] nickname
    bool fails = false;  // its chain (4 hops, Act_ActTrig) fails the mission (M04's bounty_hunter_nag_30 -> 60 -> 90)
};
// The tethers of the script `file` (full path; "" = the one AdvanceGoals last found) in `system` (a trigger without its own
// system: its arming trigger's), in the script's order - armed or not.
std::vector<Tether> Tethers(const std::string& file, u32 system);

// ---- the live trigger table as the story source (IDEAS A39, A37 S2, 30 Oct 2026; mission_live.h LiveTriggers) ----
// These read the game's own armed set instead of deriving it from the script data, and keep no memory: no last script,
// no objectives shown (a load leaves nothing stale). The table's script is its `script` (the one NameTriggers found for
// its ids); a row whose id that script lacks is skipped. Not ok / no script: empty (the caller falls back to the ini
// view, AdvanceGoals / Leashes).
struct LiveTable;
// The goals of the armed rows (state 0): each with a single condition AdvanceGoals would take - Cnd_DistShip /
// Cnd_DistVec inside for the player or a ship that follows the player, Cnd_Destroyed of the mission's solars - parsed from
// its [Trigger] line by the same rules, whose chain leads to an objective other than the step's own (`textId`'s
// objectives in that script; 0 none) or SUCCEED and never fails it; restricted to `system` (a trigger without its own
// system: its arming trigger's, as AdvanceGoals). byEntry is false (the game armed it). In the table's order.
std::vector<Goal> GoalsOfLive(const LiveTable& t, u32 system, u32 textId = 0);
// The leashes (as Leashes) of the table's script in `system` whose trigger is armed (state 0) - a leash's arming known.
std::vector<Leash> ArmedLeashes(const LiveTable& t, u32 system);
// The tethers (as Tethers) of the table's script in `system` whose trigger is armed (state 0).
std::vector<Tether> ArmedTethers(const LiveTable& t, u32 system);
// Per row of t.triggers (same order): what its action chain leads to within 4 hops (an objective nickname other than
// `textId`'s, "SUCCEED", "FAIL", or "" nothing / the script lacks the id) and the hop it was found at (0: the trigger's
// own actions; -1 none). The situation assessor names a step's kind by the armed row that sets the next objective
// (M13 docked: exit_base, Cnd_BaseExit -> defend_base).
// M13-MIMIC item 1 (docs\research\runners\M13.md "## Plan"): an escort step the PLAYER ends by reaching a place - the
// armed rows' goals (GoalsOfLive) with the player as subject, a distance to a solar or a point (M13 "Escort Sinclair to
// the Battleship Osiris" 24440: in_osiris_range = Cnd_DistShip inside, Player, osiris, 1000 -> dock_with_osiris; all three
// runners cruise the 29.7 km straight there and leave Sinclair) - refused (why) while an armed row would fail the mission
// on a death (Cnd_Destroyed whose chain fails at once: the escorted ship's) or a leash is armed (ArmedLeashes). Fails
// closed: no live table, no script, a placeholder text.
struct EscortPlace
{
    bool ok = false;
    std::vector<Goal> goals; // the player's distance goals, the table's order
    std::string why;         // not ok: "no live table" / "no script" / "no step" / "death-fail <trigger>" / "leash <name>" /
                             // "no player goal"
};
EscortPlace EscortByPlace(const LiveTable& t, u32 system, u32 textId);
// M13-MIMIC item 2: the bases whose entry the script waits for NOW - armed rows with a lone Cnd_BaseEnter = <base> whose
// chain leads on (a new objective / SUCCEED, never a FAIL at once), base nicknames lower case. M13: trent_go_back arms
// land_on_base (st01_01_base) in the same breath as it unlocks Toledo's ring, ~5 s before "Dock with Planet Toledo"
// shows - the runners dock on it. `textId`: the step's (its own objectives are no lead).
std::vector<std::string> ArmedBaseEnters(const LiveTable& t, u32 textId);
// LOCK-TRAP (`lock_trap`, modules\autopilot\docs\AUTOPILOT.md; M13 31 Oct 2026): an armed row (state 0) in `system` (its own or
// its arming trigger's) with a lone Cnd_DistShip = inside, Player, <obj>, R or Cnd_DistVec = inside, Player, x, y, z, R
// whose actions lock a dock for the player (Act_LockDock = Player, <dock>, lock), while the script still has that dock's
// unlock to come: an armed row other than the trap (hop 0) or a trigger such a row arms (Act_ActTrig, up to 4 hops, never
// through the trap, never into a complete row) holds Act_LockDock = Player, <dock>, unlock. The script means the lock to
// come first: met only after the unlock it closes the dock for good. M13 (m13.ini 1838-1843 / 1921-1957): wait_for_trent
// (within 320 m of Orillion -> lock St01_to_St03_hole) beside in_hole_range (within 2500 m of the hole's point, armed at the
// space entry) -> check_cap (NomadCap3 destroyed -> unlock); a trip that reaches the hole first leaves the lock for later.
// A lock with no unlock to come is no trap (M12 lock_lair_when_player_exits: its unlock fired before it). One entry per
// locked dock, the table's order. Fails closed: no live table / no script -> none.
struct LockTrap
{
    std::string trigger;    // the trap (lower)
    int kind = 0;           // 1: within `dist` of the object `object`; 2: within `dist` of the point `pos`
    std::string objectName; // kind 1: as the script writes it
    u32 object = 0;         // kind 1: CreateID of it (a solar's space id)
    u32 nameIds = 0;        // kind 1, a mission ship: its NPC's individual_name (its runtime id is not its nickname's)
    float pos[3] = {};      // kind 2
    float dist = 0;         // R, m
    std::string lockedName; // the dock it locks (as written)
    u32 locked = 0;         // CreateID of it
    std::string unlockBy;   // the trigger whose unlock is to come
    int unlockHops = 0;     // 0: that trigger is armed; n: n Act_ActTrig hops from an armed one
};
std::vector<LockTrap> LockTrapsOfLive(const LiveTable& t, u32 system);
struct LiveLead
{
    std::string leads;
    int hops = -1;
};
std::vector<LiveLead> LeadsOfLive(const LiveTable& t, u32 textId = 0);
// M13-MIMIC item 3 (`kill_ahead`, docs\research\runners\M13.md "## Plan" 3): may the mission solar `object` be destroyed
// NOW without changing the story's path? Its kill is the `killNo`-th of its label (or its nickname's first): every
// Cnd_Destroyed trigger of the table's script that the kill fires (nickname, or label with count <= killNo, not complete)
// must be armed already (one armed later would miss it), must not fail the mission, and must not switch off
// (Act_DeactTrig, also through its at-once Act_ActTrig hops) an armed trigger whose chain sets a new objective. M13's
// barrier: generator1_down = "Cnd_Destroyed generator, 1" deacts juni_near_center (Juni within 3.5 km of the centre ->
// marking_generators -> "Destroy the Generators") - before Juni arrives the first generator's kill would leave the step
// without its objective (M13.md step 9); R1 kills it in the 16.9 s of dialogue after that trigger fired. Fails closed.
struct KillSafe
{
    bool ok = false;
    std::string why; // ok: "" ; else "no script" / "not a mission solar" / "<watcher> not armed" / "<watcher> fails" /
                     // "<watcher> deacts <trigger> (-> <objective>)"
};
KillSafe KillAheadSafe(const LiveTable& t, u32 object, int killNo);
// M13-MIMIC item 3 (`battery_budget`): the destroy steps on mission solars still ahead of the step with text `textId`
// before the next dock at a base - the consumable windows (the study's section 3: two budgets, Toledo -> Osiris and
// Osiris -> end, split over the big fights each holds). In the script that sets `textId`, by trigger order (the story's
// order; M13's [NNObjective] list is not - it declares `penetrate` last): the step = the first trigger that sets its
// objective, the window's end = the first setter after it of an objective that docks at a base (a rep_inst whose object is
// a [MsnSolar] with `base =`, or one `isBase` accepts - the caller's system data: a planet's docking ring), and every
// Cnd_Destroyed goal on mission solars (kind 3, chain leading on) between the two, grouped by label (the first trigger
// per label). A timer step's destroy watcher (StepAdvance "timer": the script ends that step by itself) is no group.
struct FightGroup
{
    std::string label;       // the Cnd_Destroyed's label / nickname (lower)
    std::vector<u32> objects; // its solars
    std::string trigger, leads;
    int order = -1;
    // the kills the window's script waits on for the label: the highest Cnd_Destroyed count among its triggers there, at
    // most its solars (M13 window B: NomadCap3 1, the barrier's generator 4, the city's final_generator 2) - battery_budget's
    // weight
    int kills = 0;
};
struct FightsAheadView
{
    bool ok = false;
    std::string why;
    int current = -1, boundary = -1; // trigger indexes in the script (boundary -1: no base dock ahead)
    std::vector<FightGroup> groups;  // ahead, in the script's trigger order
};
FightsAheadView FightsAhead(u32 textId, u32 system, bool (*isBase)(u32 object) = nullptr);
// The [NNObjective] `nick` (lower, a Goal::leads) of script `file` (full path): its rep_inst object (CreateID of the 8th
// field) and its text id; 0 when it has no object or neither is known. M13-MIMIC item 6 (`place_dock`): in_osiris_range
// leads to dock_with_osiris = rep_inst ..., osiris - the place the escort step ends at is the next step's dock.
u32 ObjectiveObjectOf(const std::string& nick, const std::string& file, u32* textId = nullptr);

// A ship the game spawned (the controller host's SpawnedShips): its space id and exact spawn position.
struct Spawn
{
    u32 id = 0;
    float pos[3] = {};
};
// The space ids of the mission ships the current step's script will turn hostile to the player: Act_SetVibeLbltoShip /
// Act_SetVibe ..., Player, REP_HOSTILE_* in the triggers the step's chain arms, up to 4 hops forward whatever their
// conditions, in `system`, not past a trigger that sets a new objective; the ships found among `spawns` by their spawn
// position ([MsnShip] / [MsnFormation] / the spawn action's). M04: the bounty hunters follow the player neutral for ~20 s
// of comms before `send_in_hacker` turns them; the top-3 runners open fire while they are neutral (IDEAS A6). `textId` =
// the step's objective text (a placeholder names no step: none); `names` (optional): their nicknames.
std::vector<u32> PendingHostileShips(u32 textId, u32 system, const std::vector<Spawn>& spawns,
                                     std::vector<std::string>* names = nullptr);
// A kind 4 goal's ships among the spawns (as PendingHostileShips finds them: by spawn position, MapSpawns): their space
// ids, `names` (optional) their nicknames, in the spawns' order. Empty for another kind or an unknown script.
std::vector<u32> ShipIdsOf(const Goal& g, const std::vector<Spawn>& spawns, std::vector<std::string>* names = nullptr);

// SPAWN-STAGE (`spawn_stage`, modules\autopilot\docs\AUTOPILOT.md; docs\research\runners\M02.md "## Plan" 1): what a kill
// trigger's firing spawns for a later step's kill. From the trigger `trigger` of script `file` (full path), its own actions
// and the triggers it fires at once (Act_ActTrig to Cnd_True / Cnd_Timer, up to 4 hops): the ships it spawns
// (Act_SpawnShip, Act_SpawnFormation) and, among them, the first one a Cnd_Destroyed trigger armed on that walk (any hop,
// any condition) names - by nickname or by a label - that ship's spawn position, its first orders (Act_SpawnShip's own
// ObjList, else the first Act_GiveObjList on the walk) with the list's commands, and every other ship the walk spawns. M02:
// escorts_destroyed (Cnd_Destroyed Escort, 4) spawns Ashcroft at -28500, 0, 51100; start_run1 gives him ashcroft_stage2
// (GotoVec goto_cruise -22099, 0, 75896) and spawns Thug1..4 at that run point; start_stage4_attack arms ashcroft_dead.
// Fails closed (ok false, why): no script, no such trigger, no spawned kill ship.
struct SpawnAfter
{
    bool ok = false;
    std::string why;
    std::string ship;        // the spawned kill ship ([MsnShip] nickname, lower)
    std::string killTrigger; // the later step's Cnd_Destroyed trigger that names it
    bool hasPos = false;     // its spawn position is known ([MsnShip] / the spawn action's)
    float pos[3] = {};
    std::string orders;      // its first ObjList (lower), "" none found
    bool ordersFound = false; // that [ObjList] exists in the script
    std::vector<std::pair<std::string, std::string>> cmds; // the list's commands in order (keys lower, values as written)
    struct Other
    {
        std::string nick; // a ship, or a formation's nickname (lower)
        bool hasPos = false;
        float pos[3] = {};
        bool friendly = false; // the walk sets it friendly to the player (Act_SetVibe / Act_SetVibeLblToShip, REP_FRIEND*)
    };
    std::vector<Other> others; // every other ship / formation the walk spawns
};
SpawnAfter SpawnAfterKill(const std::string& trigger, const std::string& file);
// A "leaving" nag (Act_NagDistLeaving = <name>, <nagger>, <object>, <fail ids>, <dist>[, <type>] | <name>, <nagger>,
// position, x, y, z, <fail ids>, <dist>[, <type>]): the player more than `dist` from the object fails the mission (M02's
// nag_battle3: King, 5000 m, 13093 "You ran from the battle"). mission::Leashes reads only Cnd_DistShip outside rows: these
// are the scripts' other leash. Every one of script `file` (full path) in `system` (a trigger without its own system: its
// arming trigger's), in the script's order, armed or not; `offBy` the triggers whose Act_NagOff names it (escorts_destroyed
// disarms nag_battle3).
struct LeavingNag
{
    std::string name, trigger; // the nag (lower) and the trigger that sets it
    std::string object;        // as written ("" for a position)
    u32 objectId = 0;          // CreateID of it
    u32 nameIds = 0;           // a mission ship: its NPC's individual_name (its runtime id is not its nickname's)
    bool pos = false;
    float p[3] = {};
    float dist = 0;
    u32 failIds = 0;
    std::vector<std::string> offBy; // (lower)
};
std::vector<LeavingNag> LeavingNags(const std::string& file, u32 system);

// A "towards" nag (Act_NagDistTowards = OBJ, <name>, <nagger>, <object>, <fail ids> | POS, <name>, <nagger>, x, y, z,
// <fail ids>[, <dist>]): set in the same action list as the objective it comes with; the player not closing on the target
// fails the mission (M09: "You failed to fly to the Blood Dragon Hideout" while a later fight objective held the ship).
struct TowardsNag
{
    bool pos = false; // a point (pos), else the object `object` (CreateID of `name`)
    u32 object = 0;
    std::string name;
    float p[3] = {};
    float dist = 0; // the optional distance (OBJ field 5, POS field 7), 0 none given
};
// The towards nag the objective with text `textId` comes with (any script), false when it has none.
bool TowardsNagOf(u32 textId, TowardsNag& out);
// The next step's goals: from the trigger(s) that set the objective with text `textId`, the next objective their chains
// set (Act_ActTrig, up to 6 hops: dialogue-gated steps - M07 Tau-31's Tobias lines) and that objective's goals in
// `system` (GoalsAfter); empty when none is found. `next` (optional) gets the next objective's nickname.
// `nag` (optional) gets the next objective's towards nag (TowardsNag; nag->name empty and pos false when it has none).
// `look` (LOOK L0): also kind 4 and the look extras (Goal).
std::vector<Goal> NextStepGoals(u32 textId, u32 system, std::string* next = nullptr, struct TowardsNag* nag = nullptr,
                                bool look = false);
// The objective the script declares right after the one with text `textId` ([NNObjective] order = the mission's flow:
// M04 "Dock with the Trade Lane" across West Point -> "Fly to the Waypoint" depot_wp1), when it has a point in `system`
// (a navmarker / rep_inst; 0, 0, 0 = none): its position and text id. False otherwise.
// `skip`: past up to that many declared objectives without a point (0 = the very next one only).
bool NextObjectivePlace(u32 textId, u32 system, float pos[3], u32* nextText = nullptr, int skip = 0);

// TIMER-DESTROY (docs\WORK_QUEUE.md): how a step with a destruction in it advances, from the
// script alone. Forward from the setters of the objective with text `textId` (the triggers whose Act_SetNNObj names it)
// and from the triggers armed beside them (AdvanceGoals' view of the step), up to 6 hops of Act_ActTrig whatever the
// conditions, never past a trigger that sets a new objective or fails the mission: every path that sets the next
// objective or succeeds, and the step's destroy watchers (a Cnd_Destroyed / Cnd_HealthDec trigger whose chain does not
// fail the mission). `kind`:
//   "kill"  - a path to the next objective runs through a destroy condition (M09: last_generator_dies -> mlog_dock_with_arch);
//   "timer" - none does, and one runs on conditions that come by themselves alone (Cnd_Timer, Cnd_CommComplete,
//             Cnd_True): the step advances whatever is killed - M13 "Destroy the Nomads" 24420: cap_timer ->
//             trent_go_back (15 s) -> ether_one (Orillion's 0460) -> go_home, while watch_cap2 (NomadCap2 destroyed) only
//             hides the objective;
//   "other" - no kill path, and the paths wait on something else (a distance, a dock, a launch);
//   ""      - no path to a next objective found, or no script knows the text.
// Whether the step asks for a destruction at all is the caller's (the objective's words).
struct StepAdvance
{
    std::string kind;
    std::string next;    // the objective the advancing path sets ("SUCCEED"), "" none
    std::string via;     // that path's triggers after the setter, "a>b" (the timer path for "timer", the kill path for "kill")
    std::string watcher; // "kill": the kill path's destroy trigger; else the step's first destroy watcher, "" none
    std::string file;    // the script (full path)
    // `next`'s [NNObjective]: its text id, system and point (navmarker / rep_inst; hasPos false for 0, 0, 0 or an ids)
    u32 nextText = 0, nextSystem = 0;
    bool nextHasPos = false;
    float nextPos[3] = {};
    u32 nextObject = 0; // a rep_inst's object (CreateID of its 8th field: go_home's St01_dock_ring_1), 0 none
    bool ownHasPos = false; // the step's own [NNObjective] point (M13 Cap2: 6350, 0, -5080 - PSN's place)
    float ownPos[3] = {};
    // ALLY-KILL (`ally_kill`, docs\research\runners\M13_gate_allykill.md), "kill" only - the kill path's destroy trigger
    // (`watcher`) as a lone Cnd_Destroyed: the mission solars it names (nickname or label; empty: ships, or not a lone
    // Cnd_Destroyed), whether it counts any killer (no 4th field), and the dock it unlocks - the object of the first
    // Act_LockDock = Player, <obj>, unlock on the kill path from the watcher on or in the triggers those fire at once
    // (Act_ActTrig to a lone Cnd_True / Cnd_Timer, up to 4 hops); 0 none. M13 24460: check_cap = NomadCap3, 1, ALL unlocks
    // St01_to_St03_hole, lead_through_gate's own object
    std::vector<u32> killObjects;
    bool anyKiller = false;
    u32 unlocks = 0;
    // the shape ally_kill acts on (the next step's verb is the caller's): kind kill, a mission solar target, any killer,
    // the unlock is the next objective's object
    bool AllyKillShape() const
    {
        return kind == "kill" && !killObjects.empty() && anyKiller && unlocks && unlocks == nextObject;
    }
};
// `system`: the step's system (a text two scripts share: the setter in it wins; 0 any).
StepAdvance StepAdvanceOf(u32 textId, u32 system);
// The offline sweep of StepAdvanceOf (tests\look_test.cpp writes it for review): every objective of every script under
// `missionsDir` with a kind, one line "<file> <objective> [<text id>] <kind> next=<..> via=<..> watcher=<..>
// solars=<n> any-killer=<0|1> unlocks=<id> ally-kill=<0|1>", in file and [NNObjective] order.
std::vector<std::string> StepAdvanceSweep(const std::string& missionsDir);
// The trigger `trigger` of script `file` (full path) as a lone Cnd_Destroyed: 1 it counts any killer (no 4th field), 0 it
// names one (M06 d_dexter: Player), -1 no such trigger or not a lone Cnd_Destroyed.
int AnyKiller(const std::string& trigger, const std::string& file);

// Names trigger ids (CreateID of [Trigger] nicknames: the live table's, mission_live.h, or a save's [TriggerSave]): the
// script whose triggers cover the most of `ids` (ties: the first found), its path in `script` and per id its nickname
// and first condition key (lower case, "cnd_timer"; "" when that script lacks the id). False when no script knows any.
// `missionsDir` (tests): read that DATA\MISSIONS folder instead of the install's.
bool NameTriggers(const std::vector<u32>& ids, std::string& script, std::vector<std::string>& nick,
                  std::vector<std::string>& cond, const std::string& missionsDir = std::string());

// The offline decision sweep (tools\decision_check.ps1 / tools\decision_baseline.txt): every objective of every script
// under `missionsDir` (a DATA\MISSIONS folder), the system of the first trigger that sets it, and the goals of that step
// read with nothing shown but the objective itself - one line per objective that has goals, in the baseline's format
// ("<file> <objective> [<first text id>] <system> -> [('trigger', 'subject', 'what', 'leads'), ...]"; the order of the
// goals is the trigger order, the Python port's is unordered). Reads nothing from the game: build.cmd test diffs it
// against the baseline, which checks the port and the C++ against each other.
std::vector<std::string> DecisionSweep(const std::string& missionsDir);
} // namespace mission
