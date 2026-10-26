// Lanes, docking and jumps: the jump effect's timing, the mission's lane permission, lane links, the dock range and
// the current target's docking geometry through IObjInspect (bays, body, hardpoints, lane start / side). Split from
// space.cpp on 24 Oct 2026 (a file move: the functions are unchanged).
#include "game/space_internal.h"

#include "fldll.h"
#include "flexe.h"
#include "game/ini.h"
#include "log.h"

#include <unordered_map>

namespace space
{
float JumpSeconds(u32 gate)
{
    static std::unordered_map<u32, float> cache;
    const auto found = cache.find(gate);
    if (found != cache.end())
        return found->second;
    float& secs = cache[gate];
    secs = 0;
    const Api& a = GetApi();
    if (!a.createId || !InSpace())
        return secs;
    // The gate's [Object] jump_effect in its system file ...
    std::string effect;
    const std::string file = SystemFile(Mem<u32>(exe::PlayerSystem));
    for (const ini::Section& s : ini::Read(DataPath(("UNIVERSE\\" + file).c_str())))
        if (s.type == "object")
            if (const std::string* n = s.Get("nickname"); n && a.createId(n->c_str()) == gate)
                if (const std::string* e = s.Get("jump_effect"))
                    effect = *e;
    // ... and that [JumpGateEffect] in the file freelancer.ini's [Data] jump_effect names (fx\jumpeffect.ini).
    std::string effects = "FX\\jumpeffect.ini";
    {
        char exe[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, exe, MAX_PATH);
        if (char* slash = strrchr(exe, '\\'))
            strcpy_s(slash + 1, MAX_PATH - (slash + 1 - exe), "freelancer.ini");
        for (const ini::Section& s : ini::Read(exe))
            if (s.type == "data")
                if (const std::string* j = s.Get("jump_effect"))
                    effects = *j;
    }
    for (const ini::Section& s : ini::Read(DataPath(effects.c_str())))
    {
        const std::string* n = s.Get("nickname");
        if (s.type != "jumpgateeffect" || !n || (!effect.empty() && _stricmp(n->c_str(), effect.c_str()) != 0))
            continue;
        for (const char* k : {"jump_out_time", "jump_out_tunnel_time", "jump_in_tunnel_time", "jump_in_time"})
            if (const std::string* v = s.Get(k))
                secs += static_cast<float>(atof(v->c_str()));
        if (!effect.empty() || secs > 0)
            break; // the named effect (or, unnamed, the first one: the game's default)
    }
    return secs;
}

// The mission's own lane permission (Content.dll; docs\FREELANCER_RE.md "Lane permission"): the game's lane request
// handler (found by its signature) looks up the mission's per-player state by ship id and asks it about (ring,
// toward ring); 1 or 2 = it answers the request "denied". Both helpers are taken from the handler's own call
// instructions, so they follow the DLL build. Content.dll is reloaded after a death: re-resolved per module base.
int LaneOpen(u32 ring, u32 toward)
{
    typedef void*(__cdecl * StateFn)(const u32*);
    typedef int(__thiscall * CheckFn)(void*, u32, u32);
    static HMODULE base = nullptr;
    static StateFn stateOf = nullptr;
    static CheckFn check = nullptr;
    const HMODULE m = GetModuleHandleA("content.dll");
    if (!m || !ring || !InSpace())
        return -1;
    if (m != base)
    {
        base = m;
        stateOf = nullptr, check = nullptr;
        // 06F6FCB0 in HDE 0.7.1: sub esp,20h ... call <mission state>(esi) ... call <check>(ring, toward)
        const uintptr_t h = hk::FindInModule(
            "content.dll",
            "83 EC 20 55 56 8B 74 24 2C 8B 4E 04 57 8D 44 24 30 8D 6E 04 50 51 FF 15 ?? ?? ?? ?? 8A 44 24 38 83 C4 08 "
            "84 C0 BF 05 00 00 00 0F 89 ?? ?? ?? ?? 56 E8 ?? ?? ?? ?? 83 C4 04 85 C0 74 ?? 8B 56 08 8B 4D 00 52 51 8B "
            "C8 E8 ?? ?? ?? ?? 83 F8 01",
            0);
        if (h)
        {
            stateOf = reinterpret_cast<StateFn>(h + 0x31 + 5 + Mem<i32>(h + 0x32));
            check = reinterpret_cast<CheckFn>(h + 0x47 + 5 + Mem<i32>(h + 0x48));
        }
        else
            Log("space: the lane request handler was not found in content.dll: lane permission unknown");
    }
    if (!stateOf || !check)
        return -1;
    __try
    {
        const u32 ship = PlayerShipId();
        if (!ship)
            return -1;
        void* state = stateOf(&ship);
        if (!state)
            return 1; // no mission state for the player: the handler grants
        const int r = check(state, ring, toward);
        return r == 1 || r == 2 ? 0 : 1;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return -1;
    }
}

bool LaneLinks(u32 ring, u32& next, u32& prev)
{
    typedef int(__cdecl * LinksFn)(const u32&, u32&, u32&);
    static LinksFn fn = reinterpret_cast<LinksFn>(
        GetProcAddress(GetModuleHandleA("server.dll"), "?GetTradelaneNextAndPrev@SpaceObj@pub@@YAHABIAAI1@Z"));
    next = prev = 0;
    if (!fn || !InSpace())
        return false;
    __try
    {
        // (ring, next, prev): order verified in game (Li01 ring 177: next 178, prev 176 - debug `lanelinks`)
        const int rc = fn(ring, next, prev);
        return rc == 0 && (next || prev);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

float DockRange()
{
    // Common.dll compares the squared distance of a dock request with a float (HD Edition 0.7.1 patched it to 2.5e9:
    // 50 km, vanilla 10 km): found by the comparison's code, `fcomp dword ptr [range]` after the squared distance (one
    // match in vanilla v1.0, HDE 0.7.1 and the current HDE repository; the float sits at +0x19B518 in vanilla and
    // +0x19C518 in HDE: was a fixed +0x19C518, 18 Oct 2026). Searched once, when Common.dll is loaded.
    static bool searched = false;
    static uintptr_t range = 0;
    if (!searched && GetModuleHandleA("common.dll"))
    {
        searched = true;
        range = hk::FindModuleData(
            "common.dll",
            "D9 00 D8 26 D9 40 04 D8 66 04 D9 40 08 D8 66 08 D9 C2 D8 CB D9 C2 D8 CB DE C1 D9 C1 D8 CA DE C1 D8 1D "
            "?? ?? ?? ?? DD D8 DF E0 DD D8 F6 C4 05",
            34);
    }
    if (!range)
        return 0;
    __try
    {
        const float sq = Mem<float>(range);
        return sq > 0 ? std::sqrt(sq) : 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
}

void* TargetIObj()
{
    void* me = PlayerIObj();
    void* target = nullptr;
    if (const auto getTarget = dll::ObjGetTarget.get<dll::ObjGetObjFn>(me))
        getTarget(me, &target);
    return target;
}

// Transform (Common.dll): 3x3 rotation, then the position.
constexpr int kTransformPos = 9;

std::vector<DockBay> TargetDockBays()
{
    std::vector<DockBay> out;
    void* target = TargetIObj();
    const auto dock = dll::ObjGetDockHardpoints.get<dll::ObjDockPointsFn>(target);
    if (!target || !dock)
        return out;
    // The game returns 0 for a bay it has; a negative code past the last one (or for objects without bays).
    for (int i = 0; i < 16; ++i)
    {
        float t[3][32] = {}, radius = -1;
        int type = -1;
        if (CallDockPoints(dock, target, i, &type, t, &radius) != 0)
            break;
        DockBay b;
        b.index = i;
        b.type = type;
        b.radius = radius;
        memcpy(b.mount, &t[0][kTransformPos], sizeof(b.mount));
        memcpy(b.p1, &t[1][kTransformPos], sizeof(b.p1));
        memcpy(b.p2, &t[2][kTransformPos], sizeof(b.p2));
        out.push_back(b);
    }
    return out;
}

static bool CallRadius(dll::ObjRadiusFn fn, void* obj, float* radius, float* center)
{
    __try
    {
        fn(obj, radius, center); // (its return code is not documented; the radius tells)
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool TargetBody(float center[3], float& radius)
{
    void* target = TargetIObj();
    const auto fn = dll::ObjGetPhysicalRadius.get<dll::ObjRadiusFn>(target);
    radius = 0;
    return target && fn && CallRadius(fn, target, &radius, center) && radius > 0;
}

// get_dock_hardpoints, guarded (research: three transforms into generous buffers, -998 on a fault).
int CallDockPoints(dll::ObjDockPointsFn dock, void* target, int index, int* type, float (*t)[32], float* radius)
{
    __try
    {
        return dock(target, index, type, t[0], t[1], t[2], radius);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return -998;
    }
}

int CallHardpoint(dll::ObjHardpointFn fn, void* obj, const char* name, float* pos, float* rot)
{
    __try
    {
        return fn(obj, name, pos, rot);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return -998;
    }
}

bool TargetHardpoint(const char* name, float pos[3])
{
    void* target = TargetIObj();
    const auto hp = target ? dll::ObjGetHardpoint.get<dll::ObjHardpointFn>(target) : nullptr;
    float rot[16] = {};
    return hp && CallHardpoint(hp, target, name, pos, rot) == 0;
}

int CallLaneStart(dll::ObjLaneStartFn fn, void* self, void* other, const void** next, float* start, float* dir)
{
    __try
    {
        return fn(self, other, next, start, dir);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return -998;
    }
}

bool TargetLaneStart(float start[3], float dir[3], u32& next)
{
    void* me = PlayerIObj();
    void* ring = TargetIObj();
    const auto ls = dll::ObjGetLaneStart.get<dll::ObjLaneStartFn>(me);
    if (!me || !ring || !ls)
        return false;
    const void* n = nullptr;
    if (CallLaneStart(ls, me, ring, &n, start, dir) != 0 || !n)
        return false;
    next = ObjId(const_cast<void*>(n));
    return std::fabs(dir[0]) + std::fabs(dir[1]) + std::fabs(dir[2]) > 0.5f;
}

static int CallLaneDirection(dll::ObjLaneDirectionFn fn, void* ring, const float* transform)
{
    __try
    {
        bool out = false;
        return fn(ring, transform, &out) == 0 ? (out ? 1 : 0) : -1;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return -1;
    }
}

int TargetLaneSide()
{
    void* me = PlayerIObj();
    void* ring = TargetIObj();
    const auto fn = dll::ObjGetLaneDirection.get<dll::ObjLaneDirectionFn>(ring);
    const auto rot = dll::ObjGetOrientation.get<dll::ObjRefFn>(me);
    const auto pos = dll::ObjGetPosition.get<dll::ObjRefFn>(me);
    if (!me || !ring || !fn || !rot || !pos)
        return -1;
    float t[12] = {}; // Transform: the 3x3 rotation, then the position
    __try
    {
        memcpy(t, rot(me), 9 * sizeof(float));
        memcpy(t + 9, pos(me), 3 * sizeof(float));
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return -1;
    }
    return CallLaneDirection(fn, ring, t);
}

bool TargetLaneStartToward(u32 toward, float start[3], float dir[3], int& side)
{
    u32 next = 0;
    if (!TargetLaneStart(start, dir, next))
        return false;
    // get_lane_start answered for the side the ship is on now: that side value means this direction.
    side = TargetLaneSide();
    if (side < 0)
        return false;
    if (!toward || next == toward)
        return true;
    side = 1 - side;
    // The other way: the other lane hardpoint, the direction reversed - if `toward` is the ring that way.
    void* ring = TargetIObj();
    const auto hp = dll::ObjGetHardpoint.get<dll::ObjHardpointFn>(ring);
    float ringPos[3] = {}, towardPos[3] = {};
    if (!hp || !Location(GetApi(), ObjId(ring), ringPos) || !Location(GetApi(), toward, towardPos))
        return false;
    const float to[3] = {towardPos[0] - ringPos[0], towardPos[1] - ringPos[1], towardPos[2] - ringPos[2]};
    if (to[0] * dir[0] + to[1] * dir[1] + to[2] * dir[2] >= 0)
        return false; // `toward` does not lie the other way
    for (const char* n : {"HpLeftLane", "HpRightLane"})
    {
        float pos[3] = {}, rot[16] = {};
        if (CallHardpoint(hp, ring, n, pos, rot) != 0)
            continue;
        const float d[3] = {pos[0] - start[0], pos[1] - start[1], pos[2] - start[2]};
        if (d[0] * d[0] + d[1] * d[1] + d[2] * d[2] > 1.0f)
        {
            memcpy(start, pos, sizeof(pos));
            for (int i = 0; i < 3; ++i)
                dir[i] = -dir[i];
            return true;
        }
    }
    return false;
}
} // namespace space
