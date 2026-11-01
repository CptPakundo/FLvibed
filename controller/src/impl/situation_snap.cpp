// The situation snapshot's adapter (situation.h SnapshotNow, A37_PLAN S1): the only impure part of the assessor. It
// reads what the objective mode reads - the host's objective, marks, title, spawns and dock reply, the contacts (with
// the pre-aggro overlay), the pilot, mission.cpp's derived goals (AdvanceGoalsPeek: its memory read, never written),
// the live trigger table, the job table - and writes nothing: no autopilot state, no mission.cpp memory, no game call
// with an effect (GameDocksUs' grant bookkeeping is read through SinceGameGrant only). Debug `ap situation [snap|hash|last]`.
// A37 S3: the executor memory the arbiter's rules read (QueueNow) and the shadow (situation_shadow): the rules beside
// ObjectiveDecide at the exits they cover, the verdict as one event. S4: the facts and memory of the pre-stage, mark-fight,
// pre-aggro, survival and escort rules (snap v4). S5: the lane / pass / route / arrive / navmarker / rest rules' (snap v5);
// a shadowed decision's waypoints are the list as it began. S7: the arbiter (situation_arbiter; situation_exec.h) - its
// decision at an exit of the body in plan mode, the goal queue across the player's pause, the resume, a load forgetting it.
#include "autopilot_impl.h"
#include "game/jobs.h"
#include "game/mission_live.h"
#include "situation.h"
#include "situation_exec.h"
#include "situation_io.h"
#include "situation_rules.h"
#include "str.h"

