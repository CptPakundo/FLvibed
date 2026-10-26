// The New Game prompt and the mode of every save (docs\UI_PLAN.md 6.6, UI_SPEC.json mainmenu state `newgame`): in
// pad mode A on NEW GAME opens FLvibed's list NORMAL GAME / ASSISTED / SPEEDRUN MODE (one description line for the
// highlighted entry); A starts the game's own New Game natively (menu::ClickMainButton), B backs out. The choice is
// recorded WITHOUT editing a game file: the sidecar `EXE\FLvibed\saves.ini` maps each save file's name to its mode (+
// the speedrun clock and checkpoint index at the save) - written whenever the game writes a save file during a play
// session whose mode is known (a folder change notification, no polling), so the game's own Act_Save checkpoints, the
// autosave and the player's saves all carry the mode. A loaded save's mode becomes the session's (ASSISTED = the
// autopilot's competent preset + ap_hints; SPEEDRUN = `ap preset speedrun` + the timer + LiveSplit; NORMAL = nothing
// of the autopilot shows until used); a save without an entry is NORMAL. With keyboard and mouse the game's own New
// Game runs (NORMAL). The load screens show a tag ASSISTED / SPEEDRUN over the save's row (the rows from ui::ListRows,
// the first visible row from the list's scroll bar).
#pragma once
#include "common.h"
#include "uikit.h"

#include <istream>
#include <string>

namespace newgame
{
enum Mode : int
{
    UNKNOWN = -1, // no session (the main menu with nothing loaded)
    NORMAL = 0,
    ASSISTED = 1,
    SPEEDRUN = 2,
};
const char* ModeName(Mode m);

void Init();
void Shutdown();
// Every frame, whatever the device (ctl::OnFrame, early): the save folder watch, the load screens' selection, the
// session's mode from the mode transitions. `modeName` = ctl's ModeName(g_mode) ("menu", "docked", "space", ...).
void Update(float dt, const char* modeName);
// The current play session's mode (UNKNOWN in the menu with nothing loaded).
Mode Session();
// The mode recorded for a save file ("SaveC701.fl"; UNKNOWN = no entry).
Mode OfSave(const std::string& file);
// A save that is being loaded by name (menu::Load callers: the pad's A on a row, the harness's loadsave) - the session
// takes its record when the game leaves the menu.
void NoteLoading(const std::string& file);
// The save the open load screen has selected (its file name), false when none; the main menu's visibility and its
// buttons by id (padmenus shares them).
bool SelectedSave(std::string& file);
bool MainMenuVisible();
u32 MainMenuButton(u32 id);
// The speedrun clock's state the sidecar stores with each save (livesplit.cpp keeps the clock and tells it).
void SetRunState(u32 runMs, int checkpointIndex);
// A loaded save's stored run state, once per load (true on the frame the session took it; then false).
bool TakeLoadedRunState(u32& runMs, int& checkpointIndex, Mode& mode);

// The prompt (pad mode, the main menu).
void OpenPrompt();
void ClosePrompt();
bool PromptOpen();
void PromptInput(const uikit::UiInput& in, const uikit::Input::PadExtra& x);
// Inside the overlay pass: the prompt, and the tags on an open load screen's rows.
void Draw();
// Debug: newgame open|pick <n>|mode <n>|state
std::string Command(std::istream& in);
std::string Status();
} // namespace newgame
