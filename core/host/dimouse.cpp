// The virtual mouse (core\dimouse.h): input injection, the host core's since P2.
#include "all.h"

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

#include "rawmouse_rules.h"

namespace dimouse
{
typedef HRESULT(WINAPI* GetDeviceStateFn)(void* self, DWORD cb, void* data);
typedef HRESULT(WINAPI* GetDeviceDataFn)(void* self, DWORD cbObj, DIDEVICEOBJECTDATA* rgdod, DWORD* inOut, DWORD flags);

static GetDeviceStateFn g_origGetState = nullptr;
static GetDeviceDataFn g_origGetData = nullptr;
static void** g_hookedVtable = nullptr;
static void* g_device = nullptr;

static float g_pendX = 0, g_pendY = 0; // pixels waiting to be injected
static int g_pendWheel = 0;            // notches
static bool g_hasTarget = false;
static float g_targetX = 0, g_targetY = 0;
static bool g_button[8] = {};
static bool g_sentButton[8] = {}; // last state reported through the buffered path
static DWORD g_sequence = 0x40000000;

static u32 g_stateCalls = 0, g_dataCalls = 0, g_dataInjected = 0, g_lastDataTick = 0;
static u32 g_lastRealTick = 0; // last time the physical mouse did anything
static bool g_realButton[8] = {}; // the physical buttons as last reported (before the virtual ones are merged in)
static float g_realDx = 0, g_realDy = 0; // the physical motion since RealMotion last took it (pixels)
static int g_realWheel = 0;               // the physical wheel since RealWheel last took it (WHEEL_DELTA units)
static bool g_holdWheel = false;          // HoldRealWheel: the physical wheel counted, not passed on

static float CountScale()
{
    // Freelancer: pixels = counts * width * 0.00125 * sensitivity
    float s = fl::ScreenWidth() * 0.00125f * Mem<float>(exe::MouseSens);
    return s > 0.001f ? s : 1.0f;
}

static void ResolveTarget()
{
    if (!g_hasTarget)
        return;
    g_pendX = g_targetX - static_cast<float>(fl::MouseX());
    g_pendY = g_targetY - static_cast<float>(fl::MouseY());
}

static void TakeCounts(int& cx, int& cy)
{
    ResolveTarget();
    // Keep the virtual cursor inside the window: in windowed modes Freelancer releases
    // the mouse to Windows when its cursor leaves the client area.
    const float curX = static_cast<float>(fl::MouseX()), curY = static_cast<float>(fl::MouseY());
    const float maxX = static_cast<float>(fl::ScreenWidth() - 1), maxY = static_cast<float>(fl::ScreenHeight() - 1);
    g_pendX = Clampf(curX + g_pendX, 0, maxX) - curX;
    g_pendY = Clampf(curY + g_pendY, 0, maxY) - curY;
    const float s = CountScale();
    cx = static_cast<int>(lroundf(g_pendX / s));
    cy = static_cast<int>(lroundf(g_pendY / s));
    g_pendX -= cx * s;
    g_pendY -= cy * s;
    if (g_hasTarget)
        g_pendX = g_pendY = 0; // recomputed from the target next poll
}

// Freelancer's mouse object flag 0x40: buffered reads only carry buttons, motion comes
// from GetDeviceState. Without it, buffered reads (when used) carry motion as well.
static bool MotionViaState()
{
    if (Mem<u8>(exe::MouseObj, fl::MOUSE_FLAGS) & 0x40)
        return true;
    return (GetTickCount() - g_lastDataTick) > 250; // buffered path not in use
}

static bool g_background = false; // out of focus with keep-running: reads of the unacquired device are virtual

void SetBackground(bool on) { g_background = on; }

void BackgroundCursor()
{
    // Out of focus the game does not poll its (unacquired) mouse at all: the virtual cursor and buttons go straight
    // into the game's own cursor state, before its cursor update uses them.
    if (!g_background)
        return;
    ResolveTarget();
    const int maxX = fl::ScreenWidth() - 1, maxY = fl::ScreenHeight() - 1;
    const int x = std::clamp(static_cast<int>(lroundf(fl::MouseX() + g_pendX)), 0, std::max(0, maxX));
    const int y = std::clamp(static_cast<int>(lroundf(fl::MouseY() + g_pendY)), 0, std::max(0, maxY));
    g_pendX = g_pendY = 0;
    Mem<int>(exe::MouseX) = x;
    Mem<int>(exe::MouseY) = y;
    u32 buttons = Mem<u32>(exe::MouseButtons) & ~7u;
    for (int i = 0; i < 3; ++i)
        if (g_button[i])
            buttons |= 1u << i;
    Mem<u32>(exe::MouseButtons) = buttons;
}

static bool BackgroundFailure(HRESULT hr) { return g_background && (hr == DIERR_NOTACQUIRED || hr == DIERR_INPUTLOST); }

static volatile LONG g_injectedMotion = 0, g_hardwareMotion = 0, g_injectedButtons = 0; // written by the hook's thread, read at the game's polls

void NoteMotion(bool injected) { InterlockedExchange(injected ? &g_injectedMotion : &g_hardwareMotion, 1); }

void MaskInjectedButton(int index, bool down)
{
    if (down)
        InterlockedOr(&g_injectedButtons, 1L << index);
    else
        InterlockedAnd(&g_injectedButtons, ~(1L << index));
}

// What the low-level hook noted since the last read: a mapper's motion dropped (injected), real motion (hardware)
static void TakeInjected(bool& injected, bool& hardware)
{
    injected = InterlockedExchange(&g_injectedMotion, 0) != 0;
    hardware = InterlockedExchange(&g_hardwareMotion, 0) != 0;
}

// ------------------------------------------------------------------ the raw mouse (setting `mouse_raw`, rawmouse_rules.h)
// The game's DirectInput mouse carries a speed-dependent gain (3-4 px per raw count slow, 5.7-7 fast; docs\FREELANCER_RE.md "## Mouse"):
// a Raw Input listener of the host, on its own thread with a message-only window, counts the mouse's own motion, and the read that
// carries motion takes those counts in place of the device's lX / lY. It never takes the mouse from another part of the process: the
// mouse's raw input (usage page 1, usage 2) is registered only when nobody in the process has it, never with RIDEV_NOLEGACY (the game
// needs its mouse messages), and checked every second while on; any doubt leaves the game's own mouse for the run.
enum RawStatus : LONG
{
    kRawOff = 0,        // the setting
    kRawStarting,       // the thread registers
    kRawOn,             // the listener holds the mouse's registration
    kRawNoThread,       // off for the run: no thread / window / registration
    kRawNoWindow,
    kRawRegisterFailed,
    kRawTaken,          // off for the run: the mouse was registered by another part of the process before ours
    kRawLost            // off for the run: our registration was replaced or removed while on
};
static const char* const kRawStatusNames[] = {"off (mouse_raw 0)", "starting", "on", "off (no thread)", "off (no window)",
                                              "off (registration refused)", "off (the mouse's raw input is another's)",
                                              "off (registration lost)"};
static volatile LONG g_rawStatus = kRawOff;
static bool g_rawStuck = false; // off for the run: a failure, or another part of the process holds the mouse
static HANDLE g_rawThread = nullptr, g_rawStop = nullptr;
static HWND g_rawWnd = nullptr; // the listener's window, set before kRawOn
static volatile LONG g_rawHwX = 0, g_rawHwY = 0, g_rawInjX = 0, g_rawInjY = 0; // counts since the last read
static volatile LONG g_rawHwMoves = 0, g_rawInjMoves = 0, g_rawAbsolute = 0;    // relative motion events since the last read; absolute seen
static volatile LONG g_rawLastMove = 0, g_rawHeard = 0;                         // GetTickCount of the last relative motion event; any yet
static volatile LONG g_rawHwTotal = 0, g_rawInjTotal = 0, g_rawAbsTotal = 0, g_rawBehind = 0; // since the start (debug `state`)
static rawmouse::State g_rawState;
static u32 g_readsBy[5] = {}; // reads by rawmouse::Source
static u32 g_silentSaid = 0;

static void NoteRaw(const RAWINPUT& ri)
{
    const RAWMOUSE& m = ri.data.mouse;
    if (m.usFlags & MOUSE_MOVE_ABSOLUTE)
    {
        InterlockedExchange(&g_rawAbsolute, 1);
        InterlockedIncrement(&g_rawAbsTotal);
        return;
    }
    if (!m.lLastX && !m.lLastY)
        return; // a button or the wheel: DirectInput's
    const bool injected = ri.header.hDevice == nullptr;
    InterlockedIncrement(injected ? &g_rawInjTotal : &g_rawHwTotal);
    DWORD front = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &front);
    if (front != GetCurrentProcessId()) // (the process, not the main window: a wrapper's window in front is the game's too)
    {
        InterlockedIncrement(&g_rawBehind); // the game behind: nothing of the real mouse may reach it, now or when it comes back
        return;
    }
    InterlockedExchangeAdd(injected ? &g_rawInjX : &g_rawHwX, m.lLastX);
    InterlockedExchangeAdd(injected ? &g_rawInjY : &g_rawHwY, m.lLastY);
    InterlockedIncrement(injected ? &g_rawInjMoves : &g_rawHwMoves);
    InterlockedExchange(&g_rawLastMove, static_cast<LONG>(GetTickCount()));
    InterlockedExchange(&g_rawHeard, 1);
}

