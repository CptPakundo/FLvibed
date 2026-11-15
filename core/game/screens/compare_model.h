// FLvibed - the compare block as data (the HUD 2 page framework, audit 70b 2.4): an item's numbers against what the ship
// carries, as the pages show it beside an item (the equipment dealer, repair, INVENTORY). Read by compare_screen.cpp, drawn by
// modules\hud\src\pages\page_kit.h DrawCompare; moved here from page_kit.h in F3a so a screen's model can hold one. Pure.
#pragma once
#include "common.h"

#include <string>
#include <vector>

namespace screens
{
struct CompareRow
{
    std::wstring label, value, delta; // the number's name; its value and unit; the difference to the other item ("" none)
    u32 deltaRgb = 0;
};
struct CompareBlock
{
    bool ok = false;         // the item has numbers: nothing is drawn without
    std::wstring head, slot; // the heading (MOUNTED, VS ..., FITS A FREE HARDPOINT ...); the item's slot word, "" none
    u32 headRgb = 0;
    bool versus = false; // there is an item to differ from: the values stand left of the differences
    std::vector<CompareRow> rows;
};
} // namespace screens
