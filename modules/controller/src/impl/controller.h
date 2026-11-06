// ctl:: API of the pad's logic: Init, Shutdown, settings bus, publishing its views, debug verbs and steering diagnostics.
#pragma once
#include "common.h"
#include "game/space.h"
#include "settings_bus.h"

#include <istream>

namespace ctl
{
void Init();
void Shutdown();
// The settings bus (core\settings_bus.h): the [Controller] / [Autopilot] registries registered before Init loads the ini
// (impl.cpp), unregistered at Shutdown (the PAGE track; the definitions sit by kOptions in controller.cpp).
void RegisterBus();
void UnregisterBus();
// One [Controller] row by the bus, as the options page changes it: live, then saved. False when the bus refuses it.
bool SetAndSave(const char* key, const char* value);
std::string DescribeState();
std::string DescribeOwners();
void PublishPadFlight();
void WithdrawPadFlight();
void PublishFrame(); // the pad's places in core's frame (views::PadFrameView)
// A debug verb of the pad's: true with the reply (pad_verbs.cpp).
bool Verb(const std::string& op, std::istream& in, std::string& reply);
void WithdrawFrame();
float LastSteerX(); // steering sent to the host this frame (diagnostics)
float LastSteerY();
// The player used the autopilot this session (a pad start, the goal panel's DoNow): the AUTOPILOT legend row and the
// autopilot prompts may show from now on (UI_PLAN 6.3; D-FLIGHT's request).
void MarkAutopilotUsed();
// The player's stick deadzone (`stick_deadzone`): every pad reader applies it, not only the flight pad's (the options
// page has its own reader, uikit.cpp).
float StickDeadzone();
// `pad_backend` (0 XInput, 1 SDL3): every pad reader reads through the same backend (the options page's reader too). 1 too while the hands are on the pad the flight's
// reader's SDL holds (padbackend::Hands): it alone decides, so there is one SDL and one answer.
int PadBackend();
void PadForceCandidate(int answer); // debug `padfallback`
void PadTestRumble();
// `focus_only`: the physical pad is ignored while the game is in the background - flight and menus alike, one gate
// (focus_gate.h); 0 lets it act in the background in both.
bool FocusOnly();
// "set <name> <value>" from the debug channel; returns a message.
std::string SetOption(std::istream& in);
std::string BotCommand(std::istream& in);       // debug: bot fight|off|status
std::string ConfigCommand(std::istream& in);    // debug: config [save|reload|settings]
std::string AutopilotCommand(std::istream& in); // debug: ap ... (Advanced Autopilot, autopilot.cpp)
} // namespace ctl
