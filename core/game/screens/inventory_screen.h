// FLvibed - the INVENTORY page's reads and actions (the game's NN_Inventory window and its jettison panel; the HUD 2 page
// framework, audit 70b F4 unit b2; moved from modules\hud\src\nnpage.cpp unchanged). The window stays on the screen, its
// card through the page's hole and its meshes in the rows; the page reads its rows one by one through the window's own
// answer, keeps the game's selection on its focus, and works the window's own handlers and buttons (the table kActions).
// What the old page placed of the game's controls as it drew (the jettison panel's controls off the screen, the number
// field, BEST PRICES' button and label) stays its shell's, nnpage.cpp, which holds the controls' placing.
#pragma once
#include "action.h"
#include "inventory_model.h"

namespace hk
{
struct Sym; // core\hook.h: a signed game symbol
}

namespace inventoryscreen
{
// A new model (the runtime's open): it points at the one Kept.
void Open(Model& m);
// The one Kept (the shell's verb `nnpage` and its ship view read it on every page).
const Kept& State();
// The reset of a page change (the shell's, as the old one: on every change of page or window, the INVENTORY or another).
void Reset();
// Every frame of the shell, whichever page stands: the walk's deferral clocks (SyncDue) go on.
void Clock(float dt);

// The frame's reads in the old order (the runtime's frame hook: modules\hud\src\pages\dealers\inventory_page.cpp): the game's
// selection catches up with a walk when due, the game's list follows the page's shown rows (SyncList), the rows read every
// 0.5 s (at once on the open; 0.12 s after a mount) with the filters' words, the game's selection put back on the focus.
void Update(Model& m, u32 root, float dt);
// What the page read as it drew, right before its view (after the shell's baseinfo::Refresh and its placing).
void Prepare(Model& m, u32 root);

// Live answers for the prompts and the shell (read when asked, as the old page did).
int FilterOf(u32 root);
bool JettisonOpen();          // the game's jettison panel stands
bool CanMount(u32 root);      // MOUNT / UNMOUNT would do something now
bool JettisonShown();         // the game's JETTISON stands for the focus
bool TradeOffered(u32 root);  // BEST PRICES stands (a commodity's row)
bool CanTag();                // the focus is a mounted item with numbers to compare
bool Tagged();                // ... and it is the tagged one
void ToggleTag();             // the compare tag on the focus, or off again (gamedata::TagCompare: no game call)
u16 ShipViewPart();           // the ship view's selected part: the focus's id when it is mounted, 0 none

// The page's actions, each the window's own handler or button (pagerun's runner runs them, Borrowed).
extern const screens::ActionDesc kActions[];
extern const int kActionCount;

// The window the page stands over (its card's box is the page's hole: the shell cuts it).
extern const hk::Sym* const kWindowClass;
constexpr const char* kWindowName = "NN_Inventory";
constexpr const char* kCardName = "InfoCard_Info";
} // namespace inventoryscreen
