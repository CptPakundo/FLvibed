// Reticle cue: a short ring pulse around the screen centre, where the eyes are in flight (user, 19 Oct 2026: context-
// sensitive reticle tinting; HUD principles: one meaning per cue, shape + colour, < 3 Hz, an off switch).
//   autopilot engaged: a cyan ring expanding outward;  handed back: an amber ring contracting inward;
//   nanobots used: a green double ring;  shield batteries used: a blue double ring.
// Design: controller\docs\HUD_CUES.md.
#pragma once
#include "common.h"

#include <string>

namespace reticlecue
{
struct Settings
{
    bool on = true;
};
Settings& Get();

enum class Kind
{
    Engage,
    HandBack,
    Nanobots,
    Batteries
};
// Every frame: watches the autopilot's on / off and the consumable counts (any use, the player's too).
void Update(float dt, bool flying, bool autopilotActive);
void Render(void* device);
void Pulse(Kind k); // debug / tests
std::string Status();
void Shutdown(); // frees the ring texture (ctl::Shutdown, before the logic unloads)
} // namespace reticlecue
