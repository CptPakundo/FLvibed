// `ash_ride`'s adapter (autopilot_ashride.cpp; the pure half ash_ride.h): the ride's state across the stage's step and
// the chase's, the frame guard that flies the loop and the intercept, the coast gate ChaseCoast asks. Fails closed to
// `spawn_charge` as is: no loop inside the leashes -> not armed; the escort alive 35 s after the leave or a leash about to
// fire -> back to the fight; his contact not seen 10 s after the escort's death -> the chase as spawn_charge's.
#pragma once
#include "lookahead.h"

#include <string>
#include <vector>

namespace autopilot
{
struct Out;
// The decision / the supervisor, while spawn_charge stages the fleeing spawn `run`-ward of `spawn` (trigger, the last
// ship, the leashes the stage kept to): true when the ride owns the stage - armed (its loop fits every leash) or fallen
// back to the fight - and the stage's verdict is then the fight on the last ship (no trip, no hold, no keep station).
bool AshRideArm(const std::string& trigger, u32 escort, const float spawn[3], const float run[3],
                const std::vector<lookahead::StageLeash>& leashes);
bool AshRideStage(const std::string& trigger); // the ride owns this stage (ChargeKeepStation stays off)
// The guard flies the ship (the loop, or the intercept before the coast): the combat neither stops, dodges, gives up nor
// loots, and a stop keeps the cruise.
bool AshRideFlying();
// The frame guard (autopilot_frame.cpp Update, after the modes, before the safety guards).
void AshRideFrame(Out& out);
// ChaseCoast's start for the chased ship `id`: true when the ride owns it (then `*start` = its gate: he >= 295 m/s, we at
// cruise, on our velocity line and parallel, the gap 90-220 m stable 1 s); false: ChaseStep's own start.
bool AshRideCoast(u32 id, float gap, float hisSpeed, const float hisVel[3], float mySpeed, const float myVel[3],
                  const float toHim[3], bool cruising, bool shieldOut, bool* start);
} // namespace autopilot
