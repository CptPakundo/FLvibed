// FLvibed - the death page (the log after a death or a failed mission, single player: the game's NeuroNetStoryStar window with
// its three load buttons; issue 50; audit 70b F4a, moved from nnpage.cpp): its description (death_page.cpp) and its view
// (death_view.cpp). Its shell (which NeuroNet page stands, the journal's hole) is nnpage.cpp's; the pad's binds are padmenus'
// (Screen::Death: A, X, Y, its prompts in the lane).
#pragma once
#include "../page.h"
#include "../page_view.h"

namespace deathpage
{
extern const page::PageDesc kDeath;
constexpr const char* kId = "death";
void View(const page::ViewIn& in, const void* model, page::PageState& st, page::ViewOut& out);
} // namespace deathpage
