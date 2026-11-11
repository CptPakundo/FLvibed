// The Movies module's logic: which picture area a movie has, the fit to the screen, the native skip, and the slices of the movie
// loop's wait. The window over the video is core's (core\game\moviescreen.h); the lines over it are the Subtitles module's.
// Its rows (`movie_fit`, `movie_skip`) are in the subtitles' registry: read through the bus while that is on, from FLSubtitles.ini
// without it. Which intro plays is the install's own choice (HD Edition's installer; issue 80 removed `movie_intro`).
#include "../api.h"
#include "flexe.h"
#include "game/moviescreen.h"
#include "hook.h"
#include "log.h"
#include "modules_rules.h"
#include "ui/shared_rows.h"

#include <fstream>
#include <vector>

static const MoviesHostApi* g_host = nullptr;
static bool g_on = false;   // this logic saw the movie start: its own teardown is owed, whatever moviescreen's state
static std::string g_id;
static bool g_iniFit = true;
static int g_iniSkip = 0;

static int Row(const char* key, int fallback)
{
    const SettingsBus* bus = g_host->GetSettingsBus ? g_host->GetSettingsBus() : nullptr;
    char v[32];
    return bus && bus->Get("Subtitles", key, v, sizeof(v)) ? atoi(v) : fallback;
}

static std::vector<std::string> SplitTabs(const std::string& line)
{
    std::vector<std::string> out(1);
    for (char c : line)
        if (c == '\t')
            out.emplace_back();
        else
            out.back() += c;
    return out;
}

// ------------------------------------------------------------------ fitting the video to the screen
// Movie.dll stretches the video over the whole game window. We fit it instead: aspect ratio kept, and the black
// bars encoded in a video (data/movie_frames.tsv) zoomed out of view. Done inside the DirectShow renderer only
// (IBasicVideo source and destination rectangles): the renderer window keeps the size Movie.dll gave it (the game
// window's client area), so screen and video capture tools never see a size change. The overlay paints the
// margins around the destination black.
struct Frame
{
    int w = 0, h = 0;
    float par = 1;
    int cx = 0, cy = 0, cw = 0, ch = 0; // picture area (no bars)
};
static std::vector<std::pair<std::string, Frame>> g_frames;
static Frame g_frame; // of the movie playing
static void* g_basicVideo = nullptr; // IBasicVideo of the playing movie's filter graph (AddRef'd)

static void LoadFrames(const std::string& path)
{
    g_frames.clear();
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        Log("movie: cannot open %s (movies fit by their whole frame)", path.c_str());
        return;
    }
    std::string l;
    bool header = false;
    size_t skipped = 0;
    while (std::getline(in, l))
    {
        if (!l.empty() && l.back() == '\r')
            l.pop_back();
        if (l.compare(0, 3, "\xEF\xBB\xBF") == 0) // a UTF-8 BOM (an editor's save) must not break the header
            l.erase(0, 3);
        if (l.empty() || l[0] == '#')
            continue;
        if (!header)
        {
            header = true; // another column order would crop the wrong rectangle
            const std::vector<std::string> h = SplitTabs(l);
            if (h.size() < 8 || h[0] != "movie" || h[1] != "width" || h[2] != "height" || h[3] != "par" || h[4] != "crop_x" ||
                h[5] != "crop_y" || h[6] != "crop_w" || h[7] != "crop_h")
            {
                Log("movie: unexpected header in %s (movies fit by their whole frame)", path.c_str());
                return;
            }
            continue;
        }
        // movie width height par crop_x crop_y crop_w crop_h; a row with a field that is not a number is skipped
        const std::vector<std::string> t = SplitTabs(l);
        auto num = [&](size_t i, long& out)
        {
            char* end = nullptr;
            out = strtol(t[i].c_str(), &end, 10);
            return end != t[i].c_str() && !*end;
        };
        long v[8] = {};
        bool ok = t.size() >= 8 && !t[0].empty();
        for (const size_t i : {1u, 2u, 4u, 5u, 6u, 7u})
            ok = ok && num(i, v[i]);
        char* pend = nullptr;
        Frame f;
        f.par = t.size() > 3 ? strtof(t[3].c_str(), &pend) : 0;
        // sizes, the crop and the pixel aspect must be positive (FitVideo divides by them)
        if (ok && pend != t[3].c_str() && !*pend && f.par > 0 && v[1] > 0 && v[2] > 0 && v[6] > 0 && v[7] > 0)
        {
            f.w = v[1], f.h = v[2];
            f.cx = v[4], f.cy = v[5], f.cw = v[6], f.ch = v[7];
            g_frames.push_back({t[0].substr(0, 63), f});
        }
        else
            ++skipped;
    }
    Log("movie: %zu frame entries (%zu malformed rows skipped)", g_frames.size(), skipped);
}

