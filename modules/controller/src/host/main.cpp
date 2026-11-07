// The controller module's host part (started by core\flvibed.cpp when the module is on): a client of the event router
// (core\host\router.h) with the hooks only this module needs; forwards events to the controller logic (core\loader.h:
// FLvibedLogic.dll, hot-reloadable, in the dev build, linked in in the release build).
#include "common.h"
#include "modules_rules.h"

#include "../api.h"
#include "game/events.h"
#include "flexe.h"
#include "hook.h"
#include "loader.h"
#include "log.h"
#include "safemem.h"
#include "version.h"
#include "../shm.h"
#include "hapticwave.h"
#include "sdlpad.h"
#include "host/moviehooks.h"

// The settings bus: one instance in FLvibed.dll (core\settings_bus.cpp, compiled plain), reached by this C symbol.
extern "C" const SettingsBus* flv_settings_bus();
extern "C" const HudBus* flv_hud_bus();

// ------------------------------------------------------------------ state
static HostApi g_host = {};
static const ImplApi* g_impl = nullptr;
static int g_implGeneration = 0; // the logic asked for: 0 = not yet (the first frame does)
static bool g_faulted = false;
static bool g_giveBack = false; // a fault's give-back is due at the next frame
static bool g_reloadRequested = false;
static bool g_movieSdl = false; // the startup movies started the SDL thread (OnMoviePoll): the logic starts it its own way
// The SDL thread reads only when asked (sdlpad::Pad kicks one read): during the movies no frame asks, so this thread does, every 30 ms, until the logic starts
static volatile LONG g_movieAsk = 0;
static HANDLE g_movieAsker = nullptr;
static DWORD WINAPI MovieAsker(void*)
{
    while (g_movieAsk)
    {
        SdlPadState st = {};
        sdlpad::Pad(2, &st);
        ::Sleep(30);
    }
    return 0;
}
static void StopMovieSdl()
{
    if (!g_movieSdl)
        return;
    InterlockedExchange(&g_movieAsk, 0);
    if (g_movieAsker)
        WaitForSingleObject(g_movieAsker, 1000), CloseHandle(g_movieAsker), g_movieAsker = nullptr;
    sdlpad::Pad(0, nullptr);
    g_movieSdl = false;
}



// ------------------------------------------------------------------ host API functions
static void HostLogWrite(const char* line) { LogTo(g_logChannel, line); }
// The logic faulted (any of its entry points): every override it set on the host ends with it - held commands, the
// pad's steering, the HUD's hit suppression, the hidden cursor, the virtual mouse - or the ship keeps turning / firing
static void HostFailSafe()
{
    g_giveBack = g_giveBack || !g_faulted;
    g_faulted = true;
    deferred::Drop(); // what the logic queued is the logic's code
    input::ClearAllHeld();
    input::SteerStop();
    input::SetHudHitSuppression(false, nullptr, 0);
    router::ApSetGunAim(false, nullptr);
    router::SetCursorHidden(false);
    router::SetLoadDraw(nullptr);
    dimouse::Reset();
}
static void HostSetKeyTrace(bool on) { uihooks::TraceKeys(on); }
static void HostMouseStats(char* out, int len) { strncpy_s(out, len, dimouse::Stats().c_str(), _TRUNCATE); }
static void HostPushEvent(const char* text) { events::Push("%s", text); }
static void HostIdsText(u32 ids, char* out, int len) { strncpy_s(out, len, events::IdsText(ids).c_str(), _TRUNCATE); }

