// FLvibed controller host - the opt-in SDL3 gamepad backend (sdlpad.h; WORK_QUEUE Q5 slice 1). The recipe follows the
// 29 Oct 2026 measurement (research\evidence\sdl3\probe_report.md, build_results.md):
// - SDL runs on ONE thread of its own, never the game thread: SDL_Init creates a helper window on its calling thread and
//   HIDAPI's discovery window on the thread that first enumerates, and the discovery poll's PeekMessage would dispatch
//   cross-thread sent messages to every window of the calling thread - on the game thread that re-enters Freelancer's
//   window procedure from inside FLvibed. SDL_Init / SDL_Quit never run under the loader lock (DllMain): the thread
//   starts at the logic's first request.
// - No video, no audio (compiled out): SDL_INIT_GAMEPAD only. SDL_PumpEvents is never called (no message loop of ours);
//   SDL_UpdateGamepads reads the pads and the event queue is drained after every read (only the hot-plug events matter).
// - Hints at override priority (an environment variable cannot change them): background events allowed (FLvibed's own
//   focus gate decides, modules\controller\src\impl\focus_gate.h), no signal handlers, no timeBeginPeriod (the game does not
//   change the timer resolution; SDL would by default), SDL's own joystick thread for the device notifications, the
//   message-loop hint off, enhanced reports "auto"; DirectInput, RawInput, WGI and GameInput off in slice 1 (DirectInput
//   acquires exclusively and the game uses DirectInput itself - probe_report.md risk 3; its enumeration is also ~110 of
//   SDL_Init's 115-126 ms: 3-5 ms without it, measured 30 Oct 2026 out of the game; the rest unmeasured here).
// - SDL3_DYNAMIC_API (an environment variable that makes SDL LoadLibraryA a DLL of its name: probe_report.md risk 1) is
//   cleared from the process environment before the first SDL call - the source stays unpatched.
// - The read is synchronous to the frame without running SDL on the game thread: the game thread wakes the thread and
//   waits for that read at most 2 ms (none while the previous read still runs: a hot-plug enumeration takes longer),
//   then takes the newest state.
// - The SDL virtual test pads (WORK_QUEUE Q5 slice 2, IDEAS T36 slice 2; TEST ONLY, single player: the debug `sdlpad`):
//   SDL's virtual joystick driver with a real pad's USB ids (pad_backend.h kTestPads), attached / detached on this
//   thread at the game thread's request (the command waits for the answer, at most 1 s); their state comes from shm.h v3
//   (the harness's XInput form, set on the device before every read) and what SDL sends them (rumble, trigger rumble,
//   effects) is recorded there. While one is attached the newest is the pad in use (a real pad is ignored meanwhile).
#include "sdlpad.h"

#include "log.h"
#include "../pad_backend.h"
#include "../shm.h"
#include "dimouse.h"

#ifndef FLV_SDL3
#error "sdlpad.cpp needs the SDL3 headers: build.cmd wraps the controller host with FLV_SDL3 (core\pre.h)"
#endif

// the mirrored SDL values the logic uses (pad_backend.h) are SDL's own
static_assert(SDL_GAMEPAD_TYPE_UNKNOWN == padbackend::kTypeUnknown && SDL_GAMEPAD_TYPE_STANDARD == padbackend::kTypeStandard &&
                  SDL_GAMEPAD_TYPE_XBOX360 == padbackend::kTypeXbox360 && SDL_GAMEPAD_TYPE_XBOXONE == padbackend::kTypeXboxOne &&
                  SDL_GAMEPAD_TYPE_PS3 == padbackend::kTypePs3 && SDL_GAMEPAD_TYPE_PS4 == padbackend::kTypePs4 &&
                  SDL_GAMEPAD_TYPE_PS5 == padbackend::kTypePs5 &&
                  SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO == padbackend::kTypeSwitchPro &&
                  SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT == padbackend::kTypeJoyconLeft &&
                  SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT == padbackend::kTypeJoyconRight &&
                  SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR == padbackend::kTypeJoyconPair &&
                  SDL_GAMEPAD_TYPE_GAMECUBE == padbackend::kTypeGameCube && SDL_GAMEPAD_TYPE_STEAM == padbackend::kTypeSteam &&
                  SDL_GAMEPAD_TYPE_COUNT == padbackend::kTypeCount,
              "pad_backend.h SdlType mirrors SDL_GamepadType");
static_assert(SDL_GAMEPAD_BUTTON_TOUCHPAD == padbackend::kSdlTouchpad, "pad_backend.h kSdlTouchpad is SDL's touchpad click");
static_assert(SDL_GAMEPAD_BUTTON_SOUTH == 0 && SDL_GAMEPAD_BUTTON_BACK == 4 && SDL_GAMEPAD_BUTTON_START == 6 &&
                  SDL_GAMEPAD_BUTTON_LEFT_SHOULDER == 9 && SDL_GAMEPAD_BUTTON_DPAD_RIGHT == 14 &&
                  padbackend::kSdlButtons == SDL_GAMEPAD_BUTTON_DPAD_RIGHT + 1,
              "pad_backend.h kXiButtonForSdl is indexed by SDL_GamepadButton");
static_assert(XINPUT_GAMEPAD_A == 0x1000 && XINPUT_GAMEPAD_BACK == 0x0020 && XINPUT_GAMEPAD_DPAD_RIGHT == 0x0008 &&
                  XINPUT_GAMEPAD_LEFT_SHOULDER == 0x0100,
              "pad_backend.h kXiButtonForSdl holds XInput's bits");

