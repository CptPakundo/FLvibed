// ASH-RIDE's pure half (ash_ride.h): the leave rule and its fallback, the cruise loop's geometry, the intercept / overrun
// decision and the coast gate. No game, no statics.
#include "ash_ride.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace ashride
{
namespace
{
float Dist3(const float a[3], const float b[3])
{
    const float d[3] = {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
    return std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
}
float Len3(const float a[3]) { return std::sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]); }
float Dot3(const float a[3], const float b[3]) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
float Clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

std::string Fmt(const char* fmt, ...)
{
    char b[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof(b), fmt, ap);
    va_end(ap);
    return b;
}

// the horizontal unit from a to b; false when they are (nearly) one point
bool Unit2(const float a[3], const float b[3], float u[3])
{
    u[0] = b[0] - a[0], u[1] = 0, u[2] = b[2] - a[2];
    const float l = std::sqrt(u[0] * u[0] + u[2] * u[2]);
    if (l < 1)
        return false;
    u[0] /= l, u[2] /= l;
    return true;
}
} // namespace

// ---------------------------------------------------------------- 1. the leave rule
bool Leave(float shieldFrac, float hull) { return shieldFrac >= 0 && shieldFrac <= kLeaveShield && hull > 0 && hull <= kLeaveHull; }

FallbackResult LeftFallback(float leftS, bool escortAlive, const float me[3], const float vel[3],
                            const std::vector<lookahead::StageLeash>& leashes)
{
    FallbackResult r;
    if (!escortAlive)
        return r;
    if (leftS > kLeftS)
        return r.fall = true, r.why = Fmt("%.0f s", kLeftS), r;
    for (const lookahead::StageLeash& l : leashes)
    {
        if (l.radius <= 0 || l.frac <= 0)
            continue;
        const float d = Dist3(me, l.anchor), lim = (l.frac + kLeashOver) * l.radius;
        if (d <= lim || d < 1)
            continue;
        const float out[3] = {(me[0] - l.anchor[0]) / d, (me[1] - l.anchor[1]) / d, (me[2] - l.anchor[2]) / d};
        if (Dot3(vel, out) > kLeashOutSpeed)
            return r.fall = true, r.why = Fmt("leash %s %.0f/%.0f m", l.name.c_str(), d, lim), r;
    }
    return r;
}

// ---------------------------------------------------------------- 2. the loop
float CruiseTurnRadius(float cruiseSpeed, float turnRate)
{
    const float v = cruiseSpeed > 0 ? cruiseSpeed : 300.0f;
    return turnRate > 0 ? v / (kTurnUse * turnRate) : v / kTurnMeasured;
}

float LoopRadius(float turnRadius) { return Clampf(kLoopMargin * turnRadius, kLoopMinR, kLoopMaxR); }

