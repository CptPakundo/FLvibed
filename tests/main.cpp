// The test runner (tests\test.h). Usage: fltest.exe [<install dir>] [-v]
//   <install dir>  the Freelancer HD Edition folder (DATA\MISSIONS inside) for the data tests; else FLVIBED_INSTALL, else
//                  the repository's Install\Freelancer HD Edition (from build\test\); absent -> those tests are skipped
//   -v             verbose: the core's Log() lines and the data tests' details
//   -snaps <dir>   also check every recorded situation snapshot (*.snap) in <dir> (its in-game hash = the host-free one)
#include "test.h"

#include "log.h"

#include <fstream>

TestState g_test;

void Test_flhash();
void Test_ini();
void Test_utf();
void Test_textfit();
void Test_roletext();
void Test_skin_rules();
void Test_nnpage();
void Test_equip_rules();
void Test_marquee();
void Test_glyphset();
void Test_promptlabel();
void Test_promptshow();
void Test_hudwake();
void Test_handheld();
void Test_hud2_roots();
void Test_hud2_bars();
void Test_hud2_target();
void Test_namecut();
void Test_hud2_cue();
void Test_winfit();
void Test_nntabs();
void Test_navmap_layout();
void Test_padmap_zones();
void Test_navview();
void Test_navmap_labels();
void Test_movie_words();
void Test_input_rules();
void Test_mix_cues();
void Test_avoid();
void Test_route();
void Test_flight();
void Test_decision_synthetic();
void Test_decision_baseline();
void Test_decision_forget_game();
void Test_fluf_hooks();
void Test_fluf_probe();
void Test_press_gate();
void Test_focus_gate();
void Test_maneuver_watch();
void Test_target_cycle();
void Test_room_ring();
void Test_dealer_rules();
void Test_pad_slots();
void Test_pad_backend();
void Test_sdl_testpad();
void Test_orig_image();
void Test_winmode();
void Test_promptfit();
void Test_settings();
void Test_settings_reset();
void Test_mprules_tag();
void Test_tractorfail();
void Test_presets();
void Test_options_rules();
void Test_str();
void Test_universe();
void Test_linebreak();
void Test_stack_fit();
void Test_chat_size();
void Test_wgroups();
void Test_stallwatch();
void Test_mission_live();
void Test_mission_live_goals();
void Test_mission_lock_trap();
void Test_gameclock();
void Test_loader();
void Test_log_channel();
void Test_modules_rules();
void Test_settings_bus();
void Test_ordnance();
void Test_haptics();
void Test_haptics_engine();
void Test_wave_synth();
void Test_jobs();
void Test_objective_kinds();
void Test_situation_io();
void Test_situation();
void Test_situation_rules();
void Test_goalqueue();
void Test_situation_exec();
void Test_look();
void Test_city_hold();
void Test_dock_skip();
void Test_strafe();
void Test_hud2_lint();
void Test_hud2_sim();
void Test_hud2_preset();
void Test_hud2_layout();
void Test_hud2_focus();
void Test_spawn_charge();
void Test_ash_ride();
void Test_hud2_notice();