namespace sdlpad
{
static SRWLOCK g_lock = SRWLOCK_INIT; // guards g_state, g_path and the output blocks
static SdlPadState g_state = {};
static char g_path[260] = {}; // the open pad's device path
static char g_name[128] = {}; // ... and SDL's name of it (the grips' speaker found by name where no ContainerId says it: Wine)
static struct
{
    u16 low, high;
    u32 ms, seq;
} g_rumble = {};
static struct
{
    u16 left, right;
    u32 ms, seq;
} g_triggerRumble = {};
constexpr int kEffectBytes = 11;
static struct
{
    u8 left[kEffectBytes], right[kEffectBytes];
    u32 seq;
} g_triggers = {};
static HANDLE g_thread = nullptr, g_kick = nullptr;
static volatile LONG g_status = padbackend::kSdlOff;
static volatile LONG g_stop = 0, g_busy = 0, g_seq = 0;
static volatile LONG g_asked = 0; // GetTickCount of the logic's last call: the lease on what the pad is holding
constexpr DWORD kLeaseMs = 1000;  // nothing from the logic for this long, the thread puts the pad to rest by itself
constexpr DWORD kLeaseLookMs = 250;

static void Copy(char* dst, size_t cap, const char* src)
{
    size_t i = 0;
    for (; src && src[i] && i + 1 < cap; ++i)
        dst[i] = src[i];
    dst[i] = 0;
}

static double Ms(const LARGE_INTEGER& a, const LARGE_INTEGER& b)
{
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    return (b.QuadPart - a.QuadPart) * 1000.0 / static_cast<double>(f.QuadPart);
}

static void Hint(const char* name, const char* value) { SDL_SetHintWithPriority(name, value, SDL_HINT_OVERRIDE); }

// ---- A mapper's keys and mouse (padbackend::DropInjected). Low-level hooks on a thread of their own, only while the logic says so (Filter); nobody else pays
static volatile LONG g_nativeOnly = 0; // started by the fallback: only the physical PlayStation or Switch pad, set by Pad before the thread starts
static volatile LONG g_outputOnly = 0; // started for an Xbox pad's four motors: SDL's WGI driver sends, XInput goes on reading (padbackend::ImpulseRoute)
static HANDLE g_hookThread = nullptr;
static DWORD g_hookThreadId = 0;
static volatile LONG g_filterOn = 0, g_lastInput = 0, g_dropped = 0, g_said = 0;
static volatile LONG g_pressedLatch = 0; // every button seen held since the last TakePressed
static float g_stickDeadzone = 0.24f, g_triggerThreshold = 0.12f;

static bool DropNow(bool injected)
{
    if (!injected || !g_filterOn)
        return false;
    const auto since = [] { return static_cast<unsigned>(GetTickCount() - static_cast<DWORD>(g_lastInput)); };
    if (padbackend::DropInjected(true, true, since()))
        return true;
    // the mapper may have read the report before SDL did: a fresh read, then a short wait for it
    const LONG was = g_lastInput;
    if (g_kick)
        SetEvent(g_kick);
    const DWORD t0 = GetTickCount();
    while (g_lastInput == was && GetTickCount() - t0 < padbackend::kInjectWaitMs)
        SwitchToThread();
    return padbackend::DropInjected(true, true, since());
}

static void CountDrop(const char* what)
{
    InterlockedIncrement(&g_dropped);
    if (!InterlockedExchange(&g_said, 1))
        Log("SDL3 pad backend: a mapper's %s dropped: the pad reads natively (pad_sdl_fallback 0 hands it to the mapper)", what);
}

static LRESULT CALLBACK KeyHook(int code, WPARAM w, LPARAM l)
{
    if (code == HC_ACTION && DropNow((reinterpret_cast<KBDLLHOOKSTRUCT*>(l)->flags & LLKHF_INJECTED) != 0))
    {
        CountDrop("key");
        return 1;
    }
    return CallNextHookEx(nullptr, code, w, l);
}

static LRESULT CALLBACK MouseHook(int code, WPARAM w, LPARAM l)
{
    if (code == HC_ACTION)
    {
        const bool injected = (reinterpret_cast<MSLLHOOKSTRUCT*>(l)->flags & LLMHF_INJECTED) != 0;
        const int down = w == WM_LBUTTONDOWN ? 0 : w == WM_RBUTTONDOWN ? 1 : w == WM_MBUTTONDOWN ? 2 : -1;
        const int up = w == WM_LBUTTONUP ? 0 : w == WM_RBUTTONUP ? 1 : w == WM_MBUTTONUP ? 2 : -1;
        const bool moved = w == WM_MOUSEMOVE || w == WM_MOUSEWHEEL;
        if (up >= 0 && injected)
            dimouse::MaskInjectedButton(up, false);
        if (moved && !injected)
            dimouse::NoteMotion(false);
        if ((moved || down >= 0) && DropNow(injected))
        {
            if (moved)
                dimouse::NoteMotion(true);
            else
                dimouse::MaskInjectedButton(down, true);
            CountDrop("mouse");
            return 1;
        }
    }
    return CallNextHookEx(nullptr, code, w, l);
}

static DWORD WINAPI HookThread(void*)
{
    HMODULE self = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCSTR>(&HookThread), &self);
    HHOOK keys = SetWindowsHookExA(WH_KEYBOARD_LL, KeyHook, self, 0), mouse = SetWindowsHookExA(WH_MOUSE_LL, MouseHook, self, 0);
    // (a low-level hook runs on this thread and maps no module: the exe's handle serves as well. A machine can refuse every DLL's handle with error 126, whichever the DLL and
    // the process: seen 6 Nov 2026 from a bare PowerShell with six system DLLs, FLvibed's handle and file being right. The mapper's keys then counted twice)
    if (!keys || !mouse)
    {
        const DWORD first = GetLastError();
        const HMODULE exe = GetModuleHandleA(nullptr);
        if (!keys)
            keys = SetWindowsHookExA(WH_KEYBOARD_LL, KeyHook, exe, 0);
        if (!mouse)
            mouse = SetWindowsHookExA(WH_MOUSE_LL, MouseHook, exe, 0);
        Log("SDL3 pad backend: the low-level hooks by the DLL's handle %p: error %lu; by the exe's: keys %d mouse %d", self, first, keys ? 1 : 0, mouse ? 1 : 0);
    }
    if (!keys || !mouse)
        Log("SDL3 pad backend: no low-level hook (error %lu): a mapper's keys may count twice", GetLastError());
    MSG m;
    while (GetMessageA(&m, nullptr, 0, 0) > 0)
    {
    }
    if (keys)
        UnhookWindowsHookEx(keys);
    if (mouse)
        UnhookWindowsHookEx(mouse);
    return 0;
}

void Filter(int on, float stickDeadzone, float triggerThreshold)
{
    g_stickDeadzone = stickDeadzone, g_triggerThreshold = triggerThreshold;
    InterlockedExchange(&g_filterOn, on);
    if (on && !g_hookThread)
        g_hookThread = CreateThread(nullptr, 0, HookThread, nullptr, 0, &g_hookThreadId);
    else if (!on && g_hookThread)
    {
        PostThreadMessageA(g_hookThreadId, WM_QUIT, 0, 0);
        WaitForSingleObject(g_hookThread, 1000);
        CloseHandle(g_hookThread);
        g_hookThread = nullptr;
        for (int i = 0; i < 3; ++i)
            dimouse::MaskInjectedButton(i, false);
    }
}

