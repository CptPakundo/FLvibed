// Game event log for tests and the logic DLL: mission objectives, popups, mission messages,
// comm lines (hooked from Content.dll, which runs the mission scripts) plus events pushed by the
// logic DLL (mode changes, death...). Exposed to tools through the "events" debug command.
#pragma once
#include "common.h"
#include "api.h"

#include <string>

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
// Text of a Freelancer string resource id (resources.dll + [Resources] DLLs), UTF-8.
std::string IdsText(u32 ids);
} // namespace events
