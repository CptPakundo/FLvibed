// Game event log for tests and the logic DLL: mission objectives, popups, mission messages,
// comm lines (hooked from Content.dll, which runs the mission scripts) plus events pushed by the
// logic DLL (mode changes, death...). Exposed to tools through the "events" debug command. The record is the host's
// (core\host\events.cpp); the logic has Push, IdsText and forwarders (core\game\events_log.cpp).
#pragma once
#include "common.h"

#include <string>

// The current objective step (ObjectiveNow): the objective's text id, its 'R' values (reputation instances:
// space::ObjectiveObject resolves them) and its navmarker (the HUD's "Mission Waypoint").
struct ObjectiveStep
{
    u32 textId;
    u32 refs[4];
    int refCount;
    bool hasNav;
    float nav[3];
    u32 navSystem;
};

// A ship Content.dll spawned (SpawnedShips): its space id, archetype id and exact spawn position - a mission script's ships
// spawn at their [MsnShip] / [MsnFormation] position (mission::MapSpawns).
struct SpawnRec
{
    u32 id;
    u32 arch;
    float pos[3];
};

namespace events
{
// Install the Content.dll hooks once the DLL is loaded (call every frame; cheap).
void Update();
// Append an event (thread-safe). Lines are "<seq> <ms> <text>".
void Push(const char* fmt, ...);
// Events with a sequence number greater than `after`, oldest first, newline separated.
std::string Since(u32 after, int maxLen);
u32 LastSeq();
// Host only (host API v10): the objects the mission points the player at, newest first (marked + objective targets).
int MissionObjects(u32* out, int max);
int MissionMarks(u32* out, int max);
int ObjectiveRefs(u32* out, int max);
u32 ObjectiveNow(ObjectiveStep* out);
u32 ObjectiveTitle();
// v24: the current step's text as the game formats it (values filled in, codes without a handler dropped); returns the
// objective sequence (ObjectiveNow's). "" when the game's formatter was unavailable.
u32 ObjectiveText(wchar_t* out, int max);
// v25: re-read the current objective from the game's client copy (exe::ClientObjective) and take it over when it differs
// (a save load restores it without a SetMissionObjectives call); returns the objective sequence.
u32 ObjectiveReread();
// v15: every entry of the last SetObjectives (route steps), each with its R values and navmarker; returns the count.
int ObjectiveSteps(ObjectiveStep* out, int max);
u32 DockReply(u32* ship, u32* kind, u32* response);
unsigned long long CommHeardAt(u32 line);
// v17: the ships spawned this session, oldest first (the newest `max` when more); returns the count.
int SpawnedShips(SpawnRec* out, int max);
// v20: the mission results received (DisplayMissionMessage SUCCESS / FAILURE): a growing sequence, 0 none yet; `type`
// (may be null) = 1 success / 2 failure of the latest.
u32 MissionResult(u32* type);
// v34: the game loads seen (each loads a fresh Content.dll; the host's title, marks and heard comms start over there).
u32 GameLoads();
// v35: the mission checkpoint saves made (pub::Save, the log's `mission-save`): a growing sequence, 0 none yet; `ids`
// (may be null) = the latest save's description id (M01b's "Criminal Base Destroyed" 32120, at GAME SAVED).
u32 MissionSave(u32* ids);
// Text of a Freelancer string resource id (resources.dll + [Resources] DLLs), UTF-8.
std::string IdsText(u32 ids);
} // namespace events
