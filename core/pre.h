// System headers for every FLvibed module, included OUTSIDE the module namespaces. build.cmd compiles each module
// source through a wrapper ("#include pre.h; namespace flv_<module>_<part> { #include <source> }"), so the
// modules' own headers and code land in their namespace while these headers (include-guarded) are already in. A logic
// wrapper includes every core header the same way first (build.cmd :wraplogic), so core code is the one global instance.
// Every system header a module uses must be listed here, with the same configuration macros as common.h.
#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
// (the controller logic's LiveSplit client, 25 Oct 2026; ws2_32.lib in build.cmd LIBS)
#include <winsock2.h>
#include <ws2tcpip.h>

#include <Xinput.h>
#include <dinput.h>
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <audioclient.h>
#include <mmreg.h>
#include <tlhelp32.h>
#include <intrin.h>
#include <share.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <deque>
#include <fstream>
#include <istream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

// SDL3 (WORK_QUEUE Q5: the opt-in gamepad backend, modules\controller\src\host\sdlpad.cpp): only for the controller host's sources,
// whose wrappers define FLV_SDL3 (build.cmd :wrap; the headers come from build\sdl3\<version>\include as external
// headers). The logic and the other modules never see SDL: the logic reads the backend through HostApi v33. SDL's
// headers declare everything at global scope (extern "C"), so the host's namespace finds them as ::SDL_*.
#ifdef FLV_SDL3
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_joystick.h>
#include <SDL3/SDL_revision.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_version.h>
#endif
