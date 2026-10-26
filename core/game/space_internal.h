// What the space_*.cpp files share (split from space.cpp on 24 Oct 2026): the Common.dll Api loader and the class
// accessors, the class ids and struct offsets, the guarded object reads and class checks, and the helpers one of the
// files defines for another. Private to core\game\space*.cpp: nothing outside core\game includes it; the one public
// header stays space.h.
#pragma once
#include "fldll.h"
#include "game/space.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace space
{
// CObject::Class values (FLSDK): the registry is searched per class.
constexpr u32 kClassSolar = 0x303;
constexpr u32 kClassShip = 0x503;
constexpr u32 kClassLoot = 0x803; // CLoot: a CSimple, not a CEqObj (no dock / targetable / CEqObj name)
constexpr u32 kObjSystem = 0x50;  // CObject +0x50: the system id the object is in

// CBeam (gun rounds: space_rounds.cpp)
constexpr u32 kClassBeam = 0x67;

struct Api
{
    void*(__cdecl* findFirst)(u32 cls) = nullptr;
    void*(__cdecl* findNext)() = nullptr;
    const u32*(__fastcall* getId)(void* self, void* edx) = nullptr;
    u32(__fastcall* getType)(void* self, void* edx) = nullptr;
    u32(__fastcall* getName)(void* self, void* edx) = nullptr;
    bool(__fastcall* isDock)(void* self, void* edx) = nullptr;
    bool(__fastcall* isTargetable)(void* self, void* edx) = nullptr;
    const u32*(__fastcall* dockTarget)(void* self, void* edx) = nullptr;
    int(__cdecl* getLocation)(u32 id, float* pos, float* rot) = nullptr; // pub::SpaceObj::GetLocation (server.dll)
    const u16* subobjNone = nullptr;
    bool ok = false;
    // CShip / CEqObj state for the autopilot readout (optional: missing ones read as 0)
    bool(__fastcall* usingLane)(void* self, void* edx) = nullptr;
    u32(__fastcall* laneEntry)(void* self, void* edx) = nullptr;
    u32(__fastcall* laneExit)(void* self, void* edx) = nullptr;
    bool(__fastcall* jumping)(void* self, void* edx) = nullptr;
    bool(__fastcall* cruise)(void* self, void* edx) = nullptr;
    void*(__fastcall* target)(void* self, void* edx) = nullptr;
    void*(__fastcall* behavior)(void* self, void* edx) = nullptr;
    // IBehaviorManager::get_docking_port(const IObjRW*& port, int& index): the dock port the ship's behaviour holds
    bool(__thiscall* dockingPort)(void* manager, const void** port, int* index) = nullptr;
    float(__fastcall* relHealth)(void* self, void* edx) = nullptr; // CSimple::get_relative_health (hull 0..1)
    // server.dll pub::SpaceObj::GetRep / pub::Reputation::GetAttitude (optional: attitude unknown without them)
    int(__cdecl* getRep)(u32 id, int& rep) = nullptr;
    int(__cdecl* getAttitude)(const int& from, const int& to, float& att) = nullptr;
    u32(__fastcall* lootName)(void* self, void* edx) = nullptr;                    // CLoot::get_name
    const void*(__fastcall* lootContents)(void* self, void* edx) = nullptr;        // CLoot::contents_arch
    u32(__fastcall* lootUnits)(void* self, void* edx) = nullptr;                   // CLoot::get_units
    u32(__fastcall* archId)(const void* arch, void* edx) = nullptr;                // Archetype::Root::get_id
    int(__fastcall* cargoFits)(void* ship, void* edx, const void* arch) = nullptr; // CShip::get_space_for_cargo_type
    float(__fastcall* maxHit)(void* self, void* edx) = nullptr;                    // CSimple::get_max_hit_pts
    u32(__cdecl* createId)(const char* nickname) = nullptr;                        // CreateID
    const void*(__cdecl* goodByArch)(u32 archetype) = nullptr; // GoodList::find_by_archetype (the loaded goods)
    // A ship's motion and agility (ShipMotionOf): PhySys::GetVelocity (Vector by value: hidden pointer first) and the
    // ship's archetype (CShip::shiparch)
    float*(__cdecl* velocity)(float* out, const void* cobject) = nullptr;
    const u8*(__fastcall* shipArch)(const void* ship, void* edx) = nullptr;
};

// Objects, equipment and archetypes of Common.dll: fields through its exported accessors where it has them
// (CObject::get_archetype, CProjectile::get_owner, CEquip::GetID / EquipArch, CELauncher::ProjectileArch - each the
// one-instruction read of the field), else by offset only after a class check against the class's exported vtable
// (hk::IsExportedClass): another build or a mod's class fails closed. 18 Oct 2026 (was: offsets only).
struct Accessors
{
    void*(__thiscall* getArchetype)(const void* obj) = nullptr;      // ?get_archetype@CObject@@QBEPAURoot@Archetype@@XZ
    const u32*(__thiscall* projOwner)(const void* proj) = nullptr;   // ?get_owner@CProjectile@@QBEABIXZ
    u16(__thiscall* equipId)(const void* equip) = nullptr;           // ?GetID@CEquip@@QBEGXZ (sub-object id)
    const void*(__thiscall* equipArch)(const void* equip) = nullptr; // ?EquipArch@CEquip@@QBEPBUEquipment@Archetype@@XZ
    const void*(__thiscall* projArch)(const void* launcher) = nullptr; // ?ProjectileArch@CELauncher@@...
};

// CGuided: Archetype::Munition hull_damage +0x80, seeker +0x90, cruise_disruptor +0xAC (FLSDK; behind the class
// check); get_target exported by Common.dll.
constexpr u32 kClassGuided = 0xA7;
constexpr u32 kMunitionHullDamage = 0x80, kMunitionSeeker = 0x90, kMunitionCruiseDisruptor = 0xAC;
// FLSDK Archetype::Projectile lifeTime +0x70; Munition motorId +0x8C, seekerRange +0x98, seekerFovDeg +0x9C,
// maxAngularVelocity +0xA0; MotorData (Common.dll ?GetMotor@Archetype@@YAPBUMotorData@1@I@Z) delay +8, accel +0xC,
// lifetime +0x10.
constexpr u32 kProjLifetime = 0x70, kMunitionMotor = 0x8C, kMunitionSeekerRange = 0x98, kMunitionSeekerFov = 0x9C,
              kMunitionMaxTurn = 0xA0, kMotorDelay = 0x8, kMotorAccel = 0xC, kMotorLifetime = 0x10;

// Equipment (FLSDK CEquip / CELauncher / Archetype::Launcher): CEqObj equipManager +0xE4 (the ship is checked to be a
// CShip); CEquip isActive +0x10, CELauncher refireDelayElapsed +0x50 (no accessor: behind the launcher class check);
// Launcher power_usage +0x8C, refire_delay +0x90, muzzle_velocity +0x94, Archetype::Root id +0x4 (behind the
// launcher archetype check). Sub-object id, archetype and projectile archetype through the exported accessors.
constexpr u32 kEqObjEquipManager = 0xE4, kEquipActive = 0x10, kLauncherElapsed = 0x50, kArchPower = 0x8C,
              kArchRefire = 0x90, kArchMuzzle = 0x94, kArchRootId = 0x4, kExternalHardpoint = 0x20;
constexpr int kEquipClassGun = 1 << 7;

struct WaypointEntry // 24 bytes
{
    float pos[3];
    u32 system, object, seq;
};
static_assert(sizeof(WaypointEntry) == 24, "waypoint list entry");

// The zones of the player's system along a straight segment, from the game's own zone geometry
// (server.dll pub::System::InZones at points every `step` metres). Each entry: zone id (CreateID of the zone's
// nickname) and the sample index ranges it covers.
struct ZoneSpan
{
    u32 zone = 0;
    std::vector<std::pair<int, int>> ranges;
};

struct LootProps
{
    float chance = 0; // fraction
    int minWorth = 0, worthMult = 0, min = 0, max1 = 0, max2 = 0;
};

// ---- the per-frame memo (space.h BeginFrame): a memo's stamp is valid while it equals FrameStamp(); 0 = the memo is
// off (no frame begun, or after EndFrame) - never valid. InvalidateFrame() moves the stamp on.
u32 FrameStamp();
template <class T> struct FrameMemo
{
    u32 stamp = 0;
    T value{};
    bool Valid() const { return stamp != 0 && stamp == FrameStamp(); }
    // Keeps a fresh scan (stamped with this frame; with the memo off the stamp is 0: held, never reused).
    const T& Fill(T&& v)
    {
        value = std::move(v);
        stamp = FrameStamp();
        return value;
    }
    void Drop() { stamp = 0; }
};

// ---- defined in space.cpp (the Api loader, the guarded reads and class checks every file uses)
const Api& GetApi();
const Accessors& Acc();
void* PlayerIObj();
bool InSpace();
void* PlayerCShip();
extern int g_lastLocation; // pub::SpaceObj::GetLocation's last result code (diagnostics)
bool Location(const Api& a, u32 id, float pos[3]);
void Collect(const Api& a, u32 cls, u32 system, std::vector<void*>& out);
int CountClass(const Api& a, u32 cls);
bool IsClass(const void* o, const char* vt);
bool IsMunitionArch(const void* o);
bool IsLauncherArch(const void* o);
bool IsLauncherEquip(const void* o);
u32 ObjectId(const Api& a, void* o);
u32 ObjId(void* iobj);
std::string DataPath(const char* rel);

// ---- space_ship.cpp
float GamePrice(const Api& a, u32 archetype);
const std::unordered_map<u32, LootProps>& LootTable();
u32 PlayerShipArch();

// ---- space_rounds.cpp
void MunitionDamage(uintptr_t munition, float& damage, float& radius);
bool ShipExtents(void* ship, float mn[3], float mx[3]);

// ---- space_weapons.cpp
int ReadWeapons(void* ship, Weapon* out, int max, int equipClass = kEquipClassGun);
extern bool g_fireSlotsHud; // the last numbering came from the HUD list (debug `weapons`)

// ---- space_nav.cpp
WaypointEntry* WaypointAt(int i);

// ---- space_universe.cpp
std::string SystemFile(u32 system);
// (Obstacles() passes the default rules)
void AppendLethalZones(u32 system, const float center[3], float range, std::vector<Obstacle>& out,
                       const ZoneRules& rules = {});
bool ZonesAlong(const float a[3], const float b[3], float step, int& samples, std::vector<ZoneSpan>& out);

// ---- space_dock.cpp
void* TargetIObj();
int CallDockPoints(dll::ObjDockPointsFn dock, void* target, int index, int* type, float (*t)[32], float* radius);
int CallHardpoint(dll::ObjHardpointFn fn, void* obj, const char* name, float* pos, float* rot);
int CallLaneStart(dll::ObjLaneStartFn fn, void* self, void* other, const void** next, float* start, float* dir);
} // namespace space
