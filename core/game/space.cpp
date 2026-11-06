// Core of space:: - the game API table, object lookup, locations, reputation, attitude, names and the frame stamp.
#include "game/space_internal.h"

#include "fldll.h"
#include "flexe.h"
#include "log.h"
#include "text.h"

namespace space
{
const Api& GetApi()
{
    static Api a;
    if (a.ok)
        return a;
    HMODULE common = GetModuleHandleA("common.dll");
    // server.dll: the single-player server (in multiplayer it may be loaded - HDE's flplusplus loads it - but holds
    // no world, or absent - vanilla): optional; its readers fall back to the client's objects
    HMODULE server = GetModuleHandleA("server.dll");
    if (!common)
        return a;
    auto get = [](HMODULE m, const char* name) {
        return m ? reinterpret_cast<void*>(GetProcAddress(m, name)) : nullptr;
    };
    a.findFirst = reinterpret_cast<decltype(a.findFirst)>(get(common, "?FindFirst@CObject@@SAPAU1@W4Class@1@@Z"));
    a.findNext = reinterpret_cast<decltype(a.findNext)>(get(common, "?FindNext@CObject@@SAPAU1@XZ"));
    a.getId = reinterpret_cast<decltype(a.getId)>(get(common, "?get_id@CSimple@@QBEABIXZ"));
    a.getType = reinterpret_cast<decltype(a.getType)>(get(common, "?get_type@CSimple@@QBEIXZ"));
    a.getName = reinterpret_cast<decltype(a.getName)>(get(common, "?get_name@CEqObj@@UBEIXZ"));
    a.isDock = reinterpret_cast<decltype(a.isDock)>(get(common, "?is_dock@CEqObj@@QBE_NXZ"));
    a.isTargetable = reinterpret_cast<decltype(a.isTargetable)>(get(common, "?is_targetable@CEqObj@@UBE_NXZ"));
    a.dockTarget = reinterpret_cast<decltype(a.dockTarget)>(get(common, "?get_dock_target@CEqObj@@QBEABIXZ"));
    a.getLocation = reinterpret_cast<decltype(a.getLocation)>(
        get(server, "?GetLocation@SpaceObj@pub@@YAHIAAVVector@@AAVMatrix@@@Z"));
    a.subobjNone = reinterpret_cast<const u16*>(get(common, "?SUBOBJ_ID_NONE@@3GB"));
    a.usingLane = reinterpret_cast<decltype(a.usingLane)>(get(common, "?is_using_tradelane@CShip@@QBE_NXZ"));
    a.laneEntry = reinterpret_cast<decltype(a.laneEntry)>(get(common, "?get_tradelane_entrance_ring@CShip@@QAEIXZ"));
    a.laneExit = reinterpret_cast<decltype(a.laneExit)>(get(common, "?get_tradelane_exit_ring@CShip@@QAEIXZ"));
    a.jumping = reinterpret_cast<decltype(a.jumping)>(get(common, "?is_jumping@CShip@@QBE_NXZ"));
    a.cruise = reinterpret_cast<decltype(a.cruise)>(get(common, "?is_cruise_active@CShip@@QBE_NXZ"));
    a.target = reinterpret_cast<decltype(a.target)>(get(common, "?get_target@CShip@@QBEPAUIObjRW@@XZ"));
    a.behavior = reinterpret_cast<decltype(a.behavior)>(
        get(common, "?get_behavior_interface@CEqObj@@QAEPAVIBehaviorManager@@XZ"));
    a.dockingPort = reinterpret_cast<decltype(a.dockingPort)>(
        get(common, "?get_docking_port@IBehaviorManager@@QAE_NAAPBUIObjRW@@AAH@Z"));
    a.relHealth = reinterpret_cast<decltype(a.relHealth)>(get(common, "?get_relative_health@CSimple@@QBEMXZ"));
    a.getRep = reinterpret_cast<decltype(a.getRep)>(get(server, "?GetRep@SpaceObj@pub@@YAHIAAH@Z"));
    a.getAttitude = reinterpret_cast<decltype(a.getAttitude)>(get(server, "?GetAttitude@Reputation@pub@@YAHABH0AAM@Z"));
    a.lootName = reinterpret_cast<decltype(a.lootName)>(get(common, "?get_name@CLoot@@UBEIXZ"));
    a.lootContents =
        reinterpret_cast<decltype(a.lootContents)>(get(common, "?contents_arch@CLoot@@QBEPAUEquipment@Archetype@@XZ"));
    a.lootUnits = reinterpret_cast<decltype(a.lootUnits)>(get(common, "?get_units@CLoot@@QBEIXZ"));
    a.archId = reinterpret_cast<decltype(a.archId)>(get(common, "?get_id@Root@Archetype@@QBEIXZ"));
    a.cargoFits = reinterpret_cast<decltype(a.cargoFits)>(
        get(common, "?get_space_for_cargo_type@CShip@@QBEHPBUEquipment@Archetype@@@Z"));
    a.maxHit = reinterpret_cast<decltype(a.maxHit)>(get(common, "?get_max_hit_pts@CSimple@@QBEMXZ"));
    a.ownerPlayer = reinterpret_cast<decltype(a.ownerPlayer)>(get(common, "?GetOwnerPlayer@CSimple@@QBEIXZ"));
    a.createId = reinterpret_cast<decltype(a.createId)>(get(common, "?CreateID@@YAIPBD@Z"));
    a.goodByArch =
        reinterpret_cast<decltype(a.goodByArch)>(get(common, "?find_by_archetype@GoodList@@YAPBUGoodInfo@@I@Z"));
    a.velocity =
        reinterpret_cast<decltype(a.velocity)>(get(common, "?GetVelocity@PhySys@@YA?AVVector@@PBUCObject@@@Z"));
    a.shipArch = reinterpret_cast<decltype(a.shipArch)>(get(common, "?shiparch@CShip@@QBEPBUShip@Archetype@@XZ"));
    a.find = reinterpret_cast<decltype(a.find)>(get(common, "?Find@CObject@@SAPAU1@ABIW4Class@1@@Z"));
    a.release = reinterpret_cast<decltype(a.release)>(get(common, "?Release@CObject@@QAEIXZ"));
    a.enginePos = reinterpret_cast<decltype(a.enginePos)>(get(common, "?get_position@EngineObject@@QBEABVVector@@XZ"));
    a.engineRot =
        reinterpret_cast<decltype(a.engineRot)>(get(common, "?get_orientation@EngineObject@@QBEABVMatrix@@XZ"));
    a.attitudeTowards =
        reinterpret_cast<decltype(a.attitudeTowards)>(get(common, "?attitude_towards@CEqObj@@QBEMPBU1@@Z"));
    a.singlePlayer = reinterpret_cast<decltype(a.singlePlayer)>(get(common, "?SinglePlayer@@YA_NXZ"));
    a.getVibe = reinterpret_cast<decltype(a.getVibe)>(get(common, "?get_vibe@CEqObj@@QBEHXZ"));
    const bool position = a.getLocation || (a.find && a.release && a.enginePos);
    a.ok = a.findFirst && a.findNext && a.getId && a.getType && a.getName && a.isDock && a.isTargetable &&
           a.dockTarget && position && a.subobjNone;
    return a;
}

bool Multiplayer()
{
    const Api& a = GetApi();
    __try
    {
        return a.singlePlayer && !a.singlePlayer();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

u32 OwnerPlayer(u32 id)
{
    const Api& a = GetApi();
    if (!a.ok || !a.ownerPlayer)
        return 0;
    void* o = ClientFind(a, id);
    u32 player = 0;
    __try
    {
        if (o)
            player = a.ownerPlayer(o, nullptr);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        player = 0;
    }
    ClientRelease(a, o);
    return player;
}

void* ClientFind(const Api& a, u32 id, bool eqObj)
{
    if (!a.find || !id)
        return nullptr;
    __try
    {
        for (u32 cls : {kClassShip, kClassSolar, kClassLoot, kClassGuided, kClassAsteroid})
            if (eqObj && cls != kClassShip && cls != kClassSolar)
                break;
            else if (void* o = a.find(id, cls))
                return o;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
    return nullptr;
}

void ClientRelease(const Api& a, void* obj)
{
    if (!obj || !a.release)
        return;
    __try
    {
        a.release(obj, nullptr);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

// The client object's position (and orientation, row-major as pub::SpaceObj::GetLocation's Matrix).
static bool ClientPose(const Api& a, u32 id, float pos[3], float* rot)
{
    if (!a.enginePos)
        return false;
    void* o = ClientFind(a, id);
    if (!o)
        return false;
    bool ok = false;
    __try
    {
        const float* p = a.enginePos(o, nullptr);
        if (p && std::isfinite(p[0]))
        {
            pos[0] = p[0], pos[1] = p[1], pos[2] = p[2];
            ok = true;
            if (rot)
            {
                const float* r = a.engineRot ? a.engineRot(o, nullptr) : nullptr;
                ok = r && std::isfinite(r[0]);
                if (ok)
                    memcpy(rot, r, sizeof(float) * 9);
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        ok = false;
    }
    ClientRelease(a, o);
    return ok;
}

// The player is flying: GetPlayerIObj is set (the ship id outlives the ship after a death).
void* PlayerIObj() { return hk::Call<void*(__cdecl*)()>(exe::GetPlayerIObj); }
bool InSpace() { return PlayerIObj() != nullptr; }

int g_lastLocation = 0; // pub::SpaceObj::GetLocation's last result code (diagnostics)

// The server's answer where the server holds the world (single player), else the client's own object (multiplayer;
// also an object the server does not know).
static bool ServerPose(const Api& a, u32 id, float pos[3], float rot[9])
{
    if (!a.getLocation)
        return false;
    pos[0] = NAN;
    __try
    {
        g_lastLocation = a.getLocation(id, pos, rot);
        // code != 0 = unknown id (multiplayer: the outputs come back zeroed, not NaN - 28 Oct 2026)
        return g_lastLocation == 0 && !std::isnan(pos[0]);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool Location(const Api& a, u32 id, float pos[3])
{
    float rot[9];
    return ServerPose(a, id, pos, rot) || ClientPose(a, id, pos, nullptr);
}

static bool Read(const Api& a, void* obj, bool ship, Contact& c)
{
    __try
    {
        c.id = *a.getId(obj, nullptr);
        c.type = a.getType(obj, nullptr);
        c.nameIds = a.getName(obj, nullptr);
        c.dock = a.isDock(obj, nullptr);
        c.targetable = a.isTargetable(obj, nullptr);
        const u32* t = a.dockTarget(obj, nullptr);
        c.base = t ? *t : 0;
        c.ship = ship;
        if (a.relHealth)
            c.health = a.relHealth(obj, nullptr);
        if (a.maxHit) // ships and solars (weapon platforms, stations: whether guns can destroy them)
            c.maxHit = a.maxHit(obj, nullptr);
        if (ship && a.target)
        {
            // CShip::get_target -> IObjRW (its get_id)
            void* t2 = a.target(obj, nullptr);
            const auto getId = dll::ObjGetId.get<dll::ObjGetIdFn>(t2);
            c.targetId = getId ? getId(t2) : 0;
        }
        if (ship && a.ownerPlayer)
            c.player = a.ownerPlayer(obj, nullptr);
        if (ship)
        {
            // in its docking sequence: its behaviour holds a dock port (the player's own reads the same way:
            // ai::DockInProgress), or it is jumping / entering a lane - the user's moment for the queue shortcut
            // ("as the NPC is locked in the docking animation (their docking lights flash red)")
            if (a.behavior && a.dockingPort)
                if (void* m = a.behavior(obj, nullptr))
                {
                    const void* port = nullptr;
                    int index = -1;
                    c.dockPort = a.dockingPort(m, &port, &index) && port;
                }
            c.jumping = a.jumping && a.jumping(obj, nullptr);
            c.lane = a.usingLane && a.usingLane(obj, nullptr);
        }
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

// Loot (CLoot): CSimple id / type / hull, its own name (CLoot::get_name), contents (archetype, units) and how many
// units the player's hold takes (CShip::get_space_for_cargo_type); always targetable, never a dock.
static bool ReadLoot(const Api& a, void* obj, void* playerShip, Contact& c)
{
    __try
    {
        c.id = *a.getId(obj, nullptr);
        c.type = a.getType(obj, nullptr) | kLoot;
        c.nameIds = a.lootName ? a.lootName(obj, nullptr) : 0;
        c.targetable = true;
        if (a.relHealth)
            c.health = a.relHealth(obj, nullptr);
        const void* arch = a.lootContents ? a.lootContents(obj, nullptr) : nullptr;
        if (arch && a.archId)
            c.lootGood = a.archId(arch, nullptr);
        if (a.lootUnits)
            c.lootUnits = a.lootUnits(obj, nullptr);
        if (arch && playerShip && a.cargoFits)
            c.lootFits = a.cargoFits(playerShip, nullptr, arch);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

// The player's CShip (IObjInspect cobject()), null when not flying.
void* PlayerCShip()
{
    void* iobj = PlayerIObj();
    if (!iobj)
        return nullptr;
    __try
    {
        const auto cobject = dll::ObjCobject.get<dll::ObjCobjectFn>(iobj);
        return cobject ? cobject(iobj) : nullptr;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return nullptr;
    }
}

static bool Rep(const Api& a, u32 id, int& rep)
{
    if (!a.getRep)
        return false;
    __try
    {
        rep = 0;
        return a.getRep(id, rep) == 0 && rep != 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

int RepOf(const Api& a, u32 id, const void* obj)
{
    int rep = 0;
    if (Rep(a, id, rep))
        return rep;
    if (!a.getVibe)
        return 0;
    void* found = obj ? nullptr : ClientFind(a, id, true); // (CEqObj only: a pod is a CSimple)
    const void* o = obj ? obj : found;
    __try
    {
        rep = o ? a.getVibe(o, nullptr) : 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        rep = 0;
    }
    ClientRelease(a, found);
    return rep;
}

int RepId(u32 id)
{
    const Api& a = GetApi();
    return a.ok ? RepOf(a, id) : 0;
}

// Attitude of `from` towards `to` (-1 hostile .. 1 friendly); leaves `att` unchanged on failure.
static void Attitude(const Api& a, int from, int to, float& att)
{
    if (!a.getAttitude)
        return;
    __try
    {
        float v = 0;
        if (a.getAttitude(from, to, v) == 0)
            att = v;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

// system 0 = every object of the class (CBeam rounds do not carry the system id at +0x50: verified 19 Oct 2026 - none
// matched while firing; only rounds whose path reaches the player matter anyway).
void Collect(const Api& a, u32 cls, u32 system, std::vector<void*>& out)
{
    __try
    {
        for (void* o = a.findFirst(cls); o && out.size() < 4096; o = a.findNext())
            if (!system || Mem<u32>(reinterpret_cast<uintptr_t>(o) + kObjSystem) == system)
                out.push_back(o);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

// Every object of a class in the registry's list, whatever its system (diagnostics).
int CountClass(const Api& a, u32 cls)
{
    int n = 0;
    __try
    {
        for (void* o = a.findFirst(cls); o && n < 4096; o = a.findNext())
            ++n;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
    return n;
}

// The client's attitude of `obj` (a CEqObj: ship or solar) towards the player's ship: Common.dll
// CEqObj::attitude_towards, the value the client holds (in multiplayer, what the server told it - the HUD's colours
// come from it). False when the call is missing or faults.
static bool ClientAttitude(const Api& a, const void* obj, const void* playerShip, float& att)
{
    if (!a.attitudeTowards || !obj || !playerShip)
        return false;
    __try
    {
        const float v = a.attitudeTowards(obj, nullptr, playerShip);
        if (!std::isfinite(v) || v < -1.01f || v > 1.01f)
            return false;
        att = v;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool AttitudeToPlayer(u32 id, float& att)
{
    const Api& a = GetApi();
    const u32 player = InSpace() ? Mem<u32>(exe::PlayerShipId) : 0;
    int rep = 0, playerRep = 0;
    if (!a.ok || !player)
        return false;
    if (Rep(a, id, rep) && Rep(a, player, playerRep))
    {
        att = 2; // unchanged = unknown
        Attitude(a, rep, playerRep, att);
        if (att <= 1)
            return true;
    }
    // the client's own value (multiplayer: the server's reputation API has no world here)
    void* o = ClientFind(a, id, true); // CEqObj::attitude_towards: ships and solars only - a pod (CLoot : CSimple) read as
    // hostile and the objective mode fought nobody instead of shooting a job's marked pod (multiplayer)
    const bool ok = o && ClientAttitude(a, o, PlayerCShip(), att);
    ClientRelease(a, o);
    return ok;
}

// A reputation's display name (an NPC pilot's "Darcy Kale"): Common.dll Reputation::Vibe::GetName gives the cached
// text or the name record (FmtStr: template "%s0 %s1" + first / last name string ids, faction_prop.ini pools), which
// the game's own formatter (exe::FmtStrFormat, as Freelancer.exe 0x41D440 does) turns into text.
static bool RepNameRaw(int rep, wchar_t* out, int max)
{
    using GetNameFn = int(__cdecl*)(const int&, void*, void*, const u16*&);
    using CtorFn = void(__fastcall*)(void*, void*, u32, const void*);
    using DtorFn = void(__fastcall*)(void*, void*);
    static HMODULE common = GetModuleHandleA("common.dll");
    static const auto getName =
        reinterpret_cast<GetNameFn>(GetProcAddress(common, "?GetName@Vibe@Reputation@@YAHABHAAUFmtStr@@1AAPBG@Z"));
    static const auto ctor = reinterpret_cast<CtorFn>(GetProcAddress(common, "??0FmtStr@@QAE@IPBUVal@0@@Z"));
    static const auto dtor = reinterpret_cast<DtorFn>(GetProcAddress(common, "??1FmtStr@@QAE@XZ"));
    if (!rep || !getName || !ctor || !dtor || !exe::FmtStrFormat.ok() || max < 2)
        return false;
    out[0] = 0;
    alignas(8) u8 name[0x100] = {}, shortName[0x100] = {};
    bool ok = false;
    __try
    {
        ctor(name, nullptr, 0, nullptr);
        ctor(shortName, nullptr, 0, nullptr);
        const u16* cached = nullptr;
        getName(rep, name, shortName, cached);
        if (cached && *cached)
            wcsncpy_s(out, max, reinterpret_cast<const wchar_t*>(cached), _TRUNCATE);
        else
            hk::Call<int(__fastcall*)(const void*, void*, wchar_t*, int)>(exe::FmtStrFormat, name, nullptr, out, max);
        ok = out[0] != 0;
        // a template the formatter could not fill (a player report, 28 Oct 2026, multiplayer: the chat showed
        // "%s0 %s1 %s2 %d0" as the player's name) is no name: fails closed, the caller falls back
        for (const wchar_t* p = out; ok && *p; ++p)
            if (p[0] == L'%' && (p[1] == L's' || p[1] == L'd' || p[1] == L'f' || p[1] == L'M') && iswdigit(p[2]))
                ok = false;
        if (!ok)
            out[0] = 0;
        dtor(name, nullptr);
        dtor(shortName, nullptr);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        ok = false;
    }
    return ok;
}

std::wstring RepName(int rep)
{
    wchar_t buf[128];
    return RepNameRaw(rep, buf, 128) ? std::wstring(buf) : std::wstring();
}

// The object with this id among the ships and solars (the class walks, as Contacts): whether it is a ship, its name ids.
static bool FindObject(const Api& a, u32 id, bool& ship, u32& nameIds)
{
    __try
    {
        for (u32 cls : {kClassShip, kClassSolar})
            for (void* o = a.findFirst(cls); o; o = a.findNext())
                if (*a.getId(o, nullptr) == id)
                {
                    ship = cls == kClassShip, nameIds = a.getName(o, nullptr);
                    return true;
                }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
    return false;
}

// The client's player list (exe::PlayerList, multiplayer): the character name of client `clientId`, looked up as the
// map's own find does (a binary tree keyed by client id; the nil node's links are 0). Bounded: game memory.
static bool PlayerListNameRaw(u32 clientId, wchar_t* out, int max)
{
    out[0] = 0;
    if (!exe::PlayerList.ok() || !clientId || max < 2)
        return false;
    __try
    {
        const uintptr_t map = exe::PlayerList.get();
        const uintptr_t head = *reinterpret_cast<const uintptr_t*>(map + 4);
        uintptr_t node = head ? *reinterpret_cast<const uintptr_t*>(head + 4) : 0; // the root
        for (int depth = 0; node && depth < 64 && *reinterpret_cast<const uintptr_t*>(node) != 0; ++depth)
        {
            const u32 key = *reinterpret_cast<const u32*>(node + 0xC);
            if (key == clientId)
            {
                const wchar_t* s = *reinterpret_cast<const wchar_t* const*>(node + 0x10 + 4);
                const u32 len = *reinterpret_cast<const u32*>(node + 0x10 + 8);
                if (!s || !len || len > 64)
                    return false;
                wcsncpy_s(out, max, s, len < static_cast<u32>(max) ? len : static_cast<u32>(max) - 1);
                return out[0] != 0;
            }
            node = *reinterpret_cast<const uintptr_t*>(node + (clientId < key ? 0 : 8));
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        out[0] = 0;
    }
    return false;
}

std::wstring PlayerListName(u32 clientId)
{
    wchar_t buf[72];
    return PlayerListNameRaw(clientId, buf, 72) ? std::wstring(buf) : std::wstring();
}

std::wstring SpeakerName(u32 id)
{
    const Api& a = GetApi();
    bool ship = false;
    u32 nameIds = 0;
    if (!a.ok || !id || !FindObject(a, id, ship, nameIds))
        return {};
    const int rep = ship ? RepOf(a, id) : 0; // multiplayer: the client's vibe
    if (rep)
    {
        const std::wstring pilot = RepName(rep);
        if (!pilot.empty())
            return pilot;
    }
    // a player's ship (multiplayer): the character name from the client's player list - the player's own reputation
    // has no name the client can format ("%s0 %s1 %s2 %d0"), other players' have theirs cached
    if (ship)
        if (const u32 owner = OwnerPlayer(id))
        {
            const std::wstring player = PlayerListName(owner);
            if (!player.empty())
                return player;
        }
    // a ship's name id is its name record's template ("%s0 %s1 %s2 %d0", a multiplayer NPC without its pilot's record):
    // no name rather than the template
    const std::wstring t = nameIds ? text::Ids(nameIds) : std::wstring();
    return t.find(L"%s0") != std::wstring::npos || t.find(L"%d0") != std::wstring::npos ? std::wstring() : t;
}

// The per-frame memo (space.h BeginFrame): g_stamp counts frames and invalidations (never 0 while on).
static u32 g_stamp = 0, g_stampNext = 0;
u32 FrameStamp() { return g_stamp; }
static void NextStamp()
{
    if (++g_stampNext == 0)
        g_stampNext = 1;
    g_stamp = g_stampNext;
}
void BeginFrame() { NextStamp(); }
void EndFrame() { g_stamp = 0; }
void InvalidateFrame()
{
    if (g_stamp)
        NextStamp();
}

static FrameMemo<std::vector<Contact>> g_contactsMemo;

static std::vector<Contact> ContactsScan();

std::vector<Contact> Contacts()
{
    perf::Scope perfAll("space::Contacts");
    if (!FrameStamp())
        return ContactsScan(); // memo off: a plain scan, as before
    // a copy either way: call sites may change their vector
    return g_contactsMemo.Valid() ? g_contactsMemo.value : g_contactsMemo.Fill(ContactsScan());
}

static std::vector<Contact> ContactsScan()
{
    perf::Scope perfScan("Contacts scan");
    std::vector<Contact> out;
    const Api& a = GetApi();
    const u32 player = InSpace() ? Mem<u32>(exe::PlayerShipId) : 0;
    float me[3];
    if (!a.ok || !player || !Location(a, player, me))
        return out;
    const u32 system = Mem<u32>(exe::PlayerSystem);
    int playerRep = 0;
    const bool haveRep = Rep(a, player, playerRep);
    void* playerShip = PlayerCShip();
    for (u32 cls : {kClassSolar, kClassShip, kClassLoot})
    {
        std::vector<void*> objs;
        Collect(a, cls, system, objs); // the registry iteration is not reentrant: read the objects afterwards
        for (void* o : objs)
        {
            Contact c;
            const bool ok = cls == kClassLoot ? ReadLoot(a, o, playerShip, c) : Read(a, o, cls == kClassShip, c);
            if (!ok || c.id == player || !Location(a, c.id, c.pos))
                continue;
            if (c.lootGood)
                c.lootValue = Price(c.lootGood) * static_cast<float>(c.lootUnits ? c.lootUnits : 1);
            const float dx = c.pos[0] - me[0], dy = c.pos[1] - me[1], dz = c.pos[2] - me[2];
            c.distance = std::sqrt(dx * dx + dy * dy + dz * dz);
            c.targetsPlayer = c.targetId == player;
            if (cls != kClassLoot && Rep(a, c.id, c.rep) && haveRep && c.ship)
            {
                Attitude(a, c.rep, playerRep, c.attitude);
                c.attitudeAsked = true;
            }
            else if (c.ship && ClientAttitude(a, o, playerShip, c.attitude))
            {
                c.attitudeAsked = true; // multiplayer: the client's values (no server reputation here)
                c.rep = RepOf(a, c.id, o);
            }
            out.push_back(c);
        }
    }
    std::sort(out.begin(), out.end(), [](const Contact& x, const Contact& y) { return x.distance < y.distance; });
    return out;
}

const Accessors& Acc()
{
    static Accessors a;
    static bool done = false;
    if (!done)
        if (HMODULE m = GetModuleHandleA("common.dll"))
        {
            done = true;
            auto get = [m](const char* n) { return reinterpret_cast<void*>(GetProcAddress(m, n)); };
            a.getArchetype =
                reinterpret_cast<decltype(a.getArchetype)>(get("?get_archetype@CObject@@QBEPAURoot@Archetype@@XZ"));
            a.projOwner = reinterpret_cast<decltype(a.projOwner)>(get("?get_owner@CProjectile@@QBEABIXZ"));
            a.equipId = reinterpret_cast<decltype(a.equipId)>(get("?GetID@CEquip@@QBEGXZ"));
            a.equipArch =
                reinterpret_cast<decltype(a.equipArch)>(get("?EquipArch@CEquip@@QBEPBUEquipment@Archetype@@XZ"));
            a.projArch = reinterpret_cast<decltype(a.projArch)>(
                get("?ProjectileArch@CELauncher@@QBEPBUProjectile@Archetype@@XZ"));
        }
    return a;
}

bool IsClass(const void* o, const char* vt) { return hk::IsExportedClass(o, "common.dll", vt); }
bool IsMunitionArch(const void* o) { return IsClass(o, "??_7Munition@Archetype@@6B@"); }
bool IsLauncherArch(const void* o)
{
    return IsClass(o, "??_7Gun@Archetype@@6B@") || IsClass(o, "??_7Launcher@Archetype@@6B@") ||
           IsClass(o, "??_7CounterMeasureDropper@Archetype@@6B@") || IsClass(o, "??_7MineDropper@Archetype@@6B@");
}
bool IsLauncherEquip(const void* o)
{
    return IsClass(o, "??_7CEGun@@6B@") || IsClass(o, "??_7CELauncher@@6B@") ||
           IsClass(o, "??_7CECounterMeasureDropper@@6B@") || IsClass(o, "??_7CEMineDropper@@6B@");
}

u32 ObjectId(const Api& a, void* o)
{
    __try
    {
        const u32* id = a.getId(o, nullptr);
        return id ? *id : 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
}

u32 ObjId(void* iobj)
{
    const auto getId = dll::ObjGetId.get<dll::ObjGetIdFn>(iobj);
    return getId ? getId(iobj) : 0;
}

std::string DataPath(const char* rel)
{
    char path[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, path, MAX_PATH); // EXE\Freelancer.exe
    if (char* slash = strrchr(path, '\\'))
        *(slash + 1) = 0;
    return std::string(path) + "..\\DATA\\" + rel;
}

u32 PlayerShipId() { return InSpace() ? Mem<u32>(exe::PlayerShipId) : 0; }

u32 ObjectiveObject(u32 ref, const std::vector<Contact>& all, u32* byId, u32* byRep)
{
    u32 id = 0, rep = 0;
    bool idIsShip = false;
    for (const Contact& c : all) // listed objects only (ObjectPos answers for unused small ids too)
    {
        if (ref && c.id == ref)
            id = c.id, idIsShip = c.ship;
        if (ref && c.rep == static_cast<int>(ref) && !rep)
            rep = c.id;
    }
    if (byId)
        *byId = id;
    if (byRep)
        *byRep = rep;
    // 'R' is a rep instance; a space id only for a non-ship (a solar's id is its nickname's hash, never small - ship ids
    // are small and overlap the reps: a ship matched by id is someone else while the named ship is not spawned yet)
    return rep ? rep : (idIsShip ? 0 : id);
}

u32 ObjectiveObject(u32 ref, u32* byId, u32* byRep)
{
    return InSpace() ? ObjectiveObject(ref, Contacts(), byId, byRep) : 0;
}

bool ObjectPos(u32 id, float pos[3])
{
    const Api& a = GetApi();
    return a.ok && InSpace() && Location(a, id, pos);
}

bool ObjectPose(u32 id, float pos[3], float rot[9])
{
    const Api& a = GetApi();
    if (!a.ok || !InSpace())
        return false;
    return (ServerPose(a, id, pos, rot) && std::isfinite(rot[0])) || ClientPose(a, id, pos, rot);
}
} // namespace space
