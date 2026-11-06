// Our ordnance (core\game\ordnance.h, WORK_QUEUE Q4 slice 1): FlightTime against the autopilot's old MissileFlightTime,
// RoundImpact on planes (straight, turned, a wall beside the nose), the launch-site verdict in the M13 death geometry
// (research\experience\legs\2026-10-30_0135_M13-dt60b-1-1_i1: a torpedo at a 67 m hull gap), KeepOut, the firing
// site's timing and choice.
#include "game/ordnance.h"

#include "test.h"

// MissileFlightTime as it stood in modules\autopilot\src\autopilot_target.cpp at 8ad4ad9 (the reference)
static float OldFlightTime(float motorAccel, float motorDelay, float motorTime, float lifetime, float dist,
                           float closing0)
{
    const float a = motorAccel > 0 ? motorAccel : 0.0f, delay = std::max(0.0f, motorDelay),
                burn = std::max(0.0f, motorTime), life = lifetime > 0 ? lifetime : 30.0f;
    float d = dist, t = 0, v = closing0;
    if (delay > 0)
    {
        if (v > 0 && v * delay >= d)
            return d / v;
        d -= v * delay;
        t += delay;
    }
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

// A plane through `p0` with normal `n` (unit) as the ray test: metres from a to the plane on a -> b, -1 none.
struct Plane
{
    float p0[3], n[3];
    float operator()(const float a[3], const float b[3]) const
    {
        float ab[3], ap[3];
        geom::Sub(ab, b, a);
        geom::Sub(ap, p0, a);
        const float den = geom::Dot(ab, n);
        if (std::fabs(den) < 1e-9f)
            return -1;
        const float f = geom::Dot(ap, n) / den;
        if (f < 0 || f > 1)
            return -1;
        return f * geom::Len(ab);
    }
};

static ordnance::Round Torpedo()
{
    // torpedo01_mark01 (HDE weapon_equip.ini): muzzle 16.7, motor 4.4444 m/s2 for 7.5 s, lifetime 37.5, turn 0.58067,
    // blast 25, detonation_dist 6.25
    ordnance::Round r;
    r.muzzle = 16.7f, r.motorDelay = 0, r.motorAccel = 4.4444f, r.motorTime = 7.5f, r.lifetime = 37.5f;
    r.maxTurn = 0.58067f, r.blast = 25, r.detonation = 6.25f;
    return r;
}

void Test_ordnance()
{
    using namespace ordnance;
    // FlightTime: bit-identical to the old function over delays, burns, lifetimes, distances and closing speeds
    {
        const float accels[] = {0, 4.4444f, 22, 523};
        const float delays[] = {0, 0.2f, 0.9f};
        const float burns[] = {0, 0.625f, 2.75f, 7.5f};
        const float lives[] = {0, 3.125f, 37.5f};
        const float dists[] = {0, 5, 60, 250, 1100, 3000};
        const float closings[] = {-50, 0, 16.7f, 30, 146, 400};
        int same = 0, total = 0;
        for (float a : accels)
            for (float de : delays)
                for (float b : burns)
                    for (float l : lives)
                        for (float d : dists)
                            for (float c : closings)
                            {
                                Round r;
                                r.motorAccel = a, r.motorDelay = de, r.motorTime = b, r.lifetime = l;
                                const float x = FlightTime(r, d, c), y = OldFlightTime(a, de, b, l, d, c);
                                ++total;
                                same += (x == y || (x != x && y != y)) ? 1 : 0;
                            }
        CHECK(same == total);
        // the measured Javelin case (FREELANCER_RE "Missile launch speed"): 30 m/s muzzle at rest, 22 m/s2 for 2.75 s
        Round j;
        j.motorAccel = 22, j.motorTime = 2.75f, j.lifetime = 30;
        CHECK_NEAR(FlightTime(j, 30.0f * 1.0f + 11.0f, 30.0f), 1.0, 1e-4);
    }

    const float origin[3] = {0, 0, 0}, rest[3] = {0, 0, 0}, noseZ[3] = {0, 0, 1};
    // straight at a plane 100 m ahead: 100 m/s, no motor, no turn - impact at 100 m after 1 s; with a 4 m fuse at 96 m
    {
        Round r;
        r.muzzle = 100, r.lifetime = 10;
        const Plane wall = {{0, 0, 100}, {0, 0, 1}};
        const float tgt[3] = {0, 0, 1000};
        const Impact a = RoundImpact(r, origin, rest, noseZ, tgt, 0, 5, wall);
        CHECK(a.cause == kHull);
        CHECK_NEAR(a.pos[2], 100, 1e-3);
        CHECK_NEAR(a.t, 1.0, 1e-4);
        r.detonation = 4;
        const Impact b = RoundImpact(r, origin, rest, noseZ, tgt, 0, 5, wall);
        CHECK(b.cause == kHull);
        CHECK_NEAR(b.pos[2], 96, 1e-3);
        CHECK_NEAR(b.t, 0.96, 1e-4);
        // nothing within the horizon: none; the lifetime first: expired where it is
        const Plane farWall = {{0, 0, 5000}, {0, 0, 1}};
        CHECK(RoundImpact(r, origin, rest, noseZ, tgt, 0, 2, farWall).cause == kNoImpact);
        r.lifetime = 1.5f;
        const Impact e = RoundImpact(r, origin, rest, noseZ, tgt, 0, 5, farWall);
        CHECK(e.cause == kExpired);
        CHECK_NEAR(e.pos[2], 150, 0.5);
        // a ship target (no hull to the ray test): it ends within the target's reach
        r.lifetime = 10;
        const Impact s = RoundImpact(r, origin, rest, noseZ, tgt, 50, 20, farWall);
        CHECK(s.cause == kTarget);
        CHECK(s.pos[2] >= 945 && s.pos[2] <= 951);
    }
    // the nose 90 deg off the target, turning at 1 rad/s at 100 m/s (a 100 m turn radius): the impact on the turned path -
    // well to the side of the straight line to the target, at the wall, after the quarter turn
    {
        Round r;
        r.muzzle = 100, r.lifetime = 10, r.maxTurn = 1.0f;
        const float noseX[3] = {1, 0, 0}, tgt[3] = {0, 0, 300};
        const Plane wall = {{0, 0, 200}, {0, 0, 1}};
        const Impact a = RoundImpact(r, origin, rest, noseX, tgt, 0, 10, wall);
        CHECK(a.cause == kHull);
        CHECK_NEAR(a.pos[2], 200, 1e-2);
        CHECK(a.pos[0] > 40 && a.pos[0] < 110); // (a reference integration: x 55.7, t 2.65 s)
        CHECK(a.t > 2.0f && a.t < 3.0f);
        // no turn: it never meets that wall (flies along x)
        r.maxTurn = 0;
        CHECK(RoundImpact(r, origin, rest, noseX, tgt, 0, 5, wall).cause == kNoImpact);
    }
    // a wall 10 m beside the nose with the target behind it (M12: the lair's wall beside us, the generator beyond): the
    // torpedo turns into the wall at once - its burst within our reach, refused
    {
        const Round r = Torpedo();
        const Plane wall = {{10, 0, 0}, {1, 0, 0}};
        const float tgt[3] = {150, 0, 200};
        const Impact a = RoundImpact(r, origin, rest, noseZ, tgt, 0, 10, wall);
        CHECK(a.cause == kHull);
        CHECK(a.pos[0] <= 10.01f);
        float us[3];
        PredictPos(origin, rest, Goal{}, a.t, us);
        float sep = 0;
        const float need = Need(r.blast, 10, 20);
        CHECK_NEAR(need, 55, 1e-4);
        CHECK(!LaunchSafe(a, us, need, &sep));
        CHECK(sep < 30);
    }
    // the i1 geometry: a 67 m hull gap straight ahead, the target's centre 200 m beyond the hull, the torpedo; we close at
    // 150 m/s (coasting) - its burst meets us: refused. The same at rest: it bursts ~61 m off, beyond the 55 m: allowed.
    {
        const Round r = Torpedo();
        const Plane hull = {{0, 0, 67}, {0, 0, 1}};
        const float tgt[3] = {0, 0, 267};
        const float need = Need(r.blast, 10, 20);
        const float fast[3] = {0, 0, 150};
        const Impact a = RoundImpact(r, origin, fast, noseZ, tgt, 0, 10, hull);
        CHECK(a.cause == kHull);
        CHECK_NEAR(a.pos[2], 67 - 6.25, 0.01);
        float us[3];
        PredictPos(origin, fast, Goal{}, a.t, us);
        float sep = 0;
        CHECK(!LaunchSafe(a, us, need, &sep));
        CHECK(sep < 15);
        const Impact b = RoundImpact(r, origin, rest, noseZ, tgt, 0, 10, hull);
        CHECK(b.cause == kHull);
        PredictPos(origin, rest, Goal{}, b.t, us);
        CHECK(LaunchSafe(b, us, need, &sep));
        CHECK_NEAR(sep, 60.75, 0.01);
        // at rest but station keeping toward a spot 30 m on toward the hull (the hold's goal): the worse case decides
        const float spot[3] = {0, 0, 30};
        Goal g;
        g.pos = spot;
        PredictPos(origin, rest, g, b.t, us);
        CHECK(us[2] > 5 && us[2] < 30);
        CHECK(!LaunchSafe(b, us, need, &sep));
    }
    // PredictPos: the station law settles on its goal; a coast is a straight line
    {
        const float spot[3] = {100, 0, 0};
        Goal g;
        g.pos = spot;
        float us[3];
        PredictPos(origin, rest, g, 15, us);
        CHECK(geom::Dist(us, spot) < 5);
        const float v[3] = {10, 0, -5};
        PredictPos(origin, v, Goal{}, 2, us);
        CHECK_NEAR(us[0], 20, 1e-4);
        CHECK_NEAR(us[2], -10, 1e-4);
    }
    // KeepOut: a way through the sphere is bent to pass at >= need; a goal inside is moved to its surface; from inside,
    // straight out; a way clear of it is left alone
    {
        const float c[3] = {0, 0, 100};
        float goal[3] = {0, 0, 200};
        CHECK(KeepOut(origin, goal, c, 50));
        CHECK(SegmentDist(origin, goal, c) >= 49.9f);
        CHECK(geom::Dist(goal, c) >= 49.9f);
        float g2[3] = {3, 0, 110};
        CHECK(KeepOut(origin, g2, c, 50));
        CHECK(geom::Dist(g2, c) >= 49.9f);
        CHECK(SegmentDist(origin, g2, c) >= 49.9f);
        const float inside[3] = {0, 10, 90};
        float g3[3] = {0, 0, 200};
        CHECK(KeepOut(inside, g3, c, 50));
        CHECK_NEAR(geom::Dist(g3, c), 55, 1e-3);
        float g4[3] = {200, 0, 0};
        CHECK(!KeepOut(origin, g4, c, 50));
        CHECK(g4[0] == 200 && g4[1] == 0 && g4[2] == 0);
    }
    // the firing site's timing: go when the refire is due by the arrival
    {
        CHECK(!ShouldGoToSite(-1, 0));
        CHECK(ShouldGoToSite(0, 0));
        CHECK(!ShouldGoToSite(0, 0.1f));
        CHECK(ShouldGoToSite(2, 2));
        CHECK(!ShouldGoToSite(2, 2.01f));
        CHECK(ShouldGoToSite(1, -3));
        CHECK_NEAR(StationTravelTime(10), 0, 0);
        CHECK_NEAR(StationTravelTime(150), std::log(10.0) / 0.4, 1e-4);
        CHECK(StationTravelTime(300) > StationTravelTime(150));
        CHECK_NEAR(StationTravelTime(300) - StationTravelTime(150), 2.5, 1e-4);
    }
    // the firing site's choice: the lowest score among gaps >= the stand-off; none
    {
        CHECK(PickFireSite(nullptr, 0, 150) == -1);
        const SiteCand none[] = {{40, 0}, {149.9f, 10}, {-1, -5}};
        CHECK(PickFireSite(none, 3, 150) == -1);
        const SiteCand some[] = {{40, 0}, {200, 3000}, {160, 1000}, {-1, -5}, {500, 1000}};
        CHECK(PickFireSite(some, 5, 150) == 2);
        CHECK(PickFireSite(some, 5, 300) == 4);
        CHECK(PickFireSite(some, 5, 0) == 0);
    }
}
