// Advanced Autopilot - target choice, the fight as a pilot reads it, odds, missile economy and geometry. Part of autopilot.cpp until 21 Oct 2026; shared state and declarations in
// autopilot_impl.h, the module API in autopilot.h.
#include "autopilot_impl.h"

namespace autopilot
{
// ------------------------------------------------------------------ target choice
// The next target from the game's own data, the way a good pilot reads the fight: hostile ships only (attitude),
// within reach (engage radius around us, or around the escorted ship), not given up; ships attacking us first, then
// fighters (what the guns kill fastest; big ships only when nothing else is left or bigShips), damaged hulls,
// distance. The current target keeps a bonus so the choice does not flicker. (The ship class bits: top of the file.)

Situation g_sit;
void ResetSituation() { g_sit = Situation(); }

static float ClassWeight(u32 type)
{
    if (type & kCapitalBit)
        return 10;
    if (type & kCruiserBit)
        return 6;
    if (type & kGunboatBit)
        return 3;
    if (type & (kTransportBit | kFreighterBit))
        return 0.5f;
    return 1;
}

Situation Assess();
// The fight read anew (counted: "an assessment after the enemy was lost" decides the end of a fight).
// The last fight's subject (the friendly ship the hostiles went for), kept past the fight's end (the State resets at a
// stop): where a pilot at rest looks first (user + 2020 footage, M01b 19 Oct 2026: after the last Rogue the player
// turned to the prison ship and held there, level, while its crew talked).
u32 g_subjectId = 0;
ULONGLONG g_subjectTick = 0; // last seen as the subject (GetTickCount64)

void Reassess()
{
    g_sit = Assess();
    ++g.combat.assessments;
    if (g_sit.protectedId)
    {
        g_subjectId = g_sit.protectedId;
        g_subjectTick = GetTickCount64();
    }
}
u32 PickTarget()
{
    Reassess();
    return g_sit.best;
}

// Our hull damage rate: measured once 8 s of firing make it reliable; before that the guns' hull damage per second
// (loadout) at a 35 % hit rate (M02 run: 146 measured of 408 - was: 3 dps after 3 s, hits not yet landed, made every
// fight "outmatched", 18 Oct 2026).
float OurHullDps()
{
    float gunDps = 0;
    for (const space::Weapon& w : space::PlayerWeapons())
        if (w.active && !w.homing && w.ammo < 0 && w.refire > 0)
            gunDps += w.hullDamage / w.refire;
    return g.stats.firing > 8 ? g.combat.dealt / g.stats.firing
                              : std::max(g.stats.firing > 3 ? g.combat.dealt / g.stats.firing : 0.0f, 0.35f * gunDps);
}

Situation Assess()
{
    Situation sit;
    const std::vector<space::Contact> all = Contacts();
    const float* center = nullptr;
    float escortPos[3];
    if (g.mode == Mode::Escort && g.escorted)
        for (const space::Contact& c : all)
            if (c.id == g.escorted)
            {
                memcpy(escortPos, c.pos, sizeof(escortPos));
                center = escortPos;
            }
    // under the objective mode's survival stance only close attackers count (mission-pointed targets keep their own
    // reach below: M04's Battleship Unity is still fought)
    const float radius = std::min(g.mode == Mode::Escort ? g_set.escortRadius : g_set.engageRadius,
                                  ObjectiveOn() && g_obj.survival ? kSurvivalReach : 1e30f);
    ai::Self self;
    ai::ReadSelf(self);
    const float gunRange = self.gunRange > 0 ? self.gunRange : 600.0f;
    // The area's encounter level against the player's rank: hostiles in zones above the player's level carry better
    // equipment than their class shows (+25 % weight per level above).
    space::Area area;
    space::ReadArea(area);
    sit.areaLevel = area.level;
    sit.rank = area.rank;
    const float levelFactor = (area.level > 0 && area.rank >= 0)
                                  ? 1.0f + 0.25f * static_cast<float>(std::max(0, area.level - area.rank))
                                  : 1.0f;
    // Line of sight to each candidate through the game's physics (space::SightBlocker: the real hulls). A hostile
    // behind or inside something solid is not a target until it is in sight (user, 19 Oct 2026: M01b's Rogue Base
    // scrambles its fighters from inside - the ship rammed the base chasing them in their bays, under its guns; the
    // rule is general: a station, planet, platform or rock between us).
    fl::Kinematics me = {};
    const bool haveMe = fl::PlayerKinematics(me);
    std::map<u32, int> protectedVotes; // friendly ship -> hostiles targeting it
    // The mission-first target model (user, 19 Oct 2026): what the mission points the player at - its marks and the
    // objective's target objects, the player's own information (host API v10) - ranks above everything the autopilot
    // infers. Hostile ones only (a marked friendly is someone to protect, not a target). Cross-checked with the game's
    // data: a mission solar is a target only if it can be destroyed (finite hit points) by guns in reasonable time.
    u32 missionIds[32];
    const int missionN = MissionObjectIds(missionIds, 32, all);
    const auto missionPointed = [&](u32 id)
    { return std::find(missionIds, missionIds + missionN, id) != missionIds + missionN; };
    u32 best = 0;
    float bestScore = 1e30f, weightSum = 0;
    std::vector<std::pair<float, u32>> loot; // (worth, id)
    // Dangerous attackers outrank the marks too (user, 22 Oct 2026: M13's generators fought under four Nomad fighters -
    // the shield never back, nanobots 12 -> 5 in 10 s, dead; speedrun route M13 "Nomads: fighters first", the 2020
    // playthrough clears the fighters before working a generator): a ship attacking us within 1.2 km while our shield is
    // down, until it is back above half (hysteresis: a 3-attacker rule flipped the target every 1-2 s in M09 and kept us
    // on its endless reinforcements for minutes - the marks must still win while the shield holds).
    int closeAttackers = 0;
    for (const space::Contact& c : all)
        if (c.ship && c.targetsPlayer && c.distance < 1200 && c.health > 0)
            ++closeAttackers;
    static bool shieldDown = false;
    if (closeAttackers == 0 || self.shield >= 0.5f)
        shieldDown = false;
    else if (self.shield <= 0.05f)
        shieldDown = true;
    // ...and only when the race is lost (user, 22 Oct 2026: "inside the Dyson Sphere you can bumrush the generators and
    // ignore the fighters completely"): the nearest mark's hit points over our damage rate against what we have left
    // (hull, half the shield, nanobots) over the fight's loss rate - attackers first only when the mark would take more
    // than 60 % of our time to die.
    bool pressed = false;
    if (shieldDown)
    {
        float markHp = -1, markDist = 1e30f;
        for (const space::Contact& c : all)
            if (missionPointed(c.id) && c.maxHit > 0 && c.health > 0 && c.distance < markDist)
                markHp = c.maxHit * c.health, markDist = c.distance;
        const float dps = OurHullDps();
        const float lossRate = g.stats.engaged > 3 ? g.combat.lost / g.stats.engaged : 0.0f;
        const float left = std::max(0.0f, self.hull) + 0.5f * std::max(0.0f, self.shield) + 0.03f * self.nanobots;
        const float toKill = markHp > 0 && dps > 0 ? markHp / dps : 1e9f;
        const float toDie = lossRate > 0 ? left / lossRate : 1e9f;
        pressed = markHp < 0 || toKill > 0.6f * toDie;
        static bool logged = false;
        if (pressed != logged)
            events::Push("autopilot pressed=%d mark-kill=%.0fs die=%.0fs attackers=%d", pressed ? 1 : 0, toKill, toDie,
                         closeAttackers);
        logged = pressed;
    }
    for (const space::Contact& c : all)
    {
        if (c.type & kLootBit)
        {
            // Worth taking: within 3 km (the Loot behaviour flies there), the hold takes some of it, worth at least
            // loot_min_value at the base price; the more valuable and the nearer, the sooner.
            if (c.distance < 3000 && c.lootFits != 0 && c.lootValue >= g_set.lootMinValue)
            {
                loot.push_back({(c.lootValue + 1.0f) / (1.0f + c.distance / 1000.0f), c.id});
                if (self.tractorRange > 0 && c.distance < self.tractorRange)
                    ++sit.lootInReach;
                if (c.distance < sit.lootNearest)
                {
                    sit.lootNearest = c.distance;
                    memcpy(sit.lootNearestPos, c.pos, sizeof(sit.lootNearestPos));
                }
            }
            continue;
        }
        // Hostile solars our guns can destroy are targets too (M01b 19 Oct 2026: "Destroy the Weapon Platforms" - combat
        // took ships only, said "no enemies left" and rested in front of a platform until it killed the ship): a
        // targetable solar of a hostile faction (the game's attitude) whose hull left our damage rate takes down within
        // 90 s - a weapon platform, not an armoured base ("We'll need torpedoes to take it out").
        bool solar = false;
        const bool pointed = missionPointed(c.id);
        // The objective says "Destroy ..." and the mission marks it: a target whatever its attitude, unless friendly (M09 22 Oct
        // 2026: the four Shield Generators are neutral mission satellites - skipped, the fight went on at endless
        // reinforcements; speedrun note A6: marked neutrals are attacked before they turn). Solars only: a neutral marked
        // ship is someone the mission is about, not a target (M08 22 Oct 2026: "Destroy the platforms" also marks Tekagi's
        // transport, the one to capture - shot once the platforms were given up; mission failed)
        const float hostileCut = pointed && !c.ship && ObjectiveOn() && g_obj.fightObjective ? 0.3f : -0.6f;
        if (!c.ship)
        {
            // Opportunistic: down in <= 90 s. Pointed at by the mission: up to 10 minutes of guns (a capital-class
            // solar is the mission's business; beyond that it needs other means - torpedoes, the script). Never one
            // with the game's "indestructible" hit points (1e36).
            float att = 0;
            const float dps = OurHullDps();
            const float cap = pointed ? 600.0f : 90.0f;
            // a mark the "Destroy ..." objective names: its attitude may not read (a spawned mission satellite: M09's
            // generators) and it may be flagged untargetable - the objective still asks for it
            const bool forced = hostileCut > 0;
            const bool attOk = space::AttitudeToPlayer(c.id, att) || (forced && (att = 0, true));
            if ((!c.targetable && !forced) || c.health <= 0 || c.maxHit <= 0 || c.maxHit >= 1e30f ||
                c.distance > std::max(radius, pointed ? kBattleReach : 3000.0f) || !attOk || att > hostileCut ||
                dps <= 0 || c.maxHit * c.health / dps > cap)
                continue;
            solar = true;
        }
        else if (c.attitude > hostileCut || c.health == 0)
            continue;
        // The threat counts every hostile ship near us, given up or not: its class, scaled by its real toughness
        // against ours (square root: heavy freighters are not warships), attacking or not, hull left.
        if (!solar && c.distance < std::max(radius, 3000.0f))
        {
            float tough = 1;
            if (c.maxHit > 0 && self.hullMax > 0)
                tough = Clampf(sqrtf(c.maxHit / self.hullMax), 0.5f, 2.5f);
            const float w =
                ClassWeight(c.type) * tough * levelFactor * (c.targetsPlayer ? 1.5f : 1.0f) * std::max(0.25f, c.health);
            sit.threat += w;
            weightSum += w;
            for (int i = 0; i < 3; ++i)
                sit.center[i] += c.pos[i] * w;
            ++sit.hostiles;
            sit.attackers += c.targetsPlayer ? 1 : 0;
            sit.nearest = std::min(sit.nearest, c.distance);
        }
        if (g.combat.ignored.count(c.id))
            continue;
        // a target the mission keeps alive at a hull floor (Act_Invulnerable) cannot lose more: not a target while there
        // (still part of the threat above). M04 Benford's cruisers sit at 0.4 - the ship held still shooting one.
        // (not a mark a "Destroy ..." objective names: M10's experimental battleships sit at 0.99 and the script wants one
        // hit - its damage starts the chain; 22 Oct 2026)
        if (space::AtInvincibleFloor(c.id, c.health) && !(pointed && ObjectiveOn() && g_obj.fightObjective))
            continue;
        const u32 blocker = haveMe && c.distance < 8000 ? space::SightBlocker(me.pos, c.pos, c.id) : 0;
        // Hidden but far (beyond 1.5x gun range): still a target - the approach flies round the obstacle and it comes
        // into view on the way (M03 Battle of Willard, 20 Oct 2026: bombers 7 km out behind the station were skipped,
        // combat ended at once, the objective mode restarted it in a loop until they came within 3 km).
        if (blocker && c.distance < std::max(600.0f, gunRange) * 1.5f)
        {
            ++sit.hidden;
            if (c.distance < 3000)
                ++sit.hiddenNear;
            if (blocker == space::kBlockerSolar)
                sit.behindStation.push_back(c.id);
            continue;
        }
        float reach = c.distance;
        if (center)
        {
            const float dx = c.pos[0] - center[0], dy = c.pos[1] - center[1], dz = c.pos[2] - center[2];
            reach = sqrtf(dx * dx + dy * dy + dz * dz);
        }
        // A hostile fighting our side (its target is us or any friendly object - ship, station, platform) is part of
        // the player's battle out to kBattleReach (M01b, 18 Oct 2026: the prison ship's attackers fought the King 9 km
        // away - "You fled the battle"; M02 Pueblo: Rogues attacking the station beyond 3 km left combat idle while the
        // objective said "Help defend" - the rule took friendly ships only).
        // (a station's or platform's attitude comes from its reputation: Contact::attitude is ships only - bombers
        // attacking Willard station 7 km out were not part of the battle, M03 20 Oct 2026)
        bool fightsOurSide = c.targetsPlayer;
        // (survival: the battle around is not ours - only what attacks us; friendlies there may die by script)
        if (!fightsOurSide && c.targetId && !(ObjectiveOn() && g_obj.survival))
            for (const space::Contact& f : all)
                if (f.id == c.targetId && FriendlyContact(f))
                {
                    fightsOurSide = true;
                    ++protectedVotes[f.id];
                    break;
                }
        // Escort mode keeps its own area (the escorted ship's escort_radius: missions fail when the player leaves the
        // escorted ship - M02 step 14 "You ran from the battle", 18 Oct 2026); the battle reach is combat mode's.
        const float battle = g.mode == Mode::Escort ? radius : std::max(radius, kBattleReach);
        if (reach > (fightsOurSide || pointed ? battle : radius))
            continue;
        float score = c.distance;
        // Survival: a big ship the mission marks is scenery, not a target (user, 21 Oct 2026, M04: "the player targets
        // a stationary cruiser and occasionally holds position, even though it would make more sense to continue
        // targeting fighters" - Navy cruiser LNS Twilight, 42 k hull, picked over 3 attacking fighters at 147-553 m:
        // the mark's -1200 and no big-ship penalty). A big ship the objective really asks for comes as the objective
        // mode's chosen target (ObjectiveDecide, Fight verb: M04's "Destroy the Battleship Unity"), not this ranking.
        const bool big = (c.type & (kGunboatBit | kCruiserBit | kCapitalBit)) != 0;
        const bool pointedHere = pointed && !(big && ObjectiveOn() && g_obj.survival);
        if (pointedHere)
            // the mission's targets first (above attackers: an attacker close by still competes); under a "Destroy ..."
            // objective far first (M09 22 Oct 2026: 100 s on endless reinforcement fighters, the marked generators untouched,
            // the area nag failed the mission) - the objective is the marks, the rest is noise
            score -= ObjectiveOn() && g_obj.fightObjective ? 30000 : 1200; // (3000 lost to 6 km: M09)
        if (c.targetsPlayer || (g.escorted && c.targetId == g.escorted))
            score -= 800; // attackers (of us or the escorted ship) first
        if (pressed && c.ship && c.targetsPlayer && c.distance < 1200 && !pointedHere)
            score -= ObjectiveOn() && g_obj.fightObjective ? 30000 : 1200; // as high as a mark: the nearer wins
        if (c.type & kFighterBit)
            score -= 300;
        else if (!g_set.bigShips && !pointedHere && big)
            score += 5000; // out of a fighter's league: last (unless the mission points at it: M03's gunboats)
        else if (!g_set.bigShips && (c.type & (kTransportBit | kFreighterBit)))
            score += 1500;
        if (c.health > 0)
            score -= (1.0f - c.health) * 600; // finish damaged ships
        if (c.distance < gunRange)
            score -= 200; // already within the guns' reach
        if (c.id == g.target)
            // stay on the current target - a static one being held is finished first (session 5: the M13 St03 run hopped
            // between generators 2 km apart as the nearer changed, each switch a new approach; the runners finish the one
            // they started), unless the race is lost (pressed)
            score -= !c.ship && !pressed && g.hold.on && g.hold.target == c.id ? 5000 : 400;
        if (score < bestScore)
        {
            bestScore = score;
            best = c.id;
        }
    }
    if (weightSum > 0)
        for (int i = 0; i < 3; ++i)
            sit.center[i] /= weightSum;
    int votes = 0;
    for (const auto& v : protectedVotes)
        if (v.second > votes)
            votes = v.second, sit.protectedId = v.first;
    std::sort(loot.begin(), loot.end(), [](const auto& x, const auto& y) { return x.first > y.first; });
    for (size_t i = 0; i < loot.size() && i < 16; ++i)
        sit.loot.push_back(loot[i].second);
    sit.best = best;
    return sit;
}

// Our own strength in the same units (a full starter fighter ~ 3 fighters' worth with repairs aboard).
float Strength(const ai::Self& s)
{
    if (s.hull < 0)
        return 0;
    const float shield = s.shield > 0 ? s.shield : 0;
    return 2.0f * (s.hull + 0.5f * shield) + 0.15f * s.nanobots + 0.1f * s.batteries;
}

// The fight as a race (user, 19 Oct 2026: "consider the enemy's damage capabilities, in the context of what they're
// targeting / about to shoot at" - bursts kill faster than any hull threshold can react): the time they need to
// destroy us against the time we need to destroy them. Their side: the hostiles targeting us or within 3 km (about to),
// their hit points (hull + shield) and their gun potential (hull damage / refire of every active gun, the game's
// loadouts) times how much of it lands - measured once 3 s of the fight show it (our points lost per second), before
// that a low prior (kTheirHitPrior: the share NPC fire really lands, 1-15 % measured; the low end so that a winnable
// fight is never left on a guess). Our side: our points (+ half the consumables aboard) and our damage rate
// (OurHullDps: measured, before that 35 % of our guns' potential). odds = their time / ours: < 1 we lose the race.
// (Equal aim was assumed first: NPCs land 1-15 % of their potential, the autopilot ~60 %, so 3 level-5 pirates read
// 0.03 and the run from them died - 19 Oct 2026.) Re-read at most twice a second; 1e9 with nobody hostile.
const Odds& FightOdds(const ai::Self& self)
{
    static Odds o;
    static ULONGLONG at = 0;
    const ULONGLONG now = GetTickCount64();
    if (now - at < 500)
        return o;
    at = now;
    o = Odds();
    for (const space::Weapon& w : space::PlayerWeapons())
        if (!w.homing && w.active && w.refire > 0)
            o.ourPot += w.hullDamage / w.refire;
    o.ourHp = std::max(0.0f, self.hull) * self.hullMax + std::max(0.0f, self.shield) * self.shieldMax +
              self.nanobots * 600.0f * 0.5f + self.batteries * 300.0f * 0.5f; // misc_equip hit_pts, half: spent late
    for (const space::Contact& c : Contacts())
    {
        if (!c.ship || c.attitude > -0.6f || c.health == 0 || !(c.targetsPlayer || c.distance < 3000))
            continue;
        float pot = 0;
        for (const space::Weapon& w : space::ShipWeapons(c.id))
            if (!w.homing && w.active && w.refire > 0)
                pot += w.hullDamage / w.refire;
        if (pot <= 0)
            continue;
        float sh = 0, shMax = 0;
        bool up = false;
        space::ShieldOf(c.id, sh, shMax, up);
        o.theirPot += pot;
        o.theirHp += std::max(0.0f, c.health) * c.maxHit + (up ? sh : 0.0f);
        ++o.ships;
    }
    if (o.theirPot <= 0)
        return o;
    // (a floor: 8 s of firing before the first confirmed hit measured 0 and read as "unarmed" - the run from 3 pirates
    // the autopilot normally beats died, 19 Oct 2026)
    // and for the first 20 s of firing at least the prior: our measured rate counts hull points only while the targets'
    // shields take the first hits (3 pirates read 16/s at 8 s, the race 0.26 against retreat_odds 0.25 - a fight won)
    o.ourDps = std::max(OurHullDps(), (g.stats.firing < 20 ? 0.35f : 0.1f) * o.ourPot);
    // (8 s: NPC fire comes in bursts - 3 pirates measured over 4 s read 6.2, over 8 s 0.16, and the fight was won the
    // run before)
    o.measured = g.stats.engaged > 15 && g.combat.lost > 0 &&
                 g.combat.totalHp > 0; // (8 s: a burst read 0.03 in a fight won - 2 heavies)
    o.theirDps = o.measured ? g.combat.lost / g.stats.engaged * g.combat.totalHp : o.theirPot * kTheirHitPrior;
    if (o.ourDps > 0 && o.theirDps > 0 && o.theirHp > 0)
        o.odds = (o.ourHp / o.theirDps) / (o.theirHp / o.ourDps);
    else if (o.ourPot <= 0)
        o.odds = 0; // no guns against armed ships
    return o;
}

// Hull lost over the last two seconds.
float RecentDamage(float hull)
{
    if (hull < 0)
        return 0;
    g.combat.hull.push_back({g.time, hull});
    while (!g.combat.hull.empty() && g.combat.hull.front().first < g.time - 2.0f)
        g.combat.hull.pop_front();
    float top = hull;
    for (const auto& h : g.combat.hull)
        top = std::max(top, h.second);
    return top - hull;
}

// What our missiles cost to use: the cheapest loaded homing launcher's ammo price (the game's good list), the rounds
// left in every homing launcher, the player's net worth, and the rounds the hostiles in this fight are expected to drop
// (their own launchers' ammo of the same kind under the game's loot rule, space::ExpectedDrops). Re-read every 3 s.
const MissileEconomy& Economy()
{
    static MissileEconomy e;
    static float at = -100;
    if (g.time >= at && g.time - at < 3)
        return e;
    at = g.time;
    e = MissileEconomy();
    u32 ammo = 0;
    // Not `active`: a missile launcher reads isActive 0 and still fires (M02 ship, 18 Oct 2026).
    for (const space::Weapon& w : space::PlayerWeapons())
        if (w.homing && w.ammo > 0)
        {
            e.stock += w.ammo;
            const float p = w.ammoArch ? space::Price(w.ammoArch) : -1;
            if (p >= 0 && (e.price < 0 || p < e.price))
                e.price = p, ammo = w.ammoArch;
        }
    if (e.price < 0)
        return e;
    e.worth = space::PlayerWorth();
    e.cheap = e.worth > 0 && e.price <= 0.01f * e.worth;
    for (const space::Contact& h : Contacts())
    {
        if (!h.ship || h.attitude >= 0 || !(h.targetsPlayer || h.distance < 3000))
            continue;
        int carried = 0;
        for (const space::Weapon& w : space::ShipWeapons(h.id))
            if (w.ammoArch == ammo && w.ammo > 0)
                carried += w.ammo;
        e.refill += space::ExpectedDrops(ammo, e.worth, carried);
    }
    return e;
}

static void OwnMissileNew(const space::Missile& m)
{
    State::OwnMissile o;
    o.target = m.targetId ? m.targetId : g.target;
    o.launched = g.time;
    o.damage = m.hullDamage;
    o.blast = m.blastRadius;
    // the planner's launch: the missile appears within a few frames of our tap (else fire-all / the player's own)
    o.planned = g.missiles.tapFrame && g.frames - g.missiles.tapFrame <= 5;
    if (o.planned)
    {
        o.launchDist = g.missiles.lastLaunchDist;
        o.flightPred = g.missiles.lastLaunchFlight;
        o.closing = g.missiles.lastLaunchClosing;
        o.geom = g.missiles.lastLaunchGeom;
    }
    g.missiles.ownMissiles[m.id] = o;
}

// Our missiles: watched from launch to their end; then a hit when the target lost >= 40 % of the missile's damage
// within 0.4 s of it or died, else a miss - logged with the launch conditions and the closest approach (the planner's
// feedback: user, 18 Oct 2026, "I spotted a miss: investigate and improve").
void TrackOwnMissiles()
{
    const u32 me = space::PlayerShipId();
    if (!me)
        return;
    const auto hpOf = [](u32 id)
    {
        const space::Contact c = FindContact(id);
        return c.id && c.maxHit > 0 ? c.maxHit * std::max(0.0f, c.health) : -1.0f;
    };
    std::map<u32, bool> alive;
    for (const space::Missile& m : space::Missiles())
    {
        if (m.owner != me)
            continue;
        alive[m.id] = true;
        auto it = g.missiles.ownMissiles.find(m.id);
        if (it == g.missiles.ownMissiles.end())
        {
            OwnMissileNew(m);
            it = g.missiles.ownMissiles.find(m.id);
        }
        if (const space::Contact c = FindContact(it->second.target); c.id)
        {
            const float d[3] = {c.pos[0] - m.pos[0], c.pos[1] - m.pos[1], c.pos[2] - m.pos[2]};
            it->second.closest = std::min(it->second.closest, std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]));
            // its hull in the last frame before the impact (guns' share: small) - the last LIVE reading: the frame a
            // missile kills reads 0 (21 Oct 2026: 400+ such kills had been counted as misses, "1 of 6 hit")
            if (const float h = hpOf(c.id); h > 0)
                it->second.targetHp = h;
        }
    }
    for (auto it = g.missiles.ownMissiles.begin(); it != g.missiles.ownMissiles.end();)
    {
        if (alive.count(it->first))
        {
            ++it;
            continue;
        }
        g.missiles.ownGone.push_back({it->first, it->second, g.time});
        it = g.missiles.ownMissiles.erase(it);
    }
    for (auto it = g.missiles.ownGone.begin(); it != g.missiles.ownGone.end();)
    {
        if (g.time - it->gone < 0.4f)
        {
            ++it;
            continue;
        }
        const State::OwnMissile& m = it->m;
        const float now = hpOf(m.target);
        // A hit on shields leaves the hull alone (the shield takes it): the missile's end within its blast radius plus a
        // fighter's size of the target's centre counts too (18 Oct 2026: "misses" that ended 7-16 m away, Javelin
        // blast 16 m).
        // (dead now - gone or hull 0 - with the missile ending near it: the kill was the missile's)
        const bool hit =
            m.targetHp > 0 && ((now <= 0 && m.closest <= m.blast + 30.0f) ||
                               (now > 0 && m.targetHp - now >= 0.4f * m.damage) || m.closest <= m.blast + 15.0f);
        ++(hit ? g.missiles.ownMissileHits : g.missiles.ownMissileMisses);
        // the run-in's gate for this target type learns from the planner's own launches
        if (m.planned && m.geom.tArch && m.flightPred > 0)
        {
            float& gate = g_missileGate.emplace(m.geom.tArch, 1.5f).first->second;
            const float was = gate;
            gate = hit ? std::min(1.5f, gate + 0.05f) : std::max(0.6f, std::min(gate, m.flightPred - 0.2f));
            if (!hit)
                events::Push("autopilot missile-gate type=%u max-flight %.2f -> %.2fs (a miss at %.2fs)", m.geom.tArch,
                             was, gate, m.flightPred);
        }
        if (hit)
            g.missiles.ownMissileDealt +=
                now < 0 ? std::min(m.targetHp, m.damage) : std::min(m.targetHp - now, m.damage);
        events::Push("autopilot missile-outcome %s %s id=%u target=%u flight=%.2fs predicted=%.2fs launch-dist=%.0f "
                     "closing=%.0f closest=%.0f lost=%.0f dead=%d t-speed=%.0f t-turn=%.2f t-strafe=%.0f aspect=%.0f "
                     "crossing=%.0f escape=%.2f",
                     hit ? "hit" : "miss", m.planned ? "planned" : "unplanned", it->id, m.target, it->gone - m.launched,
                     m.flightPred, m.launchDist, m.closing, m.closest,
                     m.targetHp > 0 && now >= 0 ? m.targetHp - now : -1.0f, now <= 0 ? 1 : 0, m.geom.tSpeed,
                     m.geom.tTurn, m.geom.tStrafe, m.geom.aspect, m.geom.crossing, m.geom.escape);
        it = g.missiles.ownGone.erase(it);
    }
}

