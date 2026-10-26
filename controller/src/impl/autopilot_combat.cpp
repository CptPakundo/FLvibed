// Advanced Autopilot - combat: the duel, damage and repairs, the fight loop, countermeasures. Part of autopilot.cpp until 21 Oct 2026; shared state and declarations in
// autopilot_impl.h, the module API in autopilot.h.
#include "autopilot_impl.h"

namespace autopilot
{
// ------------------------------------------------------------------ combat
// The player's way out of fire (user + playlist study): afterburner away from `from`, jinking - a new offset of
// extend_jink degrees around the away direction every 0.7 s - with slide bursts. Shared by the extend and the retreat
// from fixed guns. `jinkDeg` >= 0 overrides the jink angle; `burner` false = no afterburner and no slides (both would
// cancel a cruise charge: the retreat's charge and cruise phases jink without them).
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
    const float th = (jinkDeg >= 0 ? jinkDeg : g_set.extendJink) * 3.14159265f / 180.0f;
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
        events::Push("autopilot big-hit lost=%.0f of %.0f shield=%.2f hull=%.2f nearest:%s", frameLoss, totalHp,
                     self.shield, self.hull, who.c_str());
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
        // the shield can recharge before hull damage starts; repair_shield_at only caps it (never above). Nanobots
        // only when the hull is low and being hit, or critical. g.stats.pressure / lossRate / maxHit: this frame.
        // TODO: predict from the attackers' weapons (per-shot damage, refire) once their loadouts are readable.
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
        // additional heavy damage? hold off"): below repair_hull_at only when the damage predicted for the next moment
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
        const ULONGLONG nowMs = GetTickCount64();
        if (nowMs - potAt >= 500)
        {
            potAt = nowMs;
            potential = 0;
            for (const space::Contact& c : Contacts())
                if (c.ship && c.targetsPlayer && c.attitude <= -0.6f && c.distance < 1200) // within gun range of us
                    for (const space::Weapon& w : space::ShipWeapons(c.id))
                        if (!w.homing && w.active && w.refire > 0)
                            potential += w.hullDamage / w.refire;
            // learn how much of the potential lands (aim, range, our evasion); floor 0.01 (was 0.05: 4 heavy Rheinland
            // fighters, potential 17485/s, really landed ~1 % - the floor predicted 874/s and death every moment)
            if (potential > 0 && g.stats.lossRate > 0)
                hitRate += (std::clamp(g.stats.lossRate / potential, 0.01f, 1.0f) - hitRate) * 0.2f;
        }
        float missileDmg = 0;
        for (const space::Missile& m : space::Missiles())
            if (m.targetsPlayer && m.closing > 1 && m.distance / m.closing < kHorizon)
                missileDmg += m.hullDamage;
        const float expected = std::max(g.stats.lossRate, potential * hitRate) * kHorizon + missileDmg;
        const float worst = expected + g.stats.maxHit;
        const float hullAfter = self.hull * self.hullMax - std::max(0.0f, worst - shieldLeft);
        const bool deathRisk =
            self.hullMax > 0 && hullAfter < 0.20f * self.hullMax && (g_sit.attackers > 0 || missileDmg > 0);
        // Death within the reaction time (a press acts at once): only this justifies wasting part of a unit. A risk
        // further out is better met by waiting until the gap takes a whole unit (19 Oct 2026: 3 nanobots spent at hull
        // 0.71 for +317 each, none left for the burst that killed the ship 30 s later).
        static const float kReact = 0.75f;
        float missileSoon = 0;
        for (const space::Missile& m : space::Missiles())
            if (m.targetsPlayer && m.closing > 1 && m.distance / m.closing < kReact)
                missileSoon += m.hullDamage;
        const float worstSoon = std::max(g.stats.lossRate, potential * hitRate) * kReact + missileSoon + g.stats.maxHit;
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
            self.hullMax > 0 && (g_sit.attackers > 0 || missileSoon > 0 || roundsCount > 0) &&
            (self.hull * self.hullMax - std::max(std::max(0.0f, worstSoon - shieldLeft), roundsHullSoon + missileSoon) <
             0.20f * self.hullMax);
        const float modelHullLoss = (g_sit.attackers > 0 || missileDmg > 0 || roundsCount > 0)
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
                                 (g_sit.attackers > 0 || roundsCount > 0 || missileSoon > 0) &&
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
        const int nanoFree = deathSoon || self.hull < 0.35f
                                 ? self.nanobots
                                 : self.nanobots - std::max(2, static_cast<int>(std::ceil(0.15f * self.nanobotMax)));
        const int battFree = deathSoon || shieldFailed
                                 ? self.batteries
                                 : self.batteries - std::max(2, static_cast<int>(std::ceil(0.15f * self.batteryMax)));
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
        if (battFree > 0 && needShield && shieldRoom && g.combat.shieldRepairTimer <= 0)
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
            events::Push("autopilot repair shield=%.2f batteries=%d failed=%d attackers=%d hull=%.2f worst=%.0f "
                         "shield-left=%.0f rounds=%d rounds-hull=%.0f potential=%.0f hit-rate=%.2f missiles=%.0f",
                         self.shield, self.batteries, shieldFailed ? 1 : 0, g_sit.attackers, self.hull, worst,
                         shieldLeft, roundsCount, roundsHull, potential, hitRate, missileDmg);
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

static bool BlindSpot(u32 target, float gunRange, float world[3])
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
            g.combat.blindHits = dist < 80 && lostNow > 0.02f ? g.combat.blindHits + 1 : 0;
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
        };
        std::vector<Cand> cands;
        auto add = [&](float x, float y, float z)
        {
            Cand k = {{x, y, z}, {}};
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
            const float score = coverScore + exposed[i] * 150.0f + Len3(d) / 100.0f;
            if (score < bestScore)
                bestScore = score, bestCoverScore = coverScore, best = i;
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
    }
    if (!g.combat.blindOn)
        return false;
    toWorld(g.combat.blindLocal, world);
    return true;
}

