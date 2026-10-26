// The shared prompt bar of the controller UI (docs\UI_PLAN.md 6.5, docs\UI_SPEC.json: every screen's `prompts`): the
// bottom-right row of glyph + label pairs (placement: promptbar.cpp's header, docs\UI_HUD2_EXTENDED.md 13), confirm
// first, at most five, "HOLD" marked - drawn by FLvibed in
// OnRenderOverlay while the pad is the device. `prompt_bar` (CONTEXTUAL / ALWAYS / OFF): contextual = shown for ~3 s
// when a screen first appears this session, and for as long as a decision is pending (the owner says so). The owner of
// the current screen calls Set every frame it is up (the last caller of a frame wins; nothing set = nothing drawn).
// D-MENUS created it (25 Oct 2026); D-BASES adopted it; the flight modals' own bar (flightmodal::PromptBar) folds onto
// it since the integration (INT-PAGE, 25 Oct 2026): one component draws every prompt bar.
#pragma once
#include "common.h"
#include "glyphs.h"

#include <string>

namespace promptbar
{
struct Prompt
{
    glyphs::Id btn;             // the glyph
    const wchar_t* label;       // "SELECT" (upper case, as the spec writes them); copied by Set (a temporary is fine)
    bool hold = false;          // a hold action ("HOLD" after the label)
    glyphs::Id btn2 = glyphs::COUNT; // a second glyph for the same label ("{LB} {RB} SHIFT"); COUNT = none
    float progress = 0;         // a hold in progress: 0..1 draws the sweep under the pair (the owner's hold timer)
    bool enabled = true;        // false: the label greyed (an action not available now; the goal panel's QUEUE)
};
// This frame's prompts for `screen` (a stable id per screen / state: "mainmenu", "load", "save.keyboard" ...).
// `pending` = a decision is pending (a confirm, a destructive hold): shown whatever the contextual timer says.
// `priority`: when two owners set prompts in one frame (a room's strip under the pause menu), the higher wins the
// frame (equal: the later call); the menu screens, the OSK and the New Game prompt use 10, the flight modals 5.
void Set(const char* screen, const Prompt* items, int count, bool pending = false, int priority = 0);
// The bottom lane's top edge for a screen height H (0.910 H: docs\UI_HUD2_EXTENDED.md 13), so a panel above it keeps
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
void Shutdown(); // (fonts belong to uikit; nothing owned here yet)
std::string Status();
} // namespace promptbar
