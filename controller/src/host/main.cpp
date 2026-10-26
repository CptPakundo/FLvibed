// Controller module host, part of FLvibed.dll (started by core/flvibed.cpp when the module is on).
// Owns every hook into the game and forwards events to the controller logic: FLControllerImpl.dll
// (hot-reloadable) in the dev build, linked in (FLV_STATIC) in the release build.
#include "common.h"

#include "../api.h"
#include "../dimouse.h"
#include "../events.h"
#include "../fl.h"
#include "flexe.h"
#include "hook.h"
#include "log.h"
#include "version.h"
#include "../shm.h"
#include "uibtn.h"
#include "winmode.h"

// The settings bus: one instance in FLvibed.dll (core\settings_bus.cpp, compiled plain), reached by this C symbol.
extern "C" const SettingsBus* flv_settings_bus();
extern "C" const HudBus* flv_hud_bus();

// ------------------------------------------------------------------ state
static HostApi g_host = {};
static bool g_cursorHidden = false; // v20 SetCursorHidden (CursorUpdateHook below)
static const ImplApi* g_impl = nullptr;
static HMODULE g_implModule = nullptr;
static int g_implGeneration = 0;
static bool g_faulted = false;
static bool g_reloadRequested = false;

static bool g_held[fl::CMD_COUNT];
static bool g_traceKeys = false;

static void* g_d3dDevice = nullptr;

static void HostSetSteer(bool active, float x, float y);
static void HostSetHudHitSuppression(bool on, const uintptr_t* roots, int count);

// ------------------------------------------------------------------ host API functions
static void HostLogWrite(const char* line) { LogWriteLine(line); }
static void HostSetHeld(int cmd, bool held)
{
    if (cmd > 0 && cmd < fl::CMD_COUNT)
        g_held[cmd] = held;
}
static void HostClearAllHeld() { memset(g_held, 0, sizeof(g_held)); }
static void HostSetKeyTrace(bool on) { g_traceKeys = on; }
static void HostMouseStats(char* out, int len) { strncpy_s(out, len, dimouse::Stats().c_str(), _TRUNCATE); }
static void* HostGetD3DDevice() { return g_d3dDevice; }
static void HostPushEvent(const char* text) { events::Push("%s", text); }
static void HostIdsText(u32 ids, char* out, int len) { strncpy_s(out, len, events::IdsText(ids).c_str(), _TRUNCATE); }

