// Access to the host DLL's services from the logic DLL. The host and the logic are always of one interface number
// (core\loader.h kLogicApiVersion), so every function of the HostApi is there; each wrapper only covers the time before Init
// and after Shutdown, when g_host is null.
#pragma once
#include "../api.h"
#include "game/hostcalls.h"

extern const HostApi* g_host;

// Evade behaviour guard (false: Evade stays refused).
inline bool HostGuardEvade(void** vtable) { return g_host && g_host->GuardEvade(vtable); }

// Keep running out of focus.
inline void HostSetKeepRunning(bool on)
{
    if (g_host)
        g_host->SetKeepRunning(on);
}

// Jump-ins the client has reported complete; -1 unknown (not hooked).
inline int HostJumpInCount() { return g_host ? g_host->JumpInCount() : -1; }
inline void* HostCommPlayer() { return g_host ? g_host->CommPlayer() : nullptr; }
inline int HostMissionObjects(u32* out, int max) { return g_host ? g_host->MissionObjects(out, max) : 0; }
// Marks and objective refs apart; -1 unknown.
inline int HostMissionMarks(u32* out, int max) { return g_host ? g_host->MissionMarks(out, max) : -1; }
inline int HostObjectiveRefs(u32* out, int max) { return g_host ? g_host->ObjectiveRefs(out, max) : -1; }
inline u32 HostObjectiveNow(ObjectiveStep* out) { return g_host ? g_host->ObjectiveNow(out) : 0; }
inline u32 HostObjectiveTitle() { return g_host ? g_host->ObjectiveTitle() : 0; }
// The game loads the host has seen (what the host and mission.cpp keep per game starts over at a load).
inline u32 HostGameLoads() { return g_host ? g_host->GameLoads() : 0; }
// The story's checkpoint saves (sequence, 0 = none: save:<ids> splits then never come; ids = the latest).
inline u32 HostMissionSave(u32* ids) { return g_host ? g_host->MissionSave(ids) : 0; }
// The current step's text as the game formats it; false (out "") with no formatter.
inline bool HostObjectiveText(wchar_t* out, int max)
{
    if (out && max > 0)
        out[0] = 0;
    return g_host && g_host->ObjectiveText(out, max) && out && out[0];
}
// Re-read the objective the game holds (after a load).
inline void HostObjectiveReread()
{
    if (g_host)
        g_host->ObjectiveReread();
}
// Every entry of the objective (route steps).
inline int HostObjectiveSteps(ObjectiveStep* out, int max)
{
    return g_host ? g_host->ObjectiveSteps(out, max) : 0;
}
inline unsigned long long HostCommHeardAt(u32 line) { return g_host ? g_host->CommHeardAt(line) : 0; }
inline int HostSpawnedShips(SpawnRec* out, int max) { return g_host ? g_host->SpawnedShips(out, max) : 0; }
// The server's last answer to a dock / lane request (sequence 0 = none).
inline u32 HostDockReply(u32* ship, u32* kind, u32* response)
{
    return g_host ? g_host->DockReply(ship, kind, response) : 0;
}
// The game's own last hit point on the player ship, as its damage indicator got it (kind 1 blast, 2 contact;
// sequence 0 = none).
inline u32 HostPlayerHit(float pos[3], u32* kind, u32* tick)
{
    return g_host ? g_host->PlayerHit(pos, kind, tick) : 0;
}

// the pad screens' source of the cursor hide (hostcalls::CursorHide)
inline void HostSetCursorHidden(bool on) { hostcalls::CursorHide(hostcalls::kCursorHidePad, on); }
// The mission results received (sequence, 0 = none; type 1 success / 2 failure).
inline u32 HostMissionResult(u32* type) { return g_host ? g_host->MissionResult(type) : 0; }

// Steering override.
inline void HostSteer(bool active, float x, float y)
{
    if (g_host)
        g_host->SetSteer(active, x, y);
}
// Who provides borderless windowed ("flvibed", "game", "wrapper", "off", "unverified", "undecided"); "" before Init.
inline const char* HostWindowMode(char* detail, int len)
{
    if (detail && len > 0)
        detail[0] = 0;
    return g_host && g_host->WindowMode ? g_host->WindowMode(detail, len) : "";
}
