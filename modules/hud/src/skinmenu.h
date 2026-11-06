// The main menu's CONTINUE (modules\hud\docs\UI_HUD2_SKIN.md 6.3, D9, R17): one press loads the latest save, on a floating chip in the game's first button place;
// the game's five buttons close up to the five places under it. The list is the save manager's, read at the menu; no save, no chip, and the five stay put.
// The load runs through the game's own load screen (the quick load 0x5B2C70 brings rooms up without characters), so it costs that screen's slide; a load that
// has not started after six seconds gives up and leaves the load screen open. A save of another mode says so beside the chip. Button moves are remembered and
// put back while they still hold FLvibed's value; `continue` in skin_native keeps the menu as the game's.
#pragma once
#include "common.h"

#include <string>
#include <vector>

namespace skinmenu
{
// Each frame, from uiskin's pass under the UI: the save list, the buttons' places. `menuRoot`: the main menu shown now, 0 none.
void Layout(u32 menuRoot, const std::vector<u32>& buttons, bool on);
// In the frame: a click on the chip, the load under way. `mouse`: the player is on the mouse.
void Frame(float dt, bool mouse);
// The chip, under the UI pass, and the FLvibed version beside the game's.
void Draw(u32 menuRoot);
bool Offered();
bool Focused();      // the focus stands on the chip, by the mouse or the pad
void Focus(bool on); // the pad's
void Go();
// The buttons back at the game's places.
void GiveBack();
std::string Status();
} // namespace skinmenu
