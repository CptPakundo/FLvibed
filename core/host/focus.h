// FLvibed - the game out of focus: core\host\focus.cpp. A background test start (deploy.ps1 -Background, FLVIBED_BACKGROUND=1)
// runs out of focus, on the bottom of the z-order and silent; a player's `run_in_background` only keeps the frame running.
#pragma once

namespace focus
{
bool BackgroundTestStart(); // this process is one
bool Forced();              // a background test start with the hook in place: the guard is on
void Start(bool display); // before the game creates its window; false: only a background test start hooks
void ThreadAttached();      // DLL_THREAD_ATTACH
void SetKeepRunning(bool on);
} // namespace focus
