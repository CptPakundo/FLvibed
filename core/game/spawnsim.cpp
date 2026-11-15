// The encounter spawn simulator's core (spawnsim.h): the heartbeat, the lists, the roll, the encounter / faction draw, the
// placement and the life counter of docs\kb\spawning.md, rule names (H1, L1, D3, E4, G2, X1 ...) as there.
#include "spawnsim.h"

#include <algorithm>
#include <cmath>

namespace spawnsim
{
namespace
{
const double kPi = 3.14159265358979323846;

Vec3 Add(const Vec3& a, const Vec3& b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 Sub(const Vec3& a, const Vec3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 Mul(const Vec3& a, double s) { return {a.x * s, a.y * s, a.z * s}; }
double Dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
double Len(const Vec3& a) { return std::sqrt(Dot(a, a)); }

// The game's rand(): MSVC's 15-bit LCG, so a draw `rand() * n / 32768` reads like the code it models.
struct Rng
{
    uint32_t s;
    int Next()
    {
        s = s * 214013u + 2531011u;
        return static_cast<int>((s >> 16) & 0x7fff);
    }
    double Unit() { return Next() / 32768.0; }
    int Below(int n) { return static_cast<int>(static_cast<long long>(Next()) * n / 32768); }
};

// World -> zone local (R is local -> world, so local = R^T (p - pos)).
Vec3 ToLocal(const Zone& z, const Vec3& p)
{
    const Vec3 d = Sub(p, z.pos);
    const double* r = z.rot;
    return {r[0] * d.x + r[3] * d.y + r[6] * d.z, r[1] * d.x + r[4] * d.y + r[7] * d.z,
            r[2] * d.x + r[5] * d.y + r[8] * d.z};
}

Vec3 Forward(double yaw) { return {std::sin(yaw), 0, std::cos(yaw)}; }

// The direction (x, y, z) at yaw + h, pitch v.
Vec3 Dir(double yaw, double h, double v)
{
    return {std::sin(yaw + h) * std::cos(v), std::sin(v), std::cos(yaw + h) * std::cos(v)};
}

struct Live
{
    int zone, enc, form, faction, ships;
    Vec3 pos;
    Flight flight;
    double life, longevity;
    bool hostile, inDisc, met, sighted;
};

struct EncState
{
    int perm = -1;
    double pickedAt = -1e9;
    int created = 0;
};

int PickWeighted(Rng& rng, const std::vector<double>& w)
{
    double sum = 0;
    for (double x : w)
        sum += x > 0 ? x : 0;
    if (sum <= 0)
        return -1;
    double r = rng.Unit() * sum;
    for (size_t i = 0; i < w.size(); ++i)
    {
        if (w[i] <= 0)
            continue;
        if (r < w[i])
            return static_cast<int>(i);
        r -= w[i];
    }
    for (size_t i = w.size(); i-- > 0;)
        if (w[i] > 0)
            return static_cast<int>(i);
    return -1;
}
} // namespace

void RotationMatrix(double rx, double ry, double rz, double out[9])
{
    const double a = rx * kPi / 180, b = ry * kPi / 180, c = rz * kPi / 180;
    const double ca = std::cos(a), sa = std::sin(a), cb = std::cos(b), sb = std::sin(b), cc = std::cos(c), sc = std::sin(c);
    // Rz * Ry * Rx
    out[0] = cc * cb;
    out[1] = cc * sb * sa - sc * ca;
    out[2] = cc * sb * ca + sc * sa;
    out[3] = sc * cb;
    out[4] = sc * sb * sa + cc * ca;
    out[5] = sc * sb * ca - cc * sa;
    out[6] = -sb;
    out[7] = cb * sa;
    out[8] = cb * ca;
}

bool InZone(const Zone& z, const Vec3& p, double m)
{
    const Vec3 l = ToLocal(z, p);
    const double* s = z.size;
    switch (z.shape)
    {
    case Shape::Box:
        return std::fabs(l.x) <= s[0] / 2 + m && std::fabs(l.y) <= s[1] / 2 + m && std::fabs(l.z) <= s[2] / 2 + m;
    case Shape::Cylinder: // radius, length; axis along local y
        return l.x * l.x + l.z * l.z <= (s[0] + m) * (s[0] + m) && std::fabs(l.y) <= s[1] / 2 + m;
    case Shape::Ring:
    {
        const double r = std::sqrt(l.x * l.x + l.z * l.z);
        return r <= s[0] + m && r >= s[1] - m && std::fabs(l.y) <= s[2] / 2 + m;
    }
    case Shape::Ellipsoid:
    {
        const double ax = s[0] + m, ay = s[1] + m, az = s[2] + m;
        return (l.x * l.x) / (ax * ax) + (l.y * l.y) / (ay * ay) + (l.z * l.z) / (az * az) <= 1.0;
    }
    default:
        return Dot(l, l) <= (s[0] + m) * (s[0] + m);
    }
}

double PickRadius(const Zone& z)
{
    if (z.tradelane)
        return 0;
    const double* s = z.size;
    switch (z.shape)
    {
    case Shape::Box:
        return 0.5 * std::sqrt(s[0] * s[0] + s[1] * s[1] + s[2] * s[2]);
    case Shape::Cylinder:
        return std::max(s[0], s[1] / 2);
    case Shape::Ellipsoid:
        return std::max(s[0], std::max(s[1], s[2]));
    case Shape::Ring:
        return std::max(s[0], s[2] / 2);
    default:
        return s[0];
    }
}

bool ShipFits(const World& w, const NpcShip& s, const ShipLine& line, int level)
{
    const std::string diff = "diff" + std::to_string(std::max(1, level + line.offset));
    for (const std::string* c : {&line.cls, &diff})
    {
        bool ok = false;
        const auto it = w.shipClasses.find(*c);
        if (it != w.shipClasses.end())
        {
            for (const auto& m : it->second)
                if (std::find(s.classes.begin(), s.classes.end(), m) != s.classes.end())
                    ok = true;
        }
        else
            ok = std::find(s.classes.begin(), s.classes.end(), *c) != s.classes.end();
        if (!ok)
            return false;
    }
    return true;
}

Prepared Prepare(const World& w)
{
    Prepared p;
    p.allowed.resize(w.zones.size());
    p.speed.resize(w.zones.size());
    for (size_t zi = 0; zi < w.zones.size(); ++zi)
    {
        const Zone& z = w.zones[zi];
        p.allowed[zi].resize(z.encounters.size());
        p.speed[zi].resize(z.encounters.size());
        for (size_t ei = 0; ei < z.encounters.size(); ++ei)
        {
            const ZoneEncounter& ze = z.encounters[ei];
            if (ze.encounter < 0)
                continue;
            const Encounter& enc = w.encounters[ze.encounter];
            auto& forms = p.allowed[zi][ei];
            forms.resize(enc.formations.size());
            p.speed[zi][ei].resize(enc.formations.size());
            for (size_t fi = 0; fi < enc.formations.size(); ++fi)
                for (const auto& fw : ze.factions)
                {
                    if (fw.first < 0 || fw.second <= 0)
                        continue;
                    bool all = true; // E4: some npc_ship of the faction fits every line
                    for (const ShipLine& line : enc.formations[fi].lines)
                    {
                        bool any = false;
                        for (int si : w.factions[fw.first].ships)
                            if (si >= 0 && ShipFits(w, w.ships[si], line, ze.level))
                            {
                                any = true;
                                break;
                            }
                        if (!any)
                        {
                            all = false;
                            break;
                        }
                    }
                    if (all)
                    {
                        forms[fi].push_back(fw);
                        double sum = 0; // the leader line's fitting ships, drawn uniformly (E5): their mean known speed
                        int n = 0;
                        if (!enc.formations[fi].lines.empty())
                            for (int si : w.factions[fw.first].ships)
                                if (si >= 0 && w.ships[si].speed > 0 && ShipFits(w, w.ships[si], enc.formations[fi].lines[0], ze.level))
                                {
                                    sum += w.ships[si].speed;
                                    ++n;
                                }
                        p.speed[zi][ei][fi].push_back(n ? sum / n : 0);
                    }
                }
        }
    }
    return p;
}

Vec3 WanderTarget(const Zone& z, const Engine& e, const Vec3& from, Flight& f, uint32_t& seed)
{
    Rng rng{seed};
    Vec3 out = from;
    bool found = false;
    for (;;)
    {
        Vec3 pts[16]; // PickPointsAround: direction k at k x 22.5 degrees, range uniform in [min, max] per point
        for (int k = 0; k < 16; ++k)
        {
            const double r = rng.Unit() * (f.maxRange - f.minRange) + f.minRange, a = k * kPi / 8;
            pts[k] = {from.x + r * std::cos(a), from.y, from.z + r * std::sin(a)};
        }
        bool used[16] = {};
        for (int t = 0; t < 16 && !found; ++t)
        {
            int k = rng.Below(16);
            while (used[k & 15])
                ++k;
            k &= 15;
            used[k] = true;
            if (InZone(z, pts[k], 0)) // ClassifyPoint < 1; the segment-clear test needs object geometry (not modelled)
            {
                out = pts[k];
                found = true;
            }
        }
        if (found)
        {
            if (f.minRange < e.wanderMin)
            {
                f.minRange = e.wanderMin;
                f.maxRange = e.wanderMax;
            }
            break;
        }
        if (f.minRange > e.wanderFloor) // a 0.5 s delay, both ranges halved
        {
            f.minRange *= 0.5;
            f.maxRange *= 0.5;
            continue;
        }
        f.minRange = e.wanderMin;
        f.maxRange = e.wanderMax;
        const Vec3 d = Sub(z.pos, from);
        const double len = Len(d);
        out = len <= e.wanderZoneStep ? z.pos : Add(from, Mul(d, e.wanderZoneStep / len));
        break;
    }
    seed = rng.s;
    return out;
}

bool FlyStep(const Zone& z, const Engine& e, Vec3& pos, Flight& f, double dt, uint32_t& seed)
{
    if (!f.hasTarget)
    {
        pos = Add(pos, Mul(f.vel, dt));
        return true;
    }
    const Vec3 d = Sub(f.target, pos);
    const double len = Len(d), step = f.speed * dt;
    const double arrive = f.docks ? e.dockArrive : e.wanderArrive;
    if (len - step <= arrive)
    {
        pos = len > step ? Add(pos, Mul(d, step / len)) : f.target;
        if (f.docks)
            return false;
        if (f.behavior == Behavior::Wander)
            f.target = WanderTarget(z, e, pos, f, seed);
        return true;
    }
    pos = Add(pos, Mul(d, step / len));
    return true;
}

Lists CollectLists(const World& w, const Engine& e, const Vec3& p, double yaw)
{
    Lists out;
    const int n = static_cast<int>(w.zones.size());
    std::vector<char> inL1(n, 0), inL2(n, 0);
    double best = 1e300;
    for (int i = 0; i < n; ++i) // L1: every path zone at the ship, plus the smallest other one
    {
        const Zone& z = w.zones[i];
        if (!z.populated || !InZone(z, p, e.atShip))
            continue;
        if (z.path)
        {
            out.paths.push_back(i);
            inL1[i] = 1;
        }
        else if (PickRadius(z) < best)
        {
            best = PickRadius(z);
            out.smallest = i;
        }
    }
    if (out.smallest >= 0)
        inL1[out.smallest] = 1;
    for (int i = 0; i < n; ++i)
        if (inL1[i])
        {
            const Zone& z = w.zones[i];
            if (z.hasDensity && z.density == 0)
                out.blocked = true;
            out.limit = std::max(out.limit, static_cast<double>(z.density));
        }
    for (int i = 0; i < n; ++i) // L2: additive zones within 2000 m not kept in L1 (the bigger zones at the ship come back here)
    {
        const Zone& z = w.zones[i];
        if (!z.populated || !z.additive || inL1[i] || !InZone(z, p, e.nearReach))
            continue;
        out.l2.push_back(i);
        inL2[i] = 1;
        if (out.limit < e.nearRaise * z.density)
            out.limit = z.density;
    }
    // L3: additive zones in the view frustum within 4000 m (sampled: 11 x 7 directions x 16 depths).
    std::vector<Vec3> samples;
    for (int h = 0; h < 11; ++h)
        for (int v = 0; v < 7; ++v)
        {
            const Vec3 d = Dir(yaw, (h / 10.0 - 0.5) * e.frustumH, (v / 6.0 - 0.5) * e.frustumV);
            for (int k = 1; k <= 16; ++k)
                samples.push_back(Add(p, Mul(d, e.ahead * k / 16.0)));
        }
    for (int i = 0; i < n; ++i)
    {
        const Zone& z = w.zones[i];
        if (!z.populated || !z.additive || inL1[i] || inL2[i])
            continue;
        if (Len(Sub(z.pos, p)) > e.ahead + PickRadius(z) + 1 && !z.tradelane)
            continue;
        for (const Vec3& s : samples)
            if (InZone(z, s, 0))
            {
                out.l3.push_back(i);
                break;
            }
    }
    return out;
}

Result Simulate(const World& w, const Query& q, const Engine& e, const Assume& a) { return Simulate(w, Prepare(w), q, e, a); }

Result Simulate(const World& w, const Prepared& prep, const Query& q, const Engine& e, const Assume& a)
{
    const int nf = static_cast<int>(w.factions.size());
    Result res;
    res.firstSeen.assign(nf, 0);
    res.meanInDisc.assign(nf, 0);
    res.zoneSpawns.assign(w.zones.size(), 0);
    const int facings = std::max(1, q.facings);
    std::vector<Lists> lists;
    for (int f = 0; f < facings; ++f)
        lists.push_back(CollectLists(w, e, q.player, 2 * kPi * f / facings));

    Rng rng{q.seed};
    const int seconds = static_cast<int>(q.seconds);
    const int every = std::max(1, static_cast<int>(q.sampleEvery));
    for (int run = 0; run < q.runs; ++run)
    {
        const double yaw = 2 * kPi * (run % facings) / facings;
        const Vec3 fwd = Forward(yaw);
        const Lists& L = lists[run % facings];
        std::vector<Live> live;
        std::vector<std::vector<EncState>> encState(w.zones.size());
        for (size_t zi = 0; zi < w.zones.size(); ++zi)
            encState[zi].resize(w.zones[zi].encounters.size());
        bool inCombat = false;
        int met = 0;
        double combatEnd = -1e9;
        const int phase = rng.Below(3);
        int samples = 0;
        std::vector<double> inDisc(nf, 0);

        // Places formation f of zone z (section 8); false when no arrival method finds a point.
        // `how` = the arrival method used; `vel` = the path direction (unit, path leader) or the lane velocity (tradelane).
        auto place = [&](const Zone& z, const Formation& f, bool singleShip, Vec3& pos, Vec3& vel, unsigned& how) -> bool
        {
            static const unsigned order[] = {ArriveCruise, ArriveBuzz, ArriveTradelane, ArriveDockRing, ArriveJumpGate, ArriveStation, ArriveCapital};
            const int start = rng.Below(7);
            for (int k = 0; k < 7; ++k)
            {
                const unsigned m = order[(start + k) % 7];
                if (!(f.arrival & m))
                    continue;
                const Vec3& P = q.player;
                if (m == ArriveCruise && z.path && z.shape == Shape::Cylinder)
                {
                    // path leader: on the path axis, 200 m steps from the point nearest the player, outside 2500 m of him
                    const Vec3 axis{z.rot[1], z.rot[4], z.rot[7]};
                    const double half = z.size[1] / 2;
                    const double s0 = std::max(-half, std::min(half, Dot(Sub(P, z.pos), axis)));
                    const int sign0 = rng.Below(2) ? 1 : -1;
                    for (int side = 0; side < 2; ++side)
                    {
                        const int sign = side ? -sign0 : sign0;
                        for (double s = s0; std::fabs(s) <= half; s += sign * e.pathStep)
                        {
                            const Vec3 pt = Add(z.pos, Mul(axis, s));
                            if (Len(Sub(pt, P)) >= e.spawnRing)
                            {
                                pos = pt;
                                vel = Mul(axis, rng.Below(2) ? 1 : -1); // unit: the path direction it flies
                                how = m;
                                return true;
                            }
                        }
                    }
                }
                else if (m == ArriveCruise)
                {
                    // on the 2500-2550 m sphere, inside the zone grown by zone_creation_distance, within 250 m of height
                    for (int t = 0; t < a.placementTries; ++t)
                    {
                        const double dy = (rng.Unit() * 2 - 1) * e.heightBand;
                        const double r = e.spawnRing + rng.Unit() * e.spawnRingWidth;
                        const double ang = rng.Unit() * 2 * kPi, hr = std::sqrt(std::max(0.0, r * r - dy * dy));
                        const Vec3 pt{P.x + hr * std::sin(ang), P.y + dy, P.z + hr * std::cos(ang)};
                        if (InZone(z, pt, f.zoneCreationDistance))
                        {
                            pos = pt;
                            vel = {};
                            how = m;
                            return true;
                        }
                    }
                }
                else if (m == ArriveBuzz)
                {
                    // 2500-2550 m out, 150 m above; the flight after it is the formation's behaviour (12a)
                    const double r = e.spawnRing + rng.Unit() * e.spawnRingWidth, ang = rng.Unit() * 2 * kPi;
                    pos = {P.x + r * std::sin(ang), P.y + e.buzzHeight, P.z + r * std::cos(ang)};
                    vel = {};
                    how = m;
                    return true;
                }
                else if (m == ArriveTradelane)
                {
                    // out of a lane whose ring is within 5000 m, 3750 m beyond the ring, flying on along the lane
                    std::vector<int> close;
                    for (size_t i = 0; i < w.rings.size(); ++i)
                        if (Len(Sub(w.rings[i].pos, P)) <= e.laneReach)
                            close.push_back(static_cast<int>(i));
                    if (close.empty() || rng.Unit() >= a.laneAccept)
                        continue;
                    const LaneRing& ring = w.rings[close[rng.Below(static_cast<int>(close.size()))]];
                    const double sign = rng.Below(2) ? 1 : -1;
                    pos = Add(ring.pos, Mul(ring.dir, sign * e.countRadius));
                    vel = Mul(ring.dir, sign * e.laneSpeed); // in the lane: through the disc in seconds
                    how = m;
                    return true;
                }
                else if (singleShip)
                {
                    // dock launch: single-ship formations, from a dock of that kind within 2700 m
                    std::vector<int> close;
                    for (size_t i = 0; i < w.docks.size(); ++i)
                        if ((w.docks[i].kind & m) && Len(Sub(w.docks[i].pos, P)) <= e.dockReach)
                            close.push_back(static_cast<int>(i));
                    if (close.empty())
                        continue;
                    pos = w.docks[close[rng.Below(static_cast<int>(close.size()))]].pos;
                    vel = {};
                    how = m;
                    return true;
                }
            }
            return false;
        };

        auto aliveOf = [&](int zi, int ei) {
            int n = 0;
            for (const Live& l : live)
                n += l.zone == zi && l.enc == ei;
            return n;
        };

        // One formation of an encounter copy (E2, E3, E4, E6, G3, G5); true when it spawned.
        auto tryEncounter = [&](int zi, int ei, double t) -> bool
        {
            const Zone& z = w.zones[zi];
            const ZoneEncounter& ze = z.encounters[ei];
            const Encounter& enc = w.encounters[ze.encounter];
            EncState& st = encState[zi][ei];
            std::vector<Permutation> perms = enc.permutations;
            if (perms.empty())
            {
                Permutation all;
                for (size_t i = 0; i < enc.formations.size(); ++i)
                    all.formations.push_back(static_cast<int>(i));
                perms.push_back(all);
            }
            if (st.perm < 0 || t - st.pickedAt >= perms[st.perm].reroll)
            {
                std::vector<double> wts;
                for (const auto& p : perms)
                    wts.push_back(p.weight);
                st.perm = PickWeighted(rng, wts);
                st.pickedAt = t;
            }
            if (st.perm < 0)
                return false;
            for (int fi : perms[st.perm].formations)
            {
                if (fi < 0 || fi >= static_cast<int>(enc.formations.size()))
                    continue;
                const Formation& f = enc.formations[fi];
                if (!f.simultaneous && aliveOf(zi, ei) > 0)
                    continue;
                if (f.timesToCreate >= 0 && st.created >= f.timesToCreate)
                    continue;
                const auto& allowed = prep.allowed[zi][ei][fi];
                std::vector<double> wts;
                for (const auto& fw : allowed)
                    wts.push_back(fw.second);
                const int pick = PickWeighted(rng, wts);
                if (pick < 0)
                    continue;
                const int faction = allowed[pick].first;
                int ships = 0, maxShips = 0;
                for (const ShipLine& line : f.lines)
                {
                    ships += line.minCount + rng.Below(line.maxCount - line.minCount + 1);
                    maxShips += line.maxCount;
                }
                if (ships <= 0)
                    continue;
                Vec3 pos, vel;
                unsigned how = 0;
                if (!place(z, f, maxShips <= 1, pos, vel, how))
                    continue;
                Live l{zi, ei, fi, faction, ships, pos, {}, 2 * f.longevity, f.longevity,
                       w.factions[faction].standing <= e.hostile, false, false, false};
                // flight after spawn (spawning.md 12a): speeds from the data, the guide's own way
                const double engine = prep.speed[zi][ei][fi][pick];
                Flight& fl = l.flight;
                fl.behavior = f.behavior;
                fl.engine = engine;
                fl.speed = l.hostile ? (a.hostileSpeed >= 0 ? a.hostileSpeed : engine) : 0;
                const Faction& fac = w.factions[faction];
                if (f.behavior == Behavior::Path && fac.scanChance > 0 && fac.standing >= e.hostile &&
                    fac.standing <= (fac.lawful ? e.scanMaxLawful : e.scanMaxUnlawful))
                    fl.scanChance = fac.scanChance; // F2b: the player is a scan candidate of this patrol
                if (!l.hostile)
                {
                    const double cruise = a.pathSpeed >= 0 ? a.pathSpeed : w.cruiseSpeed > 0 ? w.cruiseSpeed : e.cruiseSpeed;
                    if (how == ArriveTradelane)
                        fl.vel = vel; // rides the lane on through
                    else if (f.behavior == Behavior::Wander)
                    {
                        fl.speed = a.wanderSpeed >= 0 ? a.wanderSpeed : engine;
                        fl.minRange = e.wanderMin;
                        fl.maxRange = e.wanderMax;
                        fl.target = WanderTarget(z, e, pos, fl, rng.s);
                        fl.hasTarget = true;
                    }
                    else if (f.behavior == Behavior::Path || f.behavior == Behavior::Trade)
                    {
                        std::vector<int> docks;
                        if (f.behavior == Behavior::Trade)
                            for (size_t i = 0; i < w.docks.size(); ++i)
                                if (Len(Sub(w.docks[i].pos, pos)) <= e.tradeReach)
                                    docks.push_back(static_cast<int>(i));
                        if (!docks.empty())
                        {
                            fl.target = w.docks[docks[rng.Below(static_cast<int>(docks.size()))]].pos;
                            fl.hasTarget = fl.docks = true;
                            fl.speed = cruise;
                        }
                        else
                            fl.vel = Mul(Len(vel) > 0 ? vel : Dir(rng.Unit() * 2 * kPi, 0, 0), cruise);
                    }
                }
                live.push_back(l);
                ++st.created;
                res.zoneSpawns[zi] += 1;
                return true; // an unstarred pass stops at the first formation that spawns
            }
            return false;
        };

        for (int t = 1; t <= seconds; ++t)
        {
            // move (12a), life (X1, X2), combat state (G2): every second
            for (Live& l : live)
            {
                if (l.hostile)
                {
                    const Vec3 d = Sub(q.player, l.pos);
                    const double dist = Len(d);
                    if (dist > a.holdRange)
                        l.pos = Add(l.pos, Mul(d, std::min(l.flight.speed * e.lifeTick, dist - a.holdRange) / dist));
                }
                else if (l.flight.scanChance > 0 && !l.flight.scanRolled && Len(Sub(q.player, l.pos)) <= e.scanReach &&
                         (l.flight.scanRolled = true, rng.Unit() < l.flight.scanChance))
                    l.flight.scanLeft = a.scanHold; // F2b: one roll per patrol, then it trails the player
                if (!l.hostile && l.flight.scanLeft > 0)
                {
                    l.flight.scanLeft -= e.lifeTick;
                    const Vec3 d = Sub(q.player, l.pos);
                    const double dist = Len(d);
                    if (dist > e.scanRange)
                        l.pos = Add(l.pos, Mul(d, std::min(l.flight.engine * e.lifeTick, dist - e.scanRange) / dist));
                }
                else if (!l.hostile && !FlyStep(w.zones[l.zone], e, l.pos, l.flight, e.lifeTick, rng.s))
                {
                    l.life = -1; // docked: gone, with its formation
                    continue;
                }
                const Vec3 d = Sub(l.pos, q.player);
                const double dist = Len(d);
                const bool seen = dist <= e.seenRange && dist > 0 && Dot(d, fwd) / dist >= std::cos(e.seenHalfAngle);
                if (dist <= e.refillRadius || seen)
                    l.life = l.longevity;
                else
                    l.life -= std::min(e.maxRate, 1 + (dist - e.refillRadius) / e.rateStep) * e.lifeTick;
            }
            live.erase(std::remove_if(live.begin(), live.end(), [](const Live& l) { return l.life < 0; }), live.end());
            bool hostileNear = false;
            for (const Live& l : live)
                hostileNear |= l.hostile && Len(Sub(l.pos, q.player)) <= e.combatRadius;
            if (hostileNear && !inCombat)
            {
                inCombat = true;
                met = 0;
                for (Live& l : live)
                    l.met = false;
            }
            if (!hostileNear && inCombat)
            {
                inCombat = false;
                combatEnd = t;
            }
            if (inCombat)
                for (Live& l : live)
                    if (l.hostile && !l.met && Len(Sub(l.pos, q.player)) <= e.combatRadius)
                    {
                        l.met = true;
                        met += l.ships;
                    }

            if ((t + phase) % static_cast<int>(e.heartbeat) == 0) // H1: one heartbeat, at most one formation
            {
                if (L.blocked)
                    res.ticksBlocked += 1;
                else
                {
                    std::vector<int> order = L.paths;
                    if (L.smallest >= 0)
                        order.insert(order.begin() + rng.Below(static_cast<int>(order.size()) + 1), L.smallest);
                    const size_t nL12 = order.size() + L.l2.size();
                    order.insert(order.end(), L.l2.begin(), L.l2.end());
                    order.insert(order.end(), L.l3.begin(), L.l3.end());
                    for (size_t k = 0; k < order.size(); ++k)
                    {
                        const int zi = order[k];
                        const Zone& z = w.zones[zi];
                        const bool ahead = k >= nL12;
                        const Vec3 req = ahead ? Add(q.player, Mul(fwd, e.aheadPoint)) : q.player;
                        const double lim = ahead ? std::floor(e.aheadShare * z.density) : L.limit;
                        double n = 0;
                        for (const Live& l : live)
                            if (Len(Sub(l.pos, req)) <= e.countRadius)
                                n += l.ships;
                        if (n >= lim)
                            continue;
                        const double p = std::max(0.0, std::min(1.0, (lim - n) / z.repop));
                        if (rng.Unit() >= p)
                            continue;
                        if (inCombat ? met >= z.maxBattle : t - combatEnd < z.relief)
                        {
                            res.ticksCombatRefused += 1;
                            continue;
                        }
                        // E1: encounters by chance, one that spawns nothing leaves the draw
                        std::vector<double> wts;
                        for (const auto& ze : z.encounters)
                            wts.push_back(ze.encounter >= 0 ? ze.chance : 0);
                        bool spawned = false;
                        for (int pick; !spawned && (pick = PickWeighted(rng, wts)) >= 0;)
                        {
                            spawned = tryEncounter(zi, pick, t);
                            wts[pick] = 0;
                        }
                        if (spawned)
                            break;
                    }
                }
            }

            if (t % every != 0)
                continue;
            // first sightings as the census takes them: at its samples, where the ship is then
            for (Live& l : live)
                if (!l.sighted)
                {
                    l.sighted = true;
                    if (Len(Sub(l.pos, q.player)) > q.disc)
                        continue;
                    l.inDisc = true;
                    res.firstSeen[l.faction] += l.ships;
                    auto& cell = res.cells[{static_cast<int>(std::floor(l.pos.x / q.cell)), static_cast<int>(std::floor(l.pos.z / q.cell))}];
                    if (cell.empty())
                        cell.assign(nf, 0);
                    cell[l.faction] += l.ships;
                }
            if (t >= q.sampleFrom)
            {
                ++samples;
                for (const Live& l : live)
                    if (l.inDisc && Len(Sub(l.pos, q.player)) <= q.disc)
                        inDisc[l.faction] += l.ships;
            }
        }
        for (int f = 0; f < nf; ++f)
            res.meanInDisc[f] += samples ? inDisc[f] / samples : 0;
    }
    const double runs = std::max(1, q.runs);
    for (int f = 0; f < nf; ++f)
    {
        res.firstSeen[f] /= runs;
        res.meanInDisc[f] /= runs;
    }
    for (auto& c : res.cells)
        for (double& v : c.second)
            v /= runs;
    for (double& v : res.zoneSpawns)
        v /= runs;
    res.ticksBlocked /= runs;
    res.ticksCombatRefused /= runs;
    return res;
}
} // namespace spawnsim
