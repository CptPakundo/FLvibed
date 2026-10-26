// core\game\route.cpp's pure parts: the lane law's defaults without the game, Lane::PointAt, RideSpeed / RideTime
// against the closed forms of the ride law, LaneBetween / LearnLaneSpeed over the stub's ring chain.
#include "test.h"

#include "game/route.h"
#include "space_stub.h"

void Test_route()
{
    // no common.dll in this process: the measured law
    const route::LaneLaw& law = route::Law();
    CHECK(!law.fromGame);
    CHECK(law.speed == 2500 && law.endSpeed == 400 && law.accel == 312.5f);

    // a straight lane of three rings along +x, 10 km apart
    route::Lane lane;
    lane.rings = {1, 2, 3};
    lane.x = {0, 10000, 20000};
    lane.y = {0, 0, 0};
    lane.z = {0, 0, 0};
    lane.at = {0, 10000, 20000};
    CHECK(lane.length() == 20000);
    float p[3] = {-1, -1, -1};
    lane.PointAt(0, p);
    CHECK(p[0] == 0 && p[1] == 0 && p[2] == 0);
    lane.PointAt(5000, p);
    CHECK_NEAR(p[0], 5000, 1e-3);
    lane.PointAt(15000, p);
    CHECK_NEAR(p[0], 15000, 1e-3);
    lane.PointAt(-5, p);
    CHECK(p[0] == 0); // before the start: the first ring
    lane.PointAt(25000, p);
    CHECK_NEAR(p[0], 25000, 1e-3); // beyond the end: extrapolated along the last segment
    route::Lane empty;
    p[0] = 42;
    empty.PointAt(100, p);
    CHECK(p[0] == 42); // untouched
    CHECK(empty.length() == 0);

    // RideSpeed: from rest (5 m/s) at fromS, the ramp v^2 = v0^2 + 2 a s, capped at the speed, the last segment slowing
    // linearly to endSpeed
    CHECK_NEAR(route::RideSpeed(law, lane, 0, 0), 5, 1e-4);
    CHECK_NEAR(route::RideSpeed(law, lane, 0, 1000), std::sqrt(25 + 2 * 312.5 * 1000), 1e-2);
    CHECK_NEAR(route::RideSpeed(law, lane, 0, 10000), 2500, 1e-2);
    CHECK_NEAR(route::RideSpeed(law, lane, 0, 15000), 1450, 1e-2); // halfway down the last segment
    CHECK_NEAR(route::RideSpeed(law, lane, 0, 20000), 400, 1e-2);
    CHECK_NEAR(route::RideSpeed(law, lane, 10000, 10000), 5, 1e-4); // a ride starting at the middle ring
    // a lane's measured speed replaces the law's, the ramp stays the same fraction per second
    route::Lane slow = lane;
    slow.speed = 1250;
    CHECK_NEAR(route::RideSpeed(law, slow, 0, 10000), 1250, 1e-2);
    CHECK_NEAR(route::RideSpeed(law, slow, 0, 1000), std::sqrt(25 + 2 * 156.25 * 1000), 1e-2);
    CHECK_NEAR(route::RideSpeed(law, slow, 0, 15000), 1250 - 850 * 0.5, 1e-2);

    // RideTime: the first segment is all ramp (2500^2 / (2 x 312.5) = 10 km): (2500 - 5) / 312.5 s; the last slows
    // linearly 2500 -> 400 over 10 km: L / (v1 - v0) x ln(v1 / v0). (RideTime integrates in 50 m steps by the midpoint
    // speed; the first step, 5 -> 177 m/s, is ~0.15 s short of the exact integral - the tolerance allows that.)
    const double ramp = (2500.005 - 5) / 312.5, slowing = 10000.0 / 2100 * std::log(2500.0 / 400);
    CHECK_NEAR(route::RideTime(law, lane, 0, 10000), ramp, 0.25);
    CHECK_NEAR(route::RideTime(law, lane, 0, 20000), ramp + slowing, 0.3);
    CHECK_NEAR(route::RideTime(law, lane, 0, 0), 0, 1e-6);
    CHECK(route::RideTime(law, lane, 0, 5000) < route::RideTime(law, lane, 0, 10000));
    CHECK(route::RideTime(law, slow, 0, 20000) > route::RideTime(law, lane, 0, 20000));

    // LaneBetween over the stub's chain 11 -> 12 -> 13 -> 14 (5 km apart): the whole lane in the ride's direction
    stub::SetRings(
        {{11, {0, 0, 0}, 12, 0}, {12, {5000, 0, 0}, 13, 11}, {13, {10000, 0, 0}, 14, 12}, {14, {15000, 0, 0}, 0, 13}});
    route::Lane L;
    CHECK(route::LaneBetween(12, 13, L));
    CHECK(L.rings == std::vector<u32>({11, 12, 13, 14}));
    CHECK(L.at.size() == 4 && L.length() == 15000);
    CHECK(L.speed == 0); // nothing learned yet
    CHECK(route::LaneBetween(13, 12, L));
    CHECK(L.rings == std::vector<u32>({14, 13, 12, 11})); // the other way
    CHECK(L.x.size() == 4 && L.x[0] == 15000 && L.x[3] == 0);
    CHECK(!route::LaneBetween(12, 99, L));
    CHECK(!route::LaneBetween(12, 12, L));
    CHECK(!route::LaneBetween(0, 12, L));
    // a measured ride speed is learned for the whole chain and used by later plans over it
    CHECK(route::LearnedLaneSpeed(14) == 0);
    route::LearnLaneSpeed(12, 50); // too slow to be a ride: ignored
    CHECK(route::LearnedLaneSpeed(12) == 0);
    route::LearnLaneSpeed(12, 1800);
    CHECK(route::LearnedLaneSpeed(11) == 1800 && route::LearnedLaneSpeed(14) == 1800);
    CHECK(route::LaneBetween(12, 13, L) && L.speed == 1800);
    stub::SetRings({});
}
