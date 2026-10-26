// Targeting and aiming helpers shared by the pad's aim assist and the test bot.
// Freelancer's guns fire at the mouse cursor (within their gimbal limits), and the HUD draws a
// "prediction cross" at the lead point of the current target: aiming = cursor on that cross.
#pragma once
#include "common.h"

namespace aim
{
// Attitude at or below this counts as hostile (Freelancer's hostile threshold is -0.6).
constexpr float kHostileAttitude = -0.55f;

struct Target
{
    bool valid = false;    // the player has a target
    bool hostile = false;  // the target's attitude towards the player is hostile
    bool inRange = false;  // Freelancer computed a lead point (target within gun range)
    bool onScreen = false; // the aim point projects inside the screen
    float attitude = 0;
    float dist = 0;                 // metres
    float closing = 0;              // m/s, > 0 when the distance is shrinking
    float radius = 20;              // target physical radius (m)
    float world[3] = {};            // lead point when in range, else the target's position
    float screenX = 0, screenY = 0; // projection of `world` (pixels), valid if onScreen
};

// The player's current target, evaluated now.
Target Current();

// Steering (Freelancer mouse-flight units, see SetSteer) that turns the nose towards a world
// point at full rate: a unit direction scaled by `scale`. False when not in space.
bool TurnTowards(const float world[3], float scale, float& sx, float& sy);
// The same, proportional: full rate only beyond `fullAngle` (radians), less as the nose closes in - a calm, steady
// approach without overshoot (navigation: fast dock). The full-rate version suits a fight (between targets).
bool SteerTowards(const float world[3], float scale, float fullAngle, float& sx, float& sy);

// Keep the aim cursor off every UI control (HUD bars, panels...): hovering one switches flight to
// UI mode and held fire would click it. Uses Freelancer's hit test, so any HUD layout works.
void ClampToSafeArea(float& x, float& y);

// While the host makes the HUD transparent to Freelancer's hit test (pad flight), the cursor may go
// anywhere on screen and ClampToSafeArea only keeps it inside the window.
void SetHudTransparent(bool on);

// Angle (degrees) between the camera's view axis and the ray through screen pixel (x, y).
float AngleFromCenter(float x, float y);
} // namespace aim
