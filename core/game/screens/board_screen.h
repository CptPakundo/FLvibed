// FLvibed - the reads and actions of the job board's and the news's page (the game's NN_MissionVendor and NN_News windows; the
// HUD 2 page framework, audit 70b F3d / F3e; moved from modules\hud\src\basepage.cpp unchanged). The game's window stays where
// it stands and does the work: its rows (a row's click is the game's choice), its text (its own scroll bar scrolls it), ACCEPT
// and CLOSE. Its text and controls are placed round the page by the shell each frame before the page's reads (basepage.cpp
// BoardPlace: the game's text sits where the HUD module puts the page's panel).
#pragma once
#include "board_model.h"
#include "action.h"

namespace boardscreen
{
// The page's reads (the runtime calls them: modules\hud\src\pages\pagerun.cpp). Read: out of the window manager's slide, the
// first row picked once per opening when the game has none chosen (not whenever it has none: after a refused job the game
// clears its choice on purpose and shows the reason in the text, audit 81a_ section 4). The stamp changes each frame.
uint64_t Stamp();
bool Read(Model& m, u32 root);
// What the view shows of the game, read right before it draws (the old page read it as it drew): the rows, the chosen one's
// head or the game's caption of a refused job, the offers' difficulties, the ticker, the text's bar.
void ReadFrame(Model& m);
// The news's ticker runs on by `px` (each frame either board stands, as before: its offset is one across openings).
void Advance(float px);

// Asked as the page's prompts and pad decide, at that moment (the old page asked the game the same way), and by the shell.
int Sel(int mode);            // the game's chosen row, -1 none
u32 TextBar(int mode);        // the text's own scroll bar (it stands while the text overflows), 0 none
const char* TextName(int mode); // the window's text control (the shell places it: basepage.cpp BoardPlace)
bool AcceptStands();          // the job board's ACCEPT stands

// The windows the pages claim: NN_MissionVendor's class (exe::VtMissionVendor), NN_News's (exe::VtNews).
extern const hk::Sym* const kJobsClass;
extern const hk::Sym* const kNewsClass;
constexpr const char* kJobsName = "NN_MissionVendor";
constexpr const char* kNewsName = "NN_News";
constexpr const char* kJobsClose = "MissionVendorCloseButton";
constexpr const char* kNewsClose = "NewsCloseButton";

// The pages' actions (their ids: board_model.h Action), one table per window: the runtime's only way to act for them, each the
// window's own control (borrowed).
extern const screens::ActionDesc kJobsActions[];
extern const int kJobsActionCount;
extern const screens::ActionDesc kNewsActions[];
extern const int kNewsActionCount;
} // namespace boardscreen
