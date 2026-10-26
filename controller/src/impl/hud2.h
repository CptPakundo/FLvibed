// HUD 2 (docs\UI_HUD2.md, the owner's go of 25 Oct 2026): FLvibed's own flight HUD replacing the game's panels - the
// reticle ring (shield left, hull right, power / cruise charge below, the speed inside), the nav corner (the objective as
// the headline, the maneuver and AUTOPILOT chips with progress and ETA, NeuroNet), the target card with the threat list,
// the ship panel, the loadout (the D-pad cross, the weapon groups, the launcher), the context line under the ship and a
// transient alert card - drawn in one pass over the game's (hidden) HUD, the game's comm window, objective banner,
// target indicators and damage arcs kept native. Visual reference: tools\uimock\hud2*.py (iteration 3). Files:
// hud2.cpp (state, data at its rate, the settings, the transition, the HUD bus), hud2_parts.cpp (the components),
// hud2_kit.* (the drawing kit), hudvis.* (the native HUD switch), core\game\flicons.* (the game's flat icons).
#pragma once
#include "common.h"
#include "presets.h"

#include <istream>
#include <string>

namespace hud2
{
struct Context
{
    bool space = false;     // Mode::Space (flying, the HUD up)
    bool spaceUi = false;   // Mode::SpaceUi (a NeuroNet window over the HUD)
    bool pad = false;       // the pad is the input device (device::Pad)
    bool bot = false;       // the test bot flies
    presets::Table table;   // the effective bindings (flight or flight_ap)
    bool autopilot = false; // the Advanced Autopilot flies
    bool turret = false;    // turret view (the pad's toggle)
    int weaponGroup = 1;    // the active group as the pad last set it (1..3)
    bool apKnown = false;   // the autopilot's prompts may show (used once / ap_hints)
    bool modal = false;     // the picker / wheel / goal panel is open (its prompt bar: the context line steps aside)
};

// Once per frame from OnFrame (inside the space memo): the state machine, the data at its own rate, the transition.
void Update(float dt, const Context& c);
// From OnRenderUnderHud: the native HUD switch (hudvis) for this frame, with the dark backing under the game's 3D views
// (hud_wireframes) drawn first so their thin lines read over any scenery.
void RenderUnderHud(void* device);
// From OnRenderOverlay: the HUD itself (one pass).
void Render(void* device);
// HUD 2 replaces the game's HUD this frame (its panels hidden): the pad HUD layer's badges / legend / status line /
// world prompts stand down.
bool Active();
// Extended HUD 2's modal menus over the flight (ui_style 1, in space: uiskin::ModalOpen; set per frame from
// OnRenderUnderHud before RenderUnderHud): HUD 2 is not drawn (nor its hotspots, nor its views' backing) and the native
// switch hides the game's whole flight HUD (hudvis::SetMenuOver) - everything back the frame it clears. False = today.
void SetMenuOver(bool on);
bool MenuOver();
// The target card's bottom edge and right edge in pixels while HUD 2 shows (the speedrun timer goes under it); false
// when HUD 2 is not shown.
bool TargetCardAnchor(float& right, float& bottom);
// The reticle ring's radius in pixels (centred on the screen): the last layout's, else 0.10 H (its size at scale 100).
// TARGET AHEAD's cone (the owner, 26 Oct 2026: "the cone can be the actual center circle of HUD 2's").
float RingRadius();
// The action wheel's CONTACT FILTER while HUD 2 shows (the game's filter buttons are hidden): HUD 2's own contact list
// on, then its next filter (IMPORTANT, SHIPS, STATIONS, LOOT, ALL), then off; `now` = the filter's word or OFF. False when
// HUD 2 is not active (the wheel clicks the game's filters then).
bool CycleListFilter(std::wstring& now);
// The top of HUD 2's bottom panels (the ship panel and the loadout) in pixels while it shows: the goal panel's lower limit.
bool BottomPanelsTop(float& y);
// ImplApi OnUiButton for the host's HUD 2 hotspots (api.h kUiBtnHud2 + slot): the keyboard form's click on that slot
// (hud2_click.cpp; run in the next Update).
void OnHotspot(int slot);
// The objective line as the game's banner shows it (the host's formatted copy: values in, a blank job step skipped; an
// older host: the raw text with its format codes dropped, never shown raw); "" when there is none. Shared with the goal
// panel (its MISSION OBJECTIVE row and queue).
std::wstring ObjectiveLine(u32 textId);
// Debug: hud2 on|off|auto|state|sim <state>|perf|hot|click <slot>|list [on|off|0..4]
std::string Command(std::istream& in);
void OnDeviceLost();
void Shutdown();
} // namespace hud2