// ------------------------------------------------------------------ the startup movies: a pad skips them as a key does
// (the owner, 7 Nov 2026: "Allow controller to skip opening movies, not just keyboard"). Each pass of the movie loop (about 3 a second; the logic does not run
// yet): A, B or Menu pressed since the last pass - XInput's keystroke queue (xinput1_4 / 1_3, loaded as the logic loads XInput) and the SDL thread's latch,
// if it runs - ends the movie the way the game ends a finished one (moviehooks::Skip, as movie_skip). A background game ignores the pad (U38).
static void OnMoviePoll()
{
    using KeystrokeFn = DWORD(WINAPI*)(DWORD, DWORD, XINPUT_KEYSTROKE*);
    static KeystrokeFn keystroke = nullptr;
    static bool looked = false;
    if (!looked)
    {
        looked = true;
        HMODULE x = LoadLibraryW(L"xinput1_4.dll");
        if (!x)
            x = LoadLibraryW(L"xinput1_3.dll");
        if (x)
            keystroke = reinterpret_cast<KeystrokeFn>(GetProcAddress(x, "XInputGetKeystroke"));
    }
    DWORD pid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &pid);
    const bool front = pid == GetCurrentProcessId();
    // the logic starts after the movies, and with it the SDL thread a PlayStation or Switch pad needs (the owner's DualSense Edge skipped nothing,
    // 7 Nov 2026): started here in the native-only mode (2: those pads alone, as the logic's fallback), stopped when the logic starts (StartLogic)
    if (front && !g_movieSdl && !g_impl)
    {
        SdlPadState st = {};
        sdlpad::Pad(2, &st);
        g_movieSdl = true;
        InterlockedExchange(&g_movieAsk, 1);
        g_movieAsker = CreateThread(nullptr, 0, MovieAsker, nullptr, 0, nullptr);
        Log("movie: the SDL pad thread started for the movies' skip%s", g_movieAsker ? "" : " (no asker thread)");
    }
    const u32 sdlPressed = sdlpad::TakePressed();
    if (sdlPressed)
        Log("movie: pad buttons 0x%X seen", sdlPressed);
    bool skip = (sdlPressed & (XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_B | XINPUT_GAMEPAD_START)) != 0;
    XINPUT_KEYSTROKE ks = {};
    while (keystroke && keystroke(XUSER_INDEX_ANY, 0, &ks) == ERROR_SUCCESS)
        skip |= (ks.Flags & XINPUT_KEYSTROKE_KEYDOWN) && (ks.VirtualKey == VK_PAD_A || ks.VirtualKey == VK_PAD_B || ks.VirtualKey == VK_PAD_START);
    if (skip && front && moviehooks::Skip())
        Log("movie: skipped by the pad");
}

// ------------------------------------------------------------------ v18: FLvibed's native buttons (uibtn.cpp) -> the logic
static void OnUiButton(int id)
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
        HostFailSafe();
        Log("FATAL: exception 0x%08X in OnUiButton(%d); logic disabled until reload", GetExceptionCode(), id);
    }
}

static bool GuardedRawKey(int vk, bool down, bool textField);

static bool OnRawKey(int vk, bool down, bool textField)
{
    return g_impl && !g_faulted && g_impl->version >= 3 && g_impl->OnRawKey && GuardedRawKey(vk, down, textField);
}

static void GuardedKeyCmd(int cmd, bool down)
{
    __try
    {
        g_impl->OnKeyCmd(cmd, down);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        HostFailSafe();
        Log("FATAL: exception 0x%08X in OnKeyCmd; logic disabled until reload", GetExceptionCode());
    }
}

static void KeyCmd(int cmd, bool down)
{
    if (g_impl && !g_faulted && g_impl->version >= 4 && g_impl->OnKeyCmd)
        GuardedKeyCmd(cmd, down);
}

static bool GuardedRawKey(int vk, bool down, bool textField)
{
    __try
    {
        return g_impl->OnRawKey(vk, down, textField);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        HostFailSafe();
        Log("FATAL: exception 0x%08X in OnRawKey; logic disabled until reload", GetExceptionCode());
        return false;
    }
}

// ------------------------------------------------------------------ the logic (core\loader.h)
static void StopLogic()
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
    deferred::Drop(); // what the logic queued for the frame's end is the logic's code
    input::ClearAllHeld();
    input::SetSteer(false, 0, 0);
    input::SetHudHitSuppression(false, nullptr, 0);
    router::ApSetGunAim(false, nullptr); // v28: the guns' aim goes back to the game
    router::SetCursorHidden(false); // v20: the pad screens' cursor hide ends with the logic
    router::SetLoadDraw(nullptr);   // v40: its code goes with the logic
    dimouse::Reset();
}

