// Targeting and aiming helpers shared by the pad's aim assist and the test bot.
// Freelancer's guns fire at the mouse cursor (within their gimbal limits), and the HUD draws a
// "prediction cross" at the lead point of the current target: aiming = cursor on that cross.
#pragma once
#include "common.h"
#include "game/attitude.h"
#include "game/space.h"

namespace aim
{
// Attitude at or below this counts as hostile (Freelancer's hostile threshold is -0.6).
constexpr float kHostileAttitude = attitude::kHostile;

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

// The player's current target, evaluated now. flightFrame: screenX / Y / onScreen on the screen the ship is flown by
// (fl::WorldToFlightScreen - the autopilot's view, the same in turret view), else on the screen the player sees (the HUD).
Target Current(bool flightFrame = false);

// Steering (Freelancer mouse-flight units, see SetSteer) that turns the nose towards a world
// point at full rate: a unit direction scaled by `scale`. False when not in space.
bool TurnTowards(const float world[3], float scale, float& sx, float& sy);
// The same, proportional: full rate only beyond `fullAngle` (radians), less as the nose closes in - a calm, steady
// approach without overshoot (navigation: fast dock). The full-rate version suits a fight (between targets).
bool SteerTowards(const float world[3], float scale, float fullAngle, float& sx, float& sy);
// A steering computed in the ship's own frame (sx along its right, sy along its up) restated for the game's mouse
// flight, which turns the nose in a LEVEL frame - its right = nose x the system's up - not the ship's banked one
// (measured 28 Oct 2026: banked by b, the ship turned b off the steering). The same steering on a level ship; near
// the vertical (nose within ~11 deg of it) left as it is.
// `frame` = the axes the steering was computed along (rot columns right, up, back; nullptr = the ship's): Pursue's screen
// offsets are the chase camera's (fl::FlightScreenAxes), whose roll trails the ship's.
// `into` = the turn frame's axes (nullptr = the level frame; the camera's: `steer_level` 2).
void LevelSteer(float& sx, float& sy, const float* frame = nullptr, const float* into = nullptr);

// The reticle in gimbal aim: the screen centre, or the hostile target's lead cross within `cone` degrees of it, off every UI control.
void Gimbal(float cone, float& x, float& y);

// A stick's vertical value as what it points with: `invert_y` and the game's own option cancel as they do in flight.
float LookY(float y);

// Keep the aim cursor off every UI control (HUD bars, panels...): hovering one switches flight to
// UI mode and held fire would click it. Uses Freelancer's hit test, so any HUD layout works.
void ClampToSafeArea(float& x, float& y);

// While the host makes the HUD transparent to Freelancer's hit test (pad flight), the cursor may go
// anywhere on screen and ClampToSafeArea only keeps it inside the window.
void SetHudTransparent(bool on);
// A hostile ship near enough for the targeting press (RS takes these first, target_cycle.h): HUD 2 and the pad label the press by it.
bool NearbyEnemy(const space::Contact& c);

// Angle (degrees) between the camera's view axis and the ray through screen pixel (x, y).
float AngleFromCenter(float x, float y);
} // namespace aim
