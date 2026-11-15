// FLvibed - the job board's and the news's pages (the game's NN_MissionVendor and NN_News windows as HUD 2 pages; audit 70b F3d /
// F3e, moved from basepage.cpp, where they were one body of code in two modes): their descriptions (board_page.cpp: the claims,
// the window left where it stands, the model's reads, their own prompts, pad and wheel, the actions) and their one view
// (board_view.cpp). Their shell (which docked window stands, the game's text and controls placed round the panel before the
// page's reads, the ticker's run, the close when HUD 2 leaves, the D-pad's repeat, the verb `basepage`) is still basepage.cpp's.
#pragma once
#include "../page.h"
#include "../page_view.h"

namespace boardpage
{
extern const page::PageDesc kJobs;
extern const page::PageDesc kNews;
constexpr const char* kJobsId = "jobs";
constexpr const char* kNewsId = "news";
void View(const page::ViewIn& in, const void* model, page::PageState& st, page::ViewOut& out);
} // namespace boardpage
