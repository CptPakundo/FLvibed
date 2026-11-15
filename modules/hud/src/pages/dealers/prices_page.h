// FLvibed - the BEST PRICES page (the game's commodity list by base, NN_CommodityByBase, as a HUD 2 page; audit 70b F3a,
// moved from basepage.cpp): its description (prices_page.cpp) and its view (prices_view.cpp). Its shell (which docked window
// stands, the window's close button off the screen, the close when HUD 2 leaves, the D-pad's repeat, the verb `basepage`)
// is still basepage.cpp's until the docked pages have all moved (F3e).
#pragma once
#include "../page.h"
#include "../page_view.h"

namespace pricespage
{
extern const page::PageDesc kPrices;
constexpr const char* kId = "prices";
void View(const page::ViewIn& in, const void* model, page::PageState& st, page::ViewOut& out);
} // namespace pricespage
