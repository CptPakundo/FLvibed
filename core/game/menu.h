// Main menu and load screen, driven natively (the game's own button and load logic; no cursor, no key presses).
// Facts: docs/FREELANCER_RE.md "Main menu and load screen". Shared by every module (core/game), compiled into each.
#pragma once
#include "common.h"

#include <string>
#include <vector>

namespace menu
{
// Main menu buttons (front-end buttons, mouse handler 0x5664A0) by their id (+0xEC).
enum MainButton : u32
{
    kNewGame = 0x10,
    kLoadGame = 0x03,
    kMultiplayer = 0x04,
    kOptions = 0x0A,
    kExit = 0x0B,
};
// Clicks a visible, enabled main menu button natively: its parent's vt[0x5C](0x3004, id, 0), what the button's
// mouse handler does on release (the menu plays the click sound itself).
bool ClickMainButton(u32 id, std::string* what);

// The open load screen (NN_LoadSave), or 0: the owner window of its LoadSave_LoadGame button.
u32 LoadScreen();
struct SaveEntry
{
    int index;
    std::string id; // CHARACTER_ID: the save's file name as the game knows it
};
// The load screen's save list (the game's startup scan of the save folder, NN_LoadSave +0x344).
std::vector<SaveEntry> Saves();
// Loads the save whose file name matches (case-insensitive, ".fl" optional) through the load screen's own
// load-by-index routine 0x4880F0 (what its Load button and a double-click do).
bool Load(const std::string& file, std::string* what);

// The open pause menu (NN_Pause), or 0.
u32 PauseMenu();

// The save the game is about to load (its CHARACTER_ID = file name, e.g. "SaveC702"): the game's own pending-load id
// (exe::PendingLoadId, written by the load screens' load and the death dialog's LOAD AUTOSAVE, cleared by the game
// afterwards). Empty when none is pending or the symbol is unresolved.
std::string PendingLoad();

// Test aid: destroys a space object (ship id) through the game server's own API, pub::SpaceObj::Destroy (server.dll,
// DestroyType Fuse: it explodes, as if killed). For reaching death / mission failure states in tests.
std::string DestroyObject(u32 id);

// Debug commands: menu <new|load|multiplayer|options|exit>, saves, loadsave <file>. False if not one of them.
bool Command(const std::string& cmd, const std::string& rest, std::string* reply);
} // namespace menu
