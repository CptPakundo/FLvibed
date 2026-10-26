#include "game/ai.h"

#include "fldll.h"
#include "flexe.h"
#include "game/space.h"
#include "log.h"
#include "str.h"

#include <cstddef>
#include <fstream>
#include <map>
#include <sstream>

namespace ai
{
// ------------------------------------------------------------------ names
static const char* kOpNames[] = {"null",  "buzz",   "goto", "trail",     "flee",         "evade",      "idle",
                                 "dock",  "launch", "lane", "formation", "largeship",    "cruise",     "strafe",
                                 "guide", "face",   "loot", "follow",    "drasticevade", "freeflight", "delay"};
const char* OpName(int op) { return op >= 0 && op <= 20 ? kOpNames[op] : "none"; }
int OpFromName(const std::string& name)
{
    for (int i = 0; i <= 20; ++i)
        if (_stricmp(name.c_str(), kOpNames[i]) == 0)
            return i;
    char* end = nullptr;
    const long v = strtol(name.c_str(), &end, 10);
    return end && *end == 0 && v >= -1 && v <= 20 ? static_cast<int>(v) : -2;
}

// ------------------------------------------------------------------ game API
struct Api
{
    // server.dll pub::AI
    int(__cdecl* submitDirective)(u32 ship, void* op) = nullptr;
    int(__cdecl* submitState)(u32 ship, void* op) = nullptr;
    int(__cdecl* graphId)(u32 ship) = nullptr;
    int(__cdecl* behaviorId)(u32 ship) = nullptr;
    bool(__cdecl* getPersonality)(u32 ship, void* personality) = nullptr;
    int(__cdecl* forceTarget)(u32 ship, u32 target) = nullptr;
    int(__cdecl* removeForced)(u32 ship, u32 target) = nullptr;
    // Common.dll
    int(__cdecl* stateGraph)(const char* name, int type) = nullptr;
    void(__fastcall* personalityCtor)(void* self, void* edx) = nullptr;
    void(__fastcall* setPersonalityCtor)(void* self, void* edx) = nullptr;
    void*(__fastcall* behavior)(void* cship, void* edx) = nullptr;
    // IBehaviorManager (client side: the player's own manager)
    void(__fastcall* mSetPersonality)(void* m, void* edx, const void* personality) = nullptr;
    void(__fastcall* mGetPersonality)(void* m, void* edx, void* personality) = nullptr;
    void(__fastcall* mSetGraph)(void* m, void* edx, int graph, bool b) = nullptr;
    void(__fastcall* mRefreshGraph)(void* m, void* edx) = nullptr;
    bool(__fastcall* mSetDirective)(void* m, void* edx, const void* info, const void* op) = nullptr;
    // ?get_docking_port@IBehaviorManager@@QAE_NAAPBUIObjRW@@AAH@Z: bool (const IObjRW*& port, int& index)
    bool(__thiscall* mDockingPort)(void* m, const void** port, int* index) = nullptr;
    // The player ship's capability (Self): CShip / CEqObj / CEquipManager / CEShield
    bool(__fastcall* gunRange)(void* ship, void* edx, float* range) = nullptr;
    float(__fastcall* powerRatio)(void* ship, void* edx) = nullptr;
    int(__fastcall* repairRoom)(void* ship, void* edx) = nullptr;
    int(__fastcall* batteryRoom)(void* ship, void* edx) = nullptr;
    const u8*(__fastcall* shipArch)(void* ship, void* edx) = nullptr;
    void*(__fastcall* equipFirst)(void* mgr, void* edx, u32 cls) = nullptr;
    float(__fastcall* shieldHp)(void* shield, void* edx) = nullptr;
    float(__fastcall* shieldMax)(void* shield, void* edx) = nullptr;
    float(__fastcall* relHealth)(void* obj, void* edx) = nullptr;
    float(__fastcall* maxHit)(void* obj, void* edx) = nullptr;
    float(__fastcall* holdRemaining)(void* ship, void* edx) = nullptr;    // CShip::get_cargo_hold_remaining
    const void*(__fastcall* equipArch)(void* equip, void* edx) = nullptr; // CEquip::EquipArch
    u32(__fastcall* archId)(const void* arch, void* edx) = nullptr;       // Archetype::Root::get_id
    u32(__cdecl* createId)(const char* nickname) = nullptr;               // CreateID
    // Walking the ship's equipment for a consumable (UseConsumable)
    void(__fastcall* traverserCtor)(void* t, void* edx, int classes) = nullptr; // CEquipTraverser(int)
    void*(__fastcall* traverse)(void* mgr, void* edx, void* t) = nullptr;       // CEquipManager::Traverse
    void*(__cdecl* castCargo)(void* equip) = nullptr;                           // CECargo::cast
    u16(__fastcall* equipId)(const void* equip, void* edx) = nullptr;           // CEquip::GetID
    HMODULE common = nullptr;
    bool ok = false;
};

static const Api& GetApi()
{
    static Api a;
    if (a.ok)
        return a;
    HMODULE server = GetModuleHandleA("server.dll");
    a.common = GetModuleHandleA("common.dll");
    if (!server || !a.common)
        return a;
    auto get = [](HMODULE m, const char* n) { return reinterpret_cast<void*>(GetProcAddress(m, n)); };
    a.submitDirective = reinterpret_cast<decltype(a.submitDirective)>(
        get(server, "?SubmitDirective@AI@pub@@YA?AW4OP_RTYPE@12@IPAVBaseOp@12@@Z"));
    a.submitState = reinterpret_cast<decltype(a.submitState)>(
        get(server, "?SubmitState@AI@pub@@YA?AW4OP_RTYPE@12@IPAVBaseOp@12@@Z"));
    a.graphId = reinterpret_cast<decltype(a.graphId)>(get(server, "?get_state_graph_id@AI@pub@@YAHI@Z"));
    a.behaviorId = reinterpret_cast<decltype(a.behaviorId)>(get(server, "?get_behavior_id@AI@pub@@YAHI@Z"));
    a.getPersonality =
        reinterpret_cast<decltype(a.getPersonality)>(get(server, "?get_personality@AI@pub@@YA_NIAAVPersonality@12@@Z"));
    a.forceTarget = reinterpret_cast<decltype(a.forceTarget)>(get(server, "?submit_forced_target@AI@pub@@YAHII@Z"));
    a.removeForced = reinterpret_cast<decltype(a.removeForced)>(get(server, "?remove_forced_target@AI@pub@@YAHII@Z"));
    a.stateGraph =
        reinterpret_cast<decltype(a.stateGraph)>(get(a.common, "?get_state_graph@StateGraph@pub@@YAHPBDW4Type@12@@Z"));
    a.personalityCtor = reinterpret_cast<decltype(a.personalityCtor)>(get(a.common, "??0Personality@AI@pub@@QAE@XZ"));
    a.setPersonalityCtor =
        reinterpret_cast<decltype(a.setPersonalityCtor)>(get(a.common, "??0SetPersonalityParams@AI@pub@@QAE@XZ"));
    a.behavior = reinterpret_cast<decltype(a.behavior)>(
        get(a.common, "?get_behavior_interface@CEqObj@@QAEPAVIBehaviorManager@@XZ"));
    a.mSetPersonality = reinterpret_cast<decltype(a.mSetPersonality)>(
        get(a.common, "?set_personality@IBehaviorManager@@QAEXABVPersonality@AI@pub@@@Z"));
    a.mGetPersonality = reinterpret_cast<decltype(a.mGetPersonality)>(
        get(a.common, "?get_personality@IBehaviorManager@@QAEXAAVPersonality@AI@pub@@@Z"));
    a.mSetGraph =
        reinterpret_cast<decltype(a.mSetGraph)>(get(a.common, "?set_state_graph@IBehaviorManager@@QAEXH_N@Z"));
    a.mDockingPort = reinterpret_cast<decltype(a.mDockingPort)>(
        get(a.common, "?get_docking_port@IBehaviorManager@@QAE_NAAPBUIObjRW@@AAH@Z"));
    a.mRefreshGraph =
        reinterpret_cast<decltype(a.mRefreshGraph)>(get(a.common, "?refresh_state_graph@IBehaviorManager@@QAEXXZ"));
    a.mSetDirective = reinterpret_cast<decltype(a.mSetDirective)>(
        get(a.common, "?set_current_directive@IBehaviorManager@@QAE_NABUIDirectiveInfo@@PBVBaseOp@AI@pub@@@Z"));
    a.gunRange =
        reinterpret_cast<decltype(a.gunRange)>(get(a.common, "?get_farthest_active_gun_range@CShip@@QBE_NAAM@Z"));
    a.powerRatio = reinterpret_cast<decltype(a.powerRatio)>(get(a.common, "?get_power_ratio@CEqObj@@QBEMXZ"));
    a.repairRoom =
        reinterpret_cast<decltype(a.repairRoom)>(get(a.common, "?get_repair_kit_capacity_remaining@CShip@@QBEHXZ"));
    a.batteryRoom = reinterpret_cast<decltype(a.batteryRoom)>(
        get(a.common, "?get_shield_battery_capacity_remaining@CShip@@QBEHXZ"));
    a.shipArch = reinterpret_cast<decltype(a.shipArch)>(get(a.common, "?shiparch@CShip@@QBEPBUShip@Archetype@@XZ"));
    a.equipFirst =
        reinterpret_cast<decltype(a.equipFirst)>(get(a.common, "?FindFirst@CEquipManager@@QAEPAVCEquip@@I@Z"));
    a.shieldHp = reinterpret_cast<decltype(a.shieldHp)>(get(a.common, "?GetHitPoints@CEShield@@UBEMXZ"));
    a.shieldMax = reinterpret_cast<decltype(a.shieldMax)>(get(a.common, "?GetMaxHitPoints@CEShield@@UBEMXZ"));
    a.relHealth = reinterpret_cast<decltype(a.relHealth)>(get(a.common, "?get_relative_health@CSimple@@QBEMXZ"));
    a.maxHit = reinterpret_cast<decltype(a.maxHit)>(get(a.common, "?get_max_hit_pts@CSimple@@QBEMXZ"));
    a.holdRemaining =
        reinterpret_cast<decltype(a.holdRemaining)>(get(a.common, "?get_cargo_hold_remaining@CShip@@QBEMXZ"));
    a.equipArch =
        reinterpret_cast<decltype(a.equipArch)>(get(a.common, "?EquipArch@CEquip@@QBEPBUEquipment@Archetype@@XZ"));
    a.archId = reinterpret_cast<decltype(a.archId)>(get(a.common, "?get_id@Root@Archetype@@QBEIXZ"));
    a.createId = reinterpret_cast<decltype(a.createId)>(get(a.common, "?CreateID@@YAIPBD@Z"));
    a.traverserCtor = reinterpret_cast<decltype(a.traverserCtor)>(get(a.common, "??0CEquipTraverser@@QAE@H@Z"));
    a.traverse = reinterpret_cast<decltype(a.traverse)>(
        get(a.common, "?Traverse@CEquipManager@@QAEPAVCEquip@@AAVCEquipTraverser@@@Z"));
    a.castCargo = reinterpret_cast<decltype(a.castCargo)>(get(a.common, "?cast@CECargo@@SAPAV1@PAVCEquip@@@Z"));
    a.equipId = reinterpret_cast<decltype(a.equipId)>(get(a.common, "?GetID@CEquip@@QBEGXZ"));
    a.ok = a.submitDirective && a.submitState && a.graphId && a.behaviorId && a.stateGraph && a.personalityCtor &&
           a.setPersonalityCtor && a.behavior;
    return a;
}

static void* PlayerIObj() { return hk::Call<void*(__cdecl*)()>(exe::GetPlayerIObj); }
static u32 PlayerId() { return PlayerIObj() ? Mem<u32>(exe::PlayerShipId) : 0; }

// The player's CShip (IObjInspect cobject(), by name: coreldll.h).
static void* PlayerShip()
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

// The player's behaviour manager (CShip -> CEqObj::get_behavior_interface).
static u8* Manager(const Api& a)
{
    void* ship = PlayerShip();
    if (!ship)
        return nullptr;
    __try
    {
        return static_cast<u8*>(a.behavior(ship, nullptr));
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return nullptr;
    }
}

// CEqObj +0xE4: the equipment manager (FLHookSDK CEqObj dword 57); Archetype::Ship +0x140 / +0x144: nanobot /
// shield battery limits (verified against shiparch.ini in game). Equipment class: shield 1 << 8.
static const uintptr_t kEquipManager = 0xE4, kArchNanobots = 0x140, kArchBatteries = 0x144, kArchHold = 0x100;
static const u32 kEquipShield = 1u << 8, kEquipShieldGenerator = 1u << 9,
                 kEquipTractor = 1u << 20; // FLHookSDK EquipmentClass

static std::string Trim(std::string s);

// [Tractor] max_length of every tractor in DATA\EQUIPMENT\*.ini, by archetype id (CreateID of its nickname).
static float TractorRange(const Api& a, u32 archetype)
{
    static std::map<u32, float> ranges;
    static bool loaded = false;
    if (!loaded && a.createId)
    {
        loaded = true;
        const std::string dir = DllDir() + "..\\DATA\\EQUIPMENT\\";
        WIN32_FIND_DATAA fd;
        HANDLE h = FindFirstFileA((dir + "*.ini").c_str(), &fd);
        for (BOOL more = h != INVALID_HANDLE_VALUE; more; more = FindNextFileA(h, &fd))
        {
            std::ifstream in(dir + fd.cFileName);
            std::string line, nick;
            bool tractor = false;
            while (std::getline(in, line))
            {
                const size_t semi = line.find(';');
                if (semi != std::string::npos)
                    line.erase(semi);
                const std::string l = Trim(line);
                if (l.empty())
                    continue;
                if (l[0] == '[')
                {
                    tractor = _stricmp(l.c_str(), "[Tractor]") == 0;
                    nick.clear();
                    continue;
                }
                const size_t eq = l.find('=');
                if (!tractor || eq == std::string::npos)
                    continue;
                const std::string k = Trim(l.substr(0, eq)), v = Trim(l.substr(eq + 1));
                if (_stricmp(k.c_str(), "nickname") == 0)
                    nick = v;
                else if (_stricmp(k.c_str(), "max_length") == 0 && !nick.empty())
                    ranges[a.createId(nick.c_str())] = static_cast<float>(atof(v.c_str()));
            }
        }
        if (h != INVALID_HANDLE_VALUE)
            FindClose(h);
    }
    const auto it = ranges.find(archetype);
    return it == ranges.end() ? 0.0f : it->second;
}

// Class checks before fields are read by offset (Common.dll's exported vtables): another build or a mod's class
// fails closed instead of reading garbage.
static bool IsShip(const void* o) { return hk::IsExportedClass(o, "common.dll", "??_7CShip@@6BCEqObj@@@"); }
static bool IsShipArch(const void* o)
{
    return hk::IsExportedClass(o, "common.dll", "??_7Ship@Archetype@@6BEqObj@1@@") ||
           hk::IsExportedClass(o, "common.dll", "??_7Ship@Archetype@@6BCommReferrable@@@");
}
static bool IsShield(const void* o) { return hk::IsExportedClass(o, "common.dll", "??_7CEShield@@6B@"); }
static bool IsShieldGenerator(const void* o)
{
    return hk::IsExportedClass(o, "common.dll", "??_7CEShieldGenerator@@6B@");
}

static bool ReadSelf(const Api& a, void* ship, Self& s)
{
    if (!IsShip(ship))
        return false;
    __try
    {
        if (a.relHealth)
            s.hull = a.relHealth(ship, nullptr);
        if (a.maxHit)
            s.hullMax = a.maxHit(ship, nullptr);
        if (a.powerRatio)
            s.power = a.powerRatio(ship, nullptr);
        float range = 0;
        if (a.gunRange && a.gunRange(ship, nullptr, &range))
            s.gunRange = range;
        const u8* shipArch = a.shipArch ? a.shipArch(ship, nullptr) : nullptr;
        if (!IsShipArch(shipArch))
            shipArch = nullptr;
        if (const u8* arch = shipArch)
        {
            s.nanobotMax = *reinterpret_cast<const int*>(arch + kArchNanobots);
            s.batteryMax = *reinterpret_cast<const int*>(arch + kArchBatteries);
            if (a.archId)
                s.shipArch = a.archId(arch, nullptr);
        }
        if (a.repairRoom)
            s.nanobots = std::max(0, s.nanobotMax - a.repairRoom(ship, nullptr));
        if (a.batteryRoom)
            s.batteries = std::max(0, s.batteryMax - a.batteryRoom(ship, nullptr));
        if (a.equipFirst && a.shieldHp && a.shieldMax)
            if (void* shield = a.equipFirst(static_cast<u8*>(ship) + kEquipManager, nullptr, kEquipShield);
                IsShield(shield))
            {
                s.shieldMax = a.shieldMax(shield, nullptr);
                s.shield = s.shieldMax > 0 ? a.shieldHp(shield, nullptr) / s.shieldMax : -1;
            }
        // The fitted shield GENERATOR (st_equip.ini [ShieldGenerator]); the CEShield above is the ship's aggregate shield
        // (its archetype is the ship's built-in one, e.g. select_equip.ini cv_fighter_shield01 - read as the shield's
        // arch until 19 Oct 2026, so st_equip lookups found nothing).
        if (a.equipFirst && a.equipArch && a.archId)
            if (void* gen = a.equipFirst(static_cast<u8*>(ship) + kEquipManager, nullptr, kEquipShieldGenerator);
                IsShieldGenerator(gen))
                if (const void* arch = a.equipArch(gen, nullptr))
                    s.shieldArch = a.archId(arch, nullptr);
        if (a.equipFirst && a.equipArch && a.archId)
            if (void* tractor = a.equipFirst(static_cast<u8*>(ship) + kEquipManager, nullptr, kEquipTractor))
                if (const void* arch = a.equipArch(tractor, nullptr))
                    s.tractorArch = a.archId(arch, nullptr);
        if (shipArch)
            s.holdSize = *reinterpret_cast<const float*>(shipArch + kArchHold);
        if (a.holdRemaining)
            s.holdFree = a.holdRemaining(ship, nullptr);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool ReadSelf(Self& s)
{
    s = Self();
    const Api& a = GetApi();
    void* ship = a.common ? PlayerShip() : nullptr;
    if (!ship || !ReadSelf(a, ship, s))
        return false;
    if (s.tractorArch)
        s.tractorRange = TractorRange(a, s.tractorArch); // file data: outside the guarded read
    return true;
}

// The game's own use of a consumable with a chosen count (user, 21 Oct 2026: "one battery per press ... scale it based
// on important factors"): the HUD's repair buttons end in the client ship's IObjAffect::use_item(cargo id, count, -1)
// with the count that fills the bar (exe::UseItem, which sends IServer::SPRequestUseItem). Here the same call with our
// count: the cargo found by its archetype's class (Archetype::AClassType RepairKit 26 / ShieldBattery 27 - any mod's
// battery, not a nickname), the vtable slot verified against exe::UseItem before the call.
int UseConsumable(bool battery, int amount)
{
    const Api& a = GetApi();
    void* iobj = PlayerIObj();
    void* ship = a.common ? PlayerShip() : nullptr;
    if (amount <= 0 || !iobj || !ship || !IsShip(ship) || !a.traverserCtor || !a.traverse || !a.castCargo ||
        !a.equipId || !a.equipArch || !exe::UseItem.ok())
        return 0;
    __try
    {
        u8* affect = static_cast<u8*>(iobj) + 4; // IObjRW: IObjInspectImpl, IObjAffect (+4), IObjDestructor
        void** vtbl = *reinterpret_cast<void***>(affect);
        // vtable slot 22 = IObjAffect::use_item (flexe.h UseItem): fail closed when the slot is not that signed
        // function (another build or a mod's class) - never call an unverified slot
        if (!vtbl || !exe::UseItem.is(reinterpret_cast<uintptr_t>(vtbl[22])))
        {
            static bool logged = false;
            if (!logged)
            {
                logged = true;
                Log("ai: use_item slot (IObjAffect vtable 22 = %p) is not exe::UseItem: consumables off",
                    vtbl ? vtbl[22] : nullptr);
            }
            return 0;
        }
        alignas(8) u8 traverser[64] = {};
        a.traverserCtor(traverser, nullptr, -1); // every class
        const int wanted = battery ? 27 : 26;
        for (int guard = 0; guard < 256; ++guard)
        {
            void* e = a.traverse(static_cast<u8*>(ship) + kEquipManager, nullptr, traverser);
            if (!e)
                break;
            void* cargo = a.castCargo(e);
            const void* arch = cargo ? a.equipArch(cargo, nullptr) : nullptr;
            if (!arch)
                continue;
            // Archetype::Root::get_class_type (vtable +0x10, as the game reads it in use_item)
            typedef int(__thiscall * ClassFn)(const void*);
            const int cls = (*reinterpret_cast<ClassFn* const*>(arch))[4](arch);
            if (cls != wanted)
                continue;
            const int held = *reinterpret_cast<const int*>(static_cast<const u8*>(cargo) + 0x28); // CECargo count
            const int n = std::min(amount, held);
            if (n <= 0)
                return 0;
            typedef int(__thiscall * UseFn)(void*, u16, u32, u32);
            reinterpret_cast<UseFn>(vtbl[22])(affect, a.equipId(cargo, nullptr), static_cast<u32>(n), 0xFFFFFFFFu);
            space::InvalidateFrame(); // the hold's free space changed (Contact::lootFits; space.h per-frame memo)
            return n;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
    return 0;
}

// Personality buffers are larger than the FLSDK struct: the game's constructor and get_personality write the real
// size, whatever it is.
struct PersonalityBuf
{
    alignas(8) u8 raw[0x800];
    Personality& p() { return *reinterpret_cast<Personality*>(raw); }
};

static bool DefaultPersonality(const Api& a, PersonalityBuf& b)
{
    memset(b.raw, 0, sizeof(b.raw));
    __try
    {
        a.personalityCtor(b.raw, nullptr);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static PersonalityBuf g_current; // what Brain() submits
static bool g_currentInit = false;
static std::string g_currentLabel = "default";
static PersonalityBuf g_own; // the ship's own personality, saved by the first Brain()
static int g_ownGraph = 0;
static u32 g_brainShip = 0; // ship id the brain was put on (0 = none)
static std::string g_brainLabel;
static u32 g_forced = 0;

Personality& Current()
{
    if (!g_currentInit)
    {
        g_currentInit = DefaultPersonality(GetApi(), g_current);
        g_currentLabel = "default";
    }
    return g_current.p();
}

static bool ReadManager(const Api& a, u32 id, u8* m, State& s)
{
    const dll::BehaviorFields& f = dll::Behavior(); // offsets from Common.dll's accessors (was +0xC0 / +0xBC / +0xC8)
    if (!f.ok())
        return false;
    __try
    {
        s.op = *reinterpret_cast<int*>(m + f.op);
        s.locked = m[f.locked] != 0;
        s.priority = *reinterpret_cast<int*>(m + f.priority);
        s.graph = a.graphId(id);
        s.behavior = a.behaviorId(id);
        s.graphPlayer = a.stateGraph("PLAYER", 0);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool ReadState(State& s)
{
    const Api& a = GetApi();
    const u32 id = PlayerId();
    u8* m = a.ok ? Manager(a) : nullptr;
    if (!id || !m)
        return false;
    if (!ReadManager(a, id, m, s))
        return false;
    s.brain = BrainActive();
    s.pilot = g_brainLabel;
    s.forced = g_forced;
    return true;
}

// ------------------------------------------------------------------ maneuvers
bool Maneuver(int op, bool flag)
{
    void* self = exe::PlayerManeuvers.ok() ? Mem<void*>(exe::PlayerManeuvers) : nullptr;
    if (!self || !PlayerIObj())
        return false;
    __try
    {
        return hk::Call<bool(__thiscall*)(void*, int, bool)>(exe::ManeuverExecute, self, op, flag);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

void FreeFlight() { Maneuver(OpFreeFlight, false); }

static float HealthOf(void* iobj)
{
    static auto relHealth = reinterpret_cast<float(__fastcall*)(void*, void*)>(
        GetProcAddress(GetModuleHandleA("common.dll"), "?get_relative_health@CSimple@@QBEMXZ"));
    if (!relHealth || !iobj)
        return -1;
    __try
    {
        const auto cobject = dll::ObjCobject.get<dll::ObjCobjectFn>(iobj);
        void* ship = cobject ? cobject(iobj) : nullptr;
        return ship ? relHealth(ship, nullptr) : -1;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return -1;
    }
}

float PlayerHealth() { return HealthOf(PlayerIObj()); }

static bool ReadDockPort(const void*& port, int& index)
{
    const Api& a = GetApi();
    u8* m = a.ok ? Manager(a) : nullptr;
    if (!m || !a.mDockingPort)
        return false;
    __try
    {
        port = nullptr;
        index = -1;
        return a.mDockingPort(m, &port, &index) && port != nullptr;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool DockInProgress()
{
    const void* port;
    int index;
    return ReadDockPort(port, index);
}

bool DockPort(u32& objectId, int& index)
{
    const void* port;
    if (!ReadDockPort(port, index))
        return false;
    const auto getId = dll::ObjGetId.get<dll::ObjGetIdFn>(port);
    objectId = getId ? getId(port) : 0;
    return true;
}

bool MissionHolds()
{
    State s;
    if (!ReadState(s))
        return false;
    // A mission's Idle (Act_PobjIdle) is a full stop that leaves the player in free flight, free to fly (FREELANCER_RE
    // "Server directives"): it reads op freeflight / idle at a priority, unlocked. Not the mission flying the ship (was:
    // M13's launch parks the player under Nomad fire for ~26 s; the autopilot waited and the ship died, 22 Oct 2026).
    const bool parked = s.op == OpIdle || s.op == OpFreeFlight || s.op == OpNone;
    return s.locked || (s.priority > 0 && !parked);
}

// ------------------------------------------------------------------ directives
struct OpCtor
{
    int op;
    const char* ctor;
};
static const OpCtor kCtors[] = {
    {OpGoto, "??0DirectiveGotoOp@AI@pub@@QAE@XZ"},
    {OpFollow, "??0DirectiveFollowOp@AI@pub@@QAE@XZ"},
    {OpDock, "??0DirectiveDockOp@AI@pub@@QAE@XZ"},
    {OpIdle, "??0DirectiveIdleOp@AI@pub@@QAE@XZ"},
    {OpTrail, "??0DirectiveTrailOp@AI@pub@@QAE@XZ"},
    {OpStrafe, "??0DirectiveStrafeOp@AI@pub@@QAE@XZ"},
    {OpFace, "??0DirectiveFaceOp@AI@pub@@QAE@XZ"},
    {OpEvade, "??0DirectiveEvadeOp@AI@pub@@QAE@XZ"},
    {OpDrasticEvade, "??0DirectiveDrasticEvadeOp@AI@pub@@QAE@XZ"},
    {OpDelay, "??0DirectiveDelayOp@AI@pub@@QAE@XZ"},
    {OpFormation, "??0DirectiveFormationOp@AI@pub@@QAE@XZ"},
};
static const int kCancel = -100; // DirectiveCancelOp (not an OpType)

// Behaviour manager: one behaviour object per op at dll::Behavior().behaviours + op * 4 (21, made by Common.dll's
// behaviour factories). Evade behaviour: the threat watcher at +0x2A0 (0 = no object; part of the host guard's
// verified signature, so read only once the guard is on).
static const uintptr_t kEvadeThreat = 0x2A0;
static bool (*g_evadeGuard)(void** vtable) = nullptr;
void SetEvadeGuard(bool (*guard)(void** vtable)) { g_evadeGuard = guard; }

static int SubmitRaw(const Api& a, u32 ship, u8* op)
{
    __try
    {
        return a.submitDirective(ship, op);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return -101;
    }
}

static int ClientDirective(const Api& a, u8* m, int op, const u8* buf)
{
    // IDirectiveInfo as the maneuver dispatcher builds it: {1, 0, op}.
    const int info[3] = {1, 0, op};
    __try
    {
        return a.mSetDirective(m, nullptr, info, buf) ? 1 : 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return -101;
    }
}

static int SubmitImpl(const Directive& d, bool client, u32 shipId = 0);
int Submit(const Directive& d) { return SubmitImpl(d, d.client); }
int SubmitFor(u32 ship, const Directive& d) { return SubmitImpl(d, false, ship); }

static int SubmitImpl(const Directive& d, bool client, u32 shipId)
{
    const Api& a = GetApi();
    const u32 ship = shipId ? shipId : PlayerId();
    if (!a.ok || !ship)
        return -100;
    const char* ctorName = d.op == kCancel ? "??0DirectiveCancelOp@AI@pub@@QAE@XZ" : nullptr;
    for (const OpCtor& c : kCtors)
        if (c.op == d.op)
            ctorName = c.ctor;
    auto ctor =
        ctorName ? reinterpret_cast<void(__fastcall*)(void*, void*)>(GetProcAddress(a.common, ctorName)) : nullptr;
    // Behaviours without a directive op (Buzz, Flee, Loot, Cruise, Large Ship) run only client side: their set-op
    // ignores the op (Buzz, Flee: they take the ship's target / threat) or reads the object id at +0xC (Loot).
    const bool bare = !ctor && client && d.op >= OpBuzz && d.op <= OpDelay;
    if (!ctor && !bare)
        return -102;
    alignas(8) u8 op[0x400] = {};
    if (ctor)
        ctor(op, nullptr);
    else
        *reinterpret_cast<int*>(op + 4) = d.op;
    *reinterpret_cast<int*>(op + 8) = d.fire ? 1 : 0; // BaseOp::fireWeapons
    auto f = [&](int off) -> float& { return *reinterpret_cast<float*>(op + off); };
    auto u = [&](int off) -> u32& { return *reinterpret_cast<u32*>(op + off); };
    switch (d.op)
    {
    case OpGoto: // gotoType +0xC (0 ship, 1 point), pos +0x10, target +0x1C, range +0x50, thrust +0x54, flags +0x58..
        u(0xC) = d.hasPos ? 1 : 0;
        if (d.hasPos)
            memcpy(op + 0x10, d.pos, 12);
        u(0x1C) = d.hasPos ? 0 : d.target;
        f(0x50) = d.range;
        f(0x54) = d.thrust;
        op[0x58] = 1;
        op[0x59] = 1;
        op[0x5A] = d.cruise ? 1 : 0;
        op[0x5B] = d.cruise ? 0 : 1;
        break;
    case OpFollow: // followSpaceObj +0xC, maxDistance +0x10, offset +0x14
        u(0xC) = d.target;
        f(0x10) = d.range;
        memcpy(op + 0x14, d.offset, 12);
        break;
    case OpDock:   // DockSpaceObj +0xC (other fields: the constructor's defaults)
    case OpEvade:  // threat +0xC: the behaviour resolves it to the object it evades (Common.dll 0x62C8630)
    case OpStrafe: // target +0xC (as the maneuver dispatcher fills it)
    case OpLoot:   // loot object +0xC (Common.dll 0x62CEDD0); Buzz / Flee ignore the op
    case OpBuzz:
    case OpFlee: u(0xC) = d.target; break;
    case OpDrasticEvade: // mode +0xC (0 slide, 1 reverse; else it fails at once), slide x/y +0x10, seconds +0x1C
        u(0xC) = d.mode == 1 ? 1 : 0;
        memcpy(op + 0x10, d.offset, 12);
        f(0x1C) = d.time;
        break;
    case OpTrail: // target +0xC, distance +0x10 (the dispatcher copies the player's trail distances to +0x10/+0x14)
        u(0xC) = d.target;
        f(0x10) = d.range;
        break;
    case OpFace: // kind +0xC (1 object, 2 point), target +0x10, point +0x14, flag +0x44 (as the dispatcher fills it)
        u(0xC) = d.hasPos ? 2 : 1;
        u(0x10) = d.hasPos ? 0 : d.target;
        if (d.hasPos)
            memcpy(op + 0x14, d.pos, 12);
        op[0x44] = 1;
        break;
    case OpDelay: // DelayTime +0xC
        f(0xC) = d.time;
        break;
    default: break;
    }
    if (client)
    {
        u8* m = Manager(a);
        if (!m || !a.mSetDirective)
            return -100;
        if (d.op == OpEvade)
        {
            // The game skips the Evade threat check for the player ship and crashes once the threat is gone
            // (Common.dll+0x84509): only with the host's guard on the Evade behaviour (its object in the manager).
            const u32 behaviours = dll::Behavior().behaviours;
            if (!behaviours)
                return -104;
            u8* evade = Mem<u8*>(reinterpret_cast<uintptr_t>(m) + behaviours + OpEvade * 4);
            if (!evade || !g_evadeGuard || !g_evadeGuard(*reinterpret_cast<void***>(evade)))
                return -104;
            const int r = ClientDirective(a, m, OpEvade, op);
            if (r == 1 && !Mem<uintptr_t>(reinterpret_cast<uintptr_t>(evade) + kEvadeThreat))
            {
                ClientDirective(a, m, OpFreeFlight, nullptr); // the guard would end it next frame anyway
                return -105;
            }
            return r;
        }
        return ClientDirective(a, m, d.op == kCancel ? OpFreeFlight : d.op, op);
    }
    return SubmitRaw(a, ship, op);
}

// ------------------------------------------------------------------ personality fields (ini names)
enum FieldKind : u8
{
    FFloat,
    FInt,
    FBool,
    FWeights,   // "name, weight" into a named slot of a float array
    FToughness, // EASIEST..HARDEST
    FFireStyle, // MULTIPLE / SINGLE
    FFieldTargeting,
    FLoot,      // LT_* | LT_* flags
    FExitMode,  // formation_exit_mode names
    FAttack,    // attack_preference = TYPE, distance, FLAG | FLAG (appends)
    FSubtarget, // attack_subtarget_order = NAME (appends)
};
struct Field
{
    const char* name;
    FieldKind kind;
    size_t off;
    const char* const* slots; // FWeights: slot names in array order
};

#define PF(block, f) offsetof(Personality, block.f)
static const char* kDir[] = {"left", "right", "up", "down", nullptr};
static const char* kDodgeStyle[] = {"waggle", "waggle_random", "slide", "corkscrew", nullptr};
static const char* kBreakStyle[] = {"sideways", "outrun", "reverse", nullptr};
static const char* kHeadStyle[] = {"straight_to", "slide", "waggle", nullptr};
static const char* kPassStyle[] = {"straight_by", "break_away", "engine_kill", nullptr};
#define FLT(block, f) {#f, FFloat, PF(block, f), nullptr}
#define BOOL(block, f) {#f, FBool, PF(block, f), nullptr}
static const Field kFields[] = {
    FLT(EvadeDodge, evade_activate_range),
    {"evade_dodge_style_weight", FWeights, PF(EvadeDodge, evade_dodge_style_weight), kDodgeStyle},
    FLT(EvadeDodge, evade_dodge_cone_angle),
    FLT(EvadeDodge, evade_dodge_cone_angle_variance_percent),
    FLT(EvadeDodge, evade_dodge_waggle_axis_cone_angle),
    FLT(EvadeDodge, evade_dodge_roll_angle),
    FLT(EvadeDodge, evade_dodge_interval_time),
    FLT(EvadeDodge, evade_dodge_interval_time_variance_percent),
    FLT(EvadeDodge, evade_dodge_distance),
    FLT(EvadeDodge, evade_dodge_time),
    FLT(EvadeDodge, evade_dodge_slide_throttle),
    FLT(EvadeDodge, evade_dodge_turn_throttle),
    FLT(EvadeDodge, evade_dodge_corkscrew_turn_throttle),
    FLT(EvadeDodge, evade_dodge_corkscrew_roll_throttle),
    BOOL(EvadeDodge, evade_dodge_corkscrew_roll_flip_direction),
    {"evade_dodge_direction_weight", FWeights, PF(EvadeDodge, evade_dodge_direction_weight), kDir},
    FLT(EvadeBreak, evade_break_time),
    FLT(EvadeBreak, evade_break_interval_time),
    FLT(EvadeBreak, evade_break_afterburner_delay),
    FLT(EvadeBreak, evade_break_afterburner_delay_variance_percent),
    {"evade_break_direction_weight", FWeights, PF(EvadeBreak, evade_break_direction_weight), kDir},
    FLT(EvadeBreak, evade_break_roll_throttle),
    FLT(EvadeBreak, evade_break_turn_throttle),
    {"evade_break_style_weight", FWeights, PF(EvadeBreak, evade_break_style_weight), kBreakStyle},
    FLT(EvadeBreak, evade_break_attempt_reverse_time),
    FLT(EvadeBreak, evade_break_reverse_distance),
    FLT(BuzzHeadToward, buzz_max_time_to_head_away),
    {"buzz_head_toward_style_weight", FWeights, PF(BuzzHeadToward, buzz_head_toward_style_weight), kHeadStyle},
    FLT(BuzzHeadToward, buzz_min_distance_to_head_toward),
    FLT(BuzzHeadToward, buzz_min_distance_to_head_toward_variance_percent),
    FLT(BuzzHeadToward, buzz_head_toward_engine_throttle),
    FLT(BuzzHeadToward, buzz_head_toward_turn_throttle),
    FLT(BuzzHeadToward, buzz_head_toward_roll_throttle),
    BOOL(BuzzHeadToward, buzz_head_toward_roll_flip_direction),
    {"buzz_dodge_direction_weight", FWeights, PF(BuzzHeadToward, buzz_dodge_direction_weight), kDir},
    FLT(BuzzHeadToward, buzz_dodge_turn_throttle),
    FLT(BuzzHeadToward, buzz_dodge_cone_angle),
    FLT(BuzzHeadToward, buzz_dodge_cone_angle_variance_percent),
    FLT(BuzzHeadToward, buzz_dodge_waggle_axis_cone_angle),
    FLT(BuzzHeadToward, buzz_dodge_roll_angle),
    FLT(BuzzHeadToward, buzz_dodge_interval_time),
    FLT(BuzzHeadToward, buzz_dodge_interval_time_variance_percent),
    FLT(BuzzHeadToward, buzz_slide_throttle),
    FLT(BuzzHeadToward, buzz_slide_interval_time),
    FLT(BuzzHeadToward, buzz_slide_interval_time_variance_percent),
    {"buzz_pass_by_style_weight", FWeights, PF(BuzzPassBy, buzz_pass_by_style_weight), kPassStyle},
    FLT(BuzzPassBy, buzz_distance_to_pass_by),
    FLT(BuzzPassBy, buzz_pass_by_time),
    BOOL(BuzzPassBy, buzz_drop_bomb_on_pass_by),
    {"buzz_break_direction_weight", FWeights, PF(BuzzPassBy, buzz_break_direction_weight), kDir},
    FLT(BuzzPassBy, buzz_break_direction_cone_angle),
    FLT(BuzzPassBy, buzz_break_turn_throttle),
    FLT(BuzzPassBy, buzz_pass_by_roll_throttle),
    FLT(Trail, trail_lock_cone_angle),
    FLT(Trail, trail_break_time),
    FLT(Trail, trail_min_no_lock_time),
    FLT(Trail, trail_break_roll_throttle),
    BOOL(Trail, trail_break_afterburner),
    FLT(Trail, trail_max_turn_throttle),
    FLT(Trail, trail_distance),
    FLT(Strafe, strafe_run_away_distance),
    FLT(Strafe, strafe_attack_throttle),
    FLT(Strafe, strafe_turn_throttle),
    FLT(EngineKill, engine_kill_search_time),
    FLT(EngineKill, engine_kill_face_time),
    FLT(EngineKill, engine_kill_use_afterburner),
    FLT(EngineKill, engine_kill_afterburner_time),
    FLT(EngineKill, engine_kill_max_target_distance),
    FLT(Repair, use_shield_repair_pre_delay),
    FLT(Repair, use_shield_repair_at_damage_percent),
    FLT(Repair, use_shield_repair_post_delay),
    FLT(Repair, use_hull_repair_pre_delay),
    FLT(Repair, use_hull_repair_at_damage_percent),
    FLT(Repair, use_hull_repair_post_delay),
    {"fire_style", FFireStyle, PF(Gun, fire_style), nullptr},
    FLT(Gun, gun_fire_interval_time),
    FLT(Gun, gun_fire_interval_variance_percent),
    FLT(Gun, gun_fire_burst_interval_time),
    FLT(Gun, gun_fire_burst_interval_variance_percent),
    FLT(Gun, gun_fire_no_burst_interval_time),
    FLT(Gun, gun_fire_accuracy_cone_angle),
    FLT(Gun, gun_fire_accuracy_power),
    FLT(Gun, gun_fire_accuracy_power_npc),
    FLT(Gun, gun_range_threshold),
    FLT(Gun, gun_range_threshold_variance_percent),
    FLT(Gun, gun_target_point_switch_time),
    FLT(Gun, auto_turret_interval_time),
    FLT(Gun, auto_turret_burst_interval_time),
    FLT(Gun, auto_turret_no_burst_interval_time),
    FLT(Gun, auto_turret_burst_interval_variance_percent),
    FLT(Mine, mine_launch_interval),
    FLT(Mine, mine_launch_cone_angle),
    FLT(Mine, mine_launch_range),
    FLT(Missile, missile_launch_range),
    BOOL(Missile, missile_launch_allow_out_of_range),
    FLT(Missile, missile_launch_interval_time),
    FLT(Missile, missile_launch_interval_variance_percent),
    FLT(Missile, missile_launch_cone_angle),
    FLT(Missile, anti_cruise_missile_min_distance),
    FLT(Missile, anti_cruise_missile_max_distance),
    FLT(Missile, anti_cruise_missile_pre_fire_delay),
    FLT(Missile, anti_cruise_missile_interval_time),
    FLT(DamageReaction, evade_break_damage_trigger_percent),
    FLT(DamageReaction, evade_dodge_more_damage_trigger_percent),
    FLT(DamageReaction, drop_mines_damage_trigger_percent),
    FLT(DamageReaction, drop_mines_damage_trigger_time),
    FLT(DamageReaction, engine_kill_face_damage_trigger_percent),
    FLT(DamageReaction, engine_kill_face_damage_trigger_time),
    FLT(DamageReaction, roll_damage_trigger_percent),
    FLT(DamageReaction, roll_damage_trigger_time),
    FLT(DamageReaction, afterburner_damage_trigger_percent),
    FLT(DamageReaction, afterburner_damage_trigger_time),
    FLT(DamageReaction, brake_reverse_damage_trigger_percent),
    FLT(DamageReaction, fire_missiles_damage_trigger_percent),
    FLT(DamageReaction, fire_missiles_damage_trigger_time),
    FLT(DamageReaction, fire_guns_damage_trigger_percent),
    FLT(DamageReaction, fire_guns_damage_trigger_time),
    FLT(MissileReaction, evade_break_missile_reaction_time),
    FLT(MissileReaction, evade_slide_missile_reaction_time),
    FLT(MissileReaction, evade_afterburn_missile_reaction_time),
    FLT(MissileReaction, evade_missile_distance),
    FLT(Countermeasure, countermeasure_active_time),
    FLT(Countermeasure, countermeasure_unactive_time),
    FLT(Formation, force_attack_formation_active_time),
    FLT(Formation, force_attack_formation_unactive_time),
    FLT(Formation, break_formation_damage_trigger_percent),
    FLT(Formation, break_formation_damage_trigger_time),
    FLT(Formation, break_apart_formation_damage_trigger_percent),
    FLT(Formation, break_apart_formation_damage_trigger_time),
    FLT(Formation, break_formation_missile_reaction_time),
    FLT(Formation, break_apart_formation_missile_reaction_time),
    BOOL(Formation, break_apart_formation_on_buzz_head_toward),
    FLT(Formation, break_formation_on_buzz_head_toward_time),
    BOOL(Formation, regroup_formation_on_buzz_head_toward),
    BOOL(Formation, break_apart_formation_on_buzz_pass_by),
    FLT(Formation, break_formation_on_buzz_pass_by_time),
    BOOL(Formation, regroup_formation_on_buzz_pass_by),
    BOOL(Formation, break_apart_formation_on_evade_dodge),
    FLT(Formation, break_formation_on_evade_dodge_time),
    BOOL(Formation, regroup_formation_on_evade_dodge),
    BOOL(Formation, break_apart_formation_on_evade_break),
    FLT(Formation, break_formation_on_evade_break_time),
    BOOL(Formation, regroup_formation_on_evade_break),
    {"formation_exit_mode", FExitMode, PF(Formation, formation_exit_mode), nullptr},
    FLT(Formation, formation_exit_top_turn_break_away_throttle),
    FLT(Formation, formation_exit_roll_outrun_throttle),
    FLT(Formation, formation_exit_max_time),
    BOOL(Formation, leader_makes_me_tougher),
    {"scene_toughness_threshold", FToughness, PF(Job, scene_toughness_threshold), nullptr},
    {"target_toughness_preference", FToughness, PF(Job, target_toughness_preference), nullptr},
    FLT(Job, combat_drift_distance),
    {"attack_preference", FAttack, PF(Job, attack_order), nullptr},
    {"attack_subtarget_order", FSubtarget, PF(Job, attack_subtarget_order), nullptr},
    BOOL(Job, wait_for_leader_target),
    FLT(Job, maximum_leader_target_distance),
    {"field_targeting", FFieldTargeting, PF(Job, field_targeting), nullptr},
    {"loot_preference", FLoot, PF(Job, loot_preference), nullptr},
    {"loot_flee_threshold", FToughness, PF(Job, loot_flee_threshold), nullptr},
    {"flee_scene_threat_style", FToughness, PF(Job, flee_scene_threat_style), nullptr},
    FLT(Job, flee_when_hull_damaged_percent),
    BOOL(Job, flee_when_leader_flees_style),
    BOOL(Job, flee_no_weapons_style),
    BOOL(Job, allow_player_targeting),
    BOOL(Job, force_attack_formation),
};
#undef FLT
#undef BOOL
#undef PF

// This file's text may still carry line ends: the shared Trim with the " \t\r\n" set (core\str.h).
static std::string Trim(std::string s) { return str::Trim(s, " \t\r\n"); }
using str::Lower;
static std::vector<std::string> SplitList(const std::string& v, char sep = ',')
{
    std::vector<std::string> out;
    std::stringstream ss(v);
    std::string t;
    while (std::getline(ss, t, sep))
        out.push_back(Trim(t));
    return out;
}
static int Index(const char* const* names, const std::string& v)
{
    for (int i = 0; names[i]; ++i)
        if (_stricmp(names[i], v.c_str()) == 0)
            return i;
    return -1;
}
static const Field* FindField(const std::string& name)
{
    for (const Field& f : kFields)
        if (_stricmp(f.name, name.c_str()) == 0)
            return &f;
    return nullptr;
}

// Applies one ini value. `attackFresh`: the next attack_preference / attack_subtarget_order line starts a new list.
struct ApplyState
{
    int attack = -1, subtarget = -1;
};
static bool Apply(Personality& p, const Field& f, const std::string& value, ApplyState& st, std::string* err)
{
    u8* base = reinterpret_cast<u8*>(&p) + f.off;
    const std::vector<std::string> v = SplitList(value);
    auto fail = [&](const char* why)
    {
        if (err)
            *err = std::string(f.name) + ": " + why + " (" + value + ")";
        return false;
    };
    if (v.empty() || v[0].empty())
        return fail("no value");
    static const char* kTough[] = {"easiest", "easy", "equal", "hard", "hardest", nullptr};
    switch (f.kind)
    {
    case FFloat:
    {
        float x = static_cast<float>(atof(v[0].c_str()));
        // The [Pilot] files give angles in degrees; the game's Personality holds radians (a live police NPC:
        // trail_lock_cone_angle 0.5236 for the ini's 30). Unconverted, the AI got 30-radian cones and spawned NPCs
        // never engaged (18 Oct 2026).
        const std::string n(f.name);
        if (n.size() > 6 && n.compare(n.size() - 6, 6, "_angle") == 0)
            x *= 3.14159265f / 180.0f;
        *reinterpret_cast<float*>(base) = x;
        return true;
    }
    case FInt: *reinterpret_cast<int*>(base) = atoi(v[0].c_str()); return true;
    case FBool: *reinterpret_cast<bool*>(base) = _stricmp(v[0].c_str(), "true") == 0 || v[0] == "1"; return true;
    case FWeights:
    {
        const int i = Index(f.slots, v[0]);
        if (i < 0 || v.size() < 2)
            return fail("unknown slot");
        reinterpret_cast<float*>(base)[i] = static_cast<float>(atof(v[1].c_str()));
        return true;
    }
    case FToughness:
    {
        const int i = Index(kTough, v[0]);
        if (i < 0)
            return fail("expected EASIEST..HARDEST");
        *reinterpret_cast<int*>(base) = i;
        return true;
    }
    case FFireStyle: *reinterpret_cast<int*>(base) = _stricmp(v[0].c_str(), "single") == 0 ? 1 : 0; return true;
    case FFieldTargeting:
    {
        static const char* k[] = {"never", "low_density", "high_density", "always", nullptr};
        const int i = Index(k, v[0]);
        if (i < 0)
            return fail("expected NEVER, LOW_DENSITY, HIGH_DENSITY, ALWAYS");
        *reinterpret_cast<int*>(base) = i;
        return true;
    }
    case FLoot:
    {
        int flags = 0;
        for (const std::string& t : SplitList(v[0], '|'))
        {
            const std::string l = Lower(t);
            flags |= l == "lt_commodities" ? 1
                     : l == "lt_equipment" ? 2
                     : l == "lt_potions"   ? 4
                     : l == "lt_all"       ? 7
                                           : 0;
        }
        *reinterpret_cast<int*>(base) = flags;
        return true;
    }
    case FExitMode:
    {
        static const char* k[] = {"break_away_from_center",
                                  "break_away_from_center_afterburner",
                                  "brake_reverse",
                                  "outrun",
                                  "unused",
                                  "nothing",
                                  nullptr};
        const int i = Index(k, v[0]);
        if (i < 0)
            return fail("unknown mode");
        *reinterpret_cast<int*>(base) = i;
        return true;
    }
    case FAttack:
    {
        static const char* k[] = {"fighter", "freighter", "transport", "gunboat",          "cruiser",
                                  "capital", "tradelane", "jumpgate",  "weapons_platform", "destroyable_depot",
                                  "solar",   "anything",  nullptr};
        const int type = Index(k, v[0]);
        if (type < 0 || v.size() < 3)
            return fail("expected TYPE, distance, FLAGS");
        int flag = 0;
        for (const std::string& t : SplitList(v[2], '|'))
        {
            const std::string l = Lower(t);
            flag |= l == "guns" ? 1 : l == "guided" ? 2 : l == "unguided" ? 4 : l == "torpedo" ? 8 : 0;
        }
        auto* list = reinterpret_cast<JobAttack*>(base);
        if (st.attack < 0)
            st.attack = 0;
        if (st.attack >= 12)
            return fail("too many");
        list[st.attack] = {type, static_cast<float>(atof(v[1].c_str())), flag};
        list[++st.attack] = {12, 0, 0}; // end of list
        return true;
    }
    case FSubtarget:
    {
        static const char* k[] = {"guns", "turrets", "launchers", "towers", "engines", "hull", "anything", nullptr};
        const int i = Index(k, v[0]);
        if (i < 0)
            return fail("unknown subtarget");
        int* list = reinterpret_cast<int*>(base);
        if (st.subtarget < 0)
            st.subtarget = 0;
        if (st.subtarget >= 7)
            return fail("too many");
        list[st.subtarget] = i;
        list[++st.subtarget] = 7; // end of list
        return true;
    }
    }
    return fail("unsupported");
}

bool SetField(Personality& p, const std::string& name, const std::string& value, std::string* err)
{
    const Field* f = FindField(name);
    if (!f)
    {
        if (err)
            *err = "unknown field " + name;
        return false;
    }
    ApplyState st;
    return Apply(p, *f, value, st, err);
}

std::string GetField(const Personality& p, const std::string& name)
{
    const Field* f = FindField(name);
    if (!f)
        return "?";
    const u8* base = reinterpret_cast<const u8*>(&p) + f->off;
    char buf[160];
    switch (f->kind)
    {
    case FFloat: sprintf_s(buf, "%g", *reinterpret_cast<const float*>(base)); return buf;
    case FBool: return *reinterpret_cast<const bool*>(base) ? "true" : "false";
    case FWeights:
    {
        std::string s;
        for (int i = 0; f->slots[i]; ++i)
        {
            sprintf_s(buf, "%s%s=%g", i ? " " : "", f->slots[i], reinterpret_cast<const float*>(base)[i]);
            s += buf;
        }
        return s;
    }
    case FAttack:
    {
        std::string s;
        const auto* list = reinterpret_cast<const JobAttack*>(base);
        for (int i = 0; i < 12 && list[i].type != 12; ++i)
        {
            sprintf_s(buf, "%s%d/%g/%d", i ? " " : "", list[i].type, list[i].distance, list[i].flag);
            s += buf;
        }
        return s;
    }
    case FSubtarget:
    {
        std::string s;
        const int* list = reinterpret_cast<const int*>(base);
        for (int i = 0; i < 7 && list[i] != 7; ++i)
            s += (i ? " " : "") + std::to_string(list[i]);
        return s;
    }
    default: return std::to_string(*reinterpret_cast<const int*>(base));
    }
}

std::vector<std::string> FieldNames()
{
    std::vector<std::string> out;
    for (const Field& f : kFields)
        out.push_back(f.name);
    return out;
}

// ------------------------------------------------------------------ the game's pilot definitions
struct Section
{
    std::string type, nick;
    std::vector<std::pair<std::string, std::string>> kv;
};
static std::vector<Section> g_pilotIni;
static bool g_pilotIniLoaded = false;

static void LoadPilotFiles()
{
    if (g_pilotIniLoaded)
        return;
    g_pilotIniLoaded = true;
    const std::string dir = DllDir() + "..\\DATA\\MISSIONS\\";
    for (const char* file : {"pilots_population.ini", "pilots_story.ini"})
    {
        std::ifstream in(dir + file);
        std::string line;
        Section* cur = nullptr;
        while (std::getline(in, line))
        {
            line = Trim(line.substr(0, line.find(';')));
            if (line.empty())
                continue;
            if (line[0] == '[')
            {
                g_pilotIni.push_back({Lower(line.substr(1, line.find(']') - 1)), "", {}});
                cur = &g_pilotIni.back();
                continue;
            }
            const size_t eq = line.find('=');
            if (!cur || eq == std::string::npos)
                continue;
            const std::string k = Lower(Trim(line.substr(0, eq))), v = Trim(line.substr(eq + 1));
            if (k == "nickname")
                cur->nick = Lower(v);
            else
                cur->kv.push_back({k, v});
        }
    }
    Log("ai: %u sections from the pilot files", static_cast<unsigned>(g_pilotIni.size()));
}

std::vector<std::string> Pilots()
{
    LoadPilotFiles();
    std::vector<std::string> out;
    for (const Section& s : g_pilotIni)
        if (s.type == "pilot")
            out.push_back(s.nick);
    return out;
}

static const Section* FindSection(const std::string& type, const std::string& nick)
{
    for (const Section& s : g_pilotIni)
        if (s.nick == nick && (type.empty() ? s.type != "pilot" : s.type == type))
            return &s;
    return nullptr;
}

static bool ApplySection(Personality& p, const Section& s, int depth, std::string* err)
{
    if (depth > 8)
        return false;
    ApplyState st;
    for (const auto& kv : s.kv)
    {
        if (kv.first == "inherit")
        {
            const Section* parent = FindSection("pilot", Lower(kv.second));
            if (!parent || !ApplySection(p, *parent, depth + 1, err))
                return false;
        }
        else if (kv.first.size() > 3 && kv.first.compare(kv.first.size() - 3, 3, "_id") == 0)
        {
            const Section* block = FindSection("", Lower(kv.second));
            if (!block)
            {
                if (err)
                    *err = "block " + kv.second + " not found";
                return false;
            }
            if (!ApplySection(p, *block, depth + 1, err))
                return false;
        }
        else if (const Field* f = FindField(kv.first))
            Apply(p, *f, kv.second, st, nullptr); // an unparsable value keeps the default (as the game's loader)
    }
    return true;
}

bool LoadPilot(const std::string& pilot, Personality& out, std::string* err)
{
    LoadPilotFiles();
    const Section* s = FindSection("pilot", Lower(pilot));
    if (!s)
    {
        if (err)
            *err = "no [Pilot] " + pilot;
        return false;
    }
    PersonalityBuf b;
    if (!DefaultPersonality(GetApi(), b))
        return false;
    if (!ApplySection(b.p(), *s, 0, err))
        return false;
    memcpy(&out, b.raw, sizeof(Personality));
    return true;
}

bool ApplyBlock(const std::string& block, Personality& inout, std::string* err)
{
    LoadPilotFiles();
    const Section* s = FindSection("", Lower(block));
    if (!s)
    {
        if (err)
            *err = "no block " + block;
        return false;
    }
    PersonalityBuf b;
    if (!DefaultPersonality(GetApi(), b))
        return false;
    memcpy(b.raw, &inout, sizeof(Personality));
    if (!ApplySection(b.p(), *s, 0, err))
        return false;
    memcpy(&inout, b.raw, sizeof(Personality));
    return true;
}

bool LoadPilotOnto(const std::string& pilot, Personality& inout, std::string* err)
{
    LoadPilotFiles();
    const Section* s = FindSection("pilot", Lower(pilot));
    if (!s)
    {
        if (err)
            *err = "no [Pilot] " + pilot;
        return false;
    }
    PersonalityBuf b;
    if (!DefaultPersonality(GetApi(), b))
        return false;
    memcpy(b.raw, &inout, sizeof(Personality));
    if (!ApplySection(b.p(), *s, 0, err))
        return false;
    memcpy(&inout, b.raw, sizeof(Personality));
    return true;
}

bool CopyNpcPersonality(u32 npc, Personality& out)
{
    const Api& a = GetApi();
    if (!a.getPersonality)
        return false;
    PersonalityBuf b;
    if (!DefaultPersonality(a, b))
        return false;
    __try
    {
        if (!a.getPersonality(npc, b.raw))
            return false;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
    memcpy(&out, b.raw, sizeof(Personality));
    return true;
}

// ------------------------------------------------------------------ brain
static int SubmitPersonality(const Api& a, u32 ship, int graph, const PersonalityBuf& p)
{
    alignas(8) u8 params[0x1000] = {};
    __try
    {
        a.setPersonalityCtor(params, nullptr);
        // SetPersonalityParams: BaseOp (0xC), stateGraph +0xC, contentCallback +0x10, directiveCallback +0x14,
        // bool stateId +0x18, Personality +0x1C.
        *reinterpret_cast<int*>(params + 0xC) = graph;
        params[0x18] = 1;
        memcpy(params + 0x1C, p.raw, sizeof(Personality));
        return a.submitState(ship, params);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return -101;
    }
}

bool BrainActive() { return g_brainShip && g_brainShip == PlayerId(); }

static int GraphId(const Api& a, const char* name);

int GiveBrain(u32 ship, const Personality& p, const char* graph)
{
    const Api& a = GetApi();
    if (!a.ok || !ship)
        return -100;
    PersonalityBuf b;
    if (!DefaultPersonality(a, b))
        return -102;
    memcpy(b.raw, &p, sizeof(Personality));
    return SubmitPersonality(a, ship, GraphId(a, graph), b);
}

static int GraphId(const Api& a, const char* name)
{
    __try
    {
        return a.stateGraph(name, 0);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
}

// Save the ship's own graph and personality (once per brain session).
static void SaveOwn(const Api& a, u32 ship)
{
    DefaultPersonality(a, g_own);
    __try
    {
        g_ownGraph = a.graphId(ship);
        if (a.getPersonality)
            a.getPersonality(ship, g_own.raw);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

int Brain(const std::string& graph, const std::string& pilotLabel)
{
    const Api& a = GetApi();
    const u32 ship = PlayerId();
    if (!a.ok || !ship)
        return -100;
    const int id = GraphId(a, graph.c_str());
    if (id <= 0)
        return -103;
    if (!BrainActive())
        SaveOwn(a, ship);
    Current();
    const int rc = SubmitPersonality(a, ship, id, g_current);
    if (rc >= 0)
    {
        g_brainShip = ship;
        g_brainLabel = (pilotLabel.empty() ? g_currentLabel : pilotLabel) + " / " + graph;
        Log("ai: brain on (%s), graph %d, rc %d", g_brainLabel.c_str(), id, rc);
    }
    return rc;
}

// Client side: the personality and state graph straight on the player's own behaviour manager (experiment; the
// server's SubmitState does not reach the client-created manager of the player ship).
static int ClientGraph(const Api& a, u8* m, int graph, bool flag, const void* personality)
{
    __try
    {
        if (personality)
            a.mSetPersonality(m, nullptr, personality);
        a.mSetGraph(m, nullptr, graph, flag);
        a.mRefreshGraph(m, nullptr);
        return 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return -101;
    }
}

static int ClientBrain(const std::string& graph, bool flag)
{
    const Api& a = GetApi();
    u8* m = a.ok ? Manager(a) : nullptr;
    if (!m || !a.mSetPersonality || !a.mSetGraph || !a.mRefreshGraph)
        return -100;
    const int id = GraphId(a, graph.c_str());
    if (id <= 0)
        return -103;
    Current();
    return ClientGraph(a, m, id, flag, g_current.raw);
}

static float ClientTrailDistance()
{
    const Api& a = GetApi();
    u8* m = a.ok ? Manager(a) : nullptr;
    if (!m || !a.mGetPersonality)
        return -1;
    PersonalityBuf b;
    if (!DefaultPersonality(a, b))
        return -1;
    __try
    {
        a.mGetPersonality(m, nullptr, b.raw);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return -2;
    }
    return b.p().Trail.trail_distance;
}

// The player ship's own behaviours (Evade dodges, Trail breaks) read the personality on the player's behaviour
// manager ([manager+8]+0x1584); the ship's own one is saved once per manager and put back by Restore.
static PersonalityBuf g_shipOwn;
static u8* g_shipOwnManager = nullptr;

// The personality on a behaviour manager, guarded (was: unguarded in `ai layout`, 18 Oct 2026).
static bool GetManagerPersonality(const Api& a, u8* m, void* out)
{
    __try
    {
        a.mGetPersonality(m, nullptr, out);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool SetManagerPersonality(const Api& a, u8* m, const void* p, void* save)
{
    __try
    {
        if (save)
            a.mGetPersonality(m, nullptr, save);
        a.mSetPersonality(m, nullptr, p);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool ApplyShipPersonality(const Personality& p)
{
    const Api& a = GetApi();
    u8* m = a.ok ? Manager(a) : nullptr;
    if (!m || !a.mSetPersonality || !a.mGetPersonality)
        return false;
    PersonalityBuf b;
    if (!DefaultPersonality(a, b))
        return false;
    memcpy(b.raw, &p, sizeof(Personality));
    const bool save = g_shipOwnManager != m;
    if (save && !DefaultPersonality(a, g_shipOwn))
        return false;
    if (!SetManagerPersonality(a, m, b.raw, save ? g_shipOwn.raw : nullptr))
        return false;
    g_shipOwnManager = m;
    return true;
}

void RestoreShipPersonality()
{
    const Api& a = GetApi();
    u8* m = a.ok ? Manager(a) : nullptr;
    if (m && m == g_shipOwnManager)
        SetManagerPersonality(a, m, g_shipOwn.raw, nullptr);
    g_shipOwnManager = nullptr;
}

int ForceTarget(u32 target)
{
    const Api& a = GetApi();
    const u32 ship = PlayerId();
    if (!a.forceTarget || !a.removeForced || !ship)
        return -100;
    int rc = 0;
    __try
    {
        if (g_forced)
            a.removeForced(ship, g_forced);
        g_forced = 0;
        if (target)
        {
            rc = a.forceTarget(ship, target);
            g_forced = target;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        rc = -101;
    }
    return rc;
}

int Release()
{
    const Api& a = GetApi();
    const u32 ship = PlayerId();
    int rc = 0;
    if (ship && g_forced)
        ForceTarget(0);
    if (ship && g_brainShip == ship)
    {
        const int graph = g_ownGraph > 0 ? g_ownGraph : GraphId(a, "PLAYER");
        rc = SubmitPersonality(a, ship, graph, g_own);
        Log("ai: brain off, graph %d, rc %d", graph, rc);
    }
    g_brainShip = 0;
    g_brainLabel.clear();
    g_forced = 0;
    if (ship)
        FreeFlight();
    return rc;
}

void Update()
{
    // A new ship (load, death, launch) carries no brain of ours: forget the old session.
    if (g_brainShip && g_brainShip != PlayerId())
    {
        g_brainShip = 0;
        g_brainLabel.clear();
        g_forced = 0;
    }
}

// ------------------------------------------------------------------ debug command
static bool ParseDirective(std::istringstream& in, Directive& d, std::string* err)
{
    std::string t;
    std::vector<std::string> pos;
    while (in >> t)
    {
        const size_t eq = t.find('=');
        if (eq == std::string::npos)
        {
            pos.push_back(t);
            continue;
        }
        const std::string k = Lower(t.substr(0, eq)), v = t.substr(eq + 1);
        if (k == "range")
            d.range = static_cast<float>(atof(v.c_str()));
        else if (k == "thrust")
            d.thrust = static_cast<float>(atof(v.c_str()));
        else if (k == "cruise")
            d.cruise = v != "0" && _stricmp(v.c_str(), "false") != 0;
        else if (k == "time")
            d.time = static_cast<float>(atof(v.c_str()));
        else if (k == "fire")
            d.fire = v != "0" && _stricmp(v.c_str(), "false") != 0;
        else if (k == "mode")
            d.mode = atoi(v.c_str());
        else if (k == "client")
            d.client = v != "0" && _stricmp(v.c_str(), "false") != 0;
        else if (k == "off")
        {
            const std::vector<std::string> o = SplitList(v);
            for (size_t i = 0; i < 3 && i < o.size(); ++i)
                d.offset[i] = static_cast<float>(atof(o[i].c_str()));
        }
        else
        {
            if (err)
                *err = "unknown option " + k;
            return false;
        }
    }
    if (pos.size() >= 3)
    {
        d.hasPos = true;
        for (int i = 0; i < 3; ++i)
            d.pos[i] = static_cast<float>(atof(pos[i].c_str()));
    }
    else if (!pos.empty())
        d.target = static_cast<u32>(strtoul(pos[0].c_str(), nullptr, 0));
    return true;
}

static std::string StateLine()
{
    State s;
    if (!ReadState(s))
        return "not in space";
    char buf[300];
    sprintf_s(buf, "op=%d %s locked=%d priority=%d graph=%d player_graph=%d behavior=%d brain=%d pilot=%s forced=%u",
              s.op, OpName(s.op), s.locked, s.priority, s.graph, s.graphPlayer, s.behavior, s.brain,
              s.pilot.empty() ? "-" : s.pilot.c_str(), s.forced);
    return buf;
}

bool Command(const std::string& cmd, const std::string& rest, std::string* reply)
{
    if (cmd == "usebattery" ||
        cmd == "usenano") // test: `usebattery <n>` / `usenano <n>` - the game's use_item, n units
    {
        const int n = UseConsumable(cmd == "usebattery", rest.empty() ? 1 : atoi(rest.c_str()));
        *reply =
            n ? "used " + std::to_string(n) : std::string("not used (none aboard, not flying, or use_item unverified)");
        return true;
    }
    if (cmd != "ai")
        return false;
    std::istringstream in(rest);
    std::string sub;
    in >> sub;
    sub = Lower(sub);
    std::string err;
    if (sub.empty() || sub == "state")
        *reply = StateLine();
    else if (sub == "man") // ai man <op> [flag]: the player's maneuver dispatcher on the current target
    {
        std::string o;
        int flag = 0;
        in >> o >> flag;
        const int op = OpFromName(o);
        *reply = op < -1 ? "unknown op " + o
                         : std::string("maneuver ") + OpName(op) + (Maneuver(op, flag != 0) ? " ok" : " refused") +
                               " | " + StateLine();
    }
    else if (sub ==
             "dir") // ai dir <op|cancel> [id | x y z] [range= thrust= cruise= off=x,y,z time= mode= fire= client=]
    {
        std::string o;
        in >> o;
        Directive d;
        d.op = _stricmp(o.c_str(), "cancel") == 0 ? kCancel : OpFromName(o);
        if (d.op < -1 && d.op != kCancel)
            *reply = "unknown op " + o;
        else if (!ParseDirective(in, d, &err))
            *reply = err;
        else
            *reply = "submit " + o + " rc=" + std::to_string(Submit(d)) + " | " + StateLine();
    }
    else if (sub == "self") // ai self: the ship's capability from the game's objects
    {
        Self s;
        char buf[256];
        if (!ReadSelf(s))
            *reply = "not flying";
        else
        {
            sprintf_s(buf,
                      "hull=%.2f hull_max=%.0f shield=%.2f shield_max=%.0f power=%.2f gun_range=%.0f nanobots=%d/%d "
                      "batteries=%d/%d tractor=%.0f hold=%.0f/%.0f",
                      s.hull, s.hullMax, s.shield, s.shieldMax, s.power, s.gunRange, s.nanobots, s.nanobotMax,
                      s.batteries, s.batteryMax, s.tractorRange, s.holdFree, s.holdSize);
            *reply = buf;
        }
    }
    else if (sub == "pilots")
    {
        std::ostringstream out;
        const std::vector<std::string> p = Pilots();
        out << p.size() << " pilots:";
        for (const std::string& n : p)
            out << " " << n;
        *reply = out.str();
    }
    else if (sub == "pilot") // ai pilot <nickname>: the game's [Pilot] into the current personality
    {
        std::string n;
        in >> n;
        Current();
        if (LoadPilot(n, g_current.p(), &err))
        {
            g_currentLabel = Lower(n);
            *reply = "personality = " + g_currentLabel;
        }
        else
            *reply = "pilot failed: " + err;
    }
    else if (sub ==
             "rawdiff") // ai rawdiff <npc id> <pilot>: 4-byte words where the NPC's personality and the pilot's differ
    {
        u32 id = 0;
        std::string pilot;
        in >> id >> pilot;
        Personality live{}, file{};
        std::string loadErr;
        if (!CopyNpcPersonality(id, live) || !LoadPilot(pilot, file, &loadErr))
        {
            *reply = "rawdiff failed " + loadErr;
            return true;
        }
        std::ostringstream o;
        o << "rawdiff size=0x" << std::hex << sizeof(Personality);
        const u32* a1 = reinterpret_cast<const u32*>(&live);
        const u32* b1 = reinterpret_cast<const u32*>(&file);
        for (size_t i = 0; i < sizeof(Personality) / 4; ++i)
            if (a1[i] != b1[i])
                o << "\n+0x" << i * 4 << " live=" << a1[i] << " file=" << b1[i] << " (" << std::dec
                  << *reinterpret_cast<const float*>(&a1[i]) << " / " << *reinterpret_cast<const float*>(&b1[i]) << ")"
                  << std::hex;
        *reply = o.str();
    }
    else if (sub == "copy") // ai copy <npc id>: a live NPC's personality
    {
        u32 id = 0;
        in >> id;
        Current();
        if (CopyNpcPersonality(id, g_current.p()))
        {
            g_currentLabel = "npc " + std::to_string(id);
            *reply = "personality = " + g_currentLabel;
        }
        else
            *reply = "copy failed";
    }
    else if (sub == "default")
    {
        g_currentInit = false;
        Current();
        *reply = "personality = default";
    }
    else if (sub == "field") // ai field <name>
    {
        std::string n;
        in >> n;
        *reply = n + " = " + GetField(Current(), n);
    }
    else if (sub == "fields")
    {
        std::ostringstream out;
        for (const std::string& n : FieldNames())
            out << n << " = " << GetField(Current(), n) << "\n";
        *reply = out.str();
    }
    else if (sub == "set") // ai set <name> <value...>
    {
        std::string n, v;
        in >> n;
        std::getline(in >> std::ws, v);
        *reply = SetField(Current(), n, v, &err) ? n + " = " + GetField(Current(), n) : err;
    }
    else if (sub == "brain") // ai brain [graph]
    {
        std::string g = "FIGHTER";
        in >> g;
        *reply = "brain rc=" + std::to_string(Brain(g)) + " | " + StateLine();
    }
    else if (sub == "cbrain") // ai cbrain [graph] [flag]: experiment, client-side manager
    {
        std::string g = "FIGHTER";
        int flag = 1;
        in >> g >> flag;
        *reply = "client brain rc=" + std::to_string(ClientBrain(g, flag != 0)) +
                 " trail_distance(read back)=" + std::to_string(ClientTrailDistance()) + " | " + StateLine();
    }
    else if (sub == "ctrail")
        *reply = "client manager trail_distance=" + std::to_string(ClientTrailDistance());
    else if (sub ==
             "layout") // ai layout <float>: our struct size/offsets, and where <float> sits in the manager's copy
    {
        float want = 0;
        in >> want;
        std::ostringstream out;
        out << "sizeof(Personality)=0x" << std::hex << sizeof(Personality) << " (game 0x3D8) trail_distance@0x"
            << offsetof(Personality, Trail.trail_distance) << " Job@0x" << offsetof(Personality, Job) << std::dec;
        const Api& a = GetApi();
        u8* m = a.ok ? Manager(a) : nullptr;
        PersonalityBuf b;
        if (m && a.mGetPersonality && DefaultPersonality(a, b) && GetManagerPersonality(a, m, b.raw))
        {
            for (size_t o = 0; o + 4 <= 0x3D8; o += 4)
                if (*reinterpret_cast<float*>(b.raw + o) == want)
                    out << " found@0x" << std::hex << o << std::dec;
        }
        *reply = out.str();
    }
    else if (sub == "release")
        *reply = "release rc=" + std::to_string(Release()) + " | " + StateLine();
    else if (sub == "target") // ai target <id|0>
    {
        u32 id = 0;
        in >> id;
        *reply = "forced target rc=" + std::to_string(ForceTarget(id)) + " | " + StateLine();
    }
    else if (sub == "dockport") // ai dockport: the docking port the game assigned (object id, index)
    {
        u32 id = 0;
        int index = -1;
        *reply = DockPort(id, index) ? "dockport obj=" + std::to_string(id) + " index=" + std::to_string(index)
                                     : std::string("dockport none");
    }
    else if (sub == "free")
    {
        FreeFlight();
        *reply = StateLine();
    }
    else
        *reply = "ai: state | man <op> [flag] | dir <op|cancel> [id | x y z] [range= thrust= cruise= off= time= mode= "
                 "fire= client=] | "
                 "pilots | pilot <name> | copy <npc id> | default | field <name> | fields | set <name> <value> | "
                 "brain [graph] | release | target <id|0> | free";
    return true;
}
} // namespace ai
