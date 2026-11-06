// FLvibed - the chat window's lines as they arrive (exe::ChatFlush): the server's tag line is how a server says what it allows FLvibed
// (docs\MULTIPLAYER.md "Server permission"; core\game\mprules.* reads this). A tag-only server line is read here and never shown.
#pragma once
#include "common.h"

namespace chatlines
{
void Install();
void ModeCheck();   // once a frame: a change between single player and multiplayer starts a new session
bool Multiplayer(); // the game's own word (common.dll SinglePlayer)
// Line n, 1-based, of those seen; returns how many were seen. n = 0, or a line that left the ring of 32: only the count.
u32 LineAt(u32 n, u32* sender, wchar_t* text, int max);
// The session's last tag line, kept apart from the ring so a logic reload finds it; returns its serial, 0 = none this session.
u32 ServerTag(u32* sender, wchar_t* text, int max);
} // namespace chatlines
