// The Autopilot module's host calls, by the names its code has always used: each one fails closed when no host provides it (the controller
// off: the records run with the core's events, the pad's parts are absent).
#pragma once
#include "game/events.h"
#include "game/hostcalls.h"

inline int HostJumpInCount() { return hostcalls::g.JumpInCount ? hostcalls::g.JumpInCount() : -1; }
inline int HostMissionObjects(u32* out, int max) { return hostcalls::g.MissionObjects ? hostcalls::g.MissionObjects(out, max) : 0; }
inline int HostMissionMarks(u32* out, int max) { return hostcalls::MissionMarks(out, max); }
inline int HostObjectiveRefs(u32* out, int max) { return hostcalls::g.ObjectiveRefs ? hostcalls::g.ObjectiveRefs(out, max) : -1; }
inline u32 HostObjectiveNow(ObjectiveStep* out) { return hostcalls::ObjectiveNow(out); }
inline u32 HostObjectiveTitle() { return hostcalls::g.ObjectiveTitle ? hostcalls::g.ObjectiveTitle() : 0; }
inline int HostObjectiveSteps(ObjectiveStep* out, int max) { return hostcalls::g.ObjectiveSteps ? hostcalls::g.ObjectiveSteps(out, max) : 0; }
inline unsigned long long HostCommHeardAt(u32 line) { return hostcalls::g.CommHeardAt ? hostcalls::g.CommHeardAt(line) : 0; }
inline int HostSpawnedShips(SpawnRec* out, int max) { return hostcalls::g.SpawnedShips ? hostcalls::g.SpawnedShips(out, max) : 0; }
inline u32 HostDockReply(u32* ship, u32* kind, u32* response) { return hostcalls::g.DockReply ? hostcalls::g.DockReply(ship, kind, response) : 0; }
inline u32 HostPlayerHit(float pos[3], u32* kind, u32* tick) { return hostcalls::g.PlayerHit ? hostcalls::g.PlayerHit(pos, kind, tick) : 0; }
inline u32 HostMissionResult(u32* type) { return hostcalls::MissionResult(type); }
