// System headers for every FLvibed module, included OUTSIDE the module namespaces. build.cmd compiles each module
// source through a wrapper ("#include pre.h; namespace flv_<module>_<part> { #include <source> }"), so the
// modules' own headers and code land in their namespace while these headers (include-guarded) are already in.
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
#include <cstring>
#include <deque>
#include <fstream>
#include <istream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
