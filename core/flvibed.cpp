// FLvibed.dll - the one DLL Freelancer loads (EXE\dacom.ini [Libraries]). It holds the host (the event router and the hooks the
// modules share: core\host; each module's own hooks: its host part) and, in the release build, the logic too. Each module works
// alone or with the others:
//   EXE\FLvibed.ini   [Modules]  subtitles = 1   controller = 1   (0 = off; missing file or key = on)
// More rows and how they inherit: core\modules_rules.h. The core starts when a module is on; `safe = 1` or both switches at 0
// install nothing. The modules keep their own logs, settings and data files (FLSubtitles.log/.ini and EXE\FLSubtitles\,
// FLController.log/.ini) and debug channels.
// The module switches are also a section of the settings bus (core\settings_bus.cpp, "Modules", module "core"): the
// options page lists them with the other settings and writes them back here; they take effect at the next start.
#include "pre.h"

#include "modules_rules.h"
#include "settings_bus.h"
#include "settings_reset.h"

namespace flv_host
{
namespace router
{
void Start(HINSTANCE self, bool subtitles, bool controller, const char* rows, const bool* moduleRows);
}
namespace focus
{
void ThreadAttached();
}
} // namespace flv_host
namespace flv_subtitles_host
{
void Start(HINSTANCE self);
}
namespace flv_controller_host
{
void Start(HINSTANCE self);
}
namespace flv_display_host
{
void Start();
void AddVerbs();
}
namespace flv_movies_host
{
void Start();
}
namespace flv_autopilot_host
{
void Start();
}

extern "C" const SettingsBus* flv_settings_bus();

static char g_iniPath[MAX_PATH];
static bool g_safe = false;       // `safe = 1`: nothing starts
static bool g_on[modules::kRows]; // every row, resolved: what runs
static bool g_want[modules::kRows]; // the page's rows: what the next start should run
static char g_rows[160];          // the rows as resolved, for the log

static const settings::Def kModuleDefs[] = {
    {"subtitles", settings::Bool, &g_want[modules::kSubtitles], 0, 1,
     "Dialogue subtitles. FLvibed needs this module or the Controller module: with both off it does not start at all, this page included, so one of the two stays on. Takes effect after the game restarts."},
    {"controller", settings::Bool, &g_want[modules::kController], 0, 1,
     "The whole game on a controller: its prompts, screens and on-screen keyboard. Turning it off leaves the rows below as they are. FLvibed needs this module or the Subtitles module, so one of the two stays on. Takes effect after the game restarts."},
    {"hud", settings::Bool, &g_want[modules::kHud], 0, 1,
     "HUD 2, the Extended HUD 2 look and the shield and reticle cues, with keyboard and mouse or a controller. Off: the game's own HUD. It needs the Subtitles or the Controller module on. Takes effect after the game restarts."},
    {"autopilot", settings::Bool, &g_want[modules::kAutopilot], 0, 1,
     "The Advanced Autopilot, its goal panel and the maneuver bar's button, with keyboard and mouse or a controller. It needs the Subtitles or the Controller module on. Takes effect after the game restarts."},
    {"speedrun", settings::Bool, &g_want[modules::kSpeedrun], 0, 1,
     "Speedrun mode: NEW GAME offers it, then the timer, the splits and LiveSplit, with keyboard and mouse or a controller. It needs the Subtitles or the Controller module on. Takes effect after the game restarts."},
    {"display", settings::Bool, &g_want[modules::kDisplay], 0, 1,
     "Borderless windowed, the window's fit to its monitor, and running out of focus. It needs the Subtitles or the Controller module on. Takes effect after the game restarts."},
    {"npc", settings::Bool, &g_want[modules::kNpc], 0, 1,
     "The recorder of stalled ships, a log for bug reports. It changes nothing in the game. It needs the Subtitles or the Controller module on. Takes effect after the game restarts."},
    {"movies", settings::Bool, &g_want[modules::kMovies], 0, 1,
     "Movies fitted to the screen, the choice of intro and the skip of the startup movies. It needs the Subtitles or the Controller module on. Takes effect after the game restarts."},
};
static_assert(sizeof(kModuleDefs) / sizeof(kModuleDefs[0]) == modules::kRows, "a row of every module");
static const char* const kModuleDefaults[] = {"1", "1", "1", "1", "1", "1", "1", "1"}; // on, whatever the ini says now

// A row as the settings read a value, so `hud = on` means on here too; a number with a note behind it stays that number.
static int IniRow(const char* key, int absent)
{
    char value[32] = {};
    if (!GetPrivateProfileStringA("Modules", key, "", value, sizeof(value), g_iniPath) || !value[0])
        return absent;
    double x = 0;
    return settings::Number(value, x) ? x != 0 : atoi(value) != 0;
}

