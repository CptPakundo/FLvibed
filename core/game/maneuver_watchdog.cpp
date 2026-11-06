// The maneuver watchdog of the player's game maneuvers, run each frame, whoever flies (maneuver_watchdog.h).
#include "game/maneuver_watchdog.h"

#include "dimouse.h"
#include "game/ai.h"
#include "game/events.h"
#include "game/flightio.h"
#include "game/maneuver_watch.h"
#include "game/space.h"
#include "log.h"
#include "ui/views.h"

namespace mwatch
{
static bool g_testStall = false; // `watchdog test`: the maneuver reads as stopped

// The maneuver watchdog (setting `maneuver_watchdog`; rule and the why: maneuver_watch.h). The owner's M01 report: a
// formation with the Brandt stalled at the mission's LockManeuvers (constant hard turn, 0 m/s, the convoy gone). While a
// game formation / goto / dock runs outside the Advanced Autopilot, a stall far from the goal logs one snapshot and
// issues the same maneuver again the game's way (target the leader / goal, the player's maneuver dispatcher
// ai::Maneuver = the maneuver bar's call, as the harness's Start-FLFormation), then puts the player's target back.
void Frame()
{
    static mwatch::Watch watch;
    static int lastOp = -1;
    static u32 goal = 0;      // goto / dock: the target when the op began; formation: the ship the player formed on
    static float lastSteerAt = -100;
    const float t = static_cast<float>(GetTickCount()) * 0.001f;
    mwatch::Sample s;
    s.t = t;
    space::Pilot p;
    const bool flying = flightio::Get().maneuverWatchdog && space::ReadPilot(p);
    if (!flying)
    {
        lastOp = -1, goal = 0;
        watch.Step(s);
        return;
    }
    const bool kind = p.op == ai::OpGoto || p.op == ai::OpDock || p.op == ai::OpFormation;
    if (p.op != lastOp)
        goal = kind ? (p.op == ai::OpFormation && !p.target ? p.leader : p.target) : 0;
    lastOp = p.op;
    // the distance is to the formation's leader (the wing), else to the goal
    const u32 to = p.op == ai::OpFormation ? (p.leader ? p.leader : goal) : goal;
    float gp[3];
    const bool known = kind && to && !p.lane && !p.jumping && space::ObjectPos(to, gp);
    space::ShipMotion m;
    float pad[6] = {};
    u32 padButtons = 0;
    if (views::padflight.Raw)
        views::padflight.Raw(pad, &padButtons);
    const float stick = (std::max)((std::max)(fabsf(pad[0]), fabsf(pad[1])), (std::max)(fabsf(pad[2]), fabsf(pad[3])));
    const u32 mouseAt = dimouse::LastRealTick();
    if (stick > 0.2f || (mouseAt && GetTickCount() - mouseAt < 1000))
        lastSteerAt = t;
    s.maneuver = known && space::ShipMotionOf(space::PlayerShipId(), m) && m.ok;
    s.key = (static_cast<unsigned>(p.op) << 24) ^ to;
    s.autopilot = views::ApActive();
    s.steering = t - lastSteerAt < 1.0f;
    if (p.op == ai::OpDock && hostcalls::g.DockReply)
    {
        u32 ship = 0, replyKind = 0, response = 0;
        s.queued = hostcalls::g.DockReply(&ship, &replyKind, &response) && replyKind == 0 && response == 3; // (FREELANCER_RE "Docking")
    }
    if (s.maneuver)
    {
        s.speed = m.speed;
        const float d[3] = {gp[0] - p.pos[0], gp[1] - p.pos[1], gp[2] - p.pos[2]};
        s.dist = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        if (g_testStall)
            s.speed = 0, s.dist = 5000; // (still closing in does not count as a stall)
    }
    const mwatch::Decision dec = watch.Step(s);
    const char* opName = p.op == ai::OpGoto ? "goto" : p.op == ai::OpDock ? "dock" : "formation";
    if (dec.snapshot)
    {
        const u32 now = GetTickCount();
        const u32 padAt = views::padflight.LastPadTick ? views::padflight.LastPadTick() : 0, kbmAt = views::padflight.LastKbmTick ? views::padflight.LastKbmTick() : 0;
        // the leader / goal as the game reports it (the owner's 4 Nov report: the stall began as the convoy docked with
        // a trade lane - a formation follower runs a copy of its leader's dock, docs\FREELANCER_RE.md "Formation")
        int lDock = -1, lLane = -1, lJump = -1;
        float lSpeed = -1;
        for (const space::Contact& c : space::Contacts())
            if (c.id == to)
            {
                lDock = c.dockPort ? 1 : 0, lLane = c.lane ? 1 : 0, lJump = c.jumping ? 1 : 0;
                break;
            }
        space::ShipMotion lm;
        if (space::ShipMotionOf(to, lm) && lm.ok)
            lSpeed = lm.speed;
        events::Push("event maneuver stalled: op=%s locked=%d leader=%u goal=%u target=%u dist=%.0f speed=%.1f "
                     "throttle lt=%.2f rt=%.2f steer active=%d x=%.2f y=%.2f device=%s last inputs pad=%dms kbm=%dms "
                     "mouse=%dms sticks l=%.2f,%.2f r=%.2f,%.2f buttons=0x%X cruise=%d | leader docking=%d lane=%d "
                     "jumping=%d speed=%.1f",
                     opName, p.locked ? 1 : 0, p.leader, goal, p.target, s.dist, s.speed, pad[4], pad[5],
                     flightio::SteerActive() ? 1 : 0, flightio::SteerX(), flightio::SteerY(), views::DevicePad() ? "pad" : "kbm",
                     padAt ? static_cast<int>(now - padAt) : -1, kbmAt ? static_cast<int>(now - kbmAt) : -1,
                     mouseAt ? static_cast<int>(now - mouseAt) : -1, pad[0], pad[1], pad[2], pad[3], padButtons,
                     p.cruise ? 1 : 0, lDock, lLane, lJump, lSpeed);
    }
    if (dec.retry)
    {
        // the ship to issue it on: the one the player formed on / the goal; a formation whose ship is gone -> the leader
        float tmp[3];
        u32 on = goal && space::ObjectPos(goal, tmp) ? goal : p.op == ai::OpFormation ? p.leader : 0;
        const u32 before = p.target;
        bool targeted = on && (before == on || space::SetTarget(on));
        // Left first, then issued again - the player's own way out (the owner's 4 Nov report: the same formation issued on
        // a ship still in it got reply -1 twice and the ship kept turning in place; free flight twice, then FORMATION,
        // got reply 0 and no stall followed). The op changes in the frame of the call (FREELANCER_RE "Maneuvers").
        if (targeted)
            ai::FreeFlight();
        const bool ok = targeted && ai::Maneuver(p.op, false);
        if (before && before != on && targeted)
            space::SetTarget(before); // the player's target as it was (the maneuver keeps its own)
        space::Pilot after;
        const bool read = space::ReadPilot(after);
        events::Push("maneuver watchdog: re-issue %s on %u try %d/%d: %s (op now %d, locked=%d, leader=%u)", opName,
                     on, dec.attempt, mwatch::kMaxTries,
                     !on ? "no goal" : !targeted ? "target refused" : ok ? "accepted" : "refused",
                     read ? after.op : -1, read && after.locked ? 1 : 0, read ? after.leader : 0);
    }
    if (dec.gaveUp)
        events::Push("maneuver watchdog: %s still stalled after %d tries (dist=%.0f speed=%.1f locked=%d): giving up "
                     "until it moves",
                     opName, mwatch::kMaxTries, s.dist, s.speed, p.locked ? 1 : 0);
}
std::string Command(std::istream& in)
{
    std::string what;
    in >> what;
    if (what == "off")
    {
        g_testStall = false;
        return "watchdog test off";
    }
    if (what != "test")
        return std::string("watchdog ") + (flightio::Get().maneuverWatchdog ? "on" : "off") + (g_testStall ? ", test stall" : "");
    float best = 1e12f;
    u32 to = 0;
    for (const space::Contact& c : space::Contacts())
        if (c.dock && !c.ship && c.distance > 500 && c.distance < best)
            best = c.distance, to = c.id;
    if (!to || !space::SetTarget(to))
        return "watchdog test: no station to go to";
    const bool ok = ai::Maneuver(ai::OpGoto, false);
    g_testStall = ok;
    return std::string("watchdog test: goto ") + std::to_string(to) + (ok ? " accepted" : " refused");
}
} // namespace mwatch
