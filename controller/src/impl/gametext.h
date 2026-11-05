// The game's own text, larger: `game_text_size` over both of the game's font tables (core\game\fontscale.h).
#pragma once
#include "common.h"

#include <string>
#include <vector>

namespace gametext
{
// Every frame. `menu`: the main menu, where it is applied: no rich text window is open there, so nothing laid out with the old
// fonts is left on screen (a change made in a game takes effect at the next main menu).
void Frame(int percent, bool menu);
// Research (debug `gametext rich <percent>`): the rich table alone, at once. The caller re-lays out what is open.
void RichNow(int percent);
// The rich table at `percent` from the file read once: a window's size while it is open (windowtext). Nothing in it opens a file.
void RichSet(int percent);
// fonts.ini as the game holds it: every font the game's list has, with its face from the file. `entry` is what a text object keeps.
struct Font
{
    std::string nickname, face;
    float height = 0;
    u32 entry = 0;
};
std::vector<Font> Fonts();
std::string Status();
} // namespace gametext
