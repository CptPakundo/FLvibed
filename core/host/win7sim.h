// FLvibed - the Windows 7 simulation's run-time half: core\host\win7sim.cpp.
#pragma once
#include "common.h"

namespace win7sim
{
void Install(HMODULE self); // with FLVIBED_SIM_WIN7 only; the first thing the host does after its logs open
} // namespace win7sim
