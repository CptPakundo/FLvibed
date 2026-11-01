// FLvibed - our own ordnance: a homing round's flight, where it bursts and where we are then (WORK_QUEUE Q4 slice 1; the
// owner's rule of 29 Oct 2026, controller\docs\AUTOPILOT.md "DECISION (the owner, 29 Oct 2026 ~21:45)": full DPS always,
// splash avoided by WHERE we fire from, never by holding fire). Header-only and host-free: the game's ray test comes in
// as a callback, so the unit tests (tests\ordnance_test.cpp) run the same code as the game. Shared by every module
// (core\game), compiled into each; no system headers here (core\pre.h / common.h have them).
//
// Game facts used (docs\FREELANCER_RE.md "Missile launch speed", "Proximity fuse"): the round starts at the ship's
// velocity + the muzzle velocity along the nose, coasts through its motor delay, accelerates for the burn, then coasts
// (no drag); a seeker turns at most max_angular_velocity rad/s; the proximity fuse is a physics sphere of
// detonation_dist round the round (Server.dll arms it at launch), so a round bursts once a hull comes within that
// distance of it - and its explosion hurts at any reach inside its radius in HD Edition ("Explosion damage").
#pragma once
#include "common.h"
#include "game/geom.h"

namespace ordnance
{
// A homing round's flight data (the launcher's munition: weapon_equip.ini [Munition] / [Motor], read by
// space::ReadWeapons).
struct Round
{
    float muzzle = 0;     // muzzle velocity (m/s) along the nose, added to the ship's velocity
    float motorDelay = 0; // s of coasting before the motor lights
    float motorAccel = 0; // m/s^2 while it burns
    float motorTime = 0;  // burn (s)
    float lifetime = 0;   // s (0 or less: 30)
    float maxTurn = 0;    // the seeker's turn rate (rad/s), 0 = flies straight
    float blast = 0;      // explosion radius (m)
    float detonation = 0; // proximity fuse radius (detonation_dist, m)
};

// Seconds a round needs to close `dist` metres when it starts closing at `closing0` m/s (our range rate + the muzzle
// velocity): coasting through the motor delay, accelerating during the burn, coasting after. The target's own
// acceleration is ignored. 1e9 when the munition's lifetime runs out first. (Moved here from autopilot_target.cpp
// MissileFlightTime, 30 Oct 2026, the arithmetic unchanged: MissileFlightTime is now its wrapper.)
inline float FlightTime(const Round& r, float dist, float closing0)
{
    const float a = r.motorAccel > 0 ? r.motorAccel : 0.0f, delay = std::max(0.0f, r.motorDelay),
                burn = std::max(0.0f, r.motorTime), life = r.lifetime > 0 ? r.lifetime : 30.0f;
    float d = dist, t = 0, v = closing0;
    // delay: constant speed
    if (delay > 0)
    {
        if (v > 0 && v * delay >= d)
            return d / v;
        d -= v * delay;
        t += delay;
    }
    // burn: d = v t + a t^2 / 2
    if (burn > 0 && a > 0)
    {
        const float covered = v * burn + 0.5f * a * burn * burn;
        if (covered >= d)
        {
            const float disc = v * v + 2 * a * d;
            return t + (-v + std::sqrt(std::max(0.0f, disc))) / a;
        }
        d -= covered;
        t += burn;
        v += a * burn;
    }
    if (v <= 0)
        return 1e9f;
    t += d / v;
    return t <= life ? t : 1e9f;
}

// Why a simulated round ended.
enum Cause : int
{
    kNoImpact = 0, // still flying at the horizon
    kHull = 1,     // a hull (the ray test) came within its fuse
    kTarget = 2,   // it reached the target's reach (a target the ray test does not see: a ship)
    kExpired = 3,  // its lifetime ran out (the game removes it: it bursts where it is)
};
inline const char* CauseName(int c)
{
    return c == kHull ? "hull" : c == kTarget ? "target" : c == kExpired ? "expired" : "none";
}

struct Impact
{
    float pos[3] = {};
    float t = -1; // s after the launch, -1 none
    int cause = kNoImpact;
};

constexpr float kStep = 0.05f; // the simulation's step (s)
constexpr int kStepsPerRay = 4; // one hull test per this many steps (~0.2 s of flight)

// Turns the unit vector `h` toward the unit vector `u` by at most `maxAngle` rad (in place).
inline void TurnToward(float h[3], const float u[3], float maxAngle)
{
    const float c = Clampf(geom::Dot(h, u), -1.0f, 1.0f);
    const float angle = std::acos(c);
    if (angle <= maxAngle)
    {
        h[0] = u[0], h[1] = u[1], h[2] = u[2];
        return;
    }
    // the plane of h and u: w = the part of u across h (straight behind: any axis across h)
    float w[3] = {u[0] - h[0] * c, u[1] - h[1] * c, u[2] - h[2] * c};
    if (geom::Len(w) < 1e-4f)
    {
        const float axis[3] = {std::fabs(h[0]) < 0.9f ? 1.0f : 0.0f, std::fabs(h[0]) < 0.9f ? 0.0f : 1.0f, 0.0f};
        geom::Cross(w, h, axis);
    }
    geom::Norm(w);
    const float cs = std::cos(maxAngle), sn = std::sin(maxAngle);
    for (int i = 0; i < 3; ++i)
        h[i] = h[i] * cs + w[i] * sn;
    geom::Norm(h);
}

// A round launched now from `pos` at our velocity `vel` with the nose `nose` (unit), homing on the point `target`:
// flown in kStep steps (heading = its velocity, turned toward the target at most maxTurn rad/s; the motor's delay /
// burn as FlightTime), a hull test per kStepsPerRay steps through `hullHit(a, b)` (metres from a to the first solid on
// the segment a -> b, < 0 none: the game's ray test, the target's own hull included) on the segment stretched by the
// fuse radius (a hull within detonation_dist of the round sets it off). `targetReach` > 0: the round also ends within
// that of the target point (a ship target, whose hull the ray test does not see: its radius). Ends at its lifetime or
// at `horizon` s (kNoImpact).
template <class HullHit>
Impact RoundImpact(const Round& r, const float pos[3], const float vel[3], const float nose[3], const float target[3],
                   float targetReach, float horizon, HullHit&& hullHit)
{
    Impact out;
    float p[3] = {pos[0], pos[1], pos[2]};
    float v[3] = {vel[0] + nose[0] * r.muzzle, vel[1] + nose[1] * r.muzzle, vel[2] + nose[2] * r.muzzle};
    float speed = geom::Len(v);
    float h[3] = {nose[0], nose[1], nose[2]};
    if (speed > 1e-3f)
        h[0] = v[0] / speed, h[1] = v[1] / speed, h[2] = v[2] / speed;
    const float life = r.lifetime > 0 ? r.lifetime : 30.0f;
    const float tEnd = std::min(life, horizon);
    const float det = std::max(0.0f, r.detonation);
    const float delay = std::max(0.0f, r.motorDelay), burnEnd = delay + std::max(0.0f, r.motorTime);
    const float accel = r.motorAccel > 0 ? r.motorAccel : 0.0f;
    const float turnStep = std::max(0.0f, r.maxTurn) * kStep;
    const auto end = [&](const float at[3], float t, int cause)
    {
        out.pos[0] = at[0], out.pos[1] = at[1], out.pos[2] = at[2];
        out.t = t;
        out.cause = cause;
    };
    if (targetReach > 0 && geom::Dist(p, target) <= targetReach + det)
    {
        end(p, 0.0f, kTarget);
        return out;
    }
    float segStart[3] = {p[0], p[1], p[2]};
    float tSeg = 0, t = 0;
    // (the time from the step count, not a running sum: 30 x 0.05f summed falls short of 1.5)
    const int nSteps = tEnd > 0 ? static_cast<int>(std::ceil(tEnd / kStep - 1e-3f)) : 0;
    for (int step = 1; step <= nSteps; ++step)
    {
        // the seeker turns the heading toward the target
        float u[3];
        geom::Sub(u, target, p);
        if (geom::Len(u) > 1e-3f && turnStep > 0)
        {
            geom::Norm(u);
            TurnToward(h, u, turnStep);
        }
        // the motor (its acceleration over the step's middle: the burn's edges inside one step)
        const float t1 = step * kStep;
        const float burning = std::max(0.0f, std::min(t1, burnEnd) - std::max(t, delay));
        const float speed1 = speed + accel * burning;
        const float ds = 0.5f * (speed + speed1) * kStep;
        speed = speed1;
        for (int i = 0; i < 3; ++i)
            p[i] += h[i] * ds;
        t = t1;
        const bool reached = targetReach > 0 && geom::Dist(p, target) <= targetReach + det;
        if (step % kStepsPerRay == 0 || reached || step == nSteps)
        {
            float seg[3];
            geom::Sub(seg, p, segStart);
            const float len = geom::Len(seg);
            if (len > 1e-4f)
            {
                const float to[3] = {p[0] + seg[0] / len * det, p[1] + seg[1] / len * det, p[2] + seg[2] / len * det};
                const float hit = hullHit(segStart, to);
                if (hit >= 0 && hit <= len + det)
                {
                    const float along = std::max(0.0f, hit - det);
                    const float f = std::min(1.0f, along / len);
                    const float at[3] = {segStart[0] + seg[0] * f, segStart[1] + seg[1] * f, segStart[2] + seg[2] * f};
                    end(at, tSeg + (t - tSeg) * f, kHull);
                    return out;
                }
            }
            segStart[0] = p[0], segStart[1] = p[1], segStart[2] = p[2];
            tSeg = t;
        }
        if (reached)
        {
            end(p, t, kTarget);
            return out;
        }
    }
    if (nSteps > 0 && t >= life - 1e-3f)
        end(p, t, kExpired);
    else
    {
        out.pos[0] = p[0], out.pos[1] = p[1], out.pos[2] = p[2];
        out.t = -1;
        out.cause = kNoImpact;
    }
    return out;
}

// Where we are `t` s from now, following this frame's movement goal: the velocity steered toward
// clamp(goal error x gain, maxSpeed) at most `accel` m/s^2 (the station keeping's law, controller.cpp: gain 0.4 / s,
// <= 60 m/s; a steer-to run: a large gain, the run's top speed). `goal` null: the current velocity only.
struct Goal
{
    const float* pos = nullptr; // the goal point, null = coast
    float gain = 0.4f;          // 1 / s
    float maxSpeed = 60.0f;     // m/s
    float accel = 30.0f;        // m/s^2
};
inline void PredictPos(const float pos[3], const float vel[3], const Goal& goal, float t, float out[3])
{
    float p[3] = {pos[0], pos[1], pos[2]}, v[3] = {vel[0], vel[1], vel[2]};
    if (!goal.pos)
    {
        for (int i = 0; i < 3; ++i)
            out[i] = p[i] + v[i] * std::max(0.0f, t);
        return;
    }
    const float dvMax = std::max(0.0f, goal.accel) * kStep;
    for (float s = 0; s < t - 1e-6f; s += kStep)
    {
        const float dt = std::min(kStep, t - s);
        float want[3];
        for (int i = 0; i < 3; ++i)
            want[i] = (goal.pos[i] - p[i]) * goal.gain;
        const float wl = geom::Len(want);
        if (wl > goal.maxSpeed && wl > 0)
            geom::Scale(want, want, goal.maxSpeed / wl);
        float dv[3];
        geom::Sub(dv, want, v);
        const float dl = geom::Len(dv), lim = dvMax * (dt / kStep);
        if (dl > lim && dl > 0)
            geom::Scale(dv, dv, lim / dl);
        for (int i = 0; i < 3; ++i)
        {
            const float v0 = v[i];
            v[i] += dv[i];
            p[i] += 0.5f * (v0 + v[i]) * dt;
        }
    }
    out[0] = p[0], out[1] = p[1], out[2] = p[2];
}

// The distance a round's burst must keep from our centre: its blast + our size (5 m at the least) + a margin.
inline float Need(float blast, float ourRadius, float margin) { return blast + std::max(5.0f, ourRadius) + margin; }

// A launch is safe when its round bursts at least `need` from where we will be then (a round still flying at the
// horizon is: it bursts far on). `sep`: the separation (1e9 none).
inline bool LaunchSafe(const Impact& im, const float usAtImpact[3], float need, float* sep = nullptr)
{
    const float s = im.cause == kNoImpact ? 1e9f : geom::Dist(im.pos, usAtImpact);
    if (sep)
        *sep = s;
    return s >= need;
}

// The distance from `c` to the segment a -> b, and the fraction along it of the closest point.
inline float SegmentDist(const float a[3], const float b[3], const float c[3], float* frac = nullptr)
{
    float ab[3], ac[3];
    geom::Sub(ab, b, a);
    geom::Sub(ac, c, a);
    const float l2 = geom::Dot(ab, ab);
    const float f = l2 > 1e-8f ? Clampf(geom::Dot(ac, ab) / l2, 0.0f, 1.0f) : 0.0f;
    const float q[3] = {a[0] + ab[0] * f, a[1] + ab[1] * f, a[2] + ab[2] * f};
    if (frac)
        *frac = f;
    return geom::Dist(q, c);
}

// A movement goal kept out of a live impact sphere (centre `c`, radius `need`): from inside it, straight out to its
// surface (+ 5 m); a goal inside it, out to its surface along the line from the centre; a way from `pos` to the goal that
// crosses it, the goal moved across until the way passes at `need` (a few rounds of the same bend). True when changed.
inline bool KeepOut(const float pos[3], float goal[3], const float c[3], float need)
{
    if (need <= 0)
        return false;
    const auto outward = [&](const float from[3], float dist, float res[3])
    {
        float d[3];
        geom::Sub(d, from, c);
        float l = geom::Len(d);
        if (l < 1e-3f) // at the centre: any way out (up)
            d[0] = 0, d[1] = 1, d[2] = 0, l = 1;
        for (int i = 0; i < 3; ++i)
            res[i] = c[i] + d[i] / l * dist;
    };
    if (geom::Dist(pos, c) < need)
    {
        outward(pos, need + 5.0f, goal);
        return true;
    }
    bool changed = false;
    if (geom::Dist(goal, c) < need)
    {
        outward(goal, need, goal);
        changed = true;
    }
    for (int round = 0; round < 6; ++round)
    {
        float f = 0;
        const float d = SegmentDist(pos, goal, c, &f);
        if (d >= need - 0.01f || f <= 1e-3f)
            break;
        // the closest point moved out across the way: the goal shifted by (need - d) / f along that direction
        float ab[3];
        geom::Sub(ab, goal, pos);
        const float q[3] = {pos[0] + ab[0] * f, pos[1] + ab[1] * f, pos[2] + ab[2] * f};
        float u[3];
        geom::Sub(u, q, c);
        float ul = geom::Len(u);
        if (ul < 1e-3f) // straight through the centre: across the way (ab x up, or ab x x when the way is vertical)
        {
            const float up[3] = {0, 1, 0}, x[3] = {1, 0, 0};
            geom::Cross(u, ab, std::fabs(ab[1]) < 0.9f * geom::Len(ab) ? up : x);
            ul = geom::Len(u);
            if (ul < 1e-6f)
                break;
        }
        const float shift = (need - d) / f * 1.02f;
        for (int i = 0; i < 3; ++i)
            goal[i] += u[i] / ul * shift;
        changed = true;
    }
    return changed;
}

// The station keeping's time to a point `dist` m away (controller.cpp's law: speed = min(maxSpeed, gain x distance
// left), acceleration ignored), arrived within `arrive` m.
inline float StationTravelTime(float dist, float gain = 0.4f, float maxSpeed = 60.0f, float arrive = 15.0f)
{
    if (dist <= arrive || gain <= 0 || maxSpeed <= 0)
        return 0.0f;
    const float knee = maxSpeed / gain; // where the cap ends
    if (dist > knee)
        return (dist - knee) / maxSpeed + std::log(knee / arrive) / gain;
    return std::log(dist / arrive) / gain;
}

// The firing position's timing (the owner's rule 3: the ship moves between the ranges as the weapons need, timed to the
// launchers' refire, so no salvo waits): go to the firing site when a launcher's refire is due by the time we are there.
// travel < 0: no site.
inline bool ShouldGoToSite(float travel, float refireLeft)
{
    if (travel < 0)
        return false;
    return std::max(0.0f, refireLeft) <= travel;
}

// A firing-site candidate: its hull gap (m, the ray from it to the target's centre; < 0 unread / no hull seen) and its
// score (lower = better: the blind-spot scan's cover score).
struct SiteCand
{
    float gap = -1;
    float score = 0;
};
// The best candidate with gap >= standoff: the lowest score (the first of equals); -1 none.
inline int PickFireSite(const SiteCand* c, int n, float standoff)
{
    int best = -1;
    for (int i = 0; i < n; ++i)
        if (c[i].gap >= 0 && c[i].gap >= standoff && (best < 0 || c[i].score < c[best].score))
            best = i;
    return best;
}
} // namespace ordnance
