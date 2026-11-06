// `ash_ride`'s pure half (modules\autopilot\src\ash_ride.h; docs\research\runners\M02_ashcroft_datapass.md "The ONE
// proposal", adapted to the HDE P0 fact and the owner's correction of 31 Oct 2026: a coast is a straight line, cruise
// lost): the leave rule and its fallback, the loop's radius (the measured turn rate) and geometry inside the leashes, the
// carrot, his / our charge profiles, the intercept verdicts, the coast gate - then a closed-loop run of M02's geometry
// (Ashcroft's spawn and run point, King's 5 km nag): the loop flown at cruise, his ramp by the profile, our steering
// turn-rate limited like the measured cruise turns - where the gap stands when the gate lets the coast start.
#include "test.h"

#include "../modules/autopilot/src/ash_ride.h"

#include <cstring>

namespace
{
void Set3(float* p, float x, float y, float z) { p[0] = x, p[1] = y, p[2] = z; }

lookahead::StageLeash L(const char* name, float x, float z, float radius, float frac)
{
    lookahead::StageLeash l;
    l.name = name;
    Set3(l.anchor, x, 0, z);
    l.radius = radius, l.frac = frac;
    return l;
}

float Dist(const float* a, const float* b)
{
    const float d[3] = {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
    return std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
}

float Clampf_(float x) { return x < -1 ? -1 : x > 1 ? 1 : x; }

struct SimResult
{
    bool coast = false;
    float coastT = 0, coastGap = 0, minGap = 1e9f;
    bool overrun = false;
};

// One run: we on the loop at angle a0 going `dir`, at cruise; he spawns at t = 0 and runs by the profile. Our nose turns
// at most `rate` rad/s (full rate beyond 0.35 rad off, proportional below: the controller's SteerTo), the velocity follows
// it (a cruise turn). After the coast's start the run ends (the coast is a straight line).
SimResult Sim(const ashride::Loop& lp, const float spawn[3], float a0, int dir, float rate, float turnR, bool verbose)
{
    using namespace ashride;
    SimResult s;
    float me[3] = {lp.c[0] + lp.r * std::cos(a0), 0, lp.c[2] + lp.r * std::sin(a0)};
    // heading along the circle in `dir`
    float psi = std::atan2(std::cos(a0) * dir, -std::sin(a0) * dir); // (atan2(z, x) of the tangent)
    const float v = 300;
    bool committed = false;
    CoastGate gate;
    Verdict last = Verdict::Wait;
    for (float t = 0; t <= 20.0f; t += 0.05f)
    {
        const float hisD = lookahead::ChargeDist(lookahead::kHisCharge, t);
        const float hisV = t < lookahead::kHisCharge.slowS ? lookahead::kHisCharge.slow
                           : t < lookahead::kHisCharge.topAtS
                               ? lookahead::kHisCharge.slow + (lookahead::kHisCharge.top - lookahead::kHisCharge.slow) *
                                                                  (t - lookahead::kHisCharge.slowS) /
                                                                  (lookahead::kHisCharge.topAtS - lookahead::kHisCharge.slowS)
                               : lookahead::kHisCharge.top;
        float his[3];
        for (int i = 0; i < 3; ++i)
            his[i] = spawn[i] + lp.u[i] * hisD;
        const float vel[3] = {v * std::cos(psi), 0, v * std::sin(psi)};
        InterceptFacts f;
        memcpy(f.me, me, sizeof(f.me));
        memcpy(f.vel, vel, sizeof(f.vel));
        f.mySpeed = v, f.cruising = true, f.chargeLeftS = 0;
        memcpy(f.his, his, sizeof(f.his));
        f.hisSpeed = hisV;
        memcpy(f.u, lp.u, sizeof(f.u));
        f.tSince = t, f.turnR = turnR, f.committed = committed;
        const InterceptResult r = Intercept(f);
        if (r.v != Verdict::Wait)
            committed = true;
        if (r.v == Verdict::Overrun && t > 0)
            s.overrun = true;
        if (verbose && r.v != last)
            printf("    t=%.2f %s along=%.0f lat=%.0f path=%.0f need=%.0f t_ready=%.1f\n", t, VerdictName(r.v), r.along,
                   r.lat, r.path, r.need, r.tReady);
        last = r.v;
        float target[3];
        if (r.v == Verdict::Wait)
            LoopCarrot(me, lp, dir, v, target);
        else
            memcpy(target, r.steer, sizeof(target));
        const float gap = Dist(me, his);
        s.minGap = std::min(s.minGap, gap);
        // the coast gate (both velocities known)
        const float toHim[3] = {his[0] - me[0], 0, his[2] - me[2]};
        const float hisVel[3] = {lp.u[0] * hisV, 0, lp.u[2] * hisV};
        CoastFacts cf;
        cf.now = t, cf.gap = gap, cf.his = hisV, cf.me = v, cf.cruising = true;
        cf.velDeg = AngleDeg(vel, toHim), cf.parallelDeg = AngleDeg(vel, hisVel);
        if (CoastReady(gate, cf))
        {
            s.coast = true, s.coastT = t, s.coastGap = gap;
            return s;
        }
        // steer: the heading error, the turn limited
        const float want = std::atan2(target[2] - me[2], target[0] - me[0]);
        float err = want - psi;
        while (err > 3.14159265f)
            err -= 6.2831853f;
        while (err < -3.14159265f)
            err += 6.2831853f;
        const float w = Clampf_(err / (r.v == Verdict::Wait ? kLoopFullAngle : kSteerFullAngle)) * rate;
        psi += w * 0.05f;
        me[0] += v * std::cos(psi) * 0.05f, me[2] += v * std::sin(psi) * 0.05f;
    }
    return s;
}
} // namespace

void Test_ash_ride()
{
    using namespace ashride;

    // ---------------------------------------------------------------- 1. the leave rule and its fallback
    {
        CHECK(Leave(0, 0.59f));          // R2's bars at its press
        CHECK(Leave(0.005f, 0.36f));     // R1's
        CHECK(!Leave(0, 0.61f));         // hull too high: fight on
        CHECK(!Leave(0.2f, 0.3f));       // the shield still up
        CHECK(!Leave(-1, 0.3f));         // shield unknown: fail closed
        CHECK(!Leave(0, -1));            // hull unknown
        CHECK(!Leave(0, 0));             // dead
        const float me[3] = {0, 0, 0}, still[3] = {0, 0, 0};
        CHECK(!LeftFallback(39, true, me, still, {}).fall); // (R3's escort lived 38.3 s: no turn-back yet)
        FallbackResult fb = LeftFallback(46, true, me, still, {});
        CHECK(fb.fall && fb.why == "45 s");
        CHECK(!LeftFallback(60, false, me, still, {}).fall); // dead: the step moves on
        // King's nag 5 km at 0.8: beyond 4.2 km and moving away -> back; moving toward him -> stay
        const std::vector<lookahead::StageLeash> ls = {L("nag_battle3", 0, -4300, 5000, 0.8f)};
        const float away[3] = {0, 0, 100}, toward[3] = {0, 0, -100};
        fb = LeftFallback(5, true, me, away, ls);
        CHECK(fb.fall && fb.why.compare(0, 17, "leash nag_battle3") == 0);
        CHECK(!LeftFallback(5, true, me, toward, ls).fall);
        const float near[3] = {0, 0, -1000};
        CHECK(!LeftFallback(5, true, near, away, ls).fall);
    }

    // ---------------------------------------------------------------- 2. the loop's radius and geometry
    {
        // the Starflier: 1.667 rad/s (flight.h) x 0.74 (the measured velocity turn) -> 243 m at 300 m/s, the loop 486 m
        const float tr = CruiseTurnRadius(300, 1.667f);
        CHECK_NEAR(tr, 243.2f, 1.0f);
        CHECK_NEAR(LoopRadius(tr), 486.4f, 2.0f);
        CHECK_NEAR(CruiseTurnRadius(300, 0), 300 / kTurnMeasured, 0.5f); // no model: the measured top
        CHECK(LoopRadius(50) == kLoopMinR && LoopRadius(900) == kLoopMaxR);
        if (g_test.verbose)
            printf("ash-ride loop radius: turn radius %.0f m -> loop %.0f m, one lap %.1f s\n", tr, LoopRadius(tr),
                   2 * 3.14159f * LoopRadius(tr) / 300);

        const float spawn[3] = {0, 0, 0}, run[3] = {0, 0, 10000};
        Loop l = PlanLoop(spawn, run, 486, {});
        CHECK(l.ok && l.why == "free" && !l.clamped);
        CHECK_NEAR(l.back, 686, 1);                    // r + 200 m clear of the spawn
        CHECK_NEAR(l.c[2], -686, 1);                   // behind the spawn on his line
        CHECK(l.farthest <= kMaxFromSpawn && l.dSpawn - l.r >= kSpawnClear - 1);
        // a nag pulling the centre toward its anchor: the whole circle inside 0.8 x 5000
        std::vector<lookahead::StageLeash> ls = {L("nag", 2500, -2500, 5000, 0.8f)};
        l = PlanLoop(spawn, run, 486, ls);
        CHECK(l.ok);
        CHECK(Dist(l.c, ls[0].anchor) + l.r <= 4001);
        // a leash tighter than the loop, one far away, one that pulls it beyond 2 km of the spawn
        CHECK(PlanLoop(spawn, run, 486, {L("tight", 0, -500, 500, 0.8f)}).why == "leash tight tighter than the loop");
        Loop far = PlanLoop(spawn, run, 486, {L("away", 0, -6000, 5000, 0.8f)});
        CHECK(!far.ok && far.why.compare(0, 4, "far ") == 0);
        CHECK(!PlanLoop(spawn, run, 486, {L("a", -9000, 0, 3000, 0.8f), L("b", 9000, 0, 3000, 0.8f)}).ok);
        CHECK(PlanLoop(spawn, spawn, 486, {}).why == "no-line");

        // the way round and the carrot
        const float c[3] = {0, 0, 0};
        const float me[3] = {500, 0, 0}, ccw[3] = {0, 0, 300}, cw[3] = {0, 0, -300};
        CHECK(LoopDir(me, ccw, c) == 1 && LoopDir(me, cw, c) == -1);
        Loop lc;
        lc.ok = true, lc.r = 500;
        float car[3];
        LoopCarrot(me, lc, 1, 300, car); // on the circle at angle 0: the lead 0.6 rad on
        CHECK_NEAR(std::atan2(car[2], car[0]), 0.6f, 0.01f);
        CHECK_NEAR(Dist(car, c), 500, 1);
        const float out[3] = {5000, 0, 0};
        LoopCarrot(out, lc, 1, 300, car); // far outside: the tangent point (acos(0.1)) + the lead
        CHECK_NEAR(std::atan2(car[2], car[0]), std::acos(0.1f) + 0.6f, 0.01f);
    }

    // ---------------------------------------------------------------- 3. the profiles and the intercept
    {
        // his: the whole charge from his spawn = lookahead::ChargeDist at 9.5 s
        CHECK_NEAR(HisTimeToTop(0, 0), 9.5f, 0.01f);
        CHECK_NEAR(HisAdvance(0, 0, 9.5f), lookahead::ChargeDist(lookahead::kHisCharge, 9.5f), 1.0f);
        CHECK_NEAR(HisAdvance(75, 3, HisTimeToTop(75, 3)),
                   lookahead::ChargeDist(lookahead::kHisCharge, 9.5f) - lookahead::ChargeDist(lookahead::kHisCharge, 3), 1.0f);
        CHECK_NEAR(HisTimeToTop(187.5f, 7.5f), 2.0f, 0.01f); // mid-ramp: by his speed
        CHECK(HisTimeToTop(296, 11) == 0);
        CHECK_NEAR(HisAdvance(300, 0, 2), 600, 0.1f);
        // ours: cruising 300 x T; a fresh charge (the owner's 8.5 s: 5 s at 80, the ramp at 60 m/s^2) = kOurCharge
        CHECK_NEAR(OurAdvance(300, 0, 4), 1200, 0.1f);
        CHECK_NEAR(OurAdvance(80, 5, 10), lookahead::ChargeDist(lookahead::kOurCharge, 10), 25.0f);
        CHECK(OurAdvance(80, 5, 5) == 400);
        CHECK_NEAR(TurnLen(3.14159265f, 200), 628.3f, 0.5f); // a U-turn costs pi R
        CHECK(TurnLen(0.1f, 200) < 0.05f);

        using V = Verdict;
        const float u[3] = {0, 0, 1};
        InterceptFacts f;
        memcpy(f.u, u, sizeof(f.u));
        f.mySpeed = 300, f.cruising = true, f.turnR = 243;
        // at his spawn, 700 m behind it heading along his line: he needs 9.5 s (we fly 2850 m), our way ~1700 m - wait
        Set3(f.me, 0, 0, -700), Set3(f.vel, 0, 0, 300), Set3(f.his, 0, 0, 0), f.hisSpeed = 0, f.tSince = 0;
        InterceptResult r = Intercept(f);
        CHECK(r.v == V::Wait && r.path < r.need);
        CHECK_NEAR(r.station[2], HisAdvance(0, 0, 9.5f) - kGap, 1);
        // 2.2 km behind it: time to go
        Set3(f.me, 0, 0, -2200);
        r = Intercept(f);
        CHECK(r.v == V::Go && r.steer[2] > -2200);
        // committed: no wait any more
        Set3(f.me, 0, 0, -700), f.committed = true;
        CHECK(Intercept(f).v == V::Go);
        // not cruising: the charge counted - from 700 m behind, a fresh charge cannot be early (2.85 km -> ~1.5 km)
        f.committed = false, f.cruising = false, f.mySpeed = 80, f.chargeLeftS = 5;
        r = Intercept(f);
        CHECK(r.v == V::Go && r.need < 1700);
        f.cruising = true, f.mySpeed = 300, f.chargeLeftS = 0;
        // beside him while he ramps: pass and turn (the station behind him)
        Set3(f.me, 100, 0, 300), Set3(f.his, 0, 0, 300), f.hisSpeed = 120, f.tSince = 6;
        r = Intercept(f);
        CHECK(r.v == V::Overrun && r.steer[2] < 300);
        // he at cruise, we 150 m behind on his line: track him
        Set3(f.me, 0, 0, 1850), Set3(f.his, 0, 0, 2000), f.hisSpeed = 299, f.tSince = 11;
        r = Intercept(f);
        CHECK(r.v == V::Track && r.steer[2] == 2000);
        // ...ahead of him: overrun; far behind: go (late)
        Set3(f.me, 0, 0, 2100);
        CHECK(Intercept(f).v == V::Overrun);
        Set3(f.me, 0, 0, 900);
        r = Intercept(f);
        CHECK(r.v == V::Go && std::string(r.why) == "late");
    }

    // ---------------------------------------------------------------- 4. the coast gate
    {
        CoastGate g;
        CoastFacts f;
        f.gap = 150, f.his = 298, f.me = 299, f.cruising = true, f.velDeg = 2, f.parallelDeg = 1;
        float held = 0;
        f.now = 0;
        CHECK(!CoastReady(g, f, &held));
        f.now = 0.5f, f.gap = 155;
        CHECK(!CoastReady(g, f, &held) && held > 0.49f);
        f.now = 1.05f, f.gap = 158;
        CHECK(CoastReady(g, f, &held)); // 1 s within 10 m
        // the gap moved by more than the band: it settles anew
        CoastGate g2;
        f.now = 0, f.gap = 200;
        CoastReady(g2, f);
        f.now = 0.6f, f.gap = 185;
        CHECK(!CoastReady(g2, f));
        f.now = 1.2f, f.gap = 183;
        CHECK(!CoastReady(g2, f));
        f.now = 1.7f, f.gap = 180;
        CHECK(CoastReady(g2, f));
        // every bar: his speed, ours, the window, the velocity line, parallel courses, the shield, cruise
        const auto once = [](CoastFacts x)
        {
            CoastGate gg;
            x.now = 0;
            CoastReady(gg, x);
            x.now = 2;
            return CoastReady(gg, x);
        };
        CoastFacts b = f;
        b.gap = 150;
        CHECK(once(b));
        CoastFacts x = b;
        x.his = 280;
        CHECK(!once(x));
        x = b, x.me = 270;
        CHECK(!once(x));
        x = b, x.gap = 80;
        CHECK(!once(x));
        x = b, x.gap = 230;
        CHECK(!once(x));
        x = b, x.velDeg = 10; // the nose on him is not enough: the coast goes where we travel
        CHECK(!once(x));
        x = b, x.parallelDeg = 12;
        CHECK(!once(x));
        x = b, x.shieldOut = true;
        CHECK(!once(x));
        x = b, x.cruising = false;
        CHECK(!once(x));
        const float a[3] = {1, 0, 0}, bb[3] = {0, 0, 1}, z[3] = {0, 0, 0};
        CHECK_NEAR(AngleDeg(a, bb), 90, 0.01f);
        CHECK(AngleDeg(a, z) == 180);
    }

    // ---------------------------------------------------------------- M02: the loop and a closed-loop intercept
    {
        // m02.ini: Ashcroft spawns at -28500, 0, 51100, runs for -22099, 0, 75896; nag_battle3 = King 5 km (0.8), King
        // where as2-as4 met him (-29951, 46688: 4.6 km from the spawn, almost straight behind it on his line)
        const float ash[3] = {-28500, 0, 51100}, run[3] = {-22099, 0, 75896};
        const float r = LoopRadius(CruiseTurnRadius(300, 1.667f));
        std::vector<lookahead::StageLeash> ls = {L("nag_battle3", -29951, 46688, 5000, lookahead::kChargeNagFrac)};
        const Loop lp = PlanLoop(ash, run, r, ls);
        CHECK(lp.ok && lp.farthest <= kMaxFromSpawn);
        CHECK(Dist(lp.c, ls[0].anchor) + lp.r <= 4001);
        if (g_test.verbose)
            printf("ash-ride m02 loop: centre %.0f,%.0f r=%.0f d_spawn=%.0f farthest=%.0f %s bound=%s\n", lp.c[0],
                   lp.c[2], lp.r, lp.dSpawn, lp.farthest, lp.why.c_str(), lp.bound.c_str());
        // + the supervisor's 3 km leash on King at 0.75: no loop (as spawn_charge's point: "far")
        ls.push_back(L("ship1", -29951, 46688, 3000, lookahead::kChargeLeashFrac));
        CHECK(!PlanLoop(ash, run, r, ls).ok);

        // the closed loop: 16 start angles x both ways round; our velocity turns at the measured top (1.23 rad/s: the way
        // estimate's radius, 243 m) and - the sensitivity - at 1.1 rad/s: a slower turn than counted arrives late
        for (const float rate : {1.23f, 1.1f})
        {
            int coasts = 0, inWindow = 0, runs = 0, rams = 0;
            float worstT = 0;
            for (int dir = -1; dir <= 1; dir += 2)
                for (int k = 0; k < 16; ++k)
                {
                    ++runs;
                    const float a0 = 6.2831853f * k / 16;
                    const SimResult s = Sim(lp, ash, a0, dir, rate, CruiseTurnRadius(300, 1.667f), false);
                    coasts += s.coast ? 1 : 0;
                    inWindow += s.coast && s.coastGap >= kCoastMin && s.coastGap <= kCoastMax ? 1 : 0;
                    rams += s.minGap < 40 ? 1 : 0;
                    if (s.coast)
                        worstT = std::max(worstT, s.coastT);
                    if (g_test.verbose)
                        printf("  m02 sim rate=%.2f dir=%+d a0=%5.2f: coast=%d t=%5.2f gap=%4.0f min_gap=%4.0f "
                               "overrun=%d\n",
                               rate, dir, a0, s.coast ? 1 : 0, s.coastT, s.coastGap, s.minGap, s.overrun ? 1 : 0);
                }
            if (g_test.verbose)
                printf("ash-ride m02 sim rate=%.2f: %d/%d coasts (%d in the window), latest at %.1f s, %d rams\n", rate,
                       coasts, runs, inWindow, worstT, rams);
            // at the measured top >= 28 of 32 start phases reach the coast; at 1.1 rad/s at least half
            CHECK(rate > 1.2f ? coasts >= 28 : coasts * 2 >= runs);
            CHECK(inWindow == coasts);
            CHECK(rams == 0);
            CHECK(worstT <= 16.0f); // runners: the eject at +18..+28 s after the banner - the coast must start well before
        }
    }
}
