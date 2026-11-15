// The UI hooks two modules share (uihooks.h).
#include "all.h"

#include "uihooks.h"

#include "game/zonemix.h"
#include "input.h"
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
    // a physical key's press under a full-screen window flies nothing (its release goes on; fl::FireCommand, FLvibed's own, has no key)
    if (fl::FlightCommand(cmd) && input::FlightLocked() && exe::KeyIsDown.ok() && exe::KeyCurVk.ok() && Mem<u8>(exe::KeyIsDown) && Mem<u32>(exe::KeyCurVk))
        return false;
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

// ------------------------------------------------------------------ the nav map's zones (exe::NavMapZoneBuild; issue 61)
// The game draws every zone of the political and patrol layers as one triangle list, each zone in its palette's colour by the averaged feeling (0x45A650:
// hostile red, friendly green, neutral white), alpha 0xFF. Its filter (exe::NavMapZoneFilter) keeps one class: ALL (0) only the neutral zones, then hostile (1)
// and friendly (2); any other value keeps every zone. On HUD 2's map ALL shows every zone (the owner, issue 61): the build runs with the filter at kEveryZone,
// and every zone's outer edges are kept for an outline.
// A zone's colour is the AVERAGE of its listed factions' feelings, so an inhabited zone (police, friendly, and a dozen trading houses, neutral) comes out
// neutral white whatever share the police have (the owner: "a white blob", "opacity wasn't the cause"). HUD 2 shows each zone's MIX instead: at the build's
// one palette call per zone (exe::NavMapPalette, 0x49CE39) the zone's factions (vt[0x64] count, vt[0x68] the i-th group and weight) are read again and
// summed by the player's relation to each (Reputation::Vibe::GetGroupFeelingsTowards, the game's own call at 0x49CCC5; core\game\zonemix.h). Each
// zone takes its dominant relation's colour (neutral faint), outlined in it; neutral first, friendly and hostile on top. On the political layer, once HUD 2's
// influence cells for the shown system are ready (the logic says so: kZoneCells), the game's fill and the outlines give way to the cells (modules\hud\src\
// mapcells.cpp); the zones the build drew are kept (MapZoneIds) so the cells show only where the map shows a zone. The single filters stay the game's own.
constexpr int kEveryZone = 3;       // a filter value no button sets: the build's test (0x49CD3B) then keeps every zone
constexpr u8 kNeutralAlpha[2] = {0x30, 0x90}; // the neutral fill's alpha on HUD 2's map: political (wide overlapping areas), patrol (thin paths); the game's is 0xFF
constexpr int kMaxZoneEdges = 4096; // outer edges kept for the outline (the game caps the list at 6000 vertices, 0x49D98E)
constexpr int kMaxZones = 1024;
constexpr u32 kBuiltFilter = 0xF04; // the build's last filter: it rebuilds when the filter differs (decompiled 15 Nov 2026)
typedef u32(__fastcall* ZoneBuildFn)(void* self, void* edx); // (the draw reads al after the call: kept as the game returned it)
typedef void(__cdecl* PaletteFn)(u32* colour, float feeling);
typedef int(__cdecl* FeelingsFn)(const int& vibe, const u32& group, float& feeling);
typedef int(__thiscall* ZoneCountFn)(void* zone);
typedef bool(__thiscall* ZoneFactionFn)(void* zone, int i, u32* group, float* weight);
static ZoneBuildFn g_origZoneBuild = nullptr;
static PaletteFn g_origPalette = nullptr;
static FeelingsFn g_feelings = nullptr;
static bool g_zonesWanted = false;
static DWORD g_zonesAsked = 0;
static int g_loggedEdges = -1;
static float g_zoneEdges[kMaxZoneEdges * 7]; // a segment: x, y, z of each end as the builder left them, then the D3DCOLOR's bits
static int g_nZoneEdges = 0;
static int g_zoneFlags = 0;           // the last ask's flags (kZoneCells)
static int g_builtCells = -1;         // the flags the zone list was last processed with (-1: none)
static u32 g_drawnIds[kMaxZones];     // the political zones the last HUD 2 build drew (zone vt[0x4]: CreateID of the nickname)
static int g_nDrawnIds = 0;
static u32 g_buildIds[kMaxZones];
static int g_nBuildIds = 0;
static int g_vibe = 0;                // the player's reputation vibe the build read (0x4C4670's result)
static u32 g_relColour[3] = {0xFF00FF00u, 0xFFFFFFFFu, 0xFFFF0000u}; // the palette's colours by relation (filled from the game's palette at each build)
struct ZoneRec
{
    int first;                          // the zone's first vertex (+0xEE8 when the palette is asked: the build appends each zone after its colour)
    zonemix::Mix mix;
};
static ZoneRec g_zoneRecs[kMaxZones];
static int g_nZoneRecs = 0;
static u8* g_building = nullptr; // the map whose zones are being built for HUD 2's map (the palette hook records only then)
static int g_mixFaults = 0;