static LRESULT CALLBACK RawWndProc(HWND wnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_INPUT)
    {
        RAWINPUT ri;
        UINT size = sizeof(ri);
        if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lp), RID_INPUT, &ri, &size, sizeof(RAWINPUTHEADER)) != static_cast<UINT>(-1) &&
            ri.header.dwType == RIM_TYPEMOUSE)
            NoteRaw(ri);
    }
    return DefWindowProcA(wnd, msg, wp, lp); // (WM_INPUT too: the system frees the input)
}

// The mouse's raw input registration in this process: 1 registered (target in `target`), 0 not, -1 unknown. `log` lists every
// registration the process holds.
static int MouseRegistration(HWND* target, bool log)
{
    RAWINPUTDEVICE list[32];
    UINT n = 32;
    const UINT got = GetRegisteredRawInputDevices(list, &n, sizeof(RAWINPUTDEVICE));
    if (got == static_cast<UINT>(-1))
    {
        if (log)
            Log("dimouse: raw input: the process's registrations unreadable (error %lu, %u)", GetLastError(), n);
        return -1;
    }
    if (log && !got)
        Log("dimouse: raw input: the process has no raw input registration before FLvibed's");
    int found = 0;
    for (UINT i = 0; i < got; ++i)
    {
        char cls[64] = "-";
        if (log)
        {
            if (list[i].hwndTarget)
                GetClassNameA(list[i].hwndTarget, cls, sizeof(cls));
            Log("dimouse: raw input: the process has page 0x%02X usage 0x%02X flags 0x%08lX target %p (class %s)", list[i].usUsagePage,
                list[i].usUsage, list[i].dwFlags, list[i].hwndTarget, cls);
        }
        if (list[i].usUsagePage == 1 && list[i].usUsage == 2)
        {
            found = 1;
            if (target)
                *target = list[i].hwndTarget;
        }
    }
    return found;
}

