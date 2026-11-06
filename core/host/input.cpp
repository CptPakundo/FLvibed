// FLvibed - input (docs\MODULES_PLAN.md: core): injection (the game's held commands, the mouse-flight steering; the virtual mouse is
// its other half, dimouse.cpp), installed when a module that injects is on; and the raw keys and the UI hit test, always.
#include "all.h"

#include "input.h"

namespace input
{
static bool g_held[fl::CMD_COUNT];

void SetHeld(int cmd, bool held)
{
    if (cmd > 0 && cmd < fl::CMD_COUNT)
        g_held[cmd] = held;
}
void ClearAllHeld() { memset(g_held, 0, sizeof(g_held)); }

typedef bool(__cdecl* IsCmdHeldFn)(int cmd);
static IsCmdHeldFn g_origIsCmdHeld = nullptr;

static bool __cdecl IsCmdHeldHook(int cmd)
{
    if (cmd > 0 && cmd < fl::CMD_COUNT && g_held[cmd])
        return true;
    return g_origIsCmdHeld(cmd);
}

typedef void(__cdecl* SteerFn)();
static SteerFn g_origSteer = nullptr;
static bool g_hudSuppress = false; // the HUD's roots are out of the hit test (SetHudHitSuppression)
static bool g_steerUnderWindow = false; // SetSteerLift: the autopilot flies and a window or a menu has the cursor
static bool g_steerAutopilot = false;   // SetSteerLift: the autopilot is on, so the rear view is the player's view and not a lock
static bool g_steerActive = false;
static float g_steerX = 0, g_steerY = 0;
static int g_steerStopCalls = 0; // SteerStop: zero calls still to send

void SetSteer(bool active, float x, float y)
{
    g_steerActive = active;
    g_steerX = Clampf(x, -1, 1);
    g_steerY = Clampf(y, -1, 1);
    g_steerStopCalls = 0;
}

void SetSteerLift(bool underWindow, bool autopilot)
{
    g_steerUnderWindow = underWindow;
    g_steerAutopilot = autopilot;
}

void SteerStop()
{
    if (!g_steerActive)
        return;
    g_steerX = g_steerY = 0;
    g_steerStopCalls = 3;
}

// Freelancer steers towards the cursor (relative to the screen center) while mouse flight is
// on. For the pad we feed the stick direction to that same code for the duration of the call,
// so every game rule (trade lanes, autopilot, cutscenes, open windows) still applies, and the
// real cursor stays free for aiming.
static void __cdecl SteerHook()
{
    if (!g_steerActive)
    {
        g_origSteer();
        return;
    }
    const int savedX = Mem<int>(exe::MouseX), savedY = Mem<int>(exe::MouseY);
    const u8 savedToggle = Mem<u8>(exe::MouseFlightToggle);
    // Engage timer / engaged flag: the steering only applies once the timer passes 0.25 s, and
    // other code (HUD "MOUSE FLIGHT" label, cursor) treats a timer >= 0.25 as "mouse flight
    // on". Pretend it is engaged for this call only, then restore the player's real state.
    const float savedTimer = Mem<float>(exe::MouseFlightTimer);
    const u8 savedEngaged = Mem<u8>(exe::MouseFlightEngaged);
    const float halfW = fl::ScreenWidth() * 0.5f, halfH = fl::ScreenHeight() * 0.5f;
    Mem<int>(exe::MouseX) = static_cast<int>(lroundf(halfW + g_steerX * halfW));
    Mem<int>(exe::MouseY) = static_cast<int>(lroundf(halfH - g_steerY * halfH));
    Mem<u8>(exe::MouseFlightToggle) = 1;
    Mem<float>(exe::MouseFlightTimer) = 1.0f;
    // A hovered control refuses steering that is not engaged yet (0x4C7D62): the autopilot flying under a NeuroNet window, where the
    // cursor rests on the window, would never turn. Only then: a locked ship (a lane, a cutscene) with the flag set would be disengaged
    // again by the game on every frame, and plain flight stays as it was. The suppression alone does not say it: a menu that is no NeuroNet
    // window, the pause menu in multiplayer, keeps it on.
    if ((!g_hudSuppress || g_steerUnderWindow) && exe::HoveredControl.ok() && Mem<uintptr_t>(exe::HoveredControl))
        Mem<u8>(exe::MouseFlightEngaged) = 1;
    // Turret view locks the steering (exe::PlayerFlightState +0x5B9: the steering call skips the behaviour update): the
    // player's view, not the ship's. While FLvibed steers (the autopilot flying, the owner 28 Oct 2026: it keeps full
    // control in turret view) the lock is lifted for this call only - the camera stays the turret's.
    // The rear view (+0x5BA) stops the same update: the autopilot flies on through it, the player's own flight keeps the game's rule.
    u8* turretLock = nullptr;
    u8* rearLock = nullptr;
    u8 savedLock = 0, savedRear = 0;
    if (exe::PlayerFlightState.ok())
        if (const uintptr_t st = Mem<uintptr_t>(exe::PlayerFlightState))
        {
            turretLock = reinterpret_cast<u8*>(st + 0x5B9);
            savedLock = *turretLock;
            *turretLock = 0;
            if (g_steerAutopilot)
            {
                rearLock = reinterpret_cast<u8*>(st + 0x5BA);
                savedRear = *rearLock;
                *rearLock = 0;
            }
        }
    g_origSteer();
    if (turretLock)
        *turretLock = savedLock;
    if (rearLock)
        *rearLock = savedRear;
    Mem<int>(exe::MouseX) = savedX;
    Mem<int>(exe::MouseY) = savedY;
    Mem<u8>(exe::MouseFlightToggle) = savedToggle;
    Mem<float>(exe::MouseFlightTimer) = savedTimer;
    Mem<u8>(exe::MouseFlightEngaged) = savedEngaged;
    if (g_steerStopCalls > 0 && --g_steerStopCalls == 0)
        g_steerActive = false;
}

// ------------------------------------------------------------------ v18: raw keys (exe::WinKey) -> the logic
// The game's keyboard message handler sees every WM_KEYDOWN / UP / SYSKEYDOWN / UP before the game acts. A key is the
// player's when the call comes from Freelancer.exe itself (the return address inside its image): FLvibed's own injected
// keys (fl::SendKey calls the same entry from the logic DLL) and commands (IsCmdHeld, HandleKey) never count, so
// LastRealKeyTick is the input-device service's keyboard half and OnRawKey sees physical keys only. `textField` = the
// keyboard focus (exe::KeyFocus, the handler object a window or a text edit registers) belongs to a live control (a text
// edit's handler sits at its +0x498 - the chat's CTID, a save description), so letters are typing, not commands.
typedef bool(__cdecl* WinKeyFn)(UINT msg, WPARAM wParam, LPARAM lParam);
static WinKeyFn g_origWinKey = nullptr;
static u32 g_lastRealKeyTick = 0;
static u32 g_rawKeysSeen = 0, g_rawKeysSwallowed = 0, g_injectedKeysSeen = 0;
constexpr u32 kTextEditHandler = 0x498; // the text edit's key handler object inside the control (0x57C750)

static bool InExeImage(const void* p)
{
    const hk::Image& img = hk::GetImage();
    const uintptr_t a = reinterpret_cast<uintptr_t>(p);
    return img.base && a >= img.base && a < img.base + img.size;
}

static bool ControlInList(u32 ctl)
{
    if (!ctl || !exe::UiControls.ok())
        return false;
    __try
    {
        const u32 head = Mem<u32>(exe::UiControls);
        u32 node = head ? Mem<u32>(head + 4) : 0;
        for (int guard = 0; node && node != head && guard < 8000; ++guard, node = Mem<u32>(node + 4))
            if (Mem<u32>(node + 8) == ctl)
                return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
    return false;
}

static bool TextFieldFocused()
{
    if (!exe::KeyFocus.ok())
        return false;
    const u32 focus = Mem<u32>(exe::KeyFocus);
    if (!focus || focus < kTextEditHandler)
        return false;
    // a window's handler sits at window + 0x32C, a text edit's at edit + 0x498: the latter's owner is a live control
    const u32 edit = focus - kTextEditHandler;
    if (!ControlInList(edit))
        return false;
    static u32 loggedVt = 0;
    if (!loggedVt)
    {
        __try
        {
            loggedVt = Mem<u32>(edit);
            Log("raw keys: a text edit %08X (vtable %08X) has the keyboard focus", edit, loggedVt);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
        }
    }
    return true;
}


static bool __cdecl WinKeyHook(UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg >= WM_KEYDOWN && msg <= WM_SYSKEYUP)
    {
        const bool down = msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN;
        const bool up = msg == WM_KEYUP || msg == WM_SYSKEYUP;
        // The game's own call (beside FLUF, its detour may be the caller when FLvibed's hook could not move to its
        // handler: hk::AdoptRestoreStyle).
        if (InExeImage(_ReturnAddress()) || hk::NoteCaller(_ReturnAddress()))
        {
            ++g_rawKeysSeen;
            if (down)
                g_lastRealKeyTick = GetTickCount();
            if ((down || up) && router::RawKeysWanted() && router::RawKey(static_cast<int>(wParam), down, TextFieldFocused()))
            {
                ++g_rawKeysSwallowed;
                return true; // the logic took it: the game never sees this key
            }
        }
        else
            ++g_injectedKeysSeen;
    }
    return g_origWinKey(msg, wParam, lParam);
}

u32 LastRealKeyTick() { return g_lastRealKeyTick; }

std::string RawKeyStatus()
{
    char keys[160];
    sprintf_s(keys, "raw keys: real=%u swallowed=%u injected=%u lastRealTick=%u textField=%d\n", g_rawKeysSeen,
              g_rawKeysSwallowed, g_injectedKeysSeen, g_lastRealKeyTick, TextFieldFocused());
    return keys;
}

// ------------------------------------------------------------------ HUD hit-test suppression
typedef uintptr_t(__cdecl* HitTestFn)(const float* ray);
static HitTestFn g_origHitTest = nullptr;
static uintptr_t g_hudRoots[64];
static int g_hudRootCount = 0;

void SetHudHitSuppression(bool on, const uintptr_t* roots, int count)
{
    g_hudRootCount = 0;
    for (int i = 0; roots && i < count && i < 64; ++i)
        g_hudRoots[g_hudRootCount++] = roots[i];
    g_hudSuppress = on && g_hudRootCount > 0;
}

static bool UnderHudRoot(uintptr_t ctl)
{
    __try
    {
        // FlControl +4 = parent; walk up to the top-level window.
        for (int guard = 0; ctl && guard < 64; ++guard)
        {
            for (int i = 0; i < g_hudRootCount; ++i)
                if (g_hudRoots[i] == ctl)
                    return true;
            ctl = *reinterpret_cast<uintptr_t*>(ctl + 4);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
    return false;
}

static uintptr_t __cdecl HitTestHook(const float* ray)
{
    const uintptr_t ctl = g_origHitTest(ray);
    if (ctl && g_hudSuppress && UnderHudRoot(ctl))
        return 0;
    return ctl;
}

void InstallKeys()
{
    // WinKey: sub esp,14h; mov edx,[esp+18h] (7 bytes): the player's raw keys (LastRealKeyTick, OnRawKey)
    g_origWinKey = static_cast<WinKeyFn>(hk::Detour(exe::WinKey, WinKeyHook, 7, "raw keys (the autopilot key, the page)"));
    // HitTest: sub esp,8; mov eax,ds:[0067DBE8] (8 bytes, no relative addressing)
    g_origHitTest = static_cast<HitTestFn>(hk::Detour(exe::HitTest, HitTestHook, 8, "HUD hit suppression"));
}

void Install()
{
    // IsCmdHeld: mov ecx, ds:[0067C258] (6 bytes)
    g_origIsCmdHeld = static_cast<IsCmdHeldFn>(hk::Detour(exe::IsCmdHeld, IsCmdHeldHook, 6, "held pad commands"));
    // MouseSteer: sub esp,18h; push ebx; push esi (5 bytes). The override writes the mouse-flight globals.
    if (exe::MouseX.ok() && exe::MouseY.ok() && exe::MouseFlightToggle.ok() && exe::MouseFlightTimer.ok() &&
        exe::MouseFlightEngaged.ok())
        g_origSteer = static_cast<SteerFn>(hk::Detour(exe::MouseSteer, SteerHook, 5, "pad steering"));
    else
        Log("Hook MouseSteer NOT installed: mouse-flight globals unresolved - pad steering off");
}
} // namespace input
