// The goal queue (goalqueue.h; A37_PLAN S6): pure - a situation and records in, verdicts / records / text out.
#include "goalqueue.h"
#include "situation_io.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace goalqueue
{
namespace
{
using situation::SnapContact;
using situation::Snapshot;
// The game's object type bits (space::Type; as situation.cpp)
constexpr u32 kTJumpGate = 1u << 6, kTLaneRing = 1u << 7, kTAirlock = 1u << 10, kTJumpHole = 1u << 11;
constexpr float kHostileAtt = -0.6f;
constexpr u32 kAwaiting = 21660, kBlankText = 1;

float Dist(const float a[3], const float b[3])
{
    const float d[3] = {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
    return std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
}
const SnapContact* Find(const Snapshot& s, u32 id)
{
    if (!id)
        return nullptr;
    for (const SnapContact& c : s.contacts)
        if (c.id == id)
            return &c;
    return nullptr;
}
bool Hostile(const SnapContact& c) { return c.ship ? c.attitude <= kHostileAtt : (c.attKnown && c.att <= kHostileAtt); }
bool Finite(float v) { return std::isfinite(v) != 0; }
bool RealText(u32 t) { return t && t != kAwaiting && t != kBlankText; }

template <class E> bool InEnum(E e) { return static_cast<int>(e) >= 0 && static_cast<int>(e) < static_cast<int>(E::COUNT); }

// the types that need a subject, and what it must be
bool NeedsSubject(Type t)
{
    switch (t)
    {
    case Type::FlyTo:
    case Type::Dock:
    case Type::TradeLane:
    case Type::Jump:
    case Type::Formation:
    case Type::Follow:
    case Type::Escort:
    case Type::Attack:
    case Type::Defend: return true;
    default: return false;
    }
}
// a type that flies in space (a game lock blocks it)
bool Flies(Type t) { return t != Type::Wait && t != Type::BaseAction && t != Type::MissionStep; }

Verdict V(Reason r, std::string d = std::string())
{
    Verdict v;
    v.reason = r, v.detail = std::move(d);
    return v;
}
} // namespace

// ---------------------------------------------------------------- names
const char* TypeName(Type t)
{
    static const char* const k[] = {"mission_step", "fly_to", "dock",   "trade_lane", "jump", "formation",
                                    "follow",       "escort", "attack", "clear_area", "defend", "loot",
                                    "hold",         "repair", "wait",   "base_action"};
    static_assert(sizeof(k) / sizeof(k[0]) == static_cast<size_t>(Type::COUNT), "Type names");
    return InEnum(t) ? k[static_cast<int>(t)] : "?";
}
const char* StateName(State s)
{
    static const char* const k[] = {"pending", "active", "suspended", "done", "failed", "cancelled"};
    static_assert(sizeof(k) / sizeof(k[0]) == static_cast<size_t>(State::COUNT), "State names");
    return InEnum(s) ? k[static_cast<int>(s)] : "?";
}
const char* OriginName(Origin o)
{
    static const char* const k[] = {"story", "player", "job"};
    static_assert(sizeof(k) / sizeof(k[0]) == static_cast<size_t>(Origin::COUNT), "Origin names");
    return InEnum(o) ? k[static_cast<int>(o)] : "?";
}
const char* EngageName(Engage e)
{
    static const char* const k[] = {"auto", "avoid", "protect", "fight"};
    static_assert(sizeof(k) / sizeof(k[0]) == static_cast<size_t>(Engage::COUNT), "Engage names");
    return InEnum(e) ? k[static_cast<int>(e)] : "?";
}
const char* RefKindName(Ref::Kind k)
{
    static const char* const n[] = {"none", "object", "pos", "system"};
    static_assert(sizeof(n) / sizeof(n[0]) == static_cast<size_t>(Ref::Kind::COUNT), "Ref names");
    return InEnum(k) ? n[static_cast<int>(k)] : "?";
}
const char* CndKindName(CndKind k)
{
    static const char* const n[] = {"true",      "dist_obj", "dist_pos",   "destroyed", "docked",     "in_system",
                                    "step_left", "lane_ride", "leader",    "hull_below", "script_goal"};
    static_assert(sizeof(n) / sizeof(n[0]) == static_cast<size_t>(CndKind::COUNT), "CndKind names");
    return InEnum(k) ? n[static_cast<int>(k)] : "?";
}
const char* ProofName(Proof p)
{
    static const char* const k[] = {"valid", "achieved", "failed", "blocked"};
    static_assert(sizeof(k) / sizeof(k[0]) == static_cast<size_t>(Proof::COUNT), "Proof names");
    return InEnum(p) ? k[static_cast<int>(p)] : "?";
}
const char* ReasonName(Reason r)
{
    static const char* const k[] = {"ok",          "bad_version", "unknown_type",    "unknown_field", "bad_param",
                                    "no_subject",  "unresolved",  "bad_subject",     "knowledge_scope", "game_lock",
                                    "no_route",    "broken_constraint", "depends_on", "when",          "fail_holds"};
    static_assert(sizeof(k) / sizeof(k[0]) == static_cast<size_t>(Reason::COUNT), "Reason names");
    return InEnum(r) ? k[static_cast<int>(r)] : "?";
}
const char* MemClassName(MemClass c)
{
    static const char* const k[] = {"derived", "entry", "story", "mission", "executor"};
    static_assert(sizeof(k) / sizeof(k[0]) == static_cast<size_t>(MemClass::COUNT), "MemClass names");
    return InEnum(c) ? k[static_cast<int>(c)] : "?";
}
const char* ResumeChoiceName(ResumeChoice c)
{
    static const char* const k[] = {"nothing", "silent", "ask"};
    static_assert(sizeof(k) / sizeof(k[0]) == static_cast<size_t>(ResumeChoice::COUNT), "ResumeChoice names");
    return InEnum(c) ? k[static_cast<int>(c)] : "?";
}

// ---------------------------------------------------------------- conditions
Tri Eval(const Cnd& c, const Situation& sit)
{
    const Snapshot& s = sit.facts;
    const auto yn = [](bool b) { return b ? Tri::Yes : Tri::No; };
    Tri t = Tri::Unknown;
    switch (c.kind)
    {
    case CndKind::True: t = Tri::Yes; break;
    case CndKind::DistObj:
        if (const SnapContact* x = Find(s, c.id))
            if (s.pilotOk)
                t = yn(Dist(x->pos, s.pos) <= c.m);
        break;
    case CndKind::DistPos:
        if (s.pilotOk)
            t = yn(Dist(c.pos, s.pos) <= c.m);
        break;
    case CndKind::Destroyed:
        if (const SnapContact* x = Find(s, c.id))
            t = yn(x->health == 0);
        break;
    case CndKind::Docked: t = yn(!s.inSpace && s.base && (!c.id || s.base == c.id)); break;
    case CndKind::InSystem:
        if (s.system)
            t = yn(s.system == c.id);
        break;
    case CndKind::StepLeft: t = yn(sit.stepKey != c.id); break;
    case CndKind::LaneRide:
        if (s.pilotOk)
            t = yn(s.laneRide);
        break;
    case CndKind::Leader:
        if (s.pilotOk)
            t = yn(c.id && s.leader == c.id);
        break;
    case CndKind::HullBelow:
        if (s.hull >= 0)
            t = yn(s.hull < c.m);
        break;
    case CndKind::ScriptGoal:
        for (const situation::GoalView& g : sit.goals)
            if (g.source != "next" && g.trigger == c.name)
            {
                if (g.achieved == situation::Achieved::Yes || g.live == 2)
                    t = Tri::Yes;
                else if (g.achieved == situation::Achieved::No && t == Tri::Unknown)
                    t = Tri::No;
            }
        break;
    default: break; // (an unknown kind: Unknown; ValidateSchema refuses it)
    }
    if (c.negate && t != Tri::Unknown)
        t = t == Tri::Yes ? Tri::No : Tri::Yes;
    return t;
}
Tri AnyOf(const std::vector<Cnd>& cs, const Situation& sit)
{
    Tri out = Tri::No;
    for (const Cnd& c : cs)
    {
        const Tri t = Eval(c, sit);
        if (t == Tri::Yes)
            return Tri::Yes;
        if (t == Tri::Unknown)
            out = Tri::Unknown;
    }
    return out;
}
Tri AllOf(const std::vector<Cnd>& cs, const Situation& sit)
{
    Tri out = Tri::Yes;
    for (const Cnd& c : cs)
    {
        const Tri t = Eval(c, sit);
        if (t == Tri::No)
            return Tri::No;
        if (t == Tri::Unknown)
            out = Tri::Unknown;
    }
    return out;
}

// ---------------------------------------------------------------- the validator
Verdict ValidateSchema(const GoalRecord& r)
{
    if (r.version != kRecordVersion)
        return V(Reason::BadVersion, "record version " + std::to_string(r.version));
    if (!InEnum(r.type))
        return V(Reason::UnknownType, "type " + std::to_string(static_cast<int>(r.type)));
    if (!InEnum(r.state) || !InEnum(r.origin) || !InEnum(r.policy.engage) || !InEnum(r.subject.kind))
        return V(Reason::UnknownField, "an enum out of range (state / origin / engage / subject kind)");
    if (static_cast<int>(r.params.stepKind) < 0 || r.params.stepKind >= objkinds::GoalType::COUNT)
        return V(Reason::UnknownField, "step kind out of range");
    for (const std::vector<Cnd>* cs : {&r.when, &r.done, &r.fail})
        for (const Cnd& c : *cs)
        {
            if (!InEnum(c.kind))
                return V(Reason::UnknownField, "condition kind " + std::to_string(static_cast<int>(c.kind)));
            if (!Finite(c.m) || c.m < 0 || !Finite(c.pos[0]) || !Finite(c.pos[1]) || !Finite(c.pos[2]))
                return V(Reason::BadParam, std::string("condition ") + CndKindName(c.kind) + " argument");
            if (c.kind == CndKind::ScriptGoal && c.name.empty())
                return V(Reason::BadParam, "script_goal without a trigger");
        }
    const Params& p = r.params;
    if (!Finite(p.withinM) || p.withinM < 0 || !Finite(p.radiusM) || p.radiusM < 0 || p.count < 0 || r.repeat < -1)
        return V(Reason::BadParam, "within / radius / count / repeat");
    if (!Finite(r.subject.pos[0]) || !Finite(r.subject.pos[1]) || !Finite(r.subject.pos[2]))
        return V(Reason::BadParam, "subject position");
    if (r.policy.knowledge < 0 || r.policy.knowledge > 1)
        return V(Reason::BadParam, "knowledge " + std::to_string(r.policy.knowledge));
    if (r.subject.kind == Ref::Kind::Object && !r.subject.id)
        return V(Reason::NoSubject, "object 0");
    return V(Reason::Ok);
}

Verdict Validate(const GoalRecord& r, const Situation& sit, const Env& env)
{
    Verdict v = ValidateSchema(r);
    if (!v.ok())
        return v;
    const Snapshot& s = sit.facts;
    // references: the parent and the dependencies name entries of the queue
    if (env.queue)
    {
        const auto known = [&](u32 id)
        {
            for (const GoalRecord& e : env.queue->entries)
                if (e.id == id)
                    return true;
            return false;
        };
        if (r.parent && !known(r.parent))
            return V(Reason::Unresolved, "parent " + std::to_string(r.parent));
        for (u32 d : r.dependsOn)
            if (d == r.id || !known(d))
                return V(Reason::Unresolved, "depends_on " + std::to_string(d));
    }
    else if (!r.dependsOn.empty() || r.parent)
        return V(Reason::Unresolved, "references without a queue");
    // the subject
    if (NeedsSubject(r.type) && r.subject.kind == Ref::Kind::None)
        return V(Reason::NoSubject, TypeName(r.type));
    float subjPos[3] = {};
    bool subjPlaced = false;
    const SnapContact* sc = nullptr;
    const u32 subjSystem = r.subject.kind == Ref::Kind::System ? r.subject.id : r.subject.system;
    // (a MissionStep's subject is the game's own pointer: informational - the game decides the step, never its subject)
    const bool elsewhere = r.type != Type::MissionStep && r.subject.kind != Ref::Kind::None && subjSystem && s.system &&
                           subjSystem != s.system;
    if (r.type == Type::MissionStep)
    {
    }
    else if (elsewhere)
    {
        if (!(env.routeKnown && env.routeKnown(s.system, subjSystem)))
            return V(Reason::NoRoute, "system " + std::to_string(subjSystem) + " from " + std::to_string(s.system));
    }
    else if (r.subject.kind == Ref::Kind::Object)
    {
        sc = Find(s, r.subject.id);
        if (!sc)
            return V(Reason::Unresolved, "object " + std::to_string(r.subject.id) + " is no contact");
        if (sc->health == 0)
            return V(Reason::Unresolved, "object " + std::to_string(r.subject.id) + " destroyed");
        memcpy(subjPos, sc->pos, sizeof(subjPos)), subjPlaced = true;
        const bool ship = sc->ship;
        switch (r.type)
        {
        case Type::Dock:
            if (ship || !(sc->dock || (sc->type & (kTAirlock | kTLaneRing))))
                return V(Reason::BadSubject, "not dockable");
            break;
        case Type::TradeLane:
        {
            if (!(sc->type & kTLaneRing))
                return V(Reason::BadSubject, "no lane ring");
            for (const situation::SnapLane& l : s.lanes)
                if (l.ring == sc->id)
                {
                    // (Rule_Lane's reading: a neighbour's state counts only when it exists; closed = none open, one known)
                    const int on = l.next ? l.openNext : -1, op = l.prev ? l.openPrev : -1;
                    if (on != 1 && op != 1 && (on == 0 || op == 0))
                        return V(Reason::GameLock, "lane closed both ways");
                }
            break;
        }
        case Type::Jump:
            if (!(sc->type & (kTJumpGate | kTJumpHole)))
                return V(Reason::BadSubject, "no jump gate / hole");
            break;
        case Type::Formation:
        case Type::Follow:
        case Type::Escort:
            if (!ship || Hostile(*sc))
                return V(Reason::BadSubject, "no friendly ship");
            break;
        default: break;
        }
    }
    else if (r.subject.kind == Ref::Kind::Pos)
        memcpy(subjPos, r.subject.pos, sizeof(subjPos)), subjPlaced = true;
    // the knowledge scope
    if (r.policy.knowledge > s.knowledge)
        return V(Reason::KnowledgeScope, "needs knowledge " + std::to_string(r.policy.knowledge) + ", scope " +
                                             std::to_string(s.knowledge));
    // a game lock (the mission flies the ship, the game docks it, no ship)
    if (Flies(r.type) && (sit.held == situation::Held::MissionHolds || sit.held == situation::Held::GameDock ||
                          sit.held == situation::Held::NoShip))
        return V(Reason::GameLock, situation::HeldName(sit.held));
    // the mission's constraints: a never-attack step; a leash the subject lies outside of (unknown arming = armed)
    for (const situation::Constraint& k : sit.constraints)
    {
        if (k.kind == situation::Constraint::Kind::NeverAttack && (r.type == Type::Attack || r.type == Type::ClearArea) &&
            r.origin != Origin::Story)
            return V(Reason::BrokenConstraint, "never attack: " + k.name);
        if (k.kind == situation::Constraint::Kind::Leash && k.armed != 0 && subjPlaced && !elsewhere && Flies(r.type))
            if (const SnapContact* lc = Find(s, k.object))
                if (Dist(subjPos, lc->pos) > k.dist)
                    return V(Reason::BrokenConstraint, "outside the leash of " + k.name);
    }
    return V(Reason::Ok);
}

Verdict Prove(GoalRecord& r, const Situation& sit, const Env& env)
{
    const auto set = [&](Proof p, Verdict v)
    {
        r.proof = p, r.reason = v.reason;
        return v;
    };
    Verdict v = ValidateSchema(r);
    if (!v.ok())
        return set(Proof::Failed, v);
    if (AnyOf(r.done, sit) == Tri::Yes)
        return set(Proof::Achieved, V(Reason::Ok, "done"));
    if (AnyOf(r.fail, sit) == Tri::Yes)
        return set(Proof::Failed, V(Reason::FailHolds));
    v = Validate(r, sit, env);
    if (v.reason == Reason::GameLock)
        return set(Proof::Blocked, v);
    if (!v.ok())
        return set(Proof::Failed, v);
    if (AllOf(r.when, sit) != Tri::Yes)
        return set(Proof::Blocked, V(Reason::When));
    if (env.queue)
        for (u32 d : r.dependsOn)
            for (const GoalRecord& e : env.queue->entries)
                if (e.id == d && e.proof != Proof::Achieved && e.state != State::Done)
                    return set(Proof::Blocked, V(Reason::DependsOn, std::to_string(d)));
    return set(Proof::Valid, V(Reason::Ok));
}

void ProveAll(GoalQueue& q, const Situation& sit, const Env& env)
{
    Env e = env;
    e.queue = &q;
    // (dependencies read the entries' proofs: prove the depended-on first - one pass in order, then the rest again)
    for (GoalRecord& r : q.entries)
        Prove(r, sit, e);
    for (GoalRecord& r : q.entries)
        if (!r.dependsOn.empty())
            Prove(r, sit, e);
}

const GoalRecord* Head(const GoalQueue& q) { return q.entries.empty() ? nullptr : &q.entries.front(); }

namespace
{
// Pops entry i (achieved): its steps go with it, its id leaves every dependsOn; a repeat re-arms it at the back.
void PopAt(GoalQueue& q, size_t i)
{
    GoalRecord again = q.entries[i];
    const u32 id = again.id;
    q.entries.erase(q.entries.begin() + static_cast<std::ptrdiff_t>(i));
    q.entries.erase(std::remove_if(q.entries.begin(), q.entries.end(), [&](const GoalRecord& e) { return e.parent == id; }),
                    q.entries.end());
    for (GoalRecord& e : q.entries)
        e.dependsOn.erase(std::remove(e.dependsOn.begin(), e.dependsOn.end(), id), e.dependsOn.end());
    if (again.repeat != 0)
    {
        if (again.repeat > 0)
            --again.repeat;
        again.memory = EntryMemory();
        again.state = State::Pending;
        q.entries.push_back(again);
    }
}
void MarkHead(GoalQueue& q)
{
    for (size_t i = 0; i < q.entries.size(); ++i)
        if (q.entries[i].state == State::Pending || q.entries[i].state == State::Active)
            q.entries[i].state = i == 0 ? State::Active : State::Pending;
}
// Every achieved entry popped (D10's re-proof on resume: not only the head), at most one pass over the queue.
int PopAllAchieved(GoalQueue& q, const Situation& sit, const Env& env)
{
    int popped = 0;
    const size_t limit = q.entries.size();
    for (size_t pass = 0; pass < limit; ++pass)
    {
        ProveAll(q, sit, env);
        size_t i = 0;
        while (i < q.entries.size() && q.entries[i].proof != Proof::Achieved)
            ++i;
        if (i == q.entries.size())
            break;
        PopAt(q, i);
        ++popped;
    }
    ProveAll(q, sit, env);
    MarkHead(q);
    return popped;
}
} // namespace

int PopAchieved(GoalQueue& q, const Situation& sit, const Env& env)
{
    int popped = 0;
    const size_t limit = q.entries.size();
    for (size_t pass = 0; pass < limit && !q.entries.empty(); ++pass)
    {
        ProveAll(q, sit, env);
        if (q.entries.front().proof != Proof::Achieved)
            break;
        PopAt(q, 0);
        ++popped;
    }
    ProveAll(q, sit, env);
    MarkHead(q);
    return popped;
}

// ---------------------------------------------------------------- the story's entries, the rebuild
namespace
{
Type StepChildType(int kind) { return kind == 3 ? Type::Attack : Type::FlyTo; }
} // namespace

std::vector<GoalRecord> StoryEntries(const Situation& sit, u32& nextId)
{
    std::vector<GoalRecord> out;
    const Snapshot& s = sit.facts;
    if (sit.placeholder == situation::Placeholder::NoObjective)
        return out;
    GoalRecord st;
    st.id = nextId++;
    st.type = Type::MissionStep;
    bool job = false;
    for (int i = -1; i < static_cast<int>(s.entries.size()); ++i)
        job = job || !(i < 0 ? s.step : s.entries[static_cast<size_t>(i)]).job.empty();
    st.origin = job || sit.kindSource == situation::KindSource::Job ? Origin::Job : Origin::Story;
    st.params.stepKind = sit.kind;
    if (sit.subject && Find(s, sit.subject))
        st.subject.kind = Ref::Kind::Object, st.subject.id = sit.subject;
    else if (sit.subjectHasPos)
    {
        st.subject.kind = Ref::Kind::Pos, st.subject.system = s.step.hasNav && s.step.navSystem ? s.step.navSystem : 0;
        memcpy(st.subject.pos, sit.subjectPos, sizeof(st.subject.pos));
    }
    st.display.title = sit.textId;
    st.stepKey = sit.stepKey;
    Cnd left;
    left.kind = CndKind::StepLeft, left.id = sit.stepKey;
    st.done.push_back(left);
    out.push_back(st);
    // the script goals (the story source: the live table's armed goals, else the ini view) as the step's children
    int n = 0;
    for (const situation::GoalView& g : sit.goals)
    {
        if (g.source == "next" || g.trigger.empty() || (g.kind < 1 || g.kind > 3))
            continue;
        const situation::SnapGoal* sg = nullptr;
        for (const situation::SnapGoal& x : s.goals)
            if (x.trigger == g.trigger && x.kind == g.kind)
                sg = &x;
        GoalRecord c;
        c.id = nextId++;
        c.type = StepChildType(g.kind);
        c.origin = st.origin;
        c.parent = st.id, c.step = n++;
        c.policy.knowledge = 1; // (the script's own conditions: knowledge "all")
        c.stepKey = sit.stepKey;
        if (sg && g.kind == 1 && sg->placed)
            c.subject.kind = Ref::Kind::Pos, memcpy(c.subject.pos, sg->objPos, sizeof(c.subject.pos));
        else if (sg && g.kind == 2)
            c.subject.kind = Ref::Kind::Pos, memcpy(c.subject.pos, sg->pos, sizeof(c.subject.pos));
        if (sg)
            c.params.withinM = sg->dist, c.params.count = g.kind == 3 ? sg->count : 0;
        if (c.type == Type::Attack)
            c.type = Type::ClearArea; // (a label set, no one subject)
        else if (c.subject.kind == Ref::Kind::None)
            c.type = Type::Wait; // (an unplaced solar: the condition is still the done test)
        Cnd done;
        done.kind = CndKind::ScriptGoal, done.name = g.trigger;
        c.done.push_back(done);
        c.sourceText = g.trigger;
        out.push_back(c);
    }
    return out;
}

bool WithinArrivalReach(const Situation& sit)
{
    const Snapshot& s = sit.facts;
    if (!s.pilotOk || s.step.verb == situation::Verb::Dock)
        return false;
    u32 target = 0;
    for (int i = 0; i < s.step.refCount && !target; ++i)
        target = s.step.refObj[i];
    if (target)
    {
        const SnapContact* tc = Find(s, target);
        if (!tc || tc->ship || (tc->type & (kTAirlock | kTLaneRing | kTJumpGate | kTJumpHole)))
            return false;
        const SnapContact* dest = tc->dockRing ? Find(s, tc->dockRing) : tc; // (the arrive rule's destination)
        return dest && Dist(dest->pos, s.pos) <= kArrivalReachM;
    }
    if (!s.step.hasNav || (s.step.navSystem && s.step.navSystem != s.system))
        return false;
    for (const SnapContact& c : s.contacts) // (a marker on a way through is the nav-way's trip, never "arrived")
        if (!c.ship && (c.type & (kTAirlock | kTJumpGate | kTJumpHole)) && Dist(c.pos, s.step.nav) < 200)
            return false;
    return Dist(s.step.nav, s.pos) <= kNavReachM;
}

GoalQueue Rebuild(const Situation& sit, const GoalQueue* prev, const Env& env)
{
    GoalQueue q;
    if (prev)
    {
        q.nextId = prev->nextId, q.stepKey = prev->stepKey;
        q.story = prev->story, q.mission = prev->mission;
    }
    std::vector<GoalRecord> story = StoryEntries(sit, q.nextId);
    if (!story.empty() && story.front().type == Type::MissionStep)
        story.front().memory.arrived = WithinArrivalReach(sit); // (the step's own arrival test: S6's `arrived` exception)
    q.entries = story;
    if (prev)
    {
        Env e = env;
        e.queue = &q;
        for (const GoalRecord& r : prev->entries)
        {
            const bool carriedDock = r.origin == Origin::Story && r.type == Type::Dock && !r.parent;
            if (r.origin != Origin::Player && !carriedDock)
                continue; // (the story's / job's: rebuilt above)
            GoalRecord k = r;
            k.dependsOn.erase(std::remove_if(k.dependsOn.begin(), k.dependsOn.end(),
                                             [&](u32 d)
                                             {
                                                 for (const GoalRecord& x : prev->entries)
                                                     if (x.id == d)
                                                         return x.origin != Origin::Player;
                                                 return true;
                                             }),
                              k.dependsOn.end());
            if (k.parent && std::none_of(prev->entries.begin(), prev->entries.end(),
                                         [&](const GoalRecord& x) { return x.id == k.parent && x.origin == Origin::Player; }))
                k.parent = 0;
            q.entries.push_back(k);
        }
        // the kept ones re-proven against the rebuilt queue: a Failed one is dropped
        std::vector<GoalRecord> kept;
        for (GoalRecord& r : q.entries)
        {
            Prove(r, sit, e);
            if (r.origin == Origin::Player || (r.origin == Origin::Story && r.type == Type::Dock && !r.parent))
                if (r.proof == Proof::Failed)
                    continue;
            kept.push_back(r);
        }
        q.entries = kept;
    }
    return q;
}

// ---------------------------------------------------------------- the classed executor memory
namespace
{
// (a scalar / string / vector member by value; a float[3] member by its bytes - two names, no overload to resolve)
template <class T> bool Eq(const T& a, const T& b) { return a == b; }
template <class T> void Cp(const T& a, T& b) { b = a; }
bool Eq3(const float (&a)[3], const float (&b)[3]) { return memcmp(a, b, sizeof(a)) == 0; }
void Cp3(const float (&a)[3], float (&b)[3]) { memcpy(b, a, sizeof(b)); }
#define GQ_FIELD(key, member, cls)                                                                                     \
    {                                                                                                                  \
        key, MemClass::cls, [](const situation::Queue& a, const situation::Queue& b) { return Eq(a.member, b.member); }, \
            [](const situation::Queue& a, situation::Queue& b) { Cp(a.member, b.member); }                               \
    }
#define GQ_FIELD3(key, member, cls)                                                                                    \
    {                                                                                                                  \
        key, MemClass::cls, [](const situation::Queue& a, const situation::Queue& b) { return Eq3(a.member, b.member); }, \
            [](const situation::Queue& a, situation::Queue& b) { Cp3(a.member, b.member); }                              \
    }
} // namespace

const std::vector<FieldInfo>& MemoryFields()
{
    static const std::vector<FieldInfo> k = {
        GQ_FIELD("seq", seq, Derived),
        GQ_FIELD("mode", mode, Executor),
        GQ_FIELD("target", target, Executor),
        GQ_FIELD("sub", sub, Executor),
        GQ_FIELD("sub_target", subTarget, Executor),
        GQ_FIELD("action", action, Executor),
        GQ_FIELD("tr_dest", trHasDest, Executor),
        GQ_FIELD("tr_object", trDestObject, Executor),
        GQ_FIELD3("tr_pos", trDest, Executor),
        GQ_FIELD("lock", lockId, Executor),
        GQ_FIELD("lock_have", lockHave, Executor),
        GQ_FIELD3("lock_p2", lockP2, Executor),
        GQ_FIELD("cruise_lock", cruiseLock, Entry),
        GQ_FIELD("leash_ship", leashShip, Derived),
        GQ_FIELD("leash_dist", leashDist, Derived),
        GQ_FIELD("last_dock", lastDock, Story),
        GQ_FIELD("escort", escortShip, Entry),
        GQ_FIELD("bad_goal", badGoal, Entry),
        GQ_FIELD("dead_goals", deadGoals, Entry),
        GQ_FIELD("script_marks", scriptMarks, Entry),
        GQ_FIELD("seen_alive", seenAlive, Story),
        GQ_FIELD("seen_listed", seenListed, Story),
        GQ_FIELD("last_real", lastReal, Story),
        GQ_FIELD("arrived", arrived, Entry),
        GQ_FIELD("step_age", stepAgeMs, Entry),
        GQ_FIELD("escorted", escorted, Executor),
        GQ_FIELD("surv_title", survTitle, Mission),
        GQ_FIELD("surv_mission", survMission, Mission),
        GQ_FIELD("surv_ended", survEnded, Mission),
        GQ_FIELD("surv_system", survSystem, Mission),
        GQ_FIELD("survival", survival, Mission),
        GQ_FIELD("mission_leader", missionLeader, Entry),
        GQ_FIELD("via_next", viaNext, Entry),
        GQ_FIELD("arrive_retries", arriveRetries, Entry),
        GQ_FIELD("tr_pass", trPassRing, Executor),
    };
    return k;
}
#undef GQ_FIELD
#undef GQ_FIELD3

std::vector<const FieldInfo*> Differing(const situation::Queue& a, const situation::Queue& b)
{
    std::vector<const FieldInfo*> out;
    for (const FieldInfo& f : MemoryFields())
        if (!f.same(a, b))
            out.push_back(&f);
    return out;
}

u32 DerivedLeashShip(const Situation& sit, float& dist)
{
    const Snapshot& s = sit.facts;
    u32 ship = 0;
    dist = 0;
    for (const situation::SnapLeash& l : s.goalsLive ? s.iniLeashes : s.leashes)
    {
        if (l.off)
        {
            ship = 0, dist = 0;
            continue;
        }
        const SnapContact* oc = Find(s, l.object);
        if (!(oc && oc->ship) && l.nameIds && l.ship)
            if (const SnapContact* byName = Find(s, l.ship))
                oc = byName;
        if (oc && oc->ship)
            ship = oc->id, dist = l.dist;
    }
    return ship;
}

namespace
{
const GoalRecord* StepEntry(const GoalQueue& gq, const Situation& sit)
{
    for (const GoalRecord& r : gq.entries)
        if (r.type == Type::MissionStep && r.origin != Origin::Player && r.stepKey == sit.stepKey)
            return &r;
    return nullptr;
}
GoalRecord* StepEntry(GoalQueue& gq, const Situation& sit)
{
    return const_cast<GoalRecord*>(StepEntry(static_cast<const GoalQueue&>(gq), sit));
}
const GoalRecord* CarriedDock(const GoalQueue& gq)
{
    for (const GoalRecord& r : gq.entries)
        if (r.origin == Origin::Story && r.type == Type::Dock && !r.parent && r.subject.kind == Ref::Kind::Object)
            return &r;
    return nullptr;
}
GoalRecord DockEntry(u32 object, u32 textId, u32& nextId)
{
    GoalRecord d;
    d.id = nextId++;
    d.type = Type::Dock;
    d.origin = Origin::Story;
    d.subject.kind = Ref::Kind::Object, d.subject.id = object;
    d.display.title = textId;
    Cnd docked;
    docked.kind = CndKind::Docked; // (any base: the dock ends it)
    d.done.push_back(docked);
    Cnd gone;
    gone.kind = CndKind::Destroyed, gone.id = object;
    d.fail.push_back(gone);
    d.sourceText = "last_dock";
    return d;
}
} // namespace

ExecMemory ExecOf(const situation::Queue& q)
{
    ExecMemory e;
    e.mode = q.mode, e.target = q.target, e.sub = q.sub, e.subTarget = q.subTarget, e.action = q.action;
    e.trHasDest = q.trHasDest, e.trDestObject = q.trDestObject, memcpy(e.trDest, q.trDest, sizeof(e.trDest));
    e.lockId = q.lockId, e.lockHave = q.lockHave, memcpy(e.lockP2, q.lockP2, sizeof(e.lockP2));
    e.trPassRing = q.trPassRing, e.escorted = q.escorted;
    return e;
}

situation::Queue ToRulesQueue(const GoalQueue& gq, const ExecMemory& ex, const Situation& sit)
{
    const Snapshot& s = sit.facts;
    situation::Queue q;
    // derived each time
    q.seq = s.objSeq;
    q.leashShip = DerivedLeashShip(sit, q.leashDist);
    // executor-only
    q.mode = ex.mode, q.target = ex.target, q.sub = ex.sub, q.subTarget = ex.subTarget, q.action = ex.action;
    q.trHasDest = ex.trHasDest, q.trDestObject = ex.trDestObject, memcpy(q.trDest, ex.trDest, sizeof(q.trDest));
    q.lockId = ex.lockId, q.lockHave = ex.lockHave, memcpy(q.lockP2, ex.lockP2, sizeof(q.lockP2));
    q.trPassRing = ex.trPassRing, q.escorted = ex.escorted;
    // the step entry's memory (none: fresh)
    if (const GoalRecord* st = StepEntry(gq, sit))
    {
        const EntryMemory& m = st->memory;
        q.escortShip = m.escortShip, q.badGoal = m.badGoal, q.deadGoals = m.deadGoals, q.arrived = m.arrived;
        q.arriveRetries = m.arriveRetries, q.viaNext = m.viaNext, q.cruiseLock = m.cruiseLock;
        q.stepAgeMs = m.stepAtKnown ? (s.clock - m.stepAt) * 1000.0 : -1.0;
        q.scriptMarks = m.scriptMarks;
        q.missionLeader = m.missionLeader;
    }
    // the story's memory: the carried Dock entry, the last real text, the destroy conditions' objects seen
    if (const GoalRecord* d = CarriedDock(gq))
        q.lastDock = d->subject.id;
    q.lastReal = gq.story.lastReal;
    q.seenAlive = gq.story.seenAlive, q.seenListed = gq.story.seenListed;
    // mission scope
    q.survTitle = gq.mission.survTitle, q.survMission = gq.mission.survMission, q.survEnded = gq.mission.survEnded;
    q.survSystem = gq.mission.survSystem, q.survival = gq.mission.survival;
    return q;
}

Classed FromRulesQueue(const Situation& sit, const situation::Queue& q)
{
    Classed c;
    GoalQueue& gq = c.queue;
    gq.entries = StoryEntries(sit, gq.nextId);
    gq.stepKey = sit.stepKey;
    if (GoalRecord* st = StepEntry(gq, sit))
    {
        EntryMemory& m = st->memory;
        m.escortShip = q.escortShip, m.badGoal = q.badGoal, m.deadGoals = q.deadGoals, m.arrived = q.arrived;
        m.arriveRetries = q.arriveRetries, m.viaNext = q.viaNext, m.cruiseLock = q.cruiseLock;
        m.stepAtKnown = q.stepAgeMs >= 0, m.stepAt = m.stepAtKnown ? sit.facts.clock - q.stepAgeMs / 1000.0 : 0;
        m.scriptMarks = q.scriptMarks;
        m.missionLeader = q.missionLeader;
    }
    if (q.lastDock)
        gq.entries.push_back(DockEntry(q.lastDock, 0, gq.nextId));
    gq.story.lastReal = q.lastReal;
    gq.story.seenAlive = q.seenAlive, gq.story.seenListed = q.seenListed;
    gq.mission.survTitle = q.survTitle, gq.mission.survMission = q.survMission, gq.mission.survEnded = q.survEnded;
    gq.mission.survSystem = q.survSystem, gq.mission.survival = q.survival;
    c.exec = ExecOf(q);
    return c;
}

void CarryMemory(GoalQueue& gq, const situation::Queue& q, double clockSec)
{
    for (GoalRecord& r : gq.entries)
        if (r.type == Type::MissionStep && r.origin != Origin::Player && r.stepKey == gq.stepKey)
        {
            EntryMemory& m = r.memory;
            m.escortShip = q.escortShip, m.badGoal = q.badGoal, m.deadGoals = q.deadGoals, m.arrived = q.arrived;
            m.arriveRetries = q.arriveRetries, m.viaNext = q.viaNext, m.cruiseLock = q.cruiseLock;
            m.stepAtKnown = q.stepAgeMs >= 0, m.stepAt = m.stepAtKnown ? clockSec - q.stepAgeMs / 1000.0 : 0;
            m.scriptMarks = q.scriptMarks;
            m.missionLeader = q.missionLeader;
            break;
        }
    gq.story.seenAlive = q.seenAlive, gq.story.seenListed = q.seenListed;
    const GoalRecord* d = CarriedDock(gq);
    if (!d || d->subject.id != q.lastDock)
    {
        gq.entries.erase(std::remove_if(gq.entries.begin(), gq.entries.end(),
                                        [](const GoalRecord& r)
                                        { return r.origin == Origin::Story && r.type == Type::Dock && !r.parent; }),
                         gq.entries.end());
        if (q.lastDock)
            gq.entries.push_back(DockEntry(q.lastDock, 0, gq.nextId));
    }
    gq.story.lastReal = q.lastReal;
    gq.mission.survTitle = q.survTitle, gq.mission.survMission = q.survMission, gq.mission.survEnded = q.survEnded;
    gq.mission.survSystem = q.survSystem, gq.mission.survival = q.survival;
}

bool Advance(GoalQueue& q, const Situation& sit, const situation::Proposal& p)
{
    if (p.rule == "held" || p.rule == "none" || p.rule.empty())
        return false; // (no decision: nothing it stands for is written)
    const Snapshot& s = sit.facts;
    // the story memory (every decision, as ObjectiveDecide): the last real text; the last dock (a Dock step's base /
    // planet sets it, a Dock step at transit clears it, a placeholder the script does not know keeps it while its object
    // is a contact and no leash is in the system, anything else clears it)
    if (RealText(s.step.textId))
        q.story.lastReal = s.step.textId;
    u32 target = 0;
    for (int i = 0; i < s.step.refCount && !target; ++i)
        target = s.step.refObj[i];
    const SnapContact* tc = Find(s, target);
    const auto dropDock = [&]()
    {
        q.entries.erase(std::remove_if(q.entries.begin(), q.entries.end(),
                                       [](const GoalRecord& r)
                                       { return r.origin == Origin::Story && r.type == Type::Dock && !r.parent; }),
                        q.entries.end());
    };
    if (s.step.verb == situation::Verb::Dock && tc && !(tc->type & (kTLaneRing | kTJumpGate | kTJumpHole)))
    {
        dropDock();
        q.entries.push_back(DockEntry(tc->id, s.step.textId, q.nextId));
    }
    else if (s.step.verb == situation::Verb::Dock)
        dropDock();
    else if (const GoalRecord* d = CarriedDock(q))
    {
        const bool leashed = !(s.goalsLive ? s.iniLeashes : s.leashes).empty();
        if (!s.goalsFromLast || !Find(s, d->subject.id) || leashed)
            dropDock();
    }
    if (sit.stepKey == q.stepKey)
        return false;
    // a new step: its story entries replace the old ones (their entry memory fresh), then the key is written
    q.entries.erase(std::remove_if(q.entries.begin(), q.entries.end(),
                                   [](const GoalRecord& r)
                                   {
                                       return r.origin != Origin::Player &&
                                              !(r.origin == Origin::Story && r.type == Type::Dock && !r.parent);
                                   }),
                    q.entries.end());
    std::vector<GoalRecord> story = StoryEntries(sit, q.nextId);
    if (!story.empty())
        story.front().memory.stepAtKnown = true, story.front().memory.stepAt = s.clock;
    q.entries.insert(q.entries.begin(), story.begin(), story.end());
    q.stepKey = sit.stepKey;
    return true;
}

// ---------------------------------------------------------------- D10
ResumeChoice ResumeChoiceOf(const GoalRecord* head, const PauseFacts& pause, const ResumePolicy& pol, std::string* why)
{
    const auto say = [&](ResumeChoice c, const std::string& w)
    {
        if (why)
            *why = w;
        return c;
    };
    if (!head)
        return say(ResumeChoice::Nothing, "the queue is empty");
    if (head->proof != Proof::Valid)
        return say(ResumeChoice::Ask, std::string("the head is ") + ProofName(head->proof) + " (" + ReasonName(head->reason) + ")");
    if (!(pause.pausedMs <= pol.silentMaxMs))
        return say(ResumeChoice::Ask, "paused " + std::to_string(static_cast<long long>(pause.pausedMs)) + " ms");
    if (pause.systemChanged)
        return say(ResumeChoice::Ask, "another system");
    if (pause.docked)
        return say(ResumeChoice::Ask, "docked meanwhile");
    if (pause.jobTaken)
        return say(ResumeChoice::Ask, "a job taken meanwhile");
    if (!(pause.movedM <= pol.movedMaxM))
        return say(ResumeChoice::Ask, "moved " + std::to_string(static_cast<long long>(pause.movedM)) + " m");
    return say(ResumeChoice::Silent, "short pause, head valid");
}

Resumed Resume(const GoalQueue& paused, const Situation& sit, const PauseFacts& pause, const ResumePolicy& pol,
               const Env& env)
{
    Resumed r;
    r.queue = Rebuild(sit, &paused, env);
    int playersBefore = 0, playersAfter = 0;
    for (const GoalRecord& e : paused.entries)
        playersBefore += e.origin == Origin::Player ? 1 : 0;
    for (const GoalRecord& e : r.queue.entries)
        playersAfter += e.origin == Origin::Player ? 1 : 0;
    r.dropped = playersBefore - playersAfter;
    r.popped = PopAllAchieved(r.queue, sit, env);
    r.choice = ResumeChoiceOf(Head(r.queue), pause, pol, &r.why);
    if (r.choice == ResumeChoice::Silent)
        if (const GoalRecord* was = StepEntry(paused, sit))
            if (GoalRecord* now = StepEntry(r.queue, sit))
                now->memory = was->memory; // (the same step goes on)
    return r;
}

// ---------------------------------------------------------------- text
namespace
{
using namespace situation::io;
// a string inside a condition / list: EncStr plus ':' '|' '!' (the condition's own separators)
std::string EncX(const std::string& s)
{
    std::string o;
    for (char c : EncStr(s))
        if (c == ':')
            o += "%3A";
        else if (c == '|')
            o += "%7C";
        else if (c == '!')
            o += "%21";
        else
            o += c;
    return o;
}
std::string U(u32 v) { return std::to_string(v); }
std::string I(int v) { return std::to_string(v); }
std::string B(bool v) { return v ? "1" : "0"; }
std::string Vec(const float* p) { return EncF(p[0]) + "," + EncF(p[1]) + "," + EncF(p[2]); }
std::string UList(const std::vector<u32>& v)
{
    std::string o;
    for (size_t i = 0; i < v.size(); ++i)
        o += (i ? "," : "") + U(v[i]);
    return o;
}
std::string SList(const std::vector<std::string>& v)
{
    std::string o;
    for (size_t i = 0; i < v.size(); ++i)
        o += (i ? "," : "") + EncX(v[i]);
    return o;
}
std::string Cnds(const std::vector<Cnd>& cs)
{
    std::string o;
    for (size_t i = 0; i < cs.size(); ++i)
    {
        const Cnd& c = cs[i];
        o += (i ? "|" : "") + std::string(c.negate ? "!" : "") + CndKindName(c.kind) + ":" + U(c.id) + ":" + EncF(c.m) +
             ":" + Vec(c.pos) + ":" + EncX(c.name);
    }
    return o;
}
template <class E> bool EnumFrom(const std::string& s, E& out, const char* (*name)(E))
{
    for (int i = 0; i < static_cast<int>(E::COUNT); ++i)
        if (s == name(static_cast<E>(i)))
            return out = static_cast<E>(i), true;
    return false;
}
bool DecVec(const std::string& s, float* p)
{
    const std::vector<std::string> v = Split(s, ',');
    return v.size() == 3 && DecF(v[0], p[0]) && DecF(v[1], p[1]) && DecF(v[2], p[2]);
}
bool DecUList(const std::string& s, std::vector<u32>& out)
{
    out.clear();
    for (const std::string& x : Split(s, ','))
    {
        u32 v = 0;
        if (!DecU(x, v))
            return false;
        out.push_back(v);
    }
    return true;
}
bool DecSList(const std::string& s, std::vector<std::string>& out)
{
    out.clear();
    for (const std::string& x : Split(s, ','))
    {
        std::string v;
        if (!DecStr(x, v))
            return false;
        out.push_back(v);
    }
    return true;
}
bool DecCnds(const std::string& s, std::vector<Cnd>& out)
{
    out.clear();
    for (const std::string& x : Split(s, '|'))
    {
        Cnd c;
        std::string t = x;
        if (!t.empty() && t[0] == '!')
            c.negate = true, t = t.substr(1);
        const std::vector<std::string> f = Split(t, ':');
        if (f.size() != 5 || !EnumFrom(f[0], c.kind, CndKindName) || !DecU(f[1], c.id) || !DecF(f[2], c.m) ||
            !DecVec(f[3], c.pos) || !DecStr(f[4], c.name))
            return false;
        out.push_back(c);
    }
    return true;
}
std::string Subject(const Ref& r)
{
    return std::string(RefKindName(r.kind)) + ":" + U(r.id) + ":" + U(r.system) + ":" + Vec(r.pos);
}
bool DecSubject(const std::string& s, Ref& r)
{
    const std::vector<std::string> f = Split(s, ':');
    return f.size() == 4 && EnumFrom(f[0], r.kind, RefKindName) && DecU(f[1], r.id) && DecU(f[2], r.system) &&
           DecVec(f[3], r.pos);
}
const char* StepKindName(objkinds::GoalType t) { return objkinds::GoalTypeName(t); }

// The record's fields in their fixed order: key, writer, reader.
struct Codec
{
    const char* key;
    std::string (*put)(const GoalRecord&);
    bool (*get)(const std::string&, GoalRecord&);
};
const std::vector<Codec>& Codecs()
{
    static const std::vector<Codec> k = {
        {"id", [](const GoalRecord& r) { return U(r.id); }, [](const std::string& v, GoalRecord& r) { return DecU(v, r.id); }},
        {"v", [](const GoalRecord& r) { return I(r.version); }, [](const std::string& v, GoalRecord& r) { return DecI(v, r.version); }},
        {"type", [](const GoalRecord& r) { return std::string(TypeName(r.type)); },
         [](const std::string& v, GoalRecord& r) { return EnumFrom(v, r.type, TypeName); }},
        {"state", [](const GoalRecord& r) { return std::string(StateName(r.state)); },
         [](const std::string& v, GoalRecord& r) { return EnumFrom(v, r.state, StateName); }},
        {"origin", [](const GoalRecord& r) { return std::string(OriginName(r.origin)); },
         [](const std::string& v, GoalRecord& r) { return EnumFrom(v, r.origin, OriginName); }},
        {"parent", [](const GoalRecord& r) { return U(r.parent); }, [](const std::string& v, GoalRecord& r) { return DecU(v, r.parent); }},
        {"step", [](const GoalRecord& r) { return I(r.step); }, [](const std::string& v, GoalRecord& r) { return DecI(v, r.step); }},
        {"deps", [](const GoalRecord& r) { return UList(r.dependsOn); },
         [](const std::string& v, GoalRecord& r) { return DecUList(v, r.dependsOn); }},
        {"subject", [](const GoalRecord& r) { return Subject(r.subject); },
         [](const std::string& v, GoalRecord& r) { return DecSubject(v, r.subject); }},
        {"within", [](const GoalRecord& r) { return EncF(r.params.withinM); },
         [](const std::string& v, GoalRecord& r) { return DecF(v, r.params.withinM); }},
        {"radius", [](const GoalRecord& r) { return EncF(r.params.radiusM); },
         [](const std::string& v, GoalRecord& r) { return DecF(v, r.params.radiusM); }},
        {"count", [](const GoalRecord& r) { return I(r.params.count); },
         [](const std::string& v, GoalRecord& r) { return DecI(v, r.params.count); }},
        {"toward", [](const GoalRecord& r) { return U(r.params.toward); },
         [](const std::string& v, GoalRecord& r) { return DecU(v, r.params.toward); }},
        {"step_kind", [](const GoalRecord& r) { return std::string(StepKindName(r.params.stepKind)); },
         [](const std::string& v, GoalRecord& r)
         {
             for (int i = 0; i < static_cast<int>(objkinds::GoalType::COUNT); ++i)
                 if (v == objkinds::GoalTypeName(static_cast<objkinds::GoalType>(i)))
                     return r.params.stepKind = static_cast<objkinds::GoalType>(i), true;
             return false;
         }},
        {"when", [](const GoalRecord& r) { return Cnds(r.when); }, [](const std::string& v, GoalRecord& r) { return DecCnds(v, r.when); }},
        {"done", [](const GoalRecord& r) { return Cnds(r.done); }, [](const std::string& v, GoalRecord& r) { return DecCnds(v, r.done); }},
        {"fail", [](const GoalRecord& r) { return Cnds(r.fail); }, [](const std::string& v, GoalRecord& r) { return DecCnds(v, r.fail); }},
        {"knowledge", [](const GoalRecord& r) { return I(r.policy.knowledge); },
         [](const std::string& v, GoalRecord& r) { return DecI(v, r.policy.knowledge); }},
        {"engage", [](const GoalRecord& r) { return std::string(EngageName(r.policy.engage)); },
         [](const std::string& v, GoalRecord& r) { return EnumFrom(v, r.policy.engage, EngageName); }},
        {"side_effects", [](const GoalRecord& r) { return B(r.policy.sideEffects); },
         [](const std::string& v, GoalRecord& r) { return (v == "0" || v == "1") && ((r.policy.sideEffects = v == "1"), true); }},
        {"confirm", [](const GoalRecord& r) { return B(r.confirm); },
         [](const std::string& v, GoalRecord& r) { return (v == "0" || v == "1") && ((r.confirm = v == "1"), true); }},
        {"repeat", [](const GoalRecord& r) { return I(r.repeat); }, [](const std::string& v, GoalRecord& r) { return DecI(v, r.repeat); }},
        {"text", [](const GoalRecord& r) { return EncStr(r.sourceText); },
         [](const std::string& v, GoalRecord& r) { return DecStr(v, r.sourceText); }},
        {"title", [](const GoalRecord& r) { return U(r.display.title); },
         [](const std::string& v, GoalRecord& r) { return DecU(v, r.display.title); }},
        {"detail", [](const GoalRecord& r) { return U(r.display.detail); },
         [](const std::string& v, GoalRecord& r) { return DecU(v, r.display.detail); }},
        {"step_key", [](const GoalRecord& r) { return U(r.stepKey); }, [](const std::string& v, GoalRecord& r) { return DecU(v, r.stepKey); }},
        {"m_escort", [](const GoalRecord& r) { return U(r.memory.escortShip); },
         [](const std::string& v, GoalRecord& r) { return DecU(v, r.memory.escortShip); }},
        {"m_bad_goal", [](const GoalRecord& r) { return EncStr(r.memory.badGoal); },
         [](const std::string& v, GoalRecord& r) { return DecStr(v, r.memory.badGoal); }},
        {"m_dead_goals", [](const GoalRecord& r) { return SList(r.memory.deadGoals); },
         [](const std::string& v, GoalRecord& r) { return DecSList(v, r.memory.deadGoals); }},
        {"m_arrived", [](const GoalRecord& r) { return B(r.memory.arrived); },
         [](const std::string& v, GoalRecord& r) { return (v == "0" || v == "1") && ((r.memory.arrived = v == "1"), true); }},
        {"m_retries", [](const GoalRecord& r) { return I(r.memory.arriveRetries); },
         [](const std::string& v, GoalRecord& r) { return DecI(v, r.memory.arriveRetries); }},
        {"m_via", [](const GoalRecord& r) { return B(r.memory.viaNext); },
         [](const std::string& v, GoalRecord& r) { return (v == "0" || v == "1") && ((r.memory.viaNext = v == "1"), true); }},
        {"m_cruise_lock", [](const GoalRecord& r) { return B(r.memory.cruiseLock); },
         [](const std::string& v, GoalRecord& r) { return (v == "0" || v == "1") && ((r.memory.cruiseLock = v == "1"), true); }},
        {"m_step_at", [](const GoalRecord& r) { return r.memory.stepAtKnown ? EncD(r.memory.stepAt) : std::string("none"); },
         [](const std::string& v, GoalRecord& r)
         {
             r.memory.stepAtKnown = v != "none";
             r.memory.stepAt = 0;
             return !r.memory.stepAtKnown || DecD(v, r.memory.stepAt);
         }},
        {"m_script_marks", [](const GoalRecord& r) { return UList(r.memory.scriptMarks); },
         [](const std::string& v, GoalRecord& r) { return DecUList(v, r.memory.scriptMarks); }},
        {"m_leader", [](const GoalRecord& r) { return U(r.memory.missionLeader); },
         [](const std::string& v, GoalRecord& r) { return DecU(v, r.memory.missionLeader); }},
    };
    return k;
}
bool Fail(std::string* err, const std::string& why)
{
    if (err)
        *err = why;
    return false;
}
} // namespace

std::string SerializeRecord(const GoalRecord& rec)
{
    std::string o = "goal";
    for (const Codec& c : Codecs())
        o += std::string(" ") + c.key + "=" + c.put(rec);
    return o;
}

bool ParseRecord(const std::string& line, GoalRecord& out, std::string* err)
{
    const std::vector<std::string> tok = Split(line, ' ');
    if (tok.empty() || tok[0] != "goal")
        return Fail(err, "not a goal record");
    const std::vector<Codec>& cs = Codecs();
    if (tok.size() != cs.size() + 1)
        return Fail(err, "goal: " + std::to_string(tok.size() - 1) + " fields, want " + std::to_string(cs.size()));
    GoalRecord r;
    for (size_t i = 0; i < cs.size(); ++i)
    {
        const std::string& t = tok[i + 1];
        const size_t eq = t.find('=');
        if (eq == std::string::npos)
            return Fail(err, "goal: field " + std::to_string(i + 1) + " has no '='");
        const std::string key = t.substr(0, eq);
        if (key != cs[i].key)
            return Fail(err, "goal: field '" + key + "' where '" + cs[i].key + "' belongs (unknown / out of order)");
        if (!cs[i].get(t.substr(eq + 1), r))
            return Fail(err, "goal: bad value of " + key);
    }
    out = r;
    return true;
}

std::string Serialize(const GoalQueue& q)
{
    std::string o = "goalqueue v" + I(q.version) + " next_id=" + U(q.nextId) + " step_key=" + U(q.stepKey) +
                    " last_real=" + U(q.story.lastReal) + " surv_title=" + U(q.mission.survTitle) + " surv_mission=" +
                    B(q.mission.survMission) + " surv_ended=" + B(q.mission.survEnded) + " surv_system=" +
                    U(q.mission.survSystem) + " survival=" + B(q.mission.survival) + " seen_alive=" +
                    UList(q.story.seenAlive) + " seen_listed=" + UList(q.story.seenListed) + "\n";
    for (const GoalRecord& r : q.entries)
        o += SerializeRecord(r) + "\n";
    return o + "end\n";
}

bool Parse(const std::string& text, GoalQueue& out, std::string* err)
{
    std::vector<std::string> lines;
    for (std::string l : Split(text, '\n'))
    {
        while (!l.empty() && l.back() == '\r')
            l.pop_back();
        if (!l.empty() && l[0] != '#')
            lines.push_back(l);
    }
    if (lines.size() < 2 || lines.back() != "end")
        return Fail(err, "goalqueue: no header or no end");
    const std::vector<std::string> h = Split(lines[0], ' ');
    static const char* const keys[] = {"next_id", "step_key", "last_real", "surv_title", "surv_mission",
                                       "surv_ended", "surv_system", "survival", "seen_alive", "seen_listed"};
    const size_t nk = sizeof(keys) / sizeof(keys[0]);
    if (h.size() != nk + 2 || h[0] != "goalqueue" || h[1] != "v" + I(kQueueVersion))
        return Fail(err, "goalqueue: bad header (version / field count)");
    GoalQueue q;
    std::string val[nk];
    for (size_t i = 0; i < nk; ++i)
    {
        const std::string want = std::string(keys[i]) + "=";
        if (h[i + 2].compare(0, want.size(), want) != 0)
            return Fail(err, "goalqueue: field '" + h[i + 2] + "' where '" + keys[i] + "' belongs");
        val[i] = h[i + 2].substr(want.size());
    }
    const auto flag = [](const std::string& v, bool& b) { return (v == "0" || v == "1") && ((b = v == "1"), true); };
    if (!DecU(val[0], q.nextId) || !DecU(val[1], q.stepKey) || !DecU(val[2], q.story.lastReal) ||
        !DecU(val[3], q.mission.survTitle) || !flag(val[4], q.mission.survMission) || !flag(val[5], q.mission.survEnded) ||
        !DecU(val[6], q.mission.survSystem) || !flag(val[7], q.mission.survival) ||
        !DecUList(val[8], q.story.seenAlive) || !DecUList(val[9], q.story.seenListed))
        return Fail(err, "goalqueue: bad header value");
    for (size_t i = 1; i + 1 < lines.size(); ++i)
    {
        GoalRecord r;
        std::string e;
        if (!ParseRecord(lines[i], r, &e))
            return Fail(err, "line " + std::to_string(i + 1) + ": " + e);
        for (const GoalRecord& x : q.entries)
            if (x.id == r.id)
                return Fail(err, "line " + std::to_string(i + 1) + ": goal id " + U(r.id) + " repeated");
        q.entries.push_back(r);
    }
    out = q;
    return true;
}
} // namespace goalqueue
