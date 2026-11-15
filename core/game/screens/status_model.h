// FLvibed - the STATUS page's model (the game's NN_PlayerInfo window; the HUD 2 page framework, audit 70b F4b, moved from
// modules\hud\src\nnpage.cpp): the pilot's numbers and the reputation list as plain values. The window stays on the screen:
// the focused faction's card is the game's own, shown through the page's hole. Filled by status_screen.cpp; drawn by
// modules\hud\src\pages\dealers\status_view.cpp. Nothing here calls the game.
#pragma once
#include "common.h"
#include "screen_rules.h"
#include "status_rules.h"

#include <string>
#include <vector>

namespace statusscreen
{
constexpr int kStats = 13; // the window's rows: PlayerInfoHeading0..12, PlayerInfoStat0..12

// The page's clicks (it has no action of the game's: its card follows the focus, status_screen.cpp Follow).
enum Hit
{
    kHitRow = 100, // a click on a faction's row (arg: its index in the shown list): the focus
    kHitFilter,    // SHOW <the next filter>: as the pad's X
};

// One faction of the window's list: its reputation id, its place in the window's list (the card's index), the standing.
struct Rep
{
    u32 id = 0;
    int index = 0;
    float v = 0;
    std::wstring name;
};

// What outlives an open, as the page's statics always did: the filter, the sort, the list's focus and window, the headings.
// One of these (status_screen.cpp's) for every model.
struct Focus
{
    statusrules::Filter filter = statusrules::All;
    bool byName = false;
    screens::List list;              // over `shown`; its rows the view's (its room), its count 0 on every open
    std::wstring head[kStats];       // the window's headings without their colons
};

struct Model
{
    Focus* focus = nullptr;          // status_screen.cpp's (Open)
    // read every 0.5 s, at once on the open (the runtime's Read)
    std::vector<Rep> reps, shown;    // the window's factions that have a name; those the filter passes, sorted
    std::wstring stat[kStats];       // the window's numbers (the game writes them under the card too: read hidden then)
    u32 root = 0;                    // the window
    u32 readDue = 0;                 // when they are read next (GetTickCount; 0 at once)
    // the card: the game's own, the focused faction's (Follow)
    bool card = false;               // a card was shown since the open
    int cardShown = -1;              // whose (Rep::index)
    std::wstring cardName;
    u32 stepAt = 0, cardAt = 0;      // the focus's last step, the card's last show (GetTickCount | 1; 0 never): a walk's card waits
};
} // namespace statusscreen
