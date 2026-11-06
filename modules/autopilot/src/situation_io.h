// The snapshot as text (`snap v5`, A37_PLAN S1-S5; v1-v4 texts still parse, with their fields, and write back byte for
// byte; v2 added `script ... source_live=`, the `inigoal` / `inileash` records and `livetrig ... leads= hops=`; v3 the
// `armed`, `queue` and `decided` records, `obj ... refs_obj=`, `place ... entry= entry_pos=`, `pilot ... tractor=`,
// `settings ... tractor_exit_*`, `leash ... off= ship= placed= opos=` and goal objects as "id:placed:x:y:z"; v4 the
// `after`, `prestage` and `realnext` records, `obj ... title_escape=`, `pilot ... system=`, `settings ... big_coast=
// survival_*=`, `queue ... last_real= arrived= step_age= escorted= surv_*=`, `contact ... radius= named=`; v5 `step` /
// `entry ... ref_placed= ref_pos=`, `contact ... dock_ring=`, `lane ... next_placed= next_pos= prev_placed= prev_pos=`,
// `settings ... follow_distance=`, `queue ... mission_leader= via_next= arrive_retries= tr_pass=`): one record per
// line, "<record> <field>=<value> ...", the fields of
// a record in a fixed order (one visitor writes and reads them: they cannot drift apart), "end" last (a cut reply
// fails). Values: integers decimal, floats "%.9g" (exact for float: parse(serialize(s)) == s bit for bit, so the
// game's hash and the host-free hash of a recorded snapshot agree), doubles "%.17g", booleans 0 / 1, lists "a,b,c",
// vectors "x,y,z", strings percent-encoded (space, '%', '=', ',' and bytes outside 0x21..0x7E). Lines starting with
// '#' are comments (Save-FLSituation keeps the in-game hash as "#game-hash 0x........"). Parsing fails closed: an
// unknown record, a field out of order, a missing or extra field, a bad value, a single record missing or repeated,
// no "end" -> false with the reason. Header-only; tests\situation_io_test.cpp round-trips it.
#pragma once
#include "situation.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace situation
{
namespace io
{
inline std::string EncStr(const std::string& s)
{
    static const char* hex = "0123456789ABCDEF";
    std::string o;
    for (unsigned char c : s)
        if (c <= 0x20 || c >= 0x7F || c == '%' || c == '=' || c == ',')
            o += '%', o += hex[c >> 4], o += hex[c & 15];
        else
            o += static_cast<char>(c);
    return o;
}
inline bool DecStr(const std::string& s, std::string& out)
{
    out.clear();
    for (size_t i = 0; i < s.size(); ++i)
    {
        if (s[i] != '%')
        {
            out += s[i];
            continue;
        }
        if (i + 2 >= s.size())
            return false;
        const auto nib = [](char c) -> int
        { return c >= '0' && c <= '9' ? c - '0' : c >= 'A' && c <= 'F' ? c - 'A' + 10 : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1; };
        const int a = nib(s[i + 1]), b = nib(s[i + 2]);
        if (a < 0 || b < 0)
            return false;
        out += static_cast<char>(a * 16 + b);
        i += 2;
    }
    return true;
}
inline std::string EncF(float v)
{
    if (std::isnan(v))
        return "nan";
    char b[32];
    snprintf(b, sizeof(b), "%.9g", static_cast<double>(v));
    return b;
}
inline std::string EncD(double v)
{
    if (std::isnan(v))
        return "nan";
    char b[40];
    snprintf(b, sizeof(b), "%.17g", v);
    return b;
}
inline bool DecF(const std::string& s, float& v)
{
    if (s == "nan")
        return v = NAN, true;
    if (s.empty())
        return false;
    char* e = nullptr;
    v = strtof(s.c_str(), &e);
    return e && *e == 0;
}
inline bool DecD(const std::string& s, double& v)
{
    if (s == "nan")
        return v = NAN, true;
    if (s.empty())
        return false;
    char* e = nullptr;
    v = strtod(s.c_str(), &e);
    return e && *e == 0;
}
inline bool DecU(const std::string& s, u32& v)
{
    if (s.empty() || s[0] == '-')
        return false;
    char* e = nullptr;
    const unsigned long long x = strtoull(s.c_str(), &e, 10);
    if (!e || *e || x > 0xFFFFFFFFull)
        return false;
    v = static_cast<u32>(x);
    return true;
}
inline bool DecI(const std::string& s, int& v)
{
    if (s.empty())
        return false;
    char* e = nullptr;
    const long x = strtol(s.c_str(), &e, 10);
    if (!e || *e)
        return false;
    v = static_cast<int>(x);
    return true;
}
inline std::vector<std::string> Split(const std::string& s, char sep)
{
    std::vector<std::string> o;
    if (s.empty())
        return o;
    size_t a = 0;
    for (;;)
    {
        const size_t c = s.find(sep, a);
        o.push_back(s.substr(a, c == std::string::npos ? std::string::npos : c - a));
        if (c == std::string::npos)
            break;
        a = c + 1;
    }
    return o;
}

// The writer: appends " name=value" per field. `version`: the snapshot's (fields added in a later version are written
// only from it on; the reader likewise - a v1 text parses and writes back byte for byte).
struct Writer
{
    int version = kSnapVersion;
    std::string line;
    void put(const char* k, const std::string& v) { line += ' ', line += k, line += '=', line += v; }
    void operator()(const char* k, u32& v) { put(k, std::to_string(v)); }
    void operator()(const char* k, int& v) { put(k, std::to_string(v)); }
    void operator()(const char* k, bool& v) { put(k, v ? "1" : "0"); }
    void operator()(const char* k, float& v) { put(k, EncF(v)); }
    void operator()(const char* k, double& v) { put(k, EncD(v)); }
    void operator()(const char* k, std::string& v) { put(k, EncStr(v)); }
    void operator()(const char* k, Verb& v) { put(k, VerbName(v)); }
    void vec3(const char* k, float* p) { put(k, EncF(p[0]) + "," + EncF(p[1]) + "," + EncF(p[2])); }
    void list(const char* k, std::vector<u32>& v)
    {
        std::string s;
        for (size_t i = 0; i < v.size(); ++i)
            s += (i ? "," : "") + std::to_string(v[i]);
        put(k, s);
    }
    // the first n of a fixed array (n = the count field read just before)
    void arr(const char* k, u32* a, int& n, int max, bool setsCount)
    {
        (void)setsCount;
        std::string s;
        for (int i = 0; i < n && i < max; ++i)
            s += (i ? "," : "") + std::to_string(a[i]);
        put(k, s);
    }
    // v1 / v2 "id:placed"; v3 "id:placed:x:y:z" (the position)
    void objs(const char* k, std::vector<SnapGoalObj>& v)
    {
        std::string s;
        for (size_t i = 0; i < v.size(); ++i)
        {
            s += (i ? "," : "") + std::to_string(v[i].id) + ":" + (v[i].placed ? "1" : "0");
            if (version >= 3)
                s += ":" + EncF(v[i].pos[0]) + ":" + EncF(v[i].pos[1]) + ":" + EncF(v[i].pos[2]);
        }
        put(k, s);
    }
    void strs(const char* k, std::vector<std::string>& v) // each encoded (',' is escaped), "a,b,c"
    {
        std::string s;
        for (size_t i = 0; i < v.size(); ++i)
            s += (i ? "," : "") + EncStr(v[i]);
        put(k, s);
    }
};

// The reader: the fields of a line in the visitor's order, each name checked.
struct Reader
{
    int version = kSnapVersion;
    std::vector<std::pair<std::string, std::string>> kv;
    size_t at = 0;
    bool ok = true;
    std::string err;
    bool take(const char* k, std::string& v)
    {
        if (!ok)
            return false;
        if (at >= kv.size() || kv[at].first != k)
        {
            ok = false;
            err = std::string("field ") + k + (at < kv.size() ? " expected, found " + kv[at].first : " missing");
            return false;
        }
        v = kv[at++].second;
        return true;
    }
    void bad(const char* k)
    {
        if (ok)
            ok = false, err = std::string("bad value for ") + k;
    }
    void operator()(const char* k, u32& v)
    {
        std::string s;
        if (take(k, s) && !DecU(s, v))
            bad(k);
    }
    void operator()(const char* k, int& v)
    {
        std::string s;
        if (take(k, s) && !DecI(s, v))
            bad(k);
    }
    void operator()(const char* k, bool& v)
    {
        std::string s;
        if (take(k, s))
        {
            if (s == "0" || s == "1")
                v = s == "1";
            else
                bad(k);
        }
    }
    void operator()(const char* k, float& v)
    {
        std::string s;
        if (take(k, s) && !DecF(s, v))
            bad(k);
    }
    void operator()(const char* k, double& v)
    {
        std::string s;
        if (take(k, s) && !DecD(s, v))
            bad(k);
    }
    void operator()(const char* k, std::string& v)
    {
        std::string s;
        if (take(k, s) && !DecStr(s, v))
            bad(k);
    }
    void operator()(const char* k, Verb& v)
    {
        std::string s;
        if (!take(k, s))
            return;
        v = VerbFromName(s);
        if (std::string(VerbName(v)) != s)
            bad(k);
    }
    void vec3(const char* k, float* p)
    {
        std::string s;
        if (!take(k, s))
            return;
        const std::vector<std::string> f = Split(s, ',');
        if (f.size() != 3 || !DecF(f[0], p[0]) || !DecF(f[1], p[1]) || !DecF(f[2], p[2]))
            bad(k);
    }
    void list(const char* k, std::vector<u32>& v)
    {
        std::string s;
        if (!take(k, s))
            return;
        v.clear();
        for (const std::string& f : Split(s, ','))
        {
            u32 x = 0;
            if (!DecU(f, x))
                return bad(k);
            v.push_back(x);
        }
    }
    void arr(const char* k, u32* a, int& n, int max, bool setsCount)
    {
        std::vector<u32> v;
        list(k, v);
        if (!ok)
            return;
        if (static_cast<int>(v.size()) > max || (!setsCount && static_cast<int>(v.size()) != n))
            return bad(k);
        for (int i = 0; i < max; ++i)
            a[i] = i < static_cast<int>(v.size()) ? v[i] : 0;
        if (setsCount)
            n = static_cast<int>(v.size());
    }
    void objs(const char* k, std::vector<SnapGoalObj>& v)
    {
        std::string s;
        if (!take(k, s))
            return;
        v.clear();
        for (const std::string& f : Split(s, ','))
        {
            const std::vector<std::string> p = Split(f, ':');
            SnapGoalObj o;
            if (p.size() != (version >= 3 ? 5u : 2u) || !DecU(p[0], o.id) || (p[1] != "0" && p[1] != "1"))
                return bad(k);
            o.placed = p[1] == "1";
            if (version >= 3 && (!DecF(p[2], o.pos[0]) || !DecF(p[3], o.pos[1]) || !DecF(p[4], o.pos[2])))
                return bad(k);
            v.push_back(o);
        }
    }
    void strs(const char* k, std::vector<std::string>& v)
    {
        std::string s;
        if (!take(k, s))
            return;
        v.clear();
        for (const std::string& f : Split(s, ','))
        {
            std::string x;
            if (!DecStr(f, x))
                return bad(k);
            v.push_back(x);
        }
    }
};

// ---- the records' fields (one place for both directions)
template <class V> void Fields(V& v, SnapStep& x)
{
    v("text", x.textId);
    v.arr("refs", x.refs, x.refCount, 4, true);
    v.arr("ref_obj", x.refObj, x.refCount, 4, false);
    v("nav", x.hasNav);
    v.vec3("nav_pos", x.nav);
    v("nav_system", x.navSystem);
    v("verb", x.verb);
    v("job", x.job);
    v("job_rows", x.jobRows);
    if (v.version >= 5)
    {
        v("ref_placed", x.refPlaced);
        v.vec3("ref_pos", x.refPos);
    }
}
template <class V> void Fields(V& v, SnapObjDef& x)
{
    v("nick", x.nick);
    v("type", x.type);
    v("script", x.script);
}
template <class V> void Fields(V& v, SnapGoal& x)
{
    v("kind", x.kind);
    v("object", x.object);
    v("name", x.objectName);
    v.vec3("pos", x.pos);
    v("dist", x.dist);
    v("count", x.count);
    v("subject", x.subject);
    v("trigger", x.trigger);
    v("leads", x.leads);
    v("file", x.file);
    v("by_entry", x.byEntry);
    v("placed", x.placed);
    v.vec3("obj_pos", x.objPos);
    v.objs("objects", x.objects);
}
template <class V> void Fields(V& v, SnapLeash& x)
{
    v("object", x.object);
    v("name", x.name);
    v("dist", x.dist);
    v("name_ids", x.nameIds);
    v.list("off_comms", x.offComms);
    if (v.version >= 3)
    {
        v("off", x.off);
        v("ship", x.ship);
        v("placed", x.placed);
        v.vec3("opos", x.opos);
    }
}
template <class V> void Fields(V& v, SnapArmed& x)
{
    v("by", x.by);
    v("by_file", x.byFile);
    Fields(v, x.goal);
}
template <class V> void Fields(V& v, SnapAfter& x) // (v4)
{
    v("leads", x.leads);
    v("file", x.file);
    Fields(v, x.goal);
}
template <class V> void Fields(V& v, SnapPrestage& x) // (v4)
{
    v("last_real", x.lastReal);
    v("next", x.next);
    v("next_ok", x.nextOk);
    v.vec3("next_pos", x.nextPos);
    v("next_text", x.nextText);
    v("stage_from", x.stageFrom);
    v("stage_ok", x.stageOk);
    v.vec3("stage_pos", x.stagePos);
    v("stage_text", x.stageText);
}
template <class V> void Fields(V& v, SnapNag& x)
{
    v("has", x.has);
    v("pos", x.pos);
    v("object", x.object);
    v("name", x.name);
    v.vec3("p", x.p);
    v("dist", x.dist);
}
template <class V> void Fields(V& v, SnapLive& x)
{
    v("id", x.id);
    v("state", x.state);
    v("cond", x.cond);
    v("nick", x.nick);
    v("ini", x.iniCond);
    if (v.version >= 2)
    {
        v("leads", x.leads);
        v("hops", x.hops);
    }
}
template <class V> void Fields(V& v, SnapContact& x)
{
    v("id", x.id);
    v("type", x.type);
    v("name_ids", x.nameIds);
    v("base", x.base);
    v("ship", x.ship);
    v("dock", x.dock);
    v("targetable", x.targetable);
    v.vec3("pos", x.pos);
    v("dist", x.distance);
    v("health", x.health);
    v("attitude", x.attitude);
    v("asked", x.attitudeAsked);
    v("att_known", x.attKnown);
    v("att", x.att);
    v("target", x.targetId);
    v("targets_player", x.targetsPlayer);
    v("max_hit", x.maxHit);
    v("rep", x.rep);
    v("dock_port", x.dockPort);
    v("jumping", x.jumping);
    v("lane", x.lane);
    v("pre_hostile", x.preHostile);
    v("player", x.player);
    if (v.version >= 4)
    {
        v("radius", x.radius);
        v("named", x.named);
    }
    if (v.version >= 5)
        v("dock_ring", x.dockRing);
}
template <class V> void Fields(V& v, SnapLane& x)
{
    v("ring", x.ring);
    v("next", x.next);
    v("prev", x.prev);
    v("open_next", x.openNext);
    v("open_prev", x.openPrev);
    if (v.version >= 5)
    {
        v("next_placed", x.nextPlaced);
        v.vec3("next_pos", x.nextPos);
        v("prev_placed", x.prevPlaced);
        v.vec3("prev_pos", x.prevPos);
    }
}
template <class V> void Fields(V& v, SnapWaypoint& x)
{
    v.vec3("pos", x.pos);
    v("system", x.system);
    v("object", x.object);
}
template <class V> void Fields(V& v, SnapSpawn& x)
{
    v("id", x.id);
    v("arch", x.arch);
    v.vec3("pos", x.pos);
}
// the single records
template <class V> void ObjFields(V& v, Snapshot& s)
{
    v("seq", s.objSeq);
    v("title", s.title);
    v.list("marks", s.marks);
    if (v.version >= 3)
        v.list("refs_obj", s.refsObj);
    if (v.version >= 4)
        v("title_escape", s.titleEscape);
}
template <class V> void ScriptFields(V& v, Snapshot& s)
{
    v("from_last", s.goalsFromLast);
    v("by_title", s.goalsByTitle);
    v("standin", s.standInText);
    v("file", s.goalsScript);
    v("next", s.nextObjective);
    v.list("pending", s.pendingHostile);
    if (v.version >= 2)
        v("source_live", s.goalsLive);
}
template <class V> void LiveFields(V& v, Snapshot& s)
{
    v("ok", s.liveOk);
    v("reason", s.liveReason);
    v("script", s.liveScript);
    v("controller", s.liveController);
    v("ms", s.liveMs);
}
template <class V> void PlaceFields(V& v, Snapshot& s)
{
    v("system", s.system);
    v("base", s.base);
    v("space", s.inSpace);
    v("clock", s.clock);
    if (v.version >= 3)
    {
        v("entry", s.hasEntry);
        v.vec3("entry_pos", s.entry);
    }
}
template <class V> void PilotFields(V& v, Snapshot& s)
{
    v("ok", s.pilotOk);
    v("op", s.op);
    v("locked", s.locked);
    v("lane", s.laneRide);
    v("jumping", s.jumping);
    v("cruise", s.cruise);
    v("entry", s.laneEntry);
    v("exit", s.laneExit);
    v("leader", s.leader);
    v("target", s.target);
    v.vec3("pos", s.pos);
    v.vec3("vel", s.vel);
    v("hull", s.hull);
    v("shield", s.shield);
    if (v.version >= 3)
        v("tractor", s.tractorRange);
    if (v.version >= 4)
        v("system", s.pilotSystem);
}
template <class V> void HoldsFields(V& v, Snapshot& s)
{
    v("mission", s.missionHolds);
    v("dock", s.dockInProgress);
    v("grant_ms", s.sinceGrantMs);
    v("reply_seq", s.dockReplySeq);
    v("reply_ship", s.dockReplyShip);
    v("reply_kind", s.dockReplyKind);
    v("reply_resp", s.dockReplyResponse);
    v("result_seq", s.resultSeq);
    v("result_type", s.resultType);
}
template <class V> void SettingsFields(V& v, Snapshot& s)
{
    v("knowledge", s.knowledge);
    v("script_goals", s.scriptGoals);
    v("stale_destroy_wait", s.staleDestroyWait);
    v("prestage_next", s.prestageNext);
    v("airlock_fly_through", s.airlockFlyThrough);
    v("pre_aggro", s.preAggro);
    if (v.version >= 3)
    {
        v("tractor_exit_run", s.tractorExitRun);
        v("tractor_exit_range", s.tractorExitRange);
    }
    if (v.version >= 4)
    {
        v("big_coast", s.bigCoast);
        v("survival_calm", s.survivalCalm);
        v("survival_stage", s.survivalStage);
        v("survival_until_jump", s.survivalUntilJump);
    }
    if (v.version >= 5)
        v("follow_distance", s.followDistance);
}
template <class V> void ExecFields(V& v, Snapshot& s)
{
    v("mode", s.apMode);
    v("target", s.apTarget);
    v("obj", s.objOn);
    v("survival", s.survival);
    v("our_dock", s.ourDock);
    v("spawns", s.spawnCount);
}
template <class V> void QueueFields(V& v, Snapshot& s) // (v3)
{
    Queue& q = s.queue;
    v("seq", q.seq);
    v("mode", q.mode);
    v("target", q.target);
    v("sub", q.sub);
    v("sub_target", q.subTarget);
    v("action", q.action);
    v("tr_dest", q.trHasDest);
    v("tr_object", q.trDestObject);
    v.vec3("tr_pos", q.trDest);
    v("lock", q.lockId);
    v("lock_have", q.lockHave);
    v.vec3("lock_p2", q.lockP2);
    v("cruise_lock", q.cruiseLock);
    v("leash_ship", q.leashShip);
    v("leash_dist", q.leashDist);
    v("last_dock", q.lastDock);
    v("escort", q.escortShip);
    v("bad_goal", q.badGoal);
    v.strs("dead_goals", q.deadGoals);
    v.list("script_marks", q.scriptMarks);
    v.list("seen_alive", q.seenAlive);
    v.list("seen_listed", q.seenListed);
    if (v.version >= 4)
    {
        v("last_real", q.lastReal);
        v("arrived", q.arrived);
        v("step_age", q.stepAgeMs);
        v("escorted", q.escorted);
        v("surv_title", q.survTitle);
        v("surv_mission", q.survMission);
        v("surv_ended", q.survEnded);
        v("surv_system", q.survSystem);
        v("survival", q.survival);
    }
    if (v.version >= 5)
    {
        v("mission_leader", q.missionLeader);
        v("via_next", q.viaNext);
        v("arrive_retries", q.arriveRetries);
        v("tr_pass", q.trPassRing);
    }
}
template <class V> void DecidedFields(V& v, Snapshot& s) // (v3)
{
    v("rule", s.decided.rule);
    v("mode", s.decided.mode);
    v("target", s.decided.target);
    v("action", s.decided.action);
    v("keep", s.decided.keep);
}

template <class T> inline std::string Line(const char* rec, T& x, int version)
{
    Writer w;
    w.version = version;
    Fields(w, x);
    return rec + w.line + "\n";
}
template <class F> inline std::string LineOf(const char* rec, Snapshot& s, F f)
{
    Writer w;
    w.version = s.version;
    f(w, s);
    return rec + w.line + "\n";
}
} // namespace io

// The snapshot as text (records in a fixed order, "end" last).
inline std::string Serialize(const Snapshot& snap)
{
    using namespace io;
    Snapshot& s = const_cast<Snapshot&>(snap); // (the writer only reads: one visitor serves both directions)
    const int ver = s.version;
    std::string o = "snap v" + std::to_string(ver) + "\n";
    o += LineOf("obj", s, [](Writer& w, Snapshot& x) { ObjFields(w, x); });
    o += Line("step", s.step, ver);
    for (SnapStep& e : s.entries)
        o += Line("entry", e, ver);
    o += LineOf("script", s, [](Writer& w, Snapshot& x) { ScriptFields(w, x); });
    for (SnapObjDef& d : s.objDefs)
        o += Line("objdef", d, ver);
    for (SnapGoal& g : s.goals)
        o += Line("goal", g, ver);
    for (SnapGoal& g : s.iniGoals) // (v2)
        o += Line("inigoal", g, ver);
    for (SnapGoal& g : s.nextGoals)
        o += Line("nextgoal", g, ver);
    for (SnapArmed& g : s.armed) // (v3; an older text has none)
        if (ver >= 3)
            o += Line("armed", g, ver);
    if (ver >= 4) // (v4; an older text has none)
    {
        for (SnapAfter& g : s.after)
            o += Line("after", g, ver);
        o += Line("prestage", s.prestage, ver);
        for (SnapGoal& g : s.realNext)
            o += Line("realnext", g, ver);
    }
    for (SnapLeash& l : s.leashes)
        o += Line("leash", l, ver);
    for (SnapLeash& l : s.iniLeashes) // (v2)
        o += Line("inileash", l, ver);
    o += Line("nag", s.nag, ver);
    o += Line("nextnag", s.nextNag, ver);
    o += LineOf("live", s, [](Writer& w, Snapshot& x) { LiveFields(w, x); });
    for (SnapLive& l : s.live)
        o += Line("livetrig", l, ver);
    o += LineOf("place", s, [](Writer& w, Snapshot& x) { PlaceFields(w, x); });
    o += LineOf("pilot", s, [](Writer& w, Snapshot& x) { PilotFields(w, x); });
    o += LineOf("holds", s, [](Writer& w, Snapshot& x) { HoldsFields(w, x); });
    o += LineOf("settings", s, [](Writer& w, Snapshot& x) { SettingsFields(w, x); });
    o += LineOf("exec", s, [](Writer& w, Snapshot& x) { ExecFields(w, x); });
    if (ver >= 3)
    {
        o += LineOf("queue", s, [](Writer& w, Snapshot& x) { QueueFields(w, x); });
        o += LineOf("decided", s, [](Writer& w, Snapshot& x) { DecidedFields(w, x); });
    }
    for (SnapContact& c : s.contacts)
        o += Line("contact", c, ver);
    for (SnapLane& l : s.lanes)
        o += Line("lane", l, ver);
    for (SnapWaypoint& p : s.waypoints)
        o += Line("waypoint", p, ver);
    for (SnapSpawn& p : s.spawns)
        o += Line("spawn", p, ver);
    return o + "end\n";
}

// Parses Serialize's text. False (and `err`) when anything is off - never a partial snapshot taken for whole.
inline bool Parse(const std::string& text, Snapshot& out, std::string* err = nullptr)
{
    using namespace io;
    Snapshot s;
    s.version = 0;
    const auto fail = [&](const std::string& why, int line)
    {
        if (err)
            *err = "line " + std::to_string(line) + ": " + why;
        return false;
    };
    // (each with the first version that has it)
    static const char* const kSingles[] = {"obj",   "step",  "script",   "nag",  "nextnag", "live",    "place",
                                           "pilot", "holds", "settings", "exec", "queue",   "decided", "prestage"};
    static const int kSinceVersion[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 4};
    constexpr int kSingleCount = static_cast<int>(sizeof(kSingles) / sizeof(kSingles[0]));
    static_assert(sizeof(kSinceVersion) / sizeof(kSinceVersion[0]) == sizeof(kSingles) / sizeof(kSingles[0]), "one version each");
    int lineNo = 0, singles[kSingleCount] = {};
    bool ended = false, header = false;
    size_t a = 0;
    while (a < text.size())
    {
        size_t b = text.find('\n', a);
        if (b == std::string::npos)
            b = text.size();
        std::string line = text.substr(a, b - a);
        a = b + 1;
        ++lineNo;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
            line.pop_back();
        if (line.empty() || line[0] == '#')
            continue;
        if (ended)
            return fail("text after end", lineNo);
        if (!header)
        {
            // (every version up to this build's: an older text reads with its own fields)
            for (int ver = 1; ver <= kSnapVersion && !header; ++ver)
                if (line == "snap v" + std::to_string(ver))
                    header = true, s.version = ver;
            if (!header)
                return fail("not a snap v1..v" + std::to_string(kSnapVersion) + " header: " + line.substr(0, 40), lineNo);
            continue;
        }
        if (line == "end")
        {
            ended = true;
            continue;
        }
        Reader r;
        r.version = s.version;
        const std::vector<std::string> tok = Split(line, ' ');
        const std::string rec = tok.empty() ? std::string() : tok[0];
        for (size_t i = 1; i < tok.size(); ++i)
        {
            const size_t eq = tok[i].find('=');
            if (eq == std::string::npos)
                return fail("token without '=': " + tok[i].substr(0, 40), lineNo);
            r.kv.push_back({tok[i].substr(0, eq), tok[i].substr(eq + 1)});
        }
        int single = -1;
        for (int i = 0; i < kSingleCount; ++i)
            if (rec == kSingles[i])
                single = i;
        if (single >= 0 && singles[single]++)
            return fail("record " + rec + " repeated", lineNo);
        if (rec == "obj")
            ObjFields(r, s);
        else if (rec == "step")
            Fields(r, s.step);
        else if (rec == "entry")
            s.entries.emplace_back(), Fields(r, s.entries.back());
        else if (rec == "script")
            ScriptFields(r, s);
        else if (rec == "objdef")
            s.objDefs.emplace_back(), Fields(r, s.objDefs.back());
        else if (rec == "goal")
            s.goals.emplace_back(), Fields(r, s.goals.back());
        else if (rec == "inigoal" && s.version >= 2)
            s.iniGoals.emplace_back(), Fields(r, s.iniGoals.back());
        else if (rec == "nextgoal")
            s.nextGoals.emplace_back(), Fields(r, s.nextGoals.back());
        else if (rec == "armed" && s.version >= 3)
            s.armed.emplace_back(), Fields(r, s.armed.back());
        else if (rec == "after" && s.version >= 4)
            s.after.emplace_back(), Fields(r, s.after.back());
        else if (rec == "prestage" && s.version >= 4)
            Fields(r, s.prestage);
        else if (rec == "realnext" && s.version >= 4)
            s.realNext.emplace_back(), Fields(r, s.realNext.back());
        else if (rec == "leash")
            s.leashes.emplace_back(), Fields(r, s.leashes.back());
        else if (rec == "inileash" && s.version >= 2)
            s.iniLeashes.emplace_back(), Fields(r, s.iniLeashes.back());
        else if (rec == "nag")
            Fields(r, s.nag);
        else if (rec == "nextnag")
            Fields(r, s.nextNag);
        else if (rec == "live")
            LiveFields(r, s);
        else if (rec == "livetrig")
            s.live.emplace_back(), Fields(r, s.live.back());
        else if (rec == "place")
            PlaceFields(r, s);
        else if (rec == "pilot")
            PilotFields(r, s);
        else if (rec == "holds")
            HoldsFields(r, s);
        else if (rec == "settings")
            SettingsFields(r, s);
        else if (rec == "exec")
            ExecFields(r, s);
        else if (rec == "queue" && s.version >= 3)
            QueueFields(r, s);
        else if (rec == "decided" && s.version >= 3)
            DecidedFields(r, s);
        else if (rec == "contact")
            s.contacts.emplace_back(), Fields(r, s.contacts.back());
        else if (rec == "lane")
            s.lanes.emplace_back(), Fields(r, s.lanes.back());
        else if (rec == "waypoint")
            s.waypoints.emplace_back(), Fields(r, s.waypoints.back());
        else if (rec == "spawn")
            s.spawns.emplace_back(), Fields(r, s.spawns.back());
        else
            return fail("unknown record " + rec.substr(0, 40), lineNo);
        if (!r.ok)
            return fail(rec + ": " + r.err, lineNo);
        if (r.at != r.kv.size())
            return fail(rec + ": unknown field " + r.kv[r.at].first, lineNo);
    }
    if (!header)
        return fail("empty", lineNo);
    if (!ended)
        return fail("no end (cut?)", lineNo);
    for (int i = 0; i < kSingleCount; ++i)
        if (s.version >= kSinceVersion[i] && !singles[i])
            return fail(std::string("record missing: ") + kSingles[i], lineNo);
    out = s;
    return true;
}
} // namespace situation
