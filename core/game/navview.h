// The nav map's continuous zoom and pan (docs\MAP_PLAN.md "P2's design"): the game's own view-2 transform driven through its own fields and placement calls, so the game's map stays the game's.
// The pure part: navview_rules.h. Fails closed: a map that does not read is left alone.
#pragma once
#include "common.h"
#include "game/navview_rules.h"

#include <istream>
#include <string>

namespace navview
{
// Once a frame while the map is open, with this frame's input and the game's cursor in pixels. True when the view moved.
bool Frame(u32 root, const Drive& in, float dt, float cursorX, float cursorY);
// The view can move now: the system's or sector's view, no game animation running, a layer that shows the map.
bool Movable();
// At the widest: the system's whole map (view 1), nothing left to zoom out.
bool AtWidest();
// The full-screen map (modules\hud\src\navnew.*): the box units its viewport shows, which are not the game's fields there. 0: the game's.
void SetFrame(float boxW, float boxH);
// The screen's pixels for a box unit at the depth the map's objects are held at, by the camera; 0 none.
float PixelsPerBox(u32 root);
// The game places everything again by its fields: after they changed.
void Replace(u32 root);
// The full-screen map stays in view 2, at any zoom (the lanes follow its fields only there: read in frames, 7 Nov 2026): call each frame, with or without a pad.
void Settle(u32 root);
// The cursor stands on the map's viewport.
bool OnMap(u32 root, float x, float y);
// The player's waypoint under the cursor (its index in the game's list), -1 none: the map's waypoint records (+0x4B8 by index, type 3), within `reach` pixels.
int WaypointAt(u32 root, float x, float y, float reach);
// A map point (the records' coordinates) on the screen, pixels, by the map's own camera (+0xC80 vt[0x34]) at the place 0x496600 gives a record: where the game draws it. False: no camera.
bool Project(u32 root, float wx, float wz, float& sx, float& sy);
// The map shows the system the ship is in, at a system's or sector's view: what +0x7E0 says against the player's system, the game's own compare in 0x49EA50.
bool ShowsShipSystem(u32 root);
// The same for a point of the world, in metres.
bool ProjectWorld(u32 root, float x, float z, float& sx, float& sy);
// The named object nearest this pixel within `reach` pixels: its world point in metres and its id (0: a record with none).
bool ObjectAt(u32 root, float x, float y, float reach, float* world, u32& id);
// The named objects on the viewport now, pixels: x, y pairs into xy[0..2 max). Returns how many.
int Objects(u32 root, float* xy, int max);
// The map centres on this world point at the next Frame, at the zoom it has.
void CentreOn(float x, float z);
// The window went.
void Closed();
// Debug: map view [state] | zoom <z> | pan <du> <dt>: what the pad does, without one.
std::string Command(std::istream& in);
} // namespace navview
