// The pad on the nav map (docs\UI_PLAN.md 6.4 "nav map", UI_SPEC.json screen navmap, bindings.common.map). Phase D of
// the UI build (the D-BASES track, 25 Oct 2026). The game's own map window (NN_NavMap: NavMapInZoom / NavMapOutZoom,
// NavMapUniverseButton, NavMapShipButton, NavMapSetWaypointButton, NavMapClearWaypointButton,
// NavMapClearAllWaypointsButton, NavMapBestPathButton, NavMapCloseButton - names from Freelancer.exe's string table,
// verified in game where the report says so) stays; the pad drives its buttons natively (uiclick::Click), the waypoint
// list through the game's own calls (space::), the route through the Advanced Autopilot's travel.
//
// The map cursor: the map's objects are drawn by the nav map's own code, not as FlControls, so their screen positions
// are not readable from the control tree - the left stick drives the game's cursor through the existing free-cursor
// mechanism (the game reacts to it as to the mouse: hover names, a click selects), with the magnetism of padbases
// (which finds only FlControls: the map's buttons) and no D-pad snapping to objects. Reported honestly in the track's
// report. A FLvibed-drawn destination list (View) beside the map lists this system's bases, gates and holes by distance;
// A plots the game's best path to the chosen one (the waypoint list, as the map's own "Best path" does).
#pragma once
#include "common.h"

#include <istream>
#include <string>

class Gamepad;

namespace padmap
{
// Once per frame from padbases::Update. `mayOpen`: in space with a NeuroNet window and no other known window, or docked
// with the map's window over the room (PADFIX 25 Oct 2026: docked it was read as the room) - the map is looked for.
// Returns true while the map owns the frame (its window is open). Docked: no travel, no destination list.
bool Update(float dt, const Gamepad& pad, bool mayOpen);
u32 Owned(); // XINPUT masks consumed this frame
bool IsOpen();
void Draw(void* device); // the destination list and the selection line (OnRenderOverlay)
std::string Status();
std::string Command(std::istream& in); // debug: map state | dest
void Shutdown();
} // namespace padmap
