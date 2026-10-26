// Debug/automation commands received through the shared-memory channel.
#pragma once
#include "common.h"

namespace debug
{
// Execute one debug command line and return the reply text.
std::string Execute(const std::string& line);
// Write a textual dump of all UI controls to a file; returns the path.
std::string DumpUi();
bool SafeRead(uintptr_t addr, void* out, size_t len);
// Clears a `watch` (debug registers and the exception handler): before the logic unloads (hot reload).
void StopWatch();
// Per-frame work of debug commands that span frames (`shot`).
void Frame();
} // namespace debug
