#include "movie.h"

#include "../api.h"
#include "../shm.h"
#include "flexe.h"
#include "hook.h"
#include "log.h"
#include "data.h"
#include "display.h"
#include "movie_words.h"

extern const HostApi* g_host;

#include <fstream>
#include <vector>

namespace movie
{
struct Line
{
    std::string movie;
    float start, end;
    std::wstring speaker, text;
    unsigned textFnv = 0;          // FNV-1a of the UTF-8 text (the word_times.tsv row must match it)
    std::vector<float> wordStarts; // per word, seconds from the line's start (< 0 unknown); empty: estimated pages
};
static std::vector<Line> g_lines;
static void LoadWordTimes(const std::string& path);
static std::string g_current; // movie id playing, empty if none
static double g_t0 = 0;
static size_t g_next = 0; // next line to show
static HWND g_game = nullptr, g_overlay = nullptr;
static void LoadFrames(const std::string& path);
static double g_lastDraw = 0;

static double Now()
{
    static LARGE_INTEGER f = {};
    if (!f.QuadPart)
        QueryPerformanceFrequency(&f);
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return static_cast<double>(t.QuadPart) / f.QuadPart;
}

bool Load(const std::string& path)
{
    g_lines.clear();
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return false;
    std::string l;
    bool header = false;
    while (std::getline(in, l))
    {
        if (!l.empty() && l.back() == '\r')
            l.pop_back();
        if (l.empty() || l[0] == '#')
            continue;
        if (!header)
        {
            header = true; // movie start end speaker text note
            continue;
        }
        const std::vector<std::string> f = data::SplitTsv(l);
        if (f.size() < 5 || f[1].empty())
            continue;
        Line ln{f[0], static_cast<float>(atof(f[1].c_str())), static_cast<float>(atof(f[2].c_str())),
                data::Widen(f[3]), data::Widen(f[4])};
        ln.textFnv = moviewords::TextFnv(f[4]);
        g_lines.push_back(std::move(ln));
    }
    Log("movie: %zu lines from %s", g_lines.size(), path.c_str());
    const std::string dir = path.substr(0, path.find_last_of("\\/") + 1);
    LoadFrames(dir + "movie_frames.tsv");
    LoadWordTimes(dir + "word_times.tsv");
    return true;
}

// The intro lines' word starts (word_times.tsv rows with voice "movie": the review's forced alignment, times on the
// movie clock): a line pages on its words like dialogue (display.cpp); a line without a matching row (no row, or its
// text changed since the alignment) keeps the estimate.
static void LoadWordTimes(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return;
    size_t timed = 0, stale = 0;
    std::string l;
    bool head = false;
    while (std::getline(in, l))
    {
        if (!l.empty() && l.back() == '\r')
            l.pop_back();
        if (l.empty() || l[0] == '#')
            continue;
        if (!head) // id voice text_fnv audio_sha1 end_ms starts_ms collapsed
        {
            head = true;
            continue;
        }
        const size_t t1 = l.find('\t');
        if (t1 == std::string::npos || l.compare(t1 + 1, 6, "movie\t") != 0)
            continue; // a dialogue row (data.cpp)
        const std::vector<std::string> f = data::SplitTsv(l);
        if (f.size() < 6)
            continue;
        const unsigned fnv = static_cast<unsigned>(strtoul(f[2].c_str(), nullptr, 16));
        bool found = false;
        for (Line& ln : g_lines)
        {
            if (ln.movie != f[0] || ln.textFnv != fnv || !ln.wordStarts.empty())
                continue;
            found = true;
            if (moviewords::Relative(f[5], ln.start, moviewords::WordCount(ln.text), ln.wordStarts))
                ++timed;
            else
                ++stale;
            break;
        }
        stale += found ? 0 : 1;
    }
    Log("movie: word timings for %zu of %zu lines (%zu rows stale, ignored) from %s", timed, g_lines.size(), stale,
        path.c_str());
}

std::wstring Text()
{
    std::wstring all;
    for (const Line& ln : g_lines)
        all += ln.speaker + ln.text;
    return all;
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
static HWND g_video = nullptr;
static void* g_basicVideo = nullptr; // IBasicVideo of the playing movie's filter graph (AddRef'd)
static RECT g_dest = {};             // where the picture is drawn, in video window (= game client) pixels
static RECT g_src = {};              // the part of the video it shows (native video pixels: left, top, right, bottom)

static void LoadFrames(const std::string& path)
{
    g_frames.clear();
    std::ifstream in(path, std::ios::binary);
    std::string l;
    bool header = false;
    while (std::getline(in, l))
    {
        if (!l.empty() && l.back() == '\r')
            l.pop_back();
        if (l.empty() || l[0] == '#')
            continue;
        if (!header)
        {
            header = true;
            continue;
        }
        // movie width height par crop_x crop_y crop_w crop_h; a row with a field that is not a number is skipped
        const std::vector<std::string> t = data::SplitTsv(l);
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
        if (ok && pend != t[3].c_str() && !*pend)
        {
            f.w = v[1], f.h = v[2];
            f.cx = v[4], f.cy = v[5], f.cw = v[6], f.ch = v[7];
            g_frames.push_back({t[0].substr(0, 63), f});
        }
    }
    Log("movie: %zu frame entries", g_frames.size());
}

// Video frame size from the ASF Stream Properties Object {B7DC0791-A9B7-11CF-8EE6-00C00C205365} of a video
// stream ({BC19EFC0-5B4D-11CF-A8FD-00805F5C442B}): encoded width / height at +78 / +82.
static bool AsfFrameSize(HANDLE h, int& w, int& hgt)
{
    static const u8 kStreamProps[16] = {0x91, 0x07, 0xDC, 0xB7, 0xB7, 0xA9, 0xCF, 0x11,
                                        0x8E, 0xE6, 0x00, 0xC0, 0x0C, 0x20, 0x53, 0x65};
    static const u8 kVideo[16] = {0xC0, 0xEF, 0x19, 0xBC, 0x4D, 0x5B, 0xCF, 0x11,
                                  0xA8, 0xFD, 0x00, 0x80, 0x5F, 0x5C, 0x44, 0x2B};
    std::vector<u8> buf(64 * 1024);
    DWORD got = 0;
    SetFilePointer(h, 0, nullptr, FILE_BEGIN);
    if (!ReadFile(h, buf.data(), static_cast<DWORD>(buf.size()), &got, nullptr))
        return false;
    for (DWORD i = 0; i + 90 < got; ++i)
        if (memcmp(&buf[i], kStreamProps, 16) == 0 && memcmp(&buf[i + 24], kVideo, 16) == 0)
        {
            u32 ww = 0, hh = 0;
            memcpy(&ww, &buf[i + 78], 4);
            memcpy(&hh, &buf[i + 82], 4);
            if (ww > 0 && hh > 0 && ww < 16384 && hh < 16384)
            {
                w = static_cast<int>(ww), hgt = static_cast<int>(hh);
                return true;
            }
        }
    return false;
}

static BOOL CALLBACK FindVideoChild(HWND h, LPARAM out)
{
    if (h == g_overlay || !IsWindowVisible(h))
        return TRUE;
    *reinterpret_cast<HWND*>(out) = h;
    return FALSE;
}

static void ReleaseBasicVideo()
{
    if (g_basicVideo)
        VCall<ULONG>(g_basicVideo, 2);
    g_basicVideo = nullptr;
    g_dest = {};
}

// IBasicVideo {56A868B5-0AD4-11CE-B03A-0020AF0BA770} (IDispatch-based): vt[28] SetSourcePosition(l, t, w, h),
// vt[31] SetDestinationPosition(l, t, w, h), vt[32] GetDestinationPosition(&l, &t, &w, &h), vt[34]
// GetVideoSize(&w, &h). Source in native video pixels, destination in renderer window pixels.
static void FitVideo()
{
    if (!g_game || g_frame.w <= 0 || g_frame.h <= 0 || !display::Get().movieFit) // movie_fit 0: as Movie.dll draws it
        return;
    if (!g_video || !IsWindow(g_video))
    {
        g_video = nullptr;
        EnumChildWindows(g_game, FindVideoChild, reinterpret_cast<LPARAM>(&g_video));
        if (!g_video)
            return;
        char cls[64] = {};
        GetClassNameA(g_video, cls, sizeof(cls));
        Log("movie: video window %p (%s)", g_video, cls);
    }
    if (!g_basicVideo)
    {
        void* graph = g_host && g_host->version >= 3 ? g_host->GetMovieGraph() : nullptr;
        static const GUID kIidBasicVideo = {
            0x56A868B5, 0x0AD4, 0x11CE, {0xB0, 0x3A, 0x00, 0x20, 0xAF, 0x0B, 0xA7, 0x70}};
        if (!graph || FAILED(VCall<HRESULT>(graph, 0, &kIidBasicVideo, &g_basicVideo)) || !g_basicVideo)
        {
            g_basicVideo = nullptr;
            return;
        }
    }
    RECT rc;
    if (!GetClientRect(g_video, &rc) || rc.right <= 0 || rc.bottom <= 0)
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
    g_dest = {dl, dt, dl + dwi, dt + dhi};
    g_src = {sl, st, sl + sw, st + sh};
    static int logged = 0;
    if (logged++ < 8)
        Log("movie: fit in %dx%d window: source %ld,%ld %ldx%ld of %ldx%ld (0x%08X), dest %ld,%ld %ldx%ld (was %ld,%ld "
            "%ldx%ld) (0x%08X)",
            rc.right, rc.bottom, sl, st, sw, sh, nw, nh, hs, dl, dt, dwi, dhi, cl, ct, cwi, chi, hd);
}

// Play duration (seconds) from a WMV/ASF header: File Properties Object {8CABDCA1-A947-11CF-8EE4-00C00C205365},
// play duration (100 ns units) at +64, preroll (ms) at +80. 0 if not found.
static double AsfDuration(HANDLE h)
{
    static const u8 kFileProps[16] = {0xA1, 0xDC, 0xAB, 0x8C, 0x47, 0xA9, 0xCF, 0x11,
                                      0x8E, 0xE4, 0x00, 0xC0, 0x0C, 0x20, 0x53, 0x65};
    std::vector<u8> buf(64 * 1024);
    DWORD got = 0;
    SetFilePointer(h, 0, nullptr, FILE_BEGIN);
    if (!ReadFile(h, buf.data(), static_cast<DWORD>(buf.size()), &got, nullptr) || got < 128)
        return 0;
    for (DWORD i = 0; i + 96 < got; ++i)
        if (memcmp(&buf[i], kFileProps, 16) == 0)
        {
            unsigned long long play = 0, preroll = 0;
            memcpy(&play, &buf[i + 64], 8);
            memcpy(&preroll, &buf[i + 80], 8);
            return play / 1e7 - preroll / 1000.0;
        }
    return 0;
}

// The intro's file name depends on HD Edition's "English improvements" option (it renames the HD intro to
// fl_intro.wmv and the original to fl_intro_vanilla.wmv), and a player may put the full original intro (Bonus
// DVD, 451 s) in its place: the intro is recognised by its duration and size, not its name.
static std::string Identify(const char* path)
{
    std::string p = path ? path : "";
    std::string stem = p.substr(p.find_last_of("\\/") + 1);
    stem = stem.substr(0, stem.find_last_of('.'));
    for (char& c : stem)
        c = static_cast<char>(tolower(static_cast<u8>(c)));
    char exe[MAX_PATH];
    GetModuleFileNameA(nullptr, exe, MAX_PATH);
    std::string dir = exe;
    dir = dir.substr(0, dir.find_last_of("\\/") + 1);
    LARGE_INTEGER size = {};
    double seconds = 0;
    for (const std::string& cand : {p, dir + "..\\DATA\\" + p, dir + "..\\" + p})
    {
        HANDLE h = CreateFileA(cand.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                               0, nullptr);
        if (h == INVALID_HANDLE_VALUE)
            continue;
        GetFileSizeEx(h, &size);
        seconds = AsfDuration(h);
        g_frame = Frame();
        if (AsfFrameSize(h, g_frame.w, g_frame.h))
            g_frame.cw = g_frame.w, g_frame.ch = g_frame.h; // no bar data: fit the whole frame
        CloseHandle(h);
        break;
    }
    if (stem.rfind("fl_intro", 0) == 0)
    {
        if (seconds > 400)
            stem = "fl_intro_full"; // the full original intro (7:31)
        else if (size.QuadPart > 0 && size.QuadPart < 60LL * 1024 * 1024)
            stem = "fl_intro_vanilla"; // the 2003 intro (39 MB, 720x480); the HD one is 104 MB
        else
            stem = "fl_intro";
    }
    for (const auto& f : g_frames)
        if (f.first == stem && f.second.w == g_frame.w && f.second.h == g_frame.h)
            g_frame = f.second; // known picture area (bars zoomed out of view)
    Log("movie: %s (%lld bytes, %.1f s, %dx%d, picture %dx%d) -> %s", p.c_str(), size.QuadPart, seconds, g_frame.w,
        g_frame.h, g_frame.cw, g_frame.ch, stem.c_str());
    return stem;
}

// ------------------------------------------------------------------ drawing the lines over the movie
// One persistent 32-bit DIB of the game window's client size (premultiplied BGRA, the software renderer's target): each
// redraw clears the last frame's rectangles and draws the lines again (display::DrawMovie); the window shows it only
// where it changed.
// Path 1 (preferred): a child window of the game window, on top of the video renderer's child (the renderer's window
//   gets WS_CLIPSIBLINGS so its frames never paint over it). Its window region = the margins around the fitted picture
//   (black: the DIB's cleared pixels are opaque black to BitBlt) + the lines' rectangles, so the picture shows
//   everywhere else. Part of the game window: window capture, PrintWindow and the taskbar thumbnail show the lines.
// Path 2 (fallback, when the child cannot be made): the transparent layered window owned by the game window (before
//   26 Oct 2026 the only path; captures of the game window do not show it).
// Rate: at the display's refresh while a transition runs (display::MovieFrame.animating) or the picture changed, else a
// look at 20 Hz (a page change without a fade); a window paint only when the picture's key changed.
enum class Path
{
    None,
    Child,
    Overlay
};
static Path g_path = Path::None;
static HWND g_child = nullptr;
static HDC g_dibDc = nullptr;
static HBITMAP g_dib = nullptr, g_dibOld = nullptr;
static u32* g_bits = nullptr;
static int g_bw = 0, g_bh = 0;
static std::vector<RECT> g_lastRects; // drawn by the last redraw (cleared before the next)
static unsigned long long g_lastKey = 0;
static RECT g_lastDest = {};
static bool g_lastAnimating = true, g_clipSet = false;
static double g_refresh = 1.0 / 60; // the display's frame time (s)
static u32 g_passes = 0, g_redraws = 0, g_paints = 0;
static bool g_graphLogged = false;
static std::vector<float> g_gaps; // ms between picture updates during transitions (this movie)
static double g_lastChange = 0;
static bool g_prevAnimating = false;

static std::string GapReport()
{
    if (g_gaps.empty())
        return "no transition drawn";
    std::vector<float> s = g_gaps;
    std::sort(s.begin(), s.end());
    double sum = 0;
    for (float g : s)
        sum += g;
    char b[160];
    sprintf_s(b, "transition frames %zu: mean %.1f ms, median %.1f, p95 %.1f, max %.1f", s.size(), sum / s.size(),
              s[s.size() / 2], s[std::min(s.size() - 1, s.size() * 95 / 100)], s.back());
    return b;
}

static void* g_qualProp = nullptr; // the video renderer's IQualProp (AddRef'd; released at the movie's stop)
static void* g_audioStats = nullptr; // the audio renderer's IAMAudioRendererStats (AddRef'd; released at the stop)
// The frame reads are OFF (26 Oct 2026): the owner heard the intro's audio break up while its lines showed (disc
// install, original 1.1); measured on the intro, 75 s each: with the reads the renderer drew 8 frames and the audio had
// 50 discontinuities, without them 2,257 frames and 0 (the subtitles alone cost nothing). GetCurrentImage on the
// renderer stalls playback. Lines inside the picture keep the full box (legibility first, as without a grid); debug
// `moviesample on` brings the reads back for research only.
static bool g_noSample = true;

// AM_AUDREN_STAT_PARAM_*: 1 break count, 3 silence duration (ms), 5 discontinuities
static DWORD AudioStat(DWORD param)
{
    DWORD v = 0, unused = 0;
    if (!g_audioStats || FAILED(VCall<HRESULT>(g_audioStats, 3, param, &v, &unused)))
        return static_cast<DWORD>(-1);
    return v;
}

// The filters of the movie's graph, once per movie (which video renderer plays it: docs\DISPLAY_PLAN.md "Movies"), and
// whether a renderer offers a mixer bitmap (IVMRMixerBitmap / 9: lines blended into the video itself).
// IFilterGraph vt[5] EnumFilters; IEnumFilters vt[3] Next; IBaseFilter vt[3] GetClassID, vt[12] QueryFilterInfo.
static void LogGraph()
{
    if (g_graphLogged || !g_host || g_host->version < 3)
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
            // the audio renderer's figures (IAMAudioRendererStats vt[3] GetStatParam) for the stop line: the owner heard
            // choppy audio while the intro's lines showed (disc install, 26 Oct 2026) - tests are muted, these count it
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

static LRESULT CALLBACK OverlayProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == WM_NCHITTEST)
        return HTTRANSPARENT;
    return DefWindowProcW(h, m, w, l);
}

static LRESULT CALLBACK ChildProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m)
    {
    case WM_NCHITTEST:
        return HTTRANSPARENT;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        if (g_dibDc)
            BitBlt(dc, ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right - ps.rcPaint.left,
                   ps.rcPaint.bottom - ps.rcPaint.top, g_dibDc, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);
        EndPaint(h, &ps);
        ++g_paints;
        return 0;
    }
    }
    return DefWindowProcW(h, m, w, l);
}

