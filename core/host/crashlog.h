// FLvibed - the crash recorder: core\host\crashlog.cpp. An unhandled exception (one no handler of the game's or the mod's
// took) writes one block to EXE\FLvibed_crash.log before the game's own crash filter ends the process: the code, the
// faulting address and the stack's return addresses as module + offset, the thread, the build. Freelancer's filter leaves
// no Windows event and no dump (docs\FREELANCER_RE.md "The crash filter"), so without it a crash is only an exit code.
#pragma once

namespace crashlog
{
void Install();    // the host's start, after its logs open: in front of the filter in place, which it then calls
void FirstFrame(); // the first frame: in front again if the game set its filter after the start; any other replacement is logged
} // namespace crashlog
