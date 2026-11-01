// The goal queue (controller\src\impl\goalqueue.h, A37_PLAN S6): the A38 record's text round trip and every parse
// refusal; every validator refusal with its reason code; the re-proof states (valid / achieved / failed / blocked);
// achieve semantics (an achieved head popped before anything acts, its steps with it, repeats bounded); the step key
// written only by Advance after a decision (4c4fb6f's class); D10's resume cases; the memory classes cover the rules'
// Queue; and cold = warm over every recorded fixture: Decide(Assess(s), warm) against Decide(Assess(s), Rebuild(s))
// with the executor's own state the same - a difference is either declared memory (entry / mission scope: named per
// fixture below, with why) or a finding (memory the rebuild cannot derive: what A37 removes - recorded, not hidden).
#include "situation_fixture.h"

#include "../controller/src/impl/goalqueue.h"

#include <map>

using namespace goalqueue;
using situation::Assess;
using situation::Proposal;
using situation::Snapshot;
using situation::SnapContact;

namespace
{
constexpr u32 kRing = 1u << 7, kGate = 1u << 6;

// In space at the origin in system 100, knowledge "exposed"; contacts: 10 friendly ship 1 km, 20 hostile ship 2 km,
// 30 a station 5 km (dockable, base 3000), 40 a lane ring 8 km closed both ways, 50 a jump gate 9 km, 60 a dead ship.
Snapshot Space()
{
    Snapshot s;
    s.objSeq = 4;
    s.step.textId = 23100;
    s.system = 100, s.inSpace = true, s.pilotOk = true, s.op = 2, s.hull = 0.9f;
    s.knowledge = 0;
    const auto add = [&](u32 id, bool ship, float x, float att) -> SnapContact&
    {
        SnapContact c;
        c.id = id, c.ship = ship, c.distance = x, c.pos[0] = x, c.health = 1, c.maxHit = 1000;
        if (ship)
            c.attitude = att, c.attitudeAsked = true;
        else
            c.attKnown = true, c.att = att;
        s.contacts.push_back(c);
        return s.contacts.back();
    };
    add(10, true, 1000, 0.8f);
    add(20, true, 2000, -1);
    SnapContact& st = add(30, false, 5000, 0.2f);
    st.dock = true, st.base = 3000;
    add(40, false, 8000, 0.2f).type = kRing;
    add(50, false, 9000, 0.2f).type = kGate;
    add(60, true, 1500, -1).health = 0;
    s.lanes.push_back({40, 41, 42, 0, 0});
    return s;
}
GoalRecord Rec(Type t, u32 subject = 0)
{
    GoalRecord r;
    r.id = 1, r.type = t;
    if (subject)
        r.subject.kind = Ref::Kind::Object, r.subject.id = subject;
    return r;
}
Cnd C(CndKind k, u32 id = 0, float m = 0)
{
    Cnd c;
    c.kind = k, c.id = id, c.m = m;
    return c;
}
// A record with every field set away from its default
GoalRecord Full()
{
    GoalRecord r;
    r.id = 7, r.type = Type::Escort, r.state = State::Suspended, r.origin = Origin::Job, r.parent = 3, r.step = 2;
    r.dependsOn = {4, 5};
    r.subject.kind = Ref::Kind::Pos, r.subject.id = 9, r.subject.system = 2745692751u;
    r.subject.pos[0] = -1234.5f, r.subject.pos[1] = 0.125f, r.subject.pos[2] = 98765.25f;
    r.params.withinM = 150.5f, r.params.radiusM = 3000, r.params.count = 4, r.params.toward = 41;
    r.params.stepKind = objkinds::GoalType::Escort;
    r.when.push_back(C(CndKind::InSystem, 100));
    Cnd sg;
    sg.kind = CndKind::ScriptGoal, sg.name = "odd: name|with!seps, = and %";
    r.done.push_back(sg);
    Cnd neg = C(CndKind::DistPos, 0, 500);
    neg.negate = true, neg.pos[2] = -7.75f;
    r.done.push_back(neg);
    r.fail.push_back(C(CndKind::Destroyed, 10));
    r.fail.push_back(C(CndKind::HullBelow, 0, 0.25f));
    r.policy.knowledge = 1, r.policy.engage = Engage::Protect, r.policy.sideEffects = false;
    r.confirm = true, r.repeat = -1;
    r.sourceText = "escort the convoy to Planet Manhattan, please";
    r.display.title = 22455, r.display.detail = 22456;
    r.stepKey = 0xDEADBEEFu;
    EntryMemory& m = r.memory;
    m.escortShip = 15, m.badGoal = "bad goal", m.deadGoals = {"dead,one", "dead_two"}, m.arrived = true;
    m.arriveRetries = 3, m.viaNext = true, m.cruiseLock = true, m.stepAtKnown = true, m.stepAt = 1234.5678901234;
    m.scriptMarks = {606}, m.missionLeader = 11;
    return r;
}
std::string Replace(std::string s, const std::string& a, const std::string& b)
{
    const size_t i = s.find(a);
    if (i != std::string::npos)
        s.replace(i, a.size(), b);
    return s;
}
bool SameDecision(const Proposal& a, const Proposal& b)
{
    return a.rule == b.rule && a.mode == b.mode && a.target == b.target && a.action == b.action && a.keep == b.keep;
}
bool RouteYes(u32, u32) { return true; }
} // namespace