static void RawFail(RawStatus why, const char* what)
{
    Log("dimouse: raw mouse input off for this run: %s - the game's own mouse", what);
    InterlockedExchange(&g_rawStatus, why);
}

static DWORD WINAPI RawThread(void*)
{
    static const char kClass[] = "FLvibedRawMouse";
    HMODULE self = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCSTR>(&RawThread), &self);
    WNDCLASSA wc = {};
    wc.lpfnWndProc = RawWndProc;
    wc.hInstance = self;
    wc.lpszClassName = kClass;
    RegisterClassA(&wc); // (a second start finds it registered)
    HWND wnd = CreateWindowExA(0, kClass, "", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, self, nullptr);
    if (!wnd)
    {
        char what[64];
        sprintf_s(what, "no message window (error %lu)", GetLastError());
        RawFail(kRawNoWindow, what);
        return 0;
    }
    HWND other = nullptr;
    const int before = MouseRegistration(&other, true);
    if (before != 0)
    {
        RawFail(before > 0 ? kRawTaken : kRawRegisterFailed,
                before > 0 ? "the mouse's raw input is already registered in the process (DirectInput or a wrapper): registering would take it away"
                           : "the process's raw input registrations could not be read");
        DestroyWindow(wnd);
        return 0;
    }
    RAWINPUTDEVICE d = {1, 2, RIDEV_INPUTSINK, wnd}; // (never RIDEV_NOLEGACY: the game needs its mouse messages)
    if (!RegisterRawInputDevices(&d, 1, sizeof(d)))
    {
        char what[64];
        sprintf_s(what, "RegisterRawInputDevices failed (error %lu)", GetLastError());
        RawFail(kRawRegisterFailed, what);
        DestroyWindow(wnd);
        return 0;
    }
    g_rawWnd = wnd;
    InterlockedExchange(&g_rawStatus, kRawOn);
    Log("dimouse: raw mouse input on (window %p): the game's reads take the mouse's own counts, 1 raw count = 1 DirectInput count", wnd);
    for (;;)
    {
        if (MsgWaitForMultipleObjects(1, &g_rawStop, FALSE, INFINITE, QS_ALLINPUT) != WAIT_OBJECT_0 + 1)
            break; // (the stop event, or a failed wait)
        MSG msg;
        while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE))
            DispatchMessageA(&msg);
    }
    HWND now = nullptr;
    if (MouseRegistration(&now, false) > 0 && now == wnd) // (never another's: a replaced registration stays theirs)
    {
        RAWINPUTDEVICE remove = {1, 2, RIDEV_REMOVE, nullptr};
        RegisterRawInputDevices(&remove, 1, sizeof(remove));
    }
    DestroyWindow(wnd);
    return 0;
}

