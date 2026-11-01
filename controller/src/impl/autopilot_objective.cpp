// Advanced Autopilot - objective mode: what the current objective asks. Part of autopilot.cpp until 21 Oct 2026; shared state and declarations in
// autopilot_impl.h, the module API in autopilot.h.
#include "autopilot_impl.h"
#include "game/flhash.h"
#include "game/mission_live.h"
#include "autopilot_ashride.h"
#include "lookahead.h"
#include "situation.h"
#include "situation_exec.h"

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
static bool StartTravel(u32 object, const float* point, u32 system, bool noDock)
{
    if (s_plan) // (plan mode: recorded; the executor's own call does the rest, the kept-trip tests included)
        return PlanTravel(object, point, system, noDock), true;
    // A trip to where the running one already goes is that trip (session 8, 23 Oct 2026, M12 gen 3 -> airlock: the
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
    // ...and a trip to the object the running one goes to (session 8, M12 Omicron Minor hole: "Take the Jump Hole" came
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
static bool HostileContact(const space::Contact& c);

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

// What the objective's words ask beyond "go there" (the words only refine what the target's kind decides: a dock, an
// escort, a fight; English text - other languages fall back to the kind alone).
enum class ObjVerb
{
    None,
    Dock,
    Escort,
    Fight,
    Survive // "Defend Yourself!", "Survive", "Escape": staying alive is the task, kills are not
};

// Under a survival intent the objective mode takes no free fights (user, 20 Oct 2026, M04 verified from the 2020
// footage: "fast-and-cheap survival" to Benford - no missile, no CM, 0 of 14 nanobots / batteries, no hull damage,
// ~280 s; our free fights killed 25-32 ships in ~520 s, and the script answers every wing destroyed with its backup).
// Only attackers this close are fought; with a friendly mission ship near (Juni), the stance escorts her instead.

static ObjVerb ObjectiveVerb(u32 textId)
{
    std::string s = textId ? events::IdsText(textId) : std::string();
    for (char& ch : s)
        ch = static_cast<char>(tolower(static_cast<unsigned char>(ch)));
    // a word (or phrase) starting at a word boundary: "land " inside "Rheinland Agents" made M07's "Destroy the
    // Rheinland Agents" a dock order (22 Oct 2026)
    const auto has = [&](const char* w)
    {
        for (size_t at = s.find(w); at != std::string::npos; at = s.find(w, at + 1))
            if (at == 0 || !isalpha(static_cast<unsigned char>(s[at - 1])))
                return true;
        return false;
    };
    // "Enter the Dyson Sphere" (M13: its target is the sphere's airlock - arriving beside it left the ship outside)
    // "Meet Juni in the Manhattan Bar" (FT1 -> M02 chain, 22 Oct 2026: arrived beside the planet, never landed): a bar
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

static bool HostileContact(const space::Contact& c)
{
    float att = 0;
    return c.ship ? c.attitude <= -0.6f : (space::AttitudeToPlayer(c.id, att) && att <= -0.6f);
}

// The objective asks for a destruction: a Fight verb in any of its entries' texts (a job's entry 0 is a blank step,
// "Destroy %L0v3!" comes as entry 1 - Destroy Contraband, HDE, 26 Oct 2026).
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
static u32 PreHostileNear(const std::vector<space::Contact>& all)
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
// within the battle's reach (M03 Battle of Willard, 20 Oct 2026: bombers spawned 7 km out attacking the station were
// seen and not engaged). The game's maxHit cross-check stays with the target choice.
static bool ObjectiveThreat(const std::vector<space::Contact>& all)
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
static bool DestroyFired(const mission::Goal& sg, const std::vector<space::Contact>& all)
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
static u32 SurvivalLead(const std::vector<space::Contact>& all)
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
static bool StartOrKeep(Mode m, u32 target)
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
static bool DecHoldAt(const float* q)
{
    if (!s_plan)
        return Start(Mode::Hold, 0, q);
    s_pend.plan = situation::Plan();
    s_pend.plan.kind = situation::Plan::Kind::HoldAt, s_pend.plan.hasPoint = true;
    memcpy(s_pend.plan.point, q, sizeof(s_pend.plan.point));
    return true;
}
static bool DecDock(u32 ring, u32 toward)
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
static bool DecPass(u32 ring, float radius)
{
    if (!s_plan)
        return StartPass(ring, radius);
    s_pend.plan = situation::Plan();
    s_pend.plan.kind = situation::Plan::Kind::Pass, s_pend.plan.object = ring, s_pend.plan.radius = radius;
    return true;
}
static bool DecShoot(u32 id)
{
    if (!s_plan)
        return StartShootObject(id);
    s_pend.plan = situation::Plan();
    s_pend.plan.kind = situation::Plan::Kind::Shoot, s_pend.plan.object = id;
    return true;
}
static bool DecWaypoints(bool noDock)
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
static void DecFly(bool ok, const float* dir, float radius, float ahead)
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

static void ObjectiveStart(Mode m, u32 target, Action action, bool ok)
{
    if (s_plan) // (plan mode, A37 S7: the executor makes the call with the real start's result)
    {
        s_pend.objStart = true, s_pend.om = m, s_pend.ot = target, s_pend.oa = action, s_pend.ook = ok;
        return;
    }
    g_obj.sub = ok ? m : Mode::Off;
    g_obj.subTarget = target;
    g_obj.subSince = gameclock::NowMs64();
    // (an enum since 24 Oct 2026; as a char[16], a 17-char name with strcpy_s was a fail-fast crash, 22 Oct 2026)
    g_obj.action = action;
    // the start's Action, once per start of its own (a kept trip / running mode starts nothing): the `autopilot start
    // mode=` line is printed inside Start, before the action is known - this line follows it (the retro, 31 Oct 2026: a
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
static void ShadowAfter(const char* rule, Mode m, u32 target, Action action, bool keep = false)
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
static void ShadowExit(const char* rule, Mode m, u32 target, Action action, bool ok, bool keep = false)
{
    ObjectiveStart(m, target, action, ok);
    ShadowAfter(rule, m, target, action, keep);
}

static u32 s_lastReal = 0; // ObjectiveDecide's `lastReal` (a function static until A37 S4)
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
// at cruise (session 8, M10 exit field: the follow hit a big asteroid at ~380 m/s, hull 0.98 -> dead, 1239 s). True while
// such a trip runs and still ends within 1 km of the ship.
static bool LeashTripRuns(const space::Contact& lc)
{
    if (g.mode != Mode::Travel || g_obj.action != Action::Leash || !g.tr.hasDest || g.tr.destObject)
        return false;
    const float d[3] = {lc.pos[0] - g.tr.dest[0], lc.pos[1] - g.tr.dest[1], lc.pos[2] - g.tr.dest[2]};
    return Len3(d) < 1000.0f;
}
static void LeashBack(const space::Contact& lc)
{
    ObjectiveStart(Mode::Travel, 0, Action::Leash, StartTravel(0, lc.pos, 0, true));
}

// ------------------------------------------------------------------ LOOK L0 (look_log): what LOOK would do, logged only
// (controller\docs\LOOK_PLAN.md; the pure verdict: lookahead.h). The adapter reads what the decision already reads - the
// step's armed goals (AdvanceGoalsPeek: reads mission.cpp's memory, never writes it), the chain's (ChainGoals) and the next
// step's (NextStepGoals), the contacts, the spawns, the leashes, the towards nag, the tethers - and changes nothing: no
// start, no mode, no g_obj field. Behaviour-neutral with the switch on or off (the decision path is the same code).
static const char* VerbName(ObjVerb v)
{
    switch (v)
    {
    case ObjVerb::Dock:
        return "dock";
    case ObjVerb::Escort:
        return "escort";
    case ObjVerb::Fight:
        return "fight";
    case ObjVerb::Survive:
        return "survive";
    default:
        return "none";
    }
}

// This game's spawns (the host's list after the newest one at the last menu: PreAggroUpdate's rule).
static std::vector<mission::Spawn> LookSpawns()
{
    static std::vector<SpawnRec> buf(2048);
    const int n = HostSpawnedShips(buf.data(), static_cast<int>(buf.size()));
    SpawnRec menu = {};
    int from = 0;
    if (MenuSpawn(menu))
        for (int i = n - 1; i >= 0; --i)
            if (buf[i].id == menu.id && memcmp(buf[i].pos, menu.pos, sizeof(menu.pos)) == 0)
            {
                from = i + 1;
                break;
            }
    std::vector<mission::Spawn> out;
    for (int i = from; i < n; ++i)
    {
        mission::Spawn sp;
        sp.id = buf[i].id;
        memcpy(sp.pos, buf[i].pos, sizeof(sp.pos));
        out.push_back(sp);
    }
    return out;
}

// A mission ship's contact by its pilot's name (the leash's way, DecideBody: the name id, else the name's text against
// the reputation's name); none: an empty contact.
static space::Contact ContactByPilot(u32 nameIds, const std::vector<space::Contact>& all)
{
    if (!nameIds)
        return space::Contact{};
    const std::string want = events::IdsText(nameIds);
    for (const space::Contact& c : all)
    {
        if (!c.ship)
            continue;
        bool same = c.nameIds == nameIds;
        if (!same && c.rep && !want.empty())
        {
            const std::wstring rn = space::RepName(c.rep);
            std::string n;
            for (wchar_t ch : rn)
                n += ch < 128 ? static_cast<char>(ch) : '?';
            same = n == want;
        }
        if (same)
            return c;
    }
    return space::Contact{};
}

static void LookFactsNow(const ObjectiveStep& st, ObjVerb verb, const std::vector<space::Contact>& all,
                         lookahead::LookFacts& f)
{
    f = lookahead::LookFacts();
    const u32 sys = fl::PlayerSystem();
    // the step, or the last real one while a placeholder stands ("Awaiting": its chain still runs - survival_stage's rule)
    const u32 from = st.textId && st.textId != 21660 && st.textId != 1 ? st.textId : s_lastReal;
    f.textId = from;
    f.verb = VerbName(verb);
    space::Pilot me;
    f.havePilot = space::ReadPilot(me);
    if (f.havePilot)
        memcpy(f.me, me.pos, sizeof(f.me));
    flight::Model fm;
    if (flight::ReadModel(fm) && fm.cruiseSpeed > 1)
        f.flySpeed = fm.cruiseSpeed;
    // the goals: armed (the decision's view), the chain's (now or about to be armed), the next step's - each trigger once
    mission::AdvanceInfo info;
    std::vector<std::pair<mission::Goal, const char*>> goals;
    const auto add = [&](const std::vector<mission::Goal>& gs, const char* src)
    {
        for (const mission::Goal& ng : gs)
        {
            bool dup = false;
            for (auto& have : goals)
                if (have.first.trigger == ng.trigger && have.first.file == ng.file)
                {
                    dup = true;
                    if (ng.hops && (!have.first.hops || ng.hops < have.first.hops))
                        have.first.hops = ng.hops;
                }
            if (!dup)
                goals.push_back({ng, src});
        }
    };
    add(mission::AdvanceGoalsPeek(st.textId, sys, HostObjectiveTitle(), info, true), "armed");
    add(mission::ChainGoals(from, sys), "chain");
    add(mission::NextStepGoals(from, sys, nullptr, nullptr, true), "next");
    const std::vector<mission::Spawn> spawns = LookSpawns();
    for (const auto& [mg, src] : goals)
    {
        if (mg.kind == 4)
        {
            lookahead::KillGoal k = lookahead::KillGoalOf(mg, src);
            std::vector<std::string> names;
            const std::vector<u32> ids = mission::ShipIdsOf(mg, spawns, &names);
            for (size_t si = 0; si < k.ships.size(); ++si)
            {
                lookahead::KillShip& s = k.ships[si];
                space::Contact c;
                for (size_t j = 0; j < ids.size() && !c.id; ++j)
                    if (names[j] == s.nick)
                        c = FindContact(ids[j]);
                if (!c.id)
                    c = ContactByPilot(mg.ships[si].nameIds, all);
                if (!c.id)
                    continue;
                // (the overlay's copy: pre_aggro marks a ship the script has not turned yet)
                for (const space::Contact& o : all)
                    if (o.id == c.id)
                        c = o;
                s.id = c.id;
                s.alive = c.health != 0;
                s.preHostile = c.preHostile;
                s.hostile = !c.preHostile && c.attitude <= lookahead::kHostile;
                s.friendly = c.attitude > 0;
                s.targetable = c.targetable;
                memcpy(s.pos, c.pos, sizeof(s.pos));
                s.dist = c.distance;
                s.hull = c.health;
                if (k.scriptInv)
                    s.invRead = space::Invincible(c.id, s.inv, s.invSecond, s.invFloor);
            }
            f.kills.push_back(k);
        }
        else if ((mg.kind == 1 || mg.kind == 2) && mg.subject == "player")
        {
            lookahead::FlyGoal y = lookahead::FlyGoalOf(mg, src);
            if (mg.kind == 1)
                y.placed = space::ObjectPos(mg.object, y.pos);
            // the lane ride its chain waits for: the whole lane from its entry ring in the ride's direction (route)
            route::Lane lane;
            const u32 ring = flhash::CreateId(mg.laneFrom), toward = flhash::CreateId(mg.laneToward);
            if (!mg.laneFrom.empty() && route::LaneBetween(ring, toward, lane) && lane.rings.size() >= 2)
                for (size_t r = 0; r < lane.rings.size(); ++r)
                    if (lane.rings[r] == ring)
                    {
                        y.lane = true;
                        y.laneRideS = route::RideTime(route::Law(), lane, lane.at[r], lane.length());
                        y.laneEnd[0] = lane.x.back(), y.laneEnd[1] = lane.y.back(), y.laneEnd[2] = lane.z.back();
                        break;
                    }
            f.flies.push_back(y);
        }
    }
    // the ship leashes as the decision reads them (mission::Leashes; anchor: the ship by its pilot's name, else the object)
    for (const mission::Leash& l : mission::Leashes(sys))
    {
        bool off = false;
        for (const u32 c : l.offComms)
        {
            const unsigned long long at = HostCommHeardAt(c);
            off = off || (at && GetTickCount64() - at < 30ull * 60 * 1000);
        }
        if (off)
            continue;
        lookahead::LeashAnchor a;
        a.name = l.name, a.dist = l.dist;
        space::Contact oc = FindContact(l.object);
        if (!(oc.id && oc.ship))
            oc = ContactByPilot(l.nameIds, all);
        if (oc.id && oc.ship)
            a.found = true, memcpy(a.pos, oc.pos, sizeof(a.pos));
        else
            a.found = space::ObjectPos(l.object, a.pos);
        f.leashes.push_back(a);
    }
    // the step's towards nag (Content.dll's default distance when the script gives none)
    mission::TowardsNag nag;
    if (from && mission::TowardsNagOf(from, nag))
    {
        f.nag.has = true;
        f.nag.name = nag.name.empty() ? std::string("pos") : nag.name;
        f.nag.radius = nag.dist > 0 ? nag.dist : lookahead::kNagDefault;
        if (nag.pos)
            f.nag.placed = true, memcpy(f.nag.pos, nag.p, sizeof(f.nag.pos));
        else
            f.nag.placed = space::ObjectPos(nag.object, f.nag.pos);
    }
    // the tethers (reported only)
    for (const mission::Tether& t : mission::Tethers(info.script, sys))
    {
        lookahead::TetherFact x;
        x.a = t.a, x.b = t.b, x.trigger = t.trigger;
        x.dist = t.dist, x.secs = t.secs, x.inner = t.inner, x.fails = t.fails;
        space::Contact ac = FindContact(t.aId);
        if (!ac.id)
            ac = ContactByPilot(t.aNameIds, all);
        if (ac.id && _stricmp(t.b.c_str(), "player") == 0)
            x.gap = ac.distance;
        f.tethers.push_back(x);
    }
    // the objective the script declares next with a point here (pre-stage-next's place) - PSN
    u32 nt = 0;
    if (from && mission::NextObjectivePlace(from, sys, f.nextPos, &nt))
        f.nextPlace = true, f.nextText = nt, f.nextDestroy = ObjectiveVerb(nt) == ObjVerb::Fight;
}

// At each objective decision with `look_log` on: the verdict, logged once per change (the key: step, kind, target,
// trigger, the blockers' codes) with one line per kill candidate under the script's Act_Invulnerable (the game's own
// GetInvincible against the script's floor - the M02 probe, LOOK_PLAN 4a). Returns nothing, starts nothing.
// TIMER-DESTROY's facts now (lookahead.h TimerDestroy; `timer_destroy`): the step's advance from the script (cached per
// step and system - mission::StepAdvanceOf walks the whole script), the ship's hull and batteries, a hostile ship shooting
// at us within kSurvivalReach, the advancing path's next objective's place and the step's own navmarker.
static mission::StepAdvance s_tdAdvance;
static lookahead::TimerDestroyFacts TimerDestroyFactsNow(const ObjectiveStep& st, ObjVerb verb,
                                                         const std::vector<space::Contact>& all)
{
    lookahead::TimerDestroyFacts f;
    const u32 sys = fl::PlayerSystem();
    // `td_window` (M13-MIMIC item 2): a placeholder after a kill step whose kill has fired (its watcher no longer armed in
    // the live table) and whose next objective is a timer "Destroy ..." step - that step's rule from the kill on (M13: the
    // 25 s cap_timer gap after the first battleship, no objective shown - td1full chased NomadCap2 with 6 torpedoes; the
    // runners leave for Toledo)
    const bool placeholder = !st.textId || st.textId == 21660 || st.textId == 1;
    u32 text = verb == ObjVerb::Fight ? st.textId : 0;
    mission::LiveTable lt;
    bool ltRead = false;
    const auto live = [&]() -> const mission::LiveTable&
    {
        if (!ltRead)
            lt = mission::LiveTriggers(), ltRead = true;
        return lt;
    };
    if (!text && g_set.tdWindow && placeholder && s_lastReal)
    {
        static u32 kText = 0, kSys = 0;
        static mission::StepAdvance kAdv;
        if (kText != s_lastReal || kSys != sys)
            kText = s_lastReal, kSys = sys, kAdv = mission::StepAdvanceOf(s_lastReal, sys);
        if (kAdv.kind == "kill" && kAdv.nextText && ObjectiveVerb(kAdv.nextText) == ObjVerb::Fight && live().ok)
        {
            bool armed = false;
            for (const mission::LiveTrigger& r : live().triggers)
                armed = armed || (r.state == 0 && r.nickname == kAdv.watcher);
            if (!armed)
                text = kAdv.nextText;
        }
    }
    if (!text)
        return f;
    static u32 cText = 0, cSys = 0;
    if (cText != text || cSys != sys)
        cText = text, cSys = sys, s_tdAdvance = mission::StepAdvanceOf(text, sys);
    const mission::StepAdvance& a = s_tdAdvance;
    f.advance = a.kind;
    if (f.advance != "timer")
        return f;
    // (td_window: the script takes the next objective's dock already - an armed Cnd_BaseEnter row leading on)
    if (g_set.tdWindow && a.nextObject && live().ok)
        f.dockNow = !mission::ArmedBaseEnters(live(), text).empty(), f.nextObject = a.nextObject;
    f.speedrun = Knows(Source::Hidden);
    ai::Self self;
    if (ai::ReadSelf(self))
        f.hull = self.hull, f.batteries = self.batteries;
    space::Pilot me;
    f.havePilot = space::ReadPilot(me);
    if (f.havePilot)
        memcpy(f.me, me.pos, sizeof(f.me));
    for (const space::Contact& c : all)
        if (c.ship && c.health != 0 && HostileContact(c) && c.targetsPlayer && c.distance < kSurvivalReach)
            f.attacked = true;
    if (a.nextHasPos && (!a.nextSystem || a.nextSystem == sys))
    {
        f.nextPlace = true, memcpy(f.nextPos, a.nextPos, sizeof(f.nextPos));
        f.nextDestroy = ObjectiveVerb(a.nextText) == ObjVerb::Fight;
    }
    if (st.hasNav && text == st.textId)
        f.stepPlace = true, memcpy(f.stepPos, st.nav, sizeof(f.stepPos));
    else if (a.ownHasPos) // (the hidden step: its [NNObjective]'s own point - NomadCap2's)
        f.stepPlace = true, memcpy(f.stepPos, a.ownPos, sizeof(f.stepPos));
    return f;
}
// (the log's name for it: the next objective and the advancing path)
static std::string TimerDestroyNext() { return s_tdAdvance.next + "(" + s_tdAdvance.via + ")"; }

// ALLY-KILL's facts now (lookahead.h AllyKill; `ally_kill`; docs\research\runners\M13_gate_allykill.md): the step's
// advance from the script (cached per step and system), its shape (StepAdvance::AllyKillShape and the next step's verb
// Dock), the kill's solar alive, the friendly ships whose target it is, the dock (the next objective's object) placed now,
// and the wait at it - when the ship first came within kAkAtDock of it this step and the target's hull then (kept per
// step), a hostile ship shooting at us within kSurvivalReach. Once per step the script's data is logged.
static mission::StepAdvance s_akAdvance;
static lookahead::AllyKillFacts AllyKillFactsNow(const ObjectiveStep& st, const std::vector<space::Contact>& all)
{
    lookahead::AllyKillFacts f;
    const u32 sys = fl::PlayerSystem();
    const u32 text = st.textId;
    if (!text || text == 21660 || text == 1)
        return f;
    static u32 cText = 0, cSys = 0;
    static ULONGLONG arriveAt = 0;
    static float arriveHull = -1;
    if (cText != text || cSys != sys)
    {
        cText = text, cSys = sys, s_akAdvance = mission::StepAdvanceOf(text, sys);
        arriveAt = 0, arriveHull = -1, g_obj.akFellBack = 0; // (the wait and the fallback are the step's)
        const mission::StepAdvance& a = s_akAdvance;
        if (a.kind == "kill")
            events::Push("autopilot objective ally-kill %u data: watcher=%s any-killer=%d unlocks=%u next=%s(%u) "
                         "solars=%d next-object=%u shape=%d next-dock=%d",
                         text, a.watcher.c_str(), a.anyKiller ? 1 : 0, a.unlocks, a.next.c_str(), a.nextText,
                         static_cast<int>(a.killObjects.size()), a.nextObject, a.AllyKillShape() ? 1 : 0,
                         a.nextText && ObjectiveVerb(a.nextText) == ObjVerb::Dock ? 1 : 0);
    }
    const mission::StepAdvance& a = s_akAdvance;
    f.qualifies = a.AllyKillShape() && a.nextText && ObjectiveVerb(a.nextText) == ObjVerb::Dock &&
                  (!a.nextSystem || a.nextSystem == sys);
    if (!f.qualifies)
        return f;
    f.speedrun = Knows(Source::Hidden);
    f.fellBack = g_obj.akFellBack == text;
    space::Pilot me;
    f.havePilot = space::ReadPilot(me);
    if (f.havePilot)
        memcpy(f.me, me.pos, sizeof(f.me));
    ai::Self self;
    if (ai::ReadSelf(self))
        f.hull = self.hull, f.batteries = self.batteries;
    // the kill's solar: the first of its objects alive now
    for (u32 id : a.killObjects)
    {
        const space::Contact c = FindContact(id);
        if (c.id && c.health != 0)
        {
            f.target = c.id, f.targetAlive = true, f.targetHull = c.health;
            break;
        }
    }
    f.dockKnown = space::ObjectPos(a.unlocks, f.dockPos);
    for (const space::Contact& c : all)
    {
        if (!c.ship || c.health == 0)
            continue;
        if (f.target && c.targetId == f.target && c.attitude > 0)
            ++f.allies;
        if (HostileContact(c) && c.targetsPlayer && c.distance < kSurvivalReach)
            f.attacked = true;
    }
    if (f.havePilot && f.dockKnown)
    {
        const float d[3] = {f.dockPos[0] - f.me[0], f.dockPos[1] - f.me[1], f.dockPos[2] - f.me[2]};
        if (!arriveAt && Len3(d) <= lookahead::kAkAtDock)
            arriveAt = gameclock::NowMs64(), arriveHull = f.targetHull;
    }
    if (arriveAt)
    {
        f.waitedS = static_cast<float>(gameclock::NowMs64() - arriveAt) / 1000.0f;
        f.hullDrop = arriveHull >= 0 && f.targetHull >= 0 ? arriveHull - f.targetHull : 0.0f;
    }
    return f;
}

static void LookLog(const ObjectiveStep& st, ObjVerb verb, const std::vector<space::Contact>& all)
{
    static std::string lastKey;
    if (!g_set.lookLog)
    {
        lastKey.clear();
        return;
    }
    lookahead::LookFacts f;
    LookFactsNow(st, verb, all, f);
    const lookahead::LookResult r = lookahead::LookTarget(f);
    if (r.key == lastKey)
        return;
    lastKey = r.key;
    events::Push("autopilot look %s", lookahead::Line(r, f).c_str());
    for (const std::string& l : lookahead::InvincibleLines(f))
        events::Push("autopilot look %s", l.c_str());
}

std::string LookCommand()
{
    ObjectiveStep st = {};
    HostObjectiveNow(&st);
    const std::vector<space::Contact> all = Contacts();
    lookahead::LookFacts f;
    LookFactsNow(st, ObjectiveVerb(st.textId), all, f);
    const lookahead::LookResult r = lookahead::LookTarget(f);
    std::string s = "look_log=" + std::to_string(g_set.lookLog ? 1 : 0) + " | " + lookahead::Line(r, f) + "\n" +
                    lookahead::Describe(r, f);
    for (const std::string& l : lookahead::InvincibleLines(f))
        s += "  " + l + "\n";
    return s;
}

// ------------------------------------------------------------------ ASH-START: chase_kill / spawn_stage
// (docs\research\runners\M02.md "## Plan" 1; the pure halves: lookahead.h ChaseStep / StagePoint / FleeingSpawn; the
// coast: autopilot_frame.cpp ChaseCoast; the cruise charge and the missile hold: autopilot_combat.cpp)
// chase_kill's candidate: LOOK's kill verdict over the step's armed goals (src=armed - alive, hostile, targetable, inside
// the leashes and the towards nag: LookTarget's own filters) in a step that asks no fight, dock or escort of its own and
// points at no solar - a "Follow ..." with its navmarker (M02 22262); 0 none. `f` / `r`: LOOK's facts and verdict.
static u32 ChaseCandidate(const ObjectiveStep& st, ObjVerb verb, const std::vector<space::Contact>& all,
                          lookahead::LookFacts& f, lookahead::LookResult& r)
{
    if (!g_set.chaseKill || !Knows(Source::Hidden) || !Knows(Source::Meta) || verb != ObjVerb::None || g_obj.survival ||
        (!st.refCount && !st.hasNav))
        return 0;
    for (int i = 0; i < st.refCount; ++i)
    {
        const space::Contact c = FindContact(space::ObjectiveObject(st.refs[i], all));
        if (c.id && !c.ship)
            return 0;
    }
    LookFactsNow(st, verb, all, f);
    r = lookahead::LookTarget(f);
    return r.kind == lookahead::Kind::Kill && r.source == "armed" ? r.target : 0;
}

// spawn_stage's facts now: the step's armed kill goal (AdvanceGoalsPeek with LOOK's kind 4; armed in the live table when
// it reads) with one ship alive and the others down, what its firing spawns (mission::SpawnAfterKill), PSN's exception
// (lookahead::FleeingSpawn), the fight's leash (the leaving nags this kill switches off, anchored on their object now) and
// the stage point (lookahead::StagePoint). Not ok: `why` of the last goal looked at.
struct StagePlan
{
    bool ok = false;
    std::string why, trigger, ship, anchorName;
    lookahead::StageResult st;
    float spawn[3] = {};
    u32 last = 0;        // the goal's one live ship
    float lastFromP = 0; // its distance from the stage point
    int alive = 0, down = 0, need = 0;
    float leash = 0;
    // `spawn_charge`: the spawn flees (FleeingSpawn passed: his run point known - the charge's facts), the point is the
    // charge's (StagePoint over every leash), the leashes it kept to, the ones that pulled it, and why the charge's point
    // was not used (a datum missing: the spawn_stage point as before; a refusal: no stage)
    bool fled = false, charge = false, chargeRefused = false;
    float run[3] = {};
    int chargeLeashes = 0;
    std::string chargeBound, chargeWhy;
    std::vector<lookahead::StageLeash> chargeLs; // (`ash_ride`: the leashes the loop keeps inside)
};

// spawn_charge's ship leashes (design 1: every armed ship leash at kChargeLeashFrac): the supervisor's own (g_obj.leashShip
// - the one whose 80 % trip rule turned the as3 / as4 trips back), and the script's leashes armed in the live table
// (mission::ArmedLeashes; without the table every one of mission::Leashes, as the decision keeps to all) not disarmed by
// a heard comm, anchored as LOOK reads them (the object's ship, the pilot's name, the object). False with `miss` when an
// anchor is not found (fail closed: the spawn_stage point as before).
static bool ChargeShipLeashes(u32 sys, const std::vector<space::Contact>& all, std::vector<lookahead::StageLeash>& out,
                              std::string& miss)
{
    if (g_obj.leashShip)
    {
        const space::Contact lc = FindContact(g_obj.leashShip);
        if (!lc.id)
            return miss = "leash ship " + std::to_string(g_obj.leashShip) + " not found", false;
        lookahead::StageLeash l;
        l.name = "ship" + std::to_string(lc.id), l.radius = g_obj.leashDist, l.frac = lookahead::kChargeLeashFrac;
        memcpy(l.anchor, lc.pos, sizeof(l.anchor));
        out.push_back(l);
    }
    const mission::LiveTable lt = mission::LiveTriggers();
    for (const mission::Leash& ml : lt.ok ? mission::ArmedLeashes(lt, sys) : mission::Leashes(sys))
    {
        bool off = false;
        for (const u32 c : ml.offComms)
        {
            const unsigned long long at = HostCommHeardAt(c);
            off = off || (at && GetTickCount64() - at < 30ull * 60 * 1000);
        }
        if (off || ml.dist <= 0)
            continue;
        lookahead::StageLeash l;
        l.name = ml.name, l.radius = ml.dist, l.frac = lookahead::kChargeLeashFrac;
        space::Contact oc = FindContact(ml.object);
        if (!(oc.id && oc.ship))
            oc = ContactByPilot(ml.nameIds, all);
        if (oc.id && oc.ship)
            memcpy(l.anchor, oc.pos, sizeof(l.anchor));
        else if (!space::ObjectPos(ml.object, l.anchor))
            return miss = "leash " + ml.name + " anchor not found", false;
        out.push_back(l);
    }
    return true;
}

static StagePlan SpawnStageNow(const ObjectiveStep& st, const std::vector<space::Contact>& all)
{
    StagePlan p;
    if (!st.textId || st.textId == 21660 || st.textId == 1)
        return p.why = "no step", p;
    const u32 sys = fl::PlayerSystem();
    mission::AdvanceInfo info;
    const std::vector<mission::Goal> goals =
        mission::AdvanceGoalsPeek(st.textId, sys, HostObjectiveTitle(), info, true);
    bool any = false;
    for (const mission::Goal& kg : goals)
        any = any || (kg.kind == 4 && !kg.ships.empty());
    if (!any)
        return p.why = "no kill goal", p;
    const mission::LiveTable lt = mission::LiveTriggers();
    const std::vector<mission::Spawn> spawns = LookSpawns();
    for (const mission::Goal& kg : goals)
    {
        if (kg.kind != 4 || kg.ships.empty())
            continue;
        p = StagePlan();
        p.trigger = kg.trigger;
        if (lt.ok)
        {
            bool armed = false;
            for (const mission::LiveTrigger& r : lt.triggers)
                armed = armed || (r.state == 0 && r.nickname == kg.trigger);
            if (!armed)
            {
                p.why = "not armed";
                continue;
            }
        }
        // its ships by their spawn (ShipIdsOf): one alive, the rest down (a contact gone or at 0 hull)
        std::vector<std::string> names;
        const std::vector<u32> ids = mission::ShipIdsOf(kg, spawns, &names);
        p.need = kg.count > 0 ? kg.count : static_cast<int>(kg.ships.size());
        for (const u32 id : ids)
        {
            const space::Contact c = FindContact(id);
            if (c.id && c.health != 0)
                ++p.alive, p.last = id;
            else
                ++p.down;
        }
        if (p.alive != 1 || p.down < p.need - 1)
        {
            char b[80];
            sprintf_s(b, "%d alive, %d down of %d", p.alive, p.down, p.need);
            p.why = b;
            continue;
        }
        const mission::SpawnAfter sa = mission::SpawnAfterKill(kg.trigger, kg.file);
        if (!sa.ok)
        {
            p.why = sa.why;
            continue;
        }
        p.ship = sa.ship;
        float run[3] = {};
        std::string fw;
        if (!lookahead::FleeingSpawn(lookahead::SpawnFactsOf(sa), run, &fw))
        {
            p.why = "psn: " + fw;
            continue;
        }
        p.fled = true;
        memcpy(p.run, run, sizeof(p.run));
        memcpy(p.spawn, sa.pos, sizeof(p.spawn));
        // the fight's leash: the leaving nags this kill switches off (M02's nag_battle3: King, 5 km) - the tightest one;
        // an anchor not found refuses the stage (fail closed)
        float anchor[3] = {};
        bool haveAnchor = false, lost = false;
        std::vector<lookahead::StageLeash> nagLeashes; // (spawn_charge: every one of them, at kChargeNagFrac)
        for (const mission::LeavingNag& n : mission::LeavingNags(kg.file, sys))
        {
            if (n.dist <= 0 || std::find(n.offBy.begin(), n.offBy.end(), kg.trigger) == n.offBy.end())
                continue;
            float ap[3] = {};
            bool found = false;
            if (n.pos)
                memcpy(ap, n.p, sizeof(ap)), found = true;
            else
            {
                space::Contact oc = FindContact(n.objectId);
                if (!(oc.id && oc.ship))
                    oc = ContactByPilot(n.nameIds, all);
                if (oc.id)
                    memcpy(ap, oc.pos, sizeof(ap)), found = true;
                else
                    found = space::ObjectPos(n.objectId, ap);
            }
            if (!found)
            {
                lost = true;
                p.anchorName = n.object;
                break;
            }
            lookahead::StageLeash nl;
            nl.name = n.name, nl.radius = n.dist, nl.frac = lookahead::kChargeNagFrac;
            memcpy(nl.anchor, ap, sizeof(nl.anchor));
            nagLeashes.push_back(nl);
            if (!haveAnchor || n.dist < p.leash)
                memcpy(anchor, ap, sizeof(anchor)), p.leash = n.dist, p.anchorName = n.pos ? n.name : n.object,
                    haveAnchor = true;
        }
        if (lost)
        {
            p.why = "leash anchor " + p.anchorName + " not found";
            continue;
        }
        p.st = lookahead::StagePoint(sa.pos, run, haveAnchor ? anchor : nullptr, p.leash);
        // `spawn_charge` (design 1): the point 250 m behind the spawn on his line, 70 m aside, inside 0.8 x every leaving
        // nag and 0.75 x every armed ship leash; a ship leash's anchor not found: the spawn_stage point above (fail
        // closed); no point within them all <= 600 m from the spawn: no stage (the fight as without spawn_stage)
        if (g_set.spawnCharge)
        {
            std::vector<lookahead::StageLeash> ls = nagLeashes;
            std::string miss;
            if (!ChargeShipLeashes(sys, all, ls, miss))
                p.chargeWhy = miss;
            else
            {
                p.chargeLeashes = static_cast<int>(ls.size());
                p.chargeLs = ls;
                const lookahead::StageResult cst = lookahead::StagePoint(sa.pos, run, ls, &p.chargeBound);
                p.chargeWhy = cst.why;
                if (!cst.ok)
                {
                    p.chargeRefused = true;
                    p.why = "spawn-charge: " + cst.why;
                    return p; // (the spawn's facts kept: the charge at his spawn still applies)
                }
                p.st = cst, p.charge = true;
            }
        }
        if (!p.st.ok)
        {
            p.why = p.st.why;
            continue;
        }
        const space::Contact lc = FindContact(p.last);
        const float d[3] = {lc.pos[0] - p.st.p[0], lc.pos[1] - p.st.p[1], lc.pos[2] - p.st.p[2]};
        p.lastFromP = Len3(d);
        p.ok = true;
        p.why = fw;
        return p;
    }
    return p;
}

// The stage's verdict now: 0 none (today's fight), 1 the trip to the point, 2 the last ship fought there, 3 the hold
// there, 4 timed out (kStageTimeoutS at the point without the kill). With hysteresis against the running verdict (a
// verdict flipping every second would trip the decide-loop guard): the fight goes on to 1.3 x kStageReach, the stage
// holds to 1.5 x kStageArrive.
static int StageVerdict(const StagePlan& sp, float* toGo = nullptr)
{
    if (!sp.ok || g_obj.stageTimedOut.count(sp.trigger))
        return 0;
    space::Pilot me;
    if (!space::ReadPilot(me))
        return 0;
    const float d[3] = {sp.st.p[0] - me.pos[0], sp.st.p[1] - me.pos[1], sp.st.p[2] - me.pos[2]};
    const float l = Len3(d);
    if (toGo)
        *toGo = l;
    const bool same = g_obj.stageTrigger == sp.trigger;
    const int prev = same ? g_obj.stageLast : 0;
    // (a charge stage sits on its point: kChargeArrive, not spawn_stage's 1.5 km - probe sc1)
    const float arrive = g_set.spawnCharge && sp.charge ? lookahead::kChargeArrive : lookahead::kStageArrive;
    if (l > (prev >= 2 ? 1.5f : 1.0f) * arrive)
        return 1;
    if (same && g_obj.stageAt &&
        gameclock::NowMs64() - g_obj.stageAt > static_cast<ULONGLONG>(lookahead::kStageTimeoutS * 1000.0f))
        return 4;
    return sp.lastFromP < (prev == 2 ? 1.3f : 1.0f) * lookahead::kStageReach ? 2 : 3;
}

// The stage's verdict with `ash_ride` (autopilot_ashride.cpp): a charge stage whose ride is armed (its loop fits every
// leash) or fell back is the fight on the last ship wherever it is - no trip, no hold, no timeout: the ride's own leave
// rule and fallback decide; else StageVerdict as is. The decision and the supervisor both ask this (no verdict ping-pong).
static int StageVerdictNow(const StagePlan& sp, float* toGo = nullptr)
{
    if (g_set.ashRide && g_set.spawnCharge && sp.ok && sp.charge && sp.last &&
        AshRideArm(sp.trigger, sp.last, sp.spawn, sp.run, sp.chargeLs))
    {
        space::Pilot me;
        if (toGo && space::ReadPilot(me))
        {
            const float d[3] = {sp.st.p[0] - me.pos[0], sp.st.p[1] - me.pos[1], sp.st.p[2] - me.pos[2]};
            *toGo = Len3(d);
        }
        return 2;
    }
    return StageVerdict(sp, toGo);
}

// spawn_charge (design 3): the chased ship is the fleeing ship the stage expected (his contact matched by the chase,
// g_obj.chargeId), alive and not yet at his run point (lookahead::StillFleeing). Any fact missing: false (today's chase).
bool ChargeFleeing(u32 id)
{
    if (!g_set.spawnCharge || !g_set.spawnStage || !g_set.chaseKill || !Knows(Source::Hidden) || !id ||
        id != g_obj.chaseShip || id != g_obj.chargeId || g_obj.chargeShip.empty())
        return false;
    const space::Contact c = FindContact(id);
    return c.id && c.health != 0 && lookahead::StillFleeing(c.pos, g_obj.chargeRun);
}

// LOCK-TRAP's memory: the trap the last decision flew to ("" none: its end is logged once), the traps we were inside of
// that stayed armed (not the ship the script means: off), and the story controller they belong to (a new one forgets them)
static std::string s_trapActed;
static std::set<std::string> s_trapOff;
static u32 s_trapController = 0;

// Where a lock trap's condition is now: its point, its object (a solar or a contact by id), else a mission ship by its
// pilot's name (its runtime id is not its nickname's - the leash's way, mission::Leash::nameIds). False: not found.
static bool TrapAnchor(const mission::LockTrap& tp, const std::vector<space::Contact>& all, float p[3])
{
    if (tp.kind == 2)
        return memcpy(p, tp.pos, 3 * sizeof(float)), true;
    if (const space::Contact oc = FindContact(tp.object); oc.id)
        return memcpy(p, oc.pos, 3 * sizeof(float)), true;
    if (tp.nameIds)
    {
        const std::string want = events::IdsText(tp.nameIds);
        for (const space::Contact& c : all)
        {
            if (!c.ship)
                continue;
            bool same = c.nameIds == tp.nameIds;
            if (!same && c.rep && !want.empty())
            {
                const std::wstring rn = space::RepName(c.rep);
                std::string n;
                for (wchar_t ch : rn)
                    n += ch < 128 ? static_cast<char>(ch) : '?';
                same = n == want;
            }
            if (same)
                return memcpy(p, c.pos, 3 * sizeof(float)), true;
        }
    }
    return space::ObjectPos(tp.object, p);
}

// The lock trap step (`lock_trap`): the nearest armed trap whose condition we are outside of is flown to - a trip to 0.7 R
// on our side of its ship / point (inside the caller's ~0.8 R; the trigger fires at R), as a PreStage trip (the threat
// check leaves it alone); its ship moving, the trip's end decides again from where it is then. True: decided. Fails
// closed: no live table, no trap, no anchor -> false (today's decision).
static bool LockTrapDecide(const std::vector<space::Contact>& all)
{
    const mission::LiveTable lt = mission::LiveTriggers();
    if (lt.controller != s_trapController)
        s_trapController = lt.controller, s_trapOff.clear();
    const std::vector<mission::LockTrap> traps = mission::LockTrapsOfLive(lt, fl::PlayerSystem());
    if (!s_trapActed.empty() &&
        std::none_of(traps.begin(), traps.end(), [](const mission::LockTrap& t) { return t.trigger == s_trapActed; }))
    {
        events::Push("autopilot objective lock-trap %s: satisfied (fired or disarmed)", s_trapActed.c_str());
        s_trapActed.clear();
    }
    space::Pilot me;
    if (traps.empty() || !space::ReadPilot(me))
        return false;
    const mission::LockTrap* pick = nullptr;
    float pickPos[3] = {}, pickD = 1e30f;
    for (const mission::LockTrap& tp : traps)
    {
        if (s_trapOff.count(tp.trigger))
            continue;
        float p[3];
        if (!TrapAnchor(tp, all, p))
        {
            static std::string said;
            if (said != tp.trigger)
            {
                said = tp.trigger;
                events::Push("autopilot objective lock-trap %s: %s not found - today's decision", tp.trigger.c_str(),
                             tp.objectName.c_str());
            }
            continue;
        }
        const float d[3] = {p[0] - me.pos[0], p[1] - me.pos[1], p[2] - me.pos[2]};
        const float dist = Len3(d);
        if (dist <= 0.8f * tp.dist)
        {
            // inside: the trigger fires on its own (the game sees what we see); still armed later, it is not what we think
            // (another ship of that name, a condition we misread) - never flown to again for this story controller
            s_trapOff.insert(tp.trigger);
            continue;
        }
        if (dist < pickD)
            pick = &tp, pickD = dist, memcpy(pickPos, p, sizeof(pickPos));
    }
    if (!pick)
        return false;
    events::Push("autopilot objective lock-trap %s: within %.0f m of %s (locks %s, unlock by %s) - %.0f m now",
                 pick->trigger.c_str(), pick->dist, pick->kind == 1 ? pick->objectName.c_str() : "a point",
                 pick->lockedName.c_str(), pick->unlockBy.c_str(), pickD);
    s_trapActed = pick->trigger;
    float q[3];
    const float l = std::max(1.0f, pickD);
    for (int i = 0; i < 3; ++i)
        q[i] = pickPos[i] + (me.pos[i] - pickPos[i]) / l * pick->dist * 0.7f;
    g_obj.travelObjective = true;
    ShadowExit("lock-trap", Mode::Travel, 0, Action::PreStage, StartTravel(0, q, 0, true));
    return true;
}

// Decides what the current objective asks and starts it. Ranking (the mission's own pointers first, then the game's
// data about them): a fight the mission points at or that attacks us; the objective's target object - a friendly ship
// to join (formation, the game refuses it with capital ships: follow) or escort, a hostile to fight, a planet's docking
// ring, a station / ring / gate to fly to (dock only when the words say so); the navmarker; otherwise rest (Hold).
// (ObjectiveDecide's body since A37 S7: the decision below - the switch situation_arbiter on runs it in plan mode)
// kill_ahead's fight-step form: the destroy goals ahead of a fight step - the next step's (NextStepGoals), else the armed
// rows' (GoalsOfLive: M13's 24414 "Defend the Order Base" is set by triggers that lead nowhere; nomadcap's cap_dies is
// armed at its spawn by a sibling chain, spawnships -> nomad_timer -> freighter_dies; set ab2, 31 Oct 2026)
// (`live`: only for such a defend step - a Fight step's armed rows name its own target: 24425's cap_dies -> nomadcap)
static std::vector<mission::Goal> FightAheadGoals(u32 textId, const mission::LiveTable& lt, bool live)
{
    std::vector<mission::Goal> out;
    for (const mission::Goal& ng : mission::NextStepGoals(textId, fl::PlayerSystem()))
        if (ng.kind == 3)
            out.push_back(ng);
    if (out.empty() && live)
        for (const mission::Goal& lg : mission::GoalsOfLive(lt, fl::PlayerSystem(), textId))
            if (lg.kind == 3)
                out.push_back(lg);
    return out;
}

static void DecideBody(const char* reason)
{
    // (the coast guard flies the ship: no mode change under it - the owner, 29 Oct 2026: "mode switch still messed up")
    if (CoastActive())
        return;
    g_obj.tractorExitPending = false; // (set again below while it still applies)
    // Loop guard: more than 6 decisions in 10 s on one step (a fight that ends at once, a refused start) - pause 5 s.
    const ULONGLONG now = gameclock::NowMs64();
    if (now - g_obj.windowStart > 10000)
        g_obj.windowStart = now, g_obj.decisions = 0;
    if (++g_obj.decisions > 6)
    {
        events::Push("autopilot objective decide-loop reason=%s (more than 6 in 10 s): pausing 5 s", reason);
        g_obj.nextLook = now + 5000;
        g_obj.windowStart = now + 5000;
        g_obj.decisions = 0;
        // the step this call would have decided stays undecided: forget its key, or the next look takes it for "the
        // same objective set again" (ObjectiveUpdate's key check) and never decides it (gate q3s2 M07, 30 Oct 2026: a
        // fight ping-pong filled the guard, "Dock with the Trade Lane" arrived as the 7th decision, was swallowed, the
        // ship fought on and the nag failed the mission; earlier gates passed by 14 ms)
        g_obj.stepKey = 0;
        return;
    }
    ObjectiveStep st = {};
    const u32 seq = HostObjectiveNow(&st);
    if (seq != g_obj.seq)
    {
        g_obj.arrived = false, g_obj.arriveRetries = 0, g_obj.stepAt = gameclock::NowMs64(),
        g_obj.viaNext = g_obj.inVia = false, g_obj.badGoal.clear(), g_obj.deadGoals.clear(), g_obj.leashShip = 0,
        g_obj.dockNoWayTries = 0;
        // (chase_kill / spawn_stage: their memory is the step's; the chased ship stays while it lives - the next step
        // fights him on)
        g_obj.chaseAsked = 0, g_obj.stageAsked.clear(), g_obj.stageTrigger.clear(), g_obj.stageAt = 0,
        g_obj.stageLast = 0;
        g_obj.stageTimedOut.clear();
        if (g_obj.chaseShip && !FindContact(g_obj.chaseShip).id)
            g_obj.chaseShip = 0;
        // (spawn_charge: the fleeing ship's facts outlive the stage's step - his chase is the next one's - until he is gone)
        if (g_obj.chargeId && !FindContact(g_obj.chargeId).id)
            g_obj.chargeShip.clear(), g_obj.chargeId = 0, g_obj.chargeSaid = 0;
    }
    // A script goal's trip that ended with the step unchanged: its condition is not armed (session 8, 23 Oct 2026, M13
    // "Destroy the Nomad Battleship": within 1 km of the Osiris, then 300 m of Sinclair, then the Osiris again, every
    // ~30 s for minutes - neither fired, the battleship was never fought)
    else if (strcmp(reason, "ended") == 0 && g_obj.action == Action::ScriptGoal && !g_obj.goalTrigger.empty() &&
             g_obj.deadGoals.insert(g_obj.goalTrigger).second)
        events::Push("autopilot objective script-goal %s reached, the step unchanged: not armed - off for this step",
                     g_obj.goalTrigger.c_str());
    g_obj.seq = seq;
    g_obj.scriptMarkN = 0; // (set again below while the step's destroy condition holds)
    g_obj.defendOnly = false, g_obj.tdVerdict = 0; // (TIMER-DESTROY: set again below while its rule holds)
    g_obj.akVerdict = 0, g_obj.allyKillTarget = 0; // (ALLY-KILL: likewise)
    g_obj.killAheadWait = 0;                       // (kill_ahead: set again below while it waits for the script)
    g_obj.stageVerdict = 0, g_obj.stageNose = false; // (spawn_stage: set again below while it stages)
    g_obj.chargeStage = false;                       // (spawn_charge: likewise)
    // (A37 S3: the memory the rules read, as this decision begins - the shadow's, and since S7 the arbiter's)
    if (g_set.situationShadow || g_set.situationArbiter)
        situation::ShadowEntry();
    // An armed ship leash outranks the decision while we are far out on it (session 8, M10: the supervisor's follow and a
    // "threat" decision's trip took turns every second, 4.5 -> 5 km from the ship): follow it back first
    // (never when the step docks or jumps: the leash's phase is over - M10 followed a ship 4 min while "Dock with the
    // Hamburg Jump Hole" nagged, "You failed to dock with the Hamburg Jump Hole")
    if (g_obj.leashShip && ObjectiveVerb(st.textId) == ObjVerb::Dock)
        g_obj.leashShip = 0;
    // ...nor when the step points at a jump hole / gate (M10's "Dock with the Hamburg Jump Hole" shows a blank text: the
    // verb test missed it and the named-anchor run failed there too, 1227 s)
    for (int i = 0; i < st.refCount && g_obj.leashShip; ++i)
    {
        const space::Contact jc = FindContact(space::ObjectiveObject(st.refs[i], Contacts()));
        if (jc.id && (jc.type & (space::kJumpGate | space::kJumpHole)))
            g_obj.leashShip = 0;
    }
    // (or a navmarker at one: M10's Hamburg step has no refs, only the marker - 5c3b388 still failed there)
    if (g_obj.leashShip && st.hasNav)
        for (const space::Contact& c : Contacts())
            if (c.type & (space::kJumpGate | space::kJumpHole))
            {
                const float d[3] = {c.pos[0] - st.nav[0], c.pos[1] - st.nav[1], c.pos[2] - st.nav[2]};
                if (Len3(d) < 2000.0f)
                    g_obj.leashShip = 0;
            }
    if (g_obj.leashShip)
    {
        const space::Contact lc = FindContact(g_obj.leashShip);
        // (80 %: at 60 % the trips back at ~4.5 km of M10's 6 km broke the exit-tunnel waypoint chain every 20-40 s - the
        // script had already dropped the leash for a nag toward the hole, and the slow progress failed it, gate_s8w; 1.2 km
        // left is ~4 s at cruise)
        if (lc.id && lc.distance > 0.8f * g_obj.leashDist)
        {
            if (LeashTripRuns(lc))
                return ShadowAfter("leash", g_obj.sub, g_obj.subTarget, g_obj.action, true); // (already on the way back)
            events::Push("autopilot objective decide reason=%s: %.0f m from the leash's ship %u - back toward it",
                         reason, lc.distance, lc.id);
            LeashBack(lc);
            return ShadowAfter("leash", Mode::Travel, 0, Action::Leash);
        }
    }
    g_obj.preStageTarget = 0;
    g_obj.flyGoalR = 0;
    const std::vector<space::Contact> all = Contacts();
    // LOCK-TRAP (`lock_trap`, mission::LockTrapsOfLive): an armed "within R of <ship> -> lock <dock>" row whose dock the
    // script unlocks later is met first, so the lock comes before the unlock (M13: wait_for_trent, 320 m of Orillion ->
    // lock St01_to_St03_hole, beside in_hole_range -> check_cap -> unlock; met after check_cap the hole stays locked)
    if (g_set.lockTrap && Knows(Source::Hidden) && g_set.scriptGoals)
        if (LockTrapDecide(all))
            return;
    // A placeholder step (no target, no navmarker: "Awaiting Mission Objective") with live hostile ships the mission marks
    // within 6 km is their fight (s12d M04 Mactan: "Destroy the Rheinland Assault Wing" and "Awaiting" came in the same
    // instant; the wing marked, the survival stance followed Juni 110 s while others killed it - the user: "all the
    // wrong choices when Rheinland ships are attacking Mactan")
    bool markFight = false;
    if (ObjectiveVerb(st.textId) == ObjVerb::None && !st.refCount && !st.hasNav)
    {
        u32 mk[32];
        const int mn = MissionObjectIds(mk, 32, all);
        for (const space::Contact& c : all)
            if (c.ship && c.health != 0 && c.distance < 6000 && HostileContact(c) && !c.preHostile &&
                std::find(mk, mk + mn, c.id) != mk + mn)
                markFight = true;
        if (markFight)
            events::Push("autopilot objective decide reason=%s: a placeholder step with hostile marks - their fight",
                         reason);
    }
    const ObjVerb verb = markFight ? ObjVerb::Fight : ObjectiveVerb(st.textId);
    g_obj.fightObjective = verb == ObjVerb::Fight;
    if (g_obj.fightObjective) // what the fight's marks are (diagnostics: M09's generators were never chosen)
    {
        u32 mk[32];
        const int mn = MissionObjectIds(mk, 32, all);
        std::string s;
        for (int i = 0; i < mn && i < 8; ++i)
            for (const space::Contact& c : all)
                if (c.id == mk[i])
                {
                    float att = 9;
                    space::AttitudeToPlayer(c.id, att);
                    char b[96];
                    sprintf_s(b, " %u:ship=%d,tgt=%d,att=%.2f,hp=%.0f,dist=%.0f", c.id, c.ship ? 1 : 0,
                              c.targetable ? 1 : 0, att, c.maxHit, c.distance);
                    s += b;
                }
        events::Push("autopilot objective fight-marks n=%d%s", mn, s.c_str());
    }
    // Marked loot the objective says to destroy: shot (Shoot mode on the object, nothing tractored), unless something
    // attacks - the fight first, then the pod (Destroy Contraband, 26 Oct 2026: a blank entry 0 whose navmarker sits on
    // the pod; the trip there ended "no way" and the tractor ate the pod, again and again)
    if (!ObjectiveThreat(all))
    {
        float dd = 0;
        if (const u32 dl = DestroyMarkedLoot(all, &dd))
        {
            if (g.mode == Mode::Shoot && g.target == dl)
                return ShadowExit("marked-loot", Mode::Shoot, dl, Action::Fight, true, true);
            events::Push("autopilot objective decide reason=%s: marked loot %u to destroy at %.0f m - shoot it", reason,
                         dl, dd);
            return ShadowExit("marked-loot", Mode::Shoot, dl, Action::Fight, DecShoot(dl));
        }
    }
    // TIMER-DESTROY (`timer_destroy`; run-judged 30 Oct 2026, look0 M13 from Save3989: "Destroy the Nomads" was fought -
    // NomadCap2, 120,000 hp, from 3 km with 0 batteries after the first battleship - and the ship died in one hit 5 s
    // after "Dock with Planet Toledo"; the step ends on the script's clock ~20 s after it shows, whatever is killed, and
    // the runners dock at Toledo the moment it unlocks, R3 1.4 km flown, 0 launches, 0 hull lost): a "Destroy ..." step
    // the script advances by itself is not fought (lookahead.h TimerDestroy)
    if (g_set.timerDestroy)
    {
        const lookahead::TimerDestroyFacts tf = TimerDestroyFactsNow(st, verb, all);
        const lookahead::TdResult tr = lookahead::TimerDestroy(tf);
        g_obj.tdVerdict = static_cast<int>(tr.verdict);
        if (tr.verdict != lookahead::TdVerdict::Today)
        {
            static std::string said;
            char key[96];
            sprintf_s(key, "%u %s %s", st.textId, lookahead::TdVerdictName(tr.verdict), tr.why.c_str());
            if (said != key)
            {
                said = key;
                events::Push("autopilot objective timer-destroy %u -> %s why=%s advance=%s next=%s place=%.0f m "
                             "hull=%.2f batt=%d attacked=%d speedrun=%d",
                             st.textId, lookahead::TdVerdictName(tr.verdict), tr.why.c_str(), tf.advance.c_str(),
                             TimerDestroyNext().c_str(), tr.dist, tf.hull, tf.batteries, tf.attacked ? 1 : 0,
                             tf.speedrun ? 1 : 0);
            }
        }
        const bool notFought = tr.verdict == lookahead::TdVerdict::Stage || tr.verdict == lookahead::TdVerdict::Rest ||
                               tr.verdict == lookahead::TdVerdict::Defend || tr.verdict == lookahead::TdVerdict::Dock;
        if (notFought)
            g_obj.defendOnly = true, g_obj.fightObjective = false;
        // (td_window: the script takes the dock now - the travel docks with the next objective's object)
        if (tr.verdict == lookahead::TdVerdict::Dock)
        {
            g_obj.travelObjective = true;
            return ShadowExit("timer-destroy", Mode::Travel, tf.nextObject, Action::Dock,
                              StartTravel(tf.nextObject, nullptr, 0, false));
        }
        // (Action::PreStage: the threat check leaves the trip and the rest alone - the battle round us is no attack)
        if (tr.verdict == lookahead::TdVerdict::Stage)
        {
            g_obj.travelObjective = true;
            return ShadowExit("timer-destroy", Mode::Travel, 0, Action::PreStage, StartTravel(0, tr.point, 0, true));
        }
        if (tr.verdict == lookahead::TdVerdict::Rest)
            return ShadowExit("timer-destroy", Mode::Hold, 0, Action::PreStage, StartOrKeep(Mode::Hold, 0));
        if (tr.verdict == lookahead::TdVerdict::Defend)
            return ShadowExit("timer-destroy", Mode::Combat, 0, Action::Survive, StartOrKeep(Mode::Combat, 0));
    }
    // ALLY-KILL (`ally_kill`; docs\research\runners\M13_gate_allykill.md - M13's gate battleship: 3 deaths in ~16 legs on
    // 31 Oct, ~20 batteries spent there; R2 sits at the hole while the allies kill NomadCap3, check_cap counts any killer
    // and unlocks the hole): the step whose kill anyone may make and which unlocks the next step's dock is not fought - the
    // ship waits at that dock (a point trip: the dock itself is locked), answers only what shoots at it and docks on the
    // unlock (the next step's own decision); the fight as before once nothing friendly wears the target down
    if (g_set.allyKill)
    {
        const lookahead::AllyKillFacts af = AllyKillFactsNow(st, all);
        const lookahead::TdResult ar = lookahead::AllyKill(af);
        g_obj.akVerdict = static_cast<int>(ar.verdict);
        if (ar.verdict == lookahead::TdVerdict::Fight)
            g_obj.akFellBack = st.textId;
        if (ar.verdict != lookahead::TdVerdict::Today)
        {
            static std::string said;
            char key[96];
            sprintf_s(key, "%u %s %s", st.textId, lookahead::TdVerdictName(ar.verdict), ar.why.c_str());
            if (said != key)
            {
                said = key;
                events::Push("autopilot objective ally-kill %u -> %s why=%s target=%u hp=%.3f allies=%d dist=%.0f "
                             "waited=%.1f drop=%.3f hull=%.2f batt=%d attacked=%d",
                             st.textId, lookahead::TdVerdictName(ar.verdict), ar.why.c_str(), af.target, af.targetHull,
                             af.allies, ar.dist, af.waitedS, af.hullDrop, af.hull, af.batteries, af.attacked ? 1 : 0);
            }
        }
        if (ar.verdict == lookahead::TdVerdict::Stage || ar.verdict == lookahead::TdVerdict::Rest ||
            ar.verdict == lookahead::TdVerdict::Defend)
            g_obj.defendOnly = true, g_obj.fightObjective = false, g_obj.allyKillTarget = af.target;
        // (Action::PreStage: the threat check leaves the trip and the rest alone - the battle round us is no attack)
        if (ar.verdict == lookahead::TdVerdict::Stage)
        {
            g_obj.travelObjective = true;
            return ShadowExit("ally-kill", Mode::Travel, 0, Action::PreStage, StartTravel(0, ar.point, 0, true));
        }
        if (ar.verdict == lookahead::TdVerdict::Rest)
            return ShadowExit("ally-kill", Mode::Hold, 0, Action::PreStage, StartOrKeep(Mode::Hold, 0));
        if (ar.verdict == lookahead::TdVerdict::Defend)
            return ShadowExit("ally-kill", Mode::Combat, 0, Action::Survive, StartOrKeep(Mode::Combat, 0));
    }
    // `escort_by_place` (M13-MIMIC item 1, docs\research\runners\M13.md "## Plan"; td1full 30 Oct 2026: "Escort Sinclair to
    // the Battleship Osiris" took 136.9 s - a 16.5 s fight at the relaunch, then formation at 245 m/s - where all three
    // runners cruise the 29.7 km in 105-112 s and leave Sinclair; the step ends on the PLAYER within 1 km of the Osiris):
    // the live table's player distance goal, flown straight to (a trip: the threat check leaves it alone)
    if (g_set.escortByPlace && Knows(Source::Hidden) && verb == ObjVerb::Escort)
    {
        const mission::EscortPlace ep =
            mission::EscortByPlace(mission::LiveTriggers(), fl::PlayerSystem(), st.textId);
        space::Pilot me;
        const mission::Goal* best = nullptr;
        float bestPos[3] = {}, bestFar = 1e30f;
        if (ep.ok && space::ReadPilot(me))
            for (const mission::Goal& eg : ep.goals)
            {
                float p[3];
                if (eg.kind == 1 ? !space::ObjectPos(eg.object, p) : (memcpy(p, eg.pos, sizeof(p)), false))
                    continue;
                const float d[3] = {p[0] - me.pos[0], p[1] - me.pos[1], p[2] - me.pos[2]};
                if (Len3(d) - eg.dist < bestFar)
                    bestFar = Len3(d) - eg.dist, best = &eg, memcpy(bestPos, p, sizeof(p));
            }
        // (the place reached and the next step's dock shown, then the escort objective shown again - mm4-b_i1, 30 Oct
        // 2026: "Dock with the Battleship Osiris" 24445, 1 s later 24440 again; with no player goal left the escort fell
        // back to formation on Sinclair and stalled 69 s; 1 of 10 S415 runs): the dock shown last goes on
        if (!best && ep.why == "no player goal" && g_obj.lastDock && FindContact(g_obj.lastDock).id)
        {
            events::Push("autopilot objective escort-by-place %u: its place reached, the dock shown before (%u) goes on",
                         st.textId, g_obj.lastDock);
            return ShadowExit("dock-entry", Mode::Travel, g_obj.lastDock, Action::Dock,
                              StartTravel(g_obj.lastDock, nullptr, 0, false));
        }
        static std::string said;
        const std::string key = std::to_string(st.textId) + (best ? " " + best->trigger : " no " + ep.why);
        if (said != key)
        {
            said = key;
            events::Push("autopilot objective escort-by-place %u -> %s, %.0f m to go", st.textId,
                         best ? (best->trigger + " (" + best->leads + ")").c_str() : ("none: " + ep.why).c_str(),
                         best ? bestFar : 0.0f);
        }
        // `place_dock` (M13-MIMIC item 6, docs\research\runners\M13.md "## Plan" 6): the place is the next step's dock -
        // in_osiris_range (1 km of the Osiris) leads to dock_with_osiris, a dock with that same object, which the script
        // already lets the player take (Act_PlayerCanDock = false, osiris: the one allowed): the trip is that dock, flown
        // through the trigger at cruise into the bay (R3 docks at the 24445 banner; ours stopped 700 m out and took 12 s)
        u32 nextText = 0;
        if (g_set.placeDock && best && bestFar > 0 && best->kind == 1 &&
            mission::ObjectiveObjectOf(best->leads, best->file, &nextText) == best->object && nextText &&
            ObjectiveVerb(nextText) == ObjVerb::Dock)
        {
            events::Push("autopilot objective place-dock %s -> %s: %u docked with on the way (%u)", best->trigger.c_str(),
                         best->leads.c_str(), best->object, nextText);
            g_obj.travelObjective = true;
            g_obj.goalTrigger = best->trigger;
            return ShadowExit("place-dock", Mode::Travel, best->object, Action::Dock,
                              StartTravel(best->object, nullptr, 0, false));
        }
        if (best && bestFar > 0)
        {
            float q[3];
            const float d[3] = {me.pos[0] - bestPos[0], me.pos[1] - bestPos[1], me.pos[2] - bestPos[2]};
            const float l = std::max(1.0f, Len3(d));
            for (int i = 0; i < 3; ++i)
                q[i] = bestPos[i] + d[i] / l * best->dist * 0.7f;
            g_obj.travelObjective = true;
            g_obj.goalTrigger = best->trigger;
            return ShadowExit("escort-by-place", Mode::Travel, 0, Action::ScriptGoal, StartTravel(0, q, 0, true));
        }
    }
    u32 target = 0;
    for (int i = 0; i < st.refCount && !target; ++i)
        target = space::ObjectiveObject(st.refs[i], all);
    // No target and no navmarker: the mission's newest mark is what the words mean (M06 "Fly to Quintaine" 23142 is a
    // plain text objective; the script marks Quintaine - the ship rested and the nag failed the mission, 22 Oct 2026).
    // Not for a fight (its marks are the fight's business) and never a hostile mark here.
    const bool markTarget = !target && !st.hasNav && verb != ObjVerb::Fight; // the target below is only such a mark
    if (markTarget)
    {
        u32 marks[32];
        const int n = MissionObjectIds(marks, 32, all);
        for (int i = 0; i < n && !target; ++i)
            for (const space::Contact& c : all)
                if (c.id == marks[i] && c.health != 0 && !HostileContact(c))
                {
                    target = c.id;
                    break;
                }
        if (target)
            events::Push("autopilot objective mark-target=%u (no target, no navmarker)", target);
    }
    const auto it = std::find_if(all.begin(), all.end(), [&](const space::Contact& c) { return c.id == target; });
    const space::Contact tc = it == all.end() ? space::Contact{} : *it;
    // Marked loot already within the beam's reach is pulled in passing (TractorMarkedLoot): the running trip keeps its
    // line (session 8, 23 Oct 2026, M12 core: the power cell spawned 843 m off, a trip "to" it turned the ship about at
    // 297 m/s, the exit then lay 164 deg behind and its run looped back for 11-36 s; the user's try 3 left the core 2.5 s
    // after the tractor, never turning)
    if (markTarget && tc.id && (tc.type & space::kLoot) && g.mode == Mode::Travel && g_obj.sub == Mode::Travel)
    {
        ai::Self me;
        const float reach = ai::ReadSelf(me) && me.tractorRange > 0 ? me.tractorRange : 1500.0f;
        if (tc.distance < reach)
        {
            // (tractor_exit_run: under the cruise lock the trip's own airlock - the exit - is the way on now, the beam pulls
            // the loot in meanwhile; s10-h-i3 and 5 more fail states, the M12 core at 300 m/s: the cell's pickup completed
            // ~200 m before the static plate, the exit run took over too late for its side passage, struck the plate and
            // lost the exit door. The exit opens on the pickup, ~2.7 s later - the run to it takes ~5 s)
            const State::Travel& tr = g.tr;
            // (within tractor_exit_range of the door's outer point only: s10-j-i3, switched 970 m out, 132 m off the M12 core's
            // axis, the run headed for the rim of the ~90 m middle hole at 290 m/s, the guard's swerve turned the ship about -
            // a loop round the hub and a fail state; the cell trip re-centres on the axis through that hole first)
            float toDoor = 1e30f;
            {
                fl::Kinematics mk = {};
                if (tr.lockHave && fl::PlayerKinematics(mk))
                {
                    const float dd[3] = {tr.lockP2[0] - mk.pos[0], tr.lockP2[1] - mk.pos[1], tr.lockP2[2] - mk.pos[2]};
                    toDoor = Len3(dd);
                }
            }
            if (g_set.tractorExitRun && g_obj.cruiseLock && tr.lockId && tr.lockId != ~0u && tr.lockHave &&
                toDoor < g_set.tractorExitRange)
            {
                events::Push(
                    "autopilot objective decide reason=%s text=%u: marked loot %u at %.0f m within the beam's %.0f m - "
                    "on to the trip's airlock %u now (the beam pulls it in)",
                    reason, st.textId, tc.id, tc.distance, reach, tr.lockId);
                const u32 lock = tr.lockId;
                return ShadowExit("tractor-exit", Mode::Travel, lock, Action::Airlock, StartTravel(lock, nullptr, 0, false));
            }
            // (out of range yet: the supervisor decides again once the door is within tractor_exit_range)
            g_obj.tractorExitPending = g_set.tractorExitRun && g_obj.cruiseLock && tr.lockHave;
            events::Push(
                "autopilot objective decide reason=%s text=%u: marked loot %u at %.0f m within the beam's %.0f m - "
                "the trip goes on",
                reason, st.textId, tc.id, tc.distance, reach);
            return ShadowExit("tractor-exit", Mode::Travel, g_obj.subTarget, g_obj.action, true, true);
        }
    }
    events::Push("autopilot objective decide reason=%s text=%u verb=%d ref=%u target=%u nav=%d", reason, st.textId,
                 static_cast<int>(verb), st.refCount ? st.refs[0] : 0, target, st.hasNav ? 1 : 0);
    LookLog(st, verb, all); // (LOOK L0, look_log: logged only - nothing below reads it)
    // A lane ride the mission asks for and allows is the way out: it outranks the fight around it (M04 "Escape from
    // Liberty", 19 Oct 2026: "Dock with the Trade Lane" TL147 under attack - the fight went on, the ship fled away
    // from the lane and the mission failed 22755).
    u32 escNext = 0, escPrev = 0;
    const bool laneObjective = tc.id && verb == ObjVerb::Dock && space::LaneLinks(tc.id, escNext, escPrev);
    const bool laneEscape = laneObjective && ((escNext && space::LaneOpen(tc.id, escNext) == 1) ||
                                              (escPrev && space::LaneOpen(tc.id, escPrev) == 1));
    // a lane objective still closed: watched (also while fighting) until the mission opens it
    g_obj.laneRing = laneObjective && !laneEscape ? tc.id : 0;
    // ... and a jump gate / hole the objective names is the same kind of way out (M04 "Flee to the Magellan Jump Gate",
    // 19 Oct 2026: the threat check fought the pursuers for 74 s and the mission failed 22770 "failed to take the gate")
    const bool gateEscape = tc.id && (tc.type & (space::kJumpGate | space::kJumpHole)) != 0;
    if (gateEscape)
        return ShadowExit("jump", Mode::Travel, tc.id, Action::Jump, StartTravel(tc.id, nullptr, 0, false));
    // A mission's dock grant (Act_DockRequest) with no dock maneuver after it, an airlock within 3 km: the story's way
    // in - flown through (the fast approach's airlock run). (M12 22 Oct 2026: the lair entry granted while the step read
    // "Fly to the Waypoint" above it; the step had "ended" and the ship fought the Nomads around the lair instead - the
    // user: "M12 is ignoring the objective and throwing itself at Nomads". Before session 4 our own dock request went in.)
    if (SinceGameGrant() < kGameGrantWayInMs && !ai::DockInProgress())
    {
        const space::Contact* lock = nullptr;
        // (never the way we came in while another airlock is in reach: session 8, M12 core exit - the entry was nearer)
        float in[3];
        const bool haveIn = SystemEntry(in);
        auto cameIn = [&](const space::Contact& c)
        {
            const float d[3] = {c.pos[0] - in[0], c.pos[1] - in[1], c.pos[2] - in[2]};
            return haveIn && Len3(d) < 600.0f;
        };
        for (const space::Contact& c : all)
            if ((c.type & space::kAirlockGate) && c.distance < 3000 &&
                (!lock || (cameIn(*lock) && !cameIn(c)) || (cameIn(*lock) == cameIn(c) && c.distance < lock->distance)))
                lock = &c;
        if (lock)
        {
            events::Push("autopilot objective granted airlock %u at %.0f m: through it", lock->id, lock->distance);
            return ShadowExit("airlock", Mode::Travel, lock->id, Action::Airlock, StartTravel(lock->id, nullptr, 0, false));
        }
    }
    // The script's destroy condition for a step that names no target of its own (Source::Hidden; user, 22 Oct 2026, M13
    // from St03: "failed as soon as the player ship targeted a nomad ship rather than fly straight at the generators in the
    // trenches" - "Head to the Barrier" points nowhere, the ship followed Orillion and the threat check dogfought Nomad
    // wings until it died; the script advances on Cnd_Destroyed generator 4, and the runners kill the first generator
    // before "Destroy the generators" shows): its live objects become the marks and the step a fight - the ranking then
    // goes for them first and turns to attackers only when the race is lost (autopilot_target.cpp "pressed").
    // (also a step with a navmarker - M13 "Fly to the Waypoint" in the sphere marks the city; the runners rush the nearest
    // power generator - and "Awaiting Mission Objective": the user saw the ship fight the fighters at the sphere's entry
    // during the 45 s dialogue, 23 Oct 2026)
    // (a place the step names is no reason to skip it either: "Fly to the Waypoint" at the Nomad City - its generators end
    // the mission; 23 Oct 2026)
    space::Pilot mePilot;
    if (Knows(Source::Hidden) && g_set.scriptGoals && verb != ObjVerb::Fight && verb != ObjVerb::Escort &&
        verb != ObjVerb::Dock && (!tc.id || !tc.ship || (markTarget && !HostileContact(tc))) &&
        space::ReadPilot(mePilot))
    {
        bool fromLast = false;
        for (const mission::Goal& sg :
             mission::AdvanceGoals(st.textId, fl::PlayerSystem(), &fromLast, HostObjectiveTitle()))
        {
            if (sg.kind != 3)
                continue;
            // live objects: a contact with hull left, or beyond the contact list a solar the game still places (the city's
            // generators lie 20-60 km off)
            int alive = 0;
            float nearest = 1e30f, nearPos[3] = {};
            u32 nearId = 0;
            for (u32 id : sg.objects)
            {
                if (alive >= 8)
                    break;
                float p[3];
                bool live = false;
                const auto ci =
                    std::find_if(all.begin(), all.end(), [&](const space::Contact& c) { return c.id == id; });
                if (ci != all.end())
                    live = ci->health != 0, memcpy(p, ci->pos, sizeof(p));
                else
                    live = space::ObjectPos(id, p);
                if (!live)
                    continue;
                g_obj.scriptMarks[alive++] = id;
                const float d[3] = {p[0] - mePilot.pos[0], p[1] - mePilot.pos[1], p[2] - mePilot.pos[2]};
                const float dist = Len3(d);
                if (dist < nearest)
                    nearest = dist, nearId = id, memcpy(nearPos, p, sizeof(p));
            }
            if (!alive)
                continue;
            int dead = 0; // (kill_ahead: the kill it would be - the objects seen alive before and gone now)
            {
                std::vector<u32> seenAlive, seenListed;
                SeenDestroyed(seenAlive, seenListed);
                for (u32 id : sg.objects)
                    dead += std::find(seenListed.begin(), seenListed.end(), id) != seenListed.end() &&
                                    std::find(g_obj.scriptMarks, g_obj.scriptMarks + alive, id) ==
                                        g_obj.scriptMarks + alive
                                ? 1
                                : 0;
            }
            // stale_destroy_wait: a goal only the last objective stands in for, none of whose objects the mission marks yet,
            // is a later step's (session 11, every M09 gate: at the Tohoku entry, "Awaiting", the generators 22 km off by
            // the last objective - the trip toward them entered at_nebula_attack's 14.75 km sphere round Tekagi's base, the
            // patrol fight and its two nags failed the mission; the story sends the player to the Blood Dragon outpost first)
            // ...nor one replacing a trip step of its own (a target or navmarker: M09's "Fly to the Blood Dragon Outpost"
            // took the same unmarked generators, s11m)
            // (never one that ends the mission - leads to "null" / SUCCEED: M13's city generators are unmarked at the
            // start too, the rush is the speedrun's way; s11c died holding back)
            if (g_set.staleDestroyWait && (fromLast || tc.id || st.hasNav) && sg.leads != "null" &&
                sg.leads != "SUCCEED")
            {
                u32 mk[32];
                const int mn = MissionObjectIds(mk, 32, all);
                bool marked = false;
                for (int i = 0; i < alive && !marked; ++i)
                    marked = std::find(mk, mk + mn, g_obj.scriptMarks[i]) != mk + mn;
                // `kill_ahead` (M13-MIMIC item 3): a step of the script's own (not a stand-in) with no place of its own
                // and no towards nag - its later step's target is flown to now, and fought once the kill leaves the
                // script's way to the next objective intact (mission::KillAheadSafe); R1 is on barrier generator 1 from
                // +21 s and kills it before "Destroy the Generators" shows; ours chased Nomads 9.7 km off
                mission::TowardsNag tn;
                const bool ahead = g_set.killAhead && !marked && !fromLast && !st.hasNav &&
                                   (!tc.id || (markTarget && !HostileContact(tc))) &&
                                   !mission::TowardsNagOf(st.textId, tn);
                if (ahead)
                {
                    const mission::KillSafe ks = mission::KillAheadSafe(mission::LiveTriggers(), nearId, dead + 1);
                    static std::string said;
                    char key[200];
                    sprintf_s(key, "%s %u %d %s", sg.trigger.c_str(), nearId, dead + 1, ks.ok ? "fight" : ks.why.c_str());
                    if (said != key)
                    {
                        said = key;
                        events::Push("autopilot objective kill-ahead %s: %u at %.0f m (kill %d of %s) -> %s%s",
                                     sg.trigger.c_str(), nearId, nearest, dead + 1, sg.objectName.c_str(),
                                     ks.ok ? "fight" : "wait: ", ks.ok ? "" : ks.why.c_str());
                    }
                    if (ks.ok)
                    {
                        // (the nearest first: the marks are the live objects, the ranking takes the nearest)
                        g_obj.scriptMarkN = alive;
                        g_obj.fightObjective = true;
                        return ShadowExit("kill-ahead", Mode::Combat, 0, Action::ScriptKill,
                                          StartOrKeep(Mode::Combat, 0));
                    }
                    // wait at it: a trip to 600 m short (PreStage - no threat detour), then a rest there
                    g_obj.killAheadWait = nearId, g_obj.killAheadNo = dead + 1;
                    g_obj.travelObjective = true;
                    if (nearest > 900)
                    {
                        float q[3];
                        for (int i = 0; i < 3; ++i)
                            q[i] = nearPos[i] + (mePilot.pos[i] - nearPos[i]) * (600.0f / nearest);
                        return ShadowExit("kill-ahead", Mode::Travel, 0, Action::PreStage, StartTravel(0, q, 0, true));
                    }
                    return ShadowExit("kill-ahead", Mode::Hold, 0, Action::PreStage, StartOrKeep(Mode::Hold, 0));
                }
                if (!marked)
                {
                    events::Push(
                        "autopilot objective script-destroy %s: by the last objective, none marked - a later step's",
                        sg.trigger.c_str());
                    continue;
                }
            }
            g_obj.scriptMarkN = alive;
            g_obj.fightObjective = true;
            events::Push("autopilot objective script-destroy %s: %d of %s (%d alive, nearest %.0f m) -> %s%s",
                         sg.trigger.c_str(), sg.count, sg.objectName.c_str(), alive, nearest, sg.leads.c_str(),
                         fromLast ? " (last objective)" : "");
            // far: to 1.5 km short of the nearest first, as a trip (no threat interrupts it) - as for a far "Destroy ..."
            if (nearest > 5000)
            {
                float q[3];
                for (int i = 0; i < 3; ++i)
                    q[i] = nearPos[i] + (mePilot.pos[i] - nearPos[i]) * (1500.0f / nearest);
                g_obj.travelObjective = true;
                return ShadowExit("script-destroy", Mode::Travel, 0, Action::ToMarks, StartTravel(0, q, 0, true));
            }
            return ShadowExit("script-destroy", Mode::Combat, 0, Action::ScriptKill, StartOrKeep(Mode::Combat, 0));
        }
    }
    // The script's own condition for this step (Source::Hidden: knowledge scope "all"; session 5, 22 Oct 2026 - the top-3
    // speedruns, "objectives are triggers": M12's "Destroy all of the Nomad Ships" in Omicron Minor advances the moment the
    // player is within 8 km of Toledo - the runners fly straight there, 63 s against our 224; M08's "Fly to the Waypoint"
    // advances when Ozu, who follows the player, is within 9 km of the jump hole). A flying condition armed for the step
    // that leads on (a new objective or the mission's success, never a failure) - for a fight step, a navmarker trip or no
    // objective: fly there instead. Not for escorts, docks or a step that already points at a target to reach.
    // (a target that is only the newest friendly mark - "Awaiting Mission Objective" - is no target of the step: M12 23 Oct
    // 2026, Omicron Minor's "Destroy all of the Nomad Ships" shows for 0 ms, then "Awaiting"; the mark fallback chose
    // Juni and the ship escorted her 165 s instead of flying to Toledo - runners 63 s)
    // A Dock objective overwritten by a nag (session 8, 23 Oct 2026, M12 Omicron Minor: "Dock with Planet Toledo", then the
    // `dont_run` nag - more than 5 km from Juni - marked her; when it ended the HUD showed "Awaiting", the script still
    // ends the mission on the dock (Cnd_SpaceExit), and the ship followed Juni for minutes): while only a text the script
    // does not know follows it, the last dock target is docked with
    // (a base or planet only: a lane, gate or hole is transit - M04's "Dock with the Trade Lane" -> "Awaiting" is the ambush
    // starting; the leash test below found no leash there and the dock trip fled the battle again, gate_s8w)
    if (verb == ObjVerb::Dock && tc.id && !(tc.type & (space::kTradelaneRing | space::kJumpGate | space::kJumpHole)))
        g_obj.lastDock = tc.id;
    else if (verb == ObjVerb::Dock)
        g_obj.lastDock = 0;
    else if (g_obj.lastDock)
    {
        bool placeholder = false;
        mission::AdvanceGoals(st.textId, fl::PlayerSystem(), &placeholder, HostObjectiveTitle());
        // (never with a ship leash in the system: M04's "Dock with the Trade Lane" -> "Awaiting" is the script moving on -
        // the ambush; the dock trip flew from Juni, the ring refused, "You fled the battle" 23 s after the launch,
        // gate_s8v)
        if (!placeholder || !FindContact(g_obj.lastDock).id || !mission::Leashes(fl::PlayerSystem()).empty())
            g_obj.lastDock = 0;
        else
        {
            events::Push(
                "autopilot objective decide reason=%s: text %u is no step of the script - the last dock (%u) goes on",
                reason, st.textId, g_obj.lastDock);
            return ShadowExit("dock-entry", Mode::Travel, g_obj.lastDock, Action::Dock,
                              StartTravel(g_obj.lastDock, nullptr, 0, false));
        }
    }
    // (`prestage_next`: the last objective the script set - every step, a dock and a lane ride too: the block below skips
    // those, and M04's "Dock with the Trade Lane" was never seen there; file scope since A37 S4 - the snapshot reads it)
    u32& lastReal = s_lastReal;
    if (st.textId && st.textId != 21660 && st.textId != 1)
        lastReal = st.textId;
    if (Knows(Source::Hidden) && g_set.scriptGoals && verb != ObjVerb::Escort && verb != ObjVerb::Dock &&
        (verb == ObjVerb::Fight || !tc.id || (markTarget && !HostileContact(tc))))
    {
        bool fromLast = false, leashRefused = false;
        std::vector<mission::Goal> goals =
            mission::AdvanceGoals(st.textId, fl::PlayerSystem(), &fromLast, HostObjectiveTitle());
        // A step with its own navmarker and a text no script knows (M10's exit-tunnel waypoints show a blank " ") is not
        // the last known objective: that one's goals are stale (session 8, gate_s8y - "navpoint_12" of the way in to
        // Bruchsal read as fired, the pre-stage sent the ship 24 km back toward Bundschuh base; the leash had hidden it)
        if (fromLast && st.hasNav)
            goals.clear();
        space::Pilot me;
        const mission::Goal* best = nullptr;
        float bestPos[3] = {}, bestDist = 1e30f;
        bool bestWins = false;                // leads to the mission's success
        const mission::Goal* fired = nullptr; // a condition we are inside already: it has fired (or fires now)
        // The mission's leashes (session 8, 23 Oct 2026, M10: under "Awaiting" the ship flew to a script goal 17.6 km off,
        // over 6 km from Von Claussen - "Mission Failed. You fled the battle."): a trip whose near side lies beyond 85 % of
        // a leash from its anchor is none. The anchor: the object where its id resolves (a solar), else the nearest
        // friendly mission ship (mission ships have runtime ids, not their nicknames').
        struct Anchor
        {
            float p[3];
            float dist;
        };
        // (only a leash the player is inside now: one already exceeded without a failure is not armed - the scripts hold
        // phase-bound ones, M04's 1000 m at a lane ring)
        std::vector<Anchor> anchors;
        space::Pilot mePilot0;
        const bool havePilot = space::ReadPilot(mePilot0);
        // (`leash_live`: the armed ones only, from the game's live trigger table - M02's need_to_dock 3 km from King is
        // armed only at the mission's end by comm_1330_king, yet held the whole Ashcroft step: as3 / as4's stage trips
        // turned back at 2.4-2.5 km; without the table: every listed leash, as before)
        const mission::LiveTable leashTable = g_set.leashLive ? mission::LiveTriggers() : mission::LiveTable{};
        const u32 leashSys = fl::PlayerSystem();
        for (const mission::Leash& l :
             leashTable.ok ? mission::ArmedLeashes(leashTable, leashSys) : mission::Leashes(leashSys))
        {
            // disarmed by the script: a comm line whose end deactivates it was heard in the last 30 min (M10's
            // DX_M10_0460B_BOTZLER; the script's own state, observed - mission::Leash::offComms). Wall time: the host
            // stamps the comm with GetTickCount64 (core\game\gameclock.h: never compared with the game clock).
            bool off = false;
            for (const u32 c : l.offComms)
            {
                const unsigned long long at = HostCommHeardAt(c);
                off = off || (at && GetTickCount64() - at < 30ull * 60 * 1000);
            }
            if (off)
            {
                events::Push("autopilot objective leash %s %.0f m: disarmed by the script (its comm was heard)",
                             l.name.c_str(), l.dist);
                if (g_obj.leashShip)
                    g_obj.leashShip = 0;
                continue;
            }
            Anchor a = {{}, l.dist};
            // (the step's own friendly ship target first: M10's mark under "Awaiting" is Von Claussen - the lead rule's
            // escort / mark list did not hold him, session 8 rerun: fled the battle again)
            // The anchor: the leash's own ship found by its name first - kept even while we are outside it: the fail trigger
            // arms later and fails at once if we are out then (session 8, M10: the name resolved Von Claussen 6 km+ off, read
            // as "not armed"; the fallbacks - the step's friendly target, the escort lead - were other ships, 362 / 395 /
            // 444, and the ship fled the battle four times); else the step's friendly target / the lead; a solar: itself
            space::Contact oc = FindContact(l.object);
            // (a mission ship: its runtime id is not its nickname's - found by its pilot's name id, mission::Leash::nameIds)
            if (!(oc.id && oc.ship) && l.nameIds)
            {
                // (by the name id, else by the pilot's name text: Contact::nameIds did not match individual_name in M10)
                const std::string want = events::IdsText(l.nameIds);
                for (const space::Contact& c : all)
                {
                    if (!c.ship)
                        continue;
                    bool same = c.nameIds == l.nameIds;
                    if (!same && c.rep && !want.empty())
                    {
                        const std::wstring rn = space::RepName(c.rep);
                        std::string n;
                        for (wchar_t ch : rn)
                            n += ch < 128 ? static_cast<char>(ch) : '?';
                        same = n == want;
                    }
                    if (same)
                        oc = c;
                }
            }
            const bool named = oc.id && oc.ship;
            const space::Contact lc = named                                     ? oc
                                      : tc.id && tc.ship && !HostileContact(tc) ? tc
                                                                                : FindContact(SurvivalLead(all));
            const char* how = named ? "ship (named)" : "ship";
            bool found = lc.id && (memcpy(a.p, lc.pos, sizeof(a.p)), true);
            float op[3];
            if (!named && space::ObjectPos(l.object, op) && (!found || (oc.id && !oc.ship)))
                memcpy(a.p, op, sizeof(a.p)), found = true, how = "object";
            const float d[3] = {a.p[0] - mePilot0.pos[0], a.p[1] - mePilot0.pos[1], a.p[2] - mePilot0.pos[2]};
            const bool in = found && havePilot && (named || Len3(d) < l.dist);
            if (in)
                anchors.push_back(a);
            if (in && lc.id && strncmp(how, "ship", 4) == 0)
                g_obj.leashShip = lc.id, g_obj.leashDist = l.dist; // (the supervisor keeps every trip to it)
            events::Push("autopilot objective leash %s %.0f m: %s%s", l.name.c_str(), l.dist,
                         !found ? "no anchor" : how, found ? (in ? " (kept)" : " (outside: not armed)") : "");
        }
        const auto leashed = [&](const float* p, float r)
        {
            for (const Anchor& a : anchors)
            {
                const float d[3] = {p[0] - a.p[0], p[1] - a.p[1], p[2] - a.p[2]};
                if (Len3(d) - 0.7f * r > 0.85f * a.dist)
                    return true;
            }
            return false;
        };
        if (space::ReadPilot(me))
            for (const mission::Goal& sg : goals)
            {
                // A fresh objective is no proof of the intended path (M04's "wrong_way": Juni near a cruiser -> "defend
                // yourselves"): a script goal replaces only a fight step, a step with no objective, or a trip whose
                // condition a ship that follows the player drives (M08: Ozu within 9 km of the hole); the mission's
                // success first, then the nearest.
                if (sg.kind == 3)
                {
                    // (destroy conditions: above) - all its objects gone = fired: the script talks before the next step
                    // (M12: generator 3 down -> King's 6.5 s line -> "Fly to the Waypoint"; the ship fought a fighter)
                    if (DestroyFired(sg, all) && !fired && sg.leads != "SUCCEED")
                        fired = &sg;
                    continue;
                }
                if (!(verb == ObjVerb::Fight || fromLast || sg.subject != "player"))
                    continue;
                // a fight is replaced only by a goal its own objective's chain arms (session 8, 23 Oct 2026: M05 "Destroy
                // the Rheinland Ships" left for a ring 29 km off, armed from the system entry - "You failed to destroy the
                // Rheinland ships"; M13's battleship the same, the Osiris / Sinclair goals in turn)
                const bool wins = sg.leads == "SUCCEED" || situation::LeadsToLanding(sg.leads);
                if (verb == ObjVerb::Fight && !fromLast && sg.byEntry)
                    continue;
                // ...and only by a goal that ends the mission (gate rerun, M02 "Destroy ..." -> within 2 km of the jump
                // point 53 km off, armed by the step's own chain: "You ran from the battle" - the battle nags
                // (Act_NagDistLeaving King 5 km) fail it; the M12 case this was built for runs under "Awaiting")
                if (verb == ObjVerb::Fight && !fromLast && !wins)
                    continue;
                if (bestWins && !wins)
                    continue;
                float p[3];
                if (sg.kind == 1 ? !space::ObjectPos(sg.object, p) : (memcpy(p, sg.pos, sizeof(p)), false))
                    continue;
                if (leashed(p, sg.dist))
                {
                    events::Push("autopilot objective script-goal %s: beyond a leash of the mission - no trip",
                                 sg.trigger.c_str());
                    leashRefused = true;
                    continue;
                }
                const float d[3] = {p[0] - me.pos[0], p[1] - me.pos[1], p[2] - me.pos[2]};
                const float far = geom::Len(d);
                // inside already: the trigger fires on its own - or has fired (M12 23 Oct 2026: 678 m from the core point
                // of "within 700 m" under "Awaiting", the 0.9 margin chose it over the power cell to tractor, the trip
                // found "no way" and the ship rested in the core)
                if (far <= sg.dist)
                {
                    if (!fired && sg.leads != "SUCCEED")
                        fired = &sg;
                    continue;
                }
                if (g_obj.badGoal == sg.trigger || g_obj.deadGoals.count(sg.trigger))
                    continue; // its trip found no way this step / it was reached and did not fire
                if ((wins && !bestWins) || far - sg.dist < bestDist)
                    bestDist = far - sg.dist, best = &sg, bestWins = wins, memcpy(bestPos, p, sizeof(p));
            }
        // a navmarker trip whose marker lies inside the goal's reach is the same trip
        if (best && st.hasNav && verb != ObjVerb::Fight)
        {
            const float d[3] = {st.nav[0] - bestPos[0], st.nav[1] - bestPos[1], st.nav[2] - bestPos[2]};
            if (geom::Len(d) < best->dist)
                best = nullptr;
        }
        if (best)
        {
            // to 70 % of the condition's distance on our side of the object or point
            float q[3];
            const float d[3] = {me.pos[0] - bestPos[0], me.pos[1] - bestPos[1], me.pos[2] - bestPos[2]};
            const float l = std::max(1.0f, geom::Len(d));
            for (int i = 0; i < 3; ++i)
                q[i] = bestPos[i] + d[i] / l * best->dist * 0.7f;
            events::Push("autopilot objective script-goal %s within %.0f m of %s (%s) -> %s, %.0f m to go%s",
                         best->trigger.c_str(), best->dist, best->kind == 1 ? best->objectName.c_str() : "a point",
                         best->subject.c_str(), best->leads.c_str(), bestDist, fromLast ? " (last objective)" : "");
            g_obj.travelObjective = true;
            g_obj.goalTrigger = best->trigger;
            // A goal that arms the next one inside its own sphere is passed on the way to that one (session 8, 23 Oct
            // 2026, M12 Omicron Minor: the trip to 0.7 x 8 km of Toledo arrived, braked to 52 m/s, lost the cruise and
            // charged again for the 3 km ring trigger it armed - ~20 s; the runners cruise straight to the ring): aim at
            // the armed goal's near side, flown through - the first trigger fires in passing, the second ends the step.
            for (const mission::Goal& ng : mission::GoalsArmedBy(best->trigger, best->file, fl::PlayerSystem()))
            {
                float gp[3];
                if (ng.kind == 3 || ng.subject != "player" ||
                    !(ng.kind == 2 ? (memcpy(gp, ng.pos, sizeof(gp)), true) : space::ObjectPos(ng.object, gp)))
                    continue;
                if (leashed(gp, ng.dist))
                    continue;
                const float in[3] = {gp[0] - bestPos[0], gp[1] - bestPos[1], gp[2] - bestPos[2]};
                const float tg[3] = {gp[0] - me.pos[0], gp[1] - me.pos[1], gp[2] - me.pos[2]};
                const float tl = Len3(tg);
                if (Len3(in) > best->dist || tl <= ng.dist)
                    continue; // only a goal inside the first one's sphere (the line to it crosses that sphere)
                float q2[3], dir[3];
                for (int i = 0; i < 3; ++i)
                    q2[i] = gp[i] - tg[i] / tl * ng.dist * 0.7f, dir[i] = tg[i] / tl;
                events::Push("autopilot objective script-goal %s arms %s inside it: through to within %.0f m of %s, "
                             "%.0f m to go",
                             best->trigger.c_str(), ng.trigger.c_str(), ng.dist,
                             ng.kind == 1 ? ng.objectName.c_str() : "a point", tl - ng.dist);
                memcpy(g_obj.flyGoal, gp, sizeof(gp)), g_obj.flyGoalR = ng.dist;
                const bool ok = StartTravel(0, q2, 0, true);
                DecFly(ok, dir, ng.dist, ng.dist * 0.7f);
                return ShadowExit("script-goal", Mode::Travel, 0, Action::ScriptGoal, ok);
            }
            return ShadowExit("script-goal", Mode::Travel, 0, Action::ScriptGoal, StartTravel(0, q, 0, true));
        }
        // A fired distance condition whose chain arms the next one (session 7, 23 Oct 2026, M12 Omicron Minor: "within 8 km
        // of Toledo" fired -> "within 3 km of its ring" -> "Dock with Planet Toledo"; with nothing armed in view the ship
        // followed Juni at 5.6 km): the armed one is the trip
        if (fired && fired->kind != 3 && verb != ObjVerb::Fight)
            for (const mission::Goal& ng : mission::GoalsArmedBy(fired->trigger, fired->file, fl::PlayerSystem()))
            {
                float gp[3];
                if (ng.kind == 3 || ng.subject != "player" || g_obj.deadGoals.count(ng.trigger) ||
                    !(ng.kind == 2 ? (memcpy(gp, ng.pos, sizeof(gp)), true) : space::ObjectPos(ng.object, gp)))
                    continue;
                if (leashed(gp, ng.dist))
                    continue;
                const float tg[3] = {gp[0] - me.pos[0], gp[1] - me.pos[1], gp[2] - me.pos[2]};
                const float tl = Len3(tg);
                if (tl <= ng.dist)
                    continue;
                float q[3];
                for (int i = 0; i < 3; ++i)
                    q[i] = gp[i] - tg[i] / tl * ng.dist * 0.7f;
                events::Push("autopilot objective script-goal %s armed by %s: within %.0f m of %s -> %s, %.0f m to go",
                             ng.trigger.c_str(), fired->trigger.c_str(), ng.dist,
                             ng.kind == 1 ? ng.objectName.c_str() : "a point", ng.leads.c_str(), tl - ng.dist);
                g_obj.travelObjective = true;
                g_obj.goalTrigger = ng.trigger;
                memcpy(g_obj.flyGoal, gp, sizeof(gp)), g_obj.flyGoalR = ng.dist;
                return ShadowExit("script-goal", Mode::Travel, 0, Action::ScriptGoal, StartTravel(0, q, 0, true));
            }
        // The step's condition has fired and the script still talks before the next objective (a placeholder stands in:
        // fromLast) - be where the next step's targets are (session 7, 23 Oct 2026, the user watching M12: the ship fought 5
        // interceptors through the lair's ~31 s dialogue and reached generator 1 12 s after "Destroy all the Shield
        // Generators"; the user's try 3 and run 1 wait beside generator 1 and fire the second the banner shows - the
        // interceptors are never fought). The nearest live object of the next step's destroy condition: there by a trip,
        // then a hold 300 m off its centre on our side; the threat check leaves both alone ("pre-stage").
        // (also a fight step whose destroy condition is met while the script talks: M12's generator 3 -> King's 6.5 s line
        // -> "Fly to the Waypoint" within 500 m of the airlock's top - the ship fought a fighter meanwhile; the next step's
        // distance condition is flown to, its near side, through)
        if (fired && ((fromLast && verb == ObjVerb::None) || (fired->kind == 3 && verb == ObjVerb::Fight)))
            for (const mission::Goal& ng : mission::GoalsAfter(fired->leads, fired->file, fl::PlayerSystem()))
            {
                float gp[3];
                if ((ng.kind == 1 || ng.kind == 2) && ng.subject == "player" &&
                    (ng.kind == 2 ? (memcpy(gp, ng.pos, sizeof(gp)), true) : space::ObjectPos(ng.object, gp)))
                {
                    const float tg[3] = {gp[0] - me.pos[0], gp[1] - me.pos[1], gp[2] - me.pos[2]};
                    const float tl = Len3(tg);
                    if (tl <= ng.dist)
                        continue; // inside it already: the step fires the moment it is set
                    float q[3], dir[3];
                    for (int i = 0; i < 3; ++i)
                        q[i] = gp[i] - tg[i] / tl * ng.dist * 0.7f, dir[i] = tg[i] / tl;
                    events::Push("autopilot objective pre-stage %s -> %s: next step's %s, within %.0f m, %.0f m to go",
                                 fired->trigger.c_str(), fired->leads.c_str(), ng.trigger.c_str(), ng.dist,
                                 tl - ng.dist);
                    g_obj.travelObjective = true;
                    memcpy(g_obj.flyGoal, gp, sizeof(gp)), g_obj.flyGoalR = ng.dist;
                    const bool ok = StartTravel(0, q, 0, true);
                    DecFly(ok, dir, ng.dist, ng.dist * 0.7f);
                    return ShadowExit("pre-stage", Mode::Travel, 0, Action::PreStage, ok);
                }
                if (ng.kind != 3)
                    continue;
                float nearPos[3] = {}, nearest = 1e30f;
                u32 nearId = 0;
                for (u32 id : ng.objects)
                {
                    float p[3];
                    if (!space::ObjectPos(id, p))
                        continue;
                    const float d[3] = {p[0] - me.pos[0], p[1] - me.pos[1], p[2] - me.pos[2]};
                    if (Len3(d) < nearest)
                        nearest = Len3(d), nearId = id, memcpy(nearPos, p, sizeof(p));
                }
                if (!nearId)
                    continue;
                float q[3];
                const float stand = std::min(300.0f, nearest);
                for (int i = 0; i < 3; ++i)
                    q[i] = nearPos[i] + (me.pos[i] - nearPos[i]) * (stand / std::max(1.0f, nearest));
                events::Push("autopilot objective pre-stage %s -> %s: next step's %s, nearest %u at %.0f m",
                             fired->trigger.c_str(), fired->leads.c_str(), ng.trigger.c_str(), nearId, nearest);
                g_obj.travelObjective = true;
                g_obj.preStageTarget = nearId;
                if (nearest > 700)
                    return ShadowExit("pre-stage", Mode::Travel, 0, Action::PreStage, StartTravel(0, q, 0, true));
                return ShadowExit("pre-stage", Mode::Hold, 0, Action::PreStage, DecHoldAt(q));
            }
        // `prestage_next` (the owner, 29 Oct 2026, M04: "immediately move at top speed to Benford when reaching the
        // badlands, no waiting. This is speedrun mode, after all"; the runners cruise the 5.9 km from the last lane to
        // Benford in 22-24 s, 79-90 % cruise): a placeholder step after an objective whose end was no distance condition
        // (a lane ride: nothing "fired") - the script still talks before the next objective (M04: "Dock with the Trade
        // Lane" -> Juni's DX_M04_0410C -> "Fly to the Waypoint", 20 s of following her at 78 m/s): the next step's own
        // distance goal, read through the dialogue chain (mission::NextStepGoals), is flown to now - its near side.
        {
            std::string next;
            if (g_set.prestageNext && !fired && verb == ObjVerb::None && (fromLast || st.textId == 21660) && lastReal &&
                !st.hasNav && space::ReadPilot(me))
                for (const mission::Goal& ng : mission::NextStepGoals(lastReal, fl::PlayerSystem(), &next))
                {
                    float gp[3];
                    if (!((ng.kind == 1 || ng.kind == 2) && ng.subject == "player" &&
                          (ng.kind == 2 ? (memcpy(gp, ng.pos, sizeof(gp)), true) : space::ObjectPos(ng.object, gp))))
                        continue;
                    if (leashed(gp, ng.dist))
                        continue; // (M04: Juni's 6 km leash - the trip may not break it)
                    const float tg[3] = {gp[0] - me.pos[0], gp[1] - me.pos[1], gp[2] - me.pos[2]};
                    const float tl = Len3(tg);
                    if (tl <= ng.dist)
                        continue;
                    float q[3], dir[3];
                    for (int i = 0; i < 3; ++i)
                        q[i] = gp[i] - tg[i] / tl * ng.dist * 0.7f, dir[i] = tg[i] / tl;
                    events::Push("autopilot objective pre-stage-next %u -> %s: its %s, within %.0f m, %.0f m to go", lastReal,
                                 next.c_str(), ng.trigger.c_str(), ng.dist, tl - ng.dist);
                    g_obj.travelObjective = true;
                    memcpy(g_obj.flyGoal, gp, sizeof(gp)), g_obj.flyGoalR = ng.dist;
                    const bool ok = StartTravel(0, q, 0, true);
                    DecFly(ok, dir, ng.dist, ng.dist * 0.7f);
                    return ShadowExit("pre-stage-next", Mode::Travel, 0, Action::PreStage, ok);
                }
            // ...else the objective the script declares next, where it has a point here (M04: the chain from "Dock with
            // the Trade Lane" runs through lane-exit conditions and comms - none of them a goal the reader follows; the
            // [NNObjective] order has "Fly to the Waypoint" depot_wp1 right after it)
            float np[3];
            u32 nextText = 0;
            if (g_set.prestageNext && !fired && verb == ObjVerb::None && (fromLast || st.textId == 21660) && lastReal &&
                !st.hasNav && space::ReadPilot(me) &&
                mission::NextObjectivePlace(lastReal, fl::PlayerSystem(), np, &nextText) && !leashed(np, 0))
            {
                const float tg[3] = {np[0] - me.pos[0], np[1] - me.pos[1], np[2] - me.pos[2]};
                if (Len3(tg) > 1000.0f)
                {
                    events::Push("autopilot objective pre-stage-next %u -> %u (declared next): its point, %.0f m to go",
                                 lastReal, nextText, Len3(tg));
                    g_obj.travelObjective = true;
                    return ShadowExit("pre-stage-next", Mode::Travel, 0, Action::PreStage, StartTravel(0, np, 0, true));
                }
            }
        }
    }
    // `chase_kill` (ASH-START, docs\research\runners\M02.md "## Plan" 1; Source::Hidden + Meta): a later step's kill ship
    // armed now, alive, hostile and targetable (LOOK's kill verdict) is fought now in a step that asks no fight of its own -
    // M02 "Follow Ashcroft" (22262): the navmarker trip below never targeted him (LOOK_PLAN 7: tgt=0 until +103 s, the pod
    // segment 115 s against the runners' 18-29 s, who catch him during his ~8 s cruise charge). The combat chases a target
    // faster than our engine in cruise and the coast guard fires on him by engine-kill coasts (ChaseStep)
    if (!laneEscape)
    {
        lookahead::LookFacts cf;
        lookahead::LookResult cr;
        if (const u32 cs = ChaseCandidate(st, verb, all, cf, cr))
        {
            g_obj.chaseShip = cs, g_obj.chaseAsked = cs;
            // (spawn_charge: the ship the stage expected - his contact; ChargeFleeing reads it from now on)
            if (g_set.spawnCharge && !g_obj.chargeShip.empty() && cr.ship == g_obj.chargeShip)
                g_obj.chargeId = cs;
            if (g_obj.chaseSaid != cs)
            {
                g_obj.chaseSaid = cs;
                float dSpawn = -1;
                for (const lookahead::KillGoal& ckg : cf.kills)
                    for (const lookahead::KillShip& cks : ckg.ships)
                        if (cks.id == cs && cks.spawnKnown && cf.havePilot)
                        {
                            const float cd[3] = {cks.spawn[0] - cf.me[0], cks.spawn[1] - cf.me[1],
                                                 cks.spawn[2] - cf.me[2]};
                            dSpawn = Len3(cd);
                        }
                fl::Kinematics ck = {};
                flight::Model cm;
                const bool cruise = flight::ReadModel(cm) && cm.cruiseEngaged;
                events::Push("autopilot objective chase %s=%u d_spawn=%.0f dist=%.0f speed=%.0f cruise=%d trigger=%s",
                             cr.ship.c_str(), cs, dSpawn, cr.dist, fl::PlayerKinematics(ck) ? Len3(ck.vel) : -1.0f,
                             cruise ? 1 : 0, cr.trigger.c_str());
            }
            g_obj.travelObjective = false;
            return ShadowExit("chase", Mode::Combat, cs, Action::ScriptKill, StartOrKeep(Mode::Combat, cs));
        }
    }
    // A travel objective (fly to / dock with a place or a navmarker - not a fight, an escort, a defence or a ship to
    // follow) outranks the fight around it: the story sends the player elsewhere (user, 20 Oct 2026: "the autopilot
    // should prioritize breaking off from a fight if a story objective is telling the player to go elsewhere"; M05
    // Sprague: "Fly to the Waypoint Coordinates" while endless wings spawn - it fought 90 s first). Once there
    // (arrived), a threat is fought as before; the mission says "Destroy ..." when a fight is required.
    // (a Dock verb at a solar is a trip even when it still reads hostile: M09 Tekagi's base turns friendly only when the
    // player starts the dock maneuver - it was fought for 4 minutes, 22 Oct 2026)
    // ...and any non-fight step with a navmarker that has gone 2 min: the trip outranks the fights from then on (M13 22 Oct
    // 2026: "Fly to the Waypoint" on the hostile Nomad City - the endless waves pulled the ship into fight after fight for
    // 12 min; the speedrun rule: fight only what the script needs)
    const bool longStep = st.hasNav && verb != ObjVerb::Fight && verb != ObjVerb::Escort && g_obj.stepAt &&
                          gameclock::NowMs64() - g_obj.stepAt > 120000;
    // ...and a hostile place with a navmarker from the start (user, 22 Oct 2026, M13 from the city: "time wastes"; the 2020
    // playthrough flies straight to the Nomad City through the fighters, 20:42-21:15 - ours fought 8 of them for 2 min
    // first: a "Fly to" never asks for a fight, a "Destroy ..." does)
    const bool travelObjective =
        verb != ObjVerb::Fight && verb != ObjVerb::Escort && !g_obj.arrived &&
        ((tc.id && !tc.ship && (!HostileContact(tc) || verb == ObjVerb::Dock || st.hasNav || longStep)) ||
         (!tc.id && st.hasNav));
    g_obj.travelObjective = travelObjective;
    // `spawn_stage` (ASH-START, with chase_kill; Source::Hidden): a fight step's last kill pending, whose firing spawns a
    // later step's fleeing kill ship (M02 "Destroy the Enemy Rogues": the 4th escort's death spawns Ashcroft, who cruises
    // off to his run point 25 km away) - wait beside that spawn inside the fight's leaving nag, the last ship fought there
    // when it comes within reach; 25 s there without the kill: the fight as before (fail closed). The runners are cruising
    // 0.8-2 km from his spawn when the 4th escort dies; we sat 4.6 km off at rest (M02.md M02.ashcroft / M02.pod)
    if (g_set.spawnStage && g_set.chaseKill && Knows(Source::Hidden) && verb == ObjVerb::Fight && !laneEscape &&
        !travelObjective && !g_obj.survival)
    {
        const StagePlan sp = SpawnStageNow(st, all);
        float toGo = 0;
        int v = StageVerdictNow(sp, &toGo);
        // (spawn_charge, design 3: the fleeing ship's facts for the chase - armed whether or not the point was refused)
        if (g_set.spawnCharge && sp.fled)
        {
            if (g_obj.chargeShip != sp.ship)
                g_obj.chargeId = 0, g_obj.chargeSaid = 0;
            g_obj.chargeShip = sp.ship;
            memcpy(g_obj.chargeSpawn, sp.spawn, sizeof(g_obj.chargeSpawn));
            memcpy(g_obj.chargeRun, sp.run, sizeof(g_obj.chargeRun));
        }
        static std::string said;
        const std::string key = std::to_string(st.textId) + " " + sp.trigger + " " + std::to_string(v) + " " +
                                (sp.ok ? std::string() : sp.chargeRefused ? std::string("charge-refused") : sp.why);
        if (v == 4)
        {
            g_obj.stageTimedOut.insert(sp.trigger);
            events::Push("autopilot objective spawn-stage %s timeout -> fight (%.0f s at the point)", sp.trigger.c_str(),
                         lookahead::kStageTimeoutS);
            g_obj.stageTrigger.clear(), g_obj.stageAt = 0, g_obj.stageLast = 0;
            v = 0;
        }
        else if (said != key)
        {
            said = key;
            if (sp.ok)
                events::Push("autopilot objective spawn-stage %s %d/%d alive -> %s spawn d=%.0f P=(%.0f,%.0f) anchor=%s "
                             "%.0f/%.0f m %s to_go=%.0f -> %s (%s)",
                             sp.trigger.c_str(), sp.alive, sp.need, sp.ship.c_str(), sp.st.dSpawn, sp.st.p[0],
                             sp.st.p[2], sp.anchorName.empty() ? "-" : sp.anchorName.c_str(), sp.st.dAnchor, sp.leash,
                             sp.st.why.c_str(), toGo,
                             v == 1 ? "travel" : v == 2 ? "fight" : v == 3 ? "hold" : "today", sp.why.c_str());
            else if (!sp.trigger.empty())
                events::Push("autopilot objective spawn-stage %s: none (%s)", sp.trigger.c_str(), sp.why.c_str());
            // spawn_charge's line: the point and the leashes it keeps to (`bound` the ones that pulled it), the gap the
            // charge is expected to lock at (ChargeGap from the point's distance, our press 0 / 0.5 / 1 s after his) -
            // or why the charge's point is not used
            if (g_set.spawnCharge && sp.charge && sp.ok)
            {
                const lookahead::GapPrediction g0 = lookahead::ChargeGap(sp.st.dSpawn, 0.0f),
                                               g5 = lookahead::ChargeGap(sp.st.dSpawn, 0.5f),
                                               g1 = lookahead::ChargeGap(sp.st.dSpawn, 1.0f);
                events::Push("autopilot spawn-charge stage P=(%.0f,%.0f) d_spawn=%.0f leashes=%d clamp=%s bound=%s "
                             "anchor_d=%.0f expect_lock=%.0f/%.0f/%.0f min=%.0f -> %s ship=%s",
                             sp.st.p[0], sp.st.p[2], sp.st.dSpawn, sp.chargeLeashes, sp.st.why.c_str(),
                             sp.chargeBound.empty() ? "-" : sp.chargeBound.c_str(), sp.st.dAnchor, g0.lockGap,
                             g5.lockGap, g1.lockGap, g0.minGap,
                             v == 1 ? "travel" : v == 2 ? "fight" : v == 3 ? "hold" : "today", sp.ship.c_str());
            }
            else if (g_set.spawnCharge && sp.chargeRefused)
                events::Push("autopilot spawn-charge none (%s) leashes=%d bound=%s -> fight ship=%s",
                             sp.chargeWhy.c_str(), sp.chargeLeashes, sp.chargeBound.empty() ? "-" : sp.chargeBound.c_str(),
                             sp.ship.c_str());
            else if (g_set.spawnCharge && sp.ok && !sp.chargeWhy.empty())
                events::Push("autopilot spawn-charge fallback (%s) -> the spawn_stage point", sp.chargeWhy.c_str());
        }
        if (v)
        {
            g_obj.chargeStage = sp.charge;
            g_obj.stageLastShip = sp.last;
            if (g_obj.stageTrigger != sp.trigger)
                g_obj.stageTrigger = sp.trigger, g_obj.stageAt = 0;
            memcpy(g_obj.stagePos, sp.st.p, sizeof(g_obj.stagePos));
            memcpy(g_obj.stageDir, sp.st.u, sizeof(g_obj.stageDir));
            g_obj.stageVerdict = g_obj.stageLast = v;
            g_obj.stageAsked = sp.trigger;
            if (v != 1 && !g_obj.stageAt)
                g_obj.stageAt = gameclock::NowMs64();
            // (Action::PreStage: the supervisor's threat check leaves the trip and the hold alone - the last ship is left
            // to the fight's own allies until it comes within reach of the point)
            if (v == 1)
            {
                g_obj.travelObjective = true;
                return ShadowExit("spawn-stage", Mode::Travel, 0, Action::PreStage, StartTravel(0, sp.st.p, 0, true));
            }
            if (v == 2)
                return ShadowExit("spawn-stage", Mode::Combat, sp.last, Action::ScriptKill,
                                  StartOrKeep(Mode::Combat, sp.last));
            g_obj.stageNose = true;
            return ShadowExit("spawn-stage", Mode::Hold, 0, Action::PreStage, DecHoldAt(sp.st.p));
        }
        g_obj.stageTrigger.clear(), g_obj.stageLast = 0;
    }
    // A "Destroy ..." step whose marked targets are all far (M13 22 Oct 2026: the city's generators 39-62 km away, the
    // fighter waves dragged the ship off for 18 min - a fight engages within ~3 km only): to 1.5 km short of the nearest
    // first, as a trip (no threat interrupts it)
    if (verb == ObjVerb::Fight)
    {
        u32 mk[32];
        const int mn = MissionObjectIds(mk, 32, all);
        const space::Contact* nm = nullptr;
        for (const space::Contact& c : all)
            if (c.health != 0 && std::find(mk, mk + mn, c.id) != mk + mn && !FriendlyContact(c) &&
                (!nm || c.distance < nm->distance))
                nm = &c;
        space::Pilot me;
        // (`big_coast`: a huge static mark is flown at by the fight itself - its approach cruises in and drops 300 m outside
        // gun reach of the hull for the coast; the trip below stopped 1500 m short at rest - the owner, 29 Oct 2026: "it drops
        // cruise, STOPS, and then goes off doing ... whatever")
        bool hugeStatic = false;
        if (nm && g_set.bigCoast && !nm->ship)
            for (const space::Obstacle& o : space::Obstacles(nm->pos, 50))
                if (o.id == nm->id && o.radius >= 500.0f)
                    hugeStatic = true;
        if (hugeStatic)
            return ShadowExit("mark-fight", Mode::Combat, nm->id, Action::Fight, StartOrKeep(Mode::Combat, nm->id));
        if (nm && nm->distance > 5000 && space::ReadPilot(me))
        {
            float p[3];
            for (int i = 0; i < 3; ++i)
                p[i] = nm->pos[i] + (me.pos[i] - nm->pos[i]) * (1500.0f / nm->distance);
            g_obj.travelObjective = true;
            events::Push("autopilot objective marked target %u is %.0f m off: there first", nm->id, nm->distance);
            return ShadowExit("mark-fight", Mode::Travel, 0, Action::ToMarks, StartTravel(0, p, 0, true));
        }
    }
    // The survival intent: a Survive objective, or the mission's own title (host API v13: "ESCAPE FROM LIBERTY" - the
    // objective "Defend Yourself!" can pass unseen, 20 Oct 2026). Kept for the whole mission (the title), across the
    // objective mode's restarts (a docked scene and relaunch restarted it at Benford and it fought the cruisers).
    const u32 title = HostObjectiveTitle();
    if (title != g_survivalTitle)
    {
        g_survivalTitle = title;
        g_survivalMission = false;
        // By the title's string id first - the same in every language of the game (research A12, 29 Oct 2026: English
        // words alone missed it in a translated install): 22602 "ESCAPE FROM LIBERTY" (M04), the story's one escape
        // title (resources.tsv). The words stay as the fallback for a mod's own titles.
        static const u32 kEscapeTitles[] = {22602};
        for (const u32 id : kEscapeTitles)
            g_survivalMission = g_survivalMission || title == id;
        std::string t = title && !g_survivalMission ? events::IdsText(title) : std::string();
        for (char& ch : t)
            ch = static_cast<char>(tolower(static_cast<unsigned char>(ch)));
        if (t.find("escape") != std::string::npos || t.find("flee") != std::string::npos ||
            t.find("survive") != std::string::npos)
            g_survivalMission = true;
        g_survivalEnded = false;
        space::Pilot sp;
        g_survivalSystem = space::ReadPilot(sp) ? sp.system : 0;
    }
    // `survival_until_jump` (the owner, 29 Oct 2026, M04: "the entire mission up to and after Benford's destruction does
    // not benefit from the player doggedly pursuing time on target. After the player jumps ... that's when speedrunning
    // kills has meaning"): an escape is out of its system at the first jump - the title's stance ends there (the hunters,
    // the wing and Mactan are fought by the normal rules: M04.mactan, +104 s over par, was the stance's 1200 m reach)
    if (g_set.survivalUntilJump && g_survivalMission && !g_survivalEnded && g_survivalSystem)
    {
        space::Pilot sp;
        if (space::ReadPilot(sp) && sp.system && sp.system != g_survivalSystem)
        {
            g_survivalEnded = true;
            g_survivalMission = false;
            g_obj.survival = false;
            events::Push("autopilot objective survival ends: out of the mission's first system (%u -> %u)",
                         g_survivalSystem, sp.system);
        }
    }
    if (verb == ObjVerb::Survive && !g_survivalEnded)
        g_survivalMission = true;
    if (g_survivalMission && !g_obj.survival)
    {
        g_obj.survival = true;
        events::Push("autopilot objective survival text=%u title=%u (no free fights: attackers within %.0f m only)",
                     st.textId, title, kSurvivalReach);
    }
    // pre_aggro: a ship the script is about to turn hostile, near us, is fought now, whatever the step (s12c: the bounty
    // hunters flagged 31 s before "Destroy the Bounty Hunters" while "Fly to Freeport 4" travelled on - a travel objective
    // takes no threat, and the survival stance's target choice only close attackers); never on a dock / lane step
    // (nor on an escort step; with an armed ship leash only a ship within 75 % of it from the leash's anchor - the chase
    // must not break the leash; review, session 12)
    if (verb != ObjVerb::Dock && verb != ObjVerb::Escort && !laneEscape)
        if (const u32 ph = PreHostileNear(all))
        {
            bool inLeash = true;
            if (g_obj.leashShip)
            {
                const space::Contact lc = FindContact(g_obj.leashShip), pc = FindContact(ph);
                if (lc.id && pc.id)
                {
                    const float d[3] = {pc.pos[0] - lc.pos[0], pc.pos[1] - lc.pos[1], pc.pos[2] - lc.pos[2]};
                    inLeash = Len3(d) < 0.75f * g_obj.leashDist;
                }
            }
            if (inLeash)
                return ShadowExit("pre-aggro", Mode::Combat, ph, Action::PreAggro, StartOrKeep(Mode::Combat, ph));
        }
    // Survival with something attacking us and no named target to deal with: stay with the friendly ship the mission
    // brought (its nearest marked / mission ship - Juni) and fight only what comes at us there; none: fight only the
    // close attackers (the combat's reach is kSurvivalReach under this stance).
    // (`survival_calm`, the owner 29 Oct 2026: "better to save the effort and balance survivability and no taking hull
    // damage, while remaining close to the upcoming objective" - the runners' M04 Unity section: 0-2 kills, hull lost
    // 0.0-0.04, run 1 parked at the Unity's hull: under the stance a "destroy" step and its marks are not pursued either)
    const bool calm = g_obj.survival && g_set.survivalCalm;
    // `survival_stage` (speedrun knowledge; the owner, 29 Oct 2026, M04: "wasted some time before starting the first
    // trade lane, it was busy fighting rather than sticking close to the trade lane. Remember - it's a speedrun, lookahead
    // knowledge is key"): under the calm stance a step with no place of its own ("Defend Yourself!", "Destroy the
    // Battleship Unity": 0, 0, 0 markers) waits where the script's next place is - M04: Trade Lane Ring 147, declared
    // two objectives on - fighting only what shoots at us there; the lane step then starts at the ring (lane3 was +25 s
    // over par from a ring 2.4 km off)
    if (calm && g_set.survivalStage && Knows(Source::Hidden) && !laneEscape && !travelObjective && !st.hasNav)
    {
        const u32 from = st.textId && st.textId != 21660 && st.textId != 1 ? st.textId : lastReal;
        float sp[3];
        u32 nt = 0;
        space::Pilot mp;
        if (from && mission::NextObjectivePlace(from, fl::PlayerSystem(), sp, &nt, 3) && space::ReadPilot(mp))
        {
            const float tg[3] = {sp[0] - mp.pos[0], sp[1] - mp.pos[1], sp[2] - mp.pos[2]};
            const float tl = Len3(tg);
            if (ObjectiveThreat(all) && tl < 1500.0f)
                return ShadowExit("survival-stage", Mode::Combat, 0, Action::Survive, StartOrKeep(Mode::Combat, 0));
            float q[3];
            for (int i = 0; i < 3; ++i)
                q[i] = sp[i] - (tl > 1 ? tg[i] / tl : 0.0f) * 300.0f; // (300 m short: beside the ring, not in its mouth)
            static u32 said = 0;
            if (said != nt)
            {
                said = nt;
                events::Push("autopilot objective survival stage %u -> next place %u, %.0f m off", from, nt, tl);
            }
            if (tl > 700.0f)
                return ShadowExit("survival-stage", Mode::Travel, 0, Action::Survive, StartTravel(0, q, 0, true));
            return ShadowExit("survival-stage", Mode::Hold, 0, Action::Survive, StartOrKeep(Mode::Hold, 0));
        }
    }
    if (g_obj.survival && !laneEscape && !travelObjective && (verb != ObjVerb::Fight || calm) &&
        (calm || !(tc.id && HostileContact(tc))) && ObjectiveThreat(all))
    {
        if (const u32 lead = SurvivalLead(all))
            return ShadowExit("survival", Mode::Escort, lead, Action::Survive, StartOrKeep(Mode::Escort, lead));
        return ShadowExit("survival", Mode::Combat, 0, Action::Survive, StartOrKeep(Mode::Combat, 0));
    }
    // `kill_ahead` in a fight step (M13-MIMIC item 4, docs\research\runners\M13.md "## Plan" 4: be at the battleship from
    // its spawn - nomadcap spawns 36.9 s after the launch, "Destroy the Nomad Battleship" shows 12.4 s later; R1 is at its
    // hull 10 s before the banner, R2 fires from 1,358 m 12 s before it, td1 was 4,923 m out fighting interceptors): the
    // next step's destroy target, alive and safe to kill now (mission::KillAheadSafe), becomes this fight's mark
    // (a "defend" step with no object to escort that we fight in counts as a fight step: M13's 24414 "Defend the Order
    // Base" reads as Escort with ref 0 - the form never fired from Save3989, set ab1 31 Oct 2026: 13.7 s on interceptors
    // after nomadcap's spawn; S415 starts past it - the step runs Mode::Escort round the base, target 29, set ab3)
    const bool defendFight = verb == ObjVerb::Escort && !st.refCount;
    if (g_set.killAhead && Knows(Source::Hidden) && g_set.scriptGoals && (verb == ObjVerb::Fight || defendFight) &&
        !laneEscape && !travelObjective && !g_obj.survival && st.textId && st.textId != 21660)
    {
        space::Pilot me;
        const bool placed = space::ReadPilot(me);
        const mission::LiveTable kaLt = mission::LiveTriggers();
        for (const mission::Goal& ng : FightAheadGoals(st.textId, kaLt, defendFight))
        {
            if (ng.kind != 3 || !placed)
                continue;
            int alive = 0;
            u32 nearId = 0;
            float nearest = 1e30f;
            for (u32 id : ng.objects)
            {
                const auto ci = std::find_if(all.begin(), all.end(), [&](const space::Contact& c) { return c.id == id; });
                if (ci == all.end() || ci->health == 0 || alive >= 8)
                    continue;
                g_obj.scriptMarks[alive++] = id;
                if (ci->distance < nearest)
                    nearest = ci->distance, nearId = id;
            }
            if (!alive)
                continue;
            const mission::KillSafe ks = mission::KillAheadSafe(kaLt, nearId, 1);
            static std::string said;
            char key[200];
            sprintf_s(key, "%u %s %u %s", st.textId, ng.trigger.c_str(), nearId, ks.ok ? "fight" : ks.why.c_str());
            if (said != key)
            {
                said = key;
                events::Push("autopilot objective kill-ahead %s (next step's, in fight %u): %u at %.0f m -> %s%s",
                             ng.trigger.c_str(), st.textId, nearId, nearest, ks.ok ? "fight" : "today's fight: ",
                             ks.ok ? "" : ks.why.c_str());
            }
            if (!ks.ok)
                continue;
            g_obj.scriptMarkN = alive;
            g_obj.fightObjective = true;
            return ShadowExit("kill-ahead", Mode::Combat, 0, Action::ScriptKill, StartOrKeep(Mode::Combat, 0));
        }
    }
    // (a "destroy" step with nothing in reach rests: the threat check starts the fight once its targets come)
    // (survival: a hostile objective target is fought only when the objective says so - M04 step 1's "Dock with the
    // Trade Lane" points at TL146, a ring of the now hostile Liberty: the fight around it killed the ship in 7 s)
    if (!laneEscape && !travelObjective &&
        (ObjectiveThreat(all) || (tc.id && HostileContact(tc) && (tc.ship || tc.maxHit < 1e30f) &&
                                  (!g_obj.survival || (verb == ObjVerb::Fight && !calm)))))
    {
        if (verb == ObjVerb::Escort && tc.id && tc.ship && !HostileContact(tc))
            return ShadowExit("threat-fight", Mode::Escort, tc.id, Action::Escort, StartOrKeep(Mode::Escort, tc.id));
        // A placeholder step's marked hostiles while an escort is active are the escort's threats: the escort keeps
        // fighting them carried along with its ship, never a Combat chase away from it (set s13a, 24 Oct 2026: the
        // chase after the Rheinland wing at Mactan killed in 33-38 s against the escort's 43-44, but ended off Juni with
        // a hold / follow interlude and a longer trip - M04.mactan +16.8 s; the user: keep the escort). The escort's own
        // ship first (g.escorted, alive), else the survival stance's lead; none -> the fight as below.
        if (markFight)
        {
            u32 lead = 0;
            if (g.mode == Mode::Escort && g.escorted)
                for (const space::Contact& c : all)
                    if (c.id == g.escorted && c.ship && c.health != 0)
                        lead = c.id;
            if (!lead && g_obj.survival)
                lead = SurvivalLead(all);
            if (lead)
                return ShadowExit("threat-fight", Mode::Escort, lead, Action::Fight, StartOrKeep(Mode::Escort, lead));
        }
        // Survival: the objective's own hostile ship is the chosen target ("Destroy the Battleship Unity": the ranking no
        // longer lifts marked big ships under the stance - they are scenery otherwise, 21 Oct 2026)
        if (g_obj.survival && verb == ObjVerb::Fight && tc.id && tc.ship && HostileContact(tc))
            return ShadowExit("threat-fight", Mode::Combat, tc.id, Action::Fight, StartOrKeep(Mode::Combat, tc.id));
        return ShadowExit("threat-fight", Mode::Combat, 0, Action::Fight, StartOrKeep(Mode::Combat, 0));
    }
    // "Escort the Transport Convoy to the First Buoy": the target is where to (a buoy), the escorted ship is the friendly
    // ship the mission marks (M03: more than 1200 m from transport1 fails the mission - the travel flew ahead to the
    // buoy, 19 Oct 2026). The nearest friendly marked ship; formation as for an escort target.
    if (tc.id && tc.ship && !HostileContact(tc))
        g_obj.escortShip = tc.id; // "Fly to the Transport Convoy": the ship the escort is about
    if (verb == ObjVerb::Escort && !(tc.id && tc.ship))
    {
        // the ship the mission pointed at earlier (M03: transport1 - the convoy moves only with the player in its
        // formation; the nearest marked friendly was another ship, the convoy stalled at buoy 2), else the nearest
        // friendly ship the mission marks
        u32 marks[32];
        const int n = MissionObjectIds(marks, 32, all);
        const space::Contact* best = nullptr;
        for (const space::Contact& c : all)
            if (c.ship && c.id == g_obj.escortShip && c.health != 0)
                best = &c;
        // the ship the objective names first ("Escort Sinclair to the Battleship Osiris", M13 22 Oct 2026: the nearest
        // marked friendly was Juni - who follows the player: neither moved, Sinclair went alone and the nag failed the
        // mission), by the game's own pilot / ship name; else the nearest
        std::string text = events::IdsText(st.textId);
        for (char& ch : text)
            ch = static_cast<char>(tolower(static_cast<unsigned char>(ch)));
        // any word of 4+ letters of the name, as a word of the text (M13: the ship is "Sinclair and Quintaine")
        auto named = [&](const space::Contact& c)
        {
            const std::wstring w = space::SpeakerName(c.id) + L" ";
            std::string word;
            for (wchar_t ch : w)
            {
                if (ch < 128 && isalpha(static_cast<unsigned char>(ch)))
                {
                    word += static_cast<char>(tolower(ch));
                    continue;
                }
                if (word.size() >= 4)
                    for (size_t at = text.find(word); at != std::string::npos; at = text.find(word, at + 1))
                    {
                        const size_t end = at + word.size();
                        if ((at == 0 || !isalpha(static_cast<unsigned char>(text[at - 1]))) &&
                            (end >= text.size() || !isalpha(static_cast<unsigned char>(text[end]))))
                            return true;
                    }
                word.clear();
            }
            return false;
        };
        if (!best)
            for (const space::Contact& c : all)
                if (c.ship && !HostileContact(c) && std::find(marks, marks + n, c.id) != marks + n)
                {
                    const bool cn = named(c), bn = best && named(*best);
                    if (!best || (cn && !bn) || (cn == bn && c.distance < best->distance))
                        best = &c;
                }
        if (best && named(*best))
            events::Push("autopilot objective escort the named ship %u", best->id);
        if (best)
        {
            if (g_obj.sub != Mode::Formation && g_obj.sub != Mode::Follow)
                return ShadowExit("escort", Mode::Formation, best->id, Action::Formation,
                                  StartOrKeep(Mode::Formation, best->id));
            // (StartOrKeep never writes g_obj.sub - Start's restart stop writes only lastStop / lastStopReason - so the
            // arguments' evaluation order cannot change the mode or action read here)
            return ShadowExit("escort", g_obj.sub, best->id, g_obj.sub == Mode::Formation ? Action::Formation : Action::Follow,
                              StartOrKeep(g_obj.sub, best->id));
        }
    }
    if (tc.id && tc.ship)
    {
        if (verb == ObjVerb::Escort && g_obj.sub != Mode::Formation)
            return ShadowExit("ship-target", Mode::Formation, tc.id, Action::Formation, StartOrKeep(Mode::Formation, tc.id));
        return ShadowExit("ship-target", Mode::Follow, tc.id, Action::Follow, StartOrKeep(Mode::Follow, tc.id));
    }
    // "Dock with the Trade Lane" (a dock verb at a lane ring): ride it toward the neighbour the mission lets the player
    // take (space::LaneOpen - M03 step 5 opens only 42 -> 43; was: the travel arrived beside the ring and rested, 19 Oct
    // 2026). Far away: arrive near it first (the travel's routing); close: the fast lane entry toward that ring.
    u32 laneNext = 0, lanePrev = 0;
    if (tc.id && verb == ObjVerb::Dock && space::LaneLinks(tc.id, laneNext, lanePrev))
    {
        u32 toward = 0;
        bool known = false;
        for (u32 cand : {laneNext, lanePrev})
        {
            const int o = cand ? space::LaneOpen(tc.id, cand) : -1;
            known |= o >= 0;
            if (o == 1 && !toward)
                toward = cand;
        }
        if (!toward && known)
        {
            // closed now, but the objective names it: the mission opens it later (M04 West Point: when Juni reaches
            // TL134, then 100 s to enter) - wait beside the ring, re-decided when it opens (ObjectiveSupervise)
            g_obj.laneRing = tc.id;
            events::Push("autopilot objective lane=%u closed both ways (the mission's lane permission): waiting at it",
                         tc.id);
            if (tc.distance > 1500 && !g_obj.arrived)
                return ShadowExit("lane", Mode::Travel, tc.id, Action::LaneWait, StartTravel(tc.id, nullptr, 0, true));
            return ShadowExit("lane", Mode::Hold, 0, Action::LaneWait, StartOrKeep(Mode::Hold, 0));
        }
        g_obj.laneRing = 0;
        if (!toward)
            toward = laneNext ? laneNext : lanePrev;
        if (tc.distance > 3000 && !g_obj.arrived) // (action "lane": the threat check leaves the approach alone)
            return ShadowExit("lane", Mode::Travel, tc.id, Action::Lane, StartTravel(tc.id, nullptr, 0, true));
        const bool ok = DecDock(tc.id, toward);
        events::Push("autopilot objective lane=%u toward=%u (ride)", tc.id, toward);
        return ShadowExit("lane", Mode::Dock, tc.id, Action::Lane, ok);
    }
    // Survival never rests in place (M04 20 Oct 2026: "Awaiting Mission Objective" -> hold beside the Unity battle,
    // dead in 6 s): with nothing to fly to, stay with the lead the mission brought (Juni) - she leads the way out.
    const u32 lead = g_obj.survival && !(tc.id && !g_obj.arrived) && !st.hasNav ? SurvivalLead(all) : 0;
    // ... and beside her, rest the way a player waits: in place, the nose on the scene (HoldLook -> MissionSubject: a
    // place before a wingman - user, 20 Oct 2026: at Mactan look at Mactan, not at Juni); the supervisor rejoins her
    // the moment she moves off (survive-hold)
    // (`rule`: the shadow's branch name - the arrived rest or the final rest)
    const auto stayWithLead = [&](const char* rule)
    {
        const space::Contact lc = FindContact(lead);
        if (lc.id && lc.distance < g_set.followDistance)
            return ShadowExit(rule, Mode::Hold, 0, Action::SurviveHold, StartOrKeep(Mode::Hold, 0));
        return ShadowExit(rule, Mode::Escort, lead, Action::SurviveStay, StartOrKeep(Mode::Escort, lead));
    };
    // A travel the mission flies with a leader it named (its formation order): stay in that formation - the trip's
    // trigger may be the leader's arrival (M09 Ozu), and the leader is the mission's pace
    if (travelObjective && g_obj.missionLeader)
    {
        const space::Contact lc = FindContact(g_obj.missionLeader);
        if (lc.id && lc.health != 0 && FriendlyContact(lc))
            return ShadowExit("mission-form", Mode::Formation, lc.id, Action::MissionForm,
                              StartOrKeep(Mode::Formation, lc.id));
        g_obj.missionLeader = 0;
    }
    if (verb == ObjVerb::None || verb == ObjVerb::Survive)
        if (const float r = PassRingRadius(tc))
        {
            // the travel already flies through it (its look-ahead predicted this ring): keep it
            if (g.mode == Mode::Travel && g.tr.passRing == tc.id)
                return ShadowExit("pass", Mode::Travel, tc.id, Action::Pass, true, true);
            return ShadowExit("pass", Mode::Travel, tc.id, Action::Pass, DecPass(tc.id, r));
        }
    // A route objective (several steps: M07 "Fly to Planet Kyushu" = 5 entries, lane rings then the planet; the host
    // hands over entry 0 only): the game's waypoint list carries the whole route - when the target is one of its entries
    // but not the last, fly the list to its end (lanes and all). Was: arrived beside the first ring, looped there, and the
    // nag toward the planet failed the mission (22 Oct 2026).
    {
        // host v15: the route's entries themselves - travel to the last one (the travel plans the lanes on the way)
        ObjectiveStep steps[8];
        const int ns = HostObjectiveSteps(steps, 8);
        // the mission's own route first: step 0 a lane ring -> ride it toward the neighbour on the way to step 1 (the
        // objective advances as the ride passes its rings; a best path of our own took another lane and the nag toward
        // Kyushu failed M07, 22 Oct 2026)
        u32 n0 = 0, p0 = 0;
        if (ns >= 2 && steps[ns - 1].textId == st.textId && tc.id && space::LaneLinks(tc.id, n0, p0))
        {
            u32 s1 = 0;
            for (int i = 0; i < steps[1].refCount && !s1; ++i)
                s1 = space::ObjectiveObject(steps[1].refs[i], all);
            float s1pos[3] = {};
            const bool haveS1 = s1 ? space::ObjectPos(s1, s1pos) : steps[1].hasNav;
            if (!s1 && steps[1].hasNav)
                memcpy(s1pos, steps[1].nav, sizeof(s1pos));
            u32 toward = 0;
            float best = 1e30f;
            for (u32 cand : {n0, p0})
            {
                float cp[3];
                if (!cand || !haveS1 || !space::ObjectPos(cand, cp))
                    continue;
                const float d[3] = {cp[0] - s1pos[0], cp[1] - s1pos[1], cp[2] - s1pos[2]};
                if (Len3(d) < best)
                    best = Len3(d), toward = cand;
            }
            if (toward)
            {
                events::Push("autopilot objective route-lane ring=%u toward=%u (step 1 %u)", tc.id, toward, s1);
                if (tc.distance > 3000 && !g_obj.arrived)
                    return ShadowExit("route-lane", Mode::Travel, tc.id, Action::Lane,
                                      StartTravel(tc.id, nullptr, 0, true));
                const bool ok = DecDock(tc.id, toward);
                return ShadowExit("route-lane", Mode::Dock, tc.id, Action::Lane, ok);
            }
        }
        // (the route-lane ride above goes on from beside its ring; everything below rests once its trip is done)
        if (g_obj.arrived) // this step's travel is done: rest there (scenic hold)
            return lead ? stayWithLead("arrived")
                        : ShadowExit("arrived", Mode::Hold, 0, Action::Rest, StartOrKeep(Mode::Hold, 0));
        if (ns >= 2 && steps[ns - 1].textId == st.textId)
        {
            const ObjectiveStep& last = steps[ns - 1];
            u32 lastObj = 0;
            for (int i = 0; i < last.refCount && !lastObj; ++i)
                lastObj = space::ObjectiveObject(last.refs[i], all);
            const bool dock = verb == ObjVerb::Dock;
            bool ok = false;
            if (lastObj)
            {
                const u32 ring = space::PlanetDockRing(lastObj);
                ok = StartTravel(ring ? ring : lastObj, nullptr, 0, !dock);
            }
            else if (last.hasNav)
                ok = StartTravel(0, last.nav, last.navSystem, true);
            if (lastObj || last.hasNav)
            {
                events::Push("autopilot objective route: %d steps, to the last (object %u)", ns, lastObj);
                return ShadowExit("route", Mode::Travel, lastObj, Action::Route, ok);
            }
        }
    }
    if (tc.id)
    {
        const std::vector<space::Waypoint> wl = space::Waypoints();
        for (size_t i = 0; i + 1 < wl.size(); ++i)
        {
            const float d[3] = {wl[i].pos[0] - tc.pos[0], wl[i].pos[1] - tc.pos[1], wl[i].pos[2] - tc.pos[2]};
            if (wl[i].object != tc.id && Len3(d) > 1500)
                continue;
            const bool ok = DecWaypoints(verb != ObjVerb::Dock);
            events::Push("autopilot objective route: the mission's waypoint list (%u entries, target is entry %u)",
                         static_cast<u32>(wl.size()), static_cast<u32>(i));
            return ShadowExit("waypoints", Mode::Travel, tc.id, Action::Route, ok);
        }
    }
    if (tc.id)
    {
        const u32 ring = space::PlanetDockRing(tc.id); // a planet: its docking ring (when it has one)
        const u32 dest = ring ? ring : tc.id;
        // an airlock as the objective's own target is a way through, whatever the verb (M13 "Enter the Dyson Sphere":
        // the travel arrived beside St03_dyson_airlock_1 and the ship fought the Nomads outside until it died)
        // (a Nomad airlock reads hostile: the mission opened it, PlayerCanDock - as a Dock verb at a hostile base, abb5b5a)
        const bool dock = verb == ObjVerb::Dock || (!tc.ship && (tc.type & 0x400u));
        return ShadowExit("arrive", Mode::Travel, dest, dock ? Action::Dock : Action::Arrive,
                          StartTravel(dest, nullptr, 0, !dock));
    }
    // A navmarker on a way through - an airlock, a gate, a hole (M12 "Exit the Structure": the marker sits on the
    // core's exit dock St02c_core_exit; arriving beside it stalled 517 m off and the ship rested, 22 Oct 2026): go
    // through that object
    if (st.hasNav)
    {
        const space::Contact* way = nullptr;
        for (const space::Contact& c : all)
        {
            if (c.ship || !(c.type & (0x400u | space::kJumpGate | space::kJumpHole))) // airlock (0x400), gate, hole
                continue;
            const float d[3] = {c.pos[0] - st.nav[0], c.pos[1] - st.nav[1], c.pos[2] - st.nav[2]};
            // on it, not near it (M12 "Fly to the Core": its marker 616 m from the core's exit dock - the old 800 m sent
            // the ship to the exit, crawling through the core's radiation, 22 Oct 2026)
            if (Len3(d) < 200 && (!way || c.distance < way->distance))
                way = &c;
        }
        if (way && !HostileContact(*way))
        {
            events::Push("autopilot objective navmarker on a dockable object %u: through it", way->id);
            return ShadowExit("nav-way", Mode::Travel, way->id, Action::Dock, StartTravel(way->id, nullptr, 0, false));
        }
    }
    if (st.hasNav)
    {
        // A retry after ending short of the marker goes by a via point first: 500 m off the straight line at its middle,
        // up / down / left / right in turn (M12 22 Oct 2026: pinned against the Nomad core's inner structure, every retry
        // on the same blocked line). Then the marker.
        space::Pilot p;
        if (g_obj.viaNext && g_obj.arriveRetries > 0 && space::ReadPilot(p) &&
            (!st.navSystem || st.navSystem == p.system))
        {
            g_obj.viaNext = false;
            float d[3] = {st.nav[0] - p.pos[0], st.nav[1] - p.pos[1], st.nav[2] - p.pos[2]};
            const float dl = std::max(1.0f, Len3(d));
            for (float& x : d)
                x /= dl;
            // a perpendicular pair: world up projected off the line, and the line x up
            float up[3] = {-d[1] * d[0], 1 - d[1] * d[1], -d[1] * d[2]};
            const float ul = Len3(up);
            if (ul < 0.1f)
                up[0] = 1, up[1] = 0, up[2] = 0;
            else
                geom::Norm(up);
            float side[3];
            geom::Cross(side, d, up);
            const int k = (g_obj.arriveRetries - 1) % 4;
            const float* dir = k < 2 ? up : side;
            const float sgn = (k % 2) ? -1.0f : 1.0f;
            float via[3];
            for (int i = 0; i < 3; ++i)
                via[i] = (p.pos[i] + st.nav[i]) * 0.5f + dir[i] * sgn * 500.0f;
            g_obj.inVia = true;
            events::Push("autopilot objective via point %d (%.0f,%.0f,%.0f) before the marker", k, via[0], via[1],
                         via[2]);
            return ShadowExit("via", Mode::Travel, 0, Action::Via, StartTravel(0, via, p.system, true));
        }
        // Fly-through (session 6, 23 Oct 2026; the user's M12 core play-by-play: "flies ahead to the waypoint" without
        // slowing - the step ends inside the script's radius, run 1 never brakes): the script's own condition for this
        // step is the player within R of a point that holds the marker -> aim beyond the marker along the approach.
        // A marker near the sphere's edge (session 7, 23 Oct 2026, M12 "Fly to the Waypoint" above the airlock: marker y
        // 924, condition "within 500 m of y 474" - the ship climbed to the marker and braked there, 45 s; the user's try 3
        // went straight at the airlock, inside the sphere, ~5 s): the trip goes to the sphere's near side (0.7 R from its
        // centre toward us), flown through.
        space::Pilot me;
        float dest[3] = {st.nav[0], st.nav[1], st.nav[2]};
        float flyDir[3] = {}, flyR = 0;
        float flyAhead = 0;
        const char* flyWhy = nullptr;
        if (Knows(Source::Hidden) && g_set.scriptGoals && space::ReadPilot(me) &&
            (!st.navSystem || st.navSystem == fl::PlayerSystem()))
        {
            for (const mission::Goal& sg :
                 mission::AdvanceGoals(st.textId, fl::PlayerSystem(), nullptr, HostObjectiveTitle()))
            {
                float gp[3];
                if (sg.subject != "player" || (sg.kind == 1 ? !space::ObjectPos(sg.object, gp) : sg.kind != 2))
                    continue;
                if (sg.kind == 2)
                    memcpy(gp, sg.pos, sizeof(gp));
                const float off[3] = {st.nav[0] - gp[0], st.nav[1] - gp[1], st.nav[2] - gp[2]};
                if (Len3(off) > sg.dist)
                    continue; // the marker must lie inside the condition's sphere
                const float tg[3] = {gp[0] - me.pos[0], gp[1] - me.pos[1], gp[2] - me.pos[2]};
                if (Len3(tg) <= sg.dist)
                    break; // inside already
                if (Len3(off) > sg.dist * 0.5f)
                {
                    // near the edge: the sphere's near side instead of the marker
                    const float tl = Len3(tg);
                    for (int i = 0; i < 3; ++i)
                        dest[i] = gp[i] - tg[i] / tl * sg.dist * 0.7f;
                    flyWhy = "the condition's near side";
                    flyAhead = sg.dist * 0.7f;
                }
                else
                    flyWhy = "the marker";
                float d[3] = {dest[0] - me.pos[0], dest[1] - me.pos[1], dest[2] - me.pos[2]};
                const float dl = std::max(1.0f, Len3(d));
                for (int i = 0; i < 3; ++i)
                    flyDir[i] = d[i] / dl;
                flyR = sg.dist;
                memcpy(g_obj.flyGoal, gp, sizeof(gp)), g_obj.flyGoalR = sg.dist;
                events::Push(
                    "autopilot objective fly-through %s: within %.0f m ends the step - aim %s (%.0f,%.0f,%.0f)",
                    sg.trigger.c_str(), sg.dist, flyWhy, dest[0], dest[1], dest[2]);
                break;
            }
        }
        const bool started = StartTravel(0, dest, st.navSystem, true);
        if (flyWhy)
            DecFly(started, flyDir, flyR, flyAhead);
        return ShadowExit("navmarker", Mode::Travel, 0, Action::Navmarker, started);
    }
    if (lead)
        return stayWithLead("rest");
    ShadowExit("rest", Mode::Hold, 0, Action::Rest, StartOrKeep(Mode::Hold, 0));
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

// Twice a second while the objective mode is on (from Update, before the modes run): decide again when the objective
// changes, when what it started has ended, or when a fight comes to us.
// The newest spawn when the game was last in its menu (a load or a new game passes through it): the host's spawn list
// spans the whole process, and a mission's ships spawn at the same positions in every attempt - an earlier attempt's ids
// (perhaps reused by the game for other ships) are not this game's (review, session 12)
static SpawnRec s_menuSpawn = {};
static bool s_menuSpawnSet = false;
void NoteMenu()
{
    static std::vector<SpawnRec> buf(2048);
    const int n = HostSpawnedShips(buf.data(), static_cast<int>(buf.size()));
    s_menuSpawnSet = n > 0;
    if (n > 0)
        s_menuSpawn = buf[n - 1];
    // the mission-scope survival memory belongs to the game that ended (audit A1-2, 31 Oct 2026: M04 reloaded from before
    // its jump kept g_survivalEnded under the same title - the stance never came back)
    g_survivalTitle = 0, g_survivalMission = false, g_survivalSystem = 0, g_survivalEnded = false;
    ResetSystemEntry();
}
// (read only: the situation snapshot's "spawns since the last menu", situation_snap.cpp)
bool MenuSpawn(SpawnRec& out)
{
    out = s_menuSpawn;
    return s_menuSpawnSet;
}

// pre_aggro: once a second, the mission ships the step's script is about to turn hostile (read again when the step's
// text, the system or the spawns change) -> SetPreHostile (autopilot.cpp); neutral, near us, they read as hostile to every
// fight test (the overlay of autopilot::Contacts)
static void PreAggroUpdate()
{
    static ULONGLONG next = 0;
    static u32 lastText = 0, lastSystem = 0, lastSpawn = 0;
    static float lastRange = -1;
    static std::vector<SpawnRec> buf(2048);
    const ULONGLONG now = gameclock::NowMs64();
    if (now < next)
        return;
    next = now + 1000;
    const bool on = g_obj.on && g_set.preAggro > 0 && Knows(Source::Hidden);
    if (!on)
    {
        if (lastRange != 0)
            SetPreHostile({}, 0), lastRange = 0, lastText = 0;
        return;
    }
    ObjectiveStep st = {};
    HostObjectiveNow(&st);
    const u32 system = fl::PlayerSystem();
    const int n = HostSpawnedShips(buf.data(), static_cast<int>(buf.size()));
    const u32 newest = n > 0 ? buf[n - 1].id : 0;
    if (st.textId == lastText && system == lastSystem && lastRange == g_set.preAggro && newest == lastSpawn)
        return;
    lastText = st.textId, lastSystem = system, lastRange = g_set.preAggro, lastSpawn = newest;
    // (this game's spawns only: those after the newest one at the last menu)
    int from = 0;
    if (s_menuSpawnSet)
        for (int i = n - 1; i >= 0; --i)
            if (buf[i].id == s_menuSpawn.id && memcmp(buf[i].pos, s_menuSpawn.pos, sizeof(s_menuSpawn.pos)) == 0)
            {
                from = i + 1;
                break;
            }
    std::vector<mission::Spawn> spawns;
    for (int i = from; i < n; ++i)
    {
        mission::Spawn sp;
        sp.id = buf[i].id;
        for (int k = 0; k < 3; ++k)
            sp.pos[k] = buf[i].pos[k];
        spawns.push_back(sp);
    }
    std::vector<std::string> names;
    const std::vector<u32> ids = mission::PendingHostileShips(st.textId, system, spawns, &names);
    SetPreHostile(ids, g_set.preAggro);
    static std::vector<u32> lastIds;
    if (ids == lastIds)
        return;
    lastIds = ids;
    std::string s;
    for (size_t i = 0; i < ids.size() && i < 12; ++i)
        s += " " + names[i] + "=" + std::to_string(ids[i]);
    events::Push("autopilot pre-aggro text=%u spawns=%d ships=%d%s", st.textId, n, static_cast<int>(ids.size()),
                 s.c_str());
}

void ObjectiveSupervise()
{
    PreAggroUpdate();
    if (!g_obj.on)
        return;
    if (ai::MissionHolds())
    {
        // the mission flies the player: a formation it gave is remembered (its leader) - not one of ours
        space::Pilot mp;
        if (g.mode != Mode::Formation && space::ReadPilot(mp))
        {
            if (mp.op == 10 && !mp.leader) // a formation order, its leader not readable until it forms: wait for it
                g_obj.missionFormSince = gameclock::NowMs64();
            if (mp.leader && mp.leader != g_obj.missionLeader)
            {
                g_obj.missionLeader = mp.leader;
                events::Push("autopilot objective mission-formation leader=%u", mp.leader);
            }
            else if (!mp.leader && g_obj.missionLeader && mp.op != -1)
            {
                // the mission flies us without a leader now (M09 break_player_follow): its formation is over
                events::Push("autopilot objective mission-formation over (op=%d)", mp.op);
                g_obj.missionLeader = 0;
            }
        }
        if (g_set.situationShadow) // (A37 S3: the rules' held against this hold)
            situation::ShadowHeld("mission_holds");
        return;
    }
    // (tractor_exit_run waiting for its range: the trip to the door's airlock once within it)
    if (g_obj.tractorExitPending && g.mode == Mode::Travel)
    {
        fl::Kinematics mk = {};
        if (!g_obj.cruiseLock || !g.tr.lockHave)
            g_obj.tractorExitPending = false;
        else if (fl::PlayerKinematics(mk))
        {
            const float dd[3] = {g.tr.lockP2[0] - mk.pos[0], g.tr.lockP2[1] - mk.pos[1], g.tr.lockP2[2] - mk.pos[2]};
            if (Len3(dd) < g_set.tractorExitRange)
            {
                g_obj.tractorExitPending = false;
                ObjectiveDecide("tractor-exit");
                return;
            }
        }
    }
    if (GameDocksUs())
    {
        // the mission docks us (Act_DockRequest): a decision now would cancel it - wait until it is through
        if (!g_obj.gameDock)
            events::Push("autopilot objective the game docks the ship: waiting");
        g_obj.gameDock = true;
        if (g_set.situationShadow)
            situation::ShadowHeld("game_dock");
        return;
    }
    g_obj.gameDock = false;
    const ULONGLONG now = gameclock::NowMs64();
    if (now < g_obj.nextLook)
        return;
    g_obj.nextLook = now + 500;
    // riding a lane / jumping: the game flies; nothing to decide (a rest started now would break the ride)
    space::Pilot pilot;
    if (space::ReadPilot(pilot) && (pilot.lane || pilot.jumping))
    {
        if (g_set.situationShadow)
            situation::ShadowHeld(pilot.lane ? "lane" : "jump");
        return;
    }
    // (`ally_kill`'s recheck here, before the hold branches - probe ak3, 31 Oct 2026: placed after them it was never reached while the ship rested at the dock: no heartbeat after the Rest, the 30 s stall fallback dead)
    static ULONGLONG akLook = 0;
    static u32 akAskedSeq = 0;
    if (g_set.allyKill && g_obj.akVerdict != static_cast<int>(lookahead::TdVerdict::Defend) &&
        g_obj.akVerdict != static_cast<int>(lookahead::TdVerdict::Fight) && now - akLook >= 1000)
    {
        akLook = now;
        ObjectiveStep cur = {};
        if (HostObjectiveNow(&cur) == g_obj.seq)
        {
            const lookahead::AllyKillFacts af = AllyKillFactsNow(cur, Contacts());
            const lookahead::TdResult ar = lookahead::AllyKill(af);
            const bool fromToday = g_obj.akVerdict == static_cast<int>(lookahead::TdVerdict::Today);
            // (a heartbeat while it waits - probe ak2-i4, 31 Oct 2026: rested 3 min at the hole, the target's hull 1.000,
            // the 30 s stall fallback never seen: is this recheck reached, does the wait grow?)
            static ULONGLONG hb = 0;
            if (!fromToday && now - hb >= 10000)
            {
                hb = now;
                events::Push("autopilot objective ally-kill check verdict=%s->%s why=%s waited=%.0f drop=%.3f allies=%d",
                             lookahead::TdVerdictName(static_cast<lookahead::TdVerdict>(g_obj.akVerdict)),
                             lookahead::TdVerdictName(ar.verdict), ar.why.c_str(), af.waitedS, af.hullDrop, af.allies);
            }
            if (static_cast<int>(ar.verdict) != g_obj.akVerdict && (!fromToday || akAskedSeq != g_obj.seq))
            {
                if (fromToday)
                    akAskedSeq = g_obj.seq;
                return ObjectiveDecide("ally-kill");
            }
        }
    }
    // An armed ship leash binds every trip (session 8, M10: the script goals kept to Von Claussen's 6 km, the route's
    // navmarker trip did not - "You fled the battle" again): beyond 75 % of it on a trip, follow the ship instead
    if (g_obj.leashShip && g.mode == Mode::Travel)
    {
        const space::Contact lc = FindContact(g_obj.leashShip);
        if (!lc.id)
            g_obj.leashShip = 0;
        else if (lc.distance > 0.8f * g_obj.leashDist && !LeashTripRuns(lc))
        {
            events::Push("autopilot objective leash: %.0f m from %u (leash %.0f m) - back toward it", lc.distance,
                         lc.id, g_obj.leashDist);
            return LeashBack(lc);
        }
    }
    // the mission's formation order is forming (M09 22 Oct 2026: the hold ended before the leader was readable and our
    // travel broke the formation - Ozu stayed behind, the arch never counted): hands off until it has a leader, <= 10 s
    if (g_obj.missionFormSince && g.mode != Mode::Formation)
    {
        if (pilot.leader)
        {
            if (pilot.leader != g_obj.missionLeader)
                events::Push("autopilot objective mission-formation leader=%u (formed)", pilot.leader);
            g_obj.missionLeader = pilot.leader;
            g_obj.missionFormSince = 0;
            if (HostObjectiveNow(nullptr) == g_obj.seq)
                return ObjectiveDecide("mission formation"); // decide again now, with the leader
        }
        else if (pilot.op == 10 && now - g_obj.missionFormSince < 10000)
            return;
        else
            g_obj.missionFormSince = 0;
    }
    if (HostObjectiveNow(nullptr) != g_obj.seq)
    {
        // The same objective set again (M07 22 Oct 2026: "Fly to Planet Kyushu" re-issued every second by a repeatable
        // trigger - each one restarted the travel, the ship never left and the nag failed the mission): not a new step
        ObjectiveStep st = {};
        const u32 seq = HostObjectiveNow(&st);
        const u32 key = st.textId * 2654435761u ^ (st.refCount ? st.refs[0] : 0) ^
                        static_cast<u32>(st.hasNav ? st.nav[0] * 7 + st.nav[2] * 13 : 0);
        // ...unless the mission's marks changed with it: a step set and replaced by a placeholder in the same instant
        // (s12d M04: "Destroy the Rheinland Assault Wing" -> "Awaiting" at once, the wing marked) is a new step
        // (the host's marks alone - not our script marks nor range-dependent refs -, and only marks added count: a marked
        // ship dying is no new step; review, session 12)
        u32 mk[32];
        const int mn = std::max(0, HostMissionMarks(mk, 32));
        bool marksAdded = false;
        for (int i = 0; i < mn; ++i)
            if (std::find(g_obj.lastMarks.begin(), g_obj.lastMarks.end(), mk[i]) == g_obj.lastMarks.end())
                marksAdded = true;
        g_obj.lastMarks.assign(mk, mk + mn);
        if (key == g_obj.stepKey && g.mode != Mode::Off && !marksAdded)
        {
            g_obj.seq = seq;
            return;
        }
        g_obj.stepKey = key;
        // a mission formation belongs to the step it came with (M09: given right after the objective; M01b's earlier
        // formation with Epsilon held the ship 15 min at "Fly to the Waypoint", 22 Oct 2026)
        g_obj.missionLeader = 0;
        return ObjectiveDecide("objective");
    }
    // the lane the objective names has just opened (the mission's lane permission): take it now
    if (g_obj.laneRing)
    {
        u32 next = 0, prev = 0;
        if (space::LaneLinks(g_obj.laneRing, next, prev) && ((next && space::LaneOpen(g_obj.laneRing, next) == 1) ||
                                                             (prev && space::LaneOpen(g_obj.laneRing, prev) == 1)))
        {
            events::Push("autopilot objective lane=%u opened", g_obj.laneRing);
            g_obj.laneRing = 0;
            return ObjectiveDecide("lane opened");
        }
    }
    // a dock the mission keeps locked for now (M10 Holstein: "denied locked" until its own moment): ask again every 4 s
    if (g_obj.dockRetryAt && now >= g_obj.dockRetryAt)
    {
        g_obj.dockRetryAt = 0;
        return ObjectiveDecide("dock retry");
    }
    if (g.mode == Mode::Off)
    {
        // a script goal's trip the game cannot route: that goal is off for this step, decided again without it
        // ("no way": was strstr(lastStop, "no way"), which only the travel's "the game found no way (locked gate?)" -
        // 44 chars, whole in lastStop[48] - contains)
        if (g_obj.sub == Mode::Travel && g_obj.action == Action::ScriptGoal &&
            g_obj.lastStopReason == StopReason::NoWay)
        {
            g_obj.badGoal = g_obj.goalTrigger;
            g_obj.lastStop[0] = 0;
            g_obj.lastStopReason = StopReason::None;
            events::Push("autopilot objective script-goal %s: no way there - off for this step", g_obj.badGoal.c_str());
            return ObjectiveDecide("goal no way");
        }
        // (a dock trip the game cannot route is the same lock seen from the plot: M10 s11q-lf - Holstein "denied locked" at
        // the ring, the retry 4 s later found "no way" (the script unlocks it when Fischer's dx_m10_1370 comm completes),
        // the step counted as arrived and the ship rested 60 s to a STALL)
        // (at most 15 tries a step, ~75 s - Holstein unlocks ~25 s after the 5 km mark; a base out of reach for good then
        // ends as before; review, session 12)
        if (g_obj.sub == Mode::Travel && g_obj.lastDock && g_obj.subTarget == g_obj.lastDock &&
            g_obj.lastStopReason == StopReason::NoWay && ++g_obj.dockNoWayTries <= 15)
        {
            g_obj.dockRetryAt = now + 4000;
            g_obj.lastStop[0] = 0;
            g_obj.lastStopReason = StopReason::None;
            events::Push("autopilot objective dock %u: no way (locked?) - again in 4 s (resting meanwhile)",
                         g_obj.lastDock);
            return ObjectiveStart(Mode::Hold, 0, Action::DockWait, StartOrKeep(Mode::Hold, 0));
        }
        // ...and a gate, hole or ring the step docks with (no lastDock: transit, above): the same lock, and no "arrival"
        // to rest at - it re-decided the trip every ~1 s (M13 ak2-i4, 31 Oct 2026: "Dock with the Nomad Jump Gate" while
        // the script keeps it locked, 154 "found no way" until the leg's limit). Every 4 s for 15 tries, then every 10 s
        // until the script opens it. (A gate / hole is the "jump" exit's Action::Jump, a ring a Dock: fx1-g_i2 / i3, 31 Oct
        // 2026 - the first build keyed on Dock only, 120 tries again and two STALLs)
        if (g_obj.sub == Mode::Travel && (g_obj.action == Action::Dock || g_obj.action == Action::Jump) && g_obj.subTarget &&
            g_obj.subTarget != g_obj.lastDock && g_obj.lastStopReason == StopReason::NoWay)
        {
            const int tries = ++g_obj.dockNoWayTries;
            const ULONGLONG wait = tries <= 15 ? 4000 : 10000;
            g_obj.dockRetryAt = now + wait;
            g_obj.lastStop[0] = 0;
            g_obj.lastStopReason = StopReason::None;
            events::Push("autopilot objective dock %u: no way (locked gate?) - again in %u s (try %d, resting meanwhile)",
                         g_obj.subTarget, static_cast<u32>(wait / 1000), tries);
            return ObjectiveStart(Mode::Hold, 0, Action::DockWait, StartOrKeep(Mode::Hold, 0));
        }
        if (g_obj.sub == Mode::Travel &&
            g_obj.lastStopReason == StopReason::DockRefused) // (only "travel: dock refused")
        {
            g_obj.dockRetryAt = now + 4000;
            g_obj.lastStop[0] = 0;
            g_obj.lastStopReason = StopReason::None;
            events::Push("autopilot objective dock refused: again in 4 s (resting meanwhile)");
            return ObjectiveStart(Mode::Hold, 0, Action::DockWait, StartOrKeep(Mode::Hold, 0));
        }
        // (a loot destination still there is not "arrived": the travel goes on tractoring it - M12's power cell)
        const space::Contact sc = g_obj.sub == Mode::Travel ? FindContact(g_obj.subTarget) : space::Contact{};
        if (g_obj.inVia) // the via point reached: on to the marker
        {
            g_obj.inVia = false;
            return ObjectiveDecide("via done");
        }
        // A trip to a script condition's sphere that ended outside it is not there (session 7, 23 Oct 2026, M12: generator 3
        // -> the airlock's top, "arrived-close" 1 km short against the lair's structure, then the threat check fought
        // Nomads - the user: "once again engaging nomads"): at once again, from where the ship is now (its way re-planned),
        // at most 4 times a step
        if (g_obj.sub == Mode::Travel && g_obj.flyGoalR > 0 && g_obj.arriveRetries < 4)
        {
            space::Pilot fp;
            if (space::ReadPilot(fp))
            {
                const float d[3] = {g_obj.flyGoal[0] - fp.pos[0], g_obj.flyGoal[1] - fp.pos[1],
                                    g_obj.flyGoal[2] - fp.pos[2]};
                if (Len3(d) > g_obj.flyGoalR)
                {
                    ++g_obj.arriveRetries;
                    g_obj.viaNext = true;
                    events::Push("autopilot objective trip ended %.0f m outside the condition's %.0f m: again (%d)",
                                 Len3(d) - g_obj.flyGoalR, g_obj.flyGoalR, g_obj.arriveRetries);
                    return ObjectiveDecide("short of the condition");
                }
            }
        }
        // (a leash trip back toward its ship is no arrival of the step's trip: M10 gate_s8v - its end set "arrived", the
        // step's "Fly to the Waypoint" no longer outranked the chasing fleet, the ship fought it 78 s at 8-9 km from the
        // Hamburg hole and Von Claussen's nag failed the mission)
        if (g_obj.sub == Mode::Travel && !(sc.id && (sc.type & space::kLoot)) && g_obj.action != Action::Leash)
            g_obj.arrived = true, g_obj.arrivedAt = now;
        if (g_obj.sub == Mode::Formation && now - g_obj.subSince < 10000) // the game refused the formation
        {
            g_obj.sub = Mode::Follow;
            g_obj.formRefused = true;
            g_obj.formRetryAt = now + 3000;
            return ObjectiveStart(Mode::Follow, g_obj.subTarget, Action::Follow,
                                  Start(Mode::Follow, g_obj.subTarget, nullptr));
        }
        return ObjectiveDecide("ended");
    }
    // ...and asked again once close (M13 22 Oct 2026: refused at 1.5 km, the follow kept that distance, and Sinclair
    // starts guiding only with the player within 300 m - the nag failed the mission): within 800 m, every 5 s
    if (g_obj.formRefused && g_obj.sub == Mode::Follow && g.mode == Mode::Follow && now >= g_obj.formRetryAt)
    {
        g_obj.formRetryAt = now + 5000;
        const space::Contact lc = FindContact(g_obj.subTarget);
        if (lc.id && lc.distance < 800)
        {
            g_obj.sub = Mode::Formation;
            g_obj.formRefused = false;
            return ObjectiveStart(Mode::Formation, lc.id, Action::Formation, Start(Mode::Formation, lc.id, nullptr));
        }
    }
    if (g_obj.sub == Mode::Combat &&
        g.mode == Mode::Hold) // the fight is over (combat rests by itself): back to the goal
        return ObjectiveDecide("fight over");
    // Following a mission lead that has stopped, with no objective (M02 22 Oct 2026: King waited 3.25 km from the
    // Colorado gate, the script's next step fires at 3.2 km of it, our follow sat 150 m behind him for 25 min): both at
    // rest for 15 s -> up to 2 km from the jump gate / hole nearest the lead (within 6 km) - his stop is the hint.
    if (g.mode == Mode::Follow || g.mode == Mode::Formation)
    {
        const space::Contact lc = FindContact(g.target ? g.target : g_obj.subTarget);
        space::ShipMotion lm, me;
        const bool still = lc.id && space::ShipMotionOf(lc.id, lm) && space::ShipMotionOf(space::PlayerShipId(), me) &&
                           lm.speed < 5 && me.speed < 5;
        if (!still)
            g_obj.leadStillSince = 0;
        else if (!g_obj.leadStillSince)
            g_obj.leadStillSince = now;
        else if (now - g_obj.leadStillSince > 15000)
        {
            g_obj.leadStillSince = 0;
            const space::Contact* gate = nullptr;
            for (const space::Contact& c : Contacts())
            {
                if (c.ship || !(c.type & (space::kJumpGate | space::kJumpHole)))
                    continue;
                const float d[3] = {c.pos[0] - lc.pos[0], c.pos[1] - lc.pos[1], c.pos[2] - lc.pos[2]};
                if (Len3(d) < 6000 && (!gate || c.distance < gate->distance))
                    gate = &c;
            }
            if (gate && gate->distance > 2200)
            {
                float p[3];
                const float f = 2000.0f / gate->distance;
                space::Pilot me2;
                if (space::ReadPilot(me2))
                {
                    for (int i = 0; i < 3; ++i)
                        p[i] = gate->pos[i] + (me2.pos[i] - gate->pos[i]) * f;
                    events::Push("autopilot objective lead %u at rest: up to the gate %u (%.0f m)", lc.id, gate->id,
                                 gate->distance);
                    return ObjectiveStart(Mode::Travel, 0, Action::LeadStall, StartTravel(0, p, 0, true));
                }
            }
        }
    }
    // Resting "arrived" while the step's navmarker in this system is still more than 600 m off (M12 22 Oct 2026: the lair
    // waypoint, 1.7 km short; the script wants 500 m - 5.5 min of rest, then the jump hole's timer failed it): travel
    // again after 10 s, at most 4 times per step.
    if (g_obj.arrived && g.mode == Mode::Hold && g_obj.arriveRetries < 4 && now - g_obj.arrivedAt > 10000)
    {
        ObjectiveStep st = {};
        HostObjectiveNow(&st);
        space::Pilot p;
        if (st.hasNav && space::ReadPilot(p) && (!st.navSystem || st.navSystem == p.system))
        {
            const float d[3] = {st.nav[0] - p.pos[0], st.nav[1] - p.pos[1], st.nav[2] - p.pos[2]};
            if (Len3(d) > 600)
            {
                ++g_obj.arriveRetries;
                g_obj.viaNext = true;
                g_obj.arrived = false;
                g_obj.arrivedAt = now;
                events::Push("autopilot objective navmarker still %.0f m off: again (%d)", Len3(d),
                             g_obj.arriveRetries);
                return ObjectiveDecide("not there yet");
            }
        }
        g_obj.arrivedAt = now; // (looked: there - or no marker here)
    }
    // survival resting in place because no lead was there yet (M04 Mactan: Juni_mactan spawns 7 s after the launch) -
    // the moment one is, stay with it instead (checked at most once a second)
    static ULONGLONG leadLook = 0;
    if (g_obj.survival && g.mode == Mode::Escort && g.tactic == Tactic::Rejoin && g_obj.action == Action::SurviveStay &&
        now - leadLook >= 1000)
    {
        leadLook = now;
        const space::Contact lc = FindContact(g.escorted);
        if (lc.id && lc.distance < 0.6f * g_set.followDistance)
            return ObjectiveDecide("lead reached");
    }
    // Resting under fire (M04 20 Oct 2026: "survive-hold" 1.5 km beside Juni, 600 m from the hostile cruiser LNS Rampart
    // whose turrets fired on us while the cruiser itself targeted another ship - so no "attacker" for the threat check;
    // shield 0.92 -> 0.20, hull 1.00 -> 0.03 in 6 s, dead): being hit is the ground truth. Out of the fire, once a second
    // while the hits go on: from the toughest hostile within 2.5 km (capital ships first; else the nearest), to the far
    // side of the lead (she anchors the tethers), at least 1 km farther from it than now; no lead: 1 km straight away.
    static ULONGLONG fireLook = 0;
    if (g_obj.survival && g.mode == Mode::Hold && g.stats.pressure > 0 && now - fireLook >= 1000)
    {
        fireLook = now;
        fl::Kinematics k = {};
        const std::vector<space::Contact> all = Contacts();
        const space::Contact* src = nullptr;
        for (const space::Contact& c : all)
            if (c.ship && c.health != 0 && HostileContact(c) && c.distance < 2500 &&
                (!src || c.maxHit > src->maxHit * 1.5f ||
                 (c.maxHit > src->maxHit / 1.5f && c.distance < src->distance)))
                src = &c;
        // Fighters are fought, not fled (user, 21 Oct 2026, M04 Mactan: "the ship took forever to start engaging" - the
        // Rheinland wing's fighters, 5800 hull, fired from ~2 km and this rule held the ship out of the fight 35-60 s):
        // moving out is for what our guns cannot answer - a gunboat, cruiser or capital ship's turrets.
        const bool srcBig = src && ((src->type & (kGunboatBit | kCruiserBit | kCapitalBit)) || src->maxHit > 20000);
        if (src && !srcBig)
        {
            events::Push("autopilot objective under-fire from=%u hp=%.0f dist=%.0f: a fighter - engaging", src->id,
                         src->maxHit, src->distance);
            if (const u32 lead = SurvivalLead(all))
            {
                ObjectiveStart(Mode::Escort, lead, Action::Survive, StartOrKeep(Mode::Escort, lead));
                return;
            }
            ObjectiveStart(Mode::Combat, 0, Action::Survive, StartOrKeep(Mode::Combat, 0));
            return;
        }
        if (src && fl::PlayerKinematics(k))
        {
            float away[3] = {k.pos[0] - src->pos[0], k.pos[1] - src->pos[1], k.pos[2] - src->pos[2]};
            const float d = std::max(1.0f, Len3(away));
            for (float& a : away)
                a /= d;
            float to[3] = {k.pos[0] + away[0] * 1000, k.pos[1] + away[1] * 1000, k.pos[2] + away[2] * 1000};
            const space::Contact lc = FindContact(SurvivalLead(all));
            if (lc.id)
            {
                float side[3] = {lc.pos[0] - src->pos[0], lc.pos[1] - src->pos[1], lc.pos[2] - src->pos[2]};
                const float ls = std::max(1.0f, Len3(side));
                const float reach = std::max(d + 1000, ls + 500); // past her, and farther than now
                for (int i = 0; i < 3; ++i)
                    to[i] = src->pos[i] + side[i] / ls * reach;
            }
            const float out3[3] = {to[0] - src->pos[0], to[1] - src->pos[1], to[2] - src->pos[2]};
            events::Push("autopilot objective under-fire from=%u hp=%.0f dist=%.0f: out to %.0f m%s", src->id,
                         src->maxHit, d, Len3(out3), lc.id ? " (past the lead)" : "");
            ObjectiveStart(Mode::Hold, 0, g_obj.action, Start(Mode::Hold, 0, to)); // (the action kept)
            return;
        }
    }
    if (g_obj.survival && g.mode == Mode::Hold && now - leadLook >= 1000)
    {
        leadLook = now;
        if (g_obj.action == Action::Rest && SurvivalLead(Contacts()))
            return ObjectiveDecide("lead");
        // (the reverse - escorting her with nothing to fight, caught up: rest again - is below, in the escort branch)
        // resting beside the lead: she moved off (past the follow distance + 500 m) - go with her
        if (g_obj.action == Action::SurviveHold)
        {
            const u32 lead = SurvivalLead(Contacts());
            const space::Contact lc = FindContact(lead);
            if (!lc.id || lc.distance > g_set.followDistance + 500)
                return ObjectiveDecide("lead moved");
        }
    }
    // (not during a lane approach, a run to a gate or a travel objective's trip: the way out / the story's destination
    // outranks the fight - see ObjectiveDecide)
    // A fight the threat check started (not the step's own): looked at again every 3 s - the script's destroy condition
    // may have live objects by now (M13 23 Oct 2026: the sphere's final generators spawn after the first decision; the
    // ship fought the entry's fighters through the 27 s dialogue). The re-decision keeps a running fight (StartOrKeep).
    static ULONGLONG threatLook = 0;
    if (g.mode == Mode::Combat && g_obj.action == Action::Fight && !g_obj.fightObjective && !g_obj.survival &&
        Knows(Source::Hidden) && g_set.scriptGoals && now - threatLook >= 3000)
    {
        threatLook = now;
        ObjectiveStep cur = {};
        HostObjectiveNow(&cur);
        for (const mission::Goal& sg :
             mission::AdvanceGoals(cur.textId, fl::PlayerSystem(), nullptr, HostObjectiveTitle()))
            if (sg.kind == 3)
            {
                float p[3];
                for (u32 id : sg.objects)
                    if (space::ObjectPos(id, p))
                        return ObjectiveDecide(
                            "recheck"); // (only when the step has a destroy condition with an object up)
            }
    }
    // A fight step whose destroy condition has just been met (all its objects gone) is decided again once - the script
    // talks before the next objective and the pre-stage flies on (M12: generator 3 down, the ship fought a fighter 6.5 s)
    static ULONGLONG doneLook = 0;
    static std::string doneTrigger;
    if (g.mode == Mode::Combat && Knows(Source::Hidden) && g_set.scriptGoals && now - doneLook >= 500)
    {
        doneLook = now;
        ObjectiveStep cur = {};
        HostObjectiveNow(&cur);
        const std::vector<space::Contact> all = Contacts();
        const std::vector<mission::Goal> goals =
            mission::AdvanceGoals(cur.textId, fl::PlayerSystem(), nullptr, HostObjectiveTitle());
        for (const mission::Goal& sg : goals)
            if (sg.kind == 3 && sg.trigger != doneTrigger && sg.leads != "SUCCEED" && DestroyFired(sg, all))
            {
                doneTrigger = sg.trigger;
                events::Push("autopilot objective %s met (all of %s gone): deciding again", sg.trigger.c_str(),
                             sg.objectName.c_str());
                return ObjectiveDecide("condition-met");
            }
        // Where the ship goes after the kill (StaticHold holds on that side: the user's try 3 held generator 3 where the
        // climb to the airlock's top was straight - ours sat ~130 m deeper in the trench, looped there ~6 s and needed
        // 22-28 s to the core against ~16): the destroy goal's other live objects, and the next step's place
        g_obj.leaveN = 0;
        for (const mission::Goal& sg : goals)
        {
            if (sg.kind != 3 || sg.leads == "SUCCEED")
                continue;
            for (u32 id : sg.objects)
                if (g_obj.leaveN < 8 && space::ObjectPos(id, &g_obj.leave[g_obj.leaveN * 3]))
                    ++g_obj.leaveN;
            for (const mission::Goal& ng : mission::GoalsAfter(sg.leads, sg.file, fl::PlayerSystem()))
                if (g_obj.leaveN < 9 && (ng.kind == 1 || ng.kind == 2) && ng.subject == "player" &&
                    (ng.kind == 2 ? (memcpy(&g_obj.leave[g_obj.leaveN * 3], ng.pos, 3 * sizeof(float)), true)
                                  : space::ObjectPos(ng.object, &g_obj.leave[g_obj.leaveN * 3])))
                {
                    ++g_obj.leaveN;
                    break;
                }
        }
        g_obj.leaveAt = now;
    }
    // TIMER-DESTROY: the verdict looked at again every second - a ship now shooting at us at the place, a default-preset
    // fight that is no longer safe (a defence ends by itself: its combat stops when nothing shoots at us)
    static ULONGLONG tdLook = 0;
    if (g_set.timerDestroy && g_obj.tdVerdict != static_cast<int>(lookahead::TdVerdict::Today) &&
        g_obj.tdVerdict != static_cast<int>(lookahead::TdVerdict::Defend) && now - tdLook >= 1000)
    {
        tdLook = now;
        ObjectiveStep cur = {};
        if (HostObjectiveNow(&cur))
        {
            const lookahead::TdResult tr =
                lookahead::TimerDestroy(TimerDestroyFactsNow(cur, ObjectiveVerb(cur.textId), Contacts()));
            if (static_cast<int>(tr.verdict) != g_obj.tdVerdict)
                return ObjectiveDecide("timer-destroy");
        }
    }
    // ALLY-KILL: the verdict looked at again every second - the stage trip at the dock, a ship now shooting at us, the
    // fallback's clock; and while it reads Today, the step's rule coming to hold (a decision the coast guard swallowed:
    // DecideBody returns under CoastActive, uncounted) - never under a defence (it ends by itself) or the kept fallback
    // (from Today: asked once per step - an earlier exit of the decision keeps it Today, and an ask every second would
    // trip the decide-loop guard)
    // kill_ahead: the script's leave looked at again every second while the ship waits at the target
    static ULONGLONG kaLook = 0;
    if (g_set.killAhead && g_obj.killAheadWait && now - kaLook >= 1000)
    {
        kaLook = now;
        if (mission::KillAheadSafe(mission::LiveTriggers(), g_obj.killAheadWait, g_obj.killAheadNo).ok)
            return ObjectiveDecide("kill-ahead");
    }
    // ... and in a fight step: the next step's target spawning (M13's nomadcap, 36.9 s in) while the fight goes on - the
    // decision is asked only once the kill is allowed (a refusal asked every second would trip the decide-loop guard)
    static ULONGLONG kaFight = 0;
    // (Mode::Escort too: a "defend" step fights from the escort round its object - M13 24414, set ab3)
    if (g_set.killAhead && Knows(Source::Hidden) && (g.mode == Mode::Combat || g.mode == Mode::Escort) &&
        !g_obj.scriptMarkN && now - kaFight >= 1000)
    {
        kaFight = now;
        ObjectiveStep cur = {};
        // (a "defend" step with no object to escort too, as at the decision)
        const bool got = HostObjectiveNow(&cur) && cur.textId;
        const ObjVerb cv = got ? ObjectiveVerb(cur.textId) : ObjVerb::None;
        const bool defend = cv == ObjVerb::Escort && !cur.refCount;
        if (got && (cv == ObjVerb::Fight || defend))
        {
            const mission::LiveTable lt = mission::LiveTriggers();
            for (const mission::Goal& ng : FightAheadGoals(cur.textId, lt, defend))
                for (u32 id : ng.objects)
                {
                    // (never the target fought already: after 24425 the armed cap_dies names nomadcap itself - a
                    // decide every second, set ab3)
                    const space::Contact c = FindContact(id);
                    if (c.id && c.health != 0 && id != g.target && mission::KillAheadSafe(lt, id, 1).ok)
                        return ObjectiveDecide("kill-ahead");
                }
        }
    }
    // chase_kill: the candidate looked for every second while it is not fought - his contact or the script's hostility
    // can lag the step by a moment (the decision then took the navmarker trip); asked once per ship and step
    static ULONGLONG chLook = 0;
    // (spawn_charge: every 200 ms while the staged fleeing ship is expected and not chased yet - the press belongs at his
    // spawn, each second of lag costs ~300 m of the lock gap: lookahead::ChargeGap)
    const ULONGLONG chEvery = g_set.spawnCharge && !g_obj.chargeShip.empty() && !g_obj.chargeId ? 200 : 1000;
    if (g_set.chaseKill && Knows(Source::Hidden) && now - chLook >= chEvery &&
        !(g_obj.chaseShip && g.mode == Mode::Combat && g.target == g_obj.chaseShip))
    {
        chLook = now;
        ObjectiveStep cur = {};
        if (HostObjectiveNow(&cur) == g_obj.seq)
        {
            lookahead::LookFacts cf;
            lookahead::LookResult cr;
            const u32 cs = ChaseCandidate(cur, ObjectiveVerb(cur.textId), Contacts(), cf, cr);
            if (cs && cs != g_obj.chaseAsked)
            {
                g_obj.chaseAsked = cs;
                return ObjectiveDecide("chase");
            }
        }
    }
    // spawn_stage: in a fight step's combat, the moment its last kill is pending (asked once per trigger and step); while
    // it stages, its verdict every second (the trip arrived, the last ship within reach, the timeout)
    static ULONGLONG ssLook = 0;
    if (g_set.spawnStage && g_set.chaseKill && Knows(Source::Hidden) && now - ssLook >= 1000 &&
        (g_obj.stageVerdict || g.mode == Mode::Combat))
    {
        ssLook = now;
        ObjectiveStep cur = {};
        if (HostObjectiveNow(&cur) == g_obj.seq && (g_obj.stageVerdict || ObjectiveVerb(cur.textId) == ObjVerb::Fight))
        {
            const StagePlan sp = SpawnStageNow(cur, Contacts());
            const int v = StageVerdictNow(sp);
            if (g_obj.stageVerdict ? v != g_obj.stageVerdict : (v && sp.trigger != g_obj.stageAsked))
            {
                if (!g_obj.stageVerdict)
                    g_obj.stageAsked = sp.trigger;
                return ObjectiveDecide("spawn-stage");
            }
        }
    }
    // (nor the airlock run: M13 23 Oct 2026, the user: "did not move with intent into the airlock" - each 0.5 s a Nomad
    // nearby restarted the run, 15 restarts and a decide-loop pause on the way in)
    if (g.mode != Mode::Combat && g.mode != Mode::Escort && g_obj.action != Action::Lane &&
        g_obj.action != Action::Jump && g_obj.action != Action::Airlock && g_obj.action != Action::PreStage)
    {
        const std::vector<space::Contact> all = Contacts();
        // (pre_aggro interrupts a travel objective too - above, ObjectiveDecide)
        // (every 3 s at most: a dock / lane step declines it in the decision)
        static ULONGLONG preLook = 0;
        if (PreHostileNear(all) && g_obj.action != Action::Dock && now - preLook >= 3000)
        {
            preLook = now;
            return ObjectiveDecide("pre-aggro");
        }
        if (!(g_obj.travelObjective && g.mode == Mode::Travel) && ObjectiveThreat(all))
            return ObjectiveDecide("threat");
    }
}

} // namespace autopilot