static void FreeDib()
{
    if (g_dibDc)
    {
        SelectObject(g_dibDc, g_dibOld);
        DeleteDC(g_dibDc);
    }
    if (g_dib)
        DeleteObject(g_dib);
    g_dibDc = nullptr, g_dib = nullptr, g_dibOld = nullptr, g_bits = nullptr, g_bw = g_bh = 0;
    g_lastRects.clear();
}

static bool EnsureDib(int w, int h)
{
    if (g_bits && w == g_bw && h == g_bh)
        return true;
    FreeDib();
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    void* bits = nullptr;
    g_dibDc = CreateCompatibleDC(nullptr);
    g_dib = g_dibDc ? CreateDIBSection(g_dibDc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0) : nullptr;
    if (!g_dib || !bits)
    {
        FreeDib();
        return false;
    }
    g_dibOld = static_cast<HBITMAP>(SelectObject(g_dibDc, g_dib));
    g_bits = static_cast<u32*>(bits), g_bw = w, g_bh = h;
    memset(g_bits, 0, static_cast<size_t>(w) * h * 4);
    g_lastKey = 0; // a new picture
    return true;
}

// Clears a rectangle of the DIB: 0 (transparent; opaque black to BitBlt); with `margin` (the layered window) the parts
// outside the fitted picture `dest` opaque black.
static void ClearRect(const RECT& r, const RECT* margin)
{
    const LONG x0 = std::max<LONG>(0, r.left), x1 = std::min<LONG>(g_bw, r.right);
    for (LONG y = std::max<LONG>(0, r.top); y < std::min<LONG>(g_bh, r.bottom) && x1 > x0; ++y)
    {
        u32* row = g_bits + static_cast<size_t>(y) * g_bw;
        memset(row + x0, 0, static_cast<size_t>(x1 - x0) * 4);
        if (margin && margin->right > margin->left)
            for (LONG x = x0; x < x1; ++x)
                if (y < margin->top || y >= margin->bottom || x < margin->left || x >= margin->right)
                    row[x] = 0xFF000000;
    }
}