// The build's frame at its palette call (cdecl(u32* colour, float f) with colour = esp + 0x38 of the build): the zone at esp + 0x1180 (the ESI of the
// faction loop, 0x49CC01 / 0x49CC1B), the player's reputation vibe at esp + 0xF4 (0x4C4670's result, 0x49CB9C). Decompiled 15 Nov 2026 (issue 61).
static void __cdecl PaletteHook(u32* colour, float feeling)
{
    g_origPalette(colour, feeling);
    if (!g_building || !g_feelings || g_nZoneRecs >= kMaxZones)
        return;
    __try
    {
        const u8* frame = reinterpret_cast<const u8*>(colour) - 0x38;
        void* zone = *reinterpret_cast<void* const*>(frame + 0x1180);
        const int vibe = *reinterpret_cast<const int*>(frame + 0xF4);
        void* const* vt = *reinterpret_cast<void* const* const*>(zone);
        float acc[3] = {};
        const int n = reinterpret_cast<ZoneCountFn>(vt[0x64 / 4])(zone);
        for (int i = 0; i < n && i < 512; ++i)
        {
            u32 group = 0;
            float weight = 0, feel = 0;
            if (reinterpret_cast<ZoneFactionFn>(vt[0x68 / 4])(zone, i, &group, &weight) && g_feelings(vibe, group, feel) == 0)
                zonemix::Add(acc, feel, weight);
        }
        ZoneRec& r = g_zoneRecs[g_nZoneRecs++];
        r.first = *reinterpret_cast<const int*>(g_building + 0xEE8);
        r.mix = zonemix::Shares(acc);
        if (vibe)
            g_vibe = vibe;
        if (g_nBuildIds < kMaxZones)
            g_buildIds[g_nBuildIds++] = reinterpret_cast<u32(__thiscall*)(void*)>(vt[0x4 / 4])(zone); // the zone's id (CreateID of its nickname; the dump of 15 Nov 2026)
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        ++g_mixFaults;
    }
}

