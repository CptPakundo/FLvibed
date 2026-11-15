// FLvibed - the INVENTORY page's model (the game's NN_Inventory window with its jettison panel; the HUD 2 page framework,
// audit 70b F4 unit b2, moved from modules\hud\src\nnpage.cpp): the ship's items in the window's filter, its empty
// hardpoints, the list's focus, and what the page read of the game as it drew, as plain values. The window stays on the
// screen: the item's card is the game's own (its InfoCard_Info through the page's hole), its five meshes stand in the rows.
// Filled by inventory_screen.cpp; drawn by modules\hud\src\pages\dealers\inventory_view.cpp. Nothing here calls the game.
#pragma once
#include "common.h"
#include "compare_model.h"
#include "screen_rules.h"
#include "game/equip_rules.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace inventoryscreen
{
// The page's actions (inventory_screen.cpp's table: each the window's own handler or button) and its clicks that need the
// model (the page's own: inventory_page.cpp Hit).
enum Action
{
    kActSelect = 1, // the game's selection brought to the page's focus (InvSelectRow): its card, its JETTISON
    kActFilter,     // one of the window's five filters (arg: 0..4; InvSetFilter)
    kActMount,      // MOUNT / UNMOUNT the focus, or mount on an EMPTY hardpoint the first item of the hold that takes it (InvMount)
    kActJettison,   // JETTISON (JettisonButton); a single item asks once more first (the page's arming)
    kActRoutes,     // BEST PRICES (the hidden TradeRouteButton)
    kActSlide,      // the jettison quantity moved by `arg` notches (JettisonQuantitySlider)
    kActBar,        // ... put where the mouse clicked the bar (arg: its cell, 0..kJetCells - 1)
    kActAccept,     // the jettison's JETTISON (JettisonAcceptButton)
    kActCancel,     // its CANCEL (JettisonCancelButton)
    kHitRow = 100,  // a click on a row (arg: its index in the page's list): the focus there, the game's selection after it
    kHitTag,        // the compare tag's chip
    kHitJetLess,    // the jettison's - chip
    kHitJetMore,    // + chip
    kHitJetMax,     // MAX chip
};
constexpr int kJetCells = 20; // the jettison bar's cells the mouse can click

// One row: an item the window answers for its row, or an EMPTY hardpoint (hp >= 0).
struct Item
{
    u32 desc = 0, arch = 0;
    int row = 0, count = 1;
    u16 id = 0;
    bool mounted = false;
    float health = 1;
    std::wstring name;
    int hp = -1; // an empty hardpoint's row: its place in Kept::free
};
// The loadout as the game's data has it: what the mounted guns do a second, what they draw (baseinfo::GunOf).
struct Loadout
{
    int guns = 0;
    float hull = 0, shield = 0, energy = 0;
};

// What outlives a frame and an open, as nnpage.cpp's statics always did (one of these, inventory_screen.cpp's, for every
// model). The page's reset on an open (inventory_screen.h Reset) touches only what the old one did.
struct Kept
{
    std::vector<Item> items;
    std::vector<equiprules::Hp> free; // the ship's empty hardpoints under this filter (the game's window lists only what the ship carries)
    screens::List list;               // over `items`; its rows the view's
    float itemRead = 0;               // until the rows are read again (s)
    Loadout load;
    std::unordered_map<u32, std::wstring> gunLine; // a gun's second line by its archetype
    bool itemSelected = false;        // the game's selection is the page's focus
    std::wstring filterWord[5];       // the window's filters in the game's words
    // A walk's selection is deferred (inventory_screen.cpp SyncDue): the step's and the last sync's ages, summed every frame
    float stepAge = 1e9f, syncAge = 1e9f;
    bool syncPending = false;
    u16 jettisonArmed = 0; // the single item a first JETTISON armed (a second press throws it)
    // what the view leaves for the shell and its next frame
    bool shipView = false;  // the ship's 3D view stands in space (the shell draws it after the UI pass)
    float shipRect[4] = {}; // its centre and half sizes (pixels)
    float cells[screens::kMeshes][4] = {}; // the rows' icon cells the last list frame cut (the next frame's ground leaves them open)
    int nCells = 0;
};

// The ship's facts, docked (gamedata::Ship): the column carries them where no view of the ship stands.
struct ShipFacts
{
    bool ok = false;
    float hull = 0, hold = 0, nanobots = 0, batteries = 0;
    int guns = 0, turrets = 0, gunClass = 0, shieldClass = 0;
};

struct Model
{
    Kept* k = nullptr; // inventory_screen.cpp's (Open)
    u32 root = 0;      // the window
    bool pad = false;  // the pad's form this frame
    // read right before the view, as the page read them while it drew (inventory_screen.cpp Prepare)
    bool visible = false;  // NN_Inventory stands (else the page draws nothing)
    bool jettison = false; // the game's jettison panel stands: the page is its panel
    int qty = 0, most = 0; // its count of how many
    int filter = 0;
    bool empty = false;    // the focus is an EMPTY hardpoint
    std::vector<std::wstring> fits; // ... what in the hold takes it
    bool canMount = false; // MOUNT / UNMOUNT may go (docked, a filter with hardpoints, an item or something for the EMPTY one)
    bool jetButton = false, trade = false; // JETTISON and BEST PRICES stand
    bool canTag = false, tagged = false;   // the compare tag on the focus
    u32 taggedArch = 0;                    // the item the store compares with (gamedata::TaggedCompare)
    std::vector<std::wstring> gunLine;     // each item's second line (a gun's numbers), "" none
    std::wstring ship;                     // the ship's name, upper case
    bool inSpace = false, shipViewOk = false;
    ShipFacts facts;
    bool compare = false;
    screens::CompareBlock block;           // the focus against what it would stand beside
    long long credits = 0;
    bool cargo = false;
    int used = 0, cap = 0;

    bool OnRow() const { return k && k->list.count > 0 && k->list.sel < static_cast<int>(k->items.size()); }
};
} // namespace inventoryscreen
