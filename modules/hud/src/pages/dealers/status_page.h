// FLvibed - the STATUS page (the game's NN_PlayerInfo window as a HUD 2 page; audit 70b F4b, moved from nnpage.cpp): its
// description (status_page.cpp: the claim, the model's reads, its own prompts, pad, wheel and clicks) and its view
// (status_view.cpp). Its shell (which NeuroNet page stands, the card's hole, the world pause, the verb `nnpage`) is still
// nnpage.cpp's until the NeuroNet's pages have all moved.
#pragma once
#include "../page.h"
#include "../page_view.h"

namespace statuspage
{
extern const page::PageDesc kStatus;
constexpr const char* kId = "status";
void View(const page::ViewIn& in, const void* model, page::PageState& st, page::ViewOut& out);
} // namespace statuspage
