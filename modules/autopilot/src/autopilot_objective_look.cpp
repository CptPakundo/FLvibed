// Advanced Autopilot - the objective's look-ahead: LOOK's adapter (facts, log), the chase, the spawn stage, the lock trap. The pure
// verdict is lookahead.h.
#include "autopilot_impl.h"
#include "game/flhash.h"
#include "game/mission_live.h"
#include "autopilot_ashride.h"
#include "lookahead.h"
#include "situation.h"
#include "situation_exec.h"
#include "str.h"
#include "autopilot_objective_impl.h"

namespace autopilot
{
// ------------------------------------------------------------------ LOOK L0 (look_log): what LOOK would do, logged only
// (modules\autopilot\docs\LOOK_PLAN.md; the pure verdict: lookahead.h). The adapter reads what the decision already reads - the
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
            same = str::SameText(space::RepName(c.rep), want);
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
lookahead::TimerDestroyFacts TimerDestroyFactsNow(const ObjectiveStep& st, ObjVerb verb,
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
std::string TimerDestroyNext() { return s_tdAdvance.next + "(" + s_tdAdvance.via + ")"; }

// ALLY-KILL's facts now (lookahead.h AllyKill; `ally_kill`; docs\research\runners\M13_gate_allykill.md): the step's
// advance from the script (cached per step and system), its shape (StepAdvance::AllyKillShape and the next step's verb
// Dock), the kill's solar alive, the friendly ships whose target it is, the dock (the next objective's object) placed now,
// and the wait at it - when the ship first came within kAkAtDock of it this step and the target's hull then (kept per
// step), a hostile ship shooting at us within kSurvivalReach. Once per step the script's data is logged.
static mission::StepAdvance s_akAdvance;
lookahead::AllyKillFacts AllyKillFactsNow(const ObjectiveStep& st, const std::vector<space::Contact>& all)
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

void LookLog(const ObjectiveStep& st, ObjVerb verb, const std::vector<space::Contact>& all)
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
u32 ChaseCandidate(const ObjectiveStep& st, ObjVerb verb, const std::vector<space::Contact>& all,
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

StagePlan SpawnStageNow(const ObjectiveStep& st, const std::vector<space::Contact>& all)
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
int StageVerdictNow(const StagePlan& sp, float* toGo)
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
                same = str::SameText(space::RepName(c.rep), want);
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
bool LockTrapDecide(const std::vector<space::Contact>& all)
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
} // namespace autopilot
