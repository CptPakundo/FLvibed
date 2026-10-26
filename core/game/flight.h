// The player ship's flight performance from the game's own data and state (Common.dll CShip / PhySys / CEEngine), and
// the game's motion law in closed form, for deterministic planning (fast dock, approaches, goto): what speed, which
// thrust (engine, afterburner, cruise, reverse), when to brake - computed before moving instead of guessed or learned.
// Freelancer's linear motion: m dv/dt = F - d v (force F and linear drag d per thrust type and throttle,
// CShip::collect_force_components; mass PhySys::GetMass): top speed F / d, v(t) = vTop + (v0 - vTop) e^(-t d / m).
// Verified 18 Oct 2026: top speeds = the measured 80 (engine) / 200 (afterburner) m/s of the Starflier.
// Shared by every module (core\game), compiled into each.
#pragma once
#include "common.h"

#include <string>

namespace flight
{
// One way of thrusting: its force and drag (the game's numbers), and the closed-form motion it gives.
struct Thrust
{
    float force = 0; // N (signed: reverse is negative)
    float drag = 0;  // linear drag coefficient (N per m/s)
    float mass = 1;  // kg
    bool ok() const { return drag > 0 && mass > 0; }
    float top() const { return force / drag; }       // the speed it settles at (m/s)
    float tau() const { return mass / drag; }        // time constant (s)
    float SpeedAfter(float v0, float t) const;       // speed after t seconds from v0
    float DistanceAfter(float v0, float t) const;    // distance covered in t seconds from v0
    float TimeToSpeed(float v0, float v1) const;     // seconds from v0 to v1 (-1 if never: beyond the top speed)
    float DistanceToSpeed(float v0, float v1) const; // distance covered from v0 until v1 (-1 if never)
};

// The player ship's model now: every thrust type the game has for it, and the cruise engine's state.
struct Model
{
    bool ok = false;
    float mass = 0;
    Thrust engine;                 // engine, full throttle (ThrustEquipType Normal)
    Thrust reverse;                // engine, full reverse (throttle -1)
    Thrust burner;                 // engine + afterburner (Thruster); force 0 if the ship has none
    Thrust cruise;                 // cruise engine (Cruise)
    float cruiseChargeTime = 0;    // CEEngine::GetCruiseChargeTime (s)
    float cruiseChargeElapsed = 0; // CEEngine::GetCruiseChargeTimeElapsed (s), while charging
    bool cruiseEngaged = false;    // CEEngine::IsCruiseEngaged (true from the request: charging or cruising)
    bool hasBurner = false;
    // Cruise in flight (verified 18 Oct 2026, Starflier): during the charge the ship holds the engine's top speed;
    // then the speed rises linearly at cruiseAccel (EngineEquipConsts CRUISING_SPEED / CRUISE_ACCEL_TIME = 60 m/s^2)
    // to cruiseSpeed (CRUISING_SPEED, 300); leaving cruise follows the force law (engine or reverse) again.
    float cruiseSpeed = 0, cruiseAccel = 0;
    // Turning: the top turn rate (rad/s) = steering torque / angular drag at full input (CShip::collect_angular_force_
    // components), the slower of pitch and yaw (Starflier 25000 / 15000 = 1.667 rad/s; measured 91-94 deg/s).
    float turnRate = 0;
    float TurnTime(float angle) const { return turnRate > 0 ? angle / turnRate : 0; }
};
bool ReadModel(Model& m);

// A straight run of `distance` metres from speed v0, arriving at vArrive, the fastest way the ship can: afterburner
// then full reverse, or (if it gains) cruise - charge, ramp, cruise, out and reverse. Simulated with the game's
// laws above (deterministic), before moving. `chargeDone`: seconds of cruise charge already elapsed. `turnAngle`
// (radians): the nose's angle off the way first - the turn (at the top turn rate) comes first in both runs, and the
// cruise charge runs during it (a player engages cruise while turning).
struct RunPlan
{
    float burnerTime = -1; // seconds by afterburner (engine if none) + braking
    float cruiseTime = -1; // seconds by cruise (-1: no cruise engine)
    bool cruise = false;   // the cruise run is the faster one
    float time() const { return cruise ? cruiseTime : burnerTime; }
};
// `coarse`: 50 ms steps instead of 5 ms (route planning evaluates many legs; ~0.05 s less exact).
RunPlan PlanRun(const Model& m, float v0, float distance, float vArrive, float chargeDone = 0, float turnAngle = 0,
                bool coarse = false);
// The same straight run in closed form (no simulation: for planners that evaluate many legs): the faster of the
// afterburner run (the force law's distance x(t) = top t + (v0 - top) tau (1 - e^(-t/tau)), solved for t) and the
// cruise run (charge at the engine's top speed, linear ramp, cruise), each plus a full-reverse stop when `stop`.
// From rest it matches PlanRun within ~0.1 s.
float QuickRunTime(const Model& m, float v0, float distance, bool stop);
// Distance needed to slow from v to vArrive at full reverse (leaving cruise first if cruising: the same law).
float BrakeDistance(const Model& m, float v, float vArrive);

// Debug: `perf` - the model and the cruise constants, for research and checks.
std::string Describe();

// Flight recorder (verification of plans against real flight): while on, one CSV line per frame - time
// (QueryPerformanceCounter), position, velocity, speed, the autopilot op, lane state (entry / exit ring), cruise - to
// <EXE>\<file>. RecordFrame is called once per frame by the module that owns the frame (controller); debug `rec start
// [file] | stop | status` (RecordCommand).
void RecordFrame();
std::string RecordCommand(const std::string& rest);
} // namespace flight
