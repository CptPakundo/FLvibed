// The game's menu screens on the pad (docs\UI_SPEC.json screens mainmenu, load, pause_space, pause_docked, save,
// death, chat; UI_PLAN.md 6.4 "menus"): while the pad is the device and one of those screens is up, this adds what the
// generic D-pad focus (controller.cpp UpdateUiNav / UpdateFreeCursor: focus by the cursor, A = the game's click) does
// not give - the prompts, the first focus the spec names, single-A loads, the one-second delete hold, B as the screen's
// own back / resume / close button (native clicks), the New Game prompt, the save description's pre-fill, the on-screen
// keyboard for the description and the chat. Everything ends in the game's own logic (its buttons, its load-by-index,
// its text path). The pad input is polled once per frame here (uikit::Input) and handed to the OSK and the prompt.
#pragma once
#include "common.h"

#include <istream>
#include <string>

namespace padmenus
{
void Init();
void Shutdown();
// Every frame while the pad is the device and the controller is on, before controller.cpp's own menu / space pad
// handling (any mode: the chat's field is open in `space`). `modeName` = ctl's ModeName(g_mode).
void Update(float dt, const char* modeName);
// True when this owns the pad this frame: the New Game prompt or the OSK is open, or a button was consumed here (the
// caller skips its UI navigation, the free cursor's click and the Menu = pause tap for the frame).
bool OwnsInput();
// A frame another layer drives the pad (FLUF's windows): the pad's state is read, nothing acts.
void Idle(float dt);
// Inside the overlay pass: the prompt, the OSK, the tags, the prompt bar (in this order, over the game's screen).
void Draw();
// Debug: menus state
std::string Command(std::istream& in);
std::string Status();
} // namespace padmenus