// ------------------------------------------------------------------ v18: FLvibed's native buttons (uibtn.cpp) -> the logic
void UiButtonClicked(int id)
{
    if (!g_impl || g_faulted || g_impl->version < 3 || !g_impl->OnUiButton)
    {
        Log("ui button %d clicked: no logic to take it", id);
        return;
    }
    __try
    {
        g_impl->OnUiButton(id);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        g_faulted = true;
        Log("FATAL: exception 0x%08X in OnUiButton(%d); logic disabled until reload", GetExceptionCode(), id);
    }
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

static bool GuardedRawKey(int vk, bool down, bool textField)
{
    __try
    {
        return g_impl->OnRawKey(vk, down, textField);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        g_faulted = true;
        Log("FATAL: exception 0x%08X in OnRawKey; logic disabled until reload", GetExceptionCode());
        return false;
    }
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
            if ((down || up) && g_impl && !g_faulted && g_impl->version >= 3 && g_impl->OnRawKey &&
                GuardedRawKey(static_cast<int>(wParam), down, TextFieldFocused()))
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

static u32 HostLastRealKeyTick() { return g_lastRealKeyTick; }

// ------------------------------------------------------------------ logic DLL loading
static void UnloadImpl()
{
    if (g_impl)
    {
        __try
        {
            g_impl->Shutdown();
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            Log("impl Shutdown faulted");
        }
    }
    g_impl = nullptr;
    HostClearAllHeld();
    HostSetSteer(false, 0, 0);
    HostSetHudHitSuppression(false, nullptr, 0);
    g_cursorHidden = false; // v20: the pad screens' cursor hide ends with the logic
    dimouse::Reset();
    if (g_implModule)
    {
        FreeLibrary(g_implModule);
        g_implModule = nullptr;
    }
}

#ifdef FLV_STATIC
// Release build: the logic is linked into FLvibed.dll (no hot reload; `reload` restarts the same logic).
extern "C" const void* flv_controller_impl_api();

static bool LoadImpl()
{
    ++g_implGeneration;
    const ImplApi* api = static_cast<const ImplApi*>(flv_controller_impl_api());
    if (!api || api->version != kImplApiVersion)
    {
        Log("LoadImpl: incompatible built-in logic");
        return false;
    }
    g_impl = api;
    g_faulted = false;
    g_impl->Init(&g_host);
    Log("FLvibed " FLV_VERSION ": logic started (built in)");
    return true;
}
#else
static bool LoadImpl()
{
    // Load from a copy so the build can overwrite FLControllerImpl.dll while the game runs.
    const std::string src = DllDir() + "FLControllerImpl.dll";
    char copyName[64];
    sprintf_s(copyName, "FLControllerImpl.run%d.dll", g_implGeneration++ % 2);
    const std::string copy = DllDir() + copyName;
    if (!CopyFileA(src.c_str(), copy.c_str(), FALSE))
    {
        Log("LoadImpl: cannot copy %s (%lu)", src.c_str(), GetLastError());
        return false;
    }
    g_implModule = LoadLibraryA(copy.c_str());
    if (!g_implModule)
    {
        Log("LoadImpl: LoadLibrary failed (%lu)", GetLastError());
        return false;
    }
    auto get = reinterpret_cast<GetImplApiFn>(GetProcAddress(g_implModule, "GetImplApi"));
    const ImplApi* api = get ? get() : nullptr;
    if (!api || api->version != kImplApiVersion)
    {
        Log("LoadImpl: incompatible logic DLL");
        FreeLibrary(g_implModule);
        g_implModule = nullptr;
        return false;
    }
    g_impl = api;
    g_faulted = false;
    g_impl->Init(&g_host);
    Log("Logic DLL loaded (%s)", copyName);
    return true;
}
#endif

// ------------------------------------------------------------------ debug channel
// events [after-seq]: event log lines newer than after-seq (first line: "last <seq>")
static void EventsReply(const char* cmd, char* reply, size_t len)
{
    const u32 after = cmd[6] ? static_cast<u32>(strtoul(cmd + 7, nullptr, 10)) : 0;
    const std::string out =
        "last " + std::to_string(events::LastSeq()) + "\n" + events::Since(after, static_cast<int>(len) - 64);
    strcpy_s(reply, len, out.c_str());
}

// uibtn: the native buttons' state and the raw-key counters (its std::string stays out of PollCommands' SEH frame)
static void UiBtnReply(char* reply, size_t len)
{
    char keys[160];
    sprintf_s(keys, "raw keys: real=%u swallowed=%u injected=%u lastRealTick=%u textField=%d\n", g_rawKeysSeen,
              g_rawKeysSwallowed, g_injectedKeysSeen, g_lastRealKeyTick, TextFieldFocused());
    strcpy_s(reply, len, (uibtn::Status() + keys).c_str());
}

static void UiGenReply(char* reply, size_t len); // the UI generation (below)
static void D3dResetReply(char* reply, size_t len); // the test-only forced device Reset (below)

static void PollCommands()
{
    ShmBlock* s = shm::Get();
    if (!s || s->cmdSeq == s->cmdAck)
        return;
    char cmd[sizeof(s->cmd)];
    memcpy(cmd, s->cmd, sizeof(cmd));
    cmd[sizeof(cmd) - 1] = 0;

    if (strncmp(cmd, "events", 6) == 0 && (cmd[6] == 0 || cmd[6] == ' '))
        EventsReply(cmd, s->reply, sizeof(s->reply));
    else if (strncmp(cmd, "uibtn", 5) == 0 && (cmd[5] == 0 || cmd[5] == ' ')) // uibtn | uibtn click <id> (host-side)
    {
        if (strncmp(cmd + 5, " click ", 7) == 0)
            strcpy_s(s->reply, uibtn::DebugClick(atoi(cmd + 12)) ? "clicked" : "no such button (not created)");
        else if (strncmp(cmd + 5, " mesh", 5) == 0) // uibtn mesh [<path relative to DATA>]: the bar button's mesh probe
        {
            uibtn::SetBarMesh(cmd[10] == ' ' ? cmd + 11 : "");
            strcpy_s(s->reply, "mesh set for the next HUD build (reload a save)");
        }
        else
            UiBtnReply(s->reply, sizeof(s->reply));
    }
    else if (strcmp(cmd, "hooks") == 0) // this host's detours: mode, place, the function's own jump now (host-side)
        hk::HookReport(s->reply, sizeof(s->reply));
    else if (strcmp(cmd, "uigen") == 0) // the UI generation counter (HostApi v23) and its two halves (host-side)
        UiGenReply(s->reply, sizeof(s->reply));
    else if (strncmp(cmd, "winmode", 7) == 0 && (cmd[7] == 0 || cmd[7] == ' ')) // borderless status / altenter (host-side)
        winmode::Command(cmd + 7, s->reply, sizeof(s->reply));
    else if (strcmp(cmd, "d3dreset") == 0) // background test starts only: a device Reset as the game's own (host-side)
        D3dResetReply(s->reply, sizeof(s->reply));
    else if (strcmp(cmd, "reload") == 0)
    {
        UnloadImpl();
        strcpy_s(s->reply, LoadImpl() ? "reloaded" : "reload FAILED (see log)");
    }
    else if (strcmp(cmd, "unload") == 0)
    {
        UnloadImpl();
        strcpy_s(s->reply, "unloaded");
    }
    else if (g_impl && !g_faulted)
    {
        s->reply[0] = 0;
        __try
        {
            g_impl->Command(cmd, s->reply, sizeof(s->reply));
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            strcpy_s(s->reply, "command faulted");
        }
    }
    else
        strcpy_s(s->reply, "logic DLL not loaded");
    MemoryBarrier();
    s->cmdAck = s->cmdSeq;
}

// ------------------------------------------------------------------ game hooks
typedef void(__cdecl* FrameUpdateFn)(double dt);
static FrameUpdateFn g_origFrameUpdate = nullptr;

static void GuardedFrame(double dt)
{
    __try
    {
        if (!g_impl && !g_implModule && g_implGeneration == 0)
            LoadImpl();
        PollCommands();
        dimouse::Update();
        events::Update();
        uibtn::OnFrame();
        if (g_impl && !g_faulted)
            g_impl->OnFrame(dt);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        g_faulted = true;
        HostClearAllHeld();
        HostSetSteer(false, 0, 0);
        dimouse::Reset();
        Log("FATAL: exception 0x%08X in controller update; logic disabled until reload", GetExceptionCode());
    }
}

static void __cdecl FrameUpdateHook(double dt)
{
    if (ShmBlock* s = shm::Get())
        ++s->heartbeat;
    GuardedFrame(dt);
    g_origFrameUpdate(dt);
    // every DLL is loaded at the first frame: FLUF loaded after FLvibed moves FLvibed's hooks to its handlers; again
    // when a hook saw FLUF as its caller (a FLUF hook made later)
    static bool adopted = false;
    if (!adopted || hk::AdoptPending())
    {
        adopted = true;
        hk::AdoptRestoreStyle("controller host");
    }
}

typedef bool(__cdecl* IsCmdHeldFn)(int cmd);
static IsCmdHeldFn g_origIsCmdHeld = nullptr;

static bool __cdecl IsCmdHeldHook(int cmd)
{
    if (cmd > 0 && cmd < fl::CMD_COUNT && g_held[cmd])
        return true;
    return g_origIsCmdHeld(cmd);
}

typedef bool(__cdecl* HandleKeyFn)(int cmd, int category);
static HandleKeyFn g_origHandleKey = nullptr;

static bool __cdecl HandleKeyHook(int cmd, int category)
{
    const bool handled = g_origHandleKey(cmd, category);
    if (g_traceKeys && cmd != fl::USER_MOVE)
        Log("HandleKey %s(%d) cat=%d down=%d vk=0x%X -> %d", fl::CmdNameRaw(cmd), cmd, category,
            Mem<u8>(exe::KeyIsDown), Mem<u32>(exe::KeyCurVk), handled);
    return handled;
}

// ------------------------------------------------------------------ steering override
typedef void(__cdecl* SteerFn)();
static SteerFn g_origSteer = nullptr;
static bool g_steerActive = false;
static float g_steerX = 0, g_steerY = 0;

static void HostSetSteer(bool active, float x, float y)
{
    g_steerActive = active;
    g_steerX = Clampf(x, -1, 1);
    g_steerY = Clampf(y, -1, 1);
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
    g_origSteer();
    Mem<int>(exe::MouseX) = savedX;
    Mem<int>(exe::MouseY) = savedY;
    Mem<u8>(exe::MouseFlightToggle) = savedToggle;
    Mem<float>(exe::MouseFlightTimer) = savedTimer;
    Mem<u8>(exe::MouseFlightEngaged) = savedEngaged;
}

typedef void(__cdecl* CursorUpdateFn)();
static CursorUpdateFn g_origCursorUpdate = nullptr;

static void GuardedOverlay()
{
    __try
    {
        if (g_impl && !g_faulted && g_d3dDevice)
            g_impl->OnRenderOverlay();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        g_faulted = true;
        Log("FATAL: exception 0x%08X in overlay; logic disabled until reload", GetExceptionCode());
    }
}

// v20 SetCursorHidden: the pad screens hide the game's arrow while the pad cursor is "hidden" (the focus carries on
// without it). The game's own cursor-shown flag (exe::CursorShown, 0x6107DC) gates the draw half of CursorUpdate after
// its mouse poll: cleared around the game's call and put back, so nothing of the game's state changes.
static void HostSetCursorHidden(bool on) { g_cursorHidden = on && exe::CursorShown.ok(); }

// Freelancer polls the mouse and draws its cursor here, inside the frame's rendering.
static void __cdecl CursorUpdateHook()
{
    GuardedOverlay();
    dimouse::BackgroundCursor();
    if (g_cursorHidden && exe::CursorShown.ok())
    {
        const u8 shown = Mem<u8>(exe::CursorShown);
        Mem<u8>(exe::CursorShown) = 0;
        // Beside FLUF this hook wraps FLUF's handler, which draws an ImGui arrow for the game's current cursor and
        // ignores CursorShown: no current cursor for that call either (the game reads it only in the draw half the
        // flag skips), put back after unless the game set another meanwhile.
        const bool fluf = exe::CurrentCursor.ok() && hk::ModeOf(exe::CursorUpdate.get()) == hk::HookAtHandler;
        const u32 cursor = fluf ? Mem<u32>(exe::CurrentCursor) : 0;
        if (cursor)
            Mem<u32>(exe::CurrentCursor) = 0;
        g_origCursorUpdate();
        if (cursor && !Mem<u32>(exe::CurrentCursor))
            Mem<u32>(exe::CurrentCursor) = cursor;
        Mem<u8>(exe::CursorShown) = shown;
    }
    else
        g_origCursorUpdate();
}

typedef void(__cdecl* UiRenderFn)();
static UiRenderFn g_origUiRender = nullptr;

static void GuardedUnderHud()
{
    __try
    {
        if (g_impl && !g_faulted && g_d3dDevice && Mem<int>(exe::UiRenderSkip) <= 0)
            g_impl->OnRenderUnderHud();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        g_faulted = true;
        Log("FATAL: exception 0x%08X in under-HUD drawing; logic disabled until reload", GetExceptionCode());
    }
}

// Freelancer's UI pass: each state's frame calls it after its 3D scene; it draws every visible control (the space
// HUD too), then the cursor. Drawing first puts the drawing between the scene and the HUD.
static void __cdecl UiRenderHook()
{
    uibtn::BeforeUiDraw(); // the game's Options page stays hidden under FLvibed's page
    if (exe::UiRenderSkip.ok())
        GuardedUnderHud();
    g_origUiRender();
}

// ------------------------------------------------------------------ HUD hit-test suppression
typedef uintptr_t(__cdecl* HitTestFn)(const float* ray);
static HitTestFn g_origHitTest = nullptr;
static bool g_hudSuppress = false;
static uintptr_t g_hudRoots[64];
static int g_hudRootCount = 0;

static void HostSetHudHitSuppression(bool on, const uintptr_t* roots, int count)
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

// ------------------------------------------------------------------ base-room characters (vtable 0x5CB930)
// Constructor 0x443ED0 thiscall(this, a1, a2) ret 8, destructor 0x4440C0 thiscall(this). Both start with an
// SEH frame: push -1; push 5B97C8h (7 bytes, no relative addressing). Same hooks as FLSubtitles (they chain).
typedef void*(__fastcall* CharCtorFn)(void* self, void* edx, void* a1, void* a2);
typedef void(__fastcall* CharDtorFn)(void* self, void* edx);
static CharCtorFn g_origCharCtor = nullptr;
static CharDtorFn g_origCharDtor = nullptr;
static void* g_chars[128];
static int g_charCount = 0;

static void* __fastcall CharCtorHook(void* self, void* edx, void* a1, void* a2)
{
    void* r = g_origCharCtor(self, edx, a1, a2);
    if (g_charCount < 128)
        g_chars[g_charCount++] = self;
    return r;
}

static void __fastcall CharDtorHook(void* self, void* edx)
{
    for (int i = 0; i < g_charCount; ++i)
        if (g_chars[i] == self)
        {
            memmove(&g_chars[i], &g_chars[i + 1], sizeof(void*) * (g_charCount - i - 1));
            --g_charCount;
            break;
        }
    g_origCharDtor(self, edx);
}

static int HostGetRoomCharacters(void** out, int max)
{
    const int n = std::min(max, g_charCount);
    memcpy(out, g_chars, sizeof(void*) * n);
    return n;
}

// ------------------------------------------------------------------ jump-in complete (exe::JumpInComplete)
// u32 thiscall(this, u32 system) ret 4: the client tells the server the player's jump-in is done. Starts with
// mov eax, [abs] (5 bytes, no relative addressing).
typedef u32(__fastcall* JumpInCompleteFn)(void* self, void* edx, u32 system);
static JumpInCompleteFn g_origJumpIn = nullptr;
static volatile long g_jumpIns = 0;

static u32 __fastcall JumpInCompleteHook(void* self, void* edx, u32 system)
{
    InterlockedIncrement(&g_jumpIns);
    return g_origJumpIn(self, edx, system);
}

static int HostJumpInCount() { return g_origJumpIn ? static_cast<int>(g_jumpIns) : -1; }

// ------------------------------------------------------------------ UI generation (exe::UiControlAdd / UiControlRemove)
// Extended HUD 2 (HostApi v23): every control the game adds to its flat control list (each class's Init calls
// UiControlAdd after SetParent) and every control it removes (the FlControl destructors call UiControlRemove) bumps a
// counter, so the logic learns that a window opened or closed without walking the list each frame. One increment per
// call, nothing else (the game's thread only). Both detours or none: gated together (UiGeneration 0 = unavailable).
typedef void(__cdecl* UiControlAddFn)(void* control);
typedef bool(__cdecl* UiControlRemoveFn)(void* control);
static UiControlAddFn g_origUiAdd = nullptr;
static UiControlRemoveFn g_origUiRemove = nullptr;
static u32 g_uiAdds = 0, g_uiRemoves = 0;

static void __cdecl UiControlAddHook(void* control)
{
    ++g_uiAdds;
    g_origUiAdd(control);
}

static bool __cdecl UiControlRemoveHook(void* control)
{
    ++g_uiRemoves;
    return g_origUiRemove(control);
}

// 1 + adds + removes: grows with every change of the control tree; 0 = the detours are not installed (fails closed).
static u32 HostUiGeneration() { return g_origUiAdd && g_origUiRemove ? 1 + g_uiAdds + g_uiRemoves : 0; }

static void UiGenReply(char* reply, size_t len)
{
    const u32 gen = HostUiGeneration();
    sprintf_s(reply, len, "uigen %u adds %u removes %u%s", gen, g_uiAdds, g_uiRemoves, gen ? "" : " (hooks not installed)");
}

static void InstallUiGenerationHooks()
{
    if (!exe::UiControlAdd.ok() || !exe::UiControlRemove.ok())
    {
        Log("UI generation (Extended HUD 2) off: UiControlAdd / UiControlRemove unresolved");
        return;
    }
    // UiControlAdd: mov eax, [esp+4]; push eax (5 bytes). UiControlRemove: mov edx, [esp+4]; mov al, [edx+6Ch] (7 bytes).
    g_origUiAdd = static_cast<UiControlAddFn>(hk::Detour(exe::UiControlAdd, UiControlAddHook, 5, "UI generation (Extended HUD 2)"));
    if (g_origUiAdd)
        g_origUiRemove = static_cast<UiControlRemoveFn>(
            hk::Detour(exe::UiControlRemove, UiControlRemoveHook, 7, "UI generation (Extended HUD 2)"));
    if (g_origUiAdd && g_origUiRemove)
        Log("UI generation (Extended HUD 2): control add / remove hooked");
}

// ------------------------------------------------------------------ hits on the player ship (the HUD damage indicator)
// The game's damage indicator (HUD_DamageIndicator) gets every hit on the player ship as a world point: the source of
// a round that hit or of an explosion near the ship (exe::DmgIndSource) and contact hits (exe::DmgIndContact,
// collisions). It reduces them to 4 arcs; FLvibed keeps the point.
typedef void(__fastcall* DmgPointFn)(void* self, void* edx, const float* point);
static DmgPointFn g_origDmgSource = nullptr;
static DmgPointFn g_origDmgContact = nullptr;
static float g_hitPos[3];
static u32 g_hitKind = 0, g_hitTick = 0;
static volatile long g_hitSeq = 0;

static bool CopyPoint(const float* p, float out[3])
{
    __try
    {
        out[0] = p[0], out[1] = p[1], out[2] = p[2];
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static void RecordHit(const float* point, u32 kind)
{
    float p[3];
    const bool ok = point && CopyPoint(point, p);
    static int logged = 0;
    if (logged < 3)
    {
        ++logged;
        Log("damage indicator input kind %u point %p %s (%.0f, %.0f, %.0f)", kind, point, ok ? "read" : "unreadable",
            ok ? p[0] : 0.0f, ok ? p[1] : 0.0f, ok ? p[2] : 0.0f);
    }
    if (ok)
    {
        g_hitPos[0] = p[0], g_hitPos[1] = p[1], g_hitPos[2] = p[2];
        g_hitKind = kind;
        g_hitTick = GetTickCount();
        InterlockedIncrement(&g_hitSeq);
    }
}

static void __fastcall DmgSourceHook(void* self, void* edx, const float* point)
{
    RecordHit(point, 1);
    g_origDmgSource(self, edx, point);
}

static void __fastcall DmgContactHook(void* self, void* edx, const float* point)
{
    RecordHit(point, 2);
    g_origDmgContact(self, edx, point);
}

static u32 HostPlayerHit(float pos[3], u32* kind, u32* tick)
{
    if (pos)
        pos[0] = g_hitPos[0], pos[1] = g_hitPos[1], pos[2] = g_hitPos[2];
    if (kind)
        *kind = g_hitKind;
    if (tick)
        *tick = g_hitTick;
    return static_cast<u32>(g_hitSeq);
}

// ------------------------------------------------------------------ comm player (exe::Comm)
// thiscall(player, a1, CommRecord*) ret 8, called when a comm starts playing (the subtitles module hooks it too: the
// detours chain). Only remembers the player object.
typedef void(__fastcall* CommStartFn)(void* self, void* edx, u32 a1, const u8* rec);
static CommStartFn g_origCommStart = nullptr;
static void* volatile g_commPlayer = nullptr;

static void __fastcall CommStartHook(void* self, void* edx, u32 a1, const u8* rec)
{
    g_commPlayer = self;
    g_origCommStart(self, edx, a1, rec);
}

static void* HostCommPlayer() { return g_commPlayer; }

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
// moment the game's first window appeared let the activation through and the game took the screen (user, 19 Oct 2026).
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

// Modules loaded after the start get the import patches as they load, not at the watchdog's next round (19 Oct 2026:
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
// no Windows or game setting is touched (user, 17 Oct 2026): every stream of the process (DirectSound, the movies'
// DirectShow audio) ends in Windows' shared-mode render client, whose ReleaseBuffer gets the "silent" flag. The game
// plays everything as always (sound events, voices, timing, the subtitles), the samples handed over are silence.
static bool BackgroundTestStart()
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
            // The window title names the test game (user, 21 Oct 2026: every instance was just "Freelancer"): the instance
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

static void HostSetKeepRunning(bool on)
{
    on = on || g_forceKeepRunning;
    if (on == g_keepRunning)
        return;
    g_keepRunning = on;
    Log("keep running out of focus: %s", on ? "on" : "off");
    ApplyKeepRunning();
    if (!on && !g_appReallyActive && exe::AppActive.ok())
        Mem<u8>(exe::AppActive) = 0; // inactive again: the game pauses as it would have
}

// ------------------------------------------------------------------ Evade behaviour guard (Common.dll)
// The Evade behaviour's update (vtable slot 7) checks that its threat ([this+0x2A0], resolved from the directive's
// object id) still exists only for NPC ships; for the player ship it skips the check and dereferences the null
// threat (0x62E4500, Common.dll+0x84509) once the threat is gone or was never found. The guard finishes the
// behaviour the way the game does for NPC ships ([this+0x10] = 2, return 2). docs\FREELANCER_RE.md "AI on the
// player ship". The signature is the update's start up to the threat test (HD Edition 0.7.1 = 1.1 Common.dll).
static const char kEvadeUpdateSig[] = "83 EC 1C 55 56 8B F1 8B 46 24 05 FC 14 00 00 8B 80 84 00 00 00 8B 10 57 "
                                      "8B C8 FF 92 BC 00 00 00 84 C0 75 ?? 8B 86 A0 02 00 00 85 C0 0F 84";
constexpr int kEvadeUpdateSlot = 7;
constexpr int kEvadeThreat = 0x2A0;
constexpr int kEvadeResult = 0x10;
typedef int(__fastcall* EvadeUpdateFn)(void* self, void* edx);
static EvadeUpdateFn g_origEvadeUpdate = nullptr;
static void** g_evadeVtable = nullptr;
// The guard sits on the class's vtable, so it sees every ship's Evade (the game finishes NPC ones the same way), and a
// behaviour manager calls a finished Evade again every frame until the next directive replaces it (seen 25 / 26 Oct
// 2026: two behaviours alternating, ~100 calls a second each). Each finish is logged once: the behaviours finished and
// already logged are remembered until they run with a threat again (a few at a time; at most 20 lines per game).
static void* g_evadeFinished[8] = {};
static int g_evadeLogged = 0;

static int __fastcall EvadeUpdateGuard(void* self, void* edx)
{
    u8* b = static_cast<u8*>(self);
    const uintptr_t threat = *reinterpret_cast<uintptr_t*>(b + kEvadeThreat);
    if (threat == 0 || threat == 8) // the game's own test: no watcher, or a watcher of no object
    {
        bool known = false;
        for (void* p : g_evadeFinished)
            known = known || p == self;
        if (!known)
        {
            static unsigned next = 0;
            g_evadeFinished[next++ % 8] = self;
            if (g_evadeLogged < 20 && ++g_evadeLogged)
                Log("evade guard: threat gone, behaviour %p finished", self);
        }
        *reinterpret_cast<int*>(b + kEvadeResult) = 2;
        return 2;
    }
    for (void*& p : g_evadeFinished)
        if (p == self)
            p = nullptr; // running again with a threat: its next finish is logged
    return g_origEvadeUpdate(self, edx);
}

static bool InCommonCode(const void* p, size_t len)
{
    HMODULE common = GetModuleHandleA("common.dll");
    MEMORY_BASIC_INFORMATION mbi = {};
    if (!common || !VirtualQuery(p, &mbi, sizeof(mbi)) || mbi.AllocationBase != common)
        return false;
    return static_cast<const u8*>(p) + len <= static_cast<const u8*>(mbi.BaseAddress) + mbi.RegionSize;
}

static bool HostGuardEvade(void** vtable)
{
    if (!vtable)
        return false;
    if (vtable == g_evadeVtable)
        return true;
    if (g_evadeVtable)
        return false; // one Evade class only
    if (!InCommonCode(vtable, sizeof(void*) * (kEvadeUpdateSlot + 1)))
        return false;
    const u8* fn = static_cast<const u8*>(vtable[kEvadeUpdateSlot]);
    if (!InCommonCode(fn, 1))
    {
        // Another mod owns the update (FLUF's EvadeBehaviorHooks re-implements it from its DllMain and throws on a
        // vtable someone patched first): FLvibed leaves the slot alone and the autopilot dodges with its own slide
        // bursts and breaks (autopilot_combat.cpp: `dodge` 2 falls back). Decided once, logged once.
        static bool logged = false;
        if (!logged)
        {
            logged = true;
            HMODULE owner = nullptr;
            char path[MAX_PATH] = "?";
            if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                   reinterpret_cast<LPCSTR>(fn), &owner))
                GetModuleFileNameA(owner, path, MAX_PATH);
            const char* base = strrchr(path, '\\');
            Log("evade guard: the Evade update belongs to %s%s - not installed; the autopilot uses its own dodges "
                "(slide bursts, breaks) instead of the game's Evade",
                base ? base + 1 : path, hk::IsRestoreStyle(reinterpret_cast<uintptr_t>(fn)) ? " (FLUF's own Evade)" : "");
        }
        return false;
    }
    std::vector<u8> bytes, mask;
    if (!hk::ParseSig(kEvadeUpdateSig, bytes, mask) || !InCommonCode(fn, bytes.size()))
        return false;
    for (size_t i = 0; i < bytes.size(); ++i)
        if (mask[i] && fn[i] != bytes[i])
        {
            Log("evade guard: update code differs at +%u, not installed (Evade stays off)", static_cast<u32>(i));
            return false;
        }
    g_origEvadeUpdate = reinterpret_cast<EvadeUpdateFn>(
        hk::PatchVtable(vtable, kEvadeUpdateSlot, reinterpret_cast<void*>(EvadeUpdateGuard)));
    g_evadeVtable = vtable;
    Log("evade guard installed (vtable %p, update %p)", vtable, fn);
    return true;
}

// ------------------------------------------------------------------ Direct3D 8 capture
namespace d3d8idx
{
constexpr int D3D_CreateDevice = 15;
constexpr int Dev_Reset = 14;
} // namespace d3d8idx

typedef void*(WINAPI* Direct3DCreate8Fn)(UINT sdk);
typedef HRESULT(WINAPI* CreateDeviceFn)(void* d3d, UINT adapter, DWORD type, HWND wnd, DWORD flags, void* pp,
                                        void** outDevice);
typedef HRESULT(WINAPI* ResetFn)(void* dev, void* pp);
static Direct3DCreate8Fn g_origCreate8 = nullptr;
static CreateDeviceFn g_origCreateDevice = nullptr;
static ResetFn g_origReset = nullptr;

// The device notifications reach the logic like every other impl call: a fault disables it until reload instead of
// crashing the game inside Direct3D's Reset / CreateDevice.
static void GuardedDeviceLost()
{
    __try
    {
        g_impl->OnDeviceLost();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        g_faulted = true;
        Log("FATAL: exception 0x%08X in device-lost; logic disabled until reload", GetExceptionCode());
    }
}

static void GuardedDeviceReset()
{
    __try
    {
        g_impl->OnDeviceReset();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        g_faulted = true;
        Log("FATAL: exception 0x%08X in device-reset; logic disabled until reload", GetExceptionCode());
    }
}

// The presentation parameters of the last CreateDevice / successful Reset (D3DPRESENT_PARAMETERS, D3D8: 13 DWORDs) - for
// the test-only `d3dreset`.
static u32 g_lastPp[13] = {};
static bool g_havePp = false;

static HRESULT WINAPI ResetHook(void* dev, void* pp)
{
    if (dev == g_d3dDevice && g_impl && !g_faulted)
        GuardedDeviceLost();
    HRESULT hr = g_origReset(dev, pp);
    Log("D3D8 device Reset -> 0x%08X", hr);
    if (dev == g_d3dDevice && SUCCEEDED(hr) && pp)
        memcpy(g_lastPp, pp, sizeof(g_lastPp)), g_havePp = true;
    if (dev == g_d3dDevice && g_impl && !g_faulted && SUCCEEDED(hr))
        GuardedDeviceReset();
    return hr;
}

// `d3dreset` (26 Oct 2026, the Alt+Enter freeze of the owner's run): a background test start only (never the player's
// game) resets the device in place with its own last parameters, through the device's vtable - every Reset hook in the
// chain runs (the subtitles', FLUF's, this one) as for the game's own Reset (a resolution change). D3D_OK = no
// D3DPOOL_DEFAULT resource or state block of anyone stood in the way; D3DERR_INVALIDCALL = something was not released.
// The reply names TestCooperativeLevel before and after.
static void D3dResetReply(char* reply, size_t len)
{
    if (!g_forceKeepRunning)
    {
        strcpy_s(reply, len, "d3dreset: background test starts only");
        return;
    }
    if (!g_d3dDevice || !g_havePp)
    {
        strcpy_s(reply, len, "d3dreset: no device / parameters yet");
        return;
    }
    void** vt = *static_cast<void***>(g_d3dDevice);
    typedef HRESULT(WINAPI * TclFn)(void*);
    const TclFn tcl = reinterpret_cast<TclFn>(vt[3]); // IDirect3DDevice8::TestCooperativeLevel
    u32 pp[13];
    memcpy(pp, g_lastPp, sizeof(pp));
    const HRESULT before = tcl(g_d3dDevice);
    HRESULT hr = E_FAIL;
    __try
    {
        hr = reinterpret_cast<ResetFn>(vt[d3d8idx::Dev_Reset])(g_d3dDevice, pp);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        hr = E_UNEXPECTED;
    }
    const HRESULT after = tcl(g_d3dDevice);
    Log("d3dreset: TestCooperativeLevel 0x%08X, Reset -> 0x%08X, then 0x%08X", before, hr, after);
    sprintf_s(reply, len, "d3dreset: cooperative 0x%08X, Reset 0x%08X, cooperative after 0x%08X", before, hr, after);
}

static HRESULT WINAPI CreateDeviceHook(void* d3d, UINT adapter, DWORD type, HWND wnd, DWORD flags, void* pp,
                                       void** outDevice)
{
    HRESULT hr = g_origCreateDevice(d3d, adapter, type, wnd, flags, pp, outDevice);
    Log("D3D8 CreateDevice -> 0x%08X device=%p", hr, outDevice ? *outDevice : nullptr);
    if (SUCCEEDED(hr) && outDevice && *outDevice)
    {
        if (g_d3dDevice && g_impl && !g_faulted)
            GuardedDeviceLost();
        g_d3dDevice = *outDevice;
        if (pp)
            memcpy(g_lastPp, pp, sizeof(g_lastPp)), g_havePp = true;
        void** vt = *static_cast<void***>(g_d3dDevice);
        if (vt[d3d8idx::Dev_Reset] != ResetHook)
            g_origReset = static_cast<ResetFn>(hk::PatchVtable(vt, d3d8idx::Dev_Reset, ResetHook));
    }
    return hr;
}

static void* WINAPI Direct3DCreate8Hook(UINT sdk)
{
    void* d3d = g_origCreate8(sdk);
    Log("Direct3DCreate8(%u) -> %p", sdk, d3d);
    if (d3d)
    {
        void** vt = *static_cast<void***>(d3d);
        if (vt[d3d8idx::D3D_CreateDevice] != CreateDeviceHook)
            g_origCreateDevice =
                static_cast<CreateDeviceFn>(hk::PatchVtable(vt, d3d8idx::D3D_CreateDevice, CreateDeviceHook));
    }
    return d3d;
}

// HostApi v26: who provides borderless windowed (winmode.cpp)
static const char* HostWindowMode(char* detail, int len) { return winmode::ProviderNow(detail, len); }

// ------------------------------------------------------------------ init
static void Init(HMODULE self)
{
    LogInit(self, "FLController.log");
    Log("FLController host starting (build %s %s)", __DATE__, __TIME__);

    // Every Freelancer.exe address is verified by its signature; a hook whose code is not what we expect is not
    // installed (its feature stays off, logged), the rest works.
    exe::ResolveAll("controller host");
    hk::LogRestoreStyleAtStart("controller host");
    // Borderless windowed (winmode.h): before the game creates its window (the DPI awareness, the display hooks)
    winmode::Install((DllDir() + "FLController.ini").c_str(), BackgroundTestStart());

    shm::Init();

    g_host.version = kHostApiVersion;
    g_host.dllDir = DllDir().c_str();
    g_host.shm = shm::Get();
    g_host.LogWrite = HostLogWrite;
    g_host.SetHeld = HostSetHeld;
    g_host.ClearAllHeld = HostClearAllHeld;
    g_host.SetKeyTrace = HostSetKeyTrace;
    g_host.MouseMoveBy = dimouse::MoveBy;
    g_host.MouseMoveTo = dimouse::MoveTo;
    g_host.MouseReleaseTarget = dimouse::ReleaseTarget;
    g_host.MouseWheel = dimouse::Wheel;
    g_host.MouseButton = dimouse::SetButton;
    g_host.MouseButtonDown = dimouse::ButtonDown;
    g_host.MouseReset = dimouse::Reset;
    g_host.MouseStats = HostMouseStats;
    g_host.MouseIsCaptured = dimouse::IsCaptured;
    g_host.MouseCapture = dimouse::Capture;
    g_host.GetD3DDevice = HostGetD3DDevice;
    g_host.SetSteer = HostSetSteer;
    g_host.LastRealMouseTick = dimouse::LastRealTick;
    g_host.PushEvent = HostPushEvent;
    g_host.IdsText = HostIdsText;
    g_host.SetHudHitSuppression = HostSetHudHitSuppression;
    g_host.GetRoomCharacters = HostGetRoomCharacters;
    g_host.GuardEvade = HostGuardEvade;
    g_host.SetKeepRunning = HostSetKeepRunning;
    g_host.JumpInCount = HostJumpInCount;
    g_host.CommPlayer = HostCommPlayer;
    g_host.MissionObjects = events::MissionObjects;
    g_host.MissionMarks = events::MissionMarks;
    g_host.ObjectiveRefs = events::ObjectiveRefs;
    g_host.ObjectiveNow = events::ObjectiveNow;
    g_host.PlayerHit = HostPlayerHit;
    g_host.ObjectiveTitle = events::ObjectiveTitle;
    g_host.DockReply = events::DockReply;
    g_host.ObjectiveSteps = events::ObjectiveSteps;
    g_host.CommHeardAt = events::CommHeardAt;
    g_host.SpawnedShips = events::SpawnedShips;
    // v18: the UI build (uibtn.cpp, the raw keys above, dimouse's physical buttons)
    g_host.GetSettingsBus = flv_settings_bus;
    g_host.SetUiButton = uibtn::SetUiButton;
    g_host.UiButtonRect = uibtn::UiButtonRect;
    g_host.OptionsPageState = uibtn::OptionsPageState;
    g_host.ShowOptionsPage = uibtn::ShowOptionsPage;
    g_host.LastRealKeyTick = HostLastRealKeyTick;
    g_host.RealMouseDown = dimouse::RealButtonDown;
    g_host.RealMouseMotion = dimouse::RealMotion; // v19
    g_host.SetCursorHidden = HostSetCursorHidden; // v20
    g_host.MissionResult = events::MissionResult;
    g_host.GetHudBus = flv_hud_bus; // v21: core\hud_bus.cpp (HUD 2)
    g_host.SetHudHotspots = uibtn::SetHudHotspots;       // v22: HUD 2's clicks (uibtn.cpp)
    g_host.HudHotspotHovered = uibtn::HudHotspotHovered; // v22
    g_host.UiGeneration = HostUiGeneration;              // v23: Extended HUD 2
    g_host.ObjectiveText = events::ObjectiveText;        // v24: HUD 2's objective line
    g_host.ObjectiveReread = events::ObjectiveReread;    // v25: the objective a load restores
    g_host.WindowMode = HostWindowMode;                  // v26: who provides borderless (winmode.cpp)
    g_host.RealMouseWheel = dimouse::RealWheel;          // v27: the options page's wheel
    // DmgIndSource: sub esp,88h (6 bytes); DmgIndContact: sub esp,3Ch; push esi; mov esi,ecx (6 bytes)
    g_origDmgSource =
        static_cast<DmgPointFn>(hk::Detour(exe::DmgIndSource, DmgSourceHook, 6, "player hit points (shield cue)"));
    g_origDmgContact =
        static_cast<DmgPointFn>(hk::Detour(exe::DmgIndContact, DmgContactHook, 6, "player hit points (shield cue)"));
    g_origCommStart = static_cast<CommStartFn>(hk::Detour(exe::Comm, CommStartHook, 5, "comm player (dialogue state)"));
    // JumpInComplete: mov eax, ds:[0067ECD0] (5 bytes)
    g_origJumpIn = static_cast<JumpInCompleteFn>(
        hk::Detour(exe::JumpInComplete, JumpInCompleteHook, 5, "jump-in complete (autopilot travel)"));
    InstallUiGenerationHooks();
    // IsCmdHeld: mov ecx, ds:[0067C258] (6 bytes)
    g_origIsCmdHeld = static_cast<IsCmdHeldFn>(hk::Detour(exe::IsCmdHeld, IsCmdHeldHook, 6, "held pad commands"));
    g_origSetAppActive =
        static_cast<SetAppActiveFn>(hk::Detour(exe::SetAppActive, SetAppActiveHook, 6, "keep running out of focus"));
    if (g_origSetAppActive && exe::AppActive.ok())
    {
        // The option from the start (the logic re-applies it every frame): a game started without focus runs too.
        g_forceKeepRunning = BackgroundTestStart();
        g_keepRunning = g_forceKeepRunning || GetPrivateProfileIntA("Controller", "run_in_background", 0,
                                                                    (DllDir() + "FLController.ini").c_str()) != 0;
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
    // HandleKey: sub esp, 80h (6 bytes)
    g_origHandleKey = static_cast<HandleKeyFn>(hk::Detour(exe::HandleKey, HandleKeyHook, 6, "key trace"));
    // WinKey: sub esp,14h; mov edx,[esp+18h] (7 bytes): the player's raw keys (LastRealKeyTick, OnRawKey)
    g_origWinKey = static_cast<WinKeyFn>(hk::Detour(exe::WinKey, WinKeyHook, 7, "raw keys (the autopilot key, the page)"));
    uibtn::Install(); // FLvibed's native buttons: the bar, the Options screen, the pause menu
    // CursorUpdate: mov eax, fs:[0] (6 bytes)
    g_origCursorUpdate =
        static_cast<CursorUpdateFn>(hk::Detour(exe::CursorUpdate, CursorUpdateHook, 6, "controller overlay"));
    // UiRender: mov eax,1008h (5 bytes)
    g_origUiRender = static_cast<UiRenderFn>(hk::Detour(exe::UiRender, UiRenderHook, 5, "drawing under the HUD"));
    // FrameUpdate: push ecx; push ebp; push esi; xor ebp,ebp (5 bytes)
    g_origFrameUpdate =
        static_cast<FrameUpdateFn>(hk::Detour(exe::FrameUpdate, FrameUpdateHook, 5, "the whole controller logic"));
    // MouseSteer: sub esp,18h; push ebx; push esi (5 bytes). The override writes the mouse-flight globals.
    if (exe::MouseX.ok() && exe::MouseY.ok() && exe::MouseFlightToggle.ok() && exe::MouseFlightTimer.ok() &&
        exe::MouseFlightEngaged.ok())
        g_origSteer = static_cast<SteerFn>(hk::Detour(exe::MouseSteer, SteerHook, 5, "pad steering"));
    else
        Log("Hook MouseSteer NOT installed: mouse-flight globals unresolved - pad steering off");
    // HitTest: sub esp,8; mov eax,ds:[0067DBE8] (8 bytes, no relative addressing)
    g_origHitTest = static_cast<HitTestFn>(hk::Detour(exe::HitTest, HitTestHook, 8, "HUD hit suppression"));
    // CharCtor / CharDtor: push -1; push <SEH handler> (7 bytes). Both or neither (the list must stay consistent).
    if (exe::CharCtor.ok() && exe::CharDtor.ok())
    {
        g_origCharCtor = static_cast<CharCtorFn>(hk::Detour(exe::CharCtor, CharCtorHook, 7, "room characters"));
        g_origCharDtor = static_cast<CharDtorFn>(hk::Detour(exe::CharDtor, CharDtorHook, 7, "room characters"));
    }
    else
        Log("Hooks CharCtor/CharDtor NOT installed: signature mismatch - room characters off");
    Log("Game hooks: held=%p key=%p cursor=%p ui=%p frame=%p steer=%p", g_origIsCmdHeld, g_origHandleKey,
        g_origCursorUpdate, g_origUiRender, g_origFrameUpdate, g_origSteer);

    // Direct3D: Freelancer's render pipeline (RP8.dll) creates the D3D8 object.
    HMODULE rp8 = GetModuleHandleA("rp8.dll");
    g_origCreate8 =
        static_cast<Direct3DCreate8Fn>(hk::PatchImport(rp8, "d3d8.dll", "Direct3DCreate8", Direct3DCreate8Hook));
    Log("Direct3DCreate8 import hook %s (rp8=%p)", g_origCreate8 ? "installed" : "NOT FOUND", rp8);
}

// Thread notifications come only on a background test start (core\flvibed.cpp): each new thread is guarded against
// activating the game before its first instruction.
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
        Init(instance);
    else if (reason == DLL_THREAD_ATTACH && g_forceKeepRunning)
    {
        if (HHOOK h = SetWindowsHookExA(WH_CBT, CbtHook, nullptr, GetCurrentThreadId()))
        {
            AcquireSRWLockExclusive(&g_cbtLock);
            g_cbtHooks.push_back({GetCurrentThreadId(), h});
            ReleaseSRWLockExclusive(&g_cbtLock);
        }
    }
    return TRUE;
}
