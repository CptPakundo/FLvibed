// Advanced Autopilot - frame: Update, the per-frame supervisor of every mode. Shared state and declarations in
// autopilot_impl.h, the module API in autopilot.h.
#include "autopilot_impl.h"
#include "autopilot_ashride.h"
#include "game/hudinfo.h"
#include "lookahead.h"

namespace autopilot
{
// ------------------------------------------------------------------ frame
// A dock port held while we requested none (our runs and the dock mode set fastRequested / started): a mission's own
// Act_DockRequest flies the ship (M12's core exit after the power cell: the objective mode restarted a
// travel 0.15 s after the game granted it, the request died, and the ship sat in the core until the jump hole's 600 s
// timer failed the mission). ai::MissionHolds did not read it as a hold.
// The server's grant comes first, the dock port a moment later (M13 22 Oct 2026: the sphere airlock's Act_DockRequest
// granted at 07:42:01.56, our new decision 0.4 s later replaced it - the port was not held yet): a grant we did not ask
// for (host API v14 DockReply: kind 0, response 4) counts for 4 s.
static ULONGLONG grantAt = 0; // the last dock grant we did not request (GameDocksUs)
static u32 grantSystem = 0;   // ...and the system it came in: a system switch since (an airlock's transit) used it up

// (M12: the lair airlock's grant still counted after the transit through it - the objective
// mode took the airlock 16 m behind the ship for "the way in", found no way and fell back to a dock request there, the
// lockout; 6 min stalled, the jump hole collapsed)
ULONGLONG SinceGameGrant()
{
    if (grantAt && grantSystem && fl::PlayerSystem() != grantSystem)
        grantAt = 0;
    return grantAt ? gameclock::NowMs64() - grantAt : ~0ULL;
}

// The first position seen in the current system (kept by Update every frame): the way in, which a mission's grant of
// "the" airlock never means (M12 core: the exit's grant after the power cell, the nearest airlock
// within 3 km was the entry the ship came in by - two refused requests, the run stalled)
static u32 entrySystem = 0;
static float entryPos[3] = {};
static bool entrySeen = false; // a system switch seen (the first system's first position is only where we started)

static ULONGLONG entryAt = 0;

// A load starts over (NoteMenu; audit A1-8: a load into the same system kept the last game's way in)
void ResetSystemEntry()
{
    entrySystem = 0, entrySeen = false, entryAt = 0;
}

static void TrackSystemEntry()
{
    const u32 sys = fl::PlayerSystem();
    fl::Kinematics k = {};
    if (!sys || !fl::PlayerKinematics(k))
        return;
    if (sys != entrySystem)
        entrySeen = entrySystem != 0, entrySystem = sys, entryAt = gameclock::NowMs64();
    // (the first second after the switch: a jump's first frames still show the old system's coordinates)
    if (gameclock::NowMs64() - entryAt < 1000)
        memcpy(entryPos, k.pos, sizeof(entryPos));
}

// When the player's system last changed (gameclock::NowMs64; 0 none seen since the logic started): a jump-in whatever flew it
// (the M12 hole is entered by the objective's jump, not the travel mode - its jump-in tick never set)
ULONGLONG SystemEntryTick() { return entrySeen ? entryAt : 0; }

bool SystemEntry(float pos[3])
{
    if (!entrySeen || entrySystem != fl::PlayerSystem())
        return false;
    memcpy(pos, entryPos, sizeof(entryPos));
    return true;
}

static u32 g_dockSeen = 0;
static bool g_dockPrimed = false; // replies from before the first look are history

// The autopilot starts: the replies the player's own docks drew while it was off are history too, not a mission's grant
void SyncDockReplies()
{
    u32 ship = 0, kind = 0, response = 0;
    g_dockSeen = HostDockReply(&ship, &kind, &response);
    g_dockPrimed = true;
}

bool GameDocksUs()
{
    const bool ours = g.dock.fastRequested || (g.mode == Mode::Dock && g.started);
    u32 ship = 0, kind = 0, response = 0;
    const u32 seq = HostDockReply(&ship, &kind, &response);
    if (!g_dockPrimed)
        g_dockPrimed = true, g_dockSeen = seq;
    if (seq != g_dockSeen)
    {
        g_dockSeen = seq;
        if (kind == 0 && response == 4 && !ours)
            grantAt = gameclock::NowMs64(), grantSystem = fl::PlayerSystem();
    }
    if (ours)
        return false;
    // ...and for as long as the ship's own behaviour is a Dock we did not start (M12 core exit: the grant's 4 s ran out
    // before the dock port was held, our travel took over again and the ship died in the core's radiation)
    ai::State s;
    const bool granted = SinceGameGrant() != ~0ULL; // (cleared by a system switch since the grant)
    // ...but a grant at an airlock is flown through at once (airlock_fly_through: the game's own Dock there letterboxes
    // the ship for good) - waiting cost the 4 s after the power cell (the user: "after the tractor
    // completed its operation the autopilot stalled")
    if (granted && g_set.airlockFlyThrough && g_obj.on && !ai::DockInProgress())
        for (const space::Contact& c : Contacts())
            if ((c.type & space::kAirlockGate) && c.distance < 3000)
                return false;
    const bool dockOp = granted && ai::ReadState(s) && s.op == ai::OpDock;
    return ai::DockInProgress() || dockOp || (granted && SinceGameGrant() < kGameGrantDockMs);
}

// Mode::Follow: when the game's follow last ran, when its current run began, how many runs in a row it ended within 1 s
static float followSeen = -100, followStart = -100;
static int followShort = 0;
static bool followPrev = false;
static bool followOwn = false; // `follow_own`: our own flight holds the follow (the game's kept ending)

// Keep following `leader` (Mode::Follow, Escort's rejoin) by the game's follow while it holds. The game ends a re-issued
// follow by itself near a slow leader (op 19 <-> 17: the bar's free-flight button blinking - the owner, 28 Oct 2026, M05;
// gate mpb21's probe: 7-15 flips per instance, each follow ended 0.15-0.19 s after its issue, also in escorts, which had
// no back-off at all). Each such end is logged with the situation (`follow ended`). Once a follow ended within 1 s,
// `follow_own` (on) flies it ourselves for as long as the leader stays within 1.25 x follow_distance and at engine
// speed: the nose at the follow point (the leader's offset, a second ahead) beyond 80 m, else parallel to the leader;
// the throttle for the leader's speed + half the gap (reverse when 30 m/s too fast) - no maneuver, so no op flips.
// Without it (or once the leader runs away / cruises): the back-off of c9732aa (2.5 s doubling to 20 s, a resting
// leader below 20 m/s not chased).
static void KeepFollowing(u32 leader, int op, Out& out)
{
    static u32 last = 0;
    if (leader != last) // (a new leader: nothing carried over)
    {
        last = leader;
        followOwn = followPrev = false;
        followShort = 0;
    }
    if (followSeen > g.time) // (g.time restarts with every start: the last run's stamps would read as the future)
        followSeen = followStart = -100, followPrev = false;
    if (op == ai::OpFollow)
    {
        if (!followPrev)
            followStart = g.time; // a new follow run
        followSeen = g.time;
    }
    else if (followPrev) // it just ended
    {
        const float ran = g.time - followStart;
        followShort = ran < 1.0f ? std::min(followShort + 1, 3) : 0;
        const space::Contact c = FindContact(leader);
        space::ShipMotion lm, mm;
        const bool lOk = space::ShipMotionOf(leader, lm), mOk = space::ShipMotionOf(space::PlayerShipId(), mm);
        events::Push("autopilot follow ended after=%.2f dist=%.0f leader-speed=%.0f our-speed=%.0f op=%d short=%d", ran,
                     c.id ? c.distance : -1.0f, lOk ? lm.speed : -1.0f, mOk ? mm.speed : -1.0f, op,
                     followShort);
    }
    followPrev = op == ai::OpFollow;
    if (op == ai::OpFollow)
        return;
    const space::Contact c = FindContact(leader);
    space::ShipMotion lm;
    const bool lOk = space::ShipMotionOf(leader, lm);
    const bool near = c.id && c.distance < g_set.followDistance * 1.25f;
    if (!near)
        followShort = 0;
    flight::Model fm;
    const float top = flight::ReadModel(fm) && fm.engine.ok() ? fm.engine.top() : 80.0f;
    // (gate mpb22: the game also ends a follow at once with the leader beyond follow_distance - 27 km, 9.9 km: re-issued
    // every 0.2 s - and the own flight flapped off / on at a leader just over our engine's top speed; now once on it stays
    // on for this leader, at any distance (cruising when far), except at lane / jump speed, where the game's follow rides)
    const bool own = g_set.followOwn && lOk && lm.speed < 600.0f && (followOwn || followShort > 0 || !near);
    if (own != followOwn)
    {
        events::Push("autopilot follow own %s dist=%.0f leader-speed=%.0f", own ? "on" : "off", c.id ? c.distance : -1.0f,
                     lOk ? lm.speed : -1.0f);
        followOwn = own;
    }
    fl::Kinematics k = {};
    if (own && fl::PlayerKinematics(k))
    {
        // the follow point: the leader's offset in its level frame (x right, y up, z behind), a second of its motion on
        float right[3] = {-lm.fwd[2], 0.0f, lm.fwd[0]};
        const float rn = sqrtf(right[0] * right[0] + right[2] * right[2]);
        if (rn > 0.2f)
            right[0] /= rn, right[2] /= rn;
        else
            right[0] = 1, right[2] = 0;
        float up[3];
        geom::Cross(up, right, lm.fwd);
        const float* o = g_set.followOffset;
        float p[3];
        for (int i = 0; i < 3; ++i)
            p[i] = lm.pos[i] + right[i] * o[0] + up[i] * o[1] - lm.fwd[i] * o[2] + lm.vel[i];
        // never a follow point in a lethal sphere's berth (gate mpb23 + its recheck, 29 Oct 2026, M03 "Fly to Walker":
        // Walker resting beside Planet Willard's death zone, the point behind him inside its berth - the lethal guard
        // and the follow pulled against each other 55 s, the segment 78 -> 120 s, twice): out to the edge + 200 m
        {
            const float back[3] = {k.pos[0] - p[0], k.pos[1] - p[1], k.pos[2] - p[2]};
            static float lethalNote = -100;
            float zr = 0;
            if (OutOfLethalBerth(p, 200.0f, back, &zr) && g.time - lethalNote > 10.0f)
                lethalNote = g.time, events::Push("autopilot follow point out of a lethal zone r=%.0f", zr);
        }
        const float e[3] = {p[0] - k.pos[0], p[1] - k.pos[1], p[2] - k.pos[2]};
        const float d = Len3(e), speed = Len3(k.vel);
        const float vWant = lm.speed + 0.5f * d;
        float aim[3];
        if (d > 80.0f)
            memcpy(aim, p, sizeof(aim));
        else
            for (int i = 0; i < 3; ++i)
                aim[i] = k.pos[i] + lm.fwd[i] * 2000.0f; // (alongside: parallel to the leader)
        // (cruise beyond 2.5 km, off inside 1 km, as it is between)
        SteerTo(out, aim, Clampf(vWant / std::max(1.0f, top), 0.0f, 1.0f), 0.35f, d > 2500.0f ? 1 : d < 1000.0f ? 0 : -1);
        out.steerReverse = speed > vWant + 30.0f;
        // round what is in the way like every other flight of ours (the own follow steered straight: M03's "Fly to
        // Walker" from Planet California Minor's ring ran at the planet's death zone, the lethal guard pushing back, 55 s
        // stalled 3.8 km short of Walker in gate mpb23 and its recheck)
        ApplyAvoid(k, out, leader);
        return;
    }
    if (!SubmitSettled())
        return;
    // The game ends its follow by itself once the ship sits behind a leader that has stopped (a story scene: M05, Juni
    // waiting through her comms) - issued again at once, it ended again a few frames later and the bar's free-flight
    // button blinked through the whole scene (op 19 <-> 17 every 0.3-11 s). Inside the follow
    // distance of a leader below 20 m/s the follow rests; it is issued again when the leader moves off or the ship falls
    // back. Inside that distance it also waits after the game ended one: 2.5 s, doubled for every follow the game ended
    // within 1 s (up to 20 s) - set oddsab: with a moving leader the game still ended each re-issue after ~0.15 s.
    const float wait = 2.5f * static_cast<float>(1 << followShort);
    const bool resting = near && (g.time - followSeen < wait || (lOk && lm.speed < 20.0f));
    if (!resting)
        Follow(leader);
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
        // before the docking cutscene, and the harness read a failure). Our request is the sign: the dock
        // port no longer reads as held at that moment (the second run of 19 Oct).
        const bool travelDock = g.mode == Mode::Travel && g.dock.fastRequested && g.tr.step < g.tr.plan.steps.size() &&
                                g.tr.plan.steps[g.tr.step].kind == route::Kind::Dock;
        const bool ourDock = travelDock || (g.mode == Mode::Dock && g.started);
        Stop(ourDock ? StopReason::Docking : StopReason::MissionControls);
        return;
    }
    // Our ship destroyed: stop before anything reads the fight (was: on the death frame the target vanished with our
    // ship and counted as a kill, nanobots were tapped at hull 0 and the stop said "fled").
    // Dead = relative health exactly 0, the game's own value for a destroyed ship (pub::SpaceObj::SetRelativeHealth(ship,
    // 0) is how a ship is killed; a teardown for a dock reads 0 too). A sliver of hull is alive: was <= 0.0005, and set
    // ab11 i5 (M13) flew on at hull (0, 0.0005) for 2.5 min - shields refilling, no death camera - while
    // every restart of the objective mode stopped "player died" at once (36 times) until the checkpoint limit. The death
    // camera stops us too (controller.cpp, "player-death"), should a death frame ever read above 0.
    const float health = ai::PlayerHealth();
    if (health == 0.0f)
    {
        // What was left went in the killing blow (was: never counted - a death read lost 0.53).
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
        {
            g.stats.ordLaunches += g.missiles.ammo - ammo; // (the stop line's ord-launches)
            events::Push("autopilot missile-round-left n=%d left=%d frames-since-tap=%u", g.missiles.ammo - ammo, ammo,
                         g.missiles.tapFrame ? g.frames - g.missiles.tapFrame : 0);
        }
        g.missiles.ammo = ammo;
    }
    TrackOwnMissiles();
    if (g_set.rollTest != 0) // debug `ap set roll_test <rad/s>`: the roll engine alone, in any mode (direction, rate)
        RollApply(dt, g_set.rollTest, 10.0f);
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
        // with no docking port, lane or jump in progress (was: 0.5 s / 5 s of free flight).
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
        // leader died may run on headless).
        // The leader's existence is polled twice a second (a scan of the system's objects); the follow is issued again
        // whenever the game is not running it and has acted on our last request (state, was a 2 s timer).
        g.issueTimer -= dt;
        if (g.issueTimer <= 0)
        {
            g.issueTimer = 0.5f; // polling interval of the leader check
            if (!FindContact(g.target).id)
            {
                // (`follow_defend`: under attack the ship is not left idle where the leader died - instance 6, 29 Oct
                // 2026: the follow stopped "leader gone" among attackers and the ship sat 90 s until it died)
                if (g_set.followDefend && g_sit.attackers > 0 && mprules::Autopilot() == mprules::Allow::Full)
                {
                    events::Push("autopilot follow-defend: the leader is gone under attack (attackers=%d) - fight",
                                 g_sit.attackers);
                    Start(Mode::Combat, 0, nullptr);
                    return;
                }
                Stop(StopReason::LeaderGone);
                return;
            }
        }
        // `follow_defend`: a follow the player started answers attackers - it becomes an escort of the same leader
        // (follows it, fights what comes at us or it, rejoins); the objective mode decides its own fights
        if (g_set.followDefend && !g_obj.on && g_sit.attackers > 0 && mprules::Autopilot() == mprules::Allow::Full)
        {
            const u32 leader = g.target;
            events::Push("autopilot follow-defend attackers=%d: escorting the leader %u", g_sit.attackers, leader);
            Start(Mode::Escort, leader, nullptr);
            return;
        }
        KeepFollowing(g.target, op, out);
        break;
    case Mode::Escort:
        if (g.tactic == Tactic::Rejoin)
        {
            if (!FindContact(g.escorted).id)
            {
                Stop(StopReason::EscortedGone);
                return;
            }
            KeepFollowing(g.escorted, op, out);
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
                    // target not selected on its first frame sent the escort straight back to Rejoin).
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

// Close-quarter flight ("a lot of crashing into geometry when flying from point to point" in M12 /
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
    // the cruise ends only when the guard must brake: steering round keeps it (M12 core: a 0.1 s block
    // cancelled the cruise wish and cost a 5 s charge at 80 m/s; the runners hold cruise round the lair's geometry)
    if (fan.brake)
        out.cruise = 0;
    out.steerThrottle = fan.brake ? 0.0f : 1.0f;
    // (barely moving: turn, do not back off - reverse thrust at 20-50 m/s in front of a wall flipped with the steering
    // and the ship crawled backwards, M12 core checkpoint 23 Oct 2026)
    out.steerReverse = fan.brake && speed > 50.0f;
}

// The motion trace (the user watching: "travelling inefficiently when close to some geometry",
// "flew very sub-optimally near the Nomad Base and had some collisions" - the log held only the guard's state changes):
// 10 rows a second while the autopilot flies, FLMotion.tsv beside the log (rewritten each game start; flleg keeps it
// with the leg's evidence). Steering flips, speed dips near hulls and the guard's work are then numbers.
// (out = nullptr: the player flies - TracePlayer; mode -1, action "player", the game's own throttle; the user's own
// runs through a set piece are the reference)
// Off for players (audit A1-1: 0.1.0 wrote ~10 rows / s for everyone): `motion_log` 1, or a test
// start (deploy.ps1 -Background puts FLVIBED_BACKGROUND=1 into the game's environment), turns it on.
static bool MotionTraceOn()
{
    static const bool background = [] {
        char v[8] = {};
        return GetEnvironmentVariableA("FLVIBED_BACKGROUND", v, sizeof(v)) && v[0] == '1';
    }();
    return g_set.motionTrace || background;
}

static void MotionTrace(float dt, const Out* out)
{
    static FILE* f = nullptr;
    static bool opened = false;
    static float acc = 0, flushAcc = 0;
    if (!MotionTraceOn() || (out && !out->active))
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
        // (ms = GetTickCount64, wall time; game_ms = gameclock::NowMs64, game time - its own origin,
        // restarted by a logic hot reload; the readers take columns by name)
        if (f)
            fputs("ms\tgame_ms\tmode\taction\tspeed\tsteer\tthrottle\tab\tcruise\tslide\tguard\tclear\twall\twall_"
                  "m\ttarget\ttgt_m\t"
                  "align\tx\ty\tz\tengine\tfx\tfy\tfz\tsystem\troll\thull\tshield\n",
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
    // hull and shield at 10 Hz (run-judge 29 Oct 2026: the 1 s `self` samples showed 0.645 where a big-hit had left 0.07)
    ai::Self me;
    const bool haveSelf = ai::ReadSelf(me);
    const float hull = haveSelf ? me.hull : -1.0f, shield = haveSelf ? me.shield : -1.0f;
    if (!out)
        fprintf(f,
                "%llu\t%llu\t-1\tplayer\t%.0f\tplayer\t%.2f\t-1\t-1\t0\t0\t-1\t0\t-1\t0\t-1\t%.2f\t%.0f\t%.0f\t%.0f\t%d\t%."
                "3f\t%.3f\t%.3f\t%u\t%.0f\t%.3f\t%.3f\n",
                GetTickCount64(), gameclock::NowMs64(), speed, fl::GetThrottle(), align, k.pos[0], k.pos[1], k.pos[2], engine, fwd[0], fwd[1],
                fwd[2], fl::PlayerSystem(), geom::RollDeg(k.rot), hull, shield);
    else
    {
        const Out& o = *out;
        const char* steer = o.brake ? "brake" : o.station ? "station" : o.pursue ? "pursue" : o.steerTo ? "steer" : "-";
        const space::Contact tc = g.target ? FindContact(g.target) : space::Contact{};
        fprintf(f,
                "%llu\t%llu\t%d\t%s\t%.0f\t%s\t%.2f\t%d\t%d\t%d\t%d\t%.0f\t%d\t%.0f\t%u\t%.0f\t%.2f\t%.0f\t%.0f\t%.0f\t%d\t%."
                "3f\t%.3f\t%.3f\t%u\t%.0f\t%.3f\t%.3f\n",
                GetTickCount64(), gameclock::NowMs64(), static_cast<int>(g.mode), ObjectiveOn() ? ActionName(g_obj.action) : "-", speed,
                steer, o.steerTo ? o.steerThrottle : -1.0f, o.afterburn ? 1 : 0, o.cruise, o.slide,
                g_guardBlocked ? 1 : 0, g_guardClear, g.avoid.wallSlide, g.avoid.wallSlide ? g.avoid.wallNear : -1.0f,
                g.target, tc.id ? tc.distance : -1.0f, align, k.pos[0], k.pos[1], k.pos[2], engine, fwd[0], fwd[1],
                fwd[2], fl::PlayerSystem(), geom::RollDeg(k.rot), hull, shield);
    }
    if (flushAcc >= 1.0f)
        flushAcc = 0, fflush(f);
}

// Loot the mission marks is tractored in passing, whatever the mode (the user's M12 core:
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

// Only loot no objective points at to destroy is collected (Destroy Contraband): a tractor-all
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

// The cruise lock ("the ship must not drop cruise speed from the moment it is engaged at the core's
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
    // (...and a cruise charge: the speed falls to the engine's 80 m/s while it charges - M02 sc1's five "bumps" in the
    // chase were our own presses, 15:47:03 with the nearest ship 1124 m off; the real impacts went unlogged - run-judge)
    flight::Model bfm;
    const bool charging = flight::ReadModel(bfm) && bfm.cruiseEngaged && bfm.cruiseChargeTime > 0 &&
                          bfm.cruiseChargeElapsed < bfm.cruiseChargeTime;
    const bool slowing = out.brake || out.steerReverse || out.pursue || out.station ||
                         (out.steerTo && out.steerThrottle < 0.5f) || charging;
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

// Hull lost to a minefield ("consider any run through Zone 21 that received hull damage from the
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

// Level-off (on a long flight to a distant waypoint the ship stayed banked, the camera skewed).
// The player has no roll control (USER_Z_ROTATE / UNROTATE are polled by nothing) and FLvibed's steering - the game's
// own mouse flight - never levels: measured, a -56 deg bank stayed (0.3 deg/s), the game's auto-level toggle (0x612700
// -> IBehaviorManager::update_current_behavior_auto_level) changed nothing in free flight. The game's Goto levels a ship
// in ~2 s (-22 -> -1 deg). So on a travel's straight run (an Arrive step, the nose on its aim within 2 deg, nothing
// lethal and no hull ahead) a bank over 15 deg held 1.5 s hands the run to the game's Goto toward the same aim until the
// bank is under 3 deg (at most 4 s), then free flight again. Setting `level_off`.
// Since 28 Oct 2026 (evening; the owner: the ship "levels off in a non-smooth way when the autopilot is moving to an
// objective after it had to turn to face it" - the Goto took the steering and flew its own aim for up to 4 s): the roll
// engine (RollApply, verified: +1 rad/s raised the bank 15 deg per 0.3 s, -1 brought it back to 0.1 deg, linear, no
// drift after) levels the ship itself - a roll rate of -2 x the bank (rad/s), at most half the ship's own top roll rate:
// an exponential settle (45 deg level in ~1.5 s), the steering never handed over, the nose on the objective throughout.
// Whenever the autopilot travels, docks or goes to a point (not fighting: `roll_guns` / `roll_flair` own the roll then)
// with the nose off the vertical. The Goto hand-over below stays as the fallback where the roll engine is refused.
// The barrel roll (`roll_flair`; "instance 4 could also do some rolling... For flare. M06's race
// is also a fine spot of flare rolls"): one full turn about the nose at the ship's own top roll rate (~3.8 s on a
// fighter), the angle counted from the measured spin; a full turn ends where it began (level-off takes the rest). It
// never costs a turn rate (pitch and yaw turn alike on every ship); started only where it costs no gun (a fight: equal
// coverage at every roll angle) or no aim (a straight run).
static struct
{
    bool on = false;
    float dir = 1, left = 0, nextAt = 0;
} g_barrel;

bool BarrelRoll(float dt, float rate)
{
    if (!g_barrel.on)
        return false;
    float spin[3];
    if (space::PlayerSpin(spin))
        g_barrel.left -= std::fabs(spin[2]) * dt;
    if (g_barrel.left <= 0.05f)
    {
        g_barrel.on = false;
        RollApply(dt, 0, rate); // (stop the spin)
        events::Push("autopilot barrel-roll end");
        return false;
    }
    RollApply(dt, g_barrel.dir * rate, rate);
    return true;
}

// Bank into a turn toward `aim` (world): true while a turn is banked (rolled this frame). +rate = roll right (about the
// nose; verified in game: the up axis turns toward the right wing), so the roll that puts the aim at the ship's up is
// atan2(right, up) of the aim in the ship's frame - limited to 70 deg and scaled by how far off the nose the aim is.
bool BankIntoTurn(float dt, const fl::Kinematics& k, const float aim[3], float rate)
{
    if (!g_set.rollBank)
        return false;
    const float v[3] = {aim[0] - k.pos[0], aim[1] - k.pos[1], aim[2] - k.pos[2]};
    const float len = Len3(v);
    if (len < 1.0f)
        return false;
    const float right[3] = {k.rot[0 * 3 + 0], k.rot[1 * 3 + 0], k.rot[2 * 3 + 0]};
    const float up[3] = {k.rot[0 * 3 + 1], k.rot[1 * 3 + 1], k.rot[2 * 3 + 1]};
    float fwd[3];
    geom::Forward(fwd, k.rot);
    const float off = acosf(Clampf(Dot3(fwd, v) / len, -1.0f, 1.0f)); // the turn still to fly (rad)
    if (off < 0.26f)                                                   // (15 deg: the rest is level-off's)
        return false;
    const float r = Dot3(v, right), u = Dot3(v, up);
    const float scale = Clampf((off - 0.26f) / 0.44f, 0.0f, 1.0f); // from 0 at 15 deg to full at 40 deg
    // (the aim toward the ship's up; folded to "up or down" the bank never settled - instance 5: a continuous roll
    // through +-180 deg as the target flipped sides with the steering. The last 15 deg of the turn are level-off's - its
    // exponential return does not overshoot; banked to 8 deg the first try overshot -43 -> +19 deg)
    const float bank = Clampf(atan2f(r, u), -1.22f, 1.22f) * scale; // the roll still wanted (rad, <= 70 deg)
    // (the sign, measured on instance 5: + rate turns the up axis toward forward x up = the ship's right - the frame is
    // right-handed; an aim on the right wants a + roll. Off by default (`roll_bank`): banked against our screen-space
    // steering the ship spun through +-180 deg in both signs - the steering moves the aim round the ship's frame faster
    // than the roll follows; open)
    RollApply(dt, Clampf(2.0f * bank, -rate, rate), rate);
    static Throttle said;
    if (said.Due(g.time, 0.3f))
    {
        float spin[3] = {};
        space::PlayerSpin(spin);
        // (the turn's own direction in the ship's frame - the nose moves toward (-spin y, spin x) - beside the aim's
        // atan2(r, u): equal when the game turns the nose where we steer)
        events::Push("autopilot bank off=%.0f r=%.2f u=%.2f phi=%.0f turn=%.0f w=%.2f bank=%.0f roll=%.0f spinz=%.2f",
                     off * 57.3f, r / len, u / len, atan2f(r, u) * 57.3f, atan2f(-spin[1], spin[0]) * 57.3f,
                     sqrtf(spin[0] * spin[0] + spin[1] * spin[1]), bank * 57.3f, geom::RollDeg(k.rot), spin[2]);
    }
    return true;
}

void BarrelStop()
{
    if (!g_barrel.on)
        return;
    g_barrel.on = false;
    events::Push("autopilot barrel-roll stopped (a turn)");
}

void BarrelStart(float minGap, const char* where)
{
    if (g_barrel.on || g.time < g_barrel.nextAt)
        return;
    g_barrel.on = true;
    g_barrel.dir = (g.frames & 1) ? 1.0f : -1.0f;
    g_barrel.left = 2.0f * 3.14159265f;
    g_barrel.nextAt = g.time + minGap;
    events::Push("autopilot barrel-roll %s dir=%.0f", where, g_barrel.dir);
}

static bool LevelByRoll(float dt, Out& out)
{
    // Only while our own steering flies (the game in free flight): the game's maneuvers (Goto, Dock, Follow ...) roll the
    // ship to their own up reference - rolled against them, the ship spun on through +-180 deg (instance 5, 28 Oct 2026, a
    // travel to Fort Bush: banking and levelling against the game's dock / goto).
    {
        space::Pilot p;
        if (!space::ReadPilot(p) || p.op != ai::OpFreeFlight)
            return false;
    }
    if (g.mode != Mode::Combat)
    {
        space::ShipMotion me;
        fl::Kinematics kk = {};
        if (g_set.rollFlair && space::ShipMotionOf(space::PlayerShipId(), me) && me.rollRate > 0.1f &&
            fl::PlayerKinematics(kk))
        {
            // (no rolls on a straight run - "No need to roll when flying straight - I meant
            // rolling during maneuvers ... when flying to a distant objective you want to be level with the system's
            // plane and stay that way": the turns bank, below; straight runs level)
            BarrelStop(); // (barrel rolls are a fight's: one still turning when the fight ends stops - M07, mpb19)
            // Banking into a turn (travel, races - M06's rings): an aim more than 8 deg off the nose is rolled toward
            // the ship's up (atan2 of its right / up components), at most 70 deg, scaled by the turn (full at 30 deg)
            if (out.steerTo && !out.pursue && !out.brake && BankIntoTurn(dt, kk, out.steerPos, me.rollRate))
                return true;
        }
    }
    if (!g_set.levelOff || out.pursue ||
        !(g.mode == Mode::Travel || g.mode == Mode::Dock || g.mode == Mode::Goto || g.mode == Mode::Follow ||
          g.mode == Mode::Face || g.mode == Mode::Hold))
        return false;
    fl::Kinematics k = {};
    space::ShipMotion me;
    if (!fl::PlayerKinematics(k) || !space::ShipMotionOf(space::PlayerShipId(), me) || me.rollRate <= 0.1f)
        return false;
    float fwd[3];
    geom::Forward(fwd, k.rot);
    if (std::fabs(fwd[1]) > 0.9f) // (nose near the vertical: no meaningful bank)
        return true;
    // ...and only with the nose within 10 deg of where our steering flies it: rolled during a big turn, the screen-space
    // steering's turn bends into a spiral (gate mpb19, M07: levelling an 83 deg bank through a 159 deg turn to a lane ring
    // took the ship from 2.5 to 3.9 km off, "fast-dock stalled", the segment over its limit). The bank waits for the turn.
    if (out.steerTo)
    {
        const float aim[3] = {out.steerPos[0] - k.pos[0], out.steerPos[1] - k.pos[1], out.steerPos[2] - k.pos[2]};
        const float aimLen = Len3(aim);
        if (aimLen > 1.0f && Dot3(fwd, aim) / aimLen < 0.985f) // (10 deg)
            return true;
    }
    const float roll = geom::RollDeg(k.rot);
    static bool levelling = false;
    if (std::fabs(roll) < (levelling ? 0.5f : 2.0f))
    {
        if (levelling)
        {
            levelling = false;
            RollApply(0, 0, me.rollRate); // (stop the spin at level)
            events::Push("autopilot level-off end (level)");
        }
        return true;
    }
    if (!levelling)
        events::Push("autopilot level-off roll=%.0f speed=%.0f (by roll)", roll, Len3(k.vel));
    levelling = true;
    const float half = 0.5f * me.rollRate;
    RollApply(0, Clampf(-2.0f * roll * 3.14159265f / 180.0f, -half, half), me.rollRate);
    return true;
}

static void LevelOff(float dt, Out& out)
{
    if (LevelByRoll(dt, out))
        return;
    static float banked = 0, until = -1, quietUntil = -1;
    static bool on = false;
    const auto end = [&](const char* why)
    {
        if (!on)
            return;
        on = false;
        quietUntil = g.time + 8.0f;
        FreeFlightNow();
        events::Push("autopilot level-off end (%s)", why);
    };
    fl::Kinematics k = {};
    const bool straightRun = g_set.levelOff && g.mode == Mode::Travel && g.tr.phase == State::Travel::Fly &&
                             g.tr.step < g.tr.plan.steps.size() &&
                             g.tr.plan.steps[g.tr.step].kind == route::Kind::Arrive && out.steerTo && !out.brake &&
                             !out.station && !out.pursue && !out.steerReverse && !g_guardBlocked &&
                             fl::PlayerKinematics(k);
    if (!straightRun)
    {
        banked = 0;
        end("not a straight run");
        return;
    }
    float fwd[3];
    geom::Forward(fwd, k.rot);
    const float aim[3] = {out.steerPos[0] - k.pos[0], out.steerPos[1] - k.pos[1], out.steerPos[2] - k.pos[2]};
    const float aimLen = Len3(aim), speed = Len3(k.vel);
    const float roll = geom::RollDeg(k.rot);
    const bool aligned = aimLen > 2000 && Dot3(fwd, aim) / aimLen > 0.99939f; // 2 deg
    if (on)
    {
        out.steerTo = false; // the game's Goto flies meanwhile (the cruise as it is)
        out.cruise = -1;
        if (std::fabs(roll) < 3.0f)
            end("level");
        else if (g.time > until)
            end("4 s");
        else if (!aligned && Dot3(fwd, aim) / std::max(1.0f, aimLen) < 0.996f) // (5 deg: the aim moved)
            end("the aim moved");
        return;
    }
    banked = aligned && speed > 60 && std::fabs(fwd[1]) < 0.8f && std::fabs(roll) > 15.0f ? banked + dt : 0;
    if (banked < 1.5f || g.time < quietUntil)
        return;
    for (const space::Obstacle& z : space::LethalZones(k.pos, speed * 6 + 1000))
    {
        const float rel[3] = {z.pos[0] - k.pos[0], z.pos[1] - k.pos[1], z.pos[2] - k.pos[2]};
        if (Dot3(rel, fwd) > 0)
            return; // (the guards stand aside for the game's Goto: never with a lethal zone ahead)
    }
    space::Pilot p;
    if (!space::ReadPilot(p) || p.op != ai::OpFreeFlight || p.lane || p.jumping || ai::DockInProgress() ||
        !SubmitSettled())
        return;
    flight::Model fm;
    ai::Directive d;
    d.op = ai::OpGoto;
    d.hasPos = true;
    memcpy(d.pos, out.steerPos, sizeof(d.pos));
    d.range = 0;
    d.cruise = flight::ReadModel(fm) && fm.cruiseEngaged;
    if (SubmitNow(d) < 0)
    {
        quietUntil = g.time + 30.0f; // refused (multiplayer: no server-side AI here)
        return;
    }
    on = true;
    until = g.time + 4.0f;
    banked = 0;
    out.steerTo = false;
    events::Push("autopilot level-off roll=%.0f speed=%.0f", roll, speed);
}

// The coast (`big_coast`; the owner, 29 Oct 2026, the runners' Dyson city generators: "cruise to the first one, then
// ENGINE KILL a bit outside weapons range and start firing missiles and torpedoes, then all weapons as they come in range
// ... Killing the engine when in cruise engine only works when NO additional engine-related input is sent ... No
// stopping, no throttle action. Just engine kill"; "Do not re-engage when taking a hit, only when shields are almost
// down"). A guard over every mode, run last: while the objective mode cruises (a trip or the fight's approach) at a huge
// hostile static target ahead (radius >= 500 m) within gun reach + big_coast_lead of its hull, the engine is killed in
// cruise, the fight is started on it (the guns and launchers by their own rules) and from then on only the steering acts
// (Out::coast: the controller sends no throttle, reverse, afterburner, slide or cruise input) - until weapons range (90 %
// of gun reach), the shield almost down (15 %) or 40 s; then the engine back on and the fight as usual. The first tries
// lived inside the fight and never met a cruise: the "Fly to the Waypoint" trip braked to rest ~1 km short.
static struct
{
    u32 target = 0, done = 0;
    int phase = 0; // 0 watching, 1 coasting, 2 the engine back on and braking to the stop at weapons range
    float at = 0, killAt = 0, scanAt = -10;
} g_coast;

// The chase's coast (`chase_kill`; lookahead.h ChaseStep): the moving-ship case of the coast - the objective's fight on the
// chased ship (g_obj.chaseShip) cruising after him: with him ahead (30 deg) 110-250 m off at cruise speed the engine is
// killed and only the steering acts (Out::coast, the guns by their own rules) - the runners' 297 -> 283 m/s coasts firing
// at ~120-150 m - until the gap opens past 90 % of gun reach, our shield is almost down (15 %) or he is gone; then the
// engine back on and the cruise again (no brake: he still runs). 0 watching, 1 coasting, 2 the engine back on.
static struct
{
    u32 target = 0;
    int phase = 0;
    float at = 0, killAt = 0;
} g_chaseCoast;

bool CoastActive() { return g_coast.phase != 0 || g_chaseCoast.phase != 0; }

// A coast that no frame will finish: a stop never reaches the guards, and with the mode off their outputs are dropped. The engine they
// killed comes back here, and the objective is not held behind a phase nothing can end. `dead`: no ship to give an engine back to.
void CoastAbort(bool dead)
{
    if (!CoastActive())
        return;
    const bool off = !dead && (g_coast.phase == 1 || g_chaseCoast.phase == 1 || g_coast.phase == 2) && hudinfo::EngineKilled();
    events::Push("autopilot coast left phase=%d chase=%d engine=%s", g_coast.phase, g_chaseCoast.phase, off ? "back on" : "as it was");
    g_coast.phase = g_chaseCoast.phase = 0;
    if (off)
        flightio::Tap(fl::USER_MANEUVER_ENGINEKILL);
}

// A new game: a dock grant and the last coast were the last game's
void ForgetCoastAndGrant()
{
    CoastAbort(true);
    g_coast.done = 0;
    grantAt = 0;
}

static void ChaseCoast(Out& out)
{
    if (!g_set.chaseKill || !ObjectiveOn() || mprules::Autopilot() != mprules::Allow::Full || g_coast.phase != 0)
    {
        if (g_chaseCoast.phase == 1 && hudinfo::EngineKilled())
            out.engineKill = true; // (left mid-coast: the engine back on)
        g_chaseCoast.phase = 0;
        return;
    }
    const u32 id = g_chaseCoast.phase ? g_chaseCoast.target
                   : g.mode == Mode::Combat && g.target && g.target == g_obj.chaseShip ? g.target
                                                                                        : 0;
    if (!id)
        return;
    ai::Self self;
    fl::Kinematics k = {};
    if (!ai::ReadSelf(self) || self.gunRange <= 0 || !fl::PlayerKinematics(k))
        return;
    flight::Model fm;
    const bool haveModel = flight::ReadModel(fm);
    const space::Contact tc = FindContact(id);
    const float rel[3] = {tc.pos[0] - k.pos[0], tc.pos[1] - k.pos[1], tc.pos[2] - k.pos[2]};
    const float gap = tc.id ? Len3(rel) : 0.0f;
    float fwd[3];
    geom::Forward(fwd, k.rot);
    space::ShipMotion hm;
    lookahead::ChaseFacts f;
    f.gap = gap;
    f.bearingDeg = gap > 1 ? acosf(Clampf(Dot3(fwd, rel) / gap, -1, 1)) * 57.2958f : 0.0f;
    f.mySpeed = Len3(k.vel);
    f.hisSpeed = tc.id && space::ShipMotionOf(id, hm) ? hm.speed : 0.0f;
    f.engineTop = haveModel ? fm.engine.top() : 0.0f;
    f.gunRange = self.gunRange;
    f.cruising = haveModel && fm.cruiseEngaged;
    f.coasting = g_chaseCoast.phase == 1;
    f.killed = !tc.id || tc.health == 0;
    f.shield = self.shield;
    // (`spawn_charge`: a fleeing ship's coast starts at kChargeCoastSpeed, not 150 m/s - from the stage's gap the window
    // is reached in our charge's ramp, where a 150 m/s coast is outrun at once; and not refused while his speed still
    // reads under our engine's: lookahead.h ChaseStep)
    f.fleeing = ChargeFleeing(id);
    const bool engineOff = hudinfo::EngineKilled();
    // phase 2: the engine back on (pressed again only if it still reads off 1.5 s after the last press: the game's state
    // lags a press - big_coast's lesson), the throttle and the cruise on at once; over when the engine reads on, <= 3 s
    if (g_chaseCoast.phase == 2)
    {
        if (engineOff && g.time - g_chaseCoast.killAt > 1.5f)
        {
            out.engineKill = true;
            g_chaseCoast.killAt = g.time;
        }
        if (!engineOff || g.time - g_chaseCoast.at > 3.0f)
        {
            events::Push("autopilot chase recruise gap=%.0f speed=%.0f engine=%s", gap, f.mySpeed, engineOff ? "off" : "on");
            g_chaseCoast.phase = 0;
            return;
        }
        out.coast = false;
        out.pursue = false, out.station = false, out.brake = false;
        if (tc.id)
            SteerTo(out, tc.pos, 1.0f, 0.35f, 1);
        return;
    }
    const lookahead::ChaseResult r = lookahead::ChaseStep(f);
    if (g_chaseCoast.phase == 0)
    {
        // (`ash_ride`: its gate owns the start against the ship it rode to - he >= 295 m/s, we at cruise, he dead ahead on
        // our VELOCITY line and parallel (a coast is a straight line: the owner), the gap 90-220 m stable 1 s)
        // (hm is read above with his speed; unread, its velocity is zero and the gate refuses)
        bool rideStart = false;
        if (tc.id && AshRideCoast(id, gap, f.hisSpeed, hm.vel, f.mySpeed, k.vel, rel, f.cruising,
                                  f.shield >= 0 && f.shield < lookahead::kChaseShieldOut, &rideStart))
        {
            if (!rideStart)
                return;
        }
        else if (r.verdict != lookahead::ChaseVerdict::Coast)
            return;
        events::Push("autopilot coast start ship=%u gap=%.0f v=%.0f his=%.0f bearing=%.0f", id, gap, f.mySpeed, f.hisSpeed,
                     f.bearingDeg);
        g_chaseCoast.phase = 1, g_chaseCoast.target = id, g_chaseCoast.at = g_chaseCoast.killAt = g.time;
        out.engineKill = true;
    }
    else if (r.verdict == lookahead::ChaseVerdict::EngineOn || g.time - g_chaseCoast.at > 30.0f)
    {
        const char* why = r.verdict == lookahead::ChaseVerdict::EngineOn ? r.why : "30 s";
        events::Push("autopilot coast end (%s) ship=%u gap=%.0f v=%.0f his=%.0f after %.1f s", why, id, gap, f.mySpeed,
                     f.hisSpeed, g.time - g_chaseCoast.at);
        if (engineOff)
            out.engineKill = true; // (the engine back on - then the cruise, no brake)
        g_chaseCoast.phase = 2;
        g_chaseCoast.at = g_chaseCoast.killAt = g.time;
        out.coast = false;
        return;
    }
    // (the kill once more if the game did not take it, in the first 3 s only)
    else if (!engineOff && g.time - g_chaseCoast.killAt > 1.0f && g.time - g_chaseCoast.at < 3.0f)
    {
        out.engineKill = true;
        g_chaseCoast.killAt = g.time;
    }
    out.coast = true;
    out.pursue = false, out.station = false, out.steerTo = false, out.brake = false;
    const aim::Target t = aim::Current(true);
    memcpy(out.steerPos, t.valid && fl::TargetId() == id ? t.world : tc.pos, sizeof(out.steerPos));
}

// spawn_stage's hold: the nose along the spawned ship's run (g_obj.stageDir) - his line is where the chase begins
static void StageNose(Out& out)
{
    if (!g_set.spawnStage || !g_obj.stageNose || g.mode != Mode::Hold || g_obj.action != Action::PreStage ||
        (g.hold.phase != HoldPhase::Resting && g.hold.phase != HoldPhase::Turning))
        return;
    fl::Kinematics k = {};
    if (!fl::PlayerKinematics(k))
        return;
    const float look[3] = {k.pos[0] + g_obj.stageDir[0] * 5000.0f, k.pos[1], k.pos[2] + g_obj.stageDir[2] * 5000.0f};
    float fwd[3];
    geom::Forward(fwd, k.rot);
    const float to[3] = {look[0] - k.pos[0], 0, look[2] - k.pos[2]};
    if (Dot3(fwd, to) / std::max(1.0f, Len3(to)) > 0.9986f) // (within 3 deg: at rest - Hold's own turn rule)
        return;
    SteerTo(out, look, 0, 0.8f, 0);
    out.steerReverse = false;
    out.afterburn = false;
}

// spawn_charge's fight at the stage (design 2; lookahead.h ChargeKeep): the staged fight's last ship fought from the point -
// beyond kChargeKeep of it the pursuit keeps station (the nose and the guns stay on the escort) at a point between P and
// the escort until back within kChargeKeepIn; no chase away from P. The coast guards outrank it.
static void ChargeKeepStation(Out& out)
{
    // (not under `ash_ride`: its fight brings the last ship low wherever it flies, then the ride cruises off)
    const bool on = g_set.spawnCharge && g_set.spawnStage && g_obj.chargeStage && g_obj.stageVerdict == 2 &&
                    g.mode == Mode::Combat && g.target && g.target == g_obj.stageLastShip && out.pursue && !CoastActive() &&
                    !AshRideStage(g_obj.stageTrigger);
    if (!on)
    {
        g_obj.chargeKeepOn = false;
        return;
    }
    fl::Kinematics k = {};
    const space::Contact tc = FindContact(g.target);
    if (!fl::PlayerKinematics(k) || !tc.id)
        return;
    const lookahead::KeepResult r = lookahead::ChargeKeep(k.pos, g_obj.stagePos, tc.pos, g_obj.chargeKeepOn);
    if (r.station != g_obj.chargeKeepOn)
        events::Push("autopilot spawn-charge keep %s d_P=%.0f target=%u target_d_P=%.0f", r.station ? "on" : "off", r.dP,
                     g.target, geom::Dist(tc.pos, g_obj.stagePos));
    g_obj.chargeKeepOn = r.station;
    if (!r.station)
        return;
    // (far from the station point: flown to at full throttle, the burner when the nose is on it - the station controller
    // crawled at 24-33 m/s for 24 s in probe sc1 and never got within 400 m of P)
    const float to[3] = {r.pos[0] - k.pos[0], r.pos[1] - k.pos[1], r.pos[2] - k.pos[2]};
    const float dist = Len3(to);
    if (dist > lookahead::kChargeKeepFly)
    {
        float fwd[3];
        geom::Forward(fwd, k.rot);
        out.station = false;
        out.steerTo = true;
        memcpy(out.steerPos, r.pos, sizeof(out.steerPos));
        out.steerFullAngle = 0.35f;
        out.steerThrottle = 1.0f;
        out.afterburn = Dot3(fwd, to) / dist > 0.7f;
        return;
    }
    out.station = true;
    memcpy(out.stationPos, r.pos, sizeof(out.stationPos));
    out.steerTo = false;
    out.afterburn = false;
}

static void CoastGuard(Out& out)
{
    if (!g_set.bigCoast || !ObjectiveOn() || mprules::Autopilot() != mprules::Allow::Full)
    {
        if (g_coast.phase == 1 && hudinfo::EngineKilled())
            out.engineKill = true; // (left mid-coast: the engine back on)
        g_coast.phase = 0;
        return;
    }
    if (g_chaseCoast.phase != 0)
        return; // (the chase's coast flies the ship)
    ai::Self self;
    fl::Kinematics k = {};
    if (!ai::ReadSelf(self) || self.gunRange <= 0 || !fl::PlayerKinematics(k))
        return;
    flight::Model fm;
    const bool cruising = flight::ReadModel(fm) && fm.cruiseEngaged;
    const float speed = Len3(k.vel), range = self.gunRange * 0.9f;
    const auto hullGap = [&](const space::Contact& c, float radius)
    {
        const float h = space::RayHitDistance(k.pos, c.pos);
        const float rel[3] = {c.pos[0] - k.pos[0], c.pos[1] - k.pos[1], c.pos[2] - k.pos[2]};
        return h >= 0 ? h : Len3(rel) - radius;
    };
    if (g_coast.phase == 0)
    {
        // (g.time restarts with every start: a stamp from the last run is no reason to wait)
        if (!cruising || speed < 150.0f || (g.time >= g_coast.scanAt && g.time - g_coast.scanAt < 0.25f))
            return;
        g_coast.scanAt = g.time;
        float fwd[3];
        geom::Forward(fwd, k.rot);
        for (const space::Contact& c : Contacts())
        {
            // (a destructible solar: finite hit points - a solar's attitude reads 0, Contact::attitude is ships only, so
            // the first build never found the generators; planets, stations and rings are invulnerable)
            if (c.ship || c.health == 0 || !(c.maxHit > 0 && c.maxHit < 1e9f) || c.id == g_coast.done ||
                c.distance > 9000.0f)
                continue;
            // never a celestial body or the lanes' / gates' infrastructure, and never a static in place of the ships a
            // fight is on (gate mpb23: M01b's fight on the Rogue fighters coasted at Li01's ice moon behind
            // them - it reads finite hit points - restarted the fight on the moon and held at its hull, 52 m off its
            // death zone: dead in 135 s, a leg that had passed every gate). A static that is the fight's own target (M13's
            // generators: the objective starts the fight on them) or a trip's goal ahead stays a coast target.
            if ((c.type & (space::kCelestial | space::kTradelaneRing | space::kJumpGate | space::kJumpHole |
                           space::kDockingRing)) != 0 ||
                (g.mode == Mode::Combat && g.target != 0 && g.target != c.id))
                continue;
            // `big_coast_own` (M13-MIMIC set mm1: 3 of 3 runs failed "You failed to fly to the waypoint" -
            // during "Escort Sinclair to the Battleship Osiris" the coast opened a 78-99 s fight on NomadCap2 / NomadCap4
            // from the formation and the trip, statics no objective asked for; the runners cruise past them): only the
            // fight's own target, a mark of a "Destroy ..." step, or the trip's own destination
            if (g_set.bigCoastOwn && !(g.mode == Mode::Combat && g.target == c.id) &&
                !(g.mode == Mode::Travel && g.tr.destObject == c.id))
            {
                u32 mk[32];
                const int mn = g_obj.fightObjective ? MissionObjectIds(mk, 32, Contacts()) : 0;
                if (std::find(mk, mk + mn, c.id) == mk + mn)
                    continue;
            }
            float radius = 0;
            for (const space::Obstacle& o : space::Obstacles(c.pos, 50))
                if (o.id == c.id)
                    radius = o.radius;
            if (radius < 500.0f)
                continue;
            const float rel[3] = {c.pos[0] - k.pos[0], c.pos[1] - k.pos[1], c.pos[2] - k.pos[2]};
            const float rl = std::max(1.0f, Len3(rel));
            if (Dot3(fwd, rel) / rl < 0.7f)
                continue; // (ahead only: within ~45 deg of the nose)
            const float gap = hullGap(c, radius);
            if (gap <= range || gap > self.gunRange + g_set.bigCoastLead)
                continue;
            events::Push("autopilot coast start target=%u gap=%.0f speed=%.0f mode=%s (engine kill in cruise)", c.id, gap,
                         speed, ModeName(g.mode));
            // (the fight first - its start hands the ship to free flight, which keeps the cruise - the kill a frame
            // later, so no maneuver meets the killed engine)
            const bool restart = g.mode != Mode::Combat || g.target != c.id;
            if (restart)
                Start(Mode::Combat, c.id, nullptr);
            g_coast.phase = 1, g_coast.target = c.id, g_coast.at = g.time;
            g_coast.killAt = restart ? g.time - 0.95f : g.time;
            if (!restart)
                out.engineKill = true;
            break;
        }
        if (g_coast.phase == 0)
            return;
    }
    const space::Contact tc = FindContact(g_coast.target);
    float radius = 0;
    if (tc.id)
        for (const space::Obstacle& o : space::Obstacles(tc.pos, 50))
            if (o.id == tc.id)
                radius = o.radius;
    const bool killed = hudinfo::EngineKilled();
    const float gap = tc.id ? hullGap(tc, std::max(radius, 30.0f)) : 0.0f;
    // phase 2: the engine back on (pressed again every 0.8 s until the game shows it), then full reverse to a stop at
    // weapons range - the coast ended where the braking from its speed needs to begin (the first build kept coasting to
    // weapons range at 290 m/s with the engine off and struck the hull)
    if (g_coast.phase == 2)
    {
        // (no blind second press: the game's state lags a press, and the second one turned the engine off again - the
        // ship sat at 3-24 m/s, engine off, through the hold; again only if it still reads off 1.5 s after the last)
        if (killed && g.time - g_coast.killAt > 1.5f)
        {
            out.engineKill = true;
            g_coast.killAt = g.time;
        }
        // (to 60 m/s: the hold's run along the hull starts with speed - "zero throttle is a good way to get killed")
        if (!tc.id || speed < 60.0f || gap < 150.0f || g.time - g_coast.at > 12.0f)
        {
            events::Push("autopilot coast stopped gap=%.0f speed=%.0f engine=%s", gap, speed, killed ? "off" : "on");
            g_coast.done = g_coast.target;
            g_coast.phase = 0;
            return;
        }
        out.coast = false;
        out.pursue = false, out.station = false, out.steerTo = false;
        out.brake = true; // (the controller: throttle 0, reverse while moving)
        return;
    }
    const float stopAt = range + (fm.ok ? flight::BrakeDistance(fm, speed, 20.0f) : 600.0f) + speed * 0.3f;
    const bool shieldDown = self.shield >= 0 && self.shield < 0.15f;
    const char* why = !tc.id || tc.health == 0          ? "target gone"
                      : gap <= stopAt                   ? "the stop point"
                      : shieldDown                      ? "shield almost down"
                      : g.time - g_coast.at > 40.0f     ? "40 s"
                                                        : nullptr;
    if (why)
    {
        if (killed)
            out.engineKill = true; // (the engine back on - then the brake)
        events::Push("autopilot coast end (%s) gap=%.0f speed=%.0f after %.1f s", why, gap, speed, g.time - g_coast.at);
        g_coast.phase = 2;
        g_coast.killAt = g.time;
        g_coast.at = g.time;
        out.coast = false;
        out.brake = true;
        return;
    }
    // (the kill once more if the game did not take it, in the first 3 s only)
    if (!killed && g.time - g_coast.killAt > 1.0f && g.time - g_coast.at < 3.0f)
    {
        out.engineKill = true;
        g_coast.killAt = g.time;
    }
    out.coast = true;
    out.pursue = false, out.station = false, out.steerTo = false, out.brake = false;
    const aim::Target t = aim::Current(true);
    memcpy(out.steerPos, t.valid && fl::TargetId() == tc.id ? t.world : tc.pos, sizeof(out.steerPos));
}

// A server's word can change in flight (an admin's line, docs\MULTIPLAYER.md): what it no longer allows ends, as it would not have started
static bool ServerWordEnds()
{
    const mprules::Allow allow = mprules::Autopilot();
    if (allow == mprules::Allow::Off)
    {
        Log("autopilot: stopped by %s", mprules::Source().c_str());
        StopImpl(StopReason::Command, true, "the server's word");
        return true;
    }
    if (allow == mprules::Allow::Travel && g.mode == Mode::Combat)
    {
        Log("autopilot: fight ended by %s", mprules::Source().c_str());
        StopImpl(StopReason::Restart, true, "the server's word");
        return true;
    }
    return false;
}

void Update(float dt, Out& out)
{
    TrackSystemEntry();
    if (ServerWordEnds())
        return;
    UpdateModes(dt, out);
    AshRideFrame(out); // (`ash_ride`: the loop / the intercept at cruise over the mode; the safety guards below outrank it)
    GeometryGuard(dt, out);
    CruiseLock(out);
    LethalGuard(out);
    LevelOff(dt, out);
    TractorMarkedLoot(dt, out);
    NoTractorOnDestroyMarks(out);
    BumpTrace(dt, out);
    MinefieldTrace(dt, out);
    MotionTrace(dt, &out);
    out.steerLevel = g_set.steerLevel;
    StageNose(out);
    ChargeKeepStation(out);
    CoastGuard(out);
    ChaseCoast(out);
    out.steerCheck = g_set.steerCheck;
}

void TracePlayer(float dt) { MotionTrace(dt, nullptr); }

} // namespace autopilot
