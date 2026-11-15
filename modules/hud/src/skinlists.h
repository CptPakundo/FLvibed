// The save lists under HUD 2 (audit 139_ B): the game's LOAD GAME / SAVE GAME lists draw a fixed count of rows at the font's line height
// into a box of fixed height, so HUD 2's larger text (game_text_size) ran them past their panel. Each frame of an open list this shows
// only the rows that fit (skinrules::RowsThatFit) on both columns, trims the box's hit area to them, and keeps the game's own scroll bar
// able to reach every save (its page = the rows shown, shown whenever the saves outnumber them). Facts: docs\FREELANCER_RE.md "The save
// lists' rows". HUD 2 only: Leave gives the open list its own box, rows and bar back.
#pragma once
#include "common.h"

#include <string>

namespace skinlists
{
// Every frame of HUD 2's skin. `on`: the skin draws (the module enabled, HUD 2 selected).
void Frame(bool on);
// HUD 2 leaves: the open list's box, rows and bar as the game made them.
void GiveBack();
std::string Status();
} // namespace skinlists
