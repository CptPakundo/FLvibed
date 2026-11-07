// Core's client with the controller off (pagehost.h): the options page, and with the Autopilot module its flight services.
#include "all.h"

#include "loader.h"
#include "logic/core_api.h"
#include "modules_rules.h"

extern "C" const SettingsBus* flv_settings_bus();
extern "C" const HudBus* flv_hud_bus();

namespace pagehost
{
static CoreHostApi g_host = {};
static const CoreImplApi* g_impl = nullptr;
static bool g_opened = false;
static bool g_faulted = false;
static bool g_giveBack = false; // a fault's give-back is due at the next frame
static bool g_flies = false; // the Autopilot or the HUD runs here: the controller's flight services are ours
static bool g_story = false; // they or Speedrun: the story's events are ours
static bool g_hud = false;
static int g_depth = 0; // logic calls on the stack: a dialog's modal frame never re-enters the logic

static bool Alive() { return g_impl && !g_faulted; }

// A fault or a stop leaves nothing of the logic's in the game: held keys, the steering, the guns' aim, the virtual mouse, the HUD's hits.
static void LetGo()
{
    input::SetHudHitSuppression(false, nullptr, 0);
    router::SetCursorHidden(false); // the HUD's idle cursor
    router::SetLoadDraw(nullptr);   // (its code goes with the logic)
    if (!g_flies)
        return;
    input::ClearAllHeld();
    input::SteerStop();
    router::ApSetGunAim(false, nullptr);
    dimouse::Reset();
}

static void Fault(const char* where, DWORD code)
{
    g_giveBack = g_giveBack || !g_faulted;
    g_faulted = true;
    deferred::Drop();
    LetGo();
    Log("FATAL: exception 0x%08X in the page's %s; the page is off until reload", code, where);
}

static void HostLog(const char* line) { LogWriteLine(line); }
static void HostPushEvent(const char* text) { events::Push("%s", text); }
static void MouseStats(char* out, int outLen) { strncpy_s(out, outLen, dimouse::Stats().c_str(), _TRUNCATE); }

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
            Log("page: Shutdown faulted");
        }
    }
    g_impl = nullptr;
    deferred::Drop();
    LetGo();
}

static bool StartLogic(const void* table)
{
    const CoreImplApi* api = static_cast<const CoreImplApi*>(table);
    if (!api || api->version != kCoreApiVersion)
    {
        Log("page: incompatible logic");
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
        Fault("init", GetExceptionCode());
        return false;
    }
    return true;
}

// The page's debug verbs on the channel there is: the controller's answers them when it is on.
static void UiVerb(int, const char* line, char* reply, size_t len)
{
    if (!Alive())
    {
        strcpy_s(reply, len, "logic DLL not loaded");
        return;
    }
    reply[0] = 0;
    const int depth = g_depth;
    __try
    {
        ++g_depth;
        g_impl->Command(line, reply, static_cast<int>(len));
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        strcpy_s(reply, len, "command faulted");
    }
    g_depth = depth;
}

// As the controller's host: what the logic changed in the game goes back, once, at the top of the next frame.
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
    input::SteerStop();
    uibtn::ShowOptionsPage(false);
    Log("the game's panels and held input given back");
}

static void Frame(double dt)
{
    if (g_giveBack)
        GiveBack();
    if (!g_opened)
    {
        g_opened = true;
        StartLogic(flv_logic_loader()->Open(kLogicCore));
        cmd::Add(cmd::ChannelOf(router::kSubtitles), "ui", true, UiVerb);
        cmd::Add(cmd::ChannelOf(router::kSubtitles), "watchdog", true, UiVerb);
        if (g_story)
            for (const char* verb : {"ap", "goals", "owners", "mode", "fault", "mp", "core"})
                cmd::Add(cmd::ChannelOf(router::kSubtitles), verb, true, UiVerb);
        if (g_hud)
            for (const char* verb : {"hud2", "uiskin", "uistrip", "gametext", "cue"})
                cmd::Add(cmd::ChannelOf(router::kSubtitles), verb, true, UiVerb);
    }
    chatlines::ModeCheck();
    const int depth = g_depth;
    __try
    {
        dimouse::Update();
        if (g_story)
            events::Update();
        uibtn::OnFrame();
        if (Alive())
        {
            ++g_depth;
            g_impl->OnFrame(dt);
            --g_depth;
        }
        if (Alive())
            deferred::Run();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        g_depth = depth;
        Fault("frame", GetExceptionCode());
    }
}