static void Publish(const SdlPadState& s)
{
    AcquireSRWLockExclusive(&g_lock);
    g_state = s;
    ReleaseSRWLockExclusive(&g_lock);
}

static SdlPadTouch g_touch = {}; // under g_lock

static void ReadTouch(SDL_Gamepad* pad)
{
    SdlPadTouch t = {};
    if (pad)
        t.pads = SDL_GetNumGamepadTouchpads(pad);
    if (t.pads > 0)
    {
        t.clicked = SDL_GetGamepadButton(pad, static_cast<SDL_GamepadButton>(padbackend::kSdlTouchpad));
        const int fingers = SDL_GetNumGamepadTouchpadFingers(pad, 0);
        for (int i = 0; i < 2 && i < fingers; ++i)
        {
            bool down = false;
            float pressure = 0;
            if (SDL_GetGamepadTouchpadFinger(pad, 0, i, &down, &t.x[i], &t.y[i], &pressure))
                t.down[i] = down;
        }
    }
    AcquireSRWLockExclusive(&g_lock);
    g_touch = t;
    ReleaseSRWLockExclusive(&g_lock);
}

// The first gamepad SDL lists (the pad in use stays while it is connected: the pad_slots.h rule).
static SDL_Gamepad* OpenFirst(int& count)
{
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    SDL_Gamepad* pad = nullptr;
    if (g_outputOnly && count != 1) // which of two is XInput's pad? Nothing cheap says: two motors by XInput
    {
        SDL_free(ids);
        return nullptr;
    }
    for (int i = 0; ids && i < count && !pad; ++i)
    {
        const unsigned vendor = SDL_GetGamepadVendorForID(ids[i]);
        if (!g_nativeOnly || vendor == padbackend::kVendorSony || vendor == padbackend::kVendorNintendo) // (an Xbox pad is XInput's: the fallback is for the pad beside it)
            pad = SDL_OpenGamepad(ids[i]);
    }
    SDL_free(ids);
    return pad;
}

// ---- the SDL virtual test pads (TEST ONLY). g_tp / g_tpOrder are the SDL thread's alone; the request mailbox is
// guarded by g_lock; shm.h v3 holds the harness's state per slot and the recorded device commands.
constexpr int kSlots = padbackend::kTestPadSlots;
struct TestPad
{
    SDL_JoystickID id;  // 0 = slot empty
    SDL_Joystick* joy;  // our handle on the virtual device (its state is set through it)
    int kind;           // padbackend::kTestPads index
};
static TestPad g_tp[kSlots] = {};
static unsigned g_tpOrder[kSlots] = {}, g_tpNext = 0;
static int g_slotIds[kSlots] = {0, 1, 2, 3}; // the device callbacks' userdata
static volatile LONG g_inUse = -1;           // the slot of the test pad in use, -1 none

enum ReqOp
{
    kReqAttach = 1,
    kReqDetach,
    kReqRumble,
    kReqTriggers,
    kReqEffect,
    kReqTouch
};
struct Request
{
    int op, slot, kind; // kind: attach's kTestPads index; touch: the finger
    u32 a, b, ms;       // send: the motor / trigger speeds and the duration, or the effect's bytes; touch: x, y in thousandths, ms 1 = down
};
static Request g_req = {};
static u32 g_reqSeq = 0, g_reqDone = 0; // under g_lock
static char g_reqReply[256] = {};       // under g_lock
static HANDLE g_done = nullptr;         // set by the thread when it answered a request

static ShmBlock* TestShm()
{
    ShmBlock* s = shm::Get();
    return s && s->version >= 3 ? s : nullptr;
}

// One command SDL sent to a test pad (on this thread, inside SDL's call): the ring the harness reads (Get-FLSdlPadOut).
static void Record(int slot, u32 kind, u32 a, u32 b)
{
    ShmBlock* s = TestShm();
    if (!s)
        return;
    const u32 n = s->sdlOutSeq + 1;
    ShmBlock::SdlOut& o = s->sdlOut[(n - 1) % 32];
    o.seq = 0;
    o.slot = static_cast<u32>(slot), o.kind = kind, o.a = a, o.b = b, o.frame = s->heartbeat;
    MemoryBarrier();
    o.seq = n; // last but one: a reader that sees the new seq sees the whole entry
    s->sdlOutSeq = n;
}
static bool SDLCALL DevRumble(void* ud, Uint16 low, Uint16 high)
{
    Record(*static_cast<int*>(ud), kSdlOutRumble, low, high);
    return true;
}
static bool SDLCALL DevTriggers(void* ud, Uint16 left, Uint16 right)
{
    Record(*static_cast<int*>(ud), kSdlOutTriggers, left, right);
    return true;
}
// (a trigger block: `b` holds both triggers' modes and first zone bytes, right, left, right, left, so a test reads which stance went out)
static bool SDLCALL DevEffect(void* ud, const void* data, int size)
{
    const u8* p = static_cast<const u8*>(data);
    const u32 b = size >= 23 ? p[10] | p[21] << 8 | p[11] << 16 | static_cast<u32>(p[22]) << 24 : 0;
    Record(*static_cast<int*>(ud), kSdlOutEffect, static_cast<u32>(size), b);
    return true;
}

static void PublishSlot(int slot)
{
    if (ShmBlock* s = TestShm())
    {
        s->sdlSlot[slot].id = g_tp[slot].id;
        s->sdlSlot[slot].kind = g_tp[slot].id ? static_cast<u32>(g_tp[slot].kind + 1) : 0;
        if (g_tp[slot].id)
            s->sdlAttached |= 1u << slot;
        else
            s->sdlAttached &= ~(1u << slot);
    }
}

