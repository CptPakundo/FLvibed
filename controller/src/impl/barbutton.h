// The maneuver bar's fifth button (the Advanced Autopilot): its icon and state (docs\UI_PLAN.md 2). The host creates
// the native button (HostApi v18 SetUiButton / UiButtonRect) and routes its clicks (ImplApi v3 OnUiButton kUiBtnBar) to
// OnClick; OnKey sees every raw key (the autopilot key, [Controller] autopilot_key). Implemented in barbutton.cpp (the
// HOST track of the UI build); this header is the fixed contract between the tracks (docs\UI_BUILD.md 2).
#pragma once
#include "common.h"

namespace barbutton
{
// Once per frame (ctl::OnFrame): reads the settings through the bus ([Controller] maneuver_button, autopilot_key), the
// autopilot's state, and tells the host the button's state (SetUiButton every frame: shown, lit = Active(), enabled).
void Update();
// Inside the overlay pass (ctl::OnRenderOverlay): the ring-and-chevron glyph over the button's projected rectangle
// (UiButtonRect), the bar's light blue, the active yellow while the autopilot flies, grey when it cannot start.
void Draw();
// The native button was clicked (ImplApi OnUiButton kUiBtnBar; HUD 2's autopilot icon too): toggles the autopilot
// (Stop("bar") / autopilot::StartSmart); with Shift held it opens / closes the goal panel instead (goals.*).
void OnClick();
// A raw key as the game sees it (ImplApi OnRawKey); true = swallowed (the autopilot key toggled the autopilot; Shift +
// the key toggled the goal panel). The Shift keys are noted, never swallowed.
bool OnKey(int vk, bool down, bool textField);
// The configured key is bound by the game's live keymap (exe::KeyCommands): the key does nothing (logged once); the
// options page shows it on the row.
bool KeyRefused();
// The configured key's virtual-key code (0 = unknown name), for the page.
int KeyVk();
// Frees the glyph texture (before the logic unloads).
void Shutdown();
} // namespace barbutton
