// `city_hold_side` - the static hold at a big target that hostile weapons platforms guard (set M13-ab11-b, run-judged
// 31 Oct 2026: every death at the city's first generator held 31-67 m off the OUTWARD face, 100-163 deg from the city
// centre, in front of the platforms; docs\research\runners\M13_city_hold.md). This is its pure half: geometry in,
// verdicts out - no game call, no statics - so tests\city_hold_test.cpp runs the same code as the game. The impure
// adapter (the platforms from the live objects, the cover score by the game's turret test, the logs) is StaticHold and
// the launch rules in autopilot_combat.cpp. Compiled into the controller's logic and, plain, into build\test\fltest.exe;
// no system headers here (core\pre.h / common.h have them).
//
// World frame: y is up (the game's; the slide's horizontal tangent n x up, autopilot_combat.cpp StaticHold).
#pragma once
#include "common.h"
#include "game/geom.h"

namespace cityhold
{
// The side: within this many degrees (horizontal) of the direction away from the platforms' centroid.
constexpr float kSideDeg = 45.0f;
// The hold's hull gap (m): 110-130, aimed at 120; a re-anchor never closer than kMinGap.
constexpr float kGapLo = 120.0f, kGapHi = 130.0f, kMinGap = 100.0f;
// Ordnance only with the predicted impact and the nearest hull at least this far from us (m): above our own torpedo's
// measured 80-95 m reach (AUTOPILOT.md "Package AFK2", FREELANCER_RE "Explosion damage").
constexpr float kOrdnanceClear = 100.0f;
// The generator's height band (m, either way of its centre's y): M13_city_hold.md "Proposed rule".
constexpr float kBand = 100.0f;

// The horizontal direction from the platforms' centroid to the target (unit, y = 0): false with no platform, or with the
// centroid within 1 m of the target's vertical (no side to pick).
inline bool AwayDir(const float target[3], const float (*plat)[3], int n, float away[3])
{
    if (n <= 0)
        return false;
    float c[3] = {0, 0, 0};
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < 3; ++j)
            c[j] += plat[i][j] / static_cast<float>(n);
    away[0] = target[0] - c[0];
    away[1] = 0.0f;
    away[2] = target[2] - c[2];
    if (geom::Len(away) < 1.0f)
        return false;
    geom::Norm(away);
    return true;
}

// A candidate direction from the target's centre: `away` turned `azDeg` round the world's up axis and tilted `elDeg`
// (negative = below the target's height).
inline void SideDir(const float away[3], float azDeg, float elDeg, float out[3])
{
    const float az = azDeg * 3.14159265f / 180.0f, el = elDeg * 3.14159265f / 180.0f;
    const float h0 = away[0] * cosf(az) - away[2] * sinf(az), h2 = away[0] * sinf(az) + away[2] * cosf(az);
    out[0] = h0 * cosf(el);
    out[1] = sinf(el);
    out[2] = h2 * cosf(el);
}

// How far (deg, horizontal) a direction from the target lies off the away side; 180 when it is vertical.
inline float OffDeg(const float away[3], const float dir[3])
{
    const float hl = sqrtf(dir[0] * dir[0] + dir[2] * dir[2]);
    if (hl < 1e-4f)
        return 180.0f;
    const float c = (away[0] * dir[0] + away[2] * dir[2]) / hl;
    return acosf(std::max(-1.0f, std::min(1.0f, c))) * 180.0f / 3.14159265f;
}
inline bool OnSide(const float away[3], const float dir[3]) { return OffDeg(away, dir) <= kSideDeg + 0.01f; }

// The hold's hull gap from the base rule's (static_hold_gap + the every-gun steps + the launch need): 120-130 m.
inline float Gap(float base) { return std::max(kGapLo, std::min(kGapHi, base)); }

// A re-anchor onto our own line: allowed only with our line on the side and a gap it would set of at least kMinGap.
inline bool ReanchorOk(const float away[3], const float lineDir[3], float gap)
{
    return gap >= kMinGap && OnSide(away, lineDir);
}

// Ordnance (torpedo / missile / mine): the predicted burst (`impact`, m from us; < 0 = none within the flight: the round
// leaves) and the nearest hull round us (`hull`, m; < 0 = none within the rays) both at least kOrdnanceClear away.
inline bool OrdnanceOk(float impact, float hull)
{
    return (impact < 0 || impact >= kOrdnanceClear) && (hull < 0 || hull >= kOrdnanceClear);
}

// The height band: a point within kBand of the target centre's height.
inline bool InBand(float targetY, float y) { return fabsf(y - targetY) <= kBand; }

// A deeper point replaces the band's best only when the platform-cover score (the platform guns that can point at it,
// reach it and see it: lower is safer) is clearly lower - by at least 2 guns and a quarter of the band's. Unknown
// scores (< 0) never replace.
inline bool DeeperSafer(int bandCover, int deepCover)
{
    if (bandCover < 0 || deepCover < 0)
        return false;
    return deepCover + std::max(2, bandCover / 4) <= bandCover;
}

// The way to the side point round the target (probe ch1, 31 Oct 2026: off its point the hold handed the steering to the
// pursuit, which kept its own band on the arrival side - 131 deg off the side point for 34 s at the city's first
// generator): within kTransitDeg of the point's direction from the target's centre = the point itself; else our own
// direction turned kTransitDeg towards the point's, at the point's distance from the centre (a step round the hull, not
// through it). Returns the angle left (deg).
constexpr float kTransitDeg = 35.0f;
inline float TransitStep(const float centre[3], const float ship[3], const float point[3], float out[3])
{
    float a[3], b[3];
    for (int j = 0; j < 3; ++j)
        a[j] = ship[j] - centre[j], b[j] = point[j] - centre[j];
    const float la = geom::Len(a), lb = geom::Len(b);
    if (la < 1.0f || lb < 1.0f)
    {
        for (int j = 0; j < 3; ++j)
            out[j] = point[j];
        return 0.0f;
    }
    for (int j = 0; j < 3; ++j)
        a[j] /= la, b[j] /= lb;
    const float c = std::max(-1.0f, std::min(1.0f, geom::Dot(a, b)));
    const float left = acosf(c) * 180.0f / 3.14159265f;
    if (left <= kTransitDeg)
    {
        for (int j = 0; j < 3; ++j)
            out[j] = point[j];
        return left;
    }
    // the turn's axis a x b; opposite directions: round the world's up axis (a horizontal way round)
    float ax[3];
    geom::Cross(ax, a, b);
    if (geom::Len(ax) < 1e-3f)
    {
        // the world's up made perpendicular to a (a turn round it stays level); a vertical a: round x
        const float u = a[1];
        ax[0] = -u * a[0], ax[1] = 1.0f - u * a[1], ax[2] = -u * a[2];
        if (geom::Len(ax) < 1e-3f)
            ax[0] = 1.0f, ax[1] = ax[2] = 0.0f;
    }
    geom::Norm(ax);
    // Rodrigues: a turned by the step round ax (a is perpendicular to ax)
    const float t = kTransitDeg * 3.14159265f / 180.0f;
    float axa[3];
    geom::Cross(axa, ax, a);
    for (int j = 0; j < 3; ++j)
        out[j] = centre[j] + (a[j] * cosf(t) + axa[j] * sinf(t)) * lb;
    return left;
}
} // namespace cityhold