// Context-sensitive safety, built in (user, 18 Oct 2026: "friendlies around? take their location into account before
// using splash damage or firing something not almost guaranteed to hit but almost guaranteed to hit friendlies; no
// friendlies or neutrals in the line of fire? no need for safety"): a non-hostile ship (friendly or neutral) within
// `radius` of the line from us to the target, or within `pastRadius` of its extension `past` metres beyond the target
// (what misses flies on).
bool LineOfFireBlocked(u32 target, const float tpos[3], float radius, float past, float pastRadius)
{
    fl::Kinematics me = {};
    if (!fl::PlayerKinematics(me))
        return false;
    const auto dot = [](const float* a, const float* b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; };
    const float line[3] = {tpos[0] - me.pos[0], tpos[1] - me.pos[1], tpos[2] - me.pos[2]};
    const float len = std::max(1.0f, std::sqrt(dot(line, line)));
    for (const space::Contact& f : Contacts())
    {
        if (!f.ship || f.attitude < 0 || f.id == target || f.distance < 1)
            continue;
        const float rel[3] = {f.pos[0] - me.pos[0], f.pos[1] - me.pos[1], f.pos[2] - me.pos[2]};
        const float along = Clampf(dot(rel, line) / len, 0, len + past);
        const float off[3] = {rel[0] - line[0] / len * along, rel[1] - line[1] / len * along,
                              rel[2] - line[2] / len * along};
        if (std::sqrt(dot(off, off)) < (along > len ? pastRadius : radius))
            return true;
    }
    // ... and a station / base / planet / platform in the way or just behind the target (what misses fly on into), by
    // the game's own ray test on the real hulls (user, 21 Oct 2026, M04 Mactan: "it hit the friendly station while
    // targeting an enemy" - the Rheinland wing attacking the Lane Hackers' base; the test above took ships only)
    // A hostile solar is no reason to hold fire (session 7, 23 Oct 2026, M12: a shield generator mounted on the Nomad
    // lair has the lair's own hull behind it - "blocked" for 60 s at 13050 of 50000 hit points, the guns silent, the leg
    // stalled): only a solar that is not hostile to us (friendly, neutral, unknown) blocks.
    const float behind[3] = {tpos[0] + line[0] / len * past, tpos[1] + line[1] / len * past,
                             tpos[2] + line[2] / len * past};
    const auto blocks = [&](const float* a, const float* b, const char* where)
    {
        u32 id = 0;
        if (space::SightBlocker(a, b, target, &id) != space::kBlockerSolar)
            return false;
        float att = 0;
        const bool known = id && space::AttitudeToPlayer(id, att);
        const bool hostile = known && att <= -0.6f;
        if (id != g.combat.lineBlocker)
        {
            g.combat.lineBlocker = id;
            events::Push("autopilot line-of-fire solar %u %s the target att=%.2f%s -> %s", id, where,
                         known ? att : 0.0f, known ? "" : " (unknown)", hostile ? "fire" : "hold");
        }
        return !hostile;
    };
    return blocks(me.pos, tpos, "before") || (past > 0 && blocks(tpos, behind, "behind"));
}

