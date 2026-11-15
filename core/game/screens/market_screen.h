// FLvibed - the dealer page's reads and actions (NN_Dealer and its partner NN_Inventory in their three modes; the HUD 2 page
// framework, audit 70b F3b; moved from modules\hud\src\basepage.cpp unchanged). The game's two windows stand parked off the
// screen and do the work: each answers for its own rows (InvSelected / DealerSelectedGood), its selection drives the transfer
// panel (its price, quantity, refusal and button), its buttons trade, filter, mount and leave.
#pragma once
#include "market_model.h"
#include "action.h"

#include <string>

namespace marketscreen
{
// The page's reads (the runtime calls them: modules\hud\src\pages\pagerun.cpp). The windows' texts change under the page with
// no event it hears, so the stamp changes each frame. Read: the mode, the windows, the rows every 0.75 s out of the window
// manager's slide (at once after it, and soon after a trade, a mount or a filter), the game's lists moved over the page's
// shown rows. True when the rows were read.
uint64_t Stamp();
bool Read(Model& m, u32 root);
// What the view shows of the game, read right before it draws (the old page read it as it drew): the frame's numbers and
// words, each row's price and need, the focus's column, the card.
void ReadFrame(Model& m);
// After a filter's press: both lists from their top, the card too, the rows read again soon (the windows refill).
void Filtered(Model& m);

// Asked as the page's prompts and pad decide, at that moment (the old page asked the game the same way).
bool CanTrade(const Model& m);  // the focus is the game's selected row and the trade's button stands
bool Slider();                  // the quantity slider stands
bool CanBuyFinal();             // BUY SHIP may go (the game's ContinueButton stands)
bool RoutesStand();             // BEST PRICES stands (commodity)
bool BackStands();              // BACK TO SHIPS stands (final stage)
bool CanTag(const Model& m);    // the focus is a mounted item with numbers on your side of the equipment dealer (issue 44)
bool Tagged(const Model& m);    // ... and the one the store compares with
int NowFilter(const Model& m);  // the inventory's filter now
int Qty();                      // the slider's quantity, and the most it goes to
int QtyMost();

// The windows the page claims: NN_Dealer's class (exe::VtDealer); its partner is found by name.
extern const hk::Sym* const kWindowClass;
extern const hk::Sym* const kPartnerClass; // NN_Inventory's (exe::VtInventory)
constexpr const char* kWindowName = "NN_Dealer";
constexpr const char* kPartnerName = "NN_Inventory";
constexpr const char* kCloseButton = "Inventory_Close";

// The page's actions (their ids: market_model.h Action): the runtime's only way to act for it, each the windows' own button or
// handler (borrowed).
extern const screens::ActionDesc kActions[];
extern const int kActionCount;
} // namespace marketscreen