// The display's refresh (the game window's monitor): the redraw rate while a transition runs.
static void ReadRefresh()
{
    g_refresh = 1.0 / 60;
    MONITORINFOEXW mi = {};
    mi.cbSize = sizeof(mi);
    DEVMODEW dm = {};
    dm.dmSize = sizeof(dm);
    if (GetMonitorInfoW(MonitorFromWindow(g_game, MONITOR_DEFAULTTONEAREST), &mi) &&
        EnumDisplaySettingsW(mi.szDevice, ENUM_CURRENT_SETTINGS, &dm) && dm.dmDisplayFrequency >= 24 &&
        dm.dmDisplayFrequency <= 500)
        g_refresh = 1.0 / dm.dmDisplayFrequency;
}

static void EnsureWindow()
{
    if (g_path != Path::None || !g_game)
        return;
    static bool registered = false;
    if (!registered)
    {
        WNDCLASSW wc = {};
        wc.lpfnWndProc = ChildProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"FLSubtitlesMovieText";
        RegisterClassW(&wc);
        wc.lpfnWndProc = OverlayProc;
        wc.lpszClassName = L"FLSubtitlesMovieOverlay";
        RegisterClassW(&wc);
        registered = true;
    }
    RECT rc = {};
    GetClientRect(g_game, &rc);
    g_child = CreateWindowExW(WS_EX_NOPARENTNOTIFY, L"FLSubtitlesMovieText", L"", WS_CHILD | WS_CLIPSIBLINGS, 0, 0,
                              std::max<LONG>(1, rc.right), std::max<LONG>(1, rc.bottom), g_game, nullptr,
                              GetModuleHandleW(nullptr), nullptr);
    if (g_child)
    {
        g_path = Path::Child;
        Log("movie: lines in a child window %p of the game window %p (captured with it)", g_child, g_game);
        return;
    }
    // Owned by the game window: always above it, never above other applications, never takes focus/clicks.
    g_overlay = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
                                L"FLSubtitlesMovieOverlay", L"", WS_POPUP, 0, 0, 1, 1, g_game, nullptr,
                                GetModuleHandleW(nullptr), nullptr);
    if (g_overlay)
    {
        ShowWindow(g_overlay, SW_SHOWNOACTIVATE);
        g_path = Path::Overlay;
    }
    Log("movie: child window refused (error %lu): lines in the layered overlay window %p over %p (not in captures of "
        "the game window)",
        GetLastError(), g_overlay, g_game);
}

