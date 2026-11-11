// HUD 2 over everything (modules\hud\docs\UI_HUD2_SKIN.md 6.6): its cursor, the set of 19 names of DATA\mouse.ini as decisions\D1b_cursor.jpg draws them, and the
// hover help (ToolTip, Rollover) in the Shadow tier. Position, clicks and hover stay the game's: the shape comes from the game's current cursor name, at the game's own position,
// with the game's mesh hidden; a name the table does not know (a mod's) stays the game's and is logged. Label `cursor` in skin_native keeps the game's.
#pragma once
#include "common.h"

#include <string>

namespace skinover
{
// The game's hover help, read before the UI pass while the game shows it: `which` 0 the ToolTip box (by the hovered control), 1 the Rollover line (a
// hotspot's long hint), `ctl` that control. Drawn by Draw as HUD 2's tooltip, clear of what HUD 2 draws over the game's UI, at the free place nearest to the
// game's. False: not shown or not readable (the caller leaves the game's). FREELANCER_RE "The hover help".
bool HelpRead(int which, u32 ctl);
void HelpClear(int which);
// The full-screen map's name tag (by the cursor) as the last Draw placed it: its rectangle x0, y0, x1, y1 and its text. False while none stands. The map's labels keep out of it.
bool MapTag(float r[4], std::wstring& text);
// A word HUD 2 draws over the game's UI under the cursor (GitHub issue 71: the game's button under it stands hidden elsewhere): the help of the game control
// the word stands for, read from that control as the game reads it - the ToolTip's short tip `tip`, the Rollover's long hint `rollover` - shown once the cursor
// rested skinrules::kHelpRest on it and drawn by the word (`r` its click box, pixels x0 y0 x1 y1). Each frame before the UI pass; `ctl` 0 = no word: nothing.
void WordHelp(u32 ctl, const std::wstring& word, const float r[4], float dt, bool tip, bool rollover);
// Last of the overlay. `on`: the skin runs; `pad`: the pad owns the input (the help takes its fixed line).
void Draw(void* device, bool on, bool pad);
// Everything given back: the game's cursor.
void Shutdown();
// Debug: every flight reticle drawn in a row across the middle (`uiskin cursorsheet 1`)
void Sheet(bool on);
std::string Status();
} // namespace skinover
