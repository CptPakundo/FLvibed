// Advanced Autopilot - `ash_ride` (docs\research\runners\M02_ashcroft_datapass.md "The ONE proposal", adapted to its P0
// fact and the owner's correction of 31 Oct 2026: an engine kill out of cruise coasts straight on the velocity, cruise is
// lost and the engine back on needs the 8.5 s charge). The adapter over the pure half ash_ride.h:
//  0 armed   - the stage's fight on the last ship (the decision: AshRideArm -> the stage verdict "fight") until its shield
//              is down and its hull <= 0.6 (ashride::Leave) -> `ash-ride leave-escort`, `ash-ride loop`;
//  1 loop    - a cruise loop round a centre behind the spawn on his line, inside every leash (ashride::PlanLoop /
//              LoopCarrot), the combat on the escort left running underneath (weapons offline at cruise); back to the
//              fight when the escort lives 35 s on or a leash is about to fire (ashride::LeftFallback);
//  2 intercept - his contact chased (g_obj.chargeId): wait on the loop while early, then at cruise to the station behind
//              him he reaches cruise at; beside / ahead: pass and turn at cruise (ashride::Intercept);
//              The coasts themselves are ChaseCoast's, started only by this ride's gate (AshRideCoast); between them
//              (the engine back on, the 8.5 s charge) the intercept steers again at cruise;
//  3 done    - he is gone or at his run point (or no spawn seen): the chase as spawn_charge's.
// Never an input that leaves cruise: the guard wants cruise on in every frame it flies.
#include "autopilot_impl.h"
#include "ash_ride.h"
#include "autopilot_ashride.h"
#include "game/mprules.h"