// Seconds a missile from launcher `w` needs to close `dist` metres when it starts closing at `closing0` m/s (our range
// rate + the muzzle velocity: the munition inherits the ship's velocity - measured 18 Oct 2026): coasting through the
// motor delay, accelerating during the burn, coasting after. The target's own acceleration is ignored. 1e9 when the
// munition's lifetime runs out first.
float MissileFlightTime(const space::Weapon& w, float dist, float closing0)
{
    const float a = w.motorAccel > 0 ? w.motorAccel : 0.0f, delay = std::max(0.0f, w.motorDelay),
                burn = std::max(0.0f, w.motorTime), life = w.lifetime > 0 ? w.lifetime : 30.0f;
    float d = dist, t = 0, v = closing0;
    // delay: constant speed
    if (delay > 0)
    {
        if (v > 0 && v * delay >= d)
            return d / v;
        d -= v * delay;
        t += delay;
    }
    // burn: d = v t + a t^2 / 2
    if (burn > 0 && a > 0)
    {
        const float covered = v * burn + 0.5f * a * burn * burn;
        if (covered >= d)
        {
            const float disc = v * v + 2 * a * d;
            return t + (-v + std::sqrt(std::max(0.0f, disc))) / a;
        }
        d -= covered;
        t += burn;
        v += a * burn;
    }
    if (v <= 0)
        return 1e9f;
    t += d / v;
    return t <= life ? t : 1e9f;
}

