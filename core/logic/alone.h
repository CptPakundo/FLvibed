// What a module needs from the controller's entry when the controller is off (core_page.cpp): the Autopilot's, the HUD's and Speedrun's frame is core's
// (core\game\coreframe.h) either way; this holds what the controller's entry otherwise gives them, for keyboard and mouse: the settings rows, their file, the session
// and the verbs.
#pragma once
#include "common.h"

#include <istream>
#include <string>

namespace alone
{
void Init(const char* dllDir); // after modslots::Start
void Shutdown();
bool On(); // a module here needs the frame
bool Command(const std::string& op, std::istream& in, std::string& reply);
} // namespace alone
