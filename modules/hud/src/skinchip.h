// The skin's words as chips, and their clicks: the strip's tabs and the nav map's chips (modules\hud\docs\UI_HUD2_SKIN.md 4 "Chip", "Tab", "Focus"). A small
// helper that can move into the primitive set (skinkit) once that lands: it draws through core\ui\hud2_kit and owns nothing else.
//
// The clicks go through the box HUD 2's keyboard form uses (HostApi v22 SetHudHotspots, core\host\uibtn.cpp): the game's own hit test, hover and click sound.
// That box has one owner at a time; HUD 2 stands its set down while a window is open. A click is queued from the game's mouse dispatch and run from the
// frame, as HUD 2's are. Pad and mouse stay live together, so the set stands on both devices.
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

using Action = void (*)(int arg);
// Starts this frame's set: called once before anything records.
void HotBegin();
// A clickable rectangle for this frame, pixels. Returns its slot, -1 when the box is full (logged once).
int Hot(const navlayout::Rect& r, Action act, int arg);
// The chip under the game's cursor this frame, -1 none.
int Hovered();
// From the frame: runs the queued clicks, hands this frame's set to the host. `allowed` false stands the set down.
void Frame(bool allowed);
// A click from the host, on the game's mouse dispatch: queued. False: the set is not standing, the click is HUD 2's.
bool OnClick(int slot);
// The set is down and the box given back.
void GiveBack();
std::string Status();
} // namespace skinchip
