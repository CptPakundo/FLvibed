// Virtual mouse: injects movement, wheel and button events into the DirectInput mouse
// device that Freelancer polls, so the game's own cursor/click logic handles them.
#pragma once
#include "common.h"

namespace dimouse
{
// Install vtable hooks on the game's current mouse device (call every frame; cheap).
void Update();

// Relative movement in window pixels (accumulated until the game polls the mouse).
void MoveBy(float dx, float dy);
// Drive the cursor towards an absolute window-pixel position (exact at poll time).
void MoveTo(float x, float y);
// Stop driving the cursor to an absolute position.
void ReleaseTarget();
void Wheel(int notches);
void SetButton(int index, bool down);
bool ButtonDown(int index);
// Release all virtual buttons and pending motion.
void Reset();
// Whether Freelancer currently owns the mouse (windowed modes release it at the edges).
bool IsCaptured();
// Make Freelancer take the mouse back (as when its window is clicked).
void Capture();
// GetTickCount() of the last physical mouse activity.
u32 LastRealTick();
// The physical mouse's button state as DirectInput last reported it (0 left, 1 right, 2 middle; the virtual buttons
// excluded) - HostApi v18 RealMouseDown.
bool RealButtonDown(int index);
// The physical mouse's motion since the last call, in window pixels (counts x the game's scale); true when any -
// HostApi v19 RealMouseMotion.
bool RealMotion(int* dx, int* dy);
// The physical wheel's whole notches since the last call (+ away from the player = up; the virtual mouse's excluded) -
// HostApi v27 RealMouseWheel.
int RealWheel();
// The game runs out of focus (host keep-running): its device is not acquired, so reads fail; they then return the
// virtual mouse alone (the physical one belongs to Windows meanwhile).
void SetBackground(bool on);
// Background: write the virtual cursor and buttons into the game's cursor state (call before its cursor update).
void BackgroundCursor();

std::string Stats();
} // namespace dimouse
