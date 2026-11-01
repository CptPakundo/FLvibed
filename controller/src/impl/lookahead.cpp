// LOOK's pure half (lookahead.h; controller\docs\LOOK_PLAN.md slice L0): the kill / fly verdict over the facts, the safety
// predicates that block a candidate, PSN's StageablePlace, the path cost, the log line. No game, no statics.
#include "lookahead.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace lookahead
{
namespace
{
float Dist3(const float a[3], const float b[3])
{
    const float d[3] = {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
    return std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
}

std::string Fmt(const char* fmt, ...)
{
    char b[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof(b), fmt, ap);
    va_end(ap);
    return b;
}

// The near side of a sphere (`c`, radius `r`) seen from `from`: 70 % of the radius on our side (the decision's aim,
// autopilot_objective.cpp script-goal); `from` itself when inside.
void NearSide(const float from[3], const float c[3], float r, float out[3])
{
    const float l = Dist3(from, c);
    if (l <= 0.7f * r || l < 1)
    {
        memcpy(out, from, 3 * sizeof(float));
        return;
    }
    for (int i = 0; i < 3; ++i)
        out[i] = c[i] + (from[i] - c[i]) / l * 0.7f * r;
}

// The leash a point breaks (its name), "" none: beyond 85 % of a found anchor's distance, less 0.7 x `r` (the decision's
// `leashed`: a goal's sphere reaches that much nearer).
std::string LeashBreak(const float p[3], float r, const LookFacts& f)
{
    for (const LeashAnchor& a : f.leashes)
        if (a.found && Dist3(p, a.pos) - 0.7f * r > kLeashFrac * a.dist)
            return a.name;
    return std::string();
}

// The towards nag a point breaks: outside the nag's radius and no nearer its target than the ship is now - going there
// does not close on it (the game samples every 20 s and counts >= 900 m nearer as closing, LOOK_PLAN 4a: a static test
// here; velocity is the acting slices' business).
bool NagBreak(const float p[3], const LookFacts& f)
{
    if (!f.nag.has || !f.nag.placed || !f.havePilot)
        return false;
    const float there = Dist3(p, f.nag.pos);
    return there > f.nag.radius && there >= Dist3(f.me, f.nag.pos);
}

std::string ShipName(const KillShip& s) { return s.nick + "=" + std::to_string(s.id); }
} // namespace

const char* KindName(Kind k)
{
    switch (k)
    {
    case Kind::Kill:
        return "kill";
    case Kind::Fly:
        return "fly";
    default:
        return "none";
    }
}

bool StageablePlace(const float p[3], const LookFacts& f, std::string* why)
{
    for (const KillGoal& k : f.kills)
        for (const KillShip& s : k.ships)
            if (!s.id && s.spawnKnown && Dist3(p, s.spawn) < kPsnRadius)
            {
                if (why)
                    *why = Fmt("%s spawn %.0f m", s.nick.c_str(), Dist3(p, s.spawn));
                return false;
            }
    if (f.nextPlace && f.nextDestroy && Dist3(p, f.nextPos) < kPsnRadius)
    {
        if (why)
            *why = Fmt("destroy objective %u's place %.0f m", f.nextText, Dist3(p, f.nextPos));
        return false;
    }
    return true;
}

float PathCost(const FlyGoal& g, const LookFacts& f, const FlyGoal* dest)
{
    const float v = f.flySpeed > 1 ? f.flySpeed : 300.0f;
    const float own = std::max(0.0f, Dist3(f.me, g.pos) - g.radius) / v;
    if (!dest || dest == &g || g.leadsOrder < 0 || dest->leadsOrder < 0 || g.leadsOrder >= dest->leadsOrder)
        return own;
    if (g.lane)
        return own + g.laneRideS + std::max(0.0f, Dist3(g.laneEnd, dest->pos) - dest->radius) / v;
    float q[3];
    NearSide(f.me, g.pos, g.radius, q);
    return own + std::max(0.0f, Dist3(q, dest->pos) - dest->radius) / v;
}

LookResult LookTarget(const LookFacts& f)
{
    LookResult r;
    // ---- (a) kill: the nearest ship of an armed / chained / next kill goal that the game shows alive, hostile, targetable
    const KillGoal* kg = nullptr;
    const KillShip* ks = nullptr;
    for (const KillGoal& k : f.kills)
        for (const KillShip& s : k.ships)
        {
            const std::string who = k.trigger + "/" + s.nick + ":";
            const char* code = !s.id                ? "unspawned"
                               : !s.alive           ? "dead"
                               : s.friendly         ? "friendly"
                               : s.preHostile       ? "pre-hostile"
                               : !s.hostile         ? "not-hostile"
                               : !s.targetable      ? "untargetable"
                               : k.gateFails        ? "gate-fails"
                                                    : nullptr;
            if (code)
            {
                r.blockers.push_back(who + code);
                continue;
            }
            const std::string leash = LeashBreak(s.pos, 0, f);
            if (!leash.empty())
            {
                r.blockers.push_back(who + "leash(" + leash + ")");
                continue;
            }
            if (NagBreak(s.pos, f))
            {
                r.blockers.push_back(who + "nag(" + f.nag.name + ")");
                continue;
            }
            if (!ks || s.dist < ks->dist)
                kg = &k, ks = &s;
        }
    // ---- (b) fly: the least-cost distance goal (PathCost toward the furthest-advancing candidate)
    std::vector<const FlyGoal*> ok;
    for (const FlyGoal& g : f.flies)
    {
        const std::string who = g.trigger + ":";
        if (!g.placed)
        {
            r.blockers.push_back(who + "unplaced");
            continue;
        }
        if (f.havePilot && Dist3(f.me, g.pos) <= g.radius)
        {
            r.blockers.push_back(who + "inside");
            continue;
        }
        float q[3];
        NearSide(f.me, g.pos, g.radius, q);
        const std::string leash = LeashBreak(g.pos, g.radius, f);
        if (!leash.empty())
        {
            r.blockers.push_back(who + "leash(" + leash + ")");
            continue;
        }
        if (NagBreak(q, f))
        {
            r.blockers.push_back(who + "nag(" + f.nag.name + ")");
            continue;
        }
        std::string why;
        if (!StageablePlace(q, f, &why))
        {
            r.blockers.push_back(who + "psn(" + why + ")");
            continue;
        }
        ok.push_back(&g);
    }
    if (f.nextPlace)
        r.nextOk = StageablePlace(f.nextPos, f, &r.nextWhy);
    if (ks)
    {
        r.kind = Kind::Kill;
        r.target = ks->id, r.ship = ks->nick, r.trigger = kg->trigger, r.leads = kg->leads, r.source = kg->source;
        memcpy(r.point, ks->pos, sizeof(r.point));
        r.dist = ks->dist;
    }
    else if (!ok.empty() && f.havePilot)
    {
        const FlyGoal* dest = nullptr;
        for (const FlyGoal* g : ok)
            if (!dest || g->leadsOrder > dest->leadsOrder)
                dest = g;
        const FlyGoal* best = nullptr;
        const FlyGoal* nearest = nullptr;
        float bestCost = 0, nearDist = 0;
        for (const FlyGoal* g : ok)
        {
            const float c = PathCost(*g, f, dest);
            if (!best || c < bestCost)
                best = g, bestCost = c;
            const float d = Dist3(f.me, g->pos) - g->radius;
            if (!nearest || d < nearDist)
                nearest = g, nearDist = d;
        }
        r.kind = Kind::Fly;
        r.trigger = best->trigger, r.leads = best->leads, r.source = best->source;
        NearSide(f.me, best->pos, best->radius, r.point);
        r.dist = Dist3(f.me, r.point);
        r.cost = bestCost;
        r.nearest = nearest->trigger;
        r.nearestCost = PathCost(*nearest, f, dest);
    }
    // the key: what makes the verdict (no distances: they change every decision)
    r.key = Fmt("%u|%s|%u|%s|%s|", f.textId, KindName(r.kind), r.target, r.trigger.c_str(), r.nextOk ? "" : "psn-next");
    for (const std::string& b : r.blockers)
        r.key += b.substr(0, b.find('(')) + ";";
    return r;
}

std::string Line(const LookResult& r, const LookFacts& f)
{
    std::string s = KindName(r.kind);
    if (r.kind == Kind::Kill)
    {
        const KillGoal* kg = nullptr;
        for (const KillGoal& k : f.kills)
            if (k.trigger == r.trigger)
                kg = &k;
        float hull = -1;
        if (kg)
            for (const KillShip& sh : kg->ships)
                if (sh.id == r.target)
                    hull = sh.hull;
        s += Fmt(" %s=%u trigger=%s -> %s src=%s dist=%.0f hull=%.2f gate=%.2f inv=%s", r.ship.c_str(), r.target,
                 r.trigger.c_str(), r.leads.c_str(), r.source.c_str(), r.dist, hull, kg ? kg->healthGate : 0.0f,
                 kg && kg->scriptInv ? Fmt("%.2f", kg->scriptInvFloor).c_str() : "-");
    }
    else if (r.kind == Kind::Fly)
        s += Fmt(" trigger=%s -> %s src=%s cost=%.1f to_go=%.0f nearest=%s/%.1f", r.trigger.c_str(), r.leads.c_str(),
                 r.source.c_str(), r.cost, r.dist, r.nearest.c_str(), r.nearestCost);
    s += Fmt(" text=%u verb=%s kills=%u flies=%u", f.textId, f.verb.c_str(), static_cast<unsigned>(f.kills.size()),
             static_cast<unsigned>(f.flies.size()));
    if (f.nag.has)
        s += Fmt(" nag=%s:%.0f/%.0f", f.nag.name.c_str(), f.nag.placed && f.havePilot ? Dist3(f.me, f.nag.pos) : -1.0f,
                 f.nag.radius);
    s += Fmt(" leashes=%u tethers=%u", static_cast<unsigned>(f.leashes.size()), static_cast<unsigned>(f.tethers.size()));
    if (f.nextPlace)
        s += r.nextOk ? Fmt(" psn_next=%u:ok", f.nextText) : Fmt(" psn_next=%u:refused", f.nextText);
    s += Fmt(" blocked=%u", static_cast<unsigned>(r.blockers.size()));
    for (size_t i = 0; i < r.blockers.size() && s.size() < 420; ++i)
        s += (i ? " " : ": ") + r.blockers[i];
    if (s.size() > 470)
        s = s.substr(0, 467) + "...";
    return s;
}

std::string Describe(const LookResult& r, const LookFacts& f)
{
    std::string s = Fmt("look %s text=%u verb=%s pilot=%d me=%.0f,%.0f,%.0f fly_speed=%.0f\n", KindName(r.kind),
                        f.textId, f.verb.c_str(), f.havePilot ? 1 : 0, f.me[0], f.me[1], f.me[2], f.flySpeed);
    if (r.kind != Kind::None)
        s += Fmt("  verdict trigger=%s -> %s src=%s target=%s=%u point=%.0f,%.0f,%.0f dist=%.0f cost=%.1f nearest=%s/%.1f\n",
                 r.trigger.c_str(), r.leads.c_str(), r.source.c_str(), r.ship.c_str(), r.target, r.point[0], r.point[1],
                 r.point[2], r.dist, r.cost, r.nearest.c_str(), r.nearestCost);
    for (const KillGoal& k : f.kills)
    {
        s += Fmt("  kill %s %d of %s src=%s hops=%d -> %s gate=%.2f%s inv=%s", k.trigger.c_str(), k.count,
                 k.label.c_str(), k.source.c_str(), k.hops, k.leads.c_str(), k.healthGate,
                 k.gateFails ? " gate-fails" : "",
                 k.scriptInv ? Fmt("%s:true,%d,%.2f", k.invTrigger.c_str(), k.scriptInvSecond ? 1 : 0, k.scriptInvFloor).c_str()
                             : "-");
        s += "\n";
        for (const KillShip& sh : k.ships)
            s += Fmt("    ship %s alive=%d hostile=%d pre=%d friendly=%d targetable=%d dist=%.0f hull=%.2f spawn=%s%s\n",
                     ShipName(sh).c_str(), sh.alive ? 1 : 0, sh.hostile ? 1 : 0, sh.preHostile ? 1 : 0,
                     sh.friendly ? 1 : 0, sh.targetable ? 1 : 0, sh.dist, sh.hull,
                     sh.spawnKnown ? Fmt("%.0f,%.0f,%.0f", sh.spawn[0], sh.spawn[1], sh.spawn[2]).c_str() : "-",
                     sh.invRead ? Fmt(" game_inv=%d,%d,%.2f", sh.inv ? 1 : 0, sh.invSecond ? 1 : 0, sh.invFloor).c_str()
                                : "");
    }
    const FlyGoal* dest = nullptr;
    for (const FlyGoal& g : f.flies)
        if (g.placed && (!dest || g.leadsOrder > dest->leadsOrder))
            dest = &g;
    for (const FlyGoal& g : f.flies)
        s += Fmt("  fly %s within %.0f of %s src=%s hops=%d entry=%d -> %s (order %d) placed=%d at=%.0f,%.0f,%.0f "
                 "cost=%.1f lane=%s%s\n",
                 g.trigger.c_str(), g.radius, g.what.c_str(), g.source.c_str(), g.hops, g.byEntry ? 1 : 0,
                 g.leads.c_str(), g.leadsOrder, g.placed ? 1 : 0, g.pos[0], g.pos[1], g.pos[2],
                 g.placed && f.havePilot ? PathCost(g, f, dest) : -1.0f, g.laneFrom.empty() ? "-" : g.laneFrom.c_str(),
                 g.lane ? Fmt(" ride=%.1fs end=%.0f,%.0f,%.0f", g.laneRideS, g.laneEnd[0], g.laneEnd[1], g.laneEnd[2]).c_str()
                        : "");
    if (f.nag.has)
        s += Fmt("  nag %s placed=%d at=%.0f,%.0f,%.0f radius=%.0f\n", f.nag.name.c_str(), f.nag.placed ? 1 : 0,
                 f.nag.pos[0], f.nag.pos[1], f.nag.pos[2], f.nag.radius);
    for (const LeashAnchor& a : f.leashes)
        s += Fmt("  leash %s %.0f m anchor=%s\n", a.name.c_str(), a.dist,
                 a.found ? Fmt("%.0f,%.0f,%.0f", a.pos[0], a.pos[1], a.pos[2]).c_str() : "none");
    for (const TetherFact& t : f.tethers)
        s += Fmt("  tether %s %s-%s dist=%.0f secs=%.0f inner=%.0f fails=%d gap=%.0f (reported only)\n",
                 t.trigger.c_str(), t.a.c_str(), t.b.c_str(), t.dist, t.secs, t.inner, t.fails ? 1 : 0, t.gap);
    if (f.nextPlace)
        s += Fmt("  next place %u destroy=%d at=%.0f,%.0f,%.0f stageable=%d %s\n", f.nextText, f.nextDestroy ? 1 : 0,
                 f.nextPos[0], f.nextPos[1], f.nextPos[2], r.nextOk ? 1 : 0, r.nextWhy.c_str());
    for (const std::string& b : r.blockers)
        s += "  blocked " + b + "\n";
    return s;
}

std::vector<std::string> InvincibleLines(const LookFacts& f)
{
    std::vector<std::string> out;
    for (const KillGoal& k : f.kills)
        if (k.scriptInv)
            for (const KillShip& s : k.ships)
                out.push_back(Fmt("invincible %s=%u trigger=%s script=%s:true,%d,%.2f game=%s hull=%.3f gate=%.2f",
                                  s.nick.c_str(), s.id, k.trigger.c_str(), k.invTrigger.c_str(),
                                  k.scriptInvSecond ? 1 : 0, k.scriptInvFloor,
                                  !s.id        ? "no-contact"
                                  : s.invRead  ? Fmt("%d,%d,%.3f", s.inv ? 1 : 0, s.invSecond ? 1 : 0, s.invFloor).c_str()
                                               : "unread",
                                  s.hull, k.healthGate));
    return out;
}

const char* TdVerdictName(TdVerdict v)
{
    switch (v)
    {
    case TdVerdict::Fight:
        return "fight";
    case TdVerdict::Stage:
        return "stage";
    case TdVerdict::Rest:
        return "rest";
    case TdVerdict::Defend:
        return "defend";
    case TdVerdict::Dock:
        return "dock";
    default:
        return "today";
    }
}

TdResult TimerDestroy(const TimerDestroyFacts& f)
{
    TdResult r;
    if (f.advance != "timer" || !f.havePilot)
        return r;
    // the default preset: the fight while it is safe, else only what shoots at us (no script place to go to)
    if (!f.speedrun)
    {
        const bool hullOk = f.hull >= kTdSafeHull, battOk = f.batteries >= kTdSafeBatteries;
        if (hullOk && battOk)
            return r.verdict = TdVerdict::Fight, r.why = "safe", r;
        r.why = !hullOk ? "unsafe(hull)" : "unsafe(batteries)";
        r.verdict = f.attacked ? TdVerdict::Defend : TdVerdict::Rest;
        return r;
    }
    // the script takes the next objective's dock already (M13: Toledo's ring unlocks ~5 s before "Dock with Planet
    // Toledo" shows): dock now, wherever we are - the dock is the way out of the fight too
    if (f.dockNow && f.nextObject && !f.nextDestroy)
        return r.verdict = TdVerdict::Dock, r.why = "unlocked", r;
    // PSN: never toward a destroy objective, nor within kPsnRadius of the step's own target (its battleship)
    bool stageable = f.nextPlace && !f.nextDestroy;
    if (stageable && f.stepPlace && Dist3(f.nextPos, f.stepPos) < kPsnRadius)
        stageable = false, r.why = "psn";
    if (!stageable)
    {
        if (r.why.empty())
            r.why = !f.nextPlace ? "no-place" : "psn(destroy)";
        r.verdict = f.attacked ? TdVerdict::Defend : TdVerdict::Rest;
        return r;
    }
    const float l = Dist3(f.me, f.nextPos);
    for (int i = 0; i < 3; ++i)
        r.point[i] = f.nextPos[i] - (l > 1 ? (f.nextPos[i] - f.me[i]) / l : 0.0f) * kTdStandOff;
    r.dist = l;
    // on the way the trip outruns what chases it (survival_stage's rule); at the place, what shoots at us is answered
    if (Dist3(f.me, r.point) > kTdArrive)
        return r.verdict = f.attacked && l < 1500.0f ? TdVerdict::Defend : TdVerdict::Stage, r.why = "next", r;
    r.why = "at-place";
    r.verdict = f.attacked ? TdVerdict::Defend : TdVerdict::Rest;
    return r;
}

TdResult AllyKill(const AllyKillFacts& f)
{
    TdResult r;
    if (!f.qualifies)
        return r.why = "no-rule", r;
    if (!f.speedrun)
        return r.why = "exposed", r;
    if (!f.havePilot)
        return r.why = "no-pilot", r;
    if (!f.target || !f.targetAlive)
        return r.why = "target-gone", r;
    if (!f.dockKnown)
        return r.why = "no-dock", r;
    r.dist = Dist3(f.me, f.dockPos);
    // the fallback (fails closed): nothing friendly on it and it does not lose hull - the allies will not do it
    if (f.fellBack)
        return r.verdict = TdVerdict::Fight, r.why = "fallback(kept)", r;
    if (f.allies == 0 && f.waitedS >= kAkWaitS && f.hullDrop < kAkHullDrop)
        return r.verdict = TdVerdict::Fight, r.why = "fallback", r;
    if (f.waitedS >= kAkAllyStallS && f.hullDrop < kAkHullDrop)
        return r.verdict = TdVerdict::Fight, r.why = "fallback(allies-stall)", r;
    if (r.dist > kAkAtDock)
    {
        for (int i = 0; i < 3; ++i)
            r.point[i] = f.dockPos[i] - (r.dist > 1 ? (f.dockPos[i] - f.me[i]) / r.dist : 0.0f) * kAkStandOff;
        if (f.attacked && r.dist < kAkDefendReach)
            return r.verdict = TdVerdict::Defend, r.why = "attacked(stage)", r;
        return r.verdict = TdVerdict::Stage, r.why = "to-dock", r;
    }
    memcpy(r.point, f.me, sizeof(r.point));
    if (f.attacked)
        return r.verdict = TdVerdict::Defend, r.why = "attacked(dock)", r;
    return r.verdict = TdVerdict::Rest, r.why = f.allies > 0 ? "allies" : "waiting", r;
}

SpawnFacts SpawnFactsOf(const mission::SpawnAfter& a)
{
    SpawnFacts f;
    f.hasPos = a.hasPos;
    memcpy(f.spawn, a.pos, sizeof(f.spawn));
    f.ordersFound = a.ordersFound;
    f.cmds = a.cmds;
    for (const mission::SpawnAfter::Other& o : a.others)
    {
        SpawnFacts::Other x;
        x.nick = o.nick, x.hasPos = o.hasPos, x.friendly = o.friendly;
        memcpy(x.pos, o.pos, sizeof(x.pos));
        f.others.push_back(x);
    }
    return f;
}

namespace
{
// An ini value's comma-separated fields, trimmed (ini::Fields' rule; the pure half reads no ini)
std::vector<std::string> Split(const std::string& v)
{
    std::vector<std::string> out;
    size_t at = 0;
    while (at <= v.size())
    {
        const size_t comma = v.find(',', at);
        std::string f = v.substr(at, comma == std::string::npos ? std::string::npos : comma - at);
        const size_t b = f.find_first_not_of(" \t"), e = f.find_last_not_of(" \t");
        out.push_back(b == std::string::npos ? std::string() : f.substr(b, e - b + 1));
        if (comma == std::string::npos)
            break;
        at = comma + 1;
    }
    return out;
}
bool IsPlayer(const std::string& s) { return _stricmp(s.c_str(), "player") == 0; }
} // namespace

bool FleeingSpawn(const SpawnFacts& f, float runTo[3], std::string* why)
{
    const auto no = [&](const std::string& w)
    {
        if (why)
            *why = w;
        return false;
    };
    if (!f.hasPos)
        return no("no-spawn-pos");
    if (!f.ordersFound || f.cmds.empty())
        return no("no-orders");
    bool haveGoto = false;
    for (const auto& [key, value] : f.cmds)
    {
        const std::vector<std::string> fs = Split(value);
        // anything aimed at the player: a follow, a goto to his ship, a range kept to him (no ObjList command attacks by
        // name - an AI with orders at or around the player engages him)
        bool atPlayer = key == "followplayer";
        for (size_t i = 0; i < fs.size() && i < 3; ++i)
            atPlayer = atPlayer || IsPlayer(fs[i]);
        if (atPlayer)
            return no("at-player(" + key + ")");
        if (key == "setpriority" || key == "breakformation" || key == "makenewformation" || key == "setlifetime" ||
            key == "avoidance")
            continue;
        if (haveGoto)
            continue;
        // GotoVec = <goto type>, x, y, z, ...; GotoSpline = <goto type>, x1, y1, z1, ..., x4, y4, z4, ...: its last point
        if ((key == "gotovec" || key == "gotospline") && fs.size() >= 4)
        {
            const size_t at = key == "gotospline" && fs.size() >= 13 ? 10 : 1;
            for (int i = 0; i < 3; ++i)
                runTo[i] = static_cast<float>(atof(fs[at + i].c_str()));
            haveGoto = true;
            continue;
        }
        return no(key + " first");
    }
    if (!haveGoto)
        return no("no-goto");
    const float run = Dist3(f.spawn, runTo);
    if (run < kPsnRadius)
        return no(Fmt("short-run %.0f m", run));
    for (const SpawnFacts::Other& o : f.others)
    {
        if (o.friendly)
            continue;
        if (!o.hasPos)
            return no(o.nick + " unplaced");
        const float d = Dist3(o.pos, f.spawn);
        if (d < kPsnRadius)
            return no(Fmt("%s spawns %.0f m off", o.nick.c_str(), d));
    }
    if (why)
        *why = Fmt("flees %.0f m", run);
    return true;
}

StageResult StagePoint(const float spawn[3], const float runTo[3], const float* anchor, float leash)
{
    StageResult r;
    float u[3] = {runTo[0] - spawn[0], 0, runTo[2] - spawn[2]};
    const float ul = std::sqrt(u[0] * u[0] + u[2] * u[2]);
    if (ul < 1)
        return r.why = "no-line", r;
    u[0] /= ul, u[2] /= ul;
    float n[3] = {-u[2], 0, u[0]};
    if (anchor && (anchor[0] - spawn[0]) * n[0] + (anchor[2] - spawn[2]) * n[2] < 0)
        n[0] = -n[0], n[2] = -n[2];
    for (int i = 0; i < 3; ++i)
        r.p[i] = spawn[i] - kStageBack * u[i] + kStageSide * n[i];
    r.p[1] = spawn[1];
    memcpy(r.u, u, sizeof(r.u));
    if (anchor)
    {
        r.dAnchor = Dist3(r.p, anchor);
        const float lim = kStageLeashFrac * leash;
        if (leash > 0 && r.dAnchor > lim)
        {
            for (int i = 0; i < 3; ++i)
                r.p[i] = anchor[i] + (r.p[i] - anchor[i]) * (lim / r.dAnchor);
            r.dAnchor = lim;
            r.clamped = true;
        }
    }
    r.dSpawn = Dist3(r.p, spawn);
    r.ok = true;
    r.why = r.clamped ? "clamped" : "free";
    return r;
}

StageResult StagePoint(const float spawn[3], const float runTo[3], const std::vector<StageLeash>& leashes,
                       std::string* bound, float back, float side)
{
    StageResult r;
    if (bound)
        bound->clear();
    float u[3] = {runTo[0] - spawn[0], 0, runTo[2] - spawn[2]};
    const float ul = std::sqrt(u[0] * u[0] + u[2] * u[2]);
    if (ul < 1)
        return r.why = "no-line", r;
    u[0] /= ul, u[2] /= ul;
    memcpy(r.u, u, sizeof(r.u));
    float n[3] = {-u[2], 0, u[0]};
    // the tightest leash first (its anchor's side breaks a tie)
    const StageLeash* tight = nullptr;
    for (const StageLeash& l : leashes)
        if (l.radius > 0 && l.frac > 0 && (!tight || l.frac * l.radius < tight->frac * tight->radius))
            tight = &l;
    if (tight && (tight->anchor[0] - spawn[0]) * n[0] + (tight->anchor[2] - spawn[2]) * n[2] < 0)
        n[0] = -n[0], n[2] = -n[2];
    // per side: the wanted point, pulled into every ball by turns (64 rounds: two or three balls settle in a few)
    struct Try
    {
        float p[3];
        float moved;
        bool inside;
        std::string bound;
    } tries[2];
    for (int s = 0; s < 2; ++s)
    {
        Try& t = tries[s];
        const float sg = s == 0 ? 1.0f : -1.0f;
        float want[3];
        for (int i = 0; i < 3; ++i)
            want[i] = spawn[i] - back * u[i] + sg * side * n[i];
        want[1] = spawn[1];
        memcpy(t.p, want, sizeof(t.p));
        std::vector<bool> pulled(leashes.size(), false);
        for (int round = 0; round < 64; ++round)
        {
            bool any = false;
            for (size_t k = 0; k < leashes.size(); ++k)
            {
                const StageLeash& l = leashes[k];
                if (l.radius <= 0 || l.frac <= 0)
                    continue;
                const float lim = l.frac * l.radius, d = Dist3(t.p, l.anchor);
                if (d > lim && d > 0)
                {
                    for (int i = 0; i < 3; ++i)
                        t.p[i] = l.anchor[i] + (t.p[i] - l.anchor[i]) * (lim / d);
                    pulled[k] = any = true;
                }
            }
            if (!any)
                break;
        }
        t.inside = true;
        for (const StageLeash& l : leashes)
            if (l.radius > 0 && l.frac > 0 && Dist3(t.p, l.anchor) > l.frac * l.radius + 1.0f)
                t.inside = false;
        t.moved = Dist3(t.p, want);
        for (size_t k = 0; k < leashes.size(); ++k)
            if (pulled[k])
                t.bound += (t.bound.empty() ? "" : ",") + leashes[k].name;
    }
    const Try* best = nullptr;
    for (const Try& t : tries)
        if (t.inside && (!best || t.moved < best->moved - 0.01f))
            best = &t;
    if (!best)
        return r.why = "no-point-in-leashes", r;
    memcpy(r.p, best->p, sizeof(r.p));
    r.dSpawn = Dist3(r.p, spawn);
    r.clamped = best->moved > 0.5f;
    if (bound)
        *bound = best->bound;
    // the anchor distance of the leash with the least room left (the log's)
    float room = 1e30f;
    for (const StageLeash& l : leashes)
        if (l.radius > 0 && l.frac > 0)
        {
            const float d = Dist3(r.p, l.anchor);
            if (l.frac * l.radius - d < room)
                room = l.frac * l.radius - d, r.dAnchor = d;
        }
    if (r.dSpawn > kChargeMaxFromSpawn)
        return r.why = Fmt("far %.0f m", r.dSpawn), r;
    r.ok = true;
    r.why = r.clamped ? "clamped" : "free";
    return r;
}

bool StillFleeing(const float his[3], const float runTo[3]) { return Dist3(his, runTo) > kChargeRunDone; }

float ChargeDist(const ChargeProfile& p, float t)
{
    if (t <= 0)
        return 0;
    if (t <= p.slowS)
        return p.slow * t;
    const float ramp = p.topAtS - p.slowS;
    if (t <= p.topAtS && ramp > 0)
    {
        const float x = t - p.slowS, v = p.slow + (p.top - p.slow) * x / ramp;
        return p.slow * p.slowS + 0.5f * (p.slow + v) * x;
    }
    return p.slow * p.slowS + 0.5f * (p.slow + p.top) * std::max(0.0f, ramp) + p.top * (t - std::max(p.topAtS, p.slowS));
}

GapPrediction ChargeGap(float gap0, float ourLagS)
{
    GapPrediction g;
    g.lockT = std::max(kHisCharge.topAtS, ourLagS + kOurCharge.topAtS);
    g.minGap = gap0;
    for (float t = 0; t <= g.lockT + 1e-3f; t += 0.05f)
    {
        const float gap = gap0 + ChargeDist(kHisCharge, t) - ChargeDist(kOurCharge, t - ourLagS);
        g.minGap = std::min(g.minGap, gap);
    }
    g.lockGap = gap0 + ChargeDist(kHisCharge, g.lockT) - ChargeDist(kOurCharge, g.lockT - ourLagS);
    g.minGap = std::min(g.minGap, g.lockGap);
    return g;
}

KeepResult ChargeKeep(const float me[3], const float p[3], const float target[3], bool wasOn)
{
    KeepResult k;
    k.dP = Dist3(me, p);
    k.station = k.dP > kChargeKeep || (wasOn && k.dP > kChargeKeepIn);
    if (!k.station)
        return k;
    const float dt = Dist3(target, p);
    const float at = std::min(kChargeKeepAt, dt);
    for (int i = 0; i < 3; ++i)
        k.pos[i] = dt > 1 ? p[i] + (target[i] - p[i]) / dt * at : p[i];
    return k;
}

const char* ChaseVerdictName(ChaseVerdict v)
{
    switch (v)
    {
    case ChaseVerdict::Charge:
        return "charge";
    case ChaseVerdict::Coast:
        return "coast";
    case ChaseVerdict::EngineOn:
        return "engine-on";
    default:
        return "fight";
    }
}

ChaseResult ChaseStep(const ChaseFacts& f)
{
    ChaseResult r;
    const bool shieldOut = f.shield >= 0 && f.shield < kChaseShieldOut;
    if (f.coasting)
    {
        if (f.killed)
            return r.verdict = ChaseVerdict::EngineOn, r.why = "gone", r;
        if (shieldOut)
            return r.verdict = ChaseVerdict::EngineOn, r.why = "shield", r;
        if (f.gap > kChaseGunFrac * f.gunRange)
            return r.verdict = ChaseVerdict::EngineOn, r.why = "gap", r;
        return r.verdict = ChaseVerdict::Coast, r;
    }
    if (f.killed || f.gunRange <= 0)
        return r; // (Fight)
    // (`spawn_charge`: a fleeing ship is charged at once - from beyond the coast's near edge, or on with a charge already
    // running - whatever his speed; otherwise only a target faster than our engine)
    const bool fleeCharge = f.fleeing && (f.cruising || f.gap > kChaseCoastMin);
    if (f.hisSpeed <= f.engineTop && !fleeCharge)
        return r; // (Fight)
    if (f.cruising && f.mySpeed >= (f.fleeing ? kChargeCoastSpeed : kChaseCoastSpeed) && !shieldOut &&
        f.bearingDeg <= kChaseBearingDeg && f.gap >= kChaseCoastMin && f.gap <= kChaseCoastMax)
        return r.verdict = ChaseVerdict::Coast, r.why = "start", r;
    return r.verdict = ChaseVerdict::Charge, r;
}

KillGoal KillGoalOf(const mission::Goal& g, const char* source)
{
    KillGoal k;
    k.trigger = g.trigger, k.leads = g.leads, k.label = g.objectName, k.source = source ? source : "";
    k.count = g.count, k.hops = g.hops;
    k.healthGate = g.healthGate, k.gateFails = g.gateFails;
    k.scriptInv = g.invulnerable, k.scriptInvSecond = g.invSecond, k.scriptInvFloor = g.invFloor;
    k.invTrigger = g.invTrigger;
    for (const mission::ShipRef& r : g.ships)
    {
        KillShip s;
        s.nick = r.nick;
        s.spawnKnown = r.hasPos;
        memcpy(s.spawn, r.pos, sizeof(s.spawn));
        k.ships.push_back(s);
    }
    return k;
}

FlyGoal FlyGoalOf(const mission::Goal& g, const char* source)
{
    FlyGoal y;
    y.trigger = g.trigger, y.leads = g.leads, y.source = source ? source : "";
    y.kind = g.kind, y.hops = g.hops, y.leadsOrder = g.leadsOrder, y.byEntry = g.byEntry;
    y.what = g.kind == 1 ? g.objectName : "point";
    y.radius = g.dist;
    y.laneFrom = g.laneFrom;
    if (g.kind == 2)
        y.placed = true, memcpy(y.pos, g.pos, sizeof(y.pos));
    return y;
}
} // namespace lookahead
