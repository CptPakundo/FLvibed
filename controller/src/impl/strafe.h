// `strafe_pass` - lengthwise strafing passes along a big armed hull (the battleship class) instead of any hold
// (docs\research\runners\M13_bsdown_datapass.md "Proposal": the runners fly ~150-350 m off the Nomad battleship's hull and
// survive by MOTION - mean 89-128 m/s, afterburner pulses 70 <-> 200 m/s every 1-2 s, lengthwise passes along its axis,
// turns beyond its ends, ~12 s a cycle - while every still or slow-sliding hold at a covered site died,
// M13_bsdown_consensus.md). This is its pure half: geometry in, verdicts out - no game call, no statics - so
// tests\strafe_test.cpp runs the same code as the game. The impure adapter (the pose, the game's ray test, the turret
// count, the outputs and the logs) is StrafeFly in autopilot_combat.cpp. No system headers here (core\pre.h / common.h).
//
// Frames: everything below is in the TARGET's own frame (its rotation's columns = its local x / y / z, the pose of
// space::ObjectPose), so a target that turns slowly keeps its passes; the adapter converts world <-> local each frame.
// Measure() alone takes world points, for the ray test.
#pragma once
#include "common.h"
#include "game/geom.h"

namespace strafe
{
// The hull gap band (m, by real rays to the hull, never its sphere: the HUD's "0 m" is the bounding sphere): aimed at
// kGapWant, the nose leaves the hull under kGapLo, the slide pushes out under kGapLo + kLateralBand and in over
// kGapHi - kLateralBand. At these gaps the launch rules' line gap (torp_reach 100, the 47 m hull-beside rays) passes.
constexpr float kGapLo = 150.0f, kGapWant = 200.0f, kGapHi = 250.0f, kLateralBand = 15.0f;
// A live gap under this in a pass: the passes hit geometry - fall back (our own torpedo reaches us from 80-95 m).
constexpr float kGapAbort = 80.0f;
// The battleship class: a hull at least this long along its long axis, and that many times longer than wide.
constexpr float kMinLength = 600.0f, kMinAspect = 1.8f;
// Width samples along the long axis, per side; the line follows the widest of them within kSmooth of our station.
constexpr int kStations = 9;
constexpr float kSmooth = 300.0f; // (a crude flight with 150 m ran into the bow's slope: the line widens before the end)
// The turn stations: at least kTurnBeyond past the hull's end and kTurnMin from the centre (the runners' 1-2 km; with
// the Nomad battleship's ends at -642 / +613 that is ~1.0 km out, a ~2 km pass - R2's ~12 s cycle at ~150 m/s).
constexpr float kTurnBeyond = 400.0f, kTurnMin = 1000.0f;
// The speed schedule: full throttle always (the engine's top, ~80 m/s, is the trough: never under kFloor), the
// afterburner on for kBurnOn s, off for kBurnOff s (tau = mass / drag ~0.17 s: ~200 <-> ~80 m/s, a swing of >= 80 m/s
// every 1.6 s, the mean ~150); under 2 x kFloor the afterburner stays on (a brake by the geometry guard, a stop).
// No battery stop is planned: the dose is tapped while moving (DamageAndRepairs needs no stop).
constexpr float kBurnOn = 1.0f, kBurnOff = 0.6f, kFloor = 20.0f;
// The nose on the hull ahead: the aim point lies kAimDeg off the pass direction at our gap (its lead = gap / tan); a
// point more than kAimMaxDeg off (the hull's far end close by) is not aimed at - the nose flies the line instead.
// (18 deg: at 200 m/s the nose's closing 62 m/s nearly cancels against the slide's ~50 - the strafe speed, FREELANCER_RE
// "DrasticEvade" - so the aim holds; the point lies ~gap / sin 18 = ~650 m ahead at a 200 m gap, ~530 m at 165)
constexpr float kAimDeg = 18.0f, kAimMaxDeg = 35.0f;
// ...and once the gap fell under kGapLo, the nose goes back on the hull only at kReaimGap (no chatter at the band's floor).
constexpr float kReaimGap = 175.0f;
// Leads (m): the line point ahead when not aiming; the join point ahead of our station; the turn's entry point back
// toward the hull from the turn station.
constexpr float kLineLead = 300.0f, kJoinLead = 400.0f, kEntryBack = 500.0f;
// The join ends within kJoinTol of the line with the motion within 45 deg of the pass direction (the turn likewise).
constexpr float kJoinTol = 80.0f, kHeadingCos = 0.7071f;
// Fail closed (s): a join, a turn or a pass that lasts longer than this - something holds the ship.
constexpr float kJoinTimeout = 25.0f, kTurnTimeout = 10.0f, kPassTimeout = 30.0f;

enum class Phase
{
    Join = 0, // to the first pass line
    Pass = 1, // along the hull, the nose on it
    Turn = 2, // round at a turn station onto the next line
};
inline const char* PhaseName(Phase p) { return p == Phase::Join ? "join" : p == Phase::Pass ? "pass" : "turn"; }

// The hull as the ray test sees it, in the target's frame.
struct Hull
{
    bool ok = false;
    const char* why = "unmeasured"; // the failure ("no hull", "short", "not elongated", "no width"), "" when ok
    int axis = 2;                   // the long local axis (0 x, 1 y, 2 z)
    float lo = 0, hi = 0;           // its extent along that axis (m from the centre, lo < hi)
    float len = 0, aspect = 0;      // hi - lo; len over the widest other axis' extent
    // per side (0 +u, 1 -u, 2 +v, 3 -v; u < v the two other local axes): the hull's half-extent from the axis at each
    // station (lo .. hi, evenly), 0 where the ray met none
    float width[4][kStations] = {};
    float widthMax[4] = {};
};

// The two local axes other than `axis`, in order.
inline void OtherAxes(int axis, int& u, int& v)
{
    u = axis == 0 ? 1 : 0;
    v = axis == 2 ? 1 : 2;
}
// Unit vectors in the target's frame: the long axis; a side's direction from it.
inline void AxisDir(const Hull& h, float out[3])
{
    out[0] = out[1] = out[2] = 0;
    out[h.axis] = 1;
}
inline void SideDir(const Hull& h, int side, float out[3])
{
    int u, v;
    OtherAxes(h.axis, u, v);
    out[0] = out[1] = out[2] = 0;
    out[side < 2 ? u : v] = (side & 1) ? -1.0f : 1.0f;
}
// The sides across from `side` (k = 0, 1): the other axis' two.
inline int Perp(int side, int k) { return side < 2 ? 2 + k : k; }
inline float StationZ(const Hull& h, int k) { return h.lo + (h.hi - h.lo) * static_cast<float>(k) / (kStations - 1); }

// The hull's real width on a side at a station (linear between the samples), 0 outside its extent.
inline float WidthRaw(const Hull& h, int side, float z)
{
    if (h.hi <= h.lo || z < h.lo || z > h.hi)
        return 0.0f;
    const float f = (z - h.lo) / (h.hi - h.lo) * (kStations - 1);
    const int k = std::min(kStations - 2, std::max(0, static_cast<int>(f)));
    const float t = Clampf(f - static_cast<float>(k), 0.0f, 1.0f);
    return h.width[side][k] * (1.0f - t) + h.width[side][k + 1] * t;
}
// The nearest hull on a side as the samples see it: the distance in that side's (axial, lateral) plane from (z, r) to
// the width profile (the polyline through the stations' (z_k, width_k); its tips lie on the axis), 0 inside it. Beyond
// the hull's ends - where the live ray to the axis meets nothing - this is the gap to the end and the bulb ahead.
inline float ProfileGap(const Hull& h, int side, float z, float r)
{
    if (z >= h.lo && z <= h.hi && r <= WidthRaw(h, side, z))
        return 0.0f;
    float best = 1e9f;
    for (int k = 0; k + 1 < kStations; ++k)
    {
        const float z0 = StationZ(h, k), w0 = h.width[side][k];
        const float dz = StationZ(h, k + 1) - z0, dw = h.width[side][k + 1] - w0;
        const float l2 = dz * dz + dw * dw;
        const float t = l2 > 0 ? Clampf(((z - z0) * dz + (r - w0) * dw) / l2, 0.0f, 1.0f) : 0.0f;
        const float ez = z - (z0 + t * dz), er = r - (w0 + t * dw);
        best = std::min(best, sqrtf(ez * ez + er * er));
    }
    return best;
}
// ...the widest within kSmooth of the station: what the pass line keeps its gap to (no jerk into a narrow part).
inline float WidthAt(const Hull& h, int side, float z)
{
    return std::max({WidthRaw(h, side, z - kSmooth), WidthRaw(h, side, z), WidthRaw(h, side, z + kSmooth)});
}
// The pass line on a side at a station, `gap` off the (smoothed) hull.
inline void LinePoint(const Hull& h, int side, float z, float gap, float out[3])
{
    float a[3], d[3];
    AxisDir(h, a);
    SideDir(h, side, d);
    const float r = WidthAt(h, side, z) + gap;
    for (int i = 0; i < 3; ++i)
        out[i] = a[i] * z + d[i] * r;
}
// The turn station the pass in `dir` (+1 toward hi, -1 toward lo) runs to (m along the axis, signed).
inline float TurnStation(const Hull& h, int dir)
{
    const float end = dir > 0 ? h.hi : -h.lo; // that end's distance from the centre, in the pass direction
    const float mag = std::max(kTurnMin, end + kTurnBeyond);
    return dir > 0 ? mag : -mag;
}
// The first pass runs toward the far end (from beyond an end: in along the hull).
inline int StartDir(const Hull& h, const float pos[3]) { return pos[h.axis] >= 0 ? -1 : 1; }
// The sides ordered by how well they face `pos` (the nearest first): the first pass's choice.
inline void SideOrder(const Hull& h, const float pos[3], int order[4])
{
    float dots[4];
    for (int s = 0; s < 4; ++s)
    {
        float d[3];
        SideDir(h, s, d);
        dots[s] = geom::Dot(pos, d);
        order[s] = s;
    }
    std::sort(order, order + 4, [&](int a, int b) { return dots[a] > dots[b]; });
}
// The first side in `order` whose pass line in `dir` is clear (lineClear(side, dir): the adapter's rays); -1 none.
template <class LineClear> inline int PickSide(const int order[4], int dir, LineClear&& lineClear)
{
    for (int i = 0; i < 4; ++i)
        if (lineClear(order[i], dir))
            return order[i];
    return -1;
}
// The motion within the angle of a direction (unit), and moving at all.
inline bool Heading(const float vel[3], const float dir[3], float cosMin)
{
    const float v = geom::Len(vel);
    return v > 1.0f && geom::Dot(vel, dir) / v >= cosMin;
}
// The afterburner this frame: the pulse `t` s into the schedule, or the speed under twice the floor.
inline bool Burn(float t, float speed)
{
    if (speed < 2.0f * kFloor)
        return true;
    return fmodf(std::max(0.0f, t), kBurnOn + kBurnOff) < kBurnOn;
}
// The slide toward the gap band: +1 out (the side's direction) - under kGapLo + kLateralBand with the nose on the hull,
// under kGapWant - kLateralBand while flying the line (back to the re-aim gap quickly) -, -1 in (only by a live gap:
// the hull really is that far), 0 none.
inline int Lateral(float gap, bool live, bool aiming)
{
    if (gap < (aiming ? kGapLo + kLateralBand : kGapWant - kLateralBand))
        return 1;
    if (live && gap > kGapHi - kLateralBand)
        return -1;
    return 0;
}
// The player's slide key for a wanted world direction (Out::slide: 1 left, 2 right, 3 up, 4 down), by the larger of its
// components on the ship's right / up axes (fl::Kinematics rot: row-major, columns right / up / back); 0 when neither
// carries at least `minFrac` of it (the want lies along the nose).
inline int SlideCode(const float rot[9], const float want[3], float minFrac = 0.3f)
{
    const float l = geom::Len(want);
    if (l < 1e-3f)
        return 0;
    const float right = want[0] * rot[0 * 3 + 0] + want[1] * rot[1 * 3 + 0] + want[2] * rot[2 * 3 + 0];
    const float up = want[0] * rot[0 * 3 + 1] + want[1] * rot[1 * 3 + 1] + want[2] * rot[2 * 3 + 1];
    if (std::max(fabsf(right), fabsf(up)) < minFrac * l)
        return 0;
    return fabsf(right) >= fabsf(up) ? (right > 0 ? 2 : 1) : (up > 0 ? 3 : 4);
}
// The hull point ahead the nose goes for: on our side's hull surface at a lead of gap / tan(kAimDeg) ahead (inside the
// hull's extent, 5 % off its ends), where the hull is there (width > 0), ahead of us, at most kAimMaxDeg off the pass
// direction and (reach > 0) within reach. False = no aim (the hull's far end is close: fly the line).
inline bool AimPoint(const Hull& h, int side, int dir, const float pos[3], float gap, float reach, float out[3])
{
    float a[3], d[3];
    AxisDir(h, a);
    SideDir(h, side, d);
    const float g = std::max(gap, 50.0f);
    const float margin = 0.05f * (h.hi - h.lo);
    const float zs = Clampf(pos[h.axis] + dir * g / tanf(kAimDeg * 3.14159265f / 180.0f), h.lo + margin, h.hi - margin);
    const float w = WidthRaw(h, side, zs);
    if (w <= 0)
        return false;
    float s[3], to[3];
    for (int i = 0; i < 3; ++i)
        s[i] = a[i] * zs + d[i] * w, to[i] = s[i] - pos[i];
    const float dist = geom::Len(to);
    const float along = geom::Dot(to, a) * dir;
    if (dist < 1.0f || along <= 0 || along / dist < cosf(kAimMaxDeg * 3.14159265f / 180.0f) ||
        (reach > 0 && dist > reach))
        return false;
    memcpy(out, s, sizeof(s));
    return true;
}
// The turn's entry point: on the new line, kEntryBack from the turn station toward the hull.
inline void TurnSteer(const Hull& h, int side, int dir, float out[3])
{
    LinePoint(h, side, TurnStation(h, -dir) + dir * kEntryBack, kGapWant, out);
}

// The hull from the game's ray test (`ray(from, to)`: metres from `from` to the first solid on the segment, < 0 none;
// world points). `c` / `rot` the target's pose (rot row-major, columns = its local axes), `radius` its sphere. Along
// each local axis both ways: the farthest of 5 parallel rays from outside the sphere back toward the centre plane (a
// thin, off-centre tail still counts); the long axis is the longest; then the width per side at kStations stations.
// Fails closed: any direction without a hit ("no hull" - a ship's hull is no solid to the game's test), shorter than
// kMinLength, less elongated than kMinAspect, a side without any width.
template <class Ray> inline Hull Measure(const float c[3], const float rot[9], float radius, Ray&& ray)
{
    Hull h;
    const float r0 = std::max(radius, 100.0f) * 1.5f + 100.0f;
    const auto toWorld = [&](const float l[3], float w[3])
    {
        for (int i = 0; i < 3; ++i)
            w[i] = c[i] + rot[i * 3 + 0] * l[0] + rot[i * 3 + 1] * l[1] + rot[i * 3 + 2] * l[2];
    };
    const auto cast = [&](const float la[3], const float lb[3]) -> float
    {
        float wa[3], wb[3];
        toWorld(la, wa);
        toWorld(lb, wb);
        return ray(wa, wb);
    };
    float ext[3][2];
    for (int i = 0; i < 3; ++i)
    {
        int u, v;
        OtherAxes(i, u, v);
        for (int sg = 0; sg < 2; ++sg)
        {
            const float s = sg ? -1.0f : 1.0f, off = 0.1f * r0;
            float best = -1.0f;
            for (int k = 0; k < 5; ++k)
            {
                float a[3] = {}, b[3] = {};
                a[u] = b[u] = k == 1 ? off : k == 2 ? -off : 0.0f;
                a[v] = b[v] = k == 3 ? off : k == 4 ? -off : 0.0f;
                a[i] = s * r0;
                const float hit = cast(a, b);
                if (hit >= 0)
                    best = std::max(best, r0 - hit);
            }
            if (best < 0)
            {
                h.why = "no hull";
                return h;
            }
            ext[i][sg] = best;
        }
    }
    float lens[3];
    for (int i = 0; i < 3; ++i)
        lens[i] = ext[i][0] + ext[i][1];
    h.axis = lens[0] > lens[1] ? (lens[0] > lens[2] ? 0 : 2) : (lens[1] > lens[2] ? 1 : 2);
    int u, v;
    OtherAxes(h.axis, u, v);
    h.len = lens[h.axis];
    h.aspect = h.len / std::max(1.0f, std::max(lens[u], lens[v]));
    h.hi = ext[h.axis][0];
    h.lo = -ext[h.axis][1];
    if (h.len < kMinLength)
    {
        h.why = "short";
        return h;
    }
    if (h.aspect < kMinAspect)
    {
        h.why = "not elongated";
        return h;
    }
    for (int s = 0; s < 4; ++s)
    {
        float d[3];
        SideDir(h, s, d);
        for (int k = 0; k < kStations; ++k)
        {
            float a[3] = {}, b[3] = {};
            a[h.axis] = b[h.axis] = StationZ(h, k);
            for (int j = 0; j < 3; ++j)
                a[j] += d[j] * r0;
            const float hit = cast(a, b);
            h.width[s][k] = hit >= 0 ? std::max(0.0f, r0 - hit) : 0.0f;
            h.widthMax[s] = std::max(h.widthMax[s], h.width[s][k]);
        }
        if (h.widthMax[s] <= 0)
        {
            h.why = "no width";
            return h;
        }
    }
    h.ok = true;
    h.why = "";
    return h;
}

// One frame of the passes, in the target's frame.
struct StepIn
{
    Phase phase = Phase::Join;
    int side = 0, dir = 1;
    float pos[3] = {}, vel[3] = {}; // ours
    float gapNow = -1;              // the live gap: the ray from us to our station's axis point (m), -1 none
    float phaseTime = 0;            // s in this phase
    float reach = 0;                // the aim point at most this far (m), 0 = any
    bool aimed = false;             // the last frame's StepOut::aim
};
struct StepOut
{
    Phase phase = Phase::Join;
    int side = 0, dir = 1;
    float steer[3] = {};       // where the nose goes
    bool aim = false;          // `steer` is the hull point the guns and launchers are on
    int lateral = 0;           // the slide (Lateral): +1 out along the side's direction, -1 in, 0 none
    bool passStarted = false;  // a pass begins this frame
    bool turned = false;       // a pass ended at its turn station this frame (the turn begins)
    float turnAt = 0;          // ...our station along the axis there (m)
    float gap = -1;            // the gap used: the smaller of the live one and ProfileGap (the profile alone beyond the ends)
    const char* fallback = nullptr; // fail closed: "join" / "turn" / "pass" (too long), "geometry" (the passes hit it)
};

// The passes' state machine: Join (to the line of in.side, toward in.dir) -> Pass (the nose on the hull ahead, the slide
// keeping the gap band) -> at the turn station Turn (the next side: the far one first, then the same, then the two
// across - the first whose line `lineClear(side, dir)` finds clear) -> Pass in the other direction, and so on.
template <class LineClear> inline StepOut Step(const Hull& h, const StepIn& in, LineClear&& lineClear)
{
    StepOut o;
    o.phase = in.phase, o.side = in.side, o.dir = in.dir;
    float a[3], d[3];
    AxisDir(h, a);
    SideDir(h, in.side, d);
    const float z = in.pos[h.axis];
    const float r = geom::Dot(in.pos, d);
    const bool live = in.gapNow >= 0;
    const float planned = ProfileGap(h, in.side, z, r);
    o.gap = live ? std::min(in.gapNow, planned) : planned;
    const float want[3] = {a[0] * in.dir, a[1] * in.dir, a[2] * in.dir};
    switch (in.phase)
    {
    case Phase::Join:
    {
        const float zj = Clampf(z + in.dir * kJoinLead, TurnStation(h, -1), TurnStation(h, 1));
        LinePoint(h, in.side, zj, kGapWant, o.steer);
        float here[3];
        LinePoint(h, in.side, z, kGapWant, here);
        if (geom::Dist(here, in.pos) < kJoinTol && Heading(in.vel, want, kHeadingCos))
            o.phase = Phase::Pass, o.passStarted = true;
        else if (in.phaseTime > kJoinTimeout)
            o.fallback = "join";
        o.lateral = live && in.gapNow < kGapLo ? 1 : 0;
        break;
    }
    case Phase::Pass:
    {
        if (live && in.gapNow < kGapAbort)
        {
            o.fallback = "geometry";
            break;
        }
        if (in.phaseTime > kPassTimeout)
        {
            o.fallback = "pass";
            break;
        }
        if (in.dir * z >= in.dir * TurnStation(h, in.dir))
        {
            const int order[4] = {in.side ^ 1, in.side, Perp(in.side, 0), Perp(in.side, 1)};
            const int next = PickSide(order, -in.dir, lineClear);
            if (next < 0)
            {
                o.fallback = "geometry";
                break;
            }
            o.phase = Phase::Turn, o.side = next, o.dir = -in.dir, o.turned = true, o.turnAt = z;
            TurnSteer(h, o.side, o.dir, o.steer);
            break;
        }
        o.aim = o.gap >= (in.aimed ? kGapLo : kReaimGap) && AimPoint(h, in.side, in.dir, in.pos, o.gap, in.reach, o.steer);
        if (!o.aim)
            LinePoint(h, in.side, z + in.dir * kLineLead, kGapWant, o.steer);
        o.lateral = Lateral(o.gap, live, o.aim);
        break;
    }
    case Phase::Turn:
    {
        TurnSteer(h, in.side, in.dir, o.steer);
        if (Heading(in.vel, want, kHeadingCos))
            o.phase = Phase::Pass, o.passStarted = true;
        else if (in.phaseTime > kTurnTimeout)
            o.fallback = "turn";
        o.lateral = live && in.gapNow < kGapLo ? 1 : 0;
        break;
    }
    }
    return o;
}
} // namespace strafe
