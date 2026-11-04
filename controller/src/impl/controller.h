#pragma once
#include "common.h"
#include "settings_bus.h"

#include <istream>

namespace space
{
struct Contact;
}

namespace ctl
{
// A hostile ship near enough for the targeting press (RS: target_smart takes these first, target_cycle.h): the HUDs
// label the press by it.
bool NearbyEnemy(const space::Contact& c);
void Init();
void Shutdown();
// The settings bus (core\settings_bus.h): the [Controller] / [Autopilot] registries registered before Init loads the ini
// (impl.cpp), unregistered at Shutdown (the PAGE track; the definitions sit by kOptions in controller.cpp).
void RegisterBus();
void UnregisterBus();
void OnFrame(double dt);
void OnRenderOverlay();
void OnRenderUnderHud();
void OnDeviceLost();
void OnDeviceReset();
std::string DescribeState();
int FpsCap(int fps); // test games: frame-rate cap (fps 0 = off, < 0 = read only); returns the cap now
float LastSteerX(); // steering sent to the host this frame (diagnostics)
float LastSteerY();
// The player used the autopilot this session (a pad start, the goal panel's DoNow): the AUTOPILOT legend row and the
// autopilot prompts may show from now on (UI_PLAN 6.3; D-FLIGHT's request, 25 Oct 2026).
void MarkAutopilotUsed();
// The player's stick deadzone (`stick_deadzone`): every pad reader applies it, not only the flight pad's (the options
// page has its own reader, uikit.cpp).
float StickDeadzone();
// `pad_backend` (0 XInput, 1 SDL3): every pad reader reads through the same backend (the options page's reader too).
int PadBackend();
// `focus_only`: the physical pad is ignored while the game is in the background - flight and menus alike, one gate
// (focus_gate.h; the owner, 29 Oct 2026); 0 lets it act in the background in both.
bool FocusOnly();
// ImplApi v4: a key command the game's key handler just handled (the keyboard's maneuvers: the lock's answer, hud_prompts'
// actions; 0.2.1). The pad's own taps (Tap) are skipped: Apply answers them.
void OnKeyCmd(int cmd, bool down);
// "set <name> <value>" from the debug channel; returns a message.
std::string SetOption(std::istream& in);
std::string BotCommand(std::istream& in);       // debug: bot fight|off|status
std::string ConfigCommand(std::istream& in);    // debug: config [save|reload|settings]
std::string AutopilotCommand(std::istream& in); // debug: ap ... (Advanced Autopilot, autopilot.cpp)
} // namespace ctl
