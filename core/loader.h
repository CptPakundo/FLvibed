// FLvibed - the one owner of the logic's life (docs\MODULES_PLAN.md P1). The logic of every module is one unit: linked into
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
constexpr u32 kLogicApiVersion = 3;
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
