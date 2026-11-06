// FLvibed - the options page without the controller (docs\MODULES_PLAN.md P4). With the controller on, its frame drives the page as it
// always did. With it off, this small client takes the controller's place in the router's table, so the page keeps the same places
// in the frame and the draw passes, and drives the page through core's logic (core\logic\core_api.h).
#pragma once

namespace pagehost
{
void Start(); // at the host's start, when the controller is off
} // namespace pagehost
