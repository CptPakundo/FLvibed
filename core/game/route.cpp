#include "game/route.h"

#include "game/avoid.h"
#include "hook.h"
#include "log.h"

#include <cmath>
#include <map>
#include <sstream>

namespace route
{
// ------------------------------------------------------------------ the lane ride law
const LaneLaw& Law()
{
    static LaneLaw law;
    static bool read = false;
    if (read || !GetModuleHandleA("common.dll"))
        return law;
    read = true;
    // Common.dll's LaneAction update (RVA 0x619D0): the end phase computes 1 - (1 - END / SPEED) x progress -
    // `fld [SPEED]; fdivr [END]; fsubr [1.0]; fmul [esp+40]; fsubr [1.0]`; the entry ramp adds dt x RAMP to the speed
    // fraction - `fmul [RAMP]; fadd [ebp+EC]; fst [ebp+EC]`. HDE 0.7.1: 2500 (float), 400 and 0.125 (doubles).
    static const char kEndSig[] = "D9 05 ?? ?? ?? ?? DC 3D ?? ?? ?? ?? D8 2D ?? ?? ?? ?? D8 4C 24 40 D8 2D";
    uintptr_t end = hk::FindModuleData("common.dll", kEndSig, 2);
    uintptr_t endConst = hk::FindModuleData("common.dll", kEndSig, 8);
    if (!end)
    {
        // FLUF's AllowAdjustableTradelaneSpeed (always on; FLUF Source\Common\Fixes\AllowAdjustableTradelaneSpeed.cpp
        // 98-108) replaces `fld [SPEED]` with a jump into FLUF that loads the speed from its equipment manipulator -
        // AbstractEquipmentManipulator::GetTradelaneSpeed returns 2500, the game's constant, unless a FLUF module
        // overrides it (none shipped with the nightly does). The law is read from the original code (the module's
        // file) only when the patched instruction is that jump into restore-style code (FLUF); anything else = the
        // defaults (fails closed).
        const uintptr_t at = hk::FindInModuleOriginal("common.dll", kEndSig, 0);
        uintptr_t to = 0;
        __try
        {
            if (at && Mem<u8>(at) == 0xE9)
                to = at + 5 + Mem<i32>(at + 1);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            to = 0;
        }
        uintptr_t speedAt = 0, endAt = 0;
        if (to && hk::IsRestoreStyle(to) && hk::OriginalPointer(at + 2, &speedAt) && hk::OriginalPointer(at + 8, &endAt))
        {
            end = speedAt;
            endConst = endAt;
            Log("lane law: common.dll's lane speed is FLUF's (adjustable tradelane speed; its base = the game's "
                "constant): the law read from the original code");
        }
    }
    const uintptr_t ramp = hk::FindModuleData("common.dll", "DC 0D ?? ?? ?? ?? D8 85 EC 00 00 00 D9 95 EC 00 00 00", 2);
    if (end && endConst && ramp)
    {
        __try
        {
            const float speed = Mem<float>(end);
            const double endSpeed = Mem<double>(endConst), rate = Mem<double>(ramp);
            if (speed > 100 && speed < 100000 && endSpeed > 0 && endSpeed < speed && rate > 0 && rate < 10)
            {
                law.speed = speed;
                law.endSpeed = static_cast<float>(endSpeed);
                law.accel = static_cast<float>(rate) * speed;
                law.exitRate = speed; // measured: from full speed to rest in 1.0 s
                law.fromGame = true;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
        }
    }
    return law;
}

// ------------------------------------------------------------------ lanes
void Lane::PointAt(float s, float out[3]) const
{
    const size_t n = at.size();
    if (n == 0)
        return;
    if (n == 1 || s <= 0)
    {
        out[0] = x[0], out[1] = y[0], out[2] = z[0];
        return;
    }
    size_t i = 1;
    while (i < n - 1 && at[i] < s)
        ++i;
    const float seg = at[i] - at[i - 1];
    const float f = seg > 0 ? (s - at[i - 1]) / seg : 0; // beyond the end: extrapolated along the last segment
    out[0] = x[i - 1] + (x[i] - x[i - 1]) * f;
    out[1] = y[i - 1] + (y[i] - y[i - 1]) * f;
    out[2] = z[i - 1] + (z[i] - z[i - 1]) * f;
}

// Follows one direction of links from `from`; true when `to` is reached.
static bool Walk(u32 from, u32 to, bool forward, Lane& out)
{
    out = Lane();
    u32 ring = from;
    for (int guard = 0; guard < 64 && ring; ++guard)
    {
        float p[3];
        if (!space::ObjectPos(ring, p))
            return false;
        if (!out.rings.empty())
        {
            const float dx = p[0] - out.x.back(), dy = p[1] - out.y.back(), dz = p[2] - out.z.back();
            out.at.push_back(out.at.back() + std::sqrt(dx * dx + dy * dy + dz * dz));
        }
        else
            out.at.push_back(0);
        out.rings.push_back(ring);
        out.x.push_back(p[0]), out.y.push_back(p[1]), out.z.push_back(p[2]);
        if (ring == to)
            return out.rings.size() >= 2;
        u32 next = 0, prev = 0;
        if (!space::LaneLinks(ring, next, prev))
            return false;
        ring = forward ? next : prev;
    }
    return false;
}

static std::map<u32, float>& Learned()
{
    static std::map<u32, float> m;
    return m;
}

void LearnLaneSpeed(u32 ring, float speed)
{
    if (!ring || speed <= 100)
        return;
    // The whole chain the ring belongs to (both directions): one lane, one speed.
    for (int dir = 0; dir < 2; ++dir)
    {
        u32 r = ring;
        for (int guard = 0; guard < 64 && r; ++guard)
        {
            Learned()[r] = speed;
            u32 next = 0, prev = 0;
            if (!space::LaneLinks(r, next, prev))
                break;
            r = dir == 0 ? next : prev;
        }
    }
}

float LearnedLaneSpeed(u32 ring)
{
    const auto it = Learned().find(ring);
    return it == Learned().end() ? 0.0f : it->second;
}

static bool LaneBetweenImpl(u32 from, u32 to, Lane& out);
bool LaneBetween(u32 from, u32 to, Lane& out)
{
    if (!LaneBetweenImpl(from, to, out))
        return false;
    for (u32 r : out.rings)
        if (const float v = LearnedLaneSpeed(r))
        {
            out.speed = v;
            break;
        }
    return true;
}

static bool LaneBetweenImpl(u32 from, u32 to, Lane& out)
{
    if (!from || !to || from == to)
        return false;
    bool forward = true;
    if (!Walk(from, to, true, out))
    {
        forward = false;
        if (!Walk(from, to, false, out))
            return false;
    }
    // The whole lane in that direction: from its first ring (behind `from`) to its last (beyond `to`) - a nearer ring
    // before the best path's entry, or riding on past its exit ring, may be faster; the ride slows only at the lane's
    // real end, and the player leaves it anywhere by free flight.
    u32 first = from;
    for (int guard = 0; guard < 64; ++guard)
    {
        u32 next = 0, prev = 0;
        if (!space::LaneLinks(first, next, prev))
            return false;
        const u32 back = forward ? prev : next;
        if (!back)
            break;
        first = back;
    }
    return Walk(first, 0, forward, out) || out.rings.size() >= 2;
}

float RideSpeed(const LaneLaw& law, const Lane& lane, float fromS, float s)
{
    const float v0 = 5; // the ride starts from rest
    // This lane's measured speed when known; the ramp is the game's fraction of the speed per second either way.
    const float top = lane.speed > 0 ? lane.speed : law.speed;
    const float accel = law.accel * top / law.speed;
    float v = std::sqrt(v0 * v0 + 2 * accel * std::max(0.0f, s - fromS));
    v = std::min(v, top);
    const size_t n = lane.at.size();
    if (n >= 2)
    {
        const float a = lane.at[n - 2], b = lane.at[n - 1];
        if (s > a && b > a)
            v = std::min(v, top - (top - law.endSpeed) * std::min(1.0f, (s - a) / (b - a)));
    }
    return std::max(v, v0);
}

float RideTime(const LaneLaw& law, const Lane& lane, float fromS, float toS)
{
    const float ds = 50;
    float t = 0;
    for (float s = fromS; s < toS; s += ds)
    {
        const float step = std::min(ds, toS - s);
        t += step / RideSpeed(law, lane, fromS, s + step * 0.5f);
    }
    return t;
}

// ------------------------------------------------------------------ planning
namespace
{
struct Item
{
    Kind kind = Kind::Arrive;
    u32 object = 0;
    float pos[3] = {};
    Lane lane; // Kind::Lane: from the best path's entry ring to its exit ring
};

// The best ride on one lane from `p` towards `anchor`: entry ring k, exit s (free flight; or the lane's end), what it
// takes, and where the ship is then.
struct Ride
{
    float total = 1e30f; // entry run + ride + exit + the flight on to the anchor
    float tIn = 0, tRide = 0;
    size_t k = 0;
    float s = -1; // exit distance along the lane (-1: the end)
    float exit[3] = {};
};

struct Planner
{
    const flight::Model& m;
    const LaneLaw& law;
    std::vector<Item> items;
    std::vector<Lane> lanes;             // every lane of the system, both directions (detours off the best path)
    std::vector<space::Obstacle> bodies; // lethal zones and big solars: free flight goes round them (avoid::PathLength)
    float shipRadius = 10;
    Params prm{}; // the caller's tuning (route.h)

    Ride BestRide(const Lane& L, const float p[3], float v0, const float* anchor, bool stopAtAnchor) const
    {
        Ride best;
        const size_t n = L.rings.size();
        const float len = L.length();
        for (size_t k = 0; k + 1 < n; ++k)
        {
            // only an entry the mission lets the player take (the game checks the entry pair: M04 West Point, 19 Oct 2026
            // - an entry at another ring of a partly open lane was refused and the lane nag failed the mission)
            if (space::LaneOpen(L.rings[k], L.rings[k + 1]) == 0)
                continue;
            const float ring[3] = {L.x[k], L.y[k], L.z[k]};
            const float tIn = Fly(p, v0, ring, false) + law.entryDelay;
            if (tIn >= best.total)
                continue;
            // Ride time accumulated along the lane; exits evaluated every 500 m (Params::exitEvery) and at the end.
            float t = 0, s = L.at[k], nextEval = L.at[k] + prm.exitEvery;
            for (;;)
            {
                const bool toEnd = s >= len - 0.5f;
                if (toEnd || s >= nextEval)
                {
                    nextEval += prm.exitEvery;
                    float tRide = t, exitPos[3];
                    if (toEnd)
                    {
                        tRide += law.endDwell;
                        L.PointAt(len + law.endCarry, exitPos);
                    }
                    else
                    {
                        const float v = RideSpeed(law, L, L.at[k], s);
                        tRide += v / law.exitRate;
                        L.PointAt(s + v * v / (2 * law.exitRate), exitPos);
                    }
                    const float total = tIn + tRide + (anchor ? Fly(exitPos, 0, anchor, stopAtAnchor) : 0);
                    if (total < best.total)
                    {
                        best.total = total, best.tIn = tIn, best.tRide = tRide, best.k = k, best.s = toEnd ? -1 : s;
                        memcpy(best.exit, exitPos, sizeof(best.exit));
                    }
                    if (toEnd)
                        break;
                }
                const float step = std::min(50.0f, len - s);
                t += step / RideSpeed(law, L, L.at[k], s + step * 0.5f);
                s += step;
            }
        }
        return best;
    }

    static Step LaneStep(const Lane& L, const Ride& r)
    {
        Step s;
        s.kind = Kind::Lane;
        s.object = L.rings[r.k];
        s.toward = L.rings[r.k + 1];
        s.endRing = L.rings.back();
        s.laneLength = L.length() - L.at[r.k];
        s.exitAt = r.s < 0 ? -1 : r.s - L.at[r.k];
        s.pos[0] = L.x[r.k], s.pos[1] = L.y[r.k], s.pos[2] = L.z[r.k];
        s.eta = r.tIn + r.tRide;
        return s;
    }

    static float Dist(const float a[3], const float b[3])
    {
        const float d[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
        return std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    }

    // Free flight between two points: the faster of afterburner and cruise (the ship's model); `stop`: arrive at rest.
    float Fly(const float from[3], float v0, const float to[3], bool stop) const
    {
        // the way the avoidance flies it: round a planet's lethal zone in the way (Leeds' ring from the Iw03 hole)
        const float d = bodies.empty() ? Dist(from, to)
                                       : avoid::PathLength(from, to, bodies, shipRadius,
                                                           m.ok && m.cruiseSpeed > 0 ? m.cruiseSpeed : 300.0f,
                                                           m.turnRate > 0 ? m.turnRate : 1.6f);
        if (d < 1)
            return 0;
        if (!m.ok)
            return d / 80;
        return flight::QuickRunTime(m, v0, d, stop);
    }

    // Seconds to finish items[i..] from `p` at speed v0; the steps taken are appended to `out`.
    float Solve(size_t i, const float p[3], float v0, std::vector<Step>& out) const
    {
        if (i >= items.size())
            return 0;
        const Item& it = items[i];
        if (it.kind != Kind::Lane)
        {
            // A fixed point (gate / base / destination): straight there, or by a lane of the system the best path did
            // not take (the game's path is not always the fastest: a goal beside a lane's middle got none).
            const bool stop = it.kind == Kind::Arrive;
            Step s;
            s.kind = it.kind;
            s.object = it.object;
            memcpy(s.pos, it.pos, sizeof(s.pos));
            const float direct = Fly(p, v0, it.pos, stop);
            Ride best;
            const Lane* via = nullptr;
            // A lane can only help if reaching its nearest ring and flying on from its nearest ring, both at the
            // ship's top speed, beats what is known (a cheap lower bound: distant lanes are skipped unsimulated).
            const float vmax = std::max(1.0f, std::max(m.cruiseSpeed, m.hasBurner ? m.burner.top() : m.engine.top()));
            for (const Lane& L : lanes)
            {
                float toLane = 1e30f, fromLane = 1e30f;
                for (size_t k = 0; k < L.rings.size(); ++k)
                {
                    const float ring[3] = {L.x[k], L.y[k], L.z[k]};
                    toLane = std::min(toLane, Dist(p, ring));
                    fromLane = std::min(fromLane, Dist(ring, it.pos));
                }
                if ((toLane + fromLane) / vmax + law.entryDelay >= std::min(direct, best.total))
                    continue;
                const Ride r = BestRide(L, p, v0, it.pos, stop);
                if (r.total < best.total)
                    best = r, via = &L;
            }
            if (via && best.total < direct)
            {
                const Step ls = LaneStep(*via, best);
                out.push_back(ls);
                const float jump = it.kind == Kind::Jump ? space::JumpSeconds(it.object) : 0.0f;
                s.eta = best.total - ls.eta + jump;
                out.push_back(s);
                return best.total + jump;
            }
            // A jump: the gate's own sequence (jump out, tunnels, jump in) after the approach.
            const float jump = it.kind == Kind::Jump ? space::JumpSeconds(it.object) : 0.0f;
            s.eta = direct + jump;
            out.push_back(s);
            return direct + jump; // a jump ends the plan for this system; the destination ends it anyway
        }
        // The lane: leave it out ...
        std::vector<Step> skipSteps;
        const float skip = Solve(i + 1, p, v0, skipSteps);
        // ... or ride it: the best entry ring and exit point by the time to the next point (the next lane's entry or
        // the next fixed point).
        const Lane& L = it.lane;
        const float* anchor = nullptr;
        float anchorPos[3];
        if (i + 1 < items.size())
        {
            memcpy(anchorPos, items[i + 1].pos, sizeof(anchorPos)); // a lane's: the best path's entry ring
            anchor = anchorPos;
        }
        const Ride r = BestRide(L, p, v0, anchor, false);
        std::vector<Step> rideSteps;
        rideSteps.push_back(LaneStep(L, r));
        const float ride = r.tIn + r.tRide + Solve(i + 1, r.exit, 0, rideSteps);
        if (ride < skip)
        {
            out.insert(out.end(), rideSteps.begin(), rideSteps.end());
            return ride;
        }
        out.insert(out.end(), skipSteps.begin(), skipSteps.end());
        return skip;
    }
};
} // namespace

static bool Closed(const Lane& L, const std::vector<u32>& closed)
{
    for (u32 r : L.rings)
        if (std::find(closed.begin(), closed.end(), r) != closed.end())
            return true;
    // The mission's own lane permission (user, 19 Oct 2026: never attempt a lane that will be denied): closed when the
    // game's check denies every ring pair in this direction (space::LaneOpen 0); unknown (-1) or a pair the mission
    // opened keeps it (the travel's reactive check stays the backstop).
    bool anyOpen = false, anyKnown = false;
    for (size_t i = 0; i + 1 < L.rings.size() && !anyOpen; ++i)
    {
        const int o = space::LaneOpen(L.rings[i], L.rings[i + 1]);
        anyKnown |= o >= 0;
        anyOpen |= o != 0;
    }
    return anyKnown && !anyOpen && L.rings.size() >= 2;
}

// An arrival at an object that is not docked with (a planet - its dock is a separate ring -, a station without a dock,
// a wreck) ends outside it, not at its centre (19 Oct 2026: a travel to planet Manhattan flew into it): from the object
// toward `from`, past its physical radius and out of any costly zone (atmosphere damage, minefield: space::StrictAt),
// plus 300 m (Params::standOff). The object's radius from the game's own obstacles list.
static void StandOff(float pos[3], const float from[3], u32 object, const Params& prm)
{
    // A point just outside a lethal zone is as bad as one inside (21 Oct 2026, M01: the mission's point after the Donau
    // lay 129 m off Manhattan's 4100 m death zone - the avoidance's berth left a sliver, the ship swerved at the
    // atmosphere for 22 s and stopped 206 m short): out from the zone's centre to its surface + 300 m (Params::standOff).
    const float standOff = prm.standOff;
    if (!object)
        for (const space::Obstacle& ob : space::Obstacles(pos, standOff))
        {
            if (!ob.zone)
                continue;
            float d[3] = {pos[0] - ob.pos[0], pos[1] - ob.pos[1], pos[2] - ob.pos[2]};
            float len = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
            if (len >= ob.radius + standOff)
                continue;
            if (len < 1) // the zone's centre itself: out toward `from`
            {
                for (int i = 0; i < 3; ++i)
                    d[i] = from[i] - ob.pos[i];
                len = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
                if (len < 1)
                    continue;
            }
            for (int i = 0; i < 3; ++i)
                pos[i] = ob.pos[i] + d[i] / len * (ob.radius + standOff);
        }
    float radius = 0;
    for (const space::Obstacle& ob : space::Obstacles(pos, 1))
        if (object ? ob.id == object
                   : ob.zone) // a point (object 0): only a lethal zone it lies in, never a nameless rock
            radius = std::max(radius, ob.radius);
    float dir[3] = {from[0] - pos[0], from[1] - pos[1], from[2] - pos[2]};
    const float len = std::sqrt(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
    if (len < 1)
        return;
    for (float& d : dir)
        d /= len;
    float r = std::min(radius, len);
    const auto at = [&](float k, float out[3])
    {
        for (int i = 0; i < 3; ++i)
            out[i] = pos[i] + dir[i] * k;
    };
    float p[3];
    const float r0 = r;
    bool clear = false;
    // out of the costly zone (up to 20 km: Params::strictSteps x strictStep)
    for (int guard = 0; guard < prm.strictSteps && r < len; ++guard, r += prm.strictStep)
    {
        at(r, p);
        if (space::StrictAt(p).empty())
        {
            clear = true;
            break;
        }
    }
    // The field reaches all the way to the ship (M07 22 Oct 2026: Kress' base Bw08_01 inside its field - the stand-off
    // walked back to the ship's own position, the travel "arrived" at once and the nag failed the mission): the
    // destination is in the field, so stand off at its own radius and fly in.
    if (!clear)
        r = r0;
    if (r <= 0)
        return; // no radius known and no zone: a point object (a wreck's centre is fine)
    at(std::min(r + standOff, len), p);
    memcpy(pos, p, sizeof(p));
}

Plan Make(const flight::Model& m, const float pos[3], float speed, const std::vector<space::Waypoint>& wps, u32 system,
          const std::vector<u32>& closed, const Params& prm)
{
    Plan plan;
    std::map<u32, space::Contact> objects;
    for (const space::Contact& c : space::Contacts())
        objects[c.id] = c;
    // The entries of this system, in order: the first run of them (entries of a system left behind may still lead the
    // list; the list goes on beyond a gate into the next system).
    std::vector<space::Waypoint> here;
    for (const space::Waypoint& w : wps)
    {
        if (w.system == system)
            here.push_back(w);
        else if (!here.empty())
            break;
    }
    Planner pl{m, Law(), {}};
    pl.prm = prm;
    space::Pilot me;
    if (space::ReadPilot(me) && me.system == system)
    {
        const float far = 1e7f; // the whole system: zones and solars only count in Fly
        for (const space::Obstacle& o : space::Obstacles(pos, far))
            if (o.zone || (!o.asteroid && !o.ship && o.radius >= 300))
                pl.bodies.push_back(o);
        const float r = space::PlayerRadius();
        if (r > 0)
            pl.shipRadius = r;
    }
    for (size_t i = 0; i < here.size(); ++i)
    {
        const space::Waypoint& w = here[i];
        const auto o = objects.find(w.object);
        const u32 type = o != objects.end() ? o->second.type : 0;
        Item it;
        it.object = w.object;
        memcpy(it.pos, w.pos, sizeof(it.pos));
        if ((type & space::kTradelaneRing) && i + 1 < here.size())
        {
            const auto o2 = objects.find(here[i + 1].object);
            if (o2 != objects.end() && (o2->second.type & space::kTradelaneRing) &&
                LaneBetween(w.object, here[i + 1].object, it.lane))
            {
                // A ring closed this trip (a disrupted ride - lane hackers) cuts the best path's lane: the part beyond
                // its last closed ring stays its lane item (21 Oct 2026, Li01 147 -> 154 disrupted at 150: the item was
                // dropped, the next lane's entry was flown to directly and the ship cruised ~130 s beside 151-154 -
                // the split lanes below only serve fixed points). Closed by the mission: flown instead (left out).
                size_t cut = 0;
                for (size_t q = 0; q < it.lane.rings.size(); ++q)
                    if (std::find(closed.begin(), closed.end(), it.lane.rings[q]) != closed.end())
                        cut = q + 1;
                if (cut)
                {
                    Lane tail;
                    tail.speed = it.lane.speed;
                    for (size_t q = cut; q < it.lane.rings.size(); ++q)
                    {
                        tail.rings.push_back(it.lane.rings[q]);
                        tail.x.push_back(it.lane.x[q]), tail.y.push_back(it.lane.y[q]), tail.z.push_back(it.lane.z[q]);
                        tail.at.push_back(it.lane.at[q] - it.lane.at[cut]);
                    }
                    it.lane = tail;
                }
                if (it.lane.rings.size() >= 2 && !Closed(it.lane, closed))
                {
                    it.kind = Kind::Lane;
                    pl.items.push_back(it);
                }
                ++i;
                continue;
            }
        }
        if (type & (space::kJumpGate | space::kJumpHole))
        {
            it.kind = Kind::Jump;
            pl.items.push_back(it);
            break; // the rest is in the next system
        }
        if (i + 1 == here.size())
        {
            it.kind = o != objects.end() && o->second.dock ? Kind::Dock : Kind::Arrive;
            // A travel at a planet goes to its docking ring (user, 19 Oct 2026): the planet the arrival names, or the
            // one whose zone holds the point, with a ring (the game's link: the ring's dock_with = the planet's base) of
            // a faction not hostile to us. The fallback when the objective names no target (its target wins upstream).
            if (it.kind == Kind::Arrive)
            {
                u32 planet = o != objects.end() && (o->second.type & space::kPlanet) ? w.object : 0;
                if (!planet)
                    for (const auto& kv : objects)
                    {
                        const space::Contact& c = kv.second;
                        if (!(c.type & space::kPlanet))
                            continue;
                        for (const space::Obstacle& z : space::Obstacles(it.pos, 1))
                        {
                            const float d[3] = {it.pos[0] - z.pos[0], it.pos[1] - z.pos[1], it.pos[2] - z.pos[2]};
                            const float e[3] = {c.pos[0] - z.pos[0], c.pos[1] - z.pos[1], c.pos[2] - z.pos[2]};
                            if (z.zone && std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]) <= z.radius &&
                                std::sqrt(e[0] * e[0] + e[1] * e[1] + e[2] * e[2]) <= z.radius)
                                planet = c.id; // the point and the planet share the zone
                        }
                    }
                const u32 ring = space::PlanetDockRing(planet);
                const auto r = objects.find(ring);
                float att = 0;
                if (ring && r != objects.end() && (!space::AttitudeToPlayer(ring, att) || att > prm.ringAttitude))
                {
                    // to the ring, stopping beside it: the dock itself waits for the instruction to dock (a mission's
                    // "Fly to Planet X" comes before its "Dock with Planet X", with dialogue between)
                    it.object = ring;
                    memcpy(it.pos, r->second.pos, sizeof(it.pos));
                }
            }
            // (a point too: M03's "Fly to Planet California Minor" marks the planet's centre - the travel flew into its
            // zone and the avoidance fought it, 19 Oct 2026)
            if (it.kind == Kind::Arrive)
                StandOff(it.pos, pl.items.empty() ? pos : pl.items.back().pos, it.object, prm);
            pl.items.push_back(it);
        }
        // other intermediate entries (none seen) are left to the planner's choice of lanes
    }
    if (pl.items.empty())
        return plan;
    // Every lane of the system in both directions: from each first ring (no prev_ring) along next_ring to the end.
    for (const auto& kv : objects)
    {
        if (!(kv.second.type & space::kTradelaneRing))
            continue;
        u32 next = 0, prev = 0;
        if (!space::LaneLinks(kv.first, next, prev) || prev || !next)
            continue;
        Lane whole;
        Walk(kv.first, 0, true, whole);
        if (whole.rings.size() < 2 || Closed(whole, {})) // the mission's own permission: the whole lane
            continue;
        // A ring closed this trip (a ride disrupted there - pirates' lane hackers, 20 Oct 2026 Li01 - or refused) splits
        // the lane: no ride passes it, but the segments before and after it stay (was: the whole lane dropped, and the
        // re-plan cruised 27 km to the gate instead of re-entering beyond the ambush).
        size_t from = 0;
        for (size_t j = 0; j <= whole.rings.size(); ++j)
        {
            const bool cut =
                j == whole.rings.size() || std::find(closed.begin(), closed.end(), whole.rings[j]) != closed.end();
            if (!cut)
                continue;
            if (j - from >= 2)
            {
                Lane fwd;
                fwd.speed = whole.speed;
                for (size_t q = from; q < j; ++q)
                {
                    fwd.rings.push_back(whole.rings[q]);
                    fwd.x.push_back(whole.x[q]), fwd.y.push_back(whole.y[q]), fwd.z.push_back(whole.z[q]);
                    fwd.at.push_back(whole.at[q] - whole.at[from]);
                }
                Lane rev;
                rev.speed = fwd.speed;
                for (size_t q = fwd.rings.size(); q-- > 0;)
                {
                    rev.rings.push_back(fwd.rings[q]);
                    rev.x.push_back(fwd.x[q]), rev.y.push_back(fwd.y[q]), rev.z.push_back(fwd.z[q]);
                    rev.at.push_back(fwd.length() - fwd.at[q]);
                }
                pl.lanes.push_back(fwd);
                pl.lanes.push_back(rev);
            }
            from = j + 1;
        }
    }
    plan.time = pl.Solve(0, pos, speed, plan.steps);
    plan.ok = !plan.steps.empty();
    const Item& last = pl.items.back();
    const float* lastPos = last.kind == Kind::Lane ? nullptr : last.pos;
    if (lastPos)
        plan.direct = pl.Fly(pos, speed, lastPos, last.kind == Kind::Arrive);
    // Steps' eta: cumulative.
    float t = 0;
    std::ostringstream o;
    o.setf(std::ios::fixed);
    o.precision(1);
    for (Step& s : plan.steps)
    {
        t += s.eta;
        s.eta = t;
        switch (s.kind)
        {
        case Kind::Lane:
            o << "lane enter=" << s.object << " toward=" << s.toward << " end=" << s.endRing
              << " length=" << s.laneLength << " exit=" << s.exitAt;
            break;
        case Kind::Jump: o << "jump " << s.object; break;
        case Kind::Dock: o << "dock " << s.object; break;
        case Kind::Arrive: o << "arrive " << s.pos[0] << " " << s.pos[1] << " " << s.pos[2]; break;
        }
        o << " eta=" << s.eta << "\n";
    }
    o << "total=" << plan.time << " direct=" << plan.direct << " law=" << (Law().fromGame ? "game" : "default");
    plan.text = o.str();
    return plan;
}

void ArriveInstead(Plan& p, const float from[3], const Params& prm)
{
    if (p.steps.empty() || p.steps.back().kind != Kind::Dock)
        return;
    Step& s = p.steps.back();
    s.kind = Kind::Arrive;
    StandOff(s.pos, p.steps.size() > 1 ? p.steps[p.steps.size() - 2].pos : from, s.object, prm);
}

Plan MakeNow(const std::vector<u32>& closed, const Params& prm)
{
    space::Pilot p;
    flight::Model m;
    if (!space::ReadPilot(p) || !flight::ReadModel(m))
        return Plan();
    return Make(m, p.pos, 0, space::Waypoints(), p.system, closed, prm);
}

bool Command(const std::string& cmd, const std::string& rest, std::string* reply)
{
    if (cmd == "route")
    {
        // `route [ring ...]`: the plan from here along the current waypoint list, those rings closed as a disrupted ride
        // closes them (tests the lane split without waiting for lane hackers)
        std::vector<u32> closed;
        {
            std::istringstream in(rest);
            unsigned long v;
            while (in >> v)
                closed.push_back(static_cast<u32>(v));
        }
        const LARGE_INTEGER t0 = []
        {
            LARGE_INTEGER t;
            QueryPerformanceCounter(&t);
            return t;
        }();
        const Plan p = MakeNow(closed);
        LARGE_INTEGER t1, f;
        QueryPerformanceCounter(&t1);
        QueryPerformanceFrequency(&f);
        char ms[64];
        sprintf_s(ms, "\nplanned in %.1f ms", 1000.0 * (t1.QuadPart - t0.QuadPart) / f.QuadPart);
        *reply = p.ok ? p.text + ms : std::string("no plan (no waypoints in this system, or not flying)");
        return true;
    }
    if (cmd == "eta")
    {
        // `eta [from=<id>] <id> ...`: the plan's time to dock with each object of this system (lanes, fast docks), from
        // the ship (or from object `from=`, e.g. the base you are docked at). For choosing where to go (job boards:
        // travel time, not distance - user, 18 Oct 2026). The waypoint list is not touched.
        std::istringstream in(rest);
        std::string tok;
        flight::Model m;
        space::Pilot p;
        if (!space::ReadPilot(p) || !flight::ReadModel(m))
        {
            *reply = "not flying (the system and the flight model come from the ship in space)";
            return true;
        }
        float from[3] = {};
        const u32 system = p.system;
        memcpy(from, p.pos, sizeof(from));
        std::ostringstream o;
        o.setf(std::ios::fixed);
        o.precision(1);
        while (in >> tok)
        {
            if (tok.rfind("from=", 0) == 0)
            {
                if (!space::ObjectPos(static_cast<u32>(strtoul(tok.c_str() + 5, nullptr, 10)), from))
                    o << "from: unknown object\n";
                continue;
            }
            const u32 id = static_cast<u32>(strtoul(tok.c_str(), nullptr, 10));
            space::Waypoint w;
            if (!space::ObjectPos(id, w.pos))
            {
                o << "eta " << id << " -1 (unknown object)\n";
                continue;
            }
            w.object = id;
            w.system = system;
            const Plan pl = Make(m, from, 0, {w}, system);
            o << "eta " << id << " " << (pl.ok ? pl.time : -1.0f) << "\n";
        }
        *reply = o.str();
        return true;
    }
    if (cmd == "lanelaw")
    {
        const LaneLaw& l = Law();
        char line[200];
        sprintf_s(line, "lane law (%s): speed %.0f m/s, ramp %.1f m/s^2, end speed %.0f m/s",
                  l.fromGame ? "game" : "default", l.speed, l.accel, l.endSpeed);
        *reply = line;
        return true;
    }
    return false;
}
} // namespace route
