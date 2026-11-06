// The FLVIBED OPTIONS page, simple mode (docs\UI_OPTIONS.md): five tabs by owner (HUD / CONTROLLER / SUBTITLES / AUTOPILOT / MORE) of rows from the UI metadata (core\ui_meta.h,
// generated from docs\UI_SPEC.json) over the settings bus (core\settings_bus.h); instant apply, DEFAULT per tab, CANCEL reverts every change since the page opened, leaving
// saves. Hosted inside the game's Options screen (the host's page switch, HostApi v18 OptionsPageState == 2) or as a standalone overlay panel (the pause menu's sixth button,
// debug `ui options`); the first-start card opens as that panel over the first main menu of a fresh install. Driven by mouse, keyboard (OnRawKey: arrows, Enter, Q / E, D, C, Esc)
// and pad (D-pad rows, left / right values, A, X, Y, B, LB / RB tabs).
#pragma once
#include "common.h"
#include "settings_bus.h"

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
// Debug `ui ...` (debug.cpp): options [open|close] | tab <n> | rows | card | state.
std::string Command(std::istream& in);
// The standalone overlay opened / closed by code (the pause button, the debug command).
void Open();
void Close();
bool IsOpen();
} // namespace options_page
