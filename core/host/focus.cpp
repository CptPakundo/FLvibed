// FLvibed - the game out of focus (docs\MODULES_PLAN.md: core): keep running, and on a background test start the focus
// guard, the silence and the window title.
#include "all.h"

#include "focus.h"

namespace focus
{
// ------------------------------------------------------------------ keep running out of focus
// Freelancer runs its frame only while the byte AppActive is set (0x424F30); the window procedure's WM_ACTIVATE sets
// it through SetAppActive (0x424C60), which also releases / takes the DirectInput mouse (0x420350). With the option
// on, the game's own SetAppActive still runs with the real state (the physical mouse goes back to Windows), then the
// frame gate stays open: the game, the mod and the harness go on in the background. The virtual mouse then answers
// the game's reads of the unacquired device (dimouse::SetBackground); the physical mouse is not read.
typedef void(__cdecl* SetAppActiveFn)(bool active);
static SetAppActiveFn g_origSetAppActive = nullptr;
static bool g_keepRunning = false;
static bool g_forceKeepRunning = false; // background test start (FLVIBED_BACKGROUND): on whatever the option says
static bool g_appReallyActive = true;
static HWND g_userWindow = nullptr; // the foreground window when a background start began (another process's)
static DWORD g_startTick = 0;

static bool g_started = false; // Start went past its gate

static void ApplyKeepRunning()
{
    const bool background = g_keepRunning && !g_appReallyActive;
    dimouse::SetBackground(background);
    if (background && exe::AppActive.ok())
        Mem<u8>(exe::AppActive) = 1;
}

static void __cdecl SetAppActiveHook(bool active)
{
    g_appReallyActive = active;
    g_origSetAppActive(active);
    ApplyKeepRunning();
}

// No focus stealing while running in the background: Freelancer (0x4248F3: SetWindowPos HWND_TOPMOST + ShowWindow
// SW_SHOWNORMAL) and its graphics wrappers activate the game window and put it above everything. On a background test
// start, while the game is not the foreground window, those calls on its own windows become non-activating (no
// activation, no z-order change). Import-table patches in every module of the process, re-applied for modules
// loaded later (watchdog). A click on the game window still activates it the normal way.
typedef BOOL(WINAPI* ShowWindowFn)(HWND, int);
typedef BOOL(WINAPI* SetWindowPosFn)(HWND, HWND, int, int, int, int, UINT);
typedef BOOL(WINAPI* HwndBoolFn)(HWND);
typedef HWND(WINAPI* HwndHwndFn)(HWND);
static ShowWindowFn g_showWindow = nullptr;
static SetWindowPosFn g_setWindowPos = nullptr;
static HwndBoolFn g_setForeground = nullptr, g_bringToTop = nullptr;
static HwndHwndFn g_setActive = nullptr;

static bool NoActivate(HWND h, const char* call, int arg)
{
    if (!g_forceKeepRunning || !h)
        return false;
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    const bool ours = pid == GetCurrentProcessId();
    const bool block = ours && GetForegroundWindow() != GetAncestor(h, GA_ROOT);
    static int logged = 0;
    if (ours && logged < 40 && ++logged)
        Log("focus guard: %s(%p, %d) %s", call, h, arg, block ? "made non-activating" : "passed (already foreground)");
    return block;
}

// Not even on top: a window shown or raised without activation still covers the user's screen (and takes their
// clicks), so on a background test start the game's top-level windows stay at the bottom of the z-order (they render
// there as well) and are never topmost.
static bool TopLevel(HWND h) { return !(GetWindowLongA(h, GWL_STYLE) & WS_CHILD); }

static void SendToBottom(HWND h)
{
    if (GetWindowLongA(h, GWL_EXSTYLE) & WS_EX_TOPMOST)
        g_setWindowPos(h, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    g_setWindowPos(h, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

static BOOL WINAPI ShowWindowHook(HWND h, int cmd)
{
    if (NoActivate(h, "ShowWindow", cmd))
    {
        if (TopLevel(h) && cmd != SW_HIDE)
            SendToBottom(h);
        if (cmd == SW_SHOWNORMAL || cmd == SW_RESTORE || cmd == SW_SHOWDEFAULT)
            cmd = SW_SHOWNOACTIVATE;
        else if (cmd == SW_SHOW || cmd == SW_SHOWMAXIMIZED)
            cmd = SW_SHOWNA;
    }
    return g_showWindow(h, cmd);
}

static BOOL WINAPI SetWindowPosHook(HWND h, HWND after, int x, int y, int cx, int cy, UINT flags)
{
    if (NoActivate(h, "SetWindowPos", static_cast<int>(flags)))
    {
        flags |= SWP_NOACTIVATE | SWP_NOOWNERZORDER;
        if (TopLevel(h) && !(flags & SWP_NOZORDER))
            after = HWND_BOTTOM; // HWND_TOPMOST / HWND_TOP (0x4248F3) -> behind the user's windows
    }
    return g_setWindowPos(h, after, x, y, cx, cy, flags);
}

// A top-level window created visible is activated by Windows as it appears (verified 17 Oct 2026: the game window
// became the foreground window with every activating call blocked): while running in the background, create it
// hidden and show it without activation.
typedef HWND(WINAPI* CreateWindowExAFn)(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE,
                                        LPVOID);
typedef HWND(WINAPI* CreateWindowExWFn)(DWORD, LPCWSTR, LPCWSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE,
                                        LPVOID);
static CreateWindowExAFn g_createWindowA = nullptr;
static CreateWindowExWFn g_createWindowW = nullptr;

static bool HideNewWindow(DWORD& ex, DWORD& style)
{
    if (!g_forceKeepRunning || (style & WS_CHILD))
        return false;
    DWORD pid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &pid);
    if (pid == GetCurrentProcessId())
        return false; // the user is in the game
    ex &= ~WS_EX_TOPMOST;
    if (!(style & WS_VISIBLE))
        return false;
    style &= ~WS_VISIBLE;
    return true;
}

static HWND ShowNewWindow(HWND h, const char* how)
{
    if (h)
    {
        SendToBottom(h);
        g_showWindow(h, SW_SHOWNOACTIVATE);
        Log("focus guard: window %p created without activation (%s)", h, how);
    }
    return h;
}

static HWND WINAPI CreateWindowExAHook(DWORD ex, LPCSTR cls, LPCSTR name, DWORD style, int x, int y, int w, int h,
                                       HWND parent, HMENU menu, HINSTANCE inst, LPVOID param)
{
    const bool hide = HideNewWindow(ex, style);
    HWND r = g_createWindowA(ex, cls, name, style, x, y, w, h, parent, menu, inst, param);
    return hide ? ShowNewWindow(r, "A") : r;
}

static HWND WINAPI CreateWindowExWHook(DWORD ex, LPCWSTR cls, LPCWSTR name, DWORD style, int x, int y, int w, int h,
                                       HWND parent, HMENU menu, HINSTANCE inst, LPVOID param)
{
    const bool hide = HideNewWindow(ex, style);
    HWND r = g_createWindowW(ex, cls, name, style, x, y, w, h, parent, menu, inst, param);
    return hide ? ShowNewWindow(r, "W") : r;
}

static BOOL WINAPI SetForegroundWindowHook(HWND h)
{
    return NoActivate(h, "SetForegroundWindow", 0) ? TRUE : g_setForeground(h);
}
static BOOL WINAPI BringWindowToTopHook(HWND h)
{
    return NoActivate(h, "BringWindowToTop", 0) ? TRUE : g_bringToTop(h);
}
static HWND WINAPI SetActiveWindowHook(HWND h)
{
    return NoActivate(h, "SetActiveWindow", 0) ? GetActiveWindow() : g_setActive(h);
}

static void PatchFocusImports()
{
    HMODULE user32 = GetModuleHandleA("user32.dll");
    if (!user32)
        return;
    if (!g_showWindow)
    {
        g_showWindow = reinterpret_cast<ShowWindowFn>(GetProcAddress(user32, "ShowWindow"));
        g_setWindowPos = reinterpret_cast<SetWindowPosFn>(GetProcAddress(user32, "SetWindowPos"));
        g_setForeground = reinterpret_cast<HwndBoolFn>(GetProcAddress(user32, "SetForegroundWindow"));
        g_bringToTop = reinterpret_cast<HwndBoolFn>(GetProcAddress(user32, "BringWindowToTop"));
        g_setActive = reinterpret_cast<HwndHwndFn>(GetProcAddress(user32, "SetActiveWindow"));
        g_createWindowA = reinterpret_cast<CreateWindowExAFn>(GetProcAddress(user32, "CreateWindowExA"));
        g_createWindowW = reinterpret_cast<CreateWindowExWFn>(GetProcAddress(user32, "CreateWindowExW"));
    }
    typedef BOOL(WINAPI * EnumModulesFn)(HANDLE, HMODULE*, DWORD, DWORD*);
    static auto enumModules =
        reinterpret_cast<EnumModulesFn>(GetProcAddress(GetModuleHandleA("kernel32.dll"), "K32EnumProcessModules"));
    if (!enumModules || !g_showWindow || !g_setWindowPos || !g_setForeground || !g_bringToTop || !g_setActive ||
        !g_createWindowA || !g_createWindowW)
        return;
    HMODULE mods[512];
    DWORD bytes = 0;
    if (!enumModules(GetCurrentProcess(), mods, sizeof(mods), &bytes))
        return;
    const struct
    {
        const char* name;
        void* hook;
    } kHooks[] = {
        {"ShowWindow", reinterpret_cast<void*>(ShowWindowHook)},
        {"SetWindowPos", reinterpret_cast<void*>(SetWindowPosHook)},
        {"SetForegroundWindow", reinterpret_cast<void*>(SetForegroundWindowHook)},
        {"BringWindowToTop", reinterpret_cast<void*>(BringWindowToTopHook)},
        {"SetActiveWindow", reinterpret_cast<void*>(SetActiveWindowHook)},
        {"CreateWindowExA", reinterpret_cast<void*>(CreateWindowExAHook)},
        {"CreateWindowExW", reinterpret_cast<void*>(CreateWindowExWHook)},
    };
    for (DWORD i = 0; i < bytes / sizeof(HMODULE) && i < 512; ++i)
    {
        if (mods[i] == user32)
            continue;
        for (const auto& k : kHooks)
        {
            void** slot = hk::ImportSlot(mods[i], "user32.dll", k.name);
            if (slot && *slot != k.hook && hk::Write(reinterpret_cast<uintptr_t>(slot), &k.hook, sizeof(void*)))
            {
                char path[MAX_PATH] = {};
                GetModuleFileNameA(mods[i], path, MAX_PATH);
                Log("focus guard: %s of %s", k.name, strrchr(path, '\\') ? strrchr(path, '\\') + 1 : path);
            }
        }
    }
}

// The root of it: Windows asks the window's thread before any activation (CBT hook, HCBT_ACTIVATE), whatever API or
// module causes it. On a background test start the game's window thread refuses every activation the user did not
// ask for, so the game never becomes the foreground window by itself and DirectInput never takes the mouse. The
// user's own ways in stay: a mouse click (on the window or its taskbar button) or Alt+Tab.
static std::vector<std::pair<DWORD, HHOOK>> g_cbtHooks; // one per thread of the game (windows of any thread)
static SRWLOCK g_cbtLock = SRWLOCK_INIT;                // new threads (DllMain) and the watchdog add to it

// The user's own ways in: Alt (Alt+Tab), or a mouse button held over one of the game's own windows (a click on it or on
// its taskbar button - the taskbar belongs to Explorer, so that path is Windows' own activation, allowed by Alt-free
// clicks only when the cursor is over our window). Was: any mouse button anywhere - a click in another app at the
// moment the game's first window appeared let the activation through and the game took the screen.
static bool UserAsksForGame()
{
    if (GetAsyncKeyState(VK_MENU) & 0x8000)
        return true;
    if (!((GetAsyncKeyState(VK_LBUTTON) | GetAsyncKeyState(VK_RBUTTON) | GetAsyncKeyState(VK_MBUTTON)) & 0x8000))
        return false;
    POINT pt;
    if (!GetCursorPos(&pt))
        return false;
    HWND under = WindowFromPoint(pt);
    DWORD pid = 0;
    GetWindowThreadProcessId(under, &pid);
    if (pid == GetCurrentProcessId())
        return true;
    // the taskbar button: a click on Explorer's taskbar (Shell_TrayWnd / its secondary) while the game is listed there
    char cls[64] = {};
    HWND root = GetAncestor(under, GA_ROOT);
    GetClassNameA(root, cls, sizeof(cls));
    return !strcmp(cls, "Shell_TrayWnd") || !strcmp(cls, "Shell_SecondaryTrayWnd");
}

static LRESULT CALLBACK CbtHook(int code, WPARAM wp, LPARAM lp)
{
    if (code == HCBT_ACTIVATE && g_forceKeepRunning && !UserAsksForGame())
    {
        static int logged = 0;
        if (logged < 10 && ++logged)
            Log("focus guard: activation of window %p refused (background test start)", reinterpret_cast<HWND>(wp));
        return 1; // prevents the activation
    }
    return CallNextHookEx(nullptr, code, wp, lp);
}

// Hooks every thread of the process not hooked yet (new threads: the movie player's, the graphics wrappers').
static void HookAllThreads()
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return;
    THREADENTRY32 te = {sizeof(te)};
    const DWORD pid = GetCurrentProcessId();
    AcquireSRWLockExclusive(&g_cbtLock);
    const bool first = g_cbtHooks.empty();
    int added = 0;
    for (BOOL ok = Thread32First(snap, &te); ok; ok = Thread32Next(snap, &te))
    {
        if (te.th32OwnerProcessID != pid || std::any_of(g_cbtHooks.begin(), g_cbtHooks.end(),
                                                        [&](const auto& h) { return h.first == te.th32ThreadID; }))
            continue;
        if (HHOOK h = SetWindowsHookExA(WH_CBT, CbtHook, nullptr, te.th32ThreadID))
            g_cbtHooks.push_back({te.th32ThreadID, h}), ++added;
    }
    ReleaseSRWLockExclusive(&g_cbtLock);
    CloseHandle(snap);
    // after the start every new thread should come through DllMain: a find here is a thread the notification missed
    static int logged = 0;
    if (!first && added && logged < 10 && ++logged)
        Log("focus guard: the backup round hooked %d thread(s) the DLL notification missed", added);
}

// Modules loaded after the start get the import patches as they load, not at the watchdog's next round (
// the intro's DirectShow renderer, quartz.dll, loads with the first movie and raised the game window over the user's
// fullscreen browser for a moment - the black bars of the Digital Anvil logo). The loader's DLL notification
// (LdrRegisterDllNotification, ntdll, Vista+); the watchdog stays the backup.
typedef LONG(NTAPI* LdrRegisterDllNotificationFn)(ULONG, void*, void*, void**);
static VOID CALLBACK DllLoaded(ULONG reason, const void*, void*)
{
    if (reason == 1 && g_forceKeepRunning) // LDR_DLL_NOTIFICATION_REASON_LOADED
        PatchFocusImports();
}

// The backstop for any path the patches do not see: a top-level window of the game shown or moved up the z-order while
// the user is elsewhere goes back to the bottom, synchronously inside the call that raised it (an in-context WinEvent
// hook runs on that thread before the next composition: no visible frame).
static void CALLBACK ZOrderEvent(HWINEVENTHOOK, DWORD, HWND h, LONG obj, LONG, DWORD, DWORD)
{
    static thread_local bool inside = false;
    if (inside || !g_forceKeepRunning || obj != OBJID_WINDOW || UserAsksForGame())
        return;
    HWND w = h && h != GetDesktopWindow() ? GetAncestor(h, GA_ROOT) : fl::MainWindow();
    DWORD pid = 0;
    GetWindowThreadProcessId(w, &pid);
    if (!w || pid != GetCurrentProcessId() || GetForegroundWindow() == w || !IsWindowVisible(w))
        return;
    if (!GetWindow(w, GW_HWNDNEXT) && !(GetWindowLongA(w, GWL_EXSTYLE) & WS_EX_TOPMOST))
        return; // already at the bottom
    inside = true;
    SendToBottom(w);
    inside = false;
    static int logged = 0;
    if (logged < 20 && ++logged)
        Log("focus guard: window %p raised by an unpatched path - back to the bottom", w);
}

static void InstallLateGuards()
{
    if (auto reg = reinterpret_cast<LdrRegisterDllNotificationFn>(
            GetProcAddress(GetModuleHandleA("ntdll.dll"), "LdrRegisterDllNotification")))
    {
        static void* cookie = nullptr;
        Log("focus guard: DLL load notification %s",
            reg(0, reinterpret_cast<void*>(DllLoaded), nullptr, &cookie) == 0 ? "on" : "FAILED");
    }
    HMODULE self = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCSTR>(&ZOrderEvent), &self);
    const HWINEVENTHOOK ev = SetWinEventHook(EVENT_OBJECT_SHOW, EVENT_OBJECT_REORDER, self, ZOrderEvent,
                                             GetCurrentProcessId(), 0, WINEVENT_INCONTEXT);
    Log("focus guard: z-order backstop %s", ev ? "on" : "FAILED");
}

// ------------------------------------------------------------------ background test starts: silent
// deploy.ps1 -Background starts the game with FLVIBED_BACKGROUND=1 in its environment (never stored: the user's own
// starts are unaffected): it runs out of focus and nothing it plays reaches the speakers. Inside the process only,
// no Windows or game setting is touched: every stream of the process (DirectSound, the movies'
// DirectShow audio) ends in Windows' shared-mode render client, whose ReleaseBuffer gets the "silent" flag. The game
// plays everything as always (sound events, voices, timing, the subtitles), the samples handed over are silence.
bool BackgroundTestStart()
{
    char v[8] = {};
    return GetEnvironmentVariableA("FLVIBED_BACKGROUND", v, sizeof(v)) && v[0] == '1';
}

typedef HRESULT(STDMETHODCALLTYPE* ReleaseBufferFn)(void* self, UINT32 frames, DWORD flags);
static ReleaseBufferFn g_origReleaseBuffer = nullptr;
static volatile LONG g_silencedBuffers = 0;

static HRESULT STDMETHODCALLTYPE SilentReleaseBuffer(void* self, UINT32 frames, DWORD flags)
{
    InterlockedIncrement(&g_silencedBuffers);
    return g_origReleaseBuffer(self, frames, flags | AUDCLNT_BUFFERFLAGS_SILENT);
}

// The render client class is shared by every stream of the process: a client of our own (initialised, never
// started) gives its vtable; slot 4 = IAudioRenderClient::ReleaseBuffer. True when in place.
static bool InstallSilence()
{
    hk::Owner own(router::CoreOwner());
    IMMDeviceEnumerator* en = nullptr;
    IMMDevice* dev = nullptr;
    IAudioClient* client = nullptr;
    IAudioRenderClient* render = nullptr;
    WAVEFORMATEX* fmt = nullptr;
    bool ok = false;
    if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                                   reinterpret_cast<void**>(&en))) &&
        SUCCEEDED(en->GetDefaultAudioEndpoint(eRender, eConsole, &dev)) &&
        SUCCEEDED(dev->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&client))) &&
        SUCCEEDED(client->GetMixFormat(&fmt)) &&
        SUCCEEDED(client->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 1000000, 0, fmt, nullptr)) &&
        SUCCEEDED(client->GetService(__uuidof(IAudioRenderClient), reinterpret_cast<void**>(&render))))
    {
        void** vt = *reinterpret_cast<void***>(render);
        if (vt[4] != reinterpret_cast<void*>(SilentReleaseBuffer))
            g_origReleaseBuffer =
                reinterpret_cast<ReleaseBufferFn>(hk::PatchVtable(vt, 4, reinterpret_cast<void*>(SilentReleaseBuffer)));
        ok = g_origReleaseBuffer != nullptr;
    }
    if (render)
        render->Release();
    if (fmt)
        CoTaskMemFree(fmt);
    if (client)
        client->Release();
    if (dev)
        dev->Release();
    if (en)
        en->Release();
    return ok;
}

