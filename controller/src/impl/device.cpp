#include "device.h"

#include "../dimouse.h"
#include "../events.h"
#include "gamepad.h"
#include "host.h"
#include "log.h"
#include "uiopt.h"

#include <sstream>

namespace device
{
constexpr float kAxisGrace = 0.4f; // s after a switch during which the other device's axis-only signals are ignored

static Device g_active = Device::KbMouse; // the game starts with its own UI: the pad takes over on its first input
static bool g_switched = false;
static float g_grace = 0;         // seconds of axis grace left for the device that lost
static u32 g_lastPadTick = 0, g_lastKbmTick = 0;
static u32 g_lastKeyTick = 0, g_lastMouseTick = 0; // the host ticks as last seen
static int g_mouseFrames = 0;                      // consecutive frames with physical mouse activity
static bool g_realPadWas = false;
static bool g_disconnectPause = false;
static u32 g_switches = 0;
static const char* g_lastWhy = "start";

static void Switch(Device d, const char* why)
{
    if (d == g_active)
        return;
    g_active = d;
    g_switched = true;
    g_grace = kAxisGrace;
    g_lastWhy = why;
    ++g_switches;
    const u32 now = GetTickCount();
    (d == Device::Pad ? g_lastPadTick : g_lastKbmTick) = now;
    events::Push("device %s (%s)", d == Device::Pad ? "pad" : "kbm", why);
}

static u32 HostKeyTick()
{
    return HostAtLeast(18) && g_host->LastRealKeyTick ? g_host->LastRealKeyTick() : 0;
}

static bool HostMouseDown()
{
    if (!HostAtLeast(18) || !g_host->RealMouseDown)
        return false;
    return g_host->RealMouseDown(0) || g_host->RealMouseDown(1) || g_host->RealMouseDown(2);
}

void Update(float dt, const Gamepad& pad, bool flying)
{
    g_switched = false;
    g_grace = std::max(0.0f, g_grace - dt);
    const PadState& p = pad.Cur();
    const uiopt::UiOptions& o = uiopt::Get();

    // Pad signals: buttons (the D-pad, the stick clicks, View / Menu are buttons; PAD_LT / PAD_RT past 0.25 count as
    // buttons too but the device wants 0.5) and axes (dead-zoned sticks, triggers past 0.5).
    const bool padButton = (p.buttons & 0xFFFF) != 0 || p.lt > 0.5f || p.rt > 0.5f;
    const bool padAxis = p.lx != 0 || p.ly != 0 || p.rx != 0 || p.ry != 0;

    // Keyboard and mouse signals: the ticks advance only on physical input (the host excludes FLvibed's own).
    const u32 keyTick = HostKeyTick();
    const bool key = keyTick != 0 && keyTick != g_lastKeyTick;
    g_lastKeyTick = keyTick;
    const u32 mouseTick = dimouse::LastRealTick();
    const bool mouseTickNow = mouseTick != 0 && mouseTick != g_lastMouseTick;
    // Motion: the host reports activity, not its size (the 8-10 px threshold of UI_PLAN 6.2 needs the host to expose
    // the physical motion - requested); until then one report is read as jitter, a second within 150 ms as a move.
    // (Was: two in consecutive frames - the tick has GetTickCount's ~16 ms steps, so above ~60 fps a moving mouse rarely
    // advanced it two frames running: the pad kept the device and its aim pulled the cursor back to the centre in flight,
    // the owner, 26 Oct 2026.)
    const bool mouseMotion = mouseTickNow && g_lastMouseTick != 0 && mouseTick - g_lastMouseTick <= 150;
    g_lastMouseTick = mouseTick;
    g_mouseFrames = mouseTickNow ? g_mouseFrames + 1 : 0;
    const bool mouseButton = HostMouseDown();

    // pause_on_disconnect: a real pad that drops out in flight while it was the device in use.
    const bool realPad = pad.HasRealController();
    if (g_realPadWas && !realPad && flying && g_active == Device::Pad && o.pauseOnDisconnect)
    {
        g_disconnectPause = true;
        events::Push("device pad disconnected: pause");
    }
    g_realPadWas = realPad;

    if (o.inputMode == 1)
    {
        Switch(Device::Pad, "input_mode");
        return;
    }
    if (o.inputMode == 2)
    {
        Switch(Device::KbMouse, "input_mode");
        return;
    }

    if (g_active == Device::KbMouse)
    {
        if (padButton)
            Switch(Device::Pad, "button");
        else if (padAxis && g_grace <= 0)
            Switch(Device::Pad, "stick");
    }
    else
    {
        if (key)
            Switch(Device::KbMouse, "key");
        else if (mouseButton)
            Switch(Device::KbMouse, "mouse button");
        else if (mouseMotion && g_grace <= 0)
            Switch(Device::KbMouse, "mouse");
    }
}

Device Active() { return g_active; }
bool Switched() { return g_switched; }
u32 LastPadTick() { return g_lastPadTick; }
u32 LastKbmTick() { return g_lastKbmTick; }

bool TakeDisconnectPause()
{
    const bool v = g_disconnectPause;
    g_disconnectPause = false;
    return v;
}

u32 ConfirmMask() { return uiopt::Get().confirmButton == 1 ? XINPUT_GAMEPAD_B : XINPUT_GAMEPAD_A; }
u32 CancelMask() { return uiopt::Get().confirmButton == 1 ? XINPUT_GAMEPAD_A : XINPUT_GAMEPAD_B; }

std::string Status()
{
    std::ostringstream o;
    o << "device=" << (g_active == Device::Pad ? "pad" : "kbm") << " why=" << g_lastWhy << " switches=" << g_switches
      << " lock=" << uiopt::Get().inputMode << " keyTick=" << g_lastKeyTick << " mouseTick=" << g_lastMouseTick
      << " hostKeys=" << (HostAtLeast(18) && g_host->LastRealKeyTick ? 1 : 0)
      << " hostMouse=" << (HostAtLeast(18) && g_host->RealMouseDown ? 1 : 0) << " grace=" << g_grace;
    return o.str();
}
} // namespace device