// The distance from `from` to the first real hull on the straight line to the target's centre - the target's own hull
// included (the game's ray test); -1 when the test finds none (no collision mesh, out of space).
static float HullGap(const float from[3], const float centre[3]) { return space::RayHitDistance(from, centre); }

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
static float HoldGap()
{
    static float at = -10, gap = 0;
    if (g.time - at < 1.0f && gap > 0 && g.time >= at) // (g.time restarts at 0 with every run: re-read then)
        return gap;
    at = g.time;
    gap = g_set.staticHoldGap;
    if (g_set.missiles)
        for (const space::Weapon& w : space::PlayerWeapons())
            if (w.homing && w.ammo > 0)
                gap =
                    std::max(gap, w.blastRadius + std::max(5.0f, space::PlayerRadius()) + 16.0f); // (the launch rule's
                                                                                                  // need + 6 m)
    return gap;
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
    }
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
        if (g_obj.on && g_obj.leaveN && GetTickCount64() - g_obj.leaveAt < 3000)
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
        float best = 1e30f;
        for (int i = -1; i < 8; ++i)
        {
            float dir[3];
            if (i < 0)
                memcpy(dir, r0, sizeof(dir));
            else
            {
                const float a = static_cast<float>(i) * 3.14159265f / 4.0f;
                for (int j = 0; j < 3; ++j)
                    dir[j] = cosf(a) * u[j] + sinf(a) * v[j] + 0.5f * r0[j];
                geom::Norm(dir);
            }
            // the real hull on that side: a ray from just outside the target's sphere to its centre (from farther out it met
            // the structures around first - M13 St03: the sphere's towers put hold points 500 m out)
            float far[3];
            for (int j = 0; j < 3; ++j)
                far[j] = tc.pos[j] + dir[j] * (radius + 5.0f);
            const float h = HullGap(far, tc.pos);
            const float surface = h >= 0 ? radius + 5.0f - h : radius; // from the centre
            float p[3];
            for (int j = 0; j < 3; ++j)
                p[j] = tc.pos[j] + dir[j] * (surface + HoldGap());
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
            const float pd[3] = {p[0] - k.pos[0], p[1] - k.pos[1], p[2] - k.pos[2]};
            score += Len3(pd) / 100.0f;
            if (leaveTo)
            {
                const float lq[3] = {leaveTo[0] - p[0], leaveTo[1] - p[1], leaveTo[2] - p[2]};
                score += Len3(lq) / 4.0f;
            }
            if (score < best)
            {
                best = score;
                memcpy(g.hold.pos, p, sizeof(p));
                memcpy(g.hold.dir, dir, sizeof(dir));
            }
        }
        g.hold.on = true;
        g.hold.at = false;
        g.hold.hull = -1;
        g.hold.orbitDir = (g.frames & 1) ? 1.0f : -1.0f; // the slide's one direction for this hold
        g.hold.slideT = 0;
        const float hc[3] = {g.hold.pos[0] - tc.pos[0], g.hold.pos[1] - tc.pos[1], g.hold.pos[2] - tc.pos[2]};
        events::Push("autopilot static-hold target=%u radius=%.0f dist=%.0f point=%.0f m from the centre score=%.0f",
                     target, radius, d, Len3(hc), best);
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
        if (ownHull && (gapNow < HoldGap() - 5.0f || (!g.hold.at && gapNow < HoldGap() + 60.0f)))
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
            if (space::RayHitDistance(g.hold.pos, probe, &target, 1) < 0 && !space::SightBlocker(p, tc.pos, target))
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
            if (gapNow >= 0 && gapNow < g_set.staticHoldMineGap && lateral > 3.0f)
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
    float radius = 0;
    for (const space::Obstacle& o : space::Obstacles(tc.pos, 50))
        if (o.id == target)
            radius = o.radius;
    radius = std::max(radius, 30.0f);
    const float rel[3] = {k.pos[0] - tc.pos[0], k.pos[1] - tc.pos[1], k.pos[2] - tc.pos[2]};
    const float d = Len3(rel);
    if (g.pass.target != target)
    {
        g.pass.target = target;
        g.pass.phase = PassPhase::In;
        g.pass.since = g.time;
        events::Push("autopilot static-pass target=%u radius=%.0f dist=%.0f", target, radius, d);
    }
    ai::Self self;
    const bool shieldFailed = ai::ReadSelf(self) && self.shield >= 0 && self.shield <= 0.05f;
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
        if (!(g.combat.chosen && g.target) && !holdBig && !dwell && !holdStatic &&
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
    const aim::Target t = aim::Current();
    const bool haveEnemy = g.target && t.valid && fl::TargetId() == g.target;

    // Chase limit: a target that stays out of gun range (fleeing, cruising away) is given up for 30 s. Never a solar: it
    // cannot run (M12 22 Oct 2026: the third Shield Generator 3.2 km off was "given up" at once whenever the ship was
    // still moving away - closing < 0 - and the ship fought endless Nomads instead until it died).
    if (haveEnemy && !g.combat.chosen)
    {
        // (not while running to a big ship's flank: out of gun range on purpose - the flank point lies >= 900 m off it,
        // and the gunboat was given up 8 s into that run, then ignored 30 s: "tactic=seek", fights +22-35 s, 21 Oct 2026)
        g.combat.outOfRange = t.inRange || g.combat.flanking ? 0 : g.combat.outOfRange + dt;
        if ((g.combat.outOfRange > 8.0f || (t.dist > 2500.0f && t.closing < 0)) && FindContact(g.target).ship)
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
    const float fleeAt = g_set.flee >= 0 ? g_set.flee : g_p.Job.flee_when_hull_damaged_percent;
    const float odds = g_set.caution >= 2 ? 1.0f : 1.6f, losing = g_set.caution >= 2 ? 0.8f : 0.6f;
    // (0.25 here left a fight the strong ship had won the run before - 4 heavy fighters, hull 0.58; wins measured as
    // low as 0.08: the line is retreat_odds, doubled at caution 2)
    const bool raceLost = race.ships > 0
                              ? race.measured && race.odds < (g_set.caution >= 2 ? 2.0f : 1.0f) * g_set.retreatOdds
                              : g_sit.threat > Strength(self) * odds;
    const bool outmatched = g_set.caution > 0 && raceLost && hull >= 0 && hull < losing;
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
    const bool hopeless = !inMission && g_set.caution > 0 && hull >= 0 &&
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
            JinkAway(k, from, dt, out, g.combat.retreat == RetreatPhase::Charge ? g_set.extendJink : 10.0f, false);
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
    // the hits stop): afterburner away from the hostiles' threat centre, jinking (a new offset of extend_jink degrees
    // every 0.7 s) with slide bursts, the guns still firing when the target crosses the cone; back to the relentless
    // pursuit once no hit came for extend_quiet seconds (min 2 s, max extend_max). Triggers: evasion >= 0.6 for 0.8 s,
    // or the shield below 35 % while being hit.
    // Under the objective mode's survival stance extend is on whatever the setting (user, 20 Oct 2026: "hull is sacred" -
    // break off while the shield is low so it regenerates; kills do not matter there; the 2020 M04 opening: the shield
    // never below 0.43): the trigger is the shield below 50 % while hit (35 % otherwise), and escorting, the run ends
    // before 2000 m from the escorted ship (missions tether to it: Benford fails beyond 2500 m of Juni).
    // ... only while the fight is being lost (user, 21 Oct 2026, M04 Freeport 4: "combat behavior leaves a lot to be
    // desired, the ship took forever to start engaging" - extend after extend away from bounty hunters it was beating,
    // the target off screen behind us; odds 0.8-6.5, the user fought that one through on batteries in 2020): the race
    // (FightOdds) lost, or without loadouts the threat above our strength. A won fight is fought; batteries cover it.
    const bool raceLosing = race.ships > 0 ? race.odds < 1.0f : g_sit.threat > Strength(self);
    const bool survivalExtend = ObjectiveOn() && g_obj.survival && raceLosing;
    const bool canExtend = g_set.evade && (g_set.extend || survivalExtend) &&
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
        // Running did not shake them (still hit after extend_patience: surrounded, faster attackers): fight instead,
        // and do not try again soon.
        const bool futile = g.tacticTime >= g_set.extendPatience && g.combat.quiet < 0.5f;
        bool leash = false; // escorting: never run out of the mission's tether to the escorted ship
        fl::Kinematics mk = {};
        float esc[3];
        if (g.mode == Mode::Escort && g.escorted && fl::PlayerKinematics(mk) && space::ObjectPos(g.escorted, esc))
        {
            const float dx = mk.pos[0] - esc[0], dy = mk.pos[1] - esc[1], dz = mk.pos[2] - esc[2];
            leash = dx * dx + dy * dy + dz * dz > 2000.0f * 2000.0f;
        }
        if ((g.combat.quiet >= g_set.extendQuiet && g.tacticTime >= 2.0f) || g.tacticTime >= g_set.extendMax ||
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
    const bool canDodge = g_set.evade && g.tactic == Tactic::Engage && !hopping;
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
    if ((g.tactic == Tactic::Engage || g.tactic == Tactic::Seek) && g.target && pressure <= 0 && self.gunRange > 0)
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
            const float way = dist - self.gunRange;
            // Far targets only (user rule; 19 Oct 2026 at the Rogue Base it cruised at 611-623 m against a 600 m gun
            // range - a 5 s charge for 20 m): at least 3 km beyond gun range, whatever the run plan says.
            if (way > drop && (fm.cruiseEngaged || way > 3000))
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
                        events::Push("autopilot approach cruise dist=%.0f gun=%.0f", dist, self.gunRange);
                }
            }
            else if (g.combat.approaching)
                events::Push("autopilot approach drop dist=%.0f speed=%.0f", dist, speed);
        }
    }
    g.combat.approaching = approach;

    // Engage (and slide dodges): lead pursuit (the nose and the reticle on the lead cross, binary throttle with
    // reverse and afterburner: the controller's Pursue) or, with steer = 1, the game's Trail.
    const bool engaging = !approach && (g.tactic == Tactic::Engage || g.tactic == Tactic::Seek ||
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
    if (engaging && g.target && g_set.steer == 0)
    {
        const space::Contact tc = FindContact(g.target);
        float spot[3];
        // (only a target that sits still or crawls: a spot in a manoeuvring gunboat's frame runs away from us - M09 22 Oct
        // 2026, the last Rheinland gunboats: station keeping at <= 60 m/s, the spot 683 -> 1204 m off in 12 s, the
        // allies made the kills; the user saw the run "losing steam")
        space::ShipMotion tm;
        const bool still = !tc.ship || !space::ShipMotionOf(g.target, tm) || tm.speed < 30.0f;
        if (tc.id && still &&
            (!tc.ship || (tc.type & (kGunboatBit | kCruiserBit | kCapitalBit)) || tc.maxHit > 20000) &&
            BlindSpot(g.target, self.gunRange, spot))
        {
            blind = true;
            g.combat.flanking = false;
            fl::Kinematics k = {};
            float to[3] = {};
            if (fl::PlayerKinematics(k))
                to[0] = spot[0] - k.pos[0], to[1] = spot[1] - k.pos[1], to[2] = spot[2] - k.pos[2];
            const float far = Len3(to);
            out.afterburn = false;
            // the last 800 m by station keeping (<= 60 m/s): spots lie close to or inside the hull envelope - the full
            // speed run rammed the hull (M13 22 Oct 2026: dead at 95 % hull; the aft spots "hit" before were likely
            // the same); the afterburner only beyond 1.5 km
            if (far > 800)
            {
                memcpy(out.steerPos, spot, sizeof(out.steerPos));
                out.pursue = false;
                out.steerTo = true;
                out.steerFullAngle = 0.35f;
                out.steerThrottle = 1.0f;
                out.steerReverse = false;
                out.afterburn = far > 1500;
            }
            else
            {
                out.pursue = true;
                out.steerTo = false;
                out.station = true;
                memcpy(out.stationPos, spot, sizeof(out.stationPos));
            }
        }
    }
    else if (g.combat.blindOn)
        g.combat.blindOn = false;
    // A static target without a blind spot: hold-and-salvo (StaticHold), else attack passes (StaticPass), else an orbit
    // round it (StaticOrbit).
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
                if (g.combat.stallAt >= 0 && Len3(mv) < 30 && g.combat.dealt <= g.combat.stallDealt + 1)
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
                g.combat.stallDealt = g.combat.dealt;
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
            // the exchange since the last frame, booked to the option flown (hull taken off it: g.combat.dealt; ours: g.combat.lost)
            if (g.combat.duelDealtSeen >= 0 && tc.maxHit > 0)
                DuelAccount(dt, std::max(0.0f, g.combat.dealt - g.combat.duelDealtSeen) / tc.maxHit,
                            std::max(0.0f, g.combat.lost - g.combat.duelLostSeen));
            g.combat.duelDealtSeen = g.combat.dealt;
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
    g.combat.burst += dt;
    const float burstOn = Pf(g_p.Gun.gun_fire_burst_interval_time, 0.2f, 5, 1.5f);
    const float burstOff = Pf(g_p.Gun.gun_fire_no_burst_interval_time, 0.0f, 3, 0.1f);
    if (g.combat.burst > burstOn + burstOff)
        g.combat.burst = 0;
    // Pursuit: the controller fires whenever the reticle sits on the lead cross within fire_cone (one gate, no gaps);
    // else (the game flies) by the lead's offset from the nose.
    out.fireCone = g_set.fireCone;
    out.stagger = g_set.stagger;
    out.fire = g_set.autoFire && (out.pursue || aligned) && (!g_set.bursts || g.combat.burst <= burstOn);
    // Guns: hold fire while a friendly or neutral ship is in the corridor (bolts spread: 60 m) or behind the target
    // within gun range (a miss flies on). Checked every 0.2 s.
    if (out.fire && g.target)
    {
        if (g.time - g.combat.lineCheckAt >= 0.2f)
        {
            g.combat.lineCheckAt = g.time;
            const space::Contact tc = FindContact(g.target);
            const float range = out.gunRange > 0 ? out.gunRange : 600.0f;
            const bool blocked =
                tc.id && LineOfFireBlocked(tc.id, tc.pos, 60.0f, std::max(0.0f, range - tc.distance), 90.0f);
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
        }
    }
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
    const std::vector<space::Weapon> weaponsNow =
        g_set.missiles && g.target ? space::PlayerWeapons() : std::vector<space::Weapon>();
    // Torpedo launchers (HpTorpedo..) fire on their own key (USER_LAUNCH_TORPEDOS): never the missile planner's launcher
    // (22 Oct 2026: the torpedo, 2424 hull damage, was "the launcher" - its flight planned, the missile key tapped, not one
    // torpedo ever fired; M12's 45 000-point shield generators took minutes by guns alone under 30 Nomads).
    const space::Weapon* torp = nullptr;
    for (const space::Weapon& w : weaponsNow)
    {
        if (!w.homing || w.ammo <= 0)
            continue;
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
        // Our damage rate: measured once 8 s of firing make it reliable; before that the guns' hull damage per second
        // (loadout) at a 35 % hit rate (M02 run: 146 measured of 408 - was: 3 dps after 3 s, hits not yet landed,
        // made every fight "outmatched", 18 Oct 2026).
        const float ourDps = OurHullDps();
        const float hp = c.id && c.maxHit > 0 ? c.maxHit * std::max(0.0f, c.health) : 0.0f;
        // Outmatched from the fight as measured, not a head count (was: threat in fighter units > our health
        // fractions - four rogues were "outmatched" at full hull before a shot landed, M02 18 Oct 2026, the user: "the
        // situation did not call for it"): the guns' time to kill every hostile around (their hull left / our damage
        // rate) against our time to die at the rate we lose shield + hull. Unknown until 3 s of both: no launch.
        float hostileHp = 0;
        for (const space::Contact& h : Contacts())
            if (h.ship && h.attitude < 0 && h.maxHit > 0 && (h.targetsPlayer || h.distance < 3000))
                hostileHp += h.maxHit * std::max(0.0f, h.health);
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
        const bool deadFirst = hp > 0 && ourDps * flight + inFlight >= hp;
        // A seeker can pick up what crosses its cone: a wide corridor, 400 m past the target, plus the blast. At a static
        // target close by (a solar whose hull the ray test sees within 300 m) the round cannot miss or fly on past the
        // hull: the corridor is the blast plus 30 m and ends at the target (session 5, M13 St03: the wide corridor - the
        // allies around and the sphere's shell "behind" each generator - held 90 % of the salvos at the hold).
        const float closeHull = c.id && !c.ship ? HullGap(self3.pos, c.pos) : -1.0f;
        const bool staticClose = closeHull >= 0 && closeHull < 300.0f;
        const bool friendlyNear =
            c.id && (staticClose ? LineOfFireBlocked(c.id, c.pos, 30.0f + launcher->blastRadius, 0.0f, 0.0f)
                                 : LineOfFireBlocked(c.id, c.pos, 200.0f + launcher->blastRadius, 400.0f,
                                                     400.0f + launcher->blastRadius));
        // The economy (user, 18 Oct 2026: the player fires cheap missiles that refill in play freely - "tractor all"
        // - and keeps costly or scarce ones for need): free use when the missile is cheap for the player (price <= 1 %
        // of net worth: Javelin 25 cr) and the stock plus what the hostiles here are expected to drop (the game's
        // loot rule over what their launchers carry) stays >= 10; otherwise only when the fight needs one.
        const MissileEconomy& eco = Economy();
        const bool freeUse = eco.price >= 0 && eco.cheap && eco.stock + eco.refill >= 10;
        // The missile's starting velocity: ours plus the muzzle velocity along the nose - within the seeker's half cone
        // of the line to the target, or the seeker never sees it (a sideways drift throws the missile off the line).
        float fwd[3];
        geom::Forward(fwd, self3.rot);
        const float v0[3] = {self3.vel[0] + fwd[0] * launcher->muzzleVelocity,
                             self3.vel[1] + fwd[1] * launcher->muzzleVelocity,
                             self3.vel[2] + fwd[2] * launcher->muzzleVelocity};
        const float los[3] = {c.pos[0] - self3.pos[0], c.pos[1] - self3.pos[1], c.pos[2] - self3.pos[2]};
        const auto dot3 = [](const float* a, const float* b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; };
        const float v0n = std::sqrt(dot3(v0, v0)), losn = std::sqrt(dot3(los, los));
        // seeker_fov_deg is a half-angle (the game keeps its cosine); 80 % of it as the margin
        const float halfCone = (launcher->seekerFovDeg > 0 ? launcher->seekerFovDeg : 35.0f) * 3.14159265f / 180;
        // (a static hull within 300 m with the nose on it: the round flies into the hull whatever the seeker sees - s9m, M12
        // gen 3: the hold's lateral slide put a torpedo's start velocity off the cone at every other refire, on-line=0 with
        // aligned=1 at a 48-51 m gap)
        const bool onLine =
            (staticClose && aligned) || (v0n > 1 && losn > 1 && dot3(v0, los) / (v0n * losn) > cosf(halfCone * 0.8f));
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
        const auto blastSafe = [&](const space::Weapon& w, float fallback, bool flank)
        {
            if (gapLine < 0)
                return t.dist >= fallback;
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
            return true;
        };
        const float maxDist = launcher->seekerRange > 0 ? launcher->seekerRange : 1000.0f;
        constexpr float kGoodFlight = 1.5f;
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
        const float gate =
            space::ShipMotionOf(g.target, tmo) ? std::min(kGoodFlight, MissileGate(tmo.arch)) : kGoodFlight;
        const float flightDrop = g.missiles.prevFlightAt > 0 && g.time > g.missiles.prevFlightAt
                                     ? (g.missiles.prevFlight - flight) / (g.time - g.missiles.prevFlightAt)
                                     : 0.0f;
        g.missiles.prevFlight = flight;
        g.missiles.prevFlightAt = g.time;
        const bool improving = flightDrop > 0.4f && flight > 0.8f && g.missiles.run > 0 && g.missiles.run < 3.0f;
        // (the flight-time gate is for targets that dodge: a static one never does - from a hold the flight reads ~2.8 s,
        // the round starting at the muzzle velocity, and no missile ever went)
        const bool ready = aligned && onLine && (staticClose || (flight <= gate && !improving)) &&
                           blastSafe(*launcher, minDist, want && launcher->elapsed >= launcher->refire) &&
                           t.dist <= maxDist && shieldDownAtImpact;
        // At a hold the salvo should go out at every refire: what holds it back, every 2 s (tuning the hold-and-salvo)
        const bool holdingHere = g.hold.on && g.hold.target == g.target;
        if (holdingHere && g.hold.at && torp && torp->elapsed >= torp->refire)
        {
            static Throttle saidAt;
            if (saidAt.Due(g.time, 2))
            {
                int near = 0;
                for (const space::Contact& o : Contacts())
                    if (o.ship && o.id != g.target && o.distance < 150.0f)
                        ++near;
                events::Push(
                    "autopilot salvo-check target=%u dist=%.0f gap=%.0f nose-gap=%.0f aligned=%d on-line=%d "
                    "shield-down=%d friendly=%d ships-150m=%d hp=%.0f want=%d ready=%d flight=%.2f dead-first=%d "
                    "hold-gap=%.0f torp-need=%.0f to-point=%.0f",
                    g.target, t.dist, gapLine, gapNose, aligned ? 1 : 0, onLine ? 1 : 0, shieldDownAtImpact ? 1 : 0,
                    friendlyNear ? 1 : 0, near, hp, want ? 1 : 0, ready ? 1 : 0, flight, deadFirst ? 1 : 0, HoldGap(),
                    std::max(20.0f, torp->blastRadius + std::max(5.0f, space::PlayerRadius()) + 10.0f),
                    geom::Dist(g.hold.pos, self3.pos));
            }
        }
        // Torpedoes at big or static targets with hit points for more than two of them, on the same aim, line and
        // shield-at-impact checks, whenever a torpedo launcher has refired.
        const float torpFloor = 2 * (torp ? torp->hullDamage : 0.0f);
        if (torp && torp->elapsed >= torp->refire && (big || !c.ship) && hp > torpFloor && aligned && onLine &&
            // (beyond the target's own radius + 150 m: from a blind spot inside its hull envelope the torpedo struck the
            // hull beside us - M13 22 Oct 2026, 100 % -> 67 % hull, the second one killed the ship)
            !friendlyNear && shieldDownAtImpact &&
            blastSafe(*torp, std::max({150.0f, torp->blastRadius * 4, t.radius + 150.0f}), true) &&
            t.dist <= (torp->seekerRange > 0 ? torp->seekerRange : 1500.0f) &&
            // (no other ship within 150 m: a torpedo that meets one at the muzzle bursts beside us)
            [&]
            {
                for (const space::Contact& o : Contacts())
                    if (o.ship && o.id != g.target && o.distance < 150.0f)
                        return false;
                return true;
            }() &&
            pathClear())
        {
            out.torpedo = true;
            events::Push("autopilot torpedo-launch target=%u dist=%.0f hp=%.0f dmg=%.0f ammo=%d", g.target, t.dist, hp,
                         torp->hullDamage, torp->ammo);
        }
        if (want && !deadFirst && ready && !launcherIsTorp && pathClear())
        {
            out.missile = true;
            g.missiles.tapFrame = g.frames;
            g.missiles.lastLaunchDist = t.dist;
            g.missiles.lastLaunchFlight = flight;
            g.missiles.lastLaunchClosing = t.closing;
            g.missiles.lastLaunchGeom = MissileGeometry(g.target, *launcher, flight, t.radius);
            // inside a shield's offline window: the next one at the launcher's refire (the burst)
            // ...and at a static target we hold at (session 8, 23 Oct 2026, M12: the pilot's missile_launch_interval_time,
            // 20-45 s for military pilots, let one 4-missile salvo go per 50 000-point generator while the torpedoes went
            // every 2 s; the user: "all weapons, all missiles and all mines until each generator was destroyed")
            g.missiles.timer = burst || (holdingHere && !c.ship)
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
        else if (want && !deadFirst && !launcherIsTorp && g.tactic == Tactic::Engage && g.combat.evasion < 0.05f &&
                 !holdingHere && t.dist > minDist)
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
