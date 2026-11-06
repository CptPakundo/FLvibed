// Log channels (a file of the host's or a sink of the logic's), the plain Log(), and the perf:: CPU time tables.
#include "log.h"

#include <cstdarg>

// A channel is a file of the host's (LogOpen) or a sink of the logic's (LogAttach).
struct LogChannel
{
    void (*sink)(const char* line);
    FILE* file;
};
static LogChannel g_channels[4] = {};
static int g_channelCount = 0;
static int g_plain = -1; // the plain Log()'s file: the one opened last
static CRITICAL_SECTION g_logLock;
static bool g_lockReady = false;
static std::string g_dllDir;
static void (*g_sink)(const char* line) = nullptr;

int LogOpen(HMODULE self, const char* fileName)
{
    if (!g_lockReady)
    {
        InitializeCriticalSection(&g_logLock);
        g_lockReady = true;
    }
    char path[MAX_PATH] = {};
    GetModuleFileNameA(self, path, MAX_PATH);
    char* slash = strrchr(path, '\\');
    if (slash)
        slash[1] = 0;
    g_dllDir = path;

    if (g_channelCount >= 4)
        return -1;
    const std::string logPath = g_dllDir + fileName;
    g_channels[g_channelCount] = {nullptr, _fsopen(logPath.c_str(), "w", _SH_DENYWR)};
    g_plain = g_channelCount;
    return g_channelCount++;
}

void LogSetSink(void (*sink)(const char* line), const char* dllDir)
{
    g_sink = sink;
    g_dllDir = dllDir ? dllDir : "";
}

int LogAttach(void (*sink)(const char* line), const char* dllDir)
{
    LogSetSink(sink, dllDir);
    for (int i = 0; i < g_channelCount; ++i)
        if (g_channels[i].sink == sink)
            return i;
    if (g_channelCount >= 4)
        return -1;
    g_channels[g_channelCount] = {sink, nullptr};
    return g_channelCount++;
}

int LogChannelOf(void (*sink)(const char* line))
{
    for (int i = 0; i < g_channelCount; ++i)
        if (g_channels[i].sink == sink)
            return i;
    return -1;
}

void LogTo(int channel, const char* line)
{
    if (channel < 0 || channel >= g_channelCount)
        return;
    const LogChannel& c = g_channels[channel];
    if (c.sink)
    {
        c.sink(line);
        return;
    }
    if (!c.file || !g_lockReady)
        return;
    EnterCriticalSection(&g_logLock);
    SYSTEMTIME st;
    GetLocalTime(&st);
    fprintf(c.file, "[%02d:%02d:%02d.%03d] %s\n", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, line);
    fflush(c.file);
    LeaveCriticalSection(&g_logLock);
}

void LogWriteLine(const char* line) { LogTo(g_plain, line); }

void LogV(int channel, const char* fmt, va_list args)
{
    char buf[2048];
    vsnprintf(buf, sizeof(buf), fmt, args);
    buf[sizeof(buf) - 1] = 0;

    if (channel >= 0 && channel < g_channelCount)
        LogTo(channel, buf);
    else if (g_sink)
        g_sink(buf);
    else
        LogWriteLine(buf);
}

void Log(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    LogV(-1, fmt, args);
    va_end(args);
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
    // The order the scopes opened in, for the fullest frame since the last report (`order:` in the report): the
    // refactor's check that the modules' entry points keep their sequence (tools\fingerprint.ps1).
    const char* order[48] = {};
    int orderCount = 0;
    const char* fullest[48] = {};
    int fullestCount = 0;
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

Scope::Scope(const char* name) : slot_(Slot(name)), t0_(Now())
{
    ++g_perf.depth;
    if (g_perf.orderCount < 48)
        g_perf.order[g_perf.orderCount++] = name;
}

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
    if (g_perf.orderCount > g_perf.fullestCount)
    {
        g_perf.fullestCount = g_perf.orderCount;
        memcpy(g_perf.fullest, g_perf.order, sizeof(g_perf.order));
    }
    g_perf.orderCount = 0;
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
    s += "order:";
    for (int i = 0; i < g_perf.fullestCount; ++i)
        s += std::string(i ? " > " : " ") + g_perf.fullest[i];
    s += "\n";
    const int depth = g_perf.depth; // a report from inside a scope keeps the open count
    g_perf = State();
    g_perf.depth = depth;
    g_perf.since = now;
    return s;
}
} // namespace perf
