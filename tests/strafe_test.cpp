// `strafe_pass`'s pure half (modules\autopilot\src\strafe.h): the hull measured by rays (an ellipsoid of the Nomad
// battleship's size, M13_bsdown_datapass.md: 1,254 m long, 277 x 244 m at its widest), the pass line at the gap, the
// aim point, the turn stations, the speed schedule, the slide, the side choice and the fail-closed cases; then a crude
// flight through the state machine (turn rate, the engine's tau, the strafe) that checks the passes keep the gap band,
// turn at both ends, and that our launch rules' line gaps (torp_reach 100: the line to the centre and the nose) pass.
#include "../modules/autopilot/src/strafe.h"

#include "test.h"

namespace
{
struct Ellipsoid
{
    float c[3];
    float rot[9]; // row-major, columns = its local axes in the world
    float s[3];   // semi-axes, local
};

void ToLocal(const Ellipsoid& e, const float w[3], float l[3])
{
    const float rel[3] = {w[0] - e.c[0], w[1] - e.c[1], w[2] - e.c[2]};
    for (int i = 0; i < 3; ++i)
        l[i] = e.rot[0 * 3 + i] * rel[0] + e.rot[1 * 3 + i] * rel[1] + e.rot[2 * 3 + i] * rel[2];
}

// The game's ray test on the ellipsoid: metres from a to the surface on a -> b, -1 none (from inside: none; a hit at
// the segment's last 3 %: none, as space::RayHitDistance)
float RayEllipsoid(const Ellipsoid& e, const float a[3], const float b[3])
{
    float la[3], lb[3];
    ToLocal(e, a, la);
    ToLocal(e, b, lb);
    float p[3], d[3];
    for (int i = 0; i < 3; ++i)
        p[i] = la[i] / e.s[i], d[i] = (lb[i] - la[i]) / e.s[i];
    const float A = geom::Dot(d, d), B = 2 * geom::Dot(p, d), C = geom::Dot(p, p) - 1;
    if (C <= 0 || A <= 0)
        return -1;
    const float disc = B * B - 4 * A * C;
    if (disc < 0)
        return -1;
    const float t = (-B - sqrtf(disc)) / (2 * A);
    if (t < 0 || t >= 0.97f)
        return -1;
    const float seg[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
    return t * geom::Len(seg);
}

bool Inside(const Ellipsoid& e, const float w[3])
{
    float l[3];
    ToLocal(e, w, l);
    return (l[0] / e.s[0]) * (l[0] / e.s[0]) + (l[1] / e.s[1]) * (l[1] / e.s[1]) + (l[2] / e.s[2]) * (l[2] / e.s[2]) <= 1;
}

// Turn `h` (unit) toward `to` (unit) by at most `maxRad`.
void TurnToward(float h[3], const float to[3], float maxRad)
{
    const float c = Clampf(geom::Dot(h, to), -1, 1);
    const float ang = acosf(c);
    if (ang <= maxRad || ang < 1e-4f)
    {
        memcpy(h, to, 3 * sizeof(float));
        return;
    }
    float perp[3] = {to[0] - h[0] * c, to[1] - h[1] * c, to[2] - h[2] * c};
    if (geom::Len(perp) < 1e-5f)
        perp[0] = h[1], perp[1] = -h[0], perp[2] = 0;
    geom::Norm(perp);
    for (int i = 0; i < 3; ++i)
        h[i] = h[i] * cosf(maxRad) + perp[i] * sinf(maxRad);
    geom::Norm(h);
}
} // namespace

void Test_strafe()
{
    using namespace strafe;
    const float kId[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    Ellipsoid bs = {{1000, 50, -2000}, {1, 0, 0, 0, 1, 0, 0, 0, 1}, {140, 120, 630}};
    const auto ray = [&](const float* a, const float* b) { return RayEllipsoid(bs, a, b); };

    // --- the hull by rays: the long axis, its extent, the width profile
    Hull h = Measure(bs.c, bs.rot, 650, ray);
    CHECK(h.ok);
    CHECK(h.axis == 2);
    CHECK_NEAR(h.lo, -630, 3);
    CHECK_NEAR(h.hi, 630, 3);
    CHECK_NEAR(h.len, 1260, 6);
    CHECK_NEAR(h.aspect, 1260.0f / 280.0f, 0.1);
    CHECK_NEAR(h.width[0][kStations / 2], 140, 2); // the middle station, +x
    CHECK_NEAR(h.width[1][kStations / 2], 140, 2); // -x
    CHECK_NEAR(h.width[2][kStations / 2], 120, 2); // +y
    CHECK_NEAR(h.widthMax[3], 120, 2);
    CHECK(h.width[0][0] < 1 && h.width[0][kStations - 1] < 1); // the tips
    // ...the same hull turned 90 deg about y (its long axis along world x): the same frame-local answer
    {
        Ellipsoid t = bs;
        const float r[9] = {0, 0, 1, 0, 1, 0, -1, 0, 0}; // local x -> world -z, y -> y, z -> world x
        memcpy(t.rot, r, sizeof(r));
        const auto rayT = [&](const float* a, const float* b) { return RayEllipsoid(t, a, b); };
        const Hull ht = Measure(t.c, t.rot, 650, rayT);
        CHECK(ht.ok && ht.axis == 2);
        CHECK_NEAR(ht.len, h.len, 6);
        CHECK_NEAR(ht.width[0][kStations / 2], 140, 2);
    }
    // ...fail closed: no ray hit (a ship's hull: no solid to the game's test), a sphere, a short hull
    {
        const Hull none = Measure(bs.c, bs.rot, 650, [](const float*, const float*) { return -1.0f; });
        CHECK(!none.ok && strcmp(none.why, "no hull") == 0);
        Ellipsoid sph = {{0, 0, 0}, {1, 0, 0, 0, 1, 0, 0, 0, 1}, {400, 380, 420}};
        const Hull hs = Measure(sph.c, sph.rot, 450, [&](const float* a, const float* b) { return RayEllipsoid(sph, a, b); });
        CHECK(!hs.ok && strcmp(hs.why, "not elongated") == 0);
        Ellipsoid sh = {{0, 0, 0}, {1, 0, 0, 0, 1, 0, 0, 0, 1}, {60, 50, 250}};
        const Hull hh = Measure(sh.c, sh.rot, 260, [&](const float* a, const float* b) { return RayEllipsoid(sh, a, b); });
        CHECK(!hh.ok && strcmp(hh.why, "short") == 0);
    }

    // --- the pass line, the turn stations
    float lp[3];
    LinePoint(h, 0, 0, kGapWant, lp);
    CHECK_NEAR(lp[0], 140 + kGapWant, 3);
    CHECK_NEAR(lp[2], 0, 1e-3);
    LinePoint(h, 3, 0, kGapWant, lp);
    CHECK_NEAR(lp[1], -(120 + kGapWant), 3);
    CHECK_NEAR(TurnStation(h, 1), 630 + kTurnBeyond, 4); // 1,030: within the runners' 1-2 km
    CHECK_NEAR(TurnStation(h, -1), -(630 + kTurnBeyond), 4);
    {
        Hull longH = h;
        longH.hi = 1800, longH.lo = -1800;
        CHECK_NEAR(TurnStation(longH, 1), 1800 + kTurnBeyond, 1e-3); // a longer hull: still 400 m past its end
        Hull shortH = h;
        shortH.hi = 320, shortH.lo = -320;
        CHECK_NEAR(TurnStation(shortH, -1), -kTurnMin, 1e-3);
    }
    {
        const float behind[3] = {0, 0, -1500};
        CHECK(StartDir(h, behind) == 1);
        const float ahead[3] = {0, 0, 200};
        CHECK(StartDir(h, ahead) == -1);
        int order[4];
        const float below[3] = {30, -400, 0};
        SideOrder(h, below, order);
        CHECK(order[0] == 3);             // -y faces us
        CHECK(order[3] == 2);             // +y the farthest
    }

    // --- the aim point: from beyond the near end, 200 m off the axis, the hull ahead 22 deg off the pass direction
    {
        const float p[3] = {200, 0, -1000};
        float s[3];
        CHECK(AimPoint(h, 0, 1, p, 200, 0, s));
        CHECK_NEAR(s[2], -1000 + 200 / tanf(kAimDeg * 3.14159265f / 180), 1);
        CHECK_NEAR(s[0], WidthRaw(h, 0, s[2]), 1e-3);
        const float to[3] = {s[0] - p[0], s[1] - p[1], s[2] - p[2]};
        CHECK(to[2] / geom::Len(to) > cosf(kAimMaxDeg * 3.14159265f / 180));
        CHECK(!AimPoint(h, 0, 1, p, 200, 300, s)); // beyond the reach given
        const float nearEnd[3] = {300, 0, 420};    // the far end close: no aim
        CHECK(!AimPoint(h, 0, 1, nearEnd, 200, 0, s));
        const float wrongWay[3] = {300, 0, 0};
        CHECK(AimPoint(h, 0, -1, wrongWay, 160, 0, s) && s[2] < 0); // the other direction aims the other way
    }

    // --- the speed schedule, the slide, the side choice
    CHECK(Burn(0.5f, 150));
    CHECK(!Burn(kBurnOn + 0.2f, 150));
    CHECK(Burn(kBurnOn + kBurnOff + 0.1f, 150)); // the next pulse
    CHECK(Burn(kBurnOn + 0.2f, 30));             // under twice the floor: on
    CHECK(Lateral(140, true, true) == 1);
    CHECK(Lateral(170, true, true) == 0);  // the nose on the hull: out only under 165
    CHECK(Lateral(170, true, false) == 1); // flying the line: out to 185
    CHECK(Lateral(200, true, false) == 0);
    CHECK(Lateral(240, true, true) == -1);
    CHECK(Lateral(240, false, false) == 0); // in only by a live gap
    // the profile gap: alongside the widest part the perpendicular; beyond the bow the distance to its tip; inside 0
    CHECK_NEAR(ProfileGap(h, 0, 0, 340), 200, 3);
    CHECK_NEAR(ProfileGap(h, 0, -1030, 0), 400, 3);
    CHECK(ProfileGap(h, 0, 0, 100) == 0);
    CHECK_NEAR(ProfileGap(h, 0, -900, 200), 336, 3); // off the bow: its tip (270 ahead, 200 across), not the axis' 200
    CHECK_NEAR(ProfileGap(h, 1, -900, -200), 336, 3); // the other side alike
    {
        const float r[3] = {1, 0, 0}, l[3] = {-1, 0, 0}, u[3] = {0, 1, 0}, dn[3] = {0, -1, 0}, fw[3] = {0, 0, -1};
        CHECK(SlideCode(kId, r) == 2);
        CHECK(SlideCode(kId, l) == 1);
        CHECK(SlideCode(kId, u) == 3);
        CHECK(SlideCode(kId, dn) == 4);
        CHECK(SlideCode(kId, fw) == 0);
        // the ship rolled 90 deg about its nose (its right = world up): the world up is its right
        const float rolled[9] = {0, -1, 0, 1, 0, 0, 0, 0, 1};
        CHECK(SlideCode(rolled, u) == 2);
    }
    {
        const int order[4] = {1, 0, 2, 3};
        CHECK(PickSide(order, 1, [](int s, int) { return s != 1; }) == 0);
        CHECK(PickSide(order, 1, [](int, int) { return false; }) == -1);
    }

    // --- the state machine: a pass that finds every line blocked at its turn falls back; a live gap under the abort too
    {
        StepIn in;
        in.phase = Phase::Pass, in.side = 0, in.dir = 1;
        in.pos[0] = 200, in.pos[2] = 1100; // past the turn station
        in.vel[2] = 150;
        StepOut o = Step(h, in, [](int, int) { return false; });
        CHECK(o.fallback && strcmp(o.fallback, "geometry") == 0);
        o = Step(h, in, [](int s, int) { return s == 0; }); // only the same side clear: back on it
        CHECK(!o.fallback && o.turned && o.phase == Phase::Turn && o.side == 0 && o.dir == -1);
        o = Step(h, in, [](int, int) { return true; }); // all clear: the far side first
        CHECK(o.turned && o.side == 1);
        in.pos[2] = 0, in.pos[0] = 140 + 60, in.gapNow = 60;
        o = Step(h, in, [](int, int) { return true; });
        CHECK(o.fallback && strcmp(o.fallback, "geometry") == 0);
        in.gapNow = 200, in.pos[0] = 340, in.phaseTime = kPassTimeout + 1;
        o = Step(h, in, [](int, int) { return true; });
        CHECK(o.fallback && strcmp(o.fallback, "pass") == 0);
    }

    // --- a crude flight: from 1.6 km off the bow, 80 s. The nose turns at 1.5 rad/s, the speed follows the afterburner
    // schedule with tau 0.17 s (200 / 80 m/s), the slide adds 50 m/s across; the live gap is the ray to our station's
    // axis point. Expected: no fallback, both ends turned more than once, the gap in a pass never under kGapAbort, the
    // hull never entered, and at every aimed moment our launch rules' line gaps hold (the ray to the centre and the
    // nose's 400 m ray: none or >= 100 m - torp_reach).
    {
        float pos[3] = {450, 0, -1700}, nose[3] = {0, 0, 1}, vel[3] = {0, 0, 150};
        float speed = 150, phaseTime = 0, pulse = 0;
        Phase phase = Phase::Join;
        int order[4];
        SideOrder(h, pos, order);
        int side = order[0], dir = StartDir(h, pos);
        int turns = 0, turnsHi = 0, turnsLo = 0, aimed = 0, passFrames = 0, inside = 0, reachBad = 0;
        float minGap = 1e9f, minSpeed = 1e9f, maxSpeed = 0;
        const char* fb = nullptr;
        bool wasAimed = false;
        const float dt = 0.05f;
        for (int f = 0; f < static_cast<int>(80 / dt) && !fb; ++f)
        {
            // (world = the ellipsoid's frame shifted by its centre: the test's rot is the identity)
            float w[3] = {pos[0] + bs.c[0], pos[1] + bs.c[1], pos[2] + bs.c[2]};
            float foot[3] = {bs.c[0], bs.c[1], bs.c[2] + pos[2]};
            StepIn in;
            in.phase = phase, in.side = side, in.dir = dir;
            memcpy(in.pos, pos, sizeof(pos));
            memcpy(in.vel, vel, sizeof(vel));
            in.gapNow = RayEllipsoid(bs, w, foot);
            in.phaseTime = phaseTime;
            in.reach = 900;
            in.aimed = wasAimed;
            const StepOut o = Step(h, in, [](int, int) { return true; });
            wasAimed = o.aim;
            if (o.fallback)
            {
                fb = o.fallback;
                break;
            }
            if (o.turned)
            {
                ++turns;
                (o.turnAt > 0 ? turnsHi : turnsLo) += 1;
            }
            if (o.passStarted)
                pulse = 0;
            phaseTime = o.phase == phase ? phaseTime + dt : 0;
            phase = o.phase, side = o.side, dir = o.dir;
            if (phase == Phase::Pass)
            {
                ++passFrames;
                if (in.gapNow >= 0)
                    minGap = std::min(minGap, in.gapNow);
                minSpeed = std::min(minSpeed, speed), maxSpeed = std::max(maxSpeed, speed);
            }
            if (o.aim)
            {
                ++aimed;
                const float centre[3] = {bs.c[0], bs.c[1], bs.c[2]};
                const float line = RayEllipsoid(bs, w, centre);
                float nd[3] = {o.steer[0] - pos[0], o.steer[1] - pos[1], o.steer[2] - pos[2]};
                geom::Norm(nd);
                const float noseEnd[3] = {w[0] + nd[0] * 400, w[1] + nd[1] * 400, w[2] + nd[2] * 400};
                const float noseHit = RayEllipsoid(bs, w, noseEnd);
                if (line < 100 || (noseHit >= 0 && noseHit < 100))
                    ++reachBad;
            }
            // the ship: the nose toward the steer point, the speed by the schedule, the slide across
            float to[3] = {o.steer[0] - pos[0], o.steer[1] - pos[1], o.steer[2] - pos[2]};
            if (geom::Len(to) > 1)
            {
                geom::Norm(to);
                TurnToward(nose, to, 1.5f * dt);
            }
            pulse += dt;
            const float top = Burn(pulse, speed) ? 200.0f : 80.0f;
            speed += (top - speed) * std::min(1.0f, dt / 0.17f);
            float d[3];
            SideDir(h, side, d);
            for (int i = 0; i < 3; ++i)
                vel[i] = nose[i] * speed + d[i] * 50.0f * o.lateral, pos[i] += vel[i] * dt;
            const float wn[3] = {pos[0] + bs.c[0], pos[1] + bs.c[1], pos[2] + bs.c[2]};
            inside += Inside(bs, wn) ? 1 : 0;
        }
        if (fb)
            fprintf(stderr, "strafe flight: fallback %s\n", fb);
        CHECK(fb == nullptr);
        CHECK(turns >= 4);
        CHECK(turnsHi >= 2 && turnsLo >= 2);
        CHECK(inside == 0);
        CHECK(minGap >= kGapAbort);
        CHECK(minGap >= 100.0f);
        CHECK(reachBad == 0);
        CHECK(aimed > passFrames / 4); // the nose on the hull for a good share of every pass
        CHECK(minSpeed >= kFloor && maxSpeed - minSpeed >= 80.0f);
        if (g_test.verbose)
            printf("strafe flight: turns %d (%d hi, %d lo), aimed %d of %d pass frames, min gap %.0f, speed %.0f-%.0f\n",
                   turns, turnsHi, turnsLo, aimed, passFrames, minGap, minSpeed, maxSpeed);
    }
}
