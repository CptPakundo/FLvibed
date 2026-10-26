// Extended HUD 2's overlay on the docked screens (docs\UI_HUD2_EXTENDED.md 10): the context strip (base, room, system;
// credits, cargo, ship; a pending purchase's deltas), the character card that replaces the game's tooltip on the pad's
// room hover, and the decision numbers (price-quality words, the compare line, the trade-in / net, the job's type and
// difficulty, the offer's giver and reward). Drawn in OnRenderOverlay while ui_style is HUD 2; everything from data the
// game holds (core\game\baseinfo.*), nothing when a value is unknown (fails closed). The STRIP track implements it.
#pragma once
#include "common.h"

#include <istream>
#include <string>
#include <vector>

namespace uistrip
{
// Once per frame (OnFrame): on = ui_style is HUD 2 and the player is docked (or a NeuroNet window is open docked).
void Update(float dt, bool on);
// From OnRenderOverlay, after the game's UI pass and before the prompt bar.
void Draw(void* device);
// What Draw drew this frame (pixels: the strip's scrim, the character card, each decision word) - the prompt lane's
// keep-outs (it draws before the prompt bar in the overlay pass).
struct DrawnRect
{
    float x0, y0, x1, y1;
    int part = 0; // 0 the strip, 1 the character card, 2 the decision words (each dims under FLUF's windows on its own)
};
const std::vector<DrawnRect>& Drawn();
// The strip's bottom edge and the room bar's keep-out for other overlay owners (fractions of H; 0 when not drawn).
float StripBottom();
// The strip's right edge this frame (pixels, its scrim included; 0 = no strip drawn): the prompt lane's keep-out.
float StripRight();
// The game's `ToolTip` control while the character card replaces it (0 otherwise): uiskin hides the native box under
// the card then and only then (the pause menu's button help is the same ToolTip and stays).
u32 CardTooltip();
// The overlay runs (ui_style HUD 2, docked): its card carries the room character's offer word.
bool Active();
// The game's own name card's top-left corner (pixels) while its `ToolTip` shows on screen: padbases' offer label under
// ui_style 0 sits on it.
bool GameCardCorner(float& x, float& y);
// Under ui_style 0, from OnRenderUnderHud before the UI pass (uiskin::RenderUnder), every frame: while a room character
// is hovered, the game's own ToolTip (its name card) goes where PlaceCard's rules put a card (above the head, else
// beside, never over the character's box, inside the safe area) - a write of its +0x60 position, which the game's
// ToolTip draw projects each frame; nothing restored when the hover ends (the game places it fresh). `scene`: a room
// scene is letterboxed (the game's place).
void NativeCard(bool scene);
// Debug: uistrip state | data
std::string Command(std::istream& in);
void Shutdown();
} // namespace uistrip
