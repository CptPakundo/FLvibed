// The haptics skeleton's game side (IDEAS U30, WORK_QUEUE Q5 slice 3): the events (the hits and the shield
// failure from the player ship's shield / hull, the autopilot's hand-back, a box of the game's that waits for the
// player's answer) into the effect table and the mixer of haptics_rules.h, the mix to the pad through Gamepad::Rumble
// (XInput or the SDL backend, whichever reads the pad), scaled by `rumble`. The design, the table and the silence rules:
// haptics_rules.h; the docs: modules\hud\docs\HUD_CUES.md "Haptics".
#pragma once
#include "common.h"
#include "haptics_rules.h"

#include <sstream>
#include <string>

class Gamepad;

namespace haptics
{
// What the frame tells the haptics (controller.cpp OnFrame).
struct Frame
{
    Where where = Where::Menu;
    bool enabled = true;    // the controller module is on
    bool paused = false;    // the game is paused
    bool autopilot = false; // the Advanced Autopilot flies (its end = the hand-back)
};

// Once per frame, after the pad's read (its focus gate known): the events, the mixer, the pad.
void Update(const Frame& f, Gamepad& pad);
// The logic's shutdown: the motors off, the state cleared.
void Stop(Gamepad& pad);
// Debug `haptics [status] | table | play <effect> [level 0..1] | stop`: `play` goes through the same gates and mixer as
// the game's events (TEST: the mixer path without a fight).
std::string Command(std::istream& in);
std::string Status();
} // namespace haptics
