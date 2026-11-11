// The docked windows as pages, the NeuroNet pages' standard (nnpage.*, docs\BASE_ROOMS_PLAN.md): the commodity and equipment dealers, the ship dealer, the job board and the news. HUD 2 draws
// over the game's open window, which does the work; off, or a read that fails, and the game's window (with Extended HUD 2's plates) stands as before.
#pragma once
#include "common.h"
#include "ui/views.h"

#include <istream>
#include <string>

namespace basepage
{
void Update(float dt, bool pad);
void Draw();
void DrawAfter();
// Under the game's UI: what stands behind the game's own icons in the page.
void DrawUnder();
// After the game's update, before its UI pass: the lists' rows get their item models (nnpage_kit.h RowIcons).
void IconsUnder();
bool On();
// The game's window stands off the screen under a page: its plates and its text have nothing to do.
bool Hides();
int Prompts(views::PagePrompt* out, int max);
unsigned Pad(unsigned pressed, unsigned held, float dt);
void Shutdown();
std::string Command(std::istream& in);
} // namespace basepage
