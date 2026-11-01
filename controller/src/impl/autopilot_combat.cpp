// Advanced Autopilot - combat: the duel, damage and repairs, the fight loop, countermeasures. Part of autopilot.cpp until 21 Oct 2026; shared state and declarations in
// autopilot_impl.h, the module API in autopilot.h.
#include "autopilot_impl.h"
#include "autopilot_ashride.h"
#include "city_hold.h"
#include "game/hudinfo.h"
#include "lookahead.h"

namespace autopilot
{
// ------------------------------------------------------------------ combat
// The player's way out of fire (user + playlist study): afterburner away from `from`, jinking - a new offset of
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

// Ship-adaptive tactics (user, 21 Oct 2026: "worth coming up with a smart and adaptive model"). The duel against one
// fighter is flown one of three ways:
//   0 close turn - the lead pursuit at the close band (40-110 m, reverse thrust through sharp turns): out-turning it;
//   1 wide turn  - the same at ~180 m: a target circling inside a close chase is still followed at less turn rate;
//   2 slash      - passes on the afterburner, no reverse: through it, out to ~700 m, back in: against a better turner
//                  (it cannot hold its nose on a fast crossing ship) or several attackers (no slow turning circle).
// The prior from the agility both ships' archetypes give (turn-rate ratio ours / its: space::ShipMotionOf) and the
// attackers on us; then the fight's own measurement - each option's exchange (the target's points taken per second over
// ours lost per second, both as fractions of the totals) once it has 6 s of data - with switching when another option's
// estimate beats the current one by 25 % (an untried option: the current measurement x the priors' ratio), at most
// every 6 s. What an option measured is remembered per pair of ship types (ours, its): the next duel starts from it.
static const char* kDuelName[3] = {"close-turn", "wide-turn", "slash"};
static float DuelPrior(int opt, float ratio, int attackers)
{
    // From the A/B of 21 Oct 2026 (the starter ship's fight matrix, each option forced, turn ratios 1.0-1.4; exchange =
    // enemy hull taken / our points lost): close turn 3.35, wide turn 2.52 (0.75x), slash 1.38 (0.41x - 2-4x slower,
    // a single pirate not killed in 150 s, a death to 3 close pirates). The NPC pilots punish passes; the close chase
    // wins wherever we turn about as well. Untested: a ship that clearly out-turns us (ratio < 0.8) - there the close
    // chase is expected to lose ground and the slash to gain; the fight's measurement decides.
    const float r = ratio > 0 ? ratio : 1.0f;
    (void)attackers;
    if (opt == 0)
        return Clampf(0.5f + 0.5f * (r - 0.75f) / 0.25f, 0.5f, 1.0f);
    if (opt == 1)
        return 0.75f;
    return Clampf(0.4f + 0.5f * (0.85f - r) / 0.2f, 0.4f, 0.9f);
}
struct DuelMemory
{
    float t[3] = {}, dealt[3] = {}, lost[3] = {};
};
static std::map<unsigned long long, DuelMemory>
    g_duelMemory; // (our ship type << 32 | its type) -> what each option measured
static float DuelValue(float t, float dealt, float lost)
{
    return t > 0 ? (dealt / t + 1e-4f) / (lost / t + 1e-3f) : -1.0f;
}
// Picks the option for this frame. `key` the ship-type pair, `ratio` ours / its turn rate (0 unknown). The measurement
// (DuelAccount) goes into the pair's memory as it happens: this fight's and earlier fights' data alike.
int DuelChoose(unsigned long long key, float ratio, int attackers)
{
    DuelMemory& mem = g_duelMemory[key];
    auto value = [&](int o) { return DuelValue(mem.t[o], mem.dealt[o], mem.lost[o]); };
    auto known = [&](int o) { return mem.t[o] >= 6.0f; };
    if (g.combat.duelOpt < 0 || g.combat.duelKey != key)
    {
        g.combat.duelKey = key;
        int best = 0;
        float bestV = -1;
        for (int o = 0; o < 3; ++o)
        {
            const float v = known(o) ? value(o) * 1000.0f : DuelPrior(o, ratio, attackers); // a measured option first
            if (v > bestV)
                bestV = v, best = o;
        }
        g.combat.duelOpt = best;
        g.combat.duelOptSince = g.time;
        g.combat.slashPhase = SlashPhase::Pass;
        events::Push("autopilot duel-choice opt=%s ratio=%.2f attackers=%d from=%s", kDuelName[best], ratio, attackers,
                     known(best) ? "memory" : "prior");
        return best;
    }
    if (g.time - g.combat.duelOptSince < 6.0f || g.time < g.combat.duelDecideAt || !known(g.combat.duelOpt))
        return g.combat.duelOpt;
    g.combat.duelDecideAt = g.time + 1.0f;
    const int cur = g.combat.duelOpt;
    const float curV = value(cur), curP = DuelPrior(cur, ratio, attackers);
    int best = cur;
    float bestV = curV;
    for (int o = 0; o < 3; ++o)
    {
        if (o == cur)
            continue;
        const float v = known(o) ? value(o) : curV * DuelPrior(o, ratio, attackers) / std::max(0.05f, curP);
        if (v > bestV * 1.25f)
            best = o, bestV = v;
    }
    if (best != cur)
    {
        events::Push("autopilot duel-switch %s -> %s value=%.2f -> %.2f%s ratio=%.2f attackers=%d", kDuelName[cur],
                     kDuelName[best], curV, bestV, known(best) ? "" : " (estimated)", ratio, attackers);
        g.combat.duelOpt = best;
        g.combat.duelOptSince = g.time;
        g.combat.slashPhase = SlashPhase::Pass;
    }
    return g.combat.duelOpt;
}
// This frame's exchange, booked to the option flown: the target's points taken (fraction of its total) and ours lost
// (fraction of ours), into this fight's tally (the log) and the pair's memory (the choice).
static void DuelAccount(float dt, float dealtFrac, float lostFrac)
{
    if (g.combat.duelOpt < 0 || g.combat.duelOpt > 2)
        return;
    DuelMemory& mem = g_duelMemory[g.combat.duelKey];
    g.combat.optT[g.combat.duelOpt] += dt, g.combat.optDealt[g.combat.duelOpt] += dealtFrac,
        g.combat.optLost[g.combat.duelOpt] += lostFrac;
    mem.t[g.combat.duelOpt] += dt, mem.dealt[g.combat.duelOpt] += dealtFrac, mem.lost[g.combat.duelOpt] += lostFrac;
}
// The fight's end: what each option measured, to the log.
void DuelLog()
{
    if (g.combat.duelOpt < 0)
        return;
    for (int o = 0; o < 3; ++o)
        if (g.combat.optT[o] > 0)
            events::Push("autopilot duel-option %s t=%.1f dealt=%.2f lost=%.3f value=%.2f", kDuelName[o],
                         g.combat.optT[o], g.combat.optDealt[o], g.combat.optLost[o],
                         DuelValue(g.combat.optT[o], g.combat.optDealt[o], g.combat.optLost[o]));
}

// Damage taken and repairs (nanobots / shield batteries), every frame in EVERY mode (user, 20 Oct 2026: hull is
// sacred - M04 objective mode died in `hold` beside capital ships, shield 0.92 -> 0.20 then hull 1.00 -> 0.03 in 6 s,
// a nanobot and a battery aboard: the repairs ran only inside a fight). Outside one the situation (attackers) is read
// twice a second here; a fight reads it itself.
void DamageAndRepairs(float dt, Out& out, ai::Self& self)
{
    // Damage taken, every frame (was: only while an enemy was selected - between targets, looting and fleeing the
    // pressure stayed at its last value, so repairs saw hits that had stopped and `lost` missed damage, 18 Oct 2026):
    // the pressure is the shield + hull points lost over the last 2 s against our total. No hits, no evasion. Light
    // hits: the pursuit weaves (corkscrew around the lead cross, the reticle and the guns stay on it); more: slide
    // bursts on top; heavy: break away (or the game's Evade, dodge 2). Rises at once, fades over ~2 s once the hits stop.
    const float totalHp = std::max(1.0f, self.hullMax + self.shieldMax);
    g.combat.totalHp = totalHp;
    const float hpNow =
        (self.hull >= 0 ? self.hull * self.hullMax : 0) + (self.shield > 0 ? self.shield * self.shieldMax : 0);
    g.combat.hp.push_back({g.time, hpNow});
    while (!g.combat.hp.empty() && g.combat.hp.front().first < g.time - 2.0f)
        g.combat.hp.pop_front();
    float hpTop = hpNow;
    for (const auto& h : g.combat.hp)
        hpTop = std::max(hpTop, h.second);
    // evade_scale (default 6 %) of everything lost in 2 s = full
    const float pressure = Clampf((hpTop - hpNow) / totalHp / std::max(0.005f, g_set.evadeScale), 0, 1);
    if (g.combat.hp.size() >= 2 && g.combat.hp[g.combat.hp.size() - 2].second > hpNow)
        g.combat.lost += (g.combat.hp[g.combat.hp.size() - 2].second - hpNow) / totalHp;
    g.combat.evasion = g_set.evade ? std::max(pressure, g.combat.evasion - dt * 0.5f) : 0.0f;
    g.stats.pressure = pressure;
    // seconds without a hit (pressure covers the last 2 s): updated here, before the flee branch reads it (it was updated
    // after the branch's early return - frozen at 0 for a whole flee, so the cruise run never started, 20 Oct 2026)
    g.combat.quiet = pressure > 0 ? 0.0f : g.combat.quiet + dt;
    g.stats.shield = self.shield;
    g.stats.lossRate = (hpTop - hpNow) / 2.0f; // points per second over the last 2 s
    // The largest single hit (frame-to-frame drop) of the last 2 s: slow, heavy weapons empty a shield in one shot.
    g.stats.maxHit = 0;
    for (size_t i = 1; i < g.combat.hp.size(); ++i)
        g.stats.maxHit = std::max(g.stats.maxHit, g.combat.hp[i - 1].second - g.combat.hp[i].second);
    // Evidence for sudden deaths (M04 West Point, 20 Oct 2026: shield 359 to dead in 0.45 s beside capital ships, nothing
    // logged to say what hit): a frame-to-frame loss above 20 % of our total names the nearest hostiles - toughness
    // (hull points: fighters ~1-3k, capital ships far more), distance, whether they target us.
    // The explosions that reach us (the M13 burst probe, 29 Oct 2026: our torpedo cost us its full 2,424 with its last
    // live position 80-95 m off - FREELANCER_RE "Explosion damage"): the HUD's damage indicator gets an explosion's
    // centre (kind 1, host v12 PlayerHit); a round's report lies ~1000 m back along its flight, so a kind-1 point within
    // 300 m is a blast centre - logged with its distance to us and to the target's centre.
    static u32 hitSeq = 0;
    static float blastAt = -1.0f, blastToUs = -1.0f;
    {
        float hp3[3];
        u32 kind = 0;
        const u32 seq = HostPlayerHit(hp3, &kind, nullptr);
        if (seq != hitSeq)
        {
            hitSeq = seq;
            fl::Kinematics me = {};
            if (kind == 1 && fl::PlayerKinematics(me))
            {
                const float toUs = geom::Dist(hp3, me.pos);
                if (toUs < 300.0f)
                {
                    float toTarget = -1.0f;
                    for (const space::Contact& c : Contacts())
                        if (g.target && c.id == g.target)
                            toTarget = geom::Dist(hp3, c.pos);
                    blastAt = g.time, blastToUs = toUs;
                    events::Push("autopilot blast-near to-us=%.0f to-target=%.0f target=%u at=(%.0f,%.0f,%.0f) "
                                 "us=(%.0f,%.0f,%.0f)",
                                 toUs, toTarget, g.target, hp3[0], hp3[1], hp3[2], me.pos[0], me.pos[1], me.pos[2]);
                }
            }
        }
    }
    static Throttle lastBigHit;
    const float frameLoss = g.combat.hp.size() >= 2 ? g.combat.hp[g.combat.hp.size() - 2].second - hpNow : 0.0f;
    if (frameLoss > 0.2f * totalHp && lastBigHit.Due(g.time, 1.0f))
    {
        std::vector<space::Contact> nearby;
        for (const space::Contact& c : Contacts())
            if (c.attitude < -0.1f && c.distance < 5000)
                nearby.push_back(c);
        std::sort(nearby.begin(), nearby.end(),
                  [](const space::Contact& a, const space::Contact& b) { return a.distance < b.distance; });
        std::string who;
        for (size_t i = 0; i < nearby.size() && i < 4; ++i)
        {
            char one[96];
            sprintf_s(one, " [id=%u hp=%.0f dist=%.0f%s]", nearby[i].id, nearby[i].maxHit, nearby[i].distance,
                      nearby[i].targetsPlayer ? " on-us" : "");
            who += one;
        }
        // (and our own last round to end within 0.4 s - g.missiles.ownGone keeps them that long: whose burst it was, WORK_QUEUE
        // Q4 "the shooter logged at big hits")
        const State::OwnGone* own = nullptr;
        for (const State::OwnGone& og : g.missiles.ownGone)
            if (g.time - og.gone <= 0.4f && (!own || og.gone > own->gone))
                own = &og;
        char ownText[96] = " own-end=none";
        if (own)
            sprintf_s(ownText, " own-end=%u munition=%s to-us=%.0f %.2fs ago", own->id, own->m.torp ? "torp" : "missile",
                      own->m.lastToUs, g.time - own->gone);
        events::Push("autopilot big-hit lost=%.0f of %.0f shield=%.2f hull=%.2f last-blast=%.0fm %.2fs ago%s nearest:%s",
                     frameLoss, totalHp, self.shield, self.hull, blastToUs, blastAt >= 0 ? g.time - blastAt : -1.0f,
                     ownText, who.c_str());
    }

    g.combat.hullRepairTimer -= dt;
    g.combat.shieldRepairTimer -= dt;
    if (g_set.repair && self.hull >= 0)
    {
        const float hullAt =
            g_p.Repair.use_hull_repair_at_damage_percent > 0 ? g_p.Repair.use_hull_repair_at_damage_percent : 0.5f;
        const float shieldAt =
            g_p.Repair.use_shield_repair_at_damage_percent > 0 ? g_p.Repair.use_shield_repair_at_damage_percent : 0.7f;
        // the effect of the last use, measured (misc_equip.ini hit_pts 600 / 300 is the assumed amount). A press that used
        // nothing is retried at the next post_delay; only a second miss in a row means none usable now (10 s). (M04 21 Oct
        // 2026: one use_item press of 1 battery at shield 0.34 used nothing - the server's answer, cause unknown - and the
        // 10 s lockout that followed at once let the shield fail and the hull fall 1.00 -> 0.66 before the next press.)
        static float hullAtUse = 0, shieldAtUse = 0;
        if (g.combat.nanobotsBefore >= 0 && g.combat.hullRepairTimer <= 0)
        {
            if (self.nanobots >= g.combat.nanobotsBefore)
            {
                ++g.combat.nanobotsMissed;
                events::Push("autopilot repair-miss hull held=%d misses=%d", self.nanobots, g.combat.nanobotsMissed);
                if (g.combat.nanobotsMissed >= 2)
                    g.combat.hullRepairTimer = 10.0f, g.combat.nanobotsMissed = 0;
            }
            else
            {
                g.combat.nanobotsMissed = 0;
                events::Push("autopilot repair-effect hull %+.0f hp (used at %.0f)",
                             (self.hull - hullAtUse) * self.hullMax, hullAtUse * self.hullMax);
            }
            g.combat.nanobotsBefore = -1;
        }
        if (g.combat.batteriesBefore >= 0 && g.combat.shieldRepairTimer <= 0)
        {
            if (self.batteries >= g.combat.batteriesBefore)
            {
                ++g.combat.batteriesMissed;
                events::Push("autopilot repair-miss shield held=%d misses=%d shield=%.2f", self.batteries,
                             g.combat.batteriesMissed, self.shield);
                if (g.combat.batteriesMissed >= 2)
                    g.combat.shieldRepairTimer = 10.0f, g.combat.batteriesMissed = 0;
            }
            else
            {
                g.combat.batteriesMissed = 0;
                events::Push("autopilot repair-effect shield %+.0f hp (used at %.0f)",
                             (std::max(0.0f, self.shield) - shieldAtUse) * self.shieldMax,
                             shieldAtUse * self.shieldMax);
            }
            g.combat.batteriesBefore = -1;
        }
        // repair 1 (the player's way, user 17 Oct 2026): consumables are not spent while the fight is managed. A
        // battery only at the last moment: while attackers keep hitting us and the shield left is less than the damage
        // predicted before a battery takes effect - the loss rate of the last 2 s over battery_horizon (fast, light
        // guns) or the largest single hit of the last 2 s (slow, heavy weapons: one more empties it) - so never when
        // the shield can recharge before hull damage starts (the never-read caps repair_shield_at / repair_hull_at were
        // deleted 31 Oct 2026, AUD-A1-5). Nanobots only when the hull is low and being hit, or
        // critical. g.stats.pressure / lossRate / maxHit: this frame. The attackers' weapons (per-shot damage, refire)
        // feed the survival model below (space::ShipWeapons).
        const bool hit = g.stats.pressure > 0;
        // A failed shield reboots: its points build up again while it is offline and absorbs nothing (M03 19 Oct 2026:
        // "shield=120/821 up=0" counted as protection - no battery in the whole ambush, hull 0.37, death at the
        // Valkyries). The game's own up flag decides (space::ShieldOf).
        float shCur = 0, shMax = 0;
        bool shUp = true;
        if (space::ShieldOf(space::PlayerShipId(), shCur, shMax, shUp) && !shUp)
            self.shield = 0;
        const bool shieldFailed = self.shield >= 0 && (!shUp || self.shield < 0.10f);
        const float shieldLeft = self.shield > 0 ? self.shield * self.shieldMax : 0.0f;
        const float predicted = std::max(g.stats.lossRate * g_set.batteryHorizon, g.stats.maxHit);
        // Nanobots (user, 18 Oct 2026: "shields collapsed and hull sustained minor damage, but chances are very low for
        // additional heavy damage? hold off"): when the hull is low only if the damage predicted for the next moment
        // gets past the shield into the hull (>= 5 % of it); critical (< 35 %) only while still under fire.
        const float predictedHullLoss = std::max(0.0f, predicted - shieldLeft);
        // Survival model (user, 19 Oct 2026: "even in frugality mode the ship must survive" - decide from the odds):
        // the damage expected over the next kHorizon s = the larger of the measured loss rate and the attackers' gun
        // potential (their loadouts: hull damage / refire of every gun of each hostile on us within 2.5 km) times the
        // hit rate learned in this fight, plus every homing munition aimed at us due within the horizon, plus one more
        // largest hit (the spread). The shield counts only while it is up. Frugal: a consumable only when it changes
        // the outcome - the hull would end below 20 % (nanobot) or hits would get through to the hull (battery).
        static const float kHorizon = 2.5f;
        static ULONGLONG potAt = 0;
        static float potential = 0, hitRate = 0.2f; // learned per fight from what lands (M03: 0.15 vs 3 Valkyries)
        const ULONGLONG nowMs = gameclock::NowMs64();
        if (nowMs - potAt >= 500)
        {
            potAt = nowMs;
            potential = 0;
            for (const space::Contact& c : Contacts())
                if (c.ship && c.targetsPlayer && c.attitude <= -0.6f && c.distance < 1200) // within gun range of us
                    for (const space::Weapon& w : space::ShipWeapons(c.id))
                        if (!w.homing && w.active && w.refire > 0)
                            potential += w.hullDamage / w.refire;
            // a new fight (30 s without a gun on us) starts from the default again: a past fight's rate (other ships,
            // another run or save) must not carry over
            static ULONGLONG fightAt = 0;
            if (potential > 0)
            {
                if (nowMs - fightAt > 30000)
                    hitRate = 0.2f;
                fightAt = nowMs;
            }
            // learn how much of the potential lands (aim, range, our evasion); floor 0.01 (was 0.05: 4 heavy Rheinland
            // fighters, potential 17485/s, really landed ~1 % - the floor predicted 874/s and death every moment)
            if (potential > 0 && g.stats.lossRate > 0)
                hitRate += (std::clamp(g.stats.lossRate / potential, 0.01f, 1.0f) - hitRate) * 0.2f;
            // (the potential counts ships only: static_volley, which added one volley of each hostile static's guns as
            // the largest hit to expect, lost its SPRT and was deleted 31 Oct 2026, AUD-A1-5)
        }
        float missileDmg = 0;
        for (const space::Missile& m : space::Missiles())
            if (m.targetsPlayer && m.closing > 1 && m.distance / m.closing < kHorizon)
                missileDmg += m.hullDamage;
        const float expected = std::max(g.stats.lossRate, potential * hitRate) * kHorizon + missileDmg;
        // (the largest hit to expect: the last 2 s's)
        const float bigHit = g.stats.maxHit;
        const bool underGuns = g_sit.attackers > 0;
        const float worst = expected + bigHit;
        const float hullAfter = self.hull * self.hullMax - std::max(0.0f, worst - shieldLeft);
        const bool deathRisk = self.hullMax > 0 && hullAfter < 0.20f * self.hullMax && (underGuns || missileDmg > 0);
        // Death within the reaction time (a press acts at once): only this justifies wasting part of a unit. A risk
        // further out is better met by waiting until the gap takes a whole unit (19 Oct 2026: 3 nanobots spent at hull
        // 0.71 for +317 each, none left for the burst that killed the ship 30 s later).
        static const float kReact = 0.75f;
        float missileSoon = 0;
        for (const space::Missile& m : space::Missiles())
            if (m.targetsPlayer && m.closing > 1 && m.distance / m.closing < kReact)
                missileSoon += m.hullDamage;
        const float worstSoon = std::max(g.stats.lossRate, potential * hitRate) * kReact + missileSoon + bigHit;
        // The gun rounds already in flight that will hit (user, 19 Oct 2026: "track enemy projectiles, model the damage
        // deterministically"; space::IncomingRounds: paths vs the hull's box, the game's shield damage formula; predicted /
        // measured 1.03): simulated in arrival order - the shield takes each round's shield damage until it fails at its
        // offline_threshold, the rest hits the hull. Rounds fly 0.3-0.8 s: within the reaction time this is nearly all
        // that will land; over the longer horizon the rate model above stays the floor.
        const float offlineAt = std::max(0.0f, space::ShieldOfflineThreshold(self.shieldArch)) * self.shieldMax;
        float roundsHullSoon = 0, roundsHull = 0, roundsShieldSoon = 0;
        int roundsCount = 0;
        {
            float sh = shieldLeft;
            for (const space::Round& r : space::IncomingRounds(kHorizon, 0, self.shieldArch))
            {
                float hullHit = 0;
                if (sh > offlineAt && shUp)
                    sh -= r.shieldDamage;
                else
                    hullHit = r.hullDamage;
                roundsHull += hullHit;
                if (r.tHit <= kReact)
                {
                    roundsHullSoon += hullHit;
                    roundsShieldSoon += r.shieldDamage;
                }
                ++roundsCount;
            }
        }
        const bool deathSoon =
            self.hullMax > 0 && (underGuns || missileSoon > 0 || roundsCount > 0) &&
            (self.hull * self.hullMax - std::max(std::max(0.0f, worstSoon - shieldLeft), roundsHullSoon + missileSoon) <
             0.20f * self.hullMax);
        const float modelHullLoss = (underGuns || missileDmg > 0 || roundsCount > 0)
                                        ? std::max(std::max(0.0f, worst - shieldLeft), roundsHull + missileDmg)
                                        : 0.0f;
        // The frugal default (repair 1), the user's rules (19 Oct 2026), the damage model judging "imminent":
        // - Nanobots "when player hull damage is about to substantially increase, or when hull is severely damaged as-is":
        //   the model's hull loss over the horizon >= 15 % of the hull (or the hull would end below 20 %), or the hull
        //   below 40 % now (M03: a fight entered at 37 % was lost - nothing repaired it between fights).
        // - Shield batteries "only when the shields are depleted or near depletion and the incoming damage model
        //   determines significant damage is imminent": failed or <= 15 %, and >= 8 % of the hull would get through.
        const bool fightStart = false; // (the repair event's field; the rule was replaced by "severely damaged as-is")
        (void)predictedHullLoss;
        const bool needHull = g_set.repair == 2
                                  ? 1.0f - self.hull >= hullAt
                                  : deathRisk || modelHullLoss >= 0.15f * self.hullMax || self.hull < 0.40f;
        // "near depletion" = the fitted shield's own offline_threshold (user, 19 Oct 2026: the game's value - 0.15 for most,
        // st_equip.ini), 0.15 when unknown
        const float offline = space::ShieldOfflineThreshold(self.shieldArch);
        const bool nearDepletion = shieldFailed || (self.shield >= 0 && self.shield <= (offline > 0 ? offline : 0.15f));
        // Hull is sacred (user, 20 Oct 2026: "the player HATES to see hull damage; regenerative shields are the
        // expendable health pool"): a battery BEFORE the shield fails - when the damage expected over the horizon would
        // take what is left above the offline threshold (a failed shield reboots and absorbs nothing) - and at a small
        // predicted hull loss (2 % of the hull; was 8 %).
        // ... within the REACTION time, not the 2.5 s horizon (20 Oct 2026, M04 vs the 2020 run: the user spent 8
        // batteries and let the shield fall to 0.11 with no hull damage; the runs spent 16-18 - pressed at shield 0.87
        // from the attackers' potential x the 0.20 prior hit rate (measured 0.01-0.03), and at 0.6-0.7 for +111..+280 of
        // a 300-point unit). A press acts at once: the last useful moment (worstSoon: the reaction window's rate, the
        // largest recent hit, the munitions due in it) keeps the hull untouched and fills a larger gap per unit.
        const bool aboutToFail = !shieldFailed && shUp && self.shield >= 0 &&
                                 (underGuns || roundsCount > 0 || missileSoon > 0) &&
                                 shieldLeft - offlineAt <= worstSoon + roundsShieldSoon;
        const bool needShield = g_set.repair == 2
                                    ? self.shield >= 0 && 1.0f - self.shield >= shieldAt
                                    : (nearDepletion && modelHullLoss >= 0.02f * self.hullMax) || aboutToFail;
        (void)hit;
        // Never waste one: a nanobot restores 600 hull, a battery 300 shield (misc_equip.ini hit_pts; a battery measured
        // +340 in game with regeneration, 19 Oct 2026) - used only when at least half of it fits.
        // One press fills the bar with as many units as the gap needs (user, 19 Oct 2026; in game: a ~895 shield gap took
        // 3 batteries) - the last unit rounds up. A frugal press wastes at most 30 % of a unit, unless survival needs it
        // now (death predicted, or the hull already below 40 %).
        auto efficient = [](float gap, float unit)
        {
            if (gap < unit * 0.7f)
                return false;
            const float units = std::ceil(gap / unit);
            return units * unit - gap <= unit * 0.3f;
        };
        const float hullGap = (1.0f - self.hull) * self.hullMax,
                    shieldGap = self.shield >= 0 ? (1.0f - self.shield) * self.shieldMax : 0;
        // (with our own count - use_item - a press needs room for one unit only: at most 30 % of it spilled; the
        // packing test `efficient` stays for the button fallback, which fills the whole gap)
        const bool ownCount = exe::UseItem.ok();
        const bool hullRoom =
            hullGap >= 300.0f &&
            ((ownCount ? hullGap >= 0.7f * 600.0f : efficient(hullGap, 600.0f)) || deathSoon || self.hull < 0.40f);
        // A FAILED shield is no top-up: every hit now lands on the hull, so the last unit's rounding does not matter (user,
        // 20 Oct 2026, M04 heavy combat: "waited a bit too long to use shield batteries when shields failed" - a failed
        // shield's gap of ~1225 never packs into 300-point units within 30 %, so the press waited for death-soon at hull 0.29).
        const bool shieldRoom = self.shield >= 0 && shieldGap >= 150.0f &&
                                (shieldFailed || aboutToFail ||
                                 (ownCount ? shieldGap >= 0.7f * 300.0f : efficient(shieldGap, 300.0f)) || deathSoon);
        // How many units (user, 21 Oct 2026: "one battery per press, pressing again only while still needed" ... "scale it
        // based on important factors such as incoming fire, prediction of hull damage"): the game's use_item with our
        // count (ai::UseConsumable) instead of the HUD button's fill-the-bar count. Batteries: the shield the window's
        // damage would take (worst over the horizon: rate x potential, missiles due, one more largest hit, the rounds in
        // flight) plus the offline threshold, less what is left; nanobots: what keeps the hull above 40 % after it. At
        // least 1; at most what the gap holds with under 30 % of a unit spilled (a failed shield / death soon: the
        // whole gap rounded up), and what is aboard. The timers re-check every post_delay: pressed again only if still
        // needed. Falls back to the button when use_item does not verify.
        auto units = [](float need, float gap, float unit, bool urgent, int held)
        {
            const int fit =
                std::max(1, static_cast<int>(urgent ? std::ceil(gap / unit) : std::floor((gap + 0.3f * unit) / unit)));
            const int want = std::max(1, static_cast<int>(std::ceil(need / unit)));
            return std::max(0, std::min({want, fit, held}));
        };
        // A reserve for the rest of the leg (session 8, M12: two deaths in Omicron Minor with nanobots 0 and batteries 0 -
        // all spent in the lair, the missiles there met nothing; the user's try 3 arrived with batteries 20 of 29 and the
        // nanobots untouched): ~15 % of the hold (at least 2) is spent only when death is predicted, the hull is below
        // 35 % (nanobots) or the shield has failed (batteries)
        int nanoFree = deathSoon || self.hull < 0.35f
                                 ? self.nanobots
                                 : self.nanobots - std::max(2, static_cast<int>(std::ceil(0.15f * self.nanobotMax)));
        int battFree = deathSoon || shieldFailed
                           ? self.batteries
                           : self.batteries - std::max(2, static_cast<int>(std::ceil(0.15f * self.batteryMax)));
        // `battery_reserve` (the speedrunners arrive at M13's Nomad battleship with 27-29 batteries and spend them there,
        // in bursts on each shield failure - fights.tsv "Order base"; ours arrived with 3-29, run-judge 29 Oct 2026): while
        // a big static target lies ahead - marked by the objective, or hostile within 10 km (the escort phase before
        // M13's battleship spent 4-12 batteries while the mark was not yet up) - and its hold is not reached, 70 % of the
        // batteries stay aboard unless death is predicted or the hull is under 60 % (the shield may cycle on the way, as
        // theirs did). (The check's timer lives in g: a static one outlived g = State() at Start, g.time back to 0, and
        // came ~50 s late - run-judge 29 Oct 2026.)
        {
            if (!g_set.batteryReserve)
                g.combat.bigAhead = false;
            else if (g.time >= g.combat.bigNext)
            {
                g.combat.bigNext = g.time + 1.0f;
                const bool was = g.combat.bigAhead;
                g.combat.bigAhead = false;
                const std::vector<space::Contact> all = Contacts();
                u32 marks[32];
                const int n = MissionObjectIds(marks, 32, all);
                for (const space::Contact& c : all)
                {
                    if (c.ship || c.maxHit <= 20000 || c.health == 0)
                        continue;
                    // (a static's attitude is the game's, not the contact list's: space::AttitudeToPlayer, as the
                    // objective's fight marks read it - M13's battleship -0.60)
                    float att = 0;
                    const bool hostileNear = c.distance < 10000 && space::AttitudeToPlayer(c.id, att) && att < -0.3f;
                    if ((std::find(marks, marks + n, c.id) != marks + n || hostileNear) &&
                        !(g.combat.blindOn && g.combat.blindTarget == c.id && g.combat.blindDist < 150))
                        g.combat.bigAhead = true;
                }
                if (was != g.combat.bigAhead)
                    events::Push("autopilot battery-reserve %s batteries=%d", g.combat.bigAhead ? "on" : "off",
                                 self.batteries);
            }
            if (g.combat.bigAhead && !deathSoon && self.hull >= 0.6f)
                battFree = std::min(battFree, self.batteries - static_cast<int>(std::ceil(0.7f * self.batteryMax)));
        }
        // `battery_budget` (M13-MIMIC item 3; the study's two windows, docs\research\runners\M13.md "## Plan"): the
        // consumables split over the big fights the script still holds before the next base dock (mission::FightsAhead)
        // - the runners spend all of Toledo -> Osiris on the Nomad battleship and keep ~10 batteries + ~15 nanobots of
        // Osiris -> end for the city; td1 spent 3 / 15 / 9 off-target and died in the city with 0 / 7. The counts it began
        // with are kept per window (the groups ahead: the barrier's two steps share one), not per autopilot start
        // (reworked 31 Oct 2026, mm6 / mm9 [M]: the gate took 9 then held 20 with the shield at 0 to hull 0.40; mm9 without
        // it spent 29 -> 5 there in ~12 s at full hull in top-ups from shield 0.70 and reached the city with 0. Now: the
        // share is weighted by the kills each fight needs - window B 1 : 4 : 2, ~5 / ~16 / the rest, R1's 7 / 18 / 4 - the
        // refills come as the runners' bursts on a failing shield while fights lie ahead (no top-ups), and a failing
        // shield under fire releases the share: a failing shield is never left without a battery)
        bool budgetRhythm = false;
        if (g_set.batteryBudget && Knows(Source::Hidden))
        {
            struct Budget
            {
                u32 text = 0;
                std::vector<mission::FightGroup> groups;
                std::set<u32> seen; // the groups' objects seen alive (a group seen and gone is no fight ahead)
                std::string key, said;
                int b0 = -1, n0 = -1;
                float next = -1;
                ULONGLONG at = 0;
                bool big = false;
                int k = 0, battKeep = 0, nanoKeep = 0, wCur = 0, wAhead = 0;
            };
            static Budget bu;
            if (nowMs - bu.at >= 1000 || nowMs < bu.at)
            {
                bu.at = nowMs;
                ObjectiveStep st = {};
                const u32 text = HostObjectiveNow(&st) ? st.textId : 0;
                if (text && text != 21660 && text != 1 && text != bu.text)
                {
                    bu.text = text;
                    const mission::FightsAheadView v =
                        mission::FightsAhead(text, fl::PlayerSystem(), [](u32 o) { return space::IsDockRing(o); });
                    bu.groups = v.ok ? v.groups : std::vector<mission::FightGroup>();
                    std::string key;
                    for (const mission::FightGroup& fg : bu.groups)
                        key += fg.label + " ";
                    // (a restock - more aboard than the window began with - starts a window too)
                    if (key != bu.key)
                        bu.key = key, bu.b0 = -1;
                }
                if (bu.b0 < 0 || self.batteries > bu.b0 || self.nanobots > bu.n0)
                    bu.b0 = self.batteries, bu.n0 = self.nanobots;
                // the big fight now: our target, or a marked hostile static within 3 km, a big one (> 20,000 points)
                std::set<u32> bigIds;
                const std::vector<space::Contact> all = Contacts();
                u32 marks[32];
                const int n = MissionObjectIds(marks, 32, all);
                for (const space::Contact& c : all)
                {
                    float att = 0;
                    if (c.ship || c.maxHit <= 20000 || c.health == 0 || !space::AttitudeToPlayer(c.id, att) ||
                        att >= -0.3f)
                        continue;
                    // (ally_kill: the solar the allies kill while we wait at the dock is no fight of ours)
                    if (g_set.allyKill && g_obj.allyKillTarget && c.id == g_obj.allyKillTarget)
                        continue;
                    // (our target only within 2 km - under its turrets: ab2-b_i2 / i4 / i5 spent all 29 on interceptors
                    // 2-5 km out with the battleship as the target since its banner, and reached the spot with none; the
                    // runners spend all of theirs at the hull - run-judge 31 Oct 2026)
                    if ((c.id == g.target && c.distance < 2000) ||
                        (c.distance < 2000 && std::find(marks, marks + n, c.id) != marks + n))
                        bigIds.insert(c.id);
                }
                bu.big = !bigIds.empty();
                bu.k = 0;
                bu.wCur = 0;
                bu.wAhead = 0;
                // (a group whose objects were all seen and are gone is fought: mm4-b_i1 kept 15 of 22 nanobots for the
                // dead Nomad battleship's group after its kill)
                for (const space::Contact& c : all)
                    if (c.health != 0)
                        for (const mission::FightGroup& fg : bu.groups)
                            if (std::find(fg.objects.begin(), fg.objects.end(), c.id) != fg.objects.end())
                                bu.seen.insert(c.id);
                for (const mission::FightGroup& fg : bu.groups)
                {
                    bool mine = false, gone = !fg.objects.empty();
                    for (u32 id : fg.objects)
                    {
                        mine = mine || bigIds.count(id);
                        const auto ci =
                            std::find_if(all.begin(), all.end(), [&](const space::Contact& c) { return c.id == id; });
                        gone = gone && bu.seen.count(id) && (ci == all.end() || ci->health == 0);
                    }
                    const int w = std::max(1, fg.kills);
                    // (ally_kill: its group is not ahead either - the allies' kill)
                    if (g_set.allyKill && g_obj.allyKillTarget &&
                        std::find(fg.objects.begin(), fg.objects.end(), g_obj.allyKillTarget) != fg.objects.end())
                        continue;
                    if (mine)
                        bu.wCur = std::max(bu.wCur, w);
                    else if (!gone)
                        bu.k += 1, bu.wAhead += w;
                }
                // a big fight: its share of the window by the kills - w / (w + the kills ahead); a big target no group
                // names counts as one kill. Else every battery waits for one
                const int wc = std::max(1, bu.wCur);
                const float share = static_cast<float>(wc) / (wc + bu.wAhead);
                // (for `missile_keep_big`: the big fight now counts as ahead too - its kills still to make)
                g.combat.fightsAheadW = bu.wAhead + (bu.big ? wc : 0);
                g.combat.fightsAheadAt = g.time;
                // (no big fight ahead at all: nothing to keep for - the gate's M07 held all 20 through a fighter fight,
                // gate m13afk1, 31 Oct 2026)
                bu.battKeep = bu.big ? bu.b0 - static_cast<int>(std::ceil(bu.b0 * share)) : bu.wAhead > 0 ? bu.b0 : 0;
                bu.nanoKeep = bu.big ? bu.n0 - static_cast<int>(std::ceil(bu.n0 * share))
                                     : static_cast<int>(std::ceil(bu.n0 * static_cast<float>(bu.wAhead) /
                                                                  (bu.wAhead + 1)));
                char said[160];
                sprintf_s(said, "%u %s big=%d k=%d w=%d/%d keep=%d/%d", bu.text, bu.key.c_str(), bu.big ? 1 : 0, bu.k,
                          wc, bu.wAhead, bu.battKeep, bu.nanoKeep);
                if (bu.said != said)
                {
                    bu.said = said;
                    events::Push("autopilot battery-budget text=%u groups=%s big=%d k=%d kills=%d ahead=%d "
                                 "window-start=%d/%d keep batteries=%d nanobots=%d now=%d/%d",
                                 bu.text, bu.key.empty() ? "-" : bu.key.c_str(), bu.big ? 1 : 0, bu.k, wc, bu.wAhead,
                                 bu.b0, bu.n0, bu.battKeep, bu.nanoKeep, self.batteries, self.nanobots);
                }
            }
            // (death near frees all, as before; on fighters the hull under half frees the batteries, under half the
            // nanobots too - the 35 % / 40 % rules below stay; at a big fight a failing shield under fire frees the batteries,
            // whatever the hull: the first build held 20 with the shield at 0 down to hull 0.40, and a floor at hull 0.6
            // killed ab2-b_i1 at the Nomad gate - share spent, 24 held on a failed shield at hull 0.89, dead 2 s later.
            // The share then binds the top-ups (budgetRhythm); the spend itself is set by the exposure)
            const bool underFire = underGuns || roundsCount > 0 || missileSoon > 0 || g_sit.attackers > 0;
            const bool failingHull = bu.big && nearDepletion && underFire;
            const bool failingSmall = !bu.big && nearDepletion && underFire && bu.wAhead > 0 && self.hull >= 0.5f;
            if (bu.b0 >= 0 && !deathSoon && !failingHull && !failingSmall && (bu.big || self.hull >= 0.5f))
                battFree = std::min(battFree, self.batteries - bu.battKeep);
            // ...past the share a failing shield gets 3 units, back online, not a full bar (ab4-b_i3 at the Nomad gate:
            // bursts of 10 / 10 / 9 in 6 s at hull 1.00 emptied window B before the barrier; R1 spends 7 there, its hull
            // dipping to 0.78) - death near still frees all
            // ...down to a floor of 4 a kill still ahead, at most 8 (R1 entered the city with 4, R2 with 11): 8 of 10 ab city
            // entries had 0, two of nine legs died there in one burst at full hull (ab5-b_i5, ab7-b_i5; run-judge). (The
            // floor without the cap was 4 x 6 = 24 at the Nomad gate: ab8-b_i2 held all 24 on a shield at 0.06-0.11 and
            // died - run-judge)
            else if (bu.b0 >= 0 && !deathSoon && failingHull)
                battFree = std::min(battFree, std::max(std::min(3, self.batteries - std::min(8, 4 * bu.wAhead)),
                                                       self.batteries - bu.battKeep));
            // ...and at a small-ship fight with big ones ahead (the keep is the whole window there) a failing shield under
            // fire gets the same 3, at most 6 of the window's batteries for the small fights: ab13-b_i6 held all 29 on a
            // failing shield among Toledo's interceptors at hull >= 0.5 and died in one frame (53 s; run-judge 31 Oct 2026)
            else if (bu.b0 >= 0 && !deathSoon && failingSmall)
                battFree = std::min(battFree, std::min(3, self.batteries - (bu.b0 - 6)));
            if (bu.n0 >= 0 && !deathSoon && self.hull >= 0.5f)
                nanoFree = std::min(nanoFree, self.nanobots - bu.nanoKeep);
            // the runners' rhythm while fights lie ahead (R1 9 / 10 / 8, R2 10 / 9 at once, each on a failing shield):
            // no top-up above the shield's offline threshold - a top-up the fight's end makes useless is a battery the
            // next fight lacks
            // (...and at the last big fight of a window too: ab9-b_i2 topped up 29 -> 19 by 300 m and to 0 at 150 m out
            // of the Nomad battleship, then held 9 s with no shield until one turret volley - the runners burst 9 / 10 / 8
            // at the hull on each failure: run-judge 31 Oct 2026)
            budgetRhythm = bu.b0 >= 0 && bu.big && !nearDepletion && !deathSoon;
        }
        if (nanoFree > 0 && needHull && hullRoom && g.combat.hullRepairTimer <= 0)
        {
            const float hullNeed = std::max({0.40f * self.hullMax - hullAfter, modelHullLoss, 0.0f});
            const int n = units(hullNeed, hullGap, 600.0f, deathSoon || self.hull < 0.40f, nanoFree);
            const int used = ai::UseConsumable(false, n);
            if (!used)
                out.repairHull = true;
            events::Push("autopilot repair-units hull n=%d need=%.0f gap=%.0f %s", used ? used : -1, hullNeed, hullGap,
                         used ? "use_item" : "button");
            g.combat.nanobotsBefore = self.nanobots;
            hullAtUse = self.hull;
            g.combat.hullRepairTimer = Pf(g_p.Repair.use_hull_repair_post_delay, 1, 30, 2);
            events::Push(
                "autopilot repair hull=%.2f nanobots=%d shield=%.2f failed=%d attackers=%d fight-start=%d "
                "worst=%.0f hull-after=%.0f worst-soon=%.0f death-soon=%d rounds=%d rounds-hull=%.0f potential=%.0f "
                "hit-rate=%.3f missiles=%.0f",
                self.hull, self.nanobots, self.shield, shieldFailed ? 1 : 0, g_sit.attackers, fightStart ? 1 : 0, worst,
                hullAfter, worstSoon, deathSoon ? 1 : 0, roundsCount, roundsHull, potential, hitRate, missileDmg);
        }
        if (battFree > 0 && needShield && !budgetRhythm && shieldRoom && g.combat.shieldRepairTimer <= 0)
        {
            const float shieldNeed = std::max({worst + roundsShieldSoon + offlineAt - shieldLeft, 0.0f});
            const int n = units(shieldNeed, shieldGap, 300.0f, shieldFailed || deathSoon, battFree);
            const int used = ai::UseConsumable(true, n);
            if (!used)
                out.repairShield = true;
            events::Push("autopilot repair-units shield n=%d need=%.0f gap=%.0f %s", used ? used : -1, shieldNeed,
                         shieldGap, used ? "use_item" : "button");
            g.combat.batteriesBefore = self.batteries;
            shieldAtUse = std::max(0.0f, self.shield);
            g.combat.shieldRepairTimer = Pf(g_p.Repair.use_shield_repair_post_delay, 1, 30, 2);
            // (raw / up: the game's own points and online flag - a press into a rebooting shield reads up=0 with points)
            events::Push("autopilot repair shield=%.2f raw=%.0f/%.0f up=%d batteries=%d failed=%d attackers=%d hull=%.2f "
                         "worst=%.0f shield-left=%.0f rounds=%d rounds-hull=%.0f potential=%.0f hit-rate=%.2f "
                         "missiles=%.0f death-soon=%d",
                         self.shield, shCur, shMax, shUp ? 1 : 0, self.batteries, shieldFailed ? 1 : 0, g_sit.attackers,
                         self.hull, worst, shieldLeft, roundsCount, roundsHull, potential, hitRate, missileDmg,
                         deathSoon ? 1 : 0);
        }
    }
}

// Turret blind spots of big targets (user, 22 Oct 2026: "Rheinland battleships have blind-side gaps"; the 2020 footage
// of M13 parks inside the Nomad battleship's hull and kills it in ~45 s at full hull, where our pursuit died in 40 s):
// candidate spots in the target's own frame (26 directions x distances from 0.2 to 1.1 of its radius, and 150 / 300 m
// beyond it, those our guns reach), each scored by how many of its guns reach it - the game's own CEGun::CanPointAt and
// munition range (space::GunsCovering) - then by our way there. Re-read every second (turrets die, the target turns);
// a held spot is kept unless another has fewer guns on it. Only when the best spot is clearly better than where we are
// (2 guns fewer, or none at all). Targets with fewer than 4 guns: not used.
// Spots (the target's own frame, x y z) where we were hit anyway, per target - kept across fight restarts (M13: the
// objective's "fight over" restart forgot them and the next spot tried was one already hit).
static std::map<u32, std::vector<float>> g_blindBad;

// `fireWorld` (optional): the `ordnance_site` firing position in world space, written when g.combat.fireOn.
static bool BlindSpot(u32 target, float gunRange, float world[3], float* fireWorld = nullptr)
{
    // (turret reach from the guns' joint limits is game data a player never sees: Source::Hidden)
    if (!g_set.blindSpot || !Knows(Source::Hidden) || !target || gunRange <= 0)
        return false;
    float c[3], rot[9];
    fl::Kinematics me = {};
    if (!space::ObjectPose(target, c, rot) || !fl::PlayerKinematics(me))
        return false;
    auto toWorld = [&](const float l[3], float w[3])
    {
        for (int i = 0; i < 3; ++i)
            w[i] = c[i] + rot[i * 3 + 0] * l[0] + rot[i * 3 + 1] * l[1] + rot[i * 3 + 2] * l[2];
    };
    if (g.combat.blindTarget != target)
    {
        g.combat.blindTarget = target;
        g.combat.blindOn = false;
        g.combat.blindNext = 0;
        g.combat.blindCover = -1;
        g.combat.blindHits = 0;
        g.combat.fireOn = false;
        g.combat.fireCover = -1;
        if (g_blindBad.size() > 64)
            g_blindBad.clear();
    }
    std::vector<float>& bad = g_blindBad[target];
    if (g.time >= g.combat.blindNext)
    {
        g.combat.blindNext = g.time + 1.0f;
        // The turret test is the game's, yet what hits us decides (22 Oct 2026, M13: hit hard at a spot the test called
        // uncovered): two readings in a row at the spot with > 2 % of our hit points lost per second = a bad spot.
        const float lostNow = g.combat.lost - g.combat.blindLostAt;
        g.combat.blindLostAt = g.combat.lost;
        if (g.combat.blindOn)
        {
            float w[3];
            toWorld(g.combat.blindLocal, w);
            const float d[3] = {w[0] - me.pos[0], w[1] - me.pos[1], w[2] - me.pos[2]};
            const float dist = Len3(d);
            g.combat.blindDist = dist;
            // (blind_contact: the runners stay at one place for 21-35 s, shield cycling; ours hopped 1-4 times a fight on
            // 2 % a second and each hop restarted the approach - kept unless it costs 5 % a second)
            const float badRate = g_set.blindContact ? 0.05f : 0.02f;
            g.combat.blindHits = dist < 80 && lostNow > badRate ? g.combat.blindHits + 1 : 0;
            events::Push("autopilot blind-spot hold dist=%.0f cover=%d lost=%.3f", dist, g.combat.blindCover, lostNow);
            if (g.combat.blindHits >= 2)
            {
                bad.insert(bad.end(), {g.combat.blindLocal[0], g.combat.blindLocal[1], g.combat.blindLocal[2]});
                events::Push("autopilot blind-spot hit at spot %.0f,%.0f,%.0f: avoided from now",
                             g.combat.blindLocal[0], g.combat.blindLocal[1], g.combat.blindLocal[2]);
                g.combat.blindHits = 0;
                g.combat.blindOn = false;
            }
        }
        float radius = 0;
        for (const space::Obstacle& o : space::Obstacles(c, 50))
            if (o.id == target)
                radius = o.radius;
        if (radius <= 0)
            radius = 300;
        struct Cand
        {
            float local[3];
            float w[3];
            bool contact;
        };
        std::vector<Cand> cands;
        auto add = [&](float x, float y, float z, bool contact = false)
        {
            Cand k = {{x, y, z}, {}, contact};
            toWorld(k.local, k.w);
            cands.push_back(k);
        };
        // [0] where we are, [1] the held spot
        {
            const float rel[3] = {me.pos[0] - c[0], me.pos[1] - c[1], me.pos[2] - c[2]};
            add(rel[0] * rot[0] + rel[1] * rot[3] + rel[2] * rot[6],
                rel[0] * rot[1] + rel[1] * rot[4] + rel[2] * rot[7],
                rel[0] * rot[2] + rel[1] * rot[5] + rel[2] * rot[8]);
            add(g.combat.blindLocal[0], g.combat.blindLocal[1], g.combat.blindLocal[2]);
        }
        const float fracs[] = {0.2f, 0.35f, 0.5f, 0.7f, 0.9f, 1.1f};
        for (int x = -1; x <= 1; ++x)
            for (int y = -1; y <= 1; ++y)
                for (int z = -1; z <= 1; ++z)
                {
                    if (!x && !y && !z)
                        continue;
                    const float l = sqrtf(static_cast<float>(x * x + y * y + z * z));
                    auto at = [&](float d)
                    {
                        // our guns must reach the hull: the centre at most half a radius beyond their range
                        if (d - radius * 0.5f <= gunRange * 0.85f)
                            add(x / l * d, y / l * d, z / l * d);
                    };
                    for (float f : fracs)
                        at(radius * f);
                    at(radius + 150);
                    at(radius + 300);
                    // `blind_contact` (the speedrunners hold the M13 Nomad battleship at 0 m, fights.tsv "Order base"):
                    // the hull's surface in this direction (the game's ray test from outside its sphere to its centre),
                    // our radius + 15 m off it
                    if (g_set.blindContact)
                    {
                        const float out0 = radius + 300.0f;
                        float from[3];
                        const float dl[3] = {x / l * out0, y / l * out0, z / l * out0};
                        toWorld(dl, from);
                        const float hit = space::RayHitDistance(from, c);
                        if (hit > 0 && hit < out0)
                        {
                            // (`blind_contact_gap`: the refusal probe, 29 Oct 2026 - held radius + 15 m off the hull,
                            // the line gap read 22-44 m and the blast rule refused every torpedo (it needs blast 25 + our
                            // radius + 10 m): the runners' 0 m is safe in vanilla 1.1's 1/3 grazes, not in HD Edition)
                            const float d = out0 - hit + std::max(std::max(5.0f, space::PlayerRadius()) + 15.0f,
                                                                  g_set.blindContactGap);
                            if (d - radius * 0.5f <= gunRange * 0.85f)
                                add(x / l * d, y / l * d, z / l * d, true);
                        }
                    }
                }
        std::vector<int> cover(cands.size());
        std::vector<float> pts(cands.size() * 3);
        for (size_t i = 0; i < cands.size(); ++i)
            memcpy(&pts[i * 3], cands[i].w, sizeof(cands[i].w));
        // the joint limits widened by 0.2 rad (~11 deg): a spot at a turret's very limit counts as covered (M13: the
        // 466 / 333 m aft spots read uncovered at 0 and were hit in every run)
        const int guns = space::GunsCovering(target, reinterpret_cast<const float (*)[3]>(pts.data()),
                                             static_cast<int>(cands.size()), cover.data(), 0.2f);
        g.combat.blindGuns = guns;
        // ...and the guns of every other hostile big ship within 3 km of it count too (M10: three experimental ships -
        // one's blind spot lies in another's fire)
        if (guns >= 4)
        {
            std::vector<int> more(cands.size());
            for (const space::Contact& o : Contacts())
            {
                if (o.id == target || !o.ship || o.health == 0 || o.attitude > -0.6f ||
                    !((o.type & (kGunboatBit | kCruiserBit | kCapitalBit)) || o.maxHit > 20000))
                    continue;
                const float d[3] = {o.pos[0] - c[0], o.pos[1] - c[1], o.pos[2] - c[2]};
                if (Len3(d) > 3000)
                    continue;
                if (space::GunsCovering(o.id, reinterpret_cast<const float (*)[3]>(pts.data()),
                                        static_cast<int>(cands.size()), more.data(), 0.2f) > 0)
                    for (size_t i = 0; i < cands.size(); ++i)
                        cover[i] += more[i];
            }
        }
        if (guns < 4)
        {
            if (g.combat.blindOn)
                events::Push("autopilot blind-spot off target=%u guns=%d", target, guns);
            g.combat.blindOn = false;
            return false;
        }
        // Shelter (user, 22 Oct 2026: hugging the inside of the Rheinland battleship's bow trench, its hull blocked most
        // shots from the other ships): per spot the hostile ships within 2.5 km (up to 12) with a clear line to it through
        // the real hulls - a tiebreak among spots the turrets cannot reach (a covering turret weighs 1000, an exposed line
        // 150). blind_shelter.
        std::vector<int> exposed(cands.size(), 0);
        if (g_set.blindShelter)
        {
            int n = 0;
            for (const space::Contact& o : Contacts())
            {
                if (n >= 12 || o.id == target || !o.ship || o.health == 0 || o.attitude > -0.6f || o.distance > 2500)
                    continue;
                ++n;
                for (size_t i = 2; i < cands.size(); ++i)
                    if (!space::SightBlocker(o.pos, cands[i].w, o.id))
                        ++exposed[i];
            }
        }
        size_t best = 2;
        float bestScore = 1e30f, bestCoverScore = 1e30f;
        std::vector<float> scores(cands.size(), 1e30f); // (ordnance_site picks from them below)
        std::vector<float> coverScores(cands.size(), 1e30f);
        for (size_t i = 2; i < cands.size(); ++i)
        {
            const float d[3] = {cands[i].w[0] - me.pos[0], cands[i].w[1] - me.pos[1], cands[i].w[2] - me.pos[2]};
            float coverScore = cover[i] * 1000.0f;
            for (size_t b = 0; b + 2 < bad.size(); b += 3)
            {
                const float e[3] = {cands[i].local[0] - bad[b], cands[i].local[1] - bad[b + 1],
                                    cands[i].local[2] - bad[b + 2]};
                if (Len3(e) < 120)
                    coverScore += 5000.0f; // hit there before: as bad as five guns
            }
            // a spot we cannot fly to straight through free space - inside the target's hull, or behind a solid (M10 22 Oct
            // 2026: spots 133-333 m from the experimental ship's centre lay inside its model; the ship flew into it and
            // thrashed between them for 6 min at range 0) - weighs as 2.5 covering guns (real hulls, the target's included)
            if (space::SightBlocker(me.pos, cands[i].w, 0))
                coverScore += 2500.0f;
            // (blind_contact: a contact spot wins a tie with an equally covered one - below half a gun, above the way there)
            const float score = coverScore + exposed[i] * 150.0f + Len3(d) / 100.0f -
                                (g_set.blindContact && cands[i].contact ? 400.0f : 0.0f);
            scores[i] = score;
            coverScores[i] = coverScore;
            if (score < bestScore)
                bestScore = score, bestCoverScore = coverScore, best = i;
        }
        // `bs_approach` (M13-MIMIC 4c; the owner's "protrusion" idea, critiqued: a round holds the whole target and bursts
        // at the first hull it meets - 5,299 hits median 7 m to it - so the clear line from the spot to the centre decides
        // where our rounds burst): the 8 best spots pay 6 a metre the line falls short of bs_approach_gap (100 m short =
        // 0.6 of a covering gun: the turrets still rule, the line decides among equals). Ray tests: 8 a second
        if (g_set.bsApproach && g_set.bsApproachGap > 0)
        {
            std::vector<size_t> order;
            for (size_t i = 2; i < cands.size(); ++i)
                if (scores[i] < 1e29f)
                    order.push_back(i);
            std::sort(order.begin(), order.end(), [&](size_t a, size_t b) { return scores[a] < scores[b]; });
            if (order.size() > 8)
                order.resize(8);
            for (size_t i : order)
            {
                const float gap = space::RayHitDistance(cands[i].w, c);
                if (gap >= 0 && gap < g_set.bsApproachGap)
                    scores[i] += (g_set.bsApproachGap - gap) * 6.0f;
            }
            best = order.empty() ? best : order[0];
            for (size_t i : order)
                if (scores[i] < scores[best])
                    best = i;
            bestScore = scores[best];
            bestCoverScore = coverScores[best];
        }
        const int here = cover[0];
        const int bestCover = static_cast<int>(bestCoverScore / 1000.0f); // with the bad-spot penalty
        if (g.combat.blindOn && cover[1] <= bestCover)
            g.combat.blindCover = cover[1]; // the held spot is still as good as any
        else if (bestCover == 0 || bestCover + 2 <= here)
        {
            const bool moved = !g.combat.blindOn || bestCover < cover[1];
            memcpy(g.combat.blindLocal, cands[best].local, sizeof(g.combat.blindLocal));
            g.combat.blindCover = cover[best];
            if (moved)
                events::Push("autopilot blind-spot target=%u guns=%d radius=%.0f spot=%.0f,%.0f,%.0f cover=%d here=%d",
                             target, guns, radius, g.combat.blindLocal[0], g.combat.blindLocal[1],
                             g.combat.blindLocal[2], cover[best], here);
            g.combat.blindOn = true;
        }
        else if (g.combat.blindOn)
        {
            events::Push("autopilot blind-spot off target=%u best-cover=%d here=%d", target, bestCover, here);
            g.combat.blindOn = false;
        }
        // `ordnance_site` (WORK_QUEUE Q4 slice 1; the owner's rule 3: the ship moves between the guns' hold and a firing
        // position at the stand-off the ordnance needs): the best-scored candidate at least ordnance_standoff off the
        // hull - its gap by the ray from it to the centre, read only for the 3 best-scored candidates that can be that
        // far out (|spot| >= the stand-off; the gap is never more than that)
        g.combat.fireOn = false;
        if ((g_set.ordnanceSite || g_set.standoffHold) && g.combat.blindOn)
        {
            std::vector<size_t> order;
            for (size_t i = 2; i < cands.size(); ++i)
                if (scores[i] < 1e29f && Len3(cands[i].local) >= g_set.ordnanceStandoff)
                    order.push_back(i);
            std::sort(order.begin(), order.end(), [&](size_t a, size_t b) { return scores[a] < scores[b]; });
            // (standoff_hold: the 8 best - a stand-off of 300 m rarely fits the 3 best-covered spots, which hug the hull;
            // set ab2: the notch spot's line to the centre was ~50 m)
            // (...and 24 since the outside test - probe kb2, 31 Oct 2026: of the 8 best none was both >= 120 m and clear,
            // the clear points beyond the radius score worse on cover; the hold fell back to the 0.2 x radius spot)
            const size_t kSites = g_set.standoffHold ? 24 : 3;
            if (order.size() > kSites)
                order.resize(kSites);
            ordnance::SiteCand sc[24];
            int inside = 0;
            for (size_t j = 0; j < order.size(); ++j)
            {
                const float* w = cands[order[j]].w;
                sc[j].gap = space::RayHitDistance(w, c);
                sc[j].score = scores[order[j]];
                // (the centre ray alone read a 214 m gap at a site inside the battleship's envelope - probe kb1, 31 Oct
                // 2026: all 8 runs held 7-50 m off the hull short of local 0,0,233; run-judge): a ray from well outside
                // back to the site must reach it, and the site's six stand-off rays (the city rule) must be clear
                if (sc[j].gap >= g_set.ordnanceStandoff)
                {
                    float out[3] = {w[0] - c[0], w[1] - c[1], w[2] - c[2]};
                    const float ol = std::max(1.0f, Len3(out));
                    float far[3];
                    for (int a = 0; a < 3; ++a)
                        far[a] = w[a] + out[a] / ol * (2.0f * radius + 200.0f);
                    bool clear = space::RayHitDistance(far, w) < 0;
                    for (int axis = 0; axis < 3 && clear; ++axis)
                        for (int sgn = -1; sgn <= 1 && clear; sgn += 2)
                        {
                            float to[3] = {w[0], w[1], w[2]};
                            to[axis] += sgn * g_set.ordnanceStandoff;
                            clear = space::RayHitDistance(w, to) < 0;
                        }
                    if (!clear)
                        sc[j].gap = -1, ++inside;
                }
            }
            if (inside)
            {
                static Throttle said;
                if (said.Due(g.time, 5))
                    events::Push("autopilot fire-site rejected target=%u: %d of %d candidates inside or beside the hull",
                                 target, inside, static_cast<int>(order.size()));
            }
            const int pick = ordnance::PickFireSite(sc, static_cast<int>(order.size()), g_set.ordnanceStandoff);
            if (pick >= 0)
            {
                const size_t i = order[pick];
                const bool moved = g.combat.fireCover < 0 || geom::Dist(cands[i].local, g.combat.fireLocal) > 1.0f;
                memcpy(g.combat.fireLocal, cands[i].local, sizeof(g.combat.fireLocal));
                g.combat.fireGap = sc[pick].gap;
                g.combat.fireCover = cover[i];
                g.combat.fireOn = true;
                if (moved)
                    events::Push("autopilot fire-site spot target=%u spot=%.0f,%.0f,%.0f gap=%.0f cover=%d standoff=%.0f",
                                 target, cands[i].local[0], cands[i].local[1], cands[i].local[2], sc[pick].gap, cover[i],
                                 g_set.ordnanceStandoff);
            }
        }
    }
    if (!g.combat.blindOn)
        return false;
    toWorld(g.combat.blindLocal, world);
    if (fireWorld && g.combat.fireOn)
        toWorld(g.combat.fireLocal, fireWorld);
    return true;
}

// The distance from `from` to the first real hull on the straight line to the target's centre - the target's own hull
// included (the game's ray test); -1 when the test finds none (no collision mesh, out of space).
static float HullGap(const float from[3], const float centre[3]) { return space::RayHitDistance(from, centre); }

// The roll engine: hold a roll rate about the nose (rad/s; + = about the nose axis, the game's -z), at most `maxRate`
// (the ship's own top roll rate, steering_torque z / angular_drag z). The roll axis settles in inertia / drag (~0.03 s:
// 1000 / 35000 on a Liberty fighter), so the spin is set to the rate each frame - what the ship's own roll torque reaches
// at once (PhySys::AddToAngularVelocityOS, the difference to its spin now).
void RollApply(float dt, float want, float maxRate)
{
    (void)dt;
    float spin[3];
    if (!space::PlayerSpin(spin))
        return;
    const float target = -Clampf(want, -maxRate, maxRate); // (+z points backwards: about the nose = -z)
    const float delta[3] = {0.0f, 0.0f, target - spin[2]};
    space::AddPlayerSpin(delta);
}

// ---- The stand-off band per ship pair (the owner, 28 Oct 2026: "generalize these findings for all player ships vs all
// possible enemy ships ... maximizing time on target at all scenarios"). Measured first (43 677 combat samples of gates
// mpb0 / mpb2 / oddsab): inside 80 m the nose was on the target 50 % of the time and lost it (> 60 deg for over 1 s)
// 14 times a minute; 250-350 m 87 % and 0.14 a minute; per fight the hits per second were highest close (0-150 m 10.7,
// 150-250 m 9.2, 250-350 m 7.2) - accuracy falls with range. Hence a prior from the two ships' own numbers and the
// pair's own measurement:
// - the prior: the tracking floor 1.3 x the target's speed / our turn rate (a crossing at v from d sweeps at v / d
//   rad/s), 80 m .. 60 % of gun reach; the dodge reach (a bolt flies d / muzzle s, the target strafes 0.5 a t^2 - beyond
//   v_muzzle x sqrt(2 r / a) it can slip out of a shot) is logged beside it;
// - the measurement: per pair (our archetype, its archetype) in 50 m bins the seconds fought, the seconds on target and
//   the points dealt (hull + shield); with 60 s of a pair the band is the bin that dealt the most per second (bins
//   with 4 s or more), one bin either side every third fight while the pair has under 10 minutes (else a bin never
//   flown is never found). Kept in EXE\FLvibed\ranges.tsv (saved every 30 s of fighting).
struct RangeBins
{
    float t[16] = {}, on[16] = {}, dealt[16] = {};
};
static std::map<unsigned long long, RangeBins> g_ranges;
static bool g_rangesLoaded = false, g_rangesDirty = false;
static float g_rangesSince = 0;
static unsigned long long g_rangeKey = 0;
static u32 g_rangeTarget = 0;
static float g_rangeDealtPrev = -1;
static int g_rangeFights = 0;

static std::string RangesPath() { return DllDir() + "FLvibed\\ranges.tsv"; }

static void RangesLoad()
{
    g_rangesLoaded = true;
    FILE* f = _fsopen(RangesPath().c_str(), "r", _SH_DENYWR);
    if (!f)
        return;
    char line[256];
    while (fgets(line, sizeof line, f))
    {
        unsigned long long k = 0;
        int b = -1;
        float tt = 0, on = 0, d = 0;
        if (sscanf_s(line, "%llx %d %f %f %f", &k, &b, &tt, &on, &d) == 5 && b >= 0 && b < 16)
        {
            RangeBins& r = g_ranges[k];
            r.t[b] = tt, r.on[b] = on, r.dealt[b] = d;
        }
    }
    fclose(f);
}

static void RangesSave()
{
    FILE* f = _fsopen(RangesPath().c_str(), "w", _SH_DENYWR);
    if (!f)
        return;
    fprintf(f, "# FLvibed autopilot: fight time per ship pair (ours << 32 | theirs, hex) and 50 m bin: seconds, seconds "
               "on target, points dealt\n");
    for (const auto& kv : g_ranges)
        for (int b = 0; b < 16; ++b)
            if (kv.second.t[b] > 0)
                fprintf(f, "%llx\t%d\t%.1f\t%.1f\t%.0f\n", kv.first, b, kv.second.t[b], kv.second.on[b],
                        kv.second.dealt[b]);
    fclose(f);
    g_rangesDirty = false;
}

static float StandoffBand(const space::ShipMotion& me, const space::ShipMotion& tg, float reach, float muzzle,
                          float radius)
{
    if (!g_rangesLoaded)
        RangesLoad();
    const float track = 1.3f * tg.speed / me.turnRate;
    const float dodge =
        muzzle > 0 && tg.strafeAccel > 0.5f ? muzzle * sqrtf(2.0f * std::max(radius, 5.0f) / tg.strafeAccel) : reach;
    const float prior = Clampf(track, 80.0f, 0.6f * reach);
    const unsigned long long key = (static_cast<unsigned long long>(me.arch) << 32) | tg.arch;
    float band = prior, data = 0;
    const char* src = "prior";
    if (const auto it = g_ranges.find(key); it != g_ranges.end())
    {
        int best = -1;
        float bestRate = -1;
        for (int b = 0; b < 16; ++b)
        {
            data += it->second.t[b];
            if (it->second.t[b] >= 4.0f && it->second.dealt[b] / it->second.t[b] > bestRate)
                bestRate = it->second.dealt[b] / it->second.t[b], best = b;
        }
        if (data >= 60.0f && best >= 0)
        {
            static const int kExplore[3] = {0, -1, 1};
            const int b = data < 600.0f ? std::max(0, std::min(15, best + kExplore[g_rangeFights % 3])) : best;
            band = Clampf(b * 50.0f + 25.0f, 50.0f, 0.8f * reach);
            src = b == best ? "learned" : "explore";
        }
    }
    if (g_rangeTarget != g.target)
    {
        g_rangeTarget = g.target;
        g_rangeDealtPrev = -1;
        ++g_rangeFights;
        events::Push("autopilot standoff pair=%08X/%08X band=%.0f %s prior=%.0f track=%.0f dodge=%.0f data=%.0fs",
                     me.arch, tg.arch, band, src, prior, track, dodge, data);
    }
    g_rangeKey = key;
    return band;
}

static void RangeLearn(float dt, const aim::Target& t, bool aligned)
{
    if (!g_set.standoff || !g.target || !t.valid || g_rangeTarget != g.target || !g_rangeKey)
        return;
    const float dealt = g.combat.dealt + g.combat.dealtShield;
    // the guns' share only (gate mpb10's first minutes: missiles and torpedoes - launched from the close missile runs -
    // made the 50-100 m bins "best", 75 m against a prior of 115-223 m): what our rounds dealt is taken off
    static float missilePrev = -1;
    const float missiles = g.missiles.ownMissileDealt;
    const float byRounds = missilePrev >= 0 ? std::max(0.0f, missiles - missilePrev) : 0.0f;
    missilePrev = missiles;
    const float gained = g_rangeDealtPrev >= 0 ? std::max(0.0f, dealt - g_rangeDealtPrev - byRounds) : 0.0f;
    g_rangeDealtPrev = dealt;
    const int b = std::min(15, std::max(0, static_cast<int>(t.dist / 50.0f)));
    RangeBins& r = g_ranges[g_rangeKey];
    r.t[b] += dt;
    if (aligned)
        r.on[b] += dt;
    r.dealt[b] += gained;
    g_rangesDirty = true;
    g_rangesSince += dt;
    if (g_rangesSince >= 30.0f && g_rangesDirty)
    {
        g_rangesSince = 0;
        RangesSave();
    }
}

// Hold-and-salvo at a static target (session 5, 22 Oct 2026; the top-3 any% speedrun recordings, every generator,
// satellite and platform in all three runs - docs\research\speedrun.md V4: in fast, brake ~100 m short, hold still
// 30-80 m off the hull and fire everything, leave on the kill; M09 ~9 s per generator, M13 barrier 63-87 s for four, M12
// 72-101 s for three; never an orbit or a pass). Source::Skill. The hold point lies `static_hold_gap` off the real hull
// (HullGap) on the best of 9 sides - the line from the target to us first, then 8 round it - by a path clear of solids,
// the fewest attackers with a clear line to it, and the way there; it is fixed per target and re-chosen only when the
// side turns out bad. Beyond the station's reach: the pursuit (afterburner, braking by the closing speed); then station
// keeping on the point (nose and guns on the target; the launches below fire from it - their blast rule reads the hull
// gap, not the centre distance). Hull lost at the hold - more than 4 % of it in 3 s - marks the side bad and hands over
// to the passes for 8 s (the user's warning, 22 Oct 2026: holding still under fighters "is just asking to be destroyed";
// the runners hold because the salvo kills in seconds and batteries soak the shield fire - Repairs). False = not a
// static target, or the passes' turn.
// The hop to a static target's hold point (session 7, 23 Oct 2026; the user watching M12: "tried to engage another
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
    // Stuck against a hull (session 7, M12: 0 m/s for 40 s on a hop, pushing into the trench wall): under 10 m/s for 2 s
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
            // with what is left of it - else the afterburner (user, 23 Oct 2026: "use afterburners to close the gap to
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
        // A charge that does not complete (session 7, M12: 40 s "charging" at 80 m/s, reset by the Nomads' cruise
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
static float HoldGap() { return g.hold.city ? cityhold::Gap(HoldGapBase()) : HoldGapBase(); }

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

static bool StaticHold(u32 target, Out& out)
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
    // ...never at an armed one: the blind-spot run's (session 8, 23 Oct 2026, M13's Nomad battleship, a solar: the hold
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
    // Exposed: attackers close, the shield nearly gone and no battery to restore it - no holding still then (session 5,
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
                // ...but never off the target (user, 22 Oct 2026, speedrun #1: "as soon as a Nomad generator was targeted
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
                // them holds every torpedo / missile there (probe ch1, 31 Oct 2026: the second generator's side point
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
    // The hull ahead decides, not the centre (session 5, the user watching the full M13: "stopped firing weapons and got
    // too close to the shield generator" - a 666 m structure: the pursuit's band from its centre and the hold point off
    // its real hull never met, the ship closed to 150 m from the centre among the structure, hull 1.00 -> 0.69, the guns
    // mostly silent). Closer to the real hull ahead than the gap + 60 m: the hold point moves onto our own line at the
    // gap - held there, or pushed back out when we have come closer than the gap. The target's own hull only: the hit
    // inside its sphere (session 6, 22 Oct 2026: the ray met the M12 lair's lattice and the M13 sphere's towers 2-3 km
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
    // Never a hold point in a lethal sphere's berth (gate mpb23, 29 Oct 2026: a hold at an ice moon's hull, 102 m gap, sat
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
    // (session 7, 23 Oct 2026, M12: entered at 2.5 s of the speed / >= 250 m, the station keeping's 60 m/s crawled the
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
        // the slide (user, 22 Oct 2026, speedrun #1 M12 / M13: "near-contact sideways movements", "light orbit
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
            // the lateral run (session 7, the user: "a pure lateral run along the hull"): a speed along the hull, not a rate
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
        // all mines too (the user: "all weapons, all missiles and all mines until each generator was destroyed")
        // ...only sliding along the hull at near-zero distance (user, 23 Oct 2026: "deploying mines while at a distance
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
            else if (gapNow >= 0 && gapNow < g_set.staticHoldMineGap && lateral > 3.0f)
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

// Attack passes at a static target (session 4, 22 Oct 2026; the 2020 M13 St03 generators, catalogue c0011: all four in
// ~2 min - in fast, close along the hull firing at 50-100 m, away low over the shell between towers and through rings so
// the fighters lose the line, back for the next pass; batteries spent, nanobots untouched - the hull stays clean). In:
// the lead pursuit on the afterburner with its closing band at the radius + 80 m (it slows into the window); window:
// within the radius + 200 m, firing, at most 4 s or until the shield fails; out: to an exit 600 m beyond the hull, the
// best of 8 directions across the line to the target - a path clear of solids (real hulls) and the fewest attackers
// with a clear line to it (cover) - on the afterburner until 500 m out, then in again. Returns false when the target is
// no static one (the pursuit as before).
static bool StaticPass(u32 target, Out& out)
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
        // `big_coast` (the owner, 29 Oct 2026, from the runners' city generators: "cruise to the first one, then ENGINE
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
        // (the owner, 29 Oct 2026: "coasting from a distance to start firing early while leaving attackers behind, and as
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
static void StrafePassLog(const char* end)
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
static bool StrafeFly(u32 target, float gunRange, Out& out)
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
static bool StaticOrbit(u32 target, float dt, float point[3])
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

// The duel flight against another player (pvp_duel; the owner, 28 Oct 2026, watching two autopilots duel on a local
// server: "Both ships just fired at each other from point-blank for most of the fight's duration"; "PvP fights need to
// balance extreme evasive maneuvers and making full use of chances to inflict maximum damage while taking minimal damage
// in return"). The duels before: both on close-turn, 4-60 m apart, mirror images of each other (one deterministic AI on
// both ends), one ended by ramming. A layer over the fight's own output, from the last 0.1 s sample of who has whom in
// the 15-degree cone (the duel statistics' geometry):
//   defend   their guns on us, ours not on them: jink - a slide in a random direction held a random 0.35-1.25 s, the
//            afterburner in irregular bursts, the corkscrew - while the pursuit keeps turning the nose onto them
//   trade    both on: keep firing, sliding out of their line (a jink without the afterburner: the aim holds)
//   punish   ours on them, theirs not: steady - no slide, no weave, every gun
//   break    inside pvp_min_dist and closing: slide off on the afterburner (no ramming)
//   maneuver neither: the fight's own flight, the afterburner to close beyond gun range
// The random source is per process (two clients never fly the same pattern). Seconds per state: CombatStats `pvp-...`.
static u32 PvpRand()
{
    static u32 s = static_cast<u32>(GetTickCount64()) ^ (GetCurrentProcessId() * 2654435761u);
    s = s * 1664525u + 1013904223u;
    return s >> 8;
}

static void PvpDuel(float dt, Out& out)
{
    if (g_set.pvpDuel <= 0 || !g.target || !g.combat.nowValid || g.tactic == Tactic::Flee)
        return;
    if (g.combat.pvpTarget != g.target)
    {
        g.combat.pvpTarget = g.target;
        g.combat.pvpPlayer = space::OwnerPlayer(g.target) != 0;
        g.combat.pvpState = -1;
    }
    if (!g.combat.pvpPlayer && g_set.pvpDuel < 2)
        return;
    const bool theyOn = g.combat.nowTheyOn, weOn = g.combat.nowWeOn;
    const float d = g.combat.nowDist;
    const bool closing = d < g.combat.prevDist - 0.5f;
    if (g.time >= g.combat.jinkUntil)
    {
        g.combat.jinkSlide = 1 + static_cast<int>(PvpRand() % 4);
        g.combat.jinkUntil = g.time + 0.35f + static_cast<float>(PvpRand() % 900) / 1000.0f;
        g.combat.jinkBurn = PvpRand() % 100 < 60;
    }
    const float range = out.gunRange > 0 ? out.gunRange : 600.0f;
    // the duel's distance: the pursuit's close-in for a fighter-sized target is a few dozen metres (the first A/B round,
    // 28 Oct 2026: the whole fight at 20-70 m, "break" every second and the pursuit pulling straight back in) - hold a
    // stand-off inside the gun range instead, where a jink takes the ship out of the other's line
    out.pursueBand = std::max(out.pursueBand, std::min(g_set.pvpRange, range * 0.8f));
    int state = 0;
    if (d < g_set.pvpMinDist && closing)
    {
        state = 4;
        out.slide = g.combat.jinkSlide;
        out.afterburn = true;
    }
    else if (theyOn && !weOn)
    {
        state = 1;
        out.slide = g.combat.jinkSlide;
        out.afterburn = g.combat.jinkBurn;
        out.weave = std::max(out.weave, 1.0f);
    }
    else if (theyOn && weOn)
    {
        state = 2;
        out.slide = g.combat.jinkSlide;
    }
    else if (weOn)
    {
        state = 3;
        out.slide = 0;
        out.weave = 0;
    }
    else if (d > range * 0.8f)
        out.afterburn = true;
    g.combat.pvpT[state] += dt;
    if (state != g.combat.pvpState)
    {
        static const char* const kName[] = {"maneuver", "defend", "trade", "punish", "break"};
        g.combat.pvpState = state;
        events::Push("autopilot pvp-duel %s dist=%.0f they-on=%d we-on=%d", kName[state], d, theyOn ? 1 : 0,
                     weOn ? 1 : 0);
    }
}

void UpdateCombat(float dt, Out& out)
{
    // Never cruise in a fight (user rule; the flee below hands it to the game's goto): a cruise still running from the
    // travel before the fight, or one a game behaviour started, was left on ("leave as it is") - the ship passed its
    // targets at 300 m/s and hit almost nothing (job board Kill Ships vs level-1 Rogues, 18 Oct 2026: 13 hull dps of 272).
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
        // a live target is kept at least target_dwell seconds (session 5, M09 22 Oct 2026, the user: "indecisiveness
        // while targeting the shield generators" - the pressed rule's shield hysteresis and the nearer-wins ranking
        // flipped generator <-> attacking fighter every ~3 s, each switch a new approach and nothing fired)
        const bool dwell = g.target && g.time - g.combat.targetSince < g_set.targetDwell && FindContact(g.target).id;
        // a static target being held is fired upon until it dies (user, 22 Oct 2026, speedrun #1: "as soon as a Nomad
        // generator was targeted, it was fired upon without pause until it was destroyed") - only an emergency (hull
        // under 35 %) lets the ranking switch to the attackers
        const float hullNow = ai::PlayerHealth();
        const bool holdStatic = g.hold.on && g.hold.target == g.target && g.target && FindContact(g.target).id &&
                                !(hullNow >= 0 && hullNow < 0.35f);
        // a live enemy ship is finished before the next (`finish_target`; the owner, 28 Oct 2026: "when dealing with enemy
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
        // and the gunboat was given up 8 s into that run, then ignored 30 s: "tactic=seek", fights +22-35 s, 21 Oct 2026)
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
    // fight the strong ship wins, 4 heavy fighters at hull 0.58, threat 9.9 vs strength 4.1, 19 Oct 2026).
    // multiplayer: mp_caution / mp_flee (a lost ship is lost there - the owner, 28 Oct 2026)
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
    // Hopeless in free roam (user, 19 Oct 2026: "in free roam the player is not in a scripted do-or-die scenario, so
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
    // Calibrated on 48 fights (both ships, 12 kinds, each also flown never leaving: all winnable, 19 Oct 2026): the
    // loadout prior is rough - winnable fights read 0.18-0.25 (5 level-1 pirates, 3 level-6 close, 2 level-9 elites:
    // left and died) - so it decides alone only when extreme (< 0.4 x retreat_odds = 0.02: 4 heavy fighters read
    // 0.01, and leaving at once matters there); else the measured race decides (wins read as low as 0.08 - 2 heavy
    // fighters vs the starter ship, won 3 of 3), held 5 s.
    const float limit = race.measured ? g_set.retreatOdds : g_set.retreatOdds * 0.4f;
    g.combat.outgunnedFor = race.ships > 0 && race.odds < limit ? g.combat.outgunnedFor + dt : 0.0f;
    const bool outgunned = g.combat.outgunnedFor > 0 && (!race.measured || g.combat.outgunnedFor >= 5.0f);
    // The head count (fighter units) only where the loadouts give no race: more ships joining pushed it past 4x in a
    // fight being won (3 pirates, odds 0.70, 19 Oct 2026).
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
        // The human retreat (user, 20 Oct 2026; was: jink until 1.5 s quiet, then the game's straight goto with cruise -
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
        // afterburner's ~200 m/s passed an "above engine speed" test: phase 2 began 16 ms into the charge, 20 Oct 2026)
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
        // tractor-all phase - switched to Seek here and back to Loot below, every frame, 18 Oct 2026).
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
        // Not over while hostiles hidden nearby are about to come out (the Rogue Base's bays, 19 Oct 2026: combat said
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
        // the Rogue Base between waves and died under its guns). Evasion by the numbers (user, 19 Oct 2026: retreating
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
            // Then rest the way a player does (user, 19 Oct 2026: after the prison ship fight the ship sat 40 s in
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
    // 31 Oct 2026, AUD-A1-5) (user, 20 Oct 2026: "hull is sacred" -
    // break off while the shield is low so it regenerates; kills do not matter there; the 2020 M04 opening: the shield
    // never below 0.43): the trigger is the shield below 50 % while hit (35 % otherwise), and escorting, the run ends
    // before 2000 m from the escorted ship (missions tether to it: Benford fails beyond 2500 m of Juni).
    // ... only while the fight is being lost (user, 21 Oct 2026, M04 Freeport 4: "combat behavior leaves a lot to be
    // desired, the ship took forever to start engaging" - extend after extend away from bounty hunters it was beating,
    // the target off screen behind us; odds 0.8-6.5, the user fought that one through on batteries in 2020): the race
    // (FightOdds) lost, or without loadouts the threat above our strength. A won fight is fought; batteries cover it.
    const bool raceLosing = race.ships > 0 ? race.odds < 1.0f : g_sit.threat > Strength(self);
    const bool survivalExtend = ObjectiveOn() && g_obj.survival && raceLosing;
    // The head-on pass holds under fire (the owner, 28 Oct 2026, the fight matrix: "the player ship stops reverse thrusting
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
    // (...nor in a chase_kill chase: the cruise at him is the evasion - probe as2, 31 Oct 2026: tactic=evade broke every
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

    // Fight entry (user, 18 Oct 2026, the fast-dock philosophy): a far target is approached by cruise, dropped where a
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
            // Aggressive interception (user, 20 Oct 2026: "stop cruise engines just as it enters weapons range and fire
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

    // Out of a big ship's bow arc (user, 21 Oct 2026, M04 opening: "the jittery, straight-on approach against a cruiser
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
    // coast ended stayed on: the whole hold sat at 2-28 m/s, throttle -1, engine off; M13 city, 29 Oct 2026)
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
    // carries us into a burst we launched (M13 i1, 30 Oct 2026: the hold's goal 55 m off the hull, our torpedo's burst on it)
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
                    // the real hulls decide the way out (session 7, M12 trench: "away from the sphere's centre" pointed
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

    // The turn onto a target far off the nose, while attacked (user, 20 Oct 2026: "it gets hit a lot when it switches
    // targets ... just turns to face the next target without using afterburners or reverse thrust"): the turn is itself
    // a dodge, as a player flies it - slide bursts in a new direction every 0.7 s, the afterburner on (the flight path
    // bends fast, off the attackers' lead), or reverse thrust when the target is behind and within gun range (the
    // tighter turn brings it onto the nose sooner, without running away from it). From 60 degrees off the nose until 25,
    // at most 4 s. Inside 250 m only the slides: the pursuit's own throttle (reverse through sharp turns) is the close
    // dogfight's (first fight matrix, 20 Oct 2026: started at 40 degrees, it burned 5-8 s at 30-75 m - the ship overshot
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

    // The head-on pass (`headon_reverse`; the owner, 28 Oct 2026: "when the target is currently flying at the player,
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
        // (reverse thrust only barely beyond the guns' reach - the owner, 28 Oct 2026: "Too many seconds are wasted waiting
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
                // ...and the pass kept wide (the owner, 28 Oct 2026: "use lateral and vertical thrusters while maintaining
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
    // The hand-over to the pursuit (the owner, 28 Oct 2026: "keep the transition from reverse thrusting and sideways
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
    // Roll (`roll_guns`, `roll_flair`; the owner, 28 Oct 2026: "work on rolling and make it as useful as possible, while
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
            // (`lof_caution`, the owner, 28 Oct 2026: "too much of an over-emphasis on avoiding friendly fire ...
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
    // (user, 18 Oct 2026: "engage afterburners to close the distance and aim the nose to guarantee the shortest flight
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
    // (22 Oct 2026: the torpedo, 2424 hull damage, was "the launcher" - its flight planned, the missile key tapped, not one
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
        // The more hull damage taken, the sooner a missile is worth it (user, 18 Oct 2026, "very delicate"): the
        // kill time that warrants one scales with the hull left - 12 s at full hull, smoothly down to ~4 s at 35 %.
        const float hullLeft = hull >= 0 ? Clampf(hull, 0.35f, 1.0f) : 1.0f;
        const bool slow = ourDps > 0 && hp / ourDps > 12.0f * hullLeft;
        // Never a missile that arrives after the target is dead (user, 18 Oct 2026, the stray of M02's jailbreak fight
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
        // owner, 28 Oct 2026: "maximum use of all munitions at all times" at big solars; a stray there hits nothing)
        const bool deadFirst =
            hp > 0 && (g_set.staticFullDps && !c.ship ? inFlight >= hp : ourDps * flight + inFlight >= hp);
        // A seeker can pick up what crosses its cone: a wide corridor, 400 m past the target, plus the blast. At a static
        // target close by (a solar whose hull the ray test sees within 300 m) the round cannot miss or fly on past the
        // hull: the corridor is the blast plus 30 m and ends at the target (session 5, M13 St03: the wide corridor - the
        // allies around and the sphere's shell "behind" each generator - held 90 % of the salvos at the hold).
        const float closeHull = c.id && !c.ship ? HullGap(self3.pos, c.pos) : -1.0f;
        // (a huge static target counts as close within its seeker's reach: it never dodges, and the flight-time gate held
        // every round through the coast in - the owner, 29 Oct 2026: "the ship still fires too late")
        const bool staticClose = (closeHull >= 0 && closeHull < 300.0f) || (!c.ship && t.radius >= 500.0f && closeHull >= 0);
        // (the corridors scale by `lof_caution`, the blast never)
        const float lc = g_set.lofCaution;
        const bool friendlyNear =
            c.id && (staticClose ? LineOfFireBlocked(c.id, c.pos, 30.0f * lc + launcher->blastRadius, 0.0f, 0.0f)
                                 : LineOfFireBlocked(c.id, c.pos, 200.0f * lc + launcher->blastRadius, 400.0f * lc,
                                                     400.0f * lc + launcher->blastRadius));
        // The economy (user, 18 Oct 2026: the player fires cheap missiles that refill in play freely - "tractor all"
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
        // (`missile_lead`; the owner, 28 Oct 2026: "aim the ship and missile launcher in anticipation of where the target
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
        // us - the M13 torpedo, 22 Oct 2026)
        const float minDist = std::max({120.0f, launcher->blastRadius * 4, t.radius > 50 ? t.radius + 100.0f : 0.0f});
        // The blast rule by the real hull where the ray test sees one (session 5, 22 Oct 2026: the top-3 runners fire
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
        // `launch_site_check` (WORK_QUEUE Q4 slice 1; the owner, 29 Oct 2026: "You MUST maintain full DPS while surviving
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
        // centre lies ~430 m behind it, and the launchers held through the whole coast - the owner, 29 Oct 2026: "weapons
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
        // A missile meeting a shield is spent on it (user, 19 Oct 2026, M01b: a Javelin launched a touch too soon was
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
        // The burst (user, 21 Oct 2026: "missiles as a burst - needs high prediction accuracy"): a failed shield is a
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
        // The run-in (user, 21 Oct 2026: never launch "absent-mindedly"): the target type's learned flight limit
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
            // ...and at a static target we hold at (session 8, 23 Oct 2026, M12: the pilot's missile_launch_interval_time,
            // 20-45 s for military pilots, let one 4-missile salvo go per 50 000-point generator while the torpedoes went
            // every 2 s; the user: "all weapons, all missiles and all mines until each generator was destroyed")
            // ...and at every solar and big ship (the owner, 28 Oct 2026: "when the objective wants big solars destroyed
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

// Countermeasures, the player's way (user 17 Oct 2026): only against what a CM can divert (homing munitions:
// missiles, torpedoes, cruise disruptors; mines not verified), as few as possible, timed so they miss. A dropped CM
// ([CounterMeasure] range 1000 m, diversion_pctg 70, lifetime 3 s) diverts the homing munitions within its range by
// chance: drop one once they are inside cm_distance, again every 0.35 s while one still homes on us (91 % after two).
// countermeasures 1: cruise disruptors outside a fight (cruise disruption is what the player hates; in a fight they
// chose, it does not matter); in a fight only when what homes on us would meaningfully approach depleting the shield
// (its predicted damage - a hit is not certain - >= cm_shield_fraction of the shield left), or, with the shield
// down, cost a real part of the hull (>= 5 %). 2: every homing munition aimed at us.
void Countermeasures(float dt, Out& out)
{
    g.missiles.cmTimer -= dt;
    if (!g_set.countermeasures)
        return;
    const std::vector<space::Missile> ms = space::Missiles();
    // Per-munition bookkeeping grows for the whole session: drop it when large (no munition lives that long).
    if (g.missiles.cmDrops.size() > 256)
        g.missiles.cmDrops.clear();
    if (g.missiles.dodged.size() > 256)
        g.missiles.dodged.clear();
    ai::Self self;
    ai::ReadSelf(self);
    const float shieldPts = self.shield > 0 ? self.shield * self.shieldMax : 0.0f;
    const float hullPts = self.hull > 0 ? self.hull * self.hullMax : 0.0f;
    // Missile outcomes (statistics, every homing munition aimed at us within 1.5 km, dodged or not): when one is gone,
    // wait 0.3 s (its explosion's damage lands after it vanishes), then count a hit if our points fell by >= 40 % of its
    // damage since the last frame it was seen.
    const float pointsNow = shieldPts + hullPts;
    std::map<u32, std::pair<float, float>> seen; // id -> (damage, our points)
    for (const space::Missile& m : ms)
        if (m.targetsPlayer && m.homing && m.distance < 1500.0f)
            seen[m.id] = {m.hullDamage, pointsNow};
    for (const auto& w : g.missiles.watch)
        if (!seen.count(w.first))
            g.missiles.gone.push_back({g.time, w.second.first, w.second.second, w.first});
    g.missiles.watch.swap(seen);
    for (auto it = g.missiles.gone.begin(); it != g.missiles.gone.end();)
    {
        if (g.time - it->time < 0.3f)
        {
            ++it;
            continue;
        }
        const float lostPts = it->points - pointsNow;
        const bool hit = lostPts >= 0.4f * it->damage;
        ++(hit ? g.missiles.hits : g.missiles.misses);
        events::Push("autopilot missile-%s munition=%u lost=%.0f damage=%.0f dodged=%d", hit ? "hit" : "missed", it->id,
                     lostPts, it->damage, g.missiles.dodged.count(it->id) ? 1 : 0);
        it = g.missiles.gone.erase(it);
    }
    if (ms.empty())
        return;
    const bool fighting = (g.mode == Mode::Combat || g.mode == Mode::Escort) &&
                          (g.tactic == Tactic::Engage || g.tactic == Tactic::Evade || g.tactic == Tactic::Break ||
                           g.tactic == Tactic::Seek || g.tactic == Tactic::Extend);
    float incoming = 0; // predicted damage of what homes on us within reach
    bool cd = false;
    const space::Missile* nearest = nullptr;
    for (const space::Missile& m : ms)
    {
        if (!m.targetsPlayer || !m.homing || m.closing < -5.0f || m.distance > g_set.cmDistance)
            continue;
        incoming += m.hullDamage;
        cd = cd || m.cruiseDisruptor;
        // The nearest one times the drops and the dodge (was: the first listed, 18 Oct 2026).
        if (!nearest || m.distance < nearest->distance)
            nearest = &m;
    }
    if (!nearest)
        return;
    // ...and a cruise disruptor whenever the engine charges or cruises (session 7, 23 Oct 2026, M12: a hop between
    // generators charged for 40 s at 80 m/s - the charge reset again and again; the user's try 3 drops countermeasures
    // before and during each charge against the Nomads' disruptors)
    bool charging = false;
    {
        flight::Model fm;
        charging = flight::ReadModel(fm) && fm.cruiseEngaged;
    }
    const bool matters = g_set.countermeasures >= 2 || (cd && (!fighting || charging)) ||
                         (shieldPts > 1 ? incoming >= g_set.cmShieldFraction * shieldPts : incoming >= 0.05f * hullPts);
    if (!matters)
        return;
    // Countermeasures only with a dropper and ammo (the test Starflier has none: its CM taps did nothing and 747-damage
    // police missiles killed it, 17 Oct 2026); after 3 drops a missile still homing on us is not going to be diverted.
    const int cmAmmo = space::PlayerCountermeasures();
    int& drops = g.missiles.cmDrops[nearest->id];
    // A budget (session 8, 23 Oct 2026, M12: 3 drops per missile spent all 50 in the lair - in Omicron Minor every charge
    // was disrupted at 80 m/s and missiles killed the ship; the user: "budget the 50 over the whole leg ... keep a reserve
    // for the exit -> hole cruise"): one drop per missile under 25 left; under 12 only for a cruise disruptor while the
    // engine charges or cruises
    const int maxDrops = cmAmmo >= 25 ? 3 : 1;
    const bool reserveOk = cmAmmo >= 12 || (cd && charging);
    const bool canCm = cmAmmo > 0 && drops < maxDrops && reserveOk;
    if (canCm && g.missiles.cmTimer <= 0)
    {
        out.cm = true;
        g.missiles.cmTimer = 0.35f;
        ++drops;
        events::Push("autopilot cm munition=%u dist=%.0f closing=%.0f cd=%d incoming=%.0f shield=%.0f ammo=%d",
                     nearest->id, nearest->distance, nearest->closing, cd ? 1 : 0, incoming, shieldPts, cmAmmo);
    }
    // Dodge by maneuver (the player's way without countermeasures, and the NPC's DodgeMissileIdea): about a second
    // before impact slide hard to one side with afterburner - the missile turns slower than the ship moves sideways and
    // overshoots. One side per missile; afterburner keeps the ship's speed up through the slide.
    if (!canCm && nearest->closing > 20.0f)
    {
        const float tti = nearest->distance / nearest->closing;
        if (tti < g_set.dodgeWindow)
        {
            if (g.missiles.dodgeMissile != nearest->id)
            {
                g.missiles.dodgeMissile = nearest->id;
                g.missiles.dodged.insert(nearest->id);
                g.missiles.slide = (rand() & 1) ? 1 : 2; // left / right
                events::Push("autopilot dodge-missile munition=%u dist=%.0f closing=%.0f damage=%.0f shield=%.0f",
                             nearest->id, nearest->distance, nearest->closing, nearest->hullDamage, shieldPts);
            }
            out.dodgeSlide = g.missiles.slide;
            out.afterburn = true;
        }
    }
}

} // namespace autopilot
