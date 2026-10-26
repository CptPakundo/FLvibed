// The input-device service (docs\UI_PLAN.md 6.2, UI_BUILD.md 1): which device the player used last, Pad or keyboard
// and mouse, so the whole controller UI swaps in one frame and the game's own UI returns on a mouse move or a key.
// Last valid input wins: pad = any button, the D-pad, a trigger past 0.5, a stick past its dead zone (the Gamepad's
// values are dead-zoned, so drift never counts; the harness's virtual pad counts as the pad); keyboard and mouse = a
// key (the host's LastRealKeyTick: the game's own key handler, FLvibed's injected keys excluded), the physical mouse
// (dimouse::LastRealTick: motion, wheel, buttons; RealMouseDown for the buttons). After a switch the other device's
// axis-only signals are ignored for kAxisGrace; buttons and keys switch at once. `input_mode` locks it.
#pragma once
#include "common.h"

#include <string>

class Gamepad;

namespace device
{
enum class Device
{
    Pad,
    KbMouse,
};

// Once per frame, before the modes act on the input. `flying`: Mode::Space (pause_on_disconnect applies there only).
void Update(float dt, const Gamepad& pad, bool flying);

Device Active();
inline bool Pad() { return Active() == Device::Pad; }
inline bool KbMouse() { return Active() == Device::KbMouse; }
// True on the frame the device changed (the HUD layer vanishes / returns that frame).
bool Switched();
// The frame counter of the last switch to the pad / to keyboard and mouse (diagnostics).
u32 LastPadTick();
u32 LastKbmTick();

// pause_on_disconnect: a real pad dropped out while flying and the device was the pad - the caller taps the game's
// pause once (the flag clears on read).
bool TakeDisconnectPause();

// Menu grammar: the button that accepts and the one that cancels (XINPUT_GAMEPAD_* masks), by confirm_button: 0 =
// the bottom button accepts (A / cross / Nintendo B), 1 = the right one (Nintendo A).
u32 ConfirmMask();
u32 CancelMask();

std::string Status(); // one line for `state`
} // namespace device
