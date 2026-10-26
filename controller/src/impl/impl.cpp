// Controller logic entry points: FLControllerImpl.dll in the dev build (hot-reloaded by the controller host in
// FLvibed.dll), linked into FLvibed.dll in the release build.
#include "../api.h"
#include "flexe.h"
#include "log.h"
#include "controller.h"
#include "debug.h"
#include "game/room.h"
#include "host.h"
#include "hud2.h"
#include "flufaware.h"
#include "goals.h"
#include "options_page.h"
// The bar button (barbutton.cpp, the HOST track of the UI build): its clicks and the autopilot key are dispatched to it
// once its source exists in this folder (until then the logic links without it; the page's entry points stand).
#if __has_include("barbutton.cpp")
#include "barbutton.h"
#define FLV_HAVE_BARBUTTON 1
#endif

static void ImplInit(const HostApi* host)
{
    g_host = host;
    LogSetSink(host->LogWrite, host->dllDir);
    room::SetCharacterSource(HostAtLeast(5) ? host->GetRoomCharacters : nullptr);
    Log("Logic DLL init (build %s %s)", __DATE__, __TIME__);
    exe::ResolveAll("controller logic");
    ctl::RegisterBus(); // before Init loads the ini: the bus's defaults = the struct initialisers
    ctl::Init();
    options_page::Init();
}

static void ImplShutdown()
{
    debug::StopWatch(); // its exception handler is code of this DLL
    options_page::Shutdown();
#ifdef FLV_HAVE_BARBUTTON
    barbutton::Shutdown(); // the glyph texture belongs to this DLL
#endif
    ctl::Shutdown();
    ctl::UnregisterBus();
    Log("Logic DLL shutdown");
}

static void ImplOnFrame(double dt) { ctl::OnFrame(dt); }

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

static void ImplOnRenderOverlay() { ctl::OnRenderOverlay(); }
static void ImplOnDeviceLost() { ctl::OnDeviceLost(); }
static void ImplOnDeviceReset() { ctl::OnDeviceReset(); }

static void ImplOnRenderUnderHud() { ctl::OnRenderUnderHud(); }

// v3: a host-created native button (api.h UiButton) was clicked.
static void ImplOnUiButton(int id)
{
    if (id == kUiBtnBar)
    {
#ifdef FLV_HAVE_BARBUTTON
        barbutton::OnClick();
#else
        Log("ui button %d (bar) clicked: barbutton.cpp not built", id);
#endif
        return;
    }
    if (id >= kUiBtnHud2 && id < kUiBtnHud2 + kUiBtnHud2Count) // HostApi v22: HUD 2's keyboard-and-mouse hotspots
    {
        if (goals::OwnsHotspots()) // the open goal panel's keyboard + mouse form holds the click box (HUD 2's set is off)
            goals::OnHotspot(id - kUiBtnHud2);
        else
            hud2::OnHotspot(id - kUiBtnHud2);
        return;
    }
    options_page::OnUiButton(id);
}

// v3: a raw key as the game's key handler sees it; true = swallowed. The open page first (its navigation), then the
// bar button's autopilot key.
static bool ImplOnRawKey(int vk, bool down, bool textField)
{
    // FLUF's text box (a filter field) has the keyboard or its command palette is open (focused or not: the owner, 26
    // Oct 2026 - "much like a chat text box being open"): every key is FLUF's (FLvibed's hook sees keys before FLUF's;
    // FLUF withholds them from the game itself) - the autopilot key, the page's keys and every FLvibed key stand down
    if (flufaware::HoldsKeys())
        return false;
    if (options_page::OnRawKey(vk, down, textField))
        return true;
    if (goals::OnRawKey(vk, down, textField)) // the open goal panel's keys (its keyboard + mouse form)
        return true;
#ifdef FLV_HAVE_BARBUTTON
    return barbutton::OnKey(vk, down, textField);
#else
    return false;
#endif
}

static const ImplApi g_api = {kImplApiVersion,  ImplInit,          ImplShutdown,         ImplOnFrame,
                              ImplCommand,      ImplOnRenderOverlay, ImplOnDeviceLost,   ImplOnDeviceReset,
                              ImplOnRenderUnderHud, ImplOnUiButton, ImplOnRawKey};

#ifdef FLV_STATIC
// Release build: linked into FLvibed.dll; the host takes the API through this (unique, unexported) name.
extern "C" const void* flv_controller_impl_api() { return &g_api; }
#else
extern "C" __declspec(dllexport) const ImplApi* __cdecl GetImplApi() { return &g_api; }

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
        DisableThreadLibraryCalls(instance);
    return TRUE;
}
#endif
