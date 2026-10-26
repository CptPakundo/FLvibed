// The pad HUD layer (docs\UI_PLAN.md 6.5, UI_SPEC.json `hud` screen): while the pad is the device in flight, drawn
// over the game's HUD in OnRenderOverlay - nothing of the game's HUD is hidden (the owner's decision 16). Four parts:
// hud_glyphs: glyph badges at the corner of the HUD's own controls a button acts on (anchored on the controls by
// name, so HudShift layouts work); button_legend: the contextual list in the empty right-middle (Lost Judgment style)
// with only the actions that apply now, counts from the equipment; ap_status_line: one line under the maneuver bar
// while the autopilot flies, cruise charges or turret view is on; world_prompts: a prompt by the target when something
// unusual is possible now (dock in range, loot in reach). Everything in fractions of the screen height x text_size,
// through render.* with the game's own fonts. Vanishes the frame the device switches.
#pragma once
#include "common.h"
#include "presets.h"

#include <istream>
#include <string>
#include <vector>

namespace padhud
{
struct Context
{
    bool active = false;      // the layer may draw: the pad is the device, the controller is on, in space
    bool flying = false;      // Mode::Space (SpaceUi: a NeuroNet window is open; badges only)
    presets::Table table;     // the effective bindings of the current situation (flight or flight_ap)
    int preset = 0;           // presets::Preset
    bool autopilot = false;   // the Advanced Autopilot flies
    bool turret = false;      // turret view on (the pad's toggle)
    int weaponGroup = 1;      // the active weapon group as the pad last set it (1..3)
    bool apKnown = false;     // the player has used the autopilot (or ap_hints): its prompts may show
};

// Once per frame from OnFrame (inside the space memo): reads the game state the layer shows.
void Update(float dt, const Context& c);
// From OnRenderOverlay (over the HUD, before the cursor).
void Render(void* device);
// Phase D: a modal with its own prompt bar is open - the legend and the world prompts step aside.
void SetModal(bool on);
bool Modal();
// Debug: padhud on|off|state
std::string Command(std::istream& in);
void Shutdown(); // frees the fonts and atlases (before the logic unloads)
} // namespace padhud
