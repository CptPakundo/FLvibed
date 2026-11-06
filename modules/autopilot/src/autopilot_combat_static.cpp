// Advanced Autopilot - flying at static and big targets: the hull gap, hops, hold-and-salvo, platform cover, the strafe pass, the orbit.
// Called from UpdateCombat through autopilot_impl.h.
#include "autopilot_impl.h"
#include "city_hold.h"
#include "game/hudinfo.h"

namespace autopilot
{

// The distance from `from` to the first real hull on the straight line to the target's centre - the target's own hull
// included (the game's ray test); -1 when the test finds none (no collision mesh, out of space).
float HullGap(const float from[3], const float centre[3]) { return space::RayHitDistance(from, centre); }

// Hold-and-salvo at a static target (the top-3 any% speedrun recordings, every generator,
// satellite and platform in all three runs - docs\research\speedrun.md V4: in fast, brake ~100 m short, hold still
// 30-80 m off the hull and fire everything, leave on the kill; M09 ~9 s per generator, M13 barrier 63-87 s for four, M12
// 72-101 s for three; never an orbit or a pass). Source::Skill. The hold point lies `static_hold_gap` off the real hull
// (HullGap) on the best of 9 sides - the line from the target to us first, then 8 round it - by a path clear of solids,
// the fewest attackers with a clear line to it, and the way there; it is fixed per target and re-chosen only when the
// side turns out bad. Beyond the station's reach: the pursuit (afterburner, braking by the closing speed); then station
// keeping on the point (nose and guns on the target; the launches below fire from it - their blast rule reads the hull
// gap, not the centre distance). Hull lost at the hold - more than 4 % of it in 3 s - marks the side bad and hands over
// to the passes for 8 s (holding still under fighters "is just asking to be destroyed";
// the runners hold because the salvo kills in seconds and batteries soak the shield fire - Repairs). False = not a
// static target, or the passes' turn.
// The hop to a static target's hold point (the user watching M12: "tried to engage another
// generator without cruising its way through the tight trench at the lair's perimeter", "poor navigation between
// generators"; the user's try 3: charge on the kill, ~5 s at engine speed while turning, then 300 m/s along the trench's
// arc, the cruise cut short of the generator, held there): a way through the real geometry (avoid::RayPath, planned once
// per target, again when a leg is lost), flown point to point - on to the farthest point in plain sight - with the cruise
// when the flight model says it beats the afterburner over what is left, cut where its drop to engine speed ends at the
// hold's reach; the afterburner otherwise. False = not a hop (close, or no way found): the pursuit flies.
static bool HopFly(u32 target, const fl::Kinematics& k, Out& out)
{
    const float to[3] = {g.hold.pos[0] - k.pos[0], g.hold.pos[1] - k.pos[1], g.hold.pos[2] - k.pos[2]};
    const float far = Len3(to);
    if (far < 700)
    {
        g.hop.path.pts.clear();
        return false;
    }
    if (g.hop.target != target)
    {
        g.hop.target = target;
        g.hop.path = GeoPath();        // a new target: plan afresh
        g.hop.path.planesFirst = true; // (the planes: the 3D lattice's chords stalled the hops twice, session 7)
        g.hop.slowSince = -1;
        // (a new hop's charge starts from nothing: the last hop's wish stayed on through the hold - probe 5)
        g.hop.cruise = false;
        g.hop.cruiseAt = -10;
    }
    const float speed = Len3(k.vel);
    // Stuck against a hull (M12: 0 m/s for 40 s on a hop, pushing into the trench wall): under 10 m/s for 2 s
    // (not while charging at the start) -> 2 s backing off with a slide, then a new plan from where the ship is
    if (g.hop.backUntil > g.time)
    {
        out.pursue = false;
        out.station = false;
        out.brake = false;
        // (reverse acts with steering: the nose held where it points)
        float nose[3];
        for (int i = 0; i < 3; ++i)
            nose[i] = k.pos[i] - k.rot[i * 3 + 2] * 1000.0f;
        SteerTo(out, nose, 0, 0.35f, 0);
        out.steerReverse = true;
        out.slide = g.hop.backSlide;
        out.afterburn = false;
        return true;
    }
    if (speed < 10 && g.time - g.hop.path.plannedAt > 3)
    {
        if (g.hop.slowSince < 0)
            g.hop.slowSince = g.time;
        else if (g.time - g.hop.slowSince > 2)
        {
            g.hop.backUntil = g.time + 2.0f;
            g.hop.backSlide = 1 + (g.hop.backSlide % 4);
            g.hop.slowSince = -1;
            g.hop.path.pts.clear();
            g.hop.path.plannedAt = g.time - 10; // plan again once backed off
            events::Push("autopilot hop stuck (under 10 m/s for 2 s): backing off, slide %d, then a new way",
                         g.hop.backSlide);
            return true;
        }
    }
    else
        g.hop.slowSince = -1;
    float aim[3], rest = 0;
    char who[32];
    sprintf_s(who, "hop target=%u", target);
    if (!GeoPathAim(g.hop.path, k, g.hold.pos, target, aim, rest, who))
        return false;
    const size_t n = g.hop.path.pts.size() / 3;
    out.pursue = false;
    out.station = false;
    out.steerTo = true;
    memcpy(out.steerPos, aim, sizeof(out.steerPos));
    out.steerFullAngle = 0.35f;
    out.steerThrottle = 1.0f;
    out.steerReverse = false;
    float fwd[3];
    geom::Forward(fwd, k.rot);
    const float ad[3] = {aim[0] - k.pos[0], aim[1] - k.pos[1], aim[2] - k.pos[2]};
    const float cosNose = Dot3(fwd, ad) / std::max(1.0f, Len3(ad));
    bool cruise = false;
    flight::Model fm;
    float dbgCruiseT = -1, dbgBurnT = -1; // (the plan's numbers, logged with a change of the wish)
    if (flight::ReadModel(fm) && fm.cruise.ok())
    {
        // the cruise drops to engine speed in ~0.5 s (the user's brake 245 -> 110 m/s in 0.4 s): cut it that far plus
        // the hold's reach short of the point
        const float dropAt = speed * 0.6f + std::max(250.0f, fm.engine.top() * 2.5f);
        if (rest > dropAt)
        {
            // (hop_cruise_hold: the charge done counted by our own clock when the game's flag says none - probe 5, gen 2 -> 3:
            // "engaged=0" 4.1 s into a charge at 79 m/s; priced as a fresh 5 s charge each frame, the plan dropped it 0.9 s
            // before it completed (14.5 vs 14.3 s) and flipped the wish back and forth near the break-even - the flip-flop)
            const float ownCharge = g_set.hopCruiseHold && g.hop.cruise && g.hop.cruiseAt >= 0
                                        ? std::min(fm.cruiseChargeTime, g.time - g.hop.cruiseAt)
                                        : 0.0f;
            const float chargeDone =
                fm.cruiseEngaged && fm.cruiseChargeElapsed > 0 ? fm.cruiseChargeElapsed : ownCharge;
            const flight::RunPlan p = flight::PlanRun(fm, speed, rest - dropAt, fm.engine.top(), chargeDone,
                                                      acosf(Clampf(cosNose, -1, 1)), true);
            // a running cruise is kept; a charge at engine speed (a disruptor reset it) only while the plan still wins
            // with what is left of it - else the afterburner ("use afterburners to close the gap to
            // the next generator if it got disrupted relatively close to the next generator")
            const bool cruising = fm.cruiseEngaged && speed > fm.engine.top() + 30.0f;
            // (hop_cruise_hold: a charge once begun is kept unless the burner wins by more than a second, as the travel's
            // and the fast dock's - the user, 24 Oct 2026, M12: "terrible navigation from generator 1 to 2"; s10: the plan
            // re-read with the charge's own state flipped the wish every frame, 94-149 times in 6 of 9 hops, the charge
            // restarting at 78 m/s for seconds)
            const bool held =
                g_set.hopCruiseHold && (fm.cruiseEngaged || g.hop.cruise) && p.cruiseTime < p.burnerTime + 2.0f;
            cruise = cruising || p.cruise || held;
            dbgCruiseT = p.cruiseTime, dbgBurnT = p.burnerTime;
        }
        // A charge that does not complete (M12: 40 s "charging" at 80 m/s, reset by the Nomads' cruise
        // disruptors): after 8 s below cruise speed the hop takes the afterburner for 10 s
        // (the charge proper: still at engine speed - the ramp after it is no failure; at 251 m/s the first version cut a
        // working cruise)
        // (hop_reset_burn: a charge reset under fire - its elapsed time falls back - burns at once for 8 s, as the travel's
        // 4e6e6ef; s10-j-i1, gen 2 -> 3: the Nomads' disruptors reset the charge again and again, 15 s at 80 m/s until the
        // 8 s rule below, gen 3 35.7 s vs par 26.8)
        if (g_set.hopResetBurn && fm.cruiseEngaged)
        {
            if (g.hop.chargePrev > 0.5f && fm.cruiseChargeElapsed < g.hop.chargePrev - 0.3f && g_sit.attackers > 0 &&
                g.hop.noCruiseUntil < g.time)
            {
                g.hop.noCruiseUntil = g.time + 8.0f;
                cruise = false;
                events::Push("autopilot hop charge reset at %.1f s (attackers=%d): afterburner 8 s", g.hop.chargePrev,
                             g_sit.attackers);
            }
            g.hop.chargePrev = fm.cruiseChargeElapsed;
        }
        else
            g.hop.chargePrev = 0;
        const bool atEngine = speed < fm.engine.top() + 30.0f;
        if (ChargeFallback(g.hop.chargeSince, cruise && fm.cruiseEngaged && atEngine, true, g.time, 8.0f))
        {
            g.hop.noCruiseUntil = g.time + 10.0f;
            g.hop.chargeSince = -1;
            events::Push("autopilot hop charge did not complete in 8 s (disrupted?): afterburner for 10 s");
        }
        if (g.hop.noCruiseUntil > g.time)
            cruise = false;
    }
    if (cruise != g.hop.cruise)
    {
        g.hop.cruise = cruise;
        if (cruise)
            g.hop.cruiseAt = g.time;
        events::Push("autopilot hop cruise=%d rest=%.0f speed=%.0f leg=%u/%u engaged=%d charge=%.1f plan-cruise=%.1f "
                     "plan-burner=%.1f nose=%.2f no-cruise=%d",
                     cruise ? 1 : 0, rest, speed, static_cast<unsigned>(g.hop.path.idx + 1), static_cast<unsigned>(n),
                     fm.cruiseEngaged ? 1 : 0, fm.cruiseChargeElapsed, dbgCruiseT, dbgBurnT, cosNose,
                     g.hop.noCruiseUntil > g.time ? 1 : 0);
    }
    out.cruise = cruise ? 1 : 0;
    out.afterburn = !cruise && cosNose > 0.9f && rest > 400;
    g.hop.frame = g.frames; // (the dodges stand down while hopping)
    return true;
}

// The hold's gap to the target's hull: static_hold_gap, but never inside the blast of a loaded launcher (user, 23 Oct
// 2026, M12: "not launching torpedoes and missiles at its new very-close hold position" - at 14-21 m every salvo check
// read not ready, the launch rule wants the blast radius + 5 m; torpedoes 25 m) - its blast + 10 m while it has ammo.
// Read once a second.
// (hold_big_gap and big_standoff - a wider gap at a target of radius 500 m or more - were off after their tests and
// deleted 31 Oct 2026, AUD-A1-5)
static float g_holdTargetR = 0; // the radius StaticHold read last (the slide's sight test at a huge target)
static float HoldGapBase()
{
    static float at = -10, gap = 0;
    if (g.time - at < 1.0f && gap > 0 && g.time >= at) // (g.time restarts at 0 with every run: re-read then)
        return gap;
    at = g.time;
    gap = g_set.staticHoldGap + g.hold.gunGap;
    if (g_set.missiles)
        for (const space::Weapon& w : space::PlayerWeapons())
            if (w.homing && w.ammo > 0)
                gap =
                    std::max(gap, w.blastRadius + std::max(5.0f, space::PlayerRadius()) + 16.0f); // (the launch rule's
                                                                                                  // need + 6 m)
    return gap;
}
// (`city_hold_side`: 120-130 m at a hold the platforms guard - ab11's deaths held 31-67 m off the hull)
float HoldGap() { return g.hold.city ? cityhold::Gap(HoldGapBase()) : HoldGapBase(); }

// `city_hold_side`: the hostile armed solars guarding a static target - from the live objects, never by name: a solar
// other than the target, hostile to the player by the game's attitude (<= -0.6, HostileContact's cut), whose active guns
// the game's own turret test (space::GunsCovering: CEGun::CanPointAt + GetMunitionRange) says can point at and reach the
// target's centre - or, when none can point there, one of type WEAPONS_PLATFORM (FLHookSDK Enums.hpp ObjectType
// WeaponPlatform = 1 << 12; solararch.ini dyson_city_platform `type = WEAPONS_PLATFORM`, 8 nomad_dyson_turret01 in
// loadouts.ini: 1,500 m/s x 2 s = 3 km reach) within 3 km of the target's hull. Up to 8, with their positions.
static int GuardPlatforms(u32 target, const float tpos[3], float radius, u32* ids, float (*pos)[3], int cap)
{
    constexpr u32 kWeaponPlatform = 1u << 12;
    int n = 0;
    for (const space::Contact& c : Contacts())
    {
        if (n >= cap)
            break;
        if (c.ship || c.id == target || c.health == 0 || c.maxHit >= 1e30f)
            continue;
        const float dc[3] = {c.pos[0] - tpos[0], c.pos[1] - tpos[1], c.pos[2] - tpos[2]};
        if (Len3(dc) > radius + 3000.0f)
            continue;
        float att = 0;
        if (!space::AttitudeToPlayer(c.id, att) || att > -0.6f)
            continue;
        float pt[1][3] = {{tpos[0], tpos[1], tpos[2]}};
        int cnt = 0;
        const int guns = space::GunsCovering(c.id, pt, 1, &cnt);
        if (guns <= 0 || (cnt <= 0 && !(c.type & kWeaponPlatform)))
            continue;
        ids[n] = c.id;
        memcpy(pos[n], c.pos, sizeof(pos[n]));
        ++n;
    }
    return n;
}

// `city_hold_side`'s platform-cover score of points: per point, the guarding platforms' guns that can point at it, reach
// it (space::GunsCovering) and see it (no solid between: space::SightBlocker, the platform itself skipped - the target's
// own hull shelters). Lower is safer; -1 when no platform's guns could be read.
static void PlatformCover(const u32* ids, const float (*ppos)[3], int np, const float (*pts)[3], int n, int* cover)
{
    for (int i = 0; i < n; ++i)
        cover[i] = -1;
    for (int k = 0; k < np; ++k)
    {
        int counts[16] = {};
        if (space::GunsCovering(ids[k], pts, std::min(n, 16), counts) < 0)
            continue;
        for (int i = 0; i < n && i < 16; ++i)
        {
            if (cover[i] < 0)
                cover[i] = 0;
            if (counts[i] > 0 && !space::SightBlocker(ppos[k], pts[i], ids[k]))
                cover[i] += counts[i];
        }
    }
}

bool StaticHold(u32 target, Out& out)
{
    if (!g_set.staticHold || !target)
    {
        g.hold.on = false;
        return false;
    }
    const space::Contact tc = FindContact(target);
    fl::Kinematics k = {};
    // (a ship in between - a fighter the race made the target for a while - keeps the held generator's point: the M09
    // run re-chose and re-approached the fourth generator six times; the hold belongs to holdTarget)
    if (!tc.id || tc.ship || tc.maxHit <= 0 || !fl::PlayerKinematics(k))
        return false;
    // ...never at an armed one: the blind-spot run's (M13's Nomad battleship, a solar: the hold
    // re-anchored 42 m off its hull, hull 0.98 -> 0.23 in a second - the 22 Oct pass, before the hold existed, kept to its
    // blind spots)
    if (g.combat.blindTarget == target && g.combat.blindGuns >= 4)
    {
        g.hold.on = false;
        return false;
    }
    if (g.hold.target != target)
    {
        g.hold.target = target;
        g.hold.on = false;
        g.hold.at = false;
        g.hold.offUntil = -1;
        g.hold.hull = -1;
        g.hold.badDirs.clear();
        g.hold.gunGap = 0;
        g.hold.gunN = -1;
        g.hold.gunDone = false;
        g.hold.city = false;
        g.hold.cityPlatforms = 0;
    }
    if (!g_set.cityHoldSide)
        g.hold.city = false; // (switched off live: today's hold at once)
    if (g.hold.offUntil > g.time)
    {
        g.hold.on = false;
        return false; // the passes' turn after a bad hold
    }
    // Exposed: attackers close, the shield nearly gone and no battery to restore it - no holding still then (
    // M13 St03 22 Oct 2026: at the hold under three Nomad fighters, batteries spent on the first generators, shield 0.17,
    // the hull went 0.94 -> dead in 1.5 s - faster than any hull-loss verdict; the runners hold only while batteries keep
    // their shield up). The ship keeps moving - a wide orbit - until the shield is back.
    {
        ai::Self self;
        if (ai::ReadSelf(self) && self.batteries == 0 && self.shield < 0.25f)
        {
            int close = 0;
            for (const space::Contact& c : Contacts())
                if (c.ship && c.targetsPlayer && c.distance < 700)
                    ++close;
            if (close > 0)
            {
                // ...but never off the target (speedrun #1: "as soon as a Nomad generator was targeted
                // it was fired upon without pause until it was destroyed"): the orbit widens for 6 s, the guns stay on
                if (g.hold.evadeUntil < g.time)
                    events::Push(
                        "autopilot static-hold exposed shield=%.2f batteries=0 attackers=%d: wide orbit for 6 s",
                        self.shield, close);
                g.hold.evadeUntil = g.time + 6.0f;
            }
        }
    }
    float radius = 0;
    for (const space::Obstacle& o : space::Obstacles(tc.pos, 50))
        if (o.id == target)
            radius = o.radius;
    radius = std::max(radius, 30.0f);
    g_holdTargetR = radius;
    // (big_pass - a target of radius 500 m or more fought in attack passes, not held - was off after its tests and
    // deleted 31 Oct 2026, AUD-A1-5; `big_coast` first: the coast in, then the hold)
    const float rel[3] = {k.pos[0] - tc.pos[0], k.pos[1] - tc.pos[1], k.pos[2] - tc.pos[2]};
    const float d = std::max(1.0f, Len3(rel));
    if (!g.hold.on)
    {
        // the sides: toward us, then 8 round the line (across it and a little toward us)
        const float r0[3] = {rel[0] / d, rel[1] / d, rel[2] / d};
        float u[3] = {r0[2], 0.0f, -r0[0]};
        if (Len3(u) < 0.2f)
            u[0] = 0, u[1] = -r0[2], u[2] = r0[1];
        geom::Norm(u);
        float v[3];
        geom::Cross(v, r0, u);
        std::vector<u32> attackers;
        for (const space::Contact& c : Contacts())
            if (c.ship && c.targetsPlayer && c.distance < 2500 && attackers.size() < 8)
                attackers.push_back(c.id);
        // where we go after this kill (the objective mode's leave points, fresh): the nearest other one to the target;
        // its side of the target is preferred, 100 m nearer to it = 25 (an attacker's line = 100)
        const float* leaveTo = nullptr;
        if (g_obj.on && g_obj.leaveN && gameclock::NowMs64() - g_obj.leaveAt < 3000)
        {
            float lb = 1e30f;
            for (int n = 0; n < g_obj.leaveN; ++n)
            {
                const float* q = &g_obj.leave[n * 3];
                const float dq[3] = {q[0] - tc.pos[0], q[1] - tc.pos[1], q[2] - tc.pos[2]};
                if (Len3(dq) > radius * 3.0f && Len3(dq) < lb)
                    lb = Len3(dq), leaveTo = q;
            }
        }
        // `city_hold_side`: the platforms guarding the target, read once per hold choice (their side decides the point)
        u32 platIds[8] = {};
        float platPos[8][3] = {};
        int np = 0;
        g.hold.city = false;
        if (g_set.cityHoldSide)
        {
            np = GuardPlatforms(target, tc.pos, radius, platIds, platPos, 8);
            g.hold.cityPlatforms = np;
            g.hold.city = cityhold::AwayDir(tc.pos, platPos, np, g.hold.cityAway);
            if (!g.hold.city)
            {
                static u32 saidOff = 0;
                if (saidOff != target)
                    events::Push("autopilot city-hold off target=%u platforms=%d: no side to take, today's hold", target,
                                 np);
                saidOff = target;
            }
        }
        // the point on a side: the real hull there - a ray from just outside the target's sphere to its centre (from
        // farther out it met the structures around first - M13 St03: the sphere's towers put hold points 500 m out) -
        // and the hold's gap beyond it
        const auto pointOn = [&](const float dir[3], float far[3], float p[3])
        {
            for (int j = 0; j < 3; ++j)
                far[j] = tc.pos[j] + dir[j] * (radius + 5.0f);
            const float h = HullGap(far, tc.pos);
            const float surface = h >= 0 ? radius + 5.0f - h : radius; // from the centre
            for (int j = 0; j < 3; ++j)
                p[j] = tc.pos[j] + dir[j] * (surface + HoldGap());
        };
        float best = 1e30f;
        // (city_hold_side: the candidates on the side, 0 / +-22.5 / +-45 deg at the target's height, then two below it)
        constexpr int kCityBand = 5, kCityAll = 7;
        const float cityAz[kCityAll] = {0.0f, -22.5f, 22.5f, -45.0f, 45.0f, 0.0f, 0.0f};
        float cityP[kCityAll][3] = {}, cityDir[kCityAll][3] = {}, cityScore[kCityAll] = {}, cityClear[kCityAll] = {};
        int cityOpen[kCityAll] = {}, cityCover[kCityAll] = {};
        const float cityEl[kCityAll] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -15.0f, -30.0f};
        for (int i = g.hold.city ? 0 : -1; i < (g.hold.city ? kCityAll : 8); ++i)
        {
            float dir[3];
            if (g.hold.city)
                cityhold::SideDir(g.hold.cityAway, cityAz[i], cityEl[i], dir);
            else if (i < 0)
                memcpy(dir, r0, sizeof(dir));
            else
            {
                const float a = static_cast<float>(i) * 3.14159265f / 4.0f;
                for (int j = 0; j < 3; ++j)
                    dir[j] = cosf(a) * u[j] + sinf(a) * v[j] + 0.5f * r0[j];
                geom::Norm(dir);
            }
            float far[3], p[3];
            pointOn(dir, far, p);
            float score = 0;
            if (space::RayHitDistance(k.pos, p, &target, 1) >= 0)
                score += 10000; // the way there meets a solid (other than the target: going round it is the pursuit's)
            if (space::RayHitDistance(far, p) >= 0)
                score += 3000; // the point itself sits inside or behind structure
            for (size_t b = 0; b + 2 < g.hold.badDirs.size(); b += 3)
                if (dir[0] * g.hold.badDirs[b] + dir[1] * g.hold.badDirs[b + 1] + dir[2] * g.hold.badDirs[b + 2] > 0.8f)
                    score += 5000; // held there and lost hull
            for (u32 id : attackers)
            {
                const space::Contact ac = FindContact(id);
                if (ac.id && !space::SightBlocker(ac.pos, p, id))
                    score += 100; // in that attacker's line
            }
            // `hold_shelter` (M13-MIMIC item 3b; the owner, 31 Oct 2026, the barrier generators: "the ship stays out in
            // the open and tanking a lot of hits ... scan by ray the surrounding area to figure out a safer position"):
            // fighters move, so the shelter is geometry - each of 26 directions with no solid within 400 m of the
            // point is an open line of fire (40 each: a fully open point weighs ~10 attackers' lines)
            int open = -1;
            if (g_set.holdShelter)
            {
                open = 0;
                for (int x = -1; x <= 1; ++x)
                    for (int y = -1; y <= 1; ++y)
                        for (int z = -1; z <= 1; ++z)
                        {
                            if (!x && !y && !z)
                                continue;
                            const float l = 400.0f / sqrtf(static_cast<float>(x * x + y * y + z * z));
                            const float q[3] = {p[0] + x * l, p[1] + y * l, p[2] + z * l};
                            open += space::RayHitDistance(p, q) < 0 ? 1 : 0;
                        }
                score += 40.0f * open;
            }
            const float pd[3] = {p[0] - k.pos[0], p[1] - k.pos[1], p[2] - k.pos[2]};
            score += Len3(pd) / 100.0f;
            if (leaveTo)
            {
                const float lq[3] = {leaveTo[0] - p[0], leaveTo[1] - p[1], leaveTo[2] - p[2]};
                score += Len3(lq) / 4.0f;
            }
            if (g.hold.city)
            {
                // the ordnance clearance: the launch rule's six 100 m rays (cityOrdOk) from the point - a hull inside
                // them holds every torpedo / missile there (probe ch1: the second generator's side point
                // sat 75-87 m from another city structure, 7 torpedoes held over 18 s)
                float clear = -1.0f;
                for (int axis = 0; axis < 3; ++axis)
                    for (int sgn = -1; sgn <= 1; sgn += 2)
                    {
                        float to[3] = {p[0], p[1], p[2]};
                        to[axis] += sgn * cityhold::kOrdnanceClear;
                        const float h = space::RayHitDistance(p, to);
                        if (h >= 0 && (clear < 0 || h < clear))
                            clear = h;
                    }
                if (clear >= 0)
                    score += 2000; // (above any platform-cover difference, below the path / structure penalties)
                memcpy(cityP[i], p, sizeof(p));
                memcpy(cityDir[i], dir, sizeof(dir));
                cityScore[i] = score;
                cityOpen[i] = open;
                cityClear[i] = clear;
            }
            else if (score < best)
            {
                best = score;
                memcpy(g.hold.pos, p, sizeof(p));
                memcpy(g.hold.dir, dir, sizeof(dir));
                g.hold.open = open;
            }
        }
        if (g.hold.city)
        {
            // the platform-cover score (the guns that can point at, reach and see each point: 30 each, an attacker's line
            // weighing ~3 guns) picks among the band's points; a deeper one only when clearly less covered and as
            // reachable (none of the solid penalties the band's best is free of)
            PlatformCover(platIds, platPos, np, cityP, kCityAll, cityCover);
            int bi = -1;
            for (int i = 0; i < kCityBand; ++i)
            {
                const float s = cityScore[i] + 30.0f * std::max(0, cityCover[i]);
                if (bi < 0 || s < best)
                    best = s, bi = i;
            }
            int pick = bi;
            for (int i = kCityBand; i < kCityAll; ++i)
                if (cityhold::DeeperSafer(cityCover[pick], cityCover[i]) &&
                    cityScore[i] < cityScore[bi] + 1000.0f) // (no path / structure penalty the band's best lacks)
                    pick = i;
            if (pick != bi)
                best = cityScore[pick] + 30.0f * std::max(0, cityCover[pick]);
            memcpy(g.hold.pos, cityP[pick], sizeof(g.hold.pos));
            memcpy(g.hold.dir, cityDir[pick], sizeof(g.hold.dir));
            g.hold.open = cityOpen[pick];
            // (clear = the nearest hull within the six 100 m rays of the point, -1 none; held = candidates with one)
            int held = 0;
            for (int i = 0; i < kCityAll; ++i)
                held += cityClear[i] >= 0 ? 1 : 0;
            events::Push("autopilot city-hold side=%.0f deg gap=%.0f platforms=%d target=%u cover=%d band-cover=%d "
                         "deep=%d dy=%.0f deep-cover=%d/%d clear=%.0f held=%d/%d",
                         cityhold::OffDeg(g.hold.cityAway, g.hold.dir), HoldGap(), np, target, cityCover[pick],
                         cityCover[bi], pick >= kCityBand ? 1 : 0, g.hold.pos[1] - tc.pos[1], cityCover[kCityBand],
                         cityCover[kCityBand + 1], cityClear[pick], held, kCityAll);
        }
        g.hold.on = true;
        g.hold.at = false;
        g.hold.hull = -1;
        g.hold.orbitDir = (g.frames & 1) ? 1.0f : -1.0f; // the slide's one direction for this hold
        g.hold.slideT = 0;
        const float hc[3] = {g.hold.pos[0] - tc.pos[0], g.hold.pos[1] - tc.pos[1], g.hold.pos[2] - tc.pos[2]};
        events::Push("autopilot static-hold target=%u radius=%.0f dist=%.0f point=%.0f m from the centre score=%.0f "
                     "open=%d",
                     target, radius, d, Len3(hc), best, g.hold.open);
    }
    // The hull ahead decides, not the centre (the user watching the full M13: "stopped firing weapons and got
    // too close to the shield generator" - a 666 m structure: the pursuit's band from its centre and the hold point off
    // its real hull never met, the ship closed to 150 m from the centre among the structure, hull 1.00 -> 0.69, the guns
    // mostly silent). Closer to the real hull ahead than the gap + 60 m: the hold point moves onto our own line at the
    // gap - held there, or pushed back out when we have come closer than the gap. The target's own hull only: the hit
    // inside its sphere (the ray met the M12 lair's lattice and the M13 sphere's towers 2-3 km
    // short of the generator - "re-anchored at hull gap 120 m (centre 3049 m)", held there 8 min without a shot).
    {
        const float gapNow = HullGap(k.pos, tc.pos);
        const bool ownHull = gapNow >= 0 && d - gapNow <= radius + 10.0f;
        // (pushed back out below the launch rule's need, HoldGap - 6 m, + 1 m: at 0.7x the gap the ship drifted in to a
        // 34-35 m gap and sat there - torpedoes need ~42 m - 15-20 s without one launch at M12 gens 2 and 3, s9j-i2)
        bool reanchor = ownHull && (gapNow < HoldGap() - 5.0f || (!g.hold.at && gapNow < HoldGap() + 60.0f));
        // (`city_hold_side`: onto our own line only with that line on the side and the gap it sets >= 100 m - ab11's
        // holds re-anchored onto the outward face at 31-61 m on the way in; the chosen point stays the goal otherwise)
        if (reanchor && g.hold.city)
        {
            const float line[3] = {rel[0] / d, rel[1] / d, rel[2] / d};
            const float want = HoldGap() + (gapNow < HoldGap() ? 8.0f : 0.0f);
            if (!cityhold::ReanchorOk(g.hold.cityAway, line, want))
            {
                reanchor = false;
                if (g.time - g.hold.citySaidAt >= 2.0f || g.time < g.hold.citySaidAt)
                    events::Push("autopilot city-hold re-anchor refused target=%u line=%.0f deg off the side gap=%.0f "
                                 "want=%.0f platforms=%d",
                                 target, cityhold::OffDeg(g.hold.cityAway, line), gapNow, want, g.hold.cityPlatforms);
                g.hold.citySaidAt = g.time;
            }
        }
        if (reanchor)
        {
            const float tl = std::max(1.0f, d);
            // (pushed back: 8 m past the gap - an 8 m correction sat inside the station keeping, the ship stayed at a 39-41
            // m gap at M12 gen 3, s9l-i1, the slide round the centre carrying it over the hull's bulge)
            const float back = gapNow < HoldGap() ? 8.0f : 0.0f;
            for (int i = 0; i < 3; ++i)
                g.hold.pos[i] = k.pos[i] - rel[i] / tl * (gapNow - HoldGap() - back);
            for (int i = 0; i < 3; ++i)
                g.hold.dir[i] = rel[i] / tl;
            if (!g.hold.at)
                events::Push("autopilot static-hold re-anchored at hull gap %.0f m (centre %.0f m)", gapNow, d);
            g.hold.at = true;
        }
    }
    // Never a hold point in a lethal sphere's berth (gate mpb23: a hold at an ice moon's hull, 102 m gap, sat
    // 52 m off its death zone; the lethal guard steered out, the hold pulled back in - dead): out to the edge + 150 m
    // along the line from the sphere's centre.
    constexpr float kLethalBerth = 150.0f;
    float zr = 0;
    if (OutOfLethalBerth(g.hold.pos, kLethalBerth, rel, &zr)) // (rel: target -> ship, the way out at the centre itself)
        events::Push("autopilot static-hold out of a lethal zone r=%.0f: the point %.0f m from its centre", zr,
                     zr + kLethalBerth);
    const float to[3] = {g.hold.pos[0] - k.pos[0], g.hold.pos[1] - k.pos[1], g.hold.pos[2] - k.pos[2]};
    const float far = Len3(to);
    const float speed = Len3(k.vel);
    // station keeping (<= 60 m/s) once the ship can brake into it: 2.5 s of the speed, at least 250 m; left only 150 m
    // beyond that (the threshold moves with the speed: without the margin it flipped every frame)
    // (M12: entered at 2.5 s of the speed / >= 250 m, the station keeping's 60 m/s crawled the
    // last 400-500 m to generators 2 and 3 - 14 s each; the user's try 3 arrives fast and brakes hard): the braking
    // distance (full reverse, the flight model) + 60 m, at least 120 m
    float reach = std::max(250.0f, speed * 2.5f);
    {
        flight::Model fm;
        if (flight::ReadModel(fm))
            reach = std::max(120.0f, flight::BrakeDistance(fm, speed, 0) + 60.0f);
    }
    const bool atPoint = g.hold.at ? far < reach + 150.0f : far < reach;
    if (atPoint && !g.hold.at)
        events::Push("autopilot static-hold at point dist=%.0f speed=%.0f", far, speed);
    // Every gun on the target (`static_hold_guns`; the owner, 28 Oct 2026, M13: "too close to the generator and not
    // firing everything it has"): held with the nose on the target, the game's own test per gun (CEGun::CanPointAt at
    // its 5 deg, the tolerance its CanFire accepts) - fewer guns that can point at the centre than are fitted = the hold
    // backs off 20 m (each 1.5 s, while it can still reach), for this target.
    // (`city_hold_side`: its gap is the rule's 120-130 m - the steps are not taken there)
    if (g_set.staticHoldGuns && !g.hold.city && atPoint && far < 150 && g.time - g.hold.gunGapAt >= 1.5f)
    {
        g.hold.gunGapAt = g.time;
        const float gapNow = HullGap(k.pos, tc.pos);
        float pt[1][3] = {{tc.pos[0], tc.pos[1], tc.pos[2]}};
        // `hold_guns_hull` (M13-MIMIC item 5): the guns are counted at the hull point they fire at (aim::Current aims a
        // huge static's hull point, the ray), not at its centre - a city generator's centre lies ~430 m behind its hull,
        // so few guns could point at it from close and the hold stepped out (td1: 549-562 m from the centre, "guns 1-2
        // of 5", died; R1 holds under the satellite's cone, the generator filling the view)
        if (g_set.holdGunsHull && gapNow >= 0)
        {
            const float dl = std::max(1.0f, d);
            for (int i = 0; i < 3; ++i)
                pt[0][i] = k.pos[i] - rel[i] / dl * gapNow;
        }
        int n = 0;
        const int guns = space::GunsCovering(space::PlayerShipId(), pt, 1, &n, 0.087f);
        const float gunReach = 520.0f; // (the shortest common gun reach, 550 m, less a margin)
        // (a step that brought no gun more ends it, back one step: a gun that never points - its mount faces away - kept
        // the first runs stepping out to +240 m, ~700 m off the generators, 4 of 5 guns at every gap)
        if (g.hold.gunGap > 0 && n <= g.hold.gunN && g.hold.gunN >= 0)
        {
            if (!g.hold.gunDone)
                events::Push("autopilot static-hold guns %d of %d: no gain at +%.0f m, kept +%.0f m", n, guns,
                             g.hold.gunGap, g.hold.gunGap - 20.0f);
            if (!g.hold.gunDone)
                g.hold.gunGap -= 20.0f;
            g.hold.gunDone = true;
        }
        g.hold.gunN = n;
        if (!g.hold.gunDone && guns > 0 && n < guns && gapNow >= 0 && gapNow + 20.0f < gunReach - 50.0f &&
            g.hold.gunGap < 300.0f)
        {
            g.hold.gunGap += 20.0f; // (the re-anchor above pushes the point out once HoldGap reads it)
            events::Push("autopilot static-hold guns %d of %d point at the target: gap +20 m (extra %.0f m)", n, guns,
                         g.hold.gunGap);
        }
    }
    g.hold.at = atPoint;
    // the hold's verdict: hull lost while holding (sampled each second over 3 s)
    const float hull = ai::PlayerHealth();
    if (atPoint && far < 150 && hull >= 0)
    {
        if (g.hold.hull < 0)
            g.hold.hull = hull, g.hold.hullT = g.time;
        else if (g.time - g.hold.hullT >= 3.0f)
        {
            if (g.hold.hull - hull > 0.04f)
            {
                // that side is bad: the next frame chooses another; the pursuit keeps the guns on the target on the way
                // (never off it - the user's speedrun #1 reading; was: 8 s of attack passes whose exit runs stop firing)
                g.hold.badDirs.insert(g.hold.badDirs.end(), {g.hold.dir[0], g.hold.dir[1], g.hold.dir[2]});
                g.hold.on = false;
                events::Push("autopilot static-hold bad hull %.2f -> %.2f in %.0f s: another side", g.hold.hull, hull,
                             g.time - g.hold.hullT);
                g.hold.hull = -1;
            }
            g.hold.hull = hull, g.hold.hullT = g.time;
        }
    }
    else
        g.hold.hull = -1;
    out.pursue = true;
    out.steerTo = false;
    if (atPoint)
    {
        out.station = true;
        out.afterburn = false;
        // the slide (speedrun #1 M12 / M13: "near-contact sideways movements", "light orbit
        // movements"; measured from the far background, reference\motion: a slow ONE-WAY slide round the generator,
        // ~5 deg/s = 3-11 m/s at the hold, no reversal within a hold, sometimes none): the hold point itself moves round
        // the target on our horizontal tangent at static_hold_slide deg/s, its distance kept - the slides move the ship,
        // the nose and guns stay on. One direction per hold; it stops where the way is not clear. Exposed (shield low, no
        // battery, attackers close): three times as fast - harder to hit, still firing.
        const float dt = g.hold.slideT > 0 ? std::min(0.2f, g.time - g.hold.slideT) : 0.0f;
        g.hold.slideT = g.time;
        if (g_set.staticHoldSlide > 0 && dt > 0)
        {
            const float rc[3] = {g.hold.pos[0] - tc.pos[0], g.hold.pos[1] - tc.pos[1], g.hold.pos[2] - tc.pos[2]};
            const float R = std::max(1.0f, Len3(rc));
            const float n[3] = {rc[0] / R, rc[1] / R, rc[2] / R};
            float u[3] = {n[2], 0.0f, -n[0]}; // horizontal tangent (n x up)
            if (Len3(u) < 0.2f)
                u[0] = 0, u[1] = -n[2], u[2] = n[1];
            geom::Norm(u);
            // the lateral run (the user: "a pure lateral run along the hull"): a speed along the hull, not a rate
            float rate = g_set.staticHoldSlide * (g.hold.evadeUntil > g.time ? 3.0f : 1.0f) * 3.14159265f / 180.0f;
            if (g_set.staticHoldLateral > 0)
                rate = g_set.staticHoldLateral / R;
            // (hold_jink and big_jink - the run along the hull under fire, reversing every 2.5-4.5 s - were off after
            // their tests and deleted 31 Oct 2026, AUD-A1-5)
            const float step = g.hold.orbitDir * rate * R * dt;
            float p[3];
            for (int i = 0; i < 3; ++i)
                p[i] = rc[i] + u[i] * step;
            const float pl = std::max(1.0f, Len3(p));
            for (int i = 0; i < 3; ++i)
                p[i] = tc.pos[i] + p[i] / pl * R; // back onto the sphere: the distance kept
            // (a step ahead of the point itself tested: at speed the point moves ~a metre a frame)
            float probe[3];
            for (int i = 0; i < 3; ++i)
                probe[i] =
                    g.hold.pos[i] + (p[i] - g.hold.pos[i]) * std::max(1.0f, 40.0f / std::max(0.01f, fabsf(step)));
            // (at a huge target the clear line to its hull in front, not to its centre: the Dyson city's structure stood
            // between every run point and the generator's centre, so the run never moved - "still holds still")
            bool sightOk = true;
            if (g_holdTargetR >= 500.0f)
            {
                const float toC[3] = {tc.pos[0] - p[0], tc.pos[1] - p[1], tc.pos[2] - p[2]};
                const float h = space::RayHitDistance(p, tc.pos);
                const float cl = std::max(1.0f, Len3(toC));
                float hp[3];
                for (int i = 0; i < 3; ++i)
                    hp[i] = p[i] + toC[i] / cl * std::max(0.0f, (h >= 0 ? h : cl) - 5.0f);
                sightOk = !space::SightBlocker(p, hp, target);
            }
            else
                sightOk = !space::SightBlocker(p, tc.pos, target);
            // (`city_hold_side`: the slide stays on the side - at its edge the slide stops, or the lateral run reverses)
            bool sideOk = true;
            if (g.hold.city)
            {
                const float pr[3] = {p[0] - tc.pos[0], p[1] - tc.pos[1], p[2] - tc.pos[2]};
                sideOk = cityhold::OnSide(g.hold.cityAway, pr);
            }
            if (space::RayHitDistance(g.hold.pos, probe, &target, 1) < 0 && sightOk && sideOk)
            {
                memcpy(g.hold.pos, p, sizeof(p));
                memcpy(g.hold.dir, n, sizeof(n));
            }
            else if (g_set.staticHoldLateral > 0 && g.time - g.hold.flipAt > 2.0f)
            {
                // the lateral run meets geometry: back the other way (the slow slide stopped there)
                g.hold.flipAt = g.time;
                g.hold.orbitDir = -g.hold.orbitDir;
                events::Push("autopilot static-hold lateral run blocked: reversing");
            }
            memcpy(out.stationPos, g.hold.pos, sizeof(out.stationPos));
        }
        // all mines too ("all weapons, all missiles and all mines until each generator was destroyed")
        // ...only sliding along the hull at near-zero distance ("deploying mines while at a distance
        // isn't helpful. Players deployed mines because they orbited / moved laterally at a distance of near zero from the
        // generator's outer hull" - a mine leaves behind the ship and seeks over metres: from the old 60 m hold none hit)
        if (g_set.staticHoldMines && g.time - g.hold.mineAt > 1.5f)
        {
            const float gapNow = HullGap(k.pos, tc.pos);
            const float toT[3] = {tc.pos[0] - k.pos[0], tc.pos[1] - k.pos[1], tc.pos[2] - k.pos[2]};
            const float tl = std::max(1.0f, Len3(toT));
            const float along = Dot3(k.vel, toT) / tl;
            const float lateral = sqrtf(std::max(0.0f, Dot3(k.vel, k.vel) - along * along));
            // (`city_hold_side`: a mine seeks the nearest hull and bursts there - that hull >= 100 m off, or none)
            const bool cityMineOk = !g.hold.city || cityhold::OrdnanceOk(gapNow, gapNow);
            if (gapNow >= 0 && gapNow < g_set.staticHoldMineGap && lateral > 3.0f && !cityMineOk)
            {
                static Throttle said;
                if (said.Due(g.time, 2))
                    events::Push("autopilot city-hold launch-held target=%u kind=mine impact=%.0f hull=%.0f need=%.0f",
                                 target, gapNow, gapNow, cityhold::kOrdnanceClear);
            }
            else if (gapNow >= 0 && gapNow < g_set.staticHoldMineGap && lateral > 3.0f && mprules::Autopilot() == mprules::Allow::Full)
            {
                g.hold.mineAt = g.time;
                out.mine = true;
                events::Push("autopilot hold-mine target=%u gap=%.0f lateral=%.0f", target, gapNow, lateral);
            }
        }
    }
    else
    {
        out.station = false;
        if (g_set.staticHop && HopFly(target, k, out))
            return true;
        // the pursuit's band at the hold point's distance from the centre (it brakes by the closing speed)
        const float hp[3] = {g.hold.pos[0] - tc.pos[0], g.hold.pos[1] - tc.pos[1], g.hold.pos[2] - tc.pos[2]};
        out.pursueBand = Len3(hp);
        // `city_hold_side` near the target but off its point: the station keeping carries the ship to the side point with
        // the nose (and the guns) on the target - straight when the line is clear, else round the centre in 35 deg steps
        // at the point's distance (probe ch1: the pursuit held its own band on the arrival side, 131 deg off the point,
        // 34 s). (Here, after the hop: ch2's first build set it above this branch, which turned it off again - 296 of
        // 374 frames at final_generator5 read `pursue`, the side reached in 1 of 3 runs - run-judge 31 Oct 2026)
        if (g.hold.city && d < Len3(hp) + 400.0f)
        {
            float wp[3];
            float left = 0.0f;
            if (space::RayHitDistance(k.pos, g.hold.pos) >= 0)
                left = cityhold::TransitStep(tc.pos, k.pos, g.hold.pos, wp);
            else
                memcpy(wp, g.hold.pos, sizeof(wp));
            // (round: the steps flown at full throttle on the afterburner, the nose off the target for those seconds -
            // set ab12: at station-keeping speed the ~130 deg way round the first generator outlasted its kill, 34-39 s
            // vs ab11's 17-27, the side reached in 0-51 % of the frames - run-judge 31 Oct 2026; the runners cross the city
            // at cruise speed. Straight, the last part: station keeping with the guns on)
            if (left > cityhold::kTransitDeg)
            {
                out.station = false;
                out.pursue = false;
                out.steerTo = true;
                memcpy(out.steerPos, wp, sizeof(out.steerPos));
                out.steerFullAngle = 0;
                out.steerThrottle = 1.0f;
                out.steerReverse = false;
                out.afterburn = true;
            }
            else
            {
                out.station = true;
                out.afterburn = false;
                memcpy(out.stationPos, wp, sizeof(out.stationPos));
            }
            static Throttle said;
            if (said.Due(g.time, 2))
                events::Push("autopilot city-hold transit target=%u left=%.0f deg to-point=%.0f centre=%.0f %s", target,
                             left, far, d, left > cityhold::kTransitDeg ? "round" : "straight");
        }
    }
    return true;
}

