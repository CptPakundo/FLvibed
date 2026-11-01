// `spawn_charge`'s pure half (controller\src\impl\lookahead.h, docs\research\runners\M02_ashcroft_consensus.md "The ONE
// design"): the stage point over a list of leashes (250 m behind the spawn on his line, 70 m aside, inside every ball;
// the side that needs the least pull; the refusals "no-point-in-leashes" and "far"), StillFleeing, the charge profiles
// and the gap predictor ChargeGap, ChaseStep's `fleeing` (the charge at his spawn whatever his speed, the coast at cruise
// speed), and the keep-station rule of the fight at the point (ChargeKeep). Then M02's geometry: Ashcroft's spawn and run
// point (m02.ini, tests\look_test.cpp) with King's 5 km nag and the 3 km leash the supervisor holds on him.
#include "test.h"

#include "../controller/src/impl/lookahead.h"

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
} // namespace

void Test_spawn_charge()
{
    using namespace lookahead;
    const float spawn[3] = {0, 0, 0}, to[3] = {0, 0, 10000};
    std::string bound;

    // ---------------------------------------------------------------- the stage point over the leashes
    {
        // no leash: 250 m behind on his line, 70 m aside, nose along u
        StageResult s = StagePoint(spawn, to, std::vector<StageLeash>(), &bound);
        CHECK(s.ok && s.why == "free" && !s.clamped && s.dAnchor < 0 && bound.empty());
        CHECK_NEAR(s.p[2], -250, 0.01);
        CHECK_NEAR(std::fabs(s.p[0]), 70, 0.01);
        CHECK_NEAR(s.dSpawn, std::sqrt(250.0f * 250.0f + 70.0f * 70.0f), 0.05);
        CHECK_NEAR(s.u[2], 1, 1e-4);
        // one ship leash with room: free, on its anchor's side
        std::vector<StageLeash> ls = {L("king", -500, -2400, 3000, kChargeLeashFrac)};
        s = StagePoint(spawn, to, ls, &bound);
        CHECK(s.ok && s.why == "free" && s.p[0] < 0 && bound.empty());
        CHECK(s.dAnchor > 0 && s.dAnchor <= 0.75f * 3000 + 0.5f);
        // ...tighter (2,100 m): pulled toward it, inside, the bound leash named
        ls[0].radius = 2800;
        s = StagePoint(spawn, to, ls, &bound);
        CHECK(s.ok && s.why == "clamped" && s.clamped && bound == "king");
        CHECK(Dist(s.p, ls[0].anchor) <= 0.75f * 2800 + 1.0f);
        CHECK(s.dSpawn < kChargeMaxFromSpawn);
        // two balls (1,530 m each, 3 km apart across his line: x in -30..30 on it): both sides pulled 40 m - the tie goes
        // to the first leash's side (-x), the point at x = -30, inside both
        ls = {L("a", -1500, -250, 2040, 0.75f), L("b", 1500, -250, 2040, 0.75f)};
        s = StagePoint(spawn, to, ls, &bound);
        CHECK(s.ok && s.clamped && bound == "b");
        CHECK_NEAR(s.p[0], -30, 1.5);
        CHECK_NEAR(s.p[2], -250, 1.5);
        CHECK(Dist(s.p, ls[0].anchor) <= 1531 && Dist(s.p, ls[1].anchor) <= 1531);
        // the side that needs the least pull wins over the tightest anchor's side: the tight leash (lim 2,925 m: x <= 25
        // on the line) on -x, a wider one (lim 3,000 m: x >= 0) on +x - the -x point is pulled 70 m, the +x one 45 m
        ls = {L("tight", -2900, -250, 3900, 0.75f), L("wide", 3000, -250, 3750, 0.8f)};
        s = StagePoint(spawn, to, ls, &bound);
        CHECK(s.ok && s.p[0] > 0 && bound == "tight");
        CHECK_NEAR(s.p[0], 25, 1.5);
        // disjoint balls: no point
        ls = {L("west", -3000, 0, 1000, 0.75f), L("east", 3000, 0, 1000, 0.75f)};
        s = StagePoint(spawn, to, ls, &bound);
        CHECK(!s.ok && s.why == "no-point-in-leashes");
        // the only point inside lies ~1 km from the spawn: refused (design 1: <= 600 m)
        ls = {L("far", 0, -5000, 5000, 0.8f)};
        s = StagePoint(spawn, to, ls, &bound);
        CHECK(!s.ok && s.why.compare(0, 4, "far ") == 0 && s.dSpawn > 990 && s.dSpawn < 1010);
        // a leash with no radius / share is ignored; a run point on the spawn: no line
        ls = {L("none", 50000, 0, 0, 0.75f), L("nofrac", 50000, 0, 1000, 0)};
        s = StagePoint(spawn, to, ls, &bound);
        CHECK(s.ok && s.why == "free");
        CHECK(!StagePoint(spawn, spawn, ls, &bound).ok && StagePoint(spawn, spawn, ls).why == "no-line");
        // a custom back / side
        s = StagePoint(spawn, to, std::vector<StageLeash>(), nullptr, 400, 0);
        CHECK(s.ok && std::fabs(s.p[0]) < 0.01f);
        CHECK_NEAR(s.p[2], -400, 0.01);
    }

    // ---------------------------------------------------------------- fleeing, the profiles, the gap predictor
    {
        const float run[3] = {0, 0, 25000}, at1600[3] = {0, 0, 23400}, at1400[3] = {0, 0, 23600};
        CHECK(StillFleeing(spawn, run) && StillFleeing(at1600, run) && !StillFleeing(at1400, run));
        CHECK(ChargeDist(kOurCharge, -1) == 0 && ChargeDist(kOurCharge, 0) == 0);
        CHECK_NEAR(ChargeDist(kOurCharge, 5), 400, 0.01);
        CHECK_NEAR(ChargeDist(kOurCharge, 8.5f), 400 + 0.5f * 380 * 3.5f, 0.05); // 1,065
        CHECK_NEAR(ChargeDist(kOurCharge, 9.5f), 1365, 0.05);
        CHECK_NEAR(ChargeDist(kHisCharge, 5.5f), 412.5, 0.01);
        CHECK_NEAR(ChargeDist(kHisCharge, 9.5f), 1162.5, 0.05);
        // pressed with him (lag 0) from 260 m: we gain ~200 m (our ramp leads his by 0.5 s) - the lock at ~58 m; each
        // 0.5 s of lag gives back 150 m
        const GapPrediction g0 = ChargeGap(260, 0), g5 = ChargeGap(260, 0.5f), g1 = ChargeGap(260, 1.0f);
        CHECK_NEAR(g0.lockT, 9.5, 1e-4);
        CHECK_NEAR(g0.lockGap, 57.5, 0.5);
        CHECK_NEAR(g5.lockGap, 207.5, 0.5);
        CHECK_NEAR(g1.lockGap, 357.5, 0.5);
        CHECK(g0.minGap <= g0.lockGap + 0.01f && g0.minGap > 0);
        // a late press: the lock when OUR charge ends; the gap of today's 2-5.4 s wait: ~1 km gained by him
        const GapPrediction late = ChargeGap(260, 4.0f);
        CHECK_NEAR(late.lockT, 12.5, 1e-4);
        CHECK(late.lockGap > 1000 && late.minGap == 260);
        if (g_test.verbose)
            printf("spawn-charge gap from 260 m: lag 0 -> %.0f (min %.0f), 0.5 -> %.0f, 1 -> %.0f, 4 -> %.0f m\n",
                   g0.lockGap, g0.minGap, g5.lockGap, g1.lockGap, late.lockGap);
    }

    // ---------------------------------------------------------------- ChaseStep's `fleeing`
    {
        ChaseFacts c;
        c.gap = 260, c.bearingDeg = 15, c.mySpeed = 0, c.hisSpeed = 0, c.engineTop = 80, c.gunRange = 600, c.shield = 1;
        CHECK(ChaseStep(c).verdict == ChaseVerdict::Fight); // (today: he is no faster than our engine)
        c.fleeing = true;
        CHECK(ChaseStep(c).verdict == ChaseVerdict::Charge); // (at his spawn, at rest: the charge at once)
        c.gap = 90;
        CHECK(ChaseStep(c).verdict == ChaseVerdict::Fight); // (inside the coast's near edge, not cruising: the fight)
        c.cruising = true, c.mySpeed = 80, c.hisSpeed = 75;
        CHECK(ChaseStep(c).verdict == ChaseVerdict::Charge); // (a charge running goes on)
        c.gap = 200, c.mySpeed = 200, c.hisSpeed = 140;
        CHECK(ChaseStep(c).verdict == ChaseVerdict::Charge); // (in the ramp: no coast under kChargeCoastSpeed)
        c.fleeing = false, c.hisSpeed = 290;
        CHECK(ChaseStep(c).verdict == ChaseVerdict::Coast); // (not fleeing: today's 150 m/s)
        c.fleeing = true, c.mySpeed = 285;
        CHECK(ChaseStep(c).verdict == ChaseVerdict::Charge); // (285 < 295: still before our lock - probe sc3)
        c.mySpeed = 297;
        ChaseResult r = ChaseStep(c);
        CHECK(r.verdict == ChaseVerdict::Coast && strcmp(r.why, "start") == 0);
        c.hisSpeed = 70; // (his speed read low: the coast is not refused for it)
        CHECK(ChaseStep(c).verdict == ChaseVerdict::Coast);
        c.gap = 300;
        CHECK(ChaseStep(c).verdict == ChaseVerdict::Charge); // (beyond the window: on with the cruise)
        c.coasting = true, c.gap = 541;
        r = ChaseStep(c);
        CHECK(r.verdict == ChaseVerdict::EngineOn && strcmp(r.why, "gap") == 0); // (the coast's end as built)
        c.coasting = false, c.killed = true;
        CHECK(ChaseStep(c).verdict == ChaseVerdict::Fight);
    }

    // ---------------------------------------------------------------- the fight at the point: ChargeKeep
    {
        const float p[3] = {1000, 0, 1000}, target[3] = {1000, 0, 2000};
        float me[3];
        Set3(me, 1300, 0, 1000);
        KeepResult k = ChargeKeep(me, p, target, false);
        CHECK(!k.station);
        CHECK_NEAR(k.dP, 300, 0.01);
        Set3(me, 1450, 0, 1000);
        k = ChargeKeep(me, p, target, false);
        CHECK(k.station);
        CHECK_NEAR(k.pos[0], 1000, 0.01);
        CHECK_NEAR(k.pos[2], 1000 + kChargeKeepAt, 0.01); // (between P and the escort)
        Set3(me, 1300, 0, 1000);
        CHECK(ChargeKeep(me, p, target, true).station); // (held until within 250 m)
        Set3(me, 1200, 0, 1000);
        CHECK(!ChargeKeep(me, p, target, true).station);
        const float nearT[3] = {1000, 0, 1100}; // (the escort 100 m from P: the station on it, no farther)
        Set3(me, 1500, 0, 1000);
        k = ChargeKeep(me, p, nearT, false);
        CHECK(k.station);
        CHECK_NEAR(k.pos[2], 1100, 0.01);
        k = ChargeKeep(me, p, p, false); // (the escort at P: the station at P)
        CHECK(k.station && k.pos[0] == 1000 && k.pos[2] == 1000);
    }

    // ---------------------------------------------------------------- M02: Ashcroft's spawn, King's nag and leash
    {
        // m02.ini: Ashcroft spawns at -28500, 0, 51100 and runs for -22099, 0, 75896 (look_test.cpp); nag_battle3 = King
        // 5000 m (0.8), the supervisor's leash (`objective leash Li01_03 3000 m: ship (kept)`, as3 / as4) = King 3000 m
        // (0.75). With King where the as2-as4 stage clamped to 4.0 km of him (>= ~3.7 km from the spawn) the leash
        // leaves no point within 600 m of the spawn: refused (the fight as without the stage). King 2 km off: a point.
        const float ash[3] = {-28500, 0, 51100}, run[3] = {-22099, 0, 75896};
        std::vector<StageLeash> ls = {L("nag_battle3", -29951, 46688, 5000, kChargeNagFrac),
                                      L("ship1", -29951, 46688, 3000, kChargeLeashFrac)};
        StageResult s = StagePoint(ash, run, ls, &bound);
        CHECK(!s.ok && s.why.compare(0, 4, "far ") == 0 && s.dSpawn > 2000);
        if (g_test.verbose)
            printf("spawn-charge m02, King 4.6 km off: %s (d_spawn %.0f)\n", s.why.c_str(), s.dSpawn);
        ls[0].anchor[0] = ls[1].anchor[0] = -29000, ls[0].anchor[2] = ls[1].anchor[2] = 49165; // (~2.0 km from him)
        s = StagePoint(ash, run, ls, &bound);
        CHECK(s.ok && s.dSpawn <= kChargeMaxFromSpawn);
        CHECK(Dist(s.p, ls[1].anchor) <= 2251);
        // the point lies behind him on his line (the coast's 30 deg: within ~16 deg of the line seen from it)
        const float along = (ash[0] - s.p[0]) * s.u[0] + (ash[2] - s.p[2]) * s.u[2];
        CHECK(along > 0);
        if (g_test.verbose)
            printf("spawn-charge m02, King 2 km off: P=%.0f,%.0f d_spawn=%.0f %s bound=%s along=%.0f\n", s.p[0], s.p[2],
                   s.dSpawn, s.why.c_str(), bound.c_str(), along);
    }
}