// Can the target escape a missile launched now? (user, 21 Oct 2026: "absent-mindedly launching missiles without
// maneuvering to ensure they hit is more or less what the NPCs do, and is why the player can almost always guarantee a
// dodge"). A seeker flies pure pursuit, turning at most `maxTurn` rad/s: it loses a target whose speed across the line of
// sight exceeds maxTurn x the distance at which the blast still catches it (blast radius + the target's size). What the
// target can build up across the line before impact (after a reaction time): its velocity turned towards the beam at its
// own turn rate (from its current aspect) plus a strafe. escape = that / what the seeker can follow; >= 1: escapable.
MissileGeom MissileGeometry(u32 target, const space::Weapon& w, float flight, float targetRadius)
{
    MissileGeom g0;
    space::ShipMotion tm, me;
    if (!space::ShipMotionOf(target, tm) || !space::ShipMotionOf(space::PlayerShipId(), me))
        return g0;
    const float los[3] = {me.pos[0] - tm.pos[0], me.pos[1] - tm.pos[1], me.pos[2] - tm.pos[2]}; // target -> us
    const float r = std::max(1.0f, Len3(los));
    g0.tSpeed = tm.speed;
    g0.tArch = tm.arch;
    g0.tTurn = tm.turnRate;
    g0.tStrafe = tm.strafeAccel;
    const float along = tm.speed > 0.5f ? Dot3(tm.vel, los) / r : 0.0f;
    g0.aspect = tm.speed > 0.5f ? acosf(Clampf(along / tm.speed, -1, 1)) * 57.2958f : 0.0f;
    g0.crossing = sqrtf(std::max(0.0f, tm.speed * tm.speed - along * along));
    constexpr float kReact = 0.3f, kStrafeTop = 50.0f; // reaction (s); strafe top speed (m/s, measured 18 Oct 2026)
    const float tr = std::max(0.0f, flight - kReact);
    const float phi = g0.aspect * 3.14159265f / 180.0f, beam = 1.5707963f;
    const float turned = phi < beam ? std::min(beam, phi + tm.turnRate * tr) : std::max(beam, phi - tm.turnRate * tr);
    const float lateral = tm.speed * sinf(turned) + std::min(kStrafeTop, tm.strafeAccel * tr);
    const float follow = std::max(0.1f, w.maxTurn) * (std::max(1.0f, w.blastRadius) + targetRadius);
    g0.escape = lateral / follow;
    return g0;
}

} // namespace autopilot