static void Attach(int slot, int kind, char* reply, int len)
{
    if (g_tp[slot].id)
    {
        snprintf(reply, len, "sdlpad: slot %d already holds a test pad (detach it first)", slot);
        return;
    }
    const padbackend::TestPadKind& k = padbackend::kTestPads[kind];
    SDL_VirtualJoystickDesc d;
    SDL_INIT_INTERFACE(&d);
    d.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    d.vendor_id = static_cast<Uint16>(k.vendor);
    d.product_id = static_cast<Uint16>(k.product);
    d.naxes = SDL_GAMEPAD_AXIS_COUNT;
    d.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    static const SDL_VirtualJoystickTouchpadDesc touchpad = {2, {0, 0, 0}};
    d.ntouchpads = static_cast<Uint16>(k.touchpads);
    d.touchpads = k.touchpads ? &touchpad : nullptr;
    d.name = k.sdlName;
    d.userdata = &g_slotIds[slot];
    d.Rumble = DevRumble;
    d.RumbleTriggers = DevTriggers;
    d.SendEffect = DevEffect;
    const SDL_JoystickID id = SDL_AttachVirtualJoystick(&d);
    SDL_Joystick* joy = id ? SDL_OpenJoystick(id) : nullptr;
    if (!joy)
    {
        snprintf(reply, len, "sdlpad: attach %s failed: %s", k.name, SDL_GetError());
        if (id)
            SDL_DetachVirtualJoystick(id);
        return;
    }
    g_tp[slot] = {id, joy, kind};
    g_tpOrder[slot] = ++g_tpNext;
    PublishSlot(slot);
    const SDL_GamepadType type = SDL_GetGamepadTypeForID(id);
    snprintf(reply, len, "attached slot=%d kind=%s id=%u vendor=0x%04X product=0x%04X type=%s (%d)%s", slot, k.name,
             static_cast<unsigned>(id), k.vendor, k.product, SDL_GetGamepadStringForType(type), static_cast<int>(type),
             type == k.type ? "" : " - NOT the expected type");
    Log("SDL3 pad backend: test pad %s", reply);
}

static void Detach(int slot, SDL_Gamepad*& pad)
{
    TestPad& t = g_tp[slot];
    if (!t.id)
        return;
    if (pad && SDL_GetGamepadID(pad) == t.id)
    {
        SDL_CloseGamepad(pad);
        pad = nullptr;
    }
    SDL_CloseJoystick(t.joy);
    SDL_DetachVirtualJoystick(t.id);
    Log("SDL3 pad backend: test pad slot %d (%s, id %u) detached", slot, padbackend::kTestPads[t.kind].name,
        static_cast<unsigned>(t.id));
    t = {};
    g_tpOrder[slot] = 0;
    PublishSlot(slot);
}

// The game thread's request, if one waits: done on this thread, answered through g_reqReply + g_done.
static void Serve(SDL_Gamepad*& pad)
{
    AcquireSRWLockShared(&g_lock);
    const u32 seq = g_reqSeq;
    const Request r = g_req;
    ReleaseSRWLockShared(&g_lock);
    if (seq == g_reqDone)
        return;
    char reply[sizeof(g_reqReply)] = {};
    switch (r.op)
    {
    case kReqAttach: Attach(r.slot, r.kind, reply, sizeof(reply)); break;
    case kReqDetach:
    {
        int n = 0;
        for (int i = 0; i < kSlots; ++i)
            if ((r.slot < 0 || r.slot == i) && g_tp[i].id)
                Detach(i, pad), ++n;
        snprintf(reply, sizeof(reply), "detached %d test pad(s)", n);
        break;
    }
    case kReqTouch:
    {
        // the finger on the first attached test pad with a touchpad, applied by the next SDL_UpdateGamepads
        bool ok = false;
        for (int i = 0; i < kSlots && !ok; ++i)
            if (g_tp[i].id)
                ok = SDL_SetJoystickVirtualTouchpad(g_tp[i].joy, 0, r.kind, r.ms != 0, r.a / 1000.0f, r.b / 1000.0f, r.ms ? 1.0f : 0.0f);
        snprintf(reply, sizeof(reply), "touch: %s", ok ? "ok" : SDL_GetError());
        break;
    }
    default:
    {
        // a command to the pad in use, the way the game's own would go (the test of the device read-back)
        bool ok = false;
        static const Uint8 kEffect[64] = {}; // (the content does not matter to the read-back: its size is recorded)
        if (pad && r.op == kReqRumble)
            ok = SDL_RumbleGamepad(pad, static_cast<Uint16>(r.a), static_cast<Uint16>(r.b), r.ms);
        else if (pad && r.op == kReqTriggers)
            ok = SDL_RumbleGamepadTriggers(pad, static_cast<Uint16>(r.a), static_cast<Uint16>(r.b), r.ms);
        else if (pad)
            ok = SDL_SendGamepadEffect(pad, kEffect, static_cast<int>(r.a));
        snprintf(reply, sizeof(reply), "sent %s: %s", r.op == kReqRumble ? "rumble" : r.op == kReqTriggers ? "triggers" : "effect",
                 !pad ? "no pad in use" : ok ? "ok" : SDL_GetError());
        break;
    }
    }
    AcquireSRWLockExclusive(&g_lock);
    memcpy(g_reqReply, reply, sizeof(g_reqReply));
    g_reqDone = seq;
    ReleaseSRWLockExclusive(&g_lock);
    SetEvent(g_done);
}

// Every attached test pad's device set to the harness's state (applied by the next SDL_UpdateGamepads).
static void Feed()
{
    const ShmBlock* s = TestShm();
    for (int i = 0; s && i < kSlots; ++i)
    {
        if (!g_tp[i].id)
            continue;
        const ShmBlock::SdlSlot& v = s->sdlSlot[i];
        SDL_Joystick* j = g_tp[i].joy;
        for (int b = 0; b < padbackend::kSdlButtons; ++b)
            SDL_SetJoystickVirtualButton(j, b, padbackend::SdlButtonDown(b, v.buttons));
        const auto clamp = [](i32 x) { return static_cast<Sint16>(x < -32768 ? -32768 : x > 32767 ? 32767 : x); };
        SDL_SetJoystickVirtualAxis(j, SDL_GAMEPAD_AXIS_LEFTX, clamp(v.lx));
        SDL_SetJoystickVirtualAxis(j, SDL_GAMEPAD_AXIS_LEFTY, clamp(padbackend::SdlStickY(v.ly)));
        SDL_SetJoystickVirtualAxis(j, SDL_GAMEPAD_AXIS_RIGHTX, clamp(v.rx));
        SDL_SetJoystickVirtualAxis(j, SDL_GAMEPAD_AXIS_RIGHTY, clamp(padbackend::SdlStickY(v.ry)));
        SDL_SetJoystickVirtualAxis(j, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, clamp(padbackend::SdlTriggerRaw(v.triggers & 0xFF)));
        SDL_SetJoystickVirtualAxis(j, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,
                                   clamp(padbackend::SdlTriggerRaw((v.triggers >> 8) & 0xFF)));
    }
}

