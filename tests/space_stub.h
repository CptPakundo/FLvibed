// The test exe's stand-in for core\game\space.cpp (the game's object registry, physics and server calls), so that
// avoid.cpp, route.cpp and flight.cpp link without the game: the solids are spheres the test sets (the ray tests and
// Obstacles() see them), the lane rings a chain the test sets (ObjectPos / LaneLinks); everything else answers "nothing"
// or "unknown" the way the real functions do outside space.
#pragma once
#include "game/space.h"

namespace stub
{
// The spheres rays hit (RayHitDistance / SightBlocker) and Obstacles() returns.
void SetSolids(const std::vector<space::Obstacle>& solids);
struct Ring
{
    u32 id = 0;
    float pos[3] = {};
    u32 next = 0, prev = 0;
};
// Lane rings: ObjectPos and LaneLinks answer for them.
void SetRings(const std::vector<Ring>& rings);
// Ray tests made since the last reset.
int RayCount();
void ResetRayCount();
} // namespace stub
