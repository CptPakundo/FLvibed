// Lead pursuit, shared by the test bot and the autopilot's actuator.
#pragma once
#include "common.h"
#include "game/aim.h"

namespace leadchase
{
struct Pursuit
{
    bool reversing = false;  // reverse thrust engaged
    float reverseHold = 0;   // time before the forward/reverse state may switch again
    float lastOffset = 2.0f; // previous frame's lead offset (to see the target heading off screen)
    float phase = 0;         // corkscrew angle (weave)
};
struct PursuitOut
{
    float sx = 0, sy = 0; // steering (SetSteer)
    float ax = 0, ay = 0; // aim cursor (pixels)
    bool aligned = false; // the cursor sits on the lead cross near the nose: the guns hit
    float offset = 2.0f;  // lead cross distance from the screen centre (half-screen units); 2 = off screen
    bool reverse = false, burn = false;
    bool screenFrame = false; // sx / sy from the flight screen's offsets (its axes: fl::FlightScreenAxes), else the ship's
};

PursuitOut Pursue(const aim::Target& t, float dt, Pursuit& p, float closeBand = 0, float fireCone = 0.3f, float gunRange = 0,
                  float weave = 0, float weaveRadius = 0.6f);
} // namespace leadchase
