// Borderless windowed when nothing else provides it (the owner, 26 Oct 2026; core\winmode_rules.h has the decisions,
// docs\kb\display.md "Window mode path" the game's code). Setting `borderless` ([Controller], read at the start): 0 off,
// 1 auto (default: only when neither the game's own window style nor a Direct3D wrapper provides it), 2 always.
// While FLvibed provides it (the DPI awareness stays the exe's: the game window exists before FLvibed loads; winmode.cpp
// LogDpi): every display setup the game asks for (exe::SetupScreen: its start, the Options screen, FLUF's
// Alt+Enter) is windowed at the monitor's size - never exclusive fullscreen - and after the game's own window style
// call (exe::WindowStyle) the window loses its frame and covers its monitor. FLUF's Alt+Enter toggles between that and
// the game's bordered window. Anything unverified (a symbol, a detour) = the game's mode as today + one log line.
#pragma once
#include "common.h"

namespace winmode
{
// At the host's start (exe::ResolveAll done; before the game creates its window). `testStart`: a background test start
// (the test-only `winmode altenter` command is allowed).
void Install(const char* iniPath, bool testStart);
// HostApi v26: who provides borderless now - "flvibed", "game" (the game's own window style), "wrapper" (a Direct3D
// wrapper: `detail` names its d3d8.dll), "off", "unverified" (FLvibed's part refused), "undecided" (before the first
// display setup).
const char* ProviderNow(char* detail, int detailLen);
// Debug `winmode` (status) / `winmode altenter` (background test starts only: a display setup as FLUF's Alt+Enter makes
// it - a copy of the applied settings with the fullscreen byte flipped - refused unless FLvibed's rewrite applies, so
// it can never reach exclusive fullscreen).
void Command(const char* args, char* reply, size_t len);
} // namespace winmode
