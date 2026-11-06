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
u32 LastRealKeyTick();
std::string RawKeyStatus(); // the raw-key counters, for `uibtn`
} // namespace input
