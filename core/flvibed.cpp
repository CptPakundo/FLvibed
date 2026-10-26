// FLvibed.dll - the one DLL Freelancer loads (EXE\dacom.ini [Libraries]). It holds the modules' hosts (hooks into
// the game) and, in the release build, their logic too; each module works alone or with the others:
//   EXE\FLvibed.ini   [Modules]  subtitles = 1   controller = 1   (0 = off; missing file or key = on)
// Modules start in the order they were loaded as separate DLLs before (subtitles, then controller), so their
// detours chain as before. The modules keep their own logs, settings and data files (FLSubtitles.log/.ini and
// EXE\FLSubtitles\, FLController.log/.ini) and debug channels.
// The module switches are also a section of the settings bus (core\settings_bus.cpp, "Modules", module "core"): the
// options page lists them with the other settings and writes them back here; they take effect at the next start.
#include "pre.h"

#include "settings_bus.h"

namespace flv_subtitles_host
{
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved);
}
namespace flv_controller_host
{
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved);
}

extern "C" const SettingsBus* flv_settings_bus();

static char g_iniPath[MAX_PATH];
static bool g_modSubtitles = true, g_modController = true; // the [Modules] rows (the live values = the ini's)

static const settings::Def kModuleDefs[] = {
    {"subtitles", settings::Bool, &g_modSubtitles, 0, 1,
     "the subtitles and movies module (dialogue subtitles, movies fitted to the screen); takes effect after a restart"},
    {"controller", settings::Bool, &g_modController, 0, 1,
     "the controller module (native pad support, the Advanced Autopilot, the FLvibed UI); takes effect after a restart"},
};
static const char* const kModuleDefaults[] = {"1", "1"}; // on, whatever the ini says now

static void ModulesSave(void*)
{
    if (!g_iniPath[0])
        return;
    settings::Save("Modules", g_iniPath, kModuleDefs, sizeof(kModuleDefs) / sizeof(kModuleDefs[0]));
}

static void RegisterModules(HINSTANCE self)
{
    GetModuleFileNameA(self, g_iniPath, MAX_PATH);
    if (char* slash = strrchr(g_iniPath, '\\'))
        strcpy_s(slash + 1, MAX_PATH - (slash + 1 - g_iniPath), "FLvibed.ini");
    g_modSubtitles = GetPrivateProfileIntA("Modules", "subtitles", 1, g_iniPath) != 0;
    g_modController = GetPrivateProfileIntA("Modules", "controller", 1, g_iniPath) != 0;
    static SettingsBusSection section = {"core", "Modules", kModuleDefs, sizeof(kModuleDefs) / sizeof(kModuleDefs[0]),
                                         nullptr, nullptr, ModulesSave, kModuleDefaults};
    if (const SettingsBus* bus = flv_settings_bus())
        bus->Register(&section);
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    static bool subtitles = false, controller = false;
    if (reason == DLL_PROCESS_ATTACH)
    {
        // Thread notifications only for a background test start (deploy.ps1 -Background, FLVIBED_BACKGROUND=1): the
        // controller host guards every new thread's windows against taking the focus from its first instruction.
        char bg[8] = {};
        if (!(GetEnvironmentVariableA("FLVIBED_BACKGROUND", bg, sizeof(bg)) && bg[0] == '1'))
            DisableThreadLibraryCalls(instance);
        RegisterModules(instance); // the switches as read now: the modules start from these
        subtitles = g_modSubtitles;
        controller = g_modController;
    }
    if (subtitles)
        flv_subtitles_host::DllMain(instance, reason, reserved);
    if (controller)
        flv_controller_host::DllMain(instance, reason, reserved);
    return TRUE;
}
