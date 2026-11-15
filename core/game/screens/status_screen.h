// FLvibed - the STATUS page's reads (the game's NN_PlayerInfo window; the HUD 2 page framework, audit 70b F4b; moved from
// modules\hud\src\nnpage.cpp unchanged). The window stays on the screen, the focused faction's card (its own) through the
// page's hole; the page reads its numbers and its list of factions, and asks it for the card of the faction in focus.
#pragma once
#include "status_model.h"

namespace hk
{
struct Sym; // core\hook.h: a signed game symbol
}

namespace statusscreen
{
// A new model (the runtime's open): it points at the one Focus, whose list starts empty (count 0, the focus kept).
void Open(Model& m);
// The Focus every model points at (the verb `nnpage` reads its focus on every page).
const Focus& Kept();

// The reads (the runtime calls them: modules\hud\src\pages\pagerun.cpp). The stamp changes each frame; Read takes the
// window's numbers and factions every 0.5 s (at once on the open), true when it read.
uint64_t Stamp();
bool Read(Model& m, u32 root);
// The factions the filter passes, in the sort's order; the focus fitted to them.
void ShowReps(Model& m);
// The focus stepped (a walk's card waits for it to rest: Follow).
void Stepped(Model& m);
// The card follows the focus: the window's own card of the focused faction (exe::StatusFactionCard), a walk's when it rests
// (0.1 s) or every 0.25 s; its close button made inert (the card stands as long as the page). Once a frame, after the wheel.
void Follow(Model& m);

// The window the page stands over, and its card (whose box the page's hole is cut round: nnpage.cpp).
extern const hk::Sym* const kWindowClass;
constexpr const char* kWindowName = "NN_PlayerInfo";
constexpr const char* kCardName = "FactionInfoCard";
} // namespace statusscreen