static void ReleaseBasicVideo()
{
    if (g_basicVideo)
        VCall<ULONG>(g_basicVideo, 2);
    g_basicVideo = nullptr;
}

// IBasicVideo {56A868B5-0AD4-11CE-B03A-0020AF0BA770} (IDispatch-based): vt[28] SetSourcePosition(l, t, w, h),
// vt[31] SetDestinationPosition(l, t, w, h), vt[32] GetDestinationPosition(&l, &t, &w, &h), vt[34]
// GetVideoSize(&w, &h). Source in native video pixels, destination in renderer window pixels.
static void FitVideo()
{
    if (g_frame.w <= 0 || g_frame.h <= 0 || !Row("movie_fit", g_iniFit)) // movie_fit 0: as Movie.dll draws it
        return;
    const HWND video = moviescreen::Video();
    if (!video)
        return;
    if (!g_basicVideo)
    {
        void* graph = g_host->GetMovieGraph();
        static const GUID kIidBasicVideo = {
            0x56A868B5, 0x0AD4, 0x11CE, {0xB0, 0x3A, 0x00, 0x20, 0xAF, 0x0B, 0xA7, 0x70}};
        if (!graph || FAILED(VCall<HRESULT>(graph, 0, &kIidBasicVideo, &g_basicVideo)) || !g_basicVideo)
        {
            g_basicVideo = nullptr;
            return;
        }
    }
    RECT rc;
    if (!GetClientRect(video, &rc) || rc.right <= 0 || rc.bottom <= 0)
        return;
    const float W = static_cast<float>(rc.right), H = static_cast<float>(rc.bottom);
    // picture area (no bars) in native video pixels: movie_frames.tsv is measured on the encoded frame size
    long nw = 0, nh = 0;
    VCall<HRESULT>(g_basicVideo, 34, &nw, &nh);
    const float kx = nw > 0 ? static_cast<float>(nw) / g_frame.w : 1.f,
                ky = nh > 0 ? static_cast<float>(nh) / g_frame.h : 1.f;
    const long sl = static_cast<long>(g_frame.cx * kx + 0.5f), st = static_cast<long>(g_frame.cy * ky + 0.5f);
    const long sw = static_cast<long>(g_frame.cw * kx + 0.5f), sh = static_cast<long>(g_frame.ch * ky + 0.5f);
    // largest centred rectangle of the picture's aspect (square pixels: par) inside the window
    const float aspect = g_frame.cw * g_frame.par / g_frame.ch;
    const float dw = std::min(W, H * aspect), dh = dw / aspect;
    const long dl = static_cast<long>((W - dw) * 0.5f + 0.5f), dt = static_cast<long>((H - dh) * 0.5f + 0.5f);
    const long dwi = static_cast<long>(dw + 0.5f), dhi = static_cast<long>(dh + 0.5f);
    // apply when the renderer's destination is not ours (first frames, or Movie.dll / a window size change reset it)
    long cl = 0, ct = 0, cwi = 0, chi = 0;
    VCall<HRESULT>(g_basicVideo, 32, &cl, &ct, &cwi, &chi);
    if (cl == dl && ct == dt && cwi == dwi && chi == dhi)
        return;
    const HRESULT hs = VCall<HRESULT>(g_basicVideo, 28, sl, st, sw, sh);
    const HRESULT hd = VCall<HRESULT>(g_basicVideo, 31, dl, dt, dwi, dhi);
    moviescreen::SetPicture(RECT{dl, dt, dl + dwi, dt + dhi}, RECT{sl, st, sl + sw, st + sh});
    static int logged = 0;
    if (logged++ < 8)
        Log("movie: fit in %dx%d window: source %ld,%ld %ldx%ld of %ldx%ld (0x%08X), dest %ld,%ld %ldx%ld (was %ld,%ld "
            "%ldx%ld) (0x%08X)",
            rc.right, rc.bottom, sl, st, sw, sh, nw, nh, hs, dl, dt, dwi, dhi, cl, ct, cwi, chi, hd);
}

static bool g_graphLogged = false;
static void* g_qualProp = nullptr; // the video renderer's IQualProp (AddRef'd; released at the movie's stop)
static void* g_audioStats = nullptr; // the audio renderer's IAMAudioRendererStats (AddRef'd; released at the stop)

