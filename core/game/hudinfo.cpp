// FLvibed - HUD 2's readers (hudinfo.h), each from the game's own state: Freelancer.exe's weapon-group manager (signed in
// core\flexe.h "HUD 2 readers"), Common.dll's exported accessors (CShip, CEEngine, CETractor, CEGun, Universe,
// Archetype, Reputation::Vibe), server.dll's pub:: functions, the system files for the zone and lane names. Cheap: names
// cached by id, lists refreshed at most 10 Hz, every object read guarded (__try, in small POD-only helpers) and
// class-checked against Common.dll's exported vtables; anything unknown reads as unknown. Facts: docs\FREELANCER_RE.md
// "HUD 2 readers".
#include "hudinfo.h"

#include "fldll.h"
#include "flexe.h"
#include "game/ai.h"
#include "game/flhash.h"
#include "game/ini.h"
#include "game/space.h"
#include "log.h"
#include "str.h"
#include "text.h"

#include <fstream>
#include <sstream>
#include <unordered_map>

namespace hudinfo
{
namespace
{
// ---- the game's functions, resolved once per process (null = that reader fails closed)
struct Fns
{
    bool tried = false;
    // Common.dll
    void*(__cdecl* getEquipment)(u32) = nullptr;               // Archetype::GetEquipment
    const u8*(__cdecl* getSystem)(u32) = nullptr;              // Universe::get_system
    void*(__cdecl* findObj)(const u32&, u32) = nullptr;        // CObject::Find (adds a reference)
    u32(__thiscall* releaseObj)(void*) = nullptr;              // CObject::Release
    u32(__thiscall* getType)(const void*) = nullptr;           // CSimple::get_type
    u32(__thiscall* rootId)(const void*) = nullptr;            // Archetype::Root::get_id
    bool(__thiscall* anyThrusters)(const void*) = nullptr;     // CShip::any_thrusters_on
    void*(__thiscall* equipFirst)(void*, u32) = nullptr;       // CEquipManager::FindFirst(EquipmentClass)
    void*(__thiscall* equipById)(void*, u16) = nullptr;        // CEquipManager::FindByID(sub-object id)
    bool(__thiscall* equipActive)(const void*) = nullptr;      // CEquip::IsActive
    bool(__thiscall* engineEnabled)(const void*) = nullptr;    // CEEngine::IsControllerEnabled
    bool(__thiscall* cruiseEngaged)(const void*) = nullptr;    // CEEngine::IsCruiseEngaged
    float(__thiscall* cruiseElapsed)(const void*) = nullptr;   // CEEngine::GetCruiseChargeTimeElapsed
    int(__thiscall* tractorArms)(const void*) = nullptr;       // CETractor::CountActiveArms
    float(__thiscall* secondsTillLock)(const void*) = nullptr; // CEGun::GetSecondsTillLock
    bool(__thiscall* gunGuided)(const void*) = nullptr;        // CEGun::is_guided
    int(__thiscall* followerCount)(void*) = nullptr;           // CShip::get_formation_follower_count
    int(__thiscall* followers)(void*, void**, u32) = nullptr;  // CShip::get_formation_followers
    int(__cdecl* vibeRank)(const int&, u32&) = nullptr;        // Reputation::Vibe::GetRank
    u32(__cdecl* createId)(const char*) = nullptr;             // CreateID
    const float* disruptTime = nullptr;                        // EngineEquipConsts::CRUISE_DISRUPT_TIME
    u32 lockTimeField = 0, lockClockField = 0;                 // GetSecondsTillLock's Munition / CEGun fields
    // server.dll
    int(__cdecl* getRep)(u32, int&) = nullptr;                                  // pub::SpaceObj::GetRep
    int(__cdecl* getAffiliation)(const int&, u32&) = nullptr;                   // pub::Reputation::GetAffiliation
    int(__cdecl* getShortName)(const u32&, u32&) = nullptr;                     // pub::Reputation::GetShortGroupName
    int(__cdecl* inspectCash)(const u32&, int&) = nullptr;                      // pub::Player::InspectCash
    int(__cdecl* inZones)(u32, const float*, float, u32*, u32, u32&) = nullptr; // pub::System::InZones
    int(__cdecl* jumpTarget)(const u32&, u32&, u32&) = nullptr;                 // pub::SpaceObj::GetJumpTarget
    int(__cdecl* repRank)(const int&, float&) = nullptr;                        // pub::Reputation::GetRank
};

template <class T> void Get(T& p, HMODULE m, const char* name) { p = reinterpret_cast<T>(GetProcAddress(m, name)); }

const Fns& F()
{
    static Fns f;
    if (f.tried)
        return f;
    HMODULE c = GetModuleHandleA("common.dll"), s = GetModuleHandleA("server.dll");
    if (!c || !s)
        return f; // not loaded yet: try again later
    f.tried = true;
    Get(f.getEquipment, c, "?GetEquipment@Archetype@@YAPAUEquipment@1@I@Z");
    Get(f.getSystem, c, "?get_system@Universe@@YAPBUISystem@1@I@Z");
    Get(f.findObj, c, "?Find@CObject@@SAPAU1@ABIW4Class@1@@Z");
    Get(f.releaseObj, c, "?Release@CObject@@QAEIXZ");
    Get(f.getType, c, "?get_type@CSimple@@QBEIXZ");
    Get(f.rootId, c, "?get_id@Root@Archetype@@QBEIXZ");
    Get(f.anyThrusters, c, "?any_thrusters_on@CShip@@QBE_NXZ");
    Get(f.equipFirst, c, "?FindFirst@CEquipManager@@QAEPAVCEquip@@I@Z");
    Get(f.equipById, c, "?FindByID@CEquipManager@@QAEPAVCEquip@@G@Z");
    Get(f.equipActive, c, "?IsActive@CEquip@@UBE_NXZ");
    Get(f.engineEnabled, c, "?IsControllerEnabled@CEEngine@@UBE_NXZ");
    Get(f.cruiseEngaged, c, "?IsCruiseEngaged@CEEngine@@QBE_NXZ");
    Get(f.cruiseElapsed, c, "?GetCruiseChargeTimeElapsed@CEEngine@@QBEMXZ");
    Get(f.tractorArms, c, "?CountActiveArms@CETractor@@QBEHXZ");
    Get(f.secondsTillLock, c, "?GetSecondsTillLock@CEGun@@QBEMXZ");
    Get(f.gunGuided, c, "?is_guided@CEGun@@QBE_NXZ");
    Get(f.followerCount, c, "?get_formation_follower_count@CShip@@QAEHXZ");
    Get(f.followers, c, "?get_formation_followers@CShip@@QAEHQAPAUIObjRW@@I@Z");
    Get(f.vibeRank, c, "?GetRank@Vibe@Reputation@@YAHABHAAI@Z");
    Get(f.createId, c, "?CreateID@@YAIPBD@Z");
    Get(f.disruptTime, c, "?CRUISE_DISRUPT_TIME@EngineEquipConsts@@3MA");
    // CEGun::GetSecondsTillLock = max(0, the munition's time_to_lock - the gun's lock clock), both offsets from its own
    // code: mov eax,[ecx+4Ch]; fld [0.0]; cmp dword [eax+90h],2; jne; fstp st0; fld [eax+TTL]; fsub [ecx+CLOCK]
    const char* lockSig = "8B 41 4C D9 05 ?? ?? ?? ?? 83 B8 90 00 00 00 02 75 ?? DD D8 D9 80 ?? ?? ?? ?? D8 A1 ?? ?? ?? ??";
    f.lockTimeField = hk::ExportOperand("common.dll", "?GetSecondsTillLock@CEGun@@QBEMXZ", lockSig, 22);
    f.lockClockField = hk::ExportOperand("common.dll", "?GetSecondsTillLock@CEGun@@QBEMXZ", lockSig, 28);
    Get(f.getRep, s, "?GetRep@SpaceObj@pub@@YAHIAAH@Z");
    Get(f.getAffiliation, s, "?GetAffiliation@Reputation@pub@@YAHABHAAI@Z");
    Get(f.getShortName, s, "?GetShortGroupName@Reputation@pub@@YAHABIAAI@Z");
    Get(f.inspectCash, s, "?InspectCash@Player@pub@@YAHABIAAH@Z");
    Get(f.inZones, s, "?InZones@System@pub@@YAHIABVVector@@MQAIIAAI@Z");
    Get(f.jumpTarget, s, "?GetJumpTarget@SpaceObj@pub@@YAHABIAAI1@Z");
    Get(f.repRank, s, "?GetRank@Reputation@pub@@YAHABHAAM@Z");
    Log("hudinfo: equip=%p system=%p find=%p thrusters=%p engine=%p tractor=%p lock=%p (+0x%X / +0x%X) rank=%p cash=%p "
        "zones=%p jump=%p weapon groups=%s",
        f.getEquipment, f.getSystem, f.findObj, f.anyThrusters, f.engineEnabled, f.tractorArms, f.secondsTillLock,
        f.lockTimeField, f.lockClockField, f.vibeRank, f.inspectCash, f.inZones, f.jumpTarget,
        exe::WeaponGroups.ok() && exe::WeaponGroupCurrent.ok() ? "ok" : "UNRESOLVED");
    return f;
}

double Now() { return perf::Now(); }

// Class checks before any field is read or a class's own accessor is called on an object
bool IsShip(const void* o) { return hk::IsExportedClass(o, "common.dll", "??_7CShip@@6BCEqObj@@@"); }
bool IsEngine(const void* o)
{
    return hk::IsExportedClass(o, "common.dll", "??_7CEEngine@@6BCInternalEquip@@@") ||
           hk::IsExportedClass(o, "common.dll", "??_7CEEngine@@6BCPhysControllerEquip@@@");
}
bool IsTractor(const void* o) { return hk::IsExportedClass(o, "common.dll", "??_7CETractor@@6B@"); }
bool IsGun(const void* o) { return hk::IsExportedClass(o, "common.dll", "??_7CEGun@@6B@"); }

constexpr u32 kClassSolar = 0x303, kClassShip = 0x503, kClassLoot = 0x803;
constexpr u32 kEquipManager = 0xE4; // CEqObj +0xE4 (FLHookSDK CEqObj; core\game\space_internal.h, ai.cpp)
constexpr u32 kEquipEngine = 1u << 17, kEquipTractor = 1u << 20;

// ---- guarded reads (POD only: __try cannot sit in a function that unwinds objects)
void* CobjectOf(void* iobj)
{
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

// The player's CShip (GetPlayerIObj -> IObjInspect::cobject), null when not flying or not a CShip
void* PlayerShip()
{
    void* iobj = hk::Call<void*(__cdecl*)()>(exe::GetPlayerIObj);
    void* ship = iobj ? CobjectOf(iobj) : nullptr;
    return IsShip(ship) ? ship : nullptr;
}

void* FirstEquip(void* ship, u32 cls)
{
    const Fns& f = F();
    if (!ship || !f.equipFirst)
        return nullptr;
    __try
    {
        return f.equipFirst(static_cast<u8*>(ship) + kEquipManager, cls);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return nullptr;
    }
}

void* EquipById(void* ship, u16 sub)
{
    const Fns& f = F();
    if (!ship || !f.equipById)
        return nullptr;
    __try
    {
        return f.equipById(static_cast<u8*>(ship) + kEquipManager, sub);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return nullptr;
    }
}

// A mounted equipment's archetype id (CEquip::EquipArch, Archetype::Root::get_id), 0 unknown
u32 EquipArchId(void* eq)
{
    static const auto equipArch = reinterpret_cast<const void*(__thiscall*)(const void*)>(GetProcAddress(
        GetModuleHandleA("common.dll"), "?EquipArch@CEquip@@QBEPBUEquipment@Archetype@@XZ"));
    const Fns& f = F();
    if (!eq || !equipArch || !f.rootId)
        return 0;
    __try
    {
        const void* arch = equipArch(eq);
        return arch ? f.rootId(arch) : 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
}

void* PlayerEngine()
{
    void* e = FirstEquip(PlayerShip(), kEquipEngine);
    return IsEngine(e) ? e : nullptr;
}

// Calls a bool / int / float accessor on a checked object, `fail` on a fault
template <class R> R Guard(R(__thiscall* fn)(const void*), const void* obj, R fail)
{
    if (!fn || !obj)
        return fail;
    __try
    {
        return fn(obj);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return fail;
    }
}

u32 ReadU32(uintptr_t a, u32 fail = 0)
{
    __try
    {
        return *reinterpret_cast<const u32*>(a);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return fail;
    }
}

float ReadF32(uintptr_t a, float fail)
{
    __try
    {
        return *reinterpret_cast<const float*>(a);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return fail;
    }
}

// Archetype::Root: id +8 (its get_id), ids_name +0x14 (FLHookSDK Root; verified in game) - 0 unknown
u32 EquipIds(u32 arch)
{
    const Fns& f = F();
    if (!arch || !f.getEquipment || !f.rootId)
        return 0;
    __try
    {
        const u8* e = static_cast<const u8*>(f.getEquipment(arch));
        return e && f.rootId(e) == arch ? *reinterpret_cast<const u32*>(e + 0x14) : 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
}

// Universe::ISystem: id +0x4C, strid_name +0x68 (FLHookSDK ISystem; verified in game against universe.ini) - 0 unknown
u32 SystemIds(u32 system)
{
    const Fns& f = F();
    if (!system || !f.getSystem)
        return 0;
    __try
    {
        const u8* s = f.getSystem(system);
        return s && *reinterpret_cast<const u32*>(s + 0x4C) == system ? *reinterpret_cast<const u32*>(s + 0x68) : 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
}

// An object of the player's system by id (CObject::Find per class, the reference released at once): its CSimple type
// bits and the class it was found in. False when not found.
bool ObjectType(u32 id, u32& type, u32& cls)
{
    const Fns& f = F();
    if (!id || !f.findObj || !f.releaseObj || !f.getType)
        return false;
    static const u32 classes[] = {kClassShip, kClassSolar, kClassLoot};
    __try
    {
        for (u32 c : classes)
            if (void* o = f.findObj(id, c))
            {
                type = f.getType(o);
                f.releaseObj(o);
                cls = c;
                return true;
            }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
    return false;
}

int RepOf(u32 id)
{
    const Fns& f = F();
    int rep = 0;
    if (!id || !f.getRep)
        return 0;
    __try
    {
        if (f.getRep(id, rep) != 0)
            rep = 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        rep = 0;
    }
    return rep;
}

// The reputation's group and that group's short name (ids); false when unknown
bool AffiliationIds(int rep, u32& group, u32& ids, bool wantName)
{
    const Fns& f = F();
    group = ids = 0;
    if (!rep || !f.getAffiliation || !f.getShortName)
        return false;
    __try
    {
        if (f.getAffiliation(rep, group) != 0 || !group)
            return false;
        if (wantName && f.getShortName(group, ids) != 0)
            ids = 0;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

int VibeRank(int rep)
{
    const Fns& f = F();
    u32 rank = 0;
    if (!rep || !f.vibeRank)
        return -1;
    __try
    {
        return f.vibeRank(rep, rank) == 0 ? static_cast<int>(rank) : -1;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return -1;
    }
}

int ZonesAt(u32 system, const float pos[3], u32* ids, u32 max)
{
    const Fns& f = F();
    u32 count = 0;
    if (!f.inZones)
        return -1;
    __try
    {
        f.inZones(system, pos, 0.0f, ids, max, count);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return -1;
    }
    return static_cast<int>(count < max ? count : max);
}

u32 JumpTargetSystem(u32 gate)
{
    const Fns& f = F();
    u32 system = 0, object = 0;
    if (!gate || !f.jumpTarget)
        return 0;
    __try
    {
        return f.jumpTarget(gate, system, object) == 0 ? system : 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
}

int FollowersOf(void* ship, u32* ids, int max)
{
    const Fns& f = F();
    if (!ship || !f.followers)
        return 0;
    void* list[16] = {};
    int out = 0;
    __try
    {
        const int n = f.followers(ship, list, 16);
        for (int i = 0; i < n && i < 16 && out < max; ++i)
            if (list[i])
                if (const auto getId = dll::ObjGetId.get<dll::ObjGetIdFn>(list[i]))
                    if (const u32 id = getId(list[i]))
                        ids[out++] = id;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
    return out;
}

// A ship by id with a reference (CObject::Find); Release() it after use. Null when not a CShip of the system.
void* FindShip(u32 id)
{
    const Fns& f = F();
    if (!id || !f.findObj)
        return nullptr;
    __try
    {
        return f.findObj(id, kClassShip);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return nullptr;
    }
}
void Release(void* o)
{
    const Fns& f = F();
    if (!o || !f.releaseObj)
        return;
    __try
    {
        f.releaseObj(o);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

// A guided launcher's lock: progress 0..1 (time_to_lock 0 = locked at once), -1 not a guided CEGun
float GunLock(void* eq, uintptr_t munition)
{
    const Fns& f = F();
    if (!IsGun(eq) || !munition || !f.gunGuided || !f.secondsTillLock || !f.lockTimeField)
        return -1;
    __try
    {
        if (!f.gunGuided(eq))
            return -1;
        const float ttl = *reinterpret_cast<const float*>(munition + f.lockTimeField);
        const float left = f.secondsTillLock(eq);
        return ttl <= 0 ? 1.0f : Clampf(1.0f - left / ttl, 0, 1);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return -1;
    }
}

// ---- the weapon groups (Freelancer.exe's manager exe::WeaponGroups, docs\FREELANCER_RE.md "HUD 2 readers"): the
// current group (0-based) at +0x28 (its getter exe::WeaponGroupCurrent); the stored groups as a vector of pointers
// (begin +0xC, end +0x10); each group {vector<std::string> first +4 / last +8 / end +0xC; int index +0x10}; each
// entry a VC6 std::string of the hardpoint name, 0x10 bytes (+0 allocator, +4 char*, +8 length, +0xC capacity). The
// current group's stored copy is refreshed only when the player switches away (USER_WEAPON_GROUPn first saves the
// current selection, 0x4F5580, then loads the new one, 0x4F5920): its live membership is the weapons switched on now.
constexpr u32 kWgBegin = 0xC, kWgEnd = 0x10, kGroupFirst = 0x4, kGroupLast = 0x8, kGroupIndex = 0x10,
              kEntrySize = 0x10, kEntryPtr = 0x4, kEntryLen = 0x8;

int CurrentGroup0()
{
    const uintptr_t mgr = exe::WeaponGroups.get();
    if (!mgr || !exe::WeaponGroupCurrent.ok())
        return -1;
    int g = -1;
    __try
    {
        g = hk::Call<int(__thiscall*)(uintptr_t)>(exe::WeaponGroupCurrent, mgr);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return -1;
    }
    return g >= 0 && g < 6 ? g : -1;
}

struct HpBits
{
    char hp[32];
    u32 bits;
};
// The stored groups as (hardpoint, group bits) rows; -1 when the structure is not as expected
int StoredGroups(HpBits* rows, int max)
{
    const uintptr_t mgr = exe::WeaponGroups.get();
    if (!mgr)
        return -1;
    int n = 0;
    __try
    {
        const uintptr_t* b = *reinterpret_cast<uintptr_t* const*>(mgr + kWgBegin);
        const uintptr_t* e = *reinterpret_cast<uintptr_t* const*>(mgr + kWgEnd);
        if (!b && !e)
            return 0;
        if (!b || e < b || e - b > 16)
            return -1;
        for (const uintptr_t* g = b; g < e; ++g)
        {
            const int index = *reinterpret_cast<const int*>(*g + kGroupIndex);
            const uintptr_t first = *reinterpret_cast<const uintptr_t*>(*g + kGroupFirst);
            const uintptr_t last = *reinterpret_cast<const uintptr_t*>(*g + kGroupLast);
            if (index < 0 || index >= 6 || last < first || (last - first) % kEntrySize ||
                (last - first) / kEntrySize > 64)
                return -1;
            for (uintptr_t en = first; en < last; en += kEntrySize)
            {
                const char* hp = *reinterpret_cast<const char* const*>(en + kEntryPtr);
                const u32 len = *reinterpret_cast<const u32*>(en + kEntryLen);
                if (!hp || len == 0 || len >= sizeof(rows[0].hp))
                    continue;
                int r = 0;
                while (r < n && !(_strnicmp(rows[r].hp, hp, len) == 0 && rows[r].hp[len] == 0))
                    ++r;
                if (r == n)
                {
                    if (n == max)
                        continue;
                    memcpy(rows[n].hp, hp, len);
                    rows[n].hp[len] = 0;
                    rows[n].bits = 0;
                    ++n;
                }
                rows[r].bits |= 1u << index;
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return -1;
    }
    return n;
}

// A string id's text, cached (resources do not change while the game runs)
const std::wstring& IdsText(u32 ids)
{
    static std::unordered_map<u32, std::wstring> cache;
    const auto it = cache.find(ids);
    if (it != cache.end())
        return it->second;
    std::wstring t = ids ? text::Ids(ids) : std::wstring();
    while (!t.empty() && iswspace(t.back()))
        t.pop_back();
    return cache.emplace(ids, std::move(t)).first->second;
}

// ---- the system files: universe.ini (system nickname -> file) and each system's named zones and lane end names
std::string DataDir()
{
    char path[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, path, MAX_PATH); // EXE\Freelancer.exe
    if (char* slash = strrchr(path, '\\'))
        *(slash + 1) = 0;
    return std::string(path) + "..\\DATA\\";
}

std::string IniLine(std::string s) { return str::Trim(str::StripComment(s)); }

struct NamedZone
{
    u32 ids = 0;
    float volume = 0; // shape volume without the constant (for "the smallest zone the ship is in")
};
struct SystemData
{
    bool loaded = false;
    std::unordered_map<u32, NamedZone> zones; // zone id (CreateID of its nickname) -> its name and size
    std::unordered_map<u32, u32> laneNames;   // lane ring id -> its tradelane_space_name (the lane's end rings)
};

const SystemData& SystemOf(u32 system)
{
    static std::unordered_map<u32, SystemData> systems;
    static std::unordered_map<u32, std::string> files; // system id -> file (relative to DATA\UNIVERSE)
    static bool universeRead = false;
    SystemData& sd = systems[system];
    const Fns& f = F();
    if (sd.loaded || !f.createId)
        return sd;
    sd.loaded = true;
    if (!universeRead)
    {
        universeRead = true;
        std::ifstream in(DataDir() + "UNIVERSE\\universe.ini");
        std::string line, nick, file;
        bool sys = false;
        auto flush = [&]()
        {
            if (sys && !nick.empty() && !file.empty())
                files[f.createId(nick.c_str())] = file;
            nick.clear(), file.clear();
        };
        while (std::getline(in, line))
        {
            line = IniLine(line);
            if (line.empty())
                continue;
            if (line[0] == '[')
            {
                flush();
                sys = _stricmp(line.c_str(), "[system]") == 0;
                continue;
            }
            const size_t eq = line.find('=');
            if (eq == std::string::npos)
                continue;
            const std::string k = str::Trim(line.substr(0, eq)), v = str::Trim(line.substr(eq + 1));
            if (_stricmp(k.c_str(), "nickname") == 0)
                nick = v;
            else if (_stricmp(k.c_str(), "file") == 0)
                file = v;
        }
        flush();
    }
    const auto it = files.find(system);
    if (it == files.end())
        return sd;
    std::ifstream in(DataDir() + "UNIVERSE\\" + it->second);
    std::string line, nick, shape;
    u32 ids = 0, laneName = 0;
    float size[3] = {0, 0, 0};
    int nsize = 0;
    enum
    {
        kOther,
        kZone,
        kObject
    } sec = kOther;
    auto flush = [&]()
    {
        if (!nick.empty())
        {
            const u32 id = f.createId(nick.c_str());
            if (sec == kZone && ids)
            {
                const std::string sh = str::Lower(shape);
                float v;
                if (sh == "sphere" || nsize < 2)
                    v = size[0] * size[0] * size[0];
                else if (sh == "cylinder" || sh == "ring")
                    v = size[0] * size[0] * size[1];
                else
                    v = size[0] * size[1] * (nsize >= 3 ? size[2] : size[1]);
                sd.zones[id] = NamedZone{ids, v};
            }
            else if (sec == kObject && laneName)
                sd.laneNames[id] = laneName;
        }
        nick.clear(), shape.clear(), ids = 0, laneName = 0, nsize = 0;
    };
    while (std::getline(in, line))
    {
        line = IniLine(line);
        if (line.empty())
            continue;
        if (line[0] == '[')
        {
            flush();
            sec = _stricmp(line.c_str(), "[zone]") == 0     ? kZone
                  : _stricmp(line.c_str(), "[object]") == 0 ? kObject
                                                            : kOther;
            continue;
        }
        const size_t eq = line.find('=');
        if (eq == std::string::npos || sec == kOther)
            continue;
        const std::string k = str::Trim(line.substr(0, eq)), v = str::Trim(line.substr(eq + 1));
        if (_stricmp(k.c_str(), "nickname") == 0)
            nick = v;
        else if (sec == kZone && _stricmp(k.c_str(), "ids_name") == 0)
            ids = static_cast<u32>(strtoul(v.c_str(), nullptr, 10));
        else if (sec == kZone && _stricmp(k.c_str(), "shape") == 0)
            shape = v;
        else if (sec == kZone && _stricmp(k.c_str(), "size") == 0)
            nsize = sscanf_s(v.c_str(), "%f , %f , %f", &size[0], &size[1], &size[2]);
        else if (sec == kObject && _stricmp(k.c_str(), "tradelane_space_name") == 0)
            laneName = static_cast<u32>(strtoul(v.c_str(), nullptr, 10));
    }
    flush();
    Log("hudinfo: system %u (%s): %zu named zones, %zu lane names", system, it->second.c_str(), sd.zones.size(),
        sd.laneNames.size());
    return sd;
}

float Dist(const float a[3], const float b[3])
{
    const float d[3] = {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
    return std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
}

struct P3
{
    float v[3];
};

// ---- the lane run: the chain from the exit ring to the lane's last ring, rebuilt when the rings change
struct LaneCache
{
    u32 entry = 0, exit = 0;
    std::vector<u32> rings; // the exit ring (the next one ahead) first .. the last ring
    std::vector<P3> pos;
    float tail = 0; // metres from the exit ring along the chain to the last ring
    float dir[3] = {0, 0, 0}; // the lane's unit direction at the exit ring
    u32 last = 0;
    bool ok = false;
};
LaneCache g_lane;

// A speed from position deltas between calls (the lane ride moves the ship by position: get_velocity reads ~2 m/s)
float PositionSpeed(const float pos[3])
{
    static float last[3];
    static double lastT = 0, speed = 0;
    const double t = Now();
    if (lastT > 0 && t - lastT <= 0.02)
        return static_cast<float>(speed); // same frame / too soon: keep the estimate
    if (lastT > 0 && t - lastT < 1.0)
    {
        const double v = Dist(pos, last) / (t - lastT);
        speed = speed > 0 ? speed * 0.6 + v * 0.4 : v;
    }
    else
        speed = 0;
    memcpy(last, pos, sizeof(last));
    lastT = t;
    return static_cast<float>(speed);
}

// ---- the moments the game state changed (sampled whenever a reader runs; HUD 2 reads them every frame)
struct Edges
{
    bool jumping = false; // a jump seen from its docking sequence on
    double jumpStart = 0;
    u32 jumpGate = 0;
    float jumpTotal = 0;
    std::wstring jumpToward;
    bool jumpTail = false; // the arrival's dock maneuver at the far gate (not a dock of the player's)
    bool lane = false;
    u32 laneLast = 0;
    float laneVmax = 0;   // the ride's top speed (the lane's own; the ship slows only into the last ring)
    float laneTotal = 0;  // metres from the entry ring to the last ring (this ride)
    int laneRings = 0;    // rings from the one after the entry to the last (this ride)
    double laneStart = 0; // when this ride began
    double laneEnded = -1; // a ride that ended short of its last ring
};
Edges g_edges;

// ---- docking: the server's answers, handed in by the HUD2 track (SetDockReply) from the controller host
struct DockReplies
{
    bool fed = false;
    u32 seq = 0;
    int response = -1; // the last kind-0 answer since the dock began (-1 none)
};
DockReplies g_dock;
} // namespace

// ================================================================================================ weapons
int ActiveWeaponGroup()
{
    const int g = CurrentGroup0();
    return g < 0 ? -1 : g + 1;
}

std::vector<WeaponInfo> Weapons()
{
    static std::vector<WeaponInfo> cache;
    static double at = -1;
    const double t = Now();
    if (at >= 0 && t - at < 0.1)
        return cache;
    at = t;
    cache.clear();
    if (!space::PlayerShipId())
        return cache;
    perf::Scope scope("hudinfo weapons");
    std::vector<space::Weapon> ws = space::PlayerWeapons();
    std::stable_sort(ws.begin(), ws.end(),
                     [](const space::Weapon& a, const space::Weapon& b)
                     { return (a.fireSlot > 0 ? a.fireSlot : 999) < (b.fireSlot > 0 ? b.fireSlot : 999); });
    HpBits rows[32];
    const int n = StoredGroups(rows, 32);
    const int cur = CurrentGroup0();
    void* ship = PlayerShip();
    for (const space::Weapon& w : ws)
    {
        WeaponInfo i;
        // the archetype id from the mounted equipment itself (CEquip::EquipArch -> Root::get_id; space::Weapon::archId
        // reads Root +4, which is not the nickname hash - docs\FREELANCER_RE.md "HUD 2 readers")
        i.arch = EquipArchId(EquipById(ship, w.subObj));
        i.name = EquipName(i.arch);
        i.launcher = w.homing;
        i.ammo = w.ammo;
        if (n >= 0 && cur >= 0)
        {
            u32 bits = 0;
            for (int r = 0; r < n; ++r)
                if (_stricmp(rows[r].hp, w.hardpoint) == 0)
                    bits = rows[r].bits;
            bits &= ~(1u << cur); // the current group = what is switched on now (its stored copy is stale)
            if (w.active)
                bits |= 1u << cur;
            i.groups = bits;
        }
        cache.push_back(std::move(i));
    }
    return cache;
}

std::wstring EquipName(u32 arch)
{
    static std::unordered_map<u32, std::wstring> cache;
    if (!arch)
        return L"";
    const auto it = cache.find(arch);
    if (it != cache.end())
        return it->second;
    if (!F().getEquipment)
        return L"";
    const u32 ids = EquipIds(arch);
    return cache[arch] = ids ? IdsText(ids) : std::wstring(); // an unknown id stays unknown
}

// ================================================================================================ names
static std::wstring SystemNameOf(u32 system)
{
    static std::unordered_map<u32, std::wstring> cache;
    if (!system)
        return L"";
    const auto it = cache.find(system);
    if (it != cache.end())
        return it->second;
    if (!F().getSystem)
        return L"";
    const u32 ids = SystemIds(system);
    return cache[system] = ids ? IdsText(ids) : std::wstring();
}

std::wstring SystemName() { return SystemNameOf(space::PlayerShipId() ? Mem<u32>(exe::PlayerSystem) : 0); }

std::wstring JumpToward(u32 objId) { return SystemNameOf(JumpTargetSystem(objId)); }

// A string id's template with its "%s" places filled in order (the game's arrival formats: "%s System, %s Space.")
static std::wstring FillS(std::wstring t, const std::wstring& a, const std::wstring* b)
{
    size_t p = t.find(L"%s");
    if (p == std::wstring::npos)
        return L"";
    t.replace(p, 2, a);
    if (b)
    {
        p = t.find(L"%s", p + a.size());
        if (p == std::wstring::npos)
            return L"";
        t.replace(p, 2, *b);
    }
    return t;
}

// HDE's Territory.dll (Jason Hood; it detours the arrival line's house code 0x45B020) words the line for the systems its
// DATA\UNIVERSE\Territory.ini lists: [Systems] "format = %s System, Border Worlds." then "systems = Bw01-10, St02c"
// (a nickname prefix with a numeric range keeping the first number's width, or one nickname), each format for the
// systems listed after it; [Houses] (commented out in HDE 0.7.1: "[;Houses]") house_id + systems with one format for
// all. Read once when the plugin is loaded; system id (CreateID of the nickname) -> (format, house ids or 0).
struct TerritoryLine
{
    std::wstring format;
    u32 houseIds = 0;
};
static const std::unordered_map<u32, TerritoryLine>& Territory()
{
    static std::unordered_map<u32, TerritoryLine> map;
    static bool read = false;
    if (read)
        return map;
    read = true;
    if (!GetModuleHandleA("Territory.dll"))
        return map;
    auto names = [](const std::string& list, std::vector<std::string>& out)
    {
        for (const std::string& f : ini::Fields(list))
        {
            const size_t dash = f.find('-');
            if (dash == std::string::npos)
            {
                out.push_back(f);
                continue;
            }
            size_t d = dash; // the digits before the dash: the range's first number
            while (d > 0 && isdigit(static_cast<unsigned char>(f[d - 1])))
                --d;
            const std::string prefix = f.substr(0, d), first = f.substr(d, dash - d);
            const int a = atoi(first.c_str()), b = atoi(f.c_str() + dash + 1);
            for (int i = a; i <= b && i - a < 100; ++i)
            {
                char n[16];
                sprintf_s(n, "%0*d", static_cast<int>(first.size()), i);
                out.push_back(prefix + n);
            }
        }
    };
    auto wide = [](const std::string& s)
    {
        std::wstring w(s.size() + 1, L'\0');
        const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], static_cast<int>(w.size()));
        w.resize(n > 0 ? static_cast<size_t>(n - 1) : 0);
        return w;
    };
    for (const ini::Section& s : ini::Read(ini::DataPath("UNIVERSE\\Territory.ini")))
    {
        const bool houses = s.type == "houses", systems = s.type == "systems";
        if (!houses && !systems)
            continue;
        std::wstring format = houses ? L"%s System, %s Space." : L"";
        if (houses)
            if (const std::string* f = s.Get("format"))
                format = wide(*f);
        u32 house = 0;
        for (const auto& kv : s.kv)
        {
            if (kv.first == "format" && systems)
                format = wide(kv.second);
            else if (kv.first == "house_id")
                house = static_cast<u32>(strtoul(kv.second.c_str(), nullptr, 10));
            else if (kv.first == "systems" && !format.empty())
            {
                std::vector<std::string> list;
                names(kv.second, list);
                for (const std::string& n : list)
                    map[flhash::CreateId(n)] = TerritoryLine{format, houses ? house : 0};
            }
        }
    }
    Log("hudinfo: Territory.dll loaded - %u systems worded by its Territory.ini", static_cast<u32>(map.size()));
    return map;
}

std::wstring ArrivalLine()
{
    static u32 at = 0;
    static std::wstring cache;
    const u32 system = space::PlayerShipId() ? Mem<u32>(exe::PlayerSystem) : 0;
    if (system == at)
        return cache;
    at = system;
    cache.clear();
    const std::wstring name = SystemNameOf(system);
    if (name.empty())
        return cache;
    const auto& territory = Territory();
    const auto t = territory.find(system);
    if (t != territory.end())
    {
        if (t->second.houseIds)
        {
            const std::wstring house = IdsText(t->second.houseIds);
            cache = FillS(t->second.format, name, &house);
        }
        else
            cache = FillS(t->second.format, name, nullptr);
        if (!cache.empty())
            return cache;
    }
    // the house as the game's arrival line finds it (0x45AFF0): its byte table by strid_name - 196609, 24 entries
    static const u32 kHouse[4] = {1249, 1250, 1252, 1251};
    const u32 strid = SystemIds(system);
    u32 houseIds = 0;
    if (exe::ArrivalHouse.ok() && strid >= 196609 && strid - 196609 < 24)
    {
        const u8 i = Mem<u8>(exe::ArrivalHouse, strid - 196609);
        houseIds = i < 4 ? kHouse[i] : 0;
    }
    const std::wstring house = houseIds ? IdsText(houseIds) : std::wstring();
    if (!house.empty())
        cache = FillS(IdsText(1374), name, &house);
    if (cache.empty())
        cache = FillS(IdsText(1373), name, nullptr);
    if (cache.empty())
        cache = name; // the formats unreadable: the name alone
    return cache;
}

std::wstring ZoneName()
{
    static std::wstring cache;
    static double at = -1;
    const double t = Now();
    if (at >= 0 && t - at < 0.1)
        return cache;
    at = t;
    cache.clear();
    const u32 ship = space::PlayerShipId();
    float pos[3];
    if (!ship || !space::ObjectPos(ship, pos))
        return cache;
    const u32 system = Mem<u32>(exe::PlayerSystem);
    const SystemData& sd = SystemOf(system);
    if (sd.zones.empty())
        return cache;
    u32 ids[64];
    const int count = ZonesAt(system, pos, ids, 64);
    const NamedZone* best = nullptr;
    for (int i = 0; i < count; ++i)
    {
        const auto it = sd.zones.find(ids[i]);
        if (it != sd.zones.end() && (!best || it->second.volume < best->volume))
            best = &it->second;
    }
    if (best)
        cache = IdsText(best->ids);
    return cache;
}

std::wstring FactionName(u32 objId)
{
    static std::unordered_map<u32, std::wstring> groups; // group id -> short name
    u32 group = 0, ids = 0;
    if (!AffiliationIds(RepOf(objId), group, ids, false))
        return L"";
    const auto it = groups.find(group);
    if (it != groups.end())
        return it->second;
    AffiliationIds(RepOf(objId), group, ids, true);
    return groups[group] = ids ? IdsText(ids) : std::wstring();
}

std::wstring ClassName(u32 objId)
{
    u32 type = 0, cls = 0;
    if (!ObjectType(objId, type, cls))
        return L"";
    if (cls == kClassLoot)
        return L"LOOT";
    // CSimple::get_type bits (FLHookSDK ObjectType): ships 16..22, solars 0..15
    static const wchar_t* const ships[] = {L"FIGHTER",   L"FREIGHTER",  L"GUNBOAT",    L"CRUISER",
                                           L"TRANSPORT", L"BATTLESHIP", L"MINING SHIP"};
    static const wchar_t* const solars[] = {
        L"MOON",       L"PLANET",  L"SUN",       L"BLACK HOLE",       L"SATELLITE", L"DOCKING RING",
        L"JUMP GATE",  L"TRADE LANE", L"STATION", L"WAYPOINT",        L"AIRLOCK",   L"JUMP HOLE",
        L"WEAPONS PLATFORM", L"DEPOT", L"",       L"SATELLITE"};
    for (int i = 0; i < 7; ++i)
        if (type & (1u << (16 + i)))
            return ships[i];
    for (int i = 0; i < 16; ++i)
        if (type & (1u << i))
            return solars[i];
    return L"";
}

int Level(u32 objId)
{
    u32 type = 0, cls = 0;
    if (!ObjectType(objId, type, cls) || cls != kClassShip || objId == space::PlayerShipId())
        return -1;
    const int rank = VibeRank(RepOf(objId));
    return rank > 0 && rank < 100 ? rank : -1;
}

// The game's own "unknown" test and unknown name id on a solar (POD helper for __try); 0 = known / not checkable
static u32 UnknownIdsOf(u32 id)
{
    const Fns& f = F();
    if (!f.findObj || !f.releaseObj || !exe::ObjectUnknown.ok() || !exe::ObjectUnknownIds.ok())
        return 0;
    u32 ids = 0;
    __try
    {
        if (void* o = f.findObj(id, kClassSolar)) // only solars are ever unknown (the game's test checks the class too)
        {
            if (exe::ObjectUnknown.fn<bool(__cdecl*)(const void*)>()(o))
                ids = exe::ObjectUnknownIds.fn<u32(__cdecl*)(const void*)>()(o);
            f.releaseObj(o);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        ids = 0;
    }
    return ids;
}

// A trade lane ring's name as the game's target panel shows it: "Planet Manhattan->Fort Bush" - the system file's
// tradelane_space_name of the lane's two end rings, joined by "->" (Freelancer.exe 0x4E8D30, called by the panel's name
// builder 0x4E884A for an object of type bit 7: it walks the ring's prev / next links (+0x1D8 / +0x1DC) to both ends,
// takes each end's name (CSolar::get_tradelane_space_name) and orders them by the side of the ring's plane the viewer is
// on: the end on the viewer's side first, the far end after the arrow - the way a ride from here goes). "" when the
// lane's ends carry no names (a mod's lane) or the links / positions cannot be read.
std::wstring LaneName(u32 ringId)
{
    u32 next = 0, prev = 0;
    if (!ringId || !space::LaneLinks(ringId, next, prev))
        return L"";
    auto end = [](u32 r, bool forward)
    {
        for (int guard = 0; guard < 64; ++guard)
        {
            u32 n = 0, p = 0;
            if (!space::LaneLinks(r, n, p))
                break;
            const u32 step = forward ? n : p;
            if (!step)
                break;
            r = step;
        }
        return r;
    };
    const u32 endPrev = end(ringId, false), endNext = end(ringId, true);
    const SystemData& sd = SystemOf(space::PlayerShipId() ? Mem<u32>(exe::PlayerSystem) : 0);
    const auto a = sd.laneNames.find(endPrev), b = sd.laneNames.find(endNext);
    if (a == sd.laneNames.end() || b == sd.laneNames.end())
        return L"";
    const std::wstring na = IdsText(a->second), nb = IdsText(b->second);
    if (na.empty() || nb.empty())
        return L"";
    // the viewer's side: along the lane at this ring (toward the next ring, else from the previous one)
    float rp[3], me[3], other[3];
    bool towardNext = true;
    if (space::ObjectPos(ringId, rp) && space::ObjectPos(space::PlayerShipId(), me))
    {
        float dir[3] = {0, 0, 0};
        if (next && space::ObjectPos(next, other))
            for (int k = 0; k < 3; ++k)
                dir[k] = other[k] - rp[k];
        else if (prev && space::ObjectPos(prev, other))
            for (int k = 0; k < 3; ++k)
                dir[k] = rp[k] - other[k];
        const float side = (me[0] - rp[0]) * dir[0] + (me[1] - rp[1]) * dir[1] + (me[2] - rp[2]) * dir[2];
        towardNext = side <= 0; // behind the ring (the previous rings' side): a ride goes toward the next end
    }
    return towardNext ? na + L"->" + nb : nb + L"->" + na;
}

std::wstring DisplayName(u32 objId)
{
    struct Entry
    {
        std::wstring name;
        double at = -1;
    };
    static std::unordered_map<u32, Entry> cache;
    if (!objId)
        return L"";
    Entry& e = cache[objId];
    const double t = Now();
    if (e.at >= 0 && t - e.at < 1.0)
        return e.name;
    e.at = t;
    // a waypoint (type bit 9): the game's own test answers "unknown" for it from here, but its HUD knows the object
    // (0x4D4C70's first branch: the targetable entry's flags) and the target panel shows "Waypoint" (25 Oct 2026,
    // SaveC703, the harness's route waypoint read "Unknown Object")
    u32 type = 0, cls = 0;
    const bool waypoint = ObjectType(objId, type, cls) && (type & (1u << 9));
    const u32 ids = waypoint ? 0 : UnknownIdsOf(objId);
    std::wstring n = ids ? IdsText(ids) : std::wstring();
    if (n.empty() && (type & (1u << 7))) // a known trade lane ring: its lane, as the game's panel names it (LaneName)
        n = LaneName(objId);
    e.name = !n.empty() ? n : space::SpeakerName(objId);
    if (e.name.empty() && waypoint)
        e.name = L"Waypoint";
    if (cache.size() > 512) // a long session in busy systems: start over (names come back at once)
    {
        const std::wstring keep = e.name;
        cache.clear();
        cache[objId] = Entry{keep, t};
        return keep;
    }
    return e.name;
}

// ================================================================================================ the player's ship
bool AfterburnerOn() { return Guard(F().anyThrusters, PlayerShip(), false); }

bool EngineKilled()
{
    void* e = PlayerEngine();
    return e && F().engineEnabled && !Guard(F().engineEnabled, e, true);
}

long long Credits()
{
    const Fns& f = F();
    if (!f.inspectCash)
        return -1;
    const u32 client = 1; // single player
    int cash = 0;
    return f.inspectCash(client, cash) == 0 ? cash : -1;
}

bool TractorActive()
{
    void* t = FirstEquip(PlayerShip(), kEquipTractor);
    return IsTractor(t) && Guard(F().tractorArms, t, 0) > 0;
}

int LootInReach()
{
    static int cache = -1;
    static double at = -1;
    const double t = Now();
    if (at >= 0 && t - at < 0.1)
        return cache;
    at = t;
    cache = -1;
    if (!space::PlayerShipId())
        return cache;
    perf::Scope scope("hudinfo loot");
    ai::Self s;
    if (!ai::ReadSelf(s) || s.tractorRange <= 0)
        return cache;
    int n = 0;
    for (const space::Contact& c : space::Contacts())
        if ((c.type & space::kLoot) && c.distance <= s.tractorRange)
            ++n;
    return cache = n;
}

// ================================================================================================ travel
bool Lane(LaneRun& out)
{
    out = LaneRun();
    space::Pilot p;
    const bool flying = space::ReadPilot(p);
    const bool on = flying && p.lane && p.laneExit;
    // edge: a ride that ended away from its last ring, not by the player's free flight (op 19 at once: docs)
    if (g_edges.lane && !on && flying && g_edges.laneLast && p.op != ai::OpFreeFlight)
    {
        float last[3];
        if (space::ObjectPos(g_edges.laneLast, last) && Dist(p.pos, last) > 2000)
            g_edges.laneEnded = Now();
    }
    if (on && !g_edges.lane)
        g_edges.laneVmax = 0, g_edges.laneTotal = 0, g_edges.laneRings = 0, g_edges.laneStart = Now(); // a new ride
    g_edges.lane = on;
    if (!on)
    {
        g_edges.laneLast = 0;
        return false;
    }
    LaneCache& lc = g_lane;
    if (!lc.ok || lc.exit != p.laneExit || lc.entry != p.laneEntry)
    {
        perf::Scope scope("hudinfo lane");
        lc = LaneCache();
        lc.entry = p.laneEntry, lc.exit = p.laneExit;
        // the direction: from the entry ring toward the exit ring (the exit = the next ring ahead; it reads the entry
        // ring itself in the first frames: then the side the ride starts to - unknown until it advances)
        u32 next = 0, prev = 0;
        int dir = 0; // 1 via next_ring, -1 via prev_ring
        // (the entry ring stays the ring entered while the exit advances: walk each way from it to the exit)
        for (int way = 1; p.laneEntry != p.laneExit && !dir && way >= -1; way -= 2)
            for (u32 cur = p.laneEntry, steps = 0; cur && steps < 64 && space::LaneLinks(cur, next, prev); ++steps)
            {
                cur = way > 0 ? next : prev;
                if (cur == p.laneExit)
                {
                    dir = way;
                    break;
                }
            }
        if (!dir)
            return false; // the first frames of a ride (or an unexpected chain): no run yet
        for (u32 cur = p.laneExit; cur && lc.rings.size() < 64;)
        {
            float pos[3];
            if (!space::ObjectPos(cur, pos) || !space::LaneLinks(cur, next, prev))
                break;
            lc.rings.push_back(cur);
            lc.pos.push_back(P3{{pos[0], pos[1], pos[2]}});
            cur = dir > 0 ? next : prev;
        }
        for (size_t i = 1; i < lc.pos.size(); ++i)
            lc.tail += Dist(lc.pos[i - 1].v, lc.pos[i].v);
        lc.last = lc.rings.empty() ? 0 : lc.rings.back();
        lc.ok = !lc.rings.empty();
        // the lane's direction at the exit ring: toward the next ring, else from the ring before it (the last leg)
        float from[3];
        if (lc.pos.size() >= 2)
            for (int k = 0; k < 3; ++k)
                lc.dir[k] = lc.pos[1].v[k] - lc.pos[0].v[k];
        else if (lc.ok && space::LaneLinks(lc.rings[0], next, prev) && space::ObjectPos(dir > 0 ? prev : next, from))
            for (int k = 0; k < 3; ++k)
                lc.dir[k] = lc.pos[0].v[k] - from[k];
        const float n = std::sqrt(lc.dir[0] * lc.dir[0] + lc.dir[1] * lc.dir[1] + lc.dir[2] * lc.dir[2]);
        for (float& c : lc.dir)
            c = n > 1 ? c / n : 0; // unknown: along = 0, the tail alone
    }
    if (!lc.ok)
        return false;
    g_edges.laneLast = lc.last;
    out.ringsLeft = static_cast<int>(lc.rings.size());
    // the destination: the last ring's tradelane_space_name (the system file names the place at each end of a lane;
    // the rings between carry none)
    const SystemData& sd = SystemOf(p.system);
    const auto it = sd.laneNames.find(lc.last);
    if (it != sd.laneNames.end())
        out.toward = IdsText(it->second);
    // the ship's server position: on a lane the client's copy (Pilot::pos) lags and jumps (the progress read 0.10 ->
    // 0.05 -> 0.20 -> 0.15 and the ETA half the ride's real length, 25 Oct 2026); the server moves it smoothly
    float me[3];
    if (!space::ObjectPos(space::PlayerShipId(), me))
        memcpy(me, p.pos, sizeof(me));
    // along the lane, not straight to the exit ring: the game advances the exit ring a little after the ship has passed
    // it (a straight distance then grew: the progress stepped back 0.33 -> 0.25 at every ring); a ring behind the ship
    // no longer counts
    const float* r0 = lc.pos[0].v;
    const float along = (r0[0] - me[0]) * lc.dir[0] + (r0[1] - me[1]) * lc.dir[1] + (r0[2] - me[2]) * lc.dir[2];
    const float left = std::max(0.0f, along + lc.tail);
    if (along < 0 && out.ringsLeft > 1)
        --out.ringsLeft;
    // the ride's length and ring count, fixed at its first full read (the entry ring to the last)
    if (g_edges.laneRings <= 0)
    {
        float entry[3];
        g_edges.laneRings = out.ringsLeft;
        g_edges.laneTotal = space::ObjectPos(lc.entry, entry) ? Dist(entry, lc.pos[0].v) + lc.tail : left;
    }
    out.ringsTotal = std::max(g_edges.laneRings, out.ringsLeft);
    if (g_edges.laneTotal > 0)
        out.progress = std::clamp(1.0f - left / g_edges.laneTotal, 0.0f, 1.0f);
    // the ETA on the ride's top speed: the lane moves the ship at one speed (2498 m/s measured) and slows it only into
    // the last ring, where the position-delta speed falls and an ETA from it jumps up (read 8 s -> 20 s -> 2 s). The
    // first second of a ride is left out of the top speed (an entry by teleport would read as a jump in position).
    const float speed = PositionSpeed(me);
    if (Now() - g_edges.laneStart > 1.0)
        g_edges.laneVmax = std::max(g_edges.laneVmax, speed);
    const float v = g_edges.laneVmax > 0 ? g_edges.laneVmax : speed;
    out.eta = v > 50 ? left / v : -1;
    return true;
}

// A jump gate's or hole's object (CSimple::get_type JumpGate 1 << 6, JumpHole 1 << 11)
static bool IsJumpObject(u32 id)
{
    u32 type = 0, cls = 0;
    return id && ObjectType(id, type, cls) && (type & ((1u << 6) | (1u << 11))) != 0;
}

// A jump (verified 25 Oct 2026, SaveC703, New York -> California): the player docks with the gate - the dock request,
// the queue, the grant and the approach are an ordinary dock (Docking() reports them) - then the docking sequence starts
// (the port held, control excluded: the jump_out effect), the ship enters the tunnel (the port still held), the system
// changes, and CShip::is_jumping reads true only for the jump-in at the far end. The jump runs from the sequence's start
// for the gate's jump_effect times (space::JumpSeconds), the destination fixed at its start (the gate is not in the new
// system).
bool Jumping(Jump& out)
{
    out = Jump();
    space::Pilot p;
    if (!space::ReadPilot(p))
    {
        g_edges.jumping = false;
        return false;
    }
    u32 port = 0;
    int index = -1;
    const bool held = ai::DockPort(port, index) && port;
    const u32 dockTarget = held ? port : (p.op == ai::OpDock ? p.target : 0);
    // the player's own dock maneuver starts it (a load into space also reads a held port and is_jumping for ~10 s:
    // that is the jump-in alone)
    const bool sequence = dockTarget && (p.op == ai::OpDock || g_edges.jumping) && IsJumpObject(dockTarget) &&
                          (held || space::PlayerControlExclusion() == 4);
    if (sequence && !g_edges.jumping)
    {
        g_edges.jumpStart = Now();
        g_edges.jumpGate = dockTarget;
        g_edges.jumpTotal = space::JumpSeconds(dockTarget);
        g_edges.jumpToward = SystemNameOf(JumpTargetSystem(dockTarget));
    }
    const double since = Now() - g_edges.jumpStart;
    const bool on = sequence || p.jumping ||
                    (g_edges.jumping && g_edges.jumpTotal > 0 && since < g_edges.jumpTotal); // the tunnel, the jump-in
    const bool wasJumping = g_edges.jumping;
    g_edges.jumping = on && (sequence || g_edges.jumping); // an is_jumping alone (no sequence seen): jump-in only
    // after the jump-in the ship still holds the far gate's port in a dock maneuver for a few seconds (read as "dock
    // granted" at the arrival, 25 Oct 2026): that tail belongs to the jump, until the maneuver lets go
    if (wasJumping && !g_edges.jumping)
        g_edges.jumpTail = true;
    if (!held && p.op != ai::OpDock)
        g_edges.jumpTail = false;
    if (!on)
        return false;
    if (g_edges.jumping)
    {
        if (g_edges.jumpTotal > 0)
            out.secondsLeft = std::max(0.0f, g_edges.jumpTotal - static_cast<float>(since));
        out.toward = g_edges.jumpToward;
    }
    else if (g_edges.jumpTail)
        out.secondsLeft = 0, out.toward = g_edges.jumpToward; // the jump-in outlasting the effect times by a moment
    else
        out.toward = SystemName(); // the jump-in seen without its start (a load into it): where the ship arrives
    return true;
}

void SetDockReply(u32 seq, u32 kind, u32 response)
{
    if (!g_dock.fed)
    {
        g_dock.fed = true, g_dock.seq = seq; // answers from before the first call are history
        return;
    }
    if (seq == g_dock.seq)
        return;
    g_dock.seq = seq;
    if (kind == 0)
        g_dock.response = static_cast<int>(response);
}

bool Docking(Dock& out)
{
    out = Dock();
    space::Pilot p;
    if (!space::ReadPilot(p))
        return false;
    u32 port = 0;
    int index = -1;
    const bool held = ai::DockPort(port, index) && port;
    if (p.op != ai::OpDock && !held)
    {
        g_dock.response = -1; // the next dock starts without an answer
        return false;
    }
    if (p.lane || p.jumping)
        return false; // the dock maneuver's lane ride / jump: Lane() / Jumping()
    Jump j;
    if (Jumping(j) || g_edges.jumpTail)
        return false; // a gate's docking sequence, tunnel and arrival are the jump
    if (space::PlayerControlExclusion() == 4)
        out.state = 4;
    else if (held || g_dock.response == 4)
        out.state = 3;
    else if (g_dock.response == 3)
        out.state = 2;
    else
        out.state = 1;
    if (held && index >= 0)
        out.bay = index;
    return true;
}

bool CruiseDisrupted(float withinSeconds)
{
    const Fns& f = F();
    // the lane: a ride that ended short of its last ring (sampled by Lane())
    LaneRun run;
    Lane(run);
    if (g_edges.laneEnded >= 0 && Now() - g_edges.laneEnded <= withinSeconds)
        return true;
    // the cruise engine: EngageCruise(false, disrupted) sets the charge clock to -CRUISE_DISRUPT_TIME; it counts up to 0
    void* e = PlayerEngine();
    if (!e || !f.disruptTime)
        return false;
    const float elapsed = Guard(f.cruiseElapsed, e, 0.0f);
    const float t = ReadF32(reinterpret_cast<uintptr_t>(f.disruptTime), 0);
    return elapsed < 0 && t + elapsed <= withinSeconds;
}

// ================================================================================================ combat
float MissileLock()
{
    space::Pilot p;
    if (!space::ReadPilot(p) || !p.target)
        return -1;
    // a missile lock is on a ship only: the guns' lock clock runs on any target (a station read 1), the game's seekers
    // home on ships (Munition seeker = LOCK)
    u32 type = 0, cls = 0;
    if (!ObjectType(p.target, type, cls) || cls != kClassShip)
        return -1;
    void* ship = PlayerShip();
    if (!ship)
        return -1;
    float best = -1;
    for (const space::Weapon& w : space::PlayerWeapons())
        if (w.homing && w.active && w.ammo != 0)
            best = std::max(best, GunLock(EquipById(ship, w.subObj), w.projArch));
    return best;
}

bool PlayerFormation(Formation& out)
{
    out = Formation();
    const Fns& f = F();
    space::Pilot p;
    if (!space::ReadPilot(p) || !f.followers || !f.followerCount)
        return false;
    u32 ids[16];
    int n = 0;
    if (p.leader)
    {
        void* leader = FindShip(p.leader);
        if (IsShip(leader))
            n = FollowersOf(leader, ids, 16);
        Release(leader);
        out.leader = p.leader;
    }
    else
    {
        n = FollowersOf(PlayerShip(), ids, 16); // the player leads a wing
        if (n <= 0)
            return false;
        out.leader = space::PlayerShipId();
    }
    out.members.assign(ids, ids + std::max(0, n));
    return true;
}

// ================================================================================================ missions
// Freelancer shows no mission countdown: the story's timers are Cnd_Timer conditions the player never sees
// (docs\FREELANCER_RE.md "HUD 2 readers").
float MissionTimer() { return -1; }

// ================================================================================================ debug
std::string Status()
{
    char b[320];
    std::ostringstream o;
    auto u8s = [](const std::wstring& w) { return text::Utf8(w); };
    o << "group " << ActiveWeaponGroup() << "\n";
    for (const WeaponInfo& w : Weapons())
    {
        sprintf_s(b, "  weapon %u \"%s\" groups=0x%X launcher=%d ammo=%d\n", w.arch, u8s(w.name).c_str(), w.groups,
                  w.launcher ? 1 : 0, w.ammo);
        o << b;
    }
    o << "system \"" << u8s(SystemName()) << "\" zone \"" << u8s(ZoneName()) << "\"\n";
    space::Pilot p;
    const u32 target = space::ReadPilot(p) ? p.target : 0;
    sprintf_s(b, "target %u \"%s\" faction \"%s\" class \"%s\" level %d\n", target, u8s(DisplayName(target)).c_str(),
              u8s(FactionName(target)).c_str(), u8s(ClassName(target)).c_str(), Level(target));
    o << b;
    sprintf_s(b, "afterburner %d engine_killed %d credits %lld tractor %d loot_in_reach %d\n", AfterburnerOn() ? 1 : 0,
              EngineKilled() ? 1 : 0, Credits(), TractorActive() ? 1 : 0, LootInReach());
    o << b;
    LaneRun l;
    if (Lane(l))
        sprintf_s(b, "lane rings_left %d/%d toward \"%s\" eta %.1f progress %.2f\n", l.ringsLeft, l.ringsTotal,
                  u8s(l.toward).c_str(), l.eta, l.progress);
    else
        sprintf_s(b, "lane -\n");
    o << b;
    Jump j;
    if (Jumping(j))
        sprintf_s(b, "jump seconds_left %.1f toward \"%s\"\n", j.secondsLeft, u8s(j.toward).c_str());
    else
        sprintf_s(b, "jump -\n");
    o << b;
    Dock d;
    if (Docking(d))
        sprintf_s(b, "dock state %d queue %d bay %d (replies %s)\n", d.state, d.queue, d.bay,
                  g_dock.fed ? "fed" : "not fed");
    else
        sprintf_s(b, "dock - (replies %s)\n", g_dock.fed ? "fed" : "not fed");
    o << b;
    sprintf_s(b, "cruise_disrupted %d missile_lock %.2f mission_timer %.1f\n", CruiseDisrupted(3.0f) ? 1 : 0,
              MissileLock(), MissionTimer());
    o << b;
    Formation fm;
    if (PlayerFormation(fm))
    {
        o << "formation leader " << fm.leader << " members";
        for (u32 m : fm.members)
            o << " " << m;
        o << "\n";
    }
    else
        o << "formation -\n";
    return o.str();
}

namespace
{
// research dumps (POD-only helpers for the guarded parts)
int DumpWords(uintptr_t a, int from, int to, char* buf, size_t cap)
{
    size_t k = 0;
    for (int i = from; i < to && k + 24 < cap; i += 4)
    {
        const u32 v = ReadU32(a + i, 0xDEADDEAD);
        k += sprintf_s(buf + k, cap - k, "+%02X %08X%s", i, v, ((i - from) % 32) == 28 ? "\n" : " ");
    }
    return static_cast<int>(k);
}
u32 SystemVt3(const u8* s)
{
    typedef u32(__thiscall * IdsFn)(const void*);
    __try
    {
        return (*reinterpret_cast<IdsFn* const*>(s))[3](s);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0xFFFFFFFF;
    }
}
} // namespace

Probe& DebugProbe()
{
    static Probe p;
    return p;
}

std::string Research(const std::string& what)
{
    std::ostringstream o;
    char b[1024];
    const Fns& f = F();
    std::istringstream in(what);
    std::string op;
    in >> op;
    if (op == "probe")
    {
        Probe& p = DebugProbe();
        std::string m;
        in >> m;
        p = Probe();
        if (m != "off")
        {
            p.mode = atoi(m.c_str());
            for (float& v : p.v)
                in >> v;
        }
        sprintf_s(b, "probe mode %d (%g %g %g %g)", p.mode, p.v[0], p.v[1], p.v[2], p.v[3]);
        return b;
    }
    u32 arg = 0;
    in >> arg;
    if (op == "wg")
    {
        const uintptr_t mgr = exe::WeaponGroups.get();
        sprintf_s(b, "mgr 0x%X current0 %d\n", static_cast<u32>(mgr), CurrentGroup0());
        o << b;
        if (!mgr)
            return o.str();
        DumpWords(mgr, 0, 0x30, b, sizeof(b));
        o << b << "\n";
        const uintptr_t bg = ReadU32(mgr + kWgBegin), eg = ReadU32(mgr + kWgEnd);
        for (uintptr_t g = bg; g && g < eg && g - bg < 64; g += 4)
        {
            const uintptr_t gp = ReadU32(g);
            sprintf_s(b, "group@%08X: ", static_cast<u32>(gp));
            o << b;
            DumpWords(gp, 0, 0x14, b, sizeof(b));
            o << b << "\n";
            const uintptr_t first = ReadU32(gp + kGroupFirst), last = ReadU32(gp + kGroupLast);
            for (uintptr_t en = first; en && en < last && en - first < 0x14 * 16; en += kEntrySize)
            {
                o << "   entry ";
                DumpWords(en, 0, 0x14, b, sizeof(b));
                o << b;
                char hp[40] = {};
                const uintptr_t ptr = ReadU32(en + kEntryPtr);
                for (int i = 0; ptr && i < 39; ++i)
                {
                    const u32 w = ReadU32(ptr + i);
                    hp[i] = static_cast<char>(w & 0xFF);
                    if (!hp[i])
                        break;
                }
                o << " \"" << hp << "\"\n";
            }
        }
        HpBits rows[32];
        const int n = StoredGroups(rows, 32);
        o << "stored rows " << n << ":";
        for (int i = 0; i < n; ++i)
            o << " " << rows[i].hp << "=0x" << std::hex << rows[i].bits << std::dec;
        o << "\n";
        return o.str();
    }
    if (op == "engine")
    {
        void* raw = FirstEquip(PlayerShip(), kEquipEngine);
        void* e = PlayerEngine();
        sprintf_s(b, "engine %p (raw %p vt %08X)\n", e, raw, raw ? ReadU32(reinterpret_cast<uintptr_t>(raw)) : 0);
        o << b;
        if (!e)
            return o.str();
        sprintf_s(b, "active %d controller %d cruise %d elapsed %.2f disruptT %.2f thrusters %d\n",
                  Guard(f.equipActive, e, false) ? 1 : 0, Guard(f.engineEnabled, e, false) ? 1 : 0,
                  Guard(f.cruiseEngaged, e, false) ? 1 : 0, Guard(f.cruiseElapsed, e, 0.0f),
                  ReadF32(reinterpret_cast<uintptr_t>(f.disruptTime), -1), AfterburnerOn() ? 1 : 0);
        o << b;
        DumpWords(reinterpret_cast<uintptr_t>(e), 0, 0x60, b, sizeof(b));
        o << b;
        return o.str();
    }
    if (op == "obj") // obj <id>: type, rep, ranks, affiliation
    {
        u32 type = 0, cls = 0;
        const bool ok = ObjectType(arg, type, cls);
        const int rep = RepOf(arg);
        u32 group = 0, ids = 0;
        AffiliationIds(rep, group, ids, true);
        float rr = -1;
        int rrc = -9;
        if (rep && f.repRank)
            rrc = f.repRank(rep, rr);
        sprintf_s(b, "obj %u found %d class 0x%X type 0x%X rep %d group %u ids %u vibeRank %d repRank %.2f (rc %d)\n",
                  arg, ok ? 1 : 0, cls, type, rep, group, ids, VibeRank(rep), rr, rrc);
        o << b;
        return o.str();
    }
    if (op == "sys") // sys [id]: ISystem fields + the virtual the game's string formatter calls (vt[3])
    {
        const u32 id = arg ? arg : Mem<u32>(exe::PlayerSystem);
        const u8* s = f.getSystem ? f.getSystem(id) : nullptr;
        sprintf_s(b, "system %u at %p\n", id, s);
        o << b;
        if (!s)
            return o.str();
        DumpWords(reinterpret_cast<uintptr_t>(s), 0x40, 0x80, b, sizeof(b));
        o << b;
        sprintf_s(b, "vt[3] -> %u\n", SystemVt3(s));
        o << b;
        return o.str();
    }
    if (op == "equip") // equip <arch>: Root fields
    {
        const void* e = f.getEquipment ? f.getEquipment(arg) : nullptr;
        sprintf_s(b, "equip %u at %p ids %u\n", arg, e, EquipIds(arg));
        o << b;
        if (e)
            DumpWords(reinterpret_cast<uintptr_t>(e), 0, 0x20, b, sizeof(b)), o << b;
        return o.str();
    }
    if (op == "lock")
    {
        sprintf_s(b, "lock fields ttl +0x%X clock +0x%X\n", f.lockTimeField, f.lockClockField);
        o << b;
        void* ship = PlayerShip();
        for (const space::Weapon& w : space::PlayerWeapons())
        {
            void* eq = EquipById(ship, w.subObj);
            const bool gun = IsGun(eq);
            sprintf_s(b, "  sub %u arch %u hp %s gun %d guided %d till %.2f lock %.2f clock %.2f homing %d active %d ammo %d slot %d\n",
                      w.subObj, w.archId, w.hardpoint, gun ? 1 : 0, gun ? (Guard(f.gunGuided, eq, false) ? 1 : 0) : -1,
                      gun ? Guard(f.secondsTillLock, eq, -1.0f) : -1.0f, GunLock(eq, w.projArch),
                      gun && f.lockClockField ? ReadF32(reinterpret_cast<uintptr_t>(eq) + f.lockClockField, -1) : -1.0f,
                      w.homing ? 1 : 0, w.active ? 1 : 0, w.ammo, w.fireSlot);
            o << b;
        }
        return o.str();
    }
    if (op == "tractor")
    {
        void* t = FirstEquip(PlayerShip(), kEquipTractor);
        sprintf_s(b, "tractor %p class %d arms %d\n", t, IsTractor(t) ? 1 : 0, Guard(F().tractorArms, t, -1));
        o << b;
        return o.str();
    }
    return "usage: hudinfo [wg|engine|lock|tractor|obj <id>|sys [id]|equip <arch>]";
}
} // namespace hudinfo
