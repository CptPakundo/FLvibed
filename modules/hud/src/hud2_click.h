// HUD 2's keyboard-and-mouse clicks, the game's side (hud2_click.cpp): the hotspot set the parts record into while they
// draw, handed to the controller host; the clicks run through the game's commands; the contact list from the game's
// contacts. For hud2.cpp and hud2_data.cpp; the parts draw their clickable things in hud2_click_draw.cpp (hud2_impl.h).
#pragma once
#include "hud2_impl.h"
#include "game/space.h"

#include <istream>
#include <string>
#include <vector>

namespace hud2
{
// The frame's host (hud2_frame.h FrameHost hotStart / hotRect / hotHovered): the pass's start empties the set and records
// only when `on` (the keyboard form, flying); a slot's rectangle (pixels, without the transition's slide; whole pixels
// kept); the slot under the game's cursor last frame, -1 none.
void HostHotStart(bool on);
void HostHotRect(int slot, float x0, float y0, float x1, float y1);
int HostHotHovered();
// From Update: hands the recorded set to the host (every frame while on; `on` false hides them) and runs the clicks
// queued since the last frame.
void HotUpdate(Model& m, bool on);
// HUD 2 leaving (hud2.cpp GiveBack): the queued clicks dropped, its hotspots off the click box.
void HotGiveBack();
// A click (or the wheel) changed what the model shows: true once, then Update refreshes the 10 Hz data at once.
bool HotRefreshWanted();
// ImplApi OnUiButton for kUiBtnHud2 .. + kHotCount: queued, run in the next Update (outside the game's mouse dispatch).
void OnHotClick(int slot);
// The contact list mode (Slow at 10 Hz): fills m.threats from the contacts under the list's filter; false when the mode
// is off (the threat list as before).
bool FillList(Model& m, const std::vector<space::Contact>& all, u32 targetId);
// Debug: hud2 hot | hud2 click <slot> | hud2 list [on|off|<filter>]
std::string HotCommand(const std::string& what, std::istream& in);
} // namespace hud2
