// FLvibed - the LOG page (the game's NeuroNetStoryStar window as a HUD 2 page; audit 70b F4a, moved from nnpage.cpp): its
// description (mlog_page.cpp: the claim, the model's reads, its own prompts and pad, the actions) and its view
// (mlog_view.cpp). Its shell (which NeuroNet page stands, the journal's hole, the world pause, the verb `nnpage`) is still
// nnpage.cpp's until the NeuroNet's pages have all moved (F4b).
#pragma once
#include "../page.h"
#include "../page_view.h"

namespace mlogpage
{
extern const page::PageDesc kLog;
constexpr const char* kId = "log";
void View(const page::ViewIn& in, const void* model, page::PageState& st, page::ViewOut& out);
} // namespace mlogpage
