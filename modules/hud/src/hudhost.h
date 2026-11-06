// The HUD module's host calls, by the names its code has always used: core's table (core\game\hostcalls.h), filled by whichever entry runs. Every entry
// a caller reads may be null, and each caller checks it, so an older host or a missing service fails closed.
#pragma once
#include "game/hostcalls.h"

static const hostcalls::Table* const g_host = &hostcalls::g;

// The table already holds null for what a host lacks: every version check is true here.
inline bool HostAtLeast(u32) { return true; }

inline int HostMissionObjects(u32* out, int max) { return hostcalls::g.MissionObjects ? hostcalls::g.MissionObjects(out, max) : 0; }
inline u32 HostObjectiveNow(ObjectiveStep* out) { return hostcalls::ObjectiveNow(out); }
inline u32 HostDockReply(u32* ship, u32* kind, u32* response) { return hostcalls::g.DockReply ? hostcalls::g.DockReply(ship, kind, response) : 0; }
inline u32 HostPlayerHit(float pos[3], u32* kind, u32* tick) { return hostcalls::g.PlayerHit ? hostcalls::g.PlayerHit(pos, kind, tick) : 0; }
// The current step's text as the game formats it; false (out "") with no formatter.
inline bool HostObjectiveText(wchar_t* out, int max)
{
    if (out && max > 0)
        out[0] = 0;
    return hostcalls::g.ObjectiveText && hostcalls::g.ObjectiveText(out, max) && out && out[0];
}
