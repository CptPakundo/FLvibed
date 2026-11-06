// What the mod has shown, for tests: every subtitle and chat line with a sequence number, so a harness
// can wait for "the next line shown" instead of sleeping or tailing the log (debug command `shown [after]`).
#pragma once
#include "common.h"

#include <string>

namespace feed
{
// channel: "sub" (on-screen subtitle) or "chat"; what: category or kind.
void Push(const char* channel, const char* what, const std::wstring& speaker, const std::wstring& text);
// Lines after `after`, oldest first: first line "last <seq>", then "<seq> <ms> <channel> <what> | <speaker> | <text>".
std::string Since(u32 after);
} // namespace feed
