// FLvibed - the BEST PRICES page's model (the game's commodity list by base, NN_CommodityByBase; the HUD 2 page framework,
// audit 70b F3a): the window's rows in the order that answers the question, its words, and what the page shows beside them.
// Filled by prices_screen.cpp from the window it parks; drawn by modules\hud\src\pages\dealers\prices_view.cpp. Pure.
#pragma once
#include "common.h"
#include "prices_rules.h"
#include "screen_rules.h"

#include <string>
#include <vector>

namespace pricesscreen
{
// The page's actions (prices_screen.cpp's table: the window's own buttons).
enum Action
{
    kActClose = 1, // CLOSE (the window's own close button)
};

struct Model
{
    std::vector<screens::Quote> quotes; // the bases the player has been to, the highest price first (read every 0.2 s)
    std::wstring title;                  // the window's title, upper-cased (the good's name)
    // read each frame, as the page read them while it drew
    std::wstring colBase, colSystem, colBuySell, colPrice; // the window's column words
    std::wstring here;                                     // the base the player is docked at
    bool docked = false;
    bool cargo = false; // the hold's size was read
    int used = 0, cap = 0;
    mutable screens::List list; // the rows' window: its focus is the page's (the pad), its height the view's
    u32 rowsDue = 0;            // when the rows are read next (GetTickCount; 0 at once)
};
} // namespace pricesscreen
