// The arbiter's executor, its pure half (situation_exec.h; A37_PLAN S7): the side-effect gate, the plan comparison, the
// ObjectiveStart arguments, the pause facts and the resume's restore. No game, no statics.
#include "situation_exec.h"

#include <cmath>
#include <cstring>

namespace situation
{
namespace
{
float Dist3(const float a[3], const float b[3])
{
    const float d[3] = {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
    return std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
}
// a trip that docks with its object at its end (a base, a ring, a gate / hole, an airlock): the dock request side effect
bool DocksAtEnd(const Plan& p) { return p.kind == Plan::Kind::Travel && p.object && !p.noDock; }
Gate Verdict(GateVerdict v, const std::string& kind, const char* why)
{
    Gate g;
    g.verdict = v, g.kind = kind, g.why = why ? why : "";
    return g;
}
} // namespace

const char* GateVerdictName(GateVerdict v)
{
    switch (v)
    {
    case GateVerdict::Go: return "go";
    case GateVerdict::Keep: return "keep";
    case GateVerdict::Refuse: return "refused";
    case GateVerdict::Adopt: return "adopt";
    default: return "?";
    }
}

std::string SideEffectKind(const Plan& p, const std::string& mode)
{
    switch (p.kind)
    {
    case Plan::Kind::None: return "";
    case Plan::Kind::Mode: return mode;
    case Plan::Kind::Travel: return DocksAtEnd(p) ? "dock-trip" : "travel";
    case Plan::Kind::HoldAt: return "hold";
    case Plan::Kind::Dock: return "dock";
    case Plan::Kind::Pass: return "pass";
    case Plan::Kind::Shoot: return "shoot";
    case Plan::Kind::Waypoints: return "waypoints";
    default: return "?";
    }
}

Gate CheckSideEffect(const GateFacts& f, const Plan& p, const std::string& mode)
{
    const std::string kind = SideEffectKind(p, mode);
    if (p.kind == Plan::Kind::None)
        return Verdict(GateVerdict::Go, kind, nullptr); // (nothing is started)
    if (!f.inSpace)
        return Verdict(GateVerdict::Refuse, kind, "not in space");
    // no lane / jump while in one (a start now would break the ride: the game flies)
    if (f.lane)
        return Verdict(GateVerdict::Refuse, kind, "riding a lane");
    if (f.jumping)
        return Verdict(GateVerdict::Refuse, kind, "jumping");
    if (p.kind == Plan::Kind::Dock || DocksAtEnd(p))
    {
        if (f.gameDocks)
            return Verdict(GateVerdict::Refuse, kind, "the game docks us");
        // the game's dock to that object already runs: no second request (our own mode on it: keep, else refused - the
        // supervisor decides again once it is through)
        if (f.dockInProgress && f.dockPortObject && f.dockPortObject == p.object)
            return Verdict(f.mode != "off" ? GateVerdict::Keep : GateVerdict::Refuse, kind, "the game's dock to it runs");
        if (p.kind == Plan::Kind::Dock && f.mode == "dock" && f.target == p.object)
            return Verdict(GateVerdict::Keep, kind, "our dock to it runs");
        if (f.dockReplyPending && f.mode == "dock" && f.target != p.object)
            return Verdict(GateVerdict::Refuse, kind, "a dock reply is pending");
    }
    if (p.kind == Plan::Kind::Mode && mode == "formation" && p.object && f.leader == p.object && f.mode != "formation")
        return Verdict(GateVerdict::Adopt, kind, "already in its formation");
    return Verdict(GateVerdict::Go, kind, nullptr);
}

std::string SideEffectLine(const Gate& g, u32 target, u32 step, const std::string& rule)
{
    char b[256];
    snprintf(b, sizeof(b), "side-effect %s target=%u step=0x%08x rule=%s", g.kind.empty() ? "none" : g.kind.c_str(), target,
             step, rule.c_str());
    std::string s = b;
    if (g.verdict != GateVerdict::Go)
        s += std::string(" ") + GateVerdictName(g.verdict) + " why=" + g.why;
    return s;
}

std::string PlanDiff(const Plan& a, const Plan& b)
{
    std::string d;
    const auto add = [&](const std::string& s) { d += (d.empty() ? "" : ",") + s; };
    if (a.kind != b.kind)
    {
        add(std::string("kind ") + PlanKindName(a.kind) + "/" + PlanKindName(b.kind));
        return d; // (the rest compares different things)
    }
    if (a.kind == Plan::Kind::None)
        return d;
    if (a.object != b.object)
        add("object " + std::to_string(a.object) + "/" + std::to_string(b.object));
    if (a.hasPoint != b.hasPoint)
        add("point given/not");
    else if (a.hasPoint)
    {
        const float m = Dist3(a.point, b.point);
        if (!(m <= 1.0f))
        {
            char t[48];
            snprintf(t, sizeof(t), "point %.0f m", m);
            add(t);
        }
        if (a.system != b.system)
            add("system");
    }
    if ((a.kind == Plan::Kind::Travel || a.kind == Plan::Kind::Waypoints) && a.noDock != b.noDock)
        add("dock");
    if (a.kind == Plan::Kind::Dock && a.toward != b.toward)
        add("toward");
    if (a.kind == Plan::Kind::Pass && !(std::fabs(a.radius - b.radius) <= 1.0f))
        add("radius");
    if (a.fly != b.fly)
        add("fly given/not");
    else if (a.fly && (!(Dist3(a.flyDir, b.flyDir) <= 0.01f) || !(std::fabs(a.flyRadius - b.flyRadius) <= 1.0f) ||
                       !(std::fabs(a.flyAhead - b.flyAhead) <= 1.0f)))
        add("fly");
    return d;
}

StartArgs StartArgsOf(const Proposal& p)
{
    StartArgs a;
    a.mode = p.mode, a.target = p.target, a.action = p.action;
    a.objectiveStart = !(p.rule == "held" || p.rule == "none" || p.rule.empty() || (p.rule == "leash" && p.keep));
    return a;
}

goalqueue::PauseFacts PauseFactsOf(const PauseMark& m, const Situation& now, double nowMs)
{
    const Snapshot& s = now.facts;
    goalqueue::PauseFacts f;
    f.pausedMs = nowMs >= m.atMs ? nowMs - m.atMs : 1e12; // (the clock went back: a load - long)
    const u32 sys = s.pilotOk ? s.pilotSystem : s.system;
    f.systemChanged = m.system && sys && sys != m.system;
    f.docked = m.docked || !s.pilotOk;
    bool job = false;
    for (int i = -1; i < static_cast<int>(s.entries.size()); ++i)
        job = job || !(i < 0 ? s.step : s.entries[static_cast<size_t>(i)]).job.empty();
    job = job || now.kindSource == KindSource::Job;
    f.jobTaken = job && now.stepKey != m.stepKey; // (a job's step now, not the one paused at)
    f.movedM = m.posKnown && s.pilotOk && !f.systemChanged ? Dist3(m.pos, s.pos) : 0;
    return f;
}

Restore ResumeOf(const goalqueue::GoalQueue* paused, const Situation& sit, const goalqueue::PauseFacts& pause,
                 const goalqueue::ResumePolicy& pol, goalqueue::GoalQueue& queue)
{
    using namespace goalqueue;
    Restore r;
    const auto stepEntry = [&](const GoalQueue& q) -> const GoalRecord*
    {
        for (const GoalRecord& e : q.entries)
            if (e.type == Type::MissionStep && e.origin != Origin::Player && e.stepKey == sit.stepKey)
                return &e;
        return nullptr;
    };
    if (!paused)
    {
        queue = Rebuild(sit); // (cold: the situation alone)
        r.cold = true;
        r.choice = Head(queue) ? ResumeChoice::Ask : ResumeChoice::Nothing;
        r.why = "no paused queue (the first start in this game)";
    }
    else
    {
        const Resumed rs = Resume(*paused, sit, pause, pol);
        queue = rs.queue;
        r.choice = rs.choice, r.why = rs.why;
        r.sameStep = rs.choice == ResumeChoice::Silent && stepEntry(*paused) != nullptr;
    }
    if (const GoalRecord* st = stepEntry(queue))
        r.memory = st->memory; // (silent + same step: Resume put the paused memory there; else the rebuilt entry's)
    for (const GoalRecord& e : queue.entries)
        if (e.origin == Origin::Story && e.type == Type::Dock && !e.parent && e.subject.kind == Ref::Kind::Object)
            r.lastDock = e.subject.id;
    r.lastReal = queue.story.lastReal;
    return r;
}
} // namespace situation
