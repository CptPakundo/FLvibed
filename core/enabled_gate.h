// FLvibed - what [Controller] `enabled` means per module, pure (tests\enabled_gate_test.cpp). `enabled = 0` is one switch: past
// its place in the frame nothing of these modules runs, and no autopilot flies. What comes before it still runs: the game clock,
// the pad's read and its vibration going quiet, the stall recorder, the options page (so the switch can be turned back on).
// Each module reads its own flag from its cut on; while they are one frame they all follow the switch.
#pragma once

namespace enabled_gate
{
struct Modules
{
    bool controller; // the pad: device, screens, flight, menus
    bool hud;        // HUD 2, the skin
    bool autopilot;  // the autopilot, its goals and bar button
    bool speedrun;   // the clock, the splits
};

constexpr Modules Read(bool enabled) { return {enabled, enabled, enabled, enabled}; }
// Anything to run past the gate.
constexpr bool AnyRuns(const Modules& m) { return m.controller || m.hud || m.autopilot || m.speedrun; }
} // namespace enabled_gate