// A dialog's own loop (router.cpp ModalFrame): the frame is served from its render, and the debug channel with it, as the controller's entry does. The subtitles'
// own frame waits for the loop's end.
static void ModalFrame(double dt)
{
    if (g_depth)
        return;
    Frame(dt);
    cmd::Poll(cmd::ChannelOf(router::kSubtitles));
}

static bool LogicOnStack() { return g_depth != 0; }

static void Overlay()
{
    __try
    {
        if (Alive() && router::D3DDevice())
            g_impl->OnOverlay();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        Fault("overlay", GetExceptionCode());
    }
}

static void UnderHud()
{
    __try
    {
        if (Alive() && router::D3DDevice() && Mem<int>(exe::UiRenderSkip) <= 0)
            g_impl->OnUnderHud();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        Fault("under-HUD pass", GetExceptionCode());
    }
}

static void KeyCmd(int cmd, bool down)
{
    __try
    {
        if (Alive())
            g_impl->OnKeyCmd(cmd, down);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        Fault("key command", GetExceptionCode());
    }
}

static void DeviceLost()
{
    __try
    {
        if (g_impl) // a faulted logic too: its state blocks would fail the game's Reset under d3d8to9
            g_impl->OnDeviceLost();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        Fault("device lost", GetExceptionCode());
    }
}

static bool RawKey(int vk, bool down, bool textField)
{
    __try
    {
        return Alive() && g_impl->OnRawKey(vk, down, textField);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        Fault("raw key", GetExceptionCode());
        return false;
    }
}

static void UiButton(int id)
{
    __try
    {
        if (Alive())
            g_impl->OnUiButton(id);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        Fault("button", GetExceptionCode());
    }
}

