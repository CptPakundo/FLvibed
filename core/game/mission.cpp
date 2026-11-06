// Story mission scripts: the conditions that advance the current step (mission.h).
#include "mission.h"
#include "mission_live.h"
#include "flhash.h"
#include "ini.h"
#include "str.h"
#include "../log.h"

#include <algorithm>
#include <map>
#include <set>

namespace mission
{
using str::Lower;

// (in-tree, bit-identical to Common.dll's export: the scripts resolve without the game - build.cmd test)
static u32 CreateId(const std::string& nickname) { return flhash::CreateId(nickname); }

struct Trig
{
    std::string name;
    u32 system = 0;         // 0: any
    std::string systemNick; // the system as written, lower case ("" for any): the decision sweep's text
    std::vector<std::pair<std::string, std::string>> cnd, act; // keys lower case ("cnd_distship"), values as written
};

struct Script
{
    std::string path;
    std::vector<Trig> trig;
    std::map<std::string, size_t> byName;           // trigger nickname (lower) -> index
    std::map<u32, size_t> byId;                     // CreateID of the nickname -> index (the live table's ids)
    std::map<std::string, std::vector<u32>> objIds; // [NNObjective] nickname (lower) -> its text ids
    std::map<std::string, std::string> objType;     // [NNObjective] nickname (lower) -> its type ("navmarker", lower)
    // the [NNObjective]s in the file's order (the mission's own flow): text id, system, the point (navmarker / rep_inst)
    struct NNObj
    {
        u32 text = 0, system = 0;
        float pos[3] = {};
        bool hasPos = false;
        std::string nick;   // (lower; LOOK's Goal::leadsOrder)
        std::string object; // a rep_inst's object (its 8th field, lower; M13 go_home: st01_dock_ring_1), "" none
    };
    std::vector<NNObj> nnOrder;
    std::set<std::string> msnShips; // every [MsnShip] nickname (lower): a kind 4 goal by nickname (LOOK)
    std::set<std::string> followLists;              // [ObjList]s that follow / stay with the player
    std::set<std::string> followShips;              // ships given such a list (Act_GiveObjList / Act_SpawnShip)
    std::vector<std::pair<std::string, std::string>> solars;    // [MsnSolar] nickname, label (lower)
    std::set<std::string> baseSolars;                           // [MsnSolar]s with a `base =` (lower): dockable bases
    std::set<u32> titles;                                       // Act_SetTitle ids (the mission's title: "MISSION 13")
    std::map<std::string, std::string> shipNpc;                 // [MsnShip] nickname (lower) -> its NPC (lower)
    std::map<std::string, u32> npcName;                         // [NPC] nickname (lower) -> individual_name (string id)
    std::map<std::string, std::vector<std::string>> labelShips; // [MsnShip] label (lower) -> its ships (lower)
    // where the script's ships spawn (MapSpawns): [MsnShip] / [MsnFormation] position (nickname lower), the formation's
    // ships in order, and the spawn actions that place one elsewhere (Act_SpawnShip / Act_SpawnFormation with a position)
    struct Place
    {
        std::string nick;
        float pos[3];
    };
    std::vector<Place> shipPos, formPos;
    std::map<std::string, std::vector<std::string>> formShips;
    // [ObjList] nickname (lower) -> its commands in order, keys lower (SpawnAfterKill's first orders)
    std::map<std::string, std::vector<std::pair<std::string, std::string>>> objLists;
};

static Script Load(const std::string& path)
{
    Script s;
    s.path = path;
    for (const ini::Section& sec : ini::Read(path))
    {
        const std::string* nick = sec.Get("nickname");
        if (sec.type == "trigger" && nick)
        {
            Trig t;
            t.name = Lower(*nick);
            if (const std::string* sys = sec.Get("system"))
            {
                t.system = Lower(*sys) == "all" ? 0 : CreateId(*sys);
                t.systemNick = t.system ? Lower(*sys) : std::string();
            }
            for (const auto& kv : sec.kv)
                if (kv.first.compare(0, 4, "cnd_") == 0)
                    t.cnd.push_back(kv);
                else if (kv.first.compare(0, 4, "act_") == 0)
                    t.act.push_back(kv);
            s.byName[t.name] = s.trig.size();
            s.byId[CreateId(t.name)] = s.trig.size();
            s.trig.push_back(std::move(t));
        }
        else if (sec.type == "nnobjective" && nick)
        {
            // type = navmarker, St01, 24260, 27260, x, y, z | rep_inst, St01, 24270, 27270, x, y, z, obj | ids, 24xxx
            std::vector<u32>& ids = s.objIds[Lower(*nick)];
            if (const std::string* type = sec.Get("type"))
            {
                const std::vector<std::string> f = ini::Fields(*type);
                for (size_t i = 1; i < f.size() && i < 4; ++i)
                {
                    const u32 v = static_cast<u32>(strtoul(f[i].c_str(), nullptr, 10));
                    if (v > 0)
                        ids.push_back(v);
                }
                Script::NNObj o;
                o.nick = Lower(*nick);
                const std::string kind = f.empty() ? std::string() : Lower(f[0]);
                s.objType[Lower(*nick)] = kind;
                if ((kind == "navmarker" || kind == "rep_inst") && f.size() >= 7)
                {
                    o.system = CreateId(f[1]);
                    o.text = static_cast<u32>(strtoul(f[2].c_str(), nullptr, 10));
                    for (int i = 0; i < 3; ++i)
                        o.pos[i] = static_cast<float>(atof(f[4 + i].c_str()));
                    o.hasPos = o.pos[0] != 0 || o.pos[1] != 0 || o.pos[2] != 0; // (0, 0, 0: a "defend" marker)
                    if (kind == "rep_inst" && f.size() >= 8)
                        o.object = Lower(f[7]);
                }
                else if (kind == "ids" && f.size() >= 2)
                    o.text = static_cast<u32>(strtoul(f[1].c_str(), nullptr, 10));
                s.nnOrder.push_back(o);
            }
        }
        else if (sec.type == "msnship" && nick)
        {
            s.msnShips.insert(Lower(*nick));
            if (const std::string* npc = sec.Get("npc"))
                s.shipNpc[Lower(*nick)] = Lower(*npc);
            for (const auto& kv : sec.kv)
                if (kv.first == "label")
                    s.labelShips[Lower(kv.second)].push_back(Lower(*nick));
            if (const std::string* p = sec.Get("position"))
            {
                const std::vector<std::string> f = ini::Fields(*p);
                if (f.size() >= 3)
                    s.shipPos.push_back(
                        {Lower(*nick),
                         {static_cast<float>(atof(f[0].c_str())), static_cast<float>(atof(f[1].c_str())),
                          static_cast<float>(atof(f[2].c_str()))}});
            }
        }
        else if (sec.type == "msnformation" && nick)
        {
            std::vector<std::string>& ships = s.formShips[Lower(*nick)];
            for (const auto& kv : sec.kv)
                if (kv.first == "ship")
                    ships.push_back(Lower(kv.second));
            if (const std::string* p = sec.Get("position"))
            {
                const std::vector<std::string> f = ini::Fields(*p);
                if (f.size() >= 3)
                    s.formPos.push_back(
                        {Lower(*nick),
                         {static_cast<float>(atof(f[0].c_str())), static_cast<float>(atof(f[1].c_str())),
                          static_cast<float>(atof(f[2].c_str()))}});
            }
        }
        else if (sec.type == "npc" && nick)
        {
            if (const std::string* n = sec.Get("individual_name"))
                s.npcName[Lower(*nick)] = static_cast<u32>(strtoul(n->c_str(), nullptr, 10));
        }
        else if (sec.type == "msnsolar" && nick)
        {
            const std::string* label = sec.Get("label");
            s.solars.push_back({Lower(*nick), label ? Lower(*label) : std::string()});
            if (const std::string* b = sec.Get("base"); b && !b->empty())
                s.baseSolars.insert(Lower(*nick)); // (M13's Battleship Osiris: base = st01_02_base)
        }
        else if (sec.type == "objlist" && nick)
        {
            std::vector<std::pair<std::string, std::string>>& cmds = s.objLists[Lower(*nick)];
            for (const auto& kv : sec.kv)
            {
                if ((kv.first == "follow" || kv.first == "followplayer" || kv.first == "stayinrange") &&
                    Lower(kv.second).compare(0, 6, "player") == 0)
                    s.followLists.insert(Lower(*nick));
                if (kv.first != "nickname" && kv.first != "system")
                    cmds.push_back(kv);
            }
        }
    }
    // ships given a list that follows the player (the second pass: lists may come after the triggers that give them)
    for (const Trig& t : s.trig)
        for (const auto& kv : t.act)
            if (kv.first == "act_settitle")
            {
                if (const u32 v = static_cast<u32>(strtoul(kv.second.c_str(), nullptr, 10)))
                    s.titles.insert(v);
            }
            else if (kv.first == "act_giveobjlist" || kv.first == "act_spawnship")
            {
                const std::vector<std::string> f = ini::Fields(kv.second);
                if (f.size() >= 2 && s.followLists.count(Lower(f[1])))
                    s.followShips.insert(Lower(f[0]));
                // Act_SpawnShip = name, orders, x, y, z[, orientation]: spawned there instead
                if (kv.first == "act_spawnship" && f.size() >= 5)
                    s.shipPos.push_back(
                        {Lower(f[0]),
                         {static_cast<float>(atof(f[2].c_str())), static_cast<float>(atof(f[3].c_str())),
                          static_cast<float>(atof(f[4].c_str()))}});
            }
            else if (kv.first == "act_spawnformation")
            {
                // Act_SpawnFormation = name, x, y, z: spawned there instead
                const std::vector<std::string> f = ini::Fields(kv.second);
                if (f.size() >= 4)
                    s.formPos.push_back(
                        {Lower(f[0]),
                         {static_cast<float>(atof(f[1].c_str())), static_cast<float>(atof(f[2].c_str())),
                          static_cast<float>(atof(f[3].c_str()))}});
            }
    return s;
}

// Every mission script under `root` (a MISSIONS folder, trailing backslash): M*\*.ini that hold [Trigger]s.
static std::vector<Script> LoadAll(const std::string& root)
{
    std::vector<Script> all;
    WIN32_FIND_DATAA d;
    HANDLE h = FindFirstFileA((root + "M*").c_str(), &d);
    if (h == INVALID_HANDLE_VALUE)
        return all;
    std::vector<std::string> dirs;
    do
        if (d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            dirs.push_back(root + d.cFileName + "\\");
    while (FindNextFileA(h, &d));
    FindClose(h);
    for (const std::string& dir : dirs)
    {
        HANDLE f = FindFirstFileA((dir + "*.ini").c_str(), &d);
        if (f == INVALID_HANDLE_VALUE)
            continue;
        do
        {
            Script s = Load(dir + d.cFileName);
            if (!s.trig.empty())
                all.push_back(std::move(s));
        } while (FindNextFileA(f, &d));
        FindClose(f);
    }
    return all;
}

// Every mission script of the install, read once (DATA\MISSIONS\M*\*.ini that hold [Trigger]s).
static std::vector<Script> g_scripts;
static bool g_scriptsLoaded = false;
static std::vector<Script>& Scripts()
{
    if (g_scriptsLoaded)
        return g_scripts;
    g_scriptsLoaded = true;
    g_scripts = LoadAll(ini::DataPath("MISSIONS\\"));
    Log("mission: %u scripts with triggers", static_cast<unsigned>(g_scripts.size()));
    return g_scripts;
}

// What a trigger's action chain leads to within `depth` hops: a new objective (not one of `current`, not nn_blank) or
// SUCCEED; "" if nothing; "FAIL" if the chain fails the mission. `armedFail` false: a FAIL counts only through triggers
// that fire at once (Cnd_True / Cnd_Timer), not in a watcher the chain merely arms. `hops` (optional): the hop the answer
// was found at (0 the start trigger's own actions), -1 for "".
static std::string Leads(const Script& s, size_t start, const std::set<std::string>& current, int depth,
                         bool armedFail = true, int* hops = nullptr)
{
    if (hops)
        *hops = -1;
    struct Hop
    {
        size_t i;
        int d;
        bool sure; // reached through triggers that fire at once
    };
    std::set<size_t> seen;
    std::vector<Hop> todo = {{start, 0, true}};
    std::string found;
    while (!todo.empty())
    {
        const Hop h = todo.back();
        const size_t i = h.i;
        const int d = h.d;
        todo.pop_back();
        if (!seen.insert(i).second)
            continue;
        for (const auto& kv : s.trig[i].act)
        {
            const std::vector<std::string> f = ini::Fields(kv.second);
            if (f.empty())
                continue;
            if (kv.first == "act_changestate")
            {
                const std::string v = Lower(f[0]);
                if (v == "fail" && (h.sure || armedFail))
                {
                    if (hops)
                        *hops = d;
                    return "FAIL";
                }
                if (v == "succeed" && found.empty())
                {
                    found = "SUCCEED";
                    if (hops)
                        *hops = d;
                }
            }
            else if (kv.first == "act_setnnobj" && f.size() == 1)
            {
                const std::string o = Lower(f[0]);
                if (o != "nn_blank" && !current.count(o) && found.empty())
                {
                    found = o;
                    if (hops)
                        *hops = d;
                }
            }
            else if (kv.first == "act_acttrig" && d < depth)
            {
                const auto it = s.byName.find(Lower(f[0]));
                if (it != s.byName.end())
                {
                    const auto& c = s.trig[it->second].cnd;
                    const bool atOnce = c.size() == 1 && (c[0].first == "cnd_true" || c[0].first == "cnd_timer");
                    todo.push_back({it->second, d + 1, h.sure && atOnce});
                }
            }
        }
    }
    return found;
}

static std::map<const Script*, std::set<std::string>> g_shown; // per script: the objectives it has shown
static std::vector<Goal> GoalsOf(const Script& s, const std::set<std::string>& current, u32 system, bool byTitle,
                                 const std::set<std::string>& shown, const char* rootTrigger = nullptr,
                                 bool look = false);
static bool GoalOfTrigger(const Script& s, size_t i, const std::set<std::string>& shown, bool flyingOff, Goal& g,
                          bool look = false);
static u32 LookSystemOf(const Script& s, size_t i);

static const Script* lastScript = nullptr; // the script AdvanceGoals last found (Leashes)

// The leashes of script `s` in `system` (Leashes' rules); `only` (optional): just these trigger indexes (ArmedLeashes).
static std::vector<Leash> LeashesOf(const Script& s, u32 system, const std::set<size_t>* only)
{
    std::vector<Leash> out;
    for (size_t i = 0; i < s.trig.size(); ++i)
    {
        const Trig& t = s.trig[i];
        if (only && !only->count(i))
            continue;
        if ((t.system && t.system != system) || t.cnd.size() != 1 || t.cnd[0].first != "cnd_distship")
            continue;
        const std::vector<std::string> f = ini::Fields(t.cnd[0].second);
        // (a range only: "outside, Player, gmg_leader, 1, 25, TICK_AWAY" - M01b - counts ticks moving away, no range; read as
        // a 1 m leash, the ship flew "back toward" the leader every second, gate_s8v; M04's "juni, 6000, 10" is one)
        if (f.size() < 4 || Lower(f[0]) != "outside" || Lower(f[1]) != "player" ||
            (f.size() >= 6 && Lower(f[5]) == "tick_away") || atof(f[3].c_str()) < 100)
            continue;
        if (Leads(s, i, {}, 4) != "FAIL")
            continue;
        Leash l;
        l.name = f[2];
        l.object = CreateId(f[2]);
        l.dist = static_cast<float>(atof(f[3].c_str()));
        // a mission ship: its pilot's name (its runtime id is not its nickname's)
        const auto sn = s.shipNpc.find(Lower(f[2]));
        if (sn != s.shipNpc.end())
        {
            const auto nn = s.npcName.find(sn->second);
            if (nn != s.npcName.end())
                l.nameIds = nn->second;
        }
        // the comm lines whose end disarms it (M10: botzler_dies, Cnd_CommComplete = DX_M10_0460B_BOTZLER, deactivates
        // too_far_from_von_claussen_2 - kept after it, the leash held the ship 4.8 km from Von Claussen in the exit
        // tunnel while the script's nag toward the hole ran out, gate_s8w / s8x)
        for (const Trig& d : s.trig)
        {
            if (d.cnd.size() != 1 || d.cnd[0].first != "cnd_commcomplete")
                continue;
            for (const auto& a : d.act)
                if (a.first == "act_deacttrig" && !ini::Fields(a.second).empty() &&
                    Lower(ini::Fields(a.second)[0]) == Lower(t.name))
                {
                    const std::vector<std::string> cf = ini::Fields(d.cnd[0].second);
                    if (!cf.empty())
                        l.offComms.push_back(CreateId(Lower(cf[0])));
                }
        }
        if (l.object && l.dist > 0)
            out.push_back(l);
    }
    return out;
}

std::vector<Leash> Leashes(u32 system)
{
    return lastScript ? LeashesOf(*lastScript, system, nullptr) : std::vector<Leash>();
}

// The tethers of script `s` in `system` (Tethers' rules); `only` (optional): just these trigger indexes (ArmedTethers).
static std::vector<Tether> TethersOf(const Script& s, u32 system, const std::set<size_t>* only)
{
    std::vector<Tether> out;
    for (size_t i = 0; i < s.trig.size(); ++i)
    {
        const Trig& t = s.trig[i];
        if ((only && !only->count(i)) || t.cnd.size() != 1 || t.cnd[0].first != "cnd_tetherbroke")
            continue;
        const u32 tsys = LookSystemOf(s, i);
        if (tsys && tsys != system)
            continue;
        // Cnd_TetherBroke = <shipA>, <shipB>, <dist>, <secs>[, <inner>] (Content.dll 0x06ec11c0, LOOK_PLAN 4a)
        const std::vector<std::string> f = ini::Fields(t.cnd[0].second);
        if (f.size() < 4)
            continue;
        Tether r;
        r.a = f[0], r.b = f[1];
        r.aId = CreateId(f[0]), r.bId = CreateId(f[1]);
        r.dist = static_cast<float>(atof(f[2].c_str()));
        r.secs = static_cast<float>(atof(f[3].c_str()));
        r.inner = f.size() >= 5 ? static_cast<float>(atof(f[4].c_str())) : 0.0f;
        r.trigger = t.name;
        r.fails = Leads(s, i, {}, 4) == "FAIL";
        const auto sn = s.shipNpc.find(Lower(f[0]));
        if (sn != s.shipNpc.end())
        {
            const auto nn = s.npcName.find(sn->second);
            if (nn != s.npcName.end())
                r.aNameIds = nn->second;
        }
        out.push_back(r);
    }
    return out;
}

std::vector<Tether> Tethers(const std::string& file, u32 system)
{
    if (file.empty())
        return lastScript ? TethersOf(*lastScript, system, nullptr) : std::vector<Tether>();
    for (const Script& s : Scripts())
        if (_stricmp(s.path.c_str(), file.c_str()) == 0)
            return TethersOf(s, system, nullptr);
    return {};
}

static u32 lastText = 0; // the text of the step AdvanceGoals last found a script for (the stand-in of a placeholder)

// AdvanceGoals' body. `remember` false (AdvanceGoalsPeek, the situation snapshot): the same answer as AdvanceGoals now,
// but nothing of mission.cpp's memory changes (lastScript / lastText, the shown objectives) - a read leaves the
// autopilot's next call exactly as it would have been.
static std::vector<Goal> Advance(u32 textId, u32 system, bool* fromLast, u32 titleId, bool remember, AdvanceInfo* info,
                                 bool look = false)
{
    if (fromLast)
        *fromLast = false;
    std::vector<Goal> out;
    // the script and objective(s) behind the text
    const Script* sc = nullptr;
    std::set<std::string> current;
    // (a placeholder objective - "null", "nn_blank": "Awaiting Mission Objective" 21660 - is no step: many triggers set it,
    // M13's own watch_endgame_object among them, and its setters as roots hid the city's destroy condition)
    // ...nor is a text more than one mission's script uses (the same 21660 is nn_objsoon, mlog_awaiting_objective, ...
    // elsewhere): it names no step of one mission
    auto find = [&](u32 id)
    {
        std::set<const Script*> in;
        for (const Script& s : Scripts())
            for (const auto& o : s.objIds)
                if (o.first != "null" && o.first != "nn_blank" &&
                    std::find(o.second.begin(), o.second.end(), id) != o.second.end())
                    sc = &s, current.insert(o.first), in.insert(&s);
        if (in.size() > 1)
            sc = nullptr, current.clear();
    };
    if (textId)
        find(textId);
    if (sc)
    {
        if (remember)
            lastScript = sc, lastText = textId;
    }
    else if (lastScript && lastText)
    {
        find(lastText);
        if (fromLast)
            *fromLast = true;
        if (info && sc)
            info->standInText = lastText;
    }
    // nothing known yet (a save loaded mid-mission shows "Awaiting Mission Objective"): the script that sets the mission's
    // title, with no current objective - its triggers of the player's system entry are the roots (M13's city checkpoint)
    // (destroy conditions only: from the system entry alone the order of the flying ones is unknown - M13 in St01 would
    // offer "Sinclair 300 m" and "Osiris 1000 m")
    bool byTitle = false;
    if (!sc && titleId)
    {
        for (const Script& s : Scripts())
            if (s.titles.count(titleId))
            {
                sc = &s;
                byTitle = true;
                break;
            }
        if (sc && fromLast)
            *fromLast = true;
    }
    if (!sc)
        return out;
    if (info)
        info->script = sc->path, info->byTitle = byTitle;
    // objectives this mission has shown (current ones included): a goal must lead to a fresh one - an old branch that
    // fired long ago leads back to them
    if (!remember)
    {
        const auto was = g_shown.find(sc);
        std::set<std::string> shown = was == g_shown.end() ? std::set<std::string>() : was->second;
        shown.insert(current.begin(), current.end());
        return GoalsOf(*sc, current, system, byTitle, shown, nullptr, look);
    }
    for (const std::string& o : current)
        g_shown[sc].insert(o);
    return GoalsOf(*sc, current, system, byTitle, g_shown[sc]);
}

void ForgetGame()
{
    g_shown.clear();
    lastScript = nullptr;
    lastText = 0;
}

void UseScriptsFrom(const std::string& missionsDir)
{
    ForgetGame(); // (g_shown and lastScript point into the old list)
    std::string root = missionsDir;
    if (!root.empty() && root.back() != '\\' && root.back() != '/')
        root += '\\';
    g_scripts = root.empty() ? std::vector<Script>() : LoadAll(root);
    g_scriptsLoaded = true;
}

std::vector<Goal> AdvanceGoals(u32 textId, u32 system, bool* fromLast, u32 titleId)
{
    return Advance(textId, system, fromLast, titleId, true, nullptr);
}

std::vector<Goal> AdvanceGoalsPeek(u32 textId, u32 system, u32 titleId, AdvanceInfo& info, bool look)
{
    info = AdvanceInfo();
    return Advance(textId, system, &info.fromLast, titleId, false, &info, look);
}

std::vector<ObjectiveDef> ObjectiveDefs(u32 textId)
{
    std::vector<ObjectiveDef> out;
    if (!textId)
        return out;
    for (const Script& s : Scripts())
        for (const auto& o : s.objIds)
            if (std::find(o.second.begin(), o.second.end(), textId) != o.second.end())
            {
                ObjectiveDef d;
                d.nick = o.first;
                const auto t = s.objType.find(o.first);
                d.type = t == s.objType.end() ? std::string() : t->second;
                d.script = s.path;
                out.push_back(d);
            }
    return out;
}

std::vector<Goal> GoalsAfter(const std::string& objective, const std::string& file, u32 system, bool look)
{
    for (const Script& s : Scripts())
        if (s.path == file)
        {
            std::set<std::string> shownThen = g_shown[&s];
            shownThen.insert(Lower(objective));
            return GoalsOf(s, {Lower(objective)}, system, false, shownThen, nullptr, look);
        }
    return {};
}

std::vector<Goal> GoalsArmedBy(const std::string& trigger, const std::string& file, u32 system, bool look)
{
    for (const Script& s : Scripts())
        if (s.path == file)
            return GoalsOf(s, {}, system, false, g_shown[&s], trigger.c_str(), look);
    return {};
}

// The goals of one step: `current` = its objective nickname(s); `shown` = the objectives the mission has shown by then.
static std::vector<Goal> GoalsOf(const Script& s, const std::set<std::string>& current, u32 system, bool byTitle,
                                 const std::set<std::string>& shown, const char* rootTrigger, bool look)
{
    std::vector<Goal> out;
    // roots: the objective's setters and the whole chain that armed them (who armed the setter, who armed that, ...: M08's
    // "Ozu within 9 km of the hole" was armed by the step-1 trigger, three levels above the waypoint's setter), and the
    // system-entry triggers of the player's system. The chain itself has fired; what it armed is live.
    std::map<size_t, std::vector<size_t>> armedBy; // trigger -> the triggers that activate it
    for (size_t i = 0; i < s.trig.size(); ++i)
        for (const auto& kv : s.trig[i].act)
            if (kv.first == "act_acttrig")
            {
                const std::vector<std::string> f = ini::Fields(kv.second);
                const auto it = f.empty() ? s.byName.end() : s.byName.find(Lower(f[0]));
                if (it != s.byName.end())
                    armedBy[it->second].push_back(i);
            }
    std::set<size_t> roots;
    if (rootTrigger)
    {
        // (GoalsArmedBy: one fired trigger is the only root)
        const auto it = s.byName.find(Lower(rootTrigger));
        if (it == s.byName.end())
            return out;
        roots.insert(it->second);
    }
    for (size_t i = 0; i < s.trig.size() && !rootTrigger; ++i)
        for (const auto& kv : s.trig[i].act)
            if (kv.first == "act_setnnobj")
            {
                const std::vector<std::string> f = ini::Fields(kv.second);
                if (f.size() == 1 && current.count(Lower(f[0])))
                    roots.insert(i);
            }
    if (!rootTrigger)
    {
        std::vector<size_t> up(roots.begin(), roots.end());
        for (int level = 0; level < 8 && !up.empty(); ++level)
        {
            std::vector<size_t> next;
            for (size_t i : up)
                for (size_t p : armedBy[i])
                    if (roots.insert(p).second)
                        next.push_back(p);
            up.swap(next);
        }
    }
    const std::set<size_t> stepRoots = roots; // the objective's own chain (or the fired trigger)
    for (size_t i = 0; i < s.trig.size() && !rootTrigger; ++i)
        for (const auto& kv : s.trig[i].cnd)
            if ((kv.first == "cnd_systementer" || kv.first == "cnd_spaceenter") &&
                CreateId(ini::Fields(kv.second).empty() ? "" : ini::Fields(kv.second)[0]) == system)
                roots.insert(i);
    // armed: what the roots activate; through Cnd_True / Cnd_Timer triggers (they fired) one more level
    const auto expand = [&](const std::set<size_t>& from)
    {
        std::set<size_t> armed;
        std::vector<size_t> frontier(from.begin(), from.end());
        for (int level = 0; level < 3 && !frontier.empty(); ++level)
        {
            std::vector<size_t> next;
            for (size_t r : frontier)
                for (const auto& kv : s.trig[r].act)
                    if (kv.first == "act_acttrig")
                    {
                        const std::vector<std::string> f = ini::Fields(kv.second);
                        const auto it = f.empty() ? s.byName.end() : s.byName.find(Lower(f[0]));
                        if (it == s.byName.end() || !armed.insert(it->second).second)
                            continue;
                        const Trig& t = s.trig[it->second];
                        if (t.cnd.size() == 1 && (t.cnd[0].first == "cnd_true" || t.cnd[0].first == "cnd_timer"))
                            next.push_back(it->second);
                    }
            frontier.swap(next);
        }
        return armed;
    };
    const std::set<size_t> armed = expand(roots);
    const std::set<size_t> armedByStep = expand(stepRoots);
    std::set<size_t> off; // switched off by the chain (Act_DeactTrig)
    for (size_t r : roots)
        for (const auto& kv : s.trig[r].act)
            if (kv.first == "act_deacttrig")
            {
                const std::vector<std::string> f = ini::Fields(kv.second);
                const auto it = f.empty() ? s.byName.end() : s.byName.find(Lower(f[0]));
                if (it != s.byName.end())
                    off.insert(it->second);
            }
    // a trigger without its own system runs in the system of the trigger that arms it (M12's core trigger "within 700 m of
    // 0,0,-950" has none: without this it was a candidate in Omicron Minor)
    auto systemOf = [&](size_t i)
    {
        std::set<size_t> seen;
        while (!s.trig[i].system && seen.insert(i).second && !armedBy[i].empty())
            i = armedBy[i][0];
        return s.trig[i].system;
    };
    for (size_t i : armed)
    {
        const Trig& t = s.trig[i];
        const u32 tsys = systemOf(i);
        if (roots.count(i) || off.count(i) || (tsys && tsys != system) || t.cnd.size() != 1)
            continue; // (a root has fired; a switched-off one never will)
        Goal g;
        if (!GoalOfTrigger(s, i, shown, byTitle, g, look))
            continue;
        g.byEntry = !armedByStep.count(i);
        out.push_back(g);
    }
    return out;
}

// LOOK's extras of a goal (Goal: leadsOrder; kinds 1, 2 the lane ride the chain waits for; kind 4 the health gates and
// Act_Invulnerable on its ships). Reads the script only.
static void LookExtras(const Script& s, size_t i, Goal& g)
{
    g.leadsOrder = -1;
    if (g.leads == "SUCCEED")
        g.leadsOrder = static_cast<int>(s.nnOrder.size());
    for (size_t k = 0; k < s.nnOrder.size() && g.leadsOrder < 0; ++k)
        if (s.nnOrder[k].nick == g.leads)
            g.leadsOrder = static_cast<int>(k);
    if (g.kind == 1 || g.kind == 2)
    {
        // the chain (Act_ActTrig, any condition, 4 hops, breadth first): the first Cnd_TLEntered = Player, <ring>, <toward>
        std::vector<std::pair<size_t, int>> todo = {{i, 0}};
        std::set<size_t> seen;
        for (size_t head = 0; head < todo.size() && g.laneFrom.empty(); ++head)
        {
            const size_t j = todo[head].first;
            const int d = todo[head].second;
            if (!seen.insert(j).second)
                continue;
            const Trig& t = s.trig[j];
            if (j != i && t.cnd.size() == 1 && t.cnd[0].first == "cnd_tlentered")
            {
                const std::vector<std::string> f = ini::Fields(t.cnd[0].second);
                if (f.size() >= 3 && Lower(f[0]) == "player")
                {
                    g.laneFrom = f[1], g.laneToward = f[2];
                    break;
                }
            }
            if (d >= 4)
                continue;
            for (const auto& kv : t.act)
                if (kv.first == "act_acttrig")
                {
                    const std::vector<std::string> f = ini::Fields(kv.second);
                    const auto it = f.empty() ? s.byName.end() : s.byName.find(Lower(f[0]));
                    if (it != s.byName.end())
                        todo.push_back({it->second, d + 1});
                }
        }
        return;
    }
    if (g.kind != 4)
        return;
    std::set<std::string> names = {Lower(g.objectName)};
    for (const ShipRef& r : g.ships)
        names.insert(r.nick);
    for (size_t j = 0; j < s.trig.size(); ++j)
    {
        const Trig& t = s.trig[j];
        // Cnd_HealthDec = <ship | label>, <fraction>: what its immediate chain (Cnd_True hops, 4) does - Act_Destroy of one
        // of the names (M02: ashcroft_health_35 -> destroy_him) or a FAIL (the Cnd_Timer chains after it are deadlines of
        // their own, not the gate's: M02's pod timers)
        if (t.cnd.size() == 1 && t.cnd[0].first == "cnd_healthdec")
        {
            const std::vector<std::string> f = ini::Fields(t.cnd[0].second);
            if (f.size() < 2 || !names.count(Lower(f[0])))
                continue;
            const float frac = static_cast<float>(atof(f[1].c_str()));
            bool destroys = false, fails = false;
            std::vector<std::pair<size_t, int>> todo = {{j, 0}};
            std::set<size_t> seen;
            for (size_t head = 0; head < todo.size(); ++head)
            {
                const size_t k = todo[head].first;
                const int d = todo[head].second;
                if (!seen.insert(k).second)
                    continue;
                for (const auto& kv : s.trig[k].act)
                {
                    const std::vector<std::string> a = ini::Fields(kv.second);
                    if (a.empty())
                        continue;
                    if (kv.first == "act_destroy" && names.count(Lower(a[0])))
                        destroys = true;
                    else if (kv.first == "act_changestate" && Lower(a[0]) == "fail")
                        fails = true;
                    else if (kv.first == "act_acttrig" && d < 4)
                    {
                        const auto it = s.byName.find(Lower(a[0]));
                        if (it != s.byName.end() && s.trig[it->second].cnd.size() == 1 &&
                            s.trig[it->second].cnd[0].first == "cnd_true")
                            todo.push_back({it->second, d + 1});
                    }
                }
            }
            if (destroys)
                g.healthGate = std::max(g.healthGate, frac);
            g.gateFails = g.gateFails || fails;
        }
        // Act_Invulnerable = <ship | label>, <a>, [b], [f] (Content.dll -> pub::SpaceObj::SetInvincible(id, a, b, f))
        for (const auto& kv : t.act)
            if (kv.first == "act_invulnerable" && !g.invulnerable)
            {
                const std::vector<std::string> a = ini::Fields(kv.second);
                if (a.size() >= 2 && names.count(Lower(a[0])) && Lower(a[1]) == "true")
                {
                    g.invulnerable = true;
                    g.invSecond = a.size() >= 3 && Lower(a[2]) == "true";
                    g.invFloor = a.size() >= 4 ? static_cast<float>(atof(a[3].c_str())) : 0.0f;
                    g.invTrigger = t.name;
                }
            }
    }
}

// Trigger `i`'s single condition as a goal (GoalsOf's rules, shared with GoalsOfLive): Cnd_DistShip / Cnd_DistVec inside
// for the player or a ship that follows the player (not `flyingOff`: AdvanceGoals' by-title view), Cnd_Destroyed of the
// mission's solars; its chain must lead to an objective not in `shown` or SUCCEED and never FAIL. False: no goal.
// `look` (LOOK L0): Cnd_Destroyed of the mission's ships too (kind 4), and the look extras (LookExtras).
static bool ParseGoal(const Script& s, size_t i, const std::set<std::string>& shown, bool flyingOff, Goal& g,
                      bool look);
static bool GoalOfTrigger(const Script& s, size_t i, const std::set<std::string>& shown, bool flyingOff, Goal& g,
                          bool look)
{
    const bool ok = ParseGoal(s, i, shown, flyingOff, g, look);
    if (ok && look)
        LookExtras(s, i, g);
    return ok;
}
static bool ParseGoal(const Script& s, size_t i, const std::set<std::string>& shown, bool flyingOff, Goal& g,
                      bool look)
{
    const Trig& t = s.trig[i];
    if (t.cnd.size() != 1)
        return false;
    const std::vector<std::string> f = ini::Fields(t.cnd[0].second);
    // Cnd_DistShip = inside, <ship>, <object>, <dist>; Cnd_DistVec = inside, <ship>, x, y, z, <dist>
    if (t.cnd[0].first == "cnd_distship" && f.size() >= 4 && Lower(f[0]) == "inside")
    {
        g.kind = 1;
        g.objectName = f[2];
        g.object = CreateId(f[2]);
        g.dist = static_cast<float>(atof(f[3].c_str()));
    }
    else if (t.cnd[0].first == "cnd_distvec" && f.size() >= 6 && Lower(f[0]) == "inside")
    {
        g.kind = 2;
        for (int k = 0; k < 3; ++k)
            g.pos[k] = static_cast<float>(atof(f[2 + k].c_str()));
        g.dist = static_cast<float>(atof(f[5].c_str()));
    }
    else if (t.cnd[0].first == "cnd_destroyed" && f.size() >= 2)
    {
        // Cnd_Destroyed = <label | nickname>, <count>[, mode]: the mission's solars only (ships: not yet)
        const std::string what = Lower(f[0]);
        g.kind = 3;
        g.objectName = f[0];
        g.count = atoi(f[1].c_str());
        g.killer = f.size() >= 4 ? Lower(f[3]) : std::string(); // (the 4th field: the killer it counts; "" any)
        for (const auto& so : s.solars)
            if (so.first == what || so.second == what)
                g.objects.push_back(CreateId(so.first));
        if (g.objects.empty() && look)
        {
            // kind 4 (LOOK): the mission's ships by [MsnShip] label, else by nickname (M02 ashcroft_dead = Ashcroft, 1,
            // ALL; M06 d_holers = holeboys, -1 - count -1 = all of them, Content.dll: LOOK_PLAN 4a)
            std::vector<std::string> nicks;
            if (const auto ls = s.labelShips.find(what); ls != s.labelShips.end())
                nicks = ls->second;
            else if (s.msnShips.count(what))
                nicks.push_back(what);
            if (g.count < 0)
                g.count = static_cast<int>(nicks.size());
            if (nicks.empty() || g.count <= 0 || g.count > static_cast<int>(nicks.size()))
                return false;
            g.kind = 4;
            for (const std::string& n : nicks)
            {
                ShipRef r;
                r.nick = n;
                if (const auto sn = s.shipNpc.find(n); sn != s.shipNpc.end())
                    if (const auto nn = s.npcName.find(sn->second); nn != s.npcName.end())
                        r.nameIds = nn->second;
                // where it spawns: its [MsnShip] position first (MapSpawns' order), else its formation's
                for (const Script::Place& p : s.shipPos)
                    if (!r.hasPos && p.nick == n)
                        r.hasPos = true, memcpy(r.pos, p.pos, sizeof(r.pos));
                for (const auto& [form, ships] : s.formShips)
                    if (!r.hasPos && std::find(ships.begin(), ships.end(), n) != ships.end())
                        for (const Script::Place& p : s.formPos)
                            if (!r.hasPos && p.nick == form)
                                r.hasPos = true, memcpy(r.pos, p.pos, sizeof(r.pos));
                g.ships.push_back(r);
            }
            g.subject = "player";
            g.leads = Leads(s, i, shown, 4, false);
            if (g.leads.empty() || g.leads == "FAIL")
                return false;
            g.trigger = t.name;
            g.file = s.path;
            return true;
        }
        if (g.objects.empty() || g.count <= 0 || g.count > static_cast<int>(g.objects.size()))
            return false;
        g.subject = "player";
        g.leads = Leads(s, i, shown, 4, false);
        if (g.leads.empty() || g.leads == "FAIL")
            return false;
        g.trigger = t.name;
        g.file = s.path;
        return true;
    }
    else
        return false;
    if (flyingOff)
        return false; // (GoalsOf: from the system entry alone the order of the flying ones is unknown)
    g.subject = Lower(f[1]);
    if (g.subject != "player" && !s.followShips.count(g.subject))
        return false; // a ship we cannot bring there
    if (g.dist <= 0)
        return false;
    g.leads = Leads(s, i, shown, 4);
    if (g.leads.empty() || g.leads == "FAIL")
        return false;
    g.trigger = t.name;
    g.file = s.path;
    return true;
}

// ---- the live trigger table as the story source (A37 S2)

// The live table's script among the loaded ones: its path, else the script whose triggers cover the most of its ids.
static const Script* ScriptOfLive(const LiveTable& t)
{
    if (!t.ok)
        return nullptr;
    const Script* best = nullptr;
    size_t bestHits = 0;
    for (const Script& s : Scripts())
    {
        if (!t.script.empty() && _stricmp(s.path.c_str(), t.script.c_str()) == 0)
            return &s;
        size_t hits = 0;
        for (const LiveTrigger& r : t.triggers)
            hits += s.byId.count(r.id);
        if (hits > bestHits)
            best = &s, bestHits = hits;
    }
    return best;
}

// The objective nicknames of `textId` in script `s` (the step's own: a chain back to them is no advance).
static std::set<std::string> ObjectivesOf(const Script& s, u32 textId)
{
    std::set<std::string> out;
    if (!textId)
        return out;
    for (const auto& o : s.objIds)
        if (o.first != "null" && o.first != "nn_blank" && std::find(o.second.begin(), o.second.end(), textId) != o.second.end())
            out.insert(o.first);
    return out;
}

// A trigger's system: its own, else its arming trigger's (GoalsOf's systemOf).
static u32 SystemOfTrigger(const Script& s, size_t i)
{
    std::set<size_t> seen;
    while (!s.trig[i].system && seen.insert(i).second)
    {
        size_t up = s.trig.size();
        for (size_t p = 0; p < s.trig.size() && up == s.trig.size(); ++p)
            for (const auto& kv : s.trig[p].act)
                if (kv.first == "act_acttrig")
                {
                    const std::vector<std::string> f = ini::Fields(kv.second);
                    const auto it = f.empty() ? s.byName.end() : s.byName.find(Lower(f[0]));
                    if (it != s.byName.end() && it->second == i)
                    {
                        up = p;
                        break;
                    }
                }
        if (up == s.trig.size())
            break;
        i = up;
    }
    return s.trig[i].system;
}

std::vector<Goal> GoalsOfLive(const LiveTable& t, u32 system, u32 textId)
{
    std::vector<Goal> out;
    const Script* sc = ScriptOfLive(t);
    if (!sc)
        return out;
    const Script& s = *sc;
    const std::set<std::string> current = ObjectivesOf(s, textId);
    for (const LiveTrigger& r : t.triggers)
    {
        if (r.state != 0)
            continue; // (inactive or complete: it will not fire)
        const auto it = s.byId.find(r.id);
        if (it == s.byId.end())
            continue;
        const u32 tsys = SystemOfTrigger(s, it->second);
        if (tsys && tsys != system)
            continue;
        Goal g;
        if (!GoalOfTrigger(s, it->second, current, false, g))
            continue;
        g.byEntry = false;
        out.push_back(g);
    }
    return out;
}

std::vector<Leash> ArmedLeashes(const LiveTable& t, u32 system)
{
    const Script* sc = ScriptOfLive(t);
    if (!sc)
        return {};
    std::set<size_t> armed;
    for (const LiveTrigger& r : t.triggers)
        if (r.state == 0)
            if (const auto it = sc->byId.find(r.id); it != sc->byId.end())
                armed.insert(it->second);
    return LeashesOf(*sc, system, &armed);
}

std::vector<Tether> ArmedTethers(const LiveTable& t, u32 system)
{
    const Script* sc = ScriptOfLive(t);
    if (!sc)
        return {};
    std::set<size_t> armed;
    for (const LiveTrigger& r : t.triggers)
        if (r.state == 0)
            if (const auto it = sc->byId.find(r.id); it != sc->byId.end())
                armed.insert(it->second);
    return TethersOf(*sc, system, &armed);
}

EscortPlace EscortByPlace(const LiveTable& t, u32 system, u32 textId)
{
    EscortPlace out;
    const Script* sc = ScriptOfLive(t);
    if (!sc || !textId)
        return out.why = !t.ok ? "no live table" : !sc ? "no script" : "no step", out;
    const Script& s = *sc;
    const std::set<std::string> current = ObjectivesOf(s, textId);
    for (const LiveTrigger& r : t.triggers)
    {
        const auto it = r.state == 0 ? s.byId.find(r.id) : s.byId.end();
        if (it == s.byId.end())
            continue;
        const Trig& tr = s.trig[it->second];
        for (const auto& c : tr.cnd)
            if (c.first == "cnd_destroyed" && Leads(s, it->second, current, 4, false) == "FAIL")
                return out.why = "death-fail " + tr.name, out;
    }
    if (const std::vector<Leash> ls = ArmedLeashes(t, system); !ls.empty())
        return out.why = "leash " + ls[0].name, out;
    for (const Goal& g : GoalsOfLive(t, system, textId))
        if (g.subject == "player" && (g.kind == 1 || g.kind == 2))
            out.goals.push_back(g);
    out.ok = !out.goals.empty();
    if (!out.ok)
        out.why = "no player goal";
    return out;
}

std::vector<std::string> ArmedBaseEnters(const LiveTable& t, u32 textId)
{
    std::vector<std::string> out;
    const Script* sc = ScriptOfLive(t);
    if (!sc)
        return out;
    const std::set<std::string> current = ObjectivesOf(*sc, textId);
    for (const LiveTrigger& r : t.triggers)
    {
        const auto it = r.state == 0 ? sc->byId.find(r.id) : sc->byId.end();
        if (it == sc->byId.end())
            continue;
        const Trig& tr = sc->trig[it->second];
        if (tr.cnd.size() != 1 || tr.cnd[0].first != "cnd_baseenter")
            continue;
        const std::string lead = Leads(*sc, it->second, current, 4, false);
        const std::vector<std::string> f = ini::Fields(tr.cnd[0].second);
        if (!lead.empty() && lead != "FAIL" && !f.empty())
            out.push_back(Lower(f[0]));
    }
    return out;
}

// Act_LockDock = Player, <obj>, lock | unlock (the player's form; M12's "St01_to_St03_hole, Player, lock" is not it): the
// object as written, "" for another action / form / verb.
static std::string PlayerLockDock(const std::pair<std::string, std::string>& kv, const char* verb)
{
    if (kv.first != "act_lockdock")
        return std::string();
    const std::vector<std::string> f = ini::Fields(kv.second);
    return f.size() >= 3 && Lower(f[0]) == "player" && Lower(f[2]) == verb ? f[1] : std::string();
}

std::vector<LockTrap> LockTrapsOfLive(const LiveTable& t, u32 system)
{
    std::vector<LockTrap> out;
    const Script* sc = ScriptOfLive(t);
    if (!sc)
        return out;
    const Script& s = *sc;
    std::map<size_t, int> state; // script index -> its live row's state (absent: not in the live list)
    for (const LiveTrigger& r : t.triggers)
        if (const auto it = s.byId.find(r.id); it != s.byId.end())
            state[it->second] = r.state;
    for (const LiveTrigger& r : t.triggers)
    {
        const auto it = r.state == 0 ? s.byId.find(r.id) : s.byId.end();
        if (it == s.byId.end())
            continue;
        const size_t ti = it->second;
        const Trig& tr = s.trig[ti];
        const u32 tsys = SystemOfTrigger(s, ti);
        if ((tsys && tsys != system) || tr.cnd.size() != 1)
            continue;
        // the condition: Cnd_DistShip = inside, Player, <obj>, R | Cnd_DistVec = inside, Player, x, y, z, R
        const std::vector<std::string> f = ini::Fields(tr.cnd[0].second);
        LockTrap base;
        if (tr.cnd[0].first == "cnd_distship" && f.size() >= 4 && Lower(f[0]) == "inside" && Lower(f[1]) == "player")
        {
            base.kind = 1;
            base.objectName = f[2];
            base.object = CreateId(f[2]);
            base.dist = static_cast<float>(atof(f[3].c_str()));
            if (const auto sn = s.shipNpc.find(Lower(f[2])); sn != s.shipNpc.end())
                if (const auto nn = s.npcName.find(sn->second); nn != s.npcName.end())
                    base.nameIds = nn->second;
        }
        else if (tr.cnd[0].first == "cnd_distvec" && f.size() >= 6 && Lower(f[0]) == "inside" &&
                 Lower(f[1]) == "player")
        {
            base.kind = 2;
            for (int k = 0; k < 3; ++k)
                base.pos[k] = static_cast<float>(atof(f[2 + k].c_str()));
            base.dist = static_cast<float>(atof(f[5].c_str()));
        }
        if (!base.kind || base.dist <= 0)
            continue;
        base.trigger = tr.name;
        for (const auto& kv : tr.act)
        {
            const std::string locked = PlayerLockDock(kv, "lock");
            if (locked.empty())
                continue;
            const std::string want = Lower(locked);
            // the pending unlock: an armed row other than the trap (hop 0), or a trigger such a row arms (Act_ActTrig,
            // up to 4 hops whatever the conditions - M13 in_hole_range -> check_cap), never through the trap itself nor
            // into a complete row (it fired: a re-arm is not known to work)
            struct Hop
            {
                size_t i;
                int d;
            };
            std::vector<Hop> todo;
            for (const auto& [idx, st] : state)
                if (st == 0 && idx != ti)
                    todo.push_back({idx, 0});
            std::set<size_t> seen;
            LockTrap lt = base;
            for (size_t k = 0; k < todo.size() && lt.unlockBy.empty(); ++k)
            {
                const Hop h = todo[k]; // (breadth first: the nearest unlock)
                if (!seen.insert(h.i).second)
                    continue;
                for (const auto& a : s.trig[h.i].act)
                {
                    if (Lower(PlayerLockDock(a, "unlock")) == want)
                    {
                        lt.unlockBy = s.trig[h.i].name, lt.unlockHops = h.d;
                        break;
                    }
                    if (a.first != "act_acttrig" || h.d >= 4)
                        continue;
                    const std::vector<std::string> af = ini::Fields(a.second);
                    const auto nx = af.empty() ? s.byName.end() : s.byName.find(Lower(af[0]));
                    if (nx == s.byName.end() || nx->second == ti)
                        continue;
                    const auto ns = state.find(nx->second);
                    if (ns != state.end() && ns->second == 2)
                        continue;
                    todo.push_back({nx->second, h.d + 1});
                }
            }
            if (lt.unlockBy.empty())
                continue; // (no unlock to come: the lock is final - satisfying it first would close the way for good)
            lt.lockedName = locked;
            lt.locked = CreateId(locked);
            out.push_back(lt);
        }
    }
    return out;
}

std::vector<LiveLead> LeadsOfLive(const LiveTable& t, u32 textId)
{
    std::vector<LiveLead> out(t.triggers.size());
    const Script* sc = ScriptOfLive(t);
    if (!sc)
        return out;
    const std::set<std::string> current = ObjectivesOf(*sc, textId);
    for (size_t k = 0; k < t.triggers.size(); ++k)
        if (const auto it = sc->byId.find(t.triggers[k].id); it != sc->byId.end())
            out[k].leads = Leads(*sc, it->second, current, 4, true, &out[k].hops);
    return out;
}

static StepAdvance StepAdvanceIn(const Script& s, const std::set<std::string>& current, const std::set<size_t>& roots);
static std::set<size_t> SettersOf(const Script& s, const std::set<std::string>& current);

KillSafe KillAheadSafe(const LiveTable& t, u32 object, int killNo)
{
    KillSafe out;
    const Script* sc = ScriptOfLive(t);
    if (!sc)
        return out.why = "no script", out;
    const Script& s = *sc;
    std::string nick, label;
    for (const auto& so : s.solars)
        if (CreateId(so.first) == object)
            nick = so.first, label = so.second;
    if (nick.empty())
        return out.why = "not a mission solar", out;
    std::map<size_t, int> state; // script index -> live state (absent: never activated)
    for (const LiveTrigger& r : t.triggers)
        if (const auto it = s.byId.find(r.id); it != s.byId.end())
            state[it->second] = r.state;
    auto stateOf = [&](size_t i) { const auto it = state.find(i); return it == state.end() ? -1 : it->second; };
    for (size_t i = 0; i < s.trig.size(); ++i)
    {
        const Trig& tr = s.trig[i];
        if (tr.cnd.size() != 1 || tr.cnd[0].first != "cnd_destroyed")
            continue;
        const std::vector<std::string> f = ini::Fields(tr.cnd[0].second);
        if (f.empty())
            continue;
        const std::string what = Lower(f[0]);
        const int count = f.size() >= 2 ? atoi(f[1].c_str()) : 1;
        const bool fires = what == nick || (!label.empty() && what == label && count <= killNo);
        if (!fires || stateOf(i) == 2)
            continue;
        if (stateOf(i) != 0)
            return out.why = tr.name + " not armed", out;
        if (Leads(s, i, {}, 4, false) == "FAIL")
            return out.why = tr.name + " fails", out;
        // what its chain switches off: its own actions and the hops that fire at once after it
        std::set<size_t> seen;
        std::vector<std::pair<size_t, int>> todo = {{i, 0}};
        while (!todo.empty())
        {
            const auto [k, d] = todo.back();
            todo.pop_back();
            if (!seen.insert(k).second)
                continue;
            for (const auto& kv : s.trig[k].act)
            {
                const std::vector<std::string> af = ini::Fields(kv.second);
                const auto it = af.empty() ? s.byName.end() : s.byName.find(Lower(af[0]));
                if (it == s.byName.end())
                    continue;
                if (kv.first == "act_acttrig" && d < 4)
                {
                    const auto& c = s.trig[it->second].cnd;
                    if (c.size() == 1 && (c[0].first == "cnd_true" || c[0].first == "cnd_timer"))
                        todo.push_back({it->second, d + 1});
                }
                else if (kv.first == "act_deacttrig" && stateOf(it->second) == 0)
                {
                    const std::string lead = Leads(s, it->second, {}, 4, false);
                    if (!lead.empty() && lead != "FAIL")
                        return out.why = tr.name + " deacts " + s.trig[it->second].name + " (-> " + lead + ")", out;
                }
            }
        }
    }
    out.ok = true;
    return out;
}

FightsAheadView FightsAhead(u32 textId, u32 system, bool (*isBase)(u32 object))
{
    FightsAheadView out;
    if (!textId)
        return out.why = "no step", out;
    // the script that sets the text (in `system` when two do), as StepAdvanceOf picks it
    const Script* sc = nullptr;
    bool scHere = false;
    for (const Script& s : Scripts())
    {
        const std::set<std::string> current = ObjectivesOf(s, textId);
        if (current.empty())
            continue;
        const std::set<size_t> roots = SettersOf(s, current);
        bool here = !system;
        for (size_t r : roots)
            here = here || LookSystemOf(s, r) == system || !LookSystemOf(s, r);
        if (!sc || (here && !scHere))
            sc = &s, scHere = here;
    }
    if (!sc)
        return out.why = "no script", out;
    const Script& s = *sc;
    // (the story's order is the triggers' order in the file: M13's [NNObjective] list declares `penetrate` after the
    // city's objectives, the triggers come in story order - a step = the first trigger that sets its objective)
    auto firstSetter = [&](const std::set<std::string>& objs)
    {
        const std::set<size_t> roots = SettersOf(s, objs);
        return roots.empty() ? -1 : static_cast<int>(*roots.begin());
    };
    out.current = firstSetter(ObjectivesOf(s, textId));
    if (out.current < 0)
        return out.why = "no setter", out;
    for (const Script::NNObj& o : s.nnOrder)
        if (!o.object.empty() && (s.baseSolars.count(o.object) || (isBase && isBase(CreateId(o.object)))))
        {
            // (the step itself a base dock: its window ends now - mm4-b_i1 kept 17 of 22 nanobots on the way into the
            // Osiris for the next window's fights, which the dock's restock refills)
            const int at = firstSetter({o.nick});
            if (at >= out.current && (out.boundary < 0 || at < out.boundary))
                out.boundary = at;
        }
    // (a timer step's watcher: the step it belongs to ends by itself - M13's Cap2, watch_cap2)
    std::set<std::string> timerWatchers;
    for (const Script::NNObj& o : s.nnOrder)
        if (o.text && o.nick != "null")
        {
            const std::set<std::string> cur = ObjectivesOf(s, o.text);
            const StepAdvance sa = StepAdvanceIn(s, cur, SettersOf(s, cur));
            if (sa.kind == "timer" && !sa.watcher.empty())
                timerWatchers.insert(sa.watcher);
        }
    for (size_t i = 0; i < s.trig.size(); ++i)
    {
        Goal g;
        if (!ParseGoal(s, i, {}, false, g, false) || g.kind != 3 || timerWatchers.count(g.trigger))
            continue;
        const int order = static_cast<int>(i);
        if (order <= out.current || (out.boundary >= 0 && order > out.boundary))
            continue;
        const std::string label = Lower(g.objectName);
        const int n = static_cast<int>(g.objects.size());
        const int kills = g.count > 0 ? std::min(g.count, std::max(1, n)) : std::max(1, n);
        auto it = std::find_if(out.groups.begin(), out.groups.end(), [&](const FightGroup& x) { return x.label == label; });
        if (it != out.groups.end())
        {
            it->kills = std::max(it->kills, kills);
            continue;
        }
        FightGroup fg;
        fg.label = label;
        fg.objects = g.objects;
        fg.trigger = g.trigger;
        fg.leads = g.leads;
        fg.order = order;
        fg.kills = kills;
        out.groups.push_back(fg);
    }
    out.ok = true;
    return out;
}

u32 ObjectiveObjectOf(const std::string& nick, const std::string& file, u32* textId)
{
    if (textId)
        *textId = 0;
    for (const Script& s : Scripts())
    {
        if (_stricmp(s.path.c_str(), file.c_str()) != 0)
            continue;
        for (const Script::NNObj& o : s.nnOrder)
            if (o.nick == Lower(nick))
            {
                if (textId)
                    *textId = o.text;
                return o.object.empty() ? 0 : CreateId(o.object);
            }
    }
    return 0;
}

// The towards nag set in the same action list as one of the objectives `nicks` (lower case) in script `s`.
static bool NagOf(const Script& s, const std::set<std::string>& nicks, TowardsNag& out)
{
    for (const Trig& t : s.trig)
    {
        bool sets = false;
        const std::pair<std::string, std::string>* nag = nullptr;
        for (const auto& a : t.act)
        {
            if (a.first == "act_setnnobj" && !ini::Fields(a.second).empty() &&
                nicks.count(Lower(ini::Fields(a.second)[0])))
                sets = true;
            if (a.first == "act_nagdisttowards")
                nag = &a;
        }
        if (!sets || !nag)
            continue;
        const std::vector<std::string> f = ini::Fields(nag->second);
        if (f.size() >= 5 && Lower(f[0]) == "obj")
        {
            out = {};
            out.name = f[3];
            out.object = CreateId(f[3]);
            // (OBJ, name, nagger, object, fail ids, dist[, NAG_ALWAYS] - m02.ini "..., 21950, 2000"; review, session 11)
            out.dist = f.size() >= 6 ? static_cast<float>(atof(f[5].c_str())) : 0.0f;
            return true;
        }
        if (f.size() >= 7 && Lower(f[0]) == "pos")
        {
            out = {};
            out.pos = true;
            for (int j = 0; j < 3; ++j)
                out.p[j] = static_cast<float>(atof(f[3 + j].c_str()));
            out.dist = f.size() >= 8 ? static_cast<float>(atof(f[7].c_str())) : 0.0f;
            return true;
        }
    }
    return false;
}

bool NextObjectivePlace(u32 textId, u32 system, float pos[3], u32* nextText, int skip)
{
    if (!textId)
        return false;
    for (const Script& s : Scripts())
        for (size_t i = 0; i + 1 < s.nnOrder.size(); ++i)
        {
            if (s.nnOrder[i].text != textId)
                continue;
            // the next one with a point - past up to `skip` without one ("Defend Yourself!", the Unity: 0, 0, 0)
            for (size_t j = i + 1; j < s.nnOrder.size() && j <= i + 1 + static_cast<size_t>(skip); ++j)
            {
                const Script::NNObj& n = s.nnOrder[j];
                if (n.system && n.system != system)
                    return false; // (another system: not this leg of the way)
                if (!n.hasPos)
                    continue;
                memcpy(pos, n.pos, 3 * sizeof(float));
                if (nextText)
                    *nextText = n.text;
                return true;
            }
            return false;
        }
    return false;
}

// LOOK: the system a trigger names itself - its `system` line, else the system its condition enters (Cnd_SystemEnter /
// Cnd_NPCSystemEnter / Cnd_SpaceEnter = <system>, ...: M05's sye_br04 = Cnd_NPCSystemEnter = Br04, ...); 0 none.
static u32 OwnSystem(const Trig& t)
{
    if (t.system)
        return t.system;
    if (t.cnd.size() == 1 && (t.cnd[0].first == "cnd_systementer" || t.cnd[0].first == "cnd_npcsystementer" ||
                              t.cnd[0].first == "cnd_spaceenter"))
    {
        const std::vector<std::string> f = ini::Fields(t.cnd[0].second);
        if (!f.empty() && Lower(f[0]) != "no_params")
            return CreateId(f[0]);
    }
    return 0;
}
// LOOK: a trigger's system up its arming chain (SystemOfTrigger's walk, stopping at the first trigger that names one by
// OwnSystem - SystemOfTrigger reads the `system` line only: M05's com_980 -> ... -> sye_br04 has none, and the walk reached
// key_save_battleship's Br03 for Br04's dst_to_br04_tradelane).
static u32 LookSystemOf(const Script& s, size_t i)
{
    std::set<size_t> seen;
    while (!OwnSystem(s.trig[i]) && seen.insert(i).second)
    {
        size_t up = s.trig.size();
        for (size_t p = 0; p < s.trig.size() && up == s.trig.size(); ++p)
            for (const auto& kv : s.trig[p].act)
                if (kv.first == "act_acttrig")
                {
                    const std::vector<std::string> f = ini::Fields(kv.second);
                    const auto it = f.empty() ? s.byName.end() : s.byName.find(Lower(f[0]));
                    if (it != s.byName.end() && it->second == i)
                    {
                        up = p;
                        break;
                    }
                }
        if (up == s.trig.size())
            break;
        i = up;
    }
    return OwnSystem(s.trig[i]);
}

std::vector<Goal> ChainGoals(u32 textId, u32 system)
{
    std::vector<Goal> out;
    // the script and the step's objective(s) (PendingHostileShips' lookup: a placeholder names no step)
    const Script* sc = nullptr;
    std::set<std::string> current;
    for (const Script& s : Scripts())
        for (const auto& o : s.objIds)
            if (textId && o.first != "null" && o.first != "nn_blank" &&
                std::find(o.second.begin(), o.second.end(), textId) != o.second.end())
                sc = &s, current.insert(o.first);
    if (!sc)
        return out;
    const Script& s = *sc;
    std::set<size_t> roots;
    for (size_t i = 0; i < s.trig.size(); ++i)
        for (const auto& kv : s.trig[i].act)
            if (kv.first == "act_setnnobj")
            {
                const std::vector<std::string> f = ini::Fields(kv.second);
                if (f.size() == 1 && current.count(Lower(f[0])))
                    roots.insert(i);
            }
    // breadth first from the setters (hop 0), whatever the conditions; a trigger that sets a new objective is read but not
    // passed (the step after next is not this step's chain). The system as the chain runs: a trigger's own (OwnSystem),
    // else the one of the trigger that armed it on this walk (a setter: up its arming chain, LookSystemOf)
    struct Node
    {
        size_t i;
        int d;
        u32 sys;
    };
    std::set<size_t> seen;
    std::vector<Node> todo;
    for (size_t r : roots)
        todo.push_back({r, 0, LookSystemOf(s, r)});
    for (size_t head = 0; head < todo.size(); ++head)
    {
        const size_t i = todo[head].i;
        const int d = todo[head].d;
        const u32 tsys = todo[head].sys;
        if (!seen.insert(i).second)
            continue;
        const Trig& t = s.trig[i];
        if (!roots.count(i) && t.cnd.size() == 1)
        {
            Goal g;
            if ((!tsys || tsys == system) && GoalOfTrigger(s, i, current, false, g, true))
            {
                g.hops = d;
                out.push_back(g);
            }
        }
        bool newStep = false;
        for (const auto& kv : t.act)
            if (kv.first == "act_setnnobj")
            {
                const std::vector<std::string> f = ini::Fields(kv.second);
                if (f.size() == 1 && Lower(f[0]) != "nn_blank" && !current.count(Lower(f[0])) && !roots.count(i))
                    newStep = true;
            }
        if (d >= 4 || newStep)
            continue;
        for (const auto& kv : t.act)
            if (kv.first == "act_acttrig")
            {
                const std::vector<std::string> f = ini::Fields(kv.second);
                const auto it = f.empty() ? s.byName.end() : s.byName.find(Lower(f[0]));
                if (it != s.byName.end())
                {
                    const u32 own = OwnSystem(s.trig[it->second]);
                    todo.push_back({it->second, d + 1, own ? own : tsys});
                }
            }
    }
    return out;
}

std::vector<Goal> NextStepGoals(u32 textId, u32 system, std::string* next, TowardsNag* nag, bool look)
{
    if (nag)
        *nag = {};
    if (!textId)
        return {};
    for (const Script& s : Scripts())
    {
        std::set<std::string> nicks;
        for (const auto& [nick, ids] : s.objIds)
            if (std::find(ids.begin(), ids.end(), textId) != ids.end())
                nicks.insert(nick);
        if (nicks.empty())
            continue;
        for (size_t i = 0; i < s.trig.size(); ++i)
        {
            bool sets = false;
            for (const auto& a : s.trig[i].act)
                if (a.first == "act_setnnobj" && !ini::Fields(a.second).empty() &&
                    nicks.count(Lower(ini::Fields(a.second)[0])))
                    sets = true;
            if (!sets)
                continue;
            const std::string n = Leads(s, i, nicks, 6, false);
            if (n.empty() || n == "FAIL" || n == "SUCCEED")
                continue;
            if (next)
                *next = n;
            if (nag)
                NagOf(s, {n}, *nag);
            return GoalsAfter(n, s.path, system, look);
        }
    }
    return {};
}

// TIMER-DESTROY: StepAdvanceOf's walk over one script (`current` = the step's objective nicknames). Every path from a
// setter (depth first, a trigger at most once per path, 6 hops) ends at the first trigger that sets a new objective or
// succeeds (recorded), fails (dropped) or arms nothing further.
namespace
{
struct AdvancePath
{
    std::string next, via, killer; // killer: the path's first destroy trigger
    bool kill, self;
    std::vector<size_t> trail; // the path's triggers after the root (ALLY-KILL: the unlock on the kill path)
};
struct AdvanceWalk
{
    const Script& s;
    const std::set<std::string>& current;
    const std::set<size_t>& roots;
    std::vector<AdvancePath> paths;
    std::vector<size_t> on; // the path so far (after the root)
    std::set<size_t> watchers;

    static bool Destroys(const Trig& t)
    {
        for (const auto& c : t.cnd)
            if (c.first == "cnd_destroyed" || c.first == "cnd_healthdec")
                return true;
        return false;
    }
    // conditions that come by themselves: the clock and the script's own talk
    static bool ByItself(const Trig& t)
    {
        for (const auto& c : t.cnd)
            if (c.first != "cnd_timer" && c.first != "cnd_commcomplete" && c.first != "cnd_true")
                return false;
        return true;
    }
    void Walk(size_t i, int d, bool kill, bool self)
    {
        for (const auto& kv : s.trig[i].act)
        {
            const std::vector<std::string> f = ini::Fields(kv.second);
            if (!f.empty() && kv.first == "act_changestate" && Lower(f[0]) == "fail")
                return; // (a failure: no way on)
        }
        if (d > 0)
        {
            std::string next;
            for (const auto& kv : s.trig[i].act)
            {
                const std::vector<std::string> f = ini::Fields(kv.second);
                if (f.empty() || !next.empty())
                    continue;
                if (kv.first == "act_changestate" && Lower(f[0]) == "succeed")
                    next = "SUCCEED";
                else if (kv.first == "act_setnnobj" && f.size() == 1 && Lower(f[0]) != "nn_blank" &&
                         !current.count(Lower(f[0])))
                    next = Lower(f[0]);
            }
            if (!next.empty())
            {
                std::string via, killer;
                for (size_t j : on)
                {
                    via += (via.empty() ? "" : ">") + s.trig[j].name;
                    if (killer.empty() && Destroys(s.trig[j]))
                        killer = s.trig[j].name;
                }
                paths.push_back({next, via, killer, kill, self, on});
                return;
            }
        }
        if (d >= 6)
            return;
        for (const auto& kv : s.trig[i].act)
            if (kv.first == "act_acttrig")
                Visit(kv.second, d + 1, kill, self);
    }
    // the trigger an Act_ActTrig names, at hop `d` of the path so far
    void Visit(const std::string& arg, int d, bool kill, bool self)
    {
        const std::vector<std::string> f = ini::Fields(arg);
        const auto it = f.empty() ? s.byName.end() : s.byName.find(Lower(f[0]));
        if (it == s.byName.end() || roots.count(it->second) || std::find(on.begin(), on.end(), it->second) != on.end())
            return;
        const Trig& t = s.trig[it->second];
        const bool k = Destroys(t);
        if (k && Leads(s, it->second, current, 4, false) != "FAIL")
            watchers.insert(it->second);
        on.push_back(it->second);
        Walk(it->second, d, kill || k, self && ByItself(t));
        on.pop_back();
    }
};
} // namespace

// ALLY-KILL: trigger `t` as a lone Cnd_Destroyed = <label | nickname>, <count>[, <mode>[, <killer>]]: 1 it counts any
// killer (no 4th field), 0 it names one, -1 not a lone Cnd_Destroyed.
static int AnyKillerOf(const Trig& t)
{
    if (t.cnd.size() != 1 || t.cnd[0].first != "cnd_destroyed")
        return -1;
    const std::vector<std::string> f = ini::Fields(t.cnd[0].second);
    if (f.size() < 2)
        return -1;
    return f.size() < 4 || f[3].empty() ? 1 : 0;
}
// ...and the mission solars it names (nickname or label; ParseGoal's kind 3 rule), empty for ships / not one.
static std::vector<u32> DestroyedSolars(const Script& s, const Trig& t)
{
    std::vector<u32> out;
    if (AnyKillerOf(t) < 0)
        return out;
    const std::vector<std::string> f = ini::Fields(t.cnd[0].second);
    const std::string what = Lower(f[0]);
    for (const auto& so : s.solars)
        if (so.first == what || so.second == what)
            out.push_back(CreateId(so.first));
    return out;
}
// The dock the trigger `start` unlocks for the player: the first Act_LockDock = Player, <obj>, unlock in its own actions,
// else in the triggers it fires at once (Act_ActTrig to a lone Cnd_True / Cnd_Timer, Leads' "at once"), breadth first up
// to 4 hops; CreateID of <obj>, 0 none.
static u32 UnlockIn(const Script& s, size_t start)
{
    std::vector<std::pair<size_t, int>> todo = {{start, 0}};
    std::set<size_t> seen;
    for (size_t at = 0; at < todo.size(); ++at)
    {
        const size_t i = todo[at].first;
        const int d = todo[at].second;
        if (!seen.insert(i).second)
            continue;
        for (const auto& kv : s.trig[i].act)
        {
            const std::vector<std::string> f = ini::Fields(kv.second);
            if (kv.first == "act_lockdock" && f.size() >= 3 && Lower(f[0]) == "player" && Lower(f[2]) == "unlock")
                return CreateId(Lower(f[1]));
        }
        if (d >= 4)
            continue;
        for (const auto& kv : s.trig[i].act)
            if (kv.first == "act_acttrig")
            {
                const std::vector<std::string> f = ini::Fields(kv.second);
                const auto it = f.empty() ? s.byName.end() : s.byName.find(Lower(f[0]));
                if (it == s.byName.end())
                    continue;
                const auto& c = s.trig[it->second].cnd;
                if (c.size() == 1 && (c[0].first == "cnd_true" || c[0].first == "cnd_timer"))
                    todo.push_back({it->second, d + 1});
            }
    }
    return 0;
}

int AnyKiller(const std::string& trigger, const std::string& file)
{
    for (const Script& s : Scripts())
        if (_stricmp(s.path.c_str(), file.c_str()) == 0)
        {
            const auto it = s.byName.find(Lower(trigger));
            return it == s.byName.end() ? -1 : AnyKillerOf(s.trig[it->second]);
        }
    return -1;
}

static StepAdvance StepAdvanceIn(const Script& s, const std::set<std::string>& current, const std::set<size_t>& roots)
{
    StepAdvance out;
    out.file = s.path;
    AdvanceWalk w{s, current, roots, {}, {}, {}};
    for (size_t r : roots)
        w.Walk(r, 0, false, true);
    // ...and the triggers armed beside a setter (by the trigger that armed it - AdvanceGoals' view of the step: M13's
    // freighter_dies arms cap1_delay, the setter of "Destroy the Nomad Battleship", and cap_dies, its kill), at hop 1
    for (size_t p = 0; p < s.trig.size(); ++p)
    {
        if (roots.count(p))
            continue;
        bool parent = false;
        for (const auto& kv : s.trig[p].act)
            if (kv.first == "act_acttrig")
            {
                const std::vector<std::string> f = ini::Fields(kv.second);
                const auto it = f.empty() ? s.byName.end() : s.byName.find(Lower(f[0]));
                parent = parent || (it != s.byName.end() && roots.count(it->second));
            }
        if (parent)
            for (const auto& kv : s.trig[p].act)
                if (kv.first == "act_acttrig")
                    w.Visit(kv.second, 1, false, true);
    }
    const std::vector<AdvancePath>& paths = w.paths;
    const AdvancePath* pick = nullptr;
    for (const AdvancePath& p : paths)
        if (p.kill && !pick)
            pick = &p, out.kind = "kill";
    for (const AdvancePath& p : paths)
        if (p.self && !pick)
            pick = &p, out.kind = "timer";
    if (!pick && !paths.empty())
        pick = &paths[0], out.kind = "other";
    if (pick)
        out.next = pick->next, out.via = pick->via, out.watcher = pick->killer;
    // ALLY-KILL: the kill path's destroy trigger - its solars, its killer, the dock unlocked from it on
    if (pick && out.kind == "kill")
    {
        const auto kt = s.byName.find(pick->killer);
        if (kt != s.byName.end())
        {
            out.anyKiller = AnyKillerOf(s.trig[kt->second]) == 1;
            out.killObjects = DestroyedSolars(s, s.trig[kt->second]);
            bool from = false;
            for (size_t j : pick->trail)
            {
                from = from || j == kt->second;
                if (from && !out.unlocks)
                    out.unlocks = UnlockIn(s, j);
            }
        }
    }
    for (const Script::NNObj& o : s.nnOrder)
        if (pick && o.nick == out.next)
        {
            out.nextText = o.text, out.nextSystem = o.system, out.nextHasPos = o.hasPos;
            memcpy(out.nextPos, o.pos, sizeof(out.nextPos));
            out.nextObject = o.object.empty() ? 0 : CreateId(o.object);
            break;
        }
    for (const Script::NNObj& o : s.nnOrder)
        if (current.count(o.nick) && o.hasPos)
        {
            out.ownHasPos = true, memcpy(out.ownPos, o.pos, sizeof(out.ownPos));
            break;
        }
    if (out.watcher.empty() && !w.watchers.empty())
        out.watcher = s.trig[*w.watchers.begin()].name;
    return out;
}

// The setters of `current` in `s` (an Act_SetNNObj that names one of them).
static std::set<size_t> SettersOf(const Script& s, const std::set<std::string>& current)
{
    std::set<size_t> roots;
    for (size_t i = 0; i < s.trig.size(); ++i)
        for (const auto& kv : s.trig[i].act)
            if (kv.first == "act_setnnobj")
            {
                const std::vector<std::string> f = ini::Fields(kv.second);
                if (f.size() == 1 && current.count(Lower(f[0])))
                    roots.insert(i);
            }
    return roots;
}

StepAdvance StepAdvanceOf(u32 textId, u32 system)
{
    StepAdvance found;
    bool foundHere = false;
    for (const Script& s : Scripts())
    {
        const std::set<std::string> current = ObjectivesOf(s, textId);
        if (current.empty())
            continue;
        const std::set<size_t> roots = SettersOf(s, current);
        if (roots.empty())
            continue;
        bool here = !system;
        for (size_t r : roots)
            here = here || LookSystemOf(s, r) == system || !LookSystemOf(s, r);
        if (found.file.empty() || (here && !foundHere))
            found = StepAdvanceIn(s, current, roots), foundHere = here;
    }
    return found;
}

std::vector<std::string> StepAdvanceSweep(const std::string& missionsDir)
{
    std::vector<std::string> out;
    std::string root = missionsDir;
    if (!root.empty() && root.back() != '\\' && root.back() != '/')
        root += '\\';
    for (const Script& s : LoadAll(root))
    {
        const size_t slash = s.path.find_last_of("\\/");
        const std::string file = slash == std::string::npos ? s.path : s.path.substr(slash + 1);
        for (const Script::NNObj& o : s.nnOrder)
        {
            const std::set<size_t> roots = SettersOf(s, {o.nick});
            if (roots.empty())
                continue;
            const StepAdvance a = StepAdvanceIn(s, {o.nick}, roots);
            if (a.kind.empty())
                continue;
            out.push_back(file + " " + o.nick + " [" + std::to_string(o.text) + "] " + a.kind + " next=" + a.next +
                          " via=" + a.via + " watcher=" + a.watcher +
                          " solars=" + std::to_string(a.killObjects.size()) +
                          " any-killer=" + (a.anyKiller ? "1" : "0") + " unlocks=" + std::to_string(a.unlocks) +
                          " ally-kill=" + (a.AllyKillShape() ? "1" : "0"));
        }
    }
    return out;
}

// The script's ships among the spawns (the harness's Get-FLMissionShips, in C++): a spawn within 2 m of an [MsnShip]'s
// position (or a spawn action's) is that ship; one within 2 m of an [MsnFormation]'s is its first ship, and the next
// spawns within 1 km of it its other ships in order.
static std::vector<std::pair<u32, std::string>> MapSpawns(const Script& s, const std::vector<Spawn>& spawns)
{
    std::vector<std::pair<u32, std::string>> out;
    const auto near = [](const float* a, const float* b)
    {
        const float d[3] = {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
        return d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
    };
    for (size_t i = 0; i < spawns.size(); ++i)
    {
        const Spawn& sp = spawns[i];
        const Script::Place* hit = nullptr;
        for (const Script::Place& p : s.shipPos)
            if (near(p.pos, sp.pos) < 4.0f)
            {
                hit = &p;
                break;
            }
        if (hit)
        {
            out.push_back({sp.id, hit->nick});
            continue;
        }
        for (const Script::Place& p : s.formPos)
            if (near(p.pos, sp.pos) < 4.0f)
            {
                hit = &p;
                break;
            }
        if (!hit)
            continue;
        const auto fs = s.formShips.find(hit->nick);
        if (fs == s.formShips.end() || fs->second.empty())
            continue;
        out.push_back({sp.id, fs->second[0]});
        for (size_t j = 1; j < fs->second.size() && i + 1 < spawns.size(); ++j)
        {
            if (near(sp.pos, spawns[i + 1].pos) > 1000.0f * 1000.0f)
                break;
            ++i;
            out.push_back({spawns[i].id, fs->second[j]});
        }
    }
    return out;
}

std::vector<u32> PendingHostileShips(u32 textId, u32 system, const std::vector<Spawn>& spawns,
                                     std::vector<std::string>* names)
{
    std::vector<u32> out;
    // the script and the step's objective(s) of this text; a placeholder ("Awaiting") names no step: nothing (no roots -
    // and the last script may be another mission's after a load; review, session 12)
    const Script* sc = nullptr;
    std::set<std::string> current;
    for (const Script& s : Scripts())
        for (const auto& o : s.objIds)
            if (textId && o.first != "null" && o.first != "nn_blank" &&
                std::find(o.second.begin(), o.second.end(), textId) != o.second.end())
                sc = &s, current.insert(o.first);
    if (!sc)
        return out;
    const Script& s = *sc;
    std::map<size_t, std::vector<size_t>> armedBy;
    for (size_t i = 0; i < s.trig.size(); ++i)
        for (const auto& kv : s.trig[i].act)
            if (kv.first == "act_acttrig")
            {
                const std::vector<std::string> f = ini::Fields(kv.second);
                const auto it = f.empty() ? s.byName.end() : s.byName.find(Lower(f[0]));
                if (it != s.byName.end())
                    armedBy[it->second].push_back(i);
            }
    // roots: the step's setters and the trigger that armed them - one level, not GoalsOf's eight: the older chain's other
    // branches named the Navy wings of M04's later fights at its first step (offline replay of m04.ini, session 12; with
    // one level every step names only its own coming enemy: lane Rogues, West Point's alert fighters, Benford's
    // attackers, the bounty hunters)
    std::set<size_t> roots;
    for (size_t i = 0; i < s.trig.size(); ++i)
        for (const auto& kv : s.trig[i].act)
            if (kv.first == "act_setnnobj")
            {
                const std::vector<std::string> f = ini::Fields(kv.second);
                if (f.size() == 1 && current.count(Lower(f[0])))
                    roots.insert(i);
            }
    {
        std::vector<size_t> up(roots.begin(), roots.end());
        for (int level = 0; level < 1 && !up.empty(); ++level)
        {
            std::vector<size_t> next;
            for (size_t i : up)
                for (size_t p : armedBy[i])
                    if (roots.insert(p).second)
                        next.push_back(p);
            up.swap(next);
        }
    }
    auto systemOf = [&](size_t i)
    {
        std::set<size_t> seen;
        while (!s.trig[i].system && seen.insert(i).second && !armedBy[i].empty())
            i = armedBy[i][0];
        return s.trig[i].system;
    };
    // forward from the roots, whatever the conditions (comms complete, distances close: they come), 4 hops, in the player's
    // system; a trigger that sets a new objective is read but not passed (the step after next is not this step's)
    // (breadth first, every root at depth 0: depth first, a root reached through an older root came 3 hops deep and its own
    // chain was cut at the limit - s12b, M04's on_to_freeport_4 -> send_in_hacker never read, the hunters not found)
    std::set<std::string> labels;
    std::set<size_t> seen;
    std::vector<std::pair<size_t, int>> todo;
    for (size_t r : roots)
        todo.push_back({r, 0});
    for (size_t head = 0; head < todo.size(); ++head)
    {
        const size_t i = todo[head].first;
        const int d = todo[head].second;
        if (!seen.insert(i).second)
            continue;
        // (another system's trigger names no enemy here but still passes the chain on: M04's on_to_freeport_4 says
        // `system = Li01` and fires in Magellan, arming the hunters' chain)
        const u32 tsys = systemOf(i);
        const bool here = !tsys || tsys == system;
        bool newStep = false;
        for (const auto& kv : s.trig[i].act)
        {
            const std::vector<std::string> f = ini::Fields(kv.second);
            if (f.empty())
                continue;
            if (kv.first == "act_setnnobj" && f.size() == 1)
            {
                const std::string o = Lower(f[0]);
                if (o != "nn_blank" && !current.count(o) && !roots.count(i))
                    newStep = true;
            }
            // Act_SetVibeLbltoShip = <label>, Player, REP_HOSTILE_*; Act_SetVibe = <ship>, Player, REP_HOSTILE_* (roots
            // have fired: what they set is live already, the game's attitude shows it)
            if ((kv.first == "act_setvibelbltoship" || kv.first == "act_setvibe") && f.size() >= 3 && !roots.count(i) &&
                here && Lower(f[1]) == "player")
            {
                const std::string rep = Lower(f[2]);
                const bool hostile =
                    rep.compare(0, 11, "rep_hostile") == 0 ||
                    (rep.find_first_not_of("-0123456789.") == std::string::npos && atof(rep.c_str()) <= -0.6);
                if (hostile)
                    labels.insert((kv.first == "act_setvibe" ? "ship:" : "") + Lower(f[0]));
            }
        }
        if (d >= 4 || (newStep && !roots.count(i)))
            continue;
        for (const auto& kv : s.trig[i].act)
            if (kv.first == "act_acttrig")
            {
                const std::vector<std::string> f = ini::Fields(kv.second);
                const auto it = f.empty() ? s.byName.end() : s.byName.find(Lower(f[0]));
                if (it != s.byName.end())
                    todo.push_back({it->second, d + 1});
            }
    }
    // the labels' ships -> their spawned ids
    std::set<std::string> ships;
    for (const std::string& l : labels)
        if (l.compare(0, 5, "ship:") == 0)
            ships.insert(l.substr(5));
        else if (const auto ls = s.labelShips.find(l); ls != s.labelShips.end())
            ships.insert(ls->second.begin(), ls->second.end());
    if (ships.empty())
        return out;
    for (const auto& [id, nick] : MapSpawns(s, spawns))
        if (ships.count(nick))
        {
            out.push_back(id);
            if (names)
                names->push_back(nick);
        }
    return out;
}

std::vector<u32> ShipIdsOf(const Goal& g, const std::vector<Spawn>& spawns, std::vector<std::string>* names)
{
    std::vector<u32> out;
    if (g.kind != 4 || g.ships.empty())
        return out;
    const Script* sc = nullptr;
    for (const Script& s : Scripts())
        if (s.path == g.file)
            sc = &s;
    if (!sc)
        return out;
    std::set<std::string> want;
    for (const ShipRef& r : g.ships)
        want.insert(r.nick);
    for (const auto& [id, nick] : MapSpawns(*sc, spawns))
        if (want.count(nick))
        {
            out.push_back(id);
            if (names)
                names->push_back(nick);
        }
    return out;
}

static const Script* ScriptByPath(const std::string& file)
{
    for (const Script& s : Scripts())
        if (_stricmp(s.path.c_str(), file.c_str()) == 0)
            return &s;
    return nullptr;
}

SpawnAfter SpawnAfterKill(const std::string& trigger, const std::string& file)
{
    SpawnAfter out;
    const Script* sc = ScriptByPath(file);
    if (!sc)
        return out.why = "no script", out;
    const Script& s = *sc;
    const auto start = s.byName.find(Lower(trigger));
    if (start == s.byName.end())
        return out.why = "no trigger", out;
    // the walk: the trigger's own actions (hop 0) and the triggers it fires at once, breadth first (the order the game runs
    // them: a spawn and the orders given in the same breath); `armed` the triggers armed on the way, any condition
    std::vector<std::string> spawned;     // ships and formations, in order
    std::map<std::string, std::string> firstOrders; // ship -> its first ObjList (the spawn action's, else a give)
    std::set<std::string> friendly;       // ships / labels set friendly to the player on the walk
    std::vector<size_t> armed;
    std::set<size_t> seen;
    std::vector<std::pair<size_t, int>> todo = {{start->second, 0}};
    const auto place = [&](const std::vector<Script::Place>& ps, const std::string& nick, float p[3]) -> bool
    {
        bool found = false; // (the last one: a spawn action's own position is read after the [MsnShip]'s)
        for (const Script::Place& pl : ps)
            if (pl.nick == nick)
                memcpy(p, pl.pos, sizeof(pl.pos)), found = true;
        return found;
    };
    for (size_t head = 0; head < todo.size(); ++head)
    {
        const size_t i = todo[head].first;
        const int d = todo[head].second;
        if (!seen.insert(i).second)
            continue;
        for (const auto& kv : s.trig[i].act)
        {
            const std::vector<std::string> f = ini::Fields(kv.second);
            if (f.empty())
                continue;
            const std::string a = Lower(f[0]);
            if (kv.first == "act_spawnship" || kv.first == "act_spawnformation")
            {
                if (std::find(spawned.begin(), spawned.end(), a) == spawned.end())
                    spawned.push_back(a);
                // Act_SpawnShip = name, orders[, x, y, z]: its orders at spawn ("no_ol" none)
                if (kv.first == "act_spawnship" && f.size() >= 2 && Lower(f[1]) != "no_ol" && !firstOrders.count(a))
                    firstOrders[a] = Lower(f[1]);
            }
            else if (kv.first == "act_giveobjlist" && f.size() >= 2 && !firstOrders.count(a))
                firstOrders[a] = Lower(f[1]);
            else if ((kv.first == "act_setvibe" || kv.first == "act_setvibelbltoship") && f.size() >= 3 &&
                     Lower(f[1]) == "player" && Lower(f[2]).compare(0, 8, "rep_frie") == 0)
                friendly.insert(a);
            else if (kv.first == "act_acttrig")
            {
                const auto it = s.byName.find(a);
                if (it == s.byName.end())
                    continue;
                armed.push_back(it->second);
                const auto& c = s.trig[it->second].cnd;
                const bool atOnce = c.size() == 1 && (c[0].first == "cnd_true" || c[0].first == "cnd_timer");
                if (atOnce && d < 4)
                    todo.push_back({it->second, d + 1});
            }
        }
    }
    // the later step's kill: an armed Cnd_Destroyed naming a spawned ship (by nickname, or a label holding it)
    const auto named = [&](const std::string& ship, const std::string& what)
    {
        if (what == ship)
            return true;
        const auto ls = s.labelShips.find(what);
        return ls != s.labelShips.end() && std::find(ls->second.begin(), ls->second.end(), ship) != ls->second.end();
    };
    for (size_t ax = 0; ax < armed.size() && out.ship.empty(); ++ax)
    {
        const Trig& t = s.trig[armed[ax]];
        if (t.cnd.size() != 1 || t.cnd[0].first != "cnd_destroyed")
            continue;
        const std::vector<std::string> cf = ini::Fields(t.cnd[0].second);
        if (cf.empty())
            continue;
        for (const std::string& sp : spawned)
            if (s.msnShips.count(sp) && named(sp, Lower(cf[0])))
            {
                out.ship = sp, out.killTrigger = t.name;
                break;
            }
    }
    if (out.ship.empty())
        return out.why = "no spawned kill ship", out;
    out.ok = true;
    out.hasPos = place(s.shipPos, out.ship, out.pos);
    const auto fo = firstOrders.find(out.ship);
    if (fo != firstOrders.end())
    {
        out.orders = fo->second;
        const auto ol = s.objLists.find(out.orders);
        out.ordersFound = ol != s.objLists.end();
        if (out.ordersFound)
            out.cmds = ol->second;
    }
    for (const std::string& sp : spawned)
    {
        if (sp == out.ship)
            continue;
        SpawnAfter::Other o;
        o.nick = sp;
        o.hasPos = place(s.shipPos, sp, o.pos) || place(s.formPos, sp, o.pos);
        o.friendly = friendly.count(sp) != 0;
        for (const auto& [label, ships] : s.labelShips)
            if (friendly.count(label) && std::find(ships.begin(), ships.end(), sp) != ships.end())
                o.friendly = true;
        out.others.push_back(o);
    }
    return out;
}

std::vector<LeavingNag> LeavingNags(const std::string& file, u32 system)
{
    std::vector<LeavingNag> out;
    const Script* sc = ScriptByPath(file);
    if (!sc)
        return out;
    const Script& s = *sc;
    for (size_t i = 0; i < s.trig.size(); ++i)
    {
        const u32 tsys = LookSystemOf(s, i);
        if (tsys && system && tsys != system)
            continue;
        for (const auto& kv : s.trig[i].act)
        {
            if (kv.first != "act_nagdistleaving")
                continue;
            const std::vector<std::string> f = ini::Fields(kv.second);
            if (f.size() < 4)
                continue;
            LeavingNag n;
            n.name = Lower(f[0]), n.trigger = s.trig[i].name;
            if (Lower(f[2]) == "position")
            {
                if (f.size() < 7)
                    continue;
                n.pos = true;
                for (int k = 0; k < 3; ++k)
                    n.p[k] = static_cast<float>(atof(f[3 + k].c_str()));
                n.failIds = static_cast<u32>(strtoul(f[6].c_str(), nullptr, 10));
                n.dist = f.size() >= 8 ? static_cast<float>(atof(f[7].c_str())) : 0.0f;
            }
            else
            {
                n.object = f[2];
                n.objectId = CreateId(f[2]);
                n.failIds = static_cast<u32>(strtoul(f[3].c_str(), nullptr, 10));
                n.dist = f.size() >= 5 ? static_cast<float>(atof(f[4].c_str())) : 0.0f;
                const auto sn = s.shipNpc.find(Lower(f[2]));
                if (sn != s.shipNpc.end())
                {
                    const auto nn = s.npcName.find(sn->second);
                    if (nn != s.npcName.end())
                        n.nameIds = nn->second;
                }
            }
            for (const Trig& t : s.trig)
                for (const auto& a : t.act)
                    if (a.first == "act_nagoff" && !ini::Fields(a.second).empty() &&
                        Lower(ini::Fields(a.second)[0]) == n.name &&
                        std::find(n.offBy.begin(), n.offBy.end(), t.name) == n.offBy.end())
                        n.offBy.push_back(t.name);
            out.push_back(n);
        }
    }
    return out;
}

bool TowardsNagOf(u32 textId, TowardsNag& out)
{
    if (!textId)
        return false;
    for (const Script& s : Scripts())
    {
        // the objective's nickname(s) with this text
        std::set<std::string> nicks;
        for (const auto& [nick, ids] : s.objIds)
            if (std::find(ids.begin(), ids.end(), textId) != ids.end())
                nicks.insert(nick);
        if (!nicks.empty() && NagOf(s, nicks, out))
            return true;
    }
    return false;
}

bool NameTriggers(const std::vector<u32>& ids, std::string& script, std::vector<std::string>& nick,
                  std::vector<std::string>& cond, const std::string& missionsDir)
{
    script.clear();
    nick.assign(ids.size(), std::string());
    cond.assign(ids.size(), std::string());
    std::vector<Script> own;
    if (!missionsDir.empty())
    {
        std::string root = missionsDir;
        if (root.back() != '\\' && root.back() != '/')
            root += '\\';
        own = LoadAll(root);
    }
    const std::vector<Script>& all = missionsDir.empty() ? Scripts() : own;
    const Script* best = nullptr;
    size_t bestHits = 0;
    for (const Script& s : all)
    {
        size_t hits = 0;
        for (u32 id : ids)
            hits += s.byId.count(id);
        if (hits > bestHits)
            best = &s, bestHits = hits;
    }
    if (!best)
        return false;
    script = best->path;
    for (size_t i = 0; i < ids.size(); ++i)
    {
        const auto it = best->byId.find(ids[i]);
        if (it == best->byId.end())
            continue;
        const Trig& t = best->trig[it->second];
        nick[i] = t.name;
        if (!t.cnd.empty())
            cond[i] = t.cnd[0].first;
    }
    return true;
}

// (a Python repr of a string, as tools\resolver_check.py prints a goal tuple)
static std::string PyRepr(const std::string& s)
{
    const bool dq = s.find('\'') != std::string::npos && s.find('"') == std::string::npos;
    return (dq ? "\"" : "'") + s + (dq ? "\"" : "'");
}

std::vector<std::string> DecisionSweep(const std::string& missionsDir)
{
    std::vector<std::string> out;
    std::string root = missionsDir;
    if (!root.empty() && root.back() != '\\' && root.back() != '/')
        root += '\\';
    for (const Script& s : LoadAll(root))
    {
        const size_t slash = s.path.find_last_of("\\/");
        const std::string file = slash == std::string::npos ? s.path : s.path.substr(slash + 1);
        for (const auto& [obj, ids] : s.objIds)
        {
            // the objective's system: the first trigger that sets it and names one
            std::string sysNick;
            u32 system = 0;
            for (const Trig& t : s.trig)
                for (const auto& kv : t.act)
                    if (sysNick.empty() && kv.first == "act_setnnobj" && Lower(ini::Fields(kv.second)[0]) == obj)
                        sysNick = t.systemNick, system = t.system;
            if (sysNick.empty())
                continue;
            const std::vector<Goal> goals = GoalsOf(s, {obj}, system, false, {obj});
            if (goals.empty())
                continue;
            std::string line = file + " " + obj + " [" + (ids.empty() ? std::string() : std::to_string(ids[0])) + "] " +
                               sysNick + " -> [";
            for (size_t i = 0; i < goals.size(); ++i)
            {
                const Goal& g = goals[i];
                const Trig& t = s.trig[s.byName.at(g.trigger)];
                const std::vector<std::string> f = ini::Fields(t.cnd[0].second);
                std::string subject = g.subject, what;
                if (g.kind == 1)
                    what = f[2] + " " + f[3] + " m";
                else if (g.kind == 2)
                    what = "point " + f[2] + "," + f[3] + "," + f[4] + " " + f[5] + " m";
                else
                {
                    subject = "destroy";
                    what = std::to_string(g.count) + " of ";
                    const std::string name = Lower(f[0]);
                    bool first = true;
                    for (const auto& so : s.solars)
                        if (so.first == name || so.second == name)
                            what += (first ? "" : ",") + so.first, first = false;
                }
                line += (i ? ", (" : "(") + PyRepr(g.trigger) + ", " + PyRepr(subject) + ", " + PyRepr(what) + ", " +
                        PyRepr(g.leads) + ")";
            }
            out.push_back(line + "]");
        }
    }
    return out;
}
} // namespace mission
