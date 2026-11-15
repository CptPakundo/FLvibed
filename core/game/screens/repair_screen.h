// FLvibed - the repair page's reads and actions (the game's NN_Repair window; the HUD 2 page framework, audit 70b F3a; moved
// from modules\hud\src\basepage.cpp unchanged). The game's window stands parked off the screen and does the work: its
// ticks, its sum, its OK. Its rows are its nine; their health is the ship's list's (the window's bars hold no share to read).
#pragma once
#include "repair_model.h"
#include "action.h"

namespace repairscreen
{
// The page's reads (the runtime calls them: modules\hud\src\pages\pagerun.cpp). The window's texts change under the page
// with no event it hears, so the stamp changes each frame and Read takes the frame's values each frame (as the page read
// them while it drew), the rows every 0.2 s out of the window's slide (sooner after a tick or a repair). True when the rows
// were read.
uint64_t Stamp();
bool Read(Model& m, u32 root);
// The focus's card and compare block read again for the row in focus (after the page moved it): at once, not a frame late.
void Refocus(Model& m);
// What the `basepage` verb prints of the window itself: its sum's text, whether REPAIR may go now.
std::wstring TotalText();
bool CanMend();

// The window the page claims: NN_Repair's class (exe::VtRepair).
extern const hk::Sym* const kWindowClass;
constexpr const char* kWindowName = "NN_Repair";
constexpr const char* kCloseButton = "RepairCancelButton";

// The page's actions (their ids: repair_model.h Action): the runtime's only way to act for it, each the window's own button
// (borrowed).
extern const screens::ActionDesc kActions[];
extern const int kActionCount;
} // namespace repairscreen
