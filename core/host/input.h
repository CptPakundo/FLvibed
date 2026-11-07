// FLvibed - input injection: core\host\input.cpp. Held commands get an owner per module in P7 (docs\MODULES_PLAN.md).
#pragma once

namespace input
{
void Install();                     // the detours (exe::IsCmdHeld, exe::MouseSteer): when an injecting module starts
void InstallKeys();                 // the raw keys (exe::WinKey) and the hit test (exe::HitTest): at the host's start
void SetHeld(int cmd, bool held);   // a game command held down for exe::IsCmdHeld
void ClearAllHeld();
void SetSteer(bool active, float x, float y); // the stick's direction into the game's mouse flight
void SetSteerLift(bool underWindow, bool autopilot); // what that steering may lift, set each frame: a hovered control's refusal, the rear view's lock
// A fail-safe's stop: the steering ends with zero frames. Freelancer keeps the last direction when the override just ends, and the logic that would have
// sent them is gone.
void SteerStop();
void SetHudHitSuppression(bool on, const uintptr_t* roots, int count); // those roots out of the game's UI hit test
// Each frame a full-screen window stands: fl::FlightCommand's commands answer no key or mouse button. What FLvibed holds or fires itself goes on (the autopilot flies under the window).
void SetFlightLock();
bool FlightLocked();
// Each frame a NeuroNet window should stop the world (single player, in space): the game's own pause closes the window, so only the world's gates hear it (0x54B000, FrameUpdate); ends by itself when the calls stop.
void SetWorldPause();
// A command under that pause (a press or click on a page; a route, by the hook): the server gets a few frames with no time in them to answer it (input.cpp).
void WorldCommand();
bool TakeCommandFrame(); // the frame's: FrameUpdate runs with dt 0 when it says yes (router.cpp)
u32 LastRealKeyTick();
std::string RawKeyStatus(); // the raw-key counters, for `uibtn`
} // namespace input
