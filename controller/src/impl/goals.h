// The autopilot goal panel (docs\UI_SPEC.json `hud` state `goals`; AUTOPILOT_UX.md 5.3 / 5.4 "Panel content"): X while
// the autopilot flies (the flight_ap situation's goal_panel; ASSISTED's X; the action wheel's AUTOPILOT GOALS). One
// column, tabs ACTIONS / QUEUE / SETTINGS on the bumpers, the D-pad walks rows, every action on a face button; the
// sticks keep flying (a stick past the hand-back threshold stops the autopilot and the panel stays open).
// ACTIONS: built from the current target and the objective, only what applies - ATTACK (hostile), ESCORT / FOLLOW /
// FORMATION (a friendly ship), DOCK (dockable, lane, gate), FLY TO <target>, MISSION OBJECTIVE (the objective mode),
// TRAVEL ALONG THE WAYPOINTS (the nav map's list), HOLD HERE - each with one line of what it will do. A = DO NOW starts
// that autopilot mode at once (autopilot::Start). The autopilot has modes and an objective supervisor, NOT a goal queue:
// X = QUEUE and Y = DO NEXT are shown greyed and refused with an event; QUEUE shows what runs now (the mode, the
// objective step, a travel plan's steps) and says so. SETTINGS: the player rows engage / auto_fire / missiles /
// countermeasures / repair / loot / caution through the settings bus (the same rows the options page has).
#pragma once
#include "common.h"
#include "glyphs.h"

#include <istream>
#include <string>

namespace goals
{
void Open();
void Close();
void Toggle();
bool IsOpen();
// Once per frame while open (controller.cpp UpdateSpace, before the flight dispatch): the pad's buttons this frame.
void Update(float dt, const bool down[glyphs::COUNT]);
// Once per frame, every device (ctl::OnFrame): closes without a ship, rebuilds the rows, runs the keyboard + mouse
// form's queued keys and clicks, keeps its click box with the host (the pad's buttons stay Update's).
void Tick(float dt);
// A raw key (impl.cpp OnRawKey, before the autopilot key): while open, the options page's grammar - Up / Down rows,
// Left / Right a setting's value, Enter / Space DO NOW / CHANGE, Esc / Backspace close, Tab / Shift+Tab / Page Up /
// Page Down the tabs - is the panel's (true = swallowed); Shift is noted for Tab. Opening is barbutton's: Shift + the
// autopilot key, Shift + click on the bar button / HUD 2's autopilot icon (the pad: X while it flies, the wheel).
bool OnRawKey(int vk, bool down, bool textField);
// The panel's click box holds the host's hotspots now (its clicks are routed here, not to HUD 2: impl.cpp).
bool OwnsHotspots();
void OnHotspot(int slot);
void Render(void* device);
// The buttons the open panel owns (hidden from the flight dispatch): A, B, X, Y, LB, RB, the D-pad.
bool OwnsButton(glyphs::Id b);
// Debug: goals open|close|state|tab <n>|up|down|a|x|y|left|right
std::string Command(std::istream& in);
} // namespace goals