static void DestroyWindows()
{
    if (g_child)
        DestroyWindow(g_child);
    if (g_overlay)
        DestroyWindow(g_overlay);
    g_child = g_overlay = nullptr;
    g_path = Path::None;
    g_clipSet = false;
    FreeDib();
}

// The renderer's window never paints over the child (WS_CLIPSIBLINGS), the child stays above it (once per renderer
// window: Movie.dll makes one per movie).
static void KeepChildOnTop()
{
    if (g_path != Path::Child || !g_video || !IsWindow(g_video) || g_clipSet)
        return;
    const LONG st = GetWindowLongW(g_video, GWL_STYLE);
    if (!(st & WS_CLIPSIBLINGS))
        SetWindowLongW(g_video, GWL_STYLE, st | WS_CLIPSIBLINGS);
    SetWindowPos(g_child, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    g_clipSet = true;
}

// The game client's rectangle of the fitted picture (empty: not fitted - no margins of ours).
static RECT DestInClient()
{
    RECT v = g_dest;
    if (!g_video || !IsWindow(g_video) || v.right <= v.left)
        return RECT{};
    MapWindowPoints(g_video, g_game, reinterpret_cast<POINT*>(&v), 2);
    return v;
}

// ------------------------------------------------------------------ the box: from the picture under the line
// A line whose box lies inside the picture gets a box only over busy picture (owner, 26 Oct 2026: none on black / dark /
// flat colour; on the bars display.cpp leaves it out by the picture rectangle alone). The video's current frame
// (IBasicVideo::GetCurrentImage, vt[36]: a packed DIB) is read once a second on a worker thread (it is a read-back from
// the renderer: ~0.1 s at 720x480, up to ~0.6 s at 1440x960 measured - never on the movie loop's thread) into a 64 x 36
// luminance grid of the shown picture (the fit's source rectangle); the previous grid is kept for the change. Each line
// is judged from the grid cells under its box at once (a new line too): spread = standard deviation of the cells, change
// = their mean difference to the previous grid; busy = max(spread / 40, change / 20); on at 0.55 (0.45 for a line's
// first look), off at 0.35 (hysteresis: borderline content does not flicker). The layered window blends the box by how
// busy (0.4 .. 1 of the full box); the child window can only show it or not. No grid (a renderer without the image, an
// unknown format): no target - the full box (legibility first).
constexpr int kGw = 64, kGh = 36;
struct Grid
{
    float v[kGh][kGw];
};
static Grid g_grid, g_prevGrid, g_workGrid;
static bool g_haveGrid = false, g_havePrev = false;
static double g_lastSample = 0, g_imageMs = 0;
static u32 g_imageFails = 0, g_imageReads = 0;
static std::vector<std::pair<double, bool>> g_boxOn; // each line's box state (hysteresis)
// the worker: one read per request; the main thread hands it the IBasicVideo (AddRef'd) and the source rectangle
static HANDLE g_sampler = nullptr, g_sampleGo = nullptr;
static volatile LONG g_sampleBusy = 0, g_sampleReady = 0, g_samplerStop = 0;
static void* g_sampleVideo = nullptr;
static RECT g_sampleSrc = {};
static double g_sampleMs = 0;

static bool ReadGrid(void* bv, const RECT& src, Grid& out)
{
    static std::vector<u8> image; // (worker thread only)
    long size = 0;
    if (FAILED(VCall<HRESULT>(bv, 36, &size, static_cast<long*>(nullptr))) || size <= 40 || size > 64 << 20)
        return false;
    image.resize(static_cast<size_t>(size));
    if (FAILED(VCall<HRESULT>(bv, 36, &size, reinterpret_cast<long*>(image.data()))))
        return false;
    const auto* bi = reinterpret_cast<const BITMAPINFOHEADER*>(image.data());
    if (bi->biSize < 40 || bi->biWidth <= 0 || bi->biHeight == 0 || (bi->biBitCount != 32 && bi->biBitCount != 24) ||
        (bi->biCompression != BI_RGB && bi->biCompression != BI_BITFIELDS))
        return false;
    const int iw = bi->biWidth, ih = std::abs(bi->biHeight), bpp = bi->biBitCount / 8, stride = (iw * bpp + 3) & ~3;
    const size_t off = bi->biSize + (bi->biCompression == BI_BITFIELDS ? 12 : 0);
    if (off + static_cast<size_t>(stride) * ih > image.size())
        return false;
    const u8* px = image.data() + off;
    RECT s = src;
    if (s.right <= s.left || s.right > iw || s.bottom > ih)
        s = RECT{0, 0, iw, ih};
    for (int gy = 0; gy < kGh; ++gy)
        for (int gx = 0; gx < kGw; ++gx)
        {
            const int vx = std::clamp(static_cast<int>(s.left + (s.right - s.left) * (gx + 0.5f) / kGw), 0, iw - 1);
            const int vy = std::clamp(static_cast<int>(s.top + (s.bottom - s.top) * (gy + 0.5f) / kGh), 0, ih - 1);
            const u8* p = px + static_cast<size_t>(bi->biHeight > 0 ? ih - 1 - vy : vy) * stride + static_cast<size_t>(vx) * bpp;
            out.v[gy][gx] = 0.114f * p[0] + 0.587f * p[1] + 0.299f * p[2];
        }
    return true;
}

static DWORD WINAPI SamplerThread(LPVOID)
{
    while (WaitForSingleObject(g_sampleGo, INFINITE) == WAIT_OBJECT_0 && !g_samplerStop)
    {
        const double t0 = Now();
        const bool ok = g_sampleVideo && ReadGrid(g_sampleVideo, g_sampleSrc, g_workGrid);
        g_sampleMs = (Now() - t0) * 1000.0;
        if (g_sampleVideo)
            VCall<ULONG>(g_sampleVideo, 2);
        g_sampleVideo = nullptr;
        InterlockedExchange(&g_sampleReady, ok ? 1 : -1);
        InterlockedExchange(&g_sampleBusy, 0);
    }
    return 0;
}

// Ends the worker (before the graph goes: the movie's stop) - it may be inside a read: waited for, at most 2 s.
static void StopSampler()
{
    if (!g_sampler)
        return;
    InterlockedExchange(&g_samplerStop, 1);
    SetEvent(g_sampleGo);
    if (WaitForSingleObject(g_sampler, 2000) != WAIT_OBJECT_0)
        Log("movie: the frame sampler did not end within 2 s");
    CloseHandle(g_sampler), CloseHandle(g_sampleGo);
    g_sampler = g_sampleGo = nullptr;
    InterlockedExchange(&g_samplerStop, 0);
    InterlockedExchange(&g_sampleBusy, 0);
    InterlockedExchange(&g_sampleReady, 0);
}

static void JudgeBoxes(const display::MovieFrame& f, const RECT& dest)
{
    // the owner (26 Oct 2026): no box on movie lines at all - the text's outline and shadow carry it; with the research
    // reads on (`moviesample on`) the old judged box comes back, the full box where no grid is read yet
    display::SetMovieBoxDefault(g_noSample ? 0.0f : 1.0f);
    if (g_noSample)
        return;
    const double now = Now();
    // a finished read: its grid becomes the current one
    const LONG ready = InterlockedExchange(&g_sampleReady, 0);
    if (ready == 1)
    {
        if (g_haveGrid)
            g_prevGrid = g_grid, g_havePrev = true;
        g_grid = g_workGrid, g_haveGrid = true;
        ++g_imageReads;
        g_imageMs = std::max(g_imageMs, g_sampleMs);
    }
    else if (ready == -1 && g_imageFails++ == 0)
        Log("movie: no video frame to sample (the lines keep the full box inside the picture)");
    // a new read once a second while a line's box reaches into the picture (lines on the bars need none; a line's
    // first look uses the newest grid at once)
    bool inPicture = false;
    for (const RECT& b : f.boxes)
        inPicture |= dest.right <= dest.left ||
                     (b.right > dest.left && b.left < dest.right && b.bottom > dest.top && b.top < dest.bottom);
    if (inPicture && !g_noSample && g_basicVideo && now - g_lastSample >= 1.0 && !g_sampleBusy)
    {
        if (!g_sampler)
        {
            g_sampleGo = CreateEventW(nullptr, FALSE, FALSE, nullptr);
            g_sampler = g_sampleGo ? CreateThread(nullptr, 0, SamplerThread, nullptr, 0, nullptr) : nullptr;
        }
        if (g_sampler)
        {
            g_lastSample = now;
            VCall<ULONG>(g_basicVideo, 1); // AddRef: the worker's own reference
            g_sampleVideo = g_basicVideo, g_sampleSrc = g_src;
            InterlockedExchange(&g_sampleBusy, 1);
            SetEvent(g_sampleGo);
        }
    }
    if (!g_haveGrid)
        return;
    RECT d = dest;
    if (d.right <= d.left)
        GetClientRect(g_game, &d);
    std::vector<std::pair<double, bool>> next;
    for (size_t i = 0; i < f.ids.size(); ++i)
    {
        const RECT& b = f.boxes[i];
        const LONG l = std::max(b.left, d.left), t = std::max(b.top, d.top), r = std::min(b.right, d.right),
                   btm = std::min(b.bottom, d.bottom);
        if (r <= l || btm <= t)
            continue; // on the bars
        const int gx0 = std::clamp(static_cast<int>((l - d.left) * kGw / (d.right - d.left)), 0, kGw - 1),
                  gx1 = std::clamp(static_cast<int>((r - 1 - d.left) * kGw / (d.right - d.left)), gx0, kGw - 1),
                  gy0 = std::clamp(static_cast<int>((t - d.top) * kGh / (d.bottom - d.top)), 0, kGh - 1),
                  gy1 = std::clamp(static_cast<int>((btm - 1 - d.top) * kGh / (d.bottom - d.top)), gy0, kGh - 1);
        double sum = 0, sum2 = 0, change = 0, n = 0;
        for (int gy = gy0; gy <= gy1; ++gy)
            for (int gx = gx0; gx <= gx1; ++gx)
            {
                const float v = g_grid.v[gy][gx];
                sum += v, sum2 += v * v, n += 1;
                if (g_havePrev)
                    change += std::fabs(v - g_prevGrid.v[gy][gx]);
            }
        const double mean = sum / n, spread = std::sqrt(std::max(0.0, sum2 / n - mean * mean));
        const float busy = static_cast<float>(std::max(spread / 40.0, g_havePrev ? change / n / 20.0 : 0.0));
        int known = -1;
        for (size_t k = 0; k < g_boxOn.size(); ++k)
            if (g_boxOn[k].first == f.ids[i])
                known = static_cast<int>(k);
        const bool on = known < 0 ? busy >= 0.45f : g_boxOn[known].second ? busy > 0.35f : busy >= 0.55f;
        next.push_back({f.ids[i], on});
        display::SetMovieBox(f.ids[i], !on ? 0.0f
                                       : g_path == Path::Overlay ? std::clamp((busy - 0.35f) / 0.4f, 0.4f, 1.0f)
                                                                 : 1.0f);
    }
    g_boxOn.swap(next);
}

static void Redraw(bool force)
{
    EnsureWindow();
    RECT rc;
    if (g_path == Path::None || !GetClientRect(g_game, &rc) || rc.right <= 0 || rc.bottom <= 0)
        return;
    const int w = rc.right, h = rc.bottom;
    const bool resized = w != g_bw || h != g_bh;
    if (!EnsureDib(w, h))
        return;
    const RECT dest = DestInClient();
    const bool destChanged = memcmp(&dest, &g_lastDest, sizeof(dest)) != 0;
    // the layered window carries the margins in its own pixels (opaque black): all of them again when they moved
    const RECT* margin = g_path == Path::Overlay ? &dest : nullptr;
    if (margin && (resized || destChanged || force))
        ClearRect(RECT{0, 0, w, h}, margin);
    else
        for (const RECT& r : g_lastRects)
            ClearRect(r, margin);
    display::SetMoviePicture(dest, g_path == Path::Overlay);
    display::DrawMovie(g_bits, w, h);
    ++g_redraws;
    const display::MovieFrame& f = display::LastMovieFrame();
    JudgeBoxes(f, dest);
    g_lastAnimating = f.animating;
    const bool changed = force || resized || destChanged || f.key != g_lastKey;
    // (the cleared rectangles of the last frame must reach the window too: their union with this frame's)
    std::vector<RECT> dirty = g_lastRects;
    dirty.insert(dirty.end(), f.rects.begin(), f.rects.end());
    g_lastRects = f.rects;
    g_lastKey = f.key, g_lastDest = dest;
    if (!changed)
        return;
    // the frame-time record: the gaps between picture updates while a transition runs (logged at the movie's end)
    {
        const double now = Now();
        if (g_lastChange > 0 && (f.animating || g_prevAnimating) && now - g_lastChange < 1.0)
            g_gaps.push_back(static_cast<float>((now - g_lastChange) * 1000.0));
        g_lastChange = now, g_prevAnimating = f.animating;
    }
    if (g_path == Path::Overlay)
    {
        POINT origin = {0, 0};
        ClientToScreen(g_game, &origin);
        HDC screen = GetDC(nullptr);
        SIZE size = {w, h};
        POINT src = {0, 0};
        BLENDFUNCTION bf = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
        UpdateLayeredWindow(g_overlay, screen, &origin, &size, g_dibDc, &src, 0, &bf, ULW_ALPHA);
        ReleaseDC(nullptr, screen);
        ++g_paints;
        return;
    }
    // the child: its region = the margins around the picture + this frame's line rectangles
    if (resized)
        SetWindowPos(g_child, HWND_TOP, 0, 0, w, h, SWP_NOACTIVATE);
    KeepChildOnTop();
    HRGN rgn = CreateRectRgn(0, 0, 0, 0);
    auto add = [&](LONG l, LONG t, LONG r, LONG b)
    {
        if (r <= l || b <= t)
            return;
        HRGN p = CreateRectRgn(l, t, r, b);
        CombineRgn(rgn, rgn, p, RGN_OR);
        DeleteObject(p);
    };
    if (dest.right > dest.left)
    {
        add(0, 0, w, dest.top), add(0, dest.bottom, w, h);
        add(0, dest.top, dest.left, dest.bottom), add(dest.right, dest.top, w, dest.bottom);
    }
    // a line with its box: the whole rectangle (the box is opaque here); without: only its text's pixels (glyphs,
    // outline, shadow - drawn over black, they read as outlined text over the picture)
    std::vector<RECT> spans;
    for (size_t i = 0; i < f.rects.size(); ++i)
    {
        const RECT& r = f.rects[i];
        if (f.box[i] > 0.0f)
        {
            add(r.left, r.top, r.right, r.bottom);
            continue;
        }
        for (LONG y = r.top; y < r.bottom; ++y)
        {
            const u32* row = g_bits + static_cast<size_t>(y) * w;
            for (LONG x = r.left; x < r.right;)
            {
                while (x < r.right && (row[x] >> 24) < 24)
                    ++x;
                const LONG x0 = x;
                while (x < r.right && (row[x] >> 24) >= 24)
                    ++x;
                if (x > x0)
                    spans.push_back(RECT{x0, y, x, y + 1});
            }
        }
    }
    if (!spans.empty())
    {
        std::vector<u8> data(sizeof(RGNDATAHEADER) + spans.size() * sizeof(RECT));
        auto* rd = reinterpret_cast<RGNDATA*>(data.data());
        rd->rdh.dwSize = sizeof(RGNDATAHEADER), rd->rdh.iType = RDH_RECTANGLES;
        rd->rdh.nCount = static_cast<DWORD>(spans.size()), rd->rdh.nRgnSize = static_cast<DWORD>(spans.size() * sizeof(RECT));
        rd->rdh.rcBound = RECT{0, 0, w, h};
        memcpy(rd->Buffer, spans.data(), spans.size() * sizeof(RECT));
        if (HRGN p = ExtCreateRegion(nullptr, static_cast<DWORD>(data.size()), rd))
        {
            CombineRgn(rgn, rgn, p, RGN_OR);
            DeleteObject(p);
        }
    }
    RECT box = {};
    const bool any = GetRgnBox(rgn, &box) != NULLREGION;
    // SetWindowRgn owns the region; no redraw by it (the dirty rectangles are painted below)
    SetWindowRgn(g_child, rgn, FALSE);
    if (any != (IsWindowVisible(g_child) != FALSE))
        ShowWindow(g_child, any ? SW_SHOWNA : SW_HIDE);
    if (any)
    {
        if (resized || destChanged || force)
            InvalidateRect(g_child, nullptr, FALSE);
        else
            for (const RECT& r : dirty)
                InvalidateRect(g_child, &r, FALSE);
        UpdateWindow(g_child);
    }
}

// movie_skip 1: the startup movies end the way the game ends a finished one - the host's native skip (`movieskip`,
// subtitles\src\host\main.cpp UpdateMovieHook: the next UpdateMovie reports the movie finished; no key input, works out
// of focus), requested through the module's own debug channel (the host polls it before every UpdateMovie): the logic
// has no other way to the host (HostApi v6 is fixed). Only while the channel is idle (the harness may be using it).
static bool g_skipRequested = false;
static void RequestNativeSkip()
{
    if (g_skipRequested)
        return;
    g_skipRequested = true;
    std::string channel = "FLSubtitlesShm";
    const std::string inst = InstanceName();
    if (!inst.empty())
        channel += "_" + inst;
    HANDLE mapping = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, ("Local\\" + channel).c_str());
    if (!mapping)
        return;
    // the harness's channel mutex (flctl.core.ps1 Send-FL: Local\FLvibedCmd_<channel name>), so a command of its own
    // is never overwritten; a channel in use = no skip this time (the next pass tries again)
    HANDLE mutex = CreateMutexA(nullptr, FALSE, ("Local\\FLvibedCmd_" + channel).c_str());
    const bool owned = mutex && WaitForSingleObject(mutex, 0) == WAIT_OBJECT_0;
    ShmBlock* s = owned ? static_cast<ShmBlock*>(MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(ShmBlock)))
                        : nullptr;
    if (s && s->magic == kShmMagic && s->cmdSeq == s->cmdAck)
    {
        strcpy_s(s->cmd, "movieskip");
        ++s->cmdSeq;
        Log("movie: %s skipped (movie_skip 1)", g_current.c_str());
    }
    else
        g_skipRequested = false; // busy: try again at the next pass
    if (s)
        UnmapViewOfFile(s);
    if (owned)
        ReleaseMutex(mutex);
    if (mutex)
        CloseHandle(mutex);
    CloseHandle(mapping);
}

