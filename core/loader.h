// FLvibed - the one owner of the logic's life (the module split plan's P1). The logic of every module is one unit: linked into
// FLvibed.dll in the release build, FLvibedLogic.dll in the dev build, so a dev build has the release's shape. Each
// module's host part starts its module where it always did (the subtitles at the first movie, the controller at the first
// frame): it asks for its table here and runs its own Init. A reload or an unload is one act for all of them.
// One instance in FLvibed.dll (core\loader.cpp, compiled plain), reached by this C symbol, as the settings bus is.
#pragma once
#include "common.h"

enum LogicModule : int
{
    kLogicSubtitles = 0,
    kLogicController = 1,
    kLogicCore = 2, // the options page with the controller off (core\logic\core_api.h)
    kLogicMovies = 3,
    kLogicModules = 4
};

// What the logic hands the loader (core\logic\entry.cpp): each module's own ImplApi table.
// kLogicApiVersion is the number of the whole interface between FLvibed.dll's host and the logic, and the loader loads only a
// logic built with the same number (a dev build's hot reload; a release links both from one build). Bump it with every change to
// what crosses between them: a module's HostApi or ImplApi (modules\*\src\api.h), core\logic\core_api.h, this file, the buses
// (core\hud_bus.h, core\settings_bus.h), core\game\hostcalls.h, a shared block (modules\*\src\shm.h), or any host type the logic
// reads. No call checks a version beyond this one; when in doubt bump it: a mismatch costs a restart, a missed bump a crash.
constexpr u32 kLogicApiVersion = 9; // 9: hostcalls / controller HostApi v50 SetQuietZones(flags), MapZoneIds for the influence cells (issue 61); 8: hostcalls / controller HostApi v49 SetQuietZones, MapZoneEdges, MapZoneStripes (issue 61); 7: hostcalls / controller HostApi v48 SetRawMouse (mouse_raw); 6: controller HostApi SdlPadFilter's `on` became flags (padbackend::kFilterKeys / kFilterMouse); 5: hostcalls / HostApi v47 InstallHud2Hooks (audit R4c-2)
struct LogicApi
{
    u32 version;
    const void* module[kLogicModules];
};
typedef const LogicApi*(__cdecl* GetLogicApiFn)();

// A host's side of it. stop: the module's Shutdown and everything it set on the host ends, no call into the logic after it.
// start: the module takes its table and runs its Init. A module's fault stays its own: both guard the logic's calls.
struct LogicClient
{
    void (*stop)();
    bool (*start)(const void* table);
    void (*log)(const char* line);
};

struct LogicLoader
{
    void (*Register)(int module, const LogicClient* client);
    // The module's table, the logic loaded at the first call. A module that asked once restarts with every reload.
    const void* (*Open)(int module);
    // Stops every started module, last started first; frees the logic; loads it again; starts them in order. Only from a host
    // with no logic call on its stack (the frame's command poll).
    bool (*Reload)();
    void (*Unload)();
    const char* (*Name)(); // what is loaded, for the log: "built in" or the .run copy's file name
};

extern "C" const LogicLoader* flv_logic_loader();
