#include "../dimouse.h"

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

#include "../fl.h"
#include "hook.h"
#include "log.h"

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

static HRESULT WINAPI GetDeviceStateHook(void* self, DWORD cb, void* data)
{
    HRESULT hr = g_origGetState(self, cb, data);
    if (self == g_device && data && cb >= sizeof(DIMOUSESTATE) && BackgroundFailure(hr))
    {
        memset(data, 0, cb); // no physical input: the virtual mouse alone
        hr = DI_OK;
    }
    if (self != g_device || !data || cb < sizeof(DIMOUSESTATE) || FAILED(hr))
        return hr;

    ++g_stateCalls;
    DIMOUSESTATE2* st = static_cast<DIMOUSESTATE2*>(data);
    if (st->lX || st->lY || st->lZ || (st->rgbButtons[0] | st->rgbButtons[1] | st->rgbButtons[2]) & 0x80)
        g_lastRealTick = GetTickCount();
    for (int i = 0; i < (cb >= sizeof(DIMOUSESTATE2) ? 8 : 4); ++i)
        g_realButton[i] = (st->rgbButtons[i] & 0x80) != 0;
    if (MotionViaState())
        g_realDx += st->lX * CountScale(), g_realDy += st->lY * CountScale(), g_realWheel += st->lZ;
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
    if (self == g_device && inOut && BackgroundFailure(hr))
    {
        *inOut = 0; // no physical events: the virtual ones alone
        hr = DI_OK;
    }
    if (self != g_device || !rgdod || !inOut || FAILED(hr) || (flags & DIGDD_PEEK) ||
        cbObj != sizeof(DIDEVICEOBJECTDATA))
        return hr;

    ++g_dataCalls;
    g_lastDataTick = GetTickCount();
    DWORD n = *inOut;
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
// at every screen edge and took it back with a jump (the owner, 26 Oct 2026: "goes out of bounds, swung back to the
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
    char buf[256];
    sprintf_s(buf, "device=%p flags=0x%02X stateCalls=%u dataCalls=%u injectedEvents=%u scale=%.3f", g_device,
              g_device ? Mem<u8>(exe::MouseObj, fl::MOUSE_FLAGS) : 0, g_stateCalls, g_dataCalls, g_dataInjected,
              CountScale());
    return buf;
}
} // namespace dimouse
