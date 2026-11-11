// The new map behind its own seam (modules\hud\docs\UI_HUD2_SKIN.md 6.1, step 10): off until the hit-box sweep and a second install have passed. Off, every entry here
// answers as if it were not there and the map is navskin's: the game's own size, the game's own labels.
#pragma once
#include "common.h"
#include "navmap_layout.h"

#include <istream>
#include <string>

namespace navnew
{
// Once a draw pass while the map shows: `map` its viewport in pixels. `pad`: the pad drives the zoom and pan (controller padmap.*); else the mouse's wheel and drag do, here.
void Frame(u32 root, const navlayout::Rect& map, bool pad);
// The map is at the solver's size: the panels stand where the solver says.
bool Grown();
// The full-screen map is switched on.
bool On();
// What the panels hold above their chips: heights in pixels; DrawLeft / DrawRight return the y under what they drew.
float LeftHeight(const navlayout::Rect& panel, float H);
float RightHeight(const navlayout::Rect& panel, float H);
float DrawLeft(const navlayout::Rect& panel, float H, bool title = true);
// The left card has something to say: an objective or a route. The right one: the selection's card.
bool HasInfo();
bool HasCard();
float DrawRight(const navlayout::Rect& panel, float H);
// HUD 2's labels, marks, sectors and route over the map's viewport; no label crosses a keep-out (the plates and words drawn over the map, and the hover's name tag, whose
// place keeps no label of its own while it shows).
void DrawOverMap(const navlayout::Rect& map, const navlayout::Rect* keep = nullptr, int nKeep = 0);
// The map holds a selection (its Set / Clear Waypoint stand): the record at the game's selected point gets the card.
void SetSelected(bool on);
// The map's window went: the game's label flag back. A write inside the window goes with it.
void Closed();
// Everything back while it still holds FLvibed's value.
void Shutdown();
// Debug: navnew [state] | on | off
std::string Command(std::istream& in);
std::string Status();
} // namespace navnew