// The frame (and with it the logic) does not run while the gate is closed, and a game started without focus never
// gets an activation: a watchdog thread keeps the gate open whenever the window is not the foreground window.
static DWORD WINAPI KeepRunningWatchdog(void*)
{
    const bool quiet = BackgroundTestStart();
    if (quiet)
    {
        // First thing, before the intro movie's sound.
        const bool com = SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
        Log("background test start: running out of focus; sound %s",
            com && InstallSilence() ? "silenced inside the game process" : "NOT silenced (no render client)");
    }
    for (int tick = 0;; ++tick)
    {
        Sleep(100);
        if (quiet && tick == 100)
            Log("background test start: %ld audio buffers silenced so far", g_silencedBuffers);
        if (!g_keepRunning)
            continue;
        if (g_forceKeepRunning)
        {
            // Backup for a thread the DllMain notification missed: once a second while the game starts up (movie and
            // graphics threads appear), then every 10 s. The snapshot lists every thread of the system (~39 ms of CPU
            // each): every 100 ms it took 39 % of a core for the game's whole life (measured 21 Oct 2026 by the freeze
            // watchdog's thread table; the new threads are hooked in DllMain before their first instruction anyway).
            if (tick % (GetTickCount() - g_startTick < 30000 ? 10 : 100) == 0)
                HookAllThreads();
            // Backup for the z-order: a game window made topmost by a path the hooks do not see goes back down.
            HWND gw = fl::MainWindow();
            if (gw && GetForegroundWindow() != gw && (GetWindowLongA(gw, GWL_EXSTYLE) & WS_EX_TOPMOST))
            {
                SendToBottom(gw);
                Log("focus guard: game window was topmost, sent behind the user's windows");
            }
        }
        if (g_forceKeepRunning && tick % 10 == 0)
        {
            PatchFocusImports(); // modules loaded since (graphics wrappers)
            // The window title names the test game (every instance was just "Freelancer"): the instance
            // number (FLVIBED_INSTANCE, deploy.ps1 -Instance) or the main test install; re-applied if the game resets it.
            // WM_SETTEXT with a timeout: a stalled game must not hold this thread.
            if (HWND tw = fl::MainWindow())
            {
                static std::string title;
                if (title.empty())
                {
                    const std::string inst = InstanceName();
                    title = inst.empty() ? "Freelancer - FLvibed test, main install (background, muted)"
                                         : "Freelancer - FLvibed test instance " + inst + " (background, muted)";
                }
                std::string now(title.size() + 2, '\0'); // one more than the title: a longer text never compares equal
                DWORD_PTR r = 0;
                SendMessageTimeoutA(tw, WM_GETTEXT, now.size(), reinterpret_cast<LPARAM>(&now[0]), SMTO_ABORTIFHUNG,
                                    200, &r);
                if (lstrcmpA(now.c_str(), title.c_str()) != 0)
                    SendMessageTimeoutA(tw, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(title.c_str()), SMTO_ABORTIFHUNG,
                                        200, &r);
            }
        }
        HWND w = fl::MainWindow();
        const bool foreground = w && GetForegroundWindow() == w;
        static bool wasForeground = false;
        if (foreground != wasForeground)
            Log("focus: the game window %p (thread %lu, hooked threads %u) is %s", w,
                GetWindowThreadProcessId(w, nullptr), static_cast<u32>(g_cbtHooks.size()),
                foreground ? "now the foreground window" : "in the background");
        // Started in the background: whatever activates the game during its start goes back to the user's window
        // (the foreground process may give the foreground away). Later activations are the user's own clicks.
        if (foreground && !wasForeground && g_userWindow && GetTickCount() - g_startTick < 30000 &&
            IsWindow(g_userWindow) && g_setForeground)
        {
            g_setForeground(g_userWindow);
            Log("focus: handed back to the window that had it before the start");
        }
        wasForeground = foreground;
        if (!foreground)
        {
            g_appReallyActive = false;
            ApplyKeepRunning();
        }
    }
}

