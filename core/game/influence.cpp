// HUD 2 map's influence cells (influence.h): the hex grid, the list signatures, one simulation per signature.
#include "influence.h"

#include "game/flhash.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>

namespace influence
{
namespace
{
// A zone's bounding radius about its centre (a trade lane's too: spawnsim::PickRadius gives those 0 for the L1 pick).
double BoundRadius(const spawnsim::Zone& z)
{
    const double* s = z.size;
    switch (z.shape)
    {
    case spawnsim::Shape::Box:
        return 0.5 * std::sqrt(s[0] * s[0] + s[1] * s[1] + s[2] * s[2]);
    case spawnsim::Shape::Cylinder:
        return std::sqrt(s[0] * s[0] + s[1] * s[1] / 4);
    case spawnsim::Shape::Ellipsoid:
        return std::max(s[0], std::max(s[1], s[2]));
    case spawnsim::Shape::Ring:
        return std::sqrt(s[0] * s[0] + s[2] * s[2] / 4);
    default:
        return s[0];
    }
}

double Dist2(const spawnsim::Vec3& a, double x, double z)
{
    const double dx = a.x - x, dz = a.z - z;
    return dx * dx + dz * dz;
}

// The point of a box or cylinder zone nearest p (p itself when inside), in the plane y = 0 as far as the zone allows: the cell's sample on a lane or a
// patrol path that crosses the cell but misses its centre.
spawnsim::Vec3 Nearest(const spawnsim::Zone& z, const spawnsim::Vec3& p)
{
    const double* r = z.rot;
    const spawnsim::Vec3 d{p.x - z.pos.x, p.y - z.pos.y, p.z - z.pos.z};
    double l[3] = {r[0] * d.x + r[3] * d.y + r[6] * d.z, r[1] * d.x + r[4] * d.y + r[7] * d.z, r[2] * d.x + r[5] * d.y + r[8] * d.z};
    const double* s = z.size;
    if (z.shape == spawnsim::Shape::Box)
        for (int k = 0; k < 3; ++k)
            l[k] = std::max(-s[k] / 2, std::min(s[k] / 2, l[k]));
    else // cylinder: radius s[0] about local y, length s[1]
    {
        const double rr = std::sqrt(l[0] * l[0] + l[2] * l[2]);
        if (rr > s[0])
            l[0] *= s[0] / rr, l[2] *= s[0] / rr;
        l[1] = std::max(-s[1] / 2, std::min(s[1] / 2, l[1]));
    }
    return {z.pos.x + r[0] * l[0] + r[1] * l[1] + r[2] * l[2], z.pos.y + r[3] * l[0] + r[4] * l[1] + r[5] * l[2],
            z.pos.z + r[6] * l[0] + r[7] * l[1] + r[8] * l[2]};
}
} // namespace

int RelationOf(double standing) { return standing <= -0.6 ? kHostile : standing >= 0.6 ? kFriendly : kNeutral; }

std::vector<int> Relations(const spawnsim::World& w)
{
    std::vector<int> out;
    for (const auto& f : w.factions)
        out.push_back(RelationOf(f.standing));
    return out;
}

Field Compute(const spawnsim::World& w, const Options& o, const std::atomic<bool>* cancel)
{
    Field out;
    const spawnsim::Engine e;
    const int nz = static_cast<int>(w.zones.size());
    std::vector<double> reach(nz, 0);
    std::vector<uint32_t> ids(nz, 0);
    double x0 = 1e300, x1 = -1e300, z0 = 1e300, z1 = -1e300;
    for (int i = 0; i < nz; ++i)
    {
        const spawnsim::Zone& z = w.zones[i];
        reach[i] = BoundRadius(z);
        ids[i] = flhash::CreateId(z.nick);
        if (!z.populated)
            continue;
        x0 = std::min(x0, z.pos.x - reach[i]), x1 = std::max(x1, z.pos.x + reach[i]);
        z0 = std::min(z0, z.pos.z - reach[i]), z1 = std::max(z1, z.pos.z + reach[i]);
    }
    if (x0 > x1)
        return out;
    const double span = std::max(x1 - x0, z1 - z0);
    double spacing = o.spacing > 0 ? o.spacing : std::max(1000.0, span / 100);
    const double rowStep = 0.8660254037844386; // a hex grid's rows: sqrt(3) / 2 of the spacing apart, every other row shifted half a cell
    while (((x1 - x0) / spacing + 2) * ((z1 - z0) / (spacing * rowStep) + 2) > o.maxCells)
        spacing *= 1.25;
    out.spacing = spacing;
    const double cellR = spacing / std::sqrt(3.0); // a hex's corner radius

    // pass 1: each cell's samples (its centre, and the nearest point of every trade lane or patrol path that crosses the cell but misses the centre: a
    // cell shows the strongest place inside it, so a 1.6 km lane reads on a 3 km grid) and their list signatures (the zones the spawn rules can try
    // from there: paths at the ship, the smallest zone, the near and the view reach as a superset of every facing's frustum)
    std::map<std::vector<int>, int> sigOf;
    std::vector<std::vector<std::pair<double, double>>> members; // per signature, its sample points
    std::vector<Cell> cells;
    std::vector<std::vector<int>> cellSigs;
    auto signature = [&](const spawnsim::Vec3& p, Cell* c) -> int {
        std::vector<int> paths, l2, l3;
        int smallest = -1;
        double best = 1e300;
        for (int i = 0; i < nz; ++i)
        {
            const spawnsim::Zone& z = w.zones[i];
            const double r = reach[i] + e.ahead;
            if (!z.populated || Dist2(z.pos, p.x, p.z) > r * r || !InZone(z, p, e.atShip))
                continue;
            if (z.path)
                paths.push_back(i);
            else if (spawnsim::PickRadius(z) < best)
                best = spawnsim::PickRadius(z), smallest = i;
            if (c && !z.path && !z.encounters.empty() && c->nZones < 4 && InZone(z, p, 0))
                c->zones[c->nZones++] = ids[i];
        }
        for (int i = 0; i < nz; ++i)
        {
            const spawnsim::Zone& z = w.zones[i];
            const double r = reach[i] + e.ahead;
            if (!z.populated || !z.additive || i == smallest || Dist2(z.pos, p.x, p.z) > r * r || std::find(paths.begin(), paths.end(), i) != paths.end())
                continue;
            if (InZone(z, p, e.nearReach))
                l2.push_back(i);
            else if (InZone(z, p, e.ahead))
                l3.push_back(i);
        }
        if (paths.empty() && smallest < 0 && l2.empty() && l3.empty())
            return -1;
        std::vector<int> key = paths;
        key.push_back(-1), key.push_back(smallest), key.push_back(-2);
        key.insert(key.end(), l2.begin(), l2.end());
        key.push_back(-3);
        key.insert(key.end(), l3.begin(), l3.end());
        auto it = sigOf.find(key);
        int s = 0;
        if (it == sigOf.end())
            s = static_cast<int>(members.size()), sigOf.emplace(key, s), members.emplace_back();
        else
            s = it->second;
        members[s].emplace_back(p.x, p.z);
        return s;
    };
    int row = 0;
    for (double cz = z0; cz <= z1; cz += spacing * rowStep, ++row)
    {
        if (cancel && cancel->load())
            return out.cancelled = true, out;
        int col = -1;
        for (double cx = x0 + ((row & 1) ? spacing / 2 : 0); cx <= x1; cx += spacing)
        {
            ++col;
            const spawnsim::Vec3 p{cx, 0, cz};
            Cell c;
            c.x = static_cast<float>(cx), c.z = static_cast<float>(cz), c.col = col, c.row = row;
            std::vector<int> sigs;
            const int s = signature(p, &c);
            if (s >= 0)
                sigs.push_back(s);
            for (int i = 0; i < nz; ++i)
            {
                const spawnsim::Zone& z = w.zones[i];
                if (!z.populated || !(z.tradelane || z.path) || (z.shape != spawnsim::Shape::Box && z.shape != spawnsim::Shape::Cylinder))
                    continue;
                const double r = reach[i] + cellR;
                if (Dist2(z.pos, cx, cz) > r * r || InZone(z, p, e.atShip))
                    continue;
                const spawnsim::Vec3 q = Nearest(z, p);
                if (Dist2(q, cx, cz) > cellR * cellR)
                    continue;
                const int t = signature(q, nullptr);
                if (t >= 0 && std::find(sigs.begin(), sigs.end(), t) == sigs.end())
                    sigs.push_back(t);
            }
            if (sigs.empty())
                continue;
            cells.push_back(c);
            cellSigs.push_back(sigs);
        }
    }

    // pass 2: one simulation per signature, at the member point nearest the members' centre
    const spawnsim::Prepared prep = spawnsim::Prepare(w);
    const std::vector<int> rel = Relations(w);
    std::vector<std::array<float, 3>> strength(members.size());
    for (size_t s = 0; s < members.size(); ++s)
    {
        if (cancel && cancel->load())
            return out.cancelled = true, out;
        double mx = 0, mz = 0;
        for (const auto& m : members[s])
            mx += m.first, mz += m.second;
        mx /= members[s].size(), mz /= members[s].size();
        std::pair<double, double> at = members[s][0];
        double bestD = 1e300;
        for (const auto& m : members[s])
        {
            const double dx = m.first - mx, dz = m.second - mz, d = dx * dx + dz * dz;
            if (d < bestD)
                bestD = d, at = m;
        }
        spawnsim::Query q;
        q.player = {at.first, 0, at.second};
        q.runs = o.runs, q.seconds = o.seconds, q.seed = 61 + static_cast<uint32_t>(s);
        const spawnsim::Result r = spawnsim::Simulate(w, prep, q, e);
        // the mix from the first sightings (the measure the censuses scored the lead relation on: 86 % held out), the size from the time-mean count
        // (the measure they scored the count on): an attacker that closes and stays would otherwise outweigh a lane's traders passing in seconds
        std::array<double, 3> seen = {0, 0, 0};
        double count = 0;
        for (size_t f = 0; f < r.meanInDisc.size() && f < rel.size(); ++f)
            seen[rel[f]] += r.firstSeen[f], count += r.meanInDisc[f];
        const double total = seen[0] + seen[1] + seen[2];
        std::array<float, 3> sum = {0, 0, 0};
        for (int k = 0; k < 3 && total > 0; ++k)
            sum[k] = static_cast<float>(count * seen[k] / total);
        strength[s] = sum;
    }
    out.signatures = static_cast<int>(members.size());

    // pass 3: a cell takes its strongest sample (by its leading relation's ships)
    for (size_t i = 0; i < cells.size(); ++i)
    {
        Cell& c = cells[i];
        const std::array<float, 3>* s = nullptr;
        for (int k : cellSigs[i])
            if (!s || *std::max_element(strength[k].begin(), strength[k].end()) > *std::max_element(s->begin(), s->end()))
                s = &strength[k];
        if (!s || (*s)[0] + (*s)[1] + (*s)[2] < o.floor)
            continue;
        c.strength[0] = (*s)[0], c.strength[1] = (*s)[1], c.strength[2] = (*s)[2];
        c.lead = static_cast<int>(std::max_element(s->begin(), s->end()) - s->begin());
        for (int k = 0; k < 3; ++k)
            if (k != c.lead && (*s)[k] >= o.contested * (*s)[c.lead] && (c.second < 0 || (*s)[k] > (*s)[c.second]))
                c.second = k;
        out.cells.push_back(c);
    }
    return out;
}
} // namespace influence
