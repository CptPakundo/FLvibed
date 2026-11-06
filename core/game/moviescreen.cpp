// What the Subtitles and Movies modules share of a movie (moviescreen.h).
#include "game/moviescreen.h"

#include "flexe.h"
#include "hook.h"
#include "log.h"

#include <algorithm>

namespace moviescreen
{
static Info g_info;
static const char* g_beginPath = nullptr; // Begin's event: the second module's call for it does nothing
static bool g_playing = false;
static double g_t0 = 0, g_lastDraw = 0;
static HWND g_game = nullptr, g_overlay = nullptr, g_video = nullptr;
static RECT g_dest = {}; // where the picture is drawn, in video window (= game client) pixels
static RECT g_src = {};  // the part of the video it shows (native video pixels: left, top, right, bottom)

double Seconds()
{
    static LARGE_INTEGER f = {};
    if (!f.QuadPart)
        QueryPerformanceFrequency(&f);
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return static_cast<double>(t.QuadPart) / f.QuadPart;
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
static Info Identify(const char* path)
{
    Info info;
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
    for (const std::string& cand : {p, dir + "..\\DATA\\" + p, dir + "..\\" + p})
    {
        HANDLE h = CreateFileA(cand.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                               0, nullptr);
        if (h == INVALID_HANDLE_VALUE)
            continue;
        GetFileSizeEx(h, &size);
        info.seconds = AsfDuration(h);
        AsfFrameSize(h, info.w, info.h);
        CloseHandle(h);
        break;
    }
    info.bytes = size.QuadPart;
    if (stem.rfind("fl_intro", 0) == 0)
    {
        if (info.seconds > 400)
            stem = "fl_intro_full"; // the full original intro (7:31)
        else if (size.QuadPart > 0 && size.QuadPart < 60LL * 1024 * 1024)
            stem = "fl_intro_vanilla"; // the 2003 intro (39 MB, 720x480); the HD one is 104 MB
        else
            stem = "fl_intro";
    }
    info.id = stem;
    Log("movie: %s (%lld bytes, %.1f s, %dx%d) -> %s", p.c_str(), info.bytes, info.seconds, info.w, info.h, stem.c_str());
    return info;
}

// ------------------------------------------------------------------ the window over the video
// One persistent 32-bit DIB of the game window's client size (premultiplied BGRA, the software renderer's target): each
// redraw clears the last frame's rectangles and draws the lines again; the window shows it only where it changed.
// Path 1 (preferred): a child window of the game window, on top of the video renderer's child (the renderer's window
//   gets WS_CLIPSIBLINGS so its frames never paint over it). Its window region = the margins around the fitted picture
//   (black: the DIB's cleared pixels are opaque black to BitBlt) + the lines' rectangles, so the picture shows
//   everywhere else. Part of the game window: window capture, PrintWindow and the taskbar thumbnail show the lines.
// Path 2 (fallback, when the child cannot be made): the transparent layered window owned by the game window (captures of
//   the game window do not show it).
// Rate: at the display's refresh while a transition runs or the picture changed, else a look at 20 Hz (a page change
// without a fade); a window paint only when the picture's key changed.
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
static std::vector<float> g_gaps; // ms between picture updates during transitions (this movie)
static double g_lastChange = 0;
static bool g_prevAnimating = false;

std::string GapReport()
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

static BOOL CALLBACK FindVideoChild(HWND h, LPARAM out)
{
    if (h == g_overlay || !IsWindowVisible(h))
        return TRUE;
    *reinterpret_cast<HWND*>(out) = h;
    return FALSE;
}

HWND Video()
{
    if (g_game && (!g_video || !IsWindow(g_video)))
    {
        g_video = nullptr, g_clipSet = false;
        EnumChildWindows(g_game, FindVideoChild, reinterpret_cast<LPARAM>(&g_video));
        if (g_video)
        {
            char cls[64] = {};
            GetClassNameA(g_video, cls, sizeof(cls));
            Log("movie: video window %p (%s)", g_video, cls);
        }
    }
    return g_video;
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

// The windows' classes live in the process, their procedures in this logic: gone with it (Shutdown).
static const wchar_t kChildClass[] = L"FLSubtitlesMovieText", kOverlayClass[] = L"FLSubtitlesMovieOverlay";
static bool g_classes = false;

static void EnsureWindow()
{
    if (g_path != Path::None || !g_game)
        return;
    if (!g_classes)
    {
        WNDCLASSW wc = {};
        wc.lpfnWndProc = ChildProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = kChildClass;
        RegisterClassW(&wc);
        wc.lpfnWndProc = OverlayProc;
        wc.lpszClassName = kOverlayClass;
        RegisterClassW(&wc);
        g_classes = true;
    }
    RECT rc = {};
    GetClientRect(g_game, &rc);
    g_child = CreateWindowExW(WS_EX_NOPARENTNOTIFY, kChildClass, L"", WS_CHILD | WS_CLIPSIBLINGS, 0, 0,
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
                                kOverlayClass, L"", WS_POPUP, 0, 0, 1, 1, g_game, nullptr,
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

// Content's calls come from the Movies module's slices of the loop's wait too, outside any host guard: a fault there ends the lines, not
// the game. (Plain locals only: the __try rule.)
static bool GuardedLines(double t)
{
    __try
    {
        content.Lines(t);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool GuardedPaint(u32* pixels, int w, int h, const RECT& picture, bool translucent, Painted& out)
{
    __try
    {
        content.Paint(pixels, w, h, picture, translucent, out);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static void LinesFaulted()
{
    content = {};
    Log("movie: the lines over the movie faulted: they are off for this logic");
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
    static const std::vector<RECT> noRects;
    static const std::vector<float> noBoxes;
    Painted p;
    p.rects = &noRects, p.box = &noBoxes;
    if (content.Paint && !GuardedPaint(g_bits, w, h, dest, g_path == Path::Overlay, p))
    {
        LinesFaulted();
        p = Painted();
        p.rects = &noRects, p.box = &noBoxes;
    }
    struct
    {
        const std::vector<RECT>& rects;
        const std::vector<float>& box;
        unsigned long long key;
        bool animating;
    } f = {*p.rects, *p.box, p.key, p.animating};
    ++g_redraws;
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
        const double now = Seconds();
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

void Tick()
{
    if (content.Lines && !GuardedLines(Clock()))
        LinesFaulted();
    // at the display's rate while something moves (time-based fades and glides), else a look at 20 Hz; the window is
    // painted only when the picture changed (Redraw)
    const double now = Seconds();
    if (now - g_lastDraw >= (g_lastAnimating ? g_refresh * 0.75 : 0.05))
    {
        g_lastDraw = now;
        Redraw(false);
    }
}

void Pass()
{
    if (!g_playing)
        return;
    Video(); // (with movie_fit 0 only here: the child stays above it)
    ++g_passes;
    Tick();
}

void Begin(HWND hwnd, const char* path)
{
    if (g_playing && g_beginPath == path)
        return;
    g_beginPath = path;
    g_game = hwnd ? hwnd : Mem<HWND>(exe::MainHwnd);
    g_video = nullptr;
    g_dest = {}, g_src = {};
    g_info = Identify(path);
    g_t0 = Seconds();
    g_clipSet = false, g_lastAnimating = true;
    g_passes = g_redraws = g_paints = 0;
    g_gaps.clear(), g_lastChange = 0, g_prevAnimating = false;
    ReadRefresh();
    g_playing = true;
}

void End()
{
    if (!g_playing)
        return;
    g_playing = false;
    g_info = Info();
    g_dest = {}, g_src = {};
    DestroyWindows();
}

void Shutdown()
{
    End();
    if (g_classes)
    {
        UnregisterClassW(kChildClass, GetModuleHandleW(nullptr));
        UnregisterClassW(kOverlayClass, GetModuleHandleW(nullptr));
        g_classes = false;
    }
}

bool Playing() { return g_playing; }
const Info& Current() { return g_info; }
double Clock() { return Seconds() - g_t0; }
HWND Game() { return g_game; }
void SetPicture(const RECT& dest, const RECT& src) { g_dest = dest, g_src = src; }
const RECT& Source() { return g_src; }
bool Animating() { return g_lastAnimating; }
double RefreshSeconds() { return g_refresh; }
const char* PathName() { return g_path == Path::Child ? "child" : g_path == Path::Overlay ? "overlay" : "none"; }
u32 Passes() { return g_passes; }
u32 Redraws() { return g_redraws; }
u32 Paints() { return g_paints; }
} // namespace moviescreen