// ---- what the pad holds beyond the thread's life: its firmware keeps a trigger effect until told otherwise (docs\research\haptics-best-in-class.md 2)
// The DualSense's effect block as SDL_hidapi_ps5.c lays it out: the enable bits first, the right trigger at 10, the left at 21.
static bool SendTriggers(SDL_Gamepad* pad, const u8* left, const u8* right)
{
    u8 block[47] = {};
    block[0] = 0x04 | 0x08; // both triggers: the bits that leave the rumble and audio haptics alone
    memcpy(block + 10, right, kEffectBytes);
    memcpy(block + 21, left, kEffectBytes);
    return SDL_SendGamepadEffect(pad, block, sizeof(block));
}

// Everything off on this pad. `driven`: something was sent that a stop has to take back.
static void ToRest(SDL_Gamepad* pad, bool& driven)
{
    if (!pad || !driven)
        return;
    static const u8 kOff[kEffectBytes] = {0x05};
    SendTriggers(pad, kOff, kOff);
    SDL_RumbleGamepadTriggers(pad, 0, 0, 0);
    SDL_RumbleGamepad(pad, 0, 0, 0);
    driven = false;
}

static DWORD WINAPI Thread(void*)
{
    char env[8];
    SetLastError(0);
    if (GetEnvironmentVariableA("SDL3_DYNAMIC_API", env, sizeof(env)) != 0 || GetLastError() != ERROR_ENVVAR_NOT_FOUND)
    {
        SetEnvironmentVariableA("SDL3_DYNAMIC_API", nullptr);
        Log("SDL3 pad backend: SDL3_DYNAMIC_API was set - cleared (no SDL library is loaded by name)");
    }
    Hint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    Hint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
    Hint(SDL_HINT_TIMER_RESOLUTION, "0");
    Hint(SDL_HINT_JOYSTICK_THREAD, "1");
    Hint(SDL_HINT_WINDOWS_ENABLE_MESSAGELOOP, "0");
    Hint(SDL_HINT_JOYSTICK_ENHANCED_REPORTS, "auto");
    Hint(SDL_HINT_JOYSTICK_DIRECTINPUT, "0");
    Hint(SDL_HINT_JOYSTICK_RAWINPUT, "0");
    // (WGI takes an Xbox pad only with XInput and RawInput off inside SDL; loaded at run time, so Windows 7 just finds nothing)
    Hint(SDL_HINT_JOYSTICK_WGI, g_outputOnly ? "1" : "0");
    if (g_outputOnly)
        Hint(SDL_HINT_JOYSTICK_HIDAPI, "0");
    Hint(SDL_HINT_JOYSTICK_GAMEINPUT, "0");
    // A background test game leaves the machine's own pad alone: SDL's HIDAPI driver would open it (and an open writes its lights), so only the virtual test pads exist for it
    const bool testGame = GetEnvironmentVariableA("FLVIBED_BACKGROUND", env, sizeof(env)) == 1 && env[0] == '1';
    if (testGame)
    {
        Hint(SDL_HINT_JOYSTICK_HIDAPI, "0");
        Log("SDL3 pad backend: a background game: SDL opens no real pad, the test pads alone");
    }
    // The fallback reads the physical pad: not XInput's pads through SDL (a mirror, or a real Xbox pad listed first), and not Steam's filter, which from memory hides that
    // pad from a game Steam starts, so that native comes first there too. If Steam hides it some other way, SDL finds no pad and XInput's is the pad.
    Hint(SDL_HINT_XINPUT_ENABLED, g_nativeOnly || g_outputOnly ? "0" : "1");
    if (g_nativeOnly)
    {
        Hint(SDL_HINT_GAMECONTROLLER_IGNORE_DEVICES, "");
        Hint(SDL_HINT_HIDAPI_IGNORE_DEVICES, "");
    }

    SdlPadState s = {};
    LARGE_INTEGER t0, t1;
    QueryPerformanceCounter(&t0);
    const bool ok = SDL_Init(SDL_INIT_GAMEPAD);
    QueryPerformanceCounter(&t1);
    s.initMs = static_cast<float>(Ms(t0, t1));
    const int v = SDL_GetVersion();
    if (!ok)
    {
        s.status = padbackend::kSdlFailed;
        Copy(s.error, sizeof(s.error), SDL_GetError());
        Log("SDL3 pad backend: SDL_Init failed after %.1f ms: %s - XInput reads the pad", s.initMs, s.error);
        SDL_Quit();
        Publish(s);
        InterlockedExchange(&g_status, padbackend::kSdlFailed);
        return 0;
    }
    int count = 0;
    SDL_Gamepad* pad = OpenFirst(count);
    Log("SDL3 pad backend: SDL %d.%d.%d (%s) on its own thread, init %.1f ms, %d gamepad(s)%s%s",
        SDL_VERSIONNUM_MAJOR(v), SDL_VERSIONNUM_MINOR(v), SDL_VERSIONNUM_MICRO(v), SDL_GetRevision(), s.initMs, count,
        pad ? ", in use: " : "", pad ? SDL_GetGamepadName(pad) : "");
    s.status = padbackend::kSdlRunning;
    s.count = count;
    Publish(s);
    InterlockedCompareExchange(&g_status, padbackend::kSdlRunning, padbackend::kSdlStarting); // (a Stop that timed out said Failed: it stands)

    u32 rumbleDone = 0, triggersDone = 0, triggerRumbleDone = 0; // (a command left from before a stop is over: it must not play at the restart)
    AcquireSRWLockExclusive(&g_lock);
    rumbleDone = g_rumble.seq, triggersDone = g_triggers.seq, triggerRumbleDone = g_triggerRumble.seq;
    ReleaseSRWLockExclusive(&g_lock);
    bool driven = false; // the open pad holds something of ours, by these three:
    bool motorsOn = false, triggerMotorsOn = false, stanceOn = false;
    SDL_Gamepad* padWas = nullptr;
    u32 padSerial = 0, effectFails = 0;
    const bool quiet = testGame; // nothing new reaches a real pad, should one ever open
    while (true)
    {
        // the lease: a pad left stiff or shaking by a logic that stopped asking (a fault, a hang, a load) goes to rest by itself; a player with nothing driven pays no wake-ups
        if (WaitForSingleObject(g_kick, driven ? kLeaseLookMs : INFINITE) == WAIT_TIMEOUT)
        {
            if (GetTickCount() - static_cast<DWORD>(g_asked) >= kLeaseMs)
            {
                ToRest(pad, driven);
                motorsOn = triggerMotorsOn = stanceOn = false;
                ++padSerial; // (the logic that comes back sends what the pad should hold again)
                Log("SDL3 pad backend: nothing from the logic for %lu ms: the pad put to rest", kLeaseMs);
            }
            continue;
        }
        if (g_stop)
            break;
        InterlockedExchange(&g_busy, 1);
        Serve(pad); // a test pad attached / detached, a test command sent (TEST ONLY: debug `sdlpad`)
        Feed();
        SDL_UpdateGamepads();
        // the queue drained: the hot-plug events re-pick the pad, the rest (axis / button events) is read as state
        bool rescan = !pad;
        SDL_Event ev[32];
        for (int n; (n = SDL_PeepEvents(ev, 32, SDL_GETEVENT, SDL_EVENT_FIRST, SDL_EVENT_LAST)) > 0;)
            for (int i = 0; i < n; ++i)
                if (ev[i].type == SDL_EVENT_GAMEPAD_ADDED || ev[i].type == SDL_EVENT_GAMEPAD_REMOVED)
                    rescan = true;
        const int test = padbackend::NewestTestPad(g_tpOrder, kSlots);
        if (test >= 0)
        {
            // a test pad attached: the newest is the pad in use (a real pad is ignored meanwhile)
            if (rescan)
                SDL_free(SDL_GetGamepads(&count));
            if (!pad || SDL_GetGamepadID(pad) != g_tp[test].id)
            {
                ToRest(pad, driven);
                if (pad)
                    SDL_CloseGamepad(pad);
                if ((pad = SDL_OpenGamepad(g_tp[test].id)) != nullptr)
                    Log("SDL3 pad backend: in use the test pad on slot %d, %s (type %s, %04X:%04X), %d gamepad(s)", test,
                        SDL_GetGamepadName(pad), SDL_GetGamepadStringForType(SDL_GetGamepadType(pad)),
                        SDL_GetGamepadVendor(pad), SDL_GetGamepadProduct(pad), count);
                else
                    Log("SDL3 pad backend: the test pad on slot %d did not open: %s", test, SDL_GetError());
            }
        }
        else
        {
            if (g_inUse >= 0)
                rescan = true; // the last test pad went (its detach closed it): SDL's first pad again
            if (pad && !SDL_GamepadConnected(pad))
            {
                Log("SDL3 pad backend: %s disconnected", SDL_GetGamepadName(pad));
                SDL_CloseGamepad(pad);
                pad = nullptr;
            }
            if (rescan)
            {
                SDL_free(SDL_GetGamepads(&count));
                if (!pad && (pad = OpenFirst(count)) != nullptr)
                    Log("SDL3 pad backend: in use %s (type %s, %04X:%04X), %d gamepad(s)", SDL_GetGamepadName(pad),
                        SDL_GetGamepadStringForType(SDL_GetGamepadType(pad)), SDL_GetGamepadVendor(pad),
                        SDL_GetGamepadProduct(pad), count);
            }
        }
        InterlockedExchange(&g_inUse, test);
        if (pad != padWas)
        {
            ++padSerial, driven = motorsOn = triggerMotorsOn = stanceOn = false, padWas = pad;
            const char* path = pad ? SDL_GetGamepadPath(pad) : nullptr;
            AcquireSRWLockExclusive(&g_lock);
            Copy(g_path, sizeof(g_path), test >= 0 ? nullptr : path); // (a test pad has no endpoint, and must never reach for one)
            Copy(g_name, sizeof(g_name), test >= 0 || !pad ? nullptr : SDL_GetGamepadName(pad));
            ReleaseSRWLockExclusive(&g_lock);
        }
        AcquireSRWLockShared(&g_lock);
        const auto rumble = g_rumble;
        const auto triggerRumble = g_triggerRumble;
        const auto triggers = g_triggers;
        ReleaseSRWLockShared(&g_lock);
        if (pad && rumble.seq != rumbleDone)
        {
            SDL_RumbleGamepad(pad, rumble.low, rumble.high, rumble.ms);
            motorsOn = rumble.low || rumble.high;
        }
        rumbleDone = rumble.seq;
        const bool mayDrive = test >= 0 || !quiet;
        if (pad && mayDrive && triggerRumble.seq != triggerRumbleDone)
        {
            SDL_RumbleGamepadTriggers(pad, triggerRumble.left, triggerRumble.right, triggerRumble.ms);
            triggerMotorsOn = triggerRumble.left || triggerRumble.right;
        }
        triggerRumbleDone = triggerRumble.seq;
        if (pad && mayDrive && triggers.seq != triggersDone)
        {
            if (SendTriggers(pad, triggers.left, triggers.right))
                stanceOn = triggers.left[0] != 0x05 || triggers.right[0] != 0x05;
            else if (!effectFails++)
                Log("SDL3 pad backend: the pad refused a trigger effect: %s - the motors go on", SDL_GetError());
        }
        triggersDone = triggers.seq;
        driven = motorsOn || triggerMotorsOn || stanceOn;

        SdlPadState r = {};
        r.status = padbackend::kSdlRunning;
        r.initMs = s.initMs;
        r.count = count;
        r.connected = pad != nullptr;
        if (pad)
        {
            const bool touchView = padbackend::TouchpadIsView(SDL_GetGamepadType(pad)); // (a PlayStation pad's View is its touchpad's click)
            for (int b = 0; b < padbackend::kSdlButtons; ++b)
                if ((!touchView || b != SDL_GAMEPAD_BUTTON_BACK) && SDL_GetGamepadButton(pad, static_cast<SDL_GamepadButton>(b)))
                    r.buttons |= padbackend::kXiButtonForSdl[b];
            if (touchView && SDL_GetGamepadButton(pad, static_cast<SDL_GamepadButton>(padbackend::kSdlTouchpad)))
                r.buttons |= padbackend::kXiButtonForSdl[SDL_GAMEPAD_BUTTON_BACK];
            r.lx = padbackend::StickX(SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX));
            r.ly = padbackend::StickY(SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY));
            r.rx = padbackend::StickX(SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTX));
            r.ry = padbackend::StickY(SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTY));
            r.lt = padbackend::Trigger(SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER));
            r.rt = padbackend::Trigger(SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER));
            r.type = SDL_GetGamepadType(pad);
            r.vendor = SDL_GetGamepadVendor(pad);
            r.product = SDL_GetGamepadProduct(pad);
            r.link = SDL_GetGamepadConnectionState(pad);
            Copy(r.name, sizeof(r.name), SDL_GetGamepadName(pad));
            InterlockedOr(&g_pressedLatch, static_cast<LONG>(r.buttons)); // (TakePressed: no tap lost between the movie loop's passes)
            if (padbackend::HasInput(r.buttons, r.lx, r.ly, r.rx, r.ry, r.lt, r.rt, g_stickDeadzone, g_triggerThreshold))
                InterlockedExchange(&g_lastInput, static_cast<LONG>(GetTickCount()));
        }
        r.dropped = static_cast<u32>(g_dropped);
        r.padSerial = padSerial, r.effectFails = effectFails;
        r.seq = static_cast<u32>(InterlockedIncrement(&g_seq));
        ReadTouch(pad);
        Publish(r);
        InterlockedExchange(&g_busy, 0);
    }
    if (pad)
    {
        ToRest(pad, driven);
        SDL_RumbleGamepad(pad, 0, 0, 0);
        SDL_CloseGamepad(pad);
        pad = nullptr;
    }
    for (int i = 0; i < kSlots; ++i)
        Detach(i, pad);
    InterlockedExchange(&g_inUse, -1);
    SDL_Quit();
    return 0;
}