// AM_AUDREN_STAT_PARAM_*: 1 break count, 3 silence duration (ms), 5 discontinuities
static DWORD AudioStat(DWORD param)
{
    DWORD v = 0, unused = 0;
    if (!g_audioStats || FAILED(VCall<HRESULT>(g_audioStats, 3, param, &v, &unused)))
        return static_cast<DWORD>(-1);
    return v;
}

// The filters of the movie's graph, once per movie, keeping the renderers' figures for the stop line (which video renderer plays it: docs\DISPLAY_PLAN.md "Movies"), and
// whether a renderer offers a mixer bitmap (IVMRMixerBitmap / 9: lines blended into the video itself).
// IFilterGraph vt[5] EnumFilters; IEnumFilters vt[3] Next; IBaseFilter vt[3] GetClassID, vt[12] QueryFilterInfo.
static void LogGraph()
{
    if (g_graphLogged)
        return;
    void* graph = g_host->GetMovieGraph();
    if (!graph)
        return;
    g_graphLogged = true;
    static const GUID kIidFilterGraph = {0x56A8689F, 0x0AD4, 0x11CE, {0xB0, 0x3A, 0x00, 0x20, 0xAF, 0x0B, 0xA7, 0x70}};
    static const GUID kIidMixerBitmap = {0x1E673275, 0x0257, 0x40AA, {0xAF, 0x20, 0x7C, 0x60, 0x8D, 0x4A, 0x04, 0x28}};
    static const GUID kIidMixerBitmap9 = {0xCED175E5, 0x1935, 0x4820, {0x81, 0xBD, 0xFF, 0x6A, 0xD0, 0x0C, 0x91, 0x08}};
    void* fg = nullptr;
    if (FAILED(VCall<HRESULT>(graph, 0, &kIidFilterGraph, &fg)) || !fg)
        return;
    void* en = nullptr;
    if (SUCCEEDED(VCall<HRESULT>(fg, 5, &en)) && en)
    {
        void* f = nullptr;
        ULONG got = 0;
        for (int guard = 0; guard < 16 && VCall<HRESULT>(en, 3, 1ul, &f, &got) == S_OK && f; ++guard, f = nullptr)
        {
            GUID clsid = {};
            VCall<HRESULT>(f, 3, &clsid);
            struct
            {
                wchar_t name[128];
                void* graph;
            } info = {};
            if (SUCCEEDED(VCall<HRESULT>(f, 12, &info)) && info.graph)
                VCall<ULONG>(info.graph, 2);
            void* mb = nullptr;
            const bool mixer = SUCCEEDED(VCall<HRESULT>(f, 0, &kIidMixerBitmap, &mb)) && mb;
            if (mb)
                VCall<ULONG>(mb, 2), mb = nullptr;
            const bool mixer9 = SUCCEEDED(VCall<HRESULT>(f, 0, &kIidMixerBitmap9, &mb)) && mb;
            if (mb)
                VCall<ULONG>(mb, 2);
            // the renderer's quality figures (IQualProp: vt[3] frames dropped, vt[4] frames drawn) for the stop line
            static const GUID kIidQualProp = {0x1BD0ECB0, 0xF8E2, 0x11CE, {0xAA, 0xC6, 0x00, 0x20, 0xAF, 0x0B, 0x99, 0xA3}};
            void* q = nullptr;
            if (!g_qualProp && SUCCEEDED(VCall<HRESULT>(f, 0, &kIidQualProp, &q)) && q)
                g_qualProp = q;
            else if (q)
                VCall<ULONG>(q, 2);
            // the audio renderer's figures (IAMAudioRendererStats vt[3] GetStatParam) for the stop line: tests are muted, these
            // count a choppy intro
            static const GUID kIidAudioStats = {0x22320CB2, 0xD41A, 0x11D2, {0xBF, 0x7C, 0xD7, 0xCB, 0x9D, 0xF0, 0xBF, 0x93}};
            void* a = nullptr;
            if (!g_audioStats && SUCCEEDED(VCall<HRESULT>(f, 0, &kIidAudioStats, &a)) && a)
                g_audioStats = a;
            else if (a)
                VCall<ULONG>(a, 2);
            Log("movie: graph filter \"%ls\" {%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}%s%s", info.name,
                clsid.Data1, clsid.Data2, clsid.Data3, clsid.Data4[0], clsid.Data4[1], clsid.Data4[2], clsid.Data4[3],
                clsid.Data4[4], clsid.Data4[5], clsid.Data4[6], clsid.Data4[7], mixer ? " IVMRMixerBitmap" : "",
                mixer9 ? " IVMRMixerBitmap9" : "");
            VCall<ULONG>(f, 2);
        }
        VCall<ULONG>(en, 2);
    }
    VCall<ULONG>(fg, 2);
}