static bool DirExists(const std::string& p)
{
    const DWORD a = GetFileAttributesA(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

static std::string ExeDir()
{
    char path[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    std::string p = path;
    const size_t slash = p.find_last_of('\\');
    return slash == std::string::npos ? std::string() : p.substr(0, slash + 1);
}

static void RemoveTree(const std::string& dir)
{
    WIN32_FIND_DATAA d;
    HANDLE h = FindFirstFileA((dir + "\\*").c_str(), &d);
    if (h != INVALID_HANDLE_VALUE)
    {
        do
        {
            const std::string n = d.cFileName;
            if (n == "." || n == "..")
                continue;
            if (d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                RemoveTree(dir + "\\" + n);
            else
                DeleteFileA((dir + "\\" + n).c_str());
        } while (FindNextFileA(h, &d));
        FindClose(h);
    }
    RemoveDirectoryA(dir.c_str());
}

bool NeedInstall(const char* what)
{
    if (!g_test.install.empty())
        return true;
    ++g_test.skipped;
    printf("  skipped: %s needs the test install\n", what);
    return false;
}

std::string TempPath(const char* name, const char* sub)
{
    std::string dir = g_test.temp;
    if (sub && *sub)
    {
        dir += std::string("\\") + sub;
        CreateDirectoryA(dir.c_str(), nullptr);
    }
    return dir + "\\" + name;
}

bool WriteBytes(const std::string& path, const void* data, size_t size)
{
    std::ofstream out(path, std::ios::binary);
    out.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
    return out.good();
}

bool WriteTextFile(const std::string& path, const std::string& text)
{
    return WriteBytes(path, text.data(), text.size());
}

std::vector<std::string> ReadLines(const std::string& path)
{
    std::vector<std::string> out;
    std::ifstream in(path, std::ios::binary);
    std::string line;
    while (std::getline(in, line))
    {
        if (out.empty() && line.size() >= 3 && line.compare(0, 3, "\xEF\xBB\xBF") == 0)
            line.erase(0, 3);
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
            line.pop_back();
        out.push_back(line);
    }
    return out;
}

int main(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        if (a == "-v")
            g_test.verbose = true;
        else if (a == "--baseline" && i + 1 < argc)
            g_test.baseline = argv[++i];
        else if (a == "-snaps" && i + 1 < argc)
            g_test.snaps = argv[++i];
        else if (!a.empty() && a[0] != '-')
            g_test.install = a;
    }
    const std::string exeDir = ExeDir();
    if (g_test.install.empty())
        if (const char* e = getenv("FLVIBED_INSTALL"); e && *e)
            g_test.install = e;
    if (g_test.install.empty())
        g_test.install = exeDir + "..\\..\\Install\\Freelancer HD Edition";
    while (!g_test.install.empty() && (g_test.install.back() == '\\' || g_test.install.back() == '/'))
        g_test.install.pop_back();
    if (!DirExists(g_test.install + "\\DATA\\MISSIONS"))
    {
        printf("no test install at %s: the data tests are skipped\n", g_test.install.c_str());
        g_test.install.clear();
    }
    if (g_test.baseline.empty())
        g_test.baseline = exeDir + "..\\..\\tools\\decision_baseline.txt";
    g_test.fixtures = exeDir + "..\\..\\tests\\fixtures";
    char tmp[MAX_PATH] = {};
    GetTempPathA(MAX_PATH, tmp);
    g_test.temp = std::string(tmp) + "fltest_" + std::to_string(GetCurrentProcessId());
    CreateDirectoryA(g_test.temp.c_str(), nullptr);
    if (g_test.verbose)
        LogSetSink([](const char* line) { fprintf(stderr, "  log: %s\n", line); }, "");

    struct Test
    {
        const char* name;
        void (*fn)();
    };
    const Test tests[] = {
        {"flhash", Test_flhash},
        {"ini", Test_ini},
        {"utf", Test_utf},
        {"textfit", Test_textfit},
        {"roletext", Test_roletext},
        {"skin_rules", Test_skin_rules},
        {"nnpage", Test_nnpage},
        {"equip_rules", Test_equip_rules},
        {"marquee", Test_marquee},
        {"glyphset", Test_glyphset},
        {"promptlabel", Test_promptlabel},
        {"promptshow", Test_promptshow},
        {"hudwake", Test_hudwake},
        {"handheld", Test_handheld},
        {"hud2_roots", Test_hud2_roots},
        {"hud2_bars", Test_hud2_bars},
        {"hud2_target", Test_hud2_target},
        {"namecut", Test_namecut},
        {"hud2_cue", Test_hud2_cue},
    {"winfit", Test_winfit},
        {"nntabs", Test_nntabs},
        {"navmap_layout", Test_navmap_layout},
        {"padmap_zones", Test_padmap_zones},
        {"navview", Test_navview},
        {"navmap_labels", Test_navmap_labels},
        {"movie_words", Test_movie_words},
    {"input_rules", Test_input_rules},
        {"mix_cues", Test_mix_cues},
        {"avoid", Test_avoid},
        {"route", Test_route},
        {"flight", Test_flight},
        {"decision_synthetic", Test_decision_synthetic},
        {"decision_baseline", Test_decision_baseline},
        {"decision_forget_game", Test_decision_forget_game},
        {"fluf_hooks", Test_fluf_hooks},
        {"fluf_probe", Test_fluf_probe},
        {"press_gate", Test_press_gate},
    {"focus_gate", Test_focus_gate},
    {"maneuver_watch", Test_maneuver_watch},
    {"target_cycle", Test_target_cycle},
    {"room_ring", Test_room_ring},
    {"dealer_rules", Test_dealer_rules},
    {"pad_slots", Test_pad_slots},
        {"pad_backend", Test_pad_backend},
        {"sdl_testpad", Test_sdl_testpad},
        {"orig_image", Test_orig_image},
        {"winmode", Test_winmode},
        {"settings", Test_settings},
        {"settings_reset", Test_settings_reset},
        {"promptfit", Test_promptfit},
        {"mprules_tag", Test_mprules_tag},
        {"tractorfail", Test_tractorfail},
        {"presets", Test_presets},
        {"options_rules", Test_options_rules},
        {"str", Test_str},
        {"universe", Test_universe},
        {"linebreak", Test_linebreak},
        {"stack_fit", Test_stack_fit},
        {"chat_size", Test_chat_size},
        {"wgroups", Test_wgroups},
        {"stallwatch", Test_stallwatch},
        {"mission_live", Test_mission_live},
        {"mission_live_goals", Test_mission_live_goals},
        {"mission_lock_trap", Test_mission_lock_trap},
        {"gameclock", Test_gameclock},
        {"loader", Test_loader},
        {"log_channel", Test_log_channel},
        {"modules_rules", Test_modules_rules},
        {"settings_bus", Test_settings_bus},
        {"ordnance", Test_ordnance},
        {"haptics", Test_haptics},
        {"haptics_engine", Test_haptics_engine},
        {"wave_synth", Test_wave_synth},
        {"jobs", Test_jobs},
        {"objective_kinds", Test_objective_kinds},
        {"situation_io", Test_situation_io},
        {"situation", Test_situation},
        {"situation_rules", Test_situation_rules},
        {"goalqueue", Test_goalqueue},
        {"situation_exec", Test_situation_exec},
        {"look", Test_look},
        {"city_hold", Test_city_hold},
        {"dock_skip", Test_dock_skip},
        {"strafe", Test_strafe},
        {"hud2_lint", Test_hud2_lint},
        {"hud2_sim", Test_hud2_sim},
        {"hud2_preset", Test_hud2_preset},
        {"hud2_layout", Test_hud2_layout},
        {"hud2_focus", Test_hud2_focus},
        {"spawn_charge", Test_spawn_charge},
        {"ash_ride", Test_ash_ride},
        {"hud2_notice", Test_hud2_notice},
    };
    for (const Test& t : tests)
    {
        const int c0 = g_test.checks, f0 = g_test.failures, s0 = g_test.skipped;
        t.fn();
        printf("%-20s %-8s %d checks\n", t.name,
               g_test.failures > f0 ? "FAIL" : (g_test.skipped > s0 ? "skipped" : "ok"), g_test.checks - c0);
    }
    RemoveTree(g_test.temp);
    printf("%u tests, %d checks, %d failures, %d skipped\n", static_cast<unsigned>(sizeof(tests) / sizeof(tests[0])),
           g_test.checks, g_test.failures, g_test.skipped);
    return g_test.failures ? 1 : 0;
}
