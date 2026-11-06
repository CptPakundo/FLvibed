// Advanced Autopilot - objective mode: what the current objective asks. Shared state and declarations in
// autopilot_impl.h, the module API in autopilot.h.
#include "autopilot_impl.h"
#include "game/flhash.h"
#include "game/mission_live.h"
#include "autopilot_ashride.h"
#include "lookahead.h"
#include "situation.h"
#include "situation_exec.h"
#include "autopilot_objective_impl.h"

namespace autopilot
{
// ------------------------------------------------------------------ A37 S7: plan mode (situation_arbiter)
// With the switch on, ObjectiveDecide's body still runs - the old path as the arbiter's oracle and its trip planner (the
// geometry, the fly-through, the memory the supervisor and the travel read) - but in plan mode: every start it would
// make, and its ObjectiveStart, is recorded here instead of made. After the body the executor (ArbiterExecute) acts on
// the arbiter's decision (Decide over the goal queue, situation_snap.cpp Arbitrate): the body's own plan when the arbiter
// agrees with it, else the rules' plan (situation::Plan); every side effect through the game-first gate
// (situation_exec.h CheckSideEffect). With the switch off nothing here is ever set: each helper starts at once, as before.
struct Pending
{
    bool exit = false; // the body reached an exit (ShadowExit / ShadowAfter)
    std::string rule;
    Mode m = Mode::Off; // the exit's decision (the shadow's `old`)
    u32 target = 0;
    Action action = Action::None;
    bool keep = false;
    bool objStart = false; // its ObjectiveStart call - mode / target / action, the provisional ok
    Mode om = Mode::Off;
    u32 ot = 0;
    Action oa = Action::None;
    bool ook = false;
    situation::Plan plan; // the start it asked for (None: none)
    Mode pm = Mode::Off;  // the plan's mode (Plan::Kind::Mode)
};
static bool s_plan = false;
static Pending s_pend;

static void PlanTravel(u32 object, const float* point, u32 system, bool noDock)
{
    s_pend.plan = situation::Plan();
    situation::Plan& p = s_pend.plan;
    p.kind = situation::Plan::Kind::Travel, p.object = object, p.system = system, p.noDock = noDock;
    p.hasPoint = point != nullptr;
    if (point)
        memcpy(p.point, point, sizeof(p.point));
}

// ------------------------------------------------------------------ objective mode
// Starts a travel (ap start travel's): to an object of this system, or a point of a system (0 = this one).
bool StartTravel(u32 object, const float* point, u32 system, bool noDock)
{
    if (s_plan) // (plan mode: recorded; the executor's own call does the rest, the kept-trip tests included)
        return PlanTravel(object, point, system, noDock), true;
    // A trip to where the running one already goes is that trip (M12 gen 3 -> airlock: the
    // pre-stage trip flew at 200 m/s toward the airlock top; "Fly to the Waypoint" 7.5 s later aimed 47 m from it, and the
    // restart braked to 80, paused 1.5 s to plot and planned a 3.2 km way back round the lattice - 19.5 s for ~1.5 km)
    if (point && !object && g.mode == Mode::Travel && g.tr.hasDest && !g.tr.destObject && g.tr.noDock == noDock &&
        (system ? system : fl::PlayerSystem()) == (g.tr.destSystem ? g.tr.destSystem : fl::PlayerSystem()))
    {
        const float d[3] = {point[0] - g.tr.dest[0], point[1] - g.tr.dest[1], point[2] - g.tr.dest[2]};
        if (Len3(d) < 150.0f)
        {
            events::Push("autopilot objective trip kept: the new destination lies %.0f m from the running one's",
                         Len3(d));
            return true;
        }
    }
    // ...and a trip to the object the running one goes to (M12 Omicron Minor hole: "Take the Jump Hole" came
    // 2 s after our request was granted and the game's dock had the ship; the restart stopped it 147 m out among 10
    // Nomads - every run restarted there, one died in 4 s)
    if (object && !point && g.mode == Mode::Travel && g.tr.destObject == object && g.tr.noDock == noDock)
    {
        events::Push("autopilot objective trip kept: already on the way to %u", object);
        return true;
    }
    State::Travel t;
    t.noDock = noDock;
    t.destObject = object;
    if (point)
    {
        t.hasDest = true;
        memcpy(t.dest, point, sizeof(t.dest));
        t.destSystem = system;
    }
    else
        t.hasDest = space::ObjectPos(object, t.dest);
    if (!t.hasDest)
        return false;
    t.phase = State::Travel::Plot;
    if (!Start(Mode::Travel, t.destObject, nullptr))
        return false;
    g.tr = t;
    return true;
}

// A ring the objective wants flown through (M06's race: "Beat Hovis to the Final Gate" and the gates' blank-titled
// objectives point at Br05_track_ring_*, passed by Cnd_DistCircle - 22 Oct 2026, the probe arrived beside ring 2 and
// waited; Hovis won both races, race 2's 30 s gate timer failed the mission): a satellite-class solar that is no lane
// ring and no dock, large enough to fly through (track ring: physical radius 88 m; a buoy is a satellite too, ~2.4 m: never through one). Its radius, else 0.
float PassRingRadius(const space::Contact& c)
{
    if (!c.id || c.ship || c.dock || !(c.type & 0x10u) || (c.type & space::kTradelaneRing))
        return 0;
    u32 next = 0, prev = 0;
    if (space::LaneLinks(c.id, next, prev))
        return 0;
    for (const space::Obstacle& o : space::Obstacles(c.pos, 50))
        if (o.id == c.id)
            // track ring 88 m physical, a nav buoy ~2.4; nothing bigger than a ring can be "flown through" (M13 22 Oct
            // 2026: the Nomad City, a satellite-class solar, read as a race ring - braked at 0 m/s for 25 min)
            return o.radius >= 30 && o.radius <= 300 && !HostileContact(c) ? o.radius : 0;
    return 0;
}

bool StartPass(u32 ring, float radius)
{
    State::Travel t;
    t.destObject = ring;
    t.noDock = true;
    t.hasDest = space::ObjectPos(ring, t.dest);
    t.passRing = ring;
    t.passRadius = radius;
    t.phase = State::Travel::Fly;
    if (!t.hasDest || !Start(Mode::Travel, ring, nullptr))
        return false;
    g.tr = t;
    events::Push("autopilot objective pass ring=%u radius=%.0f", ring, radius);
    return true;
}


// Under a survival intent the objective mode takes no free fights (M04 verified from the 2020
// footage: "fast-and-cheap survival" to Benford - no missile, no CM, 0 of 14 nanobots / batteries, no hull damage,
// ~280 s; our free fights killed 25-32 ships in ~520 s, and the script answers every wing destroyed with its backup).
// Only attackers this close are fought; with a friendly mission ship near (Juni), the stance escorts her instead.

ObjVerb ObjectiveVerb(u32 textId)
{
    std::string s = textId ? events::IdsText(textId) : std::string();
    for (char& ch : s)
        ch = static_cast<char>(tolower(static_cast<unsigned char>(ch)));
    // a word (or phrase) starting at a word boundary: "land " inside "Rheinland Agents" made M07's "Destroy the
    // Rheinland Agents" a dock order
    const auto has = [&](const char* w)
    {
        for (size_t at = s.find(w); at != std::string::npos; at = s.find(w, at + 1))
            if (at == 0 || !isalpha(static_cast<unsigned char>(s[at - 1])))
                return true;
        return false;
    };
    // "Enter the Dyson Sphere" (M13: its target is the sphere's airlock - arriving beside it left the ship outside)
    // "Meet Juni in the Manhattan Bar" (FT1 -> M02 chain: arrived beside the planet, never landed): a bar
    // is inside a base
    // (a whole word: "Head to the Barrier", M13, is no bar)
    const auto word = [&](const char* w)
    {
        const size_t n = strlen(w);
        for (size_t at = s.find(w); at != std::string::npos; at = s.find(w, at + 1))
            if ((at == 0 || !isalpha(static_cast<unsigned char>(s[at - 1]))) &&
                (at + n >= s.size() || !isalpha(static_cast<unsigned char>(s[at + n]))))
                return true;
        return false;
    };
    // ("Enter Formation with the USV Brandt", M01a: a formation, not a dock - the chain failed "never entered formation")
    if (has("dock") || has("land ") || (has("enter ") && !has("formation")) || word("bar"))
        return ObjVerb::Dock;
    // before "defend": "Defend Yourself!" asks for survival (M04 step 2: the wings keep coming), not an escort
    if (has("defend yourself") || has("survive") || has("stay alive") || has("escape"))
        return ObjVerb::Survive;
    if (has("escort") || has("protect") || has("defend") || has("guard") || has("stay with") || has("formation"))
        return ObjVerb::Escort;
    if (has("destroy") || has("kill") || has("eliminate") || has("attack") || has("disable"))
        return ObjVerb::Fight;
    return ObjVerb::None;
}

bool HostileContact(const space::Contact& c)
{
    float att = 0;
    return c.ship ? c.attitude <= -0.6f : (space::AttitudeToPlayer(c.id, att) && att <= -0.6f);
}

// The objective asks for a destruction: a Fight verb in any of its entries' texts (a job's entry 0 is a blank step,
// "Destroy %L0v3!" comes as entry 1 - Destroy Contraband, HDE).
bool ObjectiveAsksDestroy()
{
    ObjectiveStep steps[8];
    const int n = HostObjectiveSteps(steps, 8);
    for (int i = 0; i < n; ++i)
        if (ObjectiveVerb(steps[i].textId) == ObjVerb::Fight)
            return true;
    ObjectiveStep st = {};
    return HostObjectiveNow(&st) && ObjectiveVerb(st.textId) == ObjVerb::Fight;
}

// Loot the mission marks while its objective asks for a destruction is the thing to destroy, never to collect (the owner,
// 26 Oct 2026, Destroy Contraband: "the job wants the cargo DESTROYED ... tries to LOOT it instead" - TractorMarkedLoot
// pulled each marked pod in, the job dropped a new one, 7 times in 12 s). The nearest such object; 0 none.
u32 DestroyMarkedLoot(const std::vector<space::Contact>& all, float* dist)
{
    u32 mk[32];
    const int mn = MissionObjectIds(mk, 32, all);
    const space::Contact* best = nullptr;
    for (const space::Contact& c : all) // (nearest first)
        if ((c.type & space::kLoot) && std::find(mk, mk + mn, c.id) != mk + mn && (!best || c.distance < best->distance))
            best = &c;
    if (!best || !ObjectiveAsksDestroy())
        return 0;
    if (dist)
        *dist = best->distance;
    return best->id;
}

// pre_aggro: the nearest live ship the script is about to turn hostile, within the switch's range (Contact::preHostile);
// 0 none
u32 PreHostileNear(const std::vector<space::Contact>& all)
{
    for (const space::Contact& c : all) // (nearest first)
        if (c.ship && c.preHostile && c.health != 0)
            return c.id;
    return 0;
}

// Our side: a friendly ship, or a station / platform / ring whose reputation is friendly to the player.
bool FriendlyContact(const space::Contact& c)
{
    float att = 0;
    return c.ship ? c.attitude > 0 : (space::AttitudeToPlayer(c.id, att) && att > 0);
}

// A fight the objective mode must take now: a hostile the mission points at within 6 km (the harness's mission battle
// reach), a hostile ship attacking us within 3 km, or one attacking our side (a friendly ship, station, platform)
// within the battle's reach (M03 Battle of Willard: bombers spawned 7 km out attacking the station were
// seen and not engaged). The game's maxHit cross-check stays with the target choice.
bool ObjectiveThreat(const std::vector<space::Contact>& all)
{
    u32 ids[32];
    const int n = MissionObjectIds(ids, 32, all);
    for (const space::Contact& c : all)
    {
        // (pre_aggro's flagged ships have their own branch in ObjectiveDecide and their own 3 s interrupt: counted here,
        // every non-travel mode re-decided at 0.5 s with one in range - review, session 12)
        if (c.health == 0 || !HostileContact(c) || c.preHostile)
            continue;
        const bool pointed = std::find(ids, ids + n, c.id) != ids + n;
        // survival: only what attacks us up close (not the battle around - its friendlies die by script in M04) - and
        // under a fight objective the ships it marks (M04 Mactan's Rheinland wing, s12d)
        if (g_obj.survival)
        {
            if (c.ship && c.targetsPlayer && c.distance < kSurvivalReach)
                return true;
            if (g_obj.fightObjective && pointed && c.ship && c.distance < 6000)
                return true;
            continue;
        }
        if ((pointed && c.distance < 6000 && (c.ship || c.maxHit < 1e30f)) ||
            (c.ship && c.targetsPlayer && c.distance < 3000))
            return true;
        if (c.ship && c.targetId && c.distance < kBattleReach)
            for (const space::Contact& f : all)
                if (f.id == c.targetId && f.id != c.id && FriendlyContact(f))
                    return true;
    }
    return false;
}

// A destroy condition met: every object it names was seen alive and is gone now (only objects seen alive count: M13's city
// generators spawn after the step begins - "none alive" then is no kill).
// (one seen in the contact list and gone from it is dead - a destroyed solar may still answer ObjectPos: M12's generator 3,
// the condition never read as met)
static std::set<u32> s_seenAlive, s_seenListed;
void SeenDestroyed(std::vector<u32>& alive, std::vector<u32>& listed)
{
    alive.assign(s_seenAlive.begin(), s_seenAlive.end());
    listed.assign(s_seenListed.begin(), s_seenListed.end());
}
bool DestroyFired(const mission::Goal& sg, const std::vector<space::Contact>& all)
{
    bool any = false, allSeen = !sg.objects.empty();
    float p[3];
    for (u32 id : sg.objects)
    {
        const auto ci = std::find_if(all.begin(), all.end(), [&](const space::Contact& c) { return c.id == id; });
        if (ci != all.end())
            s_seenListed.insert(id);
        const bool live = ci != all.end() ? ci->health != 0 : !s_seenListed.count(id) && space::ObjectPos(id, p);
        if (live)
            s_seenAlive.insert(id);
        any = any || live;
        allSeen = allSeen && s_seenAlive.count(id);
    }
    return !any && allSeen;
}

// Under the survival stance: the friendly ship the mission brought along (the objective's last friendly ship, or the
// nearest friendly mission object - Juni), within 6 km; 0 none.
u32 SurvivalLead(const std::vector<space::Contact>& all)
{
    u32 mids[32];
    const int mn = MissionObjectIds(mids, 32, all);
    const space::Contact* lead = nullptr;
    for (const space::Contact& c : all)
        if (c.ship && c.health != 0 && FriendlyContact(c) && c.distance < 6000 &&
            (c.id == g_obj.escortShip || std::find(mids, mids + mn, c.id) != mids + mn) &&
            (!lead || c.distance < lead->distance))
            lead = &c;
    return lead ? lead->id : 0;
}

// Start a mode for the objective - or keep it when it already runs with that target (a fight goes on across the
// objective changes it causes: "Awaiting Mission Objective", "Destroy ..." restarted it twice, M03 19 Oct 2026).
bool StartOrKeep(Mode m, u32 target)
{
    if (s_plan) // (plan mode, A37 S7: recorded - the executor's call keeps or starts)
    {
        s_pend.plan = situation::Plan();
        s_pend.plan.kind = situation::Plan::Kind::Mode, s_pend.plan.object = target, s_pend.pm = m;
        return true;
    }
    if (g.mode == m && m != Mode::Travel && (g.target == target || (m == Mode::Combat && !target)))
        return true;
    return Start(m, target, nullptr);
}

// ObjectiveDecide's other starts, each recorded in plan mode (A37 S7) and made at once otherwise, exactly as the body
// made them before: a hold at a point, the lane ride's dock, a ring passed, an object shot, the mission's waypoint list,
// and a started trip's fly-through
bool DecHoldAt(const float* q)
{
    if (!s_plan)
        return Start(Mode::Hold, 0, q);
    s_pend.plan = situation::Plan();
    s_pend.plan.kind = situation::Plan::Kind::HoldAt, s_pend.plan.hasPoint = true;
    memcpy(s_pend.plan.point, q, sizeof(s_pend.plan.point));
    return true;
}
bool DecDock(u32 ring, u32 toward)
{
    if (!s_plan)
    {
        const bool ok = Start(Mode::Dock, ring, nullptr);
        if (ok)
            g.dock.laneTowardPlan = toward, g.dock.fastForced = true;
        return ok;
    }
    s_pend.plan = situation::Plan();
    s_pend.plan.kind = situation::Plan::Kind::Dock, s_pend.plan.object = ring, s_pend.plan.toward = toward;
    return true;
}
bool DecPass(u32 ring, float radius)
{
    if (!s_plan)
        return StartPass(ring, radius);
    s_pend.plan = situation::Plan();
    s_pend.plan.kind = situation::Plan::Kind::Pass, s_pend.plan.object = ring, s_pend.plan.radius = radius;
    return true;
}
bool DecShoot(u32 id)
{
    if (!s_plan)
        return StartShootObject(id);
    s_pend.plan = situation::Plan();
    s_pend.plan.kind = situation::Plan::Kind::Shoot, s_pend.plan.object = id;
    return true;
}
bool DecWaypoints(bool noDock)
{
    if (s_plan)
    {
        s_pend.plan = situation::Plan();
        s_pend.plan.kind = situation::Plan::Kind::Waypoints, s_pend.plan.noDock = noDock;
        return true;
    }
    State::Travel t;
    t.noDock = noDock;
    t.phase = State::Travel::Plan;
    const bool ok = Start(Mode::Travel, 0, nullptr);
    if (ok)
        g.tr = t;
    return ok;
}
void DecFly(bool ok, const float* dir, float radius, float ahead)
{
    if (s_plan)
    {
        s_pend.plan.fly = true, s_pend.plan.flyRadius = radius, s_pend.plan.flyAhead = ahead;
        memcpy(s_pend.plan.flyDir, dir, sizeof(s_pend.plan.flyDir));
        return;
    }
    if (ok)
        memcpy(g.tr.flyDir, dir, sizeof(g.tr.flyDir)), g.tr.flyRadius = radius, g.tr.flyAhead = ahead;
}

void ObjectiveStart(Mode m, u32 target, Action action, bool ok)
{
    if (s_plan) // (plan mode, A37 S7: the executor makes the call with the real start's result)
    {
        s_pend.objStart = true, s_pend.om = m, s_pend.ot = target, s_pend.oa = action, s_pend.ook = ok;
        return;
    }
    g_obj.sub = ok ? m : Mode::Off;
    g_obj.subTarget = target;
    g_obj.subSince = gameclock::NowMs64();
    // (an enum; as a char[16], a 17-char name with strcpy_s was a fail-fast crash)
    g_obj.action = action;
    // the start's Action, once per start of its own (a kept trip / running mode starts nothing): the `autopilot start
    // mode=` line is printed inside Start, before the action is known - this line follows it (the retro: a
    // trip's start did not say which rule sent it). Its own prefix: no `autopilot start mode=` parser sees it.
    static u32 seenStart = 0;
    if (ok && StartSeq() != seenStart && g.mode == m)
    {
        seenStart = StartSeq();
        events::Push("autopilot objective start action=%s mode=%s target=%u", ActionName(action), ModeName(m), target);
    }
}

// situation_shadow (A37 S3-S5, a measurement): an exit of ObjectiveDecide the arbiter's ported rules cover (S3: up to the
// script goal; S4: the pre-stage to the ship target; S5: the rest - lanes, passes, routes, arrivals, navmarkers, rest:
// every exit) - after the old path's
// own start (exactly as before), the rules' proposal against it (situation_snap.cpp ShadowCompare). `keep`: the branch
// kept the running sub-mode. Off: one bool test.
void ShadowAfter(const char* rule, Mode m, u32 target, Action action, bool keep)
{
    if (s_plan) // (plan mode, A37 S7: the exit's decision - the arbiter's own comparison replaces the shadow's)
    {
        s_pend.exit = true, s_pend.rule = rule, s_pend.m = m, s_pend.target = target, s_pend.action = action;
        s_pend.keep = keep;
        return;
    }
    if (g_set.situationShadow)
        situation::ShadowCompare(rule, ModeName(m), target, ActionName(action), keep, keep || g_obj.sub != Mode::Off);
}
void ShadowExit(const char* rule, Mode m, u32 target, Action action, bool ok, bool keep)
{
    ObjectiveStart(m, target, action, ok);
    ShadowAfter(rule, m, target, action, keep);
}

u32 s_lastReal = 0; // ObjectiveDecide's `lastReal` (a function static until A37 S4)
u32 ObjectiveLastReal()
{
    return s_lastReal;
}
// (A37 S7, situation_arbiter: the goal queue's story memory written back before each decision - the queue is its only
// keeper, so a load, which forgets the queue, forgets it too)
void SetObjectiveLastReal(u32 v)
{
    s_lastReal = v;
}

// Back toward a leash's ship: our trip to where it is now (sphere avoidance), not the game's follow, which flies straight
// at cruise (M10 exit field: the follow hit a big asteroid at ~380 m/s, hull 0.98 -> dead, 1239 s). True while
// such a trip runs and still ends within 1 km of the ship.
bool LeashTripRuns(const space::Contact& lc)
{
    if (g.mode != Mode::Travel || g_obj.action != Action::Leash || !g.tr.hasDest || g.tr.destObject)
        return false;
    const float d[3] = {lc.pos[0] - g.tr.dest[0], lc.pos[1] - g.tr.dest[1], lc.pos[2] - g.tr.dest[2]};
    return Len3(d) < 1000.0f;
}
void LeashBack(const space::Contact& lc)
{
    ObjectiveStart(Mode::Travel, 0, Action::Leash, StartTravel(0, lc.pos, 0, true));
}


// ------------------------------------------------------------------ A37 S7: the executor (situation_arbiter)
static Action ActionFromName(const std::string& s)
{
    for (int i = 0; i <= static_cast<int>(Action::LeadStall); ++i)
        if (s == ActionName(static_cast<Action>(i)))
            return static_cast<Action>(i);
    return Action::None;
}

// A plan started (not in plan mode): each kind as the body's own helper starts it
static bool ExecPlan(const situation::Plan& p, Mode m)
{
    using K = situation::Plan::Kind;
    switch (p.kind)
    {
    case K::None: return true;
    case K::Mode: return StartOrKeep(m, p.object);
    case K::Travel:
    {
        const bool ok = StartTravel(p.object, p.hasPoint ? p.point : nullptr, p.system, p.noDock);
        if (p.fly)
            DecFly(ok, p.flyDir, p.flyRadius, p.flyAhead);
        return ok;
    }
    case K::HoldAt: return DecHoldAt(p.point);
    case K::Dock: return DecDock(p.object, p.toward);
    case K::Pass: return DecPass(p.object, p.radius);
    case K::Shoot: return DecShoot(p.object);
    case K::Waypoints: return DecWaypoints(p.noDock);
    default: return false;
    }
}

// What the gate reads of the game (situation_exec.h GateFacts)
static situation::GateFacts GateFactsNow()
{
    situation::GateFacts f;
    space::Pilot p;
    f.inSpace = space::ReadPilot(p);
    if (f.inSpace)
        f.lane = p.lane, f.jumping = p.jumping, f.leader = p.leader;
    f.gameDocks = GameDocksUs();
    u32 obj = 0;
    int index = 0;
    f.dockInProgress = f.inSpace && ai::DockPort(obj, index);
    f.dockPortObject = f.dockInProgress ? obj : 0;
    f.mode = ModeName(g.mode), f.target = g.target;
    // (our request is out and the game holds no port for it yet: no answer that lets us in)
    f.dockReplyPending = g.mode == Mode::Dock && g.dock.fastRequested && !f.dockInProgress;
    return f;
}

// The exit's writes when the executor plans the proposal itself (the body took another exit: its writes were its own)
static void ApplyWrites(const situation::Plan& p)
{
    if (p.travelObjective >= 0)
        g_obj.travelObjective = p.travelObjective == 1;
    if (p.fightObjective >= 0)
    {
        g_obj.fightObjective = p.fightObjective == 1;
        g_obj.scriptMarkN = 0;
        for (const u32 id : p.scriptMarks)
            if (g_obj.scriptMarkN < 8)
                g_obj.scriptMarks[g_obj.scriptMarkN++] = id;
    }
    if (p.setGoalTrigger)
        g_obj.goalTrigger = p.goalTrigger;
    if (p.flyGoalR > 0)
        memcpy(g_obj.flyGoal, p.flyGoal, sizeof(g_obj.flyGoal)), g_obj.flyGoalR = p.flyGoalR;
    if (p.preStageTarget)
        g_obj.preStageTarget = p.preStageTarget;
    if (p.inVia)
        g_obj.inVia = true, g_obj.viaNext = false;
}

// After the body (plan mode): the arbiter decides, the executor starts it through the game-first gate, then the
// decision's ObjectiveStart with the real start's result. Events: `side-effect <kind> ...` per start or refusal (the fuzz
// oracle, S8) and one `situation arbiter agree|differ <proposal> old=... plan=agree|<diff> ...` per decision.
static void ArbiterExecute()
{
    if (!s_pend.exit) // (no exit reached: the coast guard, the decide-loop guard)
        return;
    const Pending o = s_pend;
    s_pend = Pending();
    situation::Decided old;
    old.rule = o.rule, old.mode = ModeName(o.m), old.target = o.target, old.action = ActionName(o.action), old.keep = o.keep;
    const situation::Arbitration a = situation::Arbitrate(old);
    if (!a.have) // (no memory of the decision's start - the switch went on during it: the old exit as it was)
    {
        const bool ok = ExecPlan(o.plan, o.pm);
        if (o.objStart)
            ObjectiveStart(o.om, o.ot, o.oa, o.plan.kind == situation::Plan::Kind::None ? o.ook : ok);
        return;
    }
    const situation::Proposal& p = a.p;
    // the plan: the body's own when it took the same decision (its geometry is today's), else the rules'
    const situation::Plan& plan = a.agree ? o.plan : p.plan;
    const Mode pm = a.agree ? o.pm : ModeFromName(p.mode);
    const situation::StartArgs sa = situation::StartArgsOf(p);
    const bool objStart = a.agree ? o.objStart : sa.objectiveStart;
    const Mode om = a.agree ? o.om : ModeFromName(sa.mode);
    const u32 ot = a.agree ? o.ot : sa.target;
    const Action oa = a.agree ? o.oa : ActionFromName(sa.action);
    bool ok = a.agree ? o.ook : true;
    if (plan.kind != situation::Plan::Kind::None)
    {
        const situation::Gate gate = situation::CheckSideEffect(GateFactsNow(), plan, ModeName(pm));
        switch (gate.verdict)
        {
        case situation::GateVerdict::Go: ok = ExecPlan(plan, pm); break;
        case situation::GateVerdict::Keep: ok = true; break;
        case situation::GateVerdict::Adopt: ok = StartAdopt(pm, plan.object); break;
        default: ok = false; break;
        }
        events::Push("%s", situation::SideEffectLine(gate, plan.object ? plan.object : p.target, a.step, p.rule).c_str());
    }
    if (!a.agree)
        ApplyWrites(p.plan);
    if (objStart)
        ObjectiveStart(om, ot, oa, ok);
    const std::string pd = situation::PlanDiff(o.plan, p.plan);
    events::Push("situation arbiter %s %s old=%s,%s,%u,%s,keep=%d plan=%s ok=%d step=0x%08x ms=%.2f%s%s",
                 a.agree ? "agree" : "differ", situation::ProposalText(p).c_str(), o.rule.c_str(), ModeName(o.m),
                 o.target, ActionName(o.action), o.keep ? 1 : 0, pd.empty() ? "agree" : pd.c_str(), ok ? 1 : 0, a.step,
                 a.ms, p.detail.empty() ? "" : " why=", p.detail.c_str());
}

// The objective mode's decision. Switch off: the body, exactly as before. On (A37 S7): the first decision after an
// activation resumes the paused queue (D10), the story memory comes from the queue, the body runs in plan mode, the
// executor acts on the arbiter's decision.
void ObjectiveDecide(const char* reason)
{
    if (!g_set.situationArbiter)
        return DecideBody(reason);
    if (!CoastActive())
        situation::ArbiterResume();
    SetObjectiveLastReal(situation::ArbiterLastReal()); // (the queue is the only memory: a load forgets it)
    s_pend = Pending();
    s_plan = true;
    DecideBody(reason);
    s_plan = false;
    ArbiterExecute();
}

} // namespace autopilot
