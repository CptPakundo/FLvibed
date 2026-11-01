// The arbiter's executor, its pure half (controller\src\impl\situation_exec.h, A37_PLAN S7): every refusal of the
// side-effect gate (side effects check the game first) and its event line; the plan comparison; the ObjectiveStart
// arguments of a proposal = the old path's for every recorded decision (the fixtures' `decided`), each plan's kind fit
// for its mode; the pause facts; D10's pause / resume over every fixture - the queue's carried memory (a short pause:
// silent, the same step) decides exactly as the recorded memory (cold = warm with the queue carrying it), a long pause
// (ask: re-planned, the story memory kept) and a load (no paused queue: cold, the story memory lost - S6's last_real
// finding, decided for S7) - and the arrival reach that settles S6's `arrived` exception.
#include "situation_fixture.h"

#include "../controller/src/impl/situation_exec.h"

#include <map>

using namespace situation;
using goalqueue::GoalQueue;
using goalqueue::PauseFacts;
using goalqueue::ResumeChoice;
using goalqueue::ResumePolicy;

namespace
{
Plan Trip(u32 object, bool noDock)
{
    Plan p;
    p.kind = Plan::Kind::Travel, p.object = object, p.noDock = noDock;
    return p;
}
Plan Of(Plan::Kind k, u32 object)
{
    Plan p;
    p.kind = k, p.object = object;
    return p;
}
bool Same(const Proposal& a, const Proposal& b)
{
    return a.rule == b.rule && a.mode == b.mode && a.target == b.target && a.action == b.action && a.keep == b.keep;
}
// a plan fit for its proposal's mode (the executor starts the plan, ObjectiveStart records the mode)
bool Fits(const Proposal& p)
{
    using K = Plan::Kind;
    const K k = p.plan.kind;
    if (p.keep || p.rule == "held")
        return k == K::None;
    if (p.mode == "travel")
        return k == K::Travel || k == K::Pass || k == K::Waypoints;
    if (p.mode == "dock")
        return k == K::Dock && p.plan.object == p.target;
    if (p.mode == "shoot")
        return k == K::Shoot && p.plan.object == p.target;
    if (p.mode == "hold")
        return (k == K::Mode && p.plan.object == 0) || (k == K::HoldAt && p.plan.hasPoint);
    return k == K::Mode && p.plan.object == p.target; // combat / escort / formation / follow
}
// the rules' Queue the executor's arbiter reads from a goal queue (situation_snap.cpp Arbitrate: the leash anchor is the
// executor's own - S6 open (1))
situation::Queue RulesQueue(const GoalQueue& gq, const goalqueue::ExecMemory& ex, const Situation& sit,
                            const situation::Queue& exec)
{
    situation::Queue q = goalqueue::ToRulesQueue(gq, ex, sit);
    q.leashShip = exec.leashShip, q.leashDist = exec.leashDist;
    return q;
}
} // namespace

