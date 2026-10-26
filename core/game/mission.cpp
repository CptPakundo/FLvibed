// Story mission scripts: the conditions that advance the current step (mission.h).
#include "mission.h"
#include "flhash.h"
#include "ini.h"
#include "../log.h"

#include <algorithm>
#include <map>
#include <set>

namespace mission
{
static std::string Lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(tolower(c)); });
    return s;
}

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
    std::map<std::string, std::vector<u32>> objIds; // [NNObjective] nickname (lower) -> its text ids
    std::set<std::string> followLists;              // [ObjList]s that follow / stay with the player
    std::set<std::string> followShips;              // ships given such a list (Act_GiveObjList / Act_SpawnShip)
    std::vector<std::pair<std::string, std::string>> solars;    // [MsnSolar] nickname, label (lower)
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
            }
        }
        else if (sec.type == "msnship" && nick)
        {
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
        }
        else if (sec.type == "objlist" && nick)
        {
            for (const auto& kv : sec.kv)
                if ((kv.first == "follow" || kv.first == "followplayer" || kv.first == "stayinrange") &&
                    Lower(kv.second).compare(0, 6, "player") == 0)
                    s.followLists.insert(Lower(*nick));
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
static std::vector<Script>& Scripts()
{
    static std::vector<Script> all;
    static bool loaded = false;
    if (loaded)
        return all;
    loaded = true;
    all = LoadAll(ini::DataPath("MISSIONS\\"));
    Log("mission: %u scripts with triggers", static_cast<unsigned>(all.size()));
    return all;
}

// What a trigger's action chain leads to within `depth` hops: a new objective (not one of `current`, not nn_blank) or
// SUCCEED; "" if nothing; "FAIL" if the chain fails the mission. `armedFail` false: a FAIL counts only through triggers
// that fire at once (Cnd_True / Cnd_Timer), not in a watcher the chain merely arms.
static std::string Leads(const Script& s, size_t start, const std::set<std::string>& current, int depth,
                         bool armedFail = true)
{
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
                    return "FAIL";
                if (v == "succeed" && found.empty())
                    found = "SUCCEED";
            }
            else if (kv.first == "act_setnnobj" && f.size() == 1)
            {
                const std::string o = Lower(f[0]);
                if (o != "nn_blank" && !current.count(o) && found.empty())
                    found = o;
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
                                 const std::set<std::string>& shown, const char* rootTrigger = nullptr);

static const Script* lastScript = nullptr; // the script AdvanceGoals last found (Leashes)

std::vector<Leash> Leashes(u32 system)
{
    std::vector<Leash> out;
    if (!lastScript)
        return out;
    const Script& s = *lastScript;
    for (size_t i = 0; i < s.trig.size(); ++i)
    {
        const Trig& t = s.trig[i];
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

std::vector<Goal> AdvanceGoals(u32 textId, u32 system, bool* fromLast, u32 titleId)
{
    static u32 lastText = 0;
    if (fromLast)
        *fromLast = false;
    std::vector<Goal> out;
    // the script and objective(s) behind the text
    const Script* sc = nullptr;
    std::set<std::string> current;
    // (a placeholder objective - "null", "nn_blank": "Awaiting Mission Objective" 21660 - is no step: many triggers set it,
    // M13's own watch_endgame_object among them, and its setters as roots hid the city's destroy condition; 23 Oct 2026)
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
        lastScript = sc, lastText = textId;
    else if (lastScript && lastText)
    {
        find(lastText);
        if (fromLast)
            *fromLast = true;
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
    // objectives this mission has shown (current ones included): a goal must lead to a fresh one - an old branch that
    // fired long ago leads back to them
    for (const std::string& o : current)
        g_shown[sc].insert(o);
    return GoalsOf(*sc, current, system, byTitle, g_shown[sc]);
}

std::vector<Goal> GoalsAfter(const std::string& objective, const std::string& file, u32 system)
{
    for (const Script& s : Scripts())
        if (s.path == file)
        {
            std::set<std::string> shownThen = g_shown[&s];
            shownThen.insert(Lower(objective));
            return GoalsOf(s, {Lower(objective)}, system, false, shownThen);
        }
    return {};
}

std::vector<Goal> GoalsArmedBy(const std::string& trigger, const std::string& file, u32 system)
{
    for (const Script& s : Scripts())
        if (s.path == file)
            return GoalsOf(s, {}, system, false, g_shown[&s], trigger.c_str());
    return {};
}

// The goals of one step: `current` = its objective nickname(s); `shown` = the objectives the mission has shown by then.
static std::vector<Goal> GoalsOf(const Script& s, const std::set<std::string>& current, u32 system, bool byTitle,
                                 const std::set<std::string>& shown, const char* rootTrigger)
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
        const std::vector<std::string> f = ini::Fields(t.cnd[0].second);
        Goal g;
        g.byEntry = !armedByStep.count(i);
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
            for (const auto& so : s.solars)
                if (so.first == what || so.second == what)
                    g.objects.push_back(CreateId(so.first));
            if (g.objects.empty() || g.count <= 0 || g.count > static_cast<int>(g.objects.size()))
                continue;
            g.subject = "player";
            g.leads = Leads(s, i, shown, 4, false);
            if (g.leads.empty() || g.leads == "FAIL")
                continue;
            g.trigger = t.name;
            g.file = s.path;
            out.push_back(g);
            continue;
        }
        else
            continue;
        if (byTitle)
            continue; // (above)
        g.subject = Lower(f[1]);
        if (g.subject != "player" && !s.followShips.count(g.subject))
            continue; // a ship we cannot bring there
        if (g.dist <= 0)
            continue;
        g.leads = Leads(s, i, shown, 4);
        if (g.leads.empty() || g.leads == "FAIL")
            continue;
        g.trigger = t.name;
        g.file = s.path;
        out.push_back(g);
    }
    return out;
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

std::vector<Goal> NextStepGoals(u32 textId, u32 system, std::string* next, TowardsNag* nag)
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
            return GoalsAfter(n, s.path, system);
        }
    }
    return {};
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
