// FLvibed's own on-screen keyboard (docs\UI_SPEC.json `save` state `keyboard`, `chat`; UI_PLAN.md 6.4 "text entry"):
// QWERTY rows, digits and punctuation drawn by FLvibed over the game's screen; the D-pad / left stick moves the focus,
// A types the focused key, X space, B backspace, LB / RB shift (upper case and the digit row's symbols), Menu = done.
// Characters reach the game's focused edit control through the game's own text path: fl::SendChar = the game's
// keyboard message handler (exe::WinKey) with WM_CHAR, exactly what a physical key produces, so the edit's own
// handler (vtable slot 0xC8: case mode, allowed characters, maximum length) accepts or refuses them; Backspace and
// Enter go as key messages (WM_KEYDOWN / UP through the same handler: the edit's key handler moves the caret, erases,
// notifies its owner with 0xF003 on Enter). The Windows touch keyboard is never involved (nothing here calls it).
#pragma once
#include "common.h"
#include "uikit.h"

#include <istream>
#include <string>

namespace osk
{
enum class Purpose
{
    Save, // the save screen's description: DONE closes the keyboard (the description stays in the edit)
    Chat, // the chat field: DONE = Enter (sends the line, the game closes the field)
    Other // any other focused edit: DONE = Enter
};
void Open(Purpose p);
void Close();
bool IsOpen();
Purpose Current();
// Per frame while open, with the frame's pad input (padmenus polls once for every pad screen).
void Input(const uikit::UiInput& in, const uikit::Input::PadExtra& x);
// Inside the overlay pass (ctl::OnRenderOverlay).
void Draw();
// Types a string through the same path the keys use (debug `osk type`, the save description's pre-fill).
int Type(const std::wstring& s);
// Debug: osk open|close|type <s>|state
std::string Command(std::istream& in);
std::string Status();
} // namespace osk
