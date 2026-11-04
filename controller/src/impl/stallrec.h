// The NPC stall recorder (docs\research\npc-ai.md; the rule: stallwatch.h). Read only: while the player is in space it
// samples the hostile ships near them twice a second of game time, and when one stops it writes what the game says about
// that ship to EXE\FLvibed_npc_stalls.log: its running behaviour, job, target, neighbours, the loot near it, the mission's
// objective. The reports are rare and sporadic and the test harness has not reproduced them, so a player's session is
// what catches one. Setting `npc_stall_log`; at most kMaxRecords records a game session.
#pragma once
#include "common.h"

#include <istream>
#include <string>

namespace stallrec
{
struct Options
{
    bool on = true;
};
Options& Get();
// Once per frame; `mode` the controller's mode name, `paused` the game's pause (game time stands still).
void Update(float dt, const char* mode, bool paused);
// Debug: stalls [state] | hold <id|0> (TEST ONLY: that ship's samples stand still, the recorder's proof on a live NPC)
std::string Command(std::istream& in);
} // namespace stallrec
