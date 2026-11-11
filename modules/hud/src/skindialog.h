// HUD 2's dialogs (modules\hud\docs\UI_HUD2_SKIN.md 6.2, R10): NN_Confirm, NN_Exit, NN_Error, the "please wait" box and the cargo demand (the mission message
// box's drop-cargo form) in one centred plate. The Shadow tier: the
// dialog's art and words go, the buttons stay built and hit-testable, and the skin draws the plate and chips on their boxes, over the UI pass, since anything
// under the UI pass lies under the buttons of the screen behind. Every word on it is read at the open: a text the row expects gives the verbs of 6.2, any other
// the game's own words in the same plate. NN_Prompt's text is rich text with no known reader, so it stays native until a probe finds one (skindialog.cpp).
// A dialog whose words or boxes cannot be read stays the game's, whole.
#pragma once
#include "common.h"
#include "ui/winplates.h"

#include <vector>

namespace skindialog
{
// The window is a dialog of this file: by its family, and for the mission message box by its form (only the drop-cargo one, told by its buttons).
bool IsDialog(int fam, u32 root);
// A dialog of this family holds the game (the scrim, the flight HUD under it); the cargo demand does not: the world runs on and the box closes by itself.
bool Pauses(int fam);
// At a window's write: reads its words and buttons. False: it stays native. `hide` lists the word controls to hide with its art.
bool Prepare(const winplates::Win& w, bool mainMenuBehind, std::vector<u32>& hide);
// The window carries the skin (Prepare said so and nothing failed since).
bool Skinned(u32 root);
// Forgets a dialog that fell to native, and all no longer open.
void Forget(u32 root);
void Prune();
// Over the UI pass, between hud2kit::Begin and End. `hovered`: the control under the focus. False: this frame's boxes do not fit: it falls to native.
bool Draw(const winplates::Win& w, u32 hovered);
// The skin went off: no dialog is ours any more, Enter is the game's again.
void Clear();
// A raw key: Enter takes the focused chip's answer. True: the game never sees it.
bool EnterKey(int vk, bool down);
// The open dialog's words for the prompt lane; null when none is skinned or the row has no verbs.
const wchar_t* ActionWord();
const wchar_t* SafeWord();
// The focus starts on the action (nothing is at risk).
bool FocusAction();
// The cargo demand standing over the flight this frame: `rect` its plate as the skin places it (x0 y0 x1 y1, pixels; the game's own box while it is not
// skinned) and its two answers as the game words them (DROP CARGO, CANCEL). The flight HUD keeps off the plate and shows the pad's answers in its prompt lanes.
// False: none stands, or its box or words cannot be read.
bool FlightBox(float rect[4], std::wstring& accept, std::wstring& cancel);
} // namespace skindialog
