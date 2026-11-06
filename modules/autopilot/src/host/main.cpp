// The Autopilot module's host part: the hooks only it needs. Core headers and `Log` come from build.cmd's :wrappart. The guns' aim point, the
// jump-in count and the Evade guard reach the logic through hostcalls (router::Ap*): off, none of them is installed and the calls fail closed.
#include "flexe.h"
#include "hook.h"

// The guns' aim point. Every player gun aims at the world point exe::GunAimPoint returns ([this+0x91C], from the cursor ray through the
// current camera); while the logic sets one (the autopilot flying in turret view) the getter returns that instead, so the guns follow the
// autopilot whatever the player's camera or cursor do.
typedef void*(__fastcall* GunAimFn)(void* self, void* edx, float* out);
static GunAimFn g_origGunAim = nullptr;
static bool g_gunAimOn = false;
static float g_gunAim[3] = {};

static void* __fastcall GunAimHook(void* self, void* edx, float* out)
{
    if (!g_gunAimOn || !out)
        return g_origGunAim(self, edx, out);
    out[0] = g_gunAim[0], out[1] = g_gunAim[1], out[2] = g_gunAim[2];
    return out;
}

static bool SetGunAim(bool on, const float* world)
{
    if (!g_origGunAim)
        return false;
    if (on && world && std::isfinite(world[0]) && std::isfinite(world[1]) && std::isfinite(world[2]))
    {
        g_gunAim[0] = world[0], g_gunAim[1] = world[1], g_gunAim[2] = world[2];
        g_gunAimOn = true;
    }
    else
        g_gunAimOn = false;
    return true;
}

// exe::JumpInComplete: u32 thiscall(this, u32 system) ret 4: the client tells the server the player's jump-in is done.
typedef u32(__fastcall* JumpInCompleteFn)(void* self, void* edx, u32 system);
static JumpInCompleteFn g_origJumpIn = nullptr;
static volatile long g_jumpIns = 0;

static u32 __fastcall JumpInCompleteHook(void* self, void* edx, u32 system)
{
    InterlockedIncrement(&g_jumpIns);
    return g_origJumpIn(self, edx, system);
}

static int JumpInCount() { return g_origJumpIn ? static_cast<int>(g_jumpIns) : -1; }

// The Evade behaviour's update (vtable slot 7) checks that its threat ([this+0x2A0], resolved from the directive's
// object id) still exists only for NPC ships; for the player ship it skips the check and dereferences the null
// threat (0x62E4500, Common.dll+0x84509) once the threat is gone or was never found. The guard finishes the
// behaviour the way the game does for NPC ships ([this+0x10] = 2, return 2). docs\FREELANCER_RE.md "AI on the
// player ship". The signature is the update's start up to the threat test (HD Edition 0.7.1 = 1.1 Common.dll).
static const char kEvadeUpdateSig[] = "83 EC 1C 55 56 8B F1 8B 46 24 05 FC 14 00 00 8B 80 84 00 00 00 8B 10 57 "
                                      "8B C8 FF 92 BC 00 00 00 84 C0 75 ?? 8B 86 A0 02 00 00 85 C0 0F 84";
constexpr int kEvadeUpdateSlot = 7;
constexpr int kEvadeThreat = 0x2A0;
constexpr int kEvadeResult = 0x10;
typedef int(__fastcall* EvadeUpdateFn)(void* self, void* edx);
static EvadeUpdateFn g_origEvadeUpdate = nullptr;
static void** g_evadeVtable = nullptr;
// The guard sits on the class's vtable, so it sees every ship's Evade (the game finishes NPC ones the same way), and a
// behaviour manager calls a finished Evade again every frame until the next directive replaces it (seen 25 / 26 Oct
// 2026: two behaviours alternating, ~100 calls a second each). Each finish is logged once: the behaviours finished and
// already logged are remembered until they run with a threat again (a few at a time; at most 20 lines per game).
static void* g_evadeFinished[8] = {};
static int g_evadeLogged = 0;

