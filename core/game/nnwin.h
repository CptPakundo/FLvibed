// FLvibed - the NeuroNet's windows as a row of tabs: which one the game has open, and a press on one (core\game\nntabs.h over the game's
// window manager exe::NNManager). Core, because the HUD's strip and the pad's bumpers ask for the same thing and neither may call the other.
// Every press is the game's own command: its gates, its toggle and its switch.
#pragma once
#include "common.h"

namespace nnwin
{
// The manager's symbols resolved: a press can reach the game.
bool Ready();
// The command of the NeuroNet window the game has open, the one it is switching to while a switch runs, 0 for none or a symbol unresolved.
// A dealer shares the inventory's command: the caller tells them apart by the window.
int OpenCommand();
// CHAT and GROUP exist: the box has their buttons (multiplayer).
bool Multiplayer();
// The tab of the open window, -1 for none.
int OpenTab();
// A press on a tab's command; the neighbour tab, wrapping; the open window's close. Held through a slide, the last press wins. False: no window open.
bool Go(int cmd);
bool Step(int dir);
bool Close();
// The tab's command with no window open: what a click on the box's button sends (HUD 2's words in a room).
void Press(int cmd);
// Once a frame while a window may be open: sends a held press when the slide is over.
void Frame(float dt);
// The command a held press will send, 0 none: the strip shows where the player is going.
int Held();

// The seconds of each slide: 0 leaves the game's own. The write is remembered and restored while it still holds FLvibed's value.
// False when the symbol is not resolved or the page refused it.
bool SetSlideSeconds(float seconds);
float SlideSeconds();
} // namespace nnwin
