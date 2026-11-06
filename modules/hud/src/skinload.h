// The loading screen, form C (modules\hud\docs\UI_HUD2_SKIN.md 6.4, R17): a navy fill and the place at once, a bar only past 1.5 s, drawn over the game's own bar
// in each load frame (the host's bracket, core\host\router.h SetLoadDraw). The game never waits for it. Any failure leaves the game's native screen.
// Which data names the place on each load path is a probe: until it is answered the screen says LOADING; debug `uiskin loadplace 1` tries the player's base.
#pragma once
#include "common.h"

#include <string>

namespace skinload
{
// The bracket's call, the scene open.
void Draw();
// The UI pass runs again: the load is over.
void Ended();
void SetPlaceProbe(bool on);
std::string Status();
} // namespace skinload
