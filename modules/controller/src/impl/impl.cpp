// Controller logic entry points: in FLvibedLogic.dll in the dev build (hot-reloaded, core\loader.h), linked into
// FLvibed.dll in the release build.
#include "../api.h"
#include "flexe.h"
#include "log.h"
#include "controller.h"
#include "game/debugverbs.h"
#include "device_view.h"
#include "game/coreframe.h"
#include "game/hostcalls.h"
#include "game/modslots.h"
#include "game/mprules.h"
#include "game/room.h"
#include "host.h"
#include "ui/flufaware.h"
#include "ui/options_page.h"

static void ImplInit(const HostApi* host)
{
    g_host = host;
    LogSetSink(host->LogWrite, host->dllDir);
    ::LogClaimPlain(host->LogWrite); // core\game's lines stay here, whichever logic starts last
    // what core's logic reaches the host for, until core has its own table (a part's HostApi is its module's); set by name, so
    // a field added to hostcalls::Table cannot shift the pointers after it onto the wrong services (several share one type)
    hostcalls::Table calls = {};
    calls.SetHeld = host->SetHeld;
    calls.ClearAllHeld = host->ClearAllHeld;
    calls.PushEvent = host->PushEvent;
    calls.ChatLineAt = host->ChatLineAt;
    calls.ServerTag = host->ServerTag;
    calls.GetSettingsBus = host->GetSettingsBus;
    calls.GetHudBus = host->GetHudBus;
    calls.GetD3DDevice = host->GetD3DDevice;
    calls.OptionsPageState = host->OptionsPageState;
    calls.ShowOptionsPage = host->ShowOptionsPage;
    calls.UiButtonRect = host->UiButtonRect;
    calls.SetUiButton = host->SetUiButton;
    calls.RealMouseWheel = host->RealMouseWheel;
    calls.SetHudHitSuppression = host->SetHudHitSuppression;
    calls.WindowMode = host->WindowMode;
    calls.ModuleOn = host->ModuleOn;
    calls.ObjectiveText = host->ObjectiveText;
    calls.ObjectiveNow = host->ObjectiveNow;
    calls.MissionMarks = host->MissionMarks;
    calls.MissionResult = host->MissionResult;
    calls.MissionSave = host->MissionSave;
    calls.LogWrite = host->LogWrite;
    calls.SetSteer = host->SetSteer;
    calls.MouseMoveBy = host->MouseMoveBy;
    calls.MouseMoveTo = host->MouseMoveTo;
    calls.MouseReleaseTarget = host->MouseReleaseTarget;
    calls.MouseWheel = host->MouseWheel;
    calls.MouseButton = host->MouseButton;
    calls.MouseButtonDown = host->MouseButtonDown;
    calls.MouseReset = host->MouseReset;
    calls.MouseStats = host->MouseStats;
    calls.MouseIsCaptured = host->MouseIsCaptured;
    calls.MouseCapture = host->MouseCapture;
    calls.LastRealMouseTick = host->LastRealMouseTick;
    calls.ObjectiveTitle = host->ObjectiveTitle;
    calls.ObjectiveRefs = host->ObjectiveRefs;
    calls.ObjectiveSteps = host->ObjectiveSteps;
    calls.MissionObjects = host->MissionObjects;
    calls.DockReply = host->DockReply;
    calls.JumpInCount = host->JumpInCount;
    calls.PlayerHit = host->PlayerHit;
    calls.CommHeardAt = host->CommHeardAt;
    calls.SpawnedShips = host->SpawnedShips;
    calls.GuardEvade = host->GuardEvade;
    calls.SetGunAim = host->SetGunAim;
    calls.SetHudHotspots = host->SetHudHotspots;
    calls.HudHotspotHovered = host->HudHotspotHovered;
    calls.GameLoads = host->GameLoads;
    calls.ObjectiveReread = host->ObjectiveReread;
    calls.UiGeneration = host->UiGeneration;
    calls.GetRoomCharacters = host->GetRoomCharacters;
    calls.SetKeepRunning = host->SetKeepRunning;
    calls.SetCursorHidden = host->SetCursorHidden;
    calls.FixedDt = host->FixedDt;
    calls.CommPlayer = host->CommPlayer;
    calls.SetKeyTrace = host->SetKeyTrace;
    // calls.DeferToFrameEnd stays null: core's page host only (this entry sets it by uiclick::SetDeferral)
    calls.SetSteerLift = host->SetSteerLift;
    calls.SetTargetMute = host->SetTargetMute;
    calls.TargetMuteReady = host->TargetMuteReady;
    calls.SetLoadDraw = host->SetLoadDraw;
    calls.SetWorldPause = host->SetWorldPause;
    calls.SetFlightLock = host->SetFlightLock;
    calls.OfferDifficulties = host->OfferDifficulties;
    calls.WorldCommand = host->WorldCommand;
    calls.HoldRealWheel = host->HoldRealWheel;
    calls.InstallHud2Hooks = host->InstallHud2Hooks;
    calls.SetRawMouse = host->SetRawMouse;
    calls.SetQuietZones = host->SetQuietZones;
    calls.MapZoneEdges = host->MapZoneEdges;
    calls.MapZoneIds = host->MapZoneIds;
    hostcalls::g = calls;
    device_view::Publish();
    ctl::PublishPadFlight();
    ctl::PublishFrame();
    room::SetCharacterSource(host->GetRoomCharacters);
    // the front end's clicks run at the frame's end: a dialog's modal loop then keeps the pad and the harness alive
    uiclick::SetDeferral(host->DeferToFrameEnd);
    Log("Logic DLL init (build %s %s)", __DATE__, __TIME__);
    exe::ResolveAll("controller logic");
    modslots::Start();
    ctl::RegisterBus(); // before Init loads the ini: the bus's defaults = the struct initialisers
    ctl::Init();
    options_page::Init();
}