void Start()
{
    g_host.version = kCoreApiVersion;
    g_host.dllDir = DllDir().c_str();
    g_host.LogWrite = HostLog;
    g_host.calls.GetSettingsBus = flv_settings_bus;
    g_host.calls.GetHudBus = flv_hud_bus;
    g_host.calls.GetD3DDevice = router::D3DDevice;
    g_host.calls.OptionsPageState = uibtn::OptionsPageState;
    g_host.calls.ShowOptionsPage = uibtn::ShowOptionsPage;
    g_host.calls.UiButtonRect = uibtn::UiButtonRect;
    g_host.calls.SetUiButton = uibtn::SetUiButton;
    g_host.calls.RealMouseWheel = dimouse::RealWheel;
    g_host.calls.SetHudHitSuppression = input::SetHudHitSuppression;
    g_host.calls.ModuleOn = router::ModuleOn;
    g_host.calls.PushEvent = HostPushEvent;
    g_host.calls.DeferToFrameEnd = deferred::Add;
    g_host.calls.ChatLineAt = chatlines::LineAt;
    g_host.calls.ServerTag = chatlines::ServerTag;
    g_host.calls.WindowMode = router::WindowMode;
    g_host.calls.LogWrite = HostLog; // the records stay null below unless the Autopilot module runs here: it brings the events
    g_host.calls.SetSteer = input::SetSteer;
    g_host.calls.SetSteerLift = input::SetSteerLift;
    g_host.calls.MouseMoveBy = dimouse::MoveBy;
    g_host.calls.MouseMoveTo = dimouse::MoveTo;
    g_host.calls.MouseReleaseTarget = dimouse::ReleaseTarget;
    g_host.calls.MouseWheel = dimouse::Wheel;
    g_host.calls.MouseButton = dimouse::SetButton;
    g_host.calls.MouseButtonDown = dimouse::ButtonDown;
    g_host.calls.MouseReset = dimouse::Reset;
    g_host.calls.MouseStats = MouseStats;
    g_host.calls.MouseIsCaptured = dimouse::IsCaptured;
    g_host.calls.MouseCapture = dimouse::Capture;
    g_host.calls.LastRealMouseTick = dimouse::LastRealTick;
    g_host.calls.UiGeneration = uihooks::Generation;
    g_host.calls.SetTargetMute = uihooks::SetTargetMute;
    g_host.calls.TargetMuteReady = uihooks::TargetMuteReady;
    g_host.calls.GetRoomCharacters = router::RoomCharacters;
    g_host.calls.SetKeepRunning = focus::SetKeepRunning;
    g_host.calls.SetCursorHidden = router::SetCursorHidden;
    g_host.calls.SetLoadDraw = router::SetLoadDraw;
    g_host.calls.SetFlightLock = input::SetFlightLock;
    g_host.calls.SetWorldPause = input::SetWorldPause;
    g_hud = router::ModuleOn(modules::kHud);
    g_flies = router::ModuleOn(modules::kAutopilot) || g_hud;
    g_story = g_flies || router::ModuleOn(modules::kSpeedrun);
    if (g_hud)
    {
        uihooks::Install(KeyCmd);
        uihooks::InstallTargetMute();
    }
    if (g_story)
    {
        g_host.calls.SetHeld = input::SetHeld;
        g_host.calls.ClearAllHeld = input::ClearAllHeld;
        g_host.calls.ObjectiveText = events::ObjectiveText;
        g_host.calls.ObjectiveNow = events::ObjectiveNow;
        g_host.calls.MissionMarks = events::MissionMarks;
        g_host.calls.MissionResult = events::MissionResult;
        g_host.calls.MissionSave = events::MissionSave;
        g_host.calls.ObjectiveTitle = events::ObjectiveTitle;
        g_host.calls.ObjectiveRefs = events::ObjectiveRefs;
        g_host.calls.ObjectiveSteps = events::ObjectiveSteps;
        g_host.calls.MissionObjects = events::MissionObjects;
        g_host.calls.DockReply = events::DockReply;
        g_host.calls.CommHeardAt = events::CommHeardAt;
        g_host.calls.SpawnedShips = events::SpawnedShips;
        g_host.calls.GameLoads = events::GameLoads;
        g_host.calls.PlayerHit = uihooks::PlayerHit;
        g_host.calls.ObjectiveReread = events::ObjectiveReread;
    }
    if (g_flies)
    {
        g_host.calls.JumpInCount = router::ApJumpInCount;
        g_host.calls.GuardEvade = router::ApGuardEvade;
        g_host.calls.SetGunAim = router::ApSetGunAim;
        g_host.calls.SetHudHotspots = uibtn::SetHudHotspots;
        g_host.calls.HudHotspotHovered = uibtn::HudHotspotHovered;
        g_host.calls.SetHudHitSuppression = input::SetHudHitSuppression;
        if (router::ModuleOn(modules::kAutopilot))
            input::Install();
        uihooks::InstallHits();
    }
    static const LogicClient client = {StopLogic, StartLogic, HostLog};
    flv_logic_loader()->Register(kLogicCore, &client);
    static const router::Client events = {Frame,   nullptr, ModalFrame, LogicOnStack, Overlay,  DeviceLost, nullptr,
                                          nullptr, nullptr, uibtn::AfterManeuverLayout, nullptr, UnderHud, RawKey,  UiButton};
    router::Register(router::kController, &events);
}
} // namespace pagehost
