// FLvibed - the repair page's model (the game's NN_Repair window; the HUD 2 page framework, audit 70b F3a): what the page
// draws, as a plain value. Filled by repair_screen.cpp from the window it parks (its nine rows, their ticks, its sum: the
// window does the work) and the ship; drawn by modules\hud\src\pages\dealers\repair_view.cpp. Nothing here calls the game.
#pragma once
#include "common.h"
#include "compare_model.h"

#include <string>
#include <vector>

namespace repairscreen
{
constexpr int kRows = 9; // the window's rows (RepairCheckBox0..8)

// The page's actions (repair_screen.cpp's table: each the window's own button) and its row's click (the page's own).
enum Action
{
    kActTick = 1, // a row's tick (arg: the window's row, Row::slot)
    kActAll,      // TICK ALL / NONE (RepairToggleChecks)
    kActRepair,   // REPAIR (RepairOkButton, while canMend)
    kActClose,    // CLOSE (RepairCancelButton)
    kHitRow = 100 // a click on a row (arg: its index): its pick, and on the row in focus its tick
};

// One of the window's rows: a damaged item, or the hull (no item of the ship's list has its name).
struct Row
{
    int slot = 0; // the window's row (its controls' number)
    bool ticked = false, hull = false;
    float health = -1;
    u32 arch = 0;
    long long price = 0;
    std::wstring name;
};

struct Model
{
    std::vector<Row> rows; // the window's rows that stand, in its order (read every 0.2 s, out of the window's slide)
    int sel = 0;           // the row in focus: the page's own, at the first row on every open
    // read each frame, as the page read them while it drew
    long long total = 0, credits = 0; // the window's sum (RepairTotalCost), the player's cash
    bool canMend = false;             // the window's OK stands and its sum is more than nothing: REPAIR may go
    std::wstring shipName;
    bool shipOk = false;    // the ship's facts were read (its armour, its card)
    std::wstring shipHull;  // its armour when whole, in words (equip_rules ValueWord)
    screens::CompareBlock compare; // the row in focus against the ship (an item's; none for the hull)
    // the card: the item's in focus, else the ship's; a new card starts at its top
    u32 cardKey = 0;
    std::wstring cardText;
    mutable int cardFirst = 0, cardLines = 0, cardRoom = 0; // where it stands; what the view laid out (the view's)
    u32 rowsDue = 0;                                        // when the rows are read next (GetTickCount; 0 at once)
};
} // namespace repairscreen
