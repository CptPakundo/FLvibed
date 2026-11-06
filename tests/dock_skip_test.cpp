// `skip_dock_scene`'s pure half (modules\autopilot\src\dock_skip.h). The cases are the docks the leg logs show: a ring locks
// the ship a frame before the docking camera, so the autopilot stops "docking" first; a station's bay does not, so its run
// is still on when the camera comes (414 docks, 25 Oct - 5 Nov 2026: 0x20 rings always the first, 0x100 stations the second).
#include "../modules/autopilot/src/dock_skip.h"

#include "test.h"

void Test_dock_skip()
{
    using dockskip::Facts;
    Facts f;
    f.setting = true;

    // nobody's dock: a dock the player made by hand plays its scene
    CHECK(!dockskip::Own(f));

    // a plain autopilot trip to a planet: the ring's stop came first
    Facts ring = f;
    ring.stopped = true, ring.stoppedObject = 2439394048u;
    CHECK(dockskip::Own(ring));
    CHECK(dockskip::Object(ring, 0) == 2439394048u);

    // a plain autopilot trip to a station or base: no stop before the camera, the run is on (the tester's report: was false)
    Facts station = f;
    station.flying = true, station.flyingObject = 2661040712u;
    CHECK(dockskip::Own(station));
    CHECK(dockskip::Object(station, 2661040712u) == 2661040712u);

    // the player picked another target during the game's dock: the dock's own object still decides
    CHECK(dockskip::Object(station, 77u) == 2661040712u);

    // a run with no object of its own (the plain Dock mode on the current target): the target
    Facts bare = f;
    bare.flying = true;
    CHECK(dockskip::Own(bare));
    CHECK(dockskip::Object(bare, 55u) == 55u);

    // the objective mode: every dock, as before; nothing of a run known: the target
    Facts objective = f;
    objective.objective = true;
    CHECK(dockskip::Own(objective));
    CHECK(dockskip::Object(objective, 55u) == 55u);

    // the option off: nothing is ours, whatever ran
    for (Facts off : {ring, station, objective})
    {
        off.setting = false;
        CHECK(!dockskip::Own(off));
    }

    // multiplayer: nothing counts, a ring's stop included (its taps were seen to do nothing, the second one late)
    Facts mpStation = station, mpRing = ring, mpObjective = objective;
    mpStation.multiplayer = mpRing.multiplayer = mpObjective.multiplayer = true;
    CHECK(!dockskip::Own(mpStation));
    CHECK(!dockskip::Own(mpRing));
    CHECK(!dockskip::Own(mpObjective));
}