// movie_skip 1: the startup movies end the way the game ends a finished one; no key input, works out of focus.
static bool g_skipRequested = false;
static void RequestNativeSkip()
{
    if (g_skipRequested)
        return;
    g_skipRequested = g_host->SkipMovie();
    if (g_skipRequested)
        Log("movie: %s skipped (movie_skip 1)", moviescreen::Current().id.c_str());
}

// Movie.dll's UpdateMovie starts with Sleep(300) (Movie.dll 0x10001730: push 12Ch; call [Sleep]): the movie loop runs
// ~3 passes a second while the video plays on DirectShow's own thread. Its import of Sleep is chained while a movie
// plays: the same total wait, cut into slices - one composition frame each (DwmFlush) while a transition runs, else
// 50 ms - with a Tick of moviescreen between them. The loop's timing and the video's are untouched.
using SleepFn = void(WINAPI*)(DWORD);
static SleepFn g_prevSleep = nullptr;
static void** g_sleepSlot = nullptr;
static HRESULT(WINAPI* g_dwmFlush)() = nullptr;
static bool g_inSleep = false;
// Windows 7 with Aero off (the Basic theme): DWM is not composing and DwmFlush fails at once (FLVIBED_SIM_WIN7)
static HRESULT WINAPI DwmFlushNoComposition() { return static_cast<HRESULT>(0x80263001L); } // DWM_E_COMPOSITIONDISABLED

static void WINAPI MovieSleep(DWORD ms)
{
    SleepFn prev = g_prevSleep ? g_prevSleep : ::Sleep;
    if (!moviescreen::Playing() || g_inSleep || ms < 20 || ms > 2000)
        return prev(ms);
    g_inSleep = true;
    const double end = moviescreen::Seconds() + ms / 1000.0;
    for (;;)
    {
        moviescreen::Tick();
        const double refresh = moviescreen::RefreshSeconds();
        const bool animating = moviescreen::Animating();
        const double left = end - moviescreen::Seconds();
        if (left <= 0.002)
            break;
        if (animating && g_dwmFlush && left > refresh * 0.5)
        {
            if (FAILED(g_dwmFlush()))
                prev(static_cast<DWORD>(std::max(1.0, std::min(left, refresh) * 1000.0)));
        }
        else
            prev(static_cast<DWORD>(std::max(1.0, std::min(left, animating ? refresh : 0.05) * 1000.0)));
    }
    g_inSleep = false;
}

static void HookSleep(bool on)
{
    if (on && !g_sleepSlot)
    {
        if (!g_dwmFlush && SimWin7())
            g_dwmFlush = DwmFlushNoComposition;
        if (!g_dwmFlush)
            if (HMODULE dwm = LoadLibraryW(L"dwmapi.dll"))
                g_dwmFlush = reinterpret_cast<HRESULT(WINAPI*)()>(GetProcAddress(dwm, "DwmFlush"));
        HMODULE m = GetModuleHandleW(L"movie.dll");
        void** slot = m ? hk::ImportSlot(m, "kernel32.dll", "Sleep") : nullptr;
        if (!slot)
        {
            Log("movie: Movie.dll imports no Sleep - what is drawn over a movie updates once per movie loop pass");
            return;
        }
        g_prevSleep = static_cast<SleepFn>(hk::PatchImport(m, "kernel32.dll", "Sleep", reinterpret_cast<void*>(MovieSleep)));
        g_sleepSlot = slot;
    }
    else if (!on && g_sleepSlot)
    {
        // put the previous one back only while the slot still holds ours (another plugin chained after us keeps it;
        // ours then passes through: no movie)
        if (*g_sleepSlot == reinterpret_cast<void*>(MovieSleep) && g_prevSleep)
        {
            void* p = reinterpret_cast<void*>(g_prevSleep);
            hk::Write(reinterpret_cast<uintptr_t>(g_sleepSlot), &p, sizeof(p));
            g_sleepSlot = nullptr;
        }
    }
}

static void OnStart(HWND hwnd, const char* path)
{
    moviescreen::Begin(hwnd, path);
    ReleaseBasicVideo();
    const moviescreen::Info& info = moviescreen::Current();
    g_on = true, g_id = info.id;
    g_frame = Frame();
    g_frame.w = info.w, g_frame.h = info.h, g_frame.cw = info.w, g_frame.ch = info.h; // no bar data: fit the whole frame
    for (const auto& f : g_frames)
        if (f.first == info.id && f.second.w == g_frame.w && f.second.h == g_frame.h)
            g_frame = f.second; // known picture area (bars zoomed out of view)
    Log("movie: %s picture %dx%d", info.id.c_str(), g_frame.cw, g_frame.ch);
    g_skipRequested = false, g_graphLogged = false;
    HookSleep(true);
}