Loop PlanLoop(const float spawn[3], const float runTo[3], float r, const std::vector<lookahead::StageLeash>& leashes)
{
    Loop best;
    float u[3];
    if (!Unit2(spawn, runTo, u))
        return best.why = "no-line", best;
    memcpy(best.u, u, sizeof(best.u));
    best.r = r;
    for (const lookahead::StageLeash& l : leashes)
        if (l.radius > 0 && l.frac > 0 && l.frac * l.radius <= r)
            return best.why = "leash " + l.name + " tighter than the loop", best;
    // the candidates: r + kSpawnClear first, then the band's 100 m steps
    std::vector<float> backs = {Clampf(r + kSpawnClear, kBackMin, kBackMax)};
    for (float b = kBackMin; b <= kBackMax + 0.5f; b += 100)
        if (std::fabs(b - backs[0]) > 1)
            backs.push_back(b);
    bool have = false, anyInside = false;
    float bestMoved = 0;
    for (const float back : backs)
    {
        float want[3], c[3];
        for (int i = 0; i < 3; ++i)
            want[i] = spawn[i] - back * u[i];
        want[1] = spawn[1];
        memcpy(c, want, sizeof(c));
        std::vector<bool> pulled(leashes.size(), false);
        for (int round = 0; round < 64; ++round)
        {
            bool any = false;
            for (size_t k = 0; k < leashes.size(); ++k)
            {
                const lookahead::StageLeash& l = leashes[k];
                if (l.radius <= 0 || l.frac <= 0)
                    continue;
                const float lim = l.frac * l.radius - r, d = Dist3(c, l.anchor);
                if (d > lim && d > 0)
                {
                    for (int i = 0; i < 3; ++i)
                        c[i] = l.anchor[i] + (c[i] - l.anchor[i]) * (lim / d);
                    pulled[k] = any = true;
                }
            }
            if (!any)
                break;
        }
        bool inside = true;
        for (const lookahead::StageLeash& l : leashes)
            if (l.radius > 0 && l.frac > 0 && Dist3(c, l.anchor) + r > l.frac * l.radius + 1.0f)
                inside = false;
        if (!inside)
            continue;
        anyInside = true;
        const float moved = Dist3(c, want), dSpawn = Dist3(c, spawn);
        Loop t;
        memcpy(t.c, c, sizeof(t.c));
        memcpy(t.u, u, sizeof(t.u));
        t.r = r, t.back = back, t.dSpawn = dSpawn, t.farthest = dSpawn + r, t.clamped = moved > 0.5f;
        for (size_t k = 0; k < leashes.size(); ++k)
            if (pulled[k])
                t.bound += (t.bound.empty() ? "" : ",") + leashes[k].name;
        if (t.farthest > kMaxFromSpawn)
        {
            if (!have)
                best = t, best.why = Fmt("far %.0f m", t.farthest);
            continue;
        }
        if (!have || !best.ok || moved < bestMoved - 0.5f)
        {
            best = t, best.ok = true, best.why = t.clamped ? "clamped" : "free";
            bestMoved = moved, have = true;
        }
    }
    if (!anyInside)
        best.ok = false, best.why = "no-loop-in-leashes";
    return best;
}

int LoopDir(const float me[3], const float vel[3], const float c[3])
{
    const float rx = me[0] - c[0], rz = me[2] - c[2];
    return rx * vel[2] - rz * vel[0] >= 0 ? 1 : -1;
}

void LoopCarrot(const float me[3], const Loop& l, int dir, float speed, float out[3])
{
    const float rx = me[0] - l.c[0], rz = me[2] - l.c[2];
    const float d = std::sqrt(rx * rx + rz * rz);
    const float phi = d > 1 ? std::atan2(rz, rx) : 0.0f;
    const float tangent = d > l.r ? std::acos(Clampf(l.r / d, -1, 1)) : 0.0f;
    const float lead = Clampf((speed > 1 ? speed : 300.0f) * kLeadS / std::max(1.0f, l.r), 0.25f, 1.0f);
    const float a = phi + (dir >= 0 ? 1.0f : -1.0f) * (tangent + lead);
    out[0] = l.c[0] + l.r * std::cos(a);
    out[1] = l.c[1];
    out[2] = l.c[2] + l.r * std::sin(a);
}

// ---------------------------------------------------------------- 3. the intercept
namespace
{
constexpr float HisRate() // m/s^2 in his ramp
{
    return (lookahead::kHisCharge.top - lookahead::kHisCharge.slow) /
           (lookahead::kHisCharge.topAtS - lookahead::kHisCharge.slowS);
}
bool HisSlow(float hisSpeed) { return hisSpeed <= lookahead::kHisCharge.slow + 15.0f; }
} // namespace

float HisTimeToTop(float hisSpeed, float tSince)
{
    if (hisSpeed >= kHisTop)
        return 0;
    const lookahead::ChargeProfile& p = lookahead::kHisCharge;
    if (HisSlow(hisSpeed))
        return std::max(p.topAtS - tSince, (p.top - p.slow) / HisRate());
    return std::max(0.0f, (p.top - hisSpeed) / HisRate());
}

