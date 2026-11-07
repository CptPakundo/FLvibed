// The NeuroNet's pages: LOG, INVENTORY, STATUS and INFO as full screens (docs\NEURONET_PLAN.md), the map's standard: the screen is the window, the tabs on top, the prompts at the bottom,
// a card only when there is something to say. HUD 2 draws over the game's window, which stays open and does the work: its text stands in a hole of the page, its calls do what the page's
// buttons say. Everything the page needs of the game falls back to the game's own window: off, or a symbol unresolved, nothing here answers.
#pragma once
#include "common.h"
#include "ui/views.h"

#include <istream>
#include <string>

namespace nnpage
{
// Once a frame: which page stands, the flight lock, its reads. `pad`: the pad's prompts are in the lane; else the page says what the mouse does.
void Update(float dt, bool pad);
// Over the game's UI, under the strip.
void Draw();
// After the pass's last draw: the 3D view a page shows.
void DrawAfter();
// Before the game's UI pass (hud_module UnderBegin): hides what the game shows again every frame (the TRADE ROUTES button under the inventory's card).
void Under();
// A page stands this frame: the window's plates stand down, the pad asks the page.
bool On();
// What the lane shows; the presses (XINPUT masks); returns the buttons taken.
int Prompts(views::PagePrompt* out, int max);
unsigned Pad(unsigned pressed, unsigned held, float dt);
void Shutdown();
// Debug: nnpage [state] | press <a|b|x|y|up|down|left|right|lt|rt>
std::string Command(std::istream& in);
} // namespace nnpage
