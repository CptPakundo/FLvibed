// The autopilot's own dock: the cutscene skip, run each frame.
#include "autopilot.h"

#include "dimouse.h"
#include "flexe.h"
#include "game/ai.h"
#include "game/events.h"
#include "game/fl.h"
#include "game/flightio.h"
#include "game/space.h"
#include "log.h"
#include "ui/views.h"

namespace autopilot
{
// The autopilot's own dock: its docking / landing cutscene skipped the way a player's Escape skips it (user, 21 Oct
// 2026: "the planet landing animation could be skipped just the same as the launch animation"; the harness did it
// only in some of its calls - M03's Willard played 14 s, docks 5-6 s, skipped ones ~1 s). Only on the docking
// camera (exe::CamDocking: story cutscenes in space are not skippable and are never sent USER_CANCEL), once per
// cutscene, a second try after 1 s if it still runs.
void DockSkip(float dt, bool cutscene)
{
    static float onDockCam = -1;
    static int tries = 0;
    // ...only a dock that leads into a base (its CEqObj dock target): an airlock's transit into another system
    // (M12's Nomad lair) cut short left the ship pinned against the core's wall with the HUD hidden
    bool toBase = false;
    if (cutscene && exe::CamDocking.is(fl::CurrentCamera()) && OwnDock())
    {
        const u32 tid = fl::TargetId();
        for (const space::Contact& c : space::Contacts())
            if (c.id == tid)
                toBase = c.base != 0;
    }
    if (toBase)
    {
        onDockCam = onDockCam < 0 ? 0 : onDockCam + dt;
        if ((tries == 0 && onDockCam >= 0.1f) || (tries == 1 && onDockCam >= 1.1f))
        {
            flightio::Tap(fl::USER_CANCEL);
            ++tries;
            events::Push("autopilot dock-cutscene skipped (try %d)", tries);
        }
    }
    else
        onDockCam = -1, tries = 0;
}
} // namespace autopilot
