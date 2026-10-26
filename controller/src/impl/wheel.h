// The action wheel (docs\UI_SPEC.json `hud` state `wheel`; UI_PLAN.md 6.4 / 6.5): LB held past the hold threshold
// opens a wheel of the rare, contextual actions - SCAN CARGO, WEAPON GROUP, CRUISE DISRUPTOR, FORMATION, COMMS, CONTACT
// FILTER, AUTOPILOT GOALS, REAR VIEW (and ENGINE KILL with weapon_groups = two groups on the triggers, which unbinds L3) -
// picked with the right stick past a dead zone (angle -> slot; the D-pad steps too), done when LB is released. The
// left stick keeps flying; the right stick belongs to the wheel while it is open (controller.cpp suppresses its
// steering and the autopilot's right-stick hand-back). Native actions only: the game's commands (USER_SCAN_CARGO, the
// weapon group cycle, USER_LAUNCH_CRUISE_DISRUPTORS, USER_MANEUVER_FORMATION, USER_MANEUVER_ENGINEKILL), the HUD's own
// filter buttons through uiclick, the goal panel; REAR VIEW latches the rear camera (USER_REARVIEW_CAMERA_MODE held by
// controller.cpp while RearView() is true). COMMS = USER_RADIO, the game's "RADIO - Hails the targeted ship." (keymap P).
#pragma once
#include "common.h"
#include "glyphs.h"
#include "presets.h"

#include <istream>
#include <string>

namespace wheel
{
void Open();
// LB released: does the highlighted action and closes. Returns the action the CALLER must apply through its own
// dispatch (presets::Action::WeaponGroup - the active group lives in controller.cpp - Scan, Formation, EngineKill), or
// Action::None when the wheel did it itself (or nothing was highlighted).
presets::Action Commit();
void Cancel(); // closes without an action (the mode changed, the device switched)
bool IsOpen();
// Once per frame while open: the pad's buttons and the right stick (dead-zoned by the Gamepad).
void Update(float dt, const bool down[glyphs::COUNT], float rx, float ry);
void Render(void* device);
// The buttons the open wheel owns (hidden from the flight dispatch): the D-pad. LB stays with the dispatch (its release
// is the commit); the right stick is suppressed by the caller.
bool OwnsButton(glyphs::Id b);
// The rear-view latch toggled by the wheel's REAR VIEW slot (controller.cpp holds USER_REARVIEW_CAMERA_MODE with it).
bool RearView();
// Debug: wheel open|pick <n>|commit|cancel|state
std::string Command(std::istream& in);
} // namespace wheel
