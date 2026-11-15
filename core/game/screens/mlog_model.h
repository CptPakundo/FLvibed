// FLvibed - the LOG page's and the death page's models (the game's NeuroNetStoryStar window; the HUD 2 page framework, audit
// 70b F4a, moved from modules\hud\src\nnpage.cpp): what the pages draw round the game's journal, as plain values. The journal's
// text is the game's own, shown through a hole in the page; its two tabs and its scroll bar are the window's. Filled by
// mlog_screen.cpp; drawn by modules\hud\src\pages\neuronet\mlog_view.cpp and death_view.cpp. Nothing here calls the game.
#pragma once
#include "common.h"
#include "places_model.h"

#include <string>
#include <vector>

namespace mlogscreen
{
// LOG's actions (mlog_screen.cpp's table: the window's own tab buttons, the NeuroNet's map tab).
enum Action
{
    kActTab = 1,   // a tab: arg 1 MISSION (MissionButton), 0 JOURNAL (LogButton)
    kActShowOnMap, // SHOW ON MAP (the game's own map tab command)
};

struct Model
{
    // read in the frame's update (the runtime's Read)
    std::wstring objective;             // the current step's text as the game formats it, "" none
    std::vector<screens::Place> places; // what the player has found in this system, nearest first (none docked)
    u32 placesDue = 0;                  // when the places are read next (GetTickCount; 0 at once)
    // read at the view's moment (the runtime's Prepare), as the page read them while it drew
    bool mission = false; // the window's tab: MISSION, else JOURNAL
    float timer = 0;      // the mission's timer (seconds), 0 none
    screens::Here here;
    std::wstring ship;    // the ship's name
    int journal = 0;      // the journal's scroll bar's position (the bar's own unit, rounded; 0 with no bar): the test verb `page` prints it as first=
};

// One of the death screen's three buttons (the game's own, where the game put them): its box on the screen and its word.
struct DeathButton
{
    bool ok = false;
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    std::wstring word;
};
struct DeathModel
{
    DeathButton buttons[3]; // LOAD AUTOSAVE, MAIN MENU, LOAD GAME
    float mouseX = 0, mouseY = 0;
};
} // namespace mlogscreen
