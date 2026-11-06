// The logic's loader (core\loader.cpp): who is stopped and started by a reload, and in which order. Its release form
// (FLV_STATIC: no file, a table from flv_logic_api), so the test needs no DLL.
#define FLV_STATIC 1
#include "../core/loader.cpp"

#include "test.h"

static std::string g_trace;
static int g_tableA = 0, g_tableB = 0;
static bool g_refuse = false;

extern "C" const LogicApi* flv_logic_api()
{
    static const LogicApi api = {kLogicApiVersion, {&g_tableA, &g_tableB}};
    return g_refuse ? nullptr : &api;
}

static void StopA() { g_trace += "stop A;"; }
static void StopB() { g_trace += "stop B;"; }
static bool StartA(const void* t)
{
    g_trace += t == &g_tableA ? "start A;" : "start A wrong table;";
    return true;
}
static bool StartB(const void* t)
{
    g_trace += t == &g_tableB ? "start B;" : "start B wrong table;";
    return true;
}

void Test_loader()
{
    static const LogicClient a = {StopA, StartA, nullptr}, b = {StopB, StartB, nullptr};
    const LogicLoader* l = flv_logic_loader();
    l->Register(kLogicSubtitles, &a);
    l->Register(kLogicController, &b);

    // staged: only the module that asked restarts (a reload during the intro leaves the controller to its first frame)
    CHECK(l->Open(kLogicSubtitles) == &g_tableA);
    g_trace.clear();
    CHECK(l->Reload());
    CHECK(g_trace == "stop A;start A;");

    // both started: last started stops first, they start in order, each with its own table
    CHECK(l->Open(kLogicController) == &g_tableB);
    g_trace.clear();
    CHECK(l->Reload());
    CHECK(g_trace == "stop B;stop A;start A;start B;");

    // an unload stops both; the next reload brings both back (a host's stop with nothing running does nothing)
    g_trace.clear();
    l->Unload();
    CHECK(g_trace == "stop B;stop A;");
    g_trace.clear();
    CHECK(l->Reload());
    CHECK(g_trace == "stop B;stop A;start A;start B;");

    // a logic that does not load: reported, nobody started, the modules stay off
    g_refuse = true;
    g_trace.clear();
    CHECK(!l->Reload());
    CHECK(g_trace == "stop B;stop A;");
    CHECK(l->Open(kLogicSubtitles) == nullptr);
    g_refuse = false;
    CHECK(l->Reload());
}
