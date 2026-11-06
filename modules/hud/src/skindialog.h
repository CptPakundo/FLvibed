// HUD 2's dialogs (modules\hud\docs\UI_HUD2_SKIN.md 6.2, R10): NN_Confirm, NN_Exit, NN_Error and the "please wait" box in one centred plate. The Shadow tier: the
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
// The family is a dialog of this file.
bool IsDialog(int fam);
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
} // namespace skindialog