// movie_intro -> the host's intro override (movie.h). The files sit beside the game's own intro (DATA\MOVIES; HD
// Edition's "English improvements" option names the HD intro fl_intro.wmv and keeps the 2003 one as fl_intro_vanilla.wmv;
// the full Bonus DVD intro is the player's own copy as fl_intro_full.wmv): an absolute path, so the host's existence
// check and Movie.dll's open do not depend on the working directory.
static const char* const kIntroFiles[] = {nullptr, "fl_intro.wmv", "fl_intro_vanilla.wmv", "fl_intro_full.wmv"};

void ApplyIntro()
{
    int& choice = display::Get().movieIntro;
    if (choice < 0 || choice > 3)
        choice = 0;
    if (!g_host || g_host->version < 7 || !g_host->SetIntroOverride)
    {
        if (choice)
            Log("movie_intro %d: the host is older than v7 - the intro plays as installed", choice);
        return;
    }
    std::string path;
    if (choice)
    {
        path = std::string(g_host->dllDir) + "..\\DATA\\MOVIES\\" + kIntroFiles[choice];
        if (GetFileAttributesA(path.c_str()) == INVALID_FILE_ATTRIBUTES)
        {
            Log("movie_intro %d: %s is not in this install - back to 0 (as installed)", choice, path.c_str());
            choice = 0;
            path.clear();
        }
    }
    g_host->SetIntroOverride(path.empty() ? nullptr : path.c_str());
    Log("movie_intro %d: the intro plays %s", choice, path.empty() ? "as installed" : path.c_str());
}

