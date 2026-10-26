// Access to the host DLL's services from the logic DLL.
#pragma once
#include "../api.h"

extern const HostApi* g_host;

inline bool HostAtLeast(u32 version) { return g_host && g_host->version >= version; }

// Evade behaviour guard (host API v6+; false with older hosts: Evade stays refused).
inline bool HostGuardEvade(void** vtable) { return HostAtLeast(6) && g_host->GuardEvade(vtable); }

// Keep running out of focus (host API v7+; a no-op with older hosts).
inline void HostSetKeepRunning(bool on)
{
    if (HostAtLeast(7))
        g_host->SetKeepRunning(on);
}

// Jump-ins the client has reported complete (host API v8+); -1 unknown (older host or not hooked).
inline int HostJumpInCount() { return HostAtLeast(8) ? g_host->JumpInCount() : -1; }
inline void* HostCommPlayer() { return HostAtLeast(9) ? g_host->CommPlayer() : nullptr; }
inline int HostMissionObjects(u32* out, int max) { return HostAtLeast(10) ? g_host->MissionObjects(out, max) : 0; }
// v11: marks and objective refs apart; -1 with an older host (then only the merged v10 list).
inline int HostMissionMarks(u32* out, int max) { return HostAtLeast(11) ? g_host->MissionMarks(out, max) : -1; }
inline int HostObjectiveRefs(u32* out, int max) { return HostAtLeast(11) ? g_host->ObjectiveRefs(out, max) : -1; }
inline u32 HostObjectiveNow(ObjectiveStep* out) { return HostAtLeast(11) ? g_host->ObjectiveNow(out) : 0; }
inline u32 HostObjectiveTitle() { return HostAtLeast(13) ? g_host->ObjectiveTitle() : 0; }
// v24: the current step's text as the game formats it; false (out "") with an older host or no formatter.
inline bool HostObjectiveText(wchar_t* out, int max)
{
    if (out && max > 0)
        out[0] = 0;
    return HostAtLeast(24) && g_host->ObjectiveText(out, max) && out && out[0];
}
// v25: re-read the objective the game holds (after a load); nothing with an older host (its hooked copy stays).
inline void HostObjectiveReread()
{
    if (HostAtLeast(25))
        g_host->ObjectiveReread();
}
// v15: every entry of the objective (route steps); 0 with an older host.
inline int HostObjectiveSteps(ObjectiveStep* out, int max)
{
    return HostAtLeast(15) ? g_host->ObjectiveSteps(out, max) : 0;
}
inline unsigned long long HostCommHeardAt(u32 line) { return HostAtLeast(16) ? g_host->CommHeardAt(line) : 0; }
inline int HostSpawnedShips(SpawnRec* out, int max) { return HostAtLeast(17) ? g_host->SpawnedShips(out, max) : 0; }
// v14: the server's last answer to a dock / lane request (sequence 0 = none / older host).
inline u32 HostDockReply(u32* ship, u32* kind, u32* response)
{
    return HostAtLeast(14) ? g_host->DockReply(ship, kind, response) : 0;
}
// v12: the game's own last hit point on the player ship, as its damage indicator got it (kind 1 blast, 2 contact;
// sequence 0 = none / older host).
inline u32 HostPlayerHit(float pos[3], u32* kind, u32* tick)
{
    return HostAtLeast(12) ? g_host->PlayerHit(pos, kind, tick) : 0;
}

// v20: the game's cursor arrow hidden while on (the pad screens; a no-op with older hosts).
inline void HostSetCursorHidden(bool on)
{
    if (HostAtLeast(20))
        g_host->SetCursorHidden(on);
}
// v20: the mission results received (sequence, 0 = none / older host; type 1 success / 2 failure).
inline u32 HostMissionResult(u32* type) { return HostAtLeast(20) ? g_host->MissionResult(type) : 0; }

// Steering override (host API v2+; a no-op with older hosts).
inline void HostSteer(bool active, float x, float y)
{
    if (HostAtLeast(2))
        g_host->SetSteer(active, x, y);
}
// v26: who provides borderless windowed ("flvibed", "game", "wrapper", "off", "unverified", "undecided"); "" with an
// older host.
inline const char* HostWindowMode(char* detail, int len)
{
    if (detail && len > 0)
        detail[0] = 0;
    return HostAtLeast(26) && g_host->WindowMode ? g_host->WindowMode(detail, len) : "";
}
