// Freelancer's chat window as a second subtitle channel (ambient NPC chatter). The text goes in as
// Freelancer rich text (RDL nodes) through Freelancer.exe 0x46A150, which also shows the window in single
// player (technique from TheStarport's ChaosMod, "The Multiplayer Experience").
#pragma once
#include "common.h"

#include <string>

namespace chat
{
// "<name>: <text>" with the name in `rgb` (0xRRGGBB); name may be empty.
void Print(const std::wstring& name, u32 rgb, const std::wstring& text);
} // namespace chat