void OnSettingApplied(const settings::Def* d)
{
    if (d && strcmp(d->name, "movie_intro") == 0)
        ApplyIntro();
}

static void Tick();
static void HookSleep(bool on);

void OnStart(HWND hwnd, const char* path)
{
    g_game = hwnd ? hwnd : Mem<HWND>(exe::MainHwnd);
    g_video = nullptr;
    ReleaseBasicVideo();
    g_current = Identify(path);
    g_t0 = Now();
    g_next = 0;
    g_skipRequested = false;
    g_clipSet = false, g_graphLogged = false, g_lastAnimating = true;
    g_passes = g_redraws = g_paints = 0;
    g_gaps.clear(), g_lastChange = 0, g_prevAnimating = false;
    g_boxOn.clear(), g_lastSample = 0, g_imageFails = g_imageReads = 0, g_imageMs = 0, g_haveGrid = g_havePrev = false;
    ReadRefresh();
    HookSleep(true);
    display::Clear();
}

void OnUpdate()
{
    if (g_current.empty())
        return;
    if (display::Get().movieSkip)
        RequestNativeSkip(); // takes effect at the next movie loop pass (the host polls before UpdateMovie)
    FitVideo();
    if (!g_video || !IsWindow(g_video)) // (FitVideo finds it too; with movie_fit 0 only here: the child stays above it)
    {
        g_video = nullptr, g_clipSet = false;
        EnumChildWindows(g_game, FindVideoChild, reinterpret_cast<LPARAM>(&g_video));
    }
    LogGraph();
    ++g_passes;
    Tick();
}

