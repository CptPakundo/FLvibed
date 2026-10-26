#include "game/spawn.h"

#include "flexe.h"
#include "game/ai.h"
#include "game/ini.h"
#include "game/space.h"
#include "log.h"

#include <algorithm>
#include <map>

#include <fstream>
#include <sstream>

namespace spawn
{
// pub::SpaceObj::ShipInfo (FLHookSDK FLCoreServer.h; OwnerList<CargoDesc> = vptr + 16 bytes, left zeroed as plugins
// do). 0xC0 bytes.
struct ShipInfo
{
    u32 flag;             // 0x00: 5 for NPCs (ChaosMod's default creation flag)
    u32 system;           // 0x04
    u32 shipArchetype;    // 0x08
    float pos[3];         // 0x0C
    float unk1[3];        // 0x18
    float unk2[3];        // 0x24
    float orientation[9]; // 0x30
    u32 unk3;             // 0x54
    u32 loadout;          // 0x58
    u8 cargoDesc[20];     // 0x5C
    u32 look1, look2;     // 0x70
    u32 unk4, unk6;       // 0x78
    u32 comm;             // 0x80
    float unk7[7];        // 0x84
    u32 unk14;            // 0xA0
    int rep;              // 0xA4
    u32 pilotVoice;       // 0xA8
    u32 unk15;            // 0xAC
    u32 health;           // 0xB0: UINT_MAX = full
    u32 unk16, unk17;     // 0xB4
    u32 level;            // 0xBC
};
static_assert(sizeof(ShipInfo) == 0xC0, "ShipInfo layout");

struct Api
{
    int(__cdecl* create)(u32& id, const ShipInfo& si) = nullptr;
    int(__cdecl* destroy)(u32 id, int type) = nullptr;
    int(__cdecl* repAlloc)(int& rep, const void* scanner, const void* pilot) = nullptr;
    int(__cdecl* repGroup)(u32& group, const char* nickname) = nullptr;
    int(__cdecl* repAffiliation)(const int& rep, const u32& group) = nullptr;
    int(__cdecl* repAttitude)(const int& from, const int& to, float attitude) = nullptr;
    int(__cdecl* getRep)(u32 id, int& rep) = nullptr;
    int(__cdecl* forceTarget)(u32 ship, u32 target) = nullptr; // pub::AI::submit_forced_target
    int(__cdecl* getLocation)(u32 id, float* pos, float* rot) = nullptr;
    void(__fastcall* fmtCtor)(void* self, void* edx, u32 strid, const void* vals) = nullptr;
    void(__fastcall* fmtDtor)(void* self, void* edx) = nullptr;
    void(__fastcall* fmtBegin)(void* self, void* edx, u32 strid) = nullptr;
    void(__fastcall* fmtEnd)(void* self, void* edx) = nullptr;
    void(__fastcall* fmtAppendString)(void* self, void* edx, u32 strid) = nullptr; // FmtStr::append_string
    u32(__cdecl* createId)(const char* nickname) = nullptr;
    bool ok = false;
};

static const Api& GetApi()
{
    static Api a;
    if (a.ok)
        return a;
    HMODULE server = GetModuleHandleA("server.dll"), common = GetModuleHandleA("common.dll");
    if (!server || !common)
        return a;
    auto get = [](HMODULE m, const char* n) { return reinterpret_cast<void*>(GetProcAddress(m, n)); };
    a.create = reinterpret_cast<decltype(a.create)>(get(server, "?Create@SpaceObj@pub@@YAHAAIABUShipInfo@12@@Z"));
    a.destroy = reinterpret_cast<decltype(a.destroy)>(get(server, "?Destroy@SpaceObj@pub@@YAHIW4DestroyType@@@Z"));
    a.repAlloc = reinterpret_cast<decltype(a.repAlloc)>(get(server, "?Alloc@Reputation@pub@@YAHAAHABUFmtStr@@1@Z"));
    a.repGroup = reinterpret_cast<decltype(a.repGroup)>(get(server, "?GetReputationGroup@Reputation@pub@@YAHAAIPBD@Z"));
    a.repAffiliation =
        reinterpret_cast<decltype(a.repAffiliation)>(get(server, "?SetAffiliation@Reputation@pub@@YAHABHABI@Z"));
    a.repAttitude = reinterpret_cast<decltype(a.repAttitude)>(get(server, "?SetAttitude@Reputation@pub@@YAHABH0M@Z"));
    a.getRep = reinterpret_cast<decltype(a.getRep)>(get(server, "?GetRep@SpaceObj@pub@@YAHIAAH@Z"));
    a.forceTarget = reinterpret_cast<decltype(a.forceTarget)>(get(server, "?submit_forced_target@AI@pub@@YAHII@Z"));
    a.getLocation = reinterpret_cast<decltype(a.getLocation)>(
        get(server, "?GetLocation@SpaceObj@pub@@YAHIAAVVector@@AAVMatrix@@@Z"));
    a.fmtCtor = reinterpret_cast<decltype(a.fmtCtor)>(get(common, "??0FmtStr@@QAE@IPBUVal@0@@Z"));
    a.fmtDtor = reinterpret_cast<decltype(a.fmtDtor)>(get(common, "??1FmtStr@@QAE@XZ"));
    a.fmtBegin = reinterpret_cast<decltype(a.fmtBegin)>(get(common, "?begin_mad_lib@FmtStr@@QAEXI@Z"));
    a.fmtEnd = reinterpret_cast<decltype(a.fmtEnd)>(get(common, "?end_mad_lib@FmtStr@@QAEXXZ"));
    a.fmtAppendString = reinterpret_cast<decltype(a.fmtAppendString)>(get(common, "?append_string@FmtStr@@QAEXI@Z"));
    a.createId = reinterpret_cast<decltype(a.createId)>(get(common, "?CreateID@@YAIPBD@Z"));
    a.ok = a.create && a.destroy && a.repAlloc && a.repGroup && a.repAffiliation && a.getLocation && a.fmtCtor &&
           a.fmtDtor && a.fmtBegin && a.fmtEnd && a.createId;
    return a;
}

// Our spawns: the ship id and the reputation allocated for it. An id is ours only while the object still carries that
// reputation (was: ids only - they outlived a save load, and space ids are reused, so `spawn clear` could destroy
// another ship, a mission's too, 18 Oct 2026).
struct Spawned
{
    u32 id;
    int rep;
};
static std::vector<Spawned> g_spawned;
static float g_clearAt = -1; // spawn clearat: despawn ours once the player's hull fraction is at or below it

void CheckClearAt(float hull)
{
    if (g_clearAt < 0 || hull < 0)
        return;
    // Their munitions in flight count as damage already done (by owner): despawned ships' missiles still hit, and a
    // volley took the hull from above the threshold to 0 between two checks (base-repair FAIL 20 Oct 2026, "INCOMING
    // MISSILE" / "SHIELD FAILED"). Predicted = each homing munition's hull damage (game data: munition + explosion);
    // shields are ignored on purpose (worst case).
    const std::vector<u32> ours = Alive();
    float incoming = 0;
    std::vector<space::Missile> flying;
    for (const space::Missile& mi : space::Missiles())
        if (std::find(ours.begin(), ours.end(), mi.owner) != ours.end())
        {
            flying.push_back(mi);
            if (mi.targetsPlayer)
                incoming += mi.hullDamage;
        }
    ai::Self self;
    const float predicted =
        incoming > 0 && ai::ReadSelf(self) && self.hullMax > 0 ? hull - incoming / self.hullMax : hull;
    if (predicted > g_clearAt)
        return;
    int n = 0, m = 0, mFailed = 0;
    for (const space::Missile& mi : flying)
        (Despawn(mi.id) ? m : mFailed)++;
    for (u32 id : ours)
        n += Despawn(id) ? 1 : 0;
    Log("spawn: clearat %.2f reached (hull %.2f, predicted %.2f): despawned %d, their munitions in flight %d (failed "
        "%d)",
        g_clearAt, hull, predicted, n, m, mFailed);
    g_clearAt = -1;
}

static bool IsOurs(const Api& a, const Spawned& s)
{
    if (!a.getRep)
        return false;
    __try
    {
        float pos[3], rot[9];
        int rep = 0;
        return a.getLocation(s.id, pos, rot) == 0 && a.getRep(s.id, rep) == 0 && rep == s.rep;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

// The NPC hostile to the player only (private reputation), guarded.
static void MakeHostile(const Api& a, u32 npc, u32 player)
{
    if (!a.getRep || !a.repAttitude)
        return;
    __try
    {
        int npcRep = 0, playerRep = 0;
        if (a.getRep(npc, npcRep) == 0 && a.getRep(player, playerRep) == 0)
            a.repAttitude(npcRep, playerRep, -0.9f);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

static bool PlayerLocation(const Api& a, u32 player, float me[3])
{
    __try
    {
        float rot[9];
        return a.getLocation(player, me, rot) == 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

// [NPCShipArch] from DATA\MISSIONS\npcships.ini.
struct Template
{
    std::string ship, loadout, pilot, graph = "FIGHTER";
    u32 level = 1;
};
static bool FindTemplate(const std::string& npc, Template& t)
{
    char path[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, path, MAX_PATH); // EXE\Freelancer.exe -> DATA\MISSIONS\npcships.ini
    std::string p(path);
    p = p.substr(0, p.find_last_of("\\/") + 1) + "..\\DATA\\MISSIONS\\npcships.ini";
    std::ifstream in(p);
    std::string line;
    bool inside = false, found = false;
    auto trim = [](std::string s)
    {
        const size_t a = s.find_first_not_of(" \t\r"), b = s.find_last_not_of(" \t\r");
        return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
    };
    while (std::getline(in, line))
    {
        line = trim(line.substr(0, line.find(';')));
        if (line.empty())
            continue;
        if (line[0] == '[')
        {
            if (found)
                break;
            inside = _stricmp(line.c_str(), "[NPCShipArch]") == 0;
            continue;
        }
        const size_t eq = line.find('=');
        if (!inside || eq == std::string::npos)
            continue;
        const std::string k = trim(line.substr(0, eq)), v = trim(line.substr(eq + 1));
        if (_stricmp(k.c_str(), "nickname") == 0)
            found = _stricmp(v.c_str(), npc.c_str()) == 0;
        else if (found && k == "ship_archetype")
            t.ship = v;
        else if (found && k == "loadout")
            t.loadout = v;
        else if (found && k == "pilot")
            t.pilot = v;
        else if (found && k == "state_graph")
            t.graph = v;
        else if (found && k == "level" && v.size() > 1)
            t.level = static_cast<u32>(atoi(v.c_str() + 1)); // "d2" -> 2
    }
    return found && !t.ship.empty() && !t.loadout.empty();
}

// The name pool the game draws an NPC's name from (DATA\MISSIONS\faction_prop.ini [FactionProps] firstname_male /
// lastname: string id ranges): the faction that lists this npc_ship template, else the named faction. All zero when
// none.
struct NamePool
{
    u32 first0 = 0, first1 = 0, last0 = 0, last1 = 0;
    bool ok() const { return first0 && first1 >= first0 && last0 && last1 >= last0; }
};
static NamePool PoolFor(const std::string& npc, const std::string& faction)
{
    static std::map<std::string, NamePool> byNpc, byFaction;
    static bool loaded = false;
    auto lower = [](std::string s)
    {
        for (char& c : s)
            c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
        return s;
    };
    if (!loaded)
    {
        loaded = true;
        for (const auto& s : ini::Read(ini::DataPath("MISSIONS\\faction_prop.ini")))
        {
            if (s.type != "factionprops")
                continue;
            NamePool p;
            if (const auto* v = s.Get("firstname_male"))
                if (const auto f = ini::Fields(*v); f.size() >= 2)
                    p.first0 = strtoul(f[0].c_str(), nullptr, 10), p.first1 = strtoul(f[1].c_str(), nullptr, 10);
            if (const auto* v = s.Get("lastname"))
                if (const auto f = ini::Fields(*v); f.size() >= 2)
                    p.last0 = strtoul(f[0].c_str(), nullptr, 10), p.last1 = strtoul(f[1].c_str(), nullptr, 10);
            if (!p.ok())
                continue;
            if (const auto* a = s.Get("affiliation"))
                byFaction[lower(*a)] = p;
            for (const auto& n : s.All("npc_ship"))
                byNpc.emplace(lower(n), p); // the first faction listing it
        }
    }
    if (const auto it = byNpc.find(lower(npc)); it != byNpc.end())
        return it->second;
    if (const auto it = byFaction.find(lower(faction)); it != byFaction.end())
        return it->second;
    return {};
}

static int Create(const Api& a, ShipInfo& si, u32& id, const std::string& faction, const NamePool& names)
// (si.flag is left at the accepted value for the caller's diagnostics)
{
    alignas(8) u8 scanner[0x100] = {}, pilot[0x100] = {};
    __try
    {
        // Names like the game's NPCs: an empty scanner name, the pilot name "%s0 %s1" (ids 16163) with a first and a
        // last name from the faction's pools (was: without parts - contacts and the HUD showed "%s0 %s1").
        a.fmtCtor(scanner, nullptr, 0, nullptr);
        a.fmtBegin(scanner, nullptr, 0);
        a.fmtEnd(scanner, nullptr);
        a.fmtCtor(pilot, nullptr, 0, nullptr);
        a.fmtBegin(pilot, nullptr, 16163);
        if (names.ok() && a.fmtAppendString)
        {
            a.fmtAppendString(pilot, nullptr,
                              names.first0 + static_cast<u32>(rand()) % (names.first1 - names.first0 + 1));
            a.fmtAppendString(pilot, nullptr, names.last0 + static_cast<u32>(rand()) % (names.last1 - names.last0 + 1));
        }
        a.fmtEnd(pilot, nullptr);
        a.repAlloc(si.rep, scanner, pilot);
        u32 group = 0;
        if (faction == "player")
        {
            // The "unknown" faction fc_uk_grp (no relations with anyone: patrols ignore it) made hostile to the player
            // only, after creation on the reputation the ship carries (ChaosMod's EvilPlayer does the same; an NPC in
            // no faction at all never engages) - a clean arena, and no reputation of the player's changes.
            a.repGroup(group, "fc_uk_grp");
            if (group)
                a.repAffiliation(si.rep, group);
        }
        else
        {
            a.repGroup(group, faction.c_str());
            if (group)
                a.repAffiliation(si.rep, group);
        }
        // ShipInfo.flag: the server refuses (-4) a flag that does not match its current game session (ChaosMod copies
        // the one the game's own NPC creations pass); 5 fits a fresh process, a loaded save changes it. Keep the last
        // accepted one; when refused, probe (a refused Create creates nothing) and keep the accepted one. The accepted
        // flags climb with the session's object ids: upward from the last one first, then from 0, up to 65535 (was
        // 0..4095: after ~2 h of test spawns the ids passed 4400 and every spawn failed -4, 19 Oct 2026).
        static u32 flag = 5;
        si.flag = flag;
        int rc = group ? a.create(id, si) : -2;
        const u32 start = flag;
        for (u32 n = 1; group && rc == -4 && !id && n < 65536; ++n)
        {
            const u32 f = (start + n) & 0xFFFF;
            si.flag = f;
            rc = a.create(id, si);
            if (id)
                flag = f;
        }
        a.fmtDtor(scanner, nullptr);
        a.fmtDtor(pilot, nullptr);
        return rc;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return -101;
    }
}

static int ForceTarget(const Api& a, u32 ship, u32 target)
{
    if (!a.forceTarget)
        return -100;
    __try
    {
        return a.forceTarget(ship, target);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return -101;
    }
}
static std::string g_last; // diagnostics of the last spawn (spawn list)

u32 Spawn(const Request& r, std::string* err)
{
    const Api& a = GetApi();
    auto fail = [&](const std::string& why)
    {
        if (err)
            *err = why;
        return 0u;
    };
    if (!a.ok)
        return fail("server.dll / common.dll exports missing");
    const u32 player = Mem<u32>(exe::PlayerShipId);
    if (!player)
        return fail("not in space");
    Template t;
    if (!FindTemplate(r.npc, t))
        return fail("no [NPCShipArch] " + r.npc + " in npcships.ini");
    // Personality: a live NPC's as the base (cached for the session; internal members the [Pilot] files do not set
    // must be valid or the NPC never engages), the template's [Pilot] fields over it.
    // Refreshed from a live NPC at every spawn when one is around (some of those members look like pointers, which a
    // save reload may invalidate); the last one otherwise.
    static ai::Personality base;
    static bool haveBase = false;
    for (const space::Contact& c : space::Contacts())
        if (r.liveBase && c.ship &&
            std::none_of(g_spawned.begin(), g_spawned.end(), [&](const Spawned& s) { return s.id == c.id; }) &&
            ai::CopyNpcPersonality(c.id, base))
        {
            haveBase = true;
            break;
        }
    if (!r.pilot.empty())
        t.pilot = r.pilot;
    if (r.level)
        t.level = r.level;
    ai::Personality pers = base;
    std::string perr;
    const bool onLive = r.liveBase && haveBase;
    if (!t.pilot.empty() && !(onLive ? ai::LoadPilotOnto(t.pilot, pers, &perr) : ai::LoadPilot(t.pilot, pers, &perr)))
        return fail("pilot " + t.pilot + ": " + perr);
    // Templates' pilots often carry formation jobs (basic_job_formation: force_attack_formation - wingmen that only
    // fight under an encounter's leader); a lone spawn needs a solo job or it never attacks (18 Oct 2026).
    if (!r.job.empty() && !ai::ApplyBlock(r.job, pers, &perr))
        return fail("job " + r.job + ": " + perr);
    // Job allow_player_targeting: no job block sets it and its default (and a lawful patrol's) is off - such an NPC
    // never picks the player as a target (18 Oct 2026). A benchmark enemy must.
    ai::SetField(pers, "allow_player_targeting", "true", &perr);
    float me[3];
    if (!PlayerLocation(a, player, me))
        return fail("player location unknown");
    ShipInfo si = {};
    si.flag = 5;
    si.system = Mem<u32>(exe::PlayerSystem);
    si.shipArchetype = a.createId(t.ship.c_str());
    si.loadout = a.createId(t.loadout.c_str());
    const float ang = static_cast<float>(rand() % 3600) * 3.14159265f / 1800.0f;
    auto jitter = [&]() { return (static_cast<float>(rand() % 2001) / 1000.0f - 1.0f) * r.spread; };
    si.pos[0] = me[0] + cosf(ang) * r.distance + jitter();
    si.pos[1] = me[1] + jitter() * 0.3f;
    si.pos[2] = me[2] + sinf(ang) * r.distance + jitter();
    si.orientation[0] = si.orientation[4] = si.orientation[8] = 1.0f;
    si.look1 = a.createId("li_newscaster_head_gen_hat");
    si.look2 = a.createId("pl_female1_journeyman_body");
    si.comm = a.createId("comm_br_darcy_female");
    si.pilotVoice = a.createId("pilot_f_leg_f01a");
    si.health = 0xFFFFFFFFu;
    si.level = t.level;
    u32 id = 0;
    const int rc = Create(a, si, id, r.faction, PoolFor(r.npc, r.faction));
    if (rc == -3)
        return fail("player reputation unknown");
    if (rc == -2)
        return fail("no reputation group " + r.faction);
    if (!id)
        return fail("pub::SpaceObj::Create failed (" + std::to_string(rc) + ")");
    const int brain = t.pilot.empty() ? -1000 : ai::GiveBrain(id, pers, t.graph.c_str());
    g_last = "flag=" + std::to_string(si.flag) + " brain=" + std::to_string(brain) + " graph=" + t.graph +
             " pilot=" + t.pilot;
    // A private-reputation ship is in no faction, so the AI's target scan (by faction relations) never picks the
    // player: give it the player as its forced target, the game's own directive (pub::AI::submit_forced_target).
    if (r.faction == "player")
    {
        MakeHostile(a, id, player);
        if (r.force)
            g_last += " forced=" + std::to_string(ForceTarget(a, id, player));
    }
    // A fresh NPC has no orders (encounter NPCs get theirs from the encounter system) and only drifts: order it to
    // trail the player with weapons free - its combat graph takes over in range.
    if (r.trail)
    {
        ai::Directive d;
        d.op = ai::OpTrail;
        d.target = player;
        d.range = 150;
        d.fire = true;
        g_last += " trail=" + std::to_string(ai::SubmitFor(id, d));
    }
    // order=idle: the game's Idle directive - it stops and holds still (research captures: a still target at close range)
    if (r.idle)
    {
        ai::Directive d;
        d.op = ai::OpIdle;
        g_last += " idle=" + std::to_string(ai::SubmitFor(id, d));
    }
    g_spawned.push_back({id, si.rep});
    return id;
}

bool Despawn(u32 id)
{
    const Api& a = GetApi();
    if (!a.ok)
        return false;
    __try
    {
        return a.destroy(id, 0) == 0; // DestroyType VANISH
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

std::vector<u32> Alive()
{
    const Api& a = GetApi();
    std::vector<u32> out;
    std::vector<Spawned> keep;
    for (const Spawned& s : g_spawned)
        if (a.ok && IsOurs(a, s))
        {
            out.push_back(s.id);
            keep.push_back(s);
        }
    g_spawned = keep;
    return out;
}

bool Command(const std::string& cmd, const std::string& rest, std::string* reply)
{
    if (cmd != "spawn")
        return false;
    std::istringstream in(rest);
    std::string what;
    in >> what;
    if (what == "list" || what.empty())
    {
        std::ostringstream o;
        const std::vector<u32> alive = Alive();
        o << "spawned " << alive.size();
        for (u32 id : alive)
            o << " " << id;
        o << " | last: " << g_last;
        *reply = o.str();
        return true;
    }
    if (what == "hunt") // spawn hunt: renew every live spawn's order to trail the player, weapons free
    {
        const u32 player = Mem<u32>(exe::PlayerShipId);
        int n = 0;
        for (u32 id : Alive())
        {
            ai::Directive d;
            d.op = ai::OpTrail;
            d.target = player;
            d.range = 150;
            d.fire = true;
            n += ai::SubmitFor(id, d) >= 0 ? 1 : 0;
        }
        *reply = "hunting " + std::to_string(n);
        return true;
    }
    if (what == "brainfrom") // spawn brainfrom <npc id>: give every live spawn that NPC's personality (diagnostics)
    {
        u32 npc = 0;
        in >> npc;
        ai::Personality p;
        if (!ai::CopyNpcPersonality(npc, p))
        {
            *reply = "copy failed";
            return true;
        }
        std::ostringstream o;
        o << "brain";
        for (u32 id : Alive())
            o << " " << id << "=" << ai::GiveBrain(id, p, "FIGHTER");
        *reply = o.str();
        return true;
    }
    if (what == "braincur") // spawn braincur: give every live spawn the current `ai` personality (ai pilot / ai field)
    {
        std::ostringstream o;
        o << "brain";
        for (u32 id : Alive())
            o << " " << id << "=" << ai::GiveBrain(id, ai::Current(), "FIGHTER");
        *reply = o.str();
        return true;
    }
    if (what == "clear")
    {
        int n = 0;
        for (u32 id : Alive())
            n += Despawn(id) ? 1 : 0;
        *reply = "despawned " + std::to_string(n);
        return true;
    }
    if (what == "clearat") // spawn clearat <hull 0..1> | off: game-side, every frame (CheckClearAt), our spawns go
    {                      // the moment the player's hull falls to it - no harness poll latency (parallel sweeps)
        std::string v;
        in >> v;
        g_clearAt = (v.empty() || v == "off") ? -1.0f : static_cast<float>(atof(v.c_str()));
        *reply = g_clearAt < 0 ? "clearat off" : "clearat " + std::to_string(g_clearAt);
        return true;
    }
    Request r;
    r.npc = what;
    int count = 1;
    // Positional: faction, count, distance; options key=value.
    std::string tok;
    int pos = 0;
    while (in >> tok)
    {
        const size_t eq = tok.find('=');
        if (eq == std::string::npos)
        {
            if (pos == 0)
                r.faction = tok;
            else if (pos == 1)
                count = atoi(tok.c_str());
            else if (pos == 2)
                r.distance = static_cast<float>(atof(tok.c_str()));
            ++pos;
            continue;
        }
        const std::string k = tok.substr(0, eq), v = tok.substr(eq + 1);
        if (k == "job")
            r.job = v == "keep" ? "" : v;
        else if (k == "pilot")
            r.pilot = v;
        else if (k == "order")
            r.trail = v == "trail", r.idle = v == "idle";
        else if (k == "base")
            r.liveBase = v != "default";
        else if (k == "force")
            r.force = v != "0";
        else if (k == "level")
            r.level = static_cast<u32>(atoi(v.c_str()));
        else
        {
            *reply = "unknown option " + k;
            return true;
        }
    }
    if (r.faction.empty())
    {
        *reply =
            "usage: spawn <npcshiparch> <faction group|player> [count] [distance] [job=<block>|keep] [pilot=<nick>] "
            "[order=trail|none] [base=live|default] [force=0|1] [level=n] | list | hunt | clear";
        return true;
    }
    std::ostringstream o;
    o << "spawned";
    for (int i = 0; i < std::max(1, count); ++i)
    {
        std::string err;
        const u32 id = Spawn(r, &err);
        if (!id)
        {
            o << " failed: " << err;
            break;
        }
        o << " " << id;
    }
    *reply = o.str();
    return true;
}
} // namespace spawn