// Attack passes at a static target (the 2020 M13 St03 generators, catalogue c0011: all four in
// ~2 min - in fast, close along the hull firing at 50-100 m, away low over the shell between towers and through rings so
// the fighters lose the line, back for the next pass; batteries spent, nanobots untouched - the hull stays clean). In:
// the lead pursuit on the afterburner with its closing band at the radius + 80 m (it slows into the window); window:
// within the radius + 200 m, firing, at most 4 s or until the shield fails; out: to an exit 600 m beyond the hull, the
// best of 8 directions across the line to the target - a path clear of solids (real hulls) and the fewest attackers
// with a clear line to it (cover) - on the afterburner until 500 m out, then in again. Returns false when the target is
// no static one (the pursuit as before).
bool StaticPass(u32 target, Out& out)
{
    if (!g_set.staticPass || !target)
        return false;
    const space::Contact tc = FindContact(target);
    fl::Kinematics k = {};
    if (!tc.id || tc.ship || tc.maxHit <= 0 || !fl::PlayerKinematics(k))
        return false;
    // `blind_no_pass` (WORK_QUEUE Q4 slice 1): never at an armed one, as StaticHold - a blind spot dropped mid-fight
    // started the pass's afterburner exit run under the turrets (research\experience\legs\
    // 2026-10-30_0135_M13-dt60b-1-1_i1, the death)
    if (g_set.blindNoPass && g.combat.blindTarget == target && g.combat.blindGuns >= 4)
    {
        static Throttle saidAt;
        if (g.pass.target == target && saidAt.Due(g.time, 5))
            events::Push("autopilot static-pass off target=%u: armed (%d guns, blind_no_pass)", target,
                         g.combat.blindGuns);
        // (a pass left in its coast: the phase back to In, so big_coast's guard switches a killed engine back on)
        if (g.pass.target == target && g.pass.phase == PassPhase::Coast)
            g.pass.phase = PassPhase::In;
        return false;
    }
    float radius = 0;
    for (const space::Obstacle& o : space::Obstacles(tc.pos, 50))
        if (o.id == target)
            radius = o.radius;
    radius = std::max(radius, 30.0f);
    const float rel[3] = {k.pos[0] - tc.pos[0], k.pos[1] - tc.pos[1], k.pos[2] - tc.pos[2]};
    const float d = Len3(rel);
    ai::Self self;
    const bool haveSelf = ai::ReadSelf(self);
    if (g.pass.target != target)
    {
        g.pass.target = target;
        g.pass.phase = PassPhase::In;
        g.pass.since = g.time;
        g.pass.killTapped = false;
        // `big_coast` (from the runners' city generators: "cruise to the first one, then ENGINE
        // KILL a bit outside weapons range and start firing missiles and torpedoes, then all weapons as they come in
        // range. Engine kill supposedly helps delay enemy NPCs for a few seconds. Max damage on the generator, then
        // movement when enemy fire starts coming in"; FREELANCER_RE: no drag while the engine is killed - out of cruise
        // the ship coasts at ~105 m/s): a huge target is opened by a coast in, the nose and every weapon on it
        // (distances to the real hull, the game's ray test: the city generators' radius is 915 m, their hull ~430 m
        // from the centre - the first try measured from the radius and ended the coast at once, "close", 700 m out)
        const float gap0 = HullGap(k.pos, tc.pos);
        if (false && g_set.bigCoast && radius >= 500.0f && (gap0 < 0 ? d - radius : gap0) > 300.0f && haveSelf) // (the
            // old coast from rest: replaced by BigCoast, the coast from cruise)
        {
            g.pass.phase = PassPhase::Coast;
            g.pass.coastHull = self.hull, g.pass.coastShield = self.shield;
        }
        events::Push("autopilot static-pass target=%u radius=%.0f dist=%.0f%s", target, radius, d,
                     g.pass.phase == PassPhase::Coast ? " (coast in, engine killed)" : "");
    }
    const bool shieldFailed = haveSelf && self.shield >= 0 && self.shield <= 0.05f;
    if (g.pass.phase == PassPhase::Coast)
    {
        const bool killed = hudinfo::EngineKilled();
        const float gapRay = HullGap(k.pos, tc.pos);
        const float gap = gapRay >= 0 ? gapRay : d - radius;
        const float speed = Len3(k.vel);
        // ("coasting from a distance to start firing early while leaving attackers behind, and as
        // you reach full weapons range it's ok to try maneuvering sideways while balancing full time on target")
        // (the coast runs on until fire comes in or 300 m off the hull: at weapons range the sideways run is optional)
        const bool hit = haveSelf && ((g.pass.coastHull >= 0 && self.hull < g.pass.coastHull - 0.01f) ||
                                      (g.pass.coastShield >= 0 && self.shield < g.pass.coastShield - 0.3f));
        const char* why = hit                              ? "fire coming in"
                          : gap < 300.0f                   ? "close"
                          : g.time - g.pass.since > 30.0f ? "30 s"
                                                           : nullptr;
        if (why)
        {
            if (killed)
                out.engineKill = true; // (the engine back on)
            g.pass.phase = d < radius + 200 ? PassPhase::Firing : PassPhase::In;
            g.pass.since = g.time;
            g.pass.coastDone = target;
            events::Push("autopilot static-pass coast end (%s) gap=%.0f hull=%.2f shield=%.2f -> %s", why, gap,
                         haveSelf ? self.hull : -1.0f, haveSelf ? self.shield : -1.0f, "hold");
            return false; // (the hold from the next frame)
        }
        else
        {
            // (pressed once the cruise is off; again if it did not take within 1.5 s)
            static float killAt = -10;
            flight::Model fm;
            const bool cruising = flight::ReadModel(fm) && fm.cruiseEngaged;
            // (from rest - a load, a stop - the engine first builds the coast's speed: ~100 m/s, the cruise's own exit)
            const bool fastEnough = speed >= 60.0f;
            if (!killed && !cruising && fastEnough && (!g.pass.killTapped || g.time - killAt > 1.5f))
            {
                out.engineKill = true;
                g.pass.killTapped = true;
                killAt = g.time;
            }
            out.pursue = true;
            out.station = false;
            out.steerTo = false;
            out.afterburn = !killed && !cruising && !fastEnough;
            out.pursueBand = std::max(1.0f, d - gap + 150.0f); // (the band 150 m off the real hull, not the radius)
            return true;
        }
    }
    if (g.pass.phase == PassPhase::In && d < radius + 200)
        g.pass.phase = PassPhase::Firing, g.pass.since = g.time;
    if (g.pass.phase == PassPhase::Firing && (g.time - g.pass.since > 4.0f || shieldFailed || d < radius + 40))
    {
        // out: the exit with a clear path and the most cover
        const float rl = std::max(1.0f, d);
        const float r0[3] = {rel[0] / rl, rel[1] / rl, rel[2] / rl};
        float u[3] = {r0[2], 0.0f, -r0[0]};
        if (Len3(u) < 0.2f)
            u[0] = 0, u[1] = -r0[2], u[2] = r0[1];
        geom::Norm(u);
        float v[3];
        geom::Cross(v, r0, u);
        std::vector<u32> attackers;
        for (const space::Contact& c : Contacts())
            if (c.ship && c.targetsPlayer && c.distance < 2500 && attackers.size() < 8)
                attackers.push_back(c.id);
        float best = 1e30f;
        for (int i = 0; i < 8; ++i)
        {
            const float a = static_cast<float>(i) * 3.14159265f / 4.0f;
            // across the line (u / v) and a little onward past the target: the exit lies beyond its hull
            float dir[3];
            for (int j = 0; j < 3; ++j)
                dir[j] = cosf(a) * u[j] + sinf(a) * v[j] - 0.5f * r0[j];
            const float dl = Len3(dir);
            float q[3];
            for (int j = 0; j < 3; ++j)
                q[j] = tc.pos[j] + dir[j] / dl * (radius + 600.0f);
            float score = 0;
            if (space::RayHitDistance(k.pos, q, nullptr, 0) >= 0)
                score += 10000; // the way there meets a solid (the target's hull included)
            for (u32 id : attackers)
            {
                const space::Contact ac = FindContact(id);
                if (ac.id && !space::SightBlocker(ac.pos, q, id))
                    score += 100; // in that attacker's line
            }
            const float qd[3] = {q[0] - k.pos[0], q[1] - k.pos[1], q[2] - k.pos[2]};
            score += Len3(qd) / 100.0f;
            if (score < best)
                best = score, memcpy(g.pass.exit, q, sizeof(q));
        }
        g.pass.phase = PassPhase::Out;
        g.pass.since = g.time;
        events::Push("autopilot static-pass out score=%.0f attackers=%d", best, static_cast<int>(attackers.size()));
    }
    if (g.pass.phase == PassPhase::Out && (d > radius + 500 || g.time - g.pass.since > 8.0f))
        g.pass.phase = PassPhase::In, g.pass.since = g.time;
    if (g.pass.phase == PassPhase::Out)
    {
        out.pursue = false;
        out.station = false;
        out.steerTo = true;
        memcpy(out.steerPos, g.pass.exit, sizeof(out.steerPos));
        out.steerFullAngle = 0;
        out.steerThrottle = 1.0f;
        out.steerReverse = false;
        out.afterburn = true;
        return true;
    }
    out.pursue = true;
    out.station = false;
    out.steerTo = false;
    out.pursueBand = radius + 80.0f;
    return true;
}