static bool StartLogic(const void* table)
{
    StopMovieSdl();
    const ImplApi* api = static_cast<const ImplApi*>(table);
    if (!api || api->version != kImplApiVersion)
    {
        Log("LoadImpl: incompatible logic");
        return false;
    }
    g_impl = api;
    g_faulted = g_giveBack = false;
    __try
    {
        g_impl->Init(&g_host);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        HostFailSafe();
        Log("FATAL: exception 0x%08X in the logic's Init; logic disabled until reload", GetExceptionCode());
        return false;
    }
    Log("FLvibed " FLV_VERSION ": logic started (%s)", flv_logic_loader()->Name());
    return true;
}

static bool LoadImpl()
{
    ++g_implGeneration;
    return StartLogic(flv_logic_loader()->Open(kLogicController));
}

// ------------------------------------------------------------------ debug channel
// The verbs this module's host part answers; the rest of the channel is core\host\commands.h's and the logic's.
// events [after-seq]: event log lines newer than after-seq (first line: "last <seq>")
static void EventsVerb(int, const char* cmd, char* reply, size_t len)
{
    const u32 after = cmd[6] ? static_cast<u32>(strtoul(cmd + 7, nullptr, 10)) : 0;
    const std::string out =
        "last " + std::to_string(events::LastSeq()) + "\n" + events::Since(after, static_cast<int>(len) - 64);
    strcpy_s(reply, len, out.c_str());
}

// uibtn | uibtn click <id> | uibtn mesh [<path relative to DATA>]: the native buttons' state and the raw-key counters, a
// button's click, the bar button's mesh probe
static void UiBtnReply(char* reply, size_t len)
{
    strcpy_s(reply, len, (uibtn::Status() + input::RawKeyStatus()).c_str());
}


// Logic calls on the stack (OnFrame, Command): a modal-loop frame (below) never re-enters the logic.
static int g_implDepth = 0;
struct ImplDepth
{
    ImplDepth() { ++g_implDepth; }
    ~ImplDepth() { --g_implDepth; }
};

static int g_channel = -1; // the debug channel (core\host\commands.h)

static void PollCommands()
{
    ImplDepth depth; // also host-side commands: a reload inside a modal frame stays one at a time
    cmd::Poll(g_channel);
}

static void UiBtnVerb(int, const char* cmd, char* reply, size_t len)
{
    if (strncmp(cmd + 5, " click ", 7) == 0)
        strcpy_s(reply, len, uibtn::DebugClick(atoi(cmd + 12)) ? "clicked" : "no such button (not created)");
    else if (strncmp(cmd + 5, " mesh", 5) == 0)
    {
        uibtn::SetBarMesh(cmd[10] == ' ' ? cmd + 11 : "");
        strcpy_s(reply, len, "mesh set for the next HUD build (reload a save)");
    }
    else
        UiBtnReply(reply, len);
}

// the UI generation counter (HostApi v23) and its two halves
static void UiGenVerb(int, const char*, char* reply, size_t len) { uihooks::GenerationReply(reply, len); }
// borderless status / altenter
// SDL virtual test pads (TEST ONLY)
static void SdlPadVerb(int, const char* cmd, char* reply, size_t len) { sdlpad::Command(cmd + 6, chatlines::Multiplayer(), reply, len); }
// background test starts only: a device Reset as the game's own
static void D3dResetVerb(int, const char*, char* reply, size_t len) { router::D3dResetReply(reply, len); }

// Every other line: the logic's.
static void LogicCommand(int, const char* cmd, char* reply, size_t len)
{
    if (g_impl && !g_faulted)
    {
        reply[0] = 0;
        __try
        {
            g_impl->Command(cmd, reply, static_cast<int>(len));
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            strcpy_s(reply, len, "command faulted");
        }
    }
    else
        strcpy_s(reply, len, "logic DLL not loaded");
}

// ------------------------------------------------------------------ game hooks
static void ImplOnFrame(double dt)
{
    ImplDepth depth;
    g_impl->OnFrame(dt);
}