namespace situation
{
constexpr size_t kNearest = 48; // contacts kept by distance (+ every one the rules name)
constexpr int kSpawnsKept = 64;

// A script path relative to DATA\MISSIONS ("M07\m07.ini"): no install or profile path in a snapshot.
static std::string Relative(const std::string& path)
{
    const std::string low = str::Lower(path);
    const size_t at = low.rfind("\\missions\\");
    if (at != std::string::npos)
        return path.substr(at + 10);
    const size_t slash = path.find_last_of("\\/");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

static void FillStep(SnapStep& d, const ObjectiveStep& st, const std::vector<space::Contact>& all)
{
    d.textId = st.textId;
    d.refCount = std::max(0, std::min(st.refCount, 4));
    for (int i = 0; i < d.refCount; ++i)
    {
        d.refs[i] = st.refs[i];
        d.refObj[i] = space::ObjectiveObject(st.refs[i], all);
    }
    d.hasNav = st.hasNav;
    memcpy(d.nav, st.nav, sizeof(d.nav));
    d.navSystem = st.navSystem;
    d.verb = st.textId ? VerbOf(events::IdsText(st.textId)) : Verb::None;
    d.job = jobs::KindOf(jobs::Install(), st.textId, &d.jobRows);
    // (v5) the first resolved ref's position (the route-lane rule: ObjectivePos of the route's step 1)
    for (int i = 0; i < d.refCount; ++i)
        if (d.refObj[i])
        {
            d.refPlaced = space::ObjectPos(d.refObj[i], d.refPos);
            break;
        }
    if (!d.refPlaced)
        memset(d.refPos, 0, sizeof(d.refPos));
}

static SnapGoal GoalOf(const mission::Goal& g)
{
    SnapGoal o;
    o.kind = g.kind, o.object = g.object, o.objectName = g.objectName;
    memcpy(o.pos, g.pos, sizeof(o.pos));
    o.dist = g.dist, o.count = g.count, o.subject = g.subject, o.trigger = g.trigger, o.leads = g.leads;
    o.file = Relative(g.file), o.byEntry = g.byEntry;
    if (g.kind == 1)
        o.placed = space::ObjectPos(g.object, o.objPos);
    for (u32 id : g.objects)
    {
        SnapGoalObj x;
        x.id = id;
        x.placed = space::ObjectPos(id, x.pos);
        if (!x.placed)
            memset(x.pos, 0, sizeof(x.pos));
        o.objects.push_back(x);
    }
    return o;
}

// A leash with the objective mode's reads of its anchor (ObjectiveDecide's script-goal block: the comm that disarms it,
// the ship its pilot's name names, the object's position)
static SnapLeash LeashOf(const mission::Leash& l, const std::vector<space::Contact>& all)
{
    SnapLeash o{l.object, l.name, l.dist, l.nameIds, l.offComms};
    for (const u32 c : l.offComms)
    {
        const unsigned long long at = HostCommHeardAt(c);
        o.off = o.off || (at && GetTickCount64() - at < 30ull * 60 * 1000);
    }
    const auto self = std::find_if(all.begin(), all.end(), [&](const space::Contact& c) { return c.id == l.object; });
    if (!(self != all.end() && self->ship) && l.nameIds)
    {
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
                o.ship = c.id;
        }
    }
    o.placed = space::ObjectPos(l.object, o.opos);
    if (!o.placed)
        memset(o.opos, 0, sizeof(o.opos));
    return o;
}

static SnapNag NagOf(const mission::TowardsNag& n, bool has)
{
    SnapNag o;
    o.has = has && (n.pos || !n.name.empty());
    if (!o.has)
        return o;
    o.pos = n.pos, o.object = n.object, o.name = n.name, o.dist = n.dist;
    memcpy(o.p, n.p, sizeof(o.p));
    return o;
}

static SnapContact ContactOf(const space::Contact& c)
{
    SnapContact o;
    o.id = c.id, o.type = c.type, o.nameIds = c.nameIds, o.base = c.base;
    o.ship = c.ship, o.dock = c.dock, o.targetable = c.targetable;
    memcpy(o.pos, c.pos, sizeof(o.pos));
    o.distance = c.distance, o.health = c.health, o.attitude = c.attitude, o.attitudeAsked = c.attitudeAsked;
    if (!c.ship)
        o.attKnown = space::AttitudeToPlayer(c.id, o.att);
    o.targetId = c.targetId, o.targetsPlayer = c.targetsPlayer, o.maxHit = c.maxHit, o.rep = c.rep;
    o.dockPort = c.dockPort, o.jumping = c.jumping, o.lane = c.lane, o.preHostile = c.preHostile, o.player = c.player;
    return o;
}

Queue QueueNow()
{
    using namespace autopilot;
    Queue q;
    q.seq = g_obj.seq;
    q.mode = ModeName(g.mode), q.target = g.target;
    q.sub = ModeName(g_obj.sub), q.subTarget = g_obj.subTarget, q.action = ActionName(g_obj.action);
    q.trHasDest = g.tr.hasDest, q.trDestObject = g.tr.destObject;
    memcpy(q.trDest, g.tr.dest, sizeof(q.trDest));
    q.lockId = g.tr.lockId, q.lockHave = g.tr.lockHave;
    memcpy(q.lockP2, g.tr.lockP2, sizeof(q.lockP2));
    q.cruiseLock = g_obj.cruiseLock;
    q.leashShip = g_obj.leashShip, q.leashDist = g_obj.leashDist;
    q.lastDock = g_obj.lastDock, q.escortShip = g_obj.escortShip;
    q.badGoal = g_obj.badGoal;
    q.deadGoals.assign(g_obj.deadGoals.begin(), g_obj.deadGoals.end()); // (a std::set: sorted)
    for (int i = 0; i < g_obj.scriptMarkN && i < 8; ++i)
        q.scriptMarks.push_back(g_obj.scriptMarks[i]);
    SeenDestroyed(q.seenAlive, q.seenListed);
    // (v4) the memory the S4 rules read
    q.lastReal = ObjectiveLastReal();
    q.arrived = g_obj.arrived;
    q.stepAgeMs = g_obj.stepAt ? static_cast<double>(gameclock::NowMs64() - g_obj.stepAt) : -1.0;
    q.escorted = g.escorted;
    q.survTitle = g_survivalTitle, q.survMission = g_survivalMission, q.survEnded = g_survivalEnded;
    q.survSystem = g_survivalSystem, q.survival = g_obj.survival;
    // (v5) the memory the S5 rules read
    q.missionLeader = g_obj.missionLeader, q.viaNext = g_obj.viaNext, q.arriveRetries = g_obj.arriveRetries;
    q.trPassRing = g.tr.passRing;
    return q;
}

// The escort rule's "the objective names the ship" (ObjectiveDecide's `named`): a 4+ letter word of the ship's speaker
// name is a whole word of the step's (lower-cased) text.
static bool NamedIn(const std::string& text, u32 id)
{
    const std::wstring w = space::SpeakerName(id) + L" ";
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
}

Snapshot SnapshotNow(const Queue* queue)
{
    using namespace autopilot;
    Snapshot s;
    s.queue = queue ? *queue : QueueNow();
    s.decided.mode = ModeName(g_obj.sub), s.decided.target = g_obj.subTarget, s.decided.action = ActionName(g_obj.action);
    const std::vector<space::Contact> all = Contacts();
    // ---- the objective
    ObjectiveStep st = {};
    s.objSeq = HostObjectiveNow(&st);
    FillStep(s.step, st, all);
    ObjectiveStep steps[8];
    const int ns = HostObjectiveSteps(steps, 8);
    for (int i = 0; i < ns; ++i)
    {
        s.entries.emplace_back();
        FillStep(s.entries.back(), steps[i], all);
    }
    s.title = HostObjectiveTitle();
    u32 mk[32];
    const int mn = HostMissionMarks(mk, 32);
    if (mn > 0)
        s.marks.assign(mk, mk + mn);
    u32 refs[16];
    const int nr = HostObjectiveRefs(refs, 16);
    for (int i = 0; i < nr; ++i)
        if (const u32 id = space::ObjectiveObject(refs[i], all))
            s.refsObj.push_back(id);
    // ---- the place, the pilot, the ship
    space::Pilot pilot;
    s.pilotOk = space::ReadPilot(pilot);
    s.inSpace = s.pilotOk;
    s.system = fl::PlayerSystem();
    s.base = fl::PlayerBase();
    s.clock = gameclock::NowSec();
    s.hasEntry = SystemEntry(s.entry);
    if (!s.hasEntry)
        memset(s.entry, 0, sizeof(s.entry));
    if (s.pilotOk)
    {
        s.op = pilot.op, s.locked = pilot.locked, s.laneRide = pilot.lane, s.jumping = pilot.jumping;
        s.cruise = pilot.cruise, s.laneEntry = pilot.laneEntry, s.laneExit = pilot.laneExit, s.leader = pilot.leader;
        s.target = pilot.target, s.pilotSystem = pilot.system;
        memcpy(s.pos, pilot.pos, sizeof(s.pos));
        fl::Kinematics k = {};
        if (fl::PlayerKinematics(k))
            memcpy(s.vel, k.vel, sizeof(s.vel));
        ai::Self me;
        if (ai::ReadSelf(me))
            s.hull = me.hull, s.shield = me.shield, s.tractorRange = me.tractorRange;
    }
    // ---- the story script (read only): the ini view always, the live view beside it when the live trigger table
    // answers with a script (else the ini view is the source: multiplayer, another build, no story mission). The ini view
    // first: it reads the scripts once per process (~36 ms, A37 S2 boot), which the live table's naming needs too - its
    // timing then measures the table, as S1 did.
    mission::AdvanceInfo info;
    std::vector<SnapGoal> iniGoals;
    std::set<std::pair<std::string, std::string>> afterRead;
    for (const mission::Goal& goal : mission::AdvanceGoalsPeek(st.textId, s.system, s.title, info))
    {
        iniGoals.push_back(GoalOf(goal));
        // (v3) the goals its trigger arms - the objective mode's fly-through and fired-condition trips
        if (goal.kind != 3)
            for (const mission::Goal& ng : mission::GoalsArmedBy(goal.trigger, goal.file, s.system))
                s.armed.push_back({goal.trigger, Relative(goal.file), GoalOf(ng)});
        // (v4) the goals of the step it leads to - the pre-stage's (once per leads + script)
        if (afterRead.insert({goal.leads, goal.file}).second)
            for (const mission::Goal& ng : mission::GoalsAfter(goal.leads, goal.file, s.system))
                s.after.push_back({goal.leads, Relative(goal.file), GoalOf(ng)});
    }
    // (v4) the objective mode's last real objective after this decision's update, and what the scripts declare after it
    // (pre-stage-next; survival_stage's place skips up to two objectives without one)
    {
        SnapPrestage& p = s.prestage;
        p.lastReal = st.textId && st.textId != 21660 && st.textId != 1 ? st.textId : s.queue.lastReal;
        p.stageFrom = p.lastReal; // (survival_stage: the step's real text, else lastReal - the same expression)
        if (p.lastReal)
        {
            for (const mission::Goal& ng : mission::NextStepGoals(p.lastReal, s.system, &p.next))
                s.realNext.push_back(GoalOf(ng));
            p.nextOk = mission::NextObjectivePlace(p.lastReal, s.system, p.nextPos, &p.nextText);
            p.stageOk = mission::NextObjectivePlace(p.stageFrom, s.system, p.stagePos, &p.stageText, 3);
        }
        if (!p.nextOk)
            memset(p.nextPos, 0, sizeof(p.nextPos)), p.nextText = 0;
        if (!p.stageOk)
            memset(p.stagePos, 0, sizeof(p.stagePos)), p.stageText = 0;
    }
    // (v4) the title's escape words (ObjectiveDecide's survival title, beside its string id 22602)
    if (s.title)
    {
        const std::string t = str::Lower(events::IdsText(s.title));
        s.titleEscape = t.find("escape") != std::string::npos || t.find("flee") != std::string::npos ||
                        t.find("survive") != std::string::npos;
    }
    s.goalsFromLast = info.fromLast, s.goalsByTitle = info.byTitle, s.standInText = info.standInText;
    s.goalsScript = info.script.empty() ? std::string() : Relative(info.script);
    for (const mission::ObjectiveDef& d : mission::ObjectiveDefs(st.textId))
        s.objDefs.push_back({d.nick, d.type, Relative(d.script)});
    std::vector<SnapLeash> iniLeashes;
    for (const mission::Leash& l : mission::Leashes(s.system))
        iniLeashes.push_back(LeashOf(l, all));
    // the live trigger table (A39), timed
    LARGE_INTEGER f, t0, t1;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&t0);
    const mission::LiveTable lt = mission::LiveTriggers();
    QueryPerformanceCounter(&t1);
    s.liveMs = static_cast<float>(static_cast<double>(t1.QuadPart - t0.QuadPart) * 1000.0 / static_cast<double>(f.QuadPart));
    s.liveOk = lt.ok, s.liveReason = lt.reason, s.liveScript = lt.script.empty() ? std::string() : Relative(lt.script);
    s.liveController = lt.controller;
    const std::vector<mission::LiveLead> leads = mission::LeadsOfLive(lt, st.textId);
    for (size_t i = 0; i < lt.triggers.size(); ++i)
    {
        const mission::LiveTrigger& t = lt.triggers[i];
        s.live.push_back({t.id, t.state, t.cond ? t.cond : "", t.nickname, t.iniCond, leads[i].leads, leads[i].hops});
    }
    s.goalsLive = lt.ok && !lt.script.empty();
    if (s.goalsLive)
    {
        for (const mission::Goal& goal : mission::GoalsOfLive(lt, s.system, st.textId))
            s.goals.push_back(GoalOf(goal));
        for (const mission::Leash& l : mission::ArmedLeashes(lt, s.system))
            s.leashes.push_back(LeashOf(l, all));
        s.iniGoals = std::move(iniGoals);
        s.iniLeashes = std::move(iniLeashes);
    }
    else
    {
        s.goals = std::move(iniGoals);
        s.leashes = std::move(iniLeashes);
    }
    // the step's towards nag - only for a text one script's real objectives name (TowardsNagOf takes the first script
    // that knows the text: for 21660, shared by 15 scripts, A37 S1's first boot read another mission's nag toward Iw03_01
    // on SaveS182 in M12)
    std::set<std::string> defScripts;
    for (const SnapObjDef& d : s.objDefs)
        if (d.nick != "null" && d.nick != "nn_blank")
            defScripts.insert(d.script);
    mission::TowardsNag nag;
    s.nag = defScripts.size() == 1 ? NagOf(nag, mission::TowardsNagOf(st.textId, nag)) : SnapNag();
    const u32 from = st.textId && st.textId != 21660 && st.textId != 1 ? st.textId : info.standInText;
    if (from)
    {
        mission::TowardsNag nn;
        for (const mission::Goal& goal : mission::NextStepGoals(from, s.system, &s.nextObjective, &nn))
            s.nextGoals.push_back(GoalOf(goal));
        s.nextNag = NagOf(nn, true);
    }
    // spawns since the last menu (the objective mode's own cut: an earlier attempt's ids are not this game's)
    static std::vector<SpawnRec> buf(2048);
    const int n = HostSpawnedShips(buf.data(), static_cast<int>(buf.size()));
    int first = 0;
    SpawnRec menu = {};
    if (MenuSpawn(menu))
        for (int i = n - 1; i >= 0; --i)
            if (buf[i].id == menu.id && memcmp(buf[i].pos, menu.pos, sizeof(menu.pos)) == 0)
            {
                first = i + 1;
                break;
            }
    std::vector<mission::Spawn> spawns;
    for (int i = first; i < n; ++i)
    {
        mission::Spawn sp;
        sp.id = buf[i].id;
        memcpy(sp.pos, buf[i].pos, sizeof(sp.pos));
        spawns.push_back(sp);
        if (i >= n - kSpawnsKept)
        {
            SnapSpawn ss;
            ss.id = buf[i].id, ss.arch = buf[i].arch;
            memcpy(ss.pos, buf[i].pos, sizeof(ss.pos));
            s.spawns.push_back(ss);
        }
    }
    s.spawnCount = static_cast<int>(spawns.size());
    s.pendingHostile = mission::PendingHostileShips(st.textId, s.system, spawns);
    // ---- holds and the game's answers
    s.missionHolds = s.pilotOk && ai::MissionHolds();
    s.dockInProgress = s.pilotOk && ai::DockInProgress();
    const ULONGLONG since = SinceGameGrant();
    s.sinceGrantMs = since == ~0ULL ? -1.0 : static_cast<double>(since);
    s.dockReplySeq = HostDockReply(&s.dockReplyShip, &s.dockReplyKind, &s.dockReplyResponse);
    s.resultSeq = HostMissionResult(&s.resultType);
    // ---- the contacts: the nearest + every object the rules name
    std::vector<const space::Contact*> sorted;
    for (const space::Contact& c : all)
        sorted.push_back(&c);
    std::sort(sorted.begin(), sorted.end(), [](const space::Contact* a, const space::Contact* b)
              { return a->distance != b->distance ? a->distance < b->distance : a->id < b->id; });
    std::set<u32> want(s.marks.begin(), s.marks.end());
    for (int i = 0; i < s.step.refCount; ++i)
        want.insert(s.step.refObj[i]);
    for (const SnapStep& e : s.entries)
        for (int i = 0; i < e.refCount; ++i)
            want.insert(e.refObj[i]);
    for (const std::vector<SnapGoal>* gs : {&s.goals, &s.iniGoals, &s.nextGoals})
        for (const SnapGoal& goal : *gs)
        {
            want.insert(goal.object);
            for (const SnapGoalObj& o : goal.objects)
                want.insert(o.id);
        }
    for (const std::vector<SnapLeash>* ls : {&s.leashes, &s.iniLeashes})
        for (const SnapLeash& l : *ls)
            want.insert(l.object), want.insert(l.ship);
    want.insert(s.nag.object);
    want.insert(s.pendingHostile.begin(), s.pendingHostile.end());
    want.insert(s.leader);
    want.insert(s.target);
    // (v3) what the arbiter's rules look up beyond those: the objective's refs, the armed goals' objects, the memory's
    // ships and targets, every jump gate / hole and airlock (the leash's and the grant's tests), the friends hostile ships
    // within the battle's reach attack (the threat test)
    want.insert(s.refsObj.begin(), s.refsObj.end());
    for (const SnapArmed& a : s.armed)
    {
        want.insert(a.goal.object);
        for (const SnapGoalObj& o : a.goal.objects)
            want.insert(o.id);
    }
    for (u32 id : {s.queue.leashShip, s.queue.lastDock, s.queue.escortShip, s.queue.subTarget, s.queue.target,
                   s.queue.escorted, s.queue.missionLeader})
        want.insert(id);
    // (v4) the pre-stage's next-step goals' objects
    for (const SnapAfter& a : s.after)
    {
        want.insert(a.goal.object);
        for (const SnapGoalObj& o : a.goal.objects)
            want.insert(o.id);
    }
    for (const SnapGoal& goal : s.realNext)
        want.insert(goal.object);
    want.insert(s.queue.scriptMarks.begin(), s.queue.scriptMarks.end());
    for (const space::Contact& c : all)
    {
        if (c.type & (space::kJumpGate | space::kJumpHole | space::kAirlockGate))
            want.insert(c.id);
        if (c.ship && c.targetId && c.distance < kBattleReach)
            want.insert(c.targetId);
    }
    want.erase(0);
    for (size_t i = 0; i < sorted.size(); ++i)
    {
        const space::Contact& c = *sorted[i];
        if (i < kNearest || want.count(c.id))
            s.contacts.push_back(ContactOf(c));
    }
    // (v4) the contacts the mission points at (marks + resolved refs): a non-ship's obstacle radius (big_coast), a ship's
    // name in the step's text (the escort); (v5) also the objective's entries' resolved refs (the step's own target: the
    // pass rule's ring), and a solar's planet docking ring (the arrive rule)
    {
        std::set<u32> pointed(s.marks.begin(), s.marks.end());
        pointed.insert(s.refsObj.begin(), s.refsObj.end());
        for (int i = 0; i < s.step.refCount; ++i)
            pointed.insert(s.step.refObj[i]);
        for (const SnapStep& e : s.entries)
            for (int i = 0; i < e.refCount; ++i)
                pointed.insert(e.refObj[i]);
        const std::string text = st.textId ? str::Lower(events::IdsText(st.textId)) : std::string();
        for (SnapContact& c : s.contacts)
        {
            if (!pointed.count(c.id))
                continue;
            if (c.ship)
                c.named = NamedIn(text, c.id);
            else
            {
                for (const space::Obstacle& o : space::Obstacles(c.pos, 50))
                    if (o.id == c.id)
                        c.radius = std::max(c.radius, o.radius);
                c.dockRing = space::PlanetDockRing(c.id);
            }
        }
    }
    for (const SnapContact& c : s.contacts)
        if (c.type & space::kTradelaneRing)
        {
            SnapLane l;
            l.ring = c.id;
            if (space::LaneLinks(c.id, l.next, l.prev))
            {
                l.openNext = l.next ? space::LaneOpen(c.id, l.next) : -1;
                l.openPrev = l.prev ? space::LaneOpen(c.id, l.prev) : -1;
                // (v5) the neighbours' positions (the route-lane rule's nearer one to the route's step 1)
                l.nextPlaced = l.next && space::ObjectPos(l.next, l.nextPos);
                l.prevPlaced = l.prev && space::ObjectPos(l.prev, l.prevPos);
                if (!l.nextPlaced)
                    memset(l.nextPos, 0, sizeof(l.nextPos));
                if (!l.prevPlaced)
                    memset(l.prevPos, 0, sizeof(l.prevPos));
                s.lanes.push_back(l);
            }
        }
    for (const space::Waypoint& w : space::Waypoints())
    {
        SnapWaypoint p;
        memcpy(p.pos, w.pos, sizeof(p.pos));
        p.system = w.system, p.object = w.object;
        s.waypoints.push_back(p);
    }
    // ---- knowledge and settings; the executor's own state
    s.knowledge = g_set.knowledge;
    s.scriptGoals = g_set.scriptGoals, s.staleDestroyWait = g_set.staleDestroyWait, s.prestageNext = g_set.prestageNext;
    s.airlockFlyThrough = g_set.airlockFlyThrough, s.preAggro = g_set.preAggro;
    s.tractorExitRun = g_set.tractorExitRun, s.tractorExitRange = g_set.tractorExitRange;
    s.bigCoast = g_set.bigCoast, s.survivalCalm = g_set.survivalCalm, s.survivalStage = g_set.survivalStage;
    s.survivalUntilJump = g_set.survivalUntilJump;
    s.followDistance = g_set.followDistance;
    s.apMode = static_cast<int>(g.mode), s.apTarget = g.target, s.objOn = g_obj.on, s.survival = g_obj.survival;
    s.ourDock = g.dock.fastRequested || (g.mode == Mode::Dock && g.started);
    return s;
}

