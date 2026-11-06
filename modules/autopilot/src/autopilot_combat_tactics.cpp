// Advanced Autopilot - the duel's tactics: the ship-adaptive duel choice, turret blind spots, the roll engine, the stand-off band and its
// learning, the duel against another player. Called from UpdateCombat through autopilot_impl.h.
#include "autopilot_impl.h"

namespace autopilot
{

// Ship-adaptive tactics ("worth coming up with a smart and adaptive model"). The duel against one
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
void DuelAccount(float dt, float dealtFrac, float lostFrac)
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

// Turret blind spots of big targets ("Rheinland battleships have blind-side gaps"; the 2020 footage
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
bool BlindSpot(u32 target, float gunRange, float world[3], float* fireWorld)
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
        // The turret test is the game's, yet what hits us decides (M13: hit hard at a spot the test called
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
        // Shelter (hugging the inside of the Rheinland battleship's bow trench, its hull blocked most
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
            // (...and 24 since the outside test - probe kb2: of the 8 best none was both >= 120 m and clear,
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

// ---- The stand-off band per ship pair ("generalize these findings for all player ships vs all
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

float StandoffBand(const space::ShipMotion& me, const space::ShipMotion& tg, float reach, float muzzle,
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

void RangeLearn(float dt, const aim::Target& t, bool aligned)
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

void PvpDuel(float dt, Out& out)
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
} // namespace autopilot
