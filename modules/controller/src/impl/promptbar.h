// The shared prompt bar of the controller UI (docs\UI_PLAN.md 6.5, docs\UI_SPEC.json: every screen's `prompts`): the
// bottom-right row of glyph + label pairs (placement: promptbar.cpp's header, modules\hud\docs\UI_HUD2_EXTENDED.md 13), confirm
// first, at most five, "HOLD" marked - drawn by FLvibed in
// OnRenderOverlay while the pad is the device. `prompt_bar` (CONTEXTUAL / ALWAYS / OFF): contextual = shown for ~3 s
// when a screen first appears this session, and for as long as a decision is pending (the owner says so). The owner of
// the current screen calls Set every frame it is up (the last caller of a frame wins; nothing set = nothing drawn).
// D-MENUS created it; D-BASES adopted it; the flight modals' own bar (flightmodal::PromptBar) folds onto
// it since the integration (INT-PAGE): one component draws every prompt bar.
#pragma once
#include "common.h"
#include "ui/glyphs.h"
#include "ui/prompts.h"

#include <string>

namespace promptbar
{
using Prompt = ::prompts::Prompt;
// This frame's prompts for `screen` (a stable id per screen / state: "mainmenu", "load", "save.keyboard" ...).
// A window, menu or modal keeps its prompts for as long as it is open (the owner, 6 Nov 2026). `pending` = a decision is pending (a confirm, a destructive hold).
// `priority`: when two owners set prompts in one frame (a room's strip under the pause menu), the higher wins the
// frame (equal: the later call); the menu screens, the OSK and the New Game prompt use 10, the flight modals 5.
void Set(const char* screen, const Prompt* items, int count, bool pending = false, int priority = 0);
// The same for a screen that is no window, a base room: under CONTEXTUAL its prompts show while the screen is new or a decision is pending, and fade otherwise.
void SetAmbient(const char* screen, const Prompt* items, int count, bool pending);
// The bottom lane's top edge for a screen height H (0.910 H: modules\hud\docs\UI_HUD2_EXTENDED.md 13), so a panel above it keeps
// clear (the font argument is kept for older callers).
float Top(float H, const render::Font& label);
// Prompts are set this frame, the pad is the device and prompt_bar is not OFF: the lane is in use on this screen (whether
// the contextual pills show or have faded) - uiskin trims the plates that end in the band (rule 3).
bool LaneWanted();
// Once per frame after the owners' Set calls (ctl::OnFrame), then once per overlay pass.
void Update(float dt);
void Draw();
// True while the bar draws (the pad HUD's legend steps aside: padhud::SetModal is the owners' business).
bool Visible();
// The pills the last overlay pass drew (none when the bar was hidden or faded), each as x0 y0 x1 y1 in pixels with its HOLD word above it, at most `max`:
// for what draws before the bar in the pass and keeps out of it (the map's place names: issue 72). How many.
int Plates(float* xyxy, int max);
void Shutdown(); // (fonts belong to uikit; nothing owned here yet)
std::string Status();
// The pills frame by frame (debug `prompttrace [reset]`): every change of what Draw decided, with the frame and the time.
// One frame of a blink shows here; a poll of Status from outside the game misses it (GitHub issue 10).
std::string Trace(bool reset);
} // namespace promptbar