static void Stop()
{
    if (!g_thread)
        return;
    InterlockedExchange(&g_stop, 1);
    SetEvent(g_kick);
    LARGE_INTEGER t0, t1;
    QueryPerformanceCounter(&t0);
    const DWORD w = WaitForSingleObject(g_thread, 3000);
    QueryPerformanceCounter(&t1);
    if (w != WAIT_OBJECT_0)
    {
        // SDL_Quit hangs: the thread is left (and SDL with it); a new start is refused rather than run a second SDL
        Log("SDL3 pad backend: the thread did not stop within 3 s - left running, the backend stays off this session");
        InterlockedExchange(&g_status, padbackend::kSdlFailed);
        Publish(SdlPadState{padbackend::kSdlFailed});
        return;
    }
    Log("SDL3 pad backend: stopped (SDL_Quit, %.1f ms)", Ms(t0, t1));
    CloseHandle(g_thread);
    CloseHandle(g_kick);
    CloseHandle(g_done);
    g_thread = g_kick = g_done = nullptr;
    InterlockedExchange(&g_stop, 0);
    InterlockedExchange(&g_status, padbackend::kSdlOff);
    Publish(SdlPadState{});
}

static bool g_stuck = false; // a stop timed out: never a second SDL in the process

u32 TakePressed() { return static_cast<u32>(InterlockedExchange(&g_pressedLatch, 0)); }