static void ImplShutdown()
{
    debug::StopWatch(); // its exception handler is code of this DLL
    uiclick::SetDeferral(nullptr);
    options_page::Shutdown();
    views::BarShutdown(); // the glyph texture belongs to this DLL
    ctl::Shutdown();
    ctl::UnregisterBus();
    modslots::Stop();
    device_view::Withdraw();
    ctl::WithdrawPadFlight();
    ctl::WithdrawFrame();
    Log("Logic DLL shutdown");
}

static void ImplOnFrame(double dt) { coreframe::OnFrame(dt); }

static void ImplCommand(const char* line, char* reply, int replyLen)
{
    std::string r;
    try
    {
        r = debug::Execute(line);
    }
    catch (...)
    {
        r = "exception";
    }
    strncpy_s(reply, replyLen, r.c_str(), _TRUNCATE);
}

static void ImplOnRenderOverlay() { coreframe::OnRenderOverlay(); }
static void ImplOnDeviceLost() { coreframe::OnDeviceLost(); }
static void ImplOnDeviceReset() {}

static void ImplOnRenderUnderHud() { coreframe::OnRenderUnderHud(); }

static void ImplOnUiButton(int id)
{
    if (!coreframe::OnUiButton(id))
        options_page::OnUiButton(id);
}

static bool ImplOnRawKey(int vk, bool down, bool textField) { return coreframe::OnRawKey(vk, down, textField); }
static void ImplOnKeyCmd(int cmd, bool down) { coreframe::OnKeyCmd(cmd, down); }
static void ImplOnFault() { coreframe::OnFault(); }

static const ImplApi g_api = {kImplApiVersion,  ImplInit,          ImplShutdown,         ImplOnFrame,
                              ImplCommand,      ImplOnRenderOverlay, ImplOnDeviceLost,   ImplOnDeviceReset,
                              ImplOnRenderUnderHud, ImplOnUiButton, ImplOnRawKey, ImplOnKeyCmd, ImplOnFault};

// The module's table for the logic's entry (core\logic\entry.cpp).
extern "C" const void* flv_controller_impl_api() { return &g_api; }
