// Advanced Autopilot - combat's defence: damage taken and repairs (nanobots, shield batteries) in every mode, and countermeasures.
// Shared state: autopilot_impl.h.
#include "autopilot_impl.h"

namespace autopilot
{

// Damage taken and repairs (nanobots / shield batteries), every frame in EVERY mode (hull is
// sacred - M04 objective mode died in `hold` beside capital ships, shield 0.92 -> 0.20 then hull 1.00 -> 0.03 in 6 s,
// a nanobot and a battery aboard: the repairs ran only inside a fight). Outside one the situation (attackers) is read
// twice a second here; a fight reads it itself.
void DamageAndRepairs(float dt, Out& out, ai::Self& self)
{
    // Damage taken, every frame (was: only while an enemy was selected - between targets, looting and fleeing the
    // pressure stayed at its last value, so repairs saw hits that had stopped and `lost` missed damage):
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
    // after the branch's early return - frozen at 0 for a whole flee, so the cruise run never started)
    g.combat.quiet = pressure > 0 ? 0.0f : g.combat.quiet + dt;
    g.stats.shield = self.shield;
    g.stats.lossRate = (hpTop - hpNow) / 2.0f; // points per second over the last 2 s
    // The largest single hit (frame-to-frame drop) of the last 2 s: slow, heavy weapons empty a shield in one shot.
    g.stats.maxHit = 0;
    for (size_t i = 1; i < g.combat.hp.size(); ++i)
        g.stats.maxHit = std::max(g.stats.maxHit, g.combat.hp[i - 1].second - g.combat.hp[i].second);
    // Evidence for sudden deaths (M04 West Point: shield 359 to dead in 0.45 s beside capital ships, nothing
    // logged to say what hit): a frame-to-frame loss above 20 % of our total names the nearest hostiles - toughness
    // (hull points: fighters ~1-3k, capital ships far more), distance, whether they target us.
    // The explosions that reach us (the M13 burst probe: our torpedo cost us its full 2,424 with its last
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
        // Nanobots ("shields collapsed and hull sustained minor damage, but chances are very low for
        // additional heavy damage? hold off"): when the hull is low only if the damage predicted for the next moment
        // gets past the shield into the hull (>= 5 % of it); critical (< 35 %) only while still under fire.
        const float predictedHullLoss = std::max(0.0f, predicted - shieldLeft);
        // Survival model ("even in frugality mode the ship must survive" - decide from the odds):
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
        // further out is better met by waiting until the gap takes a whole unit (3 nanobots spent at hull
        // 0.71 for +317 each, none left for the burst that killed the ship 30 s later).
        static const float kReact = 0.75f;
        float missileSoon = 0;
        for (const space::Missile& m : space::Missiles())
            if (m.targetsPlayer && m.closing > 1 && m.distance / m.closing < kReact)
                missileSoon += m.hullDamage;
        const float worstSoon = std::max(g.stats.lossRate, potential * hitRate) * kReact + missileSoon + bigHit;
        // The gun rounds already in flight that will hit ("track enemy projectiles, model the damage
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
        // The frugal default (repair 1), the user's rules, the damage model judging "imminent":
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
        // "near depletion" = the fitted shield's own offline_threshold (the game's value - 0.15 for most,
        // st_equip.ini), 0.15 when unknown
        const float offline = space::ShieldOfflineThreshold(self.shieldArch);
        const bool nearDepletion = shieldFailed || (self.shield >= 0 && self.shield <= (offline > 0 ? offline : 0.15f));
        // Hull is sacred ("the player HATES to see hull damage; regenerative shields are the
        // expendable health pool"): a battery BEFORE the shield fails - when the damage expected over the horizon would
        // take what is left above the offline threshold (a failed shield reboots and absorbs nothing) - and at a small
        // predicted hull loss (2 % of the hull; was 8 %).
        // ... within the REACTION time, not the 2.5 s horizon (M04 vs the 2020 run: the user spent 8
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
        // +340 in game with regeneration) - used only when at least half of it fits.
        // One press fills the bar with as many units as the gap needs (in game: a ~895 shield gap took
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
        // How many units ("one battery per press, pressing again only while still needed" ... "scale it
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
        // A reserve for the rest of the leg (M12: two deaths in Omicron Minor with nanobots 0 and batteries 0 -
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
                // gate m13afk1)
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
        // The nearest one times the drops and the dodge (was: the first listed).
        if (!nearest || m.distance < nearest->distance)
            nearest = &m;
    }
    if (!nearest)
        return;
    // ...and a cruise disruptor whenever the engine charges or cruises (M12: a hop between
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
    // police missiles killed it); after 3 drops a missile still homing on us is not going to be diverted.
    const int cmAmmo = space::PlayerCountermeasures();
    int& drops = g.missiles.cmDrops[nearest->id];
    // A budget (M12: 3 drops per missile spent all 50 in the lair - in Omicron Minor every charge
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