// `strafe_pass`: the pass just flown (its samples), logged at its turn, a fallback or the strafe's end; the samples reset.
void StrafePassLog(const char* end)
{
    State::Strafe& s = g.strafe;
    if (s.frames > 0)
        events::Push("autopilot strafe pass n=%d gap=%.0f speed=%.0f launches=%d min-gap=%.0f speed-min=%.0f "
                     "speed-max=%.0f aim=%.0f%% dur=%.1f side=%d dir=%d%s",
                     s.passes, s.gapN ? s.gapSum / s.gapN : -1.0f, s.speedSum / s.frames, s.launches,
                     s.gapN ? s.gapMin : -1.0f, s.speedMin, s.speedMax, 100.0f * s.aimFrames / s.frames,
                     g.time - s.passAt, s.side, s.dir, end);
    s.frames = s.aimFrames = s.gapN = s.launches = 0;
    s.gapSum = s.speedSum = s.speedMax = 0;
    s.gapMin = s.speedMin = 1e9f;
}

// `strafe_pass` (docs\research\runners\M13_bsdown_datapass.md "Proposal", escalation step 2; the pure half strafe.h,
// tests\strafe_test.cpp; Source::Hidden + Meta: the turret count is game data a player never sees, the passes the
// runners' way): at a big armed hull - 4+ guns by the game's own turret test (space::GunsCovering), not dockable, still,
// long and elongated by the game's ray test (strafe::Measure: the long local axis, its ends, the width per side) - no
// hold: lengthwise passes along that axis, the line strafe::kGapWant off the hull, the gap read live by a ray from us to
// our station's axis point (the smaller of that and the measured profile), the slide keeping the 150-250 m band,
// afterburner pulses (strafe::Burn: ~200 <-> ~80 m/s every 1.6 s, never under 20), the nose on the hull ahead (a point
// the game's ray confirms: the guns aim at it and the launchers' alignment is read by it in UpdateCombat), a turn ~1 km
// out at each end onto the far side's line (the same side or the two across when a line's rays meet a solid). Every
// launch rule stays in force; the geometry guard (autopilot_frame.cpp) treats the hull as a wall as for every steer run.
// Fails closed - unreadable hull, under 4 guns, dockable, moving, every line blocked, a live gap under 80 m, a join / turn
// / pass that lasts too long: `strafe fallback why=` and today's fight for that target. True = flown this frame.
bool StrafeFly(u32 target, float gunRange, Out& out)
{
    State::Strafe& s = g.strafe;
    if (!g_set.strafePass || !Knows(Source::Hidden) || !Knows(Source::Meta) || !target || s.failTarget == target)
        return false;
    const space::Contact tc = FindContact(target);
    float c[3], rot[9];
    fl::Kinematics k = {};
    if (!tc.id || !space::ObjectPose(target, c, rot) || !fl::PlayerKinematics(k))
        return false;
    const auto fallback = [&](const char* why) -> bool
    {
        const bool flown = s.target == target;
        if (flown)
            StrafePassLog(" (fallback)");
        events::Push("autopilot strafe fallback why=%s target=%u phase=%s passes=%d", why, target,
                     flown ? strafe::PhaseName(s.phase) : "start", flown ? s.passes : 0);
        s = State::Strafe();
        s.failTarget = target;
        return false;
    };
    // the target's frame: world <-> local (rot row-major, columns = its local axes)
    const auto toLocal = [&](const float w[3], float l[3], bool point)
    {
        const float r[3] = {w[0] - (point ? c[0] : 0.0f), w[1] - (point ? c[1] : 0.0f), w[2] - (point ? c[2] : 0.0f)};
        for (int i = 0; i < 3; ++i)
            l[i] = rot[0 * 3 + i] * r[0] + rot[1 * 3 + i] * r[1] + rot[2 * 3 + i] * r[2];
    };
    const auto toWorld = [&](const float l[3], float w[3], bool point)
    {
        for (int i = 0; i < 3; ++i)
            w[i] = (point ? c[i] : 0.0f) + rot[i * 3 + 0] * l[0] + rot[i * 3 + 1] * l[1] + rot[i * 3 + 2] * l[2];
    };
    // a side's pass line between the turn stations, 6 legs through the game's ray test (real hulls, the target's too)
    const auto lineClear = [&](int side, int dir) -> bool
    {
        const float z0 = strafe::TurnStation(s.hull, -dir), z1 = strafe::TurnStation(s.hull, dir);
        float prev[3] = {};
        for (int i = 0; i <= 6; ++i)
        {
            float l[3], w[3];
            strafe::LinePoint(s.hull, side, z0 + (z1 - z0) * static_cast<float>(i) / 6.0f, strafe::kGapWant, l);
            toWorld(l, w, true);
            if (i > 0 && space::RayHitDistance(prev, w) >= 0)
                return false;
            memcpy(prev, w, sizeof(w));
        }
        return true;
    };
    space::ShipMotion tm;
    if (tc.ship && space::ShipMotionOf(target, tm) && tm.speed >= 30.0f)
        return fallback("moving");
    float pl[3], vl[3];
    toLocal(k.pos, pl, true);
    toLocal(k.vel, vl, false);
    if (s.target != target)
    {
        if (s.target)
            StrafePassLog(" (new target)");
        const u32 fail = s.failTarget;
        s = State::Strafe();
        s.failTarget = fail;
        if (tc.dock)
            return fallback("dockable");
        const float pt[1][3] = {{c[0], c[1], c[2]}};
        int n = 0;
        const int guns = space::GunsCovering(target, pt, 1, &n);
        if (guns < 4)
            return fallback(guns < 0 ? "guns unknown" : "under 4 guns");
        float radius = 0;
        for (const space::Obstacle& o : space::Obstacles(c, 50))
            if (o.id == target)
                radius = o.radius;
        s.hull = strafe::Measure(c, rot, radius > 0 ? radius : 300.0f,
                                 [](const float* a, const float* b) { return space::RayHitDistance(a, b); });
        if (!s.hull.ok)
        {
            char why[96];
            sprintf_s(why, "axis (%s, len %.0f, aspect %.1f)", s.hull.why, s.hull.len, s.hull.aspect);
            return fallback(why);
        }
        int order[4];
        strafe::SideOrder(s.hull, pl, order);
        s.dir = strafe::StartDir(s.hull, pl);
        s.side = strafe::PickSide(order, s.dir, lineClear);
        if (s.side < 0)
            return fallback("geometry (every line blocked)");
        s.target = target;
        s.phase = strafe::Phase::Join;
        s.startAt = s.since = s.passAt = s.lastAt = g.time;
        if (g.hold.on && g.hold.target == target)
            g.hold.on = false; // (no static hold for this target while the passes fly)
        events::Push("autopilot strafe start target=%u axis=%d len=%.0f gap=%.0f side=%d dir=%d lo=%.0f hi=%.0f "
                     "width=%.0f/%.0f/%.0f/%.0f aspect=%.1f turn=%.0f/%.0f guns=%d radius=%.0f",
                     target, s.hull.axis, s.hull.len, strafe::kGapWant, s.side, s.dir, s.hull.lo, s.hull.hi,
                     s.hull.widthMax[0], s.hull.widthMax[1], s.hull.widthMax[2], s.hull.widthMax[3], s.hull.aspect,
                     strafe::TurnStation(s.hull, -1), strafe::TurnStation(s.hull, 1), guns, radius);
    }
    // (frames not flown - an evade, the approach - do not count against the phase's time limit)
    if (g.time - s.lastAt > 0.5f)
        s.since += g.time - s.lastAt;
    s.lastAt = g.time;
    // the live gap: the ray from us to our station's point on the axis
    float footL[3] = {0, 0, 0}, foot[3];
    footL[s.hull.axis] = pl[s.hull.axis];
    toWorld(footL, foot, true);
    const float gapNow = space::RayHitDistance(k.pos, foot);
    // the aim's reach: the guns', or a launcher's seeker with rounds left
    float reach = gunRange;
    for (const space::Weapon& w : space::PlayerWeapons())
        if (w.homing && w.ammo > 0)
            reach = std::max(reach, w.seekerRange);
    strafe::StepIn in;
    in.phase = s.phase, in.side = s.side, in.dir = s.dir;
    memcpy(in.pos, pl, sizeof(pl));
    memcpy(in.vel, vl, sizeof(vl));
    in.gapNow = gapNow;
    in.phaseTime = g.time - s.since;
    in.reach = reach;
    in.aimed = s.aimed;
    const strafe::StepOut o = strafe::Step(s.hull, in, lineClear);
    if (o.fallback)
        return fallback(o.fallback);
    const float speed = Len3(k.vel);
    if (o.turned)
    {
        StrafePassLog("");
        events::Push("autopilot strafe turn at=%.0f side=%d dir=%d gap=%.0f speed=%.0f", o.turnAt, o.side, o.dir, o.gap,
                     speed);
    }
    if (o.passStarted)
    {
        StrafePassLog(" (before the pass)"); // (the join's / turn's samples are none: only a pass samples)
        ++s.passes;
        s.passAt = g.time;
    }
    if (o.phase != s.phase)
        s.since = g.time;
    s.phase = o.phase, s.side = o.side, s.dir = o.dir, s.aimed = o.aim;
    // the steer run: the nose to the step's point, full throttle, the afterburner by the pulses (on all the way while the
    // join is still far)
    float steerW[3];
    toWorld(o.steer, steerW, true);
    out.pursue = false;
    out.station = false;
    out.steerTo = true;
    memcpy(out.steerPos, steerW, sizeof(out.steerPos));
    out.steerFullAngle = s.phase == strafe::Phase::Turn ? 0.0f : 0.35f;
    out.steerThrottle = 1.0f;
    out.steerReverse = false;
    out.afterburn = strafe::Burn(g.time - s.startAt, speed) ||
                    (s.phase == strafe::Phase::Join && geom::Dist(steerW, k.pos) > 600.0f);
    // the gap band by the player's slide keys (a missile dodge's slide wins; none wanted: the evasion's bursts stay)
    if (o.lateral && !out.dodgeSlide)
    {
        float dl[3], dw[3];
        strafe::SideDir(s.hull, s.side, dl);
        for (float& x : dl)
            x *= static_cast<float>(o.lateral);
        toWorld(dl, dw, false);
        const int sl = strafe::SlideCode(k.rot, dw);
        if (sl)
            out.slide = sl;
    }
    // the hull point the nose is on, confirmed by the game's ray (within 150 m of where the profile put it)
    s.aimOn = s.aimInGuns = false;
    if (o.aim)
    {
        const float to[3] = {steerW[0] - k.pos[0], steerW[1] - k.pos[1], steerW[2] - k.pos[2]};
        const float dist = Len3(to);
        const float far[3] = {k.pos[0] + to[0] * 1.3f, k.pos[1] + to[1] * 1.3f, k.pos[2] + to[2] * 1.3f};
        const float hit = dist > 1.0f ? space::RayHitDistance(k.pos, far) : -1.0f;
        if (hit >= 0 && fabsf(hit - dist) < 150.0f)
        {
            for (int i = 0; i < 3; ++i)
                s.aim[i] = k.pos[i] + to[i] / dist * hit;
            s.aimOn = true;
            s.aimInGuns = gunRange > 0 && hit <= gunRange;
        }
    }
    if (s.phase == strafe::Phase::Pass)
    {
        ++s.frames;
        s.speedSum += speed, s.speedMin = std::min(s.speedMin, speed), s.speedMax = std::max(s.speedMax, speed);
        if (s.aimOn)
            ++s.aimFrames;
        if (gapNow >= 0)
            ++s.gapN, s.gapSum += gapNow, s.gapMin = std::min(s.gapMin, gapNow);
    }
    static Throttle said;
    if (said.Due(g.time, 1.0f))
        events::Push("autopilot strafe at phase=%s z=%.0f gap=%.0f live=%.0f speed=%.0f aim=%d guns=%d burn=%d slide=%d",
                     strafe::PhaseName(s.phase), pl[s.hull.axis], o.gap, gapNow, speed, s.aimOn ? 1 : 0,
                     s.aimInGuns ? 1 : 0, out.afterburn ? 1 : 0, o.lateral);
    g.combat.rushing = false;
    s.on = true;
    return true;
}

