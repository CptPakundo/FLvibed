// Core's own logic when the controller is off (core\host\pagehost.h): the options page, and core's frame while a module here needs it (alone.h). With the controller
// on, its entry drives both and this table is never opened.
#include "logic/core_api.h"

#include "flexe.h"
#include "game/coreframe.h"
#include "game/deckmode.h"
#include "game/fl.h"
#include "game/gamemode.h"
#include "game/maneuver_watchdog.h"
#include "game/modslots.h"
#include "game/room.h"
#include "log.h"
#include "logic/alone.h"
#include "ui/flufaware.h"
#include "ui/options_page.h"
#include "ui/uiopt.h"

#include <sstream>

static const CoreHostApi* g_host = nullptr;

static void Init(const CoreHostApi* host)
{
    g_host = host;
    LogAttach(host->LogWrite, host->dllDir);
    hostcalls::g = host->calls;
    exe::ResolveAll("core logic");
    uiclick::SetDeferral(hostcalls::g.DeferToFrameEnd); // the front end's clicks: a dialog's modal loop keeps the harness alive
    modslots::Start();
    alone::Init(host->dllDir); // (before the page: it lists the rows on the bus)
    options_page::Init();
    modslots::Init();
}

static void Shutdown()
{
    alone::Shutdown();
    modslots::Shutdown();
    options_page::Shutdown();
    modslots::Stop();
    hostcalls::g = {};
}

static void OnFrame(double dt)
{
    if (alone::On())
    {
        coreframe::OnFrame(dt);
        return;
    }
    modslots::GiveBack();
    // the page's text as core's frame sets it: Steam Deck mode and the player's own size
    deckmode::g_on = deckmode::Active(coreframe::opts.deckMode, static_cast<float>(fl::ScreenWidth()), static_cast<float>(fl::ScreenHeight()));
    uiopt::Get().textSize = deckmode::TextSize(deckmode::g_on, coreframe::opts.textSize);
    const gamemode::Mode now = gamemode::Detect(nullptr, nullptr);
    const char* mode = gamemode::Name(now);
    if (now == gamemode::Mode::Space || now == gamemode::Mode::SpaceUi)
        mwatch::Frame();
    modslots::Frame(modslots::kNpc, static_cast<float>(dt), mode, fl::GamePaused());
    modslots::Frame(modslots::kSpeedrun, static_cast<float>(dt), mode, fl::GamePaused());
    options_page::Update(dt);
    flufaware::Update(static_cast<float>(dt), coreframe::opts.flufWindows);
    if (hostcalls::g.SetKeepRunning)
        hostcalls::g.SetKeepRunning(coreframe::opts.runInBackground);
}

static void OnOverlay()
{
    if (alone::On())
    {
        coreframe::OnRenderOverlay();
        return;
    }
    modslots::Draw(modslots::kSpeedrun);
    options_page::Draw();
}

static void OnUnderHud()
{
    if (alone::On())
        coreframe::OnRenderUnderHud();
    else
        options_page::RenderUnder(hostcalls::g.GetD3DDevice ? hostcalls::g.GetD3DDevice() : nullptr);
}

static bool OnRawKey(int vk, bool down, bool textField)
{
    if (alone::On())
        return coreframe::OnRawKey(vk, down, textField);
    return flufaware::HoldsKeys() ? false : options_page::OnRawKey(vk, down, textField);
}

static void OnUiButton(int id)
{
    if (!(alone::On() && coreframe::OnUiButton(id)))
        options_page::OnUiButton(id);
}

static void OnKeyCmd(int cmd, bool down)
{
    if (alone::On())
        coreframe::OnKeyCmd(cmd, down);
}

static void OnDeviceLost()
{
    if (alone::On())
        coreframe::OnDeviceLost();
}

static void Command(const char* line, char* reply, int replyLen)
{
    std::istringstream in(line);
    std::string op;
    in >> op; // the verb that brought it here
    std::string answer;
    if (!alone::Command(op, in, answer))
        answer = options_page::Command(in);
    strncpy_s(reply, replyLen, answer.c_str(), _TRUNCATE);
}

static const CoreImplApi g_api = {kCoreApiVersion, Init, Shutdown, OnFrame, OnOverlay, OnUnderHud, OnRawKey, OnUiButton, Command, OnKeyCmd, OnDeviceLost, coreframe::OnFault};

extern "C" const void* flv_core_impl_api() { return &g_api; }
