// How the skin's chips and tabs look (modules\hud\docs\UI_HUD2_SKIN.md 4 "Chip", "Tab", "Focus"): drawing only, through
// core\ui\hud2_kit. Their clicks are skinchip.h's; this half is apart so the pages' draw kit (pages\page_kit.h) can draw a
// chip without reaching the click box. Drawn by skinchip.cpp.
#pragma once
#include "common.h"
#include "navmap_layout.h"

#include <string>

namespace skinchip
{
enum class Look
{
    Plain,    // a chip with a word
    Current,  // the layer or view in force: the yellow word, the underline
    Disabled, // the game disables it: dim, no press
    Focus,    // the pad's focus: the mustard block, the bar outside its left edge, the word in INK
    Hover,    // the mouse is on it
};

// One chip: glass, a hairline, its word centred. The word fits by the kit's rule; it is never shortened by hand.
void Chip(const navlayout::Rect& r, const std::wstring& word, float size, Look look);
// One tab of the strip: a word with no box, the open one yellow and underlined. `hover`: the mouse is on it.
void Tab(const navlayout::Rect& r, const std::wstring& word, float size, bool open, bool hover);
// A section label inside a panel: small caps in CHROME.
void Section(float x, float y, const std::wstring& word);
} // namespace skinchip
