// HUD 2's entry in the HUD module: its places in core's frame and passes (core\ui\views.h HudView and SkinView, published while HUD 2 is selected
// and called through core's door, core\ui\hud_door.h), its one give-back list and its verbs. The module's table (hud_module.cpp) calls it beside the
// extras' entry (extras\extras.h), which needs nothing of this.
#pragma once
#include "common.h"

namespace hud2entry
{
// Registers its Enter and Leave with the door: Enter publishes views::hud and views::skin, the loading screen's draw and what the idle cursor's hide
// asks of HUD 2; Leave withdraws them and gives everything back.
void Attach();
void Detach();
void Shutdown();
void Fault(int phase);
// The verbs hud2, uiskin, uistrip, nnstrip, nnpage, modelprobe, basepage, navskin, navnew; false: not one of them.
bool Command(const std::string& op, std::istream& in, std::string& reply);
// A raw key for the skin's dialogs (skindialog.h EnterKey); true: the game never sees it.
bool Key(int vk, bool down);
// `gametext window | frame 0|1 | texts | bar | fit`: HUD 2's window text and the plates' reports; false: not one of these words.
bool GameTextVerb(const std::string& what, int pct, std::string& reply);
} // namespace hud2entry
