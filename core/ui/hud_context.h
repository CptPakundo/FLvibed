// What HUD 2 is told each frame by core's frame (core\game\coreframe.cpp fills it, the pad adds its part through
// views::PadFrameView FillHud). No game header: a program with no game (the design bench) fills its own.
#pragma once
#include "common.h"
#include "ui/presets.h"

namespace views
{
struct HudContext
{
    bool space = false;     // Mode::Space (flying, the HUD up)
    bool spaceUi = false;   // Mode::SpaceUi (a NeuroNet window over the HUD)
    bool cutscene = false;  // Mode::Cutscene (an in-flight cutscene: a docking, a mission's scene)
    bool pad = false;       // the pad is the input device
    bool bot = false;       // the test bot flies
    presets::Table table;   // the effective bindings (flight or flight_ap)
    bool autopilot = false; // the Advanced Autopilot flies
    bool turret = false;    // turret view (the pad's toggle)
    int weaponGroup = 1;    // the active group as the pad last set it (1..3)
    bool apKnown = false;   // the autopilot's prompts may show (used once / ap_hints)
    bool modal = false;     // the picker / wheel / goal panel is open (its prompt bar: the context line steps aside)
    float steer = 0;        // the pad's steering this frame (0..~1.4: |x, y|): CONTEXTUAL's steer trigger (hudwake.h)
    bool firing = false;    // the pad holds fire / a launcher: CONTEXTUAL's fire trigger
};
} // namespace views
