// FLvibed - what flies the ship goes through here: the held commands with their owners, the steering override, and the options the
// flight's own math reads. The pad's flight, the test bot and the autopilot all use it, so none of them needs the others. Who may do what:
// input_rules.h and docs\MODULES_PLAN.md "P7's priority table".
#pragma once
#include "common.h"
#include "game/input_rules.h"

#include <string>

namespace flightio
{
// Send the events a key bound to `cmd` would produce (its keymap.ini "state" mask), and keep it held.
void Hold(int cmd, bool want, inputrules::Owner owner = inputrules::OwnerPad);
// A held action both the autopilot and the pad reach, while the autopilot flies (input_rules.h WhileFlying).
void Claim(inputrules::Channel c, int cmd, bool autopilot, bool pad);
bool Holding(int cmd);
// The turret's camera starts and ends without a turn input: its look commands let go without a key event.
void DropHold(int cmd);
void Tap(int cmd);
bool InTap(); // a tap is being fired: the game's key report of it is ours
// Lets go of every command held, the steering and the virtual mouse.
void ReleaseAll();
void SetFrameDt(float dt); // once a frame: a held command's key repeat, and the frame count
u32 Frame();
float& LastSteerX(); // the steering sent last, for the HUD and the debug line
float& LastSteerY();

void SetSteer(float x, float y);
void ReleaseSteer();
void TickSteerRelease();
bool SteerActive();
float SteerX();
float SteerY();

// The autopilot flies under a window or the options page (input_rules.h FliesUnderWindow): the pad, the keys and the cursor are the
// window's, so the flight reads no pad and leaves the cursor alone; the guns take a world point instead. Set once a frame by the frame.
void SetUnderWindow(bool on);
bool UnderWindow();

std::string HeldNames();  // " USER_FIRE_WEAPONS ...", for `state`
std::string HeldOwners(); // " USER_FIRE_WEAPONS=pad ...", for `owners`

// [Controller] rows the flight's math reads, kept current by the controller each frame; with it off, read once by whoever flies.
struct Options
{
    float steerScale = 0.77f;
    float aimCone = 90.0f;
    bool invertY = false;
    float autopilotCancel = 0.35f;
    bool maneuverWatchdog = true;
};
Options& Get();
} // namespace flightio
