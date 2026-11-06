// Advanced Autopilot - combat: the duel, damage and repairs, the fight loop, countermeasures. Shared state and declarations in
// autopilot_impl.h, the module API in autopilot.h.
#include "autopilot_impl.h"
#include "autopilot_ashride.h"
#include "city_hold.h"
#include "game/hudinfo.h"
#include "lookahead.h"

namespace autopilot
{
// ------------------------------------------------------------------ combat
// The player's way out of fire: afterburner away from `from`, jinking - a new offset of
// kExtendJink degrees around the away direction every 0.7 s - with slide bursts. Shared by the extend and the retreat
// from fixed guns. `jinkDeg` >= 0 overrides the jink angle; `burner` false = no afterburner and no slides (both would
// cancel a cruise charge: the retreat's charge and cruise phases jink without them).
// The extend's constants (the values of the deleted extend_* settings, 31 Oct 2026, AUD-A1-5):
static const float kExtendQuiet = 1.5f;    // resume after this long without a hit (s)
static const float kExtendMax = 8.0f;      // at most this long (s)
static const float kExtendJink = 40.0f;    // jink offset from straight away (degrees)
static const float kExtendPatience = 3.0f; // still hit after this long = futile, fight instead (s)
static void JinkAway(const fl::Kinematics& k, const float from[3], float dt, Out& out, float jinkDeg = -1,
                     bool burner = true)
{
    float d[3] = {k.pos[0] - from[0], k.pos[1] - from[1], k.pos[2] - from[2]};
    float len = geom::Len(d);
    if (len < 1)
    {
        memcpy(d, k.vel, sizeof(d));
        len = std::max(1.0f, geom::Len(d));
    }
    for (float& x : d)
        x /= len;
    g.combat.jinkTimer -= dt;
    if (g.combat.jinkTimer <= 0)
    {
        g.combat.jinkTimer = 0.7f;
        g.combat.jinkPhi = static_cast<float>(rand() % 360) * 3.14159265f / 180.0f;
        g.combat.dodgeDir = DodgeDirection();
        g.combat.dodgeTimer = 0;
    }
    // Basis around the away direction: right = d x up, up' = right x d.
    float r[3] = {d[2], 0, -d[0]};
    float rl = sqrtf(r[0] * r[0] + r[2] * r[2]);
    if (rl < 1e-3f)
    {
        r[0] = 1;
        r[2] = 0;
        rl = 1;
    }
    r[0] /= rl;
    r[2] /= rl;
    float u[3];
    geom::Cross(u, r, d);
    const float th = (jinkDeg >= 0 ? jinkDeg : kExtendJink) * 3.14159265f / 180.0f;
    for (int i = 0; i < 3; ++i)
    {
        const float side = r[i] * cosf(g.combat.jinkPhi) + u[i] * sinf(g.combat.jinkPhi);
        out.steerPos[i] = k.pos[i] + (d[i] * cosf(th) + side * sinf(th)) * 2000.0f;
    }
    out.steerTo = true;
    out.afterburn = burner;
    g.combat.dodgeTimer += dt;
    out.slide = burner && g_set.slideTime > 0 && g.combat.dodgeTimer < g_set.slideTime ? g.combat.dodgeDir : 0;
}

void UpdateCombat(float dt, Out& out)
{
    // Never cruise in a fight (user rule; the flee below hands it to the game's goto): a cruise still running from the
    // travel before the fight, or one a game behaviour started, was left on ("leave as it is") - the ship passed its
    // targets at 300 m/s and hit almost nothing (job board Kill Ships vs level-1 Rogues: 13 hull dps of 272).
    out.cruise = 0;
    const float hull = ai::PlayerHealth();
    const float damage = RecentDamage(hull);
    for (auto it = g.combat.ignored.begin(); it != g.combat.ignored.end();)
        it = it->second <= g.time ? g.combat.ignored.erase(it) : std::next(it);

    // Target bookkeeping: a destroyed target disappears from the game's objects.
    if (g.target)
    {
        const space::Contact c = FindContact(g.target);
        // Damage dealt (benchmark: per firing second): the target's hull points lost while it is ours.
        const float hp = c.id && c.health > 0 && c.maxHit > 0 ? c.health * c.maxHit : 0.0f;
        if (g.combat.dealtTarget == g.target && hp < g.combat.dealtHp)
            g.combat.dealt += g.combat.dealtHp - hp;
        float shCur = 0, shMax = 0;
        bool shUp = false;
        const float sh = c.id && c.ship && space::ShieldOf(c.id, shCur, shMax, shUp) ? shCur : -1.0f;
        if (g.combat.dealtTarget == g.target && sh >= 0 && g.combat.dealtShieldPrev > sh)
            g.combat.dealtShield += g.combat.dealtShieldPrev - sh;
        g.combat.dealtShieldPrev = sh;
        g.combat.dealtTarget = g.target;
        g.combat.dealtHp = hp;
        if (!c.id || c.health == 0)
        {
            ++g.combat.kills;
            events::Push("autopilot target-destroyed id=%u kills=%u", g.target, g.combat.kills);
            g.target = 0;
            g.combat.chosen = false;
            g.combat.pickTimer = 0;
        }
    }
    // The fight read from the game's data (Assess) twice a second, whatever the target rules: threat, attackers,
    // nearest hostile and loot feed flee, repairs and the loot phase (was: only with the target choice, so a target
    // the player chose left them stale for the whole fight and a flight never saw the hostiles fall behind, 18 Oct
    // 2026). Target choice from it; a target the player chose stays until it is destroyed. engage 0: only that one.
    g.combat.pickTimer -= dt;
    if (g.combat.pickTimer <= 0)
    {
        g.combat.pickTimer = 0.5f; // polling interval of the assessment (forced at once by kills and a lost enemy)
        Reassess();
        const u32 pick = g_sit.best;
        // a big target worked from its blind spot is kept until it dies (M10 22 Oct 2026: three experimental ships,
        // the pick flipped between them every second, no spot was ever reached, dead)
        const bool holdBig =
            g.combat.blindOn && g.target && g.combat.blindTarget == g.target && FindContact(g.target).id;
        // a live target is kept at least target_dwell seconds (M09 22 Oct 2026, the user: "indecisiveness
        // while targeting the shield generators" - the pressed rule's shield hysteresis and the nearer-wins ranking
        // flipped generator <-> attacking fighter every ~3 s, each switch a new approach and nothing fired)
        const bool dwell = g.target && g.time - g.combat.targetSince < g_set.targetDwell && FindContact(g.target).id;
        // a static target being held is fired upon until it dies (speedrun #1: "as soon as a Nomad
        // generator was targeted, it was fired upon without pause until it was destroyed") - only an emergency (hull
        // under 35 %) lets the ranking switch to the attackers
        const float hullNow = ai::PlayerHealth();
        const bool holdStatic = g.hold.on && g.hold.target == g.target && g.target && FindContact(g.target).id &&
                                !(hullNow >= 0 && hullNow < 0.35f);
        // a live enemy ship is finished before the next (`finish_target`; "when dealing with enemy
        // NPCs (and especially when under speedrun conditions), the correct move is to still finish the current target.
        // The idea is speed and efficiency" - the ranking's attacker bonus (-800) beat the current target's (-400), so
        // every new attacker took the guns off a half-dead one) - unless the hull is under 35 % (as the static hold) or
        // it has run beyond 1 km (the chase limit gives it up then)
        bool finish = false;
        if (g_set.finishTarget && g.target && !holdStatic && !(hullNow >= 0 && hullNow < 0.35f))
        {
            const space::Contact cur = FindContact(g.target);
            finish = cur.id && cur.ship && cur.health > 0 && cur.attitude < 0 && cur.distance < 1000.0f;
        }
        if (!(g.combat.chosen && g.target) && !holdBig && !dwell && !holdStatic && !finish &&
            !(g_set.engage == 0 && g.combat.kills > 0) && g.tactic != Tactic::Flee && pick != g.target && pick)
        {
            g.target = pick;
            g.combat.targetSince = g.time;
            g.combat.outOfRange = 0;
            events::Push("autopilot target id=%u", pick);
        }
    }
    if (g.target && fl::TargetId() != g.target)
        space::SetTarget(g.target); // the game's own target selection (HUD, lead cross, the guns' target)
    const aim::Target t = aim::Current(true); // (the ship's frame: the same in turret view)
    const bool haveEnemy = g.target && t.valid && fl::TargetId() == g.target;

    // Chase limit: a target that stays out of gun range (fleeing, cruising away) is given up for 30 s. Never a solar: it
    // cannot run (M12 22 Oct 2026: the third Shield Generator 3.2 km off was "given up" at once whenever the ship was
    // still moving away - closing < 0 - and the ship fought endless Nomads instead until it died).
    if (haveEnemy && !g.combat.chosen)
    {
        // (not while running to a big ship's flank: out of gun range on purpose - the flank point lies >= 900 m off it,
        // and the gunboat was given up 8 s into that run, then ignored 30 s: "tactic=seek", fights +22-35 s)
        // (nor while it closes on us or a head-on pass is flown - gate mpb12, the fight matrix: the pass reversed from 1000
        // m, the heavy came in at 1714 -> 686 m over 8 s and was "given up" outside the 600 m reach; 30 s with no target,
        // every gun silent, the owner watching)
        // (nor the objective's chase - chase_kill's fleeing ship is out of reach by nature: M02 sc1, 31 Oct 2026, Ashcroft
        // "given up" at 2012 m 17 s into the cruise chase, "no enemies left", hold, the chase restarted 2 km further back)
        // (...nor the escort `ash_ride` left behind on purpose while it loops near the spawn)
        const bool chased = (g_set.chaseKill && g_obj.chaseShip && g.target == g_obj.chaseShip) || AshRideFlying();
        g.combat.outOfRange = t.inRange || g.combat.flanking || t.closing > 10.0f || g.combat.headOn || chased
                                  ? 0
                                  : g.combat.outOfRange + dt;
        if ((g.combat.outOfRange > 8.0f || (t.dist > 2500.0f && t.closing < 0)) && !chased && FindContact(g.target).ship)
        {
            g.combat.ignored[g.target] = g.time + 30.0f;
            events::Push("autopilot target-given-up id=%u dist=%.0f", g.target, t.dist);
            g.target = 0;
            g.combat.pickTimer = 0;
            return;
        }
    }

    // No progress: a target our guns hit for 6 s on target whose hull never moves is invulnerable for now (mission
    // scripts toggle it: M01b's platforms, `Act_Invulnerable`, generated msn_objects.tsv) - given up for 20 s, another
    // target (or the wait) instead of pouring fire into it. Only time on target within gun reach counts (M08 22 Oct 2026:
    // a blind-spot hold 400-950 m off Tekagi's platforms "aimed" for 6 s out of reach, both were given up, and the marked
    // transport was shot instead - mission failed).
    if (haveEnemy)
    {
        const space::Contact tc = FindContact(g.target);
        if (g.combat.progressTarget != g.target)
        {
            g.combat.progressTarget = g.target;
            g.combat.progressHull = tc.health;
            g.combat.progressOnTarget = g.combat.onTargetInReach;
        }
        else if (tc.health >= 0 && tc.health < g.combat.progressHull - 0.001f)
        {
            g.combat.progressHull = tc.health;
            g.combat.progressOnTarget = g.combat.onTargetInReach;
        }
        else if (g.combat.onTargetInReach - g.combat.progressOnTarget > 6.0f && tc.health >= 0)
        {
            float sh = 0, shMax = 0;
            bool up = false;
            const bool shieldHolds = space::ShieldOf(g.target, sh, shMax, up) && up && sh > 0;
            if (!shieldHolds) // a shield taking the hits is progress too, unseen in the hull
            {
                g.combat.ignored[g.target] = g.time + 20.0f;
                events::Push("autopilot target-invulnerable id=%u hull=%.2f on-target=%.0fs", g.target, tc.health,
                             g.combat.onTargetInReach - g.combat.progressOnTarget);
                g.target = 0;
                g.combat.progressTarget = 0;
                g.combat.pickTimer = 0;
                return;
            }
            g.combat.progressOnTarget = g.combat.onTargetInReach;
        }
    }

    // Repairs, the NPC way: nanobots / shield batteries when the damage passes the personality's thresholds, with
    // its pre / post delays (defaults when the pilot has none). A use that changes no count backs off for 10 s.
    ai::Self self;
    ai::ReadSelf(self);

    DamageAndRepairs(dt, out, self);
    const float pressure = g.stats.pressure; // this frame's (DamageAndRepairs)

    // The race (FightOdds, below): who destroys whom first, from the loadouts and what the fight measures.
    const Odds& race = FightOdds(self);
    // Leave the fight (the game's goto away from where the threat is, with cruise): below the hull threshold (the
    // personality's flee value or `flee`), or outmatched and losing (caution: while the hull falls below 60 % / 80 %,
    // the measured race lost 4:1 / 2:1 - the head count in fighter units only where no loadouts are read: it left a
    // fight the strong ship wins, 4 heavy fighters at hull 0.58, threat 9.9 vs strength 4.1).
    // multiplayer: mp_caution / mp_flee (a lost ship is lost there - the owner)
    const bool mp = space::Multiplayer();
    const int caution = mp ? g_set.mpCaution : g_set.caution;
    const float fleeAt = mp && g_set.mpFlee >= 0 ? g_set.mpFlee
                         : g_set.flee >= 0       ? g_set.flee
                                                 : g_p.Job.flee_when_hull_damaged_percent;
    const float odds = caution >= 2 ? 1.0f : 1.6f, losing = caution >= 2 ? 0.8f : 0.6f;
    // (0.25 here left a fight the strong ship had won the run before - 4 heavy fighters, hull 0.58; wins measured as
    // low as 0.08: the line is retreat_odds, doubled at caution 2)
    const bool raceLost = race.ships > 0
                              ? race.measured && race.odds < (caution >= 2 ? 2.0f : 1.0f) * g_set.retreatOdds
                              : g_sit.threat > Strength(self) * odds;
    const bool outmatched = caution > 0 && raceLost && hull >= 0 && hull < losing;
    // Hopeless in free roam ("in free roam the player is not in a scripted do-or-die scenario, so
    // that's when 'retreat when it's useless to try to win' comes into play"): the threat 4x our strength or more -
    // leave as soon as the fight starts costing us (shield down or hull hit), not after the consumables are spent and
    // the hull is at half (4 heavy Rheinland fighters: 16x, every unit spent, fled at 0.52, died). A mission (an
    // objective on the HUD) is do-or-die: never.
    ObjectiveStep mst = {};
    const bool inMission = HostObjectiveNow(&mst) != 0 && mst.textId != 0;
    // Judged at first contact, before their guns reach us (4 heavy fighters, 19 Oct 2026, survival from the fight's
    // start: waiting for the shield to fail then a jinking run 56 s, fighting then running 50 s, fighting on 14 s, a run
    // to a base 2.5 km away 17 s - the decision came too late every time).
    // ... and judged by firepower too: the race to destroy each other (FightOdds) lost 4:1 or worse - the units above
    // undercount a few heavily armed ships, and bursts end a fight faster than a hull threshold reacts.
    if (race.ships > 0 && g.combat.oddsLogged < (race.measured ? 2 : 1))
    {
        g.combat.oddsLogged = race.measured ? 2 : 1;
        events::Push(
            "autopilot odds=%.2f %s ships=%d our-dps=%.0f their-dps=%.0f our-pot=%.0f their-pot=%.0f our-hp=%.0f "
            "their-hp=%.0f threat=%.1f strength=%.1f",
            race.odds, race.measured ? "measured" : "prior", race.ships, race.ourDps, race.theirDps, race.ourPot,
            race.theirPot, race.ourHp, race.theirHp, g_sit.threat, Strength(self));
    }
    // Calibrated on 48 fights (both ships, 12 kinds, each also flown never leaving: all winnable): the
    // loadout prior is rough - winnable fights read 0.18-0.25 (5 level-1 pirates, 3 level-6 close, 2 level-9 elites:
    // left and died) - so it decides alone only when extreme (< 0.4 x retreat_odds = 0.02: 4 heavy fighters read
    // 0.01, and leaving at once matters there); else the measured race decides (wins read as low as 0.08 - 2 heavy
    // fighters vs the starter ship, won 3 of 3), held 5 s.
    const float limit = race.measured ? g_set.retreatOdds : g_set.retreatOdds * 0.4f;
    g.combat.outgunnedFor = race.ships > 0 && race.odds < limit ? g.combat.outgunnedFor + dt : 0.0f;
    const bool outgunned = g.combat.outgunnedFor > 0 && (!race.measured || g.combat.outgunnedFor >= 5.0f);
    // The head count (fighter units) only where the loadouts give no race: more ships joining pushed it past 4x in a
    // fight being won (3 pirates, odds 0.70).
    const bool hopeless = !inMission && caution > 0 && hull >= 0 &&
                          (race.ships > 0 ? outgunned : g_sit.threat >= 4.0f * Strength(self));
    if (hopeless && g.tactic != Tactic::Flee && !g_obj.on)
    {
        SetTactic(Tactic::Flee);
        events::Push("autopilot flee hopeless threat=%.1f strength=%.1f odds=%.2f hull=%.2f shield=%.2f", g_sit.threat,
                     Strength(self), race.odds, hull, self.shield);
    }
    // Not in a mission the objective mode flies: its battles have tethers ("You fled the battle" - M04 Benford, 19 Oct
    // 2026: fled at hull 0.36, failed); the harness's mission battles run without fleeing for the same reason.
    if (g.tactic != Tactic::Flee && !g_obj.on && ((fleeAt > 0 && hull >= 0 && hull < fleeAt) || outmatched))
    {
        SetTactic(Tactic::Flee);
        events::Push("autopilot flee hull=%.2f threat=%.1f strength=%.1f", hull, g_sit.threat, Strength(self));
    }
    if (g.tactic == Tactic::Flee)
    {
        if (g_sit.nearest > 3000.0f)
        {
            Stop(StopReason::Fled);
            return;
        }
        // The human retreat (was: jink until 1.5 s quiet, then the game's straight goto with cruise -
        // a straight 5 s charge is an easy target: 4 heavy fighters, 19 Oct, dead 8 s into such a run):
        //  0 break contact: afterburner + jinks + slide bursts away from the threat while hits land;
        //  1 charge: in a lull (no hit for 0.6 s, no rounds in flight at us, no homing missile on us) request cruise and
        //    keep jinking without afterburner / slides (both cancel the charge); heavy hits again (pressure >= 0.5 in the
        //    first 2 s of the charge, or no cruise engine) -> back to 0;
        //  2 cruising: narrow jinks (10 deg) away, until 3 km clear; a cruise that drops (disrupted) -> back to 0.
        fl::Kinematics k = {};
        if (!fl::PlayerKinematics(k))
            return;
        if (Op() == ai::OpGoto)
            FreeFlightNow(); // we steer ourselves (a game goto left from an older flee)
        flight::Model fm;
        const bool haveFm = flight::ReadModel(fm) && fm.cruise.ok();
        const float speed = geom::Len(k.vel);
        // cruising = near cruise speed (the engine reports engaged with 0 s elapsed at the request itself, and the
        // afterburner's ~200 m/s passed an "above engine speed" test: phase 2 began 16 ms into the charge)
        const bool cruising =
            haveFm && fm.cruiseEngaged && speed > 0.75f * (fm.cruiseSpeed > 0 ? fm.cruiseSpeed : 300.0f);
        bool missileOnUs = false;
        for (const space::Missile& m : space::Missiles())
            missileOnUs = missileOnUs || (m.targetsPlayer && m.closing > 1);
        const bool roundsInbound = !space::IncomingRounds(1.0f, 0, self.shieldArch).empty();
        const bool lull = g.combat.quiet >= 0.6f && !roundsInbound && !missileOnUs;
        // The lull must outlast the charge (user's human retreat; 20 Oct 2026 vs 4 heavy fighters: charging at 600-700 m
        // dropped the ship from ~195 to ~80 m/s and they closed to gun range before the cruise engaged - worse than
        // staying on the afterburner): the nearest hostile, closing at its current rate plus the speed we give up, must
        // not reach gun range (~900 m for fighters' guns) within the charge time left.
        if (dt > 0 && g.combat.lastNearest >= 0 && g_sit.nearest < 1e8f)
            g.combat.nearestRate +=
                ((g_sit.nearest - g.combat.lastNearest) / dt - g.combat.nearestRate) * std::min(1.0f, dt * 2.0f);
        g.combat.lastNearest = g_sit.nearest < 1e8f ? g_sit.nearest : -1;
        const float chargeLeft = haveFm ? std::max(0.0f, fm.cruiseChargeTime - fm.cruiseChargeElapsed) : 5.0f;
        const float closingAfter = -g.combat.nearestRate + std::max(0.0f, speed - (haveFm ? fm.engine.top() : 80.0f));
        const bool chargeFits = g_sit.hostiles == 0 || g_sit.nearest - 900.0f > closingAfter * chargeLeft;
        const RetreatPhase was = g.combat.retreat;
        g.combat.retreatPhaseTime += dt;
        if (g.combat.retreat == RetreatPhase::Break && lull && haveFm && chargeFits)
            g.combat.retreat = RetreatPhase::Charge;
        else if (g.combat.retreat == RetreatPhase::Charge &&
                 (!haveFm || (g.stats.pressure >= 0.5f && g.combat.retreatPhaseTime < 2.0f) ||
                  fm.cruiseChargeElapsed < 0 || (g.combat.retreatPhaseTime > 0.5f && !fm.cruiseEngaged)))
            g.combat.retreat =
                RetreatPhase::Break; // hit hard early, cruise disrupted (elapsed < 0: CRUISE_DISRUPT_TIME
                                     // counting) or dropped
        else if (g.combat.retreat == RetreatPhase::Charge && cruising)
            g.combat.retreat = RetreatPhase::Cruise;
        else if (g.combat.retreat == RetreatPhase::Cruise && haveFm &&
                 (!fm.cruiseEngaged || fm.cruiseChargeElapsed < 0))
            g.combat.retreat = RetreatPhase::Break; // disrupted (or dropped): break contact again
        if (g.combat.retreat != was)
        {
            g.combat.retreatPhaseTime = 0;
            events::Push("autopilot retreat phase=%s quiet=%.1f rounds=%d missile=%d speed=%.0f nearest=%.0f rate=%.0f "
                         "closing_after=%.0f charge_left=%.1f",
                         g.combat.retreat == RetreatPhase::Break    ? "break"
                         : g.combat.retreat == RetreatPhase::Charge ? "charge"
                                                                    : "cruise",
                         g.combat.quiet, roundsInbound ? 1 : 0, missileOnUs ? 1 : 0, speed, g_sit.nearest,
                         g.combat.nearestRate, closingAfter, chargeLeft);
        }
        float behind[3];
        const float* from = g_sit.center;
        if (g_sit.hostiles == 0)
        {
            for (int i = 0; i < 3; ++i)
                behind[i] = k.pos[i] - k.vel[i]; // nothing to run from: keep going the way we fly
            from = behind;
        }
        if (g.combat.retreat == RetreatPhase::Break)
        {
            out.cruise = 0;
            JinkAway(k, from, dt, out);
        }
        else
        {
            out.cruise = 1;
            JinkAway(k, from, dt, out, g.combat.retreat == RetreatPhase::Charge ? kExtendJink : 10.0f, false);
        }
        return;
    }

    // Tractor all, the player's reflex: whenever worthwhile loot is within the tractor's reach and the hold has room,
    // in the fight too (the beam works while flying and shooting).
    g.tractorTimer -= dt;
    if (g_set.loot && g_sit.lootInReach > 0 && self.holdFree != 0 && g.tractorTimer <= 0)
    {
        out.collectAll = true;
        g.tractorTimer = 1.5f;
    }

    if (!haveEnemy)
    {
        // No enemy (or the new target is not selected yet): stop a headless trail and keep flying towards the next
        // target (or the nearest hostile ship at any distance) - never coast in a fight - until the next pick or the
        // end.
        if (g.combat.trailTarget)
        {
            FreeFlightNow();
            g.combat.trailTarget = 0;
        }
        // `ash_ride`: the escort died (the NPCs' kill) while the ride loops near the spawn - no "no enemies left" stop, no
        // hold (its Idle stops the ship), no loot run: the guard keeps the cruise until the chase takes him
        if (AshRideFlying())
            return;
        // (Not towards a hostile a station hides - inside its bays or behind it: that flew the ship into the Rogue Base,
        // M01b. One behind a field rock is still sought: fights in asteroid fields go on.)
        for (const space::Contact& c : Contacts())
            if (c.ship && c.health != 0 && (c.id == g.target || (!g.target && c.attitude <= -0.6f)) &&
                std::find(g_sit.behindStation.begin(), g_sit.behindStation.end(), c.id) == g_sit.behindStation.end())
            {
                out.steerTo = true;
                memcpy(out.steerPos, c.pos, sizeof(out.steerPos));
                break;
            }
        if (!out.steerTo && !g_sit.behindStation.empty())
            out.brake = true; // they come out to us: wait (the retreat below takes over if the station's guns reach us)
        // Seek, unless a loot phase is running (it decides below; was: Loot without a single-item target - the
        // tractor-all phase - switched to Seek here and back to Loot below, every frame).
        if (g.tactic != Tactic::Loot)
            SetTactic(Tactic::Seek);
        if (g_set.engage == 0 && g.combat.kills > 0 && g.mode == Mode::Combat) // an escort goes on protecting
        {
            Stop(StopReason::TargetDestroyed);
            return;
        }
        // The fight is over when an assessment taken after the enemy was lost (forced at once: pickTimer 0) finds nothing
        // to fight in reach and nobody attacking us - game state (was: 1 s before looting, 4 s before the end,
        // 18 Oct 2026). A new pick or an attacker keeps it going as before.
        if (!g.combat.lostEnemy)
        {
            g.combat.lostEnemy = true;
            g.combat.quietFrom = g.combat.assessments + 1;
            g.combat.pickTimer = 0;
        }
        // Not over while hostiles hidden nearby are about to come out (the Rogue Base's bays: combat said
        // "no enemies left" every few seconds, rested, the harness restarted it - a flapping loop at the base).
        const bool fightOver = g.combat.assessments >= g.combat.quietFrom && g_sit.best == 0 && g_sit.attackers == 0 &&
                               g_sit.hiddenNear == 0;
        // The fight is over: collect the loot nearby the way a player does - fly within the tractor's reach of it, stop,
        // tractor all (the reflex above). Only with the hold nearly full, one item at a time, the most valuable first
        // (the game's Loot behaviour flies there and tractors it). At most 30 s; enemies showing up end it.
        const bool holdTight = self.holdSize > 0 && self.holdFree >= 0 && self.holdFree < self.holdSize * 0.15f;
        const float reach = self.tractorRange > 0 ? self.tractorRange : 1500.0f;
        // (Not while still hit: fixed guns first, see below.)
        if (g_set.loot && g.mode == Mode::Combat && fightOver && g_sit.hostiles == 0 && !g_sit.loot.empty() &&
            !holdTight && g.combat.lootTime < 30.0f && g.stats.pressure <= 0)
        {
            g.combat.lootTime += dt;
            if (g.tactic != Tactic::Loot)
                SetTactic(Tactic::Loot);
            if (g_sit.lootNearest > reach * 0.8f)
            {
                out.steerTo = true;
                memcpy(out.steerPos, g_sit.lootNearestPos, sizeof(out.steerPos));
            }
            else
            {
                out.steerTo = false;
                out.brake = true; // hold position inside the reach while the beam works
            }
            return; // the end waits for the loot
        }
        if (g_set.loot && holdTight && g.mode == Mode::Combat && fightOver && g_sit.hostiles == 0 &&
            !g_sit.loot.empty() && g.stats.pressure <= 0)
        {
            const bool lootGone = g.combat.lootTarget && std::find(g_sit.loot.begin(), g_sit.loot.end(),
                                                                   g.combat.lootTarget) == g_sit.loot.end();
            g.combat.lootTime += dt;
            // A new item when: none yet, it is gone, the 20 s safety limit, or the game is not running our Loot once it
            // has acted on it (state: was "after 1 s").
            if (!g.combat.lootTarget || lootGone || g.combat.lootTime > 20.0f ||
                (SubmitSettled() && Op() != ai::OpLoot))
            {
                if (g.combat.lootTarget && !lootGone)
                    g.combat.ignored[g.combat.lootTarget] = g.time + 60.0f; // could not take it
                g.combat.lootTarget = 0;
                for (u32 id : g_sit.loot)
                    if (!g.combat.ignored.count(id))
                    {
                        g.combat.lootTarget = id;
                        break;
                    }
                g.combat.lootTime = 0;
                if (g.combat.lootTarget)
                {
                    SetTactic(Tactic::Loot);
                    space::SetTarget(g.combat.lootTarget);
                    ai::Directive d;
                    d.op = ai::OpLoot;
                    d.target = g.combat.lootTarget;
                    d.client = true;
                    const int rc = SubmitNow(d);
                    events::Push("autopilot loot id=%u rc=%d", g.combat.lootTarget, rc);
                }
            }
            if (g.combat.lootTarget)
            {
                out.steerTo = false;
                return; // the end waits for the loot
            }
        }
        // No loot phase ran this frame: leave it (once).
        if (g.tactic == Tactic::Loot)
        {
            g.combat.lootTarget = 0;
            SetTactic(Tactic::Seek);
        }
        // No ship to fight, but still hit: a station's or a platform's guns (not ships, so no "enemies"), or the only
        // hostiles still launching from a station. Never rest under fire (M01b 19 Oct 2026: the ship stopped 50 m from
        // the Rogue Base between waves and died under its guns). Evasion by the numbers (retreating
        // in a straight line is the extreme end, mathing it out): NPC guns lead a constant-velocity target, so a
        // straight flight at any speed is hit like a still one; shots fly ~700 m/s (weapon_equip.ini median; the Rogue
        // turrets 700): 0.43 s at 300 m, 0.71 s at 500 m, 1.0 s at 700 m. A strafe reversal (50 m/s, measured; 20000 N on
        // 150 kg = 133 m/s^2, ~0.4 s to full) right after a shot leaves it ~19 m off at 300 m (the Starflier's radius is
        // ~10 m) and ~60 m off at 700 m; under ~250 m the shot lands before a reversal acts. So: hold 500-900 m from the
        // guns, near the fight (within 1.5 km of where this began: the mission's area), strafing in random directions
        // reversed every 0.4-0.8 s (faster than the shots fly) and drifting sideways, not away. Only at the extreme
        // (hull below 30 % and still hit) a jinking retreat, bounded to 2.5 km from the guns / 3 km from the start.
        if ((fightOver || g_sit.best == 0) && g.mode == Mode::Combat && g.stats.pressure > 0)
        {
            fl::Kinematics k = {};
            if (fl::PlayerKinematics(k))
            {
                const space::Contact* gun = nullptr;
                const std::vector<space::Contact> all = Contacts();
                for (const space::Contact& c : all)
                {
                    float att = 0;
                    if (!c.ship && c.distance < 5000 && (!gun || c.distance < gun->distance) &&
                        space::AttitudeToPlayer(c.id, att) && att <= -0.6f)
                        gun = &c;
                }
                if (!g.combat.gunRetreat)
                {
                    g.combat.gunRetreat = true;
                    memcpy(g.combat.gunRetreatFrom, k.pos, sizeof(g.combat.gunRetreatFrom));
                    g.combat.jinkTimer = 0;
                    SetTactic(Tactic::Extend);
                    events::Push("autopilot evade-guns from=%u dist=%.0f pressure=%.2f hull=%.2f", gun ? gun->id : 0,
                                 gun ? gun->distance : 0.0f, g.stats.pressure, self.hull);
                }
                out.brake = false; // (the wait for launching fighters above)
                const float o[3] = {k.pos[0] - g.combat.gunRetreatFrom[0], k.pos[1] - g.combat.gunRetreatFrom[1],
                                    k.pos[2] - g.combat.gunRetreatFrom[2]};
                const float fromStart = geom::Len(o);
                if (self.hull >= 0 && self.hull < 0.3f)
                {
                    // The extreme end: retreat - still jinking - out of reach, bounded.
                    if ((gun && gun->distance >= 2500) || fromStart >= 3000)
                        out.brake = true;
                    else
                    {
                        float from[3];
                        for (int i = 0; i < 3; ++i) // no gun known: away from where we came from (straight on)
                            from[i] = gun ? gun->pos[i] : k.pos[i] - k.vel[i];
                        JinkAway(k, from, dt, out);
                    }
                    return;
                }
                // Strafe reversals at random 0.4-0.8 s, a random direction each time; the drift sideways with them.
                g.combat.jinkTimer -= dt;
                if (g.combat.jinkTimer <= 0)
                {
                    g.combat.jinkTimer = 0.4f + static_cast<float>(rand() % 400) / 1000.0f;
                    g.combat.jinkPhi = static_cast<float>(rand() % 360) * 3.14159265f / 180.0f;
                    g.combat.dodgeDir = 1 + rand() % 4;
                }
                out.slide = g.combat.dodgeDir;
                if (gun)
                {
                    float d[3] = {k.pos[0] - gun->pos[0], k.pos[1] - gun->pos[1], k.pos[2] - gun->pos[2]};
                    const float r = std::max(1.0f, geom::Len(d));
                    for (float& x : d)
                        x /= r;
                    float rt[3] = {d[2], 0, -d[0]};
                    const float rl = std::max(1e-3f, sqrtf(rt[0] * rt[0] + rt[2] * rt[2]));
                    rt[0] /= rl, rt[2] /= rl;
                    float up[3];
                    geom::Cross(up, rt, d);
                    float side[3];
                    for (int i = 0; i < 3; ++i)
                        side[i] = rt[i] * cosf(g.combat.jinkPhi) + up[i] * sinf(g.combat.jinkPhi);
                    // Radial part: open to 500 m, come back inside 900 m and the area bound; sideways otherwise.
                    const float radial = r < 500 ? 600.0f : (r > 900 || fromStart > 1500 ? -300.0f : 0.0f);
                    for (int i = 0; i < 3; ++i)
                        out.steerPos[i] = k.pos[i] + d[i] * radial + side[i] * 500.0f;
                    out.steerTo = true;
                    out.steerThrottle = r < 500 ? 1.0f : 0.6f;
                }
                return;
            }
        }
        g.combat.gunRetreat = false;
        if (fightOver)
        {
            if (g.mode == Mode::Escort)
            {
                SetTactic(Tactic::Rejoin);
                return;
            }
            Stop(StopReason::NoEnemiesLeft);
            // Then rest the way a player does (after the prison ship fight the ship sat 40 s in
            // empty space, the ship it saved off screen - "looks bad"): the hold, which rests beside the mission's
            // object or the fight's subject, level (ScenicPoint / HoldLook). Any next action replaces it.
            if (Start(Mode::Hold, 0, nullptr))
                g.hold.scenic = true;
        }
        return;
    }
    g.combat.lostEnemy = false;
    // An enemy during a loot phase: back to the fight (the pursuit runs only for Engage / Seek / Evade).
    if (g.tactic == Tactic::Loot)
    {
        g.combat.lootTarget = 0;
        g.combat.lootTime = 0;
        SetTactic(Tactic::Engage);
    }

    // Evasion, reactive and blended into the attack (the pressure: see the damage tracking above).
    out.weave = g.combat.evasion;
    out.weaveRadius = g_set.weaveRadius;
    g.combat.evadeCooldown -= dt;
    g.combat.breakCooldown -= dt;
    g.combat.extendCooldown -= dt;
    g.combat.hotTime = g.combat.evasion >= 0.6f ? g.combat.hotTime + dt : 0.0f;

    // Extend, the player's answer to sustained fire (user 17 Oct 2026; the playlist study: under fire the afterburner
    // share rises from 22 to 39 %, the attacker goes off screen, a hot spell lasts ~6 s, the attack resumes ~3 s after
    // the hits stop): afterburner away from the hostiles' threat centre, jinking (a new offset of kExtendJink degrees
    // every 0.7 s) with slide bursts, the guns still firing when the target crosses the cone; back to the relentless
    // pursuit once no hit came for kExtendQuiet seconds (min 2 s, max kExtendMax). Triggers: evasion >= 0.6 for 0.8 s,
    // or the shield below 35 % while being hit.
    // Only under the objective mode's survival stance (the general `extend` switch lost its benchmark and was deleted
    // 31 Oct 2026, AUD-A1-5) ("hull is sacred" -
    // break off while the shield is low so it regenerates; kills do not matter there; the 2020 M04 opening: the shield
    // never below 0.43): the trigger is the shield below 50 % while hit (35 % otherwise), and escorting, the run ends
    // before 2000 m from the escorted ship (missions tether to it: Benford fails beyond 2500 m of Juni).
    // ... only while the fight is being lost (M04 Freeport 4: "combat behavior leaves a lot to be
    // desired, the ship took forever to start engaging" - extend after extend away from bounty hunters it was beating,
    // the target off screen behind us; odds 0.8-6.5, the user fought that one through on batteries in 2020): the race
    // (FightOdds) lost, or without loadouts the threat above our strength. A won fight is fought; batteries cover it.
    const bool raceLosing = race.ships > 0 ? race.odds < 1.0f : g_sit.threat > Strength(self);
    const bool survivalExtend = ObjectiveOn() && g_obj.survival && raceLosing;
    // The head-on pass holds under fire (the fight matrix: "the player ship stops reverse thrusting
    // and starts pushing towards the enemy when it takes fire ... it is to be expected for the player ship to take fire
    // when it's reversing away from an incoming enemy. The point is to maximize time on target while making sure shields
    // don't fail, while making the most out of shields when they're still up"): while the pass is flown, no extend, break
    // or evade as long as the shield is above `headon_shield` (0.25) and the hull above half.
    // ...never with a homing round coming at us: the shield is spent on gunfire, a missile is dodged as ever (gate mpb19,
    // the fight matrix: c705 held its pass under two Mk II missiles, 489 points each on a 1700-point ship - dead in 15 s in
    // a fight it wins in 24-32 s)
    bool missileIn = false;
    for (const space::Missile& m : space::Missiles())
        if (m.targetsPlayer && !m.cruiseDisruptor && m.distance < 1500.0f)
            missileIn = true;
    // ...nor in a fight whose race is being lost (gate mpb20, the fight matrix: c705's starter against two elite pirates,
    // odds 0.18 - the pass held to shield 0.24, the shield never came back, dead 6 s later; it won that fight in 50-65 s
    // in every gate before): the hold is for a race we win
    const bool passHold =
        g.combat.headOn && !missileIn && !raceLosing && self.shield > g_set.headonShield && hull > 0.5f;
    const bool canExtend = !passHold && g_set.evade && survivalExtend &&
                           (g.tactic == Tactic::Engage || (g.tactic == Tactic::Evade && !g.combat.gameEvade));
    const float extendShield = survivalExtend ? 0.50f : 0.35f;
    if (canExtend && g.combat.extendCooldown <= 0 &&
        (g.combat.hotTime >= 0.8f || (self.shield >= 0 && self.shield < extendShield && pressure > 0)))
    {
        SetTactic(Tactic::Extend);
        g.combat.jinkTimer = 0;
        ++g.stats.extends;
        events::Push("autopilot extend evasion=%.2f shield=%.2f hull=%.2f attackers=%d", g.combat.evasion, self.shield,
                     hull, g_sit.attackers);
    }
    if (g.tactic == Tactic::Extend)
    {
        g.stats.extendTime += dt;
        // Running did not shake them (still hit after kExtendPatience: surrounded, faster attackers): fight instead,
        // and do not try again soon.
        const bool futile = g.tacticTime >= kExtendPatience && g.combat.quiet < 0.5f;
        bool leash = false; // escorting: never run out of the mission's tether to the escorted ship
        fl::Kinematics mk = {};
        float esc[3];
        if (g.mode == Mode::Escort && g.escorted && fl::PlayerKinematics(mk) && space::ObjectPos(g.escorted, esc))
        {
            const float dx = mk.pos[0] - esc[0], dy = mk.pos[1] - esc[1], dz = mk.pos[2] - esc[2];
            leash = dx * dx + dy * dy + dz * dz > 2000.0f * 2000.0f;
        }
        if ((g.combat.quiet >= kExtendQuiet && g.tacticTime >= 2.0f) || g.tacticTime >= kExtendMax ||
            futile || leash)
        {
            events::Push("autopilot extend-end after=%.1f quiet=%.1f%s%s", g.tacticTime, g.combat.quiet,
                         futile ? " futile" : "", leash ? " leash" : "");
            SetTactic(Tactic::Engage);
            g.combat.extendCooldown = futile ? 6.0f : 3.0f;
            g.combat.hotTime = 0;
        }
        else
        {
            fl::Kinematics k = {};
            if (fl::PlayerKinematics(k))
                JinkAway(k, g_sit.hostiles > 0 ? g_sit.center : t.world, dt, out); // the threat centre or the target
            else
                out.afterburn = true;
        }
    }

    // (no dodges on a hop between static targets: the hop at speed is the evasion - session 7, M12: evade / engage flipped
    // every 1-4 s under 10-15 attackers and the way to generator 3 grew to 4.7 km; the user's try 3 ignores them there)
    const bool hopping = g.frames - g.hop.frame <= 2;
    // (...nor in a chase_kill chase: the cruise at him is the evasion - probe as2: tactic=evade broke every
    // cruise charge at Ashcroft, the gap grew 740 -> 3354 -> 5535 m, the pod 123 s vs the baseline's 114-120)
    // (...nor while `ash_ride` flies the loop / the intercept at cruise: a game evade would take the ship and the cruise)
    const bool chasing = (g_set.chaseKill && g.target && g.target == g_obj.chaseShip) || AshRideFlying();
    const bool canDodge = !passHold && g_set.evade && g.tactic == Tactic::Engage && !hopping && !chasing;
    const float breakTrigger = g_p.DamageReaction.evade_break_damage_trigger_percent;
    if (canDodge && breakTrigger > 0 && breakTrigger < 1 && damage >= breakTrigger && g.combat.breakCooldown <= 0)
    {
        SetTactic(Tactic::Break);
        BreakAway(t);
        g.combat.breakCooldown = 8.0f;
    }
    else if (canDodge && g.combat.evadeCooldown <= 0 && g.combat.evasion > 0.5f)
    {
        SetTactic(Tactic::Evade);
        // dodge 1: slide bursts on top of the pursuit (the aim stays on the target); dodge 2: the game's Evade (the
        // ship's personality picks the style; slide bursts when it is refused).
        g.combat.gameEvade = g_set.dodge == 2 && GameEvade(g.target);
        g.combat.dodgeDir = DodgeDirection();
        g.combat.dodgeTimer = 0;
        g.combat.evadeCooldown = Pf(g_p.EvadeDodge.evade_dodge_interval_time, 1, 10, 2) + 2.0f;
    }
    const float evadeTime = g.tactic == Tactic::Break ? Pf(g_p.EvadeBreak.evade_break_time, 1, 6, 3)
                                                      : Pf(g_p.EvadeDodge.evade_dodge_time, 1, 6, 2.5f);
    if ((g.tactic == Tactic::Evade || g.tactic == Tactic::Break) && g.tacticTime > evadeTime)
        SetTactic(Tactic::Engage);
    // The game's Evade ends by itself (its dodge is over, or the threat is gone): back to the trail.
    if (g.tactic == Tactic::Evade && g.combat.gameEvade && SubmitSettled() && Op() != ai::OpEvade) // (was: after 0.3 s)
        SetTactic(Tactic::Engage);
    if (g.tactic == Tactic::Evade && !g.combat.gameEvade)
    {
        // Slide bursts of 0.5 s, a new direction every dodge interval (min 0.8 s); stops as the pressure fades.
        g.combat.dodgeTimer += dt;
        if (g.combat.dodgeTimer > std::max(0.8f, Pf(g_p.EvadeDodge.evade_dodge_interval_time, 0.8f, 5, 1)))
        {
            g.combat.dodgeTimer = 0;
            g.combat.dodgeDir = DodgeDirection();
        }
        out.slide = g.combat.dodgeTimer < g_set.slideTime ? g.combat.dodgeDir : 0;
        if (g.combat.evasion < 0.3f)
            SetTactic(Tactic::Engage);
    }
    if (g.tactic == Tactic::Break)
        out.afterburn = true;

    // Fight entry (the fast-dock philosophy): a far target is approached by cruise, dropped where a
    // full-reverse slowdown to fighting speed ends at gun range - the guns come back online as the first shots can hit,
    // no time lost, the least time exposed. Only while not under fire and the cruise run is the faster one (the
    // flight model's plan, the turn included); a running cruise stays until the drop point.
    bool approach = false;
    const bool coasting = CoastActive(); // (the coast guard flies the ship: autopilot_frame.cpp CoastGuard)
    // `big_cruise_in` (M13-MIMIC; R1 cruises 13 s at 300 m/s straight at the Nomad battleship through its interceptors):
    // at a huge static target (radius >= 500 m) the cruise in goes on under fire and in an evade - ab4-b_i1 dropped its
    // charge at 4.5 km on the first evade and flew 200 m/s, ab2-b_i2 / i4 crawled 4.9 -> 1.3 km at 80 m/s for ~45 s
    bool bigCruise = false;
    if (g_set.bigCruiseIn && g.target && !coasting)
    {
        const space::Contact tc = FindContact(g.target);
        if (tc.id && !tc.ship && tc.distance > 2000)
            for (const space::Obstacle& o : space::Obstacles(tc.pos, 50))
                bigCruise = bigCruise || (o.id == g.target && o.radius >= 500.0f);
    }
    if (!coasting && (((g.tactic == Tactic::Engage || g.tactic == Tactic::Seek) && pressure <= 0) || bigCruise) &&
        g.target && self.gunRange > 0)
    {
        fl::Kinematics k = {};
        flight::Model fm;
        if (fl::PlayerKinematics(k) && flight::ReadModel(fm) && fm.cruise.ok())
        {
            const float to[3] = {t.world[0] - k.pos[0], t.world[1] - k.pos[1], t.world[2] - k.pos[2]};
            const float dist = geom::Len(to);
            const float speed = geom::Len(k.vel);
            const float fight = fm.engine.top();
            // Aggressive interception ("stop cruise engines just as it enters weapons range and fire
            // immediately"): the cruise is cut just short of gun range - half a second of travel - and the guns open
            // at once, the speed carried into the attack (was: dropped where a full slowdown to fighting speed would
            // end at gun range - far out, the ship then crawled in: the Battle of Willard's bombers, M03).
            const float drop = speed * 0.5f + speed * dt * 2;
            float way = dist - self.gunRange;
            // (`big_coast`: at a huge static target the cruise ends 300 m outside gun reach of its hull, not its centre -
            // the coast in begins there)
            if (g_set.bigCoast)
            {
                const space::Contact tc = FindContact(g.target);
                if (tc.id && !tc.ship)
                    for (const space::Obstacle& o : space::Obstacles(tc.pos, 50))
                        if (o.id == g.target && o.radius >= 500.0f)
                        {
                            const float h = HullGap(k.pos, tc.pos);
                            way = (h >= 0 ? h : dist - o.radius) - self.gunRange * 0.9f; // (the cruise to weapons range:
                                                                                        // the coast takes over before)
                        }
            }
            // `chase_kill` (lookahead.h ChaseStep): the chased ship faster than our engine's top speed - the cruise charge at
            // once (no 3 km floor) and no drop at gun range: he outruns any fight at engine speed (M02's Ashcroft cruises at
            // ~300 m/s from his ~8 s charge on); the coast guard fires on him by engine-kill coasts (ChaseCoast)
            lookahead::ChaseResult chase;
            if (g_set.chaseKill && g.target && g.target == g_obj.chaseShip)
            {
                space::ShipMotion hm;
                lookahead::ChaseFacts cf;
                cf.gap = dist, cf.mySpeed = speed, cf.engineTop = fight, cf.gunRange = self.gunRange;
                cf.hisSpeed = space::ShipMotionOf(g.target, hm) ? hm.speed : 0.0f;
                cf.cruising = fm.cruiseEngaged, cf.shield = self.shield;
                float fwd[3];
                geom::Forward(fwd, k.rot);
                cf.bearingDeg = dist > 1 ? acosf(Clampf(Dot3(fwd, to) / dist, -1, 1)) * 57.2958f : 0.0f;
                cf.fleeing = ChargeFleeing(g.target); // (`spawn_charge`: the charge at his spawn, whatever his speed)
                chase = lookahead::ChaseStep(cf);
                // spawn_charge's press line, once per ship: the charge started while he is no faster than our engine (the
                // press `fleeing` brought forward) or at once - with the gap the predictor expects at the lock (our press
                // counted 0 s after his: the spawn line's first figure)
                if (cf.fleeing && chase.verdict == lookahead::ChaseVerdict::Charge && !fm.cruiseEngaged &&
                    g_obj.chargeSaid != g.target)
                {
                    g_obj.chargeSaid = g.target;
                    const float sd[3] = {g_obj.chargeSpawn[0] - k.pos[0], g_obj.chargeSpawn[1] - k.pos[1],
                                         g_obj.chargeSpawn[2] - k.pos[2]};
                    const lookahead::GapPrediction gp = lookahead::ChargeGap(dist, 0.0f);
                    events::Push("autopilot spawn-charge cruise at spawn d=%.0f his=%.0f me=%.0f engine=%.0f "
                                 "from_spawn=%.0f bearing=%.0f expect_lock=%.0f min=%.0f target=%u",
                                 dist, cf.hisSpeed, speed, fight, Len3(sd), cf.bearingDeg, gp.lockGap, gp.minGap, g.target);
                }
            }
            if (chase.verdict == lookahead::ChaseVerdict::Charge)
            {
                approach = true;
                SteerTo(out, t.world, 1.0f, 0.35f, 1);
                out.afterburn = false;
                if (!g.combat.approaching)
                    events::Push("autopilot approach cruise dist=%.0f gun=%.0f chase target=%u", dist, self.gunRange,
                                 g.target);
            }
            // Far targets only (user rule; 19 Oct 2026 at the Rogue Base it cruised at 611-623 m against a 600 m gun
            // range - a 5 s charge for 20 m): at least 3 km beyond gun range, whatever the run plan says.
            else if (way > drop && (fm.cruiseEngaged || way > 3000))
            {
                float fwd[3];
                geom::Forward(fwd, k.rot);
                const float cosNose = dist > 1 ? (fwd[0] * to[0] + fwd[1] * to[1] + fwd[2] * to[2]) / dist : 1.0f;
                const flight::RunPlan p =
                    flight::PlanRun(fm, speed, way, fight, fm.cruiseEngaged ? fm.cruiseChargeElapsed : 0,
                                    acosf(Clampf(cosNose, -1, 1)));
                approach = fm.cruiseEngaged ? true : p.cruise;
                if (approach)
                {
                    SteerTo(out, t.world, 1.0f, 0.35f, 1);
                    out.afterburn = false;
                    if (!g.combat.approaching)
                        events::Push("autopilot approach cruise dist=%.0f gun=%.0f%s", dist, self.gunRange,
                                     bigCruise && !(pressure <= 0 && (g.tactic == Tactic::Engage ||
                                                                      g.tactic == Tactic::Seek))
                                         ? " big-cruise-in (under fire)"
                                         : "");
                }
            }
            else if (g.combat.approaching)
                events::Push("autopilot approach drop dist=%.0f speed=%.0f", dist, speed);
        }
    }
    g.combat.approaching = approach;

    // Engage (and slide dodges): lead pursuit (the nose and the reticle on the lead cross, binary throttle with
    // reverse and afterburner: the controller's Pursue) or, with steer = 1, the game's Trail.
    const bool engaging = !approach && !coasting && (g.tactic == Tactic::Engage || g.tactic == Tactic::Seek ||
                                        (g.tactic == Tactic::Evade && !g.combat.gameEvade));
    if (engaging)
    {
        if (g.tactic == Tactic::Seek)
            SetTactic(Tactic::Engage);
        if (g_set.steer == 0)
        {
            out.pursue = true;
            out.pursueBand = g_set.engageDistance;
            out.gunRange = self.gunRange;
            // The ship flies free under pursuit: end a game behaviour still running (trail, an ended evade's goto) -
            // again only once the game has acted on our last submit (state, not a 0.5 s timer).
            if (SubmitSettled() && Op() != ai::OpFreeFlight)
            {
                FreeFlightNow();
                g.combat.trailTarget = 0;
            }
        }
        // Re-issue when the game dropped it, and on a new target: a Trail whose target died keeps running headless.
        else if (SubmitSettled() && (Op() != ai::OpTrail || g.combat.trailTarget != g.target))
        {
            Trail(g.target);
            g.combat.trailTarget = g.target;
        }
    }

    // Out of a big ship's bow arc (M04 opening: "the jittery, straight-on approach against a cruiser
    // ... the Liberty Cruiser has a main that shoots straight, and so the ship presents itself to high damage potential"):
    // a gunboat / cruiser / capital ship's main guns cover its nose - inside 35 deg of its nose axis within 3 km the
    // pursuit gives way to a run to a flank point beside it (perpendicular to the axis, our own side, afterburner) until
    // we are 50 deg off the axis (hysteresis: no jitter); then the attack from the flank.
    if (engaging && g.target && g_set.steer == 0)
    {
        const space::Contact tc = FindContact(g.target);
        float tp[3], tr[9];
        fl::Kinematics me = {};
        if ((tc.type & (kGunboatBit | kCruiserBit | kCapitalBit)) && space::ObjectPose(g.target, tp, tr) &&
            fl::PlayerKinematics(me))
        {
            const float nose[3] = {-tr[0 * 3 + 2], -tr[1 * 3 + 2], -tr[2 * 3 + 2]};
            float rel[3] = {me.pos[0] - tp[0], me.pos[1] - tp[1], me.pos[2] - tp[2]};
            const float d = Len3(rel);
            const float along = Dot3(rel, nose);
            const float offDeg = d > 1 ? acosf(Clampf(along / d, -1, 1)) * 57.2958f : 90.0f;
            if (!g.combat.flanking && d < 3000 && offDeg < 35)
            {
                g.combat.flanking = true;
                events::Push("autopilot bow-arc target=%u dist=%.0f off-axis=%.0fdeg: to the flank", g.target, d,
                             offDeg);
            }
            else if (g.combat.flanking && (offDeg > 50 || d > 3500))
            {
                g.combat.flanking = false;
                events::Push("autopilot bow-arc clear off-axis=%.0fdeg", offDeg);
            }
            if (g.combat.flanking)
            {
                float side[3] = {rel[0] - nose[0] * along, rel[1] - nose[1] * along, rel[2] - nose[2] * along};
                float sl = Len3(side);
                if (sl < 50) // right on the axis: the ship's own up
                {
                    side[0] = tr[0 * 3 + 1], side[1] = tr[1 * 3 + 1], side[2] = tr[2 * 3 + 1];
                    sl = 1;
                }
                const float reach = std::max(900.0f, d * 0.8f);
                for (int i = 0; i < 3; ++i)
                    out.steerPos[i] = tp[i] + side[i] / sl * reach;
                out.pursue = false;
                out.steerTo = true;
                out.steerFullAngle = 0.35f;
                out.steerThrottle = 1.0f;
                out.steerReverse = false;
                out.afterburn = true;
            }
        }
        else
            g.combat.flanking = false;
    }
    else
        g.combat.flanking = false;

    // A big target's turret blind spot (BlindSpot): flown to on the afterburner from afar, then held - station keeping
    // with the nose and the guns on the target - while its guns cannot reach us. Supersedes the bow-arc flank run.
    bool blind = false;
    g.combat.rushing = false;
    // `strafe_pass` (StrafeFly): this frame's passes are set again below; a strafe whose target is no longer ours ends
    g.strafe.on = false;
    g.strafe.aimOn = false;
    if (g.strafe.target && g.strafe.target != g.target)
    {
        StrafePassLog(" (end)");
        events::Push("autopilot strafe end target=%u passes=%d", g.strafe.target, g.strafe.passes);
        const u32 fail = g.strafe.failTarget;
        g.strafe = State::Strafe();
        g.strafe.failTarget = fail;
    }
    if (engaging && g.target && g_set.steer == 0)
    {
        const space::Contact tc = FindContact(g.target);
        float spot[3], fireSpot[3] = {};
        // (only a target that sits still or crawls: a spot in a manoeuvring gunboat's frame runs away from us - M09 22 Oct
        // 2026, the last Rheinland gunboats: station keeping at <= 60 m/s, the spot 683 -> 1204 m off in 12 s, the
        // allies made the kills; the user saw the run "losing steam")
        space::ShipMotion tm;
        const bool still = !tc.ship || !space::ShipMotionOf(g.target, tm) || tm.speed < 30.0f;
        const bool bigStill =
            tc.id && still && (!tc.ship || (tc.type & (kGunboatBit | kCruiserBit | kCapitalBit)) || tc.maxHit > 20000);
        // `strafe_pass`: at a big armed hull the passes instead of any hold (the blind-spot hold, standoff_hold's site,
        // the static hold) - the blind-spot scan still runs, its state read by the launch and battery rules as today
        bool strafing = false;
        if (g_set.strafePass && bigStill)
        {
            float s0[3], s1[3] = {};
            BlindSpot(g.target, self.gunRange, s0, s1);
            strafing = StrafeFly(g.target, self.gunRange, out);
            if (strafing)
                blind = true, g.combat.flanking = false;
        }
        if (!strafing && bigStill && BlindSpot(g.target, self.gunRange, spot, fireSpot))
        {
            blind = true;
            g.combat.flanking = false;
            fl::Kinematics k = {};
            // `standoff_hold` (M13-MIMIC item 4; the owner, 30 Oct 2026 late: "maintain [full DPS] while keeping a safe
            // distance from launcher-related splash damage, while also attempting evasive maneuvers that don't reduce
            // time on target"): the firing site (ordnance_site's: the best-covered spot >= ordnance_standoff off the hull)
            // is THE hold for guns and launchers alike - no swing to a contact spot between salvos (q4s1: the site stood
            // 14-63 m off the hull, the station law crawling between the two) - reached at rush speed (crawling costs
            // ~2x: 330-360 hit points a second within 760 m of the Nomad battleship's centre vs 143-229 moving, 248
            // archived M13 traces) and held with a slow slide round it, the nose on the target (the runners' one-way slide)
            const bool siteHold = g_set.standoffHold && g.combat.fireOn;
            if (siteHold)
            {
                memcpy(spot, fireSpot, sizeof(spot));
                // (only where a turret bears on the site: at a site no gun covers the slide only cost aim - mm5,
                // cover 0 at a 214 m gap, the goal chased 78-120 m off at 25-80 m/s, on target 15 of 69 s, the kill
                // ~18 s later than the blind-spot hold's)
                if (g_set.standoffSlide > 0 && g_set.standoffSlideSpeed > 0 && g.combat.fireCover > 0)
                {
                    // a circle of standoff_slide round the site, across the line from the target's centre
                    float a[3] = {fireSpot[0] - tc.pos[0], fireSpot[1] - tc.pos[1], fireSpot[2] - tc.pos[2]};
                    const float al = Len3(a);
                    if (al > 1)
                    {
                        for (float& x : a)
                            x /= al;
                        const float up[3] = {std::fabs(a[1]) > 0.9f ? 1.0f : 0.0f, std::fabs(a[1]) > 0.9f ? 0.0f : 1.0f, 0};
                        float u[3] = {a[1] * up[2] - a[2] * up[1], a[2] * up[0] - a[0] * up[2], a[0] * up[1] - a[1] * up[0]};
                        const float ul = std::max(1e-3f, Len3(u));
                        for (float& x : u)
                            x /= ul;
                        const float v[3] = {a[1] * u[2] - a[2] * u[1], a[2] * u[0] - a[0] * u[2], a[0] * u[1] - a[1] * u[0]};
                        const float w = g.time * g_set.standoffSlideSpeed / g_set.standoffSlide;
                        for (int i = 0; i < 3; ++i)
                            spot[i] += g_set.standoffSlide * (std::cos(w) * u[i] + std::sin(w) * v[i]);
                    }
                }
            }
            float to[3] = {};
            if (fl::PlayerKinematics(k))
                to[0] = spot[0] - k.pos[0], to[1] = spot[1] - k.pos[1], to[2] = spot[2] - k.pos[2];
            const float far = Len3(to);
            out.afterburn = false;
            // The rush (first the `blind_rush` switch, tested and not kept: deleted 31 Oct 2026, AUD-A1-5; the firing
            // site's hold and bs_approach fly it) (the speedrunners at M13's Nomad battleship, fights.tsv "Order base":
            // cruise / afterburner in to
            // 0 m, held there 21-35 s, hull >= 0.86; ours crawled 600 -> 150 m at 40-50 m/s for 13-31 s under 4-9 of its
            // 15 turrets - nomad_turret01, 697 a hit - and 4 of 13 runs died in that crawl or the hold, run-judge 29 Oct
            // 2026): full throttle and the afterburner on to the spot until the braking distance (full reverse, the
            // flight model, + 60 m - the M12 hold's rule), then the station keeping; the geometry guard brakes before a
            // hull (it did not exist at the 22 Oct ram below)
            float rushReach = 0;
            // (`bs_approach`, M13-MIMIC 4c: the owner's "no slow crawl under fire" - the rush with the launches kept, which
            // launch_site_check clears per round; the spots scored by their line to the centre in BlindSpot)
            const bool rushOn = siteHold || g_set.bsApproach;
            if (rushOn)
            {
                flight::Model fm;
                const float speed = Len3(k.vel);
                rushReach = flight::ReadModel(fm) ? std::max(120.0f, flight::BrakeDistance(fm, speed, 0) + 60.0f)
                                                  : std::max(250.0f, speed * 2.5f);
            }
            // the last 800 m by station keeping (<= 60 m/s): spots lie close to or inside the hull envelope - the full
            // speed run rammed the hull (M13 22 Oct 2026: dead at 95 % hull; the aft spots "hit" before were likely
            // the same); the afterburner only beyond 1.5 km
            // (closing on the spot - from afar too: the rush's 1,251 m self-burst lesson came from a round launched at
            // 1,101 m that ended 14 m from us, where we flew to - rush_hold_fire)
            g.combat.rushing = rushOn && far > rushReach;
            if (far > 800 || (rushOn && far > rushReach))
            {
                memcpy(out.steerPos, spot, sizeof(out.steerPos));
                out.pursue = false;
                out.steerTo = true;
                out.steerFullAngle = 0.35f;
                out.steerThrottle = 1.0f;
                out.steerReverse = false;
                out.afterburn = far > 1500 || (rushOn && far > rushReach + 100.0f);
                static Throttle rushSaid;
                if (rushOn && far <= 800 && rushSaid.Due(g.time, 1.0f))
                    events::Push("autopilot blind-rush dist=%.0f speed=%.0f brake-at=%.0f%s", far, Len3(k.vel), rushReach,
                                 siteHold ? " to=fire-site" : "");
            }
            else
            {
                out.pursue = true;
                out.steerTo = false;
                out.station = true;
                // `ordnance_site` (WORK_QUEUE Q4 slice 1): the firing position while a launcher with ammo has its refire
                // due by the time we are there (the station law's travel time), else the guns spot - no salvo waits
                bool useFire = false;
                float travel = -1, soonest = 1e9f;
                if (!siteHold && g_set.ordnanceSite && g.combat.fireOn && g_set.missiles &&
                    mprules::Autopilot() == mprules::Allow::Full)
                {
                    travel = ordnance::StationTravelTime(geom::Dist(fireSpot, k.pos));
                    for (const space::Weapon& w : space::PlayerWeapons())
                        if (w.homing && w.ammo > 0)
                            soonest = std::min(soonest, std::max(0.0f, w.refire - w.elapsed));
                    useFire = soonest < 1e8f && ordnance::ShouldGoToSite(travel, soonest);
                }
                if (useFire != g.combat.fireUsed)
                    events::Push("autopilot fire-site %s target=%u dist=%.0f travel=%.1fs refire-left=%.1fs gap=%.0f "
                                 "cover=%d guns-spot-dist=%.0f",
                                 useFire ? "on" : "off", g.target, geom::Dist(fireSpot, k.pos), travel,
                                 soonest < 1e8f ? soonest : -1.0f, g.combat.fireGap, g.combat.fireCover, far);
                g.combat.fireUsed = useFire;
                memcpy(out.stationPos, useFire && !siteHold ? fireSpot : spot, sizeof(out.stationPos));
                static Throttle siteSaid;
                if (siteHold && siteSaid.Due(g.time, 2.0f))
                    events::Push("autopilot standoff-hold target=%u gap=%.0f cover=%d dist=%.0f speed=%.0f", g.target,
                                 g.combat.fireGap, g.combat.fireCover, far, Len3(k.vel));
            }
        }
    }
    else if (g.combat.blindOn)
        g.combat.blindOn = false;
    // A static target without a blind spot: hold-and-salvo (StaticHold), else attack passes (StaticPass), else an orbit
    // round it (StaticOrbit).
    // (`big_coast`: an engine the coast killed is switched back on whatever else runs - the kill pressed in the frame the
    // coast ended stayed on: the whole hold sat at 2-28 m/s, throttle -1, engine off; M13 city)
    if (g.pass.killTapped && g.pass.phase != PassPhase::Coast && hudinfo::EngineKilled())
    {
        static float restoreAt = -10;
        if (g.time - restoreAt > 0.8f)
        {
            restoreAt = g.time;
            out.engineKill = true;
            events::Push("autopilot static-pass engine back on (after the coast)");
        }
    }
    else if (g.pass.phase != PassPhase::Coast && !hudinfo::EngineKilled())
        g.pass.killTapped = false;
    if (!blind && engaging && g.target && g_set.steer == 0)
    {
        float p[3];
        if (!StaticHold(g.target, out) && !StaticPass(g.target, out) && StaticOrbit(g.target, dt, p))
        {
            out.pursue = true;
            out.steerTo = false;
            out.station = true;
            out.afterburn = false;
            memcpy(out.stationPos, p, sizeof(out.stationPos));
        }
    }
    else if (!engaging || !g.target)
        g.combat.orbitTarget = 0;
    if (!g.target)
        g.hold.on = false; // (a detour to another target keeps the hold: it belongs to holdTarget)

    // `ordnance_keepout` (WORK_QUEUE Q4 slice 1): our own live rounds' impact spheres (TrackOwnMissiles: where each will
    // burst, its blast + our radius + launch_site_margin) bend this frame's station / steer goal - the movement never
    // carries us into a burst we launched (M13 i1: the hold's goal 55 m off the hull, our torpedo's burst on it)
    if (g_set.ordnanceKeepout && engaging && (out.station || out.steerTo) && !g.missiles.ownMissiles.empty())
    {
        fl::Kinematics k = {};
        if (fl::PlayerKinematics(k))
        {
            float* goal = out.station ? out.stationPos : out.steerPos;
            for (const auto& om : g.missiles.ownMissiles)
            {
                const State::OwnMissile& m = om.second;
                if (!m.impactOk || m.tImpact < g.time - 0.5f)
                    continue;
                const float was[3] = {goal[0], goal[1], goal[2]};
                if (ordnance::KeepOut(k.pos, goal, m.impact, m.need))
                {
                    static Throttle saidAt;
                    if (saidAt.Due(g.time, 1))
                        events::Push("autopilot keepout round=%u impact-in=%.2fs need=%.0f goal-moved=%.0f us-to-impact=%.0f %s",
                                     om.first, m.tImpact - g.time, m.need, geom::Dist(was, goal),
                                     geom::Dist(k.pos, m.impact), out.station ? "station" : "steer");
                }
            }
        }
    }

    // Wedged (M12 22 Oct 2026: the pursuit of a shield generator pushed the ship into the Nomad lair's structure - 4 min
    // at speed -1, nothing dealt, the 600 s timer ran out): no 30 m moved and nothing dealt in 6 s while fighting -> 3 s on
    // the afterburner straight away from the nearest solid (else backwards), then the fight again.
    if (engaging && g.target && !out.station)
    {
        fl::Kinematics k = {};
        if (fl::PlayerKinematics(k))
        {
            if (g.combat.breakUntil > g.time)
            {
                out.pursue = false;
                out.steerTo = true;
                memcpy(out.steerPos, g.combat.breakPos, sizeof(out.steerPos));
                out.steerFullAngle = 0.35f;
                out.steerThrottle = 1.0f;
                out.steerReverse = false;
                out.afterburn = true;
            }
            else if (g.combat.stallAt < 0 || g.time - g.combat.stallAt > 6)
            {
                const float mv[3] = {k.pos[0] - g.combat.stallPos[0], k.pos[1] - g.combat.stallPos[1],
                                     k.pos[2] - g.combat.stallPos[2]};
                if (g.combat.stallAt >= 0 && Len3(mv) < 30 && g.combat.dealt + g.combat.dealtShield <= g.combat.stallDealt + 1)
                {
                    float away[3] = {k.rot[0 * 3 + 2], k.rot[1 * 3 + 2], k.rot[2 * 3 + 2]}; // backwards
                    float best = 1e30f;
                    for (const space::Obstacle& o : space::Obstacles(k.pos, 600))
                    {
                        if (o.ship || o.zone)
                            continue;
                        const float d[3] = {k.pos[0] - o.pos[0], k.pos[1] - o.pos[1], k.pos[2] - o.pos[2]};
                        const float dl = Len3(d), gap = dl - o.radius;
                        if (dl > 1 && gap < best)
                            best = gap, away[0] = d[0] / dl, away[1] = d[1] / dl, away[2] = d[2] / dl;
                    }
                    // the real hulls decide the way out (M12 trench: "away from the sphere's centre" pointed
                    // at the trench's outer wall - 12 s pinned, every break-out braked by the guard): the clear direction
                    // nearest it by the ray fan (the game's ray test)
                    const avoid::FanResult fan = avoid::RayFan(k.pos, away, 200.0f, space::PlayerRadius(), nullptr, 0);
                    if (fan.clear > 40.0f)
                        memcpy(away, fan.dir, sizeof(away));
                    for (int i = 0; i < 3; ++i)
                        g.combat.breakPos[i] = k.pos[i] + away[i] * 600.0f;
                    g.combat.breakUntil = g.time + 3.0f;
                    events::Push("autopilot combat wedged (moved %.0f m, nothing dealt in 6 s): breaking out",
                                 Len3(mv));
                }
                g.combat.stallAt = g.time;
                memcpy(g.combat.stallPos, k.pos, sizeof(g.combat.stallPos));
                g.combat.stallDealt = g.combat.dealt + g.combat.dealtShield; // (shield hits count: a held-off target is no wedge)
            }
        }
    }

    // A pursuit through a costly field (M11 22 Oct 2026: a fight at the Osiris mouth drifted into a minefield, one mine
    // and the ship was gone at 94 % hull): while one is here or 3 s ahead, the line to the target is the avoidance's
    // (ApplyAvoid, strict there); a blocked line is flown around, the guns on the gimbal meanwhile.
    if (out.pursue && !out.station && g_set.avoid && g.target)
    {
        fl::Kinematics k = {};
        if (fl::PlayerKinematics(k))
        {
            const float ahead[3] = {k.pos[0] + k.vel[0] * 3, k.pos[1] + k.vel[1] * 3, k.pos[2] + k.vel[2] * 3};
            // ...or a solid / lethal zone the next 4 s of flight pass through (M13 22 Oct 2026: killed outright at 96 %
            // hull chasing a Dyson generator, nothing had hit us)
            bool solidAhead = false;
            const float sp = Len3(k.vel);
            for (const space::Obstacle& o : space::Obstacles(k.pos, sp * 4 + 3000))
            {
                if (o.ship || o.asteroid || o.id == g.target)
                    continue;
                // (the structure the target sits on - a generator on the Dyson sphere - is where the fight is)
                // (never a lethal zone: M13 22 Oct 2026, killed outright at 89 % hull chasing a generator that sits on the
                // Dyson sphere - its barrier zone holds the generator)
                const float tt[3] = {t.world[0] - o.pos[0], t.world[1] - o.pos[1], t.world[2] - o.pos[2]};
                if (!o.zone && Len3(tt) < o.radius + 100)
                    continue;
                const float rel[3] = {o.pos[0] - k.pos[0], o.pos[1] - k.pos[1], o.pos[2] - k.pos[2]};
                const float tc = sp > 1 ? Clampf(Dot3(rel, k.vel) / (sp * sp), 0, 4) : 0;
                const float cp[3] = {k.pos[0] + k.vel[0] * tc - o.pos[0], k.pos[1] + k.vel[1] * tc - o.pos[1],
                                     k.pos[2] + k.vel[2] * tc - o.pos[2]};
                if (Len3(cp) < o.radius + 60)
                    solidAhead = true;
            }
            if (solidAhead || !space::StrictAt(k.pos).empty() || !space::StrictAt(ahead).empty())
            {
                Out probe = out;
                probe.pursue = false;
                probe.steerTo = true;
                memcpy(probe.steerPos, t.world, sizeof(probe.steerPos));
                probe.steerFullAngle = 0.15f;
                probe.steerThrottle = 1.0f;
                ApplyAvoid(k, probe, g.target);
                if (memcmp(probe.steerPos, t.world, sizeof(probe.steerPos)) != 0)
                    out = probe;
            }
        }
    }

    // The turn onto a target far off the nose, while attacked ("it gets hit a lot when it switches
    // targets ... just turns to face the next target without using afterburners or reverse thrust"): the turn is itself
    // a dodge, as a player flies it - slide bursts in a new direction every 0.7 s, the afterburner on (the flight path
    // bends fast, off the attackers' lead), or reverse thrust when the target is behind and within gun range (the
    // tighter turn brings it onto the nose sooner, without running away from it). From 60 degrees off the nose until 25,
    // at most 4 s. Inside 250 m only the slides: the pursuit's own throttle (reverse through sharp turns) is the close
    // dogfight's (first fight matrix: started at 40 degrees, it burned 5-8 s at 30-75 m - the ship overshot
    // its target, fights 20 % slower and one marginal win lost).
    const bool canTurn = engaging && !blind && g_set.steer == 0 && g_set.evade && g_set.turnEvade &&
                         g.tactic == Tactic::Engage && !hopping;
    g.combat.turnCooldown -= dt;
    fl::Kinematics tk = {};
    if (canTurn && fl::PlayerKinematics(tk))
    {
        const float to[3] = {t.world[0] - tk.pos[0], t.world[1] - tk.pos[1], t.world[2] - tk.pos[2]};
        const float dist = geom::Len(to);
        float fwd[3];
        geom::Forward(fwd, tk.rot);
        const float off =
            dist > 1 ? acosf(Clampf((fwd[0] * to[0] + fwd[1] * to[1] + fwd[2] * to[2]) / dist, -1, 1)) : 0;
        const float deg = off * 180.0f / 3.14159265f;
        const bool attacked = pressure > 0 || g_sit.attackers > 0;
        if (!g.combat.turning && attacked && deg > 60 && g.combat.turnCooldown <= 0)
        {
            g.combat.turning = true;
            g.combat.turnStart = g.time;
            g.combat.turnSlideTimer = 0;
            g.combat.dodgeDir = DodgeDirection();
            ++g.stats.turns;
            events::Push("autopilot turn-evade off=%.0f dist=%.0f attackers=%d", deg, dist, g_sit.attackers);
        }
        else if (g.combat.turning && (deg < 25 || !attacked || g.time - g.combat.turnStart > 4))
        {
            g.combat.turning = false;
            g.combat.turnCooldown = 2;
            events::Push("autopilot turn-evade-end after=%.1f off=%.0f", g.time - g.combat.turnStart, deg);
        }
        if (g.combat.turning)
        {
            g.stats.turnTime += dt;
            g.combat.turnSlideTimer += dt;
            if (g.combat.turnSlideTimer > 0.7f)
            {
                g.combat.turnSlideTimer = 0;
                g.combat.dodgeDir = DodgeDirection();
            }
            out.slide = g.combat.turnSlideTimer < std::max(0.3f, g_set.slideTime) ? g.combat.dodgeDir : 0;
            const float reach = self.gunRange > 0 ? self.gunRange : 600.0f;
            out.turnManeuver = dist < 250 ? 0 : deg > 100 && dist < reach ? 2 : 1;
        }
    }
    else if (g.combat.turning)
        g.combat.turning = false;

    // The ship-adaptive duel (DuelChoose, above): against a fighter-class target in our own pursuit (not big ships -
    // their bow arc has its own flank rule - nor while turning onto a target under fire).
    if (engaging && g.target && g_set.steer == 0 && g_set.adaptive && !g.combat.flanking && !g.combat.turning)
    {
        const space::Contact tc = FindContact(g.target);
        space::ShipMotion tm, me;
        if (tc.id && !(tc.type & (kGunboatBit | kCruiserBit | kCapitalBit)) && space::ShipMotionOf(g.target, tm) &&
            space::ShipMotionOf(space::PlayerShipId(), me))
        {
            const float ratio = tm.turnRate > 0 && me.turnRate > 0 ? me.turnRate / tm.turnRate : 0.0f;
            int opt = DuelChoose((static_cast<unsigned long long>(me.arch) << 32) | tm.arch, ratio, g_sit.attackers);
            if (g_set.duel >= 0) // forced (A/B): booked to the forced option
                opt = g.combat.duelOpt = g_set.duel;
            // the exchange since the last frame, booked to the option flown (hull + shield taken off it, of its hull +
            // shield; ours: g.combat.lost, of our hull + shield - hull alone read every duel against a shield as lost)
            const float dealtPts = g.combat.dealt + g.combat.dealtShield;
            float tsh = 0, tshMax = 0;
            bool tup = false;
            space::ShieldOf(g.target, tsh, tshMax, tup);
            if (g.combat.duelDealtSeen >= 0 && tc.maxHit > 0)
                DuelAccount(dt, std::max(0.0f, dealtPts - g.combat.duelDealtSeen) / (tc.maxHit + std::max(0.0f, tshMax)),
                            std::max(0.0f, g.combat.lost - g.combat.duelLostSeen));
            g.combat.duelDealtSeen = dealtPts;
            g.combat.duelLostSeen = g.combat.lost;
            if (opt == 1)
                out.pursueBand = 180.0f;
            else if (opt == 2)
            {
                // The slash: the pass on the afterburner, no reverse, guns on the lead; through it (inside 90 m, or
                // opening past it within 250 m) the extend - away on the afterburner with small jinks - out to 700 m
                // (or 4.5 s), then the next pass.
                if (g.combat.slashPhase == SlashPhase::Pass &&
                    (t.dist < 90.0f || (t.closing < -20.0f && t.dist < 250.0f)))
                {
                    g.combat.slashPhase = SlashPhase::Extend;
                    g.combat.slashSince = g.time;
                }
                else if (g.combat.slashPhase == SlashPhase::Extend &&
                         (t.dist > 700.0f || g.time - g.combat.slashSince > 4.5f))
                {
                    g.combat.slashPhase = SlashPhase::Pass;
                    g.combat.slashSince = g.time;
                }
                if (g.combat.slashPhase == SlashPhase::Pass)
                    out.turnManeuver = 1;
                else
                {
                    fl::Kinematics k = {};
                    if (fl::PlayerKinematics(k))
                    {
                        out.pursue = false;
                        JinkAway(k, t.world, dt, out, 10.0f, true);
                        out.slide = 0;
                    }
                }
            }
        }
    }

    // The head-on pass (`headon_reverse`; "when the target is currently flying at the player,
    // the player should consider reverse thrusting and starting a predictive turn towards the direction the enemy ship
    // is flying towards, then shift back to forward thrust to stay on the enemy ship" - more time firing and hitting):
    // a ship within 150-1000 m whose nose is on us (25 deg) and closing over 40 m/s, in a pursuit we fly: reverse
    // thrust (the pass lasts longer), the nose led to where it is flying (its position + its velocity over half the
    // time to the pass, at most 1.5 s) while the guns stay on its lead (SetGunAim); forward again once it has passed or
    // turned off.
    // (held once begun - gate mpb5's first minutes: on / off every 40 ms around the entry's 40 m/s, the reverse thrust
    // itself pulling the closing under it - until it has passed (closing <= 0), turned off us (45 deg) or is 100 m away)
    // The stand-off (`standoff`; the owner, 28 Oct 2026, the fight matrix: "so close to the enemy that when the enemy
    // turns and moves sideways ... the player loses track for a few seconds"): a target crossing at v from d metres
    // sweeps across the nose at v / d rad/s - closer than v / our turn rate the nose cannot follow. The pursuit's close
    // band against a ship is at least 1.3 x its speed / our turn rate (80 m .. 60 % of gun range; was 45-70 m by its
    // size); a missile run still closes in (below).
    if (g_set.standoff && out.pursue && g.target)
    {
        space::ShipMotion tgm, mym;
        if (space::ShipMotionOf(g.target, tgm) && space::ShipMotionOf(space::PlayerShipId(), mym) && mym.turnRate > 0.1f)
        {
            const float reach = out.gunRange > 0 ? out.gunRange : 600.0f;
            float muzzle = 0;
            for (const space::Weapon& w : space::PlayerWeapons())
                if (!w.homing && w.active && w.ammo < 0)
                    muzzle = std::max(muzzle, w.muzzleVelocity);
            const float need = StandoffBand(mym, tgm, reach, muzzle, t.radius);
            if (out.pursueBand < need)
                out.pursueBand = need;
        }
    }
    bool headOn = false;
    const bool wasOn = g.combat.headOn;
    // (it begins in a slide evade too - the evade is dropped for it; it ends once the shield is down to `headon_shield`
    // or the hull under half: the evasions take over then)
    const bool shieldOk = (self.shield < 0 || self.shield > g_set.headonShield) && !missileIn && !raceLosing;
    if (g_set.headonReverse && out.pursue &&
        (g.tactic == Tactic::Engage || (g.tactic == Tactic::Evade && !g.combat.gameEvade)) && shieldOk &&
        hull > 0.5f && t.valid &&
        // (reverse thrust only barely beyond the guns' reach - "Too many seconds are wasted waiting
        // for the enemy ship to come into range. Reverse thrust should have a max range that's barely over weapons range":
        // begun inside 1.1x the reach, held to 1.15x; reversing from 1000 m kept a heavy out of a 600 m reach for 8 s)
        t.dist > (wasOn ? 100.0f : 150.0f) &&
        t.dist < (out.gunRange > 0 ? out.gunRange : 600.0f) * (wasOn ? 1.15f : 1.1f) &&
        t.closing > (wasOn ? 0.0f : 40.0f))
    {
        space::ShipMotion tm;
        fl::Kinematics me = {};
        if (space::ShipMotionOf(g.target, tm) && tm.speed > 20.0f && fl::PlayerKinematics(me))
        {
            const float toUs[3] = {me.pos[0] - tm.pos[0], me.pos[1] - tm.pos[1], me.pos[2] - tm.pos[2]};
            const float d = std::max(1.0f, Len3(toUs));
            if (Dot3(tm.fwd, toUs) / d > (wasOn ? 0.707f : 0.906f)) // 25 deg to begin, 45 deg to end
            {
                headOn = true;
                const float ahead = std::min(1.5f, 0.5f * t.dist / std::max(t.closing, 20.0f));
                for (int i = 0; i < 3; ++i)
                    out.steerAheadPos[i] = tm.pos[i] + tm.vel[i] * ahead;
                out.steerAhead = true;
                out.turnManeuver = 2; // reverse thrust, no afterburner
                // ...and the pass kept wide ("use lateral and vertical thrusters while maintaining
                // nose on the predicted firing point" so the ship does not get too close as it flies by): where it will
                // pass us - the relative position at the closest approach, r + v t with t = -(r.v) / |v|^2 - and a
                // slide against that miss vector on the stronger of our right / up axes while it is under the
                // clearance (the stand-off's 80 m floor, or the band); the nose stays on the lead point.
                const float rr[3] = {tm.pos[0] - me.pos[0], tm.pos[1] - me.pos[1], tm.pos[2] - me.pos[2]};
                const float rv[3] = {tm.vel[0] - me.vel[0], tm.vel[1] - me.vel[1], tm.vel[2] - me.vel[2]};
                const float vv = Dot3(rv, rv);
                const float tc = vv > 1.0f ? std::max(0.0f, -Dot3(rr, rv) / vv) : 0.0f;
                const float miss[3] = {rr[0] + rv[0] * tc, rr[1] + rv[1] * tc, rr[2] + rv[2] * tc};
                const float clearance = std::max(80.0f, out.pursueBand);
                if (Len3(miss) < clearance && !out.dodgeSlide)
                {
                    const float right[3] = {me.rot[0 * 3 + 0], me.rot[1 * 3 + 0], me.rot[2 * 3 + 0]};
                    const float up[3] = {me.rot[0 * 3 + 1], me.rot[1 * 3 + 1], me.rot[2 * 3 + 1]};
                    const float ar = -Dot3(miss, right), au = -Dot3(miss, up); // away from where it passes
                    out.slide = fabsf(ar) >= fabsf(au) ? (ar > 0 ? 2 : 1) : (au > 0 ? 3 : 4);
                }
                if (!g.combat.headOn)
                {
                    if (g.tactic == Tactic::Evade)
                        SetTactic(Tactic::Engage);
                    events::Push("autopilot head-on target=%u dist=%.0f closing=%.0f lead=%.2fs shield=%.2f", g.target,
                                 t.dist, t.closing, ahead, self.shield);
                }
            }
        }
    }
    if (headOn)
        g.combat.headOnLead = std::min(1.5f, 0.5f * t.dist / std::max(t.closing, 20.0f));
    if (g.combat.headOn && !headOn)
    {
        g.combat.headOnEndAt = g.time;
        events::Push("autopilot head-on end dist=%.0f closing=%.0f shield=%.2f hull=%.2f", t.dist, t.closing,
                     self.shield, hull);
    }
    // The hand-over to the pursuit ("keep the transition from reverse thrusting and sideways
    // thrusting to pursuit mode smooth and continuous, lest the player ship misses on valuable time on target"): for 0.8 s
    // the nose's point glides from the pass's ahead point (the target + its velocity x the pass's lead, read live) to the
    // pursuit's lead on a smoothstep - no jump of the steering - while the guns stay on the real lead; the throttle is the
    // pursuit's at once (forward: stay on the enemy).
    bool handOver = false;
    if (!headOn && g.combat.headOnEndAt >= 0 && g.time - g.combat.headOnEndAt < 0.8f && out.pursue && t.valid)
    {
        space::ShipMotion tm;
        if (space::ShipMotionOf(g.target, tm))
        {
            float s = (g.time - g.combat.headOnEndAt) / 0.8f;
            s = s * s * (3.0f - 2.0f * s);
            for (int i = 0; i < 3; ++i)
                out.steerAheadPos[i] =
                    (tm.pos[i] + tm.vel[i] * g.combat.headOnLead) * (1.0f - s) + t.world[i] * s;
            out.steerAhead = true;
            handOver = true;
        }
    }
    g.combat.headOn = headOn;

    // Weapons: the reticle on the lead cross (pursuit, or the gimbal aim while the game flies); fire whenever the
    // lead is near the nose and in range (bursts = 1: in the personality's bursts).
    out.gimbal = !out.pursue;
    bool aligned = false;
    if (t.onScreen && t.inRange)
    {
        const float w = static_cast<float>(fl::ScreenWidth()), h = static_cast<float>(fl::ScreenHeight());
        const float ox = (t.screenX - w * 0.5f) / (w * 0.5f), oy = (t.screenY - h * 0.5f) / (h * 0.5f);
        aligned = sqrtf(ox * ox + oy * oy) < g_set.fireCone;
    }
    RangeLearn(dt, t, aligned);
    // `strafe_pass`: the nose on the pass's hull point (StrafeFly) - the guns' and the launchers' alignment read by that
    // point, not by the target's own hull point (aim::Current's, on the line to its centre: abeam for half of each pass)
    bool strafeLaunch = false;
    if (g.strafe.on && g.strafe.aimOn)
    {
        float sp[3];
        const float w = static_cast<float>(fl::ScreenWidth()), h = static_cast<float>(fl::ScreenHeight());
        if (fl::WorldToFlightScreen(g.strafe.aim, sp) && sp[0] >= 0 && sp[1] >= 0 && sp[0] < w && sp[1] < h)
        {
            const float ox = (sp[0] - w * 0.5f) / (w * 0.5f), oy = (sp[1] - h * 0.5f) / (h * 0.5f);
            strafeLaunch = sqrtf(ox * ox + oy * oy) < g_set.fireCone;
        }
        aligned = strafeLaunch && g.strafe.aimInGuns;
    }
    g.combat.burst += dt;
    const float burstOn = Pf(g_p.Gun.gun_fire_burst_interval_time, 0.2f, 5, 1.5f);
    const float burstOff = Pf(g_p.Gun.gun_fire_no_burst_interval_time, 0.0f, 3, 0.1f);
    if (g.combat.burst > burstOn + burstOff)
        g.combat.burst = 0;
    // Pursuit: the controller fires whenever the reticle sits on the lead cross within fire_cone (one gate, no gaps);
    // else (the game flies) by the lead's offset from the nose.
    out.fireCone = g_set.fireCone;
    out.stagger = g_set.stagger;
    // (a multiplayer server that allows travel only: no shot - mprules.h)
    // Every gun that can reach (`fire_covering`; the owner, 28 Oct 2026, watching the M13 generators: "I expect a full
    // and complete usage of weapons"): the lead off the nose - the static hold's wide orbit, evasion - left every gun
    // silent although the gimballed guns and the turret could point at it (debug gunpoint: CanPointAt 1, CanFire ok;
    // the guns 7 s idle while the torpedo and missiles flew). The game's own test (CEGun::CanPointAt, joint limits and
    // munition range) per gun every 0.2 s; while one can, the guns aim at the lead itself (host v28 SetGunAim) instead
    // of the cursor's ray and fire - a gun that cannot point is refused by the game (FireResult angle), costs nothing.
    bool covering = false;
    if (g_set.fireCovering && !out.pursue && !aligned && t.valid && t.inRange)
    {
        if (g.time - g.combat.coverAt >= 0.2f)
        {
            g.combat.coverAt = g.time;
            const float pt[1][3] = {{t.world[0], t.world[1], t.world[2]}};
            int n = 0;
            g.combat.covering = space::GunsCovering(space::PlayerShipId(), pt, 1, &n) > 0 ? n : 0;
        }
        covering = g.combat.covering > 0;
    }
    out.fire = g_set.autoFire && (out.pursue || aligned || covering) && (!g_set.bursts || g.combat.burst <= burstOn) &&
               mprules::Autopilot() == mprules::Allow::Full;
    if ((covering || ((headOn || handOver) && t.inRange)) && out.fire) // (head-on: the nose leads, the guns on the lead)
    {
        out.gunAim = true;
        memcpy(out.gunAimPos, t.world, sizeof(out.gunAimPos));
    }
    if (aligned && strafeLaunch && out.fire) // (`strafe_pass`: every gun on the pass's hull point)
    {
        out.gunAim = true;
        memcpy(out.gunAimPos, g.strafe.aim, sizeof(out.gunAimPos));
    }
    // Roll (`roll_guns`, `roll_flair`; "work on rolling and make it as useful as possible, while
    // also still considering adding roll when it's not strictly necessary ... an addition to the autopilot's arsenal of
    // maneuvering options"). The player has no roll command; pitch and yaw turn alike on every ship (shiparch: steering
    // torque and angular drag equal on both axes, 68 of 69 entries), so a roll never speeds the nose - but each gun's
    // swivel limits sit in its mount's frame: rolled, a lead off the nose can come into reach of guns that cannot point
    // at it now. Rolling the ship by +a about its nose = turning the lead point by -a about it: the game's own gun test
    // (CEGun::CanPointAt, GunsCovering) at 8 angles every 0.25 s. More guns at another angle -> roll toward it (the
    // shorter way, the ship's own top roll rate); no angle loses a gun and a sideways slide is flown -> a barrel roll
    // that way (flair). Events `roll guns now=.. best=.. at=..` / `roll flair` / `roll off` on each change.
    {
        int why = 0;
        float want = 0;
        space::ShipMotion mym;
        fl::Kinematics k = {};
        // (no roll in a big turn: rolled while the screen-space steering turns hard, the turn bends into a spiral - the
        // lead within 25 deg of the nose (holds and orbits keep it there); a barrel roll in progress stops for a turn)
        bool noseNear = false;
        if (t.valid && fl::PlayerKinematics(k))
        {
            float fwd[3];
            geom::Forward(fwd, k.rot);
            const float v[3] = {t.world[0] - k.pos[0], t.world[1] - k.pos[1], t.world[2] - k.pos[2]};
            const float vl = Len3(v);
            noseNear = vl > 1.0f && Dot3(fwd, v) / vl > 0.906f;
        }
        if (!noseNear)
            BarrelStop();
        const bool rollable = (g_set.rollGuns || g_set.rollFlair) && noseNear && t.inRange && t.dist < 900.0f &&
                              space::ShipMotionOf(space::PlayerShipId(), mym) && mym.rollRate > 0.1f;
        // `roll_commit` (gate mpb21: of 1182 roll_guns choices the median lasted 0.51 s, 477 ended at the first 0.25 s
        // re-check - a 45 deg roll takes ~1.3 s at 0.6 x the top rate - and of 464 followed by a new choice only 55 began
        // at the guns the earlier one went for: re-chosen from 8 angles round the ship's orientation of the moment, a
        // roll half done pointed somewhere else every check). Now a choice is an angle: the roll flown is measured (the
        // spin about the nose), slowed toward the end (4 x the angle left), held to the end unless two checks in a row
        // find the chosen angle reaching no more guns than now (or 2.5 s pass), then 0.5 s before the next choice.
        // Events `roll guns now=.. best=.. goal=..` / `roll guns reached now=.. from=.. gain=.. in=..` / `... dropped`.
        if (g.combat.rollGoalOn && (!rollable || !g_set.rollGuns || !g_set.rollCommit))
        {
            g.combat.rollGoalOn = false;
            g.combat.rollHoldUntil = g.time + 0.5f;
            events::Push("autopilot roll guns dropped (%s) left=%.0f", rollable ? "off" : "no lead", g.combat.rollGoal * 57.3f);
        }
        if (rollable)
        {
            const float f[3] = {-k.rot[0 * 3 + 2], -k.rot[1 * 3 + 2], -k.rot[2 * 3 + 2]}; // the nose
            const float v[3] = {t.world[0] - k.pos[0], t.world[1] - k.pos[1], t.world[2] - k.pos[2]};
            float fxv[3];
            geom::Cross(fxv, f, v);
            const float fv = Dot3(f, v);
            // the lead as it would sit with the ship rolled by `roll` (rad, RollApply's +): the lead turned by -roll
            const auto leadAt = [&](float roll, float* p)
            {
                const float c = cosf(-roll), s = sinf(-roll);
                for (int j = 0; j < 3; ++j)
                    p[j] = k.pos[j] + v[j] * c + fxv[j] * s + f[j] * fv * (1 - c);
            };
            if (g.combat.rollGoalOn)
            {
                float spin[3];
                if (space::PlayerSpin(spin))
                    g.combat.rollGoal -= -spin[2] * dt; // (RollApply's + = spin z -)
            }
            if (g.time - g.combat.rollCheckAt >= 0.25f)
            {
                g.combat.rollCheckAt = g.time;
                if (g.combat.rollGoalOn)
                {
                    float pts[2][3];
                    leadAt(0, pts[0]);
                    leadAt(g.combat.rollGoal, pts[1]);
                    int c2[2] = {};
                    if (space::GunsCovering(space::PlayerShipId(), pts, 2, c2, 0.087f) <= 0)
                        c2[0] = c2[1] = 0;
                    g.combat.rollCounts[0] = c2[0];
                    // (the guns it went for already reach: arrived - the first runs dropped such rolls as "no gain"
                    // 10 deg short with now = gain; else no gain left at the chosen angle twice in a row = drop)
                    if (c2[0] >= g.combat.rollGain)
                        g.combat.rollGoal = 0;
                    g.combat.rollWorse = c2[1] <= c2[0] ? g.combat.rollWorse + 1 : 0;
                }
                else
                {
                    float pts[8][3];
                    for (int i = 0; i < 8; ++i)
                        leadAt(static_cast<float>(i) * 3.14159265f / 4.0f, pts[i]); // the ship rolled by i x 45 deg
                    // (5 deg, the tolerance CanFire accepts - at 0 the mpb11 runs read 0-1 of 5 guns where all fired,
                    // and rolled for gains that were none)
                    if (space::GunsCovering(space::PlayerShipId(), pts, 8, g.combat.rollCounts, 0.087f) <= 0)
                        memset(g.combat.rollCounts, 0, sizeof(g.combat.rollCounts));
                }
            }
            const int* n = g.combat.rollCounts;
            int best = 0, lo = n[0];
            for (int i = 1; i < 8; ++i)
            {
                lo = std::min(lo, n[i]);
                // more guns wins; on a tie the smaller turn (i 1 / 7 = 45 deg before 2 / 6 ...)
                const int di = std::min(i, 8 - i), db = std::min(best, 8 - best);
                if (n[i] > n[best] || (n[i] == n[best] && di < db && best != 0))
                    best = i;
            }
            if (g.combat.rollGoalOn)
            {
                const float left = g.combat.rollGoal;
                const bool arrived = std::fabs(left) < 0.05f || left * g.combat.rollDir <= 0;
                if (arrived || g.combat.rollWorse >= 2 || g.time - g.combat.rollGoalAt > 2.5f)
                {
                    float p[1][3];
                    leadAt(0, p[0]);
                    int now = 0;
                    space::GunsCovering(space::PlayerShipId(), p, 1, &now, 0.087f);
                    events::Push("autopilot roll guns %s now=%d from=%d gain=%d in=%.2f left=%.0f",
                                 arrived ? "reached" : g.combat.rollWorse >= 2 ? "dropped (no gain)" : "dropped (2.5 s)",
                                 now, g.combat.rollFrom, g.combat.rollGain, g.time - g.combat.rollGoalAt, left * 57.3f);
                    g.combat.rollGoalOn = false;
                    g.combat.rollHoldUntil = g.time + 0.5f;
                    RollApply(dt, 0, mym.rollRate); // (stop the spin there)
                    memset(g.combat.rollCounts, 0, sizeof(g.combat.rollCounts));
                    g.combat.rollCheckAt = -10; // (a fresh 8-angle check next frame)
                }
                else
                {
                    why = 1;
                    want = g.combat.rollDir * Clampf(4.0f * std::fabs(left), 0.25f * mym.rollRate, mym.rollRate);
                }
            }
            else if (g_set.rollGuns && g_set.rollCommit && n[best] > n[0])
            {
                if (g.time >= g.combat.rollHoldUntil)
                {
                    const float ang = static_cast<float>(best <= 4 ? best : best - 8) * 3.14159265f / 4.0f;
                    g.combat.rollGoalOn = true;
                    g.combat.rollGoal = ang;
                    g.combat.rollDir = ang > 0 ? 1.0f : -1.0f;
                    g.combat.rollGoalAt = g.time;
                    g.combat.rollFrom = n[0];
                    g.combat.rollGain = n[best];
                    g.combat.rollWorse = 0;
                    events::Push("autopilot roll guns now=%d best=%d goal=%.0f counts=%d,%d,%d,%d,%d,%d,%d,%d", n[0],
                                 n[best], ang * 57.3f, n[0], n[1], n[2], n[3], n[4], n[5], n[6], n[7]);
                    why = 1;
                    want = g.combat.rollDir * mym.rollRate;
                    g.combat.rollWhy = 1; // (logged here)
                }
                else
                    why = -1; // (the hold after a roll: nothing else rolls meanwhile)
            }
            else if (g_set.rollGuns && !g_set.rollCommit && n[best] > n[0])
            {
                why = 1;
                const float ang = static_cast<float>(best <= 4 ? best : best - 8) * 45.0f; // the shorter way (deg)
                want = (ang > 0 ? 1.0f : -1.0f) * mym.rollRate * (fabsf(ang) <= 45.0f ? 0.6f : 1.0f);
            }
            else if (g_set.rollFlair && (out.slide == 1 || out.slide == 2) && lo == n[0] && n[0] > 0)
            {
                why = 2;
                want = (out.slide == 2 ? 1.0f : -1.0f) * mym.rollRate; // with the slide's direction
            }
            else if (g_set.rollFlair && lo == n[0] && *std::max_element(n, n + 8) == n[0] && n[0] > 0)
                BarrelStart(8.0f, "fight"); // every angle reaches with the same guns: a full turn costs none
        }
        // a barrel roll in progress goes on (its own events) unless the guns want a roll of their own
        if (why != 1 && mym.rollRate > 0.1f && BarrelRoll(dt, mym.rollRate))
            why = 3;
        // ...else a turn toward the lead is banked (a fight's maneuvers: the lead more than 8 deg off the nose)
        if (why == 0 && mym.rollRate > 0.1f && t.valid && BankIntoTurn(dt, k, t.world, mym.rollRate))
            why = 4;
        if (why != g.combat.rollWhy)
        {
            const int* n = g.combat.rollCounts;
            if (why == 1)
                events::Push("autopilot roll guns now=%d best=%d rate=%.2f counts=%d,%d,%d,%d,%d,%d,%d,%d", n[0],
                             *std::max_element(n, n + 8), want, n[0], n[1], n[2], n[3], n[4], n[5], n[6], n[7]);
            else if (why == 2)
                events::Push("autopilot roll flair slide=%d rate=%.2f guns=%d", out.slide, want, n[0]);
            else if (why == 0)
                events::Push("autopilot roll off"); // (3 = a barrel roll: its own events)
            g.combat.rollWhy = why;
        }
        if (why == 1 || why == 2)
            RollApply(dt, want, mym.rollRate);
    }
    // Guns: hold fire while a friendly or neutral ship is in the corridor (bolts spread: 60 m) or behind the target
    // within gun range (a miss flies on). Checked every 0.2 s.
    if (out.fire && g.target)
    {
        if (g.time - g.combat.lineCheckAt >= 0.2f)
        {
            g.combat.lineCheckAt = g.time;
            const space::Contact tc = FindContact(g.target);
            const float range = out.gunRange > 0 ? out.gunRange : 600.0f;
            // (behind a solar - a generator, a platform - nothing: its own hull takes the misses; the owner, 28 Oct
            // 2026, M13: the guns silent 13 s at a generator 150 m off while a friendly flew somewhere behind it)
            // (`lof_caution`, "too much of an over-emphasis on avoiding friendly fire ...
            // tighten up the overhead of caution" - a bolt hardly spreads: at 0.35 a 21 m corridor, 31 m behind for 140 m)
            const float lc = g_set.lofCaution;
            const float past = tc.ship ? std::min(std::max(0.0f, range - tc.distance), 400.0f * lc) : 0.0f;
            const bool blocked = tc.id && LineOfFireBlocked(tc.id, tc.pos, 60.0f * lc, past, 90.0f * lc);
            if (blocked != g.combat.lineBlocked)
                events::Push("autopilot line-of-fire %s target=%u", blocked ? "blocked" : "clear", g.target);
            g.combat.lineBlocked = blocked;
        }
        if (g.combat.lineBlocked)
            out.fire = false;
    }
    // The target's shield: predicted failure (its fall over the last 1.5 s, g.missiles.shieldHist) vs the real one, and how long it
    // stays down (the offline window a burst must land in)
    {
        float sh = 0, shMax = 0;
        bool up = false;
        if (g.combat.shTarget != g.target)
        {
            g.combat.shTarget = g.target;
            g.combat.shPredAt = g.combat.shPredFor = g.combat.shDownAt = -1;
            g.combat.shWasUp = true;
        }
        const bool shKnown = space::ShieldOf(g.target, sh, shMax, up);
        // the history the missile planner reads too (every frame, whatever the launcher's state)
        if (g.missiles.shieldTarget != g.target)
        {
            g.missiles.shieldTarget = g.target;
            g.missiles.shieldHist.clear();
        }
        g.missiles.shieldHist.push_back({g.time, shKnown && up ? sh : 0.0f});
        while (!g.missiles.shieldHist.empty() && g.missiles.shieldHist.front().first < g.time - 1.5f)
            g.missiles.shieldHist.pop_front();
        if (shKnown && shMax > 0)
        {
            const bool isUp = up && sh > 0;
            float rate = 0;
            if (g.missiles.shieldTarget == g.target && g.missiles.shieldHist.size() >= 2)
            {
                const float span = g.missiles.shieldHist.back().first - g.missiles.shieldHist.front().first;
                if (span > 0.2f)
                    rate = std::max(0.0f, (g.missiles.shieldHist.front().second - g.missiles.shieldHist.back().second) /
                                              span);
            }
            // it fails at its offline threshold, not at 0
            const float thr = std::max(0.0f, space::ShipShieldThreshold(g.target)) * shMax;
            const float toFail = std::max(0.0f, sh - thr);
            if (isUp && rate > 0 && g.combat.shPredAt < 0 && toFail / rate < 2.0f)
            {
                g.combat.shPredAt = g.time;
                g.combat.shPredFor = g.time + toFail / rate;
            }
            if (g.combat.shWasUp && !isUp)
            {
                g.combat.shDownAt = g.time;
                events::Push("autopilot target-shield down target=%u predicted-at=%.2f predicted-err=%.2fs lead=%.2fs",
                             g.target, g.combat.shPredAt,
                             g.combat.shPredFor >= 0 ? g.time - g.combat.shPredFor : -99.0f,
                             g.combat.shPredAt >= 0 ? g.time - g.combat.shPredAt : -1.0f);
                g.combat.shPredAt = g.combat.shPredFor = -1;
            }
            else if (!g.combat.shWasUp && isUp && g.combat.shDownAt >= 0)
            {
                events::Push("autopilot target-shield up target=%u down-for=%.2fs at=%.0f/%.0f", g.target,
                             g.time - g.combat.shDownAt, sh, shMax);
                g.combat.shDownAt = -1;
            }
            g.combat.shWasUp = isUp;
        }
    }
    if (g.time - g.combat.duelSampleAt >= 0.1f)
    {
        const float step = g.combat.duelSampleAt > 0 ? g.time - g.combat.duelSampleAt : 0.1f;
        g.combat.duelSampleAt = g.time;
        space::ShipMotion tm, me;
        if (space::ShipMotionOf(g.target, tm) && space::ShipMotionOf(space::PlayerShipId(), me))
        {
            const float toUs[3] = {me.pos[0] - tm.pos[0], me.pos[1] - tm.pos[1], me.pos[2] - tm.pos[2]};
            const float d = std::max(1.0f, Len3(toUs));
            const bool theyOn = d < 900 && Dot3(tm.fwd, toUs) / d > 0.966f; // 15 deg
            const bool weOn = d < 900 && -Dot3(me.fwd, toUs) / d > 0.966f;
            g.combat.duelT += step;
            g.combat.duelTheyOn += theyOn ? step : 0;
            g.combat.duelWeOn += weOn ? step : 0;
            g.combat.duelBoth += theyOn && weOn ? step : 0;
            if (tm.turnRate > 0 && me.turnRate > 0)
                g.combat.duelTurnRatio = me.turnRate / tm.turnRate;
            g.combat.prevDist = g.combat.nowValid ? g.combat.nowDist : d;
            g.combat.nowValid = true;
            g.combat.nowTheyOn = theyOn;
            g.combat.nowWeOn = weOn;
            g.combat.nowDist = d;
        }
        else
            g.combat.nowValid = false;
    }
    PvpDuel(dt, out);
    g.stats.engaged += dt;
    g.stats.distTime += t.dist * dt;
    if (aligned)
        g.stats.onTarget += dt;
    // (and firing: guns held for a friendly in the line are no proof of an invulnerable target - M12 23 Oct 2026, a
    // shield generator given up at hull 0.03 while Juni crossed the line, flown back to 3 km later)
    if (aligned && t.inRange && out.fire)
        g.combat.onTargetInReach += dt;
    if (g.stats.pressure > 0)
        g.stats.hitTime += dt;
    if (g.combat.evasion > 0.05f)
    {
        g.stats.evadeTime += dt;
        if (g.stats.pressure <= 0)
            g.stats.evadeNoHit += dt;
        if (aligned)
            g.stats.evadeOnTarget += dt;
    }
    if (out.fire)
        g.stats.firing += dt;
    g.combat.wantFire = out.fire;

    // Missiles: only when the fight warrants one (need or the economy) AND the shot is near certain. The launch planner
    // ("engage afterburners to close the distance and aim the nose to guarantee the shortest flight
    // time and the least tracking"; measured: the munition inherits the ship's velocity, docs\FREELANCER_RE.md):
    // predicted flight time from the munition's own data (muzzle, motor delay / accel / burn) and our closing speed;
    // launch when the nose is on, the missile's starting velocity (ours + muzzle along the nose) points within its
    // seeker cone, the flight is short (<= 1.5 s), the target is beyond blast-safe range and inside seeker range;
    // otherwise run in (afterburner, nose on the target, throttle forward) - up to 6 s, then let it be for a while.
    g.missiles.timer -= dt;
    const space::Weapon* launcher = nullptr;
    // (read every frame with a target: torpedoes follow their launcher's refire, not the missile timer - M12: 2-3
    // torpedoes per 50 000-point generator under that timer)
    const std::vector<space::Weapon> weaponsNow = g_set.missiles && g.target && mprules::Autopilot() == mprules::Allow::Full
                                                      ? space::PlayerWeapons()
                                                      : std::vector<space::Weapon>();
    // Torpedo launchers (HpTorpedo..) fire on their own key (USER_LAUNCH_TORPEDOS): never the missile planner's launcher
    // (the torpedo, 2424 hull damage, was "the launcher" - its flight planned, the missile key tapped, not one
    // torpedo ever fired; M12's 45 000-point shield generators took minutes by guns alone under 30 Nomads).
    const space::Weapon* torp = nullptr;
    for (const space::Weapon& w : weaponsNow)
    {
        if (!w.homing || w.ammo <= 0)
            continue;
        if (w.elapsed >= w.refire)
            g.stats.ordPossible += dt; // (the stop line's ord-possible: a launch that could have gone)
        if (_strnicmp(w.hardpoint, "HpTorpedo", 9) == 0)
        {
            if (!torp || w.hullDamage > torp->hullDamage)
                torp = &w;
            continue;
        }
        if (!launcher || w.hullDamage > launcher->hullDamage)
            launcher = &w;
    }
    // (a hold at a static target: a timer left from the last fight waits no longer than a refire - below)
    if (g.missiles.timer > 2.0f && g.hold.on && g.hold.target == g.target && g.target)
    {
        const space::Contact hc = FindContact(g.target);
        if (hc.id && !hc.ship)
            g.missiles.timer = 2.0f;
    }
    // `chase_kill`: no round at the chased ship under half his hull (HDE's full splash at a coast's 110-250 m; a round on
    // the pod he ejects fails the mission - M02's 22355)
    if (g_set.chaseKill && g.target && g.target == g_obj.chaseShip)
    {
        const space::Contact cc = FindContact(g.target);
        if (cc.id && cc.health >= 0 && cc.health < lookahead::kChaseNoMissileHull)
            launcher = nullptr, torp = nullptr;
        // `spawn_charge` (design 3): nor beyond 600 m while he flees (lookahead::kChargeNoMissileBeyond)
        if (cc.id && cc.distance > lookahead::kChargeNoMissileBeyond && ChargeFleeing(g.target))
            launcher = nullptr, torp = nullptr;
    }
    if (g.missiles.timer > 0)
        launcher = nullptr; // the missile planner waits for its timer
    const bool launcherIsTorp = !launcher && torp;
    if (launcherIsTorp)
        launcher = torp; // (the planner's geometry for the torpedo alone)
    if (!launcher && g.time - g.missiles.runSeen > 1)
        g.missiles.run = 0;
    if (launcher)
    {
        const space::Contact c = FindContact(g.target);
        // Our damage rate in hull + shield points (OurDps) against the points left (hull + shield while up).
        const float ourDps = OurDps();
        const float hp = c.id && c.maxHit > 0 ? PointsLeft(c) : 0.0f;
        // Outmatched from the fight as measured, not a head count (was: threat in fighter units > our health
        // fractions - four rogues were "outmatched" at full hull before a shot landed, M02 18 Oct 2026, the user: "the
        // situation did not call for it"): the guns' time to kill every hostile around (their hull left / our damage
        // rate) against our time to die at the rate we lose shield + hull. Unknown until 3 s of both: no launch. (Their
        // points with the shield, as ours and our damage rate count it.)
        float hostileHp = 0;
        for (const space::Contact& h : Contacts())
            if (h.ship && h.attitude < 0 && h.maxHit > 0 && (h.targetsPlayer || h.distance < 3000))
                hostileHp += PointsLeft(h);
        const float lossRate =
            g.stats.engaged > 3 ? g.combat.lost / g.stats.engaged : 0.0f; // of our full points per second
        const float left =
            !g.combat.hp.empty() && g.combat.totalHp > 0 ? g.combat.hp.back().second / g.combat.totalHp : 1.0f;
        const float timeToDie = lossRate > 0 ? left / lossRate : 1e9f;
        const float timeToKill = ourDps > 0 ? hostileHp / ourDps : 0.0f;
        const bool missileOutmatched = ourDps > 0 && lossRate > 0 && timeToDie < 1.25f * timeToKill;
        const bool big = (c.type & (kGunboatBit | kCruiserBit | kCapitalBit)) != 0;
        // The more hull damage taken, the sooner a missile is worth it ("very delicate"): the
        // kill time that warrants one scales with the hull left - 12 s at full hull, smoothly down to ~4 s at 35 %.
        const float hullLeft = hull >= 0 ? Clampf(hull, 0.35f, 1.0f) : 1.0f;
        const bool slow = ourDps > 0 && hp / ourDps > 12.0f * hullLeft;
        // Never a missile that arrives after the target is dead (the stray of M02's jailbreak fight
        // - "You have attacked a friendly object": "my energy weapons + the offensive consumables already launched will
        // destroy the target by the time a new one hits"): the guns' damage over the new missile's flight plus every
        // missile of ours already homing on it, against the hull it has left.
        fl::Kinematics self3 = {};
        fl::PlayerKinematics(self3);
        const float flight = MissileFlightTime(*launcher, t.dist, t.closing + launcher->muzzleVelocity);
        float inFlight = 0;
        const u32 me = space::PlayerShipId();
        for (const space::Missile& m : space::Missiles())
            if (m.owner == me && m.targetId == g.target)
                inFlight += m.hullDamage;
        // (`static_full_dps`: at a static target only the rounds already homing - OurDps counts the launchers too, so over a
        // 9-16 s flight they were counted twice: the M13 city's missiles stopped at 51k of a generator's 75k, mm9; the
        // "maximum use of all munitions at all times" at big solars; a stray there hits nothing)
        const bool deadFirst =
            hp > 0 && (g_set.staticFullDps && !c.ship ? inFlight >= hp : ourDps * flight + inFlight >= hp);
        // A seeker can pick up what crosses its cone: a wide corridor, 400 m past the target, plus the blast. At a static
        // target close by (a solar whose hull the ray test sees within 300 m) the round cannot miss or fly on past the
        // hull: the corridor is the blast plus 30 m and ends at the target (M13 St03: the wide corridor - the
        // allies around and the sphere's shell "behind" each generator - held 90 % of the salvos at the hold).
        const float closeHull = c.id && !c.ship ? HullGap(self3.pos, c.pos) : -1.0f;
        // (a huge static target counts as close within its seeker's reach: it never dodges, and the flight-time gate held
        // every round through the coast in - "the ship still fires too late")
        const bool staticClose = (closeHull >= 0 && closeHull < 300.0f) || (!c.ship && t.radius >= 500.0f && closeHull >= 0);
        // (the corridors scale by `lof_caution`, the blast never)
        const float lc = g_set.lofCaution;
        const bool friendlyNear =
            c.id && (staticClose ? LineOfFireBlocked(c.id, c.pos, 30.0f * lc + launcher->blastRadius, 0.0f, 0.0f)
                                 : LineOfFireBlocked(c.id, c.pos, 200.0f * lc + launcher->blastRadius, 400.0f * lc,
                                                     400.0f * lc + launcher->blastRadius));
        // The economy (the player fires cheap missiles that refill in play freely - "tractor all"
        // - and keeps costly or scarce ones for need): free use when the missile is cheap for the player (price <= 1 %
        // of net worth: Javelin 25 cr) and the stock plus what the hostiles here are expected to drop (the game's
        // loot rule over what their launchers carry) stays >= 10; otherwise only when the fight needs one.
        const MissileEconomy& eco = Economy();
        // (`missile_keep_big`: while battery_budget's look-ahead holds big fights ahead - read within 3 s - a small ship
        // gets no free missile; the need path below stays)
        const bool keepBig = g_set.missileKeepBig && c.ship && !big && g.combat.fightsAheadW > 0 &&
                             g.time - g.combat.fightsAheadAt >= 0 && g.time - g.combat.fightsAheadAt < 3.0f;
        const bool freeUse = eco.price >= 0 && eco.cheap && eco.stock + eco.refill >= 10 && !keepBig;
        if (keepBig && eco.price >= 0 && eco.cheap)
        {
            static Throttle said;
            if (said.Due(g.time, 5))
                events::Push("autopilot missile-keep target=%u: no free use, big fights ahead w=%d stock=%d", g.target,
                             g.combat.fightsAheadW, eco.stock);
        }
        // The missile's starting velocity: ours plus the muzzle velocity along the nose - within the seeker's half cone
        // of the line to the target, or the seeker never sees it (a sideways drift throws the missile off the line).
        float fwd[3];
        geom::Forward(fwd, self3.rot);
        const float v0[3] = {self3.vel[0] + fwd[0] * launcher->muzzleVelocity,
                             self3.vel[1] + fwd[1] * launcher->muzzleVelocity,
                             self3.vel[2] + fwd[2] * launcher->muzzleVelocity};
        // (`missile_lead`; "aim the ship and missile launcher in anticipation of where the target
        // will be") the line to where a moving target will be at impact - its velocity over the predicted flight - not to
        // where it is: a crossing target flies out of a seeker cone that only held its present position
        space::ShipMotion tmv;
        const bool moving = g_set.missileLead && c.ship && space::ShipMotionOf(g.target, tmv) && tmv.speed >= 20.0f;
        const float lead = moving ? std::min(flight, 3.0f) : 0.0f;
        const float los[3] = {c.pos[0] + tmv.vel[0] * lead - self3.pos[0], c.pos[1] + tmv.vel[1] * lead - self3.pos[1],
                              c.pos[2] + tmv.vel[2] * lead - self3.pos[2]};
        const auto dot3 = [](const float* a, const float* b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; };
        const float v0n = std::sqrt(dot3(v0, v0)), losn = std::sqrt(dot3(los, los));
        // seeker_fov_deg is a half-angle (the game keeps its cosine); 80 % of it as the margin
        const float halfCone = (launcher->seekerFovDeg > 0 ? launcher->seekerFovDeg : 35.0f) * 3.14159265f / 180;
        // (a static hull within 300 m with the nose on it: the round flies into the hull whatever the seeker sees - s9m, M12
        // gen 3: the hold's lateral slide put a torpedo's start velocity off the cone at every other refire, on-line=0 with
        // aligned=1 at a 48-51 m gap)
        const bool onLine =
            (staticClose && (aligned || strafeLaunch)) || (v0n > 1 && losn > 1 && dot3(v0, los) / (v0n * losn) > cosf(halfCone * 0.8f));
        // No round into a solid on the way (M12 22 Oct 2026: a torpedo at a generator behind the Nomad lair's lattice
        // struck the lattice beside us - hull 93 % -> dead, the frame shows the blast at our nose): the straight line to
        // the target through the game's physics (real hulls; the target itself and ships do not count). Read only for a
        // launch that is otherwise ready, logged when it holds one.
        // ...and along the nose: a round leaves along it and turns slowly (the Starkiller 0.58 rad/s; the sheet of the
        // second M12 death shows the blast at launch beside the lair's wall, the line to the target clear)
        const auto pathClear = [&]
        {
            const float ahead[3] = {self3.pos[0] + fwd[0] * 400.0f, self3.pos[1] + fwd[1] * 400.0f,
                                    self3.pos[2] + fwd[2] * 400.0f};
            u32 blocker = c.id ? space::SightBlocker(self3.pos, c.pos, c.id) : 0;
            if (!blocker && c.id)
                blocker = space::SightBlocker(self3.pos, ahead, c.id);
            static Throttle heldAt;
            if (blocker && heldAt.Due(g.time, 2))
            {
                events::Push("autopilot launch-held target=%u dist=%.0f: a solid (class 0x%X) in the way", g.target,
                             t.dist, blocker);
            }
            return blocker == 0;
        };
        // (and outside a big target's own radius: launched from inside its hull envelope a round strikes the hull beside
        // us - the M13 torpedo)
        const float minDist = std::max({120.0f, launcher->blastRadius * 4, t.radius > 50 ? t.radius + 100.0f : 0.0f});
        // The blast rule by the real hull where the ray test sees one (the top-3 runners fire
        // their salvos from 0-100 m at every static and capital target - speedrun.md V4): the round must burst at least
        // twice its blast radius (30 m at the least; torpedo 25 m, missiles 16 m in the HDE data) from us, at the first
        // hull on the straight line to the target and along the nose (it leaves along the nose and turns slowly), the
        // target's own hull included. Solars and rocks only: a ship target (its hull is no solid to the test) keeps the
        // centre-distance rule (`fallback`) - the M13 battleship torpedo.
        const float noseEnd[3] = {self3.pos[0] + fwd[0] * 400.0f, self3.pos[1] + fwd[1] * 400.0f,
                                  self3.pos[2] + fwd[2] * 400.0f};
        const float gapLine = closeHull;
        const float gapNose = gapLine >= 0 ? space::RayHitDistance(self3.pos, noseEnd) : -1.0f;
        // (`flank`: a round is about to go - the flank rays and their log only then: 8 rays a frame at every aligned hold,
        // and a log of salvos that never were; review, session 12)
        // `launch_site_check` (WORK_QUEUE Q4 slice 1; "You MUST maintain full DPS while surviving
        // as efficiently as possible" - splash avoided by WHERE we fire from): at a static or big target the round is
        // flown ahead from the launcher (core\game\ordnance.h RoundImpact: our velocity + the muzzle along the nose, the
        // motor, the seeker's turn, the game's ray test with the fuse) to where it bursts, and we to where this frame's
        // station / steer goal and our velocity carry us by then (the worse of that and our velocity alone); a launch
        // only when the two lie at least its blast + our radius + launch_site_margin apart. It replaces the proxies
        // there: self_burst's learned distance and rush_hold_fire.
        // (`bs_approach`: the check at a blind-spot target too, without launch_site_check everywhere - its rush launches
        // all the way in where self_burst's learned distance held them 7-33 times a fight (blind_rush), and the barrier /
        // city holds keep today's rules: the check there held launches at 41-45 m hull gaps, set ab2)
        // (`blind_site_check`: the same without the dash - ab8 without either lost 2 of 5 legs to its own rounds at the
        // battleships, a torpedo at a 51 m line gap with the target's hull 11 m beside us and missiles bursting 81-83 m
        // off; ab5-ab7 with the check there lost none at a battleship)
        // (the probe bsc0 with the check there died at +38 s: a missile passed at a predicted 56 m against our measured
        // 80-95 m reach, and at margin 55 every launch was held (ab2) - blind_site_check keeps only the torpedo's
        // hull-beside hold below)
        const bool siteCheck = c.id && ((g_set.launchSiteCheck && (!c.ship || big)) ||
                                        (g_set.bsApproach && g.combat.blindOn && g.combat.blindTarget == g.target));
        // the round flown ahead from the launcher to where it bursts (core\game\ordnance.h RoundImpact; launch_site_check's
        // and city_hold_side's), with the horizon it was flown to
        const auto impactOf = [&](const space::Weapon& w, float& horizon) -> ordnance::Impact
        {
            const ordnance::Round r = RoundOf(w);
            const float ft = MissileFlightTime(w, t.dist, t.closing + w.muzzleVelocity);
            horizon = Clampf(ft < 1e8f ? ft + 1.0f : 12.0f, 2.0f, 12.0f);
            // (a ship's hull is no solid to the ray test: its sphere is its reach)
            const float reach = c.ship ? std::max(1.0f, t.radius) : 0.0f;
            return ordnance::RoundImpact(r, self3.pos, self3.vel, fwd, c.pos, reach, horizon,
                                         [](const float* a, const float* b) { return space::RayHitDistance(a, b); });
        };
        const auto siteSafe = [&](const space::Weapon& w, bool say) -> bool
        {
            if (w.elapsed < w.refire)
                return true; // (nothing leaves before its refire: the verdict is read in the launch frame)
            // one simulation per launcher kind and 0.1 s (up to ~60 rays each)
            struct Memo
            {
                float at = -10;
                u32 target = 0;
                bool ok = true;
            };
            static Memo memo[2];
            const int kind = _strnicmp(w.hardpoint, "HpTorpedo", 9) == 0 ? 0 : 1;
            if (memo[kind].target == g.target && g.time - memo[kind].at >= 0 && g.time - memo[kind].at < 0.1f)
                return memo[kind].ok;
            float horizon = 0;
            const ordnance::Impact im = impactOf(w, horizon);
            ordnance::Goal goal;
            flight::Model fm;
            const bool haveFm = flight::ReadModel(fm) && fm.engine.ok();
            goal.accel = haveFm && fm.engine.mass > 0 ? fm.engine.force / fm.engine.mass : 30.0f;
            if (out.station)
                goal.pos = out.stationPos; // (the station keeping's law: 0.4 / s, <= 60 m/s)
            else if (out.steerTo)
            {
                goal.pos = out.steerPos;
                goal.gain = 100.0f;
                const float top = haveFm ? (out.afterburn && fm.hasBurner && fm.burner.ok() ? fm.burner.top()
                                                                                            : fm.engine.top())
                                         : 80.0f;
                goal.maxSpeed = top * Clampf(out.steerThrottle, 0.0f, 1.0f);
            }
            const float tI = im.t >= 0 ? im.t : horizon;
            float usGoal[3], usCoast[3];
            ordnance::PredictPos(self3.pos, self3.vel, goal, tI, usGoal);
            ordnance::PredictPos(self3.pos, self3.vel, ordnance::Goal{}, tI, usCoast);
            const float need = ordnance::Need(w.blastRadius, space::PlayerRadius(), g_set.launchSiteMargin);
            float sepGoal = 1e9f, sepCoast = 1e9f;
            ordnance::LaunchSafe(im, usGoal, need, &sepGoal);
            ordnance::LaunchSafe(im, usCoast, need, &sepCoast);
            const float sep = std::min(sepGoal, sepCoast);
            const bool ok = sep >= need;
            memo[kind] = {g.time, g.target, ok};
            static Throttle saidAt[2][2];
            if (say && saidAt[kind][ok ? 1 : 0].Due(g.time, 2))
                events::Push("autopilot launch-site %s weapon=%s target=%u impact-dist=%.0f t=%.2f sep=%.0f need=%.0f "
                             "cause=%s sep-goal=%.0f sep-coast=%.0f goal=%s det=%.2f dist=%.0f speed=%.0f",
                             ok ? "ok" : "held", kind == 0 ? "torp" : "missile", g.target,
                             im.cause != ordnance::kNoImpact ? geom::Dist(im.pos, self3.pos) : -1.0f, im.t,
                             sep < 1e8f ? sep : -1.0f, need, ordnance::CauseName(im.cause),
                             sepGoal < 1e8f ? sepGoal : -1.0f, sepCoast < 1e8f ? sepCoast : -1.0f,
                             out.station ? "station" : out.steerTo ? "steer" : "none", w.detonationDist, t.dist,
                             Len3(self3.vel));
            return ok;
        };
        // (`self_burst`: rounds at this target burst at their launch from closer in - learned, autopilot.cpp g_selfBurst)
        // (skipped under launch_site_check at its targets: a proxy of that check)
        const auto burstSafe = [&]
        {
            const auto sb = g_selfBurst.find(g.target);
            if (!g_set.selfBurst || siteCheck || sb == g_selfBurst.end() || t.dist >= sb->second)
                return true;
            static Throttle saidAt;
            if (saidAt.Due(g.time, 2))
                events::Push("autopilot launch-held target=%u dist=%.0f: rounds burst at the launch inside %.0f m", g.target,
                             t.dist, sb->second);
            return false;
        };
        const auto blastSafe = [&](const space::Weapon& w, float fallback, bool flank)
        {
            if (!burstSafe())
                return false;
            if (gapLine < 0)
                return t.dist >= fallback && (!siteCheck || siteSafe(w, flank));
            // (the blast radius + 5 m, 20 m at the least: the near-contact hold at 30 m keeps every salvo; was 2x)
            // (+ our own radius: the blast reaches the ship's hull, not its centre - M12 gen 3 / gen 2, three deaths at full
            // hull while torpedoes went off ~30 m from the generator's hull, blast 25 m, session 8)
            const float need = std::max(20.0f, w.blastRadius + std::max(5.0f, space::PlayerRadius()) + 10.0f);
            if (gapLine < need || (gapNose >= 0 && gapNose < need))
                return false;
            // ...nor with another solid beside us within that reach (s11o9-i2, M09: the arch's shield generator sits on a
            // big rock; the hold put the ship ~15 m off the rock face, the two centre rays were clear, the first torpedo
            // burst on the rock (1,996 points, shield 0.05), the second 1.5 s later killed the ship at full hull in 10 ms):
            // rays across the nose (right / left / up / down) and 45 deg forward of them, the target itself skipped
            const u32 ign[1] = {c.id};
            for (int axis = 0; axis < 2 && g_set.blastFlank && flank; ++axis)
                for (int sgn = -1; sgn <= 1; sgn += 2)
                    for (int fwdMix = 0; fwdMix < 2; ++fwdMix)
                    {
                        const float sx = sgn * self3.rot[0 * 3 + axis], sy = sgn * self3.rot[1 * 3 + axis],
                                    sz = sgn * self3.rot[2 * 3 + axis];
                        const float f = fwdMix ? 1.0f : 0.0f, n = fwdMix ? need * 0.7071f : need;
                        const float to[3] = {self3.pos[0] + (sx + f * fwd[0]) * n, self3.pos[1] + (sy + f * fwd[1]) * n,
                                             self3.pos[2] + (sz + f * fwd[2]) * n};
                        const float h = space::RayHitDistance(self3.pos, to, ign, 1);
                        if (h >= 0)
                        {
                            static Throttle saidAt;
                            if (saidAt.Due(g.time, 2))
                            {
                                events::Push(
                                    "autopilot launch-held target=%u dist=%.0f: a solid %.0f m beside us (blast "
                                    "reach %.0f m)",
                                    g.target, t.dist, h, need);
                            }
                            return false;
                        }
                    }
            // (after the rules above, which stay: the launch site itself)
            return !siteCheck || siteSafe(w, flank);
        };
        const float maxDist = launcher->seekerRange > 0 ? launcher->seekerRange : 1000.0f;
        // (the seeker's reach by the hull at a huge target - aim::Current aims at its hull point: the city generators'
        // centre lies ~430 m behind it, and the launchers held through the whole coast - "weapons
        // fire started too late")
        const float wt[3] = {t.world[0] - self3.pos[0], t.world[1] - self3.pos[1], t.world[2] - self3.pos[2]};
        const float reachDist = t.radius >= 500.0f ? std::min(t.dist, Len3(wt)) : t.dist;
        // ...and the nose on it counts beyond gun reach (`aligned` is the guns' - in their range: no round left in the coast)
        bool launchAligned = aligned;
        if (!launchAligned && t.radius >= 500.0f && !c.ship && t.onScreen)
        {
            const float sw = static_cast<float>(fl::ScreenWidth()), sh = static_cast<float>(fl::ScreenHeight());
            const float ox = (t.screenX - sw * 0.5f) / (sw * 0.5f), oy = (t.screenY - sh * 0.5f) / (sh * 0.5f);
            launchAligned = sqrtf(ox * ox + oy * oy) < g_set.fireCone;
        }
        // (`strafe_pass`: the nose on the pass's hull point, within the seekers' reach - beyond the guns' too)
        if (strafeLaunch)
            launchAligned = true;
        constexpr float kGoodFlight = 1.5f;
        constexpr float kChargeFleeMissileFlight = 4.5f; // the fleeing chase's gate (below): the 4.1 s worst case + margin
        // A missile meeting a shield is spent on it (M01b: a Javelin launched a touch too soon was
        // swallowed by a Rogue's failing shield): launch only when the target's shield (the game's
        // pub::SpaceObj::GetShieldHealth) is down at impact - down now, or emptied before the missile arrives at the
        // rate it is falling (sampled each frame over the last 1.5 s: our guns' and anyone else's fire, as measured).
        float shieldNow = 0, shieldMax = 0;
        bool shieldUp = false;
        const bool shieldKnown = space::ShieldOf(g.target, shieldNow, shieldMax, shieldUp);
        float shieldRate = 0; // (g.missiles.shieldHist: kept every frame above) // points per second falling
        if (g.missiles.shieldHist.size() >= 2)
        {
            const float span = g.missiles.shieldHist.back().first - g.missiles.shieldHist.front().first;
            if (span > 0.2f)
                shieldRate =
                    std::max(0.0f, (g.missiles.shieldHist.front().second - g.missiles.shieldHist.back().second) / span);
        }
        // (it fails at its generator's offline threshold, not at 0: ShipShieldThreshold)
        const float shieldToFail =
            std::max(0.0f, shieldNow - std::max(0.0f, space::ShipShieldThreshold(g.target)) * shieldMax);
        const bool shieldDownAtImpact =
            !shieldKnown || !shieldUp || shieldToFail <= 0 || shieldToFail <= shieldRate * flight;
        // The burst ("missiles as a burst - needs high prediction accuracy"): a failed shield is a
        // reason of its own - it stays down for its generator's offline_rebuild_time (measured: exactly 12.00 s every
        // time, back at the offline threshold) and every missile landing in that window hits the hull. The window
        // left: from the failure (g.combat.shDownAt, the shield tracking above), or the whole window when the shield fails
        // before this missile lands (the threshold-aware prediction; to 0 it came true 1.0-1.5 s early). A missile
        // counts only if it lands with 0.5 s to spare; the overkill rule (deadFirst: the guns over the flight + every
        // missile of ours already homing on it) ends the burst. Inside the window the launcher refires at its own
        // rate, not the personality's interval (below).
        const float rebuild = space::ShipShieldRebuild(g.target) > 0 ? space::ShipShieldRebuild(g.target) : 12.0f;
        float windowLeft = -1;
        if (g.combat.shTarget == g.target && g.combat.shDownAt >= 0)
            windowLeft = rebuild - (g.time - g.combat.shDownAt);
        else if (shieldKnown && shieldUp && shieldRate > 0 && shieldToFail / shieldRate < flight)
            windowLeft = rebuild - (flight - shieldToFail / shieldRate); // fails mid-flight: open at impact
        const bool burst = windowLeft >= flight + 0.5f && hp > 0.5f * launcher->hullDamage;
        const bool want = (freeUse || missileOutmatched || big || slow || burst) && !friendlyNear && c.id;
        // The run-in (never launch "absent-mindedly"): the target type's learned flight limit
        // (MissileGate) instead of a fixed one, and no launch while the run is still shortening the flight fast - the
        // afterburner run closes in, so a moment's wait buys a shorter flight (> 0.4 s less per second; at most 3 s into
        // the run, and never inside 0.8 s of flight, the near-certain shots).
        space::ShipMotion tmo;
        // (`spawn_charge`: a ship fleeing straight for his run point does not dodge - the gate is the missile's catch time
        // from the coast, 2.1-4.1 s at 150-250 m behind a 270-300 m/s target from our ~290 (ordnance::FlightTime over the
        // HDE missiles: accel 22 m/s2 for 2.75 s, muzzle 30.3, lifetime 13.75 s - game-facts 31 Oct 2026); the 1.5 s gate
        // never passed there and the runners' Javelins go in the coast (M02.md M02.pod). Within kChargeNoMissileBeyond)
        const float gate = ChargeFleeing(g.target)                ? kChargeFleeMissileFlight
                           : space::ShipMotionOf(g.target, tmo) ? std::min(kGoodFlight, MissileGate(tmo.arch))
                                                                 : kGoodFlight;
        const float flightDrop = g.missiles.prevFlightAt > 0 && g.time > g.missiles.prevFlightAt
                                     ? (g.missiles.prevFlight - flight) / (g.time - g.missiles.prevFlightAt)
                                     : 0.0f;
        g.missiles.prevFlight = flight;
        g.missiles.prevFlightAt = g.time;
        const bool improving = flightDrop > 0.4f && flight > 0.8f && g.missiles.run > 0 && g.missiles.run < 3.0f;
        // (the flight-time gate is for targets that dodge: a static one never does - from a hold the flight reads ~2.8 s,
        // the round starting at the muzzle velocity, and no missile ever went)
        // (`missile_lead`; the owner: "launch missiles while afterburning") at a moving ship the launch comes out of an
        // afterburner run of at least 0.4 s wherever a run can be flown (engaging, not evading, not holding): the round
        // inherits our velocity - the closer and faster, the less the target can do (gates mpb2 / oddsab, 645 planned
        // rounds: closing 50-150 m/s hit 0.84, over 150 m/s 0.93)
        const bool runPossible = g.tactic == Tactic::Engage && g.combat.evasion < 0.05f &&
                                 !(g.hold.on && g.hold.target == g.target) && !launcherIsTorp &&
                                 !(g.combat.blindOn && g.combat.blindTarget == g.target);
        const bool burnOk = !moving || !runPossible || g.missiles.run >= 0.4f;
        // (rush_hold_fire: closing on a blind spot; skipped under launch_site_check at its targets - a proxy of that check)
        const bool rushHold = g_set.rushHoldFire && g.combat.rushing && !siteCheck;
        // `torp_reach` / `missile_reach` at a big or static target: the hull along the line and the nose at least this far
        // (probe tr0: missiles launched 148 m out burst 14 / 23 m from us, -2,828; logged when a refired round is held)
        const auto reachOk = [&](float reach, const space::Weapon* w, const char* kind)
        {
            if (reach <= 0 || !(big || !c.ship) || !w || w->elapsed < w->refire ||
                (gapLine >= reach && (gapNose < 0 || gapNose >= reach)))
                return true;
            static Throttle said[2];
            if (said[kind[0] == 't' ? 0 : 1].Due(g.time, 2))
                events::Push("autopilot launch-held target=%u: %s reach %.0f m, hull gap %.0f / nose %.0f", g.target, kind,
                             reach, gapLine, gapNose);
            return false;
        };
        // `city_hold_side` at a hold the platforms guard: a round only with its predicted burst (impactOf, the launch
        // site's simulation) and the nearest hull round us (the line, the nose and six 100 m rays along the world axes,
        // as the torpedo-launch line reads it) both >= 100 m off - ab11-b_i4: our torpedo at a 129 m gap with another hull
        // 72 m off, 0.93 -> 0.26. Read in the launch frame, one simulation per kind and 0.1 s
        const auto cityOrdOk = [&](const space::Weapon* w, const char* kind)
        {
            if (!g.hold.city || !g.hold.on || g.hold.target != g.target || !w || w->elapsed < w->refire)
                return true;
            struct Memo
            {
                float at = -10;
                u32 target = 0;
                bool ok = true;
            };
            static Memo memo[2];
            const int ki = kind[0] == 't' ? 0 : 1;
            if (memo[ki].target == g.target && g.time - memo[ki].at >= 0 && g.time - memo[ki].at < 0.1f)
                return memo[ki].ok;
            float hull = -1.0f;
            for (float h : {gapLine, gapNose})
                if (h >= 0 && (hull < 0 || h < hull))
                    hull = h;
            for (int axis = 0; axis < 3; ++axis)
                for (int sgn = -1; sgn <= 1; sgn += 2)
                {
                    float to[3] = {self3.pos[0], self3.pos[1], self3.pos[2]};
                    to[axis] += sgn * cityhold::kOrdnanceClear;
                    const float h = space::RayHitDistance(self3.pos, to);
                    if (h >= 0 && (hull < 0 || h < hull))
                        hull = h;
                }
            float horizon = 0;
            const ordnance::Impact im = impactOf(*w, horizon);
            const float impact = im.cause != ordnance::kNoImpact ? geom::Dist(im.pos, self3.pos) : -1.0f;
            const bool ok = cityhold::OrdnanceOk(impact, hull);
            memo[ki] = {g.time, g.target, ok};
            static Throttle said[2];
            if (!ok && said[ki].Due(g.time, 2))
                events::Push("autopilot city-hold launch-held target=%u kind=%s impact=%.0f hull=%.0f need=%.0f "
                             "platforms=%d",
                             g.target, kind, impact, hull, cityhold::kOrdnanceClear, g.hold.cityPlatforms);
            return ok;
        };
        const bool ready = !rushHold && launchAligned && onLine && burnOk && (staticClose || (flight <= gate && !improving)) &&
                           blastSafe(*launcher, minDist, want && launcher->elapsed >= launcher->refire) &&
                           reachDist <= maxDist && shieldDownAtImpact &&
                           (launcherIsTorp || reachOk(g_set.missileReach, launcher, "missile")) &&
                           (launcherIsTorp || cityOrdOk(launcher, "missile"));
        // At a hold the salvo should go out at every refire: what holds it back, every 2 s (tuning the hold-and-salvo)
        const bool holdingHere = g.hold.on && g.hold.target == g.target;
        // (and in a big target's blind-spot fight: why launches stop there was not logged - run-judge 29 Oct 2026, the
        // shape60 contact hold fired 1-6 torpedoes in a minute; the blast checks and the rush hold are logged too)
        const bool blindHere = g.combat.blindOn && g.combat.blindTarget == g.target;
        if (((holdingHere && g.hold.at) || blindHere) && torp && torp->elapsed >= torp->refire)
        {
            static Throttle saidAt;
            if (saidAt.Due(g.time, 2))
            {
                int near = 0;
                for (const space::Contact& o : Contacts())
                    if (o.ship && o.id != g.target && o.distance < 150.0f)
                        ++near;
                const float torpFallback = std::max({150.0f, torp->blastRadius * 4, t.radius + 150.0f});
                events::Push(
                    "autopilot salvo-check%s target=%u dist=%.0f gap=%.0f nose-gap=%.0f aligned=%d on-line=%d "
                    "shield-down=%d friendly=%d ships-150m=%d hp=%.0f want=%d ready=%d flight=%.2f dead-first=%d "
                    "hold-gap=%.0f torp-need=%.0f to-point=%.0f rush-hold=%d blast-missile=%d blast-torp=%d path=%d",
                    blindHere ? " blind" : "", g.target, t.dist, gapLine, gapNose, aligned ? 1 : 0, onLine ? 1 : 0,
                    shieldDownAtImpact ? 1 : 0, friendlyNear ? 1 : 0, near, hp, want ? 1 : 0, ready ? 1 : 0, flight,
                    deadFirst ? 1 : 0, HoldGap(),
                    std::max(20.0f, torp->blastRadius + std::max(5.0f, space::PlayerRadius()) + 10.0f),
                    holdingHere ? geom::Dist(g.hold.pos, self3.pos) : g.combat.blindDist, rushHold ? 1 : 0,
                    launcher && blastSafe(*launcher, minDist, false) ? 1 : 0, blastSafe(*torp, torpFallback, true) ? 1 : 0,
                    pathClear() ? 1 : 0);
            }
        }
        // Torpedoes at big or static targets with hit points for more than two of them, on the same aim, line and
        // shield-at-impact checks, whenever a torpedo launcher has refired.
        const float torpFloor = 2 * (torp ? torp->hullDamage : 0.0f);
        // (`blind_site_check`: no torpedo with any hull - the target's too, which the flank rays skip - within its blast
        // reach along a world axis: ab8-b_i5 died 33 ms after one launched at a 51 m line gap, the hull 11 m beside us)
        const auto hullBeside = [&]
        {
            // (`static_site_check`: at a static hold too - both M13 city deaths were our own torpedoes with a hull 31 / 51 m
            // from us, ab5-b_i5 / ab7-b_i5: the driver's M13 log analysis, 31 Oct 2026)
            const bool atBlind = g_set.blindSiteCheck && g.combat.blindOn && g.combat.blindTarget == g.target;
            const bool atHold = g_set.staticSiteCheck && g.hold.on && g.hold.target == g.target;
            if (!torp || !(atBlind || atHold))
                return false;
            const float reach = torp->blastRadius + std::max(5.0f, space::PlayerRadius()) + 10.0f;
            for (int axis = 0; axis < 3; ++axis)
                for (int sgn = -1; sgn <= 1; sgn += 2)
                {
                    float to[3] = {self3.pos[0], self3.pos[1], self3.pos[2]};
                    to[axis] += sgn * reach;
                    if (space::RayHitDistance(self3.pos, to) >= 0)
                    {
                        static Throttle said;
                        if (said.Due(g.time, 2))
                            events::Push("autopilot launch-held target=%u: a hull within %.0f m beside us (torpedo)",
                                         g.target, reach);
                        return true;
                    }
                }
            return false;
        };
        if (!rushHold && torp && torp->elapsed >= torp->refire && (big || !c.ship) && hp > torpFloor && launchAligned && onLine &&
            // (beyond the target's own radius + 150 m: from a blind spot inside its hull envelope the torpedo struck the
            // hull beside us - M13 22 Oct 2026, 100 % -> 67 % hull, the second one killed the ship)
            !friendlyNear && shieldDownAtImpact &&
            blastSafe(*torp, std::max({150.0f, torp->blastRadius * 4, t.radius + 150.0f}), true) &&
            reachDist <= (torp->seekerRange > 0 ? torp->seekerRange : 1500.0f) &&
            // (no other ship within 150 m: a torpedo that meets one at the muzzle bursts beside us)
            [&]
            {
                for (const space::Contact& o : Contacts())
                    if (o.ship && o.id != g.target && o.distance < 150.0f)
                        return false;
                return true;
            }() &&
            pathClear() && !hullBeside() &&
            // (`torp_reach`: the hull along the line and the nose at least this far - our torpedo's 2,424 reached us from
            // 80-95 m (FREELANCER_RE "Explosion damage"); ab8-b_i5 died 33 ms after one at a 51 m line gap, bsc0's burst
            // 94 m off, ab8-b_i4 to hull 0.09 at the barrier: run-judge 31 Oct 2026)
            reachOk(g_set.torpReach, torp, "torpedo") && cityOrdOk(torp, "torpedo"))
        {
            out.torpedo = true;
            if (g.strafe.on)
                ++g.strafe.launches;
            g.missiles.torpFrame = g.frames;
            g.missiles.torpDist = t.dist;
            // (the hull gaps the blast rule read, and the nearest hull of six 100 m rays along the world axes: the M13
            // burst probe, 29 Oct 2026 - salvos from above the Nomad battleship burst at the launch with both rays clear)
            float nearHull = -1.0f;
            for (int axis = 0; axis < 3; ++axis)
                for (int sgn = -1; sgn <= 1; sgn += 2)
                {
                    float to[3] = {self3.pos[0], self3.pos[1], self3.pos[2]};
                    to[axis] += sgn * 100.0f;
                    const float h = space::RayHitDistance(self3.pos, to);
                    if (h >= 0 && (nearHull < 0 || h < nearHull))
                        nearHull = h;
                }
            events::Push("autopilot torpedo-launch target=%u dist=%.0f hp=%.0f dmg=%.0f ammo=%d gap=%.0f nose-gap=%.0f "
                         "near-hull=%.0f",
                         g.target, t.dist, hp, torp->hullDamage, torp->ammo, gapLine, gapNose, nearHull);
        }
        if (want && !deadFirst && ready && !launcherIsTorp && pathClear())
        {
            out.missile = true;
            if (g.strafe.on)
                ++g.strafe.launches;
            if (moving && runPossible)
                out.afterburn = true; // (the run's burn through the launch frame)
            g.missiles.tapFrame = g.frames;
            g.missiles.lastLaunchDist = t.dist;
            g.missiles.lastLaunchFlight = flight;
            g.missiles.lastLaunchClosing = t.closing;
            g.missiles.lastLaunchGeom = MissileGeometry(g.target, *launcher, flight, t.radius);
            // inside a shield's offline window: the next one at the launcher's refire (the burst)
            // ...and at a static target we hold at (M12: the pilot's missile_launch_interval_time,
            // 20-45 s for military pilots, let one 4-missile salvo go per 50 000-point generator while the torpedoes went
            // every 2 s; the user: "all weapons, all missiles and all mines until each generator was destroyed")
            // ...and at every solar and big ship ("when the objective wants big solars destroyed
            // like generators or capital ships - maximum use of all munitions at all times")
            g.missiles.timer = burst || !c.ship || big
                                   ? std::max(0.5f, launcher->refire)
                                   : Pf(g_p.Missile.missile_launch_interval_time, 1, 30, 5);
            events::Push(
                "autopilot missile-launch target=%u dist=%.0f why=%s%s%s%s%s hp=%.0f dps=%.0f hull=%.2f "
                "kill-all=%.0fs die=%.0fs price=%.0f stock=%d refill=%.1f worth=%.0f in-flight=%.0f flight=%.2fs "
                "closing=%.0f run=%.1fs shield=%.0f/%.0f up=%d falling=%.0f/s window-left=%.1fs",
                g.target, t.dist, freeUse ? "free-use " : "", missileOutmatched ? "outmatched " : "",
                big ? "big-ship " : "", slow ? "slow-kill " : "", burst ? "burst" : "", hp, ourDps, hull, timeToKill,
                std::min(timeToDie, 9999.0f), eco.price, eco.stock, eco.refill, eco.worth, inFlight, flight, t.closing,
                g.missiles.run, shieldNow, shieldMax, shieldUp ? 1 : 0, shieldRate, windowLeft);
            g.missiles.run = 0;
        }
        // (no run from a hold: the salvo fires from the held point)
        // (...nor from a big target's blind spot - gate mpb18, M10: with missiles at a big ship on every refire, a run
        // dashed from 887 m out of the Rheinland battleship's blind spot into 16 of its guns, hull 0.66 -> dead in a
        // second; the salvo goes from the spot, as from a hold)
        else if (want && !deadFirst && !launcherIsTorp && g.tactic == Tactic::Engage && g.combat.evasion < 0.05f &&
                 !holdingHere && !(g.combat.blindOn && g.combat.blindTarget == g.target) && !g.strafe.on &&
                 t.dist > minDist)
        {
            // The run: close in on the afterburner with the nose on the target (pursuit keeps it there), throttle
            // forward (no pursuit band to reverse at); the launch comes when the flight is short and on line.
            if (g.missiles.run == 0)
                events::Push("autopilot missile-run target=%u dist=%.0f flight=%.2fs closing=%.0f on-line=%d", g.target,
                             t.dist, flight, t.closing, onLine ? 1 : 0);
            g.missiles.run += dt;
            g.missiles.runSeen = g.time;
            out.afterburn = true;
            out.pursueBand = 30;
            if (g.missiles.run > 6)
            {
                events::Push("autopilot missile-run-abandoned target=%u dist=%.0f flight=%.2fs", g.target, t.dist,
                             flight);
                g.missiles.run = 0;
                g.missiles.timer = 4;
            }
        }
        else if (g.time - g.missiles.runSeen > 1)
            g.missiles.run = 0; // a run ends after a second without its conditions (one frame's flicker keeps it)
    }
}

} // namespace autopilot
