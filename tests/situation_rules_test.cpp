// The arbiter's S3-S5 rules (controller\src\impl\situation_rules.h, A37_PLAN S3-S5): each rule over synthetic situations -
// the branch's own conditions both ways (it holds / one condition fails), the same mode / target / action as
// ObjectiveDecide's branch, and the order between them (DecideS3, Decide); then the recorded fixtures: every one decides
// without fault, and a
// fixture carrying the old path's decision (`decided rule=...`, recorded by `ap situation last` under situation_shadow)
// gets exactly that decision from the rules.
#include "situation_fixture.h"

#include "../controller/src/impl/situation_rules.h"

using namespace situation;

namespace
{
constexpr u32 kJumpGate = 1u << 6, kAirlock = 1u << 10, kJumpHole = 1u << 11, kLoot = 1u << 28;
constexpr u32 kRing = 1u << 7, kSatellite = 0x10; // (a lane ring; a satellite-class solar: a race ring)

// In space at the origin, a step with no words, no target, no navmarker; the hidden knowledge on; no contact.
Snapshot Base()
{
    Snapshot s;
    s.objSeq = 3;
    s.step.textId = 23100;
    s.system = 1, s.inSpace = true, s.pilotOk = true, s.op = 2;
    s.knowledge = 1, s.objOn = true;
    return s;
}
SnapContact& Add(Snapshot& s, u32 id, bool ship, float x, float att = 0)
{
    SnapContact c;
    c.id = id, c.ship = ship, c.pos[0] = x, c.distance = x < 0 ? -x : x, c.attitude = att, c.health = 1;
    c.maxHit = 1000;
    s.contacts.push_back(c);
    return s.contacts.back();
}
Proposal Run(const Snapshot& s, const Queue& q) { return DecideS3(Assess(s), q); }
bool Is(const Proposal& p, const char* rule, const char* mode, u32 target, const char* action, bool keep = false)
{
    const bool ok = p.rule == rule && p.mode == mode && p.target == target && p.action == action && p.keep == keep;
    if (!ok)
        fprintf(stderr, "  situation_rules: got %s, want %s %s %u %s keep=%d\n", ProposalText(p).c_str(), rule, mode, target,
                action, keep ? 1 : 0);
    return ok;
}
SnapGoal Goal(int kind, const char* trigger, float x, float dist, const char* leads)
{
    SnapGoal g;
    g.kind = kind, g.trigger = trigger, g.dist = dist, g.leads = leads, g.subject = "player", g.file = "M12\\m12.ini";
    if (kind == 1)
        g.object = 9000, g.placed = true, g.objPos[0] = x;
    else
        g.pos[0] = x;
    return g;
}
} // namespace

