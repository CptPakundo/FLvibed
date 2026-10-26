// Borderless windowed when nothing else provides it (winmode.h; decisions in core\winmode_rules.h).
#include "winmode.h"

#include "flexe.h"
#include "hook.h"
#include "log.h"
#include "winmode_rules.h"

namespace winmode
{
namespace
{
typedef bool(__cdecl* SetupScreenFn)(HWND wnd, const u8* request, u8* applied);
typedef void(__cdecl* WindowStyleFn)(HWND wnd, bool fullscreen, int w, int h);
SetupScreenFn g_origSetup = nullptr;
WindowStyleFn g_origStyle = nullptr;

constexpr int kSettingsSize = 0x28, kOffW = 8, kOffH = 0xC, kOffFullscreen = 0x25;

int g_setting = 1;
bool g_testStart = false;
bool g_styleKnown = false;
u32 g_gameStyle = 0;
Provider g_provider = Provider::Undecided;
bool g_bordered = false;   // Alt+Enter's state: the game's bordered window instead of FLvibed's borderless one
bool g_started = false;    // a display setup succeeded
bool g_borderless = false; // the setup in progress / last applied asks for the borderless window
char g_wrapper[MAX_PATH] = {};
int g_dpiStart = -1, g_dpiNow = -1; // the process's DPI awareness (-1 unknown, 0 unaware, 1 system, 2 per monitor)
u32 g_setups = 0, g_rewrites = 0, g_toggles = 0, g_styled = 0;

// The process's DPI awareness (GetAwarenessFromDpiAwarenessContext of the thread's context, the process default
// unless a thread overrides it); -1 when the system cannot tell (before Windows 10 1607: IsProcessDPIAware).
int DpiAwareness()
{
    HMODULE u = GetModuleHandleA("user32.dll");
    typedef void*(WINAPI * GetCtxFn)();
    typedef int(WINAPI * AwarenessFn)(void*);
    auto getCtx = u ? reinterpret_cast<GetCtxFn>(GetProcAddress(u, "GetThreadDpiAwarenessContext")) : nullptr;
    auto aw = u ? reinterpret_cast<AwarenessFn>(GetProcAddress(u, "GetAwarenessFromDpiAwarenessContext")) : nullptr;
    if (getCtx && aw)
        return aw(getCtx());
    typedef BOOL(WINAPI * IsAwareFn)();
    auto isAware = u ? reinterpret_cast<IsAwareFn>(GetProcAddress(u, "IsProcessDPIAware")) : nullptr;
    return isAware ? (isAware() ? 1 : 0) : -1;
}

// The DPI awareness is the exe's (a manifest - HDE's -, the Windows compatibility flag HIGHDPIAWARE, __COMPAT_LAYER):
// FLvibed cannot change it for the game. The game creates its window (0x5B32E5, exe::MainHwnd) before it loads the
// DACOM libraries (0x5B1300) that load FLvibed, and a window keeps the awareness it was created with. A DPI-unaware
// game gets the borderless window at the monitor's logical size, scaled up by Windows (covers the monitor, soft at
// high DPI); an aware one renders at the physical size. Logged once.
void LogDpi()
{
    g_dpiStart = g_dpiNow = DpiAwareness();
    Log("winmode: DPI awareness %d (%s; the exe's own: FLvibed loads after the game window exists)", g_dpiStart,
        g_dpiStart == 0 ? "unaware: the borderless window is scaled by Windows at high DPI" : "aware: physical pixels");
}

// A Direct3D wrapper owns the device: the d3d8.dll the game loaded is not the system's (HDE's d3d8to9 with DxWrapper,
// dgVoodoo, ...). Its path in g_wrapper.
bool WrapperLoaded()
{
    g_wrapper[0] = 0;
    HMODULE m = GetModuleHandleA("d3d8.dll");
    char path[MAX_PATH] = {}, dir[MAX_PATH] = {}, sys[MAX_PATH] = {}, wow[MAX_PATH] = {};
    if (!m || !GetModuleFileNameA(m, path, MAX_PATH))
        return false;
    strcpy_s(dir, path);
    if (char* slash = strrchr(dir, '\\'))
        *slash = 0;
    GetSystemDirectoryA(sys, MAX_PATH);
    GetSystemWow64DirectoryA(wow, MAX_PATH);
    const bool system = (sys[0] && _stricmp(dir, sys) == 0) || (wow[0] && _stricmp(dir, wow) == 0);
    if (!system)
    {
        const char* name = strrchr(path, '\\');
        sprintf_s(g_wrapper, "%s%s", name ? name + 1 : path,
                  GetModuleHandleA("dxwrapper.dll") ? " (DxWrapper loaded)" : " in the game folder");
    }
    return !system;
}

Provider DecideNow()
{
    const bool wrapper = WrapperLoaded();
    return Decide(g_setting, g_origSetup && g_origStyle, g_styleKnown, g_gameStyle, wrapper);
}

void LogProvider(const char* when)
{
    Log("winmode: borderless %d (%s) - %s%s%s (%s)", g_setting,
        g_setting == 0 ? "off" : (g_setting == 1 ? "auto" : "always"), ProviderName(g_provider),
        g_provider == Provider::Wrapper ? ": " : "", g_provider == Provider::Wrapper ? g_wrapper : "", when);
}

bool MonitorOf(HWND wnd, RECT& r)
{
    HMONITOR mon = MonitorFromWindow(wnd, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi = {sizeof(mi)};
    if (!mon || !GetMonitorInfoA(mon, &mi))
        return false;
    r = mi.rcMonitor;
    return r.right > r.left && r.bottom > r.top;
}

// After the game's own windowed style call: no caption / frame, the monitor's rectangle (or centred on it).
void MakeBorderless(HWND wnd, int w, int h)
{
    RECT mon;
    if (!wnd || !MonitorOf(wnd, mon))
    {
        Log("winmode: no monitor for the window - the game's window as it is");
        return;
    }
    const LONG style = GetWindowLongA(wnd, GWL_STYLE);
    const LONG ex = GetWindowLongA(wnd, GWL_EXSTYLE);
    const LONG nstyle = (style & ~static_cast<LONG>(WS_CAPTION | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX |
                                                     WS_MAXIMIZEBOX | WS_BORDER | WS_DLGFRAME)) |
                        static_cast<LONG>(WS_POPUP);
    const LONG nex = ex & ~static_cast<LONG>(WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_DLGMODALFRAME);
    SetWindowLongA(wnd, GWL_STYLE, nstyle);
    SetWindowLongA(wnd, GWL_EXSTYLE, nex);
    const Rect p = Place(mon.left, mon.top, mon.right - mon.left, mon.bottom - mon.top, w, h);
    SetWindowPos(wnd, nullptr, p.x, p.y, p.w, p.h,
                 SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOACTIVATE);
    ++g_styled;
    RECT wr = {}, cr = {};
    GetWindowRect(wnd, &wr);
    GetClientRect(wnd, &cr);
    Log("winmode: borderless window %dx%d at (%d,%d) on the monitor (%d,%d %dx%d); style 0x%08X -> 0x%08X; client %dx%d",
        static_cast<int>(wr.right - wr.left), static_cast<int>(wr.bottom - wr.top), static_cast<int>(wr.left),
        static_cast<int>(wr.top), static_cast<int>(mon.left), static_cast<int>(mon.top),
        static_cast<int>(mon.right - mon.left), static_cast<int>(mon.bottom - mon.top), static_cast<u32>(style),
        static_cast<u32>(GetWindowLongA(wnd, GWL_STYLE)), static_cast<int>(cr.right), static_cast<int>(cr.bottom));
}

bool __cdecl SetupScreenHook(HWND wnd, const u8* request, u8* applied)
{
    if (g_provider == Provider::Undecided)
    {
        g_provider = DecideNow(); // the d3d8.dll the device comes from is loaded by now
        LogProvider("at the first display setup");
    }
    ++g_setups;
    if (g_provider != Provider::FLvibed || !request)
        return g_origSetup(wnd, request, applied);
    u8 buf[kSettingsSize];
    memcpy(buf, request, sizeof(buf));
    Request req = {buf[kOffFullscreen] != 0, *reinterpret_cast<int*>(buf + kOffW), *reinterpret_cast<int*>(buf + kOffH)};
    const bool appliedFull = applied ? applied[kOffFullscreen] != 0 : req.fullscreen;
    RECT mon = {};
    const bool haveMon = wnd && MonitorOf(wnd, mon);
    const Setup s = Apply(req, appliedFull, g_started, g_bordered, haveMon ? mon.right - mon.left : 0,
                          haveMon ? mon.bottom - mon.top : 0);
    buf[kOffFullscreen] = s.out.fullscreen ? 1 : 0;
    *reinterpret_cast<int*>(buf + kOffW) = s.out.w;
    *reinterpret_cast<int*>(buf + kOffH) = s.out.h;
    g_borderless = s.borderless;
    if (s.toggled)
        ++g_toggles;
    if (req.fullscreen != s.out.fullscreen || req.w != s.out.w || req.h != s.out.h)
        ++g_rewrites;
    const bool ok = g_origSetup(wnd, buf, applied);
    if (ok)
        g_started = true;
    Log("winmode: display setup %dx%d %s (applied %s)%s -> %dx%d windowed, %s: %s; render %dx%d, applied fullscreen "
        "byte %d",
        req.w, req.h, req.fullscreen ? "fullscreen" : "windowed", appliedFull ? "fullscreen" : "windowed",
        s.toggled ? " = Alt+Enter toggle" : "", s.out.w, s.out.h, s.borderless ? "borderless" : "bordered",
        ok ? "ok" : "FAILED", Mem<int>(exe::ScreenW), Mem<int>(exe::ScreenH), applied ? applied[kOffFullscreen] : -1);
    return ok;
}

void __cdecl WindowStyleHook(HWND wnd, bool fullscreen, int w, int h)
{
    g_origStyle(wnd, fullscreen, w, h);
    if (g_provider != Provider::FLvibed || fullscreen || !g_borderless || w <= 0 || h <= 0)
        return;
    MakeBorderless(wnd, w, h);
}
} // namespace

void Install(const char* iniPath, bool testStart)
{
    g_testStart = testStart;
    g_setting = static_cast<int>(GetPrivateProfileIntA("Controller", "borderless", 1, iniPath));
    if (g_setting < 0 || g_setting > 2)
        g_setting = 1;
    if (g_setting == 0)
    {
        g_provider = Provider::Off;
        LogProvider("setting");
        return;
    }
    g_styleKnown = exe::WindowedStyle.ok();
    if (g_styleKnown)
        g_gameStyle = Mem<u32>(exe::WindowedStyle, 1);
    // SetupScreen: mov eax, ds:[imm32] (5 bytes, absolute); WindowStyle: mov al,[esp+8]; sub esp,10h (7 bytes)
    g_origSetup = static_cast<SetupScreenFn>(
        hk::Detour(exe::SetupScreen, reinterpret_cast<void*>(SetupScreenHook), 5, "borderless windowed"));
    g_origStyle = static_cast<WindowStyleFn>(
        hk::Detour(exe::WindowStyle, reinterpret_cast<void*>(WindowStyleHook), 7, "borderless windowed"));
    // A hook without its partner would rewrite a setup without restyling the window (or the reverse): both or neither
    // act (the provider is Unverified then; the installed one passes through).
    const Provider tentative = DecideNow();
    Log("winmode: the game's windowed style 0x%08X%s; Direct3D 8: %s", g_gameStyle,
        g_styleKnown ? "" : " (site unverified)", g_wrapper[0] ? g_wrapper : "the system's");
    LogDpi();
    Log("winmode: at the start the provider would be %s (decided at the first display setup)", ProviderName(tentative));
}

const char* ProviderNow(char* detail, int detailLen)
{
    if (detail && detailLen > 0)
        strcpy_s(detail, static_cast<size_t>(detailLen), g_provider == Provider::Wrapper ? g_wrapper : "");
    return ProviderName(g_provider);
}

void Command(const char* args, char* reply, size_t len)
{
    while (args && *args == ' ')
        ++args;
    if (args && strcmp(args, "altenter") == 0)
    {
        // As FLUF's Alt+Enter: a copy of the applied settings, the fullscreen byte flipped, SetupScreen with the game
        // window and the applied settings. Only where FLvibed's rewrite is certain to apply (it never reaches exclusive
        // fullscreen): a background test start, FLvibed the provider, a setup done, the applied settings readable.
        HWND wnd = Mem<HWND>(exe::MainHwnd);
        if (!g_testStart || g_provider != Provider::FLvibed || !g_started || !exe::DisplaySettings.ok() || !wnd)
        {
            sprintf_s(reply, len, "winmode altenter: refused (test start %d, provider %s, started %d, settings %d)",
                      g_testStart, ProviderName(g_provider), g_started, exe::DisplaySettings.ok());
            return;
        }
        u8* applied = &Mem<u8>(exe::DisplaySettings);
        u8 buf[kSettingsSize];
        memcpy(buf, applied, sizeof(buf));
        buf[kOffFullscreen] ^= 1;
        const bool ok = SetupScreenHook(wnd, buf, applied);
        sprintf_s(reply, len, "winmode altenter: setup %s, now %s", ok ? "ok" : "FAILED",
                  g_bordered ? "bordered" : "borderless");
        return;
    }
    HWND wnd = Mem<HWND>(exe::MainHwnd);
    RECT wr = {}, cr = {}, mon = {};
    LONG style = 0;
    if (wnd)
    {
        GetWindowRect(wnd, &wr);
        GetClientRect(wnd, &cr);
        style = GetWindowLongA(wnd, GWL_STYLE);
        MonitorOf(wnd, mon);
    }
    sprintf_s(reply, len,
              "winmode: setting %d provider %s%s%s\ngame windowed style 0x%08X (%s)\ndpi start %d now %d (window %u)\n"
              "state: %s, %s; setups %u rewrites %u toggles %u styled %u\n"
              "window (%d,%d %dx%d) client %dx%d style 0x%08X; monitor (%d,%d %dx%d); render %dx%d",
              g_setting, ProviderName(g_provider), g_provider == Provider::Wrapper ? ": " : "",
              g_provider == Provider::Wrapper ? g_wrapper : "", g_gameStyle, g_styleKnown ? "verified" : "unverified",
              g_dpiStart, g_dpiNow, wnd ? GetDpiForWindow(wnd) : 0, g_started ? "started" : "not started",
              g_bordered ? "bordered" : (g_borderless ? "borderless" : "game's"), g_setups, g_rewrites, g_toggles,
              g_styled, static_cast<int>(wr.left), static_cast<int>(wr.top), static_cast<int>(wr.right - wr.left),
              static_cast<int>(wr.bottom - wr.top), static_cast<int>(cr.right), static_cast<int>(cr.bottom),
              static_cast<u32>(style), static_cast<int>(mon.left), static_cast<int>(mon.top),
              static_cast<int>(mon.right - mon.left), static_cast<int>(mon.bottom - mon.top), Mem<int>(exe::ScreenW),
              Mem<int>(exe::ScreenH));
}
} // namespace winmode
