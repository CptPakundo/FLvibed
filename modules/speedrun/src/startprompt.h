// The New Game choice on keyboard and mouse: with the Controller module off a click on NEW GAME opens this list instead of the game's own start, so speedrun mode can
// be chosen with no pad (with the Controller on, its prompt is the one, and only the pad reaches it). The click is taken by the hit-test suppression core already has,
// so the game never sees it; the choice then clicks NEW GAME the game's way (session::Begin).
#pragma once
#include "common.h"

#include <istream>
#include <string>

namespace startprompt
{
void Update(); // the Speedrun module's frame
void Draw();   // its draw: the list, and the mark on NEW GAME under the pointer (the game's own hover needs the hit test this takes away)
void Shutdown();
// While the list is open every key is its: up / down, Enter, Escape closes it. Under it the game's menu would take them (Escape: the Exit dialog).
bool OnKey(int vk, bool down);
// Debug: startprompt open|close|pick <mode>|state. `pick` works as a click on that row.
std::string Command(std::istream& in);
} // namespace startprompt
