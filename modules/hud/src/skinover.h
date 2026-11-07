// HUD 2 over everything (modules\hud\docs\UI_HUD2_SKIN.md 6.6): its cursor, the set of 19 names of DATA\mouse.ini as decisions\D1b_cursor.jpg draws them, and the
// ToolTip in the Shadow tier. Position, clicks and hover stay the game's: the shape comes from the game's current cursor name, at the game's own position,
// with the game's mesh hidden; a name the table does not know (a mod's) stays the game's and is logged. Label `cursor` in skin_native keeps the game's.
#pragma once
#include "common.h"

#include <string>

namespace skinover
{
// The ToolTip's text and place while the game shows it, read before the UI pass; false: it stays native. The window's own text field is decompiled only.
bool TipRead(u32 tip);
// Last of the overlay. `on`: the skin runs; `pad`: the pad owns the input (the help takes its fixed line).
void Draw(void* device, bool on, bool pad);
// Everything given back: the game's cursor.
void Shutdown();
// Debug: every flight reticle drawn in a row across the middle (`uiskin cursorsheet 1`)
void Sheet(bool on);
std::string Status();
} // namespace skinover
