// Story mission scripts (DATA\MISSIONS\Mxx\*.ini, plain text): what the current step's armed triggers really wait for -
// the game's own conditions read from the install, never a copy (session 5, 22 Oct 2026; the top-3 speedruns: "an
// objective is a trigger, not an order" - M12's "Destroy all of the Nomad Ships" advances the moment the player is within
// 8 km of Toledo, M08's "Fly to the Waypoint" when Ozu, who follows the player, is within 9 km of the jump hole;
// docs\research\speedrun.md V3, IDEAS A21). These are script-derived facts; which caller uses them under which setting
// is the caller's business.
//
// The script's runtime state (which triggers are armed) is not read from the game; it is derived from the data: the
// triggers armed by the current objective's setter, by the trigger that armed the setter, and by the triggers that fire
// on entering the player's system (with their Cnd_True / Cnd_Timer children), restricted to the player's system.
#pragma once
#include "common.h"

#include <string>
#include <vector>

namespace mission
{
struct Goal
{
    int kind = 0;             // 1: within `dist` of the solar `object`; 2: within `dist` of the point `pos`;
                              // 3: `count` of the mission solars `objects` destroyed
    u32 object = 0;           // kind 1: the solar's id (CreateID of its nickname)
    std::string objectName;   // kind 1: its nickname as the script writes it; kind 3: the label / nickname
    float pos[3] = {};        // kind 2
    float dist = 0;           // m
    std::vector<u32> objects; // kind 3: the [MsnSolar]s named or labelled so (space id = CreateID of the nickname)
    int count = 0;            // kind 3
    std::string subject;      // "player", or the ship (that follows the player) the condition names
    std::string trigger;      // the trigger whose condition this is
    std::string leads;        // what it leads to: an objective nickname, or "SUCCEED"
    std::string file;         // the script (full path)
    bool byEntry = false;     // armed only through a system-entry trigger, not the step's own objective chain (whether
                              // it is live during the step is unknown: M05 / M13 fights, session 8)
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
// The goals of the step a goal leads to (`objective` = its Goal::leads nickname, `file` = its Goal::file), as they will be
// armed once that objective is set: the look-ahead for a step whose own condition has fired while the script still
// talks (M12: "within 2.7 km of the lair" -> ~31 s of dialogue -> "Destroy all the Shield Generators"; the runners wait
// at generator 1 and fire the second the banner shows).
std::vector<Goal> GoalsAfter(const std::string& objective, const std::string& file, u32 system);
// The goals a fired trigger (`trigger`, of `file`) arms - the next level of the chain (session 7, M12 Omicron Minor:
// "within 8 km of Toledo" fired, its chain arms "within 3 km of the ring" -> "Dock with Planet Toledo"; the ship stood by
// Juni with nothing to fly to).
std::vector<Goal> GoalsArmedBy(const std::string& trigger, const std::string& file, u32 system);
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
// The leashes of the current mission's script (the one AdvanceGoals last found) in `system` (session 8, 23 Oct 2026, M10:
// "Awaiting" -> a script goal 17.6 km off, more than 6 km from Von Claussen -> "You fled the battle"). Whether one is armed
// is not tracked: the caller keeps to all of them.
std::vector<Leash> Leashes(u32 system);

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
std::vector<Goal> NextStepGoals(u32 textId, u32 system, std::string* next = nullptr, struct TowardsNag* nag = nullptr);

// The offline decision sweep (tools\decision_check.ps1 / tools\decision_baseline.txt): every objective of every script
// under `missionsDir` (a DATA\MISSIONS folder), the system of the first trigger that sets it, and the goals of that step
// read with nothing shown but the objective itself - one line per objective that has goals, in the baseline's format
// ("<file> <objective> [<first text id>] <system> -> [('trigger', 'subject', 'what', 'leads'), ...]"; the order of the
// goals is the trigger order, the Python port's is unordered). Reads nothing from the game: build.cmd test diffs it
// against the baseline, which checks the port and the C++ against each other.
std::vector<std::string> DecisionSweep(const std::string& missionsDir);
} // namespace mission
