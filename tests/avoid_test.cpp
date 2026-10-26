// core\game\avoid.cpp over synthetic obstacles: Steer (the elastic band), PathLength (the run round big bodies; Keep's
// lethal-zone berth shows through both), RayFan over the stub's sphere ray-caster (tests\space_stub.cpp).
#include "test.h"

#include "game/avoid.h"
#include "space_stub.h"

static space::Obstacle Solid(float x, float y, float z, float radius, u32 id)
{
    space::Obstacle o;
    o.pos[0] = x, o.pos[1] = y, o.pos[2] = z;
    o.radius = radius;
    o.id = id;
    return o;
}

static float Dist(const float a[3], const float b[3])
{
    const float d[3] = {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
    return std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
}

void Test_avoid()
{
    // The ship at the origin flying -z at 80 m/s, the aim 2 km ahead (Starflier-like: radius 10 m, 1.6 rad/s).
    avoid::Input in;
    in.vel[2] = -80;
    in.aim[2] = -2000;
    in.shipRadius = 10;
    in.turnRate = 1.6f;
    avoid::Memory mem;

    // nothing in the way: the aim itself, clear to the aim
    avoid::Result r = avoid::Steer(in, {}, mem);
    CHECK(!r.blocked);
    CHECK(r.steer[0] == in.aim[0] && r.steer[1] == in.aim[1] && r.steer[2] == in.aim[2]);
    CHECK(r.clearAhead == 2000);
    CHECK(r.checked == 0 && !r.strict && r.maxSpeed > 1e8f);
    CHECK(!mem.active && mem.path.empty());

    // How far the band goes round a 100 m solar 400 m ahead whose centre is `off` m beside the line (the ship exactly on
    // the line): the largest lateral offset of the path, and Result::extra. Observed (24 Oct 2026): ~90 m of offset at
    // every `off` from 0 to 30 m (the exactly-centred case breaks its symmetry through the one point at the centre), and
    // `extra` negative throughout: the window ends inside the body's clearance (endFree), the end point moves, and the
    // path's length no longer compares with the window. Diagnostics (verbose), not checks.
    auto roundAt = [&](float off, float* extra)
    {
        avoid::Memory m;
        const avoid::Result res = avoid::Steer(in, {Solid(off, 0, -400, 100, 7)}, m);
        float maxOff = 0;
        for (size_t i = 0; i + 2 < m.path.size(); i += 3)
            maxOff = std::max(maxOff, std::sqrt(m.path[i] * m.path[i] + m.path[i + 1] * m.path[i + 1]));
        if (extra)
            *extra = res.extra;
        return maxOff;
    };
    if (g_test.verbose)
        for (const float off : {0.0f, 0.1f, 1.0f, 5.0f, 30.0f})
        {
            float extra = 0;
            const float maxOff = roundAt(off, &extra);
            printf("  obstacle %5.1f m off the line: path offset %6.1f m, extra %6.1f m\n", off, maxOff, extra);
        }

    // a 100 m solar across the line 400 m ahead (its centre 30 m beside it): blocked, the path leaves the line and
    // clears the body
    const space::Obstacle block = Solid(30, 0, -400, 100, 7);
    r = avoid::Steer(in, {block}, mem);
    CHECK(r.blocked);
    CHECK(r.checked == 1);
    CHECK(r.clearAhead == 0);
    CHECK(r.residual == 0);                // no final point cuts into the body's radii
    CHECK(!r.strict && r.maxSpeed > 1e8f); // light contact is fine outside strict fields: no slowing
    CHECK(mem.active);
    CHECK(mem.path.size() % 3 == 0 && mem.path.size() >= 5 * 3);
    bool allClear = true;
    float maxOffset = 0;
    for (size_t i = 3; i + 2 < mem.path.size(); i += 3) // (the first point is the ship itself)
    {
        const float p[3] = {mem.path[i], mem.path[i + 1], mem.path[i + 2]};
        allClear &= Dist(p, block.pos) >= block.radius + in.shipRadius - 0.01f;
        maxOffset = std::max(maxOffset, std::sqrt(p[0] * p[0] + p[1] * p[1]));
    }
    CHECK(allClear);
    CHECK(maxOffset > 80);                      // the band went round (|x - 30| >= 110 beside the centre), not through
    CHECK(r.steer[2] < 0 && r.steer[2] > -400); // the steer point lies ahead, before the body
    // the warm start keeps the side: the next frame's path lies on the same side (the summed lateral offsets)
    auto lateral = [](const std::vector<float>& path)
    {
        float sum = 0;
        for (size_t i = 0; i + 2 < path.size(); i += 3)
            sum += path[i] + path[i + 1];
        return sum;
    };
    const float side0 = lateral(mem.path);
    CHECK(std::fabs(side0) > 100);
    r = avoid::Steer(in, {block}, mem);
    CHECK(r.blocked);
    CHECK(lateral(mem.path) * side0 > 0);

    // a 50 m rock 200 m ahead, the window (400 m) ending well beyond it: the detour's extra length is positive
    avoid::Memory fresh;
    space::Obstacle rockAhead = Solid(10, 0, -200, 50, 0);
    rockAhead.asteroid = true;
    r = avoid::Steer(in, {rockAhead}, fresh);
    CHECK(r.blocked);
    CHECK(r.extra > 0 && r.extra < 200);
    CHECK(r.residual == 0);
    CHECK(fresh.path.size() >= 3 && std::fabs(fresh.path.back() + 400) < 1); // the path ends on the line, 400 m ahead

    // a mine among the obstacles: strict (slowing allowed)
    space::Obstacle mine = Solid(0, 0, -400, 100, 0);
    mine.mine = true;
    mine.asteroid = true;
    r = avoid::Steer(in, {mine}, mem);
    CHECK(r.strict);
    CHECK(r.blocked);
    in.strict = true;
    r = avoid::Steer(in, {}, mem);
    CHECK(r.strict && !r.blocked);
    in.strict = false;

    // off the line (200 m beside it): clear for the window, the body counted
    r = avoid::Steer(in, {Solid(200, 0, -400, 50, 8)}, mem);
    CHECK(!r.blocked);
    CHECK(r.checked == 1);
    CHECK(r.clearAhead > 0 && r.clearAhead < 2000); // the window, not the whole way (a body is near)

    // the body being docked with is ignored; small breakable debris is not avoided
    in.ignore = {7};
    r = avoid::Steer(in, {block}, mem);
    CHECK(!r.blocked && r.checked == 0);
    in.ignore.clear();
    space::Obstacle debris = Solid(0, 0, -400, 30, 0);
    debris.breakable = true;
    debris.asteroid = true;
    r = avoid::Steer(in, {debris}, mem);
    CHECK(!r.blocked && r.debrisIgnored == 1 && r.checked == 0);
    // ... but a breakable mine is
    debris.mine = true;
    r = avoid::Steer(in, {debris}, mem);
    CHECK(r.blocked && r.debrisIgnored == 0);

    // at the aim already: nothing to do
    avoid::Input at = in;
    at.aim[2] = -0.5f;
    r = avoid::Steer(at, {block}, mem);
    CHECK(!r.blocked && !mem.active);

    // PathLength: a 10 km run; a 1 km station midway makes it longer, small / rock / ship bodies do not
    const float from[3] = {0, 0, 0}, to[3] = {0, 0, -10000};
    CHECK_NEAR(avoid::PathLength(from, to, {}, 10, 80, 1.6f), 10000, 1e-3);
    const space::Obstacle station = Solid(0, 0, -5000, 1000, 1);
    const float round = avoid::PathLength(from, to, {station}, 10, 80, 1.6f);
    CHECK(round > 10000 && round < 13000);
    CHECK_NEAR(avoid::PathLength(from, to, {Solid(0, 0, -5000, 299, 2)}, 10, 80, 1.6f), 10000, 1e-3);
    space::Obstacle rock = Solid(0, 0, -5000, 1000, 0);
    rock.asteroid = true;
    CHECK_NEAR(avoid::PathLength(from, to, {rock}, 10, 80, 1.6f), 10000, 1e-3);
    space::Obstacle ship = Solid(0, 0, -5000, 1000, 3);
    ship.ship = true;
    CHECK_NEAR(avoid::PathLength(from, to, {ship}, 10, 80, 1.6f), 10000, 1e-3);
    CHECK_NEAR(avoid::PathLength(from, to, {station}, 10, 80, 1.6f, 1), 10000, 1e-3); // ignored by id
    // a body beside the line that the run never meets
    CHECK_NEAR(avoid::PathLength(from, to, {Solid(3000, 0, -5000, 1000, 4)}, 10, 80, 1.6f), 10000, 1e-3);
    // a lethal zone of the same size gets the berth (Keep: half a second of travel + half the turning radius): longer
    space::Obstacle zone = station;
    zone.zone = true;
    zone.id = 0;
    const float roundZone = avoid::PathLength(from, to, {zone}, 10, 80, 1.6f);
    CHECK(roundZone > round);
    // ... and a faster ship keeps more (compared at its own speed: its wider look-ahead window bends earlier, which
    // makes the whole detour shorter than the slow ship's - 450 vs 530 m here - so the berth shows against the plain
    // body at the same speed, not against the slow ship)
    CHECK(avoid::PathLength(from, to, {zone}, 10, 300, 1.6f) > avoid::PathLength(from, to, {station}, 10, 300, 1.6f));
    // a body inside another's clearance (a planet inside its death zone) is gone round once
    const space::Obstacle planet = Solid(0, 0, -5000, 600, 5);
    CHECK_NEAR(avoid::PathLength(from, to, {zone, planet}, 10, 80, 1.6f), roundZone, 1e-2);
    // a short run
    const float near[3] = {0, 0, -0.5f};
    CHECK_NEAR(avoid::PathLength(from, near, {station}, 10, 80, 1.6f), 0.5, 1e-4);

    // RayFan over the stub's spheres: clear -> the wanted direction; a sphere ahead -> a clear direction beside it
    stub::SetSolids({});
    const float pos[3] = {0, 0, 0}, wanted[3] = {0, 0, -1};
    stub::ResetRayCount();
    avoid::FanResult f = avoid::RayFan(pos, wanted, 1000, 10, nullptr, 0);
    CHECK(!f.blocked && !f.brake);
    CHECK_NEAR(f.dir[2], -1, 1e-4);
    CHECK_NEAR(f.clear, 1000, 1e-3);
    CHECK(stub::RayCount() > 0);
    stub::SetSolids({Solid(0, 0, -500, 100, 9)});
    f = avoid::RayFan(pos, wanted, 1000, 10, nullptr, 0);
    CHECK(f.blocked && !f.brake);
    CHECK(f.dir[2] < 0 && f.dir[2] > -0.999f); // ahead, but off the wanted line
    CHECK_NEAR(std::sqrt(f.dir[0] * f.dir[0] + f.dir[1] * f.dir[1] + f.dir[2] * f.dir[2]), 1, 1e-3);
    CHECK(f.clear >= 999);
    const float end[3] = {f.dir[0] * 1000, f.dir[1] * 1000, f.dir[2] * 1000};
    CHECK(space::RayHitDistance(pos, end, nullptr, 0) < 0); // the direction it chose really is clear
    // the sphere ignored: the wanted direction again
    const u32 ignore = 9;
    f = avoid::RayFan(pos, wanted, 1000, 10, &ignore, 1);
    CHECK(!f.blocked);
    stub::SetSolids({});
}
