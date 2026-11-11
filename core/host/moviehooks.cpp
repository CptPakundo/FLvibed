// The movie event (moviehooks.h). MediaPlayer::PlayMovie(HWND, bool, const char* path) returns long; UpdateMovie() returns bool (still
// playing); StopMovie(). The startup movies play before the first frame update: a client opens its logic at its first event.
#include "all.h"

namespace moviehooks
{
typedef long(__cdecl* PlayMovieFn)(HWND, bool, const char*);
typedef bool(__cdecl* UpdateMovieFn)();
typedef void(__cdecl* StopMovieFn)();
typedef HRESULT(WINAPI* CoCreateFn)(REFCLSID, void*, DWORD, REFIID, void**);
static PlayMovieFn g_origPlayMovie = nullptr;
static UpdateMovieFn g_origUpdateMovie = nullptr;
static StopMovieFn g_origStopMovie = nullptr;
static CoCreateFn g_origCoCreate = nullptr;
static bool g_installed = false;
static bool g_moviePlaying = false; // a movie runs (PlayMovie .. UpdateMovie false / StopMovie)
static bool g_skipMovie = false;    // the next UpdateMovie reports the movie finished
static void* g_movieGraph = nullptr;
static const GUID kClsidFilterGraph = {0xE436EBB3, 0x524F, 0x11CE, {0x9F, 0x53, 0x00, 0x20, 0xAF, 0x0B, 0xA7, 0x70}};

// These lines stay with the movies' first home: the subtitles' log, the controller's without it.
static void Note(const char* fmt, ...)
{
    const int subtitles = router::LogChannel(router::kSubtitles);
    va_list args;
    va_start(args, fmt);
    LogV(subtitles >= 0 ? subtitles : router::LogChannel(router::kController), fmt, args);
    va_end(args);
}

static long __cdecl PlayMovieHook(HWND hwnd, bool b, const char* path)
{
    const long r = g_origPlayMovie(hwnd, b, path);
    g_moviePlaying = r >= 0;
    Note("PlayMovie(%s) -> %ld", path ? path : "", r);
    if (r >= 0) // a movie that failed to start never plays: no start event (the logic would keep it as current)
        router::MovieEvent(0, hwnd, path);
    return r;
}

// Keeps the filter graph, so the logic can set the video's destination rectangle (fit to screen).
static HRESULT WINAPI CoCreateHook(REFCLSID clsid, void* outer, DWORD ctx, REFIID iid, void** out)
{
    const HRESULT hr = g_origCoCreate(clsid, outer, ctx, iid, out);
    if (SUCCEEDED(hr) && out && *out && IsEqualGUID(clsid, kClsidFilterGraph))
        g_movieGraph = *out;
    return hr;
}

static bool __cdecl UpdateMovieHook()
{
    router::MoviePoll();
    // reported finished: the game's movie loop then ends it on its own path (no key input, works out of focus)
    const bool playing = g_skipMovie ? false : g_origUpdateMovie();
    g_skipMovie = false;
    g_moviePlaying = playing;
    router::MovieEvent(playing ? 1 : 2, nullptr, nullptr);
    return playing;
}

static void __cdecl StopMovieHook()
{
    g_moviePlaying = false;
    router::MovieEvent(2, nullptr, nullptr);
    g_origStopMovie();
    g_movieGraph = nullptr;
}

void* Graph() { return g_movieGraph; }

bool Skip()
{
    g_skipMovie = g_moviePlaying;
    return g_moviePlaying;
}

void SkipVerb(int, const char*, char* reply, size_t len) { strcpy_s(reply, len, Skip() ? "skipping movie" : "no movie playing"); }

void Install()
{
    if (g_installed)
        return;
    g_installed = true;
    HMODULE exe = GetModuleHandleA(nullptr);
    g_origPlayMovie = static_cast<PlayMovieFn>(hk::PatchImport(
        exe, "movie.dll", "?PlayMovie@MediaPlayer@@YAJPAUHWND__@@_NPBD@Z", reinterpret_cast<void*>(PlayMovieHook)));
    g_origUpdateMovie = static_cast<UpdateMovieFn>(hk::PatchImport(exe, "movie.dll", "?UpdateMovie@MediaPlayer@@YA_NXZ",
                                                                   reinterpret_cast<void*>(UpdateMovieHook)));
    g_origStopMovie = static_cast<StopMovieFn>(
        hk::PatchImport(exe, "movie.dll", "?StopMovie@MediaPlayer@@YAXXZ", reinterpret_cast<void*>(StopMovieHook)));
    g_origCoCreate = static_cast<CoCreateFn>(hk::PatchImport(
        GetModuleHandleA("movie.dll"), "ole32.dll", "CoCreateInstance", reinterpret_cast<void*>(CoCreateHook)));
    Note("Movie hooks: play=%p update=%p stop=%p", g_origPlayMovie, g_origUpdateMovie, g_origStopMovie);
}
} // namespace moviehooks
