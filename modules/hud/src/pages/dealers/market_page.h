// FLvibed - the dealer page (the game's NN_Dealer with NN_Inventory as a HUD 2 page in its three modes: the commodity trader,
// the equipment dealer with mount, the ship dealer's final stage; audit 70b F3b, moved from basepage.cpp): its description
// (market_page.cpp: the claim, the park of both windows, the model's reads, its own prompts, pad and clicks, the actions) and
// its view (market_view.cpp). Its shell (which docked window stands, the close when HUD 2 leaves, the D-pad's repeat, the verb
// `basepage`) is still basepage.cpp's until the docked pages have all moved (F3e).
#pragma once
#include "../page.h"
#include "../page_view.h"

namespace marketpage
{
extern const page::PageDesc kMarket;
constexpr const char* kId = "market";
void View(const page::ViewIn& in, const void* model, page::PageState& st, page::ViewOut& out);
} // namespace marketpage
