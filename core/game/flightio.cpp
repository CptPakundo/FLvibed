// What flies the ship goes through here (flightio.h).
#include "game/flightio.h"

#include "dimouse.h"
#include "game/fl.h"
#include "game/hostcalls.h"
#include "log.h"

#include <map>

namespace flightio
{
// Commands currently held and by whom (a mode change releases them), with the time until the next emulated key repeat for commands
// declared "repeat".
struct HoldState
{
    bool down = false;
    float repeat = 0;
    inputrules::Owner owner = inputrules::OwnerNone;
};
static std::map<int, HoldState> g_holding;
static float g_frameDt = 0;

// A hold ends with its release: the map's mask says so, or the map does not list the command and so says nothing. Without it a slide up or down held on vanilla's or a
// mod's key map never stopped (the game re-reads the held commands only on a key-up, 0x4D812D). tests\strafe_release.ps1.
static bool EndsWithKeyUp(int cmd)
{
    if (fl::CommandFlags(cmd) & fl::CMDF_KEYUP)
        return true;
    if (fl::CommandMapped(cmd))
        return false;
    static bool logged[fl::CMD_COUNT];
    if (cmd > 0 && cmd < fl::CMD_COUNT && !logged[cmd])
    {
        logged[cmd] = true;
        Log("key map: %s is not listed: its release is sent anyway", fl::CmdName(cmd));
    }
    return true;
}

// Send the events a key bound to `cmd` would produce (per its keymap.ini "state" mask).
void Hold(int cmd, bool want, inputrules::Owner owner)
{
    HoldState& h = g_holding[cmd];
    h.owner = want ? owner : inputrules::OwnerNone;
    const u32 flags = fl::CommandFlags(cmd);
    if (want && h.down && (flags & fl::CMDF_REPEAT))
    {
        h.repeat -= g_frameDt;
        if (h.repeat <= 0)
        {
            fl::FireCommand(cmd, true);
            h.repeat = 1.0f / 15.0f;
        }
    }
    if (want == h.down)
        return;
    h.down = want;
    fl::SetHeld(cmd, want);
    if (want && (flags & fl::CMDF_KEYDOWN))
    {
        fl::FireCommand(cmd, true);
        h.repeat = 1.0f / 15.0f;
    }
    if (!want && EndsWithKeyUp(cmd))
        fl::FireCommand(cmd, false);
}

static bool g_inTap = false; // a pad tap is being fired (OnKeyCmd skips it: Apply answered it)
void Tap(int cmd)
{
    const u32 flags = fl::CommandFlags(cmd);
    g_inTap = true;
    if (flags & fl::CMDF_KEYDOWN)
        fl::FireCommand(cmd, true);
    if (flags & fl::CMDF_KEYUP)
        fl::FireCommand(cmd, false);
    g_inTap = false;
}

static void Steer(bool active, float x, float y)
{
    if (hostcalls::g.SetSteer)
        hostcalls::g.SetSteer(active, x, y);
}

static int g_steerStopFrames = 0;
static bool g_steerActive = false;
static float g_steerX = 0, g_steerY = 0; // the last override sent (the maneuver watchdog's snapshot)

void SetSteer(float x, float y)
{
    Steer(true, x, y);
    g_steerActive = true;
    g_steerStopFrames = 0;
    g_steerX = x, g_steerY = y;
}

void ReleaseSteer()
{
    if (g_steerActive)
    {
        Steer(true, 0, 0);
        g_steerActive = false;
        g_steerStopFrames = 3;
        g_steerX = g_steerY = 0;
    }
}

void TickSteerRelease()
{
    if (!g_steerActive && g_steerStopFrames > 0 && --g_steerStopFrames == 0)
        Steer(false, 0, 0);
}

void ReleaseAll()
{
    for (auto& h : g_holding)
    {
        if (h.second.down)
        {
            fl::SetHeld(h.first, false);
            if (EndsWithKeyUp(h.first))
                fl::FireCommand(h.first, false);
            h.second.down = false;
            h.second.owner = inputrules::OwnerNone;
        }
    }
    ReleaseSteer();
    dimouse::Reset();
}

static bool g_underWindow = false;

void SetUnderWindow(bool on)
{
    if (on && !g_underWindow)
    {
        dimouse::ReleaseTarget(); // the cursor is the window's from this frame on, and the turret look's stick with it
        for (int cmd : {fl::USER_LOOK_ROTATE_CAMERA_LEFT, fl::USER_LOOK_ROTATE_CAMERA_RIGHT, fl::USER_LOOK_ROTATE_CAMERA_UP, fl::USER_LOOK_ROTATE_CAMERA_DOWN})
            Hold(cmd, false);
    }
    g_underWindow = on;
}

bool UnderWindow() { return g_underWindow; }

bool Holding(int cmd) { return g_holding[cmd].down; }

void DropHold(int cmd)
{
    HoldState& h = g_holding[cmd];
    h.down = false, h.owner = inputrules::OwnerNone;
}

void Claim(inputrules::Channel c, int cmd, bool autopilot, bool pad)
{
    Hold(cmd, inputrules::Want(c, autopilot, pad), inputrules::ClaimedBy(c, autopilot, pad));
}

bool InTap() { return g_inTap; }
static u32 g_frame = 0;
void SetFrameDt(float dt)
{
    g_frameDt = dt;
    ++g_frame;
}
u32 Frame() { return g_frame; }
float& LastSteerX()
{
    static float x = 0;
    return x;
}
float& LastSteerY()
{
    static float y = 0;
    return y;
}
bool SteerActive() { return g_steerActive; }
float SteerX() { return g_steerX; }
float SteerY() { return g_steerY; }

std::string HeldNames()
{
    std::string s;
    for (auto& h : g_holding)
        if (h.second.down)
            s += std::string(" ") + fl::CmdName(h.first);
    return s;
}

std::string HeldOwners()
{
    static const char* const kNames[] = {"none", "pad", "autopilot", "bot"};
    std::string s;
    for (auto& h : g_holding)
        if (h.second.down)
            s += std::string(" ") + fl::CmdName(h.first) + "=" + kNames[h.second.owner];
    return s;
}

Options& Get()
{
    static Options o;
    return o;
}
} // namespace flightio
