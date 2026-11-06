// The new map behind its own seam (modules\hud\docs\UI_HUD2_SKIN.md 6.1, step 10): off until the hit-box sweep and a second install have passed. Off, every entry here
// answers as if it were not there and the map is navskin's: the game's own size, the game's own labels.
#pragma once
#include "common.h"
#include "navmap_layout.h"

#include <istream>
#include <string>

namespace navnew
{
// Once a draw pass while the map shows: `map` its viewport in pixels.
void Frame(u32 root, const navlayout::Rect& map);
// The map is at the solver's size: the panels stand where the solver says.
bool Grown();
// What the panels hold above their chips: heights in pixels; DrawLeft / DrawRight return the y under what they drew.
float LeftHeight(const navlayout::Rect& panel, float H);
float RightHeight(const navlayout::Rect& panel, float H);
float DrawLeft(const navlayout::Rect& panel, float H);
float DrawRight(const navlayout::Rect& panel, float H);
// HUD 2's labels, marks and route over the map's viewport.
void DrawOverMap(const navlayout::Rect& map);
// The map's window went: the game's label flag back. A write inside the window goes with it.
void Closed();
// Everything back while it still holds FLvibed's value.
void Shutdown();
// Debug: navnew [state] | on | off
std::string Command(std::istream& in);
std::string Status();
} // namespace navnew
