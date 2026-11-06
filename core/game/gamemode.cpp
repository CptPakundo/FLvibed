// Mode detection (gamemode.h).
#include "gamemode.h"

#include "fl.h"
#include "room.h"

namespace gamemode
{
const char* Name(Mode m)
{
    switch (m)
    {
    case Mode::Menu: return "menu";
    case Mode::Docked: return "docked";
    case Mode::Space: return "space";
    case Mode::SpaceUi: return "space-ui";
    case Mode::Cutscene: return "cutscene";
    }
    return "?";
}

bool SpaceWindowOpen()
{
    static ULONGLONG at = 0;
    static bool open = false;
    const ULONGLONG now = GetTickCount64();
    if (now - at >= 100)
    {
        at = now;
        open = false;
        for (const char* n : {"NeuroNetNavMap", "NN_Inventory", "NeuroNetStoryStar", "NN_PlayerInfo", "NN_Help"})
            if (uiclick::FindVisible(n))
            {
                open = true;
                break;
            }
    }
    return open;
}

Mode Detect(bool (*onlyHudOpen)(), bool (*spaceWindowOpen)())
{
    if (fl::PlayerBase())
        return Mode::Docked;
    if (fl::PlayerIObj())
    {
        const u32 cam = fl::CurrentCamera();
        if (exe::CamCutscene.is(cam) || exe::CamDocking.is(cam))
            return Mode::Cutscene;
        // Freelancer switches to the arrow cursor while a NeuroNet window takes the mouse - and also while the cursor
        // rests on the HUD, which is still flight.
        if (_stricmp(fl::CursorName(), "arrow") == 0 && !(onlyHudOpen && onlyHudOpen()))
            return Mode::SpaceUi;
        // (D-BASES) the NeuroNet windows hang under the HUD's own NeuroNet root: known by their root controls
        if (spaceWindowOpen && spaceWindowOpen())
            return Mode::SpaceUi;
        return Mode::Space;
    }
    return Mode::Menu;
}
} // namespace gamemode
