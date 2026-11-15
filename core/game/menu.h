// Main menu and load screen, driven natively (the game's own button and load logic; no cursor, no key presses).
// Facts: docs/FREELANCER_RE.md "Main menu and load screen". One instance for the whole logic (core/game).
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
// The open NN_LoadSave in either mode (load or save), or 0.
u32 LoadSaveScreen();

// One column of a save list (the game's ListBox, FREELANCER_RE "The save lists' rows"): its rows as the game holds them and
// what it is drawn with: the box's height in pixels (+0x89C projected at its depth, as ListBox::Draw does) and the font's
// line height (the row pitch of the draw and the hover).
struct SaveColumn
{
    u32 list = 0;
    int count = 0, rows = 0, first = 0; // +0x8A4 rows held, +0x8A8 rows drawn, +0x88C the first drawn
    float height = 0, boxPx = 0;        // +0x89C (UI units), projected
    int linePx = 0;
};
// The open load / save screen's two columns (names, dates), the scroll bar they share (0 when not found) and the rows the
// game gives them (20 on the front list, NN_LoadSave +0x1754 in game: 20 load, 16 save). False when none is open.
struct SaveLists
{
    bool front = false;
    u32 screen = 0, scroll = 0;
    int gameRows = 0;
    // In game, the bar's height in pixels (UIScroll +0x338 projected at its depth; 0 when not read, and on the front list): the
    // panel's room for rows. The in-game ListBox's own box (0.0325 x rows) is taller than its panel (the bar, 0.465 load / 0.37
    // save, ends where the panel's rows do); the front list's box is its panel.
    float scrollPx = 0;
    SaveColumn names, dates;
};
bool OpenSaveLists(SaveLists& out);
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
