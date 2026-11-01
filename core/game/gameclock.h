// FLvibed - the game clock (30 Oct 2026, docs\WORK_QUEUE.md Q3 slice 2a; research\evidence\timing\probe_report.md
// section 4): game time = the sum of the dt the game passes FrameUpdate. The decision timers and the rate estimates
// (closing speeds, the player's velocity, the rounds' predicted hit times) read it, so they keep their meaning when the
// game runs faster or slower than the wall clock (the TEST ONLY debug `fixeddt`: each frame 1/fps of game time, the
// game as fast as it draws) and stand still while the game is paused (dt 0). UI / HUD animation, profiling, frame
// pacing (fpscap) and the logs' time stamps stay on wall time. Header only; unit test tests\gameclock_test.cpp.
//
// Who advances it: the controller logic's OnFrame, once per frame (controller\src\impl\controller.cpp). The state is a
// C++17 inline variable, so every module part that includes this header (build.cmd's namespace wrapper; core\game is
// compiled into the subtitles' logic too) has its own copy: only the controller logic's copy moves, so only the
// controller logic reads it (the subtitles call none of the core\game functions that do: space_rounds.cpp's Missiles /
// IncomingRounds / TrackRounds / LastRoundHit). The controller host never includes it.
//
// Starts at one day, not 0: the callers' "0 = never" stamps (a static ULONGLONG at = 0) then read as long ago against
// every window they measure (the longest 30 min), as they did against GetTickCount64's uptime.
//
// A logic hot reload restarts the clock (dev: FLControllerImpl.dll is unloaded, every static and inline variable starts
// again) - together with every stamp taken from it, which live in the same DLL: the stamps are 0 (never) again and the
// clock is back at one day, so each timer starts fresh (a window in progress is forgotten, as before a reload: the
// logic's state was reset anyway). In the release build `reload` restarts the same linked-in logic without re-running
// static initialisers: the clock and its stamps both carry on. Either way the clock never runs backwards against a
// stamp taken from it. What must never be compared with it: the host's own stamps (CommHeardAt, PlayerHit's tick),
// which are wall time (GetTickCount64 / GetTickCount in the never-reloaded host).
#pragma once
#include "common.h"

namespace gameclock
{
constexpr double kStartSec = 86400.0; // one day (above)
inline double g_nowSec = kStartSec;

// Once per game frame: the frame's game dt in seconds. 0 (paused), negative, NaN or absurd (> 1 h) adds nothing.
inline void Advance(double dtSeconds)
{
    if (dtSeconds > 0 && dtSeconds < 3600)
        g_nowSec += dtSeconds;
}
// Game time in seconds (the rate estimates' clock).
inline double NowSec() { return g_nowSec; }
// Game time in milliseconds, 64 bits: the stand-in for GetTickCount64 (ULONGLONG stamps and windows).
inline unsigned long long NowMs64() { return static_cast<unsigned long long>(g_nowSec * 1000.0); }
// Game time in milliseconds, 32 bits like GetTickCount: `NowMs() - stamp < window` stays right across the wrap
// (49.7 days of game time after the start).
inline u32 NowMs() { return static_cast<u32>(NowMs64()); }
// Tests only: put the clock at `sec`.
inline void Reset(double sec = kStartSec) { g_nowSec = sec; }
} // namespace gameclock
