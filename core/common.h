// FLvibed - shared includes and small helpers for every module (compiled into each module's namespace).
#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
// 16-bit leftovers (minwindef.h: empty `near` / `far`): a variable named `near` broke the build twice (20 Oct 2026)
#undef near
#undef far

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int16_t i16;
typedef int32_t i32;

// Read/write a value at an absolute address inside Freelancer.exe's memory.
template <class T> inline T& Mem(uintptr_t addr) { return *reinterpret_cast<T*>(addr); }

inline float Clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

// An environment variable of the game process, sized from its value (a fixed buffer once cut FLVIBED_INSTANCE at 15
// characters and the instance silently fell back to the main channel, 26 Oct 2026); "" when unset.
inline std::string EnvVar(const char* name)
{
    for (int tries = 0; tries < 3; ++tries) // the value may change between the size query and the read
    {
        const DWORD need = GetEnvironmentVariableA(name, nullptr, 0); // with the terminator
        if (need == 0)
            return {};
        std::string v(need, '\0');
        const DWORD got = GetEnvironmentVariableA(name, &v[0], need); // without the terminator
        if (got < need)
        {
            v.resize(got);
            return v;
        }
    }
    return {};
}

// The parallel test game's instance name (deploy.ps1 -Instance, FLVIBED_INSTANCE); "" = the main install. It names
// the debug channels ("Local\FLControllerShm_<name>") and the background window title.
inline std::string InstanceName() { return EnvVar("FLVIBED_INSTANCE"); }
