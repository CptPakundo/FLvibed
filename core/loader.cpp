// FLvibed - the logic's loader (core\loader.h): one instance in FLvibed.dll, compiled plain beside core\flvibed.cpp
// (build.cmd :link). The game's thread only, like the hosts' frames that call it.
#include "pre.h"

#include "loader.h"

#ifdef FLV_STATIC
extern "C" const LogicApi* flv_logic_api();
#endif

namespace
{
const LogicClient* g_clients[kLogicModules] = {};
bool g_opened[kLogicModules] = {}; // the module took its table: every reload starts it again
const LogicApi* g_api = nullptr;
char g_name[64] = "";
#ifndef FLV_STATIC
HMODULE g_module = nullptr;
int g_generation = 0;
#endif

void Say(const char* fmt, ...)
{
    char line[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);
    line[sizeof(line) - 1] = 0;
    for (const LogicClient* c : g_clients)
        if (c && c->log)
        {
            c->log(line);
            return;
        }
}

#ifdef FLV_STATIC
const LogicApi* LoadApi()
{
    strcpy_s(g_name, "built in");
    return flv_logic_api();
}

void FreeApi() {}
#else
std::string Dir()
{
    HMODULE self = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCSTR>(&Say), &self);
    char path[MAX_PATH] = {};
    GetModuleFileNameA(self, path, MAX_PATH);
    if (char* slash = strrchr(path, '\\'))
        slash[1] = 0;
    return path;
}

// From a copy, so the build can overwrite FLvibedLogic.dll while the game runs; the copies take turns, so a file the system
// has not let go of yet (a scan of the image just released) never fails a reload. One is live at a time.
const LogicApi* LoadApi()
{
    const std::string dir = Dir(), src = dir + "FLvibedLogic.dll";
    sprintf_s(g_name, "FLvibedLogic.run%d.dll", g_generation++ % 2);
    const std::string copy = dir + g_name;
    if (!CopyFileA(src.c_str(), copy.c_str(), FALSE))
    {
        Say("logic: cannot copy %s (%lu)", src.c_str(), GetLastError());
        return nullptr;
    }
    g_module = LoadLibraryA(copy.c_str());
    if (!g_module)
    {
        Say("logic: LoadLibrary failed (%lu)", GetLastError());
        return nullptr;
    }
    auto get = reinterpret_cast<GetLogicApiFn>(GetProcAddress(g_module, "GetLogicApi"));
    return get ? get() : nullptr;
}

void FreeApi()
{
    if (g_module)
        FreeLibrary(g_module);
    g_module = nullptr;
}
#endif

bool Load()
{
    if (g_api)
        return true;
    const LogicApi* api = LoadApi();
    if (!api || api->version != kLogicApiVersion)
    {
        Say("logic: incompatible (%s)", g_name);
        FreeApi();
        return false;
    }
    g_api = api;
    return true;
}

void StopAll()
{
    for (int m = kLogicModules - 1; m >= 0; --m)
        if (g_opened[m] && g_clients[m] && g_clients[m]->stop)
            g_clients[m]->stop();
    g_api = nullptr;
    FreeApi();
}

void Register(int module, const LogicClient* client)
{
    if (module >= 0 && module < kLogicModules)
        g_clients[module] = client;
}

const void* Open(int module)
{
    if (module < 0 || module >= kLogicModules || !Load())
        return nullptr;
    g_opened[module] = true;
    return g_api->module[module];
}

bool Reload()
{
    StopAll();
    if (!Load())
        return false;
    bool ok = true;
    for (int m = 0; m < kLogicModules; ++m)
        if (g_opened[m] && g_clients[m] && g_clients[m]->start)
            ok = g_clients[m]->start(g_api->module[m]) && ok;
    return ok;
}

const char* Name() { return g_name; }

const LogicLoader g_loader = {Register, Open, Reload, StopAll, Name};
} // namespace

extern "C" const LogicLoader* flv_logic_loader() { return &g_loader; }