static void ClearRaw()
{
    InterlockedExchange(&g_rawHwX, 0), InterlockedExchange(&g_rawHwY, 0);
    InterlockedExchange(&g_rawInjX, 0), InterlockedExchange(&g_rawInjY, 0);
    InterlockedExchange(&g_rawHwMoves, 0), InterlockedExchange(&g_rawInjMoves, 0), InterlockedExchange(&g_rawAbsolute, 0);
}

static void StopRaw(RawStatus after)
{
    if (g_rawThread)
    {
        SetEvent(g_rawStop);
        if (WaitForSingleObject(g_rawThread, 1000) != WAIT_OBJECT_0)
        {
            Log("dimouse: the raw mouse thread did not stop within 1 s - left, off for this run");
            after = kRawNoThread, g_rawStuck = true;
        }
        else
            CloseHandle(g_rawThread), CloseHandle(g_rawStop);
        g_rawThread = g_rawStop = nullptr;
        g_rawWnd = nullptr;
    }
    InterlockedExchange(&g_rawStatus, after);
    g_rawState = {};
    ClearRaw();
}

void SetRaw(bool on)
{
    const LONG status = g_rawStatus;
    if (g_rawThread && status >= kRawNoThread) // the thread failed and ended by itself: off for the run
    {
        WaitForSingleObject(g_rawThread, 1000);
        CloseHandle(g_rawThread), CloseHandle(g_rawStop);
        g_rawThread = g_rawStop = nullptr;
        g_rawStuck = true;
        return;
    }
    if (on && !g_rawThread && !g_rawStuck)
    {
        ClearRaw();
        g_rawState = {};
        g_rawStop = CreateEventA(nullptr, TRUE, FALSE, nullptr);
        InterlockedExchange(&g_rawStatus, kRawStarting);
        g_rawThread = g_rawStop ? CreateThread(nullptr, 0, RawThread, nullptr, 0, nullptr) : nullptr;
        if (!g_rawThread)
        {
            char what[64];
            sprintf_s(what, "no thread (error %lu)", GetLastError());
            RawFail(kRawNoThread, what);
            if (g_rawStop)
                CloseHandle(g_rawStop), g_rawStop = nullptr;
            g_rawStuck = true;
        }
    }
    else if (!on && g_rawThread)
    {
        StopRaw(kRawOff);
        Log("dimouse: raw mouse input off (mouse_raw 0): the game's own mouse");
    }
}

// While on, once a second: the registration is still ours. Replaced (DirectInput or a wrapper registering after us) or removed: off for
// the run, the other's registration left alone.
static void RawVerify()
{
    static DWORD last = 0;
    const DWORD now = GetTickCount();
    if (g_rawStatus != kRawOn || now - last < 1000)
        return;
    last = now;
    HWND target = nullptr;
    const int r = MouseRegistration(&target, false);
    if (r == 1 && target == g_rawWnd)
        return;
    Log("dimouse: raw mouse input lost: the mouse's raw input is now %s - off for this run, the game's own mouse",
        r == 1 ? "another window's" : r == 0 ? "unregistered" : "unreadable");
    if (r == 1)
        MouseRegistration(nullptr, true);
    StopRaw(kRawLost);
    g_rawStuck = true;
}

