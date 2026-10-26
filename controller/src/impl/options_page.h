// The FLVIBED OPTIONS page (docs\UI_PLAN.md 3, docs\UI_BUILD.md): five tabs (GENERAL / CONTROLLER / AUTOPILOT /
// SUBTITLES / MOVIES) of rows from the UI metadata (core\ui_meta.h, generated from docs\UI_SPEC.json) over the settings
// bus (core\settings_bus.h); instant apply, DEFAULT per tab, CANCEL reverts every change since the page opened, leaving
// saves. Hosted inside the game's Options screen (the host's page switch, HostApi v18 OptionsPageState == 2) or as a
// standalone overlay panel (the pause menu's sixth button, debug `ui options`). Driven by mouse, keyboard (OnRawKey) and
// pad (D-pad rows, left / right values, A, B, LB / RB tabs).
#pragma once
#include "common.h"

struct SettingsBus;

namespace options_page
{
// A bus over this DLL's own sections for a host without one (older than v18): controller.cpp registers its
// [Controller] / [Autopilot] registries here then, so the page shows its own module's rows; the subtitles' rows need the
// host's bus.
const SettingsBus* LocalBus();
void Init();
void Shutdown();
// Once per frame, early in ctl::OnFrame (before the pad flight code): input and the hosting state.
void Update(double dt);
// Inside the overlay pass (ctl::OnRenderOverlay): draws the page when it is open.
void Draw();
void RenderUnder(void* device); // (the under-HUD pass: the hosted page's opaque base under the Options window)
// True while the page is open and takes the pad / keyboard: the flight and menu pad handling must not also act.
bool WantsInput();
// A host-created native button was clicked (ImplApi v3 OnUiButton): kUiBtnOptionsPage / kUiBtnPauseFlvibed.
void OnUiButton(int id);
// A raw key (ImplApi v3 OnRawKey); true = swallowed (the page's keyboard navigation while it is open).
bool OnRawKey(int vk, bool down, bool textField);
// Debug `ui ...` (debug.cpp): options [open|close] | tab <n> | rows | state.
std::string Command(std::istream& in);
// The standalone overlay opened / closed by code (the pause button, the debug command).
void Open();
void Close();
bool IsOpen();
} // namespace options_page
