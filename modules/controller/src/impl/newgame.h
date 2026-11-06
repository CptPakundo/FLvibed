// The New Game prompt on the pad (docs\UI_PLAN.md 6.6, UI_SPEC.json mainmenu state `newgame`): in pad mode A on NEW GAME opens FLvibed's list NORMAL GAME / ASSISTED /
// SPEEDRUN MODE (one description line for the highlighted entry); A starts the game's own New Game natively (session::Begin), B backs out. The load screens show a
// tag ASSISTED / SPEEDRUN over the save's row (the rows from ui::ListRows, the first visible row from the list's scroll bar). The modes themselves: core\game\session.h.
#pragma once
#include "common.h"
#include "ui/uikit.h"

#include <istream>
#include <string>

namespace newgame
{
void OpenPrompt();
void ClosePrompt();
bool PromptOpen();
void PromptInput(const uikit::UiInput& in, const uikit::Input::PadExtra& x);
// Inside the overlay pass: the prompt, and the tags on an open load screen's rows.
void Draw();
std::string Status();
// Debug: newgame open|close|pick <n>|mode <n>|records|rescan|state
std::string Command(std::istream& in);
} // namespace newgame