// One read that carries motion: `x` / `y` hold the device's own and get what the game is given (rawmouse::Decide). `acquired` false (the call
// failed, the game behind): the sums since the last read are dropped.
static rawmouse::Source RawMotion(bool acquired, bool dropInjected, LONG& x, LONG& y)
{
    rawmouse::ReadIn in;
    in.rawOn = g_rawStatus == kRawOn;
    in.acquired = acquired;
    in.dropInjected = dropInjected;
    in.absoluteSeen = InterlockedExchange(&g_rawAbsolute, 0) != 0;
    in.hwEvents = InterlockedExchange(&g_rawHwMoves, 0), in.injEvents = InterlockedExchange(&g_rawInjMoves, 0);
    in.hwX = InterlockedExchange(&g_rawHwX, 0), in.hwY = InterlockedExchange(&g_rawHwY, 0);
    in.injX = InterlockedExchange(&g_rawInjX, 0), in.injY = InterlockedExchange(&g_rawInjY, 0);
    in.msSinceRaw = g_rawHeard ? GetTickCount() - static_cast<DWORD>(g_rawLastMove) : ~0u;
    in.diX = x, in.diY = y;
    const bool wasSilent = g_rawState.passThrough;
    const rawmouse::ReadOut out = rawmouse::Decide(in, g_rawState);
    ++g_readsBy[static_cast<int>(out.source)];
    if (out.source == rawmouse::Source::Silent && !wasSilent && g_silentSaid++ < 5)
        Log("dimouse: raw mouse silent while DirectInput moved (%u ms): DirectInput's motion until the next raw hardware event", in.msSinceRaw);
    if (acquired)
        x = out.x, y = out.y;
    return out.source;
}

static HRESULT WINAPI GetDeviceStateHook(void* self, DWORD cb, void* data)
{
    HRESULT hr = g_origGetState(self, cb, data);
    const bool acquired = SUCCEEDED(hr) && !g_background; // (the real mouse reaches the game only through a device it holds, in front)
    if (self == g_device && data && cb >= sizeof(DIMOUSESTATE) && BackgroundFailure(hr))
    {
        memset(data, 0, cb); // no physical input: the virtual mouse alone
        hr = DI_OK;
    }
    if (self == g_device && !acquired && MotionViaState())
    {
        LONG x = 0, y = 0;
        RawMotion(false, false, x, y); // (the raw sums dropped)
    }
    if (self != g_device || !data || cb < sizeof(DIMOUSESTATE) || FAILED(hr))
        return hr;

    ++g_stateCalls;
    DIMOUSESTATE2* st = static_cast<DIMOUSESTATE2*>(data);
    bool injected, hardware;
    TakeInjected(injected, hardware);
    if (injected && !hardware)
        st->lX = st->lY = st->lZ = 0;
    if (acquired && MotionViaState())
        RawMotion(true, injected, st->lX, st->lY); // raw counts in place of the device's (a mapper's dropped: the hardware's alone)
    if (const LONG mask = g_injectedButtons)
        for (int i = 0; i < 4; ++i)
            if (mask & (1L << i))
                st->rgbButtons[i] &= 0x7F;
    if (st->lX || st->lY || st->lZ || (st->rgbButtons[0] | st->rgbButtons[1] | st->rgbButtons[2]) & 0x80)
        g_lastRealTick = GetTickCount();
    for (int i = 0; i < (cb >= sizeof(DIMOUSESTATE2) ? 8 : 4); ++i)
        g_realButton[i] = (st->rgbButtons[i] & 0x80) != 0;
    if (MotionViaState())
        g_realDx += st->lX * CountScale(), g_realDy += st->lY * CountScale(), g_realWheel += st->lZ;
    if (g_holdWheel)
        st->lZ = 0;
    if (MotionViaState())
    {
        int cx, cy;
        TakeCounts(cx, cy);
        st->lX += cx;
        st->lY += cy;
        st->lZ += g_pendWheel * WHEEL_DELTA;
        g_pendWheel = 0;
    }
    const int count = cb >= sizeof(DIMOUSESTATE2) ? 8 : 4;
    for (int i = 0; i < count; ++i)
        if (g_button[i])
            st->rgbButtons[i] |= 0x80;
    return hr;
}