// ---------------------------------------------------------------- the shadow (situation_shadow)
static Queue s_entry;          // the memory as the running decision began
static std::vector<SnapWaypoint> s_entryWaypoints; // (S5) the waypoint list then
static bool s_entryHave = false;
static std::string s_last;     // the last shadowed decision: its snapshot + "#shadow" + "#game-hash" (ap situation last)

void ShadowEntry()
{
    s_entry = QueueNow();
    // (S5) the waypoint list as the decision begins: the route rule reads it before any start, and a start's stop of a
    // plotted travel restores the player's list (StopImpl -> RestoreWaypoints) before the exit's snapshot
    s_entryWaypoints.clear();
    for (const space::Waypoint& w : space::Waypoints())
    {
        SnapWaypoint p;
        memcpy(p.pos, w.pos, sizeof(p.pos));
        p.system = w.system, p.object = w.object;
        s_entryWaypoints.push_back(p);
    }
    s_entryHave = true;
}

// A differ is explained only from its facts: its snapshot text into the log ("snap| " lines, `situation_io` Parse reads
// them back for a host-free replay of Decide). The first 16 per process (one leg): gate s7arb2's M13 differ could not be
// read back from its run (30 Oct 2026).
static void LogDiffer(const std::string& text)
{
    static int n = 0;
    if (n >= 16)
        return;
    Log("snap| --- differ %d", ++n);
    size_t at = 0;
    while (at < text.size())
    {
        const size_t e = text.find('\n', at);
        const std::string ln = text.substr(at, e == std::string::npos ? std::string::npos : e - at);
        Log("snap| %s", ln.c_str());
        at = e == std::string::npos ? text.size() : e + 1;
    }
}

