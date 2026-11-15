// FLvibed - the INVENTORY page (the game's NN_Inventory window with its jettison panel as a HUD 2 page; audit 70b F4 unit b2,
// moved from nnpage.cpp): its description (inventory_page.cpp: the claim, the model's frame and reads, its own prompts, pad
// and clicks) and its view (inventory_view.cpp). Its shell (which NeuroNet page stands, the card's hole, the game's controls
// placed off the page, the world pause and the ship view's blink, the D-pad's repeat, the verb `nnpage`) is still nnpage.cpp's
// until the NeuroNet's pages have all moved.
#pragma once
#include "../page.h"
#include "../page_view.h"

namespace inventorypage
{
extern const page::PageDesc kInventory;
constexpr const char* kId = "inventory";
void View(const page::ViewIn& in, const void* model, page::PageState& st, page::ViewOut& out);
} // namespace inventorypage
