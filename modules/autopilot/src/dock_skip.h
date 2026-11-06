// `skip_dock_scene`'s pure half: whether the dock on the docking camera is the autopilot's own, and which object it is.
// Facts in, verdict out - no game call, no statics - so tests\dock_skip_test.cpp runs the same code as the game. The impure
// adapter is OwnDock / OwnDockObject in autopilot.cpp; the skip itself DockSkip in autopilot_watch.cpp. Compiled into the
// autopilot's logic and, plain, into build\test\fltest.exe; no system headers here (core\pre.h / common.h have them).
#pragma once
#include "common.h"

namespace dockskip
{
struct Facts
{
    bool setting = false;     // skip_dock_scene
    bool objective = false;   // the objective mode is on: every dock on the docking camera is its own
    bool stopped = false;     // the autopilot stopped "docking" in the last 5 s: the game locked the ship first (a ring)
    bool flying = false;      // the dock this run asked for is still on: no lock came first (a station's bay)
    bool multiplayer = false; // USER_CANCEL does not end the scene with the world on a server: nothing counts there
    u32 stoppedObject = 0;    // the dock object of the stop
    u32 flyingObject = 0;     // the dock object of the run (0: the run docks with whatever is targeted)
};

// The dock is ours. Fails closed: whatever is not known to be ours plays its scene. Multiplayer plays every scene (6 Nov
// 2026, a ring on a local server: both taps did nothing, the scene ran its 1.1 s and the second tap landed 38 ms before
// docked, where an Escape opens the pause menu).
inline bool Own(const Facts& f)
{
    return f.setting && !f.multiplayer && (f.objective || f.stopped || f.flying);
}

// The object whose dock target says where the dock leads: the one the autopilot asked for, else the target (the
// player may have picked another target since the request; an objective's dock may be no run of ours at all).
inline u32 Object(const Facts& f, u32 target)
{
    if (f.flying && f.flyingObject)
        return f.flyingObject;
    if (f.stopped && f.stoppedObject)
        return f.stoppedObject;
    return target;
}
} // namespace dockskip