// The verdict over a snapshot of now with `queue` as the memory; `old` = the old path's decision there; `heldWhy`: a
// hold of ObjectiveSupervise's (the proposal's detail must name the same hold); `waypoints`: the list as a decision began
// (null: now's).
static void Shadow(const Queue& queue, const Decided& old, bool started, const char* heldWhy = nullptr,
                   const std::vector<SnapWaypoint>* waypoints = nullptr)
{
    LARGE_INTEGER f, t0, t1;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&t0);
    Snapshot snap = SnapshotNow(&queue);
    snap.decided = old;
    if (waypoints)
        snap.waypoints = *waypoints;
    const Situation sit = Assess(snap);
    const Proposal p = Decide(sit, snap.queue);
    QueryPerformanceCounter(&t1);
    const double ms = static_cast<double>(t1.QuadPart - t0.QuadPart) * 1000.0 / static_cast<double>(f.QuadPart);
    const bool agree = p.rule == old.rule && p.mode == old.mode && p.target == old.target && p.action == old.action &&
                       p.keep == old.keep && (!heldWhy || p.detail == heldWhy);
    char line[512];
    snprintf(line, sizeof(line), "situation shadow %s %s%s%s old=%s,%u,%s,keep=%d%s new=%s text=%u step=0x%08x ms=%.2f%s%s",
             agree ? "agree" : "differ", old.rule.c_str(), heldWhy ? ":" : "", heldWhy ? heldWhy : "", old.mode.c_str(),
             old.target, old.action.c_str(), old.keep ? 1 : 0, started ? "" : " (start refused)", ProposalText(p).c_str(),
             snap.step.textId, sit.step, ms, p.detail.empty() ? "" : " why=", p.detail.c_str());
    for (char* c = line; *c; ++c) // (one line: a detail never breaks the event log)
        if (*c == '\n' || *c == '\r')
            *c = ' ';
    if (heldWhy)
    {
        static std::string lastHeld;
        // (a hold: an event when its verdict changes, not every second - the numbers left out of the comparison)
        const std::string key = std::string(agree ? "agree " : "differ ") + heldWhy + " " + ProposalText(p) + " " + p.detail;
        if (key == lastHeld)
            return;
        lastHeld = key;
    }
    events::Push("%s", line);
    char b[96];
    snprintf(b, sizeof(b), "#game-hash 0x%08x step=0x%08x\n", sit.hash, sit.step);
    s_last = Serialize(snap) + "#shadow " + line + "\n" + b;
    if (!agree)
        LogDiffer(s_last);
}