static int __fastcall EvadeUpdateGuard(void* self, void* edx)
{
    u8* b = static_cast<u8*>(self);
    const uintptr_t threat = *reinterpret_cast<uintptr_t*>(b + kEvadeThreat);
    if (threat == 0 || threat == 8) // the game's own test: no watcher, or a watcher of no object
    {
        bool known = false;
        for (void* p : g_evadeFinished)
            known = known || p == self;
        if (!known)
        {
            static unsigned next = 0;
            g_evadeFinished[next++ % 8] = self;
            if (g_evadeLogged < 20 && ++g_evadeLogged)
                Log("evade guard: threat gone, behaviour %p finished", self);
        }
        *reinterpret_cast<int*>(b + kEvadeResult) = 2;
        return 2;
    }
    for (void*& p : g_evadeFinished)
        if (p == self)
            p = nullptr; // running again with a threat: its next finish is logged
    return g_origEvadeUpdate(self, edx);
}

static bool InCommonCode(const void* p, size_t len)
{
    HMODULE common = GetModuleHandleA("common.dll");
    MEMORY_BASIC_INFORMATION mbi = {};
    if (!common || !VirtualQuery(p, &mbi, sizeof(mbi)) || mbi.AllocationBase != common)
        return false;
    return static_cast<const u8*>(p) + len <= static_cast<const u8*>(mbi.BaseAddress) + mbi.RegionSize;
}

static bool GuardEvade(void** vtable)
{
    if (!vtable)
        return false;
    if (vtable == g_evadeVtable)
        return true;
    if (g_evadeVtable)
        return false; // one Evade class only
    if (!InCommonCode(vtable, sizeof(void*) * (kEvadeUpdateSlot + 1)))
        return false;
    const u8* fn = static_cast<const u8*>(vtable[kEvadeUpdateSlot]);
    if (!InCommonCode(fn, 1))
    {
        // Another mod owns the update (FLUF's EvadeBehaviorHooks re-implements it from its DllMain and throws on a
        // vtable someone patched first): FLvibed leaves the slot alone and the autopilot dodges with its own slide
        // bursts and breaks (autopilot_combat.cpp: `dodge` 2 falls back). Decided once, logged once.
        static bool logged = false;
        if (!logged)
        {
            logged = true;
            HMODULE owner = nullptr;
            char path[MAX_PATH] = "?";
            if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                   reinterpret_cast<LPCSTR>(fn), &owner))
                GetModuleFileNameA(owner, path, MAX_PATH);
            const char* base = strrchr(path, '\\');
            Log("evade guard: the Evade update belongs to %s%s - not installed; the autopilot uses its own dodges "
                "(slide bursts, breaks) instead of the game's Evade",
                base ? base + 1 : path, hk::IsRestoreStyle(reinterpret_cast<uintptr_t>(fn)) ? " (FLUF's own Evade)" : "");
        }
        return false;
    }
    std::vector<u8> bytes, mask;
    if (!hk::ParseSig(kEvadeUpdateSig, bytes, mask) || !InCommonCode(fn, bytes.size()))
        return false;
    for (size_t i = 0; i < bytes.size(); ++i)
        if (mask[i] && fn[i] != bytes[i])
        {
            Log("evade guard: update code differs at +%u, not installed (Evade stays off)", static_cast<u32>(i));
            return false;
        }
    hk::Owner own(router::CoreOwner());
    g_origEvadeUpdate = reinterpret_cast<EvadeUpdateFn>(
        hk::PatchVtable(vtable, kEvadeUpdateSlot, reinterpret_cast<void*>(EvadeUpdateGuard)));
    if (!g_origEvadeUpdate)
        return false; // the slot was not writable (logged; Evade stays off)
    g_evadeVtable = vtable;
    Log("evade guard installed (vtable %p, update %p)", vtable, fn);
    return true;
}

void Start()
{
    g_logChannel = router::LogChannel(router::LogChannel(router::kController) >= 0 ? router::kController : router::kSubtitles);
    hk::Owner own(router::CoreOwner());
    // JumpInComplete: mov eax, ds:[0067ECD0] (5 bytes)
    g_origJumpIn = static_cast<JumpInCompleteFn>(
        hk::Detour(exe::JumpInComplete, JumpInCompleteHook, 5, "jump-in complete (autopilot travel)"));
    uibtn::InstallBar(); // the bar's button; the Options screen's and the pause menu's are the host core's
    // GunAimPoint: mov eax,[esp+4]; add ecx,91Ch (10 bytes, no relative addressing)
    g_origGunAim = static_cast<GunAimFn>(hk::Detour(exe::GunAimPoint, GunAimHook, 10, "the autopilot's guns in turret view"));
    static const router::ApHost host = {JumpInCount, GuardEvade, SetGunAim};
    router::SetApHost(&host);
}