void Test_situation_rules()
{
    // ---- a goal leading to a landing: a token, not "land" anywhere (audit A1-4; DATA\MISSIONS nicknames)
    CHECK(situation::LeadsToLanding("land"));
    CHECK(situation::LeadsToLanding("land_on_leeds"));
    CHECK(situation::LeadsToLanding("landing_on_osiris"));
    CHECK(situation::LeadsToLanding("force_land_Osiris"));
    CHECK(situation::LeadsToLanding("forceland"));
    CHECK(situation::LeadsToLanding("mlog_land_on_ca_minor"));
    CHECK(!situation::LeadsToLanding("mlog_destroy_the_rheinland_forces"));
    CHECK(!situation::LeadsToLanding("badlands_tether_30"));
    CHECK(!situation::LeadsToLanding("freighter_rheinland"));
    CHECK(!situation::LeadsToLanding("junilands"));
    CHECK(!situation::LeadsToLanding("SUCCEED"));
    CHECK(!situation::LeadsToLanding(""));
    const Queue q0;
    // ---- nothing holds: a later slice's rule
    CHECK(Run(Base(), q0).rule == "none");

    // ---- Rule_Held: ObjectiveSupervise's holds; docked / no ship hold nothing (the objective mode decides there)
    {
        Snapshot s = Base();
        Queue q;
        q.sub = "travel", q.subTarget = 5, q.action = "route";
        s.missionHolds = true;
        Proposal p = Run(s, q);
        CHECK(Is(p, "held", "travel", 5, "route", true) && p.detail == "mission_holds");
        s.missionHolds = false, s.laneRide = true;
        CHECK(Run(s, q).detail == "lane");
        s.laneRide = false, s.jumping = true;
        CHECK(Run(s, q).detail == "jump");
        s.jumping = false, s.sinceGrantMs = 1000; // a mission's fresh dock grant
        CHECK(Run(s, q).detail == "game_dock");
        s.ourDock = true; // (our own dock is no game dock)
        CHECK(Run(s, q).rule == "none");
        s.ourDock = false, s.sinceGrantMs = -1;
        s.inSpace = false, s.pilotOk = false, s.base = 77;
        CHECK(Assess(s).held == Held::Docked && !Rule_Held(Assess(s), q));
    }

    // ---- Rule_Leash: far out on an armed ship leash -> back toward the ship
    {
        Snapshot s = Base();
        Add(s, 50, true, 5000, 0.8f);
        Queue q;
        q.leashShip = 50, q.leashDist = 6000;
        CHECK(Is(Run(s, q), "leash", "travel", 0, "leash"));
        q.mode = "travel", q.action = "leash", q.sub = "travel", q.trHasDest = true, q.trDest[0] = 5500; // the trip runs
        CHECK(Is(Run(s, q), "leash", "travel", 0, "leash", true));
        q.trDest[0] = 7000; // ...but ends 2 km from the ship: a new trip
        CHECK(Is(Run(s, q), "leash", "travel", 0, "leash"));
        q.trDestObject = 50; // (a trip to an object is not ours back)
        CHECK(!Run(s, q).keep);
        q = Queue(), q.leashShip = 50, q.leashDist = 6000;
        s.contacts[0].distance = 4000; // within 80 %
        CHECK(!Rule_Leash(Assess(s), q));
        s.contacts[0].distance = 5000;
        Snapshot d = s;
        d.step.verb = Verb::Dock; // the step docks: the leash's phase is over
        CHECK(!Rule_Leash(Assess(d), q));
        Snapshot j = s; // the step points at a jump hole
        Add(j, 70, false, 20000).type = kJumpHole;
        j.step.refs[0] = 1, j.step.refObj[0] = 70, j.step.refCount = 1;
        CHECK(!Rule_Leash(Assess(j), q));
        Snapshot n = s; // ...or its navmarker lies within 2 km of a jump gate
        Add(n, 71, false, 30000).type = kJumpGate;
        n.step.hasNav = true, n.step.nav[0] = 31500;
        CHECK(!Rule_Leash(Assess(n), q));
        n.step.nav[0] = 33000;
        CHECK(Rule_Leash(Assess(n), q).has_value());
        CHECK(!Rule_Leash(Assess(Base()), q)); // the ship gone from the contacts
    }

    // ---- Rule_MarkedLoot: marked loot a destroy objective names, nothing attacking -> shoot it
    {
        Snapshot s = Base();
        Add(s, 60, false, 900).type = kLoot;
        s.marks = {60};
        s.step.verb = Verb::Fight;
        CHECK(Is(Run(s, q0), "marked-loot", "shoot", 60, "fight"));
        Queue q;
        q.mode = "shoot", q.target = 60;
        CHECK(Is(Run(s, q), "marked-loot", "shoot", 60, "fight", true));
        Snapshot e = s; // a later entry's words ("Destroy %L0v3!" as entry 1)
        e.step.verb = Verb::None;
        SnapStep e1;
        e1.verb = Verb::Fight;
        e.entries = {SnapStep(), e1};
        CHECK(Rule_MarkedLoot(Assess(e), q0).has_value());
        e.entries.clear();
        CHECK(!Rule_MarkedLoot(Assess(e), q0)); // no destroy words: no shot
        Snapshot t = s; // an attacker within 3 km: the fight first
        Add(t, 61, true, 2500, -1).targetsPlayer = true;
        CHECK(!Rule_MarkedLoot(Assess(t), q0));
        Snapshot u = s; // ...under the survival stance only within 1.2 km
        Queue qs;
        qs.survival = true; // (the stance as the decision began: the memory's)
        Add(u, 61, true, 2500, -1).targetsPlayer = true;
        CHECK(Rule_MarkedLoot(Assess(u), qs).has_value());
        u.survival = true; // (the snapshot's own stance - taken at the exit, after the S4 update - is not the branch's)
        CHECK(!Rule_MarkedLoot(Assess(u), q0));
        u.version = 3; // ...but a v3 snapshot's is (no memory of it recorded)
        CHECK(Rule_MarkedLoot(Assess(u), q0).has_value());
        Snapshot b = s; // a hostile ship attacking a friendly station within 15 km
        Add(b, 62, true, 12000, -1).targetId = 63;
        SnapContact& st = Add(b, 63, false, 14000);
        st.attKnown = true, st.att = 0.5f;
        CHECK(!Rule_MarkedLoot(Assess(b), q0));
        Snapshot unm = s;
        unm.marks.clear();
        CHECK(!Rule_MarkedLoot(Assess(unm), q0)); // unmarked loot
    }

    // ---- Rule_TractorExit: the mark fallback's loot within the beam while a travel runs
    {
        Snapshot s = Base();
        Add(s, 61, false, 800).type = kLoot;
        s.marks = {61};
        s.tractorRange = 1000;
        Queue q;
        q.mode = "travel", q.sub = "travel", q.subTarget = 9, q.action = "script-goal";
        CHECK(Is(Run(s, q), "tractor-exit", "travel", 9, "script-goal", true)); // the trip goes on
        s.tractorExitRun = true, s.tractorExitRange = 600;
        q.cruiseLock = true, q.lockId = 77, q.lockHave = true, q.lockP2[0] = 500;
        CHECK(Is(Run(s, q), "tractor-exit", "travel", 77, "airlock")); // on to the trip's airlock
        q.lockP2[0] = 2000; // the door beyond tractor_exit_range: on
        CHECK(Is(Run(s, q), "tractor-exit", "travel", 9, "script-goal", true));
        q.lockP2[0] = 500, q.lockId = ~0u; // (no real lock)
        CHECK(Run(s, q).keep);
        s.tractorRange = 500; // out of the beam's reach
        CHECK(!Rule_TractorExit(Assess(s), q));
        s.tractorRange = 0; // unknown reach: 1500 m
        CHECK(Rule_TractorExit(Assess(s), q).has_value());
        q.sub = "combat"; // no travel of ours
        CHECK(!Rule_TractorExit(Assess(s), q));
    }

    // ---- Rule_Jump: the step's target is a jump gate / hole
    {
        Snapshot s = Base();
        Add(s, 70, false, 9000).type = kJumpGate;
        s.step.refs[0] = 4, s.step.refObj[0] = 70, s.step.refCount = 1;
        CHECK(Is(Run(s, q0), "jump", "travel", 70, "jump"));
        s.contacts[0].type = kJumpHole;
        CHECK(Is(Run(s, q0), "jump", "travel", 70, "jump"));
        s.contacts[0].type = 0;
        CHECK(Run(s, q0).rule == "none");
    }

    // ---- Rule_Airlock: a mission's grant, no dock maneuver, an airlock within 3 km - not the way we came in
    {
        Snapshot s = Base();
        s.sinceGrantMs = 5000; // (past the 4 s the grant holds the ship)
        Add(s, 81, false, 1000).type = kAirlock;
        Add(s, 80, false, 2000).type = kAirlock;
        CHECK(Is(Run(s, q0), "airlock", "travel", 81, "airlock")); // the nearest
        s.hasEntry = true, s.entry[0] = 1100; // ...unless we came in by it
        CHECK(Is(Run(s, q0), "airlock", "travel", 80, "airlock"));
        s.sinceGrantMs = 25000; // the grant too old
        CHECK(!Rule_Airlock(Assess(s), q0));
        s.sinceGrantMs = 5000, s.dockInProgress = true; // the game's dock maneuver runs (and holds the ship)
        CHECK(!Rule_Airlock(Assess(s), q0));
        s.dockInProgress = false, s.contacts[0].distance = s.contacts[1].distance = 3500;
        CHECK(!Rule_Airlock(Assess(s), q0));
        Snapshot g = Base(); // a fresh grant with an airlock in reach: flown through, not held (airlock_fly_through)
        g.sinceGrantMs = 1000;
        Add(g, 81, false, 1000).type = kAirlock;
        CHECK(Is(Run(g, q0), "airlock", "travel", 81, "airlock"));
    }

    // ---- Rule_ScriptDestroy: the script's destroy condition for a step with no target of its own
    {
        Snapshot s = Base();
        SnapGoal g = Goal(3, "gens_dead", 0, 0, "SUCCEED");
        g.count = 2;
        g.objects = {{90, true, {}}, {91, true, {40000, 0, 0}}, {92, false, {}}};
        s.goals = {g};
        Add(s, 90, false, 3000);
        CHECK(Is(Run(s, q0), "script-destroy", "combat", 0, "script-kill")); // the nearest 3 km: their fight
        s.contacts[0].health = 0; // gone: the next is 40 km off -> a trip
        CHECK(Is(Run(s, q0), "script-destroy", "travel", 0, "to-marks"));
        s.contacts[0].health = 1;
        Snapshot l = s; // the live view is the assessor's; the rule reads the ini view
        l.goalsLive = true, l.iniGoals = l.goals, l.goals.clear();
        CHECK(Is(Run(l, q0), "script-destroy", "combat", 0, "script-kill"));
        l.iniGoals.clear(), l.goals = s.goals;
        CHECK(Run(l, q0).rule == "none");
        Snapshot st = s; // stale_destroy_wait: by the last objective, none marked, a later step's
        st.goalsFromLast = true, st.goals[0].leads = "nm_next";
        CHECK(!Rule_ScriptDestroy(Assess(st), q0));
        st.marks = {91};
        CHECK(Rule_ScriptDestroy(Assess(st), q0).has_value());
        st.marks.clear(), st.staleDestroyWait = false;
        CHECK(Rule_ScriptDestroy(Assess(st), q0).has_value());
        st.staleDestroyWait = true, st.goals[0].leads = "null"; // (one that ends the mission is never a later step's)
        CHECK(Rule_ScriptDestroy(Assess(st), q0).has_value());
        Snapshot f = s;
        f.step.verb = Verb::Fight; // a fight step: its marks are the fight's
        CHECK(!Rule_ScriptDestroy(Assess(f), q0));
        f.step.verb = Verb::Dock;
        CHECK(!Rule_ScriptDestroy(Assess(f), q0));
        f.step.verb = Verb::None, f.knowledge = 0; // the knowledge scope "exposed": hidden data off
        CHECK(!Rule_ScriptDestroy(Assess(f), q0));
        f.knowledge = 1, f.scriptGoals = false;
        CHECK(!Rule_ScriptDestroy(Assess(f), q0));
        f.scriptGoals = true;
        Add(f, 99, true, 2000, 0.5f); // a target ship of the step's own
        f.step.refs[0] = 2, f.step.refObj[0] = 99, f.step.refCount = 1;
        CHECK(!Rule_ScriptDestroy(Assess(f), q0));
        Snapshot gone = s; // nothing alive: no condition
        gone.contacts[0].health = 0, gone.goals[0].objects[1].placed = false;
        CHECK(!Rule_ScriptDestroy(Assess(gone), q0));
    }

    // ---- Rule_DockEntry: the last dock behind a placeholder the script does not know
    {
        Snapshot s = Base();
        s.step.textId = 21660, s.goalsFromLast = true;
        Add(s, 95, false, 12000).dock = true;
        Queue q;
        q.lastDock = 95;
        CHECK(Is(Run(s, q), "dock-entry", "travel", 95, "dock"));
        Snapshot l = s; // a ship leash in the system: the script moves on
        l.leashes.push_back({50, "juni", 6000, 0, {}});
        CHECK(!Rule_DockEntry(Assess(l), q));
        l.goalsLive = true, l.iniLeashes.clear(); // (the ini view's leashes count, not the live view's)
        CHECK(Rule_DockEntry(Assess(l), q).has_value());
        s.goalsFromLast = false; // a step the script knows
        CHECK(!Rule_DockEntry(Assess(s), q));
        s.goalsFromLast = true, s.step.verb = Verb::Dock; // a dock step sets the memory itself
        CHECK(!Rule_DockEntry(Assess(s), q));
        s.step.verb = Verb::None, s.contacts.clear(); // the target gone
        CHECK(!Rule_DockEntry(Assess(s), q));
    }

    // ---- Rule_ScriptGoal: the step's flying condition
    {
        Snapshot s = Base(); // "Awaiting": the last objective's goal stands in
        s.step.textId = 21660, s.goalsFromLast = true;
        s.goals = {Goal(1, "near_toledo", 20000, 8000, "nm_land")};
        CHECK(Is(Run(s, q0), "script-goal", "travel", 0, "script-goal"));
        Queue q;
        q.badGoal = "near_toledo"; // its trip found no way this step
        CHECK(!Rule_ScriptGoal(Assess(s), q));
        q = Queue(), q.deadGoals = {"near_toledo"};
        CHECK(!Rule_ScriptGoal(Assess(s), q));
        Snapshot own = s; // a real step: only a condition another ship drives (Ozu within 9 km of the hole)
        own.goalsFromLast = false, own.step.textId = 23100;
        CHECK(!Rule_ScriptGoal(Assess(own), q0));
        own.goals[0].subject = "ozu";
        CHECK(Rule_ScriptGoal(Assess(own), q0).has_value());
        Snapshot nav = own; // a navmarker inside the goal's reach: the same trip
        nav.step.hasNav = true, nav.step.nav[0] = 19000;
        CHECK(!Rule_ScriptGoal(Assess(nav), q0));
        nav.step.nav[0] = -30000;
        CHECK(Rule_ScriptGoal(Assess(nav), q0).has_value());
        nav.goalsFromLast = true; // a navmarker step the script does not know: the last step's goals are stale
        CHECK(!Rule_ScriptGoal(Assess(nav), q0));
        Snapshot fight = s; // a fight step: only a goal of its own chain that ends the mission
        fight.goalsFromLast = false, fight.step.textId = 23100, fight.step.verb = Verb::Fight;
        fight.goals[0].leads = "nm_next";
        CHECK(!Rule_ScriptGoal(Assess(fight), q0));
        fight.goals[0].leads = "SUCCEED";
        CHECK(Rule_ScriptGoal(Assess(fight), q0).has_value());
        fight.goals[0].byEntry = true; // (armed from the system entry, not by the step's chain)
        CHECK(!Rule_ScriptGoal(Assess(fight), q0));
        Snapshot esc = s;
        esc.step.verb = Verb::Escort;
        CHECK(!Rule_ScriptGoal(Assess(esc), q0));
        // a leash: the goal 20 km off lies beyond 85 % of a 6 km leash round its ship next to us
        Snapshot ls = s;
        ls.leashes.push_back({50, "vonclaussen", 6000, 0, {}});
        Add(ls, 50, true, 1000, 0.5f);
        CHECK(!Rule_ScriptGoal(Assess(ls), q0));
        ls.leashes[0].off = true; // disarmed by its comm
        CHECK(Rule_ScriptGoal(Assess(ls), q0).has_value());
        ls.leashes[0].off = false, ls.contacts[0].ship = false; // a solar 7 km off: outside it, not armed
        ls.contacts[0].pos[0] = 7000, ls.contacts[0].distance = 7000;
        ls.leashes[0].placed = true, ls.leashes[0].opos[0] = 7000;
        CHECK(Rule_ScriptGoal(Assess(ls), q0).has_value());
        ls.leashes[0].nameIds = 21001, ls.leashes[0].ship = 51; // ...its pilot by name: a ship always anchors
        Add(ls, 51, true, 5000, 0.5f);
        CHECK(!Rule_ScriptGoal(Assess(ls), q0));
        // inside a goal already: it fired - the goal its chain arms is the trip
        Snapshot fired = Base();
        fired.step.textId = 21660, fired.goalsFromLast = true;
        fired.goals = {Goal(2, "in_core", 300, 700, "nm_next")};
        CHECK(!Rule_ScriptGoal(Assess(fired), q0));
        SnapArmed a;
        a.by = "in_core", a.byFile = "M12\\m12.ini", a.goal = Goal(2, "at_exit", 9000, 500, "nm_exit");
        fired.armed = {a};
        CHECK(Is(Run(fired, q0), "script-goal", "travel", 0, "script-goal"));
        q = Queue(), q.deadGoals = {"at_exit"};
        CHECK(!Rule_ScriptGoal(Assess(fired), q));
        fired.armed[0].byFile = "M13\\m13.ini"; // (another script's trigger of that name)
        CHECK(!Rule_ScriptGoal(Assess(fired), q0));
        // a fired destroy condition is no trip (all its objects seen alive and gone: DestroyFired over the memory)
        Snapshot dk = Base();
        SnapGoal gd = Goal(3, "gens_dead", 0, 0, "nm_next");
        gd.objects = {{90, false, {}}};
        dk.goals = {gd};
        dk.step.verb = Verb::Fight;
        Queue seen;
        seen.seenAlive = {90}, seen.seenListed = {90};
        CHECK(!Rule_ScriptGoal(Assess(dk), seen));
        // through: a goal armed inside the first one's sphere
        Snapshot th = s;
        a.by = "near_toledo", a.byFile = "M12\\m12.ini", a.goal = Goal(1, "near_ring", 20500, 3000, "nm_dock");
        th.armed = {a};
        Proposal p = Run(th, q0);
        CHECK(Is(p, "script-goal", "travel", 0, "script-goal") && p.detail.find("arms near_ring") != std::string::npos);
        // the mission's success outranks a nearer goal
        Snapshot w = s;
        w.goals[0].leads = "nm_dock"; // ("land" in the step it leads to counts as the success too)
        w.goals.push_back(Goal(2, "far_win", 60000, 1000, "SUCCEED"));
        p = Run(w, q0);
        CHECK(p.detail.find("far_win") == 0);
    }

    // ---- the order: the hold > the leash > marked loot > ... > the script goal
    {
        Snapshot s = Base();
        s.goalsFromLast = true;
        s.goals = {Goal(1, "near_toledo", 20000, 8000, "nm_land")};
        Add(s, 70, false, 9000).type = kJumpGate;
        s.step.refs[0] = 4, s.step.refObj[0] = 70, s.step.refCount = 1;
        CHECK(Run(s, q0).rule == "jump"); // (a target of the step's own: no script goal either)
        s.step.refCount = 0;
        CHECK(Run(s, q0).rule == "script-goal");
        Queue q;
        q.leashShip = 50, q.leashDist = 6000;
        Add(s, 50, true, 5500, 0.8f);
        CHECK(Run(s, q).rule == "leash");
        s.laneRide = true;
        CHECK(Run(s, q).rule == "held");
    }

    // ================================================================ S4
    const auto RunAll = [](const Snapshot& s, const Queue& q) { return Decide(Assess(s), q); };
    // ---- Rule_PreStage: the script block's fired condition while the script talks -> the next step's goal
    {
        Snapshot s = Base(); // "Awaiting" by the last objective, inside its 700 m condition: fired
        s.step.textId = 21660, s.goalsFromLast = true;
        s.goals = {Goal(2, "in_core", 300, 700, "nm_next")};
        SnapAfter a;
        a.leads = "nm_next", a.file = "M12\\m12.ini", a.goal = Goal(2, "at_exit", 9000, 500, "nm_exit");
        s.after = {a};
        CHECK(Is(RunAll(s, q0), "pre-stage", "travel", 0, "pre-stage"));
        CHECK(DecideS3(Assess(s), q0).rule == "none"); // (no S3 rule there)
        Snapshot in = s; // inside the next step's goal already: it fires the moment it is set
        in.after[0].goal = Goal(2, "at_exit", 200, 500, "nm_exit");
        CHECK(!Rule_PreStage(Assess(in), q0));
        Snapshot other = s; // GoalsAfter of another script's objective of that name
        other.after[0].file = "M13\\m13.ini";
        CHECK(!Rule_PreStage(Assess(other), q0));
        Snapshot real = s; // a real step (not by the last objective) and no fight: no pre-stage
        real.goalsFromLast = false, real.step.textId = 23100, real.goals[0].subject = "ozu";
        CHECK(!Rule_PreStage(Assess(real), q0));
        Snapshot k3 = s; // the next step's destroy condition: its nearest placed object - a trip, within 700 m a hold
        a.goal = Goal(3, "gens", 0, 0, "SUCCEED");
        a.goal.objects = {{90, true, {5000, 0, 0}}, {91, false, {}}};
        k3.after = {a};
        CHECK(Is(RunAll(k3, q0), "pre-stage", "travel", 0, "pre-stage"));
        k3.after[0].goal.objects[0].pos[0] = 500;
        CHECK(Is(RunAll(k3, q0), "pre-stage", "hold", 0, "pre-stage"));
        k3.after[0].goal.objects[0].placed = false; // nothing placed: no stand
        CHECK(!Rule_PreStage(Assess(k3), q0));
        Snapshot f = Base(); // a fight step whose destroy condition is met (seen alive, gone)
        f.step.verb = Verb::Fight;
        SnapGoal gd = Goal(3, "gen3_dead", 0, 0, "nm_fly");
        gd.objects = {{90, false, {}}};
        f.goals = {gd};
        a.leads = "nm_fly", a.goal = Goal(2, "airlock_top", 4000, 500, "nm_in");
        f.after = {a};
        Queue seen;
        seen.seenAlive = {90}, seen.seenListed = {90};
        CHECK(Is(RunAll(f, seen), "pre-stage", "travel", 0, "pre-stage"));
        CHECK(!Rule_PreStage(Assess(f), q0)); // (never seen alive: not fired)
        f.step.verb = Verb::None; // a destroy fired under a real non-fight step: not this branch
        CHECK(!Rule_PreStage(Assess(f), seen));
    }

    // ---- Rule_PreStageNext: a placeholder after a step that fired nothing -> the next step's own goal / declared place
    {
        Snapshot s = Base();
        s.step.textId = 21660, s.goalsFromLast = true;
        s.prestage.lastReal = 22610, s.realNext = {Goal(2, "depot_wp1", 20000, 1000, "nm_x")};
        Queue q;
        q.lastReal = 22610;
        CHECK(Is(RunAll(s, q), "pre-stage-next", "travel", 0, "pre-stage"));
        CHECK(!Rule_PreStageNext(Assess(s), q0)); // no last real objective
        Snapshot off = s;
        off.prestageNext = false;
        CHECK(!Rule_PreStageNext(Assess(off), q));
        Snapshot stale = s; // the snapshot's facts from another text: fail closed
        stale.prestage.lastReal = 22600;
        CHECK(!Rule_PreStageNext(Assess(stale), q));
        Snapshot nav = s;
        nav.step.hasNav = true, nav.step.nav[0] = 50000;
        CHECK(!Rule_PreStageNext(Assess(nav), q));
        Snapshot real = s; // a real step's text is lastReal itself; not by the last objective and no 21660: none
        real.step.textId = 22610, real.goalsFromLast = false;
        CHECK(!Rule_PreStageNext(Assess(real), q));
        real.step.textId = 21660; // (21660 counts even when the ini view does not stand in)
        CHECK(Rule_PreStageNext(Assess(real), q).has_value());
        Snapshot pl = s; // no goal: the declared next place, beyond 1 km
        pl.realNext.clear(), pl.prestage.nextOk = true, pl.prestage.nextPos[0] = 5000, pl.prestage.nextText = 22615;
        CHECK(Is(RunAll(pl, q), "pre-stage-next", "travel", 0, "pre-stage"));
        pl.prestage.nextPos[0] = 800;
        CHECK(!Rule_PreStageNext(Assess(pl), q));
        Snapshot ls = s; // Juni's 6 km leash: the 20 km goal is beyond it, the declared place 5 km off is not
        ls.leashes.push_back({50, "juni", 6000, 0, {}});
        Add(ls, 50, true, 500, 0.5f);
        CHECK(!Rule_PreStageNext(Assess(ls), q));
        ls.prestage.nextOk = true, ls.prestage.nextPos[0] = 5000;
        CHECK(Rule_PreStageNext(Assess(ls), q).has_value());
        Snapshot fired = s; // a fired condition: the pre-stage's business
        fired.goals = {Goal(2, "in_core", 300, 700, "nm_next")};
        CHECK(!Rule_PreStageNext(Assess(fired), q));
    }

    // ---- Rule_MarkFight: a fight step's marks - a huge static one is the fight's, a far one a trip first
    {
        Snapshot s = Base();
        s.step.verb = Verb::Fight;
        Add(s, 80, true, 8000, -1);
        s.marks = {80};
        CHECK(Is(RunAll(s, q0), "mark-fight", "travel", 0, "to-marks"));
        s.contacts[0].distance = 4000, s.contacts[0].pos[0] = 4000; // in reach: the threat's fight (the order)
        CHECK(!Rule_MarkFight(Assess(s), q0));
        CHECK(Is(RunAll(s, q0), "threat-fight", "combat", 0, "fight"));
        Snapshot fr = s; // a friendly mark is not the fight's
        fr.contacts[0].attitude = 0.5f, fr.contacts[0].distance = 8000;
        CHECK(!Rule_MarkFight(Assess(fr), q0));
        Snapshot big = Base(); // big_coast: a huge hostile station
        big.step.verb = Verb::Fight;
        SnapContact& st = Add(big, 81, false, 3000);
        st.attKnown = true, st.att = -1, st.radius = 900;
        big.marks = {81};
        CHECK(Is(RunAll(big, q0), "mark-fight", "combat", 81, "fight"));
        big.bigCoast = false;
        CHECK(!Rule_MarkFight(Assess(big), q0));
        big.bigCoast = true, big.contacts[0].radius = 400; // not huge
        CHECK(!Rule_MarkFight(Assess(big), q0));
        Snapshot nf = s; // not a fight step
        nf.step.verb = Verb::None, nf.contacts[0].distance = 8000;
        CHECK(!Rule_MarkFight(Assess(nf), q0));
    }

    // ---- Rule_PreAggro: a ship about to turn hostile, near -> fought now
    {
        Snapshot s = Base();
        Add(s, 85, true, 2000, 0.1f).preHostile = true;
        CHECK(Is(RunAll(s, q0), "pre-aggro", "combat", 85, "pre-aggro"));
        Snapshot d = s;
        d.step.verb = Verb::Dock;
        CHECK(!Rule_PreAggro(Assess(d), q0));
        d.step.verb = Verb::Escort;
        CHECK(!Rule_PreAggro(Assess(d), q0));
        s.contacts[0].health = 0;
        CHECK(!Rule_PreAggro(Assess(s), q0));
        s.contacts[0].health = 1;
        Queue q; // an armed ship leash: only within 75 % of it from its ship
        q.leashShip = 50, q.leashDist = 6000;
        Add(s, 50, true, -1000, 0.5f);
        CHECK(Rule_PreAggro(Assess(s), q).has_value()); // 3 km from the leash's ship
        s.contacts[0].pos[0] = 4000, s.contacts[0].distance = 4000;
        CHECK(!Rule_PreAggro(Assess(s), q)); // 5 km: beyond 4.5
        Snapshot nv = s; // (a navmarker at a jump gate clears the leash first: then no 75 % limit)
        Add(nv, 72, false, 40000).type = kJumpGate;
        nv.step.hasNav = true, nv.step.nav[0] = 41000;
        CHECK(Rule_PreAggro(Assess(nv), q).has_value());
        Snapshot at = Base(); // ...and before an attacker's fight (the order)
        Add(at, 85, true, 2000, 0.1f).preHostile = true;
        Add(at, 86, true, 1500, -1).targetsPlayer = true;
        CHECK(RunAll(at, q0).rule == "pre-aggro");
    }

    // ---- the survival stance: the stage, the attacked stance, its mission memory
    {
        Snapshot s = Base();
        s.title = 22602; // ESCAPE FROM LIBERTY: the stance from the title
        s.prestage.lastReal = 23100, s.prestage.stageFrom = 23100, s.prestage.stageOk = true, s.prestage.stagePos[0] = 5000;
        CHECK(Is(RunAll(s, q0), "survival-stage", "travel", 0, "survive"));
        s.prestage.stagePos[0] = 500;
        CHECK(Is(RunAll(s, q0), "survival-stage", "hold", 0, "survive"));
        s.prestage.stagePos[0] = 1200; // an attacker and the place within 1.5 km: the fight there
        Add(s, 86, true, 1000, -1).targetsPlayer = true;
        CHECK(Is(RunAll(s, q0), "survival-stage", "combat", 0, "survive"));
        Snapshot ns = s; // survival_stage off: the attacked stance - no lead: the close attackers
        ns.survivalStage = false;
        CHECK(Is(RunAll(ns, q0), "survival", "combat", 0, "survive"));
        Add(ns, 87, true, 2000, 0.6f); // Juni, marked
        ns.marks = {87};
        CHECK(Is(RunAll(ns, q0), "survival", "escort", 87, "survive"));
        ns.contacts[0].distance = 2000; // the attacker beyond the stance's 1.2 km: no threat
        CHECK(!Rule_Survival(Assess(ns), q0));
        Snapshot calm = s; // survival_calm off: a stage needs the calm stance
        calm.survivalCalm = false;
        CHECK(!Rule_SurvivalStage(Assess(calm), q0));
        Snapshot nav = s; // a step with its own navmarker: a trip, no stage
        nav.step.hasNav = true, nav.step.nav[0] = 9000;
        CHECK(!Rule_SurvivalStage(Assess(nav), q0));
        Snapshot ex = s; // the knowledge scope "exposed"
        ex.knowledge = 0;
        CHECK(!Rule_SurvivalStage(Assess(ex), q0));
        Snapshot mis = s; // the stage's facts from another text: fail closed
        mis.prestage.stageFrom = 23000;
        CHECK(!Rule_SurvivalStage(Assess(mis), q0));
        // the memory: the stance stays for the mission's title; survival_until_jump ends it out of the first system
        Snapshot j = Base();
        j.title = 22602, j.pilotSystem = 6;
        j.prestage = s.prestage;
        Queue q;
        q.survTitle = 22602, q.survMission = true, q.survSystem = 5, q.survival = true;
        CHECK(!Rule_SurvivalStage(Assess(j), q)); // (jumped: 5 -> 6)
        j.survivalUntilJump = false;
        CHECK(Rule_SurvivalStage(Assess(j), q).has_value());
        j.survivalUntilJump = true, j.pilotSystem = 5;
        CHECK(Rule_SurvivalStage(Assess(j), q).has_value());
        q.survEnded = true, q.survMission = false, q.survival = false; // ended earlier: stays ended for the title
        CHECK(!Rule_SurvivalStage(Assess(j), q));
        Snapshot w = Base(); // the escape words of another title, or a Survive step's
        w.title = 30001, w.titleEscape = true, w.prestage = s.prestage;
        CHECK(Rule_SurvivalStage(Assess(w), q0).has_value());
        w.titleEscape = false;
        CHECK(!Rule_SurvivalStage(Assess(w), q0));
        w.step.verb = Verb::Survive;
        CHECK(Rule_SurvivalStage(Assess(w), q0).has_value());
    }

    // ---- Rule_ThreatFight: a threat or the step's own hostile target
    {
        Snapshot s = Base();
        Add(s, 86, true, 2500, -1).targetsPlayer = true;
        CHECK(Is(RunAll(s, q0), "threat-fight", "combat", 0, "fight"));
        Snapshot e = s; // an escort step's friendly ship target: the escort
        e.step.verb = Verb::Escort;
        Add(e, 101, true, 900, 0.5f);
        e.step.refs[0] = 3, e.step.refObj[0] = 101, e.step.refCount = 1;
        CHECK(Is(RunAll(e, q0), "threat-fight", "escort", 101, "escort"));
        Snapshot m = Base(); // a placeholder's marked hostiles while an escort runs: carried with it
        Add(m, 88, true, 4000, -1);
        m.marks = {88};
        Add(m, 102, true, 300, 0.5f);
        Queue q;
        q.mode = "escort", q.escorted = 102;
        CHECK(Is(RunAll(m, q), "threat-fight", "escort", 102, "fight"));
        CHECK(Is(RunAll(m, q0), "threat-fight", "combat", 0, "fight")); // (no escort running)
        Snapshot h = Base(); // a hostile ship target, no threat: its fight
        Add(h, 103, true, 7000, -1);
        h.step.refs[0] = 4, h.step.refObj[0] = 103, h.step.refCount = 1;
        CHECK(Is(RunAll(h, q0), "threat-fight", "combat", 0, "fight"));
        h.title = 22602, h.survivalCalm = false, h.step.verb = Verb::Fight; // survival: the objective's own hostile ship
        CHECK(Is(RunAll(h, q0), "threat-fight", "combat", 103, "fight"));
        h.survivalCalm = true; // the calm stance pursues nothing
        CHECK(!Rule_ThreatFight(Assess(h), q0));
        Snapshot t = s; // a trip objective outranks the fight around it
        t.step.hasNav = true, t.step.nav[0] = 30000;
        CHECK(!Rule_ThreatFight(Assess(t), q0));
        Queue arrived; // ...until it arrived
        arrived.arrived = true;
        CHECK(Rule_ThreatFight(Assess(t), arrived).has_value());
    }

    // ---- Rule_Escort / Rule_ShipTarget: the escort's ship, a ship target
    {
        Snapshot s = Base(); // "Escort the Transport Convoy to the First Buoy": the target is a buoy, the ship a mark
        s.step.verb = Verb::Escort;
        Add(s, 130, false, 5000);
        s.step.refs[0] = 7, s.step.refObj[0] = 130, s.step.refCount = 1;
        Add(s, 110, true, 1000, 0.5f);
        Add(s, 111, true, 3000, 0.5f).named = true;
        s.marks = {110, 111};
        CHECK(Is(RunAll(s, q0), "escort", "formation", 111, "formation")); // the named ship before the nearer
        Snapshot mk = s; // (no target: the mark fallback makes the nearest friendly mark the ship target)
        mk.step.refCount = 0;
        CHECK(Is(RunAll(mk, q0), "ship-target", "formation", 110, "formation"));
        s.contacts[2].named = false;
        CHECK(Is(RunAll(s, q0), "escort", "formation", 110, "formation"));
        Queue q;
        q.escortShip = 111; // the ship the objective pointed at earlier
        CHECK(Is(RunAll(s, q), "escort", "formation", 111, "formation"));
        q.sub = "follow"; // the follow already running (the game refused formation)
        CHECK(Is(RunAll(s, q), "escort", "follow", 111, "follow"));
        Snapshot none = Base();
        none.step.verb = Verb::Escort;
        CHECK(!Rule_Escort(Assess(none), q0));
        Snapshot t = Base(); // a friendly ship target: follow it; an escort step: formation
        Add(t, 120, true, 2000, 0.5f);
        t.step.refs[0] = 5, t.step.refObj[0] = 120, t.step.refCount = 1;
        CHECK(Is(RunAll(t, q0), "ship-target", "follow", 120, "follow"));
        t.step.verb = Verb::Escort;
        CHECK(Is(RunAll(t, q0), "ship-target", "formation", 120, "formation"));
        CHECK(!Rule_Escort(Assess(t), q0)); // (a ship target: not the marks' escort)
        Queue f;
        f.sub = "formation"; // formation already asked: follow (the old path's fall-through)
        CHECK(Is(RunAll(t, f), "ship-target", "follow", 120, "follow"));
        Snapshot sol = Base(); // a solar target: no ship rule (S5's arrival)
        Add(sol, 121, false, 2000);
        sol.step.refs[0] = 6, sol.step.refObj[0] = 121, sol.step.refCount = 1;
        CHECK(!Rule_ShipTarget(Assess(sol), q0) && Is(RunAll(sol, q0), "arrive", "travel", 121, "arrive"));
    }

    // ================================================================ S5
    const auto Ref = [](Snapshot& s, u32 id)
    {
        s.step.refs[0] = 9, s.step.refObj[0] = id, s.step.refCount = 1;
    };
    // ---- nothing else: rest - the rule that always holds
    {
        CHECK(Is(RunAll(Base(), q0), "rest", "hold", 0, "rest"));
        Snapshot s = Base(); // the survival stance with a lead (the ship the objective last pointed at)
        s.title = 22602, s.survivalStage = false;
        Add(s, 200, true, 1000, 0.5f);
        Queue q;
        q.escortShip = 200;
        CHECK(Is(RunAll(s, q), "rest", "hold", 0, "survive-hold")); // beside it within follow_distance
        s.contacts[0].distance = 2500;
        CHECK(Is(RunAll(s, q), "rest", "escort", 200, "survive-stay"));
        s.followDistance = 3000;
        CHECK(Is(RunAll(s, q), "rest", "hold", 0, "survive-hold"));
        s.contacts[0].distance = 7000; // beyond the lead's 6 km: no lead
        CHECK(Is(RunAll(s, q), "rest", "hold", 0, "rest"));
        s.contacts[0].distance = 1000, s.title = 0; // no stance
        CHECK(Is(RunAll(s, q), "rest", "hold", 0, "rest"));
    }
    // ---- Rule_Lane: a Dock step at a lane ring
    {
        Snapshot s = Base();
        s.step.verb = Verb::Dock;
        Add(s, 140, false, 5000).type = kRing;
        Ref(s, 140);
        s.lanes.push_back({140, 141, 142, 1, -1});
        CHECK(Is(RunAll(s, q0), "lane", "travel", 140, "lane")); // far: a trip first
        Queue arr;
        arr.arrived = true;
        CHECK(Is(RunAll(s, arr), "lane", "dock", 140, "lane")); // (arrived beside it: the ride)
        s.contacts[0].distance = 2000;
        CHECK(Is(RunAll(s, q0), "lane", "dock", 140, "lane"));
        s.lanes[0].openNext = 0, s.lanes[0].openPrev = 0; // closed both ways: wait at it
        CHECK(Is(RunAll(s, q0), "lane", "travel", 140, "lane-wait"));
        CHECK(Is(RunAll(s, arr), "lane", "hold", 0, "lane-wait"));
        s.contacts[0].distance = 1000;
        CHECK(Is(RunAll(s, q0), "lane", "hold", 0, "lane-wait"));
        s.lanes[0].openNext = -1, s.lanes[0].openPrev = -1; // unknown: the ride toward the next ring
        Proposal p = RunAll(s, q0);
        CHECK(Is(p, "lane", "dock", 140, "lane") && p.detail.find("toward 141") != std::string::npos);
        s.lanes[0].next = 0, s.lanes[0].openPrev = 0; // (no next ring; the previous one closed: known, closed)
        CHECK(Is(RunAll(s, q0), "lane", "hold", 0, "lane-wait"));
        Snapshot nd = s; // not a dock step
        nd.step.verb = Verb::None;
        CHECK(!Rule_Lane(Assess(nd), q0));
        Snapshot nl = s; // no lane links: an arrival
        nl.lanes.clear();
        CHECK(!Rule_Lane(Assess(nl), q0) && Is(RunAll(nl, q0), "arrive", "travel", 140, "dock"));
    }
    // ---- Rule_MissionForm: a trip with the leader the mission named
    {
        Snapshot s = Base();
        Add(s, 150, false, 8000);
        Ref(s, 150);
        Add(s, 151, true, 500, 0.5f);
        Queue q;
        q.missionLeader = 151;
        CHECK(Is(RunAll(s, q), "mission-form", "formation", 151, "mission-form"));
        CHECK(Is(RunAll(s, q0), "arrive", "travel", 150, "arrive")); // no leader
        Snapshot dead = s;
        dead.contacts[1].health = 0;
        CHECK(!Rule_MissionForm(Assess(dead), q) && RunAll(dead, q).rule == "arrive");
        Snapshot host = s;
        host.contacts[1].attitude = -1;
        CHECK(!Rule_MissionForm(Assess(host), q));
        q.arrived = true; // arrived: no trip objective
        CHECK(!Rule_MissionForm(Assess(s), q) && Is(RunAll(s, q), "arrived", "hold", 0, "rest"));
    }
    // ---- Rule_Pass: a race ring as the target
    {
        Snapshot s = Base();
        SnapContact& r = Add(s, 160, false, 4000);
        r.type = kSatellite, r.radius = 88;
        Ref(s, 160);
        CHECK(Is(RunAll(s, q0), "pass", "travel", 160, "pass"));
        Queue q;
        q.mode = "travel", q.trPassRing = 160; // the travel already flies through it
        CHECK(Is(RunAll(s, q), "pass", "travel", 160, "pass", true));
        s.step.verb = Verb::Survive;
        CHECK(Rule_Pass(Assess(s), q0).has_value());
        const auto no = [&](void (*f)(Snapshot&))
        {
            Snapshot x = s;
            f(x);
            return !Rule_Pass(Assess(x), q0);
        };
        CHECK(no([](Snapshot& x) { x.contacts[0].radius = 2.4f; }));  // a buoy
        CHECK(no([](Snapshot& x) { x.contacts[0].radius = 900; }));   // bigger than a ring
        CHECK(no([](Snapshot& x) { x.contacts[0].type = kSatellite | kRing; }));
        CHECK(no([](Snapshot& x) { x.contacts[0].type = 0; }));       // no satellite
        CHECK(no([](Snapshot& x) { x.contacts[0].dock = true; }));
        CHECK(no([](Snapshot& x) { x.lanes.push_back({160, 161, 0, 1, -1}); })); // lane links
        CHECK(no([](Snapshot& x) { x.contacts[0].attKnown = true, x.contacts[0].att = -1; })); // hostile
        CHECK(no([](Snapshot& x) { x.step.verb = Verb::Dock; }));
        Snapshot buoy = s;
        buoy.step.verb = Verb::None, buoy.contacts[0].radius = 2.4f;
        CHECK(Is(RunAll(buoy, q0), "arrive", "travel", 160, "arrive"));
    }
    // ---- Rule_RouteLane / Rule_Route: a route objective (the step = the last entry's text)
    {
        Snapshot s = Base();
        s.step.textId = 23130;
        Add(s, 170, false, 5000).type = kRing;
        Ref(s, 170);
        SnapStep e1, e2;
        e1.textId = e2.textId = 23130;
        e1.refs[0] = 10, e1.refObj[0] = 180, e1.refCount = 1, e1.refPlaced = true, e1.refPos[0] = 20000;
        e2.refs[0] = 11, e2.refObj[0] = 190, e2.refCount = 1, e2.refPlaced = true, e2.refPos[0] = 40000;
        s.entries = {s.step, e1, e2};
        SnapLane l = {170, 171, 172, -1, -1};
        l.nextPlaced = l.prevPlaced = true, l.nextPos[0] = 15000, l.prevPos[0] = -5000;
        s.lanes = {l};
        Proposal p = RunAll(s, q0);
        CHECK(Is(p, "route-lane", "travel", 170, "lane") && p.detail.find("toward 171") != std::string::npos);
        Queue arr;
        arr.arrived = true; // (arrived beside the ring: the ride - before the arrived rest)
        CHECK(Is(RunAll(s, arr), "route-lane", "dock", 170, "lane"));
        Snapshot nav = s; // step 1 by its navmarker behind us: toward the previous ring
        nav.entries[1].refCount = 0, nav.entries[1].refObj[0] = 0, nav.entries[1].hasNav = true;
        nav.entries[1].nav[0] = -9000;
        p = RunAll(nav, q0);
        CHECK(Is(p, "route-lane", "travel", 170, "lane") && p.detail.find("toward 172") != std::string::npos);
        Snapshot unp = s; // step 1's object not placed: no ride - the route to its last entry
        unp.entries[1].refPlaced = false;
        CHECK(!Rule_RouteLane(Assess(unp), q0) && Is(RunAll(unp, q0), "route", "travel", 190, "route"));
        unp.lanes[0].nextPlaced = unp.lanes[0].prevPlaced = false, unp.entries[1].refPlaced = true; // (no neighbour placed)
        CHECK(!Rule_RouteLane(Assess(unp), q0));
        CHECK(Is(RunAll(unp, arr), "arrived", "hold", 0, "rest")); // (arrived: rest before the route)
        Snapshot nav2 = unp; // the last entry by its navmarker
        nav2.entries[2].refCount = 0, nav2.entries[2].refObj[0] = 0, nav2.entries[2].hasNav = true;
        CHECK(Is(RunAll(nav2, q0), "route", "travel", 0, "route"));
        nav2.entries[2].hasNav = false; // nothing to fly to: the arrival at the target
        CHECK(!Rule_Route(Assess(nav2), q0) && Is(RunAll(nav2, q0), "arrive", "travel", 170, "arrive"));
        Snapshot other = s; // the step is not the route's last text
        other.entries[2].textId = 23131;
        CHECK(!Rule_RouteLane(Assess(other), q0) && !Rule_Route(Assess(other), q0));
        Snapshot one = s; // one entry: no route
        one.entries.resize(1);
        CHECK(!Rule_Route(Assess(one), q0));
    }
    // ---- Rule_Waypoints / Rule_Arrive
    {
        Snapshot s = Base();
        Add(s, 220, false, 9000);
        Ref(s, 220);
        s.waypoints = {{{8000, 0, 0}, 1, 0}, {{20000, 0, 0}, 1, 0}};
        CHECK(Is(RunAll(s, q0), "waypoints", "travel", 220, "route")); // an entry within 1.5 km of the target
        s.waypoints = {{{30000, 0, 0}, 1, 0}, {{9000, 0, 0}, 1, 0}}; // only the last one: the arrival
        CHECK(!Rule_Waypoints(Assess(s), q0) && Is(RunAll(s, q0), "arrive", "travel", 220, "arrive"));
        s.waypoints[0].object = 220; // the entry is the target object itself
        CHECK(Rule_Waypoints(Assess(s), q0).has_value());
        s.waypoints.clear();
        s.contacts[0].dockRing = 221; // a planet: its docking ring
        CHECK(Is(RunAll(s, q0), "arrive", "travel", 221, "arrive"));
        s.step.verb = Verb::Dock;
        CHECK(Is(RunAll(s, q0), "arrive", "travel", 221, "dock"));
        s.step.verb = Verb::None, s.contacts[0].dockRing = 0, s.contacts[0].type = kAirlock; // an airlock: through it
        CHECK(Is(RunAll(s, q0), "arrive", "travel", 220, "dock"));
        CHECK(!Rule_Arrive(Assess(Base()), q0));
    }
    // ---- the navmarker: through a way it sits on, a via point on a retry, else the trip to it
    {
        Snapshot s = Base();
        s.step.hasNav = true, s.step.nav[0] = 5000;
        CHECK(Is(RunAll(s, q0), "navmarker", "travel", 0, "navmarker"));
        Add(s, 240, false, 5100).type = kAirlock;
        CHECK(Is(RunAll(s, q0), "nav-way", "travel", 240, "dock"));
        s.contacts[0].pos[0] = 5300, s.contacts[0].distance = 5300; // 300 m off the marker: not on it
        CHECK(!Rule_NavWay(Assess(s), q0));
        s.contacts[0].pos[0] = 5100, s.contacts[0].distance = 5100;
        Add(s, 241, false, 4950).type = kJumpGate; // the nearer of two - hostile: none (not the next one)
        s.contacts[1].attKnown = true, s.contacts[1].att = -1;
        CHECK(!Rule_NavWay(Assess(s), q0));
        s.contacts[1].attKnown = false;
        CHECK(Is(RunAll(s, q0), "nav-way", "travel", 241, "dock"));
        s.contacts.clear();
        Add(s, 242, true, 5000).type = kAirlock; // (a ship is never the way)
        CHECK(!Rule_NavWay(Assess(s), q0));
        Queue q;
        q.viaNext = true, q.arriveRetries = 1;
        CHECK(Is(RunAll(s, q), "via", "travel", 0, "via"));
        q.arriveRetries = 0;
        CHECK(!Rule_Via(Assess(s), q));
        q.arriveRetries = 2;
        s.step.navSystem = 9, s.pilotSystem = 8; // the marker in another system
        CHECK(!Rule_Via(Assess(s), q) && RunAll(s, q).rule == "navmarker");
        s.pilotSystem = 9;
        CHECK(Rule_Via(Assess(s), q).has_value());
        s.pilotOk = false;
        CHECK(!Rule_Via(Assess(s), q));
        Snapshot ld = Base(); // a navmarker: no rest beside the survival lead
        ld.step.hasNav = true, ld.title = 22602, ld.survivalStage = false;
        Add(ld, 200, true, 1000, 0.5f);
        Queue lq;
        lq.escortShip = 200;
        CHECK(!Rule_Rest(Assess(ld), lq)->target && Rule_Rest(Assess(ld), lq)->action == "rest");
        lq.arrived = true; // (arrived: the stance rests beside its lead)
        CHECK(Is(RunAll(ld, lq), "arrived", "hold", 0, "rest"));
        ld.step.hasNav = false;
        CHECK(Is(RunAll(ld, lq), "arrived", "hold", 0, "survive-hold"));
    }
    // ---- the S5 order: lane before the arrival, the route's ride before the arrived rest, the waypoints before the
    // arrival, the marker's way before its via point
    {
        Snapshot s = Base();
        s.step.verb = Verb::Dock;
        Add(s, 140, false, 5000).type = kRing;
        Ref(s, 140);
        s.lanes.push_back({140, 141, 0, 1, -1});
        CHECK(RunAll(s, q0).rule == "lane" && Rule_Arrive(Assess(s), q0).has_value());
        Snapshot w = Base();
        Add(w, 220, false, 9000);
        Ref(w, 220);
        w.waypoints = {{{9000, 0, 0}, 1, 220}, {{20000, 0, 0}, 1, 0}};
        CHECK(RunAll(w, q0).rule == "waypoints" && Rule_Arrive(Assess(w), q0).has_value());
        Snapshot n = Base();
        n.step.hasNav = true, n.step.nav[0] = 5000;
        Add(n, 240, false, 5100).type = kAirlock;
        Queue q;
        q.viaNext = true, q.arriveRetries = 1;
        CHECK(RunAll(n, q).rule == "nav-way" && Rule_Via(Assess(n), q) && Rule_Navmarker(Assess(n), q));
    }

    // ---- the recorded fixtures: every one decides; one with the old path's decision gets exactly that one
    int n = 0, decided = 0, agree = 0;
    for (const std::string& dir : {g_test.fixtures + "\\situation", g_test.snaps})
        for (const SnapFixture& f : dir.empty() ? std::vector<SnapFixture>() : SnapFixtures(dir))
        {
            Snapshot x;
            if (!Parse(f.text, x))
                continue; // (situation_io reports it)
            const Situation sit = Assess(x);
            const Proposal p = Decide(sit, x.queue);
            ++n;
            CHECK(p.rule != "none"); // (S5: every exit of ObjectiveDecide is ported - the rest rule always holds)
            if (g_test.verbose)
                printf("  %s: %s%s%s\n", f.name.c_str(), ProposalText(p).c_str(), p.detail.empty() ? "" : " why=",
                       p.detail.c_str());
            // (the S1 / S2 fixtures, recorded with the autopilot off: each step is a later slice's - M07's lane ride and M03's
            // Willard target S5, M12's mark fight S4, docked / no objective S5; no S3 branch holds there)
            if (x.version < 3)
            {
                const Proposal p3 = DecideS3(sit, x.queue);
                CHECK(p3.rule == "none");
                if (p3.rule != "none")
                    fprintf(stderr, "  situation_rules: %s: %s\n", f.name.c_str(), ProposalText(p3).c_str());
            }
            // (S4: M12's "Awaiting" with the Nomads marked within 6 km is the mark fight - the threat fight's combat)
            if (f.name == "m12_hole_s182.snap")
                CHECK(p.rule == "threat-fight" && p.mode == "combat" && p.target == 0 && p.action == "fight");
            if (x.decided.rule.empty())
                continue;
            ++decided;
            Proposal want;
            want.rule = x.decided.rule, want.mode = x.decided.mode, want.target = x.decided.target;
            want.action = x.decided.action, want.keep = x.decided.keep;
            const bool same = p.rule == want.rule && p.mode == want.mode && p.target == want.target &&
                              p.action == want.action && p.keep == want.keep;
            CHECK(same);
            if (!same)
                fprintf(stderr, "  situation_rules: %s: rules %s, the old path %s\n", f.name.c_str(), ProposalText(p).c_str(),
                        ProposalText(want).c_str());
            agree += same ? 1 : 0;
        }
    // (A37 S3's in-game check: m07_gate_jump_s207.snap, the jump; S4's: 7 more - threat-fight x4 (M12 mark fight, M03
    // convoy escort, M03 Valkyries, M13 battleship), escort (M03 convoy), mark-fight (M13 huge static), pre-stage-next (M07);
    // S5's: 10 more - lane + rest (M07 gate), arrive + arrived (M03 California Minor), arrive (M03 Willard), navmarker +
    // nav-way (M12 core), rest (M03 free roam) and S3's first airlock + tractor-exit in game (M12 core))
    CHECK(decided >= 18);
    printf("  %d recorded snapshots decided; %d carry the old path's decision, %d of them agree\n", n, decided, agree);
}