void SetKeepRunning(bool on)
{
    if (!g_started)
        return; // the Display module is off: the game pauses out of focus, as its own
    on = on || g_forceKeepRunning;
    if (on == g_keepRunning)
        return;
    g_keepRunning = on;
    Log("keep running out of focus: %s", on ? "on" : "off");
    ApplyKeepRunning();
    if (!on && !g_appReallyActive && exe::AppActive.ok())
        Mem<u8>(exe::AppActive) = 0; // inactive again: the game pauses as it would have
}

bool Forced() { return g_forceKeepRunning; }

// `display`: the module that keeps a game running. Without it a player's game keeps nothing running, so only a background test start
// hooks here.
void Start(bool display)
{
    if (!display && !BackgroundTestStart())
        return;
    g_started = true;
    g_origSetAppActive =
        static_cast<SetAppActiveFn>(hk::Detour(exe::SetAppActive, SetAppActiveHook, 6, "keep running out of focus"));
    if (g_origSetAppActive && exe::AppActive.ok())
    {
        // The option from the start (the logic re-applies it every frame): a game started without focus runs too.
        g_forceKeepRunning = BackgroundTestStart();
        g_keepRunning = g_forceKeepRunning || (display && GetPrivateProfileIntA("Controller", "run_in_background", 0,
                                                                                   (DllDir() + "FLController.ini").c_str()) != 0);
        if (g_forceKeepRunning) // a test start in the background: the game must not take the user's screen
        {
            HookAllThreads(); // the game's main thread (loading FLvibed, it creates the game window) and the others
            Log("focus guard: activation hooks on %u thread(s)", static_cast<u32>(g_cbtHooks.size()));
            PatchFocusImports(); // before the game creates and shows its window
            InstallLateGuards(); // modules loaded later (the movie renderer), paths the patches miss
            HWND fg = GetForegroundWindow();
            DWORD pid = 0;
            GetWindowThreadProcessId(fg, &pid);
            if (fg && pid != GetCurrentProcessId())
                g_userWindow = fg;
            g_startTick = GetTickCount();
        }
        if (HANDLE t = CreateThread(nullptr, 0, KeepRunningWatchdog, nullptr, 0, nullptr))
            CloseHandle(t);
    }
}

// A new thread of the game, on a background test start only (core\flvibed.cpp): guarded before its first instruction.
void ThreadAttached()
{
    if (!g_forceKeepRunning)
        return;
    if (HHOOK h = SetWindowsHookExA(WH_CBT, CbtHook, nullptr, GetCurrentThreadId()))
    {
        AcquireSRWLockExclusive(&g_cbtLock);
        g_cbtHooks.push_back({GetCurrentThreadId(), h});
        ReleaseSRWLockExclusive(&g_cbtLock);
    }
}
} // namespace focus
