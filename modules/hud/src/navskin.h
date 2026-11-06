// The nav map's frame and words (modules\hud\docs\UI_HUD2_SKIN.md 6.1, the first step): the frame off (+0x84 = -1, bit 1 of the two side windows), the game's icons
// hidden and replaced by words, each a chip calling the native button by its own action (uiclick::Click), around the native map at its own size. The game keeps
// its art, lanes, zones, pan, zoom and selection. Fails closed: a read that does not hold, or `skin_native = navmap`, gives back the old map for the session.
//
// The chips reach both devices: the mouse through skinchip's click box, the pad's panel zone through the view (core\ui\views.h MapChip).
#pragma once
#include "common.h"
#include "ui/views.h"
#include "ui/winplates.h"

#include <istream>
#include <string>

namespace navskin
{
// The controls to hide on this map: the frame's two side windows and every icon a chip replaces. False: leave this window native.
struct Hides
{
    u32 art[48];
    int nArt = 0;
};
bool Bind(u32 root, Hides& out);
// The map's window went away.
void Unbind();
// Set when a read stopped holding: uiskin restores the window once, and the map stays the game's for the session.
bool TakeFailure();

// The under pass's start, in any mode: no chip stands until Draw says so.
void BeginPass();
// In the skin's draw pass: the panels and chips, their keep-outs and hotspots. Nothing while no map is bound.
void Draw(const winplates::Win* map, bool pad);

// The view
int Chips(views::MapChip* out, int max);
void ChipPress(int id);
void ChipStep(int id, int dir);
void ChipFocus(int id);

// `skin_native` (hidden, a comma list of family labels). Another package adds the row: this is the one place it is read.
bool NativeAsked();

// Debug: navskin [state] | native on|off | press <id>
std::string Command(std::istream& in);
} // namespace navskin