static void ReadIni(int (&ini)[modules::kRows])
{
    for (int r = 0; r < modules::kRows; ++r)
        ini[r] = IniRow(modules::kRowNames[r], modules::kAbsent);
}

static void ModulesSave(void*)
{
    if (!g_iniPath[0])
        return;
    int ini[modules::kRows];
    ReadIni(ini);
    bool write[modules::kRows];
    modules::ToWrite(ini, g_want, write);
    for (int r = 0; r < modules::kRows; ++r)
        if (write[r])
            WritePrivateProfileStringA("Modules", modules::kRowNames[r], settings::Value(kModuleDefs[r]).c_str(), g_iniPath);
}

static void ModulesApplied(void*, const settings::Def* d)
{
    const int row = static_cast<int>(d - kModuleDefs);
    if (row < 0 || row >= modules::kRows || !g_want[row] || !g_iniPath[0])
        return;
    int ini[modules::kRows];
    ReadIni(ini);
    modules::Follow(ini, row, g_want);
}

static void ReadRows()
{
    int ini[modules::kRows];
    ReadIni(ini);
    modules::Resolve(ini, g_on);
    memcpy(g_want, g_on, sizeof(g_want));
    g_safe = IniRow("safe", 0) != 0;
    size_t used = 0;
    for (int r = 0; r < modules::kRows; ++r)
        used += sprintf_s(g_rows + used, sizeof(g_rows) - used, "%s%s %d", r ? ", " : "", modules::kRowNames[r], g_on[r]);
}

static void RegisterModules(HINSTANCE self)
{
    GetModuleFileNameA(self, g_iniPath, MAX_PATH);
    if (char* slash = strrchr(g_iniPath, '\\'))
    {
        // a release that changed the defaults resets an older release's settings first (settings_reset.h), before
        // the switches below or any module read them
        slash[1] = '\0';
        const bool oldSettings = settings_reset::SettingsFound(g_iniPath); // (before the reset moves them aside and before any module writes its ini)
        settings_reset::Run(g_iniPath);
        settings_reset::MoveValues(g_iniPath);
        if (settings_reset::FirstCardFor(settings_reset::CardStamped(g_iniPath), oldSettings) == settings_reset::FirstCard::Stamp)
            settings_reset::StampCard(g_iniPath);
        strcpy_s(slash + 1, MAX_PATH - (slash + 1 - g_iniPath), "FLvibed.ini");
    }
    ReadRows();
    static SettingsBusSection section = {"core", "Modules", kModuleDefs, sizeof(kModuleDefs) / sizeof(kModuleDefs[0]),
                                         nullptr, ModulesApplied, ModulesSave, kModuleDefaults};
    if (const SettingsBus* bus = flv_settings_bus())
        bus->Register(&section);
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID)
{
    static bool started = false;
    if (reason == DLL_PROCESS_ATTACH)
    {
        // Thread notifications only for a background test start (deploy.ps1 -Background, FLVIBED_BACKGROUND=1): the host
        // guards every new thread's windows against taking the focus from its first instruction.
        char bg[8] = {};
        if (!(GetEnvironmentVariableA("FLVIBED_BACKGROUND", bg, sizeof(bg)) && bg[0] == '1'))
            DisableThreadLibraryCalls(instance);
        // Pinned for the life of the process: FLvibed leaves detours, vtable and import patches in the game's code, so the
        // DLL must never go before the game's last call into them (a player's report: a crash on exit
        // executing at an unmapped address = code of an unloaded DLL; the game's shutdown may free its plugins first).
        HMODULE pinned = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                           reinterpret_cast<LPCWSTR>(&DllMain), &pinned);
        RegisterModules(instance); // the switches as read now: the modules start from these
        started = !g_safe && (g_on[modules::kSubtitles] || g_on[modules::kController]);
        if (started)
        {
            flv_host::router::Start(instance, g_on[modules::kSubtitles], g_on[modules::kController], g_rows, g_on);
            if (g_on[modules::kSubtitles])
                flv_subtitles_host::Start(instance);
            if (g_on[modules::kDisplay])
                flv_display_host::Start();
            if (g_on[modules::kController])
                flv_controller_host::Start(instance);
            if (g_on[modules::kDisplay])
                flv_display_host::AddVerbs();
            if (g_on[modules::kMovies])
                flv_movies_host::Start();
            if (g_on[modules::kAutopilot])
                flv_autopilot_host::Start();
        }
    }
    else if (reason == DLL_THREAD_ATTACH && started)
        flv_host::focus::ThreadAttached();
    return TRUE;
}
