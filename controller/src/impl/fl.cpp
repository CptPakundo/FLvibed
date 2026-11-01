#include "../fl.h"

#include "fldll.h"
#include "log.h"

namespace fl
{
typedef bool(__cdecl* HandleKeyFn)(int cmd, int category);

const char* CmdName(int cmd) { return CmdNameRaw(cmd); }

int CmdFromName(const char* name)
{
    if (!name || !*name)
        return -1;
    if (isdigit(static_cast<unsigned char>(name[0])))
    {
        int v = static_cast<int>(strtol(name, nullptr, 0));
        return (v >= 0 && v < CMD_COUNT) ? v : -1;
    }
    for (int i = 0; i < CMD_COUNT; ++i)
    {
        if (_stricmp(CmdName(i), name) == 0)
            return i;
    }
    return -1;
}

// Freelancer's loaded key commands: list at [exe::KeyCommands]; entry (node+8): +0 command, +4 ids_name, +8 ids_info,
// +C event mask, +14/+18 key vector. Guarded (no C++ objects in this frame): the game rebuilds the list when the player
// edits the controls, and this runs on every fired command (29 Oct 2026; the other walks were guarded already).
static void ReadCommandFlags(u32* cache)
{
    __try
    {
        const u32 sentinel = Mem<u32>(exe::KeyCommands);
        u32 node = sentinel ? Mem<u32>(sentinel) : 0;
        for (int guard = 0; node && node != sentinel && guard < 1000; ++guard)
        {
            const u32 id = Mem<u32>(node + 8);
            if (id < CMD_COUNT)
                cache[id] = Mem<u32>(node + 8 + 0xC);
            node = Mem<u32>(node);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

u32 CommandFlags(int cmd)
{
    static u32 cache[CMD_COUNT];
    static DWORD cacheTick = 0;
    const DWORD now = GetTickCount();
    if (!cacheTick || now - cacheTick > 2000)
    {
        memset(cache, 0, sizeof(cache));
        if (exe::KeyCommands.ok())
            ReadCommandFlags(cache);
        cacheTick = now;
    }
    if (cmd < 0 || cmd >= CMD_COUNT || !cache[cmd])
        return CMDF_KEYDOWN;
    return cache[cmd];
}

bool FireCommand(int cmd, bool down)
{
    // The key state globals tell the dispatcher what kind of event this is: without them the call is refused.
    if (cmd <= 0 || cmd >= CMD_COUNT || !exe::KeyIsDown.ok() || !exe::KeyCurVk.ok() || !exe::KeyCurMods.ok())
        return false;

    // Mimic a key event with no particular physical key or modifiers.
    const u8 savedDown = Mem<u8>(exe::KeyIsDown);
    const u32 savedVk = Mem<u32>(exe::KeyCurVk);
    const u32 savedMods = Mem<u32>(exe::KeyCurMods);
    Mem<u8>(exe::KeyIsDown) = down ? 1 : 0;
    Mem<u32>(exe::KeyCurVk) = 0;
    Mem<u32>(exe::KeyCurMods) = 0;

    const bool handled = hk::Call<HandleKeyFn>(exe::HandleKey, cmd, 1);

    Mem<u8>(exe::KeyIsDown) = savedDown;
    Mem<u32>(exe::KeyCurVk) = savedVk;
    Mem<u32>(exe::KeyCurMods) = savedMods;
    return handled;
}

typedef bool(__cdecl* WinKeyFn)(UINT msg, WPARAM wParam, LPARAM lParam);

bool SendKey(int vk, bool down)
{
    const UINT scan = MapVirtualKeyA(vk, MAPVK_VK_TO_VSC);
    LPARAM lParam = 1 | (scan << 16);
    if (!down)
        lParam |= 0xC0000000; // previous state down, transition up
    return hk::Call<WinKeyFn>(exe::WinKey, static_cast<UINT>(down ? WM_KEYDOWN : WM_KEYUP), static_cast<WPARAM>(vk),
                              lParam);
}

void TapKey(int vk)
{
    SendKey(vk, true);
    SendKey(vk, false);
}

bool SendChar(wchar_t ch)
{
    return hk::Call<WinKeyFn>(exe::WinKey, static_cast<UINT>(WM_CHAR), static_cast<WPARAM>(ch), static_cast<LPARAM>(1));
}

bool GamePaused() { return hk::Call<bool(__cdecl*)()>(exe::GamePaused); }
void* PlayerIObj() { return hk::Call<void*(__cdecl*)()>(exe::GetPlayerIObj); }
u32 PlayerBase() { return Mem<u32>(exe::PlayerBase); }
u32 PlayerSystem() { return Mem<u32>(exe::PlayerSystem); }

const char* CursorName()
{
    const char* p = Mem<const char*>(exe::CursorName);
    return p ? p + 4 : "";
}
} // namespace fl

namespace fl
{
// IObjInspect virtuals by name (core\fldll.h; was: slots 0-3, 8, 11, 26, 29, 84 hard-coded).
static bool ReadKinematics(void* iobj, Kinematics& out)
{
    const auto pos = dll::ObjGetPosition.get<dll::ObjRefFn>(iobj);
    const auto vel = dll::ObjGetVelocity.get<dll::ObjVectorFn>(iobj);
    const auto angVel = dll::ObjGetAngularVelocity.get<dll::ObjVectorFn>(iobj);
    const auto rot = dll::ObjGetOrientation.get<dll::ObjRefFn>(iobj);
    if (!pos || !vel || !angVel || !rot)
        return false;
    __try
    {
        memcpy(out.pos, pos(iobj), sizeof(out.pos));
        vel(iobj, out.vel);
        angVel(iobj, out.angVel);
        memcpy(out.rot, rot(iobj), sizeof(out.rot));
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool PlayerKinematics(Kinematics& out)
{
    void* iobj = PlayerIObj();
    return iobj && ReadKinematics(iobj, out);
}

static void* ReadTarget(void* iobj)
{
    const auto getTarget = dll::ObjGetTarget.get<dll::ObjGetObjFn>(iobj);
    if (!getTarget)
        return nullptr;
    __try
    {
        void* target = nullptr;
        getTarget(iobj, &target);
        return target;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return nullptr;
    }
}

// Common.dll CShip::get_tgt_lead_fire_pos(Vector&) on the ship (IObjInspect cobject()); the per-gun
// IObjInspect::get_tgt_lead_fire_pos needs a gun's sub-object id.
typedef bool(__thiscall* ShipLeadFn)(const void* ship, float* out); // ?...@@QBE_NAAVVector@@@Z: bool (Vector&) const

static int ReadLead(void* iobj, float* out)
{
    static ShipLeadFn lead = reinterpret_cast<ShipLeadFn>(
        GetProcAddress(GetModuleHandleA("common.dll"), "?get_tgt_lead_fire_pos@CShip@@QBE_NAAVVector@@@Z"));
    const auto cobject = dll::ObjCobject.get<dll::ObjCobjectFn>(iobj);
    if (!lead || !cobject)
        return -1;
    __try
    {
        void* ship = cobject(iobj);
        return ship && lead(ship, out) ? 1 : 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return -1;
    }
}

int TargetLeadPos(float out[3])
{
    void* iobj = PlayerIObj();
    return iobj ? ReadLead(iobj, out) : -1;
}

static bool ReadCameraAt(u32 cam, CameraView& out)
{
    __try
    {
        memcpy(out.rot, reinterpret_cast<const void*>(cam + 4), sizeof(out.rot));
        memcpy(out.pos, reinterpret_cast<const void*>(cam + 0x28), sizeof(out.pos));
        out.cx = Mem<float>(cam + 0x68), out.cy = Mem<float>(cam + 0x6C);
        out.sx = Mem<float>(cam + 0x78), out.sy = Mem<float>(cam + 0x7C);
        out.nearPlane = Mem<float>(cam + 0xA4);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool ReadCamera(CameraView& out)
{
    const u32 cam = CurrentCamera();
    return cam && ReadCameraAt(cam, out);
}

bool WorldToScreen(const float world[3], float out[3])
{
    // the current 3D camera (fl.h CameraView; Freelancer's own 0x40F4F0 path reported every point as hidden when
    // called by us)
    CameraView c;
    if (!ReadCamera(c))
        return false;
    const float d[3] = {world[0] - c.pos[0], world[1] - c.pos[1], world[2] - c.pos[2]};
    float v[3];
    for (int i = 0; i < 3; ++i)
        v[i] = c.rot[0 * 3 + i] * d[0] + c.rot[1 * 3 + i] * d[1] + c.rot[2 * 3 + i] * d[2];
    if (v[2] > -c.nearPlane)
        return false; // behind the near plane
    out[0] = c.cx + c.sx * (v[0] / -v[2]);
    out[1] = c.cy + c.sy * (v[1] / -v[2]);
    out[2] = -v[2];
    return true;
}

bool FlightScreenAxes(float rot[9])
{
    if (TurretView())
    {
        Kinematics me;
        if (!PlayerKinematics(me))
            return false;
        memcpy(rot, me.rot, 9 * sizeof(float));
        return true;
    }
    CameraView c;
    if (!ReadCamera(c))
        return false;
    memcpy(rot, c.rot, sizeof(c.rot));
    return true;
}

bool WorldToFlightScreen(const float world[3], float out[3])
{
    if (!TurretView())
        return WorldToScreen(world, out);
    Kinematics me;
    CameraView c;
    if (!ReadCamera(c) || !PlayerKinematics(me))
        return false;
    // the ship's axes as the camera's (rot columns: +x right, +y up, looks along -z, as a camera's)
    const float d[3] = {world[0] - me.pos[0], world[1] - me.pos[1], world[2] - me.pos[2]};
    float v[3];
    for (int i = 0; i < 3; ++i)
        v[i] = me.rot[0 * 3 + i] * d[0] + me.rot[1 * 3 + i] * d[1] + me.rot[2 * 3 + i] * d[2];
    if (v[2] > -c.nearPlane)
        return false; // behind the ship
    out[0] = c.cx + c.sx * (v[0] / -v[2]);
    out[1] = c.cy + c.sy * (v[1] / -v[2]);
    out[2] = -v[2];
    return true;
}

static bool ReadAttitude(void* from, const void* to, float& out)
{
    const auto attitude = dll::ObjGetAttitudeTowards.get<dll::ObjAttitudeFn>(from);
    if (!attitude)
        return false;
    __try
    {
        out = 0;
        attitude(from, &out, to);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool TargetAttitude(float& out)
{
    void* iobj = PlayerIObj();
    void* target = iobj ? ReadTarget(iobj) : nullptr;
    return target && ReadAttitude(target, iobj, out);
}

bool TargetRadius(float& out)
{
    void* iobj = PlayerIObj();
    void* target = iobj ? ReadTarget(iobj) : nullptr;
    const auto radius = dll::ObjGetPhysicalRadius.get<dll::ObjRadiusFn>(target);
    if (!radius)
        return false;
    __try
    {
        float center[3];
        out = 0;
        radius(target, &out, center);
        return out > 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

uintptr_t TargetHandle()
{
    void* iobj = PlayerIObj();
    return iobj ? reinterpret_cast<uintptr_t>(ReadTarget(iobj)) : 0;
}

bool TargetKinematics(Kinematics& out)
{
    void* iobj = PlayerIObj();
    void* target = iobj ? ReadTarget(iobj) : nullptr;
    return target && ReadKinematics(target, out);
}

// IObjInspect get_id(): the space object id (pub::SpaceObj API).
static u32 ReadId(void* obj)
{
    const auto getId = dll::ObjGetId.get<dll::ObjGetIdFn>(obj);
    if (!getId)
        return 0;
    __try
    {
        return getId(obj);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
}

u32 TargetId()
{
    void* iobj = PlayerIObj();
    void* target = iobj ? ReadTarget(iobj) : nullptr;
    return target ? ReadId(target) : 0;
}
} // namespace fl
