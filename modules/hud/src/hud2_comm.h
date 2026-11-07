// The game's comm window under HUD 2 whenever the subtitles' panel does not hold it (they are off, the module is, no line stands for it): at its own place it lay under the nav block, its
// caption over the nav's lines (Shockwave, 7 Nov 2026). It stands below the nav and follows it, the speaker's name under it in HUD 2's text. The mover: core\game\commwin.*.
#pragma once
#include "common.h"

namespace hud2comm
{
// Each frame of the flight: `up`, HUD 2 shows; `x`, `below`: the nav's text column and bottom edge, px.
void Frame(bool up, float x, float below, float H);
void Draw(float H, bool named); // inside HUD 2's kit pass; `named`: the subtitles' panel names the speaker (one value, one place)
} // namespace hud2comm
