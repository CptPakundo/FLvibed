// The Movies module's host part: a client of the router's movie event, and the module's logic. Core headers and `Log`
// come from build.cmd's :wrappart.
#include "../api.h"
#include "loader.h"

extern "C" const SettingsBus* flv_settings_bus();

static MoviesHostApi g_host = {};
static const MoviesImplApi* g_impl = nullptr;
static bool g_opened = false;
static bool g_faulted = false;

static void HostLogWrite(const char* line) { LogTo(g_logChannel, line); }

static void Fault(const char* where, DWORD code)
{
    g_faulted = true;
    Log("FATAL: exception 0x%08X in the movies' %s; they play as the game plays them until reload", code, where);
}

static void StopLogic()
{
    if (g_impl)
    {
        __try
        {
            g_impl->Shutdown();
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            Log("movies: Shutdown faulted");
        }
    }
    g_impl = nullptr;
}

static bool StartLogic(const void* table)
{
    const MoviesImplApi* api = static_cast<const MoviesImplApi*>(table);
    if (!api || api->version != kMoviesApiVersion)
    {
        Log("movies: incompatible logic");
        return false;
    }
    g_impl = api;
    g_faulted = false;
    __try
    {
        g_impl->Init(&g_host);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        Fault("init", GetExceptionCode());
        return false;
    }
    return true;
}

static void GuardedMovie(int event, void* hwnd, const char* path)
{
    __try
    {
        if (!g_opened)
        {
            g_opened = true;
            StartLogic(flv_logic_loader()->Open(kLogicMovies));
        }
        // a faulted logic still gets the stop: its teardown (the windows, the Sleep import, the graph's references)
        if (g_impl && (!g_faulted || event == 2))
            g_impl->OnMovie(event, hwnd, path);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        Fault("movie", GetExceptionCode());
    }
}

void Start()
{
    const int home = router::LogChannel(router::kSubtitles) >= 0 ? router::kSubtitles : router::kController;
    g_logChannel = router::LogChannel(home);
    hk::Owner own(router::OwnerOf(home));
    g_host.version = kMoviesApiVersion;
    g_host.ModuleOn = router::ModuleOn;
    g_host.dllDir = DllDir().c_str();
    g_host.LogWrite = HostLogWrite;
    g_host.GetSettingsBus = flv_settings_bus;
    g_host.GetMovieGraph = moviehooks::Graph;
    g_host.SkipMovie = moviehooks::Skip;
    static const LogicClient client = {StopLogic, StartLogic, HostLogWrite};
    flv_logic_loader()->Register(kLogicMovies, &client);
    static router::Client events = {};
    events.movie = GuardedMovie;
    router::Register(router::kMovies, &events);
    moviehooks::Install(); // (the subtitles' part installed them already when it is on)
}