static void GuardedFrame(double dt)
{
    const int depth = g_implDepth; // a fault skips ImplDepth's destructor: left counted, no modal frame would run again
    __try
    {
        if (g_implGeneration == 0)
            LoadImpl();
        PollCommands();
        dimouse::Update();
        events::Update();
        uibtn::OnFrame();
        chatlines::ModeCheck();
        if (g_impl && !g_faulted)
            ImplOnFrame(dt);
        if (!g_faulted)
            deferred::Run();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        g_implDepth = depth;
        HostFailSafe();
        Log("FATAL: exception 0x%08X in controller update; logic disabled until reload", GetExceptionCode());
    }
}

// A dialog's own loop (core\host\router.cpp ModalFrame): no frame update comes, so the frame is served from its render.
static void OnModalFrame(double dt)
{
    if (g_implDepth)
        return;
    if (ShmBlock* s = shm::Get())
        ++s->heartbeat;
    GuardedFrame(dt);
}

static bool LogicOnStack() { return g_implDepth != 0; }

// ---- Fixed dt (IDEAS T2, 30 Oct 2026; research\evidence\timing\probe_report.md): TEST ONLY, single player only, set
// by the debug command `fixeddt` (HostApi v32). The game's frame timer (exe::FrameTimer) waits for 1/[FpsCapF], then
// writes the clamped dt every reader takes (exe::FrameDt: FrameUpdate, ~90 getter callers, Server.dll's ElapseTime,
// the pub::SetTimer timers, Cnd_Timer); with fixed dt on, the cap is lifted before it runs (no minimum frame time: the
// game resets the cap on state entry, so every frame) and its outputs are overwritten with 1/fps after it - the game
// runs as fast as it draws, each frame advancing 1/fps of game time. Left on wall time (the probe's list): the server's
// own clock (pub::GetTime, the save's time), its damage timestamps, audio and comm lengths.
typedef void(__cdecl* FrameTimerFn)();
static FrameTimerFn g_origFrameTimer = nullptr;
static double g_fixedDt = 0; // seconds per frame, 0 = off
static float g_fpsCapSaved = 0;
static void __cdecl FrameTimerHook()
{
    const double fixed = g_fixedDt;
    if (fixed > 0)
        Mem<float>(exe::FpsCapF) = 100000.0f;
    g_origFrameTimer();
    if (fixed > 0)
    {
        const double was = Mem<double>(exe::FrameDt);
        Mem<double>(exe::FrameDt) = fixed;
        Mem<double>(exe::FrameDtRaw) = fixed;
        Mem<double>(exe::FrameDtAccum) += fixed - was;
    }
}
// The single-player server's own clock (pub::GetTime / GetRwTime, the save's time): Server.dll's ElapseTime (RVA 0x65160
// in HDE 0.7.1) adds Delta::read_dt() - wall time - to it: mov ecx,<delta>; call [read_dt]; fcom [<max>]; ...; fld
// [<clock>]; push esi; mov esi,[esp+8]; fadd st,st1; push esi; fstp [<clock>]. With fixed dt on, FrameUpdateHook sets
// it to its value before the frame + the frame's game dt whenever ElapseTime moved it (a paused frame leaves it alone).
// The clock's address: both operands of that sequence, equal. 0 = not found (logged once per Server.dll load).
static const char* const kServerClockSig =
    "B9 ?? ?? ?? ?? FF 15 ?? ?? ?? ?? DC 15 ?? ?? ?? ?? DF E0 F6 C4 05 7A 08 DD D8 DD 05 ?? ?? ?? ?? DD 05 ?? ?? ?? ?? 56 "
    "8B 74 24 08 D8 C1 56 DD 1D";
