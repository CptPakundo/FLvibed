// FLvibed - the play session's mode and the mode of every save (docs\UI_PLAN.md 6.6). The choice is recorded WITHOUT editing a game file: the sidecar
// `EXE\FLvibed\saves.ini` maps each save file's name to its mode, the speedrun clock and the checkpoint index at the save, written whenever the game writes a save
// during a session whose mode is known (a folder change notification, no polling), so the game's own Act_Save checkpoints, the autosave and the player's saves all
// carry the mode. A loaded save's mode becomes the session's (ASSISTED = the autopilot's competent preset + ap_hints; SPEEDRUN = `ap preset speedrun`, the timer,
// LiveSplit; NORMAL = nothing of the autopilot shows until used); a save without an entry is NORMAL. Core's, so speedrun and the autopilot have it with the
// controller off; the choice at New Game is the Controller's prompt, or the Speedrun module's on keyboard and mouse. It runs while `Init` ran, in core's frame.
#pragma once
#include "common.h"
#include "game/menu.h"

#include <string>
#include <vector>

namespace session
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
// Every frame, whatever the device: the save folder watch, the load screens' selection, the session's mode from the mode transitions. `modeName`: gamemode::Name.
void Update(float dt, const char* modeName);
Mode Current(); // UNKNOWN in the menu with nothing loaded
Mode OfSave(const std::string& file); // the mode recorded for a save file ("SaveC701.fl"; UNKNOWN = no entry)
// A save that is being loaded by name: the session takes its record when the game leaves the menu.
void NoteLoading(const std::string& file);
// The save the open load screen has selected (its file name), false when none.
bool SelectedSave(std::string& file);
bool MainMenuVisible();
u32 MainMenuButton(u32 id); // a shown main menu button by id, 0 = any shown one; 0 = none
u32 CharSelect();           // the front end's load screen, 0 = closed
const std::vector<menu::SaveEntry>& CachedSaves(); // the open load screen's list, refreshed at 2 Hz
// The speedrun clock's state the sidecar stores with each save.
void SetRunState(u32 runMs, int checkpointIndex);
// A loaded save's stored run state, once per load (true on the frame the session took it; then false).
bool TakeLoadedRunState(u32& runMs, int& checkpointIndex, Mode& mode);
// The game's own New Game, started as `pick`: the cut modules hear it, the session takes the mode when the game leaves the menu. `what`: the click's reply.
bool Begin(Mode pick, std::string* what);
// The choice at New Game as the player reads it: both prompts' words. What it offers depends on the modules: `Offered`.
struct Choice
{
    const wchar_t* name;
    const wchar_t* tag;
    const wchar_t* desc;
};
const Choice& ChoiceOf(int mode);
bool Offered(int mode);
const wchar_t* DescOf(int mode); // the autopilot's sentence goes with its module
void Force(Mode m); // tests: the session's mode now
void Rescan();
std::string Status();
std::string Records();
} // namespace session