void Touch(SdlPadTouch* out)
{
    if (!out)
        return;
    AcquireSRWLockShared(&g_lock);
    *out = g_touch;
    ReleaseSRWLockShared(&g_lock);
}

int Pad(int on, SdlPadState* out)
{
    InterlockedExchange(&g_asked, static_cast<LONG>(GetTickCount()));
    if (!on)
    {
        if (g_thread && !g_stuck)
        {
            Stop();
            g_stuck = g_thread != nullptr;
        }
    }
    else if (!g_thread && !g_stuck)
    {
        InterlockedExchange(&g_nativeOnly, on == 2);
        InterlockedExchange(&g_outputOnly, on == padbackend::kSdlStartImpulse);
        g_kick = CreateEventA(nullptr, FALSE, FALSE, nullptr);
        g_done = CreateEventA(nullptr, FALSE, FALSE, nullptr);
        AcquireSRWLockExclusive(&g_lock);
        g_reqDone = g_reqSeq; // (a request left unanswered by an earlier thread is not served by this one)
        ReleaseSRWLockExclusive(&g_lock);
        InterlockedExchange(&g_status, padbackend::kSdlStarting);
        Publish(SdlPadState{padbackend::kSdlStarting});
        g_thread = g_kick && g_done ? CreateThread(nullptr, 0, Thread, nullptr, 0, nullptr) : nullptr;
        if (!g_thread)
        {
            Log("SDL3 pad backend: no thread (error %lu) - XInput reads the pad", GetLastError());
            if (g_kick)
                CloseHandle(g_kick), g_kick = nullptr;
            if (g_done)
                CloseHandle(g_done), g_done = nullptr;
            InterlockedExchange(&g_status, padbackend::kSdlFailed);
            Publish(SdlPadState{padbackend::kSdlFailed});
            g_stuck = true; // (no thread now: nothing to retry with)
        }
    }
    else if (g_status == padbackend::kSdlRunning && !g_busy)
    {
        // a fresh read for this frame: wake the thread, wait for its read (at most 2 ms)
        const LONG before = g_seq;
        SetEvent(g_kick);
        LARGE_INTEGER f, t0, t;
        QueryPerformanceFrequency(&f);
        QueryPerformanceCounter(&t0);
        // (a timed wait on an event could oversleep to the system timer's 15.6 ms when the read stalls; this bound is
        // exact - a read takes well under it, measured in sdlpad's probe)
        const LONGLONG limit = f.QuadPart / 500;
        do
        {
            SwitchToThread();
            QueryPerformanceCounter(&t);
        } while (g_seq == before && t.QuadPart - t0.QuadPart < limit);
    }
    if (out)
    {
        AcquireSRWLockShared(&g_lock);
        *out = g_state;
        ReleaseSRWLockShared(&g_lock);
        out->status = static_cast<int>(g_status);
    }
    return static_cast<int>(g_status);
}

void Rumble(u16 low, u16 high, u32 ms)
{
    AcquireSRWLockExclusive(&g_lock);
    g_rumble.low = low, g_rumble.high = high, g_rumble.ms = ms, ++g_rumble.seq;
    ReleaseSRWLockExclusive(&g_lock);
    if (g_status == padbackend::kSdlRunning && g_kick)
        SetEvent(g_kick);
}

bool Path(char* out, int len)
{
    AcquireSRWLockShared(&g_lock);
    Copy(out, static_cast<size_t>(len), g_path);
    ReleaseSRWLockShared(&g_lock);
    return out[0] != 0;
}

bool Name(char* out, int len)
{
    AcquireSRWLockShared(&g_lock);
    Copy(out, static_cast<size_t>(len), g_name);
    ReleaseSRWLockShared(&g_lock);
    return out[0] != 0;
}

