// Advanced Autopilot - hold: the Hold mode's frame (UpdateHold) - keep station at the point given or where the ship rests,
// clear of a base's lanes and of lethal zones, the scenic point, the look. Split from UpdateModes (autopilot_frame.cpp) on
// 24 Oct 2026, the case's body as it was (its switch-level `break`s now `return`s): the behaviour is unchanged.
#include "autopilot_impl.h"

namespace autopilot
{
// ------------------------------------------------------------------ hold
// The Hold mode's frame (the `case Mode::Hold` of UpdateModes until 24 Oct 2026). `op` is the ship's AI op UpdateModes read at
// the top of the frame (Op()), passed in so the yield test reads the same value as before.
void UpdateHold(float dt, Out& out, int op)
{
    // Keep station (user, 18 Oct 2026: "hold still at the waypoint" while scanning - free flight drifts on):
    // the point given, or where the ship was at the start; the player's run to a point, then brake on it.
    // A hold never cruises (21 Oct 2026, M05: the objective mode swapped a travel for a hold in the jump tunnel to
    // Leeds; a "restart" stop keeps the cruise for the next mode, Idle does not end it, and the ship cruised 28 s
    // away from Juni at 299 m/s - the 2020 playthrough stopped at the hole's exit and watched the scene).
    out.cruise = 0;
    fl::Kinematics k = {};
    const bool haveKin = fl::PlayerKinematics(k);
    space::Pilot pi;
    const bool havePilot = space::ReadPilot(pi);
    // A jump moves the ship into another system: the hold point (and any stop issued in the tunnel) belonged to the
    // old one - start over: stop, then rest where the ship arrived. While jumping there is no point at all (the
    // repro, 21 Oct 2026: a hold begun in the jump kept its start point from the old system's coordinates and flew
    // at it 30 s in the new one - the system had already switched on its first frame).
    if (havePilot && pi.jumping)
    {
        g.hasPos = false;
        g.hold.phase = HoldPhase::Run;
        g.hold.system = 0;
        g.hold.scenicTried = true;
        return;
    }
    if (havePilot && pi.system && g.hold.system && pi.system != g.hold.system)
    {
        events::Push("autopilot hold new system=%u (was %u): hold starts over", pi.system, g.hold.system);
        g.hasPos = false;
        g.hold.phase = HoldPhase::Run;
        g.hold.scenicTried = true; // no scenic move in a system just entered
    }
    if (havePilot && pi.system)
        g.hold.system = pi.system;
    // Per-second state while moving (the M05 hold above logged nothing for 28 s)
    if (g.time - g.hold.trace >= 1)
    {
        g.hold.trace = g.time;
        flight::Model fm;
        const bool haveFm = flight::ReadModel(fm);
        const float sp = haveKin ? Len3(k.vel) : -1;
        if (!haveKin || sp > 5 || (haveFm && fm.cruiseEngaged))
            events::Push("autopilot hold state kin=%d phase=%d has-pos=%d speed=%.0f cruise=%d settled=%d system=%u",
                         haveKin, static_cast<int>(g.hold.phase), g.hasPos, sp, haveFm ? fm.cruiseEngaged : -1,
                         SubmitSettled(), havePilot ? pi.system : 0);
    }
    if (!haveKin)
        return;
    // How the game parks its own ships (vanilla mission ObjLists): Idle (m01a's "stop_player" stops the player with
    // it: a full stop in about a second, then free flight at rest) inside a StayInRange leash, the nose turned by
    // the Face maneuver. Was: our steering aimed at the hold point the
    // ship sat on - the nose chased a point beside it and the ship spun (user, 18 Oct 2026).
    // No point given, a fight just ended over a friendly ship: rest beside it (scenic idling, ScenicPoint).
    if (!g.hasPos && g.hold.phase == HoldPhase::Run && !g.hold.scenicTried && g.hold.scenic)
    {
        g.hold.scenicTried = true;
        float sp[3];
        if (ScenicPoint(k.pos, sp))
        {
            memcpy(g.pos, sp, sizeof(g.pos));
            g.hasPos = true;
            char why = 0;
            const u32 subject = RestSubject(why);
            events::Push("autopilot hold scenic subject=%u why=%c at=%.0f,%.0f,%.0f", subject, why ? why : '-', sp[0],
                         sp[1], sp[2]);
        }
    }
    // ... but never in a base's launch / docking lanes (user, 21 Oct 2026, M04 Mactan: "the player sits idle just
    // outside Mactan's docking bays when launching ... it blocks spawning fighters from leaving and is, well...
    // rude"): near a dockable base (not a lane ring, gate or hole) the rest point is off our exit line - its lanes
    // run outward from the base - rotated 45 deg around it, at a distance from the base's own size (user: "1.5 km
    // out is excessive, pick something relative to the size of the station"): two radii from its centre (one clear
    // of its surface), at least its radius + 150 m; applies within 2.5 radii + 300 m.
    if (!g.hasPos && g.hold.phase == HoldPhase::Run)
    {
        const space::Contact* base = nullptr;
        float baseR = 0;
        for (const space::Contact& c : Contacts())
        {
            if (c.ship || !c.dock || c.distance > 6000 ||
                (c.type & (space::kTradelaneRing | space::kJumpGate | space::kJumpHole)))
                continue;
            float r = 0;
            for (const space::Obstacle& ob : space::Obstacles(c.pos, 10))
                if (ob.id == c.id)
                    r = ob.radius;
            if (r > 0 && c.distance < 2.5f * r + 300 && (!base || c.distance < base->distance))
                base = &c, baseR = r;
        }
        if (base)
        {
            float dir[3] = {k.pos[0] - base->pos[0], k.pos[1] - base->pos[1], k.pos[2] - base->pos[2]};
            const float d = std::max(1.0f, Len3(dir));
            for (float& v : dir)
                v /= d;
            const float up[3] = {0, 1, 0};
            float p[3];
            geom::Cross(p, dir, up);
            const float pl = Len3(p);
            if (pl > 0.1f)
            {
                const float r = std::max({d, 2.0f * baseR, baseR + 150.0f}), c45 = 0.7071f;
                for (int i = 0; i < 3; ++i)
                    g.pos[i] = base->pos[i] + (dir[i] * c45 + p[i] / pl * c45) * r;
                // never inside a planet's lethal zone (M10 22 Oct 2026: Holstein's ring - the point 503 m off it lay
                // in the planet, the ship flew in and died): out from the zone's centre to its surface + 300 m
                for (const space::Obstacle& ob : space::Obstacles(g.pos, 300))
                {
                    if (!ob.zone)
                        continue;
                    float zd[3] = {g.pos[0] - ob.pos[0], g.pos[1] - ob.pos[1], g.pos[2] - ob.pos[2]};
                    const float zl = Len3(zd);
                    if (zl >= ob.radius + 300 || zl < 1)
                        continue;
                    for (int i = 0; i < 3; ++i)
                        g.pos[i] = ob.pos[i] + zd[i] / zl * (ob.radius + 300);
                    events::Push("autopilot hold point moved out of a lethal zone (radius %.0f)", ob.radius);
                }
                g.hasPos = true;
                events::Push(
                    "autopilot hold clear of base=%u's lanes (radius %.0f): %.0f m from it, 45 deg off the exit line",
                    base->id, baseR, r);
            }
        }
    }
    // No point given: hold where the ship comes to rest - stop first (Idle) rather than turn back to the start.
    if (!g.hasPos && g.hold.phase == HoldPhase::Run)
    {
        ai::Directive d;
        d.op = ai::OpIdle;
        SubmitNow(d);
        g.hold.phase = HoldPhase::Stopping;
        g.tacticTime = 0;
        return;
    }
    // Another behaviour took the ship (a native maneuver - jump, dock, lane -, a mission directive): a hold is a
    // wait, so it yields at once instead of pulling the ship back (the harness parks the ship between actions).
    if (SubmitSettled() && op >= 0 && op != ai::OpFreeFlight && op != ai::OpIdle)
    {
        events::Push("autopilot hold yields op=%s", ai::OpName(op));
        StopImpl(StopReason::Yield, false);
        return;
    }
    const float off[3] = {g.pos[0] - k.pos[0], g.pos[1] - k.pos[1], g.pos[2] - k.pos[2]};
    const float speed = Len3(k.vel);
    if (g.hasPos && g.hold.phase != HoldPhase::Run && Len3(off) > std::max(g_set.holdRadius, 160.0f))
    {
        events::Push("autopilot hold leash dist=%.0f", Len3(off));
        g.hold.phase = HoldPhase::Run;
        FreeFlightNow(); // our steering flies the way back
    }
    if (g.hold.phase == HoldPhase::Run)
    {
        if (!ArriveAt(dt, g.pos, out, true)) // a hold is never in a hurry
            return;
        g.dock.fastBrakeCommit = false; // a drift away starts a new run
        ai::Directive d;
        d.op = ai::OpIdle;
        const int r = SubmitNow(d);
        events::Push("autopilot hold idle result=%d", r);
        g.hold.phase = HoldPhase::Stopping;
        g.tacticTime = 0;
        return;
    }
    if (!SubmitSettled())
        return;
    if (g.hold.phase == HoldPhase::Stopping) // stopping (or 4 s at most)
    {
        if (speed > 3 && g.tacticTime < 4)
            return;
        if (!g.hasPos) // the rest point is the hold point
        {
            memcpy(g.pos, k.pos, sizeof(g.pos));
            g.hasPos = true;
        }
        g.hold.phase = HoldPhase::Resting;
        g.hold.pick = -10; // look now
    }
    // At rest (Idle is a one-shot full stop; free flight then stays at rest): every 3 s choose where to look and
    // stop a drift with Idle; the nose turns there by steering at zero throttle (a far point: no spin). Steering,
    // not the game's Face maneuver: Face works only on the selection (it would change the player's target) and
    // creeps on at ~2.5 m/s (18 Oct 2026).
    if (g.time - g.hold.pick >= 3)
    {
        g.hold.pick = g.time;
        const char src = HoldLook(k.pos, g.hold.look);
        // level: heading only, no pitch (the system's plane) - except waiting at the next step's target (pre-stage):
        // the nose on it, pitch and all, so the first shot goes the moment it can do damage (user, 23 Oct 2026, M12:
        // the ship "turned away due to some idle logic" beside generator 1 - the look was levelled 260 m above it)
        if (!(g_obj.preStageTarget && g_obj.action == Action::PreStage))
            g.hold.look[1] = k.pos[1];
        if (src != g.hold.lookSrc)
            events::Push("autopilot hold look=%c at=%.0f,%.0f,%.0f", src ? src : '-', g.hold.look[0], g.hold.look[1],
                         g.hold.look[2]);
        g.hold.lookSrc = src;
        if (speed > 5)
        {
            ai::Directive d;
            d.op = ai::OpIdle;
            SubmitNow(d);
            return;
        }
    }
    if (!g.hold.lookSrc)
        return;
    float fwd[3];
    geom::Forward(fwd, k.rot);
    const float to[3] = {g.hold.look[0] - k.pos[0], g.hold.look[1] - k.pos[1], g.hold.look[2] - k.pos[2]};
    const float d = Len3(to);
    const float cosNose = d > 1 ? Dot3(fwd, to) / d : 1.0f;
    // Turn when 10 deg off, until within 3 (phase 2 = turning): a resting pilot does not chase a drifting subject
    // degree by degree (was 6 / 2: the prison ship rest twitched, user 19 Oct 2026).
    if (g.hold.phase == HoldPhase::Resting && cosNose < 0.9848f)
        g.hold.phase = HoldPhase::Turning;
    else if (g.hold.phase == HoldPhase::Turning && cosNose > 0.9986f)
        g.hold.phase = HoldPhase::Resting;
    if (g.hold.phase == HoldPhase::Turning)
    {
        // (full angle 0.8: proportional below ~45 deg - a calm, unhurried turn that settles, no overshoot)
        SteerTo(out, g.hold.look, 0, 0.8f, 0);
        out.steerReverse = false;
        out.afterburn = false;
    }
    return;
}
} // namespace autopilot