// The lines on the movie clock, then the picture: from each movie loop pass and from the slices of the loop's own wait
// (MovieSleep) - lines start on time and transitions draw at the display's rate.
static void Tick()
{
    const double t = Now() - g_t0;
    for (; g_next < g_lines.size(); ++g_next)
    {
        const Line& ln = g_lines[g_next];
        if (ln.movie != g_current)
            continue;
        if (ln.start > t)
            break;
        if (ln.end > t && display::Get().movieSubtitles) // not already over (a late first update skips past lines)
            display::Show(display::Category::Story, ln.speaker,
                          ln.speaker == L"Computer" ? display::Relation::Computer : display::Relation::Neutral, ln.text,
                          ln.end - ln.start, 0, false, ln.wordStarts.empty() ? nullptr : &ln.wordStarts);
    }
    display::Update(0);
    // at the display's rate while something moves (time-based fades and glides), else a look at 20 Hz; the window is
    // painted only when the picture changed (Redraw)
    const double now = Now();
    if (now - g_lastDraw >= (g_lastAnimating ? g_refresh * 0.75 : 0.05))
    {
        g_lastDraw = now;
        Redraw(false);
    }
}

// Movie.dll's UpdateMovie starts with Sleep(300) (Movie.dll 0x10001730: push 12Ch; call [Sleep]): the movie loop runs
// ~3 passes a second while the video plays on DirectShow's own thread. Its import of Sleep is chained while a movie
// plays: the same total wait, cut into slices - one composition frame each (DwmFlush) while a transition runs, else
// 50 ms - with a Tick between them. The loop's timing and the video's are untouched.
using SleepFn = void(WINAPI*)(DWORD);
static SleepFn g_prevSleep = nullptr;
static void** g_sleepSlot = nullptr;
static HRESULT(WINAPI* g_dwmFlush)() = nullptr;
static bool g_inSleep = false;