void Test_goalqueue()
{
    // ---- the record's text: a full record round-trips; every refusal fails closed
    {
        const GoalRecord r = Full();
        const std::string line = SerializeRecord(r);
        GoalRecord back;
        std::string err;
        CHECK(ParseRecord(line, back, &err));
        CHECK(SerializeRecord(back) == line);
        CHECK(back.dependsOn == r.dependsOn && back.done.size() == 2 && back.done[0].name == r.done[0].name);
        CHECK(back.done[1].negate && back.done[1].pos[2] == -7.75f && back.memory.stepAt == r.memory.stepAt);
        CHECK(back.memory.deadGoals == r.memory.deadGoals && back.sourceText == r.sourceText);
        CHECK(back.params.stepKind == objkinds::GoalType::Escort && back.policy.engage == Engage::Protect);
        GoalRecord x;
        CHECK(!ParseRecord(line + " extra=1", x, &err));                          // an unknown (extra) field
        CHECK(!ParseRecord(Replace(line, " confirm=1", ""), x, &err));            // a missing field
        CHECK(!ParseRecord(Replace(line, "confirm=", "confirmed="), x, &err));    // an unknown key
        CHECK(!ParseRecord(Replace(line, "type=escort", "type=teleport"), x, &err)); // an unknown type
        CHECK(!ParseRecord(Replace(line, "state=suspended", "state=sleeping"), x, &err));
        CHECK(!ParseRecord(Replace(line, "origin=job", "origin=radio"), x, &err));
        CHECK(!ParseRecord(Replace(line, "engage=protect", "engage=berserk"), x, &err));
        CHECK(!ParseRecord(Replace(line, "in_system:", "warp_drive:"), x, &err)); // an unknown condition kind
        CHECK(!ParseRecord(Replace(line, "step_kind=escort", "step_kind=dance"), x, &err));
        CHECK(!ParseRecord(Replace(line, "within=150.5", "within=far"), x, &err)); // a bad value
        CHECK(!ParseRecord(Replace(line, "subject=pos:", "subject=star:"), x, &err));
        CHECK(!ParseRecord(Replace(line, "m_arrived=1", "m_arrived=2"), x, &err));
        // two fields swapped: out of order
        CHECK(!ParseRecord(Replace(line, "title=22455 detail=22456", "detail=22456 title=22455"), x, &err));
        CHECK(!ParseRecord("goals id=1", x, &err));
        // the queue's text
        GoalQueue q;
        q.nextId = 9, q.stepKey = 77, q.story.lastReal = 23250, q.mission.survTitle = 22602, q.mission.survMission = true;
        q.mission.survSystem = 5, q.mission.survival = true;
        q.story.seenAlive = {606, 607}, q.story.seenListed = {606};
        q.entries = {Full(), Rec(Type::Hold)};
        const std::string text = Serialize(q);
        GoalQueue qb;
        CHECK(Parse(text, qb, &err));
        CHECK(Serialize(qb) == text && qb.entries.size() == 2 && qb.story.lastReal == 23250 && qb.mission.survival);
        CHECK(qb.story.seenAlive == q.story.seenAlive && qb.story.seenListed == q.story.seenListed);
        GoalQueue qe; // (empty lists round-trip too)
        CHECK(Parse(Serialize(qe), qb, &err) && qb.story.seenAlive.empty());
        CHECK(!Parse(Replace(text, "end\n", ""), qb, &err));                      // no end
        CHECK(!Parse(Replace(text, "goalqueue v2", "goalqueue v1"), qb, &err));   // another version
        CHECK(!Parse(Replace(text, " survival=1", ""), qb, &err));                // a missing header field
        CHECK(!Parse(Replace(text, "goal id=1 ", "goal id=7 "), qb, &err));       // an id repeated
        CHECK(!Parse(Replace(text, "goal id=1 ", "goal id=x "), qb, &err));       // a bad record
    }

    // ---- the validator: each refusal with its reason code
    {
        const Snapshot s0 = Space();
        const situation::Situation sit = Assess(s0);
        const auto reason = [&](const GoalRecord& r, const situation::Situation& at, const Env& env = Env())
        { return Validate(r, at, env).reason; };
        CHECK(reason(Rec(Type::FlyTo, 30), sit) == Reason::Ok);
        CHECK(reason(Rec(Type::Dock, 30), sit) == Reason::Ok);
        CHECK(reason(Rec(Type::Formation, 10), sit) == Reason::Ok);
        CHECK(reason(Rec(Type::Jump, 50), sit) == Reason::Ok);
        CHECK(reason(Rec(Type::Hold), sit) == Reason::Ok);
        GoalRecord v2 = Rec(Type::Hold);
        v2.version = kRecordVersion + 1;
        CHECK(reason(v2, sit) == Reason::BadVersion);
        GoalRecord ut = Rec(Type::Hold);
        ut.type = Type::COUNT;
        CHECK(reason(ut, sit) == Reason::UnknownType);
        GoalRecord uf = Rec(Type::Hold);
        uf.done.push_back(C(CndKind::COUNT));
        CHECK(reason(uf, sit) == Reason::UnknownField);
        GoalRecord us = Rec(Type::Hold);
        us.state = State::COUNT;
        CHECK(reason(us, sit) == Reason::UnknownField);
        GoalRecord bp = Rec(Type::FlyTo, 30);
        bp.params.withinM = -1;
        CHECK(reason(bp, sit) == Reason::BadParam);
        bp.params.withinM = NAN;
        CHECK(reason(bp, sit) == Reason::BadParam);
        GoalRecord sgn = Rec(Type::Hold);
        sgn.done.push_back(C(CndKind::ScriptGoal)); // (no trigger)
        CHECK(reason(sgn, sit) == Reason::BadParam);
        CHECK(reason(Rec(Type::FlyTo), sit) == Reason::NoSubject);
        CHECK(reason(Rec(Type::FlyTo, 999), sit) == Reason::Unresolved); // no such contact
        CHECK(reason(Rec(Type::Attack, 60), sit) == Reason::Unresolved); // destroyed
        GoalRecord dep = Rec(Type::Hold);
        dep.dependsOn = {77};
        GoalQueue empty;
        Env withQ;
        withQ.queue = &empty;
        CHECK(reason(dep, sit, withQ) == Reason::Unresolved);            // a dependency the queue does not hold
        CHECK(reason(dep, sit) == Reason::Unresolved);                   // references without a queue
        GoalRecord par = Rec(Type::Hold);
        par.parent = 5;
        CHECK(reason(par, sit, withQ) == Reason::Unresolved);
        CHECK(reason(Rec(Type::Dock, 10), sit) == Reason::BadSubject);      // a ship is no base
        CHECK(reason(Rec(Type::Formation, 20), sit) == Reason::BadSubject); // a hostile
        CHECK(reason(Rec(Type::Jump, 30), sit) == Reason::BadSubject);      // a station is no gate
        CHECK(reason(Rec(Type::TradeLane, 30), sit) == Reason::BadSubject);
        CHECK(reason(Rec(Type::TradeLane, 40), sit) == Reason::GameLock);   // closed both ways
        GoalRecord ks = Rec(Type::FlyTo, 30);
        ks.policy.knowledge = 1;
        CHECK(reason(ks, sit) == Reason::KnowledgeScope);
        Snapshot sAll = s0;
        sAll.knowledge = 1;
        CHECK(reason(ks, Assess(sAll)) == Reason::Ok);
        Snapshot held = s0;
        held.missionHolds = true;
        CHECK(reason(Rec(Type::FlyTo, 30), Assess(held)) == Reason::GameLock);
        CHECK(reason(Rec(Type::Wait), Assess(held)) == Reason::Ok); // (waiting is no flight)
        GoalRecord far = Rec(Type::FlyTo);
        far.subject.kind = Ref::Kind::Pos, far.subject.system = 200, far.subject.pos[0] = 1;
        CHECK(reason(far, sit) == Reason::NoRoute);
        Env route;
        route.routeKnown = RouteYes;
        CHECK(reason(far, sit, route) == Reason::Ok);
        GoalRecord sys = Rec(Type::Jump);
        sys.subject.kind = Ref::Kind::System, sys.subject.id = 300;
        CHECK(reason(sys, sit) == Reason::NoRoute);
        // a never-attack step (a job's *_dont_kill_ships); a leash the subject lies outside of
        Snapshot nk = s0;
        nk.step.job = "mb_dont_kill_ships";
        CHECK(reason(Rec(Type::Attack, 20), Assess(nk)) == Reason::BrokenConstraint);
        Snapshot ls = s0;
        ls.leashes.push_back({10, "juni", 3000, 0, {}});
        CHECK(reason(Rec(Type::FlyTo, 30), Assess(ls)) == Reason::BrokenConstraint); // 4 km from Juni, leash 3 km
        CHECK(reason(Rec(Type::FlyTo, 20), Assess(ls)) == Reason::Ok);
        CHECK(std::string(ReasonName(Reason::BrokenConstraint)) == "broken_constraint");
    }

    // ---- the re-proof: valid / achieved / failed / blocked
    {
        const Snapshot s0 = Space();
        const situation::Situation sit = Assess(s0);
        GoalRecord fly = Rec(Type::FlyTo, 30);
        fly.done.push_back(C(CndKind::DistObj, 30, 500));
        CHECK(Prove(fly, sit).ok() && fly.proof == Proof::Valid);
        fly.done[0].m = 6000;
        Prove(fly, sit);
        CHECK(fly.proof == Proof::Achieved);
        GoalRecord esc = Rec(Type::Escort, 10);
        esc.fail.push_back(C(CndKind::Destroyed, 10));
        Prove(esc, sit);
        CHECK(esc.proof == Proof::Valid);
        Snapshot dead = s0;
        dead.contacts[0].health = 0;
        Prove(esc, Assess(dead));
        CHECK(esc.proof == Proof::Failed && esc.reason == Reason::FailHolds);
        GoalRecord gone = Rec(Type::Dock, 999);
        Prove(gone, sit);
        CHECK(gone.proof == Proof::Failed && gone.reason == Reason::Unresolved);
        Snapshot held = s0;
        held.missionHolds = true;
        GoalRecord h = Rec(Type::FlyTo, 30);
        Prove(h, Assess(held));
        CHECK(h.proof == Proof::Blocked && h.reason == Reason::GameLock);
        GoalRecord w = Rec(Type::FlyTo, 30);
        w.when.push_back(C(CndKind::InSystem, 999));
        Prove(w, sit);
        CHECK(w.proof == Proof::Blocked && w.reason == Reason::When);
        GoalRecord unk = Rec(Type::FlyTo, 30);
        unk.done.push_back(C(CndKind::Destroyed, 999)); // (not among the contacts: Unknown, never done)
        Prove(unk, sit);
        CHECK(unk.proof == Proof::Valid);
        // a dependency not done yet blocks; done (popped) it no longer does
        GoalQueue q;
        GoalRecord a = Rec(Type::FlyTo, 30), b = Rec(Type::Dock, 30);
        a.id = 1, b.id = 2, b.dependsOn = {1};
        q.entries = {a, b};
        ProveAll(q, sit);
        CHECK(q.entries[1].proof == Proof::Blocked && q.entries[1].reason == Reason::DependsOn);
        // the condition language's tri-state
        CHECK(Eval(C(CndKind::Destroyed, 60), sit) == Tri::Yes && Eval(C(CndKind::Destroyed, 10), sit) == Tri::No);
        Cnd neg = C(CndKind::Destroyed, 999);
        neg.negate = true;
        CHECK(Eval(neg, sit) == Tri::Unknown); // (negating Unknown stays Unknown)
        CHECK(Eval(C(CndKind::InSystem, 100), sit) == Tri::Yes && Eval(C(CndKind::Docked), sit) == Tri::No);
        CHECK(Eval(C(CndKind::HullBelow, 0, 0.5f), sit) == Tri::No && Eval(C(CndKind::StepLeft, sit.stepKey), sit) == Tri::No);
        CHECK(Eval(C(CndKind::StepLeft, sit.stepKey + 1), sit) == Tri::Yes);
    }

    // ---- achieve semantics: achieved heads popped before anything acts
    {
        const Snapshot s0 = Space();
        const situation::Situation sit = Assess(s0);
        GoalQueue q;
        GoalRecord a = Rec(Type::FlyTo, 30), b = Rec(Type::FlyTo, 10), c = Rec(Type::Dock, 30), d = Rec(Type::Hold);
        a.id = 1, b.id = 2, c.id = 3, d.id = 4;
        a.done.push_back(C(CndKind::DistObj, 30, 6000)); // done
        b.done.push_back(C(CndKind::DistObj, 10, 1500)); // done
        c.done.push_back(C(CndKind::Docked, 3000));      // not yet
        c.dependsOn = {2};
        d.parent = 1; // a's step: goes with it
        q.entries = {a, b, c, d};
        CHECK(PopAchieved(q, sit) == 2);
        CHECK(q.entries.size() == 1 && Head(q)->id == 3 && Head(q)->proof == Proof::Valid);
        CHECK(Head(q)->state == State::Active && Head(q)->dependsOn.empty()); // (the popped id left its dependsOn)
        // a repeat re-arms at the back; a forever-repeat in a queue of one stops after one pass
        GoalQueue r;
        GoalRecord p = Rec(Type::FlyTo, 30);
        p.done.push_back(C(CndKind::DistObj, 30, 6000));
        p.repeat = 2, p.memory.arrived = true;
        GoalRecord n = Rec(Type::Hold);
        n.id = 2;
        r.entries = {p, n};
        CHECK(PopAchieved(r, sit) == 1);
        CHECK(r.entries.size() == 2 && r.entries[0].id == 2 && r.entries[1].repeat == 1 && !r.entries[1].memory.arrived);
        GoalQueue f;
        GoalRecord e = p;
        e.repeat = -1;
        f.entries = {e};
        CHECK(PopAchieved(f, sit) == 1 && f.entries.size() == 1);
        // nothing acts on an achieved head: after the pop the rules see the next entry's memory, not the popped one's
        GoalQueue empty;
        CHECK(PopAchieved(empty, sit) == 0 && Head(empty) == nullptr);
    }

    // ---- the step key: written only by Advance after a decision for a new step (4c4fb6f's class)
    {
        Snapshot sA = Space();
        sA.step.textId = 23240, sA.objSeq = 1;
        Snapshot sB = sA;
        sB.step.textId = 23245, sB.objSeq = 2;
        const situation::Situation a = Assess(sA), b = Assess(sB);
        CHECK(a.stepKey != b.stepKey);
        GoalQueue q = Rebuild(a); // (cold: the rebuild writes no key)
        CHECK(q.stepKey == 0);
        Proposal go;
        go.rule = "arrive", go.mode = "travel", go.target = 30, go.action = "arrive";
        CHECK(Advance(q, a, go) && q.stepKey == a.stepKey);
        // the decision's memory lives in the step's entry
        for (GoalRecord& r : q.entries)
            if (r.type == Type::MissionStep)
                r.memory.arrived = true, r.memory.deadGoals = {"x"};
        // step B shows; the assess, the re-proof and the pops write nothing ...
        GoalQueue q2 = q;
        ProveAll(q2, b);
        PopAchieved(q2, b);
        CHECK(q2.stepKey == a.stepKey);
        // ... a held verdict decides nothing: the key stays A, so the next look still sees a new step
        Proposal held;
        held.rule = "held", held.keep = true;
        CHECK(!Advance(q2, b, held) && q2.stepKey == a.stepKey);
        // the decision for B replaces the story entries (fresh memory) and only then writes the key
        CHECK(Advance(q2, b, go) && q2.stepKey == b.stepKey);
        const situation::Queue rq = ToRulesQueue(q2, ExecMemory(), b);
        CHECK(!rq.arrived && rq.deadGoals.empty() && rq.stepAgeMs >= 0);
        CHECK(!Advance(q2, b, go)); // (the same step again: no new entry)
        // the story memory: lastReal kept through a placeholder; a Dock step's base carried behind a placeholder
        Snapshot sD = sA;
        sD.step.textId = 23260, sD.objSeq = 3, sD.step.verb = situation::Verb::Dock;
        sD.step.refs[0] = 1, sD.step.refObj[0] = 30, sD.step.refCount = 1;
        GoalQueue qd = Rebuild(Assess(sD));
        Advance(qd, Assess(sD), go);
        Snapshot sP = sA;
        sP.step.textId = 21660, sP.objSeq = 4, sP.goalsFromLast = true;
        Advance(qd, Assess(sP), go);
        const situation::Queue rp = ToRulesQueue(qd, ExecMemory(), Assess(sP));
        CHECK(rp.lastDock == 30 && rp.lastReal == 23260);
        Snapshot sR = sA;
        sR.step.textId = 23270, sR.objSeq = 5; // a real step the script knows: the carried dock ends
        Advance(qd, Assess(sR), go);
        CHECK(ToRulesQueue(qd, ExecMemory(), Assess(sR)).lastDock == 0);
    }

    // ---- D10: the re-proof on resume
    {
        Snapshot s0 = Space();
        const situation::Situation sit = Assess(s0);
        GoalQueue paused = Rebuild(sit);
        Proposal go;
        go.rule = "arrive";
        Advance(paused, sit, go);
        for (GoalRecord& r : paused.entries)
            if (r.type == Type::MissionStep)
                r.memory.arriveRetries = 2;
        GoalRecord mine = Rec(Type::FlyTo, 30);
        mine.id = paused.nextId++, mine.done.push_back(C(CndKind::DistObj, 30, 500));
        GoalRecord doneMine = Rec(Type::FlyTo, 10);
        doneMine.id = paused.nextId++, doneMine.done.push_back(C(CndKind::DistObj, 10, 1500));
        GoalRecord deadMine = Rec(Type::Escort, 60); // (its ship destroyed: no longer valid)
        deadMine.id = paused.nextId++;
        paused.entries.push_back(mine), paused.entries.push_back(deadMine);
        PauseFacts shortPause;
        shortPause.pausedMs = 5000, shortPause.movedM = 200;
        const ResumePolicy pol;
        // a short pause, the same step, the story head valid: silent, the step's memory kept, the dead escort dropped
        Resumed r = Resume(paused, sit, shortPause, pol);
        CHECK(r.choice == ResumeChoice::Silent && r.dropped == 1);
        CHECK(Head(r.queue) && Head(r.queue)->type == Type::MissionStep);
        CHECK(ToRulesQueue(r.queue, ExecMemory(), sit).arriveRetries == 2);
        bool kept = false;
        for (const GoalRecord& e : r.queue.entries)
            kept = kept || e.id == mine.id;
        CHECK(kept);
        // long / another system / docked / a job / far: ask (and the step re-planned fresh)
        PauseFacts p = shortPause;
        p.pausedMs = 120000;
        Resumed rl = Resume(paused, sit, p, pol);
        CHECK(rl.choice == ResumeChoice::Ask && ToRulesQueue(rl.queue, ExecMemory(), sit).arriveRetries == 0);
        p = shortPause, p.systemChanged = true;
        CHECK(Resume(paused, sit, p, pol).choice == ResumeChoice::Ask);
        p = shortPause, p.docked = true;
        CHECK(Resume(paused, sit, p, pol).choice == ResumeChoice::Ask);
        p = shortPause, p.jobTaken = true;
        CHECK(Resume(paused, sit, p, pol).choice == ResumeChoice::Ask);
        p = shortPause, p.movedM = 20000;
        CHECK(Resume(paused, sit, p, pol).choice == ResumeChoice::Ask);
        // the story moved on meanwhile: the old step popped (rebuilt for the new one), the player's achieved one popped
        Snapshot s1 = s0;
        s1.step.textId = 23105, s1.objSeq = 5;
        GoalQueue onlyMine;
        onlyMine.entries = {doneMine, mine};
        onlyMine.nextId = 10;
        Resumed rm = Resume(onlyMine, Assess(s1), shortPause, pol);
        CHECK(rm.popped == 1 && Head(rm.queue) && Head(rm.queue)->type == Type::MissionStep);
        CHECK(Head(rm.queue)->display.title == 23105);
        // no objective, no player goal: nothing to resume
        Snapshot sn = s0;
        sn.step.textId = 0;
        GoalQueue none;
        CHECK(Resume(none, Assess(sn), shortPause, pol).choice == ResumeChoice::Nothing);
        // a blocked head asks
        Snapshot sh = s0;
        sh.step.textId = 0, sh.missionHolds = true;
        GoalQueue hq;
        hq.entries = {mine};
        CHECK(Resume(hq, Assess(sh), shortPause, pol).choice == ResumeChoice::Ask);
        std::string why;
        CHECK(ResumeChoiceOf(nullptr, shortPause, pol, &why) == ResumeChoice::Nothing && !why.empty());
    }

    // ---- the memory classes cover the rules' Queue (every field once)
    {
        size_t n = 0;
        std::map<std::string, int> seen;
        for (const FieldInfo& f : MemoryFields())
            ++n, ++seen[f.name];
        CHECK(n == 35 && seen.size() == n);
        situation::Queue a, b;
        CHECK(Differing(a, b).empty());
        b.arrived = true, b.lastReal = 5, b.trDest[1] = 2;
        const std::vector<const FieldInfo*> d = Differing(a, b);
        CHECK(d.size() == 3);
        for (const FieldInfo* f : d)
            f->copy(b, a);
        CHECK(Differing(a, b).empty());
    }

    // ---- the story entries from the live state: S182's Toledo goal is a knowledge-"all" child of the step
    {
        Snapshot x;
        const std::string text = ReadTextFile(g_test.fixtures + "\\situation\\m12_hole_s182.snap");
        if (situation::Parse(text, x))
        {
            const situation::Situation sit = Assess(x);
            u32 next = 1;
            const std::vector<GoalRecord> st = StoryEntries(sit, next);
            CHECK(st.size() >= 2 && st[0].type == Type::MissionStep && st[0].display.title == 21660);
            bool toledo = false;
            for (const GoalRecord& r : st)
                if (r.sourceText == "range_for_sendcomm_dx_m12_0500_vonclaussen")
                {
                    toledo = r.parent == st[0].id && r.type == Type::FlyTo && r.policy.knowledge == 1;
                    GoalRecord k = r;
                    Snapshot ex = x;
                    ex.knowledge = 0;
                    k.parent = 0;
                    CHECK(Validate(k, Assess(ex)).reason == Reason::KnowledgeScope);
                }
            CHECK(toledo);
        }
        else
            CHECK(!"m12_hole_s182.snap parses");
    }

    // ---- cold = warm over every recorded fixture
    // For each: the recorded memory (warm) and the queue rebuilt from the situation alone (cold), the executor's own
    // state the same in both (it is the running sub-mode's, not memory the queue keeps; the "from off" column drops it
    // too: an activation). Expected: "agree", "exception:<field>" (declared memory: entry / mission scope) or
    // "finding:<field>" (memory the rebuild cannot derive from the situation - A37's target).
    const std::map<std::string, std::string> expect = {
        {"free_docked_c701.snap", "agree"},
        {"job_killships_c701.snap", "agree"},
        {"m03_california_arrive_s009.snap", "agree"},
        // S6's EXCEPTION (entry memory) `arrived` = this step's travel ended (503 m from California Minor's ring): the cold
        // queue flew it again (arrive). Settled in S7: the rebuilt step entry's own arrival test (goalqueue::
        // WithinArrivalReach, within 1 km of the ring) derives it - agree
        {"m03_california_arrived_s009.snap", "agree"},
        {"m03_convoy_escort_s008.snap", "agree"},
        {"m03_convoy_threatfight_s008.snap", "agree"},
        {"m03_end_free_rest_s023.snap", "agree"},
        {"m03_end_free_s023.snap", "agree"},
        {"m03_valkyries_threatfight_s009.snap", "agree"},
        {"m03_willard_arrive_s010.snap", "agree"},
        {"m03_willard_s010.snap", "agree"},
        {"m07_gate_jump_s207.snap", "agree"},
        {"m07_gate_lane_s207.snap", "agree"},
        {"m07_gate_rest_s207.snap", "agree"},
        // FINDING (story memory the rebuild cannot derive): "Awaiting" after the Tau-31 lane ride - warm, lastReal 23250
        // names the next step's goal (pre-stage-next toward juni_cautious); cold, nothing in the snapshot names the last
        // real objective (the live table's stand-in names the script, not the text) and the mark fallback follows Juni
        // (ship-target 16). ObjectiveDecide's s_lastReal survives toggles and loads today: exactly A37's hidden memory.
        {"m07_tau31_prestagenext_s208.snap", "finding:last_real"},
        {"m07_tau31_s208.snap", "agree"},
        {"m12_core_airlock_s180.snap", "agree"},
        {"m12_core_navmarker_s180.snap", "agree"},
        {"m12_core_navway_s180.snap", "agree"},
        {"m12_core_tractorexit_s180.snap", "agree"}, // (from off it differs: the running travel is the executor's)
        {"m12_hole_s182.snap", "agree"},
        {"m12_hole_threatfight_s182.snap", "agree"},
        {"m13_battleship_markfight_s350.snap", "agree"},
        {"m13_battleship_threatfight_s350.snap", "agree"},
        {"m13_bsdown_blank_s349.snap", "agree"}, // (gate s7arb2's differ: lossless only with the seen lists in the story)
        {"m13_start_3989.snap", "agree"},
    };
    int fixtures = 0, agree = 0, exceptions = 0, findings = 0, lossless = 0, offDiffer = 0, withMemory = 0;
    std::map<std::string, int> lackedField;
    for (const SnapFixture& f : SnapFixtures(g_test.fixtures + "\\situation"))
    {
        Snapshot x;
        if (!situation::Parse(f.text, x))
            continue;
        ++fixtures;
        const situation::Situation sit = Assess(x);
        const Proposal pRec = situation::Decide(sit, x.queue);
        // the classing is lossless: the recorded memory through the goal queue decides the same
        const Classed warm = FromRulesQueue(sit, x.queue);
        const situation::Queue qWarm = ToRulesQueue(warm.queue, warm.exec, sit);
        const Proposal pWarm = situation::Decide(sit, qWarm);
        const bool same = SameDecision(pRec, pWarm);
        CHECK(same);
        lossless += same ? 1 : 0;
        if (!same)
            fprintf(stderr, "  goalqueue: %s: classing lost memory: %s vs %s\n", f.name.c_str(), ProposalText(pRec).c_str(),
                    ProposalText(pWarm).c_str());
        // cold: the situation alone
        const GoalQueue cold = Rebuild(sit);
        const situation::Queue qCold = ToRulesQueue(cold, warm.exec, sit);
        const Proposal pCold = situation::Decide(sit, qCold);
        // (how much the comparison tests: the memory the cold queue lacks, fixture by fixture)
        bool lacks = false;
        for (const FieldInfo* fi : Differing(qWarm, qCold))
            lacks = true, ++lackedField[fi->name];
        withMemory += lacks ? 1 : 0;
        std::string verdict = "agree";
        if (!SameDecision(pWarm, pCold))
        {
            // which memory makes the difference: each differing field put back alone
            std::string cause;
            MemClass cls = MemClass::COUNT;
            for (const FieldInfo* fi : Differing(qWarm, qCold))
            {
                situation::Queue t = qCold;
                fi->copy(qWarm, t);
                if (SameDecision(situation::Decide(sit, t), pWarm))
                    cause += (cause.empty() ? "" : "+") + std::string(fi->name), cls = fi->cls;
            }
            if (cause.empty())
                cause = "combined";
            const bool declared = cls == MemClass::Entry || cls == MemClass::Mission;
            verdict = std::string(declared ? "exception:" : "finding:") + cause;
            (declared ? exceptions : findings) += 1;
        }
        else
            ++agree;
        // from off (an activation: no running sub-mode either) - reported, not compared
        const Proposal pOff = situation::Decide(sit, ToRulesQueue(cold, ExecMemory(), sit));
        const bool offSame = SameDecision(pOff, pWarm);
        offDiffer += offSame ? 0 : 1;
        const auto it = expect.find(f.name);
        const bool expected = it != expect.end() && it->second == verdict;
        CHECK(expected);
        if (g_test.verbose || !expected)
            printf("  %-40s %-28s warm %s | cold %s | from off %s%s\n", f.name.c_str(), verdict.c_str(),
                   ProposalText(pWarm).c_str(), ProposalText(pCold).c_str(), ProposalText(pOff).c_str(),
                   expected ? "" : (it == expect.end() ? " (no expected verdict)" : (" (expected " + it->second + ")").c_str()));
    }
    CHECK(fixtures >= 25);
    printf("  cold = warm over %d fixtures: %d agree, %d declared-memory exceptions, %d findings; classing lossless %d / %d; "
           "from off differs %d\n",
           fixtures, agree, exceptions, findings, lossless, fixtures, offDiffer);
    std::string lacked;
    for (const auto& kv : lackedField)
        lacked += " " + kv.first + "=" + std::to_string(kv.second);
    printf("  the cold queue lacks recorded memory in %d fixtures:%s\n", withMemory, lacked.c_str());
}