static bool ReadD(uintptr_t a, double& v)
{
    __try
    {
        v = *reinterpret_cast<const double*>(a);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}
static bool WriteD(uintptr_t a, double v)
{
    __try
    {
        *reinterpret_cast<double*>(a) = v;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}
static uintptr_t ServerClock()
{
    static HMODULE seen = nullptr;
    static uintptr_t clock = 0;
    const HMODULE s = GetModuleHandleA("server.dll");
    if (s != seen)
    {
        seen = s, clock = 0;
        const uintptr_t at = s ? hk::FindInModule("server.dll", kServerClockSig, 0) : 0;
        u32 a = 0, b = 0;
        if (at && safemem::Read(at + 34, &a, 4) && safemem::Read(at + 48, &b, 4) && a == b)
            clock = a;
        Log("fixeddt: the server clock %s (0x%08X)", clock ? "found" : "NOT found - it stays on wall time", clock);
    }
    return clock;
}

// v32: fps > 0 = fixed dt of 1/fps (refused, -1: multiplayer, the hook or a symbol missing, fps outside 10..1000); 0 =
// off (the cap restored); < 0 = read only. Returns the fixed fps now (0 = off).
static int HostFixedDt(int fps)
{
    const auto now = [] { return g_fixedDt > 0 ? static_cast<int>(1.0 / g_fixedDt + 0.5) : 0; };
    if (fps < 0)
        return now();
    if (fps == 0)
    {
        if (g_fixedDt > 0 && g_fpsCapSaved > 0)
            Mem<float>(exe::FpsCapF) = g_fpsCapSaved;
        g_fixedDt = 0;
        Log("fixeddt off");
        return 0;
    }
    if (fps < 10 || fps > 1000 || !g_origFrameTimer || !exe::FpsCapF.ok() || !exe::FrameDt.ok() || !exe::FrameDtRaw.ok() ||
        !exe::FrameDtAccum.ok() || chatlines::Multiplayer())
        return -1;
    if (g_fixedDt <= 0)
        g_fpsCapSaved = Mem<float>(exe::FpsCapF);
    g_fixedDt = 1.0 / fps;
    Log("fixeddt %d: game dt %.6f s per frame (the cap %.0f lifted)", fps, g_fixedDt, g_fpsCapSaved);
    return fps;
}

static uintptr_t g_serverClock = 0;
static double g_clockBefore = 0;
static bool g_haveClock = false;

// What the logic changed in the game goes back (coreframe::OnFault), once, here: no logic on the stack, outside the scene, before the next UI pass. The fault
// itself may have come in a Reset or a draw pass; never from a modal frame.
static void GiveBack()
{
    g_giveBack = false;
    if (g_impl)
    {
        __try
        {
            g_impl->OnFault();
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            Log("the fault's give-back faulted");
        }
    }
    input::SteerStop(); // (the release above asks for the steering again)
    uibtn::ShowOptionsPage(false); // the page's place back to the game's: nothing draws it now
    Log("the game's panels and held input given back");
}

static void BeforeGame(double dt)
{
    if (ShmBlock* s = shm::Get())
        ++s->heartbeat;
    if (g_giveBack)
        GiveBack();
    GuardedFrame(dt);
    // fixed dt (TEST ONLY): the server's own clock follows the game dt too (ServerClock above)
    g_serverClock = g_fixedDt > 0 ? ServerClock() : 0;
    g_haveClock = g_serverClock && ReadD(g_serverClock, g_clockBefore);
}

static void AfterGame(double dt)
{
    double clockAfter = 0;
    if (g_haveClock && ReadD(g_serverClock, clockAfter) && clockAfter != g_clockBefore)
        WriteD(g_serverClock, g_clockBefore + dt);
}

static void GuardedOverlay()
{
    __try
    {
        if (g_impl && !g_faulted && router::D3DDevice())
            g_impl->OnRenderOverlay();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        HostFailSafe();
        Log("FATAL: exception 0x%08X in overlay; logic disabled until reload", GetExceptionCode());
    }
}

static void GuardedUnderHud()
{
    __try
    {
        if (g_impl && !g_faulted && router::D3DDevice() && Mem<int>(exe::UiRenderSkip) <= 0)
            g_impl->OnRenderUnderHud();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        HostFailSafe();
        Log("FATAL: exception 0x%08X in under-HUD drawing; logic disabled until reload", GetExceptionCode());
    }
}


// ------------------------------------------------------------------ comm player (exe::Comm)
// A comm starts playing: only remembers the player object.
static void* volatile g_commPlayer = nullptr;

static void OnComm(void* player, const u8*) { g_commPlayer = player; }

static void* HostCommPlayer() { return g_commPlayer; }

// ------------------------------------------------------------------ the device (core\host\router.cpp)
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
        HostFailSafe();
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
        HostFailSafe();
        Log("FATAL: exception 0x%08X in device-reset; logic disabled until reload", GetExceptionCode());
    }
}

