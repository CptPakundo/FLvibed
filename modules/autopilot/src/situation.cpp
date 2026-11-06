// The situation assessor (situation.h): pure - the snapshot in, the situation out; no game, no statics.
#include "situation.h"
#include "situation_io.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>

namespace situation
{
// The game's object type bits the rules read (space::Type; core\game\space.h - kept here so the assessor needs no game
// header)
constexpr u32 kTJumpGate = 1u << 6, kTLaneRing = 1u << 7, kTAirlock = 1u << 10, kTJumpHole = 1u << 11;
constexpr u32 kAwaitingText = 21660; // "Awaiting Mission Objective"
constexpr u32 kBlankText = 1;        // " "
constexpr int kOpDock = 7;           // pub::AI::OpType Dock (space::Pilot::op)

const char* VerbName(Verb v)
{
    switch (v)
    {
    case Verb::Dock: return "dock";
    case Verb::Escort: return "escort";
    case Verb::Fight: return "fight";
    case Verb::Survive: return "survive";
    default: return "none";
    }
}
Verb VerbFromName(const std::string& s)
{
    for (Verb v : {Verb::Dock, Verb::Escort, Verb::Fight, Verb::Survive})
        if (s == VerbName(v))
            return v;
    return Verb::None;
}

Verb VerbOf(const std::string& text)
{
    std::string s = text;
    for (char& ch : s)
        ch = static_cast<char>(tolower(static_cast<unsigned char>(ch)));
    // (the rules and their reasons: autopilot_objective.cpp ObjectiveVerb - a word or phrase at a word boundary)
    const auto has = [&](const char* w)
    {
        for (size_t at = s.find(w); at != std::string::npos; at = s.find(w, at + 1))
            if (at == 0 || !isalpha(static_cast<unsigned char>(s[at - 1])))
                return true;
        return false;
    };
    const auto word = [&](const char* w)
    {
        const size_t n = strlen(w);
        for (size_t at = s.find(w); at != std::string::npos; at = s.find(w, at + 1))
            if ((at == 0 || !isalpha(static_cast<unsigned char>(s[at - 1]))) &&
                (at + n >= s.size() || !isalpha(static_cast<unsigned char>(s[at + n]))))
                return true;
        return false;
    };
    if (has("dock") || has("land ") || (has("enter ") && !has("formation")) || word("bar"))
        return Verb::Dock;
    if (has("defend yourself") || has("survive") || has("stay alive") || has("escape"))
        return Verb::Survive;
    if (has("escort") || has("protect") || has("defend") || has("guard") || has("stay with") || has("formation"))
        return Verb::Escort;
    if (has("destroy") || has("kill") || has("eliminate") || has("attack") || has("disable"))
        return Verb::Fight;
    return Verb::None;
}

const char* PlaceholderName(Placeholder p)
{
    switch (p)
    {
    case Placeholder::NoObjective: return "no_objective";
    case Placeholder::Awaiting: return "awaiting";
    case Placeholder::Blank: return "blank";
    default: return "none";
    }
}
const char* StandInName(StandIn s)
{
    switch (s)
    {
    case StandIn::Live: return "live";
    case StandIn::IniLast: return "ini_last";
    case StandIn::Title: return "title";
    default: return "none";
    }
}
const char* KindSourceName(KindSource k)
{
    switch (k)
    {
    case KindSource::Job: return "job";
    case KindSource::Script: return "script";
    case KindSource::Verb: return "verb";
    case KindSource::Unknown: return "unknown";
    default: return "none";
    }
}
const char* SubjectSourceName(SubjectSource s)
{
    switch (s)
    {
    case SubjectSource::Ref: return "ref";
    case SubjectSource::Nav: return "nav";
    case SubjectSource::Mark: return "mark";
    case SubjectSource::Goal: return "goal";
    default: return "none";
    }
}
const char* HeldName(Held h)
{
    switch (h)
    {
    case Held::Docked: return "docked";
    case Held::NoShip: return "no_ship";
    case Held::MissionHolds: return "mission_holds";
    case Held::GameDock: return "game_dock";
    case Held::Lane: return "lane";
    case Held::Jump: return "jump";
    default: return "none";
    }
}
const char* ThreatWhyName(ThreatWhy t)
{
    switch (t)
    {
    case ThreatWhy::Attacker: return "attacker";
    case ThreatWhy::Battle: return "battle";
    case ThreatWhy::PreHostile: return "pre_hostile";
    default: return "pointed";
    }
}
const char* AchievedName(Achieved a)
{
    switch (a)
    {
    case Achieved::Yes: return "yes";
    case Achieved::No: return "no";
    default: return "unknown";
    }
}

u32 LegacyStepKey(const SnapStep& st)
{
    // (ObjectiveSupervise's expression, character for character: the in-game cross-check compares the two)
    return st.textId * 2654435761u ^ (st.refCount ? st.refs[0] : 0) ^
           static_cast<u32>(st.hasNav ? st.nav[0] * 7 + st.nav[2] * 13 : 0);
}

static u32 Fnv(const void* data, size_t n, u32 h = 2166136261u)
{
    const unsigned char* p = static_cast<const unsigned char*>(data);
    for (size_t i = 0; i < n; ++i)
        h = (h ^ p[i]) * 16777619u;
    return h;
}

static float Dist3(const float a[3], const float b[3])
{
    const float d[3] = {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
    return std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
}

static const char* CndOfKind(int kind)
{
    return kind == 1 ? "cnd_distship" : kind == 2 ? "cnd_distvec" : kind == 3 ? "cnd_destroyed" : "";
}

namespace
{
struct World
{
    std::vector<const SnapContact*> byDist; // nearest first, ties by id: the order of the contacts never matters
    const AssessPolicy& p;
    explicit World(const Snapshot& s, const AssessPolicy& pol) : p(pol)
    {
        for (const SnapContact& c : s.contacts)
            byDist.push_back(&c);
        std::sort(byDist.begin(), byDist.end(), [](const SnapContact* a, const SnapContact* b)
                  { return a->distance != b->distance ? a->distance < b->distance : a->id < b->id; });
    }
    const SnapContact* Find(u32 id) const
    {
        if (!id)
            return nullptr;
        for (const SnapContact* c : byDist)
            if (c->id == id)
                return c;
        return nullptr;
    }
    bool Hostile(const SnapContact& c) const
    {
        return c.ship ? c.attitude <= p.hostileAttitude : (c.attKnown && c.att <= p.hostileAttitude);
    }
    static bool Friendly(const SnapContact& c) { return c.ship ? c.attitude > 0 : (c.attKnown && c.att > 0); }
};
} // namespace

Situation Assess(const Snapshot& s, const AssessPolicy& p)
{
    Situation sit;
    sit.facts = s;
    const World w(s, p);
    // ---- the step: its key from the snapshot alone
    sit.textId = s.step.textId;
    sit.stepKey = LegacyStepKey(s.step);
    if (!s.marks.empty())
    {
        std::vector<u32> m = s.marks;
        std::sort(m.begin(), m.end());
        sit.marksKey = Fnv(m.data(), m.size() * sizeof(u32));
    }
    sit.step = sit.stepKey ^ (sit.marksKey * 0x9E3779B1u);
    // ---- placeholder, stand-in
    // a step a script knows: its text named by real objectives of exactly one script (mission.cpp's rule - 21660 is
    // "nn_objsoon", "mlog_awaiting_objective", ... in 15 scripts: no step of one mission)
    bool realDef = false;
    std::string realScript;
    for (const SnapObjDef& d : s.objDefs)
        if (d.nick != "null" && d.nick != "nn_blank")
        {
            if (!realDef)
                realScript = d.script;
            else if (d.script != realScript)
                realScript.clear();
            realDef = true;
        }
    const bool oneScript = realDef && !realScript.empty();
    if (!sit.textId)
        sit.placeholder = Placeholder::NoObjective;
    else if (sit.textId == kAwaitingText)
        sit.placeholder = Placeholder::Awaiting;
    else if (sit.textId == kBlankText || (!s.objDefs.empty() && !realDef))
        sit.placeholder = Placeholder::Blank;
    sit.known = oneScript || (!s.step.job.empty() && s.step.job != "ambiguous");
    // the stand-in of a placeholder: the live table's script (what the game itself arms now, A37 S2) before mission.cpp's
    // title / last-objective derivation
    if (s.goalsLive && sit.placeholder != Placeholder::None)
        sit.standIn = StandIn::Live;
    else if (s.goalsByTitle)
        sit.standIn = StandIn::Title;
    else if (s.goalsFromLast && s.standInText)
        sit.standIn = StandIn::IniLast, sit.standInText = s.standInText;
    sit.verb = s.step.verb;
    // A step with no words, target or navmarker and live hostile marks within reach is their fight (ObjectiveDecide's
    // markFight: the verb becomes Fight - before any stand-in, as there)
    bool markFight = false;
    if (sit.verb == Verb::None && !s.step.refCount && !s.step.hasNav)
        for (u32 id : s.marks)
            if (const SnapContact* c = w.Find(id))
                markFight = markFight || (c->ship && c->health != 0 && c->distance < p.pointedReach && w.Hostile(*c) &&
                                          !c->preHostile);
    if (markFight)
        sit.verb = Verb::Fight;
    // ---- the subject (ObjectiveDecide's target choice, read only)
    for (int i = 0; i < s.step.refCount && !sit.subject; ++i)
        if (s.step.refObj[i])
            sit.subject = s.step.refObj[i], sit.subjectSource = SubjectSource::Ref;
    if (sit.subject)
    {
        if (const SnapContact* c = w.Find(sit.subject))
            sit.subjectHasPos = true, memcpy(sit.subjectPos, c->pos, sizeof(sit.subjectPos));
    }
    else if (s.step.hasNav)
    {
        sit.subjectSource = SubjectSource::Nav;
        sit.subjectHasPos = true;
        memcpy(sit.subjectPos, s.step.nav, sizeof(sit.subjectPos));
    }
    else if (sit.verb != Verb::Fight)
    {
        for (size_t i = 0; i < s.marks.size() && !sit.subject; ++i)
            if (const SnapContact* c = w.Find(s.marks[i]))
                if (c->health != 0 && !w.Hostile(*c))
                {
                    sit.subject = c->id, sit.subjectSource = SubjectSource::Mark;
                    sit.subjectHasPos = true, memcpy(sit.subjectPos, c->pos, sizeof(sit.subjectPos));
                }
    }
    if (sit.subjectSource == SubjectSource::None && sit.standIn != StandIn::None)
        for (const SnapGoal& g : s.goals)
        {
            if (g.kind == 1 && g.placed)
                sit.subject = g.object, sit.subjectHasPos = true, memcpy(sit.subjectPos, g.objPos, sizeof(g.objPos));
            else if (g.kind == 2)
                sit.subjectHasPos = true, memcpy(sit.subjectPos, g.pos, sizeof(g.pos));
            else if (g.kind == 3)
                for (const SnapContact* c : w.byDist) // the nearest live one of its objects
                {
                    const bool mine = std::any_of(g.objects.begin(), g.objects.end(),
                                                  [&](const SnapGoalObj& o) { return o.id == c->id; });
                    if (mine && c->health != 0)
                    {
                        sit.subject = c->id, sit.subjectHasPos = true;
                        memcpy(sit.subjectPos, c->pos, sizeof(sit.subjectPos));
                        break;
                    }
                }
            if (sit.subjectHasPos)
            {
                sit.subjectSource = SubjectSource::Goal;
                break;
            }
        }
    if (sit.subjectHasPos && s.pilotOk)
        sit.subjectDist = Dist3(sit.subjectPos, s.pos);
    const SnapContact* subj = w.Find(sit.subject);
    // ---- the affordances of the subject
    if (subj)
    {
        sit.afford.dock = subj->dock;
        sit.afford.jump = (subj->type & (kTJumpGate | kTJumpHole)) != 0;
        sit.afford.airlock = (subj->type & kTAirlock) != 0;
        if (subj->type & kTLaneRing)
        {
            sit.afford.lane = -1;
            for (const SnapLane& l : s.lanes)
                if (l.ring == subj->id)
                    sit.afford.lane = l.openNext == 1 || l.openPrev == 1 ? 1 : (l.openNext == 0 || l.openPrev == 0) ? 0 : -1;
        }
    }
    // ---- the kind (A41) and its source: the job's text id > the script (NNObjective type + Cnd_) > the words
    using objkinds::GoalType;
    std::string jobKind, jobNote;
    for (int i = -1; i < static_cast<int>(s.entries.size()) && jobKind.empty(); ++i)
    {
        const SnapStep& e = i < 0 ? s.step : s.entries[static_cast<size_t>(i)];
        if (e.job == "ambiguous")
            jobNote = "job_ambiguous";
        else if (!e.job.empty())
            jobKind = e.job;
    }
    if (!jobKind.empty())
    {
        const objkinds::JobRow* r = objkinds::JobOf(jobKind.c_str());
        sit.kind = r ? r->type : GoalType::Unknown;
        sit.kindSource = KindSource::Job;
        sit.kindDetail = "job:" + jobKind + (r ? "" : ":no_row");
    }
    else if (markFight)
    {
        sit.kind = GoalType::Attack;
        sit.kindSource = KindSource::Verb;
        sit.kindDetail = "mark_fight";
    }
    else if (sit.placeholder != Placeholder::None && sit.standIn == StandIn::None)
    {
        // a placeholder with nothing standing in: the script's time (no objective at all: none)
        sit.kind = sit.placeholder == Placeholder::NoObjective ? GoalType::None : GoalType::Wait;
        sit.kindSource = sit.kind == GoalType::None ? KindSource::None : KindSource::Unknown;
        sit.kindDetail = PlaceholderName(sit.placeholder);
    }
    else
    {
        std::string type; // the [NNObjective] type of the step itself (never a placeholder's)
        for (const SnapObjDef& d : s.objDefs)
            if (oneScript && sit.placeholder == Placeholder::None && d.nick != "null" && d.nick != "nn_blank" && type.empty())
                type = d.type;
        const bool scripted = (sit.placeholder == Placeholder::None && oneScript) || sit.standIn != StandIn::None;
        std::string detail = sit.standIn != StandIn::None ? std::string(StandInName(sit.standIn)) : type;
        if (scripted)
        {
            // the subject's class first: a lane ring, a gate, an airlock, a base the words dock with, a ship
            if (subj && (subj->type & kTLaneRing))
                sit.kind = GoalType::TradeLane, detail += "+lane_ring";
            else if (subj && (subj->type & (kTJumpGate | kTJumpHole)))
                sit.kind = GoalType::Jump, detail += "+jump";
            else if (subj && (subj->type & kTAirlock))
                sit.kind = GoalType::Dock, detail += "+airlock";
            else if (subj && subj->dock && sit.verb == Verb::Dock)
                sit.kind = GoalType::Dock, detail += "+dock";
            else if (subj && subj->ship && w.Hostile(*subj))
                sit.kind = GoalType::Attack, detail += "+hostile_ship";
            else if (subj && subj->ship && sit.verb == Verb::Escort)
                sit.kind = GoalType::Escort, detail += "+friendly_ship";
            else
            {
                // the step's own condition (an objective is a trigger: M12's "Destroy all of the Nomad Ships" advances
                // within 8 km of Toledo): the first goal the step itself armed, else any
                const SnapGoal* g = nullptr;
                for (const SnapGoal& x : s.goals)
                    if (!g || (g->byEntry && !x.byEntry))
                        g = &x;
                // (live: no flying / destroy goal armed - the armed row that sets the next objective names the kind by
                // its Cnd_: M13 docked, exit_base Cnd_BaseExit -> defend_base; its own actions first, then the fewest
                // hops, then the table's order)
                const SnapLive* row = nullptr;
                if (!g && s.goalsLive)
                    for (const SnapLive& l : s.live)
                        if (l.state == 0 && !l.leads.empty() && l.leads != "FAIL" && l.hops >= 0 &&
                            (!row || l.hops < row->hops))
                            row = &l;
                std::string rowCnd;
                if (row)
                {
                    rowCnd = row->cond.empty() ? row->iniCond : row->cond;
                    for (char& ch : rowCnd)
                        ch = static_cast<char>(tolower(static_cast<unsigned char>(ch)));
                }
                const objkinds::CndRow* cr = rowCnd.empty() ? nullptr : objkinds::CndOf(rowCnd.c_str());
                if (g)
                    sit.kind = g->kind == 3 ? GoalType::Attack : GoalType::FlyTo, detail += std::string("+") + CndOfKind(g->kind);
                else if (cr)
                    sit.kind = cr->type, detail += "+live:" + rowCnd;
                else if (type == "navmarker")
                    sit.kind = GoalType::FlyTo;
            }
            if (sit.kind != GoalType::None)
                sit.kindSource = KindSource::Script, sit.kindDetail = detail;
        }
        if (sit.kindSource == KindSource::None)
        {
            switch (sit.verb)
            {
            case Verb::Dock:
                sit.kind = sit.afford.lane != -2 ? GoalType::TradeLane : sit.afford.jump ? GoalType::Jump : GoalType::Dock;
                break;
            case Verb::Escort: sit.kind = GoalType::Escort; break;
            case Verb::Fight: sit.kind = GoalType::Attack; break;
            case Verb::Survive: sit.kind = GoalType::Defend; break;
            default: sit.kind = sit.subjectSource != SubjectSource::None ? GoalType::FlyTo : GoalType::Wait; break;
            }
            sit.kindSource = KindSource::Verb;
            sit.kindDetail = std::string("verb:") + VerbName(sit.verb) + (jobNote.empty() ? "" : ";" + jobNote);
        }
    }
    // ---- what holds the ship (ObjectiveSupervise's order: the mission, the game's dock, a lane, a jump)
    if (!s.inSpace)
        sit.held = s.base ? Held::Docked : Held::NoShip;
    else if (s.missionHolds)
        sit.held = Held::MissionHolds;
    else
    {
        // GameDocksUs over the snapshot: not our own dock; the dock port held, or a mission's grant with the Dock
        // behaviour or still fresh - except a granted airlock within reach (flown through, never waited at)
        const bool granted = s.sinceGrantMs >= 0;
        bool airlockNear = false;
        for (const SnapContact* c : w.byDist)
            airlockNear = airlockNear || ((c->type & kTAirlock) && c->distance < p.airlockReach);
        const bool flyThrough = granted && s.airlockFlyThrough && s.objOn && !s.dockInProgress && airlockNear;
        const bool gameDock = !s.ourDock && !flyThrough &&
                              (s.dockInProgress || (granted && s.op == kOpDock) || (granted && s.sinceGrantMs < p.grantDockMs));
        if (gameDock)
            sit.held = Held::GameDock;
        else if (s.laneRide)
            sit.held = Held::Lane;
        else if (s.jumping)
            sit.held = Held::Jump;
    }
    // ---- threats (ObjectiveThreat's rules + the pre-aggro ships); the mission's pointers = its marks and the
    // objective's resolved refs (S1 leaves out the run's script marks: memory)
    std::vector<u32> pointed = s.marks;
    for (int i = 0; i < s.step.refCount; ++i)
        if (s.step.refObj[i])
            pointed.push_back(s.step.refObj[i]);
    for (const SnapContact* cp : w.byDist)
    {
        const SnapContact& c = *cp;
        if (c.health == 0)
            continue;
        if (c.ship && c.preHostile)
        {
            sit.threats.push_back({c.id, ThreatWhy::PreHostile, c.distance});
            continue;
        }
        if (!w.Hostile(c))
            continue;
        const bool pt = std::find(pointed.begin(), pointed.end(), c.id) != pointed.end();
        if (s.survival)
        {
            if (c.ship && c.targetsPlayer && c.distance < p.survivalReach)
                sit.threats.push_back({c.id, ThreatWhy::Attacker, c.distance});
            else if (sit.verb == Verb::Fight && pt && c.ship && c.distance < p.pointedReach)
                sit.threats.push_back({c.id, ThreatWhy::Pointed, c.distance});
            continue;
        }
        if (pt && c.distance < p.pointedReach && (c.ship || c.maxHit < 1e30f))
            sit.threats.push_back({c.id, ThreatWhy::Pointed, c.distance});
        else if (c.ship && c.targetsPlayer && c.distance < p.attackerReach)
            sit.threats.push_back({c.id, ThreatWhy::Attacker, c.distance});
        else if (c.ship && c.targetId && c.distance < p.battleReach)
        {
            const SnapContact* f = w.Find(c.targetId);
            if (f && f->id != c.id && World::Friendly(*f))
                sit.threats.push_back({c.id, ThreatWhy::Battle, c.distance});
        }
    }
    // ---- constraints
    for (const SnapLeash& l : s.leashes)
    {
        Constraint k;
        k.kind = Constraint::Kind::Leash, k.object = l.object, k.name = l.name, k.dist = l.dist;
        k.armed = s.goalsLive ? 1 : -1; // (live: ArmedLeashes returns the armed ones only)
        sit.constraints.push_back(k);
    }
    if (s.nag.has)
    {
        Constraint k;
        k.kind = Constraint::Kind::Nag, k.object = s.nag.object, k.name = s.nag.name, k.dist = s.nag.dist, k.armed = 1;
        sit.constraints.push_back(k);
    }
    for (int i = -1; i < static_cast<int>(s.entries.size()); ++i)
    {
        const SnapStep& e = i < 0 ? s.step : s.entries[static_cast<size_t>(i)];
        if (e.job.size() > 16 && e.job.compare(e.job.size() - 16, 16, "_dont_kill_ships") == 0)
        {
            Constraint k;
            k.kind = Constraint::Kind::NeverAttack, k.name = e.job, k.armed = 1;
            sit.constraints.push_back(k);
            break;
        }
    }
    // ---- the script goals: their live state and whether they are achieved
    const auto view = [&](const SnapGoal& g, const char* source)
    {
        GoalView v;
        v.source = source, v.trigger = g.trigger, v.kind = g.kind, v.cnd = CndOfKind(g.kind), v.leads = g.leads;
        if (s.liveOk)
        {
            v.live = -1;
            for (const SnapLive& l : s.live)
                if (!l.nick.empty() && l.nick == g.trigger)
                    v.live = l.state;
        }
        if (g.kind == 1 && g.subject == "player" && g.placed && s.pilotOk)
            v.distance = Dist3(g.objPos, s.pos), v.achieved = v.distance <= g.dist ? Achieved::Yes : Achieved::No;
        else if (g.kind == 2 && g.subject == "player" && s.pilotOk)
            v.distance = Dist3(g.pos, s.pos), v.achieved = v.distance <= g.dist ? Achieved::Yes : Achieved::No;
        else if (g.kind == 3 && !g.objects.empty())
        {
            int alive = 0;
            for (const SnapGoalObj& o : g.objects)
            {
                const SnapContact* c = w.Find(o.id);
                if (c ? c->health != 0 : o.placed)
                    ++alive;
                if (c && c->health != 0 && (v.distance < 0 || c->distance < v.distance))
                    v.distance = c->distance;
            }
            const int dead = static_cast<int>(g.objects.size()) - alive;
            v.achieved = dead >= g.count ? Achieved::Yes : Achieved::No;
        }
        sit.goals.push_back(v);
    };
    for (const SnapGoal& g : s.goals)
        view(g, s.goalsLive ? "live" : "ini");
    for (const SnapGoal& g : s.nextGoals)
        view(g, "next");
    if (s.liveOk)
    {
        sit.liveArmed = 0;
        for (const SnapLive& l : s.live)
            sit.liveArmed += l.state == 0 ? 1 : 0;
    }
    // ---- the story source and the live view against the ini view (v2)
    sit.snapVersion = s.version;
    sit.storyLive = s.goalsLive;
    if (!s.goalsLive)
        sit.storyReason = !s.liveOk ? s.liveReason : "the live table's ids name no script";
    else
    {
        const auto goalKey = [](const SnapGoal& g) { return g.file + "|" + g.trigger; };
        const auto leashKey = [](const SnapLeash& l) { return std::to_string(l.object) + "|" + io::EncF(l.dist); };
        for (const SnapGoal& g : s.goals)
        {
            const bool both = std::any_of(s.iniGoals.begin(), s.iniGoals.end(),
                                          [&](const SnapGoal& x) { return goalKey(x) == goalKey(g); });
            if (both)
                ++sit.diffBoth;
            else
                sit.diff.push_back({"goal", "live", g.trigger, g.leads});
        }
        for (const SnapGoal& g : s.iniGoals)
            if (!std::any_of(s.goals.begin(), s.goals.end(), [&](const SnapGoal& x) { return goalKey(x) == goalKey(g); }))
                sit.diff.push_back({"goal", "ini", g.trigger, g.leads});
        for (const SnapLeash& l : s.leashes)
        {
            const bool both = std::any_of(s.iniLeashes.begin(), s.iniLeashes.end(),
                                          [&](const SnapLeash& x) { return leashKey(x) == leashKey(l); });
            if (both)
                ++sit.diffBoth;
            else
                sit.diff.push_back({"leash", "live", l.name, io::EncF(l.dist)});
        }
        for (const SnapLeash& l : s.iniLeashes)
            if (!std::any_of(s.leashes.begin(), s.leashes.end(), [&](const SnapLeash& x) { return leashKey(x) == leashKey(l); }))
                sit.diff.push_back({"leash", "ini", l.name, io::EncF(l.dist)});
    }
    // ---- the hash: over the description's lines after the first
    const std::string d = Describe(sit);
    const size_t nl = d.find('\n');
    sit.hash = Fnv(d.data() + (nl == std::string::npos ? d.size() : nl + 1),
                   nl == std::string::npos ? 0 : d.size() - nl - 1);
    return sit;
}

std::string Describe(const Situation& sit)
{
    using io::EncStr;
    char b[512];
    std::string o;
    snprintf(b, sizeof(b), "situation v1 hash=0x%08x\n", sit.hash);
    o += b;
    snprintf(b, sizeof(b), "step key=0x%08x marks=0x%08x id=0x%08x text=%u placeholder=%s known=%d standin=%s standin_text=%u\n",
             sit.stepKey, sit.marksKey, sit.step, sit.textId, PlaceholderName(sit.placeholder), sit.known ? 1 : 0,
             StandInName(sit.standIn), sit.standInText);
    o += b;
    snprintf(b, sizeof(b), "kind type=%s source=%s detail=%s verb=%s\n", objkinds::GoalTypeName(sit.kind),
             KindSourceName(sit.kindSource), EncStr(sit.kindDetail).c_str(), VerbName(sit.verb));
    o += b;
    if (sit.subjectHasPos)
        snprintf(b, sizeof(b), "subject source=%s id=%u pos=%.0f,%.0f,%.0f dist=%.0f\n", SubjectSourceName(sit.subjectSource),
                 sit.subject, sit.subjectPos[0], sit.subjectPos[1], sit.subjectPos[2], sit.subjectDist);
    else
        snprintf(b, sizeof(b), "subject source=%s id=%u pos=- dist=-1\n", SubjectSourceName(sit.subjectSource), sit.subject);
    o += b;
    snprintf(b, sizeof(b), "held why=%s\n", HeldName(sit.held));
    o += b;
    for (const Threat& t : sit.threats)
    {
        snprintf(b, sizeof(b), "threat id=%u why=%s dist=%.0f\n", t.id, ThreatWhyName(t.why), t.distance);
        o += b;
    }
    for (const Constraint& k : sit.constraints)
    {
        const char* kind = k.kind == Constraint::Kind::Leash ? "leash" : k.kind == Constraint::Kind::Nag ? "nag" : "never_attack";
        snprintf(b, sizeof(b), "constraint kind=%s object=%u name=%s dist=%.0f armed=%d\n", kind, k.object,
                 EncStr(k.name).c_str(), k.dist, k.armed);
        o += b;
    }
    for (const GoalView& g : sit.goals)
    {
        snprintf(b, sizeof(b), "goal source=%s trigger=%s kind=%d cnd=%s leads=%s live=%d achieved=%s dist=%.0f\n",
                 g.source.c_str(), EncStr(g.trigger).c_str(), g.kind, g.cnd.c_str(), EncStr(g.leads).c_str(), g.live,
                 AchievedName(g.achieved), g.distance);
        o += b;
    }
    snprintf(b, sizeof(b), "live armed=%d\n", sit.liveArmed);
    o += b;
    if (sit.snapVersion >= 2) // (v1 snapshots: their hash as S1 computed it)
    {
        size_t live = 0, ini = 0;
        for (const Situation::Diff& d : sit.diff)
            (d.only == "live" ? live : ini) += 1;
        snprintf(b, sizeof(b), "story source=%s reason=%s both=%d only_live=%u only_ini=%u\n", sit.storyLive ? "live" : "ini",
                 EncStr(sit.storyReason).c_str(), sit.diffBoth, static_cast<unsigned>(live), static_cast<unsigned>(ini));
        o += b;
        for (const Situation::Diff& d : sit.diff)
        {
            snprintf(b, sizeof(b), "diff %s only=%s name=%s detail=%s\n", d.what.c_str(), d.only.c_str(),
                     EncStr(d.name).c_str(), EncStr(d.detail).c_str());
            o += b;
        }
    }
    snprintf(b, sizeof(b), "afford dock=%d lane=%d jump=%d airlock=%d\n", sit.afford.dock ? 1 : 0, sit.afford.lane,
             sit.afford.jump ? 1 : 0, sit.afford.airlock ? 1 : 0);
    o += b;
    return o;
}
} // namespace situation
