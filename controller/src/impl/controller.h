#pragma once
#include "common.h"
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
void OnFrame(double dt);
void OnRenderOverlay();
void OnRenderUnderHud();
void OnDeviceLost();
void OnDeviceReset();
std::string DescribeState();
float LastSteerX(); // steering sent to the host this frame (diagnostics)
float LastSteerY();
// The player used the autopilot this session (a pad start, the goal panel's DoNow): the AUTOPILOT legend row and the
// autopilot prompts may show from now on (UI_PLAN 6.3; D-FLIGHT's request, 25 Oct 2026).
void MarkAutopilotUsed();
// "set <name> <value>" from the debug channel; returns a message.
std::string SetOption(std::istream& in);
std::string BotCommand(std::istream& in);       // debug: bot fight|off|status
std::string ConfigCommand(std::istream& in);    // debug: config [save|reload|settings]
std::string AutopilotCommand(std::istream& in); // debug: ap ... (Advanced Autopilot, autopilot.cpp)
} // namespace ctl
