// FLvibed - what the screens read from and press on the game's parked window (the HUD 2 page framework, audit 70b F3a): a
// control's text by its name, a numbered control, the window's own close button. Shared by the docked pages' screens and
// their shell (modules\hud\src\basepage.cpp), so a close is pressed at one place.
#pragma once
#include "common.h"

#include <string>

namespace screens
{
// The text of the shown control `name`, "" when none stands (winplates::TextOf).
std::wstring TextIn(const char* name);
// The shown control `format` with `i` in it ("RepairCheckBox%d"), 0 none.
u32 Ctl(const char* format, int i);
// The window's own close button `button` pressed (a borrowed click, as the window's own X); false when it does not stand.
bool PressClose(const char* button);

// Whether control `ctl` stands in `window` (by its parents, at most 8 up). At the final stage the two lists' controls do not
// stand in one order, so a named control of one list is told by its window (moved from basepage.cpp in F3b).
bool InWindow(u32 ctl, u32 window);
// The first or second control named `name` on the screen that stands in `window`, 0 none (baseinfo::Find's nth).
u32 CtlIn(const char* name, u32 window);
// The game's list window (NN_Inventory, NN_Dealer) scrolled by its own bar so its five rows start at `first` (-1: as it is),
// or start where its five meshes show the most of the page's shown rows (their game rows, -1 none: BestWindowStart). A
// borrowed scroll (moved from modules\hud\src\nnpage_kit.h in F3b; its name stays there).
void SyncList(u32 window, int first);
void SyncList(u32 window, const int* gameRow, int n);
} // namespace screens
