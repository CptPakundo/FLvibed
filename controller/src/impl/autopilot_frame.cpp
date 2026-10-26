// Advanced Autopilot - frame: Update, the per-frame supervisor of every mode. Part of autopilot.cpp until 21 Oct 2026; shared state and declarations in
// autopilot_impl.h, the module API in autopilot.h.
#include "autopilot_impl.h"

namespace autopilot
{
// ------------------------------------------------------------------ frame
// A dock port held while we requested none (our runs and the dock mode set fastRequested / started): a mission's own
// Act_DockRequest flies the ship (M12's core exit after the power cell, 22 Oct 2026: the objective mode restarted a
// travel 0.15 s after the game granted it, the request died, and the ship sat in the core until the jump hole's 600 s
// timer failed the mission). ai::MissionHolds did not read it as a hold.
// The server's grant comes first, the dock port a moment later (M13 22 Oct 2026: the sphere airlock's Act_DockRequest
// granted at 07:42:01.56, our new decision 0.4 s later replaced it - the port was not held yet): a grant we did not ask
// for (host API v14 DockReply: kind 0, response 4) counts for 4 s.
static ULONGLONG grantAt = 0; // the last dock grant we did not request (GameDocksUs)
static u32 grantSystem = 0;   // ...and the system it came in: a system switch since (an airlock's transit) used it up

// (session 5, 22 Oct 2026, M12: the lair airlock's grant still counted after the transit through it - the objective
// mode took the airlock 16 m behind the ship for "the way in", found no way and fell back to a dock request there, the
// lockout; 6 min stalled, the jump hole collapsed)
ULONGLONG SinceGameGrant()
{
    if (grantAt && grantSystem && fl::PlayerSystem() != grantSystem)
        grantAt = 0;
    return grantAt ? GetTickCount64() - grantAt : ~0ULL;
}

// The first position seen in the current system (kept by Update every frame): the way in, which a mission's grant of
// "the" airlock never means (session 8, 23 Oct 2026, M12 core: the exit's grant after the power cell, the nearest airlock
// within 3 km was the entry the ship came in by - two refused requests, the run stalled)
static u32 entrySystem = 0;
static float entryPos[3] = {};
static bool entrySeen = false; // a system switch seen (the first system's first position is only where we started)

static ULONGLONG entryAt = 0;

static void TrackSystemEntry()
{
    const u32 sys = fl::PlayerSystem();
    fl::Kinematics k = {};
    if (!sys || !fl::PlayerKinematics(k))
        return;
    if (sys != entrySystem)
        entrySeen = entrySystem != 0, entrySystem = sys, entryAt = GetTickCount64();
    // (the first second after the switch: a jump's first frames still show the old system's coordinates)
    if (GetTickCount64() - entryAt < 1000)
        memcpy(entryPos, k.pos, sizeof(entryPos));
}

// When the player's system last changed (GetTickCount64; 0 none seen since the logic started): a jump-in whatever flew it
// (the M12 hole is entered by the objective's jump, not the travel mode - its jump-in tick never set)
ULONGLONG SystemEntryTick() { return entrySeen ? entryAt : 0; }

bool SystemEntry(float pos[3])
{
    if (!entrySeen || entrySystem != fl::PlayerSystem())
        return false;
    memcpy(pos, entryPos, sizeof(entryPos));
    return true;
}

bool GameDocksUs()
{
    const bool ours = g.dock.fastRequested || (g.mode == Mode::Dock && g.started);
    static u32 seen = 0;
    static bool primed = false; // replies from before the first look are history
    u32 ship = 0, kind = 0, response = 0;
    const u32 seq = HostDockReply(&ship, &kind, &response);
    if (!primed)
        primed = true, seen = seq;
    if (seq != seen)
    {
        seen = seq;
        if (kind == 0 && response == 4 && !ours)
            grantAt = GetTickCount64(), grantSystem = fl::PlayerSystem();
    }
    if (ours)
        return false;
    // ...and for as long as the ship's own behaviour is a Dock we did not start (M12 core exit: the grant's 4 s ran out
    // before the dock port was held, our travel took over again and the ship died in the core's radiation)
    ai::State s;
    const bool granted = SinceGameGrant() != ~0ULL; // (cleared by a system switch since the grant)
    // ...but a grant at an airlock is flown through at once (airlock_fly_through: the game's own Dock there letterboxes
    // the ship for good) - waiting cost the 4 s after the power cell (session 7, 23 Oct 2026, the user: "after the tractor
    // completed its operation the autopilot stalled")
    if (granted && g_set.airlockFlyThrough && g_obj.on && !ai::DockInProgress())
        for (const space::Contact& c : Contacts())
            if ((c.type & space::kAirlockGate) && c.distance < 3000)
                return false;
    const bool dockOp = granted && ai::ReadState(s) && s.op == ai::OpDock;
    return ai::DockInProgress() || dockOp || (granted && SinceGameGrant() < kGameGrantDockMs);
}

static void UpdateModes(float dt, Out& out)
{
    out = Out();
    ObjectiveSupervise();
    if (g.mode == Mode::Off)
        return;
    if (GameDocksUs())
    {
        // hand over without cancelling the ship's directive: that IS the mission's dock (M13 22 Oct 2026: the cancel of
        // a plain Stop ended the sphere airlock's granted dock, the ship stayed outside)
        StopImpl(StopReason::GameDocks, false);
        return;
    }
    if (ai::MissionHolds())
    {
        // The game's docking sequence after our own dock request locks the ship too: that is the dock going through,
        // not a mission taking over (was: travel to Manhattan's ring stopped "mission controls the ship" one frame
        // before the docking cutscene, and the harness read a failure, 19 Oct 2026). Our request is the sign: the dock
        // port no longer reads as held at that moment (the second run of 19 Oct).
        const bool travelDock = g.mode == Mode::Travel && g.dock.fastRequested && g.tr.step < g.tr.plan.steps.size() &&
                                g.tr.plan.steps[g.tr.step].kind == route::Kind::Dock;
        const bool ourDock = travelDock || (g.mode == Mode::Dock && g.started);
        Stop(ourDock ? StopReason::Docking : StopReason::MissionControls);
        return;
    }
    // Our ship destroyed: stop before anything reads the fight (was: on the death frame the target vanished with our
    // ship and counted as a kill, nanobots were tapped at hull 0 and the stop said "fled", 18 Oct 2026).
    const float health = ai::PlayerHealth();
    if (health >= 0 && health <= 0.0005f)
    {
        // What was left went in the killing blow (was: never counted - a death read lost 0.53, 18 Oct 2026).
        if (g.combat.totalHp > 0 && !g.combat.hp.empty())
            g.combat.lost += g.combat.hp.back().second / g.combat.totalHp;
        StopImpl(StopReason::PlayerDied, false);
        return;
    }
    g.time += dt;
    ++g.frames;
    g.tacticTime += dt;
    out.active = true;
    const int op = Op();
    Countermeasures(dt, out);
    // Every homing round that leaves, with the frames since our last missile tap (a round with no tap of ours, or
    // several per tap, shows here; was: one launch decision, 3 rounds gone, M02 18 Oct 2026).
    {
        int ammo = 0;
        for (const space::Weapon& w : space::PlayerWeapons())
            if (w.homing && w.ammo > 0)
                ammo += w.ammo;
        if (g.missiles.ammo >= 0 && ammo < g.missiles.ammo)
            events::Push("autopilot missile-round-left n=%d left=%d frames-since-tap=%u", g.missiles.ammo - ammo, ammo,
                         g.missiles.tapFrame ? g.frames - g.missiles.tapFrame : 0);
        g.missiles.ammo = ammo;
    }
    TrackOwnMissiles();
    // Guns: count shots (refire timer restarts) and, while we wanted fire, the shots possible.
    for (const space::Weapon& w : space::PlayerWeapons())
    {
        if (w.homing || w.ammo >= 0 || w.refire <= 0)
            continue;
        const auto it = g.stats.gunElapsed.find(w.subObj);
        if (it != g.stats.gunElapsed.end() && w.elapsed < it->second)
        {
            ++g.stats.gunShots;
            g.stats.gunShotDamage += w.hullDamage;
        }
        g.stats.gunElapsed[w.subObj] = w.elapsed;
        if (g.combat.wantFire && w.active)
            g.stats.gunMaxShots += dt / w.refire;
    }

    // Damage and repairs in every mode (a fight does them inside UpdateCombat): hold, travel, follow, dock, a rejoining
    // escort - the situation they read (attackers on us) is assessed here twice a second.
    if (!(g.mode == Mode::Combat || (g.mode == Mode::Escort && g.tactic != Tactic::Rejoin)))
    {
        ai::Self self;
        if (ai::ReadSelf(self))
        {
            static float sitTimer = 0;
            sitTimer -= dt;
            if (sitTimer <= 0)
            {
                sitTimer = 0.5f;
                g_sit = Assess();
            }
            DamageAndRepairs(dt, out, self);
        }
    }

    switch (g.mode)
    {
    case Mode::Goto:
    case Mode::Dock:
    case Mode::Formation:
    case Mode::Face:
    {
        // Fast dock: the player-style approach first (it hands over to the game's dock with its request).
        if (g.mode == Mode::Dock && g.dock.fastRequested && g.dock.fastLined)
            QueuedShortcut();
        if (g.mode == Mode::Dock && (g_set.fastDock || g.dock.fastForced) && !g.hasPos && !g.started &&
            FastApproach(dt, out))
            break;
        if (!g.started)
        {
            if (g.hasPos)
            {
                ai::Directive d;
                d.op = ai::OpGoto;
                d.hasPos = true;
                memcpy(d.pos, g.pos, sizeof(g.pos));
                d.range = 150;
                SubmitNow(d);
            }
            else
            {
                if (g.target)
                    space::SetTarget(g.target);
                const int want = g.mode == Mode::Goto        ? ai::OpGoto
                                 : g.mode == Mode::Dock      ? ai::OpDock
                                 : g.mode == Mode::Formation ? ai::OpFormation
                                                             : ai::OpFace;
                MarkSubmit();
                ai::Maneuver(want, false);
            }
            g.started = true;
            break;
        }
        // The game ends the maneuver itself (arrival, refused): hand back without touching it, once it has acted on our
        // request (frames, not a 1 s wait). A dock goes on through moments of free flight - queued at the port, a lane
        // ride, a jump - which the game reports: it ends when docked (controller mode change) or when free flight comes
        // with no docking port, lane or jump in progress (was: 0.5 s / 5 s of free flight, 18 Oct 2026).
        if (op == ai::OpFreeFlight && g.mode != Mode::Face && SubmitSettled())
        {
            bool dockGoesOn = false;
            if (g.mode == Mode::Dock)
            {
                space::Pilot p;
                dockGoesOn = ai::DockInProgress() || (space::ReadPilot(p) && (p.lane || p.jumping));
            }
            if (!dockGoesOn)
                StopImpl(StopReason::ManeuverEnded, false);
        }
        break;
    }
    case Mode::Follow:
        // The leader is checked while the game's follow runs too (was: only when it had ended - a follow whose
        // leader died may run on headless, 18 Oct 2026).
        // The leader's existence is polled twice a second (a scan of the system's objects); the follow is issued again
        // whenever the game is not running it and has acted on our last request (state, was a 2 s timer).
        g.issueTimer -= dt;
        if (g.issueTimer <= 0)
        {
            g.issueTimer = 0.5f; // polling interval of the leader check
            if (!FindContact(g.target).id)
            {
                Stop(StopReason::LeaderGone);
                return;
            }
        }
        if (op != ai::OpFollow && SubmitSettled())
            Follow(g.target);
        break;
    case Mode::Escort:
        if (g.tactic == Tactic::Rejoin)
        {
            if (!FindContact(g.escorted).id)
            {
                Stop(StopReason::EscortedGone);
                return;
            }
            if (op != ai::OpFollow && SubmitSettled())
                Follow(g.escorted);
            // Look out for hostiles near the escorted ship (attackers of it first: PickTarget), then fight them.
            g.combat.seekTimer -= dt;
            if (g.combat.seekTimer <= 0)
            {
                g.combat.seekTimer = 0.5f;
                if (const u32 pick = PickTarget())
                {
                    g.target = pick;
                    g.combat.pickTimer = 0.5f;
                    // A new fight: its "no enemy" time starts over (was: still past the 4 s of the last one, so a
                    // target not selected on its first frame sent the escort straight back to Rejoin, 18 Oct 2026).
                    g.combat.lostEnemy = false;
                    g.combat.lootTime = 0;
                    events::Push("autopilot target id=%u", pick);
                    SetTactic(Tactic::Engage);
                    break;
                }
            }
            if (fl::TargetId() != g.escorted)
                space::SetTarget(g.escorted); // the escorted ship stays selected while rejoining
            break;
        }
        UpdateCombat(dt, out);
        if (g.mode == Mode::Escort && g.tactic == Tactic::Rejoin)
        {
            g.target = 0;
            space::SetTarget(g.escorted);
        }
        break;
    case Mode::Combat:
        // Disable, not destroy (missions: "Disable Enemy's Engines", M02 Cosmo at 50 %): the chosen target at or below
        // disable_at of its hull ends the fight at once, checked every frame (a harness poll would overshoot).
        if (g_set.disableAt > 0 && g.combat.chosen && g.target)
        {
            const space::Contact c = FindContact(g.target);
            if (c.id && c.health >= 0 && c.health <= g_set.disableAt)
            {
                events::Push("autopilot target-disabled id=%u health=%.2f", c.id, c.health);
                Stop(StopReason::TargetDisabled);
                break;
            }
        }
        UpdateCombat(dt, out);
        break;
    case Mode::Shoot: UpdateShoot(dt, out); break;
    case Mode::Travel: UpdateTravel(dt, out); break;
    case Mode::Hold: UpdateHold(dt, out, op); break;
    case Mode::Off: break;
    }
}

// Close-quarter flight (user, 22 Oct 2026: "a lot of crashing into geometry when flying from point to point" in M12 /
// M13; "the player is required to make close quarter flight decisions that are highly aware of nearby geometry" - the
// 2020 playthrough flies the lair's trench): over every mode's steering, the way the ship is moving is tested against
// the real hulls (avoid::RayFan, the game's ray test) out to its stopping distance + 0.8 s + 40 m; blocked = steer along
// the clear direction nearest the goal (the steer point, the station point, the target), no afterburner or cruise,
// reverse when even that is not clear beyond the stopping distance. The game's own sequences (an exclusion set, our dock
// request out) and slow flight (< 20 m/s) are left alone. The thing being entered is no obstacle: a dock / jump step's
// object, the fast-dock target; the pursuit's target neither (the pursuit brakes for it by the closing speed).
static bool g_guardBlocked = false; // (the motion trace's view of the guard)
static float g_guardClear = -1;

static void GeometryGuard(float dt, Out& out)
{
    static float nextAt = 0, clock = 0;
    static bool blocked = false;
    static avoid::FanResult fan;
    clock += dt;
    fl::Kinematics k = {};
    const bool steering = out.steerTo || out.pursue || out.station;
    // (a wedged fight's break-out is its own ray-chosen move: the guard braking it kept the ship pinned, session 7)
    if (!g_set.geometryGuard || !out.active || !steering || out.brake || g.dock.fastRequested ||
        g.combat.breakUntil > g.time || space::PlayerControlExclusion() != 0 || !fl::PlayerKinematics(k))
    {
        g_guardClear = -1;
        blocked = false;
        g.avoid.wallSlide = 0;
        return;
    }
    const float speed = Len3(k.vel);
    if (speed < 20)
    {
        if (blocked)
            events::Push("autopilot geometry-guard clear (slow)");
        blocked = false;
        g.avoid.wallSlide = 0;
        return;
    }
    flight::Model fm;
    const bool haveModel = flight::ReadModel(fm);
    const float stop = haveModel ? flight::BrakeDistance(fm, speed, 0) : speed * 1.5f;
    const float look = Clampf(stop + speed * 0.8f + 40.0f, 80.0f, 1500.0f);
    if (clock >= nextAt)
    {
        nextAt = clock + 0.1f;
        u32 ignore[3] = {};
        int n = 0;
        // a rotating part being crossed by its timing (ArriveAt's wheel): the timing's, not the guard's
        if (g.arrive.wheelFrame == g.frames && g.arrive.wheelPart)
            ignore[n++] = g.arrive.wheelPart;
        // the target: exempt for the lead pursuit (it brakes for the target by the closing speed) and for a dock / jump;
        // a station run or a steer run in a fight treats its hull as a wall (M10: flown into an experimental ship)
        const bool fight = g.mode == Mode::Combat || g.mode == Mode::Escort;
        if (g.target && !(fight && (out.station || out.steerTo)))
            ignore[n++] = g.target;
        if (g.mode == Mode::Travel && g.tr.step < g.tr.plan.steps.size())
        {
            const route::Step& s = g.tr.plan.steps[g.tr.step];
            if ((s.kind == route::Kind::Dock || s.kind == route::Kind::Jump) && s.object && s.object != g.target)
                ignore[n++] = s.object;
        }
        // the motion first: clear along the velocity = nothing to do
        const float vdir[3] = {k.vel[0] / speed, k.vel[1] / speed, k.vel[2] / speed};
        const avoid::FanResult mv = avoid::RayFan(k.pos, vdir, look, space::PlayerRadius(), ignore, n);
        g_guardClear = mv.blocked ? mv.clear : look;
        bool now = false;
        if (mv.blocked)
        {
            // the goal's direction, then the fan round it
            float goal[3] = {vdir[0], vdir[1], vdir[2]};
            const float* to = out.steerTo ? out.steerPos : out.station ? out.stationPos : nullptr;
            space::Contact tc;
            if (!to && g.target && (tc = FindContact(g.target)).id)
                to = tc.pos;
            if (to)
                for (int i = 0; i < 3; ++i)
                    goal[i] = to[i] - k.pos[i];
            fan = avoid::RayFan(k.pos, goal, look, space::PlayerRadius(), ignore, n);
            now =
                fan.blocked || mv.clear < stop + speed * 0.3f; // a clear goal while the motion is still headed in: turn
            if (!fan.blocked)
                fan.brake = mv.clear < stop;
            // A planned way being flown (GeoPathAim this frame; session 7, 23 Oct 2026, M12 trench hops at 270-300 m/s:
            // the velocity of a curving line points at the bend's outer wall - blocked, braked, the cruise lost twice in one
            // hop): the planned leg to the aim decides - clear for the ship (LineClear) and the motion not about to hit
            // before the ship can stop = not blocked; the plan's berth keeps it off the walls
            // (...and the motion safe for now - clear beyond the stop + 0.3 s - is enough: s9z-i2, M12 hop gen 2 -> gen 1 at
            // 287 m/s, the line to the way's next corner was blocked while the motion was clear (hit = look, 330 m); the
            // guard's swing put the ship 250 m off the way, the hop re-planned onto a 6-leg way and dropped the cruise for
            // 200 m/s, ~7 s; off-the-way re-plans in ~2 of 3 recent runs. The way's legs are tested for the ship's width)
            if (now && g.avoid.pathAimFrame == g.frames && mv.clear >= stop + speed * 0.3f)
                now = false, fan.brake = false;
            else if (now && g.avoid.pathAimFrame == g.frames && mv.clear >= stop &&
                     LineClear(k.pos, g.avoid.pathAim, space::PlayerRadius()))
                now = false, fan.brake = false;
            // An airlock run lined up on the door's axis (s9k-i2 / i3, M12 core exit: on the axis at 300 m/s, 10-20 m off
            // it, the swept rays met the ring round the exit door's mouth; the guard's full-rate swing put the ship ~180 m
            // off the axis, the run then missed the door): the door's own line is the way in - within 40 m of the axis,
            // the motion within 12 deg of it and the motion not about to hit before the ship can stop
            if (now && g.dock.fastKind == DockKind::Airlock && mv.clear >= stop)
            {
                const float rp[3] = {k.pos[0] - g.dock.bayP2[0], k.pos[1] - g.dock.bayP2[1],
                                     k.pos[2] - g.dock.bayP2[2]};
                const float al = Dot3(rp, g.dock.bayAxis);
                const float lat3[3] = {rp[0] - g.dock.bayAxis[0] * al, rp[1] - g.dock.bayAxis[1] * al,
                                       rp[2] - g.dock.bayAxis[2] * al};
                if (al > 0 && Len3(lat3) < 40.0f && -Dot3(vdir, g.dock.bayAxis) > 0.978f)
                    now = false, fan.brake = false;
            }
        }
        if (now != blocked)
            events::Push(now ? "autopilot geometry-guard blocked hit=%.0f look=%.0f stop=%.0f speed=%.0f clear=%.0f "
                               "brake=%d rays=%d"
                             : "autopilot geometry-guard clear",
                         mv.clear, look, stop, speed, fan.clear, fan.brake ? 1 : 0, mv.rays + fan.rays);
        blocked = now;
        g_guardBlocked = now;
        // The walls beside the way (the 2020 M12 trench, 4:28-4:41 at 200 m/s: straight down the middle, small sideways
        // corrections when a wall or beam came close): rays across the motion, 80 m right / left / up / down in the
        // ship's frame; one closer than 35 m (the other side clear) = slide away from it, the player's strafe keys.
        g.avoid.wallSlide = 0;
        static const struct
        {
            int axis; // the ship's local axis: 0 right, 1 up
            float sign;
            int slideAway; // Out::slide away from that wall: 1 left, 2 right, 3 up, 4 down
        } kSides[] = {{0, 1, 1}, {0, -1, 2}, {1, 1, 4}, {1, -1, 3}};
        float near[4];
        for (int s = 0; s < 4; ++s)
        {
            const int c = kSides[s].axis;
            const float to[3] = {k.pos[0] + kSides[s].sign * k.rot[0 * 3 + c] * 80.0f,
                                 k.pos[1] + kSides[s].sign * k.rot[1 * 3 + c] * 80.0f,
                                 k.pos[2] + kSides[s].sign * k.rot[2 * 3 + c] * 80.0f};
            const float h = space::RayHitDistance(k.pos, to, ignore, n);
            near[s] = h < 0 ? 1e9f : h;
        }
        for (int s = 0; s < 4; ++s)
            if (near[s] < 35.0f && near[s ^ 1] > near[s] + 15.0f && (!g.avoid.wallSlide || near[s] < g.avoid.wallNear))
                g.avoid.wallSlide = kSides[s].slideAway, g.avoid.wallNear = near[s];
    }
    if (g.avoid.wallSlide && !out.dodgeSlide && g.avoid.pathAimFrame != g.frames) // (a planned way keeps its own berth)
        out.slide = g.avoid.wallSlide;
    if (!blocked)
        return;
    out.pursue = false;
    out.station = false;
    out.steerTo = true;
    for (int i = 0; i < 3; ++i)
        out.steerPos[i] = k.pos[i] + fan.dir[i] * look;
    out.steerFullAngle = 0; // full-rate turn
    out.afterburn = false;
    // the cruise ends only when the guard must brake: steering round keeps it (session 6, M12 core: a 0.1 s block
    // cancelled the cruise wish and cost a 5 s charge at 80 m/s; the runners hold cruise round the lair's geometry)
    if (fan.brake)
        out.cruise = 0;
    out.steerThrottle = fan.brake ? 0.0f : 1.0f;
    // (barely moving: turn, do not back off - reverse thrust at 20-50 m/s in front of a wall flipped with the steering
    // and the ship crawled backwards, M12 core checkpoint 23 Oct 2026)
    out.steerReverse = fan.brake && speed > 50.0f;
}

// The motion trace (session 6, 23 Oct 2026; the user watching: "travelling inefficiently when close to some geometry",
// "flew very sub-optimally near the Nomad Base and had some collisions" - the log held only the guard's state changes):
// 10 rows a second while the autopilot flies, FLMotion.tsv beside the log (rewritten each game start; flleg keeps it
// with the leg's evidence). Steering flips, speed dips near hulls and the guard's work are then numbers.
// (out = nullptr: the player flies - TracePlayer; mode -1, action "player", the game's own throttle; the user's own
// runs through a set piece are the reference, 23 Oct 2026)
static void MotionTrace(float dt, const Out* out)
{
    static FILE* f = nullptr;
    static bool opened = false;
    static float acc = 0, flushAcc = 0;
    if (!g_set.motionTrace || (out && !out->active))
        return;
    acc += dt;
    flushAcc += dt;
    if (acc < 0.1f)
        return;
    acc = 0;
    if (!opened)
    {
        opened = true;
        f = _fsopen((DllDir() + "FLMotion.tsv").c_str(), "w",
                    _SH_DENYNO); // (readable while the game runs: flleg copies it)
        if (f)
            fputs("ms\tmode\taction\tspeed\tsteer\tthrottle\tab\tcruise\tslide\tguard\tclear\twall\twall_"
                  "m\ttarget\ttgt_m\t"
                  "align\tx\ty\tz\tengine\tfx\tfy\tfz\tsystem\n",
                  f);
    }
    fl::Kinematics k = {};
    if (!f || !fl::PlayerKinematics(k))
        return;
    const float speed = Len3(k.vel);
    // the nose against the motion: 1 = flying straight ahead (forward = the rotation's third column, negated)
    float fwd[3];
    geom::Forward(fwd, k.rot);
    const float align = speed > 1 ? Dot3(fwd, k.vel) / speed : 0;
    // the engine's real state: 2 cruising (charged), 1 charging, 0 off
    flight::Model fm;
    const int engine = flight::ReadModel(fm) && fm.cruiseEngaged ? (speed > 250 ? 2 : 1) : 0;
    if (!out)
        fprintf(f,
                "%llu\t-1\tplayer\t%.0f\tplayer\t%.2f\t-1\t-1\t0\t0\t-1\t0\t-1\t0\t-1\t%.2f\t%.0f\t%.0f\t%.0f\t%d\t%."
                "3f\t%.3f\t%.3f\t%u\n",
                GetTickCount64(), speed, fl::GetThrottle(), align, k.pos[0], k.pos[1], k.pos[2], engine, fwd[0], fwd[1],
                fwd[2], fl::PlayerSystem());
    else
    {
        const Out& o = *out;
        const char* steer = o.brake ? "brake" : o.station ? "station" : o.pursue ? "pursue" : o.steerTo ? "steer" : "-";
        const space::Contact tc = g.target ? FindContact(g.target) : space::Contact{};
        fprintf(f,
                "%llu\t%d\t%s\t%.0f\t%s\t%.2f\t%d\t%d\t%d\t%d\t%.0f\t%d\t%.0f\t%u\t%.0f\t%.2f\t%.0f\t%.0f\t%.0f\t%d\t%."
                "3f\t%.3f\t%.3f\t%u\n",
                GetTickCount64(), static_cast<int>(g.mode), ObjectiveOn() ? ActionName(g_obj.action) : "-", speed,
                steer, o.steerTo ? o.steerThrottle : -1.0f, o.afterburn ? 1 : 0, o.cruise, o.slide,
                g_guardBlocked ? 1 : 0, g_guardClear, g.avoid.wallSlide, g.avoid.wallSlide ? g.avoid.wallNear : -1.0f,
                g.target, tc.id ? tc.distance : -1.0f, align, k.pos[0], k.pos[1], k.pos[2], engine, fwd[0], fwd[1],
                fwd[2], fl::PlayerSystem());
    }
    if (flushAcc >= 1.0f)
        flushAcc = 0, fflush(f);
}

// Loot the mission marks is tractored in passing, whatever the mode (session 6, 23 Oct 2026; the user's M12 core:
// "tractors the Nomad Power Cell without slowing down"): within the beam's reach, every 1.5 s, the travel's own
// destination rule kept (autopilot_travel.cpp TractorDestination).
static void TractorMarkedLoot(float dt, Out& out)
{
    static float timer = 0;
    timer -= dt;
    if (!out.active || timer > 0 || !ObjectiveOn())
        return;
    timer = 0.5f; // (looked at twice a second; tapped at most every 1.5 s)
    const std::vector<space::Contact> all = Contacts();
    if (DestroyMarkedLoot(all, nullptr))
        return; // (a mark the objective says to destroy: shot, never pulled in - NoTractorOnDestroyMarks)
    u32 marks[32];
    const int n = MissionObjectIds(marks, 32, all);
    ai::Self me;
    const float reach = ai::ReadSelf(me) && me.tractorRange > 0 ? me.tractorRange : 1500.0f;
    for (const space::Contact& c : all)
        if ((c.type & space::kLoot) && c.distance < reach && std::find(marks, marks + n, c.id) != marks + n)
        {
            out.collectAll = true;
            timer = 1.5f;
            events::Push("autopilot tractor marked loot %u at %.0f m", c.id, c.distance);
            return;
        }
}

// Only loot no objective points at to destroy is collected (the owner, 26 Oct 2026, Destroy Contraband): a tractor-all
// (the combat's loot reflex, the travel's destination, the shoot mode's pull, the marked-loot pass) pulls in everything
// within the beam, so it is dropped while a pod the objective says to destroy is within it. After every other rule.
static void NoTractorOnDestroyMarks(Out& out)
{
    if (!out.collectAll)
        return;
    float d = 0;
    const u32 pod = DestroyMarkedLoot(Contacts(), &d);
    ai::Self me;
    const float reach = ai::ReadSelf(me) && me.tractorRange > 0 ? me.tractorRange : 1500.0f;
    if (!pod || d >= reach)
        return;
    out.collectAll = false;
    static Throttle saidAt;
    if (saidAt.Due(g.time, 5))
        events::Push("autopilot tractor withheld: marked loot %u at %.0f m is to be destroyed", pod, d);
}

// The cruise lock (user, 23 Oct 2026: "the ship must not drop cruise speed from the moment it is engaged at the core's
// entrance up to the jump hole after the escape"; the runners and try 3 cruise through the core and out): after every
// other rule - the guard included - the cruise stays wanted and nothing brakes, reverses or burns; steering only. Not in a
// fight or a hold, not while the game docks us, not during the charge fallback (a disruptor's reset: the afterburner).
static void CruiseLock(Out& out)
{
    if (!g_obj.cruiseLock || !g_obj.on || !out.active || g.dock.fastRequested || g.mode == Mode::Combat ||
        g.mode == Mode::Hold || g.mode == Mode::Escort || g.arrive.chargeOffUntil > g.time)
        return;
    out.cruise = 1;
    out.afterburn = false;
    out.brake = false;
    out.steerReverse = false;
    if (out.steerTo && out.steerThrottle < 1.0f)
        out.steerThrottle = 1.0f;
}

// A bump (s9l-i2, M12 core: 300 -> 201 m/s in half a second at z ~-1100 on the axis, the shield 0.87 -> 0.78, nothing in
// the ray test there; runs 5 m apart passed or struck it - something that moves): a speed loss of 40 m/s within 0.2 s
// with no brake or reverse wanted logs the nearest objects, once a second at most.
static void BumpTrace(float dt, const Out& out)
{
    static float prevSpeed = -1, acc = 0;
    static Throttle saidAt;
    fl::Kinematics k = {};
    if (!out.active || !fl::PlayerKinematics(k))
    {
        prevSpeed = -1;
        return;
    }
    acc += dt;
    if (acc < 0.2f)
        return;
    acc = 0;
    const float speed = Len3(k.vel);
    // (the pursuit and station keeping brake by themselves: their full reverse read as bumps - s9m, the gen approaches)
    const bool slowing =
        out.brake || out.steerReverse || out.pursue || out.station || (out.steerTo && out.steerThrottle < 0.5f);
    if (prevSpeed > 0 && prevSpeed - speed > 40.0f && !slowing && saidAt.Due(g.time, 1.0f))
    {
        std::vector<space::Contact> all = Contacts();
        std::sort(all.begin(), all.end(),
                  [](const space::Contact& a, const space::Contact& b) { return a.distance < b.distance; });
        char near[256] = {};
        int len = 0;
        for (size_t i = 0; i < all.size() && i < 4 && len < 200; ++i)
            len += snprintf(near + len, sizeof(near) - len, " %u/0x%X/%s%.0fm", all[i].id, all[i].type,
                            all[i].ship ? "ship/" : "", all[i].distance);
        events::Push("autopilot bump %.0f -> %.0f m/s at (%.0f,%.0f,%.0f) system=%u nearest:%s", prevSpeed, speed,
                     k.pos[0], k.pos[1], k.pos[2], fl::PlayerSystem(), near);
    }
    prevSpeed = speed;
}

// Hull lost to a minefield (the user, 23 Oct 2026: "consider any run through Zone 21 that received hull damage from the
// minefield as a fail state"): the hull falling while the ship is in a mine-danger field with a mine within 150 m ->
// "autopilot minefield-damage", which tools\..\flleg.ps1 counts as a failed leg. 4x a second, once a second at most.
// A mine's blast takes the shield first: hull lost while the shield is up and not falling is the zone's own damage, not
// a mine (s11gate M10: Zone_Rh04_minefield has damage = 200 - the only minefield zone with damage in the game -, the exit
// tunnel drained the hull ~1.7 %/s at a full shield, and a mine 80 m off the tunnel's wall made it a "minefield" fail)
static void MinefieldTrace(float dt, const Out& out)
{
    static float acc = 0, prevHull = -1, prevShield = -1, saidAt = -10;
    acc += dt;
    if (acc < 0.25f)
        return;
    acc = 0;
    fl::Kinematics k = {};
    ai::Self s;
    if (!out.active || !ai::ReadSelf(s) || s.hull <= 0 || !fl::PlayerKinematics(k))
    {
        prevHull = prevShield = -1;
        return;
    }
    const float hull = s.hull;
    const bool shieldTook = s.shield < 0.02f || (prevShield >= 0 && s.shield < prevShield - 0.005f);
    // (g.time restarts with every mode start: a throttle stamped in an earlier run would silence the fail event)
    if (prevHull >= 0 && hull < prevHull - 0.001f && shieldTook && (g.time - saidAt > 1.0f || g.time < saidAt) &&
        space::StrictAt(k.pos).find("mine") != std::string::npos)
    {
        float nearest = 1e9f;
        for (const space::Obstacle& o : space::Obstacles(k.pos, 150.0f))
            if (o.mine)
            {
                const float d[3] = {o.pos[0] - k.pos[0], o.pos[1] - k.pos[1], o.pos[2] - k.pos[2]};
                nearest = std::min(nearest, Len3(d));
            }
        if (nearest <= 150.0f)
        {
            saidAt = g.time;
            events::Push("autopilot minefield-damage hull %.3f -> %.3f mine %.0f m at (%.0f,%.0f,%.0f) system=%u",
                         prevHull, hull, nearest, k.pos[0], k.pos[1], k.pos[2], fl::PlayerSystem());
        }
    }
    prevHull = hull;
    prevShield = s.shield;
}

// The last line before a planet's / sun's death zone, whatever the mode flies (s12b, M04 i3: the survive mode's evasion
// ran at 200 m/s into Manhattan's death sphere - r 4100, 2,000,000/s - 84 m outside it 0.5 s before; dead at full hull.
// Only the pursuit and the travel's avoidance knew the lethal spheres): closing on one so that the flight reaches its edge
// within 3 s (+200 m) -> steer out of it and along the tangent, no cruise, no afterburner toward it.
static void LethalGuard(Out& out)
{
    if (!out.active)
        return;
    fl::Kinematics k = {};
    if (!fl::PlayerKinematics(k))
        return;
    const float sp = Len3(k.vel);
    if (sp < 20)
        return;
    // (never against the game's own maneuver - its dock flies to a planet's ring ~500 m off the death zone, its goto and
    // lanes keep their own berth: review, session 12)
    space::Pilot gp;
    if (ai::DockInProgress() || (space::ReadPilot(gp) && (gp.op == ai::OpDock || gp.op == ai::OpGoto ||
                                                          gp.op == ai::OpTradeLane || gp.lane || gp.jumping)))
        return;
    for (const space::Obstacle& z : space::LethalZones(k.pos, sp * 4 + 500))
    {
        const float rel[3] = {z.pos[0] - k.pos[0], z.pos[1] - k.pos[1], z.pos[2] - k.pos[2]};
        const float d = Len3(rel);
        if (d < 1)
            continue;
        const float n[3] = {rel[0] / d, rel[1] / d, rel[2] / d}; // toward the centre
        const float closing = Dot3(k.vel, n);
        const float edge = d - z.radius;
        if (closing <= 0 || edge > closing * 3 + 200)
            continue;
        // (the mode's own aim outside the sphere, short of its edge, ends the flight first - a planet ring's fast dock at
        // full speed, the ring ~500 m off Holstein's death zone; the pursued target the same)
        bool aimFirst = false;
        float aim[3] = {};
        bool haveAim = false;
        if (out.steerTo)
            memcpy(aim, out.steerPos, sizeof(aim)), haveAim = true;
        else if (out.pursue && g.target)
        {
            const space::Contact tc = FindContact(g.target);
            if (tc.id)
                memcpy(aim, tc.pos, sizeof(aim)), haveAim = true;
        }
        if (haveAim)
        {
            const float ac[3] = {aim[0] - z.pos[0], aim[1] - z.pos[1], aim[2] - z.pos[2]};
            const float am[3] = {aim[0] - k.pos[0], aim[1] - k.pos[1], aim[2] - k.pos[2]};
            aimFirst = Len3(ac) > z.radius + 50 && Len3(am) < edge + 300;
        }
        if (aimFirst)
            continue;
        // out along the tangent of the motion, tilted away from the centre
        float tan[3] = {k.vel[0] - n[0] * closing, k.vel[1] - n[1] * closing, k.vel[2] - n[2] * closing};
        const float tl = Len3(tan);
        float dir[3];
        for (int i = 0; i < 3; ++i)
            dir[i] = (tl > 1 ? tan[i] / tl : 0.0f) * 0.6f - n[i] * 0.8f;
        const float dl = Len3(dir);
        float outPos[3];
        for (int i = 0; i < 3; ++i)
            outPos[i] = k.pos[i] + dir[i] / dl * 1500.0f;
        SteerTo(out, outPos, 1.0f, 0, 0);
        out.pursue = false;
        out.station = false;
        out.afterburn = false;
        static Throttle saidAt;
        if (saidAt.Due(g.time, 2))
        {
            events::Push("autopilot lethal-guard zone r=%.0f edge=%.0f m closing=%.0f m/s: steering out", z.radius,
                         edge, closing);
        }
        return;
    }
}

void Update(float dt, Out& out)
{
    TrackSystemEntry();
    UpdateModes(dt, out);
    GeometryGuard(dt, out);
    CruiseLock(out);
    LethalGuard(out);
    TractorMarkedLoot(dt, out);
    NoTractorOnDestroyMarks(out);
    BumpTrace(dt, out);
    MinefieldTrace(dt, out);
    MotionTrace(dt, &out);
}

void TracePlayer(float dt) { MotionTrace(dt, nullptr); }

} // namespace autopilot
