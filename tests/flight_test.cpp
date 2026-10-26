// core\game\flight.cpp's closed forms over a synthetic flight model (a Starflier-like ship: engine 80 m/s, afterburner
// 200 m/s, cruise 300 m/s after a 5 s charge at 60 m/s^2): the force law's Thrust methods, BrakeDistance, and
// QuickRunTime against the simulated PlanRun (the header: from rest they agree within ~0.1 s).
#include "test.h"

#include "game/flight.h"

void Test_flight()
{
    flight::Model m;
    m.ok = true;
    m.mass = 100;
    m.engine = {8000, 100, 100};   // top 80 m/s, tau 1 s
    m.reverse = {-8000, 100, 100}; // full reverse
    m.burner = {20000, 100, 100};  // top 200 m/s
    m.hasBurner = true;
    m.cruiseChargeTime = 5;
    m.cruiseSpeed = 300;
    m.cruiseAccel = 60;
    m.turnRate = 1.6f;

    // the force law: v(t) = top + (v0 - top) e^(-t / tau)
    CHECK_NEAR(m.engine.top(), 80, 1e-4);
    CHECK_NEAR(m.engine.tau(), 1, 1e-4);
    CHECK_NEAR(m.engine.SpeedAfter(0, 1), 80 * (1 - std::exp(-1.0)), 1e-3);
    CHECK_NEAR(m.engine.SpeedAfter(0, 100), 80, 1e-3);
    CHECK_NEAR(m.engine.DistanceAfter(0, 100), 80 * 100 - 80, 0.5); // x(t) = top t + (v0 - top) tau (1 - e^(-t/tau))
    CHECK_NEAR(m.engine.TimeToSpeed(0, 40), std::log(2.0), 1e-3);
    CHECK(m.engine.TimeToSpeed(0, 100) < 0); // never: beyond the top speed
    CHECK_NEAR(m.burner.TimeToSpeed(0, 100), std::log(2.0), 1e-3);
    // braking from 200 m/s to rest at full reverse, and no braking when already slower
    const float brake = flight::BrakeDistance(m, 200, 0);
    CHECK(brake > 0 && brake < 300);
    CHECK_NEAR(m.reverse.DistanceToSpeed(200, 0), brake, 1e-3);
    CHECK(flight::BrakeDistance(m, 50, 80) == 0);
    CHECK(flight::BrakeDistance(m, 200, 0) > flight::BrakeDistance(m, 100, 0));

    // QuickRunTime vs PlanRun from rest, stopping at the end
    for (const float d : {300.0f, 2000.0f, 20000.0f})
    {
        const float quick = flight::QuickRunTime(m, 0, d, true);
        const flight::RunPlan plan = flight::PlanRun(m, 0, d, 0);
        if (std::fabs(quick - plan.time()) > 0.3f)
            fprintf(stderr, "  d = %g: QuickRunTime %.3f s, PlanRun %.3f s (cruise %d)\n", d, quick, plan.time(),
                    plan.cruise);
        CHECK_NEAR(quick, plan.time(), 0.3);
    }
    // a long run goes by cruise: charge (5 s at 80 m/s), ramp 80 -> 300 (3.67 s), the rest at 300
    const float longRun = flight::QuickRunTime(m, 0, 20000, false);
    CHECK(longRun > 65 && longRun < 75);
    CHECK(flight::PlanRun(m, 0, 20000, 0).cruise);
    // a short run is faster on the afterburner
    CHECK(!flight::PlanRun(m, 0, 300, 0).cruise);
    CHECK(flight::QuickRunTime(m, 0, 0, true) == 0);
    flight::Model off;
    CHECK(flight::QuickRunTime(off, 0, 1000, true) == 0);
    CHECK(flight::PlanRun(off, 0, 1000, 0).time() < 0);
    // the turn first, the charge during it
    CHECK_NEAR(flight::PlanRun(m, 0, 2000, 0, 0, 1.6f).time(), flight::PlanRun(m, 0, 2000, 0).time() + 1, 0.6);
}