static HRESULT WINAPI GetDeviceDataHook(void* self, DWORD cbObj, DIDEVICEOBJECTDATA* rgdod, DWORD* inOut, DWORD flags)
{
    const DWORD capacity = inOut ? *inOut : 0;
    HRESULT hr = g_origGetData(self, cbObj, rgdod, inOut, flags);
    const bool acquired = SUCCEEDED(hr) && !g_background;
    const bool motionHere = !(Mem<u8>(exe::MouseObj, fl::MOUSE_FLAGS) & 0x40); // (flag 0x40: motion comes by GetDeviceState, the flags wait for it)
    if (self == g_device && inOut && BackgroundFailure(hr))
    {
        *inOut = 0; // no physical events: the virtual ones alone
        hr = DI_OK;
    }
    if (self == g_device && !acquired && motionHere && !(flags & DIGDD_PEEK))
    {
        LONG x = 0, y = 0;
        RawMotion(false, false, x, y); // (the raw sums dropped)
    }
    if (self != g_device || !rgdod || !inOut || FAILED(hr) || (flags & DIGDD_PEEK) ||
        cbObj != sizeof(DIDEVICEOBJECTDATA))
        return hr;

    ++g_dataCalls;
    g_lastDataTick = GetTickCount();
    DWORD n = *inOut;
    const LONG mask = g_injectedButtons;
    bool injected = false, hardware = false;
    if (motionHere)
        TakeInjected(injected, hardware);
    const bool motionOff = injected && !hardware;
    if (mask || motionOff) // a mapper's events out of the buffer
    {
        DWORD kept = 0;
        for (DWORD i = 0; i < n; ++i)
        {
            const DWORD ofs = rgdod[i].dwOfs;
            const bool button = ofs >= DIMOFS_BUTTON0 && ofs <= DIMOFS_BUTTON7;
            if (button ? (mask & (1L << (ofs - DIMOFS_BUTTON0))) != 0 : motionOff)
                continue;
            rgdod[kept++] = rgdod[i];
        }
        *inOut = n = kept;
    }
    if (motionHere && acquired) // the raw counts in place of the device's X / Y events (as GetDeviceStateHook's lX / lY)
    {
        LONG x = 0, y = 0;
        for (DWORD i = 0; i < n; ++i)
            if (rgdod[i].dwOfs == DIMOFS_X)
                x += static_cast<int>(rgdod[i].dwData);
            else if (rgdod[i].dwOfs == DIMOFS_Y)
                y += static_cast<int>(rgdod[i].dwData);
        if (RawMotion(true, injected, x, y) == rawmouse::Source::Raw)
        {
            DWORD kept = 0;
            for (DWORD i = 0; i < n; ++i)
                if (rgdod[i].dwOfs != DIMOFS_X && rgdod[i].dwOfs != DIMOFS_Y)
                    rgdod[kept++] = rgdod[i];
            n = kept;
            const LONG value[2] = {x, y};
            const DWORD axis[2] = {static_cast<DWORD>(DIMOFS_X), static_cast<DWORD>(DIMOFS_Y)};
            for (int a = 0; a < 2; ++a)
                if (value[a] && n < capacity)
                {
                    rgdod[n].dwOfs = axis[a];
                    rgdod[n].dwData = static_cast<DWORD>(value[a]);
                    rgdod[n].dwTimeStamp = GetTickCount();
                    rgdod[n].dwSequence = ++g_sequence;
                    rgdod[n].uAppData = 0;
                    ++n;
                }
            *inOut = n;
        }
    }
    if (n)
        g_lastRealTick = g_lastDataTick;
    for (DWORD i = 0; i < n; ++i) // the physical events (ours are appended below)
    {
        if (rgdod[i].dwOfs >= DIMOFS_BUTTON0 && rgdod[i].dwOfs <= DIMOFS_BUTTON7)
            g_realButton[rgdod[i].dwOfs - DIMOFS_BUTTON0] = (rgdod[i].dwData & 0x80) != 0;
        else if (rgdod[i].dwOfs == DIMOFS_X && !(Mem<u8>(exe::MouseObj, fl::MOUSE_FLAGS) & 0x40))
            g_realDx += static_cast<int>(rgdod[i].dwData) * CountScale();
        else if (rgdod[i].dwOfs == DIMOFS_Y && !(Mem<u8>(exe::MouseObj, fl::MOUSE_FLAGS) & 0x40))
            g_realDy += static_cast<int>(rgdod[i].dwData) * CountScale();
        else if (rgdod[i].dwOfs == DIMOFS_Z && !(Mem<u8>(exe::MouseObj, fl::MOUSE_FLAGS) & 0x40))
            g_realWheel += static_cast<int>(rgdod[i].dwData);
    }
    if (g_holdWheel) // (counted above, kept from the game)
    {
        DWORD kept = 0;
        for (DWORD i = 0; i < n; ++i)
            if (rgdod[i].dwOfs != DIMOFS_Z)
                rgdod[kept++] = rgdod[i];
        *inOut = n = kept;
    }
    const DWORD now = GetTickCount();
    auto push = [&](DWORD ofs, DWORD value)
    {
        if (n >= capacity)
            return false;
        rgdod[n].dwOfs = ofs;
        rgdod[n].dwData = value;
        rgdod[n].dwTimeStamp = now;
        rgdod[n].dwSequence = ++g_sequence;
        rgdod[n].uAppData = 0;
        ++n;
        ++g_dataInjected;
        return true;
    };

    if (!(Mem<u8>(exe::MouseObj, fl::MOUSE_FLAGS) & 0x40))
    {
        int cx, cy;
        TakeCounts(cx, cy);
        if (cx)
            push(DIMOFS_X, static_cast<DWORD>(cx));
        if (cy)
            push(DIMOFS_Y, static_cast<DWORD>(cy));
        if (g_pendWheel)
        {
            if (push(DIMOFS_Z, static_cast<DWORD>(g_pendWheel * WHEEL_DELTA)))
                g_pendWheel = 0;
        }
    }
    for (int i = 0; i < 8; ++i)
    {
        if (g_button[i] != g_sentButton[i])
        {
            if (push(DIMOFS_BUTTON0 + i, g_button[i] ? 0x80 : 0))
                g_sentButton[i] = g_button[i];
        }
    }
    *inOut = n;
    return hr;
}

