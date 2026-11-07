// The game's windows' text by role (`window_text`; modules\hud\docs\UI_HUD2.md 5.36-5.38, core\game\roletext.h): text takes the room it has.
#pragma once
#include "common.h"

#include <string>

namespace windowtext
{
// A text control's height as laid out, pixels (0 = none): what stands under a text goes by it, the text having grown.
float TextHeight(u32 ctl);
void Park(u32 ctl); // this text off the screen until another or 0 is asked: HUD 2 says it instead
std::string Overflows(); // every text out of its frame or box, one a line: `gametext fit`
float TextWidthPx(u32 ctl); // a shown text's laid out width, 0 none
// Every frame. `on`: the setting and HUD 2, at any game_text_size: the ladder reads the fonts as the game holds them and the rich table goes back to the size
// applied, so Large's 125 keeps text by role. Off puts every font back.
void Frame(bool on);
// Before the UI pass draws: the row names cut to fit (the text-fit rule's last step) set again where the game put the full name back
void ApplyCuts();
// In HUD 2's draw pass: those names drawn whole as marquees over their rows
void DrawMarquees();
// R8's frame: the NeuroNet windows stand on one top line and left edge. The log, status and info windows always; NN_Inventory only with this, until probe 18
// has shown its hit boxes follow (debug `gametext frame 0|1`).
void SetInventoryFrame(bool on);
// Debug `gametext window`: what the open window has now.
std::string Status();
// Debug `gametext texts`: every shown text and rich text of any window, with its font, size and box.
std::string Texts();
} // namespace windowtext