float HisAdvance(float hisSpeed, float tSince, float T)
{
    if (T <= 0)
        return 0;
    const lookahead::ChargeProfile& p = lookahead::kHisCharge;
    if (hisSpeed >= kHisTop)
        return hisSpeed * T;
    const bool slow = HisSlow(hisSpeed);
    const float v0 = slow ? p.slow : hisSpeed;
    const float rampS = (p.top - v0) / HisRate();
    // (in the slow phase: the slow speed until his ramp starts - HisTimeToTop's end less the ramp)
    const float slowLeft = slow ? std::max(0.0f, HisTimeToTop(hisSpeed, tSince) - rampS) : 0.0f;
    const float inSlow = std::min(T, slowLeft);
    const float inRamp = std::min(T - inSlow, rampS);
    const float vEnd = v0 + HisRate() * inRamp;
    return p.slow * inSlow + 0.5f * (v0 + vEnd) * inRamp + p.top * std::max(0.0f, T - inSlow - inRamp);
}

float OurAdvance(float mySpeed, float chargeLeftS, float T)
{
    if (T <= 0)
        return 0;
    const lookahead::ChargeProfile& p = lookahead::kOurCharge;
    const float c = std::min(std::max(0.0f, chargeLeftS), T);
    float dist = (chargeLeftS > 0 ? p.slow : 0.0f) * c;
    float t = T - c;
    const float v0 = chargeLeftS > 0 ? p.slow : std::max(mySpeed, p.slow);
    if (v0 >= p.top)
        return dist + v0 * t;
    const float ramp = std::min(t, (p.top - v0) / kOurRamp);
    dist += (v0 + 0.5f * kOurRamp * ramp) * ramp;
    t -= ramp;
    return dist + p.top * std::max(0.0f, t);
}

float TurnLen(float angle, float R)
{
    const float a = Clampf(std::fabs(angle), 0, 3.14159265f);
    return R * (a - std::sin(a));
}

const char* VerdictName(Verdict v)
{
    switch (v)
    {
    case Verdict::Go:
        return "go";
    case Verdict::Overrun:
        return "overrun";
    case Verdict::Track:
        return "track";
    default:
        return "wait";
    }
}

namespace
{
float Angle2(const float a[3], const float b[3]) // horizontal angle between two directions (rad)
{
    const float la = std::sqrt(a[0] * a[0] + a[2] * a[2]), lb = std::sqrt(b[0] * b[0] + b[2] * b[2]);
    if (la < 1e-3f || lb < 1e-3f)
        return 0;
    return std::acos(Clampf((a[0] * b[0] + a[2] * b[2]) / (la * lb), -1, 1));
}

// our way to `q` arriving along `u`: by the join point j (q - k u, k = our offset from his line, at most kJoinMax) when
// we are behind it, else straight at q; the turns' extra way (TurnLen) counted at radius R
float PathTo(const float me[3], const float heading[3], const float q[3], const float u[3], float lat, float R, float j[3])
{
    const float k = std::min(lat, kJoinMax);
    const float aQ = (me[0] - q[0]) * u[0] + (me[2] - q[2]) * u[2];
    for (int i = 0; i < 3; ++i)
        j[i] = aQ <= -k ? q[i] - u[i] * k : q[i];
    const float toJ[3] = {j[0] - me[0], j[1] - me[1], j[2] - me[2]};
    float len = Len3(toJ) + TurnLen(Angle2(heading, toJ), R);
    if (aQ <= -k && k > 1)
    {
        const float jq[3] = {q[0] - j[0], q[1] - j[1], q[2] - j[2]};
        len += Len3(jq) + TurnLen(Angle2(toJ, u), R);
    }
    else
        len += TurnLen(Angle2(toJ, u), R);
    return len;
}
} // namespace