// Each vertex's zone (the palette hook's records), the triangles sorted neutral first, then friendly / hostile (stable), each zone's outline: the edges only
// one triangle has (a fan's spokes and a band's seams are shared; every zone has vertices of its own, so zones never share an edge), in its dominant
// relation's colour; last the vertices' colours: the dominant relation's (neutral faint). `cells`: HUD 2's influence cells stand in for the fill (alpha 0,
// no outline).
static void SortAndOutline(u8* map, int layer, bool cells)
{
    const u32 verts = *reinterpret_cast<u32*>(map + 0xEE0), idx = *reinterpret_cast<u32*>(map + 0xEE4);
    const int nv = *reinterpret_cast<int*>(map + 0xEE8), ni = *reinterpret_cast<int*>(map + 0xEEC);
    g_nZoneEdges = 0;
    if (!verts || !idx || nv <= 0 || nv > 6000 || ni <= 0 || ni > 6000 * 6)
        return;
    u8* v = reinterpret_cast<u8*>(verts);
    u16* ix = reinterpret_cast<u16*>(idx);
    static short zoneOf[6000];
    for (int i = 0; i < nv; ++i)
        zoneOf[i] = -1;
    for (int k = 0; k < g_nZoneRecs; ++k)
        for (int i = std::max(0, g_zoneRecs[k].first), end = k + 1 < g_nZoneRecs ? g_zoneRecs[k + 1].first : nv; i < end && i < nv; ++i)
            zoneOf[i] = static_cast<short>(k);
    auto mixOf = [&](int i) -> const zonemix::Mix* { return i < nv && zoneOf[i] >= 0 ? &g_zoneRecs[zoneOf[i]].mix : nullptr; };
    // the zone's dominant relation (a vertex of no record: by the game's colour, white = neutral)
    auto relOf = [&](int i) {
        const zonemix::Mix* m = mixOf(i);
        if (!m)
            return i < nv && (*reinterpret_cast<const u32*>(v + i * 16 + 12) & 0xFFFFFFu) == 0xFFFFFFu ? static_cast<int>(zonemix::kNeutral) : -1;
        return zonemix::Dominant(*m);
    };
    if (cells)
    {
        for (int i = 0; i < nv; ++i)
            *reinterpret_cast<u32*>(v + i * 16 + 12) &= 0xFFFFFFu; // the cells are all of it
        return;
    }
    static u16 sorted[6000 * 6];
    int ns = 0;
    for (int pass = 0; pass < 2; ++pass)
        for (int t = 0; t + 2 < ni; t += 3)
            if ((relOf(ix[t]) == zonemix::kNeutral) == (pass == 0))
                sorted[ns] = ix[t], sorted[ns + 1] = ix[t + 1], sorted[ns + 2] = ix[t + 2], ns += 3;
    memcpy(ix, sorted, ns * sizeof(u16));
    // every triangle's edges as (low, high) index pairs; an edge seen twice is inside a zone
    static u32 edges[6000 * 6];
    int ne = 0;
    for (int t = 0; t + 2 < ns; t += 3)
    {
        const int tri[3] = {ix[t], ix[t + 1], ix[t + 2]};
        if (tri[0] >= nv || tri[1] >= nv || tri[2] >= nv)
            continue;
        for (int k = 0; k < 3; ++k)
        {
            const u32 p = static_cast<u32>(tri[k]), q = static_cast<u32>(tri[(k + 1) % 3]);
            edges[ne++] = p < q ? (p << 16) | q : (q << 16) | p;
        }
    }
    std::sort(edges, edges + ne);
    for (int i = 0; i < ne && g_nZoneEdges < kMaxZoneEdges;)
    {
        int j = i + 1;
        while (j < ne && edges[j] == edges[i])
            ++j;
        if (j - i == 1)
        {
            const float* p = reinterpret_cast<const float*>(v + (edges[i] >> 16) * 16);
            const float* q = reinterpret_cast<const float*>(v + (edges[i] & 0xFFFF) * 16);
            float* out = g_zoneEdges + g_nZoneEdges++ * 7;
            out[0] = p[0], out[1] = p[1], out[2] = p[2], out[3] = q[0], out[4] = q[1], out[5] = q[2];
            const zonemix::Mix* m = mixOf(edges[i] >> 16);
            memcpy(out + 6, m ? &g_relColour[zonemix::Dominant(*m)] : static_cast<const void*>(p + 3), 4); // the dominant relation's colour
        }
        i = j;
    }
    const u8 faint = kNeutralAlpha[layer == 2 ? 1 : 0];
    for (int i = 0; i < nv; ++i)
    {
        u32& c = *reinterpret_cast<u32*>(v + i * 16 + 12);
        const int rel = relOf(i);
        if (rel == zonemix::kNeutral)
            c = (g_relColour[rel] & 0xFFFFFFu) | (static_cast<u32>(faint) << 24);
        else if (rel >= 0)
            c = (g_relColour[rel] & 0xFFFFFFu) | (c & 0xFF000000u); // a mixed zone in its dominant relation's colour
    }
}

