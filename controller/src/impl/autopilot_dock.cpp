// Advanced Autopilot - fast dock: the queue shortcut, the player-style approach, dock_regain. Part of autopilot.cpp until 21 Oct
// 2026; the geometry planners (LineClear, ArcAround, PassageVia, GeoPathAim, ApplyAvoid) moved to autopilot_geo.cpp on 24 Oct
// 2026. Shared state and declarations in autopilot_impl.h, the module API in autopilot.h.
#include "autopilot_impl.h"

namespace autopilot
{
// ------------------------------------------------------------------ fast dock

// The ship going in at the entrance g.dock.bayP2 (a bay's p2 / a lane start), 0 none: a non-hostile ship near it that is
// slow (not riding a lane through the ring at ~2500 m/s) and closing on it or already in it (20 Oct 2026, Li01->Li02
// gate: the "busy" ships were arrivals spawned at the gate, flying out - waiting for them only cost time, and formation
// on them leads nowhere; a police patrol idling 77 m from the Manhattan lane start: the formation on it did nothing).
// Contacts carry no velocity: it comes from their positions between frames (unknown on a ship's first frame). Only
// near the entrance: `dist` = ours to it (from 27 km the shortcut's 30 s ran out 4.4 km short of the gate).
u32 EnteringShip(float dist)
{
    struct Seen
    {
        float pos[3];
        float time;
        bool goingIn;
        bool lockedIn;
        float eta; // to the entrance at its closing speed (s)
    };
    static std::map<u32, Seen> seen;
    g.dock.lockedIn = 0;
    g.dock.aheadEta = 1e9f;
    if (dist >= 3000)
        return 0;
    u32 in = 0;
    float bestR = 1e30f;
    const u32 me = space::PlayerShipId();
    std::map<u32, Seen> now;
    for (const space::Contact& c : Contacts())
    {
        if (!c.ship || c.id == me || c.health == 0 || c.attitude <= -0.6f)
            continue;
        const float d[3] = {c.pos[0] - g.dock.bayP2[0], c.pos[1] - g.dock.bayP2[1], c.pos[2] - g.dock.bayP2[2]};
        const float r = Len3(d);
        if (r >= 2000) // 600 m for "busy", 2 km for who reaches the entrance first
            continue;
        const auto it = seen.find(c.id);
        if (it != seen.end() && g.time - it->second.time < 0.05f)
        {
            // too short a step for a velocity (frames of ~6 ms): keep the sample and its verdict (the flag flickered
            // every frame at the Manhattan ring - a wait event per frame)
            now[c.id] = it->second;
        }
        else
        {
            Seen s{{c.pos[0], c.pos[1], c.pos[2]}, g.time, false, false, 1e9f};
            if (it != seen.end())
            {
                const float dt = g.time - it->second.time;
                const float v[3] = {(c.pos[0] - it->second.pos[0]) / dt, (c.pos[1] - it->second.pos[1]) / dt,
                                    (c.pos[2] - it->second.pos[2]) / dt};
                const float closing = r > 1 ? -(v[0] * d[0] + v[1] * d[1] + v[2] * d[2]) / r : 0.0f; // > 0: in
                s.goingIn = Len3(v) < 600 && (closing > 5 || r < 60);
                s.eta = !s.goingIn ? 1e9f : closing > 1 ? r / closing : 0.0f;
                s.goingIn = s.goingIn && r < 600;
                // Locked in = in the docking animation (the user's moment: "as the NPC is locked in the docking
                // animation (their docking lights flash red)", 20 Oct 2026): on the bay's approach line - within 40 m
                // of it, between p2 and the mount - and moving in along it, or already past p2. The client's copies
                // of NPCs hold no dock port (probe 20 Oct 2026: none of 18 ships, one went in), so the animation's
                // own path tells it.
                // On any dock point's path, not just ours (20 Oct 2026: 2 rushes found no lock with a ship 1.6 s from
                // the gate - bigger ships dock at the large berths, which our bay choice skips).
                auto onPath = [&](const float p2[3], const float mount[3])
                {
                    const float a[3] = {mount[0] - p2[0], mount[1] - p2[1], mount[2] - p2[2]};
                    const float len = Len3(a);
                    if (len < 1)
                        return false;
                    const float u[3] = {a[0] / len, a[1] / len, a[2] / len}; // inward
                    const float q[3] = {c.pos[0] - p2[0], c.pos[1] - p2[1], c.pos[2] - p2[2]};
                    const float along = q[0] * u[0] + q[1] * u[1] + q[2] * u[2];
                    const float off[3] = {q[0] - u[0] * along, q[1] - u[1] * along, q[2] - u[2] * along};
                    const float inward = v[0] * u[0] + v[1] * u[1] + v[2] * u[2];
                    // moving in, or past p2 and not moving out (20 Oct 2026: arrivals spawn in the gate's mouth and fly
                    // out along the same path - "past p2" alone took 3 of them for dockers: formation, no effect)
                    return Len3(off) < 40 && along > -20 && along < len + 40 &&
                           (inward > 3 || (along > 15 && inward > -3));
                };
                if (Len3(v) < 600)
                {
                    if (g.dock.allBays.empty())
                    {
                        const float mount[3] = {g.dock.bayP2[0] - g.dock.bayAxis[0] * g.dock.bayLen,
                                                g.dock.bayP2[1] - g.dock.bayAxis[1] * g.dock.bayLen,
                                                g.dock.bayP2[2] - g.dock.bayAxis[2] * g.dock.bayLen};
                        s.lockedIn = onPath(g.dock.bayP2, mount);
                    }
                    for (const space::DockBay& b : g.dock.allBays)
                        s.lockedIn = s.lockedIn || onPath(b.p2, b.mount);
                }
                // lanes: the game's own flag - entering the lane (is_using_tradelane) while still slow at the start (a
                // rider passing the ring is at ~2500 m/s; 20 Oct 2026: no lane NPC ever matched the path test)
                if (g.dock.fastKind == DockKind::Lane && c.lane && r < 300 && Len3(v) < 600)
                    s.lockedIn = true;
            }
            now[c.id] = s;
        }
        if (now[c.id].lockedIn)
            g.dock.lockedIn = c.id;
        // when it reaches the entrance (0: in the animation already) - "will it hit the dock queue first?"
        if (now[c.id].lockedIn)
            g.dock.aheadEta = 0;
        else
            g.dock.aheadEta = std::min(g.dock.aheadEta, now[c.id].eta);
        // the one deepest in: the ship actually docking (20 Oct 2026: a convoy was going in - transport + escorts - and
        // the try took whichever was listed last; the formation then charged the cruise engine at the gate's mouth)
        if (now[c.id].goingIn && r < bestR)
            in = c.id, bestR = r;
    }
    seen = std::move(now);
    return in;
}

// Where the user's queue shortcut works: jump gates and trade lanes (user, 20 Oct 2026: "valid for jump gates and trade
// lanes for sure, jump holes maybe too. Definitely not docking or landing" - 3 tries at the Manhattan docking ring had
// no effect). Holes: to verify.
// Lanes are left out on the data: every lane request was granted at once (40+ replies kind 2 response 4, never queued,
// 20 Oct 2026 - the user does not remember lane queues either), so a lane rush only cost time (+4 s each, no lock).
static bool ShortcutPlace()
{
    return g.dock.fastKind != DockKind::Lane &&
           (FindContact(g.target).type & (space::kJumpGate | space::kJumpHole)) != 0;
}

// The user's queue shortcut (20 Oct 2026: "position at the entrance while a non-hostile NPC is in the docking
// animation, then Enter Formation - the jump animation starts at once"): the game's own formation maneuver (the pad's
// Enter Formation: select it, ai::Maneuver OpFormation) on the ship going in (never hostile).
static void StartFormationShortcut(u32 ship, float dist, const char* why)
{
    g.dock.formationTried = true;
    g.dock.formationAt = g.time;
    g.dock.formationTakenIn = false;
    space::SetTarget(ship);
    MarkSubmit();
    ai::Maneuver(ai::OpFormation, false);
    events::Push("autopilot fast-dock formation-shortcut busy-by=%u dist=%.0f%s", ship, dist, why);
}

// The running try: 0 none, 1 hands off (the game flies us), 2 it missed / ended just now (the dock's target is selected
// again; the caller requests the dock, which replaces the formation maneuver). Taken in = the game's dock port, a jump
// or a lane ride (20 Oct 2026, Li01->Li02 gate: 4 s after the formation the ship was in the gate's jump sequence, "not
// the only ones headed to the California system"). Never ai::MissionHolds here: the formation is itself a priority
// directive (a lane ring: "taken in" at once, hands off for good, the police leader flew us into its fight with Rogues
// and we died undefended). Hands off at most 20 s in all; a jump / lane ride that has begun is the travel's business.
static int FormationShortcutState()
{
    if (!g.dock.formationTried || g.dock.formationAt <= 0)
        return 0;
    if (g.time - g.dock.formationAt < 20)
    {
        space::Pilot p;
        const bool takenIn = ai::DockInProgress() || (space::ReadPilot(p) && (p.jumping || p.lane));
        if (takenIn && !g.dock.formationTakenIn)
        {
            events::Push("autopilot fast-dock formation-shortcut taken in after %.1f s", g.time - g.dock.formationAt);
            g.dock.formationTakenIn = true;
        }
        if (takenIn || g.time - g.dock.formationAt < 6)
            return 1;
    }
    events::Push("autopilot fast-dock formation-shortcut %s after %.0f s: requesting, the game queues",
                 g.dock.formationTakenIn ? "ended" : "no effect", g.time - g.dock.formationAt);
    g.dock.formationAt = -1;
    g.dock.formationEndAt = g.time;
    space::SetTarget(g.target);
    return 2;
}

// After the request: the server's answer (host API v14). "Queued" (kind 0 response 3: the game's "dock in use -
// entering queue") at a gate / lane is the moment for the shortcut (user, 20 Oct 2026: "'Dock in use - entering queue'
// would have been a perfect opportunity"; a 44 s gate queue went unseen by the busy test alone).
void QueuedShortcut()
{
    // the dock point the game assigned (vs the bay our approach lined up on: the user saw a swerve at Leeds' ring)
    {
        u32 portObj = 0;
        int portIdx = -1;
        if (!g.dock.portLogged && ai::DockPort(portObj, portIdx))
        {
            g.dock.portLogged = true;
            events::Push("autopilot fast-dock port assigned=%d ours=%d", portIdx, g.dock.fastBay);
        }
    }
    u32 ship = 0, kind = 0, response = 0;
    const u32 seq = HostDockReply(&ship, &kind, &response);
    if (seq != g.dock.replySeen)
    {
        g.dock.replySeen = seq;
        if (ship == space::PlayerShipId())
        {
            events::Push("autopilot fast-dock reply kind=%u response=%u", kind, response);
            // dock (0) and lane (2) answers only: kind 1 answers the formation request itself (response 0)
            if (kind == 0 || kind == 2)
                g.dock.queuedSince = response == 3 ? g.time : -1.0f;
            if (kind == 0 && response == 4)
                g.dock.grantAt = g.time, g.dock.grantCrawlFor = 0; // (grantHanded resets with a new fast request only)
            if (kind == 0 && response == 3 && !ShortcutPlace())
                events::Push("autopilot fast-dock queued at a base: the game's dock waits for its turn");
        }
    }
    // Queued at a gate / lane while travelling: the game would hold us in its queue (tries from there, 379-635 m out,
    // all missed - 20 Oct 2026); instead back to the entrance and the user's way (FastApproach's shortcut park), once
    // per approach: the dock maneuver ends (free flight), the fast approach takes over again, and its request queues us
    // anew if no ship locks in. (Travel only: the dock mode would read the free flight as its maneuver ending.)
    // Queued while still at the entrance (the rush's time ran out there): the queue exists because a ship is about to
    // dock - its lock is the moment, so watch for it (dead time anyway); formation on it, a miss requests again.
    const int fs = FormationShortcutState();
    if (fs == 1)
        return;
    if (fs == 2)
    {
        MarkSubmit();
        ai::Maneuver(ai::OpDock, false);
        events::Push("autopilot fast-dock request again (after the shortcut)");
        return;
    }
    if (g.dock.queuedSince >= 0 && g.dock.queueTries < 2 && g.mode == Mode::Travel && ShortcutPlace())
    {
        fl::Kinematics k = {};
        if (fl::PlayerKinematics(k))
        {
            const float d[3] = {g.dock.bayP2[0] - k.pos[0], g.dock.bayP2[1] - k.pos[1], g.dock.bayP2[2] - k.pos[2]};
            const float dist = Len3(d);
            if (dist < 150)
            {
                EnteringShip(dist);
                if (g.dock.lockedIn)
                {
                    ++g.dock.queueTries;
                    StartFormationShortcut(g.dock.lockedIn, dist, " (queued, locked in)");
                }
                return;
            }
        }
    }
    // One shortcut try per approach, and only with a ship going in in sight (20 Oct 2026: after a missed try the
    // queued take-over waited 5 s at the entrance for nobody, then queued again).
    if (g.dock.queuedSince < 0 || g.dock.parkForQueue || g.dock.formationTried || g.mode != Mode::Travel ||
        !ShortcutPlace())
        return;
    {
        fl::Kinematics k = {};
        if (!fl::PlayerKinematics(k))
            return;
        const float d[3] = {g.dock.bayP2[0] - k.pos[0], g.dock.bayP2[1] - k.pos[1], g.dock.bayP2[2] - k.pos[2]};
        EnteringShip(Len3(d));
        if (g.dock.aheadEta > 1e8f)
        {
            g.dock.queuedSince = -1; // nobody visible ahead: stay in the game's queue
            return;
        }
    }
    g.dock.parkForQueue = true;
    g.dock.rushing = false;
    g.dock.shortcutSince = g.dock.parkedSince = -1;
    g.dock.fastRequested = false;
    g.dock.queuedSince = -1;
    ++g.dock.queueTries;
    MarkSubmit();
    ai::FreeFlight();
    events::Push("autopilot fast-dock queued: to the entrance for the shortcut");
}

// The player-style approach of a fast dock (g_set.fastDock): to the outer approach point p2 of the bay the game will
// assign, at full speed, braking late, and the dock request just before p2 - the docking cutscene starts at p2
// (docs\FREELANCER_RE.md "Docking geometry"). True while it flies the ship (also on the frame the request goes out);
// false when it does not apply (no bays: lane rings, ships) - the game's own dock then.
// dock_regain (session 11; M06's Hood: granted at 8 m / 182 m/s, then the game's dock crawled at 11 m/s while the moving
// Hood pulled away 217 -> 635 m for 63 s; M04's Benford the same, 51 s - fail states): after a request the distance to
// the dock target growing for 4 s -> the request dropped, the fast approach lines up again. Called from FastApproach and
// from the travel's dock step (which stops calling the approach once the request is out). True when it dropped one.
bool DockRegain(float dt, u32 target)
{
    // (a fast approach's request only - a bay chosen: the travel's game-dock fallback for a target without bays sets
    // fastRequested too, and a drop there would loop game-dock -> drop forever; review, session 11)
    if (!(g_set.dockRegain && g.dock.fastRequested && g.dock.fastBay >= 0 && target &&
          g.time - g.dock.fastRequestAt > 3.0f))
    {
        g.dock.losingFor = 0, g.dock.gainDist = -1;
        return false;
    }
    // (sampled each second: at the crawl's 11 m/s a frame moves ~0.1 m)
    static float sampleAt = -10;
    if (g.time - sampleAt < 1.0f && g.dock.gainDist >= 0)
        return false;
    sampleAt = g.time;
    (void)dt;
    const space::Contact tc = FindContact(target);
    if (!tc.id)
        return false;
    // (only a crawl loses ground - the stalls drifted at ~11 m/s; the game's own dock flying out to a gate's entry point
    // after a queue also opens the distance, at speed: s12c M04 Magellan gate, a grant dropped 5 s after it came, +12 s)
    fl::Kinematics kc = {};
    const bool crawl = fl::PlayerKinematics(kc) && Len3(kc.vel) < 40.0f;
    // A grant the game does not follow (s12c M04 Benford: granted at 24 m / 285 m/s, the ship bumped the station and
    // drifted; re-lined, granted again at 203 m at rest, nothing again; the travel's 20 s "stalled: the game's dock"
    // fallback docked it 10 s later - 59.6 s, over the checkpoint limit): crawling 4 s after a grant -> the game's own
    // dock maneuver at once, once per grant
    if (g.dock.grantAt >= 0 && g.dock.grantAt >= g.dock.fastRequestAt && !g.dock.grantHanded)
    {
        g.dock.grantCrawlFor = crawl ? g.dock.grantCrawlFor + 1.0f : 0.0f;
        if (g.dock.grantCrawlFor >= 4.0f)
        {
            events::Push("autopilot fast-dock grant not followed (%.0f m, crawling %.0f s): the game's dock",
                         tc.distance, g.dock.grantCrawlFor);
            g.dock.grantHanded = true;
            space::SetTarget(target);
            MarkSubmit();
            ai::Maneuver(ai::OpDock, false);
            g.dock.fastRequestAt = g.time;
            g.dock.losingFor = 0;
            g.dock.gainDist = -1;
            return true;
        }
    }
    if (!crawl)
        g.dock.losingFor = 0;
    else if (g.dock.gainDist >= 0 && tc.distance > g.dock.gainDist + 2.0f)
        g.dock.losingFor += 1.0f;
    else if (g.dock.gainDist >= 0 && tc.distance < g.dock.gainDist - 2.0f)
        g.dock.losingFor = 0;
    g.dock.gainDist = tc.distance;
    if (g.dock.losingFor <= 4.0f)
        return false;
    events::Push("autopilot fast-dock losing ground (%.0f m, %.0f s): lining up again", tc.distance, g.dock.losingFor);
    g.dock.fastRequested = false;
    g.dock.fastLined = false;
    g.dock.losingFor = 0;
    g.dock.gainDist = -1;
    FreeFlightNow();
    return true;
}

bool FastApproach(float dt, Out& out)
{
    // Never at rest under fire while a dock waits (session 7, 23 Oct 2026, M12: 440 m short of the Omicron Minor hole the
    // game's dock parked the ship at 0 m/s among 10 Nomads - dead in 1.5 s at full hull; the hole opens to the player only
    // with Juni near it, and we outran her. The user's try 3 circled at ~80 m/s for ~10 s there): the game's dock holding
    // us under 15 m/s for 1.5 s with attackers about -> 4 s on the afterburner round the entrance, then the request again.
    fl::Kinematics kw = {};
    if (g.dock.fastRequested && fl::PlayerKinematics(kw) && g.time - g.dock.fastRequestAt > 2.0f)
    {
        const float sp = Len3(kw.vel);
        g.dock.restFor = sp < 15 ? g.dock.restFor + dt : 0.0f;
        if (g.dock.restFor > 1.5f && g_sit.attackers >= 2)
        {
            events::Push(
                "autopilot fast-dock held at rest under fire (attackers=%d): circling the entrance 4 s, then again",
                g_sit.attackers);
            g.dock.fastRequested = false;
            g.dock.fastLined = false;
            g.dock.circleUntil = g.time + 4.0f;
            g.dock.restFor = 0;
            FreeFlightNow();
        }
    }
    DockRegain(dt, g.target);
    if (g.dock.circleUntil > g.time && fl::PlayerKinematics(kw))
    {
        // a circle of 350 m round the entrance (p2), level, on the afterburner: the point 60 deg ahead on it
        const float rel[3] = {kw.pos[0] - g.dock.bayP2[0], 0.0f, kw.pos[2] - g.dock.bayP2[2]};
        const float rl = std::max(1.0f, Len3(rel));
        const float a = atan2f(rel[2], rel[0]) + 1.05f;
        const float circ[3] = {g.dock.bayP2[0] + cosf(a) * 350.0f, g.dock.bayP2[1], g.dock.bayP2[2] + sinf(a) * 350.0f};
        (void)rl;
        SteerTo(out, circ, 1.0f, 0.35f, 0);
        out.steerReverse = false;
        out.afterburn = true;
        return true;
    }
    if (g.dock.fastRequested)
        return false; // the game's dock from here (QueuedShortcut runs beside it: UpdateTravel, the dock mode)
    if (g.target && fl::TargetId() != g.target)
        space::SetTarget(g.target);
    fl::Kinematics k = {};
    if (!fl::PlayerKinematics(k))
        return true;
    if (g.dock.fastBay < 0)
    {
        if (g.dock.fastTried)
            return false;
        if (fl::TargetId() != g.target)
            return true; // the selection shows in the game's state next frame
        g.dock.fastTried = true;
        // The bay the game assigns: so far seen = the player-size bay whose p2 is nearest (Fort Bush from the east:
        // bay 2, the east-facing one). Types 4 / 5 are larger moorings.
        float best = 1e30f;
        g.dock.fastBays = 0;
        g.dock.allBays = space::TargetDockBays(); // every dock point, the large berths too: where NPCs lock in
        for (const space::DockBay& b : g.dock.allBays)
        {
            if (b.type == 4 || b.type == 5)
                continue;
            // a docking ring (type 6) is one bay for a fighter, however many points it exposes (user, 19 Oct 2026)
            g.dock.fastBays = b.type == 6 ? 1 : g.dock.fastBays + 1;
            const float d[3] = {b.p2[0] - k.pos[0], b.p2[1] - k.pos[1], b.p2[2] - k.pos[2]};
            const float a[3] = {b.p2[0] - b.mount[0], b.p2[1] - b.mount[1], b.p2[2] - b.mount[2]};
            const float dist = Len3(d), al = Len3(a);
            if (dist < best && al > 0.1f)
            {
                best = dist;
                g.dock.fastBay = b.index;
                g.dock.fastKind = b.type == 6                              ? DockKind::Ring
                                  : b.type == 8 && g_set.airlockFlyThrough ? DockKind::Airlock
                                                                           : DockKind::Bay;
                for (int i = 0; i < 3; ++i)
                {
                    g.dock.bayP2[i] = b.p2[i];
                    g.dock.bayAxis[i] = a[i] / al;
                    g.dock.bayMount[i] = b.mount[i];
                }
                g.dock.bayLen = al;
            }
        }
        // A lane ring has no bays: its ride starts at a lane hardpoint (get_lane_start for the side the ship is on), in
        // the lane's direction - approached like a bay's p2 along the lane axis (docs\FREELANCER_RE.md "Lane ride").
        // lane_toward: the ring the ride should head to (0 = the way the ship's side gives).
        float start[3], dir[3];
        const u32 toward = g.dock.laneTowardPlan ? g.dock.laneTowardPlan : g_set.laneToward;
        if (g.dock.fastBay < 0 && space::TargetLaneStartToward(toward, start, dir, g.dock.laneSide))
        {
            g.dock.laneNext = toward;
            if (!g.dock.laneNext)
            {
                float s[3], d[3];
                space::TargetLaneStart(s, d, g.dock.laneNext); // the way the ship's side gives
            }
            g.dock.fastBay = 0;
            g.dock.fastKind = DockKind::Lane; // (no bay was picked: the kind was None)
            g.dock.bayLen = 60;               // a lane start's "animation": the short run from the start into the ring
            g.dock.laneCross = space::TargetLaneSide() != g.dock.laneSide; // that way starts from the ring's other side
            for (int i = 0; i < 3; ++i)
            {
                g.dock.bayP2[i] = start[i];
                g.dock.bayAxis[i] = -dir[i];
            }
            const float d[3] = {start[0] - k.pos[0], start[1] - k.pos[1], start[2] - k.pos[2]};
            best = Len3(d);
        }
        if (g.dock.fastBay < 0)
            return false;
        g.dock.fastStart = g.time;
        g.dock.fastBest.Reset(g.time);
        g.dock.ringNear = 1e30f;
        events::Push("autopilot fast-dock %s=%d dist=%.0f next=%u side=%d now=%d dock-points=%d",
                     g.dock.fastKind == DockKind::Lane ? "lane" : "bay", g.dock.fastBay, best, g.dock.laneNext,
                     g.dock.laneSide, g.dock.fastKind == DockKind::Lane ? space::TargetLaneSide() : -1,
                     static_cast<int>(g.dock.allBays.size()));
        // every dock point (which one the game assigns is logged after the request: `fast-dock port`)
        for (const space::DockBay& b : g.dock.allBays)
        {
            const float d[3] = {b.p2[0] - k.pos[0], b.p2[1] - k.pos[1], b.p2[2] - k.pos[2]};
            const float a[3] = {b.p2[0] - b.mount[0], b.p2[1] - b.mount[1], b.p2[2] - b.mount[2]};
            const float al = std::max(0.1f, Len3(a)), dl = std::max(0.1f, Len3(d));
            // the angle between our way in (to p2) and the bay's inward axis: 0 = straight down its line
            const float cosIn = -(d[0] * a[0] + d[1] * a[1] + d[2] * a[2]) / (al * dl);
            events::Push("autopilot fast-dock point index=%d type=%d dist=%.0f len=%.0f way-in=%.0fdeg", b.index,
                         b.type, dl, al, std::acos(std::clamp(cosIn, -1.0f, 1.0f)) * 57.2958f);
        }
    }
    // The lane's staging point lies beyond the ring's body (the start is ~250 m off its centre).
    float stage = g.dock.fastKind == DockKind::Lane ? std::max(g_set.dockStage, 700.0f) : g_set.dockStage;
    // an airlock's staging point beyond its own body (the Dyson door: 600 m; a staging point inside it had the run circle)
    if (g.dock.fastKind == DockKind::Airlock)
    {
        float bc[3], br;
        if (space::TargetBody(bc, br))
            stage = std::max(stage, br * 1.1f);
        // ...but no farther than two turning circles at the speed now when the door's front is free that far (user, 23
        // Oct 2026, M12 lair top: the staging point far up the axis made the run climb to y 580 and loop 270 deg; try 3
        // braked late and dove straight in)
        flight::Model sm;
        if (flight::ReadModel(sm) && sm.turnRate > 0)
        {
            const float near = std::max(150.0f, 2.0f * Len3(k.vel) / sm.turnRate);
            float sp[3];
            for (int i = 0; i < 3; ++i)
                sp[i] = g.dock.bayP2[i] + g.dock.bayAxis[i] * near;
            if (near < stage && LineClear(g.dock.bayP2, sp, space::PlayerRadius()))
                stage = near;
        }
    }
    float rel[3] = {k.pos[0] - g.dock.bayP2[0], k.pos[1] - g.dock.bayP2[1], k.pos[2] - g.dock.bayP2[2]};
    const float along = Dot3(rel, g.dock.bayAxis);
    const float latV[3] = {rel[0] - g.dock.bayAxis[0] * along, rel[1] - g.dock.bayAxis[1] * along,
                           rel[2] - g.dock.bayAxis[2] * along};
    const float lat = Len3(latV), dist = Len3(rel);
    // A stalled approach hands over to the game's own dock (M09 22 Oct 2026: Tekagi's Base inside its asteroid - the run
    // circled the rock for 5 min, never requesting): no 100 m gained on the bay in 20 s.
    // (an airlock: 60 s - its request is the lockout, FastApproach's airlock note; M13 22 Oct 2026: stalled 409 m out
    // while the avoidance detoured, the request at 20 s locked the ship out for the rest of the mission)
    if (g.dock.fastBest.Stalled(dist, 100, g.time, g.dock.fastKind == DockKind::Airlock ? 60.0f : 20.0f))
    {
        events::Push("autopilot fast-dock stalled dist=%.0f (no progress for 20 s): the game's dock", dist);
        return false;
    }
    // Lined up: in front of the bay inside a ~35 degree cone; from then on the final run-in straight at p2.
    if (!g.dock.fastLined && along > 30 && lat < along * 0.7f)
        g.dock.fastLined = true;
    // A lane needs no line-up (user, 18 Oct 2026: the staging point was a detour): straight at the start point, from
    // either side. After the request the game stops the ship (as full reverse does: flight::BrakeDistance) and starts
    // the ride once it rests within ~15 m of the start, taking the direction from the ship's side of the ring then
    // (get_lane_direction; the side at the request does not matter). So the aim is 10 m short of the start on the
    // wanted side, and the request goes out when the stop point lands there (Li01 ring 178, 18 Oct 2026: stopped on the
    // start -> ride 0.42 s after the request; stopped 40 m short -> 2.2 s, the game crept it forward; past the ring's
    // plane -> 3.8 s, backed up).
    if (g.dock.fastKind == DockKind::Lane)
        g.dock.fastLined = true;
    // A planet's docking ring needs no line-up either (user, 20 Oct 2026, M04 Leeds: "the player has to swerve sideways and
    // up before finally landing"; the 2020 footage: straight at the ring at ~300 m/s, the game's dock taking over close
    // in). Its two dock points enter at ~106 deg to a straight approach (Manhattan): lining up on one was the swerve, and
    // slow - requested at p2: 16-28 s more to docked; requested early while still heading straight (215 m at 273 m/s, and
    // Leeds 380 m): docked 7 s later, the game's own maneuver doing the alignment. So: straight at p2, full speed, the
    // request ~1.4 s out (at least 200 m), no braking of ours (the game slows the ship after the request).
    // (an airlock: straight at its mount once in front of the door - from the side or behind the line-up as at a gate:
    // straight at the mount from 1.5 km off its axis went round the outside, M13 22 Oct 2026)
    // ...or level with an airlock's mouth and close to its axis (s9r, the user's try 3 at the M12 lair top: level 15-25 m
    // below the door's p2 at 200 m/s straight at the axis, the pitch down from 160 m off it, the charge from ~100 m - in
    // by a curve, along -75 at 96 m off, -188 at 19 m off; ours climbed to 100-140 m above the door to line up, at 80 m/s,
    // ~6 s): the curve in from 130 m off the axis
    const bool mouthLevel = g.dock.fastKind == DockKind::Airlock && along < 90 && along > -0.5f * g.dock.bayLen;
    if (g.dock.fastKind == DockKind::Ring ||
        (g.dock.fastKind == DockKind::Airlock && along > 30 && lat < along * 1.5f) || (mouthLevel && lat < 130))
        g.dock.fastLined = true;
    // ...and an airlock's line-up lapses outside that cone and the door's tube (session 8, 23 Oct 2026, M12 lair top: lined
    // 61 m above the door at 300 m/s in cruise, the 90 deg turn down overshot, and the sticky line-up kept the cruise out to
    // 1.2 km - 25 s at the door)
    else if (g.dock.fastKind == DockKind::Airlock && g.dock.fastLined && along > -g.dock.bayLen &&
             lat > std::max(160.0f, along * 1.5f))
        g.dock.fastLined = false;
    // (level with the mouth, not lined yet: its centre, 40 m in, flown at level - try 3's line)
    const bool mouthRun = mouthLevel && !g.dock.fastLined && lat < 800;
    // (a "clear line from beside the door - straight in" rule, session 7, was reverted: in 3 of 3 runs the ship arrived fast
    // and nearly perpendicular, the rule took over and the run circled the door at 77-80 m/s - 2 of 3 never got in)
    // An airlock (dock type 8: the Dyson sphere's door, the Nomad lair and core) is flown through as the player does
    // (2020 footage, M13 19:41-19:43 and M12 5:12 / 5:52: straight into the opening door, HUD up, no docking camera): the
    // ship crosses the docking sphere at the mount and the server switches the system (airlock_in). The game's Dock
    // maneuver there locks the player out for good: its start sets the ship's control exclusion 4 (letterbox, no
    // steering), which only a landing, a jump's end or a launch clears, and an airlock's system switch is none of them
    // (Freelancer-Decomp space_state.cpp 0x54B4A6, client_iclient_space.cpp SystemSwitchOut / In; measured 22 Oct 2026:
    // 0 -> 4 at the dock, 4 for the rest of M13). Lined up, the aim is 200 m past the mount, at full speed.
    float aim[3];
    for (int i = 0; i < 3; ++i)
        aim[i] = g.dock.fastKind == DockKind::Lane ? g.dock.bayP2[i] + g.dock.bayAxis[i] * 10.0f
                 : g.dock.fastKind == DockKind::Airlock && g.dock.fastLined
                     ? g.dock.bayMount[i] - g.dock.bayAxis[i] * 200.0f
                 : g.dock.fastLined ? g.dock.bayP2[i]
                 : mouthRun         ? g.dock.bayP2[i] - g.dock.bayAxis[i] * 40.0f
                                    : g.dock.bayP2[i] + g.dock.bayAxis[i] * stage;
    // Around the station's body: a straight line through it goes via a point beside it instead (a lane run goes
    // through its ring by design: the lane tracks pass inside the hoop; a lined-up airlock run through its door).
    float c[3], r;
    if (g.dock.fastKind != DockKind::Lane &&
        !(g.dock.fastKind == DockKind::Airlock && (g.dock.fastLined || mouthRun)) && space::TargetBody(c, r))
    {
        const float seg[3] = {aim[0] - k.pos[0], aim[1] - k.pos[1], aim[2] - k.pos[2]};
        const float sl2 = Dot3(seg, seg);
        const float toC[3] = {c[0] - k.pos[0], c[1] - k.pos[1], c[2] - k.pos[2]};
        const float t = sl2 > 1 ? Clampf(Dot3(toC, seg) / sl2, 0, 1) : 0;
        float off[3] = {k.pos[0] + seg[0] * t - c[0], k.pos[1] + seg[1] * t - c[1], k.pos[2] + seg[2] * t - c[2]};
        const float ol = Len3(off);
        if (ol < r * 1.2f && t > 0 && t < 1)
        {
            if (ol < 1)
            {
                off[0] = 0, off[1] = 1, off[2] = 0;
            }
            const float n = ol < 1 ? 1 : ol;
            for (int i = 0; i < 3; ++i)
                aim[i] = c[i] + off[i] / n * r * 1.5f;
        }
    }
    // Speed, planned from the ship's flight model (core\game\flight: the game's force, drag, mass and cruise law,
    // verified against real flight 18 Oct 2026; user: deterministic calculations from the ship's stats): the exact
    // braking distance by full reverse decides when to brake (committed once begun: never re-armed), and the plan for
    // the way left - afterburner, or cruise if it gains even with its charge - decides the thrust.
    flight::Model fm;
    const bool haveModel = flight::ReadModel(fm);
    const float speed = Len3(k.vel);
    const float toAim[3] = {aim[0] - k.pos[0], aim[1] - k.pos[1], aim[2] - k.pos[2]};
    const float aimDist = Len3(toAim);
    const float requestAt = g.dock.fastKind == DockKind::Lane ? g_set.laneRequestDistance : g_set.dockRequestDistance;
    // a docking ring: the request this far from p2 (above); at least 400 m (20 Oct 2026, M04 Leeds: a floor of 200 m let
    // the distance shrink with the speed - slowed beside the ring, 300 m from p2, the ship stopped and never requested)
    // ...or at p2 itself at full speed (ring_request: the runners' way, the landing scene at the ring - see the setting)
    const float ringRequest = g_set.ringRequest > 0 ? g_set.ringRequest + speed * dt : std::max(400.0f, speed * 1.4f);
    const float toGo = !g.dock.fastLined                                                           ? aimDist
                       : g.dock.fastKind == DockKind::Lane || g.dock.fastKind == DockKind::Airlock ? aimDist
                       : g.dock.fastKind == DockKind::Ring ? std::max(0.0f, dist - ringRequest)
                                                           : std::max(0.0f, along - requestAt);
    const float arrive = g_set.dockArriveSpeed > 0 && g.dock.fastKind != DockKind::Airlock
                             ? g_set.dockArriveSpeed
                             : 1e6f; // no braking: full speed through
    // Braking distance plus one frame of travel at this speed (a late frame would overshoot the point).
    const float brakeDist = haveModel ? flight::BrakeDistance(fm, speed, arrive) + speed * dt : 0.0f;
    if (g.dock.fastLined && !g.dock.fastBrakeCommit && toGo <= brakeDist)
        g.dock.fastBrakeCommit = true;
    // The turn onto the bay's axis at the staging point must fit the turning circle (user, 19 Oct 2026: at California
    // Minor's ring the ship came from the planet side at full speed, overshot the staging point and looped round): a
    // turn of angle a needs a radius v / w within the staging leg - slow to w * stage / 2 for a full reversal (less for
    // a gentler turn), braking where the flight model says, as the travel's own arrival does (ArriveAt).
    float turnSpeed = 1e6f;
    if (!g.dock.fastLined && g.dock.fastKind != DockKind::Lane && haveModel && fm.turnRate > 0 && speed > 1)
    {
        const float inbound[3] = {-g.dock.bayAxis[0], -g.dock.bayAxis[1], -g.dock.bayAxis[2]};
        const float cosTurn = Dot3(k.vel, inbound) / speed;
        const float s = std::sqrt(std::max(0.0f, (1 - cosTurn) * 0.5f)); // sin(a / 2): 1 for a reversal
        if (cosTurn < 0.87f)                                             // more than ~30 degrees to turn
            turnSpeed = std::max(40.0f, fm.turnRate * stage * 0.5f / std::max(0.25f, s));
    }
    // An airlock's door close by and not lined up yet: engine speed, no cruise, no burner (session 8, 23 Oct 2026, M12 lair
    // top: the run arrived beside the door in cruise and circled it at 290-300 m/s, 400-670 m off its axis, for 22 s; once
    // at 80 m/s it was in within 7 s - and the user's try 3 levels off at y ~400, 200 -> 80 m/s, then straight down in)
    // (also lined up with the nose more than ~45 deg off the aim: the turn onto the axis is still to come)
    bool noseOff = false;
    float noseCos = 1;
    {
        float f0[3];
        geom::Forward(f0, k.rot);
        const float ta[3] = {aim[0] - k.pos[0], aim[1] - k.pos[1], aim[2] - k.pos[2]};
        const float tl = Len3(ta);
        noseCos = tl > 1 ? Dot3(f0, ta) / tl : 1.0f;
        noseOff = noseCos < 0.7f;
    }
    // (a turn still to make only - nose off the aim: lined up or not, a nose on the aim burns in; the user, 23 Oct 2026,
    // s9f: the ship "stopped afterburning and took its time to enter the core")
    // (airlock_burn: only as slow as the turn left needs - the aim at distance d, a off the nose, is reached on a circle
    // of radius v / w while d >= 2 R sin a, as the travel's arrival; the user, 24 Oct 2026, M12 gen 3 -> the lair door:
    // "an unnecessary slowdown ... it can all be afterburned"; s10 base: 200 -> 80 m/s, then 80 <-> 180 m/s pulses)
    float airlockTurn = 120.0f;
    // (the way in only - not under the cruise lock: s10 sets C-G, the M12 core's exit run without its slow-down at the
    // door circled the hub at ~290 m/s and passed the exit door 226-295 m off its axis - 7 fail states; set B without
    // airlock_burn 6 / 6. The user's note was gen 3 -> the core entrance.)
    const bool airlockBurnOn = g_set.airlockBurn && !g_obj.cruiseLock;
    if (airlockBurnOn && haveModel && fm.turnRate > 0)
    {
        const float sinA = std::sqrt(std::max(0.0f, 1 - noseCos * noseCos));
        const float sinUse = noseCos < 0 ? 1.0f : std::max(0.05f, sinA); // (behind: a reversal - the full circle)
        airlockTurn = std::max(80.0f, aimDist * fm.turnRate / (2 * sinUse));
    }
    const bool airlockSlow =
        g.dock.fastKind == DockKind::Airlock && noseOff && dist < 1200.0f && (!airlockBurnOn || speed > airlockTurn);
    if (airlockSlow)
        turnSpeed = std::min(turnSpeed, airlockTurn);
    const bool slowForTurn =
        speed > turnSpeed &&
        (airlockSlow ||
         aimDist <= (haveModel ? flight::BrakeDistance(fm, speed, turnSpeed) : 0.0f) + speed * dt + 50.0f);
    const bool braking = g.dock.fastBrakeCommit || slowForTurn;
    float fwd[3];
    geom::Forward(fwd, k.rot); // the nose (TurnTowards: -z)
    const float cosNose = aimDist > 1 ? Dot3(fwd, toAim) / aimDist : 1.0f;
    // Cruise only on a straight line (nose within 10 degrees): planned for the way left from the speed now (a running
    // cruise stays unless the burner would now win by more than a second - no switching back and forth).
    // The way left to the request point: lined up = straight to it; else to the staging point, then the staging leg
    // (was: + the whole distance to the bay - counted twice, 18 Oct 2026).
    // + the way round a planet's lethal zone between here and the aim (21 Oct 2026, M04: Planet Leeds' ring lies behind
    // the planet seen from the Iw03 hole - the straight-line plan said 23.5 s, the run round the zone's berth took 28.3),
    // as the avoidance flies it (avoid::PathLength); read once a second.
    if (g.dock.fastDetourAt < 0 || g.time - g.dock.fastDetourAt > 1)
    {
        g.dock.fastDetourAt = g.time;
        const float shipR = space::PlayerRadius();
        g.dock.fastDetour = std::max(
            0.0f, avoid::PathLength(k.pos, aim, space::Obstacles(k.pos, aimDist + 5000), shipR > 0 ? shipR : 10,
                                    haveModel && fm.cruiseSpeed > 0 ? fm.cruiseSpeed : 300.0f,
                                    haveModel && fm.turnRate > 0 ? fm.turnRate : 1.6f, g.target) -
                      aimDist);
    }
    const float way = (g.dock.fastLined                    ? toGo
                       : g.dock.fastKind == DockKind::Lane ? aimDist
                                                           : aimDist + std::max(0.0f, stage - requestAt)) +
                      g.dock.fastDetour;
    // The plan includes the turn onto the way (the top turn rate from the game's data) with the cruise charge running
    // during it, as a player engages cruise while turning; a running cruise stays on through turns (the charge is
    // paid) until the brake point - the way a player cruises right up to where the docking starts (user, 18 Oct 2026).
    bool wantCruise = haveModel && fm.cruiseEngaged && !braking && !airlockSlow;
    if (haveModel && !braking && !airlockSlow && way > 0)
    {
        const float turn = acosf(Clampf(cosNose, -1, 1));
        const flight::RunPlan p =
            flight::PlanRun(fm, speed, way, arrive, fm.cruiseEngaged ? fm.cruiseChargeElapsed : 0, turn);
        // Once charging or cruising, cruise stays unless the burner would win by more than a second (never at full
        // cruise speed: 300 > 200 m/s); otherwise the faster plan. fast_thrust 1 / 2 force burner / cruise (tests).
        wantCruise = g_set.fastThrust == 1   ? false
                     : g_set.fastThrust == 2 ? true
                     : fm.cruiseEngaged      ? p.cruiseTime < p.burnerTime + 1.0f
                                             : p.cruise;
        if (!g.dock.fastPlanned)
        {
            g.dock.fastPlanned = true;
            g.dock.fastPredicted = (g.time - g.dock.fastStart) + (g_set.fastThrust == 1   ? p.burnerTime
                                                                  : g_set.fastThrust == 2 ? p.cruiseTime
                                                                                          : p.time());
            events::Push("autopilot fast-dock plan way=%.0f detour=%.0f turn=%.0fdeg burner=%.1fs cruise=%.1fs -> %s",
                         way, g.dock.fastDetour, turn * 57.3f, p.burnerTime, p.cruiseTime,
                         wantCruise ? "cruise" : "burner");
        }
    }
    // A pilot's run, not a fight (user, 18 Oct 2026: the first version jittered): proportional steering (full rate
    // only beyond 20 degrees, no overshoot) and the afterburner with hysteresis - on once the nose is within 8 degrees,
    // off beyond 25 degrees, when braking, or while cruising.
    const bool burnWas = g.dock.fastBurn;
    if (braking || wantCruise || airlockSlow || cosNose < 0.906f || !fm.hasBurner)
        g.dock.fastBurn = false;
    else if (cosNose > 0.990f)
        g.dock.fastBurn = true;
    // (airlock_burn: the burner through the turn-in too while the turn still fits 20 m/s faster - s10-lair: the door run
    // turned in from 118 m/s with the burner off for the whole turn, 4.8 s for the last ~450 m)
    // (never while cruising or under the cruise lock: the burner drops a cruise - s10-e-i3, M12 core exit: 298 -> 113 m/s
    // at z -536, the exit run circled the hub ~12 s)
    if (g_set.airlockBurn && g.dock.fastKind == DockKind::Airlock && !braking && !wantCruise && fm.hasBurner &&
        cosNose > 0.5f && speed + 20.0f < airlockTurn && !fm.cruiseEngaged && !g_obj.cruiseLock)
        g.dock.fastBurn = true;
    g.dock.fastBurnSwitches += g.dock.fastBurn != burnWas ? 1 : 0;
    // An airlock run whose line is not clear (session 7, 23 Oct 2026, M12 core exit: started on the entrance side of the
    // core's hub, the run cruised at ~300 m/s round the hub for 30 s before it found the 90 m hole on the axis): the way
    // through the real geometry (GeoPathAim) to the aim, no cruise until the line is clear
    float aimUse[3] = {aim[0], aim[1], aim[2]};
    bool airPath = false, onAxis = false;
    // (only on the way to the door, more than 400 m from its outer point p2, and judged by the line to p2: the line to the
    // aim inside the airlock is never clear, so every run planned to p2 and circled it at engine speed, 210-380 m from the
    // mount - 3 of 3 runs, session 7)
    const float toP2[3] = {g.dock.bayP2[0] - k.pos[0], g.dock.bayP2[1] - k.pos[1], g.dock.bayP2[2] - k.pos[2]};
    if (g.dock.fastKind == DockKind::Airlock && Len3(toP2) > 400.0f)
    {
        if (g.time - g.arrive.lineClearAt > 0.25f)
        {
            g.arrive.lineClearAt = g.time;
            g.arrive.lineClear = LineClear(k.pos, g.dock.bayP2, space::PlayerRadius());
        }
        float rest = 0;
        // Back onto the door's axis first when that is clear (session 8, M12 core exit, gate_s8z: the cell step ended ~200 m
        // off the exit axis, the line to p2 crossed the hub beside its ~90 m hole, the planner's 9-11 leg lattice ways took
        // 24-28 s at engine speed; the runners and the user's try 3 keep to the axis, 20-52 m off it, 2.5 s): a point on
        // the axis ahead (3 s of speed, at least 300 m nearer the door than we are) when the way to it and on along the
        // axis to p2 are both clear - tested 4x a second
        if (!g.arrive.lineClear && along > 400.0f)
        {
            float rj[3];
            float a2 = std::max(0.0f, along - std::max(300.0f, speed * 3.0f));
            // (under the cruise lock: the farthest point of the axis in plain sight, as the travel's rejoin - s9g-i2, the
            // M12 cell tractored early, the run began 300-450 m off the exit axis and circled the hub ~30 s)
            if (g_obj.cruiseLock)
                for (const float lead : {1500.0f, 1200.0f, 950.0f, 750.0f, 550.0f, 400.0f})
                {
                    const float c2 = along - lead;
                    if (c2 < 0)
                        continue;
                    float q[3];
                    for (int i = 0; i < 3; ++i)
                        q[i] = g.dock.bayP2[i] + g.dock.bayAxis[i] * c2;
                    if (LineClear(k.pos, q, space::PlayerRadius()))
                    {
                        a2 = c2;
                        break;
                    }
                }
            for (int i = 0; i < 3; ++i)
                rj[i] = g.dock.bayP2[i] + g.dock.bayAxis[i] * a2;
            if (g.time - g.arrive.rejoinAt > 0.25f)
            {
                g.arrive.rejoinAt = g.time;
                const float shipR = space::PlayerRadius();
                const bool was = g.arrive.rejoinOk;
                // (under the cruise lock: decisive, as the travel's - s9e bounced off the M12 core's hub beside the hole
                // for ~20 s at cruise, 100-230 m off the axis)
                g.arrive.rejoinOk =
                    g_obj.cruiseLock || (LineClear(k.pos, rj, shipR) && LineClear(rj, g.dock.bayP2, shipR));
                if (g.arrive.rejoinOk && !was)
                    events::Push("autopilot fast-dock airlock: onto the door's axis %.0f m ahead (%.0f m off it)",
                                 along - a2, lat);
            }
            if (g.arrive.rejoinOk)
            {
                memcpy(aimUse, rj, sizeof(aimUse));
                onAxis = true;
                const float tc[3] = {rj[0] - k.pos[0], rj[1] - k.pos[1], rj[2] - k.pos[2]};
                const float tcl = Len3(tc);
                g.dock.fastBurn = tcl > 1 && Dot3(fwd, tc) / tcl > 0.95f && !braking && fm.hasBurner;
            }
        }
        // (the way to the door's outer point p2: the run's aim lies past the mount inside the airlock, where no way from
        // outside reaches - 24 000-30 000 rays, 88-162 ms each, "none", session 7)
        if (!g.arrive.lineClear && !onAxis &&
            GeoPathAim(g.arrive.path, k, g.dock.bayP2, g.target, aimUse, rest, "airlock"))
        {
            wantCruise = false;
            // (the nose judged against the way's carrot, which is steered at - against the door's aim the burner never lit
            // on a bent way: the M12 core exit ran 743 m at 80 m/s, ff430f5's runs, user try 3 ~300)
            const float tc[3] = {aimUse[0] - k.pos[0], aimUse[1] - k.pos[1], aimUse[2] - k.pos[2]};
            const float tcl = Len3(tc);
            const float cosWay = tcl > 1 ? Dot3(fwd, tc) / tcl : 1.0f;
            g.dock.fastBurn = cosWay > 0.95f && !braking && !airlockSlow && fm.hasBurner;
            airPath = true;
        }
    }
    // The charge started ~2.5 s before the transit, lined up and nose on (user's try 3: 1.9 s before the core entry at
    // 122 m/s; the user, 23 Oct 2026: "as soon as the ship approaches the entrance, it turns on cruise and enters
    // immediately"): the cruise completes inside, where the cruise lock keeps it
    // (a lead charge once begun is kept through the turn-in: dropped, it would start again from zero)
    const bool chargeAt = g_set.airlockChargeAt > 0 && haveModel && fm.engine.top() > 0;
    if (g.dock.fastKind == DockKind::Airlock && g.dock.fastLined && (!noseOff || (chargeAt && fm.cruiseEngaged)) &&
        !braking && haveModel && fm.cruiseChargeTime > 0)
    {
        const float m[3] = {g.dock.bayMount[0] - k.pos[0], g.dock.bayMount[1] - k.pos[1],
                            g.dock.bayMount[2] - k.pos[2]};
        // (0.5 s: the user, 23 Oct 2026, s9f: "afterburned right up to the entrance itself and charge the cruise engines"
        // - at 2.5 s the charge dropped the afterburner ~500 m out and the ship crept in)
        // (airlock_charge_at: the charge's share done at the transit - the user, 24 Oct 2026: "Cruise engines should start
        // their charge BEFORE entering the core airlock ... at 50% charge when the ship moves inside the core" (a margin,
        // and the delay cut); s9z, M12 core entry: the charge from 0.5 s out left the first ~5 s inside the core at 80
        // m/s; run 3 of the top-3 crosses the door and the core in cruise, v13717-13730. The way during the charge is
        // the engine's force law from the speed now - the burner is off while charging)
        bool charge;
        if (chargeAt)
        {
            const float left = std::max(0.0f, fm.cruiseChargeTime * std::min(1.0f, g_set.airlockChargeAt) -
                                                  (fm.cruiseEngaged ? fm.cruiseChargeElapsed : 0.0f));
            charge = fm.cruiseEngaged || Len3(m) < fm.engine.DistanceAfter(speed, left) + speed * dt;
        }
        else
            charge = Len3(m) < std::max(20.0f, speed) * 0.5f;
        if (charge)
        {
            wantCruise = true;
            g.dock.fastBurn = false;
        }
    }
    // A passage wide enough for the ship at speed (s9m-s9t, M12 core exit: a static plate across the core at z -1060 ..
    // -1100, y -10 .. +30, with a ~20 m hole on the axis - the ray along the axis passed, the ship at 300 m/s struck the
    // plate 0-23 m off the axis in 9 of 15 runs; the probe, 23 Oct 2026: clear above y +40 and below y -20, the user's
    // try 3 crossed at (4, 52)): when the line to the aim is clear for a ray but not for the ship + 20 m, a point 50-90 m
    // to the side at the blocking depth whose lines in and out are clear with that margin - flown through, then the aim
    // (ref_line: the reference flight's line instead, where it applies - RefLineAim)
    if (!RefLineAim(k, g.dock.bayP2, aimUse) && g.dock.fastKind == DockKind::Airlock && speed > 120)
        PassageVia(k, aimUse);
    SteerTo(out, aimUse, braking ? 0.0f : 1.0f, 0.35f, wantCruise ? 1 : 0);
    out.steerReverse = braking && speed > (slowForTurn ? turnSpeed : arrive);
    out.afterburn = g.dock.fastBurn;
    // (under the cruise lock no solid's sphere is avoided: inside the M12 core the core's own sphere pushed the run 120-280
    // m off the exit axis near the door, s9f-i1 bounced there; the guard still steers off real walls)
    ApplyAvoid(k, out, g.target,
               g.dock.fastKind == DockKind::Airlock &&
                   (g_obj.cruiseLock || (Len3(toP2) > 400.0f && (g.arrive.lineClear || airPath || onAxis))));
    // Pinned on an airlock run (session 8, M12 gate_s8y core exit: full throttle, 6 m/s, the nose 85 deg off the way's aim
    // and not turning - 21 s sliding along the core's structure): the travel's unstick - forward thrust without forward
    // speed for 1.5 s -> 2 s backing off and sliding to a side (a different one each time)
    if (g.dock.fastKind == DockKind::Airlock)
    {
        const float vFwd = Dot3(k.vel, fwd);
        Unstick(out, g.time, vFwd, out.steerThrottle > 0.5f && !braking && vFwd < 10 && dist > 60, 1.5f, 2.0f,
                g.arrive.stuckSince, g.arrive.unstickUntil, g.arrive.unstickDir, "fast-dock airlock");
    }
    // Lanes: where the game's stop would leave the ship if the request went out now.
    bool laneStopOnAim = false;
    if (g.dock.fastKind == DockKind::Lane)
    {
        const float stop = haveModel ? flight::BrakeDistance(fm, speed, 0) : 0.0f;
        float miss[3];
        for (int i = 0; i < 3; ++i)
            miss[i] = k.pos[i] + (speed > 1 ? k.vel[i] / speed * stop : 0.0f) - aim[i];
        laneStopOnAim = Len3(miss) <= requestAt || aimDist <= 5;
        // The game's own pick of the ride if the request went out now (get_lane_start for the ship as it is): only a
        // request it would answer with the wanted direction (from the other side: the first frame it picks it).
        float s[3], d[3];
        u32 next = 0;
        const bool gameWants = space::TargetLaneStart(s, d, next) && next == g.dock.laneNext;
        laneStopOnAim = gameWants && (g.dock.laneCross || laneStopOnAim);
    }
    // One bay and ships at it: at gates (and holes) the user's queue shortcut below; at bases the request goes in at
    // the normal point and the server's answer decides (21 Oct 2026; was a hold short of the bay on our own "busy"
    // reading - 19 Oct, M03 Willard). Only until the request is out (the lane ride kept this running 31 km down the
    // lane, a wait event every frame).
    bool forceRequest = false;
    if ((g.dock.fastKind == DockKind::Lane || g.dock.fastBays == 1) && g.dock.fastLined && !g.dock.fastRequested)
    {
        u32 busyBy = EnteringShip(dist);
        // The user's queue shortcut, their way (20 Oct 2026): "idle my ship right at the physical entrance to the jump
        // gate, maybe even a little bit further in (as the gate opens), and then as the NPC is locked in the docking
        // animation (their docking lights flash red), that's when I hit the formation button. Doing this guarantees I
        // immediately start the system jump"; lanes alike "very close to the correct ring in a way that is similar
        // fast-docking". Jump gates and lanes only ("definitely not docking or landing"; holes: maybe). The place: the
        // entrance (p2, a little inside); the moment: a ship locked in (EnteringShip: on the animation's path). Earlier
        // tries from 100-600 m on ships merely closing all missed (one even charged the cruise engine: an escort, not
        // the docker). Only when the queue is not empty (user, 20 Oct 2026: "this is only a shortcut if the docking
        // queue is not empty, otherwise the normal fast-dock behavior is optimal"): at gates and lanes a ship going in
        // never holds us short.
        const bool place = ShortcutPlace();
        if (place || g.dock.fastKind == DockKind::Lane) // lanes never queued (see ShortcutPlace): no hold-short either
            busyBy = 0;
        // A race, not a wait (user, 20 Oct 2026: "a player wouldn't choose to wait around to make this work. It's either
        // a fast-dock or quickly ascertaining an NPC will hit the dock queue first, and then the player rushes in to the
        // perfect spot to then toggle formation and instantly jump. The focus is to get the jump going as fast as
        // possible."): a ship going in that reaches the entrance before our fast dock would (EnteringShip's ETA at its
        // closing speed; locked in = now) - or the server's "queued" - starts the rush: on at full speed to the entrance,
        // no request, formation the moment a ship is locked in there; the rush ends (and the request goes in) when no ship
        // is ahead any more, or 5 s at the entrance, or 30 s in all.
        // ...never under fire (session 8, 23 Oct 2026, M12 Omicron Minor hole: the rush parked at the entrance behind a
        // Nomad going in, 5 attackers, hull 0.56 -> dead in 4 s; the plain request under fire below takes the hole at once)
        const bool underFire = g.stats.pressure > 0 || g_sit.attackers >= 3;
        if (place && !g.dock.formationTried && !g.dock.rushing && !underFire)
        {
            float ourEta = g.dock.fastPredicted - (g.time - g.dock.fastStart);
            if (ourEta <= 0)
                ourEta = dist / std::max(speed, 50.0f);
            if (g.dock.parkForQueue || g.dock.aheadEta < ourEta)
            {
                g.dock.rushing = true;
                g.dock.shortcutSince = g.time;
                g.dock.parkedSince = -1;
                events::Push("autopilot fast-dock shortcut-rush ahead-eta=%.1f our-eta=%.1f dist=%.0f bay-len=%.0f%s",
                             g.dock.aheadEta, ourEta, dist, g.dock.bayLen, g.dock.parkForQueue ? " (queued)" : "");
            }
        }
        if (g.dock.rushing && !g.dock.formationTried && underFire)
        {
            g.dock.rushing = false;
            g.dock.formationTried = true;
            events::Push("autopilot fast-dock shortcut-rush off (under fire, attackers=%d): the fast dock as usual",
                         g_sit.attackers);
        }
        if (g.dock.rushing && !g.dock.formationTried)
        {
            const float inset = std::min(30.0f, 0.1f * g.dock.bayLen);
            const float park[3] = {g.dock.bayP2[0] - g.dock.bayAxis[0] * inset,
                                   g.dock.bayP2[1] - g.dock.bayAxis[1] * inset,
                                   g.dock.bayP2[2] - g.dock.bayAxis[2] * inset};
            const float pd[3] = {park[0] - k.pos[0], park[1] - k.pos[1], park[2] - k.pos[2]};
            const float parkDist = Len3(pd);
            if (g.dock.lockedIn && parkDist < 80)
            {
                g.dock.rushing = false;
                StartFormationShortcut(g.dock.lockedIn, dist, " (locked in)");
            }
            else
            {
                if (parkDist < 25 && speed < 10 && g.dock.parkedSince < 0)
                {
                    g.dock.parkedSince = g.time;
                    events::Push("autopilot fast-dock shortcut-rush at the entrance after %.1f s",
                                 g.time - g.dock.shortcutSince);
                }
                const bool ahead = g.dock.parkForQueue || g.dock.aheadEta < 1e8f;
                // At the entrance: on while the ship ahead is still coming (it locks in on arrival - the queue it makes
                // would cost us the same time, and leaving the entrance gives the moment away), 10 s at most; 2 s when
                // none is in sight any more (no waiting around: user)
                // (about to arrive = under 5 s away: a ship 10.8 s out never locked in and the 10 s hold was lost time)
                const bool timeUp = g.dock.parkedSince >= 0
                                        ? g.time - g.dock.parkedSince > (g.dock.aheadEta < 5 ? 10 : 2)
                                        : g.time - g.dock.shortcutSince > 30;
                if (ahead && !timeUp)
                {
                    // far off: the fast approach as it is (full speed at p2); near: on to the entrance point, braking
                    // only as late as the flight model allows
                    if (parkDist < 600)
                    {
                        out.steerTo = true;
                        memcpy(out.steerPos, park, sizeof(out.steerPos));
                        const bool brake = haveModel ? flight::BrakeDistance(fm, speed, 0) + speed * dt >= parkDist - 10
                                                     : speed > std::min(150.0f, parkDist * 0.6f);
                        out.cruise = 0;
                        out.afterburn = !brake && parkDist > 150 && out.afterburn;
                        out.steerThrottle = brake || parkDist < 15 ? 0.0f : 1.0f;
                        out.steerReverse = brake && speed > 3;
                    }
                    return true;
                }
                g.dock.rushing = false;
                g.dock.formationTried = true;
                g.dock.formationAt = -1;
                // at the entrance: the request now; still on the way: the fast dock goes on as if no rush (20 Oct 2026:
                // a call-off at a lane requested from 1.1 km - the game's slow entry instead of the fast one)
                forceRequest = g.dock.parkedSince >= 0 || parkDist < 100;
                events::Push("autopilot fast-dock shortcut-rush off (%s): %s",
                             !ahead                    ? "no ship ahead any more"
                             : g.dock.parkedSince >= 0 ? "time up at the entrance"
                                                       : "30 s",
                             forceRequest ? "requesting" : "the fast dock as usual");
            }
        }
        // A ship at a base's bay no longer holds us short before the request (user, 21 Oct 2026: "request at the normal
        // point and hold short only if the game answers queued"): our own reading of "busy" guessed what the server
        // decides - at Planet Leeds (M05) two ships at the bay held us 13 s (the busy test flickered: afterburner taps)
        // and the request at 400 m was granted at once; the 2020 playthrough docked there in ~18 s. The server's
        // answer is the signal: 3 = queued, and the game's own dock maneuver then waits for its turn (the reply
        // handling logs it).
        (void)busyBy;
        const int fs = FormationShortcutState();
        if (fs == 1)
            return true; // the game flies us: no steering, no request of ours meanwhile
        if (fs == 2)
            forceRequest = true; // the request now: it replaces the formation maneuver
    }
    // An airlock is entered by flying into it, never by a request (above). Past the mount by 50 m and still here (this
    // runs only before the system switch) for 3 s - the door closed, the sphere missed: the request after all, as
    // before (the lockout is then the game's; it is logged as control-exclusion).
    if (g.dock.fastKind == DockKind::Airlock)
    {
        static float logAt = -10;
        if (g.time - logAt >= 2 || g.time < logAt)
        {
            logAt = g.time;
            const float m[3] = {g.dock.bayMount[0] - k.pos[0], g.dock.bayMount[1] - k.pos[1],
                                g.dock.bayMount[2] - k.pos[2]};
            events::Push("autopilot fast-dock airlock run mount=%.0f along=%.0f lat=%.0f speed=%.0f nose=%.2f aim=%.0f",
                         Len3(m), along, lat, speed, cosNose, aimDist);
        }
        // A pass beside the door under the cruise lock (s9j-i3, M12 core exit: a bump at the power cell's bar left the run
        // 145 m off the axis; at ~290 m/s the turn (radius ~280 m) never fitted the core's ~300 m tube, the run looped
        // past the door 10 times in 58 s until the stall fallback): the lock gives way for this run - the run lines up at
        // engine speed and charges 0.5 s before the transit as usual, the transit puts the lock back on
        if (g_obj.cruiseLock && g.dock.airlockAlong > 0 && along <= 0 && lat > 120)
        {
            g_obj.cruiseLock = false;
            events::Push("autopilot cruise lock off: the airlock run passed the door %.0f m off its axis at %.0f m/s - "
                         "lining up below cruise",
                         lat, speed);
        }
        g.dock.airlockAlong = along;
        if (along < -g.dock.bayLen - 50 && lat < 200 && g.dock.airlockPastAt < 0) // (through the door: near its axis)
            g.dock.airlockPastAt = g.time;
        if (g.dock.airlockPastAt < 0 || g.time - g.dock.airlockPastAt < 3)
            return true;
        events::Push("autopilot fast-dock airlock: past the mount %.0f m, no transit in 3 s - requesting",
                     -along - g.dock.bayLen);
        g.dock.fastKind = DockKind::Bay; // (the airlock requested as a bay from here)
        forceRequest = true;
        g.dock.fastLined = true;
    }
    // Under fire beside the entry (M12 22 Oct 2026: 10 m from the jump hole's entry but facing away, the line-up loop of
    // 500 m ran under 15 Nomads - dead in 11 s): the request at once, the game's own dock maneuver turns the ship in.
    // ...but not a lined-up run closing fast (s9x-i1, M12 Omicron Beta hole: requested at 400 m among 15 attackers, the
    // game's dock crawled the rest at 80 -> 3 m/s, the jump took ~13 s, 9 Nomads followed into Omicron Minor and the ship died
    // there; the runners hold 299 m/s to 131 m and jump at once): its request comes at the normal point, at speed
    const float toP2n[3] = {g.dock.bayP2[0] - k.pos[0], g.dock.bayP2[1] - k.pos[1], g.dock.bayP2[2] - k.pos[2]};
    const bool closingFast = g.dock.fastLined && speed > 100 && dist > 1 && Dot3(k.vel, toP2n) / (speed * dist) > 0.9f;
    if (!forceRequest && g.dock.fastKind != DockKind::Lane && dist < 400 && !closingFast &&
        (g.stats.pressure > 0 || g_sit.attackers >= 3))
    {
        events::Push("autopilot fast-dock under fire %.0f m from p2 (attackers=%d): requesting now", dist,
                     g_sit.attackers);
        forceRequest = true;
        g.dock.fastLined = true;
    }
    // Failsafe: lined up but at rest for 3 s with no request (a deadlock of any rule above - Leeds 20 Oct 2026: 3 min at
    // rest beside the ring) - the request now; the game's dock takes it from anywhere.
    g.dock.fastStill = speed < 5 && g.dock.fastLined ? g.dock.fastStill + dt : 0.0f;
    if (g.dock.fastStill > 3 && !forceRequest)
    {
        events::Push("autopilot fast-dock at rest %.0f m from p2 with no request: requesting", dist);
        forceRequest = true;
    }
    // A ring run to be requested at p2 that passes it (missed by a frame, or pushed off by the avoidance): the request at
    // once - the game's dock turns back from there (Fort Bush: 20 m past p2, 2.7 s)
    if (g.dock.fastKind == DockKind::Ring && g_set.ringRequest > 0 && !forceRequest)
    {
        if (dist < g.dock.ringNear)
            g.dock.ringNear = dist;
        else if (g.dock.ringNear < 150 && dist > g.dock.ringNear + 10)
        {
            events::Push("autopilot fast-dock ring: past p2 (nearest %.0f m, now %.0f m) - requesting", g.dock.ringNear,
                         dist);
            forceRequest = true;
        }
    }
    if (g.dock.fastLined && (forceRequest || (g.dock.fastKind == DockKind::Lane   ? laneStopOnAim
                                              : g.dock.fastKind == DockKind::Ring ? dist <= ringRequest
                                                                                  : along <= requestAt)))
    {
        g.dock.fastRequested = true;
        g.dock.fastRequestAt = g.time;
        g.dock.grantHanded = false; // (a new fast request: its grant may be handed over once)
        MarkSubmit();
        ai::Maneuver(ai::OpDock, false);
        g.started = true;
        events::Push("autopilot fast-dock request bay=%d dist=%.0f speed=%.0f after=%.1f predicted=%.1f "
                     "burn-switches=%d",
                     g.dock.fastBay, dist, speed, g.time - g.dock.fastStart, g.dock.fastPredicted,
                     g.dock.fastBurnSwitches);
    }
    return true;
}

} // namespace autopilot
