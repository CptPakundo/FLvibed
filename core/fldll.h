// Virtual functions of the game's DLL interfaces that FLvibed calls, by name (hk::Virtual, core\hook.h): the slot is
// found in the game's own exported vtable, so no slot number is hard-coded and every Common.dll build that exports
// these names works (vanilla v1.0, HD Edition 0.7.1 and the current HDE repository export the same names, checked
// 18 Oct 2026; the bytes of the three files differ). The call types are written from the mangled names: `UBE` =
// virtual const thiscall; `?AV<class>` = a class returned by value (a hidden pointer argument, returned again);
// `AB` = a const reference; `H` int, `M` float, `I` unsigned int, `AA` a reference argument, `PB`/`PA` pointers.
// Use: `if (auto f = dll::ObjGetId.get<dll::ObjGetIdFn>(iobj)) id = f(iobj);` - null when unknown (fail closed).
#pragma once
#include "hook.h"

namespace dll
{
constexpr const char* kCommon = "common.dll";
// IObjInspect, implemented by IObjInspectImpl (the inspect interface of every space object: GetPlayerIObj, targets)
constexpr const char* kVtIObjInspect = "??_7IObjInspectImpl@@6BIObjInspect@@@";

// const Vector& get_position() const
typedef const float*(__thiscall* ObjRefFn)(const void* self);
inline hk::Virtual ObjGetPosition{kCommon, kVtIObjInspect, "?get_position@IObjInspectImpl@@UBEABVVector@@XZ"};
// const Matrix& get_orientation() const
inline hk::Virtual ObjGetOrientation{kCommon, kVtIObjInspect, "?get_orientation@IObjInspectImpl@@UBEABVMatrix@@XZ"};
// Vector get_velocity() const / get_angular_velocity() const (by value: hidden pointer)
typedef float*(__thiscall* ObjVectorFn)(const void* self, float* out);
inline hk::Virtual ObjGetVelocity{kCommon, kVtIObjInspect, "?get_velocity@IObjInspectImpl@@UBE?AVVector@@XZ"};
inline hk::Virtual ObjGetAngularVelocity{kCommon, kVtIObjInspect,
                                         "?get_angular_velocity@IObjInspectImpl@@UBE?AVVector@@XZ"};
// const unsigned int get_id() const
typedef u32(__thiscall* ObjGetIdFn)(const void* self);
inline hk::Virtual ObjGetId{kCommon, kVtIObjInspect, "?get_id@IObjInspectImpl@@UBE?BIXZ"};
// const CObject* cobject() const
typedef void*(__thiscall* ObjCobjectFn)(const void* self);
inline hk::Virtual ObjCobject{kCommon, kVtIObjInspect, "?cobject@IObjInspectImpl@@UBEPBUCObject@@XZ"};
// int get_target(IObjRW*&) const / int get_formation_leader(IObjRW*&) const
typedef int(__thiscall* ObjGetObjFn)(const void* self, void** out);
inline hk::Virtual ObjGetTarget{kCommon, kVtIObjInspect, "?get_target@IObjInspectImpl@@UBEHAAPAUIObjRW@@@Z"};
inline hk::Virtual ObjGetFormationLeader{kCommon, kVtIObjInspect,
                                         "?get_formation_leader@IObjInspectImpl@@UBEHAAPAUIObjRW@@@Z"};
// int get_attitude_towards(float&, const IObjInspect*) const
typedef int(__thiscall* ObjAttitudeFn)(const void* self, float* out, const void* other);
inline hk::Virtual ObjGetAttitudeTowards{kCommon, kVtIObjInspect,
                                         "?get_attitude_towards@IObjInspectImpl@@UBEHAAMPBUIObjInspect@@@Z"};
// int get_physical_radius(float&, Vector&) const
typedef int(__thiscall* ObjRadiusFn)(const void* self, float* radius, float* center);
inline hk::Virtual ObjGetPhysicalRadius{kCommon, kVtIObjInspect,
                                        "?get_physical_radius@IObjInspectImpl@@UBEHAAMAAVVector@@@Z"};

// int get_dock_hardpoints(int dockIndex, TERMINAL_TYPE*, Transform* dockMount, Transform* dockPoint1,
//                         Transform* dockPoint2, float* dockRadius) const - the docking geometry of a dockable object
// (FLHookSDK IObj/Base.hpp); Transform layout found in game (research: debug `dockpoints`).
typedef int(__thiscall* ObjDockPointsFn)(const void* self, int index, int* type, float* mount, float* point1,
                                         float* point2, float* radius);
inline hk::Virtual ObjGetDockHardpoints{
    kCommon, kVtIObjInspect, "?get_dock_hardpoints@IObjInspectImpl@@UBEHHPAW4TERMINAL_TYPE@@PAVTransform@@11PAM@Z"};

// int get_hardpoint(const char* name, Vector* pos, Matrix* rot) const - a hardpoint of the object's model.
typedef int(__thiscall* ObjHardpointFn)(const void* self, const char* name, float* pos, float* rot);
inline hk::Virtual ObjGetHardpoint{kCommon, kVtIObjInspect,
                                   "?get_hardpoint@IObjInspectImpl@@UBEHPBDPAVVector@@PAVMatrix@@@Z"};
// int get_lane_start(const IObjInspect* other, const IObjInspect*& next, Vector& start, Vector& dir) const - where a lane
// ride from a ring starts (the ring's HpLeftLane / HpRightLane, by direction: Common.dll RVA 0x62430). Research.
typedef int(__thiscall* ObjLaneStartFn)(const void* self, const void* other, const void** next, float* start,
                                        float* dir);
inline hk::Virtual ObjGetLaneStart{kCommon, kVtIObjInspect,
                                   "?get_lane_start@IObjInspectImpl@@UBEHPBUIObjInspect@@AAPBU2@AAVVector@@2@Z"};

// int get_lane_direction(const Transform& ship, bool* out) const on a lane ring - the game's own test of which way a ride
// from there goes: the sign of (ring - ship position) . the ring's z axis (Common.dll RVA 0x60E30).
typedef int(__thiscall* ObjLaneDirectionFn)(const void* self, const float* transform, bool* out);
inline hk::Virtual ObjGetLaneDirection{kCommon, kVtIObjInspect,
                                       "?get_lane_direction@IObjInspectImpl@@UBEHABVTransform@@PA_N@Z"};

// CELauncher (guns and launchers): int GetAmmoCount() const
typedef int(__thiscall* LauncherAmmoFn)(const void* self);
inline hk::Virtual LauncherGetAmmoCount{kCommon, "??_7CELauncher@@6B@", "?GetAmmoCount@CELauncher@@UBEHXZ"};

// IBehaviorManager (the ship's AI: CEqObj::get_behavior_interface) has no exported vtable; its field offsets are read
// from Common.dll's own exported accessors (hk::ExportOperand: the bytes must match), once per process. 0 = unknown.
struct BehaviorFields
{
    u32 op = 0;         // pub::AI::OpType of the running behaviour (-1 none): cancel_behavior `mov eax,[ecx+op]`
    u32 behaviours = 0; // behaviour objects by op (op * 4): cancel_behavior `mov ecx,[ecx+eax*4+behaviours]`
    u32 locked = 0;     // maneuvers locked (u8): lock_maneuvers `mov [ecx+locked],al`
    u32 priority = 0;   // directive priority: set_directive_priority `mov [ecx+priority],eax`
    bool ok() const { return op && behaviours && locked && priority; }
};
inline const BehaviorFields& Behavior()
{
    static BehaviorFields f;
    static bool tried = false;
    if (!tried && GetModuleHandleA(kCommon))
    {
        tried = true;
        f.op = hk::ExportOperand(kCommon, "?cancel_behavior@IBehaviorManager@@QAE_NW4CancelRequestType@@@Z",
                                 "8B 81 ?? ?? ?? ?? 83 F8 FF 74 ?? 8B 8C 81 ?? ?? ?? ??", 2);
        f.behaviours = hk::ExportOperand(kCommon, "?cancel_behavior@IBehaviorManager@@QAE_NW4CancelRequestType@@@Z",
                                         "8B 81 ?? ?? ?? ?? 83 F8 FF 74 ?? 8B 8C 81 ?? ?? ?? ??", 14);
        f.locked = hk::ExportOperand(kCommon, "?lock_maneuvers@IBehaviorManager@@QAE_N_N@Z",
                                     "8A 44 24 04 88 81 ?? ?? ?? ?? B0 01 C2 04 00", 6);
        f.priority = hk::ExportOperand(
            kCommon, "?set_directive_priority@IBehaviorManager@@QAE?AW4OP_RTYPE@AI@pub@@W4DirectivePriority@34@@Z",
            "8B 81 ?? ?? ?? ?? 85 C0 74 ?? 8B 44 24 04 89 81 ?? ?? ?? ??", 16);
    }
    return f;
}

// Debug: every virtual above with its slot (-1 = unknown), one per line.
inline std::string Slots()
{
    hk::Virtual* all[] = {&ObjGetPosition,
                          &ObjGetOrientation,
                          &ObjGetVelocity,
                          &ObjGetAngularVelocity,
                          &ObjGetId,
                          &ObjCobject,
                          &ObjGetTarget,
                          &ObjGetFormationLeader,
                          &ObjGetAttitudeTowards,
                          &ObjGetPhysicalRadius,
                          &ObjGetDockHardpoints,
                          &ObjGetHardpoint,
                          &ObjGetLaneStart,
                          &ObjGetLaneDirection,
                          &LauncherGetAmmoCount};
    std::string out;
    for (hk::Virtual* v : all)
        out += std::to_string(v->Slot()) + " " + v->method + "\n";
    const BehaviorFields& b = Behavior();
    char line[160];
    sprintf_s(line, "IBehaviorManager fields: op +0x%X behaviours +0x%X locked +0x%X priority +0x%X\n", b.op,
              b.behaviours, b.locked, b.priority);
    return out + line;
}
} // namespace dll