void Test_situation_exec()
{
    // ---- the side-effect gate: each refusal
    {
        GateFacts f; // in space, nothing running
        const Plan dockTrip = Trip(30, false), trip = Trip(30, true), ride = Of(Plan::Kind::Dock, 40);
        Plan form = Of(Plan::Kind::Mode, 10);
        CHECK(CheckSideEffect(f, dockTrip, "travel").verdict == GateVerdict::Go);
        CHECK(CheckSideEffect(f, Plan(), "travel").verdict == GateVerdict::Go && CheckSideEffect(f, Plan(), "travel").kind.empty());
        // not in space / a lane ride / a jump: nothing at all
        GateFacts x = f;
        x.inSpace = false;
        CHECK(CheckSideEffect(x, trip, "travel").verdict == GateVerdict::Refuse);
        CHECK(CheckSideEffect(x, form, "hold").why == "not in space");
        x = f, x.lane = true;
        CHECK(CheckSideEffect(x, ride, "dock").verdict == GateVerdict::Refuse && CheckSideEffect(x, ride, "dock").why == "riding a lane");
        CHECK(CheckSideEffect(x, form, "combat").verdict == GateVerdict::Refuse); // (no start breaks the ride)
        x = f, x.jumping = true;
        CHECK(CheckSideEffect(x, dockTrip, "travel").why == "jumping");
        // the game docks us: no dock request, but a plain trip / fight is the decision's (the supervisor holds anyway)
        x = f, x.gameDocks = true;
        CHECK(CheckSideEffect(x, dockTrip, "travel").why == "the game docks us");
        CHECK(CheckSideEffect(x, ride, "dock").verdict == GateVerdict::Refuse);
        CHECK(CheckSideEffect(x, trip, "travel").verdict == GateVerdict::Go);
        // the game's dock to the same object already runs: keep while our mode runs, refused with nothing of ours
        x = f, x.dockInProgress = true, x.dockPortObject = 30, x.mode = "travel", x.target = 30;
        CHECK(CheckSideEffect(x, dockTrip, "travel").verdict == GateVerdict::Keep);
        CHECK(CheckSideEffect(x, dockTrip, "travel").why == "the game's dock to it runs");
        x.mode = "off";
        CHECK(CheckSideEffect(x, dockTrip, "travel").verdict == GateVerdict::Refuse);
        x.dockPortObject = 31, x.mode = "travel"; // (another object's port: a new request is the decision's)
        CHECK(CheckSideEffect(x, dockTrip, "travel").verdict == GateVerdict::Go);
        // our own dock to it runs: keep (no second request); our request to another awaits its answer: refused
        x = f, x.mode = "dock", x.target = 40;
        CHECK(CheckSideEffect(x, ride, "dock").verdict == GateVerdict::Keep && CheckSideEffect(x, ride, "dock").why == "our dock to it runs");
        x.dockReplyPending = true, x.target = 41;
        CHECK(CheckSideEffect(x, ride, "dock").why == "a dock reply is pending");
        CHECK(CheckSideEffect(x, dockTrip, "travel").verdict == GateVerdict::Refuse);
        CHECK(CheckSideEffect(x, trip, "travel").verdict == GateVerdict::Go); // (no dock at its end)
        // a formation request while the game already has us under that leader: adopted; another leader / ours: go
        x = f, x.leader = 10;
        CHECK(CheckSideEffect(x, form, "formation").verdict == GateVerdict::Adopt);
        CHECK(CheckSideEffect(x, form, "follow").verdict == GateVerdict::Go);
        x.mode = "formation", x.target = 10;
        CHECK(CheckSideEffect(x, form, "formation").verdict == GateVerdict::Go); // (StartOrKeep keeps our own)
        x = f, x.leader = 11;
        CHECK(CheckSideEffect(x, form, "formation").verdict == GateVerdict::Go);
        // the kinds and the event line (the fuzz oracle's)
        CHECK(SideEffectKind(dockTrip, "travel") == "dock-trip" && SideEffectKind(trip, "travel") == "travel");
        CHECK(SideEffectKind(ride, "dock") == "dock" && SideEffectKind(form, "escort") == "escort");
        CHECK(SideEffectKind(Of(Plan::Kind::HoldAt, 0), "hold") == "hold" && SideEffectKind(Of(Plan::Kind::Pass, 5), "travel") == "pass");
        CHECK(SideEffectKind(Of(Plan::Kind::Shoot, 5), "shoot") == "shoot" && SideEffectKind(Of(Plan::Kind::Waypoints, 0), "travel") == "waypoints");
        x = f, x.lane = true;
        CHECK(SideEffectLine(CheckSideEffect(x, ride, "dock"), 40, 0x1234abcd, "lane") ==
              "side-effect dock target=40 step=0x1234abcd rule=lane refused why=riding a lane");
        CHECK(SideEffectLine(CheckSideEffect(f, trip, "travel"), 30, 7, "arrive") == "side-effect travel target=30 step=0x00000007 rule=arrive");
    }

    // ---- the plan comparison
    {
        Plan a = Trip(0, true);
        a.hasPoint = true, a.point[0] = 1000;
        Plan b = a;
        CHECK(PlanDiff(a, b).empty());
        b.point[0] = 1000.5f; // (within 1 m)
        CHECK(PlanDiff(a, b).empty());
        b.point[0] = 1037;
        CHECK(PlanDiff(a, b) == "point 37 m");
        b = a, b.noDock = false;
        CHECK(PlanDiff(a, b) == "dock");
        b = a, b.fly = true, b.flyDir[0] = 1, b.flyRadius = 500;
        CHECK(PlanDiff(a, b) == "fly given/not");
        a.fly = true, a.flyDir[0] = 1, a.flyRadius = 500;
        CHECK(PlanDiff(a, b).empty());
        a.flyAhead = 350;
        CHECK(PlanDiff(a, b) == "fly");
        CHECK(PlanDiff(a, Of(Plan::Kind::HoldAt, 0)) == "kind travel/hold-at");
        CHECK(PlanDiff(Of(Plan::Kind::Dock, 4), Of(Plan::Kind::Dock, 5)) == "object 4/5");
        CHECK(PlanText(Trip(9, false)) == "travel object=9 dock=1");
    }

    // ---- the decision's ObjectiveStart
    {
        Proposal held;
        held.rule = "held", held.keep = true;
        CHECK(!StartArgsOf(held).objectiveStart);
        Proposal leashKeep;
        leashKeep.rule = "leash", leashKeep.keep = true, leashKeep.mode = "travel", leashKeep.action = "leash";
        CHECK(!StartArgsOf(leashKeep).objectiveStart); // (the trip back runs: the old exit calls no ObjectiveStart)
        Proposal loot;
        loot.rule = "marked-loot", loot.keep = true, loot.mode = "shoot", loot.target = 77, loot.action = "fight";
        const StartArgs a = StartArgsOf(loot);
        CHECK(a.objectiveStart && a.mode == "shoot" && a.target == 77 && a.action == "fight");
    }

    // ---- the pause facts
    {
        Snapshot s;
        s.pilotOk = true, s.pilotSystem = 100, s.system = 100, s.pos[0] = 3000;
        s.step.textId = 23100;
        const Situation sit = Assess(s);
        PauseMark m;
        m.atMs = 50000, m.system = 100, m.posKnown = true, m.stepKey = sit.stepKey;
        PauseFacts f = PauseFactsOf(m, sit, 62000);
        CHECK_NEAR(f.pausedMs, 12000, 0.5);
        CHECK_NEAR(f.movedM, 3000, 0.5);
        CHECK(!f.systemChanged && !f.docked && !f.jobTaken);
        CHECK(PauseFactsOf(m, sit, 1000).pausedMs > 1e9); // (the clock went back: a load - long)
        PauseMark o = m;
        o.system = 200;
        f = PauseFactsOf(o, sit, 62000);
        CHECK(f.systemChanged && f.movedM == 0);
        o = m, o.docked = true;
        CHECK(PauseFactsOf(o, sit, 62000).docked);
        Snapshot docked = s;
        docked.pilotOk = false;
        CHECK(PauseFactsOf(m, Assess(docked), 62000).docked);
        Snapshot job = s;
        job.step.textId = 327739, job.step.job = "guide_to_main_battle";
        CHECK(PauseFactsOf(m, Assess(job), 62000).jobTaken);
    }

    // ---- over every recorded fixture: the ObjectiveStart arguments, the plans, pause / resume, a load
    // Expected per fixture: the cold (load) verdict - "agree", or "finding:last_real" (S6's finding, decided for S7: the
    // load loses the story memory). The short pause (silent) and the long one (ask) must agree everywhere (the queue
    // carries the entry memory through a short pause, the story memory through both).
    const std::map<std::string, std::string> coldExpect = {
        // FINDING (decided, A37_PLAN S7): "Awaiting" after the Tau-31 lane ride - with the queue the pause keeps lastReal
        // (warm = paused), a load forgets it and the mark fallback follows Juni (ship-target follow 16)
        {"m07_tau31_prestagenext_s208.snap", "finding:last_real"},
    };
    int fixtures = 0, decided = 0, mapped = 0, fit = 0, pointed = 0, travel0 = 0, silentAgree = 0, askAgree = 0,
        coldAgree = 0, sameStep = 0;
    std::string coldDiffer;
    for (const SnapFixture& fx : SnapFixtures(g_test.fixtures + "\\situation"))
    {
        Snapshot x;
        if (!situation::Parse(fx.text, x))
            continue;
        ++fixtures;
        const Situation sit = Assess(x);
        const Proposal warm = Decide(sit, x.queue);
        // the plan fits its mode; a trip with no target object goes to a point
        const bool fits = Fits(warm);
        CHECK(fits);
        fit += fits ? 1 : 0;
        if (warm.mode == "travel" && !warm.keep && warm.plan.kind == Plan::Kind::Travel && !warm.plan.object)
        {
            ++travel0;
            CHECK(warm.plan.hasPoint);
            pointed += warm.plan.hasPoint ? 1 : 0;
        }
        if (!fits)
            printf("  %-40s plan %s does not fit %s\n", fx.name.c_str(), PlanText(warm.plan).c_str(), ProposalText(warm).c_str());
        // Decide -> executor = the old ObjectiveStart arguments (the recorded decision, where the fixture has one)
        if (!x.decided.rule.empty())
        {
            ++decided;
            const StartArgs a = StartArgsOf(warm);
            const bool same = a.objectiveStart && warm.rule == x.decided.rule && a.mode == x.decided.mode &&
                              a.target == x.decided.target && a.action == x.decided.action && warm.keep == x.decided.keep;
            CHECK(same);
            mapped += same ? 1 : 0;
            if (!same || g_test.verbose)
                printf("  %-40s decided %s %s,%u,%s keep=%d | executor %s,%u,%s (%s)\n", fx.name.c_str(),
                       x.decided.rule.c_str(), x.decided.mode.c_str(), x.decided.target, x.decided.action.c_str(),
                       x.decided.keep ? 1 : 0, a.mode.c_str(), a.target, a.action.c_str(), PlanText(warm.plan).c_str());
        }
        // the pause: the executor's memory classed into the queue (as every decision leaves it), the stop's carry
        const goalqueue::Classed c = goalqueue::FromRulesQueue(sit, x.queue);
        GoalQueue paused = c.queue;
        goalqueue::CarryMemory(paused, x.queue, x.clock);
        PauseMark mark;
        mark.atMs = 100000, mark.system = x.pilotOk ? x.pilotSystem : x.system, mark.posKnown = x.pilotOk;
        memcpy(mark.pos, x.pos, sizeof(mark.pos));
        mark.stepKey = paused.stepKey;
        const ResumePolicy pol;
        // a short pause: silent, the same step, its memory back - the decision the recorded memory makes
        {
            GoalQueue q;
            const Restore r = ResumeOf(&paused, sit, PauseFactsOf(mark, sit, 105000), pol, q);
            const Proposal p = Decide(sit, RulesQueue(q, c.exec, sit, x.queue));
            const bool ok = Same(p, warm) && (r.choice != ResumeChoice::Silent || r.sameStep || !goalqueue::Head(q));
            CHECK(ok);
            silentAgree += ok ? 1 : 0;
            sameStep += r.sameStep ? 1 : 0;
            if (r.sameStep)
                CHECK(r.memory.arrived == x.queue.arrived && r.memory.escortShip == x.queue.escortShip &&
                      r.lastReal == x.queue.lastReal && r.lastDock == x.queue.lastDock);
            if (!ok || g_test.verbose)
                printf("  %-40s silent %s (%s why=%s) vs warm %s\n", fx.name.c_str(), ProposalText(p).c_str(),
                       goalqueue::ResumeChoiceName(r.choice), r.why.c_str(), ProposalText(warm).c_str());
        }
        // a long pause: ask - re-planned, the story memory (lastReal, the carried dock) kept
        {
            GoalQueue q;
            const Restore r = ResumeOf(&paused, sit, PauseFactsOf(mark, sit, 100000 + 600000), pol, q);
            CHECK(r.choice != ResumeChoice::Silent && !r.sameStep);
            CHECK(r.lastReal == x.queue.lastReal);
            const Proposal p = Decide(sit, RulesQueue(q, c.exec, sit, x.queue));
            const bool ok = Same(p, warm);
            askAgree += ok ? 1 : 0;
            if (!ok || g_test.verbose)
                printf("  %-40s ask %s vs warm %s\n", fx.name.c_str(), ProposalText(p).c_str(), ProposalText(warm).c_str());
            CHECK(ok);
        }
        // a load: no paused queue - cold (the situation alone; the step's own arrival test)
        {
            GoalQueue q;
            const Restore r = ResumeOf(nullptr, sit, PauseFacts(), pol, q);
            CHECK(r.cold && r.lastReal == 0);
            const Proposal p = Decide(sit, RulesQueue(q, c.exec, sit, x.queue));
            std::string verdict = "agree";
            if (!Same(p, warm))
            {
                // which memory: lastReal put back alone
                situation::Queue t = RulesQueue(q, c.exec, sit, x.queue);
                t.lastReal = x.queue.lastReal;
                verdict = Same(Decide(sit, t), warm) ? "finding:last_real" : "differ";
                coldDiffer += " " + fx.name;
            }
            else
                ++coldAgree;
            const auto it = coldExpect.find(fx.name);
            const std::string want = it == coldExpect.end() ? "agree" : it->second;
            CHECK(verdict == want);
            if (verdict != want || g_test.verbose)
                printf("  %-40s cold %s: %s vs warm %s (expected %s)\n", fx.name.c_str(), verdict.c_str(),
                       ProposalText(p).c_str(), ProposalText(warm).c_str(), want.c_str());
            if (fx.name == "m07_tau31_prestagenext_s208.snap")
                CHECK(p.rule == "ship-target" && p.mode == "follow" && warm.rule == "pre-stage-next");
            // S6's `arrived` exception settled: the step's own arrival test (503 m from California Minor's ring)
            if (fx.name == "m03_california_arrived_s009.snap")
                CHECK(goalqueue::WithinArrivalReach(sit) && r.memory.arrived && p.rule == "arrived");
        }
    }
    CHECK(fixtures >= 25 && decided >= 18);
    CHECK(mapped == decided && fit == fixtures && pointed == travel0);
    CHECK(silentAgree == fixtures && askAgree == fixtures);
    printf("  executor over %d fixtures: ObjectiveStart = the old arguments %d / %d decided, plans fit %d / %d (%d point "
           "trips with a point); pause -> resume: silent agrees %d / %d (%d the same step), ask %d / %d; a load (cold) "
           "agrees %d / %d, differs:%s\n",
           fixtures, mapped, decided, fit, fixtures, pointed, silentAgree, fixtures, sameStep, askAgree, fixtures, coldAgree,
           fixtures, coldDiffer.empty() ? " none" : coldDiffer.c_str());

    // ---- the rules' own plans (the differ path's geometry, as the old exits compute it): the via point, the far mark's
    // trip 1.5 km short, the leash's trip to its ship, the navmarker's point
    {
        Snapshot s;
        s.pilotOk = true, s.system = 100, s.pilotSystem = 100;
        s.step.textId = 24230, s.step.hasNav = true, s.step.nav[0] = 1000;
        situation::Queue q;
        q.viaNext = true, q.arriveRetries = 1; // (k = 0: up)
        const std::optional<Proposal> via = Rule_Via(Assess(s), q);
        CHECK(via && via->plan.kind == Plan::Kind::Travel && via->plan.hasPoint && via->plan.inVia && via->plan.system == 100);
        if (via)
        {
            CHECK_NEAR(via->plan.point[0], 500, 0.01);
            CHECK_NEAR(via->plan.point[1], 500, 0.01);
            CHECK_NEAR(via->plan.point[2], 0, 0.01);
        }
        q.arriveRetries = 4; // (k = 3: the side, the other way)
        const std::optional<Proposal> via3 = Rule_Via(Assess(s), q);
        CHECK(via3 && std::fabs(via3->plan.point[1]) < 0.01f && std::fabs(std::fabs(via3->plan.point[2]) - 500) < 0.01f);
        const std::optional<Proposal> nav = Rule_Navmarker(Assess(s), situation::Queue());
        CHECK(nav && nav->plan.hasPoint && nav->plan.point[0] == 1000 && !nav->plan.fly);
        // a fight step's mark 10 km off: the trip to 1.5 km short of it
        Snapshot f;
        f.pilotOk = true, f.system = 100, f.step.textId = 22520, f.step.verb = Verb::Fight;
        SnapContact m;
        m.id = 70, m.ship = true, m.attitude = -1, m.attitudeAsked = true, m.health = 1, m.distance = 10000, m.pos[0] = 10000;
        f.contacts.push_back(m);
        f.marks = {70};
        const std::optional<Proposal> mf = Rule_MarkFight(Assess(f), situation::Queue());
        CHECK(mf && mf->mode == "travel" && mf->plan.hasPoint && mf->plan.travelObjective == 1);
        if (mf)
            CHECK_NEAR(mf->plan.point[0], 8500, 0.5);
        // the leash's ship 2 km off a 1 km leash: back toward where it is now
        Snapshot l;
        l.pilotOk = true, l.system = 100, l.step.textId = 21660;
        SnapContact ship;
        ship.id = 10, ship.ship = true, ship.attitude = 0.8f, ship.attitudeAsked = true, ship.health = 1, ship.distance = 2000;
        ship.pos[2] = 2000;
        l.contacts.push_back(ship);
        situation::Queue lq;
        lq.leashShip = 10, lq.leashDist = 1000;
        const std::optional<Proposal> le = Rule_Leash(Assess(l), lq);
        CHECK(le && le->plan.kind == Plan::Kind::Travel && le->plan.hasPoint && le->plan.point[2] == 2000 && !le->plan.object);
    }

    // ---- the arrival reach (synthetic): a non-dock step's target within 1 km; a navmarker within 600 m (not on a way through)
    {
        Snapshot s;
        s.pilotOk = true, s.system = 100;
        s.step.textId = 22525, s.step.refCount = 1, s.step.refObj[0] = 50;
        SnapContact st;
        st.id = 50, st.pos[0] = 900, st.distance = 900, st.dock = true;
        s.contacts.push_back(st);
        CHECK(goalqueue::WithinArrivalReach(Assess(s)));
        s.contacts[0].pos[0] = 1500;
        CHECK(!goalqueue::WithinArrivalReach(Assess(s)));
        s.contacts[0].pos[0] = 900, s.step.verb = Verb::Dock; // (a dock step docks: never "arrived")
        CHECK(!goalqueue::WithinArrivalReach(Assess(s)));
        s.step.verb = Verb::None, s.contacts[0].ship = true; // (a ship is followed, not arrived at)
        CHECK(!goalqueue::WithinArrivalReach(Assess(s)));
        Snapshot n;
        n.pilotOk = true, n.system = 100;
        n.step.textId = 24230, n.step.hasNav = true, n.step.nav[2] = 500;
        CHECK(goalqueue::WithinArrivalReach(Assess(n)));
        SnapContact lock;
        lock.id = 60, lock.type = 1u << 10, lock.pos[2] = 450, lock.distance = 450;
        n.contacts.push_back(lock); // (the marker on an airlock: the nav-way's trip)
        CHECK(!goalqueue::WithinArrivalReach(Assess(n)));
        n.contacts.clear(), n.step.nav[2] = 800;
        CHECK(!goalqueue::WithinArrivalReach(Assess(n)));
        n.step.nav[2] = 500, n.pilotOk = false;
        CHECK(!goalqueue::WithinArrivalReach(Assess(n)));
    }
}
