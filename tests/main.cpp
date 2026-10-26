// The test runner (tests\test.h). Usage: fltest.exe [<install dir>] [-v]
//   <install dir>  the Freelancer HD Edition folder (DATA\MISSIONS inside) for the data tests; else FLVIBED_INSTALL, else
//                  the repository's Install\Freelancer HD Edition (from build\test\); absent -> those tests are skipped
//   -v             verbose: the core's Log() lines and the data tests' details
#include "test.h"

#include "log.h"

#include <fstream>

TestState g_test;

void Test_flhash();
void Test_ini();
void Test_utf();
void Test_textfit();
void Test_movie_words();
void Test_avoid();
void Test_route();
void Test_flight();
void Test_decision_synthetic();
void Test_decision_baseline();
void Test_fluf_hooks();
void Test_fluf_probe();
void Test_press_gate();
void Test_orig_image();
void Test_winmode();
void Test_promptfit();

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
        {"movie_words", Test_movie_words},
        {"avoid", Test_avoid},
        {"route", Test_route},
        {"flight", Test_flight},
        {"decision_synthetic", Test_decision_synthetic},
        {"decision_baseline", Test_decision_baseline},
        {"fluf_hooks", Test_fluf_hooks},
        {"fluf_probe", Test_fluf_probe},
        {"press_gate", Test_press_gate},
        {"orig_image", Test_orig_image},
        {"winmode", Test_winmode},
        {"promptfit", Test_promptfit},
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
