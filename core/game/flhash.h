// Freelancer's nickname hash (Common.dll's exported CreateID: the space id of a solar, an archetype id, a voice-bank
// entry's name) in-tree, bit-identical to the export (verified by tests\flhash_test.cpp against tools\flhash.ps1), so
// the code that only reads game data (mission scripts, effect tables) runs without the game (build.cmd test).
// The algorithm: a reflected CRC-style step with the polynomial 0xA001 << 14 over the ASCII lower-cased bytes, the result
// byte-swapped, shifted right by 2 and its top bit set (tools\flhash.ps1, subtitles\review\FLSubtitles\asr\flhash.py).
#pragma once
#include "common.h"

#include <string>

namespace flhash
{
u32 CreateId(const char* nickname);
inline u32 CreateId(const std::string& nickname) { return CreateId(nickname.c_str()); }
} // namespace flhash
