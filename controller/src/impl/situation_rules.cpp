// The arbiter's rules, S3-S5 (situation_rules.h): pure - the situation, its facts and the memory in, a proposal out; no
// game, no statics. Each rule is ObjectiveDecide's branch line for line (autopilot_objective.cpp; the branch's comments
// there hold the reasons and the regressions behind every condition) over the snapshot's facts instead of the game's.
// What ObjectiveDecide writes on its way to a branch and reads after it (the leash's clears and the script block's
// anchors -> g_obj.leashShip, lastReal, the survival stance's mission memory) each rule derives from the memory as the
// decision began (Queue) the same way: no rule depends on another having run.
#include "situation_rules.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>

namespace situation
{
namespace
{
// The game's object type bits (space::Type; as situation.cpp)
constexpr u32 kTJumpGate = 1u << 6, kTAirlock = 1u << 10, kTJumpHole = 1u << 11, kTLoot = 1u << 28;
constexpr int kMarksMax = 32;                    // MissionObjectIds' buffer
constexpr double kGameGrantWayInMs = 20000;      // autopilot_impl.h
constexpr float kSurvivalReach = 1200, kBattleReach = 15000, kHostileAtt = -0.6f;
constexpr u32 kAwaiting = 21660, kBlankText = 1, kEscapeTitle = 22602; // ObjectiveDecide's placeholders, M04's title

float Dist(const float a[3], const float b[3])
{
    const float d[3] = {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
    return std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
}

// What every rule reads the way ObjectiveDecide computes it before its branches: the mission's pointers
// (MissionObjectIds), the verb (with the mark fight), the step's target (a ref, else the mark fallback).
struct Ctx
{
    const Snapshot& s;
    const Queue& q;
    std::vector<u32> pointed;
    Verb verb = Verb::None;
    bool markFight = false, markTarget = false;
    u32 target = 0;
    const SnapContact* tc = nullptr;

    const SnapContact* Find(u32 id) const
    {
        if (!id)
            return nullptr;
        for (const SnapContact& c : s.contacts)
            if (c.id == id)
                return &c;
        return nullptr;
    }
    static bool Hostile(const SnapContact& c)
    {
        return c.ship ? c.attitude <= kHostileAtt : (c.attKnown && c.att <= kHostileAtt);
    }
    static bool Friendly(const SnapContact& c) { return c.ship ? c.attitude > 0 : (c.attKnown && c.att > 0); }
    bool Pointed(u32 id) const { return std::find(pointed.begin(), pointed.end(), id) != pointed.end(); }
    bool KnowsHidden() const { return s.knowledge >= 1; }
    // the ini view: AdvanceGoals / Leashes (the objective mode's own; the live view is the assessor's)
    const std::vector<SnapGoal>& IniGoals() const { return s.goalsLive ? s.iniGoals : s.goals; }
    const std::vector<SnapLeash>& IniLeashes() const { return s.goalsLive ? s.iniLeashes : s.leashes; }

    Ctx(const Situation& sit, const Queue& queue) : s(sit.facts), q(queue)
    {
        // MissionObjectIds: the marks (newest first), the objective's refs, the step's destroy condition's objects
        for (size_t i = 0; i < s.marks.size() && pointed.size() < static_cast<size_t>(kMarksMax); ++i)
            pointed.push_back(s.marks[i]);
        for (u32 id : s.refsObj)
            if (pointed.size() < static_cast<size_t>(kMarksMax) && id && !Pointed(id))
                pointed.push_back(id);
        for (u32 id : q.scriptMarks)
            if (s.objOn && pointed.size() < static_cast<size_t>(kMarksMax) && !Pointed(id))
                pointed.push_back(id);
        // a placeholder step (no words, target or navmarker) with live hostile marks within 6 km: their fight
        if (s.step.verb == Verb::None && !s.step.refCount && !s.step.hasNav)
            for (const SnapContact& c : s.contacts)
                if (c.ship && c.health != 0 && c.distance < 6000 && Hostile(c) && !c.preHostile && Pointed(c.id))
                    markFight = true;
        verb = markFight ? Verb::Fight : s.step.verb;
        for (int i = 0; i < s.step.refCount && !target; ++i)
            target = s.step.refObj[i];
        markTarget = !target && !s.step.hasNav && verb != Verb::Fight;
        if (markTarget)
            for (size_t i = 0; i < pointed.size() && !target; ++i)
                if (const SnapContact* c = Find(pointed[i]))
                    if (c->health != 0 && !Hostile(*c))
                        target = c->id;
        tc = Find(target);
    }

    // ObjectiveThreat: a hostile the mission points at within 6 km, a hostile ship attacking us within 3 km, one
    // attacking our side within the battle's reach; under the survival stance only close attackers (and a fight's marks).
    // `survival`: the stance as ObjectiveDecide holds it at the branch - S3's branches run before its survival update: the
    // stance as the decision began (the memory's, v4; a v3 snapshot's own, taken then too); S4's: SurvivalNow
    bool Threat() const { return Threat(s.version >= 4 ? q.survival : s.survival); }
    bool Threat(bool survival) const
    {
        for (const SnapContact& c : s.contacts)
        {
            if (c.health == 0 || !Hostile(c) || c.preHostile)
                continue;
            const bool pt = Pointed(c.id);
            if (survival)
            {
                if (c.ship && c.targetsPlayer && c.distance < kSurvivalReach)
                    return true;
                if (verb == Verb::Fight && pt && c.ship && c.distance < 6000)
                    return true;
                continue;
            }
            if ((pt && c.distance < 6000 && (c.ship || c.maxHit < 1e30f)) || (c.ship && c.targetsPlayer && c.distance < 3000))
                return true;
            if (c.ship && c.targetId && c.distance < kBattleReach)
                if (const SnapContact* f = Find(c.targetId))
                    if (f->id != c.id && Friendly(*f))
                        return true;
        }
        return false;
    }
    // SurvivalLead: the nearest live friendly ship within 6 km that the objective last pointed at or the mission marks
    const SnapContact* SurvivalLead() const
    {
        const SnapContact* lead = nullptr;
        for (const SnapContact& c : s.contacts)
            if (c.ship && c.health != 0 && Friendly(c) && c.distance < 6000 && (c.id == q.escortShip || Pointed(c.id)) &&
                (!lead || c.distance < lead->distance))
                lead = &c;
        return lead;
    }
    // g_obj.leashShip after ObjectiveDecide's clears before the leash branch: none when the step docks (its words) or
    // points at a jump gate / hole (a ref, or a navmarker within 2 km of one)
    u32 LeashCleared() const
    {
        u32 leash = q.leashShip;
        if (leash && s.step.verb == Verb::Dock) // (the words alone: ObjectiveVerb, no mark fight)
            leash = 0;
        for (int i = 0; i < s.step.refCount && leash; ++i)
            if (const SnapContact* jc = Find(s.step.refObj[i]))
                if (jc->type & (kTJumpGate | kTJumpHole))
                    leash = 0;
        if (leash && s.step.hasNav)
            for (const SnapContact& g : s.contacts)
                if ((g.type & (kTJumpGate | kTJumpHole)) && Dist(g.pos, s.step.nav) < 2000.0f)
                    leash = 0;
        return leash;
    }

    // ---- S4: what ObjectiveDecide derives after the script block
    // lastReal after its update (before the script block): the step's text unless a placeholder, else the memory's
    u32 LastReal() const
    {
        const u32 t = s.step.textId;
        return t && t != kAwaiting && t != kBlankText ? t : q.lastReal;
    }
    // the objective is a trip (fly to / dock with a place or a navmarker): not a fight / escort, not arrived; a solar
    // target (hostile only for a dock, a navmarker or a step past 2 min), or a navmarker alone
    bool TravelObjective() const
    {
        const bool longStep = s.step.hasNav && verb != Verb::Fight && verb != Verb::Escort && q.stepAgeMs > 120000;
        return verb != Verb::Fight && verb != Verb::Escort && !q.arrived &&
               ((tc && !tc->ship && (!Hostile(*tc) || verb == Verb::Dock || s.step.hasNav || longStep)) ||
                (!tc && s.step.hasNav));
    }
    // a lane ride the step asks for and the mission allows (one way open) - the snapshot's lane record of the target
    bool LaneEscape() const
    {
        if (!(tc && verb == Verb::Dock))
            return false;
        for (const SnapLane& l : s.lanes)
            if (l.ring == tc->id)
                return (l.next && l.openNext == 1) || (l.prev && l.openPrev == 1);
        return false;
    }
    // the survival stance as ObjectiveDecide updates it before the pre-aggro branch: the mission-scope memory (the title,
    // its escape words or id 22602, the first system; survival_until_jump ends it at the first jump; a Survive step
    // sets it), then the objective run's stance
    struct Survival
    {
        u32 title = 0;
        bool mission = false, ended = false;
        u32 system = 0;
        bool on = false; // g_obj.survival
    };
    Survival SurvivalNow() const
    {
        Survival v;
        v.title = q.survTitle, v.mission = q.survMission, v.ended = q.survEnded, v.system = q.survSystem, v.on = q.survival;
        if (s.title != v.title)
        {
            v.title = s.title;
            v.mission = s.title == kEscapeTitle || (s.title && s.titleEscape);
            v.ended = false;
            v.system = s.pilotOk ? s.pilotSystem : 0;
        }
        if (s.survivalUntilJump && v.mission && !v.ended && v.system && s.pilotOk && s.pilotSystem &&
            s.pilotSystem != v.system)
            v.ended = true, v.mission = false, v.on = false;
        if (verb == Verb::Survive && !v.ended)
            v.mission = true;
        if (v.mission && !v.on)
            v.on = true;
        return v;
    }

    // ---- S5: what ObjectiveDecide derives after the ship target
    // the target's lane record (space::LaneLinks answered: a ring among the contacts), null none
    const SnapLane* LaneOf(u32 id) const
    {
        if (!id)
            return nullptr;
        for (const SnapLane& l : s.lanes)
            if (l.ring == id)
                return &l;
        return nullptr;
    }
    // the survival stance's lead to rest beside (`lead`): the stance (as updated before the pre-aggro branch), no
    // target still to reach, no navmarker - SurvivalLead (the escort memory unchanged: a ship target never gets here)
    const SnapContact* Lead() const
    {
        if (!(SurvivalNow().on && !(tc && !q.arrived) && !s.step.hasNav))
            return nullptr;
        return SurvivalLead();
    }
    // PassRingRadius: a satellite-class solar that is no lane ring and no dock, with no lane links, flown through when
    // its obstacle radius is 30..300 m and it is not hostile; 0 none
    float PassRadius() const
    {
        if (!tc || tc->ship || tc->dock || !(tc->type & 0x10u) || (tc->type & (1u << 7)) || LaneOf(tc->id))
            return 0;
        return tc->radius >= 30 && tc->radius <= 300 && !Hostile(*tc) ? tc->radius : 0;
    }
    // the objective's route: two or more entries, the step being the last one's text
    bool RouteStep() const { return s.entries.size() >= 2 && s.entries.back().textId == s.step.textId; }
    static u32 FirstRef(const SnapStep& e)
    {
        for (int i = 0; i < e.refCount; ++i)
            if (e.refObj[i])
                return e.refObj[i];
        return 0;
    }
};

// ObjectiveDecide's script-goal block (knowledge "all", script_goals, not an escort / dock step, and a fight, no target or
// only the mark fallback's non-hostile one): the goals (the ini view; none for a navmarker step by the last objective),
// the leashes' anchors (and what they leave in g_obj.leashShip), the goal loop's best trip and its first fired condition.
// The script goal, the pre-stage and the pre-stage-next rules read it; the pre-aggro rule its leash.
struct Block
{
    bool applies = false;
    bool fromLast = false;
    std::vector<SnapGoal> goals;
    struct Anchor
    {
        float p[3];
        float dist;
    };
    std::vector<Anchor> anchors;
    u32 leashShip = 0; // g_obj.leashShip / leashDist after the block
    float leashDist = 0;
    int best = -1, fired = -1; // indices into goals
    float bestPos[3] = {}, bestDist = 1e30f;
    bool bestWins = false;

    bool Leashed(const float* p, float r) const
    {
        for (const Anchor& a : anchors)
            if (Dist(p, a.p) - 0.7f * r > 0.85f * a.dist)
                return true;
        return false;
    }
};

Block ScriptBlock(const Ctx& c)
{
    const Snapshot& s = c.s;
    const Queue& q = c.q;
    const SnapContact* tc = c.tc;
    const Verb verb = c.verb;
    Block b;
    b.leashShip = c.LeashCleared(), b.leashDist = q.leashDist;
    b.applies = c.KnowsHidden() && s.scriptGoals && verb != Verb::Escort && verb != Verb::Dock &&
                (verb == Verb::Fight || !tc || (c.markTarget && !Ctx::Hostile(*tc)));
    if (!b.applies)
        return b;
    b.fromLast = s.goalsFromLast;
    b.goals = c.IniGoals();
    if (b.fromLast && s.step.hasNav) // (a step with its own navmarker and a text no script knows: the last one's are stale)
        b.goals.clear();
    // the leashes' anchors (only one we are inside now)
    for (const SnapLeash& l : c.IniLeashes())
    {
        if (l.off) // disarmed by the script (its comm was heard)
        {
            b.leashShip = 0;
            continue;
        }
        Block::Anchor a = {{}, l.dist};
        const SnapContact* oc = c.Find(l.object);
        if (!(oc && oc->ship) && l.nameIds && l.ship)
            if (const SnapContact* byName = c.Find(l.ship))
                oc = byName;
        const bool named = oc && oc->ship;
        const SnapContact* lc = named ? oc : tc && tc->ship && !Ctx::Hostile(*tc) ? tc : c.SurvivalLead();
        bool found = lc != nullptr, object = false;
        if (found)
            memcpy(a.p, lc->pos, sizeof(a.p));
        if (!named && l.placed && (!found || (oc && !oc->ship)))
            memcpy(a.p, l.opos, sizeof(a.p)), found = true, object = true;
        const bool in = found && s.pilotOk && (named || Dist(a.p, s.pos) < l.dist);
        if (in)
            b.anchors.push_back(a);
        if (in && lc && !object) // (the supervisor keeps every trip to it)
            b.leashShip = lc->id, b.leashDist = l.dist;
    }
    // DestroyFired over the memory of the objects seen alive / listed (as the old path's statics, updated as it reads)
    std::set<u32> seenAlive(q.seenAlive.begin(), q.seenAlive.end()), seenListed(q.seenListed.begin(), q.seenListed.end());
    const auto destroyFired = [&](const SnapGoal& sg)
    {
        bool any = false, allSeen = !sg.objects.empty();
        for (const SnapGoalObj& o : sg.objects)
        {
            const SnapContact* x = c.Find(o.id);
            if (x)
                seenListed.insert(o.id);
            const bool live = x ? x->health != 0 : !seenListed.count(o.id) && o.placed;
            if (live)
                seenAlive.insert(o.id);
            any = any || live;
            allSeen = allSeen && seenAlive.count(o.id);
        }
        return !any && allSeen;
    };
    if (s.pilotOk)
        for (size_t i = 0; i < b.goals.size(); ++i)
        {
            const SnapGoal& sg = b.goals[i];
            if (sg.kind == 3)
            {
                if (destroyFired(sg) && b.fired < 0 && sg.leads != "SUCCEED")
                    b.fired = static_cast<int>(i);
                continue;
            }
            if (!(verb == Verb::Fight || b.fromLast || sg.subject != "player"))
                continue;
            const bool wins = sg.leads == "SUCCEED" || LeadsToLanding(sg.leads);
            if (verb == Verb::Fight && !b.fromLast && sg.byEntry)
                continue;
            if (verb == Verb::Fight && !b.fromLast && !wins)
                continue;
            if (b.bestWins && !wins)
                continue;
            float p[3];
            if (sg.kind == 1)
            {
                if (!sg.placed)
                    continue;
                memcpy(p, sg.objPos, sizeof(p));
            }
            else
                memcpy(p, sg.pos, sizeof(p));
            if (b.Leashed(p, sg.dist))
                continue;
            const float far = Dist(p, s.pos);
            if (far <= sg.dist)
            {
                if (b.fired < 0 && sg.leads != "SUCCEED")
                    b.fired = static_cast<int>(i);
                continue;
            }
            if (q.badGoal == sg.trigger || std::find(q.deadGoals.begin(), q.deadGoals.end(), sg.trigger) != q.deadGoals.end())
                continue;
            if ((wins && !b.bestWins) || far - sg.dist < b.bestDist)
                b.bestDist = far - sg.dist, b.best = static_cast<int>(i), b.bestWins = wins, memcpy(b.bestPos, p, sizeof(p));
        }
    if (b.best >= 0 && s.step.hasNav && verb != Verb::Fight && Dist(s.step.nav, b.bestPos) < b.goals[b.best].dist)
        b.best = -1; // (a navmarker trip whose marker lies inside the goal's reach is the same trip)
    return b;
}

// a kind 1 / 2 goal's position (a point; a solar the game places)
bool PlacedAt(const SnapGoal& g, float* p)
{
    if (g.kind == 2)
        return memcpy(p, g.pos, 3 * sizeof(float)), true;
    if (!g.placed)
        return false;
    return memcpy(p, g.objPos, 3 * sizeof(float)), true;
}

// (the plan: a mode start on the target by default - StartOrKeep; the travel / dock / pass / shoot exits set theirs)
Proposal Make(const char* rule, const char* mode, u32 target, const char* action, std::string detail = std::string())
{
    Proposal p;
    p.rule = rule, p.mode = mode, p.target = target, p.action = action, p.detail = std::move(detail);
    p.plan.kind = Plan::Kind::Mode, p.plan.object = target;
    return p;
}
// the running sub-mode goes on
Proposal Keep(const char* rule, const Queue& q, std::string detail = std::string())
{
    Proposal p;
    p.rule = rule, p.mode = q.sub, p.target = q.subTarget, p.action = q.action, p.keep = true, p.detail = std::move(detail);
    return p;
}
// ---- the plans (A37 S7), each the old exit's start
// StartTravel(object, nullptr, 0, noDock)
Proposal& ToObject(Proposal& p, u32 object, bool noDock)
{
    p.plan = Plan();
    p.plan.kind = Plan::Kind::Travel, p.plan.object = object, p.plan.noDock = noDock;
    return p;
}
// StartTravel(0, point, system, true)
Proposal& ToPoint(Proposal& p, const float* point, u32 system = 0)
{
    p.plan = Plan();
    p.plan.kind = Plan::Kind::Travel, p.plan.hasPoint = true, p.plan.system = system, p.plan.noDock = true;
    memcpy(p.plan.point, point, sizeof(p.plan.point));
    return p;
}
// the trip flown through toward `dir` (g.tr flyDir / flyRadius / flyAhead)
void Fly(Plan& p, const float* dir, float radius, float ahead)
{
    p.fly = true, p.flyRadius = radius, p.flyAhead = ahead;
    memcpy(p.flyDir, dir, sizeof(p.flyDir));
}
void FlyGoal(Plan& p, const float* at, float r)
{
    memcpy(p.flyGoal, at, sizeof(p.flyGoal));
    p.flyGoalR = r;
}
// the near side of a sphere round `gp` (radius r) seen from `from`: gp - (gp - from) / |gp - from| * r * 0.7; `dir` the
// unit way there (ObjectiveDecide's q / q2 and dir)
void NearSide(const float* gp, const float* from, float r, float* q, float* dir)
{
    const float tg[3] = {gp[0] - from[0], gp[1] - from[1], gp[2] - from[2]};
    const float tl = std::sqrt(tg[0] * tg[0] + tg[1] * tg[1] + tg[2] * tg[2]);
    for (int i = 0; i < 3; ++i)
        q[i] = gp[i] - tg[i] / tl * r * 0.7f, dir[i] = tg[i] / tl;
}
// `stand` m from `at` on the line toward `from` (`dist` = |at - from|, its floor 1 m): at + (from - at) * (stand / dist)
void Short(const float* at, const float* from, float stand, float dist, float* q)
{
    for (int i = 0; i < 3; ++i)
        q[i] = at[i] + (from[i] - at[i]) * (stand / std::max(1.0f, dist));
}
} // namespace

const char* PlanKindName(Plan::Kind k)
{
    static const char* const n[] = {"none", "mode", "travel", "hold-at", "dock", "pass", "shoot", "waypoints"};
    static_assert(sizeof(n) / sizeof(n[0]) == static_cast<size_t>(Plan::Kind::COUNT), "a name per plan kind");
    const size_t i = static_cast<size_t>(k);
    return i < sizeof(n) / sizeof(n[0]) ? n[i] : "?";
}

std::string PlanText(const Plan& p)
{
    char b[256];
    int n = snprintf(b, sizeof(b), "%s object=%u", PlanKindName(p.kind), p.object);
    if (p.hasPoint && n > 0 && n < static_cast<int>(sizeof(b)))
        n += snprintf(b + n, sizeof(b) - n, " point=%.0f,%.0f,%.0f system=%u", p.point[0], p.point[1], p.point[2], p.system);
    if (n > 0 && n < static_cast<int>(sizeof(b)))
        n += snprintf(b + n, sizeof(b) - n, " dock=%d", p.noDock ? 0 : 1);
    if (p.toward && n > 0 && n < static_cast<int>(sizeof(b)))
        n += snprintf(b + n, sizeof(b) - n, " toward=%u", p.toward);
    if (p.radius > 0 && n > 0 && n < static_cast<int>(sizeof(b)))
        n += snprintf(b + n, sizeof(b) - n, " radius=%.0f", p.radius);
    if (p.fly && n > 0 && n < static_cast<int>(sizeof(b)))
        snprintf(b + n, sizeof(b) - n, " fly=%.2f,%.2f,%.2f r=%.0f ahead=%.0f", p.flyDir[0], p.flyDir[1], p.flyDir[2],
                 p.flyRadius, p.flyAhead);
    return b;
}

std::string ProposalText(const Proposal& p)
{
    char b[160];
    snprintf(b, sizeof(b), "%s mode=%s target=%u action=%s keep=%d", p.rule.c_str(), p.mode.c_str(), p.target,
             p.action.c_str(), p.keep ? 1 : 0);
    return b;
}

std::optional<Proposal> Rule_Held(const Situation& sit, const Queue& q)
{
    switch (sit.held)
    {
    case Held::MissionHolds:
    case Held::GameDock:
    case Held::Lane:
    case Held::Jump: return Keep("held", q, HeldName(sit.held));
    default: return std::nullopt;
    }
}

std::optional<Proposal> Rule_Leash(const Situation& sit, const Queue& q)
{
    const Ctx c(sit, q);
    const u32 leash = c.LeashCleared();
    if (!leash)
        return std::nullopt;
    const SnapContact* lc = c.Find(leash);
    if (!lc || !(lc->distance > 0.8f * q.leashDist))
        return std::nullopt;
    char d[96];
    snprintf(d, sizeof(d), "%.0f m from the leash's ship %u (leash %.0f m)", lc->distance, lc->id, q.leashDist);
    // LeashTripRuns: our trip back to where the ship is still ends within 1 km of it
    if (q.mode == "travel" && q.action == "leash" && q.trHasDest && !q.trDestObject && Dist(lc->pos, q.trDest) < 1000.0f)
        return Keep("leash", q, d);
    Proposal p = Make("leash", "travel", 0, "leash", d);
    return ToPoint(p, lc->pos); // (LeashBack: a trip to where the ship is now)
}

std::optional<Proposal> Rule_MarkedLoot(const Situation& sit, const Queue& q)
{
    const Ctx c(sit, q);
    if (c.Threat())
        return std::nullopt;
    // DestroyMarkedLoot: the nearest mission-marked loot, while a Fight verb is in any entry's words
    const SnapContact* best = nullptr;
    for (const SnapContact& x : c.s.contacts)
        if ((x.type & kTLoot) && c.Pointed(x.id) && (!best || x.distance < best->distance))
            best = &x;
    bool asks = c.s.step.verb == Verb::Fight;
    for (const SnapStep& e : c.s.entries)
        asks = asks || e.verb == Verb::Fight;
    if (!best || !asks)
        return std::nullopt;
    char d[64];
    snprintf(d, sizeof(d), "marked loot %u to destroy at %.0f m", best->id, best->distance);
    Proposal p = Make("marked-loot", "shoot", best->id, "fight", d);
    p.keep = q.mode == "shoot" && q.target == best->id; // (already shooting it: ObjectiveStart only)
    p.plan.kind = p.keep ? Plan::Kind::None : Plan::Kind::Shoot;
    return p;
}

std::optional<Proposal> Rule_TractorExit(const Situation& sit, const Queue& q)
{
    const Ctx c(sit, q);
    const Snapshot& s = c.s;
    if (!(c.markTarget && c.tc && (c.tc->type & kTLoot) && q.mode == "travel" && q.sub == "travel"))
        return std::nullopt;
    const float reach = s.tractorRange > 0 ? s.tractorRange : 1500.0f;
    if (!(c.tc->distance < reach))
        return std::nullopt;
    const float toDoor = q.lockHave && s.pilotOk ? Dist(q.lockP2, s.pos) : 1e30f;
    char d[96];
    snprintf(d, sizeof(d), "marked loot %u at %.0f m within the beam's %.0f m", c.tc->id, c.tc->distance, reach);
    if (s.tractorExitRun && q.cruiseLock && q.lockId && q.lockId != ~0u && q.lockHave && toDoor < s.tractorExitRange)
    {
        Proposal p = Make("tractor-exit", "travel", q.lockId, "airlock", d);
        return ToObject(p, q.lockId, false);
    }
    Proposal p = Keep("tractor-exit", q, d);
    p.mode = "travel"; // (ObjectiveStart(Travel, subTarget, action, true))
    return p;
}

std::optional<Proposal> Rule_Jump(const Situation& sit, const Queue& q)
{
    const Ctx c(sit, q);
    if (!c.tc || !(c.tc->type & (kTJumpGate | kTJumpHole)))
        return std::nullopt;
    Proposal p = Make("jump", "travel", c.tc->id, "jump");
    return ToObject(p, c.tc->id, false);
}

std::optional<Proposal> Rule_Airlock(const Situation& sit, const Queue& q)
{
    const Ctx c(sit, q);
    const Snapshot& s = c.s;
    if (!(s.sinceGrantMs >= 0 && s.sinceGrantMs < kGameGrantWayInMs && !s.dockInProgress))
        return std::nullopt;
    const auto cameIn = [&](const SnapContact& x) { return s.hasEntry && Dist(x.pos, s.entry) < 600.0f; };
    const SnapContact* lock = nullptr;
    for (const SnapContact& x : s.contacts)
        if ((x.type & kTAirlock) && x.distance < 3000 &&
            (!lock || (cameIn(*lock) && !cameIn(x)) || (cameIn(*lock) == cameIn(x) && x.distance < lock->distance)))
            lock = &x;
    if (!lock)
        return std::nullopt;
    char d[64];
    snprintf(d, sizeof(d), "granted airlock %u at %.0f m", lock->id, lock->distance);
    Proposal p = Make("airlock", "travel", lock->id, "airlock", d);
    return ToObject(p, lock->id, false);
}

std::optional<Proposal> Rule_ScriptDestroy(const Situation& sit, const Queue& q)
{
    const Ctx c(sit, q);
    const Snapshot& s = c.s;
    const SnapContact* tc = c.tc;
    if (!(c.KnowsHidden() && s.scriptGoals && c.verb != Verb::Fight && c.verb != Verb::Escort && c.verb != Verb::Dock &&
          (!tc || !tc->ship || (c.markTarget && !Ctx::Hostile(*tc))) && s.pilotOk))
        return std::nullopt;
    const bool fromLast = s.goalsFromLast;
    for (const SnapGoal& sg : c.IniGoals())
    {
        if (sg.kind != 3)
            continue;
        int alive = 0;
        u32 marks[8] = {};
        float nearest = 1e30f, nearPos[3] = {};
        for (const SnapGoalObj& o : sg.objects)
        {
            if (alive >= 8)
                break;
            const float* p = nullptr;
            bool live = false;
            if (const SnapContact* x = c.Find(o.id))
                live = x->health != 0, p = x->pos;
            else
                live = o.placed, p = o.pos;
            if (!live)
                continue;
            marks[alive++] = o.id;
            const float dist = Dist(p, s.pos);
            if (dist < nearest)
                nearest = dist, memcpy(nearPos, p, sizeof(nearPos));
        }
        if (!alive)
            continue;
        // stale_destroy_wait: by the last objective / replacing a trip step, none of its objects marked yet - a later
        // step's (never one that ends the mission)
        if (s.staleDestroyWait && (fromLast || tc || s.step.hasNav) && sg.leads != "null" && sg.leads != "SUCCEED")
        {
            bool marked = false;
            for (int i = 0; i < alive && !marked; ++i)
                marked = c.Pointed(marks[i]);
            if (!marked)
                continue;
        }
        char d[160];
        snprintf(d, sizeof(d), "%s: %d alive, nearest %.0f m -> %s%s", sg.trigger.c_str(), alive, nearest, sg.leads.c_str(),
                 fromLast ? " (last objective)" : "");
        // (the exit's writes: the live objects are the marks, the step a fight; far: a trip 1.5 km short of the nearest)
        Proposal p = Make("script-destroy", "combat", 0, "script-kill", d);
        if (nearest > 5000)
        {
            float qp[3];
            Short(nearPos, s.pos, 1500.0f, nearest, qp);
            p = Make("script-destroy", "travel", 0, "to-marks", d);
            ToPoint(p, qp);
            p.plan.travelObjective = 1;
        }
        p.plan.fightObjective = 1;
        p.plan.scriptMarks.assign(marks, marks + alive);
        return p;
    }
    return std::nullopt;
}

std::optional<Proposal> Rule_DockEntry(const Situation& sit, const Queue& q)
{
    const Ctx c(sit, q);
    // (a Dock step sets or clears the memory: no entry from it; any other step keeps it only behind a placeholder the
    // script does not know, the dock target still in space and no ship leash in the system)
    if (c.verb == Verb::Dock || !q.lastDock)
        return std::nullopt;
    if (!c.s.goalsFromLast || !c.Find(q.lastDock) || !c.IniLeashes().empty())
        return std::nullopt;
    char d[80];
    snprintf(d, sizeof(d), "text %u is no step of the script - the last dock goes on", c.s.step.textId);
    Proposal p = Make("dock-entry", "travel", q.lastDock, "dock", d);
    return ToObject(p, q.lastDock, false);
}

std::optional<Proposal> Rule_ScriptGoal(const Situation& sit, const Queue& q)
{
    const Ctx c(sit, q);
    const Snapshot& s = c.s;
    const Verb verb = c.verb;
    const Block b = ScriptBlock(c);
    if (!b.applies)
        return std::nullopt;
    const bool fromLast = b.fromLast;
    const auto armedBy = [&](const SnapGoal& g)
    {
        std::vector<const SnapGoal*> out;
        for (const SnapArmed& a : s.armed)
            if (a.by == g.trigger && a.byFile == g.file)
                out.push_back(&a.goal);
        return out;
    };
    const SnapGoal* best = b.best >= 0 ? &b.goals[b.best] : nullptr;
    const SnapGoal* fired = b.fired >= 0 ? &b.goals[b.fired] : nullptr;
    const float* bestPos = b.bestPos;
    const float bestDist = b.bestDist;
    const auto leashed = [&](const float* p, float r) { return b.Leashed(p, r); };
    const auto placedAt = PlacedAt;
    if (best)
    {
        char d[200];
        for (const SnapGoal* ng : armedBy(*best))
        {
            float gp[3];
            if (ng->kind == 3 || ng->subject != "player" || !placedAt(*ng, gp))
                continue;
            if (leashed(gp, ng->dist))
                continue;
            const float tl = Dist(gp, s.pos);
            if (Dist(gp, bestPos) > best->dist || tl <= ng->dist)
                continue; // (only a goal inside the first one's sphere)
            snprintf(d, sizeof(d), "%s arms %s inside it: through, %.0f m to go", best->trigger.c_str(), ng->trigger.c_str(),
                     tl - ng->dist);
            // (the armed goal's near side, flown through; the supervisor's retry reads its sphere)
            float q2[3], dir[3];
            NearSide(gp, s.pos, ng->dist, q2, dir);
            Proposal p = Make("script-goal", "travel", 0, "script-goal", d);
            ToPoint(p, q2);
            Fly(p.plan, dir, ng->dist, ng->dist * 0.7f);
            FlyGoal(p.plan, gp, ng->dist);
            p.plan.travelObjective = 1, p.plan.setGoalTrigger = true, p.plan.goalTrigger = best->trigger;
            return p;
        }
        snprintf(d, sizeof(d), "%s within %.0f m -> %s, %.0f m to go%s", best->trigger.c_str(), best->dist,
                 best->leads.c_str(), bestDist, fromLast ? " (last objective)" : "");
        // to 70 % of the condition's distance on our side of the object or point
        const float dv[3] = {s.pos[0] - bestPos[0], s.pos[1] - bestPos[1], s.pos[2] - bestPos[2]};
        const float l = std::max(1.0f, std::sqrt(dv[0] * dv[0] + dv[1] * dv[1] + dv[2] * dv[2]));
        float qp[3];
        for (int i = 0; i < 3; ++i)
            qp[i] = bestPos[i] + dv[i] / l * best->dist * 0.7f;
        Proposal p = Make("script-goal", "travel", 0, "script-goal", d);
        ToPoint(p, qp);
        p.plan.travelObjective = 1, p.plan.setGoalTrigger = true, p.plan.goalTrigger = best->trigger;
        return p;
    }
    // a fired distance condition whose chain arms the next one: the armed one is the trip
    if (fired && fired->kind != 3 && verb != Verb::Fight)
        for (const SnapGoal* ng : armedBy(*fired))
        {
            float gp[3];
            if (ng->kind == 3 || ng->subject != "player" ||
                std::find(q.deadGoals.begin(), q.deadGoals.end(), ng->trigger) != q.deadGoals.end() || !placedAt(*ng, gp))
                continue;
            if (leashed(gp, ng->dist))
                continue;
            const float tl = Dist(gp, s.pos);
            if (tl <= ng->dist)
                continue;
            char d[200];
            snprintf(d, sizeof(d), "%s armed by %s: within %.0f m -> %s, %.0f m to go", ng->trigger.c_str(),
                     fired->trigger.c_str(), ng->dist, ng->leads.c_str(), tl - ng->dist);
            float qp[3], dir[3];
            NearSide(gp, s.pos, ng->dist, qp, dir);
            Proposal p = Make("script-goal", "travel", 0, "script-goal", d);
            ToPoint(p, qp); // (no fly-through: the old exit sets none)
            FlyGoal(p.plan, gp, ng->dist);
            p.plan.travelObjective = 1, p.plan.setGoalTrigger = true, p.plan.goalTrigger = ng->trigger;
            return p;
        }
    return std::nullopt;
}

// ---------------------------------------------------------------- S4
std::optional<Proposal> Rule_PreStage(const Situation& sit, const Queue& q)
{
    const Ctx c(sit, q);
    const Snapshot& s = c.s;
    const Verb verb = c.verb;
    const Block b = ScriptBlock(c);
    if (!b.applies || b.fired < 0)
        return std::nullopt;
    const SnapGoal& fired = b.goals[b.fired];
    if (!((b.fromLast && verb == Verb::None) || (fired.kind == 3 && verb == Verb::Fight)))
        return std::nullopt;
    char d[200];
    for (const SnapAfter& a : s.after) // (GoalsAfter(fired->leads, fired->file), in its order)
    {
        if (a.leads != fired.leads || a.file != fired.file)
            continue;
        const SnapGoal& ng = a.goal;
        float gp[3];
        if ((ng.kind == 1 || ng.kind == 2) && ng.subject == "player" && PlacedAt(ng, gp))
        {
            const float tl = Dist(gp, s.pos);
            if (tl <= ng.dist)
                continue; // inside it already: the step fires the moment it is set
            snprintf(d, sizeof(d), "%s -> %s: next step's %s, within %.0f m, %.0f m to go", fired.trigger.c_str(),
                     fired.leads.c_str(), ng.trigger.c_str(), ng.dist, tl - ng.dist);
            float qp[3], dir[3];
            NearSide(gp, s.pos, ng.dist, qp, dir);
            Proposal p = Make("pre-stage", "travel", 0, "pre-stage", d);
            ToPoint(p, qp);
            Fly(p.plan, dir, ng.dist, ng.dist * 0.7f);
            FlyGoal(p.plan, gp, ng.dist);
            p.plan.travelObjective = 1;
            return p;
        }
        if (ng.kind != 3)
            continue;
        float nearest = 1e30f, nearPos[3] = {};
        u32 nearId = 0;
        for (const SnapGoalObj& o : ng.objects) // (ObjectPos: the game's placement, not the contact list)
        {
            if (!o.placed)
                continue;
            const float dd = Dist(o.pos, s.pos);
            if (dd < nearest)
                nearest = dd, nearId = o.id, memcpy(nearPos, o.pos, sizeof(nearPos));
        }
        if (!nearId)
            continue;
        snprintf(d, sizeof(d), "%s -> %s: next step's %s, nearest %u at %.0f m", fired.trigger.c_str(), fired.leads.c_str(),
                 ng.trigger.c_str(), nearId, nearest);
        // (300 m off its centre on our side: a trip there, within 700 m a hold at that point)
        float qp[3];
        Short(nearPos, s.pos, std::min(300.0f, nearest), nearest, qp);
        Proposal p = Make("pre-stage", nearest > 700 ? "travel" : "hold", 0, "pre-stage", d);
        if (nearest > 700)
            ToPoint(p, qp);
        else
        {
            p.plan.kind = Plan::Kind::HoldAt, p.plan.hasPoint = true;
            memcpy(p.plan.point, qp, sizeof(qp));
        }
        p.plan.travelObjective = 1, p.plan.preStageTarget = nearId;
        return p;
    }
    return std::nullopt;
}

std::optional<Proposal> Rule_PreStageNext(const Situation& sit, const Queue& q)
{
    const Ctx c(sit, q);
    const Snapshot& s = c.s;
    const Block b = ScriptBlock(c);
    const u32 lastReal = c.LastReal();
    if (!(b.applies && s.prestageNext && b.fired < 0 && c.verb == Verb::None &&
          (b.fromLast || s.step.textId == kAwaiting) && lastReal && !s.step.hasNav && s.pilotOk))
        return std::nullopt;
    // (the snapshot's next-step facts are read from its own lastReal: from another text they are no answer - fail closed)
    if (s.prestage.lastReal != lastReal)
        return std::nullopt;
    char d[200];
    for (const SnapGoal& ng : s.realNext) // NextStepGoals(lastReal)
    {
        float gp[3];
        if (!((ng.kind == 1 || ng.kind == 2) && ng.subject == "player" && PlacedAt(ng, gp)))
            continue;
        if (b.Leashed(gp, ng.dist))
            continue; // (M04: Juni's 6 km leash - the trip may not break it)
        const float tl = Dist(gp, s.pos);
        if (tl <= ng.dist)
            continue;
        snprintf(d, sizeof(d), "%u -> %s: its %s, within %.0f m, %.0f m to go", lastReal, s.prestage.next.c_str(),
                 ng.trigger.c_str(), ng.dist, tl - ng.dist);
        float qp[3], dir[3];
        NearSide(gp, s.pos, ng.dist, qp, dir);
        Proposal p = Make("pre-stage-next", "travel", 0, "pre-stage", d);
        ToPoint(p, qp);
        Fly(p.plan, dir, ng.dist, ng.dist * 0.7f);
        FlyGoal(p.plan, gp, ng.dist);
        p.plan.travelObjective = 1;
        return p;
    }
    // ...else the objective the script declares next, where it has a point here
    if (s.prestage.nextOk && !b.Leashed(s.prestage.nextPos, 0))
    {
        const float tl = Dist(s.prestage.nextPos, s.pos);
        if (tl > 1000.0f)
        {
            snprintf(d, sizeof(d), "%u -> %u (declared next): its point, %.0f m to go", lastReal, s.prestage.nextText, tl);
            Proposal p = Make("pre-stage-next", "travel", 0, "pre-stage", d);
            ToPoint(p, s.prestage.nextPos);
            p.plan.travelObjective = 1;
            return p;
        }
    }
    return std::nullopt;
}

std::optional<Proposal> Rule_MarkFight(const Situation& sit, const Queue& q)
{
    const Ctx c(sit, q);
    const Snapshot& s = c.s;
    if (c.verb != Verb::Fight)
        return std::nullopt;
    const SnapContact* nm = nullptr; // the nearest live mark that is not ours
    for (const SnapContact& x : s.contacts)
        if (x.health != 0 && c.Pointed(x.id) && !Ctx::Friendly(x) && (!nm || x.distance < nm->distance))
            nm = &x;
    if (!nm)
        return std::nullopt;
    char d[96];
    // big_coast: a huge static mark (obstacle radius >= 500 m) is flown at by the fight itself
    if (s.bigCoast && !nm->ship && nm->radius >= 500.0f)
    {
        snprintf(d, sizeof(d), "huge static mark %u (radius %.0f m): the fight's coast", nm->id, nm->radius);
        return Make("mark-fight", "combat", nm->id, "fight", d);
    }
    if (nm->distance > 5000 && s.pilotOk)
    {
        snprintf(d, sizeof(d), "marked target %u is %.0f m off: there first", nm->id, nm->distance);
        float qp[3];
        Short(nm->pos, s.pos, 1500.0f, nm->distance, qp);
        Proposal p = Make("mark-fight", "travel", 0, "to-marks", d);
        ToPoint(p, qp);
        p.plan.travelObjective = 1;
        return p;
    }
    return std::nullopt;
}

std::optional<Proposal> Rule_PreAggro(const Situation& sit, const Queue& q)
{
    const Ctx c(sit, q);
    const Snapshot& s = c.s;
    if (c.verb == Verb::Dock || c.verb == Verb::Escort || c.LaneEscape())
        return std::nullopt;
    const SnapContact* ph = nullptr; // PreHostileNear: the nearest live ship the script is about to turn hostile
    for (const SnapContact& x : s.contacts)
        if (x.ship && x.preHostile && x.health != 0 && (!ph || x.distance < ph->distance))
            ph = &x;
    if (!ph)
        return std::nullopt;
    // with an armed ship leash only a ship within 75 % of it from the leash's anchor
    const Block b = ScriptBlock(c);
    bool inLeash = true;
    if (b.leashShip)
        if (const SnapContact* lc = c.Find(b.leashShip))
            inLeash = Dist(ph->pos, lc->pos) < 0.75f * b.leashDist;
    if (!inLeash)
        return std::nullopt;
    char d[80];
    snprintf(d, sizeof(d), "pre-hostile %u at %.0f m", ph->id, ph->distance);
    return Make("pre-aggro", "combat", ph->id, "pre-aggro", d);
}

std::optional<Proposal> Rule_SurvivalStage(const Situation& sit, const Queue& q)
{
    const Ctx c(sit, q);
    const Snapshot& s = c.s;
    const Ctx::Survival sv = c.SurvivalNow();
    const bool calm = sv.on && s.survivalCalm;
    if (!(calm && s.survivalStage && c.KnowsHidden() && !c.LaneEscape() && !c.TravelObjective() && !s.step.hasNav))
        return std::nullopt;
    const u32 from = c.LastReal(); // (the step's real text, else lastReal)
    if (!(from && s.prestage.stageFrom == from && s.prestage.stageOk && s.pilotOk))
        return std::nullopt;
    const float tl = Dist(s.prestage.stagePos, s.pos);
    char d[96];
    snprintf(d, sizeof(d), "%u -> next place %u, %.0f m off", from, s.prestage.stageText, tl);
    if (c.Threat(sv.on) && tl < 1500.0f)
        return Make("survival-stage", "combat", 0, "survive", d);
    if (tl > 700.0f)
    {
        // (300 m short of the place: beside the ring, not in its mouth)
        float qp[3];
        for (int i = 0; i < 3; ++i)
            qp[i] = s.prestage.stagePos[i] - (tl > 1 ? (s.prestage.stagePos[i] - s.pos[i]) / tl : 0.0f) * 300.0f;
        Proposal p = Make("survival-stage", "travel", 0, "survive", d);
        return ToPoint(p, qp);
    }
    return Make("survival-stage", "hold", 0, "survive", d);
}

std::optional<Proposal> Rule_Survival(const Situation& sit, const Queue& q)
{
    const Ctx c(sit, q);
    const Snapshot& s = c.s;
    const Ctx::Survival sv = c.SurvivalNow();
    const bool calm = sv.on && s.survivalCalm;
    const SnapContact* tc = c.tc;
    if (!(sv.on && !c.LaneEscape() && !c.TravelObjective() && (c.verb != Verb::Fight || calm) &&
          (calm || !(tc && Ctx::Hostile(*tc))) && c.Threat(sv.on)))
        return std::nullopt;
    if (const SnapContact* lead = c.SurvivalLead())
        return Make("survival", "escort", lead->id, "survive", "attacked: with the mission's lead");
    return Make("survival", "combat", 0, "survive", "attacked: close attackers only");
}

std::optional<Proposal> Rule_ThreatFight(const Situation& sit, const Queue& q)
{
    const Ctx c(sit, q);
    const Snapshot& s = c.s;
    const Ctx::Survival sv = c.SurvivalNow();
    const bool calm = sv.on && s.survivalCalm;
    const SnapContact* tc = c.tc;
    const Verb verb = c.verb;
    const bool hostileTarget = tc && Ctx::Hostile(*tc) && (tc->ship || tc->maxHit < 1e30f) &&
                               (!sv.on || (verb == Verb::Fight && !calm));
    if (c.LaneEscape() || c.TravelObjective() || !(c.Threat(sv.on) || hostileTarget))
        return std::nullopt;
    if (verb == Verb::Escort && tc && tc->ship && !Ctx::Hostile(*tc))
        return Make("threat-fight", "escort", tc->id, "escort", "the escort's ship under threat");
    // a placeholder's marked hostiles while an escort runs: the escort's (its own ship alive, else the survival lead)
    if (c.markFight)
    {
        u32 lead = 0;
        if (q.mode == "escort" && q.escorted)
            for (const SnapContact& x : s.contacts)
                if (x.id == q.escorted && x.ship && x.health != 0)
                    lead = x.id;
        if (!lead && sv.on)
            if (const SnapContact* l = c.SurvivalLead())
                lead = l->id;
        if (lead)
            return Make("threat-fight", "escort", lead, "fight", "the mark fight carried with the escort");
    }
    if (sv.on && verb == Verb::Fight && tc && tc->ship && Ctx::Hostile(*tc))
        return Make("threat-fight", "combat", tc->id, "fight", "survival: the objective's own hostile ship");
    return Make("threat-fight", "combat", 0, "fight", hostileTarget ? "the step's hostile target" : "a threat");
}

std::optional<Proposal> Rule_Escort(const Situation& sit, const Queue& q)
{
    const Ctx c(sit, q);
    const Snapshot& s = c.s;
    const SnapContact* tc = c.tc;
    if (!(c.verb == Verb::Escort && !(tc && tc->ship)))
        return std::nullopt;
    // (the escort memory as ObjectiveDecide updates it just before: a friendly ship target - none here, so the memory's)
    const SnapContact* best = nullptr;
    for (const SnapContact& x : s.contacts)
        if (x.ship && x.id == q.escortShip && x.health != 0)
            best = &x;
    if (!best) // the ship the objective names first, else the nearest friendly mark
        for (const SnapContact& x : s.contacts)
            if (x.ship && !Ctx::Hostile(x) && c.Pointed(x.id))
            {
                const bool cn = x.named, bn = best && best->named;
                if (!best || (cn && !bn) || (cn == bn && x.distance < best->distance))
                    best = &x;
            }
    if (!best)
        return std::nullopt;
    char d[64];
    snprintf(d, sizeof(d), "escort %u%s", best->id, best->named ? " (named)" : "");
    if (q.sub != "formation" && q.sub != "follow")
        return Make("escort", "formation", best->id, "formation", d);
    return Make("escort", q.sub == "formation" ? "formation" : "follow", best->id,
                q.sub == "formation" ? "formation" : "follow", d);
}

std::optional<Proposal> Rule_ShipTarget(const Situation& sit, const Queue& q)
{
    const Ctx c(sit, q);
    const SnapContact* tc = c.tc;
    if (!(tc && tc->ship))
        return std::nullopt;
    if (c.verb == Verb::Escort && q.sub != "formation")
        return Make("ship-target", "formation", tc->id, "formation", "the step's ship to escort");
    return Make("ship-target", "follow", tc->id, "follow", "the step's ship");
}

// ---------------------------------------------------------------- S5
namespace
{
// stayWithLead: beside the lead within follow_distance a rest, else with it
Proposal StayWithLead(const char* rule, const SnapContact& lead, float followDistance)
{
    char d[64];
    snprintf(d, sizeof(d), "survival: the lead %u at %.0f m", lead.id, lead.distance);
    if (lead.distance < followDistance)
        return Make(rule, "hold", 0, "survive-hold", d);
    return Make(rule, "escort", lead.id, "survive-stay", d);
}
} // namespace

std::optional<Proposal> Rule_Lane(const Situation& sit, const Queue& q)
{
    const Ctx c(sit, q);
    const SnapContact* tc = c.tc;
    const SnapLane* l = tc && c.verb == Verb::Dock ? c.LaneOf(tc->id) : nullptr;
    if (!l)
        return std::nullopt;
    u32 toward = 0;
    bool known = false;
    const u32 cands[2] = {l->next, l->prev};
    const int opens[2] = {l->openNext, l->openPrev};
    for (int i = 0; i < 2; ++i)
    {
        const int o = cands[i] ? opens[i] : -1;
        known |= o >= 0;
        if (o == 1 && !toward)
            toward = cands[i];
    }
    char d[96];
    if (!toward && known) // closed both ways: the mission opens it later - wait beside the ring
    {
        snprintf(d, sizeof(d), "lane %u closed both ways: waiting at it (%.0f m)", tc->id, tc->distance);
        if (tc->distance > 1500 && !q.arrived)
        {
            Proposal p = Make("lane", "travel", tc->id, "lane-wait", d);
            return ToObject(p, tc->id, true);
        }
        return Make("lane", "hold", 0, "lane-wait", d);
    }
    if (!toward)
        toward = l->next ? l->next : l->prev;
    snprintf(d, sizeof(d), "lane %u toward %u at %.0f m", tc->id, toward, tc->distance);
    if (tc->distance > 3000 && !q.arrived)
    {
        Proposal p = Make("lane", "travel", tc->id, "lane", d);
        return ToObject(p, tc->id, true);
    }
    Proposal p = Make("lane", "dock", tc->id, "lane", d);
    p.plan.kind = Plan::Kind::Dock, p.plan.toward = toward;
    return p;
}

std::optional<Proposal> Rule_MissionForm(const Situation& sit, const Queue& q)
{
    const Ctx c(sit, q);
    if (!(c.TravelObjective() && q.missionLeader))
        return std::nullopt;
    const SnapContact* lc = c.Find(q.missionLeader);
    if (!(lc && lc->health != 0 && Ctx::Friendly(*lc))) // (the old path forgets the leader: no later rule reads it)
        return std::nullopt;
    return Make("mission-form", "formation", lc->id, "mission-form", "the mission's named leader");
}

std::optional<Proposal> Rule_Pass(const Situation& sit, const Queue& q)
{
    const Ctx c(sit, q);
    if (!(c.verb == Verb::None || c.verb == Verb::Survive))
        return std::nullopt;
    const float r = c.PassRadius();
    if (!(r > 0))
        return std::nullopt;
    char d[64];
    snprintf(d, sizeof(d), "ring %u radius %.0f m: through it", c.tc->id, r);
    Proposal p = Make("pass", "travel", c.tc->id, "pass", d);
    p.keep = q.mode == "travel" && q.trPassRing == c.tc->id; // (the travel already flies through it)
    p.plan.kind = p.keep ? Plan::Kind::None : Plan::Kind::Pass, p.plan.radius = r;
    return p;
}

std::optional<Proposal> Rule_RouteLane(const Situation& sit, const Queue& q)
{
    const Ctx c(sit, q);
    const Snapshot& s = c.s;
    const SnapContact* tc = c.tc;
    const SnapLane* l = tc && c.RouteStep() ? c.LaneOf(tc->id) : nullptr;
    if (!l)
        return std::nullopt;
    // step 1's place: its first resolved ref (where the game places it), else its navmarker
    const SnapStep& e1 = s.entries[1];
    const u32 s1 = Ctx::FirstRef(e1);
    const bool haveS1 = s1 ? e1.refPlaced : e1.hasNav;
    const float* s1pos = s1 ? e1.refPos : e1.nav;
    u32 toward = 0;
    float best = 1e30f;
    const u32 cands[2] = {l->next, l->prev};
    const bool placed[2] = {l->nextPlaced, l->prevPlaced};
    const float* pos[2] = {l->nextPos, l->prevPos};
    for (int i = 0; i < 2; ++i)
    {
        if (!cands[i] || !haveS1 || !placed[i])
            continue;
        const float dd = Dist(pos[i], s1pos);
        if (dd < best)
            best = dd, toward = cands[i];
    }
    if (!toward)
        return std::nullopt;
    char d[96];
    snprintf(d, sizeof(d), "route lane %u toward %u (step 1 %u)", tc->id, toward, s1);
    if (tc->distance > 3000 && !q.arrived)
    {
        Proposal p = Make("route-lane", "travel", tc->id, "lane", d);
        return ToObject(p, tc->id, true);
    }
    Proposal p = Make("route-lane", "dock", tc->id, "lane", d);
    p.plan.kind = Plan::Kind::Dock, p.plan.toward = toward;
    return p;
}

std::optional<Proposal> Rule_Arrived(const Situation& sit, const Queue& q)
{
    if (!q.arrived)
        return std::nullopt;
    const Ctx c(sit, q);
    if (const SnapContact* lead = c.Lead())
        return StayWithLead("arrived", *lead, c.s.followDistance);
    return Make("arrived", "hold", 0, "rest", "this step's travel is done");
}

std::optional<Proposal> Rule_Route(const Situation& sit, const Queue& q)
{
    const Ctx c(sit, q);
    if (!c.RouteStep())
        return std::nullopt;
    const SnapStep& last = c.s.entries.back();
    const u32 lastObj = Ctx::FirstRef(last);
    if (!(lastObj || last.hasNav))
        return std::nullopt;
    char d[64];
    snprintf(d, sizeof(d), "%u steps, to the last (object %u)", static_cast<unsigned>(c.s.entries.size()), lastObj);
    Proposal p = Make("route", "travel", lastObj, "route", d);
    if (lastObj) // (its planet's docking ring when it has one - the snapshot's dock_ring of the entry's ref; docked for a Dock step)
    {
        const SnapContact* lc = c.Find(lastObj);
        return ToObject(p, lc && lc->dockRing ? lc->dockRing : lastObj, c.verb != Verb::Dock);
    }
    return ToPoint(p, last.nav, last.navSystem);
}

std::optional<Proposal> Rule_Waypoints(const Situation& sit, const Queue& q)
{
    const Ctx c(sit, q);
    const SnapContact* tc = c.tc;
    if (!tc)
        return std::nullopt;
    const std::vector<SnapWaypoint>& wl = c.s.waypoints;
    for (size_t i = 0; i + 1 < wl.size(); ++i)
    {
        if (wl[i].object != tc->id && Dist(wl[i].pos, tc->pos) > 1500)
            continue;
        char d[80];
        snprintf(d, sizeof(d), "the mission's waypoint list (%u entries, target is entry %u)",
                 static_cast<unsigned>(wl.size()), static_cast<unsigned>(i));
        Proposal p = Make("waypoints", "travel", tc->id, "route", d);
        p.plan = Plan();
        p.plan.kind = Plan::Kind::Waypoints, p.plan.noDock = c.verb != Verb::Dock;
        return p;
    }
    return std::nullopt;
}

std::optional<Proposal> Rule_Arrive(const Situation& sit, const Queue& q)
{
    const Ctx c(sit, q);
    const SnapContact* tc = c.tc;
    if (!tc)
        return std::nullopt;
    const u32 dest = tc->dockRing ? tc->dockRing : tc->id; // a planet: its docking ring
    // an airlock as the step's own target is a way through, whatever the verb
    const bool dock = c.verb == Verb::Dock || (!tc->ship && (tc->type & kTAirlock));
    Proposal p = Make("arrive", "travel", dest, dock ? "dock" : "arrive");
    return ToObject(p, dest, !dock);
}

std::optional<Proposal> Rule_NavWay(const Situation& sit, const Queue& q)
{
    const Ctx c(sit, q);
    const Snapshot& s = c.s;
    if (!s.step.hasNav)
        return std::nullopt;
    const SnapContact* way = nullptr; // an airlock / gate / hole the marker sits on (within 200 m), the nearest
    for (const SnapContact& x : s.contacts)
    {
        if (x.ship || !(x.type & (kTAirlock | kTJumpGate | kTJumpHole)))
            continue;
        if (Dist(x.pos, s.step.nav) < 200 && (!way || x.distance < way->distance))
            way = &x;
    }
    if (!way || Ctx::Hostile(*way))
        return std::nullopt;
    char d[64];
    snprintf(d, sizeof(d), "navmarker on a dockable object %u: through it", way->id);
    Proposal p = Make("nav-way", "travel", way->id, "dock", d);
    return ToObject(p, way->id, false);
}

std::optional<Proposal> Rule_Via(const Situation& sit, const Queue& q)
{
    const Snapshot& s = sit.facts;
    if (!(s.step.hasNav && q.viaNext && q.arriveRetries > 0 && s.pilotOk &&
          (!s.step.navSystem || s.step.navSystem == s.pilotSystem)))
        return std::nullopt;
    const int k = (q.arriveRetries - 1) % 4;
    char d[64];
    snprintf(d, sizeof(d), "via point %d before the marker", k);
    // 500 m off the straight line at its middle: up / down / left / right in turn (world up projected off the line, and
    // the line x up)
    float dv[3] = {s.step.nav[0] - s.pos[0], s.step.nav[1] - s.pos[1], s.step.nav[2] - s.pos[2]};
    const float dl = std::max(1.0f, std::sqrt(dv[0] * dv[0] + dv[1] * dv[1] + dv[2] * dv[2]));
    for (float& x : dv)
        x /= dl;
    float up[3] = {-dv[1] * dv[0], 1 - dv[1] * dv[1], -dv[1] * dv[2]};
    const float ul = std::sqrt(up[0] * up[0] + up[1] * up[1] + up[2] * up[2]);
    if (ul < 0.1f)
        up[0] = 1, up[1] = 0, up[2] = 0;
    else
        for (float& x : up)
            x /= ul;
    const float side[3] = {dv[1] * up[2] - dv[2] * up[1], dv[2] * up[0] - dv[0] * up[2], dv[0] * up[1] - dv[1] * up[0]};
    const float* dir = k < 2 ? up : side;
    const float sgn = (k % 2) ? -1.0f : 1.0f;
    float via[3];
    for (int i = 0; i < 3; ++i)
        via[i] = (s.pos[i] + s.step.nav[i]) * 0.5f + dir[i] * sgn * 500.0f;
    Proposal p = Make("via", "travel", 0, "via", d);
    ToPoint(p, via, s.pilotSystem);
    p.plan.inVia = true;
    return p;
}

std::optional<Proposal> Rule_Navmarker(const Situation& sit, const Queue& q)
{
    const Ctx c(sit, q);
    const Snapshot& s = c.s;
    if (!s.step.hasNav)
        return std::nullopt;
    Proposal p = Make("navmarker", "travel", 0, "navmarker");
    // fly-through: the script's own condition for this step - the player within R of a point that holds the marker - aims
    // beyond the marker along the approach; a marker near the sphere's edge: its near side, flown through (the ini view:
    // AdvanceGoals of the step's text, unfiltered)
    float dest[3] = {s.step.nav[0], s.step.nav[1], s.step.nav[2]};
    Plan fly;
    if (c.KnowsHidden() && s.scriptGoals && s.pilotOk && (!s.step.navSystem || s.step.navSystem == s.system))
        for (const SnapGoal& sg : c.IniGoals())
        {
            float gp[3];
            if (sg.subject != "player" || !(sg.kind == 2 || (sg.kind == 1 && sg.placed)))
                continue;
            PlacedAt(sg, gp);
            if (Dist(s.step.nav, gp) > sg.dist)
                continue; // the marker must lie inside the condition's sphere
            const float tl = Dist(gp, s.pos);
            if (tl <= sg.dist)
                break; // inside already
            float ahead = 0;
            if (Dist(s.step.nav, gp) > sg.dist * 0.5f)
            {
                for (int i = 0; i < 3; ++i)
                    dest[i] = gp[i] - (gp[i] - s.pos[i]) / tl * sg.dist * 0.7f;
                ahead = sg.dist * 0.7f;
            }
            const float dd[3] = {dest[0] - s.pos[0], dest[1] - s.pos[1], dest[2] - s.pos[2]};
            const float dl = std::max(1.0f, std::sqrt(dd[0] * dd[0] + dd[1] * dd[1] + dd[2] * dd[2]));
            const float dir[3] = {dd[0] / dl, dd[1] / dl, dd[2] / dl};
            Fly(fly, dir, sg.dist, ahead);
            FlyGoal(fly, gp, sg.dist);
            break;
        }
    ToPoint(p, dest, s.step.navSystem);
    if (fly.fly)
    {
        Fly(p.plan, fly.flyDir, fly.flyRadius, fly.flyAhead);
        FlyGoal(p.plan, fly.flyGoal, fly.flyGoalR);
    }
    return p;
}

std::optional<Proposal> Rule_Rest(const Situation& sit, const Queue& q)
{
    const Ctx c(sit, q);
    if (const SnapContact* lead = c.Lead())
        return StayWithLead("rest", *lead, c.s.followDistance);
    return Make("rest", "hold", 0, "rest");
}

namespace
{
using RuleFn = std::optional<Proposal> (*)(const Situation&, const Queue&);
Proposal First(const RuleFn* rules, size_t n, const Situation& sit, const Queue& q)
{
    for (size_t i = 0; i < n; ++i)
        if (std::optional<Proposal> p = rules[i](sit, q))
            return *p;
    Proposal none;
    none.rule = "none";
    return none;
}
// ObjectiveDecide's order (ObjectiveSupervise's holds first)
const RuleFn kRules[] = {Rule_Held,      Rule_Leash,         Rule_MarkedLoot, Rule_TractorExit, Rule_Jump,
                         Rule_Airlock,   Rule_ScriptDestroy, Rule_DockEntry,  Rule_ScriptGoal,
                         // S4
                         Rule_PreStage,  Rule_PreStageNext,  Rule_MarkFight,  Rule_PreAggro,    Rule_SurvivalStage,
                         Rule_Survival,  Rule_ThreatFight,   Rule_Escort,     Rule_ShipTarget,
                         // S5
                         Rule_Lane,      Rule_MissionForm,   Rule_Pass,       Rule_RouteLane,   Rule_Arrived,
                         Rule_Route,     Rule_Waypoints,     Rule_Arrive,     Rule_NavWay,      Rule_Via,
                         Rule_Navmarker, Rule_Rest};
constexpr size_t kS3Rules = 9;
} // namespace

Proposal DecideS3(const Situation& sit, const Queue& q) { return First(kRules, kS3Rules, sit, q); }
Proposal Decide(const Situation& sit, const Queue& q)
{
    return First(kRules, sizeof(kRules) / sizeof(kRules[0]), sit, q);
}
} // namespace situation
