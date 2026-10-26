#pragma once
#include "common.h"

// Host: opens the module's log (fileName, e.g. "FLController.log") next to the DLL.
void LogInit(HMODULE self, const char* fileName);
// Logic DLL: forward log lines to the host instead of opening a file.
void LogSetSink(void (*sink)(const char* line), const char* dllDir);
void LogWriteLine(const char* line);
void Log(const char* fmt, ...);
// Directory containing FLvibed.dll (with trailing backslash): the game's EXE folder.
const std::string& DllDir();

// CPU time per entry point (debug `perf cpu` in the controller; the pattern of the subtitles' PerfScope, ported 24 Oct
// 2026 for "measure the bottleneck first"): perf::Scope s("name") adds the scope's QueryPerformanceCounter time to
// that name (a string literal: names are kept by pointer, at most 32). NewFrame() once per frame counts the frames and
// closes the frame's sum of outermost scopes (nested scopes count for their own name only) for the worst frame.
// Report() prints the totals per frame since the last report and resets them. Game thread only; per module part.
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
