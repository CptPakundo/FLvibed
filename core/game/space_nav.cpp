// The player's piloting state and navigation: the native target command, the pilot readout (ReadPilot), the
// waypoint list (save / set / restore / select) and the nav map's best path. Split from space.cpp on 24 Oct 2026 (a
// file move: the functions are unchanged).
#include "game/space_internal.h"

#include "fldll.h"
#include "flexe.h"

namespace space
{
constexpr u32 kSetTargetAction = 0x1C; // USER_SET_TARGET
// exe::ObjectAction: bool cdecl(action*): camera, then the player ship's controller

static bool CallAction(void* action)
{
    __try
    {
        return hk::Call<bool(__cdecl*)(void*)>(exe::ObjectAction, action);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool SetTarget(u32 id)
{
    const Api& a = GetApi();
    if (!a.ok || !id || !InSpace())
        return false;
    // The 0x28-byte action the contact list builds (0x4CFFA0): +0 action, +0x10 object id, +0x14 sub-object,
    // byte +0x26 = 1.
    u8 action[0x28] = {};
    *reinterpret_cast<u32*>(action) = kSetTargetAction;
    *reinterpret_cast<u32*>(action + 0x10) = id;
    *reinterpret_cast<u32*>(action + 0x14) = *a.subobjNone;
    action[0x26] = 1;
    return CallAction(action);
}

bool ReadPilot(Pilot& p)
{
    p = Pilot();
    const Api& a = GetApi();
    void* iobj = PlayerIObj();
    if (!a.ok || !iobj)
        return false;
    p.system = Mem<u32>(exe::PlayerSystem);
    Location(a, Mem<u32>(exe::PlayerShipId), p.pos);
    __try
    {
        const auto cobject = dll::ObjCobject.get<dll::ObjCobjectFn>(iobj);
        void* ship = cobject ? cobject(iobj) : nullptr;
        if (!ship)
            return true;
        if (a.usingLane)
            p.lane = a.usingLane(ship, nullptr);
        if (a.laneEntry)
            p.laneEntry = a.laneEntry(ship, nullptr);
        if (a.laneExit)
            p.laneExit = a.laneExit(ship, nullptr);
        if (a.jumping)
            p.jumping = a.jumping(ship, nullptr);
        if (a.cruise)
            p.cruise = a.cruise(ship, nullptr);
        if (a.target)
            p.target = ObjId(a.target(ship, nullptr));
        void* leader = nullptr;
        if (const auto getLeader = dll::ObjGetFormationLeader.get<dll::ObjGetObjFn>(iobj))
            getLeader(iobj, &leader);
        p.leader = ObjId(leader);
        if (const uintptr_t bm = a.behavior ? reinterpret_cast<uintptr_t>(a.behavior(ship, nullptr)) : 0)
        {
            // IBehaviorManager fields from Common.dll's own accessors (core\fldll.h; was +0xC0 / +0xBC)
            const dll::BehaviorFields& f = dll::Behavior();
            if (f.ok())
            {
                p.op = Mem<int>(bm + f.op);
                p.locked = Mem<u8>(bm + f.locked) != 0;
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
    return true;
}

// The player's waypoint list (24-byte entries at exe::Waypoints: position, +0xC system, +0x10 object id, +0x14
// sequence; count exe::WaypointCount, at most 99), the functions the nav map and the contact list use on it:
// exe::AddWaypoint bool cdecl(const Vector*, u32 system, u32 object), exe::ClearWaypoints void cdecl() (count = 0,
// the HUD drops the waypoint object), exe::SelectWaypoint void cdecl(int index) (target it: creates the waypoint
// object). All or nothing: without one of them the waypoint functions refuse.
typedef void(__cdecl* ClearWaypointsFn)();
typedef bool(__cdecl* AddWaypointFn)(const float*, u32, u32);
typedef void(__cdecl* SelectWaypointFn)(int);

static bool WaypointsOk()
{
    return exe::Waypoints.ok() && exe::WaypointCount.ok() && exe::AddWaypoint.ok() && exe::ClearWaypoints.ok() &&
           exe::SelectWaypoint.ok();
}

void ClearWaypoints()
{
    if (InSpace() && WaypointsOk())
    {
        InvalidateFrame(); // the HUD drops its waypoint object (the per-frame memo, space.h)
        hk::Call<ClearWaypointsFn>(exe::ClearWaypoints);
    }
}

bool RemoveWaypoint(int index)
{
    if (!InSpace() || !WaypointsOk() || !exe::RemoveWaypoint.ok() || index < 0 || index >= Mem<int>(exe::WaypointCount))
        return false;
    InvalidateFrame();
    hk::Call<void(__cdecl*)(int)>(exe::RemoveWaypoint, index);
    return true;
}

// The player's list as it was before SetWaypoint replaced it (the mission's navmarkers), restored by
// RestoreWaypoints.
static std::vector<WaypointEntry> g_savedWaypoints;
static bool g_waypointsSaved = false;
static float g_ownWaypoint[3] = {};
static void* g_savedFlight = nullptr; // the player ship (IObj) whose list was saved

WaypointEntry* WaypointAt(int i) { return reinterpret_cast<WaypointEntry*>(exe::Waypoints.get() + 24 * i); }

// The list holds only our own waypoint, or nothing (reached / dropped by the HUD).
static bool OnlyOwnWaypoint()
{
    const int n = Mem<int>(exe::WaypointCount);
    return n == 0 || (n == 1 && WaypointAt(0)->pos[0] == g_ownWaypoint[0] && WaypointAt(0)->pos[2] == g_ownWaypoint[2]);
}

int RestoreWaypoints(bool ownList)
{
    if (!g_waypointsSaved || !InSpace() || !WaypointsOk())
        return -1;
    g_waypointsSaved = false;
    // A saved list belongs to the flight it was taken in (was: kept until a restore - a harness goto that failed
    // before its restore left it armed, and a later flight, another save's too, got its navmarkers, 18 Oct 2026).
    if (g_savedFlight != PlayerIObj())
        return -1;
    // Restore only if the list still holds our own waypoint or nothing (reached): anything else is the mission's
    // newer list. ownList: the caller just read the list as its own best-path route (the autopilot's plan).
    if (!ownList && !OnlyOwnWaypoint())
        return -2;
    InvalidateFrame(); // the waypoint calls change the HUD's waypoint object (the per-frame memo, space.h)
    __try
    {
        hk::Call<ClearWaypointsFn>(exe::ClearWaypoints);
        for (const WaypointEntry& e : g_savedWaypoints)
            hk::Call<AddWaypointFn>(exe::AddWaypoint, static_cast<const float*>(e.pos), e.system, e.object);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return -1;
    }
    return static_cast<int>(g_savedWaypoints.size());
}

u32 SelectFirstWaypoint()
{
    if (!InSpace() || !WaypointsOk() || Mem<int>(exe::WaypointCount) < 1)
        return 0;
    InvalidateFrame(); // selecting creates the waypoint object (the per-frame memo, space.h)
    __try
    {
        hk::Call<SelectWaypointFn>(exe::SelectWaypoint, 0);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
    Pilot pl;
    return ReadPilot(pl) ? pl.target : 0;
}

void SaveWaypoints()
{
    if (!InSpace() || !WaypointsOk())
        return;
    const int n = Mem<int>(exe::WaypointCount);
    g_savedWaypoints.assign(WaypointAt(0), WaypointAt(n > 0 && n <= 99 ? n : 0));
    g_waypointsSaved = true;
    g_savedFlight = PlayerIObj();
}

u32 SetWaypoint(const float pos[3])
{
    if (!InSpace() || !WaypointsOk())
        return 0;
    // Save the list unless it is already saved for this flight and still ours (a route of several waypoints).
    if (!g_waypointsSaved || g_savedFlight != PlayerIObj() || !OnlyOwnWaypoint())
    {
        const int n = Mem<int>(exe::WaypointCount);
        g_savedWaypoints.assign(WaypointAt(0), WaypointAt(n > 0 && n <= 99 ? n : 0));
        g_waypointsSaved = true;
        g_savedFlight = PlayerIObj();
    }
    g_ownWaypoint[0] = pos[0], g_ownWaypoint[1] = pos[1], g_ownWaypoint[2] = pos[2];
    InvalidateFrame(); // clear + select replace the HUD's waypoint object (the per-frame memo, space.h)
    __try
    {
        hk::Call<ClearWaypointsFn>(exe::ClearWaypoints);
        float p[3] = {pos[0], pos[1], pos[2]};
        if (!hk::Call<AddWaypointFn>(exe::AddWaypoint, static_cast<const float*>(p), Mem<u32>(exe::PlayerSystem), 0u))
            return 0;
        if (Mem<int>(exe::WaypointCount) < 1)
            return 0;
        hk::Call<SelectWaypointFn>(exe::SelectWaypoint, 0);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
    Pilot pl;
    return ReadPilot(pl) ? pl.target : 0;
}

std::vector<Waypoint> Waypoints()
{
    std::vector<Waypoint> out;
    if (!exe::Waypoints.ok() || !exe::WaypointCount.ok())
        return out;
    const int n = Mem<int>(exe::WaypointCount);
    for (int i = 0; i < n && i < 99; ++i)
    {
        const WaypointEntry* w = WaypointAt(i);
        Waypoint p;
        memcpy(p.pos, w->pos, sizeof(p.pos));
        p.system = w->system;
        p.object = w->object;
        p.seq = w->seq;
        out.push_back(p);
    }
    return out;
}

bool PlotBestPath(const float end[3], u32 endObject, u32 endSystem)
{
    Pilot p;
    if (!InSpace() || !exe::PlotBestPath.ok() || !ReadPilot(p))
        return false;
    typedef void(__cdecl * PlotFn)(const float*, const float*, u32, u32);
    hk::Call<PlotFn>(exe::PlotBestPath, p.pos, end, endObject, endSystem ? endSystem : p.system);
    return true;
}
} // namespace space