// A window that covers its whole monitor (borderless - FLvibed's, the game's own style or a wrapper's) keeps the mouse
// as exclusive fullscreen does: the mouse object's flag 0x10 (clamp at the edges instead of releasing). The game sets it
// only from its fullscreen flag (0x420350's last argument, 0x6679F0), which borderless leaves off, so it let the mouse go
// at every screen edge and took it back with a jump ("goes out of bounds, swung back to the
// other side"). The cover test runs twice a second, the flag every frame (each capture rewrites it); exclusive
// fullscreen has it already (left alone); a bordered window (FLUF's Alt+Enter) gets the game's own behaviour back.
static void ClampInBorderless()
{
    static DWORD lastCheck = 0;
    static bool covers = false, set = false; // set: the flag is ours
    const DWORD now = GetTickCount();
    if (now - lastCheck >= 500)
    {
        lastCheck = now;
        HWND wnd = Mem<HWND>(exe::MainHwnd);
        RECT client = {};
        POINT tl = {0, 0};
        MONITORINFO mi = {sizeof(mi)};
        const bool was = covers;
        covers = wnd && GetClientRect(wnd, &client) && ClientToScreen(wnd, &tl) &&
                 GetMonitorInfoW(MonitorFromWindow(wnd, MONITOR_DEFAULTTONEAREST), &mi) &&
                 tl.x <= mi.rcMonitor.left && tl.y <= mi.rcMonitor.top && tl.x + client.right >= mi.rcMonitor.right &&
                 tl.y + client.bottom >= mi.rcMonitor.bottom;
        if (covers != was)
            Log("dimouse: the window %s its monitor - %s", covers ? "covers" : "no longer covers",
                covers ? "the mouse stays in at the edges (as in fullscreen)" : "the game's edge release");
    }
    u8& flags = Mem<u8>(exe::MouseObj, fl::MOUSE_FLAGS);
    if (covers && !(flags & 0x10))
        flags |= 0x10, set = true;
    else if (!covers && set)
        flags &= ~0x10, set = false;
}

