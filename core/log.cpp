#include "log.h"

#include <cstdarg>

static FILE* g_log = nullptr;
static CRITICAL_SECTION g_logLock;
static bool g_lockReady = false;
static std::string g_dllDir;
static void (*g_sink)(const char* line) = nullptr;

void LogInit(HMODULE self, const char* fileName)
{
    InitializeCriticalSection(&g_logLock);
    g_lockReady = true;

    char path[MAX_PATH] = {};
    GetModuleFileNameA(self, path, MAX_PATH);
    char* slash = strrchr(path, '\\');
    if (slash)
        slash[1] = 0;
    g_dllDir = path;

    std::string logPath = g_dllDir + fileName;
    g_log = _fsopen(logPath.c_str(), "w", _SH_DENYWR);
}

void LogSetSink(void (*sink)(const char* line), const char* dllDir)
{
    g_sink = sink;
    g_dllDir = dllDir ? dllDir : "";
}

void LogWriteLine(const char* line)
{
    if (!g_log || !g_lockReady)
        return;
    EnterCriticalSection(&g_logLock);
    SYSTEMTIME st;
    GetLocalTime(&st);
    fprintf(g_log, "[%02d:%02d:%02d.%03d] %s\n", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, line);
    fflush(g_log);
    LeaveCriticalSection(&g_logLock);
}

void Log(const char* fmt, ...)
{
    char buf[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    buf[sizeof(buf) - 1] = 0;

    if (g_sink)
        g_sink(buf);
    else
        LogWriteLine(buf);
}

const std::string& DllDir() { return g_dllDir; }

// ---------------------------------------------------------------- perf (log.h)
namespace perf
{
namespace
{
struct Entry
{
    const char* name = nullptr;
    double total = 0, worst = 0; // seconds since the last report; the longest single call
    long calls = 0;
};
struct State
{
    Entry entries[32];
    int used = 0;
    int depth = 0;       // open scopes
    double frameAcc = 0; // this frame's outermost scopes so far
    double worstFrame = 0;
    long frames = 0;
    double since = 0;
};
State g_perf;
} // namespace

double Now()
{
    static LARGE_INTEGER freq = {};
    if (!freq.QuadPart)
        QueryPerformanceFrequency(&freq);
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return static_cast<double>(t.QuadPart) / static_cast<double>(freq.QuadPart);
}

static int Slot(const char* name)
{
    for (int i = 0; i < g_perf.used; ++i)
        if (g_perf.entries[i].name == name || strcmp(g_perf.entries[i].name, name) == 0)
            return i;
    if (g_perf.used >= 32)
        return -1;
    g_perf.entries[g_perf.used].name = name;
    return g_perf.used++;
}

Scope::Scope(const char* name) : slot_(Slot(name)), t0_(Now()) { ++g_perf.depth; }

Scope::~Scope()
{
    const double d = Now() - t0_;
    if (g_perf.depth > 0)
        --g_perf.depth;
    if (g_perf.depth == 0)
        g_perf.frameAcc += d;
    if (slot_ < 0 || slot_ >= g_perf.used) // a Report() while this scope was open reset the table
        return;
    Entry& e = g_perf.entries[slot_];
    e.total += d;
    e.worst = d > e.worst ? d : e.worst;
    ++e.calls;
}

void NewFrame()
{
    if (g_perf.since == 0)
        g_perf.since = Now();
    if (g_perf.frameAcc > g_perf.worstFrame)
        g_perf.worstFrame = g_perf.frameAcc;
    g_perf.frameAcc = 0;
    g_perf.depth = 0; // called outside every scope: a scope a fault skipped (SEH, no unwinding) is not open any more
    ++g_perf.frames;
}

std::string Report()
{
    const double now = Now();
    const double span = g_perf.since > 0 ? now - g_perf.since : 0;
    const double f = static_cast<double>(g_perf.frames > 0 ? g_perf.frames : 1);
    char line[200];
    sprintf_s(line, "frames %ld over %.1f s (%.0f fps); worst frame (outermost scopes) %.1f us\n", g_perf.frames, span,
              span > 0.001 ? g_perf.frames / span : 0.0, g_perf.worstFrame * 1e6);
    std::string s = line;
    for (int i = 0; i < g_perf.used; ++i)
    {
        const Entry& e = g_perf.entries[i];
        sprintf_s(line, "  %-18s %8.1f us/frame  %6ld calls  worst call %8.1f us\n", e.name, e.total / f * 1e6, e.calls,
                  e.worst * 1e6);
        s += line;
    }
    if (!g_perf.used)
        s += "  (no scope ran)\n";
    const int depth = g_perf.depth; // a report from inside a scope keeps the open count
    g_perf = State();
    g_perf.depth = depth;
    g_perf.since = now;
    return s;
}
} // namespace perf
