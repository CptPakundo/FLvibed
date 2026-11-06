// The haptics engine's game side (modules\controller\docs\HAPTICS.md): the game's state read into Signals, the engine and the renderers of haptics_rules.h and haptics_render.h,
// and the result to the pad through Gamepad (XInput or the SDL backend, whichever reads the pad). Closed, it costs one branch a frame: no pad, or the pad not in use, or everything off.
#pragma once
#include "common.h"
#include "../pad_backend.h"
#include "../wave_synth.h"
#include "haptics_rules.h"
#include "haptics_render.h"

#include <sstream>
#include <string>

class Gamepad;

namespace haptics
{
// What the frame tells the haptics (controller.cpp PadFeel).
struct Frame
{
    Where where = Where::Menu;
    bool enabled = true;    // the controller module is on
    bool paused = false;    // the game is paused
    bool autopilot = false; // the Advanced Autopilot flies (its end = the hand-back)
    bool window = false;    // a NeuroNet window is open in flight: the triggers page there
    Role role[2] = {Role::None, Role::None}; // each trigger's role in the live table: left, right
    bool held[2] = {false, false};           // a finger past the press on that trigger
    bool fireHeld = false, firePressed = false, launcherPressed = false;
};

// Once per frame, after the pad's read (its focus gate known): the events, the mixer, the pad.
void Update(const Frame& f, Gamepad& pad);
// The logic's shutdown, a fault: the motors off, the triggers Off, the state cleared.
void Stop(Gamepad& pad);
// Debug `haptics [status] | table | play <effect> [level 0..1] | stop | record on|off|dump`: `play` goes through the same gates and mixer as the game's events (TEST: the
// mixer path without a fight); `record` keeps what would reach the pad, in order.
std::string Command(std::istream& in);
std::string Status();
} // namespace haptics