namespace autopilot
{
namespace
{
struct Ride
{
    std::string trigger;      // the stage the ride belongs to ("" none)
    u32 escort = 0;           // the last ship
    float spawn[3] = {}, run[3] = {};
    std::vector<lookahead::StageLeash> leashes;
    bool refused = false;     // the loop did not fit: spawn_charge as is
    bool off = false;         // fallen back: the fight on the escort
    int phase = 0;            // 0 armed, 1 loop, 2 intercept, 3 done
    ashride::Loop loop;
    int dir = 0;
    ULONGLONG leftAt = 0, escortDeadAt = 0, hisAt = 0, lastLeave = 0;
    u32 his = 0;
    bool committed = false;
    int verdict = -1;         // the intercept's last verdict (logged on change) and when
    ULONGLONG verdictAt = 0;
    ashride::CoastGate gate;
    bool coastSaid = false;
    bool pausedSaid = false;
} s_ride;

float NowS() { return static_cast<float>(gameclock::NowMs64()) / 1000.0f; }
float SinceS(ULONGLONG at) { return at ? static_cast<float>(gameclock::NowMs64() - at) / 1000.0f : 0.0f; }

bool Switches()
{
    return g_set.ashRide && g_set.spawnCharge && g_set.spawnStage && g_set.chaseKill && Knows(Source::Hidden);
}

float TurnRadius()
{
    flight::Model fm;
    const bool ok = flight::ReadModel(fm);
    return ashride::CruiseTurnRadius(ok && fm.cruiseSpeed > 0 ? fm.cruiseSpeed : 300.0f, ok ? fm.turnRate : 0.0f);
}

// the escort's shield fraction (-1 unknown) and hull (-1 unknown)
void EscortBars(u32 id, float& shield, float& hull)
{
    shield = hull = -1;
    const space::Contact c = FindContact(id);
    if (!c.id)
        return;
    hull = c.health;
    float cur = 0, top = 0;
    bool up = false;
    if (space::ShieldOf(id, cur, top, up))
        shield = !up ? 0.0f : top > 0 ? cur / top : -1.0f;
}

// fly to `p` at cruise: no input that leaves or blocks the cruise (no afterburner, reverse, brake, slide, engine kill)
void Fly(Out& out, const float p[3], float fullAngle)
{
    SteerTo(out, p, 1.0f, fullAngle, 1);
    out.pursue = out.station = out.brake = out.steerReverse = out.steerAhead = out.gunAim = false;
    out.afterburn = false;
    out.slide = out.dodgeSlide = out.turnManeuver = 0;
    out.weave = 0;
    out.missile = out.torpedo = out.mine = false;
    out.fire = false;
    out.engineKill = false;
    out.coast = false;
}

void Reset(const char* why)
{
    if (s_ride.phase == 1 || s_ride.phase == 2)
        events::Push("autopilot ash-ride end (%s) phase=%d", why, s_ride.phase);
    s_ride = Ride();
}
} // namespace

bool AshRideArm(const std::string& trigger, u32 escort, const float spawn[3], const float run[3],
                const std::vector<lookahead::StageLeash>& leashes)
{
    if (!Switches() || trigger.empty())
        return false;
    if (s_ride.trigger != trigger)
    {
        Reset("new stage");
        s_ride.trigger = trigger;
        memcpy(s_ride.spawn, spawn, sizeof(s_ride.spawn));
        memcpy(s_ride.run, run, sizeof(s_ride.run));
        s_ride.leashes = leashes;
        const float r = ashride::LoopRadius(TurnRadius());
        const ashride::Loop lp = ashride::PlanLoop(spawn, run, r, leashes);
        if (!lp.ok)
        {
            s_ride.refused = true;
            events::Push("autopilot ash-ride none (%s) r=%.0f leashes=%u -> spawn_charge", lp.why.c_str(), r,
                         static_cast<unsigned>(leashes.size()));
            return false;
        }
        s_ride.loop = lp;
        events::Push("autopilot ash-ride armed %s escort=%u loop center=(%.0f,%.0f) r=%.0f d_spawn=%.0f farthest=%.0f %s "
                     "bound=%s -> fight it to shield 0 / hull <= %.2f",
                     trigger.c_str(), escort, lp.c[0], lp.c[2], lp.r, lp.dSpawn, lp.farthest, lp.why.c_str(),
                     lp.bound.empty() ? "-" : lp.bound.c_str(), ashride::kLeaveHull);
    }
    if (s_ride.refused)
        return false;
    s_ride.escort = escort;
    if (s_ride.phase <= 1)
        s_ride.leashes = leashes; // (the anchors move: King fights - refreshed each look while the nag holds)
    return true;
}

bool AshRideStage(const std::string& trigger)
{
    return Switches() && !trigger.empty() && s_ride.trigger == trigger && !s_ride.refused;
}

bool AshRideFlying() { return Switches() && !s_ride.off && (s_ride.phase == 1 || s_ride.phase == 2) && !CoastActive(); }

bool AshRideCoast(u32 id, float gap, float hisSpeed, const float hisVel[3], float mySpeed, const float myVel[3],
                  const float toHim[3], bool cruising, bool shieldOut, bool* start)
{
    if (!Switches() || s_ride.off || s_ride.phase < 2 || !id || id != s_ride.his)
        return false;
    ashride::CoastFacts f;
    f.now = NowS(), f.gap = gap, f.his = hisSpeed, f.me = mySpeed, f.cruising = cruising, f.shieldOut = shieldOut;
    f.velDeg = ashride::AngleDeg(myVel, toHim), f.parallelDeg = ashride::AngleDeg(myVel, hisVel);
    float held = 0;
    *start = ashride::CoastReady(s_ride.gate, f, &held);
    if (*start)
        events::Push("autopilot ash-ride coast gap=%.0f his=%.0f me=%.0f vel_off=%.1f parallel=%.1f stable=%.1f s", gap,
                     hisSpeed, mySpeed, f.velDeg, f.parallelDeg, held);
    return true;
}

void AshRideFrame(Out& out)
{
    if (s_ride.trigger.empty())
        return;
    if (!Switches() || !ObjectiveOn() || mprules::Autopilot() != mprules::Allow::Full)
    {
        Reset("objective off");
        return;
    }
    if (s_ride.refused || s_ride.off || s_ride.phase == 3)
        return;
    fl::Kinematics k = {};
    if (!fl::PlayerKinematics(k))
        return;
    const float speed = Len3(k.vel);
    flight::Model fm;
    const bool haveFm = flight::ReadModel(fm);
    const bool cruising = haveFm && fm.cruiseEngaged;
    const ULONGLONG now = gameclock::NowMs64();

    // 0 armed: the stage's fight on the escort - the leave rule (10 Hz)
    if (s_ride.phase == 0)
    {
        if (g_obj.stageVerdict != 2 || g_obj.stageTrigger != s_ride.trigger || g.mode != Mode::Combat ||
            now - s_ride.lastLeave < 100)
            return;
        s_ride.lastLeave = now;
        float shield = -1, hull = -1;
        EscortBars(s_ride.escort, shield, hull);
        if (!ashride::Leave(shield, hull))
            return;
        // (the loop again with the leashes as they are now)
        ashride::Loop lp = ashride::PlanLoop(s_ride.spawn, s_ride.run, s_ride.loop.r, s_ride.leashes);
        if (!lp.ok)
        {
            s_ride.off = true;
            events::Push("autopilot ash-ride fallback (%s) -> fight escort=%u", lp.why.c_str(), s_ride.escort);
            return;
        }
        s_ride.loop = lp;
        s_ride.dir = ashride::LoopDir(k.pos, k.vel, lp.c);
        s_ride.phase = 1, s_ride.leftAt = now;
        const float ds[3] = {s_ride.spawn[0] - k.pos[0], s_ride.spawn[1] - k.pos[1], s_ride.spawn[2] - k.pos[2]};
        const float dc[3] = {lp.c[0] - k.pos[0], lp.c[1] - k.pos[1], lp.c[2] - k.pos[2]};
        events::Push("autopilot ash-ride leave-escort hull=%.2f shield=%.2f escort=%u d_spawn=%.0f me=%.0f cruise=%d",
                     hull, shield, s_ride.escort, Len3(ds), speed, cruising ? 1 : 0);
        events::Push("autopilot ash-ride loop center=(%.0f,%.0f) r=%.0f d_spawn=%.0f farthest=%.0f dir=%+d d_center=%.0f "
                     "%s bound=%s",
                     lp.c[0], lp.c[2], lp.r, lp.dSpawn, lp.farthest, s_ride.dir, Len3(dc), lp.why.c_str(),
                     lp.bound.empty() ? "-" : lp.bound.c_str());
    }

    // his contact chased (the chase matched the staged ship): the intercept
    if (s_ride.phase == 1 && g_obj.chargeId && ChargeFleeing(g_obj.chargeId))
    {
        s_ride.phase = 2, s_ride.his = g_obj.chargeId, s_ride.hisAt = now, s_ride.verdict = -1;
        const space::Contact hc = FindContact(s_ride.his);
        const float ds[3] = {s_ride.spawn[0] - k.pos[0], s_ride.spawn[1] - k.pos[1], s_ride.spawn[2] - k.pos[2]};
        events::Push("autopilot ash-ride spawn his=%u d=%.0f d_spawn=%.0f me=%.0f cruise=%d since_leave=%.1f s", s_ride.his,
                     hc.distance, Len3(ds), speed, cruising ? 1 : 0, SinceS(s_ride.leftAt));
    }

    if (s_ride.phase == 1)
    {
        const space::Contact ec = FindContact(s_ride.escort);
        const bool alive = ec.id && ec.health != 0;
        if (!alive && !s_ride.escortDeadAt)
            s_ride.escortDeadAt = now;
        const ashride::FallbackResult fb = ashride::LeftFallback(SinceS(s_ride.leftAt), alive, k.pos, k.vel, s_ride.leashes);
        if (fb.fall)
        {
            s_ride.off = true;
            events::Push("autopilot ash-ride fallback (%s) -> fight escort=%u hull=%.2f", fb.why.c_str(), s_ride.escort,
                         ec.health);
            return;
        }
        if (s_ride.escortDeadAt && SinceS(s_ride.escortDeadAt) > ashride::kSpawnWaitS)
        {
            s_ride.phase = 3;
            events::Push("autopilot ash-ride fallback (no spawn seen %.0f s after the escort) -> spawn_charge",
                         ashride::kSpawnWaitS);
            return;
        }
        // (the combat on the escort - or the step's trip in the moment the step changes - flies underneath)
        if (g.mode != Mode::Combat && g.mode != Mode::Travel)
        {
            if (!s_ride.pausedSaid)
                events::Push("autopilot ash-ride paused mode=%s", ModeName(g.mode));
            s_ride.pausedSaid = true;
            return;
        }
        s_ride.pausedSaid = false;
        float car[3];
        ashride::LoopCarrot(k.pos, s_ride.loop, s_ride.dir, speed, car);
        Fly(out, car, ashride::kLoopFullAngle);
        return;
    }

    if (s_ride.phase != 2)
        return;
    // (the coast flies the ship - a straight line, the nose on him; after it, with the engine back on and the charge
    // running, the intercept steers again at cruise: the combat's chase would drop the cruise into its pursuit while this
    // ride's gate holds the next coast back)
    if (CoastActive())
        return;
    const space::Contact hc = FindContact(s_ride.his);
    if (!hc.id || hc.health == 0 || !ChargeFleeing(s_ride.his))
    {
        s_ride.phase = 3;
        events::Push("autopilot ash-ride end (%s)", !hc.id || hc.health == 0 ? "gone" : "not fleeing");
        return;
    }
    if (g.mode != Mode::Combat || g.target != s_ride.his)
        return;
    space::ShipMotion hm;
    const bool haveHm = space::ShipMotionOf(s_ride.his, hm);
    ashride::InterceptFacts f;
    memcpy(f.me, k.pos, sizeof(f.me));
    memcpy(f.vel, k.vel, sizeof(f.vel));
    f.mySpeed = speed;
    f.cruising = cruising;
    // (the charge still to run: engaged -> its time left; not engaged -> the whole charge, the owner's 8.5 s)
    f.chargeLeftS = !haveFm                   ? 0.0f
                    : !fm.cruiseEngaged        ? (fm.cruiseChargeTime > 0 ? fm.cruiseChargeTime : lookahead::kOurCharge.slowS)
                    : fm.cruiseChargeElapsed >= 0 && fm.cruiseChargeElapsed < fm.cruiseChargeTime
                        ? fm.cruiseChargeTime - fm.cruiseChargeElapsed
                        : 0.0f;
    memcpy(f.his, hc.pos, sizeof(f.his));
    f.hisSpeed = haveHm ? hm.speed : 0.0f;
    float u[3] = {s_ride.run[0] - s_ride.spawn[0], 0, s_ride.run[2] - s_ride.spawn[2]};
    const float ul = std::max(1.0f, Len3(u));
    for (float& x : u)
        x /= ul;
    memcpy(f.u, u, sizeof(f.u));
    f.tSince = SinceS(s_ride.hisAt);
    f.turnR = TurnRadius();
    f.committed = s_ride.committed;
    const ashride::InterceptResult r = ashride::Intercept(f);
    if (r.v != ashride::Verdict::Wait)
        s_ride.committed = true;
    // (on a change, at most 4 a second: a boundary flicker must not flood the log)
    if (static_cast<int>(r.v) != s_ride.verdict && now - s_ride.verdictAt >= 250)
    {
        s_ride.verdict = static_cast<int>(r.v), s_ride.verdictAt = now;
        events::Push("autopilot ash-ride intercept %s (%s) t=%.1f his=%.0f me=%.0f cruise=%d charge_left=%.1f gap=%.0f "
                     "along=%.0f lat=%.0f path=%.0f need=%.0f t_ready=%.1f station=(%.0f,%.0f)",
                     ashride::VerdictName(r.v), r.why, f.tSince, f.hisSpeed, speed, cruising ? 1 : 0, f.chargeLeftS,
                     hc.distance, r.along, r.lat, r.path, r.need, r.tReady, r.station[0], r.station[2]);
    }
    if (r.v == ashride::Verdict::Wait)
    {
        float car[3];
        ashride::LoopCarrot(k.pos, s_ride.loop, s_ride.dir, speed, car);
        Fly(out, car, ashride::kLoopFullAngle);
        return;
    }
    Fly(out, r.steer, ashride::kSteerFullAngle);
}
} // namespace autopilot
