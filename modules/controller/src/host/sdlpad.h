// FLvibed controller host - the opt-in SDL3 gamepad backend (`pad_backend` 1; WORK_QUEUE Q5 slice 1, IDEAS U29): SDL3
// 3.4.18 linked statically (build.cmd, tools\sdl3\build_sdl3.cmd) and run on one host thread of its own. The logic asks
// through HostApi v33 (api.h SdlPad / SdlPadRumble); the pure pieces (button / axis mapping, the glyph vendor, the
// backend choice) are modules\controller\src\pad_backend.h.
#pragma once
#include "../api.h"

namespace sdlpad
{
// HostApi v33 SdlPad: on != 0 starts the thread (once) and returns the newest read; on = 0 stops it (SDL_Quit).
int Pad(int on, SdlPadState* out);
// HostApi v33 SdlPadRumble: a vibration request the thread sends at its next read.
void Rumble(u16 low, u16 high, u32 ms);
// HostApi v38 SdlPadFilter: a mapper's injected keys and mouse dropped while the pad has input.
void Filter(int on, float stickDeadzone, float triggerThreshold);
// The host-side debug command `sdlpad [status] | attach <xbox|ps5|switchpro> [slot] | detach [slot|all] | send ...`
// (WORK_QUEUE Q5 slice 2; TEST ONLY: refused in multiplayer and while SDL is not running): SDL virtual test pads on
// the backend's thread, their state and their read-back in shm.h v3 (tools\harness\flctl.core.ps1 *-FLSdlPad*).
void Command(const char* args, bool multiplayer, char* reply, int len);
} // namespace sdlpad
