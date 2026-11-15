// FLvibed - the LOG page's and the death page's reads and actions (the game's NeuroNetStoryStar window; the HUD 2 page
// framework, audit 70b F4a; moved from modules\hud\src\nnpage.cpp unchanged). The window stays on the screen, its journal's
// text through the page's hole (the HUD module's windowtext places it); the page reads round it.
#pragma once
#include "mlog_model.h"
#include "action.h"

namespace mlogscreen
{
// The pages' reads (the runtime calls them: modules\hud\src\pages\pagerun.cpp). The stamp changes each frame: Read takes the
// objective each frame and the places every 2 s (at once on the open), true when the places were read. Prepare takes, right
// before the view, what the page read while it drew (the tab, the timer, where the player is, the ship, the journal's bar).
uint64_t Stamp();
bool Read(Model& m, u32 root);
void Prepare(Model& m, u32 root);
// The death page's: its three buttons and the mouse, right before the view.
void PrepareDeath(DeathModel& m, u32 root);

// The log after a death or a failed mission, in single player (docs\FREELANCER_RE.md "The death / mission-failed dialog"):
// the window shows its load buttons under the journal.
bool DeathShown();

// The window both pages stand over: NeuroNetStoryStar's class (exe::VtStoryStar).
extern const hk::Sym* const kWindowClass;
constexpr const char* kWindowName = "NeuroNetStoryStar";
constexpr const char* kTextName = "StoryStarHistoryText"; // the journal's text, which shows through the page

// LOG's actions (their ids: mlog_model.h Action): the runtime's only way to act for it.
extern const screens::ActionDesc kActions[];
extern const int kActionCount;
} // namespace mlogscreen
