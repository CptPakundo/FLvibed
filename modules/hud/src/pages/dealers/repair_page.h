// FLvibed - the repair page (the game's NN_Repair window as a HUD 2 page; audit 70b F3a, moved from basepage.cpp): its
// description (repair_page.cpp: the claim, the park, the model's reads, its own prompts and pad, the actions) and its view
// (repair_view.cpp). Its shell (which docked window stands, the close when HUD 2 leaves, the D-pad's repeat, the verb
// `basepage`) is still basepage.cpp's until the docked pages have all moved (F3e).
#pragma once
#include "../page.h"
#include "../page_view.h"

namespace repairpage
{
extern const page::PageDesc kRepair;
constexpr const char* kId = "repair";
void View(const page::ViewIn& in, const void* model, page::PageState& st, page::ViewOut& out);
} // namespace repairpage
