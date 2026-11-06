// The UI hooks two modules share (uihooks.h).
#include "all.h"

#include "uihooks.h"

#include "safemem.h"

namespace uihooks
{
// UiControlAdd / UiControlRemove: a count per call, nothing else (the game's thread only). Both detours or none: gated together (Generation 0 = unavailable).
typedef void(__cdecl* UiControlAddFn)(void* control);
typedef bool(__cdecl* UiControlRemoveFn)(void* control);
static UiControlAddFn g_origUiAdd = nullptr;
static UiControlRemoveFn g_origUiRemove = nullptr;
static u32 g_uiAdds = 0, g_uiRemoves = 0;

static void __cdecl UiControlAddHook(void* control)
{
    ++g_uiAdds;
    g_origUiAdd(control);
}

static bool __cdecl UiControlRemoveHook(void* control)
{
    ++g_uiRemoves;
    return g_origUiRemove(control);
}

u32 Generation() { return g_origUiAdd && g_origUiRemove ? 1 + g_uiAdds + g_uiRemoves : 0; }

void GenerationReply(char* reply, size_t len)
{
    const u32 gen = Generation();
    sprintf_s(reply, len, "uigen %u adds %u removes %u%s", gen, g_uiAdds, g_uiRemoves, gen ? "" : " (hooks not installed)");
}

static void InstallGeneration()
{
    if (!exe::UiControlAdd.ok() || !exe::UiControlRemove.ok())
    {
        Log("UI generation (Extended HUD 2) off: UiControlAdd / UiControlRemove unresolved");
        return;
    }
    // UiControlAdd: mov eax, [esp+4]; push eax (5 bytes). UiControlRemove: mov edx, [esp+4]; mov al, [edx+6Ch] (7 bytes)
    g_origUiAdd = static_cast<UiControlAddFn>(hk::Detour(exe::UiControlAdd, UiControlAddHook, 5, "UI generation (Extended HUD 2)"));
    if (g_origUiAdd)
        g_origUiRemove = static_cast<UiControlRemoveFn>(hk::Detour(exe::UiControlRemove, UiControlRemoveHook, 7, "UI generation (Extended HUD 2)"));
    if (g_origUiAdd && g_origUiRemove)
        Log("UI generation (Extended HUD 2): control add / remove hooked");
}

typedef bool(__cdecl* HandleKeyFn)(int cmd, int category);
static HandleKeyFn g_origHandleKey = nullptr;
static void (*g_onKeyCmd)(int cmd, bool down) = nullptr;
static bool g_traceKeys = false;

static bool __cdecl HandleKeyHook(int cmd, int category)
{
    const bool handled = g_origHandleKey(cmd, category);
    // the logic sees the key command after the game handled it (the maneuver lock's answer)
    if (g_onKeyCmd && cmd != fl::USER_MOVE)
        g_onKeyCmd(cmd, exe::KeyIsDown.ok() ? Mem<u8>(exe::KeyIsDown) != 0 : true);
    if (g_traceKeys && cmd != fl::USER_MOVE)
        Log("HandleKey %s(%d) cat=%d down=%d vk=0x%X -> %d", fl::CmdNameRaw(cmd), cmd, category, Mem<u8>(exe::KeyIsDown), Mem<u32>(exe::KeyCurVk), handled);
    return handled;
}

void Install(void (*onKeyCmd)(int cmd, bool down))
{
    g_onKeyCmd = onKeyCmd;
    if (g_origHandleKey)
        return;
    InstallGeneration();
    // HandleKey: sub esp, 80h (6 bytes)
    g_origHandleKey = static_cast<HandleKeyFn>(hk::Detour(exe::HandleKey, HandleKeyHook, 6, "key trace"));
    Log("Game hooks: key=%p", g_origHandleKey);
}

// ------------------------------------------------------------------ hits on the player ship (the HUD damage indicator)
// The game's damage indicator (HUD_DamageIndicator) gets every hit on the player ship as a world point: the source of
// a round that hit or of an explosion near the ship (exe::DmgIndSource) and contact hits (exe::DmgIndContact,
// collisions). It reduces them to 4 arcs; FLvibed keeps the point.
typedef void(__fastcall* DmgPointFn)(void* self, void* edx, const float* point);
static DmgPointFn g_origDmgSource = nullptr;
static DmgPointFn g_origDmgContact = nullptr;
static float g_hitPos[3];
static u32 g_hitKind = 0, g_hitTick = 0;
static volatile long g_hitSeq = 0;

static bool CopyPoint(const float* p, float out[3])
{
    __try
    {
        out[0] = p[0], out[1] = p[1], out[2] = p[2];
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static void RecordHit(const float* point, u32 kind)
{
    float p[3];
    const bool ok = point && CopyPoint(point, p);
    static int logged = 0;
    if (logged < 3)
    {
        ++logged;
        Log("damage indicator input kind %u point %p %s (%.0f, %.0f, %.0f)", kind, point, ok ? "read" : "unreadable",
            ok ? p[0] : 0.0f, ok ? p[1] : 0.0f, ok ? p[2] : 0.0f);
    }
    if (ok)
    {
        g_hitPos[0] = p[0], g_hitPos[1] = p[1], g_hitPos[2] = p[2];
        g_hitKind = kind;
        g_hitTick = GetTickCount();
        InterlockedIncrement(&g_hitSeq);
    }
}

static void __fastcall DmgSourceHook(void* self, void* edx, const float* point)
{
    RecordHit(point, 1);
    g_origDmgSource(self, edx, point);
}

static void __fastcall DmgContactHook(void* self, void* edx, const float* point)
{
    RecordHit(point, 2);
    g_origDmgContact(self, edx, point);
}

u32 PlayerHit(float pos[3], u32* kind, u32* tick)
{
    if (pos)
        pos[0] = g_hitPos[0], pos[1] = g_hitPos[1], pos[2] = g_hitPos[2];
    if (kind)
        *kind = g_hitKind;
    if (tick)
        *tick = g_hitTick;
    return static_cast<u32>(g_hitSeq);
}

void InstallHits()
{
    if (g_origDmgSource || g_origDmgContact)
        return;
    g_origDmgSource =
        static_cast<DmgPointFn>(hk::Detour(exe::DmgIndSource, DmgSourceHook, 6, "player hit points (shield cue)"));
    g_origDmgContact =
        static_cast<DmgPointFn>(hk::Detour(exe::DmgIndContact, DmgContactHook, 6, "player hit points (shield cue)"));
}

// ---- the target label's mute. The draw also plays the target sound and does more than draw, so it always runs: only its vertices and its text go.
typedef void(__fastcall* IndicatorDrawFn)(void* self, void* edx, void* obj, const float* rec);
typedef void(__cdecl* HudVertexFn)(float x, float y, float z);
typedef void(__fastcall* TextDrawFn)(void* self, void* edx);
static IndicatorDrawFn g_origIndicatorDraw = nullptr;
static HudVertexFn g_origHudVertex = nullptr;
static TextDrawFn g_origTextDraw = nullptr;
static int g_textTries = 0; // (a refused patch tries again, a few times; then the native target stays)
constexpr int kTextTries = 8;
static bool g_muteWanted = false, g_muting = false;
constexpr uintptr_t kIndicatorText = 0x40C;
constexpr int kTextDrawSlot = 0x34 / 4;

static void __cdecl HudVertexHook(float x, float y, float z)
{
    if (!g_muting) // (100 callers: one branch)
        g_origHudVertex(x, y, z);
}

static void __fastcall TextDrawHook(void* self, void* edx)
{
    if (!g_muting)
        g_origTextDraw(self, edx);
}

static void __fastcall IndicatorDrawHook(void* self, void* edx, void* obj, const float* rec)
{
    if (!g_origTextDraw && self && g_textTries < kTextTries)
    {
        // the text's class is known only at run time: its draw slot patched once, chained, gated by the flag. Before
        // the mute is wanted: TargetMuteReady answers yes only once this holds, so the corners never go without the label.
        uintptr_t text = 0;
        void** vt = nullptr;
        if (safemem::Read(reinterpret_cast<uintptr_t>(self) + kIndicatorText, &text, sizeof text) && text &&
            safemem::Read(text, &vt, sizeof vt) && vt)
        {
            ++g_textTries;
            g_origTextDraw = reinterpret_cast<TextDrawFn>(hk::PatchVtable(vt, kTextDrawSlot, reinterpret_cast<void*>(TextDrawHook)));
            if (g_origTextDraw || g_textTries == kTextTries)
                Log("target label: the text's draw slot hooked (vtable %p): %s", vt, g_origTextDraw ? "ok" : "REFUSED");
        }
    }
    g_muting = g_muteWanted && g_origTextDraw; // (never half a label)
    g_origIndicatorDraw(self, edx, obj, rec);
    g_muting = false;
}

void InstallTargetMute()
{
    if (g_origHudVertex)
        return;
    // HudVertex: mov ecx,ds:[...] (6 bytes); HudIndicatorDraw: mov eax,fs:[0] (6 bytes). The vertex gate first: the indicator hook mutes only with it in.
    g_origHudVertex = static_cast<HudVertexFn>(hk::Detour(exe::HudVertex, HudVertexHook, 6, "HUD 2's targeting display"));
    if (g_origHudVertex)
        g_origIndicatorDraw = static_cast<IndicatorDrawFn>(hk::Detour(exe::HudIndicatorDraw, IndicatorDrawHook, 6, "HUD 2's targeting display"));
}

void SetTargetMute(bool on) { g_muteWanted = on; }
bool TargetMuteReady() { return g_origHudVertex && g_origIndicatorDraw && g_origTextDraw; }

void TraceKeys(bool on) { g_traceKeys = on; }
bool TraceKeys() { return g_traceKeys; }
} // namespace uihooks
