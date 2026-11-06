// FLvibed - core's own logic table: what core\host\pagehost.cpp drives when the controller is off. A host and a logic each compile
// this header in their own namespace; the two tables meet through the loader as plain pointers, as a module's HostApi / ImplApi do.
#pragma once
#include "common.h"
#include "game/hostcalls.h"

constexpr u32 kCoreApiVersion = 4;

struct CoreHostApi
{
    u32 version;
    const char* dllDir;
    void (*LogWrite)(const char* line);
    hostcalls::Table calls;
};

struct CoreImplApi
{
    u32 version;
    void (*Init)(const CoreHostApi* host);
    void (*Shutdown)();
    void (*OnFrame)(double dt);
    void (*OnOverlay)();
    void (*OnUnderHud)();
    bool (*OnRawKey)(int vk, bool down, bool textField);
    void (*OnUiButton)(int id);
    void (*Command)(const char* line, char* reply, int replyLen);
    void (*OnKeyCmd)(int cmd, bool down);
    void (*OnDeviceLost)();
    void (*OnFault)(); // as ImplApi::OnFault
};
