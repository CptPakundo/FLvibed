// The spawn simulator's offline loader: fills spawnsim::World from the files the game loads (EXE\freelancer.ini [Data]
// universe / solar, the system file, its [EncounterParameters] files, DATA\MISSIONS\shipclasses.ini, npcships.ini,
// faction_prop.ini), read through ini::Read (text or BINI) with the game's keys and defaults (docs\kb\spawning.md
// sections 1-2, 6, 8). Host-free; the in-game loader will fill the same World from the game's memory.
#pragma once

#include "game/spawnsim.h"

#include <map>
#include <string>

namespace spawnsim
{
// `dataDir`: the DATA folder with a trailing backslash; `system`: the system's nickname (universe.ini); `standing`: the
// player's reputation per group (lower case; missing = 0). False with `error` set when the system or a needed file is missing.
bool LoadSystem(const std::string& dataDir, const std::string& system, const std::map<std::string, double>& standing,
                World& out, std::string& error);
// The nickname of the system whose id (CreateID of its nickname) is `id`, from the universe file EXE\freelancer.ini names; "" when none.
std::string SystemNickname(const std::string& dataDir, uint32_t id);
// A save file's `house = value, group` lines (lower case group -> value).
std::map<std::string, double> ReadStandings(const std::string& savePath);
} // namespace spawnsim
