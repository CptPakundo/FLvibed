// FLvibed - the INFO page (the NeuroNet's CURRENT INFORMATION; the HUD 2 page framework's first page, audit 70b F2a): its
// description (info_page.cpp: the claim, the park, the model's reads, the bindings, the actions) and its view
// (info_view.cpp). The NeuroNet's shell round it (the strip's choice of page, the ground, the title, the left column and
// the ships, the mouse's chips, the world pause) is still nnpage.cpp's: it shares LOG's helpers, and moves with LOG (F4a).
#pragma once
#include "../page.h"
#include "../page_view.h"
#include "game/screens/nnhelp_model.h"

namespace infopage
{
extern const page::PageDesc kInfo;
constexpr const char* kId = "info";
// The view (info_view.cpp), and the text as it laid it out for PageState::wrapW (the `nnpage info` verb's [LINES]).
void View(const page::ViewIn& in, const void* model, page::PageState& st, page::ViewOut& out);
const std::vector<inforules::Line>& Lines();
} // namespace infopage