void ShadowCompare(const char* branch, const char* mode, u32 target, const char* action, bool keep, bool started)
{
    if (!s_entryHave) // (the switch went on during this decision: no memory of its start)
        return;
    s_entryHave = false;
    Decided old;
    old.rule = branch, old.mode = mode, old.target = target, old.action = action, old.keep = keep;
    Shadow(s_entry, old, started, nullptr, &s_entryWaypoints);
}

void ShadowHeld(const char* why)
{
    static double lastAt = -1e9;
    const double now = gameclock::NowSec();
    if (now >= lastAt && now - lastAt < 1.0) // (at most once a second of game time; a clock gone back - a load - resets)
        return;
    lastAt = now;
    const Queue q = QueueNow();
    Decided old;
    old.rule = "held", old.mode = q.sub, old.target = q.subTarget, old.action = q.action, old.keep = true;
    Shadow(q, old, true, why);
}

// ---------------------------------------------------------------- the arbiter (situation_arbiter, A37 S7)
// The goal queue - the only memory across a pause: while the objective mode runs, the executor's working memory
// (ObjectiveRun) is classed into it at every decision (FromRulesQueue, lossless: S6) and the step key written after
// Decide (Advance); the player's stop keeps it (s_paused) with the memory as it stands (CarryMemory); the next start
// re-proves it (Resume) or, with none (the first start in this game), rebuilds it from the situation alone.
static goalqueue::GoalQueue s_q;
static goalqueue::GoalQueue s_paused;
static bool s_pausedHave = false;
static PauseMark s_pause;
static bool s_resumePending = false;