void Triggers(const u8* left, const u8* right)
{
    AcquireSRWLockExclusive(&g_lock);
    memcpy(g_triggers.left, left, kEffectBytes);
    memcpy(g_triggers.right, right, kEffectBytes);
    ++g_triggers.seq;
    ReleaseSRWLockExclusive(&g_lock);
    if (g_status == padbackend::kSdlRunning && g_kick)
        SetEvent(g_kick);
}

void TriggerRumble(u16 left, u16 right, u32 ms)
{
    AcquireSRWLockExclusive(&g_lock);
    g_triggerRumble.left = left, g_triggerRumble.right = right, g_triggerRumble.ms = ms, ++g_triggerRumble.seq;
    ReleaseSRWLockExclusive(&g_lock);
    if (g_status == padbackend::kSdlRunning && g_kick)
        SetEvent(g_kick);
}

// The debug command `sdlpad ...` (host-side, the game thread; TEST ONLY): a request to the SDL thread, its answer awaited.
static void Ask(const Request& r, char* reply, int len)
{
    AcquireSRWLockExclusive(&g_lock);
    g_req = r;
    const u32 seq = ++g_reqSeq;
    ReleaseSRWLockExclusive(&g_lock);
    SetEvent(g_kick);
    const ULONGLONG until = GetTickCount64() + 1000;
    for (;;)
    {
        AcquireSRWLockShared(&g_lock);
        const bool done = g_reqDone == seq;
        if (done)
            snprintf(reply, len, "%s", g_reqReply);
        ReleaseSRWLockShared(&g_lock);
        if (done)
            return;
        const ULONGLONG now = GetTickCount64();
        if (now >= until)
        {
            snprintf(reply, len, "sdlpad: no answer from the SDL thread within 1 s");
            return;
        }
        WaitForSingleObject(g_done, static_cast<DWORD>(until - now));
    }
}

void Command(const char* args, bool multiplayer, char* reply, int len)
{
    while (*args == ' ')
        ++args;
    char op[16] = {}, w1[16] = {};
    long n[3] = {0, 0, 0};
    const int got = sscanf_s(args, "%15s %15s %ld %ld %ld", op, static_cast<unsigned>(sizeof(op)), w1,
                             static_cast<unsigned>(sizeof(w1)), &n[0], &n[1], &n[2]);
    const bool status = got < 1 || strcmp(op, "status") == 0;
    if (status)
    {
        // (no SDL call: the thread's published state and the shm block only)
        const ShmBlock* s = TestShm();
        int at = snprintf(reply, len, "sdlpad status=%d in_use=%ld attached=0x%X out_seq=%u", static_cast<int>(g_status),
                          static_cast<long>(g_inUse), s ? s->sdlAttached : 0, s ? s->sdlOutSeq : 0);
        for (int i = 0; s && i < kSlots && at > 0 && at < len; ++i)
            if (s->sdlSlot[i].kind)
                at += snprintf(reply + at, len - at, " slot%d=%s/id%u", i,
                               padbackend::kTestPads[(s->sdlSlot[i].kind - 1) % padbackend::kTestPadKinds].name,
                               s->sdlSlot[i].id);
        if (!s)
            snprintf(reply + at, len - at, " (no shm v3)");
        return;
    }
    if (multiplayer)
    {
        snprintf(reply, len, "sdlpad refused: test pads are single player only");
        return;
    }
    if (!TestShm())
    {
        snprintf(reply, len, "sdlpad refused: no shared memory v3");
        return;
    }
    if (g_status != padbackend::kSdlRunning || !g_kick || !g_done)
    {
        snprintf(reply, len, "sdlpad refused: the SDL backend is not running (status %d; `set pad_backend 1` starts it)",
                 static_cast<int>(g_status));
        return;
    }
    Request r = {};
    if (strcmp(op, "attach") == 0)
    {
        r.op = kReqAttach;
        r.kind = padbackend::TestPadKindOf(w1);
        r.slot = got >= 3 ? static_cast<int>(n[0]) : 0;
        if (r.kind < 0 || r.slot < 0 || r.slot >= kSlots)
        {
            snprintf(reply, len, "usage: sdlpad attach <xbox|ps5|switchpro> [slot 0-%d]", kSlots - 1);
            return;
        }
    }
    else if (strcmp(op, "detach") == 0)
    {
        r.op = kReqDetach;
        r.slot = got < 2 || strcmp(w1, "all") == 0 ? -1 : w1[0] >= '0' && w1[0] <= '9' && !w1[1] ? w1[0] - '0' : kSlots;
        if (r.slot >= kSlots)
        {
            snprintf(reply, len, "usage: sdlpad detach [slot 0-%d | all]", kSlots - 1);
            return;
        }
    }
    else if (strcmp(op, "touch") == 0 && got >= 5) // touch <finger> <x 0-1000> <y 0-1000> <1 down | 0 up>
    {
        r.op = kReqTouch;
        r.kind = w1[0] == '1' ? 1 : 0;
        r.a = static_cast<u32>(n[0] < 0 ? 0 : n[0] > 1000 ? 1000 : n[0]);
        r.b = static_cast<u32>(n[1] < 0 ? 0 : n[1] > 1000 ? 1000 : n[1]);
        r.ms = n[2] != 0;
    }
    else if (strcmp(op, "send") == 0 && got >= 3)
    {
        r.op = strcmp(w1, "rumble") == 0 ? kReqRumble : strcmp(w1, "triggers") == 0 ? kReqTriggers
             : strcmp(w1, "effect") == 0 ? kReqEffect : 0;
        const auto speed = [](long v) { return static_cast<u32>(v < 0 ? 0 : v > 65535 ? 65535 : v); };
        r.a = r.op == kReqEffect ? static_cast<u32>(n[0] < 1 ? 1 : n[0] > 64 ? 64 : n[0]) : speed(n[0]);
        r.b = speed(n[1]);
        r.ms = static_cast<u32>(n[2] < 0 ? 0 : n[2] > 10000 ? 10000 : n[2]);
        if (!r.op)
        {
            snprintf(reply, len, "usage: sdlpad send rumble|triggers <0-65535> <0-65535> <ms> | send effect <bytes 1-64>");
            return;
        }
    }
    else
    {
        snprintf(reply, len, "usage: sdlpad [status] | attach <xbox|ps5|switchpro> [slot] | detach [slot|all] | send "
                             "rumble|triggers <low> <high> <ms> | send effect <bytes> | touch <finger> <x> <y> <1|0> (TEST ONLY, pad_backend 1)");
        return;
    }
    Ask(r, reply, len);
}
} // namespace sdlpad