InterceptResult Intercept(const InterceptFacts& f)
{
    InterceptResult r;
    const float d[3] = {f.me[0] - f.his[0], 0, f.me[2] - f.his[2]};
    r.along = d[0] * f.u[0] + d[2] * f.u[2];
    const float latv[3] = {d[0] - r.along * f.u[0], f.me[1] - f.his[1], d[2] - r.along * f.u[2]};
    r.lat = Len3(latv);
    r.tReady = HisTimeToTop(f.hisSpeed, f.tSince);
    const float adv = HisAdvance(f.hisSpeed, f.tSince, r.tReady);
    for (int i = 0; i < 3; ++i)
        r.station[i] = f.his[i] + f.u[i] * (adv - kGap);
    float heading[3] = {f.vel[0], 0, f.vel[2]};
    if (Len3(heading) < 1)
        memcpy(heading, f.u, sizeof(heading));
    const float R = f.turnR > 0 ? f.turnR : 240.0f;
    float j[3];
    r.path = PathTo(f.me, heading, r.station, f.u, r.lat, R, j);
    r.need = OurAdvance(f.mySpeed, f.chargeLeftS, r.tReady);
    const auto steerAt = [&](const float p[3]) { memcpy(r.steer, p, sizeof(r.steer)); };
    // he is at cruise (or about to be): track him from behind, pass and turn when beside / ahead, else close in
    if (r.tReady <= kReadyS)
    {
        float live[3];
        for (int i = 0; i < 3; ++i)
            live[i] = f.his[i] - f.u[i] * kGap;
        if (r.along > kOverrunAlong)
            return r.v = Verdict::Overrun, r.why = "ahead", steerAt(live), r;
        if (r.along >= -kTrackMax && r.lat <= kTrackLat)
            return r.v = Verdict::Track, r.why = "behind", steerAt(f.his), r;
        PathTo(f.me, heading, live, f.u, r.lat, R, j);
        return r.v = Verdict::Go, r.why = "late", steerAt(j), r;
    }
    // he still ramps: beside / ahead of him we would pass him - pass and turn at cruise onto the station behind him
    if (r.along > kOverrunAlong)
    {
        float live[3];
        for (int i = 0; i < 3; ++i)
            live[i] = f.his[i] - f.u[i] * kGap;
        return r.v = Verdict::Overrun, r.why = "beside", steerAt(live), r;
    }
    // early (our way to the station shorter than what we fly in his time to cruise): keep looping - never after a Go
    if (!f.committed && r.path < r.need - kSlack)
        return r.v = Verdict::Wait, r.why = "early", r;
    return r.v = Verdict::Go, r.why = f.committed ? "on" : "timed", steerAt(j), r;
}

// ---------------------------------------------------------------- 4. the coast gate
float AngleDeg(const float a[3], const float b[3])
{
    const float la = Len3(a), lb = Len3(b);
    if (la < 1e-3f || lb < 1e-3f)
        return 180;
    return std::acos(Clampf(Dot3(a, b) / (la * lb), -1, 1)) * 57.2958f;
}

bool CoastReady(CoastGate& g, const CoastFacts& f, float* stableS)
{
    const bool ok = f.cruising && f.me >= kCoastMe && f.his >= kCoastHis && !f.shieldOut && f.gap >= kCoastMin &&
                    f.gap <= kCoastMax && f.velDeg <= kCoastVelDeg && f.parallelDeg <= kCoastParallelDeg;
    if (!ok)
    {
        g.since = -1;
        if (stableS)
            *stableS = 0;
        return false;
    }
    if (g.since < 0 || std::fabs(f.gap - g.ref) > kStableBand)
        g.since = f.now, g.ref = f.gap;
    const float held = f.now - g.since;
    if (stableS)
        *stableS = held;
    return held >= kStableS;
}
} // namespace ashride