// A static target (a generator, a platform too weak for a blind spot) is fought on the move, never from a hover (user, 22
// Oct 2026: "M12 had the player ship HOLD STILL as it was shooting target generators. That's just asking for the player
// to be destroyed"; the pursuit's band settles at rest in front of anything that never moves). The baseline is the 2020
// playthrough (M12 3:20-4:01: in on the afterburner, ~30-80 m/s through the firing window close to the generator,
// breaking away and back, never at rest, hull 93-100 %): a station point that runs round the target at its radius
// + 150 m in the plane through us, ~45 m/s, the direction flipping every 3-6 s, bobbing +-60 m - the nose stays on the
// target (station keeping), so the guns work throughout. A leg of the orbit that meets a solid (real hulls, SightBlocker)
// turns the other way. True with the station point; false = not a static target in reach (the pursuit as before).
bool StaticOrbit(u32 target, float dt, float point[3])
{
    if (!g_set.staticOrbit || !target)
        return false;
    const space::Contact tc = FindContact(target);
    fl::Kinematics k = {};
    if (!tc.id || tc.ship || tc.maxHit <= 0 || !fl::PlayerKinematics(k))
        return false;
    float radius = 0;
    for (const space::Obstacle& o : space::Obstacles(tc.pos, 50))
        if (o.id == target)
            radius = o.radius;
    const float R = std::max(radius, 30.0f) + 150.0f;
    const float rel[3] = {k.pos[0] - tc.pos[0], k.pos[1] - tc.pos[1], k.pos[2] - tc.pos[2]};
    const float d = Len3(rel);
    if (d > R + 600 || d < 1)
    {
        g.combat.orbitTarget = 0; // still on the way in: the run-in is the pursuit's (afterburner, as the player's)
        return false;
    }
    if (g.combat.orbitTarget != target)
    {
        // the plane through us: radial r0 now, tangent t0 = r0 x up (or x z when straight above)
        g.combat.orbitTarget = target;
        for (int i = 0; i < 3; ++i)
            g.combat.orbitR0[i] = rel[i] / d;
        float t0[3] = {g.combat.orbitR0[2], 0.0f, -g.combat.orbitR0[0]}; // r0 x (0,1,0)
        if (Len3(t0) < 0.2f)
            t0[0] = 0, t0[1] = -g.combat.orbitR0[2], t0[2] = g.combat.orbitR0[1]; // r0 x (1,0,0)
        const float tl = Len3(t0);
        for (int i = 0; i < 3; ++i)
            g.combat.orbitT0[i] = t0[i] / tl;
        g.combat.orbitAngle = 0;
        g.combat.orbitDir = (g.frames & 1) ? 1.0f : -1.0f;
        g.combat.orbitFlipAt = g.time + 3.0f + static_cast<float>(g.frames % 30) / 10.0f;
        events::Push("autopilot static-orbit target=%u radius=%.0f orbit=%.0f dist=%.0f", target, radius, R, d);
    }
    auto at = [&](float a, float out3[3])
    {
        const float bob = 60.0f * sinf(g.time * 0.7f);
        const float up[3] = {g.combat.orbitR0[1] * g.combat.orbitT0[2] - g.combat.orbitR0[2] * g.combat.orbitT0[1],
                             g.combat.orbitR0[2] * g.combat.orbitT0[0] - g.combat.orbitR0[0] * g.combat.orbitT0[2],
                             g.combat.orbitR0[0] * g.combat.orbitT0[1] - g.combat.orbitR0[1] * g.combat.orbitT0[0]};
        for (int i = 0; i < 3; ++i)
            out3[i] = tc.pos[i] + R * (cosf(a) * g.combat.orbitR0[i] + sinf(a) * g.combat.orbitT0[i]) + bob * up[i];
    };
    if (g.time >= g.combat.orbitFlipAt)
    {
        g.combat.orbitDir = -g.combat.orbitDir;
        g.combat.orbitFlipAt = g.time + 3.0f + static_cast<float>(g.frames % 30) / 10.0f;
    }
    // 45 m/s along the circle, the point kept ~1.5 s ahead of where the ship can follow
    g.combat.orbitAngle += g.combat.orbitDir * 45.0f / R * dt;
    float next[3];
    at(g.combat.orbitAngle + g.combat.orbitDir * 70.0f / R, next);
    if (space::SightBlocker(k.pos, next, target))
    {
        g.combat.orbitDir = -g.combat.orbitDir;
        g.combat.orbitFlipAt = g.time + 3.0f;
        at(g.combat.orbitAngle + g.combat.orbitDir * 70.0f / R, next);
    }
    memcpy(point, next, sizeof(next));
    return true;
}
} // namespace autopilot