static u32 __fastcall ZoneBuildHook(void* self, void* edx)
{
    // (asked by every frame of HUD 2's map; the game may build more than once a frame, so an ask holds for a moment; on the classic HUD nobody asks)
    const bool wanted = g_zonesWanted && GetTickCount() - g_zonesAsked < 500;
    int layer = 0, filter = -1;
    int* filterAt = exe::NavMapZoneFilter.ok() ? reinterpret_cast<int*>(exe::NavMapZoneFilter.get()) : nullptr;
    const bool every = filterAt && safemem::Read(reinterpret_cast<u32>(filterAt), &filter, 4) && filter == 0;
    if (!wanted || !every || !exe::NavMapMode.ok() || !safemem::Read(static_cast<u32>(exe::NavMapMode.get()), &layer, 4) || (layer != 1 && layer != 2))
    {
        // the single filters, the other layers and the classic HUD: the game's own (a list HUD 2 processed is rebuilt: the filter differs)
        g_nZoneEdges = 0, g_builtCells = -1;
        return g_origZoneBuild(self, edx);
    }
    const bool cells = layer == 1 && (g_zoneFlags & kZoneCells);
    if (g_builtCells != (cells ? 1 : 0))
        *reinterpret_cast<int*>(static_cast<u8*>(self) + kBuiltFilter) = -1; // the cells came or went: the list is built afresh, in the game's colours
    *filterAt = kEveryZone; // (the build keeps its last filter at +0xF04 and rebuilds when it differs, so the classic HUD's ALL rebuilds the game's own)
    if (g_origPalette)
        for (int r = 0; r < 3; ++r)
        {
            // the palette's bytes are R, G, B, A in memory; the build packs them into the vertex's D3DCOLOR (A R G B) at 0x49D963
            u32 c = 0;
            g_origPalette(&c, r == zonemix::kFriendly ? 1.0f : r == zonemix::kHostile ? -1.0f : 0.0f);
            g_relColour[r] = (c & 0xFF000000u) | ((c & 0xFFu) << 16) | (c & 0xFF00u) | ((c >> 16) & 0xFFu);
        }
    const int kept = g_nZoneRecs;
    g_nZoneRecs = 0, g_nBuildIds = 0, g_building = static_cast<u8*>(self);
    const u32 built = g_origZoneBuild(self, edx);
    g_building = nullptr;
    *filterAt = filter;
    // the build reuses its list while nothing changed (+0xF04): no zone recorded but vertices there = the list already processed
    if (g_nZoneRecs == 0 && *reinterpret_cast<int*>(static_cast<u8*>(self) + 0xEE8) > 0)
    {
        g_nZoneRecs = kept;
        return built;
    }
    if (layer == 1)
        memcpy(g_drawnIds, g_buildIds, g_nBuildIds * sizeof(u32)), g_nDrawnIds = g_nBuildIds;
    g_builtCells = cells ? 1 : 0;
    __try
    {
        SortAndOutline(static_cast<u8*>(self), layer, cells);
        const int logKey = g_nZoneEdges * 8 + layer * 2 + (cells ? 1 : 0);
        if (logKey != g_loggedEdges)
        {
            g_loggedEdges = logKey;
            Log("map zones: layer %d, every zone, %s, %d vertices, %d zones (%d ids), %d outline edges, %d faults; colours %08X %08X %08X", layer,
                cells ? "HUD 2's cells stand in for the fill" : "dominant-relation fill", *reinterpret_cast<int*>(static_cast<u8*>(self) + 0xEE8), g_nZoneRecs, g_nBuildIds,
                g_nZoneEdges, g_mixFaults, g_relColour[0], g_relColour[1], g_relColour[2]);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        g_nZoneEdges = 0;
    }
    return built;
}

void InstallMapZones()
{
    if (g_origZoneBuild)
        return;
    // NavMapZoneBuild: mov eax,1174h (5 bytes; the stack probe's relative call follows it)
    g_origZoneBuild = static_cast<ZoneBuildFn>(hk::Detour(exe::NavMapZoneBuild, ZoneBuildHook, 5, "HUD 2's map zones"));
    // NavMapPalette: fld [esp+8]; fcomp [abs] (4 + 6 bytes). The palette is shared with the HUD: the hook passes through unless a HUD 2 build runs.
    g_origPalette = static_cast<PaletteFn>(hk::Detour(exe::NavMapPalette, PaletteHook, 10, "HUD 2's map zone mix"));
    g_feelings = reinterpret_cast<FeelingsFn>(GetProcAddress(GetModuleHandleA("common.dll"), "?GetGroupFeelingsTowards@Vibe@Reputation@@YAHABHABIAAM@Z"));
}

void SetQuietZones(int flags) { g_zonesWanted = true, g_zonesAsked = GetTickCount(), g_zoneFlags = flags; }

int MapZoneEdges(float* xz, int maxSegments)
{
    const int n = std::min(g_nZoneEdges, maxSegments);
    if (xz && n > 0)
        memcpy(xz, g_zoneEdges, n * 7 * sizeof(float));
    return n;
}

int MapZoneIds(u32* ids, int maxIds, int* vibe, u32* colours)
{
    if (colours)
        memcpy(colours, g_relColour, sizeof(g_relColour));
    if (vibe)
        *vibe = g_vibe;
    const int n = std::min(g_nDrawnIds, maxIds);
    if (ids && n > 0)
        memcpy(ids, g_drawnIds, n * sizeof(u32));
    return n;
}

void SetTargetMute(bool on) { g_muteWanted = on; }
bool TargetMuteReady() { return g_origHudVertex && g_origIndicatorDraw && g_origTextDraw; }

void TraceKeys(bool on) { g_traceKeys = on; }
bool TraceKeys() { return g_traceKeys; }
} // namespace uihooks