static void WINAPI MovieSleep(DWORD ms)
{
    SleepFn prev = g_prevSleep ? g_prevSleep : ::Sleep;
    if (g_current.empty() || g_inSleep || ms < 20 || ms > 2000)
        return prev(ms);
    g_inSleep = true;
    const double end = Now() + ms / 1000.0;
    for (;;)
    {
        Tick();
        const double left = end - Now();
        if (left <= 0.002)
            break;
        if (g_lastAnimating && g_dwmFlush && left > g_refresh * 0.5)
        {
            if (FAILED(g_dwmFlush()))
                prev(static_cast<DWORD>(std::max(1.0, std::min(left, g_refresh) * 1000.0)));
        }
        else
            prev(static_cast<DWORD>(std::max(1.0, std::min(left, g_lastAnimating ? g_refresh : 0.05) * 1000.0)));
    }
    g_inSleep = false;
}

static void HookSleep(bool on)
{
    if (on && !g_sleepSlot)
    {
        if (!g_dwmFlush)
            if (HMODULE dwm = LoadLibraryW(L"dwmapi.dll"))
                g_dwmFlush = reinterpret_cast<HRESULT(WINAPI*)()>(GetProcAddress(dwm, "DwmFlush"));
        HMODULE m = GetModuleHandleW(L"movie.dll");
        void** slot = m ? hk::ImportSlot(m, "kernel32.dll", "Sleep") : nullptr;
        if (!slot)
        {
            Log("movie: Movie.dll imports no Sleep - the lines update once per movie loop pass");
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

void OnStop()
{
    if (g_current.empty())
        return;
    StopSampler();
    const double t = Now() - g_t0;
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
        "%u frame samples%s, slowest %.1f ms; renderer frames drawn %d, dropped %d; audio breaks %ld, discontinuities %ld, "
        "silence %ld ms)",
        g_current.c_str(), t, g_path == Path::Child ? "child window" : g_path == Path::Overlay ? "layered overlay" : "none",
        t > 0 ? g_passes / t : 0.0, 1.0 / g_refresh, g_redraws, g_paints, GapReport().c_str(), g_imageReads,
        g_noSample ? " (sampling off)" : "", g_imageMs, drawn, dropped, breaks, discont, silenceMs);
    g_current.clear();
    ReleaseBasicVideo();
    display::Clear();
    DestroyWindows();
    HookSleep(false);
}

std::string SetSampling(bool on)
{
    g_noSample = !on;
    return std::string("movie frame sampling ") + (on ? "on" : "off");
}

std::string Describe()
{
    std::string s = "movie lines " + std::to_string(g_lines.size()) +
                    (g_current.empty() ? ", none playing" : ", playing " + g_current);
    if (!g_current.empty())
    {
        const double t = Now() - g_t0;
        char b[200];
        sprintf_s(b, ", path %s, loop %.0f passes/s, refresh %.0f Hz, %u redraws, %u paints, animating %d",
                  g_path == Path::Child ? "child" : g_path == Path::Overlay ? "overlay" : "none",
                  t > 0 ? g_passes / t : 0.0, 1.0 / g_refresh, g_redraws, g_paints, g_lastAnimating ? 1 : 0);
        s += b;
    }
    return s;
}
} // namespace movie
