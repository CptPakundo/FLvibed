// The model probe (audit R3b, 12 Nov 2026; ..\audit_2026-11-12\25_research_3d_models.md): a test verb that draws an
// item's 3D model turning in a square of its own, from HUD 2's pass under the UI, so no game window takes part. Since audit
// 70b F7 it draws through the model slot (modelslot.h), the component pages use, so its samples are the slot's tests.
// Reached only through the debug channel, which exists only with the test switch (core\common.h TestSwitch).
#pragma once

namespace modelprobe
{
// modelprobe [state] | on [file|-] [x y] [size] [count] | off | set flags|fov|fill|ambient|pitch|spin <v>
std::string Command(std::istream& in);
// Each frame from the pass under the UI, before modelslot::Pass: asks the slot for the squares the verb set up.
void Ask();
} // namespace modelprobe
