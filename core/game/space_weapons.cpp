// Weapons: a ship's guns / launchers / droppers through the equipment traverser, the HUD fire slots, turret cover
// (CEGun::CanPointAt) and the player's countermeasures. Split from space.cpp on 24 Oct 2026 (a file move: the
// functions are unchanged).
#include "game/space_internal.h"

#include "fldll.h"
#include "flexe.h"
#include "game/room.h"

namespace space
{
int ReadWeapons(void* ship, Weapon* out, int max, int equipClass /* = kEquipClassGun (space_internal.h) */)
{
    using Ctor = void*(__fastcall*)(void*, void*, int);
    using Trav = void*(__fastcall*)(void*, void*, void*);
    static const HMODULE common = GetModuleHandleA("common.dll");
    static const Ctor ctor = reinterpret_cast<Ctor>(GetProcAddress(common, "??0CEquipTraverser@@QAE@H@Z"));
    static const Trav trav =
        reinterpret_cast<Trav>(GetProcAddress(common, "?Traverse@CEquipManager@@QAEPAVCEquip@@AAVCEquipTraverser@@@Z"));
    const Accessors& acc = Acc();
    if (!ctor || !trav || !ship || !acc.equipId || !acc.equipArch || !acc.projArch ||
        !IsClass(ship, "??_7CShip@@6BCEqObj@@@"))
        return 0;
    int n = 0;
    __try
    {
        alignas(8) unsigned char traverser[32] = {};
        ctor(traverser, nullptr, equipClass);
        void* mgr = reinterpret_cast<unsigned char*>(ship) + kEqObjEquipManager;
        for (void* e = trav(mgr, nullptr, traverser); e && n < max; e = trav(mgr, nullptr, traverser))
        {
            if (!IsLauncherEquip(e))
                continue; // not a gun / launcher / dropper class we know: never read by offset
            const uintptr_t p = reinterpret_cast<uintptr_t>(e);
            Weapon& w = out[n++];
            w.subObj = acc.equipId(e);
            w.active = Mem<u8>(p + kEquipActive) != 0;
            w.elapsed = Mem<float>(p + kLauncherElapsed);
            // CExternalEquip mountedHardpoint +0x20 (FLSDK): the hardpoint name ("HpWeapon02")
            if (const char* hp = Mem<const char*>(p + kExternalHardpoint))
                strncpy_s(w.hardpoint, hp, _TRUNCATE);
            // CELauncher::GetAmmoCount: rounds left for ammo launchers, -1 for energy guns (unlimited).
            const auto ammo = dll::LauncherGetAmmoCount.get<dll::LauncherAmmoFn>(e);
            w.ammo = ammo ? ammo(e) : -1;
            const uintptr_t arch = reinterpret_cast<uintptr_t>(acc.equipArch(e));
            if (arch && IsLauncherArch(reinterpret_cast<const void*>(arch)))
            {
                // Archetype::Root::get_id (the id at +8, the nickname hash). The old raw +4 read was an unrelated field
                // (426997752 for the Azrael whose id is 3058004237; found by the READERS track, 25 Oct 2026)
                if (const Api& a = GetApi(); a.archId)
                    w.archId = a.archId(reinterpret_cast<const void*>(arch), nullptr);
                w.power = Mem<float>(arch + kArchPower);
                w.refire = Mem<float>(arch + kArchRefire);
                w.muzzleVelocity = Mem<float>(arch + kArchMuzzle);
            }
            const uintptr_t proj = reinterpret_cast<uintptr_t>(acc.projArch(e));
            if (proj && IsMunitionArch(reinterpret_cast<const void*>(proj)))
            {
                MunitionDamage(proj, w.hullDamage, w.blastRadius);
                w.homing = Mem<u32>(proj + kMunitionSeeker) == 2;
                w.projArch = proj;
                w.lifetime = Mem<float>(proj + kProjLifetime);
                if (w.homing)
                {
                    w.seekerRange = Mem<float>(proj + kMunitionSeekerRange);
                    // stored as the cosine of seeker_fov_deg (Javelin 35 -> 0.819152, 18 Oct 2026): a half-angle
                    w.seekerFovDeg =
                        acosf(std::max(-1.0f, std::min(1.0f, Mem<float>(proj + kMunitionSeekerFov)))) * 57.29578f;
                    w.maxTurn = Mem<float>(proj + kMunitionMaxTurn);
                    // (squared in the archetype; a value outside 0..100 m is taken as unread)
                    if (const float d2 = Mem<float>(proj + kMunitionDetonationSq); d2 > 0 && d2 <= 10000.0f)
                        w.detonationDist = std::sqrt(d2);
                    using MotorFn = const void*(__cdecl*)(u32);
                    static const MotorFn motor = reinterpret_cast<MotorFn>(
                        GetProcAddress(GetModuleHandleA("common.dll"), "?GetMotor@Archetype@@YAPBUMotorData@1@I@Z"));
                    if (const u32 mid = Mem<u32>(proj + kMunitionMotor); mid && motor)
                        if (const uintptr_t m = reinterpret_cast<uintptr_t>(motor(mid)))
                        {
                            w.motorDelay = Mem<float>(m + kMotorDelay);
                            w.motorAccel = Mem<float>(m + kMotorAccel);
                            w.motorTime = Mem<float>(m + kMotorLifetime);
                        }
                }
                // Archetype::Root::get_id (the raw +4 read gave other values for munitions, 18 Oct 2026)
                if (const Api& a = GetApi(); a.archId)
                    w.ammoArch = a.archId(reinterpret_cast<const void*>(proj), nullptr);
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
    return n;
}

// The game's fire slots (USER_FIRE_WEAPON1..10) are the HUD status window's weapon list, not the equipment order the
// traverser walks (18 Oct 2026: the traverser gives the Javelin first, the game fires it on slot 2). Freelancer.exe:
// the "HUD_Status" control's command handler (switch at 0x4DBACB, case USER_FIRE_WEAPON1 0x4DBEC2) fires entry n-1 of
// the vector at object +0x590 / +0x594 (36-byte entries, u16 sub-object id first). Read from there and accepted only
// when every entry is one of the ship's weapons; else (HUD hidden) by hardpoint name, which matched the measurement.
static bool HudFireSlots(Weapon* w, int n)
{
    // The control lookup walks every UI control: once per 2 s (the read below validates the cached one each time).
    static u32 ctl = 0;
    static DWORD at = 0;
    if (!ctl || GetTickCount() - at > 2000)
    {
        ctl = uiclick::FindVisible("HUD_Status");
        at = GetTickCount();
    }
    if (!ctl)
        return false;
    __try
    {
        // The named control is the object's sub-object at +0x38 (its vtable is the one the constructor 0x4DA170 stores
        // there): the list is at object +0x590 = control +0x558.
        const u32 begin = Mem<u32>(ctl + 0x558), end = Mem<u32>(ctl + 0x55C);
        if (!begin || end < begin || (end - begin) % 36 != 0 || (end - begin) / 36 > 16)
            return false;
        // Entries that are not guns / launchers (the list also holds other equipment, e.g. a 4th entry on the M02 ship)
        // keep their slot number; the read counts only when every gun and launcher of ours is in it.
        const int count = static_cast<int>((end - begin) / 36);
        int slotOf[16] = {};
        for (int k = 0; k < count && k < 10; ++k)
        {
            const u16 sub = Mem<u16>(begin + 36 * k);
            for (int i = 0; i < n; ++i)
                if (w[i].subObj == sub)
                    slotOf[i] = k + 1;
        }
        for (int i = 0; i < n; ++i)
            if (!slotOf[i])
                return false; // not the list we think it is
        for (int i = 0; i < n; ++i)
            w[i].fireSlot = slotOf[i];
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool g_fireSlotsHud = false; // the last numbering came from the HUD list (debug `weapons`)
static void NumberFireSlots(Weapon* w, int n)
{
    g_fireSlotsHud = HudFireSlots(w, n);
    if (g_fireSlotsHud)
        return;
    for (int i = 0; i < n; ++i)
    {
        w[i].fireSlot = 0;
        if (!w[i].hardpoint[0])
            continue;
        int rank = 1;
        for (int j = 0; j < n; ++j)
            if (j != i && w[j].hardpoint[0] && _stricmp(w[j].hardpoint, w[i].hardpoint) < 0)
                ++rank;
        w[i].fireSlot = rank;
    }
}

static std::vector<Weapon> PlayerWeaponsScan()
{
    Weapon buf[16];
    const int n = InSpace() ? ReadWeapons(PlayerCShip(), buf, 16) : 0;
    NumberFireSlots(buf, n);
    return std::vector<Weapon>(buf, buf + n);
}

// Memoised per frame (space.h BeginFrame); g_fireSlotsHud keeps the scan's value.
static FrameMemo<std::vector<Weapon>> g_weaponsMemo;

std::vector<Weapon> PlayerWeapons()
{
    if (!FrameStamp())
        return PlayerWeaponsScan(); // memo off: a plain scan, as before
    return g_weaponsMemo.Valid() ? g_weaponsMemo.value : g_weaponsMemo.Fill(PlayerWeaponsScan());
}

std::vector<Weapon> ShipWeapons(u32 id)
{
    const Api& a = GetApi();
    if (!a.ok || !id || !InSpace())
        return {};
    std::vector<void*> objs;
    Collect(a, kClassShip, Mem<u32>(exe::PlayerSystem), objs);
    for (void* o : objs)
        if (ObjectId(a, o) == id)
        {
            Weapon buf[16];
            const int n = ReadWeapons(o, buf, 16);
            return std::vector<Weapon>(buf, buf + n);
        }
    return {};
}

// Turret reach by the game's own test (Common.dll CEGun, 22 Oct 2026): CanPointAt(point, tolerance) checks the point's
// direction from the turret origin against the gun's joint limits widened by `tolerance` (rad; a gun without joints:
// its axis +- tolerance) - read from its disassembly, 0x6298860; GetTurretOrigin returns the Vector by hidden pointer;
// GetMunitionRange the projectile's reach (m).
using GunPointFn = bool(__thiscall*)(const void*, const float*, float);
using GunOriginFn = float*(__thiscall*)(const void*, float*);
using GunRangeFn = float(__thiscall*)(const void*);

static int CoverPoints(void* obj, const float (*pts)[3], int n, int* counts, float tolerance)
{
    using Ctor = void*(__fastcall*)(void*, void*, int);
    using Trav = void*(__fastcall*)(void*, void*, void*);
    static const HMODULE common = GetModuleHandleA("common.dll");
    static const Ctor ctor = reinterpret_cast<Ctor>(GetProcAddress(common, "??0CEquipTraverser@@QAE@H@Z"));
    static const Trav trav =
        reinterpret_cast<Trav>(GetProcAddress(common, "?Traverse@CEquipManager@@QAEPAVCEquip@@AAVCEquipTraverser@@@Z"));
    static const GunPointFn point =
        reinterpret_cast<GunPointFn>(GetProcAddress(common, "?CanPointAt@CEGun@@QBE_NABVVector@@M@Z"));
    static const GunOriginFn origin =
        reinterpret_cast<GunOriginFn>(GetProcAddress(common, "?GetTurretOrigin@CEGun@@QBE?AVVector@@XZ"));
    static const GunRangeFn range =
        reinterpret_cast<GunRangeFn>(GetProcAddress(common, "?GetMunitionRange@CEGun@@QBEMXZ"));
    if (!ctor || !trav || !point || !origin || !range || !obj ||
        !(IsClass(obj, "??_7CShip@@6BCEqObj@@@") || IsClass(obj, "??_7CSolar@@6B@")))
        return -1;
    int guns = 0;
    __try
    {
        alignas(8) unsigned char traverser[32] = {};
        ctor(traverser, nullptr, kEquipClassGun);
        void* mgr = reinterpret_cast<unsigned char*>(obj) + kEqObjEquipManager;
        for (void* e = trav(mgr, nullptr, traverser); e; e = trav(mgr, nullptr, traverser))
        {
            if (!IsClass(e, "??_7CEGun@@6B@") || !Mem<u8>(reinterpret_cast<uintptr_t>(e) + kEquipActive))
                continue;
            ++guns;
            float o[3] = {};
            origin(e, o);
            const float reach = range(e);
            for (int i = 0; i < n; ++i)
            {
                const float d[3] = {pts[i][0] - o[0], pts[i][1] - o[1], pts[i][2] - o[2]};
                if (reach > 0 && d[0] * d[0] + d[1] * d[1] + d[2] * d[2] > reach * reach)
                    continue;
                if (point(e, pts[i], tolerance))
                    ++counts[i];
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return -1;
    }
    return guns;
}

int GunsCovering(u32 id, const float (*pts)[3], int n, int* counts, float tolerance)
{
    for (int i = 0; i < n; ++i)
        counts[i] = 0;
    const Api& a = GetApi();
    if (!a.ok || !id || !InSpace())
        return -1;
    for (u32 cls : {kClassShip, kClassSolar})
    {
        std::vector<void*> objs;
        Collect(a, cls, Mem<u32>(exe::PlayerSystem), objs);
        for (void* o : objs)
            if (ObjectId(a, o) == id)
                return CoverPoints(o, pts, n, counts, tolerance);
    }
    return -1;
}

// (CoverPoints' traversal, with each counted gun's munition damage and refire read as ReadWeapons does)
static int VolleyRaw(void* obj, const float pt[3], float tolerance, float* volley, float* perSecond)
{
    using Ctor = void*(__fastcall*)(void*, void*, int);
    using Trav = void*(__fastcall*)(void*, void*, void*);
    static const HMODULE common = GetModuleHandleA("common.dll");
    static const Ctor ctor = reinterpret_cast<Ctor>(GetProcAddress(common, "??0CEquipTraverser@@QAE@H@Z"));
    static const Trav trav =
        reinterpret_cast<Trav>(GetProcAddress(common, "?Traverse@CEquipManager@@QAEPAVCEquip@@AAVCEquipTraverser@@@Z"));
    static const GunPointFn point =
        reinterpret_cast<GunPointFn>(GetProcAddress(common, "?CanPointAt@CEGun@@QBE_NABVVector@@M@Z"));
    static const GunOriginFn origin =
        reinterpret_cast<GunOriginFn>(GetProcAddress(common, "?GetTurretOrigin@CEGun@@QBE?AVVector@@XZ"));
    static const GunRangeFn range =
        reinterpret_cast<GunRangeFn>(GetProcAddress(common, "?GetMunitionRange@CEGun@@QBEMXZ"));
    const Accessors& acc = Acc();
    if (!ctor || !trav || !point || !origin || !range || !obj || !acc.equipArch || !acc.projArch ||
        !(IsClass(obj, "??_7CShip@@6BCEqObj@@@") || IsClass(obj, "??_7CSolar@@6B@")))
        return -1;
    int guns = 0;
    __try
    {
        alignas(8) unsigned char traverser[32] = {};
        ctor(traverser, nullptr, kEquipClassGun);
        void* mgr = reinterpret_cast<unsigned char*>(obj) + kEqObjEquipManager;
        for (void* e = trav(mgr, nullptr, traverser); e; e = trav(mgr, nullptr, traverser))
        {
            if (!IsClass(e, "??_7CEGun@@6B@") || !Mem<u8>(reinterpret_cast<uintptr_t>(e) + kEquipActive))
                continue;
            const uintptr_t proj = reinterpret_cast<uintptr_t>(acc.projArch(e));
            if (!proj || !IsMunitionArch(reinterpret_cast<const void*>(proj)) || Mem<u32>(proj + kMunitionSeeker) == 2)
                continue;
            float o[3] = {};
            origin(e, o);
            const float reach = range(e);
            const float d[3] = {pt[0] - o[0], pt[1] - o[1], pt[2] - o[2]};
            if (reach > 0 && d[0] * d[0] + d[1] * d[1] + d[2] * d[2] > reach * reach)
                continue;
            if (!point(e, pt, tolerance))
                continue;
            float dmg = 0, blast = 0;
            MunitionDamage(proj, dmg, blast);
            const uintptr_t arch = reinterpret_cast<uintptr_t>(acc.equipArch(e));
            const float refire = arch && IsLauncherArch(reinterpret_cast<const void*>(arch)) ? Mem<float>(arch + kArchRefire) : 0;
            *volley += dmg;
            if (refire > 0)
                *perSecond += dmg / refire;
            ++guns;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return -1;
    }
    return guns;
}

int VolleyAt(u32 id, const float pt[3], float* volley, float* perSecond, float tolerance)
{
    *volley = 0, *perSecond = 0;
    const Api& a = GetApi();
    if (!a.ok || !id || !InSpace())
        return -1;
    for (u32 cls : {kClassShip, kClassSolar})
    {
        std::vector<void*> objs;
        Collect(a, cls, Mem<u32>(exe::PlayerSystem), objs);
        for (void* o : objs)
            if (ObjectId(a, o) == id)
                return VolleyRaw(o, pt, tolerance, volley, perSecond);
    }
    return -1;
}

// Debug `gunpoint`: each active player gun's own verdict on a world point - CEGun::CanPointAt (its joint limits),
// CanFire (the game's FireResult, FLHookSDK Enums: 9 success, 5 gun angle, 4 power, 2 refire, 6 cruise), the distance against its munition's reach, the hardpoint
// (the owner, 28 Oct 2026: on the M13 generators / a Nomad battleship not every gun fired - probe before a fix).
static const char* FireResultName(int r)
{
    static const char* const n[] = {"destroyed", "disabled", "refire", "ammo", "power", "angle", "cruise", "lane", "cloak", "ok"};
    return r >= 0 && r <= 9 ? n[r] : "?";
}
static const char* GunPointRaw(const float pt[3], char* buf, int size)
{
    using Ctor = void*(__fastcall*)(void*, void*, int);
    using Trav = void*(__fastcall*)(void*, void*, void*);
    using FireFn = int(__thiscall*)(const void*, const float*);
    static const HMODULE common = GetModuleHandleA("common.dll");
    static const Ctor ctor = reinterpret_cast<Ctor>(GetProcAddress(common, "??0CEquipTraverser@@QAE@H@Z"));
    static const Trav trav =
        reinterpret_cast<Trav>(GetProcAddress(common, "?Traverse@CEquipManager@@QAEPAVCEquip@@AAVCEquipTraverser@@@Z"));
    static const GunPointFn point =
        reinterpret_cast<GunPointFn>(GetProcAddress(common, "?CanPointAt@CEGun@@QBE_NABVVector@@M@Z"));
    static const GunOriginFn origin =
        reinterpret_cast<GunOriginFn>(GetProcAddress(common, "?GetTurretOrigin@CEGun@@QBE?AVVector@@XZ"));
    static const GunRangeFn range =
        reinterpret_cast<GunRangeFn>(GetProcAddress(common, "?GetMunitionRange@CEGun@@QBEMXZ"));
    static const FireFn canFire =
        reinterpret_cast<FireFn>(GetProcAddress(common, "?CanFire@CEGun@@MBE?AW4FireResult@@ABVVector@@@Z"));
    void* ship = InSpace() ? PlayerCShip() : nullptr;
    if (!ctor || !trav || !point || !origin || !range || !canFire || !ship)
        return "gunpoint: not in space or the CEGun exports are missing";
    int used = sprintf_s(buf, size, "gunpoint at %.0f,%.0f,%.0f\n", pt[0], pt[1], pt[2]);
    __try
    {
        alignas(8) unsigned char traverser[32] = {};
        ctor(traverser, nullptr, kEquipClassGun);
        void* mgr = reinterpret_cast<unsigned char*>(ship) + kEqObjEquipManager;
        for (void* e = trav(mgr, nullptr, traverser); e && used < size - 200; e = trav(mgr, nullptr, traverser))
        {
            if (!IsClass(e, "??_7CEGun@@6B@"))
                continue;
            float o[3] = {};
            origin(e, o);
            const float d[3] = {pt[0] - o[0], pt[1] - o[1], pt[2] - o[2]};
            const char* hp = Mem<const char*>(reinterpret_cast<uintptr_t>(e) + kExternalHardpoint);
            const int w = _snprintf_s(buf + used, size - used, _TRUNCATE,
                                      "%s active=%d point=%d point5deg=%d fire=%s dist=%.0f reach=%.0f\n",
                                      hp && !IsBadStringPtrA(hp, 32) ? hp : "-",
                                      Mem<u8>(reinterpret_cast<uintptr_t>(e) + kEquipActive) ? 1 : 0, point(e, pt, 0) ? 1 : 0,
                                      point(e, pt, 0.087f) ? 1 : 0, FireResultName(canFire(e, pt)), sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]),
                                      range(e));
            if (w <= 0)
                break;
            used += w;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return "gunpoint: fault";
    }
    return buf;
}
std::string GunPointReport(const float pt[3])
{
    char buf[2048];
    return GunPointRaw(pt, buf, sizeof buf);
}

int PlayerCountermeasures()
{
    Weapon buf[4];
    const int n = InSpace() ? ReadWeapons(PlayerCShip(), buf, 4, 1 << 6) : 0; // EquipmentClass CM
    // Every mounted dropper counts by its ammo: the equip "active" flag reads 0 for droppers and launchers (guns 1) -
    // filtering on it made this -1 with a dropper of 28 aboard, and no countermeasure was ever dropped (M04 West Point
    // deaths to missiles, 20 Oct 2026; `equipall`).
    int ammo = -1;
    for (int i = 0; i < n; ++i)
        ammo = std::max(ammo, 0) + std::max(0, buf[i].ammo);
    return ammo;
}

int PlayerMines()
{
    // As the countermeasures: every mounted dropper by its ammo (the "active" flag reads 0 for droppers). FLHookSDK
    // EquipmentClass Mine = 1 << 5 (CM 1 << 6, Gun 1 << 7).
    Weapon buf[4];
    const int n = InSpace() ? ReadWeapons(PlayerCShip(), buf, 4, 1 << 5) : 0;
    int ammo = -1;
    for (int i = 0; i < n; ++i)
        ammo = std::max(ammo, 0) + std::max(0, buf[i].ammo);
    return ammo;
}
} // namespace space
