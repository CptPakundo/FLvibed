// The weapon group editor (GitHub issue 4, 3 Nov 2026; docs\UI_HUD2.md "Weapon groups on the pad"): the action wheel's
// EDIT GROUPS opens a pad-native list of the ship's guns for the active group. A switches a gun on / off in it
// (USER_TOGGLE_WEAPONn), LB / RB go to the group before / after it (USER_WEAPON_GROUPn, which also makes it the active
// group), B closes. The game keeps the groups: it stores the selection when the group changes. The guns only, as the
// HUD's group rows: launchers fire on their own buttons in every mode. Works in both HUD styles.
#pragma once
#include "common.h"
#include "glyphs.h"
#include "wgroups_rules.h"

#include <istream>
#include <string>

namespace wgroups
{
void Open();
void Close();
bool IsOpen();
// Once per frame while open (controller.cpp UpdateSpace, before the flight dispatch): the pad's buttons this frame.
void Update(float dt, const bool down[glyphs::COUNT]);
// Inside the overlay pass (after padhud::Render).
void Render(void* device);
// The buttons the open editor owns (hidden from the flight dispatch): A, B, LB, RB, the D-pad's up / down.
bool OwnsButton(glyphs::Id b);
// The groups with guns (bit g-1 = group g); 0 = unknown.
unsigned GroupsWithGuns();
// Debug: wgroups open|close|up|down|a|next|prev|state
std::string Command(std::istream& in);
} // namespace wgroups
