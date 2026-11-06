// The log's channels (core\log.cpp, core\logic\module_log.h): one logic, each module's lines in its own file. What goes
// wrong without them is silent: a line in the wrong log, or none.
#include "log.h"

#include "test.h"

#include <cstdarg>

static std::string g_a, g_b;
static void SinkA(const char* line) { g_a += std::string(line) + ";"; }
static void SinkB(const char* line) { g_b += std::string(line) + ";"; }

static void Say(int channel, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    LogV(channel, fmt, args);
    va_end(args);
}

void Test_log_channel()
{
    const int a = LogAttach(SinkA, "C:\\dir\\");
    CHECK(DllDir() == "C:\\dir\\"); // ai.cpp and the data readers take the game's folder from here
    Log("core %d", 1);              // one module attached: core\game's lines go to it
    CHECK(g_a == "core 1;");

    const int b = LogAttach(SinkB, "C:\\dir\\");
    CHECK(a != b);
    Say(a, "subtitles %s", "line");
    Say(b, "controller %s", "line");
    Log("core %d", 2); // the channel attached last
    CHECK(g_a == "core 1;subtitles line;");
    CHECK(g_b == "controller line;core 2;");

    // a reload attaches again: the same channels, nothing grows
    CHECK(LogAttach(SinkA, "C:\\dir\\") == a);
    CHECK(LogAttach(SinkB, "C:\\dir\\") == b);
    g_a.clear(), g_b.clear();
    Say(a, "after");
    CHECK(g_a == "after;");
    CHECK(g_b.empty());
}
