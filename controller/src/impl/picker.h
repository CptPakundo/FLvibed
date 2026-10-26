// The target picker (docs\UI_SPEC.json `hud` state `picker`; AUTOPILOT_UX.md 5.4a; UI_PLAN.md 6.5): LB tap opens a
// pad-native list over the same objects the game lists, filter-independent (space::Contacts), in categories on the
// bumpers - THREATS (hostile ships, attackers first), MISSION (what the objective points at, the mission waypoint),
// NAVIGATION (bases, docking rings, lane rings, gates, holes), SHIPS (the others), LOOT - rows sorted by relevance with
// name, distance, the HUD's attitude colour and the action A would start. D-pad up / down walks the rows (never the
// left stick: it keeps flying); moving the focus previews the target NATIVELY (space::SetTarget, the game's own target
// box follows); A keeps the selection and closes, B restores the target the player had and closes. Drawn right-middle
// where the legend was (padhud steps aside: SetModal), with its own prompt bar bottom right.
#pragma once
#include "common.h"
#include "glyphs.h"

#include <istream>
#include <string>

namespace picker
{
void Open();
// keep = false restores the target the player had when the picker opened (B); true keeps the previewed one (A).
void Close(bool keep);
bool IsOpen();
// Once per frame while open (controller.cpp UpdateSpace, before the flight dispatch): the pad's buttons this frame.
void Update(float dt, const bool down[glyphs::COUNT]);
// Inside the overlay pass (after padhud::Render).
void Render(void* device);
// The buttons the open picker owns (hidden from the flight dispatch): A, B, LB, RB, the D-pad.
bool OwnsButton(glyphs::Id b);
// Debug: picker open|close|state|up|down|next|prev|a|b
std::string Command(std::istream& in);
} // namespace picker