static void DeviceLost()
{
    if (g_impl) // a faulted logic too: its state blocks would fail the game's Reset under d3d8to9
        GuardedDeviceLost();
}

static void DeviceReset()
{
    if (g_impl && !g_faulted)
        GuardedDeviceReset();
}


// ------------------------------------------------------------------ init
void Start(HMODULE)
{
    g_logChannel = router::LogChannel(router::kController);
    Log("FLController host starting (build %s %s)", __DATE__, __TIME__);

    hk::Owner own(router::OwnerOf(router::kController));
    static const LogicClient client = {StopLogic, StartLogic, HostLogWrite};
    flv_logic_loader()->Register(kLogicController, &client);
    static const router::Client events = {BeforeGame,  AfterGame,   OnModalFrame, LogicOnStack,
                                          GuardedOverlay, DeviceLost, DeviceReset,  nullptr,
                                          OnComm,      uibtn::AfterManeuverLayout, nullptr,
                                          GuardedUnderHud, OnRawKey,  OnUiButton, nullptr, OnMoviePoll};
    router::Register(router::kController, &events);

    shm::Init();
    if (ShmBlock* s = shm::Get())
    {
        g_channel = cmd::Open(router::kController, &s->cmdSeq, &s->cmdAck, s->cmd, sizeof(s->cmd), s->reply, sizeof(s->reply),
                              LogicCommand);
        cmd::Add(g_channel, "events", true, EventsVerb);
        cmd::Add(g_channel, "uibtn", true, UiBtnVerb);
        cmd::Add(g_channel, "uigen", false, UiGenVerb);
        cmd::Add(g_channel, "sdlpad", true, SdlPadVerb);
        cmd::Add(g_channel, "d3dreset", false, D3dResetVerb);
    }

    g_host.version = kHostApiVersion;
    g_host.dllDir = DllDir().c_str();
    g_host.shm = shm::Get();
    g_host.LogWrite = HostLogWrite;
    g_host.SetHeld = input::SetHeld;
    g_host.ClearAllHeld = input::ClearAllHeld;
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
    g_host.GetD3DDevice = router::D3DDevice;
    g_host.SetSteer = input::SetSteer;
    g_host.LastRealMouseTick = dimouse::LastRealTick;
    g_host.PushEvent = HostPushEvent;
    g_host.IdsText = HostIdsText;
    g_host.SetHudHitSuppression = input::SetHudHitSuppression;
    g_host.GetRoomCharacters = router::RoomCharacters;
    g_host.GuardEvade = router::ApGuardEvade;
    g_host.SetKeepRunning = focus::SetKeepRunning;
    g_host.JumpInCount = router::ApJumpInCount;
    g_host.CommPlayer = HostCommPlayer;
    g_host.MissionObjects = events::MissionObjects;
    g_host.MissionMarks = events::MissionMarks;
    g_host.ObjectiveRefs = events::ObjectiveRefs;
    g_host.ObjectiveNow = events::ObjectiveNow;
    g_host.PlayerHit = uihooks::PlayerHit;
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
    g_host.LastRealKeyTick = input::LastRealKeyTick;
    g_host.RealMouseDown = dimouse::RealButtonDown;
    g_host.RealMouseMotion = dimouse::RealMotion; // v19
    g_host.SetCursorHidden = router::SetCursorHidden; // v20
    g_host.MissionResult = events::MissionResult;
    g_host.GetHudBus = flv_hud_bus; // v21: core\hud_bus.cpp (HUD 2)
    g_host.SetHudHotspots = uibtn::SetHudHotspots;       // v22: HUD 2's clicks (uibtn.cpp)
    g_host.HudHotspotHovered = uibtn::HudHotspotHovered; // v22
    g_host.UiGeneration = uihooks::Generation;              // v23: Extended HUD 2
    g_host.ObjectiveText = events::ObjectiveText;        // v24: HUD 2's objective line
    g_host.ObjectiveReread = events::ObjectiveReread;    // v25: the objective a load restores
    g_host.WindowMode = router::WindowMode;               // v26: who provides borderless (modules\display)
    g_host.RealMouseWheel = dimouse::RealWheel;          // v27: the options page's wheel
    g_host.SetGunAim = router::ApSetGunAim;              // v28: the guns' aim point (the autopilot in turret view)
    g_host.DeferToFrameEnd = deferred::Add;              // v29: front-end clicks out of the logic's stack
    g_host.ChatLineAt = chatlines::LineAt;                 // v30: the chat's lines (a server's permission)
    g_host.ServerTag = chatlines::ServerTag;                 // v31: the session's tag line (kept across logic reloads)
    g_host.FixedDt = HostFixedDt;                        // v32: fixed dt (TEST ONLY: debug `fixeddt`)
    g_host.SdlPad = sdlpad::Pad;                         // v33: the opt-in SDL3 gamepad backend (pad_backend 1; sdlpad.cpp)
    g_host.SdlPadRumble = sdlpad::Rumble;                // v33
    g_host.SdlPadFilter = sdlpad::Filter;                // v38: a mapper's keys and mouse dropped
    g_host.SdlPadTriggers = sdlpad::Triggers;            // v41: Adaptive Triggers
    g_host.SdlPadTriggerRumble = sdlpad::TriggerRumble;  // v41: Impulse Triggers
    g_host.PadWave = hapticwave::Open;                   // v41: the DualSense grips' stream
    g_host.PadWaveFeed = hapticwave::Feed;
    g_host.PadWaveWhy = hapticwave::Why;
    g_host.SdlPadTouchRead = sdlpad::Touch;              // v42: the touchpad's fingers
    g_host.SetWorldPause = input::SetWorldPause;         // v44: a NeuroNet window stops the world
    g_host.OfferDifficulties = events::OfferDifficulties; // v45: the job board's difficulties
    g_host.WorldCommand = input::WorldCommand;           // v45: a command under the NeuroNet pause
    g_host.HoldRealWheel = dimouse::HoldRealWheel;       // v46: the physical wheel kept from the game
    g_host.SetFlightLock = input::SetFlightLock;         // v43: the keys and mouse fly nothing under the full-screen map
    g_host.SetTargetMute = uihooks::SetTargetMute;       // v39: HUD 2's targeting display
    g_host.TargetMuteReady = uihooks::TargetMuteReady;   // v39
    g_host.ModuleOn = router::ModuleOn;                  // v36: the cut modules' switches
    g_host.GameLoads = events::GameLoads;                // v34: the game loads (Content.dll loads)
    g_host.MissionSave = events::MissionSave;            // v35: the story's checkpoint saves (speedrun save:<ids> splits)
    g_host.SetSteerLift = input::SetSteerLift;           // v37: what the autopilot's steering may lift
    g_host.SetLoadDraw = router::SetLoadDraw;            // v40: the load's frame
    // DmgIndSource: sub esp,88h (6 bytes); DmgIndContact: sub esp,3Ch; push esi; mov esi,ecx (6 bytes)
    if (router::ModuleOn(modules::kHud) || router::ModuleOn(modules::kAutopilot))
        uihooks::InstallHits();
    if (router::ModuleOn(modules::kHud))
        uihooks::InstallTargetMute();
    uihooks::Install(KeyCmd);
    input::Install();
    // FrameTimer: push ebp; mov ebp,esp; and esp,-8 (6 bytes). Passes through unless the debug `fixeddt` is on.
    g_origFrameTimer = static_cast<FrameTimerFn>(hk::Detour(exe::FrameTimer, FrameTimerHook, 6, "fixed dt (test only)"));
}