Arbitration Arbitrate(const Decided& old)
{
    Arbitration a;
    if (!s_entryHave) // (the switch went on during this decision: no memory of its start)
        return a;
    s_entryHave = false;
    LARGE_INTEGER f, t0, t1;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&t0);
    // (at the exit, before anything is started: mission.cpp's memory as the decision left it, the memory and the waypoint
    // list as it began)
    Snapshot snap = SnapshotNow(&s_entry);
    snap.decided = old;
    snap.waypoints = s_entryWaypoints;
    const Situation sit = Assess(snap);
    goalqueue::Classed c = goalqueue::FromRulesQueue(sit, s_entry);
    c.queue.stepKey = s_q.stepKey; // (the step the story entries were last decided for: Advance's)
    c.queue.nextId = std::max(c.queue.nextId, s_q.nextId);
    for (const goalqueue::GoalRecord& r : s_q.entries) // (the player's own entries: none before S9)
        if (r.origin == goalqueue::Origin::Player)
            c.queue.entries.push_back(r);
    Queue rq = goalqueue::ToRulesQueue(c.queue, c.exec, sit);
    // (A37 S6 open (1): the derived leash anchor covers the named-ship anchors only - the executor's own anchor is read
    // until the derivation covers the target-ship / survival-lead ones; its supervisor's leash trip stays on the old path)
    rq.leashShip = s_entry.leashShip, rq.leashDist = s_entry.leashDist;
    a.p = Decide(sit, rq);
    goalqueue::Advance(c.queue, sit, a.p);
    // (the story memory is the old body's while it is the oracle: it writes its last real text only past the exits
    // before the script-goal block (jump, lane, the way in, ...), Advance at every decision - the queue's value fed back
    // into the body made pre-stage-next fire after every jump step: gate s7arb M11, three steps, a route into Zone 21's
    // mines and a death; run-judge 30 Oct 2026. The body has decided by now: carry its value)
    c.queue.story.lastReal = autopilot::ObjectiveLastReal();
    s_q = c.queue;
    QueryPerformanceCounter(&t1);
    a.ms = static_cast<double>(t1.QuadPart - t0.QuadPart) * 1000.0 / static_cast<double>(f.QuadPart);
    a.have = true;
    a.step = sit.step;
    a.agree = a.p.rule == old.rule && a.p.mode == old.mode && a.p.target == old.target && a.p.action == old.action &&
              a.p.keep == old.keep;
    char b[160];
    snprintf(b, sizeof(b), "#arbiter %s %s plan=%s\n#game-hash 0x%08x step=0x%08x\n", a.agree ? "agree" : "differ",
             ProposalText(a.p).c_str(), PlanKindName(a.p.plan.kind), sit.hash, sit.step);
    s_last = Serialize(snap) + b;
    if (!a.agree)
        LogDiffer(s_last);
    return a;
}

