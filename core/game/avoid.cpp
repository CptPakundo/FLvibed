#include "game/avoid.h"

#include <algorithm>
#include <cmath>

namespace avoid
{
namespace
{
struct V
{
    float x = 0, y = 0, z = 0;
};
V Of(const float* a) { return {a[0], a[1], a[2]}; }
V operator+(V a, V b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
V operator-(V a, V b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
V operator*(V a, float s) { return {a.x * s, a.y * s, a.z * s}; }
float Dot(V a, V b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
V Cross(V a, V b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
float Len(V a) { return std::sqrt(Dot(a, a)); }
V Unit(V a, V fallback = {0, 0, -1})
{
    const float l = Len(a);
    return l > 1e-6f ? a * (1 / l) : fallback;
}

struct Body
{
    V c;
    float radii; // both radii (contact below this)
    float need;  // + the margin: the clearance the path keeps
};

// Is the straight run from p along unit dir for len metres clear of every body (with its margin)?
bool Clear(const std::vector<Body>& bodies, V p, V dir, float len)
{
    for (const Body& b : bodies)
    {
        const V q = b.c - p;
        const float need = std::min(b.need, Len(q) * 0.98f); // already inside: only closer is blocked
        const float t = Dot(q, dir);
        if (t < -need || t > len + need)
            continue;
        const float tc = std::max(0.0f, std::min(t, len));
        if (Len(q - dir * tc) < need)
            return false;
    }
    return true;
}

// (the band's step and rounds: Params::step, Params::iterations - avoid.h)

// The clearance kept beyond an obstacle's radii (Steer and PathLength alike).
float Keep(const space::Obstacle& o, V aim, float radii, float shipRadius, float speed, float turnRate, float margin)
{
    if (!o.zone)
        return margin;
    // A lethal zone (planet / sun core) kills on contact: the game's own berth - its goto (measured 19 Oct 2026,
    // straight at planet Manhattan at cruise) circled the death zone ~600 m outside its surface - or two seconds
    // of travel for faster ships (M01b 19 Oct 2026: the normal ~15 m margin grazed Pittsburgh's zone - "DANGER").
    // From the ship's own capabilities, not fixed numbers (user, 20 Oct 2026: the swing kept 600 m outside a zone
    // already larger than the visible planet - "if Trenton is even closer to the atmosphere than the player ship,
    // surely there was no need?" - and "what if a mod increased or decreased" the speeds): the room to correct the
    // course = half a second of travel (the reaction) + half the turning radius at this speed (speed / the flight
    // model's turn rate), at least two ship radii. Starflier at cruise ~250-300 m (the 15 m that grazed Pittsburgh
    // stays far off); a modded faster or more sluggish ship gets more, a nimbler one less.
    const float turnRadius = speed / std::max(0.1f, turnRate);
    const float keep = std::max(margin, std::max(2 * shipRadius, 0.5f * speed + 0.5f * turnRadius));
    // The berth never forbids the destination (19 Oct 2026: California Minor's docking ring sits ~300 m above the
    // planet's zone - the 600 m berth pushed the ship off the bay's approach and it circled the ring for minutes):
    // where the aim point lies inside it, the berth shrinks to what the aim point leaves (the zone itself still
    // stands).
    const float aimGap = Len(aim - Of(o.pos)) - radii; // the aim point's clearance from the zone's surface
    return std::max(margin, std::min(keep, aimGap - margin));
}
} // namespace

float PathLength(const float from[3], const float to[3], const std::vector<space::Obstacle>& bodies, float shipRadius,
                 float speed, float turnRate, u32 ignore)
{
    const V a = Of(from), b = Of(to);
    const float straight = Len(b - a);
    if (straight < 1)
        return straight;
    const V u = (b - a) * (1 / straight);
    const float margin = std::max(10.0f, 0.05f * speed);
    struct Sphere
    {
        V c;
        float R;
    };
    std::vector<Sphere> spheres;
    for (const space::Obstacle& o : bodies)
    {
        if (ignore && o.id == ignore)
            continue;
        // Only bodies a run goes round as a whole (lethal zones, planets, big stations): field rocks and ships are the
        // steering's business on the way and cost little.
        if (!o.zone && (o.asteroid || o.ship || o.radius < 300))
            continue;
        const float radii = o.radius + shipRadius;
        const V c = Of(o.pos);
        float R = radii + Keep(o, b, radii, shipRadius, speed, turnRate, margin);
        R = std::min(R,
                     std::min(Len(a - c), Len(b - c)) * 0.999f); // an end inside the clearance: only round what is left
        spheres.push_back({c, R});
    }
    float extra = 0;
    for (size_t i = 0; i < spheres.size(); ++i)
    {
        const V c = spheres[i].c;
        const float R = spheres[i].R;
        // A body inside another's clearance (a planet inside its own death zone: M04 counted Leeds twice, 2046 m for the
        // ~1470 flown) is gone round with it.
        bool nested = false;
        for (size_t j = 0; j < spheres.size() && !nested; ++j)
            nested = j != i && Len(spheres[j].c - c) + R <= spheres[j].R + (j < i ? 0.01f : -0.01f);
        if (nested)
            continue;
        const float along = Dot(c - a, u);
        const float t = std::clamp(along, 0.0f, straight);
        if (Len(a + u * t - c) >= R)
            continue;
        // The steering bends only once the clearance is inside its look-ahead window (Steer: ~5 s of travel, at least
        // twice the turning radius + 10 ship radii): straight until then, from there the shortest way round the sphere -
        // the tangent from each end and the arc between them, in the plane of the two ends and the centre. (Leeds from
        // 20 km: the ideal path alone said 900 m extra, the window 1140, flown ~1220.)
        const float window = std::max(speed * 5, 2 * speed / std::max(0.1f, turnRate) + 10 * shipRadius);
        const float lat2 = Dot(c - a, c - a) - along * along, reach = R + window;
        const float bend = lat2 >= reach * reach ? 0.0f : std::clamp(along - std::sqrt(reach * reach - lat2), 0.0f, t);
        const V p = a + u * bend;
        const float cp = Len(p - c), cb = Len(b - c);
        const float theta = std::acos(std::clamp(Dot(p - c, b - c) / (cp * cb), -1.0f, 1.0f));
        const float arc = theta - std::acos(std::min(1.0f, R / cp)) - std::acos(std::min(1.0f, R / cb));
        if (arc <= 0)
            continue;
        const float around = bend + std::sqrt(cp * cp - R * R) + std::sqrt(cb * cb - R * R) + R * arc;
        extra += std::max(0.0f, around - straight);
    }
    return straight + extra;
}

Result Steer(const Input& in, const std::vector<space::Obstacle>& obstacles, Memory& mem, const Params& prm)
{
    Result r;
    memcpy(r.steer, in.aim, sizeof(r.steer));
    const V p = Of(in.pos), v = Of(in.vel), aim = Of(in.aim);
    const float goal = Len(aim - p);
    if (goal < 1)
    {
        mem.active = false;
        mem.path.clear();
        return r;
    }
    const V u = (aim - p) * (1 / goal);
    const float speed = Len(v);
    const V vdir = speed > 1 ? v * (1 / speed) : u;
    // The window: ~5 s of travel, not beyond the aim; at least what the ship needs to turn round (its turning radius at
    // this speed x 2) plus 10 ship radii - from the ship's own flight model, not a fixed 600 m (user, 20 Oct 2026:
    // adaptive to ships a mod makes faster or slower).
    const float window =
        std::min(goal, std::max(speed * 5, 2 * speed / std::max(0.1f, in.turnRate) + 10 * in.shipRadius));
    bool mines = false;
    for (const space::Obstacle& o : obstacles)
        mines |= o.mine;
    r.strict = mines || in.strict;
    // Margin: room for steering error; more in a minefield (contact is costly there).
    const float margin = (r.strict ? 2.0f : 1.0f) * std::max(10.0f, 0.05f * speed);
    std::vector<Body> bodies;
    for (const space::Obstacle& o : obstacles)
    {
        if (o.id && std::find(in.ignore.begin(), in.ignore.end(), o.id) != in.ignore.end())
            continue;
        // Small breakable debris is not avoided (user, 18-19 Oct 2026): it breaks harmlessly on contact and turns into
        // loot; mines and real rocks still are.
        if (o.breakable && !o.mine && o.radius < 40)
        {
            ++r.debrisIgnored;
            continue;
        }
        const float radii = o.radius + in.shipRadius;
        const float keep = Keep(o, aim, radii, in.shipRadius, speed, in.turnRate, margin); // the berth of a lethal zone
        Body b{Of(o.pos), radii, radii + keep};
        if (Len(b.c - p) - b.need > window + prm.step)
            continue;
        bodies.push_back(b);
    }
    r.checked = static_cast<int>(bodies.size());
    if (Clear(bodies, p, u, window))
    {
        r.clearAhead = bodies.empty() ? goal : window;
        mem.active = false;
        mem.path.clear();
        return r;
    }
    r.clearAhead = 0;
    // The chain: n + 1 points, s_i = i * step along the line to the aim.
    const int n = std::max(4, static_cast<int>(std::ceil(window / prm.step)));
    const float step = window / n;
    std::vector<V> P(n + 1);
    // Lateral offsets of the last path at the same distance along the line (warm start), else none.
    std::vector<V> oldPts;
    for (size_t i = 0; i + 2 < mem.path.size(); i += 3)
        oldPts.push_back({mem.path[i], mem.path[i + 1], mem.path[i + 2]});
    auto oldOffset = [&](float s) -> V
    {
        // the old point whose projection on the line is nearest s (lateral part only)
        V best{};
        float bestD = 1e30f;
        for (const V& q : oldPts)
        {
            const float sq = Dot(q - p, u);
            if (std::fabs(sq - s) < bestD)
            {
                bestD = std::fabs(sq - s);
                best = (q - p) - u * sq;
            }
        }
        return bestD < step * 2 ? best : V{};
    };
    for (int i = 0; i <= n; ++i)
        P[i] = p + u * (step * i) + (mem.active ? oldOffset(step * i) : V{});
    P[0] = p;
    P[1] = p + vdir * step; // the path starts along the ship's velocity
    const V end = p + u * window;
    P[n] = end;
    bool endFree = false;
    for (const Body& b : bodies)
        endFree |= Len(end - b.c) < b.need;
    // A sideways direction for points sitting right on a body's centre line.
    const V side = Unit(Cross(u, std::fabs(u.y) < 0.9f ? V{0, 1, 0} : V{1, 0, 0}));
    for (int it = 0; it < prm.iterations; ++it)
    {
        // Pull straight (shortest, least bending) ...
        for (int i = 2; i < n; ++i)
            P[i] = P[i] + ((P[i - 1] + P[i + 1]) * 0.5f - P[i]) * 0.5f;
        if (endFree)
            P[n] = P[n] + (end - P[n]) * 0.1f + (P[n - 1] + (P[n - 1] - P[n - 2]) - P[n]) * 0.4f;
        // ... and out of every clearance.
        for (int i = 2; i <= n; ++i)
        {
            if (i == n && !endFree)
                continue;
            for (const Body& b : bodies)
            {
                const V d = P[i] - b.c;
                const float l = Len(d);
                if (l < b.need)
                {
                    const V away = l > 1e-3f ? d * (1 / l) : Unit(side - u * Dot(side, u), side);
                    P[i] = b.c + away * b.need;
                }
            }
        }
    }
    // The bends against the turning circle: the direction may change by at most step x turn rate / speed per step.
    const float allowed = speed > 1 && in.turnRate > 0 ? step * in.turnRate / speed : 1e9f;
    float worst = 0, worstAt = 0, length = 0, maxOffset = 0;
    for (int i = 1; i <= n; ++i)
    {
        length += Len(P[i] - P[i - 1]);
        maxOffset = std::max(maxOffset, Len((P[i] - p) - u * Dot(P[i] - p, u)));
        if (i < n)
        {
            const V a = Unit(P[i] - P[i - 1]), b2 = Unit(P[i + 1] - P[i]);
            const float bend = std::acos(std::max(-1.0f, std::min(1.0f, Dot(a, b2))));
            if (bend / allowed > worst)
                worst = bend / allowed, worstAt = step * i;
        }
        for (const Body& b : bodies)
            r.residual = std::max(r.residual, b.radii - Len(P[i] - b.c));
    }
    r.extra = length - window;
    r.blocked = maxOffset > 2;
    if (r.strict && worst > 1)
    {
        r.maxSpeed = speed / worst; // the bends fit the turning circle at this speed
        r.imminent = worstAt < std::max(50.0f, speed * 0.4f);
    }
    // Steer to the path point ~0.4 s ahead (at least 75 m).
    const float look = std::max(75.0f, speed * 0.4f);
    const int li = std::min(n, std::max(1, static_cast<int>(std::lround(look / step))));
    r.steer[0] = P[li].x, r.steer[1] = P[li].y, r.steer[2] = P[li].z;
    mem.active = r.blocked;
    mem.path.clear();
    for (const V& q : P)
        mem.path.insert(mem.path.end(), {q.x, q.y, q.z});
    return r;
}

FanResult RayFan(const float pos[3], const float wanted[3], float look, float shipRadius, const u32* ignore,
                 int nIgnore)
{
    FanResult r;
    V w = Of(wanted);
    const float wl = Len(w);
    if (wl < 1e-3f || look <= 0)
        return r;
    w = w * (1.0f / wl);
    // a basis round the wanted direction
    V side = Cross(w, V{0, 1, 0});
    if (Len(side) < 0.2f)
        side = Cross(w, V{1, 0, 0});
    side = side * (1.0f / Len(side));
    const V up = Cross(side, w);
    const V p = Of(pos);
    const float halfWidth = std::max(5.0f, shipRadius) * 1.2f;
    // how far a direction is clear: the nearest hit of three rays - the centre and both sides of the ship, across the
    // turn away from `w` (the sides are what scrape a wall)
    auto clearOf = [&](V d)
    {
        V across = Cross(d, up);
        if (Len(across) < 0.2f)
            across = side;
        across = across * (1.0f / Len(across));
        float c = look;
        for (int k = -1; k <= 1; ++k)
        {
            const V from = p + across * (halfWidth * static_cast<float>(k));
            const V to = from + d * look;
            const float f[3] = {from.x, from.y, from.z}, t[3] = {to.x, to.y, to.z};
            const float h = space::RayHitDistance(f, t, ignore, nIgnore);
            ++r.rays;
            if (h >= 0)
                c = std::min(c, h);
        }
        return c;
    };
    const float c0 = clearOf(w);
    r.dir[0] = w.x, r.dir[1] = w.y, r.dir[2] = w.z;
    r.clear = c0;
    if (c0 >= look)
        return r;
    r.blocked = true;
    static const float kRings[] = {12, 25, 40, 60, 85};
    V best = w;
    float bestClear = c0;
    for (float deg : kRings)
    {
        const float a = deg * 3.14159265f / 180.0f, ca = std::cos(a), sa = std::sin(a);
        V ringBest{};
        float ringClear = -1;
        for (int i = 0; i < 8; ++i)
        {
            const float b = static_cast<float>(i) * 3.14159265f / 4.0f;
            const V d = w * ca + (side * std::cos(b) + up * std::sin(b)) * sa;
            const float c = clearOf(d);
            if (c > ringClear)
                ringClear = c, ringBest = d;
        }
        if (ringClear > bestClear)
            bestClear = ringClear, best = ringBest;
        if (ringClear >= look) // the nearest ring with a clear way: done
            break;
    }
    r.dir[0] = best.x, r.dir[1] = best.y, r.dir[2] = best.z;
    r.clear = bestClear;
    r.brake = bestClear < look;
    return r;
}

// ---- the rotating part's crossing (WheelScan) ----
namespace
{
// (the grid - cells per side, m apart - and the angle bins round the pivot: Params::scanN, scanStep, bins - avoid.h)
struct ScanHits
{
    std::vector<V> part, still; // hit cells: the moving part's / other solids'
};
ScanHits ScanGrid(const V& centre, const V& dir, const V& e1, const V& e2, u32 part, const Params& prm)
{
    const int kScanN = prm.scanN;
    const float kScanStep = prm.scanStep;
    ScanHits h;
    for (int i = 0; i < kScanN; ++i)
        for (int j = 0; j < kScanN; ++j)
        {
            const V p = centre + e1 * ((i - kScanN / 2) * kScanStep) + e2 * ((j - kScanN / 2) * kScanStep);
            const V f = p - dir * 120.0f, t = p + dir * 120.0f;
            const float ff[3] = {f.x, f.y, f.z}, tt[3] = {t.x, t.y, t.z};
            u32 id = 0;
            const u32 cls = space::SightBlocker(ff, tt, 0, &id);
            if (cls == 0x1 && (!part || id == part))
                h.part.push_back(p);
            else if (cls)
                h.still.push_back(p);
        }
    return h;
}
V Centroid(const std::vector<V>& v)
{
    V c{};
    for (const V& p : v)
        c = c + p;
    return v.empty() ? c : c * (1.0f / static_cast<float>(v.size()));
}
// occupancy per angle bin for cells within `band` of radius r (r < 0: every cell beyond 60 m)
std::vector<float> AngleHist(const std::vector<V>& cells, const V& pivot, const V& e1, const V& e2, float r, float band,
                             int kBins)
{
    std::vector<float> h(kBins, 0.0f);
    for (const V& p : cells)
    {
        const V d = p - pivot;
        const float x = Dot(d, e1), y = Dot(d, e2), rr = std::sqrt(x * x + y * y);
        if (r < 0 ? rr < 60 : std::fabs(rr - r) > band)
            continue;
        float a = std::atan2(y, x);
        if (a < 0)
            a += 6.2831853f;
        h[static_cast<int>(a / 6.2831853f * kBins) % kBins] += 1.0f;
    }
    return h;
}
} // namespace

bool WheelScanStep(WheelScan& w, const float hit[3], const float dir[3], u32 part, float now, std::string* info,
                   const Params& prm)
{
    const int kBins = prm.bins;
    const float kScanStep = prm.scanStep;
    V d = Of(dir);
    d = d * (1.0f / std::max(1e-3f, Len(d)));
    if (w.phase == 0)
    {
        V e1 = Cross(d, V{0, 1, 0});
        if (Len(e1) < 0.2f)
            e1 = Cross(d, V{1, 0, 0});
        e1 = e1 * (1.0f / Len(e1));
        const V e2 = Cross(d, e1);
        const ScanHits h = ScanGrid(Of(hit), d, e1, e2, part, prm);
        if (h.part.empty())
        {
            w.phase = -1;
            return false;
        }
        w.part = part;
        memcpy(w.plane, hit, sizeof(w.plane));
        w.dir[0] = d.x, w.dir[1] = d.y, w.dir[2] = d.z;
        w.e1[0] = e1.x, w.e1[1] = e1.y, w.e1[2] = e1.z;
        w.e2[0] = e2.x, w.e2[1] = e2.y, w.e2[2] = e2.z;
        const V c = Centroid(h.part);
        w.pivot[0] = c.x, w.pivot[1] = c.y, w.pivot[2] = c.z;
        // (the first scan's cells kept as points, flattened, in mask0: x y z per cell as bytes is no use - keep floats)
        w.mask0.assign(reinterpret_cast<const unsigned char*>(h.part.data()),
                       reinterpret_cast<const unsigned char*>(h.part.data() + h.part.size()));
        w.t0 = now;
        w.phase = 1;
        return true;
    }
    if (w.phase != 1)
        return w.phase == 2;
    const V e1 = Of(w.e1), e2 = Of(w.e2);
    // the second scan centred on the first one's centroid (a part reaching past the first grid biased it)
    const ScanHits h = ScanGrid(Of(w.pivot), d, e1, e2, w.part, prm);
    if (h.part.size() < 4)
    {
        w.phase = -1;
        return false;
    }
    const V pivot = Centroid(h.part);
    std::vector<V> first(w.mask0.size() / sizeof(V));
    memcpy(first.data(), w.mask0.data(), first.size() * sizeof(V));
    // the spin: the angular shift (+-29 deg: a six-fold part repeats at 60) that best maps scan 1 onto scan 2
    const std::vector<float> a1 = AngleHist(first, pivot, e1, e2, -1, 0, kBins),
                             a2 = AngleHist(h.part, pivot, e1, e2, -1, 0, kBins);
    int best = 0;
    float bestC = -1;
    for (int s = -kBins / 12; s <= kBins / 12; ++s)
    {
        float c = 0;
        for (int b = 0; b < kBins; ++b)
            c += a1[b] * a2[(b + s + kBins) % kBins];
        if (c > bestC)
            bestC = c, best = s;
    }
    const float dt = std::max(0.05f, now - w.t0);
    w.spin = static_cast<float>(best) * 6.2831853f / kBins / dt;
    // the ring: the radius with the most free angle (the part and the solids still there), outside the hub
    float maxR = 0;
    for (const V& p : h.part)
        maxR = std::max(maxR, Len(p - pivot));
    // (the smallest ring at least half free, else the freest: the freest was often outside the spokes, 400+ m off the axis -
    // s9x-i1 crossed at 410 m, could not regain the axis before the small hole at z -400 .. -600 and struck its rim; the
    // user's try 3 crossed ~190 m out, between the spokes, timed to a gap)
    float bestR = 0, bestFree = -1, halfR = 0, halfFree = 0;
    std::vector<unsigned char> bestMask, halfMask;
    for (float r = 60; r <= maxR + 60; r += 10)
    {
        const std::vector<float> hp = AngleHist(h.part, pivot, e1, e2, r, 15, kBins),
                                 hs = AngleHist(h.still, pivot, e1, e2, r, 15, kBins);
        int occ = 0;
        std::vector<unsigned char> m(kBins);
        for (int b = 0; b < kBins; ++b)
        {
            // (a bin the grid cannot sample at this radius - fewer cells than bins near the hub - counts as unknown: busy)
            m[b] = hs[b] > 0 ? 2 : hp[b] > 0 ? 1 : 0;
            occ += m[b] ? 1 : 0;
        }
        // cells per bin at this radius: r * 2pi / kBins m of arc vs the 25 m grid - a band below one cell per bin reads
        // empty by sampling, not by space
        if (r * 6.2831853f / kBins < kScanStep * 0.8f)
            continue;
        const float freeFrac = 1.0f - static_cast<float>(occ) / kBins;
        if (freeFrac > bestFree)
            bestFree = freeFrac, bestR = r, bestMask = m;
        if (halfR <= 0 && freeFrac >= 0.5f)
            halfR = r, halfFree = freeFrac, halfMask = m;
    }
    if (halfR > 0)
        bestR = halfR, bestFree = halfFree, bestMask = halfMask;
    if (bestR <= 0)
    {
        w.phase = -1;
        return false;
    }
    w.pivot[0] = pivot.x, w.pivot[1] = pivot.y, w.pivot[2] = pivot.z;
    w.ring = bestR;
    w.ringFree = bestFree;
    w.ringMask = bestMask;
    w.t0 = now;
    w.phase = 2;
    if (info)
    {
        char b[200];
        sprintf_s(b, "pivot (%.0f,%.0f,%.0f) spin %.0f deg/s ring %.0f m free %.0f %% cells %u/%u maxR %.0f", pivot.x,
                  pivot.y, pivot.z, w.spin * 57.2958f, bestR, bestFree * 100, static_cast<unsigned>(first.size()),
                  static_cast<unsigned>(h.part.size()), maxR);
        *info = b;
    }
    return true;
}

bool WheelAim(const WheelScan& w, const float from[3], float now, float tArrive, float aim[3], const Params& prm)
{
    const int kBins = prm.bins;
    if (w.phase != 2 || w.ringMask.size() != static_cast<size_t>(kBins))
        return false;
    const V pivot = Of(w.pivot), e1 = Of(w.e1), e2 = Of(w.e2);
    const V rel = Of(from) - pivot;
    float self = std::atan2(Dot(rel, e2), Dot(rel, e1));
    // the part's turn by the arrival: a bin b of the mask (at t0) sits at b + spin * (T - t0)
    const float turn = w.spin * (now + tArrive - w.t0);
    const float binA = 6.2831853f / kBins;
    // margin: the timing's error (0.35 s of spin) + half a bin each side
    const int margin = 1 + static_cast<int>(std::fabs(w.spin) * 0.35f / binA);
    auto busyAt = [&](float ang)
    {
        // the mask bin now covering angle `ang`: moving parts turned, solids still
        float a0 = ang - turn;
        a0 = std::fmod(a0, 6.2831853f);
        if (a0 < 0)
            a0 += 6.2831853f;
        float as = std::fmod(ang, 6.2831853f);
        if (as < 0)
            as += 6.2831853f;
        const unsigned char m = w.ringMask[static_cast<int>(a0 / binA) % kBins];
        const unsigned char s = w.ringMask[static_cast<int>(as / binA) % kBins];
        return m == 1 || s == 2;
    };
    for (int k = 0; k <= kBins / 2; ++k)
        for (int sgn : {1, -1})
        {
            const float ang = self + sgn * k * binA;
            bool ok = true;
            for (int m = -margin; m <= margin && ok; ++m)
                ok = !busyAt(ang + m * binA);
            if (!ok)
                continue;
            const V p = pivot + (e1 * std::cos(ang) + e2 * std::sin(ang)) * w.ring;
            aim[0] = p.x, aim[1] = p.y, aim[2] = p.z;
            return true;
        }
    return false;
}

static std::vector<float> RayPathAt(const float from[3], const float to[3], float halfWidth, int mode,
                                    const u32* ignore, int nIgnore, int& rays, const Params& prm);
static const char* g_lastMode = "none";

// The ship-width clear test of the planners: the centre line and four parallel lines `halfWidth` off it.
static bool ClearFor(V p, V q, float halfWidth, const u32* ignore, int nIgnore, int& rays)
{
    V d = q - p;
    const float dl = Len(d);
    if (dl < 1)
        return true;
    d = d * (1.0f / dl);
    V s = Cross(d, V{0, 1, 0});
    if (Len(s) < 0.2f)
        s = Cross(d, V{1, 0, 0});
    s = s * (1.0f / Len(s));
    const V u = Cross(s, d);
    const V offs[5] = {V{0, 0, 0}, s * halfWidth, s * -halfWidth, u * halfWidth, u * -halfWidth};
    for (const V& o : offs)
    {
        const V f = p + o, t = q + o;
        const float ff[3] = {f.x, f.y, f.z}, tt[3] = {t.x, t.y, t.z};
        ++rays;
        if (space::RayHitDistance(ff, tt, ignore, nIgnore) >= 0)
            return false;
    }
    return true;
}

// The 3D way (session 7, 23 Oct 2026, M12 generator 3 -> the airlock's top: the user's try 3 first runs ~180 m along the
// trench, then climbs to y ~400 - out of any one plane through both ends; the planes found nothing or the 3.5 km way round):
// lazy A* over a lattice in the box round both ends (+600 m, 120 m cells - coarser when the box is big), the 26 neighbours
// of each cell, each edge ray-tested for the ship's width when first used; string-pulled.
static std::vector<float> RayPathGrid(const float from[3], const float to[3], float halfWidth, const u32* ignore,
                                      int nIgnore, int& rays, const Params& prm)
{
    const V a = Of(from), b = Of(to);
    V lo{std::min(a.x, b.x) - 600, std::min(a.y, b.y) - 600, std::min(a.z, b.z) - 600};
    V hi{std::max(a.x, b.x) + 600, std::max(a.y, b.y) + 600, std::max(a.z, b.z) + 600};
    float step = 120;
    int nx, ny, nz;
    for (;;)
    {
        nx = static_cast<int>((hi.x - lo.x) / step) + 1;
        ny = static_cast<int>((hi.y - lo.y) / step) + 1;
        nz = static_cast<int>((hi.z - lo.z) / step) + 1;
        if (static_cast<long long>(nx) * ny * nz <= 40000)
            break;
        step *= 1.25f;
    }
    const int cells = nx * ny * nz, start = cells, goal = cells + 1, N = cells + 2;
    auto pos = [&](int i)
    {
        if (i == start)
            return a;
        if (i == goal)
            return b;
        const int x = i % nx, y = (i / nx) % ny, z = i / (nx * ny);
        return V{lo.x + x * step, lo.y + y * step, lo.z + z * step};
    };
    auto cellOf = [&](const V& p)
    {
        const int x = std::max(0, std::min(nx - 1, static_cast<int>((p.x - lo.x) / step + 0.5f)));
        const int y = std::max(0, std::min(ny - 1, static_cast<int>((p.y - lo.y) / step + 0.5f)));
        const int z = std::max(0, std::min(nz - 1, static_cast<int>((p.z - lo.z) / step + 0.5f)));
        return x + nx * (y + ny * z);
    };
    std::vector<float> gc(N, 1e30f);
    std::vector<int> prev(N, -1);
    std::vector<char> closed(N, 0);
    std::vector<std::pair<float, int>> open; // (f = g + h, node) as a min-heap
    const auto push = [&](int v, float g)
    {
        open.push_back({-(g + Len(b - pos(v))), v});
        std::push_heap(open.begin(), open.end());
    };
    gc[start] = 0;
    push(start, 0);
    const int goalCell = cellOf(b);
    // (Params::latticeBudget per attempt, 15 000: at 30 000 the wide berth's search from inside the M12 trench used up the
    // whole budget - 80 000 rays, 167 ms, nothing found - before the narrower berths ran)
    const int kBudget = prm.latticeBudget;
    const int raysAt = rays;
    while (!open.empty() && rays - raysAt < kBudget)
    {
        std::pop_heap(open.begin(), open.end());
        const int u = open.back().second;
        open.pop_back();
        if (closed[u])
            continue;
        closed[u] = 1;
        if (u == goal)
            break;
        const V pu = pos(u);
        // neighbours: the 26 round a cell (the start: the cells round its own), and the goal from its cell's surroundings
        const int cu = u == start ? cellOf(a) : u;
        const int x0 = cu % nx, y0 = (cu / nx) % ny, z0 = cu / (nx * ny);
        auto relax = [&](int v)
        {
            if (closed[v])
                return;
            const V pv = pos(v);
            const float w = Len(pv - pu);
            if (gc[u] + w >= gc[v])
                return;
            if (!ClearFor(pu, pv, halfWidth, ignore, nIgnore, rays))
                return;
            gc[v] = gc[u] + w;
            prev[v] = u;
            push(v, gc[v]);
        };
        // (the start: two cells round it - from inside a trench the 26 next cells were all blocked for the berth and the search
        // found nothing, M12 generator 3, session 7)
        const int reach = u == start ? 2 : 1;
        for (int dz = -reach; dz <= reach; ++dz)
            for (int dy = -reach; dy <= reach; ++dy)
                for (int dx = -reach; dx <= reach; ++dx)
                {
                    const int x = x0 + dx, y = y0 + dy, z = z0 + dz;
                    if (x < 0 || y < 0 || z < 0 || x >= nx || y >= ny || z >= nz)
                        continue;
                    const int v = x + nx * (y + ny * z);
                    if (v != u)
                        relax(v);
                }
        {
            const int gx = goalCell % nx, gy = (goalCell / nx) % ny, gz = goalCell / (nx * ny);
            if (std::abs(gx - x0) <= 2 && std::abs(gy - y0) <= 2 && std::abs(gz - z0) <= 2)
                relax(goal);
        }
    }
    std::vector<float> out;
    if (prev[goal] < 0)
        return out;
    std::vector<int> chain;
    for (int v = goal; v >= 0; v = prev[v])
        chain.push_back(v);
    std::reverse(chain.begin(), chain.end());
    size_t i = 0;
    while (i + 1 < chain.size())
    {
        size_t j = chain.size() - 1;
        while (j > i + 1 && !ClearFor(pos(chain[i]), pos(chain[j]), halfWidth, ignore, nIgnore, rays))
            --j;
        const V p = pos(chain[j]);
        out.insert(out.end(), {p.x, p.y, p.z});
        i = j;
    }
    return out;
}

std::vector<float> RayPath(const float from[3], const float to[3], float shipRadius, const u32* ignore, int nIgnore,
                           int* raysOut, bool planesFirst, float narrowGain, const Params& prm)
{
    // a wide berth first (session 7, M12: a line 11-27 m off the trench walls set off the wall slide and the guard, which
    // ended the cruise), the ship's own width only when no wide way exists; the level plane first in each (the berths:
    // Params::berths)
    int rays = 0;
    std::vector<float> p;
    g_lastMode = "none";
    const auto lengthOf = [&](const std::vector<float>& q)
    {
        float len = 0;
        V last = Of(from);
        for (size_t i = 0; i + 2 < q.size(); i += 3)
        {
            const V v{q[i], q[i + 1], q[i + 2]};
            len += Len(v - last);
            last = v;
        }
        return len;
    };
    // (a channel's middle first: the shortest way hugs a curved trench's inner wall - M12 hop, session 7: zigzag between
    // the walls, a crash at 296 m/s; the user's try 3 holds the middle - an 80 m berth forces a centred line)
    // Every plane of a berth is tried and the shortest way kept (session 7, M12 generator 3 -> the airlock's top: the level
    // plane's 3.6 km round the lair was taken first; the user's try 3 climbs straight to y ~400 - 2.1 km, the vertical
    // plane's way); the next berth only when none of its planes found one.
    // (narrowGain > 0: after a wide berth's way, the next berth too - kept when shorter by that fraction; the M12 trench's
    // hoop frames have windows ~60-120 m across (3D probe, 24 Oct 2026): the 80 m berth never fits one, its way runs
    // round them, 3.9 km for 3.1 km straight; the user's try 3 threads them, ~3.3 km)
    float wideLen = 1e30f;
    bool narrowTried = false;
    for (const Params::Berth& berth : prm.berths)
    {
        const float w = std::max(berth.floor, shipRadius * berth.radii);
        if (!p.empty())
        {
            if (narrowGain <= 0 || narrowTried)
                break;
            narrowTried = true;
            wideLen = lengthOf(p);
            std::vector<float> keep(p);
            const char* keepMode = g_lastMode;
            p.clear();
            float nb = 1e30f;
            for (const int mode : {0, 1, 2})
            {
                if (rays >= prm.rayBudget)
                    break;
                std::vector<float> q = RayPathAt(from, to, w, mode, ignore, nIgnore, rays, prm);
                if (!q.empty() && lengthOf(q) < nb)
                    nb = lengthOf(q), p.swap(q);
            }
            if (p.empty() || nb > wideLen * (1.0f - narrowGain))
            {
                p.swap(keep);
                g_lastMode = keepMode;
            }
            else
            {
                static char m2[48];
                sprintf_s(m2, "narrow plane, berth %.0f m (wide %.0f m)", w, wideLen);
                g_lastMode = m2;
            }
            break;
        }
        float bestLen = 1e30f;
        for (const int mode : {0, 1, 2, 3}) // level, vertical (over / under), solid-centre planes; 3D lattice
        {
            // (ray budget Params::rayBudget, 80 000 in all: 12 000 measured 28 ms, session 7)
            if ((w >= prm.noCentrePlaneBerth && mode == 2) || rays >= prm.rayBudget ||
                (planesFirst && (mode == 3 || !p.empty())))
                continue;
            std::vector<float> q = mode == 3 ? RayPathGrid(from, to, w, ignore, nIgnore, rays, prm)
                                             : RayPathAt(from, to, w, mode, ignore, nIgnore, rays, prm);
            if (q.empty())
                continue;
            const float len = lengthOf(q);
            if (len < bestLen)
            {
                bestLen = len;
                p.swap(q);
                static char m[48];
                sprintf_s(m, "%s, berth %.0f m",
                          mode == 0   ? "level plane"
                          : mode == 1 ? "vertical plane"
                          : mode == 2 ? "solid-centre plane"
                                      : "3D lattice",
                          w);
                g_lastMode = m;
            }
        }
    }
    if (raysOut)
        *raysOut = rays;
    return p;
}

const char* RayPathMode() { return g_lastMode; }

static std::vector<float> RayPathAt(const float from[3], const float to[3], float halfWidth, int mode,
                                    const u32* ignore, int nIgnore, int& rays, const Params& prm)
{
    const V a = Of(from), b = Of(to);
    // the plane: through both ends and the centre of the biggest solid the straight line crosses (the lair round its trench)
    const V mid = (a + b) * 0.5f;
    const float segLen = Len(b - a);
    V c = mid;
    float bestR = 0;
    const float m[3] = {mid.x, mid.y, mid.z};
    for (const space::Obstacle& o : space::Obstacles(m, segLen * 0.5f + 500))
    {
        if (o.ship || o.asteroid || o.zone || o.radius <= bestR)
            continue;
        const V oc = Of(o.pos);
        const V ab = b - a;
        const float t = std::max(0.0f, std::min(1.0f, Dot(oc - a, ab) / std::max(1.0f, Dot(ab, ab))));
        if (Len(a + ab * t - oc) < o.radius)
            bestR = o.radius, c = oc;
    }
    V n{};
    if (mode == 1)
    {
        // the vertical plane through both ends: its normal horizontal, across the line - over or under what is in the way
        const V d = (b - a) * (1.0f / std::max(1.0f, segLen));
        n = Cross(d, V{0, 1, 0});
        if (Len(n) < 0.2f)
            return {};
        n = n * (1.0f / Len(n));
        c = c - n * Dot(c - a, n);
    }
    else if (mode == 0)
    {
        // the level plane through both ends (session 7, M12: the lair's trench lies in the system's plane - the solid-centre
        // plane pivoted on a shield dome 284 m up, and the hop left the trench by +-250 m): its normal = world up off the line;
        // the grid's centre = the solid's centre dropped into it
        const V d = (b - a) * (1.0f / std::max(1.0f, segLen));
        n = V{0, 1, 0} - d * d.y;
        if (Len(n) < 0.2f)
            return {}; // a near-vertical line has no level plane
        n = n * (1.0f / Len(n));
        c = c - n * Dot(c - a, n);
    }
    else
    {
        n = Cross(a - c, b - c);
        if (Len(n) < 1e-3f * std::max(1.0f, Len(a - c) * Len(b - c)))
        {
            n = Cross(b - a, V{0, 1, 0});
            if (Len(n) < 1e-3f)
                n = Cross(b - a, V{1, 0, 0});
            if (Len(c - mid) < 1.0f)
                c = mid;
        }
        n = n * (1.0f / Len(n));
    }
    // clear for the ship's width: the centre line and both sides across it, in the plane and out of it
    auto clear = [&](V p, V q)
    {
        V d = q - p;
        const float dl = Len(d);
        if (dl < 1)
            return true;
        d = d * (1.0f / dl);
        V across = Cross(d, n);
        if (Len(across) < 0.2f)
            across = Cross(d, V{0, 1, 0});
        across = across * (1.0f / std::max(1e-3f, Len(across)));
        const V offs[5] = {V{0, 0, 0}, across * halfWidth, across * -halfWidth, n * halfWidth, n * -halfWidth};
        for (const V& o : offs)
        {
            const V f = p + o, t = q + o;
            const float ff[3] = {f.x, f.y, f.z}, tt[3] = {t.x, t.y, t.z};
            ++rays;
            if (space::RayHitDistance(ff, tt, ignore, nIgnore) >= 0)
                return false;
        }
        return true;
    };
    std::vector<float> out;
    if (clear(a, b))
    {
        out = {to[0], to[1], to[2]};
        return out;
    }
    // nodes: polar rings round c in the plane (0.75 .. 1.25 of the ends' radii, 7.5 deg apart), the two ends
    V e1 = a - c;
    if (Len(e1) < 1)
        e1 = Cross(n, V{0, 1, 0});
    e1 = e1 * (1.0f / Len(e1));
    const V e2 = Cross(n, e1);
    const float ra = Len(a - c), rb = Len(b - c);
    // (inner rings to half the ends' radius: the user's try-3 hops dip to 0.58 of the generators' radius)
    const float r0 = std::max(50.0f, std::min(ra, rb) * 0.5f), r1 = std::max(ra, rb) * 1.25f + 100;
    // (Params::rings x angles, 13 rings x 72 directions: the M12 trench is ~200 m wide - 7 x 48 left nodes ~250 m apart and
    // the line struck its outer wall, session 7)
    const int kRings = prm.rings, kAngles = prm.angles;
    std::vector<V> node;
    node.reserve(kRings * kAngles + 2);
    for (int i = 0; i < kRings; ++i)
    {
        const float r = r0 + (r1 - r0) * static_cast<float>(i) / (kRings - 1);
        for (int j = 0; j < kAngles; ++j)
        {
            const float ang = static_cast<float>(j) * 6.2831853f / kAngles;
            node.push_back(c + (e1 * std::cos(ang) + e2 * std::sin(ang)) * r);
        }
    }
    // ...and a line of nodes through the destination and the solid's centre, out past the centre (session 7, M12 core
    // exit: the way out runs along the core's axis through a 90 m hole - no polar node lands on it), every 60 m
    const int gridEnd = static_cast<int>(node.size());
    {
        V axis = c - b;
        const float al = Len(axis);
        if (al > 50)
        {
            axis = axis * (1.0f / al);
            for (float s = 60; s < al * 2 && node.size() < static_cast<size_t>(gridEnd + 80); s += 60)
                node.push_back(b + axis * s);
        }
    }
    const int lineEnd = static_cast<int>(node.size());
    const int start = lineEnd, goal = start + 1;
    node.push_back(a);
    node.push_back(b);
    const int N = static_cast<int>(node.size());
    std::vector<float> dist(N, 1e30f);
    std::vector<int> prev(N, -1);
    std::vector<char> done(N, 0);
    dist[start] = 0;
    auto neighbours = [&](int u, std::vector<int>& nb)
    {
        nb.clear();
        if (u < gridEnd)
        {
            const int ri = u / kAngles, ai = u % kAngles;
            for (int dr = -1; dr <= 1; ++dr)
                for (int da = -2; da <= 2; ++da)
                {
                    const int r = ri + dr;
                    if ((dr || da) && r >= 0 && r < kRings)
                        nb.push_back(r * kAngles + (ai + da + kAngles) % kAngles);
                }
        }
        // the line's nodes: their neighbours on the line, and grid / line nodes within 400 m of them (both ways)
        if (u >= gridEnd && u < lineEnd)
        {
            if (u > gridEnd)
                nb.push_back(u - 1);
            if (u + 1 < lineEnd)
                nb.push_back(u + 1);
        }
        for (int v = gridEnd; v < lineEnd; ++v)
            if (v != u && u != goal && Len(node[v] - node[u]) < 400)
                nb.push_back(v);
        if (u >= gridEnd && u < lineEnd)
            for (int v = 0; v < gridEnd; ++v)
                if (Len(node[v] - node[u]) < 400)
                    nb.push_back(v);
        // the ends join every node near them; every node tries the goal
        for (int v = 0; v < start; ++v)
            if (u == start && Len(node[v] - a) < 700)
                nb.push_back(v);
        if (u != goal)
            nb.push_back(goal);
    };
    std::vector<int> nb;
    const int kMaxRays = prm.planeBudget; // per plane and berth (the caller tries up to six)
    const int raysAtStart = rays;
    while (rays - raysAtStart < kMaxRays)
    {
        int u = -1;
        for (int v = 0; v < N; ++v)
            if (!done[v] && dist[v] < 1e29f && (u < 0 || dist[v] < dist[u]))
                u = v;
        if (u < 0 || u == goal)
            break;
        done[u] = 1;
        neighbours(u, nb);
        for (int v : nb)
        {
            if (done[v])
                continue;
            const float w = Len(node[v] - node[u]);
            if (dist[u] + w >= dist[v])
                continue;
            if (v == goal && w > 3000)
                continue; // the goal only from close by (long rays cost and say little)
            if (!clear(node[u], node[v]))
                continue;
            dist[v] = dist[u] + w;
            prev[v] = u;
        }
    }
    if (prev[goal] < 0)
        return out; // no way found: the caller keeps its own steering
    std::vector<int> chain;
    for (int v = goal; v >= 0; v = prev[v])
        chain.push_back(v);
    std::reverse(chain.begin(), chain.end());
    // string-pulling: from each kept point to the farthest one in plain sight
    size_t i = 0;
    while (i + 1 < chain.size())
    {
        size_t j = chain.size() - 1;
        while (j > i + 1 && !clear(node[chain[i]], node[chain[j]]))
            --j;
        const V& p = node[chain[j]];
        out.insert(out.end(), {p.x, p.y, p.z});
        i = j;
    }
    return out;
}
} // namespace avoid