static void OnUpdate()
{
    if (!g_on)
        return;
    if (Row("movie_skip", g_iniSkip))
        RequestNativeSkip(); // takes effect at the next movie loop pass
    FitVideo();
    LogGraph();
    if (!moviescreen::content.Lines)
        moviescreen::Pass(); // with the subtitles on, their event follows this one and makes the pass
}

static void OnStop()
{
    if (!g_on)
        return;
    g_on = false;
    const double t = moviescreen::Clock();
    int dropped = -1, drawn = -1;
    if (g_qualProp)
    {
        VCall<HRESULT>(g_qualProp, 3, &dropped), VCall<HRESULT>(g_qualProp, 4, &drawn);
        VCall<ULONG>(g_qualProp, 2);
        g_qualProp = nullptr;
    }
    const long breaks = static_cast<long>(AudioStat(1)), silenceMs = static_cast<long>(AudioStat(3)),
               discont = static_cast<long>(AudioStat(5));
    if (g_audioStats)
        VCall<ULONG>(g_audioStats, 2), g_audioStats = nullptr;
    Log("movie: %s stopped after %.1f s (path %s; loop %.1f passes/s, refresh %.0f Hz; %u redraws, %u window paints; %s; "
        "renderer frames drawn %d, dropped %d; audio breaks %ld, discontinuities %ld, silence %ld ms)",
        g_id.c_str(), t, moviescreen::PathName(), t > 0 ? moviescreen::Passes() / t : 0.0,
        1.0 / moviescreen::RefreshSeconds(), moviescreen::Redraws(), moviescreen::Paints(), moviescreen::GapReport().c_str(), drawn,
        dropped, breaks, discont, silenceMs);
    ReleaseBasicVideo();
    HookSleep(false);
    moviescreen::End(); // here, not left to the subtitles: their logic may have faulted
}

// With the subtitles off their registry is gone: the rows' home is here, so the page shows them live and the file keeps them.
static const settings::Def kRows[] = {FLV_ROW_MOVIE_FIT(&g_iniFit) FLV_ROW_MOVIE_SKIP(&g_iniSkip)};
static SettingsBusSection g_section;
static int g_busId = 0;
static const SettingsBus* Bus() { return g_host->GetSettingsBus ? g_host->GetSettingsBus() : nullptr; }

static void RowsSave(void*)
{
    const std::string ini = std::string(g_host->dllDir) + "FLSubtitles.ini";
    for (const settings::Def& d : kRows)
        WritePrivateProfileStringA("Subtitles", d.name, settings::Value(d).c_str(), ini.c_str());
}

static void ImplInit(const MoviesHostApi* host)
{
    g_host = host;
    LogSetSink(host->LogWrite, host->dllDir);
    exe::ResolveAll("movies logic");
    if (const SettingsBus* bus = Bus(); bus && !host->ModuleOn(modules::kSubtitles)) // (before the load: the bus's defaults are the values as they start)
    {
        g_section = {"movies", "Subtitles", kRows, sizeof(kRows) / sizeof(kRows[0]), nullptr, nullptr, RowsSave, nullptr};
        g_busId = bus->Register(&g_section);
    }
    const std::string ini = std::string(host->dllDir) + "FLSubtitles.ini";
    g_iniFit = GetPrivateProfileIntA("Subtitles", "movie_fit", 1, ini.c_str()) != 0;
    g_iniSkip = static_cast<int>(GetPrivateProfileIntA("Subtitles", "movie_skip", 0, ini.c_str()));
    LoadFrames(std::string(host->dllDir) + "FLSubtitles\\movie_frames.tsv");
}

// The logic goes (a reload, an unload): a movie keeps playing, as Movie.dll draws it, and nothing of this code stays behind in the
// process: the Sleep import, and (moviescreen) the windows and their classes.
static void ImplShutdown()
{
    if (const SettingsBus* bus = Bus(); bus && g_busId)
        bus->Unregister(g_busId);
    g_busId = 0;
    OnStop();
    moviescreen::Shutdown();
}

static void ImplOnMovie(int event, void* hwnd, const char* path)
{
    if (event == 0)
        OnStart(static_cast<HWND>(hwnd), path);
    else if (event == 1)
        OnUpdate();
    else
        OnStop();
}

static const MoviesImplApi g_api = {kMoviesApiVersion, ImplInit, ImplShutdown, ImplOnMovie};

extern "C" const void* flv_movies_impl_api() { return &g_api; }
