// FLvibed's native buttons (controller host, docs\UI_PLAN.md 2 / 3.1, docs\UI_BUILD.md): the maneuver bar's fifth
// button, the FLVIBED page button of the game's Options screen with its page switch, and the pause menu's sixth button.
// All are the game's own controls, created through its factories (core\flexe.h CreateIconButton / CreateTextControl /
// CreateWireControl) and parented to the game's windows, so hit test, hover, click sound and the click-vs-fire
// arbitration are the game's; their clicks reach the controller logic through ImplApi v3 OnUiButton (main.cpp
// UiButtonClicked), their state comes from the logic through HostApi v18 SetUiButton. Everything fails closed: an
// unresolved symbol or an unexpected window layout creates nothing and logs which button stays off.
#pragma once
#include "common.h"

#include <string>

struct HudHotspot;

namespace uibtn
{
// Installs the detours (after exe::ResolveAll): the chained ManeuverLayout observer and the frame sizer, the icon
// button mouse handler, NN_Preferences / NN_Pause InitElements and message handlers.
void Install();
// Once per frame, before the logic runs: validates the tracked controls against the game's control list (a HUD or
// window rebuilt by the game destroyed ours with it), so no dangling pointer is used this frame.
void OnFrame();
// At the start of the game's UI pass (before any control is drawn): keeps the game's page hidden while FLvibed's page
// shows (the game re-shows its labels every frame).
void BeforeUiDraw();
// Debug `uibtn mesh <path>`: the bar button's mesh name for the next HUD build (a probe for a shipped .3db; empty = none).
void SetBarMesh(const char* path);
// HostApi v18
void SetUiButton(int id, bool shown, bool lit, bool enabled);
bool UiButtonRect(int id, float* cx, float* cy, float* hw, float* hh);
// HostApi v22: HUD 2's hotspots (invisible icon buttons in a host-created top-level box; uibtn.cpp "HUD 2's hotspots").
int SetHudHotspots(const HudHotspot* spots, int count);
int HudHotspotHovered();
int OptionsPageState();
bool ShowOptionsPage(bool on);
// Debug (the host's `uibtn` command): the buttons' state, controls and the last placement.
std::string Status();
// Debug: a click on one of our buttons by id, through the same path as the game's release (test aid).
bool DebugClick(int id);
} // namespace uibtn

// main.cpp: a click on one of FLvibed's buttons, forwarded to the logic (guarded).
void UiButtonClicked(int id);
