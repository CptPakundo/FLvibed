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
    // what core's logic reaches the host for, until core has its own table (a part's HostApi is its module's)
    hostcalls::g = {host->SetHeld, host->ClearAllHeld, host->PushEvent, host->ChatLineAt, host->ServerTag,
                    host->GetSettingsBus, host->GetHudBus, host->GetD3DDevice, host->OptionsPageState, host->ShowOptionsPage,
                    host->UiButtonRect, host->SetUiButton, host->RealMouseWheel, host->SetHudHitSuppression, host->WindowMode,
                    host->version >= 36 ? host->ModuleOn : nullptr, host->version >= 24 ? host->ObjectiveText : nullptr,
                    host->version >= 11 ? host->ObjectiveNow : nullptr, host->version >= 11 ? host->MissionMarks : nullptr,
                    host->version >= 20 ? host->MissionResult : nullptr, host->version >= 35 ? host->MissionSave : nullptr, host->LogWrite,
                    host->version >= 2 ? host->SetSteer : nullptr, host->MouseMoveBy, host->MouseMoveTo, host->MouseReleaseTarget,
                    host->MouseWheel, host->MouseButton, host->MouseButtonDown, host->MouseReset, host->MouseStats,
                    host->MouseIsCaptured, host->MouseCapture, host->version >= 2 ? host->LastRealMouseTick : nullptr,
                    host->version >= 13 ? host->ObjectiveTitle : nullptr, host->version >= 11 ? host->ObjectiveRefs : nullptr,
                    host->version >= 15 ? host->ObjectiveSteps : nullptr, host->version >= 10 ? host->MissionObjects : nullptr,
                    host->version >= 14 ? host->DockReply : nullptr, host->version >= 8 ? host->JumpInCount : nullptr,
                    host->version >= 12 ? host->PlayerHit : nullptr, host->version >= 16 ? host->CommHeardAt : nullptr,
                    host->version >= 17 ? host->SpawnedShips : nullptr, host->version >= 6 ? host->GuardEvade : nullptr,
                    host->version >= 28 ? host->SetGunAim : nullptr, host->version >= 22 ? host->SetHudHotspots : nullptr,
                    host->version >= 22 ? host->HudHotspotHovered : nullptr, host->version >= 34 ? host->GameLoads : nullptr,
                    host->version >= 25 ? host->ObjectiveReread : nullptr,
                    host->version >= 23 ? host->UiGeneration : nullptr, host->version >= 5 ? host->GetRoomCharacters : nullptr,
                    host->version >= 7 ? host->SetKeepRunning : nullptr, host->version >= 20 ? host->SetCursorHidden : nullptr,
                    host->version >= 32 ? host->FixedDt : nullptr, host->version >= 9 ? host->CommPlayer : nullptr, host->SetKeyTrace,
                    nullptr, host->version >= 37 ? host->SetSteerLift : nullptr};
    device_view::Publish();
    ctl::PublishPadFlight();
    ctl::PublishFrame();
    room::SetCharacterSource(HostAtLeast(5) ? host->GetRoomCharacters : nullptr);
    // the front end's clicks run at the frame's end: a dialog's modal loop then keeps the pad and the harness alive
    uiclick::SetDeferral(HostAtLeast(29) ? host->DeferToFrameEnd : nullptr);
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
