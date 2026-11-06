// FLvibed - the maneuver watchdog, the live part of maneuver_watch.h: a formation, goto or dock of the player's that stalled far from its goal is left and
// issued again (setting `maneuver_watchdog`, read through flightio::Get()). Core's, so it protects every player whichever modules are on.
#pragma once
#include <istream>
#include <string>

namespace mwatch
{
// Each frame in flight, whoever flies. Stands down while the Advanced Autopilot flies and while the player steers.
void Frame();
// Test aid (debug verb `watchdog`): `watchdog test` starts a game goto to the nearest station and makes it read as stopped, so the live part runs in game;
// `watchdog off` ends that. tests\watchdog_modules.ps1.
std::string Command(std::istream& in);
} // namespace mwatch