void Update()
{
    RawVerify();
    void* dev = Mem<void*>(exe::MouseObj, fl::MOUSE_DI_DEVICE);
    if (!dev)
        return;
    ClampInBorderless();
    if (dev != g_device)
    {
        g_device = dev;
        Log("dimouse: game mouse device %p (flags 0x%02X)", dev, Mem<u8>(exe::MouseObj, fl::MOUSE_FLAGS));
    }
    void** vtable = *static_cast<void***>(dev);
    if (vtable != g_hookedVtable && vtable[9] != GetDeviceStateHook)
    {
        hk::Owner own(router::CoreOwner());
        g_origGetState = static_cast<GetDeviceStateFn>(hk::PatchVtable(vtable, 9, GetDeviceStateHook));
        g_origGetData = static_cast<GetDeviceDataFn>(hk::PatchVtable(vtable, 10, GetDeviceDataHook));
        g_hookedVtable = vtable;
        Log("dimouse: hooked device vtable %p", vtable);
    }
}

void MoveBy(float dx, float dy)
{
    g_hasTarget = false;
    g_pendX += dx;
    g_pendY += dy;
}

void MoveTo(float x, float y)
{
    g_hasTarget = true;
    g_targetX = x;
    g_targetY = y;
}

void ReleaseTarget()
{
    g_hasTarget = false;
    g_pendX = g_pendY = 0;
}

void Wheel(int notches) { g_pendWheel += notches; }
void HoldRealWheel(bool on) { g_holdWheel = on; }

void SetButton(int index, bool down)
{
    if (index >= 0 && index < 8)
        g_button[index] = down;
}

bool ButtonDown(int index) { return index >= 0 && index < 8 && g_button[index]; }

bool IsCaptured() { return (Mem<u8>(exe::MouseObj, fl::MOUSE_FLAGS) & 1) != 0; }

u32 LastRealTick() { return g_lastRealTick; }

bool RealButtonDown(int index) { return index >= 0 && index < 8 && g_realButton[index]; }

bool RealMotion(int* dx, int* dy)
{
    const int x = static_cast<int>(lroundf(g_realDx)), y = static_cast<int>(lroundf(g_realDy));
    g_realDx -= x, g_realDy -= y;
    if (dx)
        *dx = x;
    if (dy)
        *dy = y;
    return x != 0 || y != 0;
}

int RealWheel()
{
    const int notches = g_realWheel / WHEEL_DELTA; // whole notches; the rest stays for the next call
    g_realWheel -= notches * WHEEL_DELTA;
    return notches;
}

void Capture()
{
    if (!IsCaptured())
    {
        hk::Call<void(__cdecl*)(bool)>(exe::SetMouseCapture, true);
        Log("dimouse: re-captured game mouse (flags 0x%02X)", Mem<u8>(exe::MouseObj, fl::MOUSE_FLAGS));
    }
}

void Reset()
{
    memset(g_button, 0, sizeof(g_button));
    g_pendX = g_pendY = 0;
    g_pendWheel = 0;
    g_hasTarget = false;
}

std::string Stats()
{
    char buf[512];
    const LONG status = g_rawStatus;
    sprintf_s(buf,
              "device=%p flags=0x%02X stateCalls=%u dataCalls=%u injectedEvents=%u scale=%.3f raw=%s rawHwEvents=%ld rawInjEvents=%ld "
              "rawAbsEvents=%ld rawBehind=%ld readsRaw=%u readsDiOff=%u readsDiAbsolute=%u readsDiSilent=%u readsNothing=%u",
              g_device, g_device ? Mem<u8>(exe::MouseObj, fl::MOUSE_FLAGS) : 0, g_stateCalls, g_dataCalls, g_dataInjected, CountScale(),
              status >= 0 && status <= kRawLost ? kRawStatusNames[status] : "?", static_cast<long>(g_rawHwTotal),
              static_cast<long>(g_rawInjTotal), static_cast<long>(g_rawAbsTotal), static_cast<long>(g_rawBehind),
              g_readsBy[static_cast<int>(rawmouse::Source::Raw)], g_readsBy[static_cast<int>(rawmouse::Source::Off)],
              g_readsBy[static_cast<int>(rawmouse::Source::Absolute)], g_readsBy[static_cast<int>(rawmouse::Source::Silent)],
              g_readsBy[static_cast<int>(rawmouse::Source::Nothing)]);
    return buf;
}
} // namespace dimouse
