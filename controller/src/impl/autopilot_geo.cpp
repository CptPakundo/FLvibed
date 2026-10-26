// Advanced Autopilot - geometry: the planners over the game's ray test - LineClear, ArcAround, PassageVia, GeoPathAim, ApplyAvoid
// (used by the combat hops, the travel arrivals and the fast dock). Split from autopilot_dock.cpp on 24 Oct 2026, names,
// bodies and comments as they were: the behaviour is unchanged. Shared state and declarations in autopilot_impl.h.
#include "autopilot_impl.h"

namespace autopilot
{
// ------------------------------------------------------------------ geometry
// The autopilot's own obstacle avoidance (core\game\avoid) over a steering output: the steer point goes around what is
// in the way (field rocks, mines, solars, ships; not `ignore`, the object being docked with or entered), and the speed
// drops when the turn needs it (cruise / afterburner off, braking).
bool LineClear(const float a[3], const float b[3], float radius)
{
    float d[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
    const float dl = Len3(d);
    if (dl < 1)
        return true;
    geom::Norm(d);
    float s[3] = {d[2], 0.0f, -d[0]}; // across (horizontal), then up = d x s
    if (Len3(s) < 0.2f)
        s[0] = 1, s[1] = 0, s[2] = 0;
    geom::Norm(s);
    float u[3];
    geom::Cross(u, d, s);
    const float w = std::max(5.0f, radius) * 1.5f;
    const float offs[5][2] = {{0, 0}, {w, 0}, {-w, 0}, {0, w}, {0, -w}};
    for (const auto& o : offs)
    {
        float f[3], t[3];
        for (int i = 0; i < 3; ++i)
            f[i] = a[i] + s[i] * o[0] + u[i] * o[1], t[i] = b[i] + s[i] * o[0] + u[i] * o[1];
        if (space::RayHitDistance(f, t) >= 0)
            return false;
    }
    return true;
}

// An arc round a structure (user, 23 Oct 2026, watching the M12 hops: "jerking around too much, veering off the trench at
// the inner perimeter" - the planner's polylines were chased and planned again at cruise, turn rates to 370 deg/s, 32 s
// a hop; the user's try 3 flies one smooth curve round the lair's axis, dipping inward from r ~1900 to ~1300-1500 at the
// trench's middle height, 16.5 s): when one big solid's sphere holds both ends and the straight line cuts across it, arcs
// round its centre in the plane of the two ends - the radius blended from one end's to the other's and dipped inward
// mid-way by 0 / 15 / 28 / 40 % - each chord clear for the ship's width; the shortest clear one. False when none.
static bool ArcAround(const float a[3], const float b[3], float shipR, std::vector<float>& out, int& rays)
{
    const float mid[3] = {(a[0] + b[0]) * 0.5f, (a[1] + b[1]) * 0.5f, (a[2] + b[2]) * 0.5f};
    const float ab[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
    const float half = Len3(ab) * 0.5f;
    if (half < 300.0f)
        return false;
    const space::Obstacle* big = nullptr;
    const std::vector<space::Obstacle> obs = space::Obstacles(mid, half + 4000.0f);
    for (const space::Obstacle& o : obs)
    {
        if (o.asteroid || o.ship || o.zone || o.mine || o.radius < 800.0f)
            continue;
        const float da[3] = {a[0] - o.pos[0], a[1] - o.pos[1], a[2] - o.pos[2]};
        const float db[3] = {b[0] - o.pos[0], b[1] - o.pos[1], b[2] - o.pos[2]};
        if (Len3(da) > o.radius * 1.2f || Len3(db) > o.radius * 1.2f)
            continue;
        if (!big || o.radius > big->radius)
            big = &o;
    }
    if (!big)
        return false;
    const float* c = big->pos;
    float u[3] = {a[0] - c[0], a[1] - c[1], a[2] - c[2]}, w[3] = {b[0] - c[0], b[1] - c[1], b[2] - c[2]};
    const float ra = Len3(u), rb = Len3(w);
    if (ra < 100.0f || rb < 100.0f)
        return false;
    // (both ends out in the structure's ring, not near its middle: inside the M12 core - the ship 470-800 m from the
    // centre - every arc met the hub at its first chords, s9e)
    if (std::min(ra, rb) < 0.6f * big->radius)
    {
        events::Push("autopilot arc round %u: an end %.0f m from its centre (radius %.0f) - none", big->id,
                     std::min(ra, rb), big->radius);
        return false;
    }
    for (int i = 0; i < 3; ++i)
        u[i] /= ra, w[i] /= rb;
    // the straight line must cut inside both ends' radius (else it goes round already)
    const float mc[3] = {mid[0] - c[0], mid[1] - c[1], mid[2] - c[2]};
    if (Len3(mc) > 0.85f * std::min(ra, rb))
        return false;
    const float cosPhi = Clampf(Dot3(u, w), -1.0f, 1.0f);
    const float phi = acosf(cosPhi);
    if (phi < 0.35f || phi > 3.0f)
        return false; // (under 20 deg: no need; near 180: the plane is undefined)
    float n[3];
    geom::Cross(n, u, w);
    geom::Norm(n);
    float e2[3];
    geom::Cross(e2, n, u);
    const int N = 16;
    float bestLen = 1e30f;
    char why[160] = {};
    int whyN = 0;
    // (the depths, then the same 60 m above / below the plane mid-way: a level plane through the ends may graze the
    // trench's floor or roof)
    for (const float lift : {0.0f, 60.0f, -60.0f})
        for (const float depth : {0.0f, 0.12f, 0.22f, 0.30f, 0.38f, 0.46f})
        {
            std::vector<float> pts;
            float len = 0, prev[3] = {a[0], a[1], a[2]};
            bool clear = true;
            int blockedAt = 0;
            for (int i = 1; i <= N && clear; ++i)
            {
                const float s = static_cast<float>(i) / N;
                float q[3];
                if (i == N)
                    memcpy(q, b, sizeof(q));
                else
                {
                    const float rad = (ra * (1 - s) + rb * s) * (1.0f - depth * sinf(3.14159265f * s));
                    const float ph = phi * s;
                    for (int j = 0; j < 3; ++j)
                        q[j] = c[j] + rad * (cosf(ph) * u[j] + sinf(ph) * e2[j]) + n[j] * lift * sinf(3.14159265f * s);
                }
                // (the first chord from 25 m out: at a kill point the ship sits at the dead target's hull)
                float from[3];
                memcpy(from, prev, sizeof(from));
                if (i == 1)
                {
                    const float d0[3] = {q[0] - a[0], q[1] - a[1], q[2] - a[2]};
                    const float l0 = std::max(1.0f, Len3(d0));
                    for (int j = 0; j < 3; ++j)
                        from[j] = a[j] + d0[j] / l0 * std::min(25.0f, l0 * 0.5f);
                }
                // (the first and last chords are not tested: they meet the target's / the dead target's own hull at a
                // generator's hold point - s9e: 0.00:16 / 0.38:1 blocks on every hop; the hold run flies those ends)
                if (i > 1 && i < N)
                {
                    rays += 5;
                    clear = LineClear(from, q, shipR);
                }
                if (!clear)
                    blockedAt = i;
                const float dq[3] = {q[0] - prev[0], q[1] - prev[1], q[2] - prev[2]};
                len += Len3(dq);
                pts.insert(pts.end(), q, q + 3);
                memcpy(prev, q, sizeof(prev));
            }
            if (clear && len < bestLen)
                bestLen = len, out.swap(pts);
            else if (!clear && lift == 0.0f && whyN < static_cast<int>(sizeof(why)) - 12)
                whyN += sprintf_s(why + whyN, sizeof(why) - whyN, " %.2f:%d", depth, blockedAt);
        }
    if (bestLen >= 1e29f)
        events::Push("autopilot arc round %u (r %.0f / %.0f, %.0f deg): none clear - depth:chord blocked%s", big->id,
                     ra, rb, phi * 57.3f, why);
    return bestLen < 1e29f;
}

// A hole a single ray passes but the ship at speed does not (the M12 core's plate, see the airlock run): the line to the aim
// clear along its centre, not for the ship's radius + 20 m. The blocking depth from four rays offset by that margin; a point
// there 50-90 m to the side (8 directions) whose lines in and out are clear with the margin becomes the aim until the ship
// is past its plane. Re-tested 4x a second; true while it steers.
bool PassageVia(const fl::Kinematics& k, float aim[3])
{
    if (g.dock.passOn)
    {
        const float rel[3] = {k.pos[0] - g.dock.passVia[0], k.pos[1] - g.dock.passVia[1], k.pos[2] - g.dock.passVia[2]};
        if (Dot3(rel, g.dock.passDir) > 0)
        {
            g.dock.passOn = false;
            events::Push("autopilot passage passed (%.0f,%.0f,%.0f)", g.dock.passVia[0], g.dock.passVia[1],
                         g.dock.passVia[2]);
        }
        else
        {
            // the passage point itself until 120 m before the plate (s9w: the shifted aim alone, ~1.1 km away, moved the line
            // at the plate by ~10 m - both runs struck it 6-16 m off the axis), then the aim shifted by the offset: a line
            // parallel to the way through the plate, not a corner there (s9u-i3: the corner at the via point sent the ship
            // on down to 149 m off the axis and into the ring at z -1430)
            const float tp[3] = {g.dock.passPt[0] - k.pos[0], g.dock.passPt[1] - k.pos[1], g.dock.passPt[2] - k.pos[2]};
            if (Dot3(tp, g.dock.passDir) > 120.0f)
                memcpy(aim, g.dock.passPt, sizeof(g.dock.passPt));
            else
                for (int i = 0; i < 3; ++i)
                    aim[i] += g.dock.passOff[i];
            return true;
        }
    }
    if (g.time - g.dock.passCheckAt < 0.25f)
        return false;
    g.dock.passCheckAt = g.time;
    const float d[3] = {aim[0] - k.pos[0], aim[1] - k.pos[1], aim[2] - k.pos[2]};
    const float len = Len3(d);
    if (len < 150)
        return false;
    const float dir[3] = {d[0] / len, d[1] / len, d[2] / len};
    const float m = space::PlayerRadius() + 20.0f;
    const float hitRay = space::RayHitDistance(k.pos, aim);
    if (hitRay >= 0 || LineClear(k.pos, aim, m))
        return false; // blocked outright (the planner's case) or wide enough
    float u[3] = {-dir[2], 0, dir[0]};
    if (Len3(u) < 0.1f)
        u[0] = 1, u[1] = 0, u[2] = 0;
    geom::Norm(u);
    float v[3];
    geom::Cross(v, dir, u);
    float h = 1e30f;
    for (int s = 0; s < 4; ++s)
    {
        const float* b = s < 2 ? u : v;
        const float sg = (s % 2) ? -m : m;
        const float a0[3] = {k.pos[0] + b[0] * sg, k.pos[1] + b[1] * sg, k.pos[2] + b[2] * sg};
        const float a1[3] = {aim[0] + b[0] * sg, aim[1] + b[1] * sg, aim[2] + b[2] * sg};
        const float hh = space::RayHitDistance(a0, a1);
        if (hh >= 0 && hh < h)
            h = hh;
    }
    if (h > len)
        return false;
    const float block[3] = {k.pos[0] + dir[0] * h, k.pos[1] + dir[1] * h, k.pos[2] + dir[2] * h};
    // (passage_window: the opening's own centre - a 13 x 13 grid of 15 m cells in the obstruction's plane, each a 160 m ray
    // along the way through it; the clear cell with the most room round it (at least the ship + 20 m), the nearer of equals,
    // with clear lines in and out; probe 5, the exit run from the cell's tractor met the M12 core's ~90 m middle hole 83 m
    // off its centre, "64 m wide at most" sent it 120 m to the side round the hub, the run spiralled away from the door,
    // 6.1 km for 1.2. The plate's ~20 m hole has no such room: the open space above / below it wins)
    if (g_set.passageWindow)
    {
        const int G = 6;
        bool cl[2 * G + 1][2 * G + 1];
        for (int gi = -G; gi <= G; ++gi)
            for (int gj = -G; gj <= G; ++gj)
            {
                float c0[3], c1[3];
                for (int j = 0; j < 3; ++j)
                {
                    const float c = block[j] + u[j] * gi * 15.0f + v[j] * gj * 15.0f;
                    c0[j] = c - dir[j] * 80.0f, c1[j] = c + dir[j] * 80.0f;
                }
                cl[gi + G][gj + G] = space::RayHitDistance(c0, c1) < 0;
            }
        const int need = static_cast<int>(ceilf(m / 15.0f));
        struct Cand
        {
            int room, gi, gj;
        };
        std::vector<Cand> cands;
        for (int gi = -G; gi <= G; ++gi)
            for (int gj = -G; gj <= G; ++gj)
            {
                if (!cl[gi + G][gj + G])
                    continue;
                int room = 99;
                for (int oi = -G; oi <= G; ++oi)
                    for (int oj = -G; oj <= G; ++oj)
                        if (!cl[oi + G][oj + G])
                            room = std::min(room, std::max(std::abs(oi - gi), std::abs(oj - gj)));
                if (room >= need)
                    cands.push_back({room, gi, gj});
            }
        std::sort(
            cands.begin(), cands.end(), [](const Cand& x, const Cand& y)
            { return x.room != y.room ? x.room > y.room : x.gi * x.gi + x.gj * x.gj < y.gi * y.gi + y.gj * y.gj; });
        int tried = 0;
        for (const Cand& c : cands)
        {
            if (++tried > 12)
                break;
            float o[3], p[3], past[3], aimO[3];
            for (int j = 0; j < 3; ++j)
            {
                o[j] = u[j] * c.gi * 15.0f + v[j] * c.gj * 15.0f;
                p[j] = block[j] + o[j];
                past[j] = p[j] + dir[j] * 80.0f;
                aimO[j] = aim[j] + o[j];
            }
            if (LineClear(k.pos, past, m) && LineClear(past, aim, m) && LineClear(past, aimO, m))
            {
                g.dock.passOn = true;
                memcpy(g.dock.passVia, past, sizeof(past));
                memcpy(g.dock.passDir, dir, sizeof(dir));
                memcpy(g.dock.passOff, o, sizeof(o));
                memcpy(g.dock.passPt, p, sizeof(p));
                events::Push(
                    "autopilot passage: the line is %.0f m wide at most %.0f m ahead - the opening's centre %.0f m "
                    "off, %d m of room, through (%.0f,%.0f,%.0f)",
                    2 * m, h, Len3(o), c.room * 15, p[0], p[1], p[2]);
                memcpy(aim, h > 120.0f ? p : aimO, sizeof(p));
                return true;
            }
        }
    }
    // (120 m only under the session-10 switches: off, the 50-90 m search is as before)
    const bool wider = g_set.passageWindow;
    for (const float off : {50.0f, 70.0f, 90.0f, 120.0f})
        for (int i = 0; i < 8; ++i)
        {
            if (off > 100.0f && !wider)
                break;
            const float a = i * 0.785398f;
            float o[3], p[3], past[3], aimO[3];
            for (int j = 0; j < 3; ++j)
            {
                o[j] = (cosf(a) * u[j] + sinf(a) * v[j]) * off;
                p[j] = block[j] + o[j];
                past[j] =
                    p[j] + dir[j] * 80.0f; // (tested through the plate: s9u-i2, a line ending at its face - inside
                aimO[j] = aim[j] + o[j];   //  the solid a ray sees nothing - passed y -13 at the plate's lower edge)
            }
            if (LineClear(k.pos, past, m) && LineClear(past, aim, m) && LineClear(past, aimO, m))
            {
                g.dock.passOn = true;
                memcpy(g.dock.passVia, past, sizeof(past));
                memcpy(g.dock.passDir, dir, sizeof(dir));
                memcpy(g.dock.passOff, o, sizeof(o));
                memcpy(g.dock.passPt, p, sizeof(p));
                events::Push(
                    "autopilot passage: the line is %.0f m wide at most %.0f m ahead - %.0f m to the side, through "
                    "(%.0f,%.0f,%.0f)",
                    2 * m, h, off, p[0], p[1], p[2]);
                memcpy(aim, h > 120.0f ? p : aimO, sizeof(p)); // (more than 120 m before the plate: the point itself)
                return true;
            }
        }
    return false;
}

bool GeoPathAim(GeoPath& p, const fl::Kinematics& k, const float dest[3], u32 ignore, float aim[3], float& rest,
                const char* who)
{
    const u32 ign[1] = {ignore};
    const int nIgn = ignore ? 1 : 0;
    const float moved[3] = {dest[0] - p.dest[0], dest[1] - p.dest[1], dest[2] - p.dest[2]};
    // A way left behind is planned afresh from where the ship is (session 7, M12-s7-z3-b: after the turbine the cruise
    // overshot ~200 m off the core's axis; the follow chased a leg it had left - 3 min round and round between the wheel
    // and the hub, never through the hole): more than 250 m off the leg flown, or 8 s without 50 m of the way gained
    const char* replanWhy = nullptr;
    if (!p.pts.empty() && g.time - p.plannedAt > 2)
    {
        const size_t n0 = p.pts.size() / 3;
        const float* a = p.idx == 0 ? p.start : &p.pts[(p.idx - 1) * 3];
        const float* b = &p.pts[std::min(p.idx, n0 - 1) * 3];
        const float ab[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
        const float ap[3] = {k.pos[0] - a[0], k.pos[1] - a[1], k.pos[2] - a[2]};
        const float t = Clampf(Dot3(ap, ab) / std::max(1.0f, Dot3(ab, ab)), 0, 1);
        const float off[3] = {ap[0] - ab[0] * t, ap[1] - ab[1] * t, ap[2] - ab[2] * t};
        if (Len3(off) > 250)
            replanWhy = "off the way";
        else if (p.bestRest.Since(g.time) > 8)
            replanWhy = "no progress";
    }
    if (replanWhy)
        events::Push("autopilot path %s: %s - planning again", who, replanWhy);
    // (a failed plan is tried again after 6 s, not every frame or every 3 s: each try costs up to 8000 rays; meanwhile the
    // elbows alone each second - a few hundred rays; session 8, M12: the first plan beside generator 3 found none, the
    // elbow 6 s later was clear)
    // (...or once the ship is 60 m from where the failed plan started, at most each 1.5 s: at the generator-3 kill the ship
    // sits against the dead generator's hull and the first plan finds none; drifted off, the full plan finds the vertical
    // plane's way at once - waiting the 6 s, the elbow's slower ways were taken instead, gen 3 -> core 28-41 s vs 21)
    const float fromStart[3] = {k.pos[0] - p.start[0], k.pos[1] - p.start[1], k.pos[2] - p.start[2]};
    const bool fullTry = replanWhy || (Len3(moved) > 100 && g.time - p.plannedAt > 1) ||
                         (p.pts.empty() && g.time - p.plannedAt > 6) ||
                         (p.pts.empty() && g.time - p.plannedAt > 1.5f && Len3(fromStart) > 60.0f);
    const bool elbowTry = !fullTry && p.pts.empty() && g.time - p.elbowAt > 1;
    if (fullTry || elbowTry)
    {
        p.elbowAt = g.time;
        p.bestRest.Reset(g.time);
        int rays = 0;
        LARGE_INTEGER f0, f1, fq;
        QueryPerformanceCounter(&f0);
        // An elbow first (session 8, 23 Oct 2026, M12 gen 3 -> the airlock top: the user's try 3 climbs ~45 deg to the
        // destination's height and flies level to it, ~14 s; the lattice plan took 2.6-3.3 km for 1.4-1.7 km straight, the
        // first try found none in 77 000 rays): one via point over the straight line at the destination's height (+-150 m),
        // both legs clear for the ship's width - the shortest such way; else the planner
        const float r = space::PlayerRadius();
        // A leg from the ship is tested from 25 m out along it (session 8, M12: at the generator-3 kill the ship sits at
        // the dead generator's hull - every ray from its own position hit at once; 3-4 s later, drifted off, the same
        // elbows were clear)
        const auto clearFromShip = [&](const float* to)
        {
            float d[3] = {to[0] - k.pos[0], to[1] - k.pos[1], to[2] - k.pos[2]};
            const float dl = Len3(d);
            if (dl <= 50.0f)
                return LineClear(k.pos, to, r);
            float s[3];
            for (int i = 0; i < 3; ++i)
                s[i] = k.pos[i] + d[i] / dl * 25.0f;
            return LineClear(s, to, r);
        };
        // the shortest elbow from `from` (its length over `limit` is not tried); via = the corner
        const auto elbow = [&](const float from[3], float limit, float via[3])
        {
            float best = limit;
            bool found = false;
            for (const float t : {0.1f, 0.25f, 0.4f, 0.6f})
                for (const float dh : {0.0f, 150.0f, -150.0f})
                {
                    float v[3];
                    for (int i = 0; i < 3; ++i)
                        v[i] = from[i] + (dest[i] - from[i]) * t;
                    v[1] = dest[1] + dh;
                    const float a[3] = {v[0] - from[0], v[1] - from[1], v[2] - from[2]};
                    const float b[3] = {dest[0] - v[0], dest[1] - v[1], dest[2] - v[2]};
                    const float len = Len3(a) + Len3(b);
                    if (len >= best || fabsf(v[1] - from[1]) < 50.0f)
                        continue; // (no height change: the straight line, tested already)
                    rays += 10;
                    if ((from == k.pos ? clearFromShip(v) : LineClear(from, v, r)) && LineClear(v, dest, r))
                        best = len, memcpy(via, v, 3 * sizeof(float)), found = true;
                }
            return found ? best : 1e30f;
        };
        // the arc round a structure first (ArcAround): one smooth curve, not a polyline to chase
        std::vector<float> arcPts;
        const bool arcOk = fullTry && ArcAround(k.pos, dest, r, arcPts, rays);
        float via[3] = {}, out0[3] = {};
        float best = arcOk ? 1e30f : elbow(k.pos, 1e30f, via);
        bool firstLeg = false;
        // ...then the planner (RayPath) before an "out first" elbow (session 8, 12 M12 runs gen 3 -> core entry: the
        // planner's way 21.0-21.5 s (3 of 3), an elbow 23.0, every "out + elbow" 28-45 s - its legs ran beside the trench
        // walls at 80 m/s under the guard and were planned again 1-2 times); "out + elbow" only in a full try the planner
        // failed, never in the cheap 1 s tries
        std::vector<float> planned;
        bool plannerTried = false;
        if (best >= 1e29f && fullTry && !arcOk)
        {
            planned = avoid::RayPath(k.pos, dest, r, ign, nIgn, &rays, p.planesFirst, 0.0f);
            plannerTried = true;
            // ...and again from 60 m off the nearest solid (s9j-s9l, M12 gen 3 -> core: 7 of 9 runs found no way from the
            // kill point for 5-6.4 s, 84 000 rays a try, crawling at 80 m/s; the two that planned at once entered the core 7-8
            // s sooner): six rays round the ship, the nearest hit within 80 m -> the start 60 m the other way, if clear
            if (planned.empty())
            {
                static const float kDirs[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
                int nearest = -1;
                float nd = 80.0f;
                for (int d = 0; d < 6; ++d)
                {
                    const float to[3] = {k.pos[0] + kDirs[d][0] * 80.0f, k.pos[1] + kDirs[d][1] * 80.0f,
                                         k.pos[2] + kDirs[d][2] * 80.0f};
                    const float h = space::RayHitDistance(k.pos, to);
                    if (h >= 0 && h < nd)
                        nd = h, nearest = d;
                }
                rays += 7;
                if (nearest >= 0)
                {
                    const float launch[3] = {k.pos[0] - kDirs[nearest][0] * 60.0f, k.pos[1] - kDirs[nearest][1] * 60.0f,
                                             k.pos[2] - kDirs[nearest][2] * 60.0f};
                    if (space::RayHitDistance(k.pos, launch) < 0)
                    {
                        std::vector<float> rest2 = avoid::RayPath(launch, dest, r, ign, nIgn, &rays, p.planesFirst);
                        if (!rest2.empty())
                        {
                            planned.assign(launch, launch + 3);
                            planned.insert(planned.end(), rest2.begin(), rest2.end());
                            events::Push(
                                "autopilot path %s: none from the ship (a solid %.0f m off) - planned from 60 m "
                                "the other way",
                                who, nd);
                        }
                    }
                }
            }
        }
        // ...or out first, then the elbow (session 8, M12: from beside generator 3 in the trench every direct elbow met the
        // structure; the user's try 3 slid out along the trench, then climbed - probed with the ray test: from our kill point
        // (659, -20, -1566) level to the user's (759, -15, -1444) clear, from there the climb to y ~400 and the level leg to
        // the airlock top clear): 8 headings x 3 climbs (level first), 150 / 250 / 450 m
        if (best >= 1e29f && plannerTried && planned.empty())
        {
            const float straight[3] = {dest[0] - k.pos[0], dest[1] - k.pos[1], dest[2] - k.pos[2]};
            const float sl = Len3(straight);
            for (int h = 0; h < 8; ++h)
                for (const float climb : {0.0f, 0.5f, 1.0f})
                    for (const float d0 : {150.0f, 250.0f, 450.0f}) // (the user's slide out: 158 m)
                    {
                        const float a = h * 0.785398f;
                        float v0[3] = {cosf(a), climb, sinf(a)};
                        const float vl = Len3(v0);
                        for (int i = 0; i < 3; ++i)
                            v0[i] = k.pos[i] + v0[i] / vl * d0;
                        if (dest[1] < k.pos[1])
                            v0[1] = k.pos[1] - (v0[1] - k.pos[1]); // (the climb toward the destination's side)
                        if (d0 + sl * 0.5f >= std::min(best, 1.6f * sl))
                            continue; // (never more than 1.6x the straight way: the planner's turn - its ways ran 1.2-2x)
                        rays += 5;
                        if (!clearFromShip(v0))
                            continue;
                        float v1[3];
                        const float e = elbow(v0, std::min(best, 1.6f * sl) - d0, v1);
                        if (e < 1e29f && d0 + e < best)
                            best = d0 + e, memcpy(out0, v0, sizeof(out0)), memcpy(via, v1, sizeof(via)),
                            firstLeg = true;
                    }
        }
        p.arc = arcOk;
        if (arcOk)
            p.pts = arcPts;
        else if (best < 1e29f && firstLeg)
            p.pts = {out0[0], out0[1], out0[2], via[0], via[1], via[2], dest[0], dest[1], dest[2]};
        else if (best < 1e29f)
            p.pts = {via[0], via[1], via[2], dest[0], dest[1], dest[2]};
        else if (elbowTry)
            return false; // (the planner keeps its pace: 6 s, or 60 m moved)
        else
            p.pts = planned;
        QueryPerformanceCounter(&f1);
        QueryPerformanceFrequency(&fq);
        const double ms = static_cast<double>(f1.QuadPart - f0.QuadPart) * 1000.0 / static_cast<double>(fq.QuadPart);
        memcpy(p.dest, dest, sizeof(p.dest));
        memcpy(p.start, k.pos, sizeof(p.start));
        p.idx = 0;
        p.plannedAt = g.time;
        p.centre[0] = p.centre[1] = p.centre[2] = 0;
        float len = 0;
        float last[3] = {k.pos[0], k.pos[1], k.pos[2]};
        for (size_t i = 0; i + 2 < p.pts.size(); i += 3)
        {
            const float d[3] = {p.pts[i] - last[0], p.pts[i + 1] - last[1], p.pts[i + 2] - last[2]};
            len += Len3(d);
            memcpy(last, &p.pts[i], sizeof(last));
        }
        const float straight[3] = {dest[0] - k.pos[0], dest[1] - k.pos[1], dest[2] - k.pos[2]};
        events::Push("autopilot path %s legs=%u length=%.0f straight=%.0f rays=%d %.1f ms (%s)", who,
                     static_cast<unsigned>(p.pts.size() / 3), len, Len3(straight), rays, ms,
                     arcOk          ? "arc"
                     : best < 1e29f ? (firstLeg ? "out + elbow" : "elbow")
                                    : avoid::RayPathMode());
    }
    if (p.pts.empty())
        return false;
    size_t n = p.pts.size() / 3;
    // the point list with the start first: leg i runs from P(i) to P(i + 1), leg idx is flown
    auto P = [&](size_t i) { return i == 0 ? p.start : &p.pts[(i - 1) * 3]; };
    // on along the legs: past the end of the leg flown (its projection beyond 1) -> the next
    while (p.idx + 1 < n + 0)
    {
        const float* a = P(p.idx);
        const float* b = P(p.idx + 1);
        const float ab[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
        const float ap[3] = {k.pos[0] - a[0], k.pos[1] - a[1], k.pos[2] - a[2]};
        const float l2 = std::max(1.0f, Dot3(ab, ab));
        // (past the leg's end along it, or within 150 m of it: session 7, M12 - circling a leg end inside the turning circle,
        // the projection never reached 1)
        const float be[3] = {b[0] - k.pos[0], b[1] - k.pos[1], b[2] - k.pos[2]};
        if (Dot3(ap, ab) / l2 < 1.0f && Len3(be) > 150.0f)
        {
            // ...or within 3 s of speed of it with the next corner in plain sight for the ship's width (session 8, M12
            // gen 3 -> airlock: the "out" leg's end lay inside the turning circle at engine speed - 11 s round it at 78 m/s,
            // 110-170 m off, until "off the way" planned again; the other run of the pair needed 21 s for the whole section)
            if (p.arc || p.idx + 1 >= n || Len3(be) > std::max(150.0f, Len3(k.vel) * 3.0f) || g.time - p.skipAt < 0.25f)
                break;
            p.skipAt = g.time;
            if (!LineClear(k.pos, P(p.idx + 2), space::PlayerRadius()))
                break;
            events::Push("autopilot path %s: corner %u skipped %.0f m short (the next one in sight)", who,
                         static_cast<unsigned>(p.idx), Len3(be));
            // the way now starts here: the flown points and the skipped corner go (judged from the skipped corner, the
            // ship lay "off the way" at once - a new plan right after each skip, ff430f5's runs)
            p.pts.erase(p.pts.begin(), p.pts.begin() + static_cast<std::ptrdiff_t>((p.idx + 1) * 3));
            memcpy(p.start, k.pos, sizeof(p.start));
            p.idx = 0;
            n = p.pts.size() / 3;
            continue;
        }
        ++p.idx;
    }
    // Pure pursuit (session 7, 23 Oct 2026: steering at the legs' corners at 200-300 m/s overshot into the M12 trench's
    // outer wall; the user's try 3 flies smooth arcs): the carrot is the point ~1.5 s of speed ahead along the legs from
    // our projection on the leg flown (250 m at least), shortened while it is not in plain sight
    const float speed = Len3(k.vel);
    float carrot[3];
    {
        const float* a = P(p.idx);
        const float* b = P(std::min(p.idx + 1, n));
        float ab[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
        const float l = std::max(1.0f, Len3(ab));
        const float ap[3] = {k.pos[0] - a[0], k.pos[1] - a[1], k.pos[2] - a[2]};
        float t = Clampf(Dot3(ap, ab) / l, 0, l); // metres along this leg
        float left = std::max(250.0f, speed * 1.5f);
        size_t leg = p.idx;
        for (;;)
        {
            const float* u = P(leg);
            const float* v = P(std::min(leg + 1, n));
            const float uv[3] = {v[0] - u[0], v[1] - u[1], v[2] - u[2]};
            const float ll = Len3(uv);
            if (t + left <= ll || leg + 1 >= n)
            {
                const float s = ll > 1 ? std::min(ll, t + left) / ll : 1.0f;
                for (int i = 0; i < 3; ++i)
                    carrot[i] = u[i] + uv[i] * s;
                break;
            }
            left -= ll - t;
            t = 0;
            ++leg;
        }
        // not in plain sight (a corner's inside): back toward the leg's end
        if (g.time - p.sightAt > 0.25f)
        {
            p.sightAt = g.time;
            const float* e = P(std::min(p.idx + 1, n));
            if (space::RayHitDistance(k.pos, carrot, ign, nIgn) >= 0)
                memcpy(carrot, e, sizeof(carrot));
        }
    }
    // Centring (the user, 23 Oct 2026: "the trench is symmetrical... the best line keeps identical distance between the
    // lower and upper parts" of the lair's torus): from the carrot, rays up / down and to both sides across the way (300 m);
    // where both of a pair hit, the carrot moves to their middle - at most 120 m, smoothed
    {
        const float* e = P(std::min(p.idx + 1, n));
        float fwd[3] = {e[0] - k.pos[0], e[1] - k.pos[1], e[2] - k.pos[2]};
        const float fl = std::max(1.0f, Len3(fwd));
        for (float& x : fwd)
            x /= fl;
        float side[3] = {fwd[2], 0.0f, -fwd[0]};
        const float sl = Len3(side);
        float want[3] = {0, 0, 0};
        if (sl > 0.2f && !p.arc) // (an arc is the line itself: no nudging)
        {
            geom::Norm(side);
            float up[3];
            geom::Cross(up, fwd, side);
            for (const float* ax : {static_cast<const float*>(up), static_cast<const float*>(side)})
            {
                float hp[2];
                for (int sgn = 0; sgn < 2; ++sgn)
                {
                    const float s = sgn ? -300.0f : 300.0f;
                    const float to[3] = {carrot[0] + ax[0] * s, carrot[1] + ax[1] * s, carrot[2] + ax[2] * s};
                    hp[sgn] = space::RayHitDistance(carrot, to, ign, nIgn);
                }
                if (hp[0] >= 0 && hp[1] >= 0)
                {
                    const float off = Clampf((hp[0] - hp[1]) * 0.5f, -120.0f, 120.0f);
                    for (int i = 0; i < 3; ++i)
                        want[i] += ax[i] * off;
                }
            }
        }
        for (int i = 0; i < 3; ++i)
        {
            p.centre[i] += (want[i] - p.centre[i]) * 0.2f; // smoothed: no twitch between frames
            carrot[i] += p.centre[i];
        }
    }
    memcpy(aim, carrot, sizeof(float) * 3);
    memcpy(g.avoid.pathAim, carrot, sizeof(g.avoid.pathAim));
    g.avoid.pathAimFrame = g.frames;
    {
        const float* b = P(std::min(p.idx + 1, n));
        const float d[3] = {b[0] - k.pos[0], b[1] - k.pos[1], b[2] - k.pos[2]};
        rest = Len3(d);
    }
    for (size_t i = p.idx + 1; i < n; ++i)
    {
        const float* u = P(i);
        const float* v = P(i + 1);
        const float e[3] = {v[0] - u[0], v[1] - u[1], v[2] - u[2]};
        rest += Len3(e);
    }
    p.bestRest.Gained(rest, 50, g.time); // (progress along the way: the no-progress re-plan above)
    return true;
}

void ApplyAvoid(const fl::Kinematics& k, Out& out, u32 ignore, bool solidsClear)
{
    if (!g_set.avoid || !out.steerTo)
        return;
    flight::Model fm;
    const bool have = flight::ReadModel(fm);
    avoid::Input in;
    memcpy(in.pos, k.pos, sizeof(in.pos));
    memcpy(in.vel, k.vel, sizeof(in.vel));
    memcpy(in.aim, out.steerPos, sizeof(in.aim));
    const float r = space::PlayerRadius();
    if (r > 0)
        in.shipRadius = r;
    if (have && fm.turnRate > 0)
        in.turnRate = fm.turnRate;
    if (ignore)
        in.ignore.push_back(ignore);
    const float speed = Len3(k.vel);
    // The aim point's own structure (M12's Nomad core 22 Oct 2026: the exit waypoint inside the core's sphere - the sphere
    // "obstacle" held the ship at 8 m/s, "arrived" 1.4 km short in the radiation; M13: generators on the Dyson sphere): a
    // solid whose sphere holds the aim point is not avoided. Arrivals from outside reach it radially (ArriveAt staging).
    // ...nor a sphere the ship is inside already (session 7, 23 Oct 2026, M12 core: the radiation zone's sphere and the
    // core's held both the ship and the core point - "avoid detour" x40 at max-speed 8 m/s, no progress for 60 s in a
    // zone that kills a parked ship in ~70 s): there is nothing to go round; inside, the real hulls are the geometry
    // guard's (ray test)
    std::vector<space::Obstacle> near = space::Obstacles(k.pos, std::max(800.0f, speed * 6));
    for (auto it = near.begin(); it != near.end();)
    {
        const float a[3] = {k.pos[0] - it->pos[0], k.pos[1] - it->pos[1], k.pos[2] - it->pos[2]};
        const float b[3] = {out.steerPos[0] - it->pos[0], out.steerPos[1] - it->pos[1], out.steerPos[2] - it->pos[2]};
        // (a lethal zone - a planet's or sun's core - only when the aim lies inside it too: the trip crosses it anyway)
        const bool inside = !it->ship && !it->asteroid && Len3(a) < it->radius && (!it->zone || Len3(b) < it->radius);
        // (a clear real line: a solid's sphere is no reason to go round - its hull is not in the way)
        const bool clearSolid = solidsClear && !it->ship && !it->asteroid && !it->zone;
        it = inside || clearSolid ? near.erase(it) : std::next(it);
    }
    for (const space::Obstacle& o : near)
    {
        if (o.ship || o.asteroid || o.zone || !o.id)
            continue;
        const float b[3] = {out.steerPos[0] - o.pos[0], out.steerPos[1] - o.pos[1], out.steerPos[2] - o.pos[2]};
        if (Len3(b) < o.radius)
            in.ignore.push_back(o.id);
    }
    // A costly field here or ~3 s ahead (minefield, explosive gas, extremely dense, damaging): strict avoidance - except a
    // damage zone the ship is in already: there speed is the way out (the M12 core's radiation)
    const float ahead[3] = {k.pos[0] + k.vel[0] * 3, k.pos[1] + k.vel[1] * 3, k.pos[2] + k.vel[2] * 3};
    std::string why = space::StrictAt(k.pos);
    if (why == "damage")
        why.clear();
    else if (why.empty())
        why = space::StrictAt(ahead);
    in.strict = !why.empty();
    if (in.strict != g.avoid.strict)
    {
        g.avoid.strict = in.strict;
        events::Push("autopilot avoid strict=%d %s", in.strict ? 1 : 0, why.c_str());
    }
    const bool wasActive = g.avoid.mem.active;
    const avoid::Result res = avoid::Steer(in, near, g.avoid.mem);
    // The thrust plan stays the run's own (over the whole way): avoidance keeps a cruise and steers through (user,
    // 18 Oct 2026), so obstacles do not shorten the run a cruise is planned over (was: dropped at every detour - the
    // ship spent most of a debris field re-charging at 80 m/s).
    if (!res.blocked)
        return;
    ++g.avoid.frames;
    memcpy(out.steerPos, res.steer, sizeof(out.steerPos));
    if (g.avoid.mem.active && !wasActive)
    {
        ++g.avoid.detours;
        events::Push("autopilot avoid detour=%d extra=%.0f max-speed=%.0f obstacles=%d debris-ignored=%d",
                     g.avoid.detours, res.extra, res.maxSpeed < 1e8f ? res.maxSpeed : -1.0f, res.checked,
                     res.debrisIgnored);
    }
    // Around obstacles: sharper steering (full rate beyond ~9 degrees off).
    out.steerFullAngle = 0.15f;
    if (res.maxSpeed < 1e8f && !res.strict)
    {
        // Outside minefields (user, 18 Oct 2026): light contact and a little shield damage do not matter - keep the
        // speed (and a cruise) and turn as hard as the ship can onto the best detour (was: an "imminent hit" rule
        // dropped cruise every few seconds in a plain debris field and left the ship re-charging at 80 m/s).
        out.steerFullAngle = 0.02f;
        return;
    }
    if (res.maxSpeed < 1e8f && have && fm.cruiseEngaged && !res.imminent)
    {
        // Strict field, cruising: keep the cruise and turn hard; it goes only for a hit closer than ~0.4 s.
        out.steerFullAngle = 0.02f;
        return;
    }
    if (res.maxSpeed < 1e8f)
    {
        if (have && fm.cruiseEngaged)
            ++g.avoid.cruiseDrops; // statistics (frames): an imminent hit made the avoidance end a cruise
        if (have && fm.cruiseSpeed > res.maxSpeed)
            out.cruise = 0;
        if (have && fm.hasBurner && fm.burner.top() > res.maxSpeed)
            out.afterburn = false;
        if (speed > res.maxSpeed)
        {
            out.cruise = 0;
            out.afterburn = false;
            out.steerThrottle = 0;
            out.steerReverse = speed > res.maxSpeed + 5;
        }
    }
}
} // namespace autopilot
