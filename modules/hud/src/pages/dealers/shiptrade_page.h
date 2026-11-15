// FLvibed - the ship dealer page (the game's NN_ShipTrader window as a HUD 2 page; audit 70b F3c, moved from basepage.cpp): its
// description (shiptrade_page.cpp: the claim, the park, the model's reads, its own prompts, pad and wheel, the actions) and its
// view (shiptrade_view.cpp). Its shell (which docked window stands, the 3D view brought back after the park, the close when
// HUD 2 leaves, the D-pad's repeat, the verb `basepage`) is still basepage.cpp's until the docked pages have all moved (F3e).
// The final stage the BUY button opens is the dealer's page (market_page.h).
#pragma once
#include "../page.h"
#include "../page_view.h"

namespace shiptradepage
{
extern const page::PageDesc kShipTrade;
constexpr const char* kId = "shiptrade";
void View(const page::ViewIn& in, const void* model, page::PageState& st, page::ViewOut& out);
} // namespace shiptradepage