void ArbiterActivated()
{
    s_resumePending = true;
}

void ArbiterResume()
{
    using namespace autopilot;
    if (!s_resumePending)
        return;
    s_resumePending = false;
    const Snapshot snap = SnapshotNow();
    const Situation sit = Assess(snap);
    const double nowMs = static_cast<double>(gameclock::NowMs64());
    goalqueue::ResumePolicy pol;
    pol.silentMaxMs = static_cast<double>(g_set.resumeSilentS) * 1000.0;
    pol.movedMaxM = g_set.resumeMovedM;
    const goalqueue::PauseFacts pf = s_pausedHave ? PauseFactsOf(s_pause, sit, nowMs) : goalqueue::PauseFacts();
    goalqueue::GoalQueue q;
    const Restore r = ResumeOf(s_pausedHave ? &s_paused : nullptr, sit, pf, pol, q);
    s_pausedHave = false;
    s_q = q;
    // into the working memory (the activation reset ObjectiveRun): this step's - the decision's step upkeep would wipe it
    // (its seq marks the step as seen); a silent resume of the same step gets all of it back, any other start the
    // rebuilt entry's (fresh but `arrived` by the step's own arrival test); the story's dock and last real text always
    g_obj.seq = snap.objSeq;
    const goalqueue::EntryMemory& m = r.memory;
    g_obj.arrived = m.arrived;
    g_obj.stepAt = gameclock::NowMs64();
    if (r.sameStep)
    {
        g_obj.escortShip = m.escortShip, g_obj.badGoal = m.badGoal, g_obj.arriveRetries = m.arriveRetries;
        g_obj.deadGoals.clear();
        g_obj.deadGoals.insert(m.deadGoals.begin(), m.deadGoals.end());
        g_obj.viaNext = m.viaNext, g_obj.cruiseLock = m.cruiseLock, g_obj.missionLeader = m.missionLeader;
        if (m.stepAtKnown && snap.clock >= m.stepAt)
        {
            const ULONGLONG age = static_cast<ULONGLONG>((snap.clock - m.stepAt) * 1000.0);
            if (age < g_obj.stepAt)
                g_obj.stepAt -= age;
        }
    }
    g_obj.lastDock = r.lastDock;
    SetObjectiveLastReal(r.lastReal);
    events::Push("situation resume %s cold=%d same_step=%d paused_ms=%.0f moved=%.0f system_changed=%d docked=%d job=%d "
                 "arrived=%d escort=%u dead_goals=%u last_dock=%u last_real=%u step=0x%08x why=%s%s",
                 goalqueue::ResumeChoiceName(r.choice), r.cold ? 1 : 0, r.sameStep ? 1 : 0, pf.pausedMs, pf.movedM,
                 pf.systemChanged ? 1 : 0, pf.docked ? 1 : 0, pf.jobTaken ? 1 : 0, g_obj.arrived ? 1 : 0, g_obj.escortShip,
                 static_cast<unsigned>(g_obj.deadGoals.size()), r.lastDock, r.lastReal, sit.step, r.why.c_str(),
                 r.choice == goalqueue::ResumeChoice::Ask ? " (S9 draws RESUME / CLEAR; until then the start re-plans it)"
                                                           : "");
}

