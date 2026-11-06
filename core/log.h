// Logging API: one channel per module file or sink, the plain Log(), and perf:: CPU timing per entry point.
#pragma once
#include "common.h"

// Host: opens a module's log (fileName, e.g. "FLController.log") next to the DLL. Returns its channel; the plain Log() goes
// to the file opened last (the host's own lines: the controller's log, the subtitles' when the controller is off).
int LogOpen(HMODULE self, const char* fileName);
// Tests and tools: the plain Log() goes to `sink`, no file.
void LogSetSink(void (*sink)(const char* line), const char* dllDir);
// The logic's channels: one per module, so each module's lines stay in its own file (core\logic\module_log.h). Attaching
// makes `sink` the place of the plain Log() too, the lines of core\game itself: the channel attached last. Same sink, same
// channel (a reload attaches again). Returns the channel.
int LogAttach(void (*sink)(const char* line), const char* dllDir);
int LogChannelOf(void (*sink)(const char* line)); // the channel of an attached sink, -1 without one
void LogTo(int channel, const char* line); // a finished line, into one channel
void LogV(int channel, const char* fmt, va_list args); // channel < 0: the plain Log()'s
void LogWriteLine(const char* line);
void Log(const char* fmt, ...);
// Directory containing FLvibed.dll (with trailing backslash): the game's EXE folder.
const std::string& DllDir();

// CPU time per entry point (debug `perf cpu` in the controller; the pattern of the subtitles' PerfScope, ported 24 Oct
// 2026 for "measure the bottleneck first"): perf::Scope s("name") adds the scope's QueryPerformanceCounter time to
// that name (a string literal: names are kept by pointer, at most 32). NewFrame() once per frame counts the frames and
// closes the frame's sum of outermost scopes (nested scopes count for their own name only) for the worst frame.
// Report() prints the totals per frame since the last report and resets them. Game thread only; one table for the logic
// (only the controller reports it; the subtitles keep their own PerfScope).
namespace perf
{
double Now(); // seconds (QueryPerformanceCounter)
class Scope
{
  public:
    explicit Scope(const char* name);
    ~Scope();
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;

  private:
    int slot_;
    double t0_;
};
void NewFrame(); // outside every scope (it also closes scopes a fault left open)
std::string Report();
} // namespace perf
