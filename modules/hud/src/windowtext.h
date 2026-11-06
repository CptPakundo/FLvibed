// The game's windows' text by role (`window_text`; modules\hud\docs\UI_HUD2.md 5.36-5.38, core\game\roletext.h): text takes the room it has.
#pragma once
#include "common.h"

#include <string>

namespace windowtext
{
// A text control's height as laid out, pixels (0 = none): what stands under a text goes by it, the text having grown.
float TextHeight(u32 ctl);
// Every frame. `on`: the setting, HUD 2 and game_text_size 100; off puts every font back.
void Frame(bool on);
// Debug `gametext window`: what the open window has now.
std::string Status();
// Debug `gametext texts`: every shown text and rich text of any window, with its font, size and box.
std::string Texts();
} // namespace windowtext