void ArbiterPause()
{
    using namespace autopilot;
    s_paused = s_q;
    goalqueue::CarryMemory(s_paused, QueueNow(), gameclock::NowSec());
    s_pausedHave = true;
    s_pause = PauseMark();
    s_pause.atMs = static_cast<double>(gameclock::NowMs64());
    s_pause.system = fl::PlayerSystem();
    space::Pilot p;
    s_pause.posKnown = space::ReadPilot(p);
    if (s_pause.posKnown)
        memcpy(s_pause.pos, p.pos, sizeof(s_pause.pos)), s_pause.system = p.system ? p.system : s_pause.system;
    s_pause.stepKey = s_paused.stepKey;
    events::Push("situation pause step_key=0x%08x entries=%u arrived=%d last_real=%u last_dock=%u", s_paused.stepKey,
                 static_cast<unsigned>(s_paused.entries.size()), g_obj.arrived ? 1 : 0, ObjectiveLastReal(),
                 g_obj.lastDock);
}

void ArbiterNoteDocked()
{
    if (s_pausedHave)
        s_pause.docked = true;
}

void ArbiterForgetGame()
{
    s_q = goalqueue::GoalQueue();
    s_paused = goalqueue::GoalQueue();
    s_pausedHave = false;
    s_pause = PauseMark();
}

u32 ArbiterLastReal()
{
    return s_q.story.lastReal;
}

std::string ArbiterQueueText()
{
    char b[256];
    snprintf(b, sizeof(b), "arbiter situation_arbiter=%d resume_pending=%d paused=%d\n", autopilot::g_set.situationArbiter ? 1 : 0,
             s_resumePending ? 1 : 0, s_pausedHave ? 1 : 0);
    std::string s = b + goalqueue::Serialize(s_q);
    if (s_pausedHave)
    {
        snprintf(b, sizeof(b), "#paused at_ms=%.0f system=%u pos_known=%d step_key=0x%08x docked=%d\n", s_pause.atMs,
                 s_pause.system, s_pause.posKnown ? 1 : 0, s_pause.stepKey, s_pause.docked ? 1 : 0);
        s += b + goalqueue::Serialize(s_paused);
    }
    return s;
}

std::string Command(const std::string& sub)
{
    if (sub == "last")
        return s_last.empty() ? std::string("no shadowed decision yet (ap set situation_shadow 1, then a decision)") : s_last;
    if (sub == "queue")
        return ArbiterQueueText();
    LARGE_INTEGER f, t0, t1;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&t0);
    const Snapshot snap = SnapshotNow();
    QueryPerformanceCounter(&t1);
    const double ms = static_cast<double>(t1.QuadPart - t0.QuadPart) * 1000.0 / static_cast<double>(f.QuadPart);
    const Situation sit = Assess(snap);
    char b[256];
    if (sub == "snap") // the snapshot text, the in-game hash of this very snapshot as its last (comment) line
    {
        snprintf(b, sizeof(b), "#game-hash 0x%08x step=0x%08x snap_ms=%.2f\n", sit.hash, sit.step, ms);
        return Serialize(snap) + b;
    }
    if (sub == "hash") // the hash of the live snapshot and of its text re-parsed (the round trip in game)
    {
        const std::string text = Serialize(snap);
        Snapshot back;
        std::string err;
        const bool ok = Parse(text, back, &err);
        const u32 re = ok ? Assess(back).hash : 0;
        snprintf(b, sizeof(b), "situation hash=0x%08x reparsed=0x%08x roundtrip=%s step=0x%08x text=%u snap_ms=%.2f bytes=%u",
                 sit.hash, re, !ok ? "PARSE-FAIL" : (re == sit.hash && Serialize(back) == text) ? "ok" : "DIFF", sit.step,
                 snap.step.textId, ms, static_cast<unsigned>(text.size()));
        return ok ? std::string(b) : std::string(b) + " " + err;
    }
    if (!sub.empty())
        return "ap situation [snap|hash|last|queue]";
    // (the measurements after the situation: not part of it, not hashed; logged - A37 S2's LiveTriggers timing)
    snprintf(b, sizeof(b), "timing snap_ms=%.2f live_ms=%.3f live_rows=%u story=%s", ms, snap.liveMs,
             static_cast<unsigned>(snap.live.size()), snap.goalsLive ? "live" : "ini");
    Log("situation: %s text=%u", b, snap.step.textId);
    return Describe(sit) + b;
}
} // namespace situation
