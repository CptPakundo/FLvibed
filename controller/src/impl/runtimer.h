// The HUD timer of speedrun mode (docs\UI_SPEC.json `hud` state `speedrun`, setting `speedrun_timer`): top right under
// the NeuroNet box, in pad mode AND with keyboard and mouse (the timer belongs to speedrun-mode saves, not to a device):
// the run time h:mm:ss.t (livesplit.cpp's clock: game time, loads excluded), the current split's name (the next
// checkpoint's label) and, after a split, the delta of that segment against the player's best segment, coloured good /
// bad; the bests live in `EXE\FLvibed\splits_best.ini` (best segment per checkpoint, kept by this module). Nothing of
// it is called official (speedrun.com times Freelancer in real time by hand).
#pragma once
#include "common.h"

#include <istream>
#include <string>

namespace runtimer
{
void Init();
void Shutdown();
// Every frame (ctl::OnFrame): notices splits and keeps the bests.
void Update(float dt);
// Inside the overlay pass.
void Draw();
// Debug: timer state|reset-best
std::string Command(std::istream& in);
std::string Status();
} // namespace runtimer
