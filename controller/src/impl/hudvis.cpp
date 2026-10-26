#include "hudvis.h"

#include "debug.h"
#include "flexe.h"
#include "game/hudinfo.h"
#include "log.h"
#include "ui.h"

#include <sstream>

namespace hudvis
{
constexpr u32 kParent = 0x04, kChild = 0x0C, kSibling = 0x08, kName = 0x18, kState = 0x6C;

struct Root
{
    const char* child; // the named control the root is found through
    const char* label; // for the status line
    // while this returns true the root stays shown (restored for that time): a native line HUD 2 does not show in full,
    // a window the game opens inside the root
    bool (*keep)(const struct Root&) = nullptr;
    bool self = false; // the named control itself (a child), not its top-level parent
    uintptr_t ctl = 0; // the cached root, 0 = not found
    u32 vtable = 0;
    char name[33] = {};
    bool hidden = false; // the switch cleared its bits
    u8 saved = 0;        // the visible bits the game wanted (restored on the way back)
    u32 rewrites = 0;    // frames the game had shown it again (the per-frame re-hide)
    uintptr_t aux[8] = {}; // controls under the root its keep() reads (cached with the root)
    int nAux = 0;
};

// The cruise line "Cruise Disrupted - n" (the seconds until the cruise engine can charge again) carries what HUD 2's
// 3 s alert card does not: it stays native for as long as the engine's charge clock is negative (sampled at 10 Hz).
static bool CruiseDisruptedNow(const Root&)
{
    static bool cache = false;
    static DWORD at = 0;
    const DWORD now = GetTickCount();
    if (now - at >= 100)
        at = now, cache = hudinfo::CruiseDisrupted(1.0e6f);
    return cache;
}

// The NeuroNet windows (the nav map, inventory, mission log, player info, help) are children of the NeuroNet box's
// switch buttons (NNNavmapButton > NeuroNetNavMap ..., verified 25 Oct 2026): hiding NN_MainButton hid an opened map
// too. A window is open while its switch button (NN<name>Button, not ...ButtonRoot) is shown (bit 1): the root then
// stays shown - the NeuroNet box with the window - until it closes. Five cached byte reads a frame.
static bool NeuroNetWindowOpen(const Root& r)
{
    for (int i = 0; i < r.nAux; ++i)
    {
        u8 st = 0;
        if (debug::SafeRead(r.aux[i] + kState, &st, 1) && (st & 2))
            return true;
    }
    return false;
}

// The switch buttons under the NeuroNet root (direct children named NN...Button)
static void FindNeuroNetButtons(Root& r)
{
    r.nAux = 0;
    u32 c = 0;
    debug::SafeRead(r.ctl + kChild, &c, 4);
    for (int guard = 0; c && guard < 32 && r.nAux < 8; ++guard)
    {
        char n[33] = {};
        if (debug::SafeRead(c + kName, n, 32))
        {
            const size_t len = strlen(n);
            if (len > 8 && strncmp(n, "NN", 2) == 0 && strcmp(n + len - 6, "Button") == 0)
                r.aux[r.nAux++] = c;
        }
        if (!debug::SafeRead(c + kSibling, &c, 4))
            break;
    }
}

// The chat window (MsgWin) stays shown while the player types into it (the chat input CTID shown; looked up at 4 Hz:
// it exists only while open).
static bool ChatInputOpen(const Root&)
{
    static bool cache = false;
    static DWORD at = 0;
    const DWORD now = GetTickCount();
    if (now - at >= 250)
    {
        at = now;
        u8 st = 0;
        const uintptr_t c = ui::FindControl("CTID");
        cache = c && debug::SafeRead(c + kState, &st, 1) && (st & 2);
    }
    return cache;
}

// The game's hover help - the mouse-over label (Rollover: "Rogues - Ken Norris" beside the cursor) and the mouse-over box
// (ToolTip: "NeuroNet", "Police Base / Fort Bush") - only for an intentional hover (owner, 25 Oct 2026; SetHoverHelp,
// decided by HUD 2's frame from the device service: in flight a real mouse move within the last seconds; menus, the
// NeuroNet windows and the bases keep the game's own). Otherwise, in both HUD styles, each frame at the UI pass's entry
// (after the game's update started it, before anything draws) the shown help is hidden the game's own way (0x4BE390:
// state bit 1 off, the ToolTip's showing flag +0x159 = 0, so its hover timer starts over instead of re-showing it the
// next frame): it never reaches the screen. They are not restored: the game shows them again on the next real hover.
static bool g_kbmForm = false;
static bool KeyboardForm(const Root&) { return g_kbmForm; }
void SetKeyboardForm(bool kbm) { g_kbmForm = kbm; }
static bool g_hoverAllowed = true;
static u32 g_hoverHides = 0;
void SetHoverHelp(bool allowed) { g_hoverAllowed = allowed; }

// A modal menu over the flight (SetMenuOver): every flight root hidden, the menus' mouse-over label / box (the roots
// kept by KeyboardForm) as the game wants them
static bool g_menuOver = false;
static u32 g_menuFrames = 0;
void SetMenuOver(bool on) { g_menuOver = on; }

static Root g_roots[] = {
    {"ManeuverButton0", "maneuvers"},
    {"NNNavmapButtonRoot", "neuronet", NeuroNetWindowOpen},
    {"TargetCloseButton", "target"},
    {"AllFilterButton", "contactlist"},
    {"HudRepairButton", "status"},
    {"HUD_Core", "gauges"},
    // the cruise engine's line under the reticle ("CRUISE ENGINE CHARGING - WEAPONS OFFLINE - n%" while it charges):
    // its own top-level control HUD_CruiseProgress, which draws its text child itself (0x4D5EB0)
    {"HUD_CruiseProgress", "cruise", CruiseDisruptedNow},
    // the chat window (the round 3 critique: its lines at ~10 px from the edge beside HUD 2's comm feed - two comm
    // surfaces at once): the subtitles module draws the chatter in HUD 2's band while HUD 2 shows (display.cpp
    // DrawHud2Feed); the window itself returns with the game's HUD, and while the player types
    {"MsgWin", "chat", ChatInputOpen},
    // the game's damage arcs (~0.22 H around the reticle; the round 3 critique: they compete with the ring): HUD 2 shows a
    // hit's side itself (the amber flash at 0.13 H from the same hit, host v12 PlayerHit)
    {"HUD_DamageIndicator", "damage"},
    // the hover help (ApplyHoverHelp, not the HUD 2 switch): the mouse-over label (top-level Rollover, vt 0x5D5454) and
    // the mouse-over box (top-level ToolTip, vt 0x5D5F7C)
    {"Rollover", "rollover", KeyboardForm},
    {"ToolTip", "tooltip", KeyboardForm},
    // the game's red missile warning (Targetable_Objects > MissileAlert, blinking at the classic target panel's corner -
    // over HUD 2's card, the known bug): HUD 2's MISSILE INCOMING card and the ring's chevrons carry it. Only this child:
    // its parent draws the brackets and arrows (kept native)
    {"MissileAlert", "missilealert", nullptr, true},
};
static DWORD g_lastFind = 0, g_lastNameCheck = 0;
static u32 g_finds = 0, g_drops = 0;

// The system arrival line (NN_ArrivalText, one per showing, exe::ArrivalText): hidden only when HUD 2 says it shows
// the same line (SetHideArrival) - its "..., Liberty Space." and its showing at a launch are not in HUD 2's place line.
struct Arrival
{
    bool want = false;   // HUD 2 asked for it to be hidden
    uintptr_t ctl = 0;   // the instance last hidden
    u8 saved = 0;
    u32 hides = 0;
} g_arrival;

static bool ReadState(uintptr_t ctl, u8& state) { return debug::SafeRead(ctl + kState, &state, 1); }

static bool WriteState(uintptr_t ctl, u8 state)
{
    __try
    {
        *reinterpret_cast<volatile u8*>(ctl + kState) = state;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

// The cached root is still the control it was: its vtable (and, when `name`, its name) unchanged.
static bool Valid(const Root& r, bool name)
{
    if (!r.ctl)
        return false;
    u32 vt = 0;
    if (!debug::SafeRead(r.ctl, &vt, 4) || vt != r.vtable)
        return false;
    if (name)
    {
        char n[33] = {};
        if (!debug::SafeRead(r.ctl + kName, n, 32) || strcmp(n, r.name) != 0)
            return false;
    }
    return true;
}

static void Drop(Root& r)
{
    r.ctl = 0, r.vtable = 0, r.hidden = false, r.name[0] = 0, r.nAux = 0;
    ++g_drops;
}

static void Find(Root& r)
{
    uintptr_t c = ui::FindControl(r.child);
    for (int guard = 0; c && guard < 16 && !r.self; ++guard)
    {
        u32 parent = 0;
        if (!debug::SafeRead(c + kParent, &parent, 4))
            return;
        if (!parent)
            break;
        c = parent;
    }
    if (!c || !debug::SafeRead(c, &r.vtable, 4) || !debug::SafeRead(c + kName, r.name, 32))
        return;
    r.name[32] = 0;
    r.ctl = c;
    r.hidden = false;
    r.nAux = 0;
    if (r.keep == NeuroNetWindowOpen)
        FindNeuroNetButtons(r);
    ++g_finds;
}

static void Restore(Root& r)
{
    if (!r.hidden)
        return;
    u8 st = 0;
    if (Valid(r, true) && ReadState(r.ctl, st))
        WriteState(r.ctl, static_cast<u8>(st | r.saved));
    r.hidden = false;
}

// ---- the arrival line
static uintptr_t ArrivalNow()
{
    if (!exe::ArrivalText.ok())
        return 0;
    u32 p = 0;
    debug::SafeRead(exe::ArrivalText.get(), &p, 4);
    return p;
}

static void RestoreArrival()
{
    if (!g_arrival.ctl)
        return;
    u8 st = 0;
    if (ArrivalNow() == g_arrival.ctl && ReadState(g_arrival.ctl, st)) // still the one hidden (it is deleted when done)
        WriteState(g_arrival.ctl, static_cast<u8>(st | g_arrival.saved));
    g_arrival.ctl = 0;
}

static void ApplyArrival(bool hide)
{
    const bool want = g_arrival.want || g_menuOver || hudinfo::DebugProbe().mode == 4; // research: `hudinfo probe 4`
    if (!hide || !want)
    {
        RestoreArrival();
        return;
    }
    const uintptr_t a = ArrivalNow(); // one guarded read a frame; 0 almost always
    if (a != g_arrival.ctl)
        g_arrival.ctl = 0; // the one hidden is gone (deleted with its line)
    u8 st = 0;
    if (!a || !ReadState(a, st) || !(st & 3))
        return;
    g_arrival.saved = st & 3;
    if (WriteState(a, static_cast<u8>(st & ~3)))
        g_arrival.ctl = a, ++g_arrival.hides;
}

void SetHideArrival(bool hide) { g_arrival.want = hide; }

// ---- the game's 3D views without their panels (25 Oct 2026; docs\FREELANCER_RE.md "UI", "HUD 2: the 3D views")
// The target panel's owner (the root control - 0x38) draws its 3D view in its vt[0x38] = exe::TargetViewDraw: the
// object its watch holder (+0x398) refers to, through its view camera (+0x3A0 / +0x3B8), into its rectangle (+0x4B4
// centre x, y, z = -1 in UI camera space, +0x4C0 / +0x4C4 half extents; 0x4E70E0 builds the viewport from them every
// draw) - only while its view mode (+0x648, the game's setter 0x4E5460) is 1. The panel's own draw calls it after its
// background child; called directly on the owner while the panel is hidden, it draws the view alone.
// ONE mechanism for both views on every build (FIX-VIEWS, 25 Oct 2026): the target's view = that draw as the panel has
// it; the player's ship = the same draw on the same owner with the status panel's own watch holder (its +0x398: the
// player's ship, kept by the game on every build - HDE's HudStatus.dll draws its ship view from it; vanilla's status slot
// is the empty stub 0x401E90). For each draw the owner's holder, view mode (1), selected subtarget (+0x534 = the game's
// SUBOBJ_ID_NONE for the ship) and rectangle are written, the draw called, and every field put back before anything else
// runs: the panel keeps its own state. Vanilla's target view stayed empty because its mode was 0 (the game sets 1 only
// when the player picks a target in the contact list, 0x4CFE76) - the draw itself returned at once.
static Root* RootByLabel(const char* label)
{
    for (Root& r : g_roots)
        if (strcmp(r.label, label) == 0)
            return &r;
    return nullptr;
}

constexpr u32 kOwner = 0x38, kViewDraw = 0x38, kViewRect = 0x4B4; // owner = root - 0x38; vt slot; centre + halves
constexpr u32 kHolder = 0x398, kViewMode = 0x648, kSubTarget = 0x534;
struct View
{
    const char* root;      // the root whose watch holder it shows (the draw is always the target panel's owner)
    bool want = false;     // requested this frame (SetViewRect with show)
    float px[4] = {};      // the requested rectangle: centre x, y, half w, half h (pixels)
    u32 draws = 0, refused = 0, faults = 0;
    bool shown = false;    // drawn by the last Apply
    const char* why = "";  // the last refusal
};
static View g_views[2] = {{"status"}, {"target"}};

// The game's "no subobject" id (Common.dll's exported SUBOBJ_ID_NONE), read once; false when not found.
static bool SubobjNone(u16& v)
{
    static const u16* p = nullptr;
    static bool looked = false;
    if (!looked)
    {
        looked = true;
        if (HMODULE c = GetModuleHandleA("common.dll"))
            p = reinterpret_cast<const u16*>(GetProcAddress(c, "?SUBOBJ_ID_NONE@@3GB"));
    }
    return p && debug::SafeRead(reinterpret_cast<uintptr_t>(p), &v, 2);
}

static bool UiCam(float& cx, float& cy, float& kx, float& ky)
{
    if (!exe::UiCamera.ok())
        return false;
    const uintptr_t c = exe::UiCamera.get();
    return debug::SafeRead(c + 0x68, &cx, 4) && debug::SafeRead(c + 0x6C, &cy, 4) && debug::SafeRead(c + 0x78, &kx, 4) &&
           debug::SafeRead(c + 0x7C, &ky, 4) && kx != 0 && ky != 0;
}

static bool WriteFloats(uintptr_t a, const float* v, int n)
{
    __try
    {
        for (int i = 0; i < n; ++i)
            reinterpret_cast<volatile float*>(a)[i] = v[i];
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

// The target panel's owner when its view slot is the game's target view (signed); 0 otherwise: never called.
static uintptr_t TargetViewOwner(uintptr_t& fn)
{
    const Root* t = RootByLabel("target");
    fn = 0;
    if (!t || !t->ctl || !exe::TargetViewDraw.ok())
        return 0;
    const uintptr_t owner = t->ctl - kOwner;
    u32 vt = 0, slot = 0;
    if (!debug::SafeRead(owner, &vt, 4) || !debug::SafeRead(vt + kViewDraw, &slot, 4) || slot != exe::TargetViewDraw.get())
        return 0;
    fn = slot;
    return owner;
}

// The ship view: the status panel's own view (its watch holder +0x398 = the player's ship, its scene +0x3A0 and camera
// +0x3B8, kept by the game on every build) drawn by the panel base's routines in the order HDE's HudStatus.dll calls
// them (its vt[0x38] replacement, read from its code): save the viewport, the viewport from the rectangle, the scene with
// the parts coloured by health, the object, then the UI camera and the viewport back. Vanilla's status slot is the empty
// stub 0x401E90 - the same calls draw the view there. The rectangle is written for the draw and put back after it.
static bool ShipViewSymbols()
{
    return exe::ViewSetViewport.ok() && exe::ViewPrepare.ok() && exe::ViewDrawObject.ok() && exe::RenderGetInt.ok() &&
           exe::RenderSetCamera.ok() && exe::RenderSetViewport.ok() && exe::UiCamera.ok();
}

static void DrawShipView(View& v, uintptr_t owner, float cx, float cy, float kx, float ky)
{
    u32 holder = 0;
    float saveRect[5] = {};
    if (!debug::SafeRead(owner + kHolder, &holder, 4) || !holder)
    {
        ++v.refused, v.why = "no object in the status panel";
        return;
    }
    if (!debug::SafeRead(owner + kViewRect, saveRect, sizeof(saveRect)) || saveRect[2] > -0.01f)
    {
        ++v.refused, v.why = "not the layout measured";
        return;
    }
    const float z = saveRect[2], shift = 0.0125f * 2.0f * cx;
    const float want[5] = {(v.px[0] + shift - cx) / kx * -z, (v.px[1] - cy) / ky * -z, z, fabsf(v.px[2] / kx * -z),
                           fabsf(v.px[3] / ky * -z)};
    int vp[4] = {};
    bool ok = false, vpSaved = false;
    __try
    {
        hk::Call<void(__cdecl*)(int, int*)>(exe::RenderGetInt, 0xBA2, vp);
        vpSaved = vp[2] > 0 && vp[3] > 0;
        if (vpSaved)
        {
            WriteFloats(owner + kViewRect, want, 5);
            reinterpret_cast<void(__thiscall*)(uintptr_t, int, float)>(exe::ViewSetViewport.get())(owner, 1, -0.0125f);
            reinterpret_cast<void(__thiscall*)(uintptr_t)>(exe::ViewPrepare.get())(owner);
            const uintptr_t w = holder - 8;
            const u32 vt = *reinterpret_cast<const u32*>(w);
            void* obj = reinterpret_cast<void*(__thiscall*)(uintptr_t)>(*reinterpret_cast<const u32*>(vt + 0x10))(w);
            if (obj)
                reinterpret_cast<void(__thiscall*)(uintptr_t, uintptr_t, void*)>(exe::ViewDrawObject.get())(
                    owner + 0x3A0, owner + 0x3B8, obj);
            ok = obj != nullptr;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        ++v.faults;
    }
    __try
    {
        if (vpSaved)
        {
            hk::Call<void(__cdecl*)(uintptr_t)>(exe::RenderSetCamera, exe::UiCamera.get());
            hk::Call<void(__cdecl*)(int, int, int, int, const char*, int)>(exe::RenderSetViewport, vp[0], vp[1], vp[2], vp[3],
                                                                                 "hudvis", 0);
            WriteFloats(owner + kViewRect, saveRect, 5);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
    if (ok)
        ++v.draws, v.shown = true, v.why = "";
    else if (!v.faults)
        ++v.refused, v.why = vpSaved ? "no engine object" : "viewport unreadable";
}

// One view: the target owner's draw with `holder` (the object to show) at the requested rectangle; all its fields back
// afterwards (also after a fault).
static void DrawView(View& v, bool hide)
{
    Root* r = RootByLabel(v.root);
    const bool on = hide && v.want && r && r->ctl && r->hidden;
    v.want = false; // a request holds for one frame
    v.shown = false;
    if (!on)
        return;
    auto refuse = [&](const char* why)
    {
        ++v.refused;
        v.why = why;
    };
    float cx, cy, kx, ky;
    if (!UiCam(cx, cy, kx, ky))
        return refuse("no UI camera");
    if (&v == &g_views[0]) // the ship: the status panel's own view
    {
        if (!ShipViewSymbols())
            return refuse("view routines unsigned");
        return DrawShipView(v, r->ctl - kOwner, cx, cy, kx, ky);
    }
    uintptr_t fn = 0;
    const uintptr_t owner = TargetViewOwner(fn);
    if (!owner)
        return refuse("no target view draw");
    const uintptr_t src = r->ctl - kOwner; // the target's own holder
    u32 holder = 0, saveHolder = 0, saveMode = 0;
    u16 saveSub = 0, none = 0;
    float saveRect[5] = {};
    if (!debug::SafeRead(src + kHolder, &holder, 4) || !holder)
        return refuse("no object in the panel's holder");
    if (!debug::SafeRead(owner + kHolder, &saveHolder, 4) || !debug::SafeRead(owner + kViewMode, &saveMode, 4) ||
        !debug::SafeRead(owner + kSubTarget, &saveSub, 2) || !debug::SafeRead(owner + kViewRect, saveRect, sizeof(saveRect)))
        return refuse("owner unreadable");
    if (saveRect[2] > -0.01f || saveMode > 8)
        return refuse("not the layout measured"); // (z = -1, a small mode): leave it alone
    const bool other = src != owner;
    if (other && !SubobjNone(none))
        return refuse("SUBOBJ_ID_NONE not found");
    // pixels -> UI camera space at the view's depth (pixel = centre + k * xy / -z). The viewport the game builds from the
    // rectangle (0x4E70E0) sits 0.0125 x the screen width left of it (its argument -0.0125 x the width, measured 48 px at
    // 3840): the centre is moved right by as much so the view lands where asked.
    const float z = saveRect[2];
    const float shift = 0.0125f * 2.0f * cx;
    const float want[5] = {(v.px[0] + shift - cx) / kx * -z, (v.px[1] - cy) / ky * -z, z, fabsf(v.px[2] / kx * -z),
                           fabsf(v.px[3] / ky * -z)};
    const u32 mode1 = 1;
    bool ok = false;
    __try
    {
        *reinterpret_cast<volatile u32*>(owner + kHolder) = holder;
        *reinterpret_cast<volatile u32*>(owner + kViewMode) = mode1;
        if (other)
            *reinterpret_cast<volatile u16*>(owner + kSubTarget) = none;
        WriteFloats(owner + kViewRect, want, 5);
        reinterpret_cast<void(__thiscall*)(uintptr_t)>(fn)(owner);
        ok = true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        ++v.faults;
    }
    __try
    {
        *reinterpret_cast<volatile u32*>(owner + kHolder) = saveHolder;
        *reinterpret_cast<volatile u32*>(owner + kViewMode) = saveMode;
        *reinterpret_cast<volatile u16*>(owner + kSubTarget) = saveSub;
        WriteFloats(owner + kViewRect, saveRect, 5);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
    if (ok)
        ++v.draws, v.shown = true, v.why = "";
}

void SetViewRect(int which, float cx, float cy, float halfW, float halfH, bool show)
{
    if (which < 0 || which > 1)
        return;
    View& v = g_views[which];
    v.want = show && halfW > 0 && halfH > 0;
    v.px[0] = cx, v.px[1] = cy, v.px[2] = halfW, v.px[3] = halfH;
}

bool ViewShown(int which) { return which >= 0 && which <= 1 && g_views[which].shown; }

// The object the target panel's 3D view shows: the owner's +0x398 holds the panel's watched object (0x4E4007: the
// pointer - 8 + 0xC, its +4 = the CSimple), the CSimple's id at +0xB0 (0x4D4C70). 0 none / unreadable.
u32 TargetViewObject()
{
    const Root* r = RootByLabel("target");
    if (!r || !r->ctl)
        return 0;
    u32 p = 0, obj = 0, id = 0;
    if (!debug::SafeRead(r->ctl - kOwner + 0x398, &p, 4) || !p || !debug::SafeRead(p + 8, &obj, 4) || !obj ||
        !debug::SafeRead(obj + 0xB0, &id, 4))
        return 0;
    return id;
}

// Lets the target panel run its frame with its own logic (its bits as the game wants them) until the next Apply hides
// it again before anything draws: the panel builds its 3D view for a new target only while it is shown.
void PulseTargetPanel()
{
    Root* r = RootByLabel("target");
    u8 st = 0;
    if (r && r->ctl && r->hidden && Valid(*r, false) && ReadState(r->ctl, st))
        WriteState(r->ctl, static_cast<u8>(st | r->saved));
}

static void DrawViews(bool hide)
{
    const hudinfo::Probe& p = hudinfo::DebugProbe(); // research: `hudinfo probe 2|3 cx cy hw hh` (pixels)
    if (p.mode == 2 || p.mode == 3)
        SetViewRect(p.mode - 2, p.v[0], p.v[1], p.v[2], p.v[3], true);
    for (View& v : g_views)
        DrawView(v, hide);
}

static bool HoverHelpRoot(const Root& r) { return r.keep == KeyboardForm; }

// The roots still the controls they were (revalidated, rediscovered at 2 Hz while one is missing); `hoverOnly`: just the
// hover help's two (the classic HUD: nothing else is touched).
static void Revalidate(DWORD now, bool hoverOnly)
{
    const bool nameCheck = now - g_lastNameCheck > 2000;
    if (nameCheck)
        g_lastNameCheck = now;
    bool missing = false;
    for (Root& r : g_roots)
    {
        if (hoverOnly && !HoverHelpRoot(r))
            continue;
        if (r.ctl && !Valid(r, nameCheck))
            Drop(r);
        if (!r.ctl)
            missing = true;
    }
    if (missing && now - g_lastFind > 500) // the HUD is being built: look again twice a second
    {
        g_lastFind = now;
        for (Root& r : g_roots)
            if (!r.ctl && (!hoverOnly || HoverHelpRoot(r)))
                Find(r);
    }
}

// The hover help hidden the game's own way while it is not intentional (SetHoverHelp); never under a modal menu (the
// menus' button help is the ToolTip).
static void ApplyHoverHelp(bool menu)
{
    if (g_hoverAllowed || menu)
        return;
    for (Root& r : g_roots)
    {
        u8 st = 0;
        if (!HoverHelpRoot(r) || !r.ctl || !ReadState(r.ctl, st) || !(st & 2))
            continue;
        if (WriteState(r.ctl, static_cast<u8>(st & ~3)))
            ++g_hoverHides;
        if (strcmp(r.name, "ToolTip") == 0) // its showing flag, as the game's hide (0x4BE390) clears it
        {
            __try
            {
                *reinterpret_cast<volatile u8*>(r.ctl + 0x159) = 0;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
            }
        }
    }
}

void Apply(bool hide)
{
    const DWORD now = GetTickCount();
    const bool menu = g_menuOver;
    hide = hide || menu;
    if (menu)
        ++g_menuFrames;
    ApplyArrival(hide);
    if (!hide)
    {
        DrawViews(false);
        for (Root& r : g_roots)
            Restore(r);
        if (!g_hoverAllowed)
        {
            Revalidate(now, true);
            ApplyHoverHelp(false);
        }
        return;
    }
    Revalidate(now, false);
    ApplyHoverHelp(menu);
    for (Root& r : g_roots)
    {
        if (HoverHelpRoot(r))
            continue; // (ApplyHoverHelp's)
        // under a modal menu only the menus' own label / box stay (the keep rules of the flight - an open NeuroNet
        // window, the disrupted cruise line, the chat input - go under the scrim with the rest)
        if (r.ctl && (menu ? r.keep == KeyboardForm : r.keep && r.keep(r)))
        {
            Restore(r);
            continue;
        }
        u8 st = 0;
        if (!r.ctl || !ReadState(r.ctl, st) || !(st & 3))
            continue;
        if (r.hidden)
            ++r.rewrites;
        r.saved = st & 3; // what the game wants it to be (it showed it)
        if (WriteState(r.ctl, static_cast<u8>(st & ~3)))
            r.hidden = true;
    }
    DrawViews(!menu); // no 3D view under the menu (its requests lapse: the game's rectangles go back)
}

void Invalidate()
{
    for (Root& r : g_roots)
    {
        Restore(r);
        if (r.ctl)
            Drop(r);
    }
    RestoreArrival();
    g_lastFind = 0;
}

int Hidden()
{
    int n = 0;
    for (const Root& r : g_roots)
        n += r.hidden ? 1 : 0;
    return n;
}

std::string Status()
{
    std::ostringstream o;
    o << "hudvis hidden=" << Hidden() << " finds=" << g_finds << " drops=" << g_drops << " hoverHelp=" << (g_hoverAllowed ? "allowed" : "suppressed")
      << " hoverHides=" << g_hoverHides << " menuOver=" << g_menuOver
      << " menuFrames=" << g_menuFrames << "\n";
    for (const Root& r : g_roots)
    {
        u8 st = 0;
        const bool have = r.ctl && ReadState(r.ctl, st);
        char line[160];
        sprintf_s(line, "  %-12s %-18s ctl=%08X state=%s%02X hidden=%d saved=%d rewrites=%u%s\n", r.label,
                  r.ctl ? r.name : "(not found)", static_cast<u32>(r.ctl), have ? "" : "?", st, r.hidden, r.saved,
                  r.rewrites, r.ctl && r.keep && r.keep(r) ? " kept" : "");
        o << line;
    }
    char line[160];
    sprintf_s(line, "  arrival      want=%d now=%08X hidden=%08X hides=%u%s\n", g_arrival.want ? 1 : 0,
              static_cast<u32>(ArrivalNow()), static_cast<u32>(g_arrival.ctl), g_arrival.hides,
              exe::ArrivalText.ok() ? "" : " (symbol unresolved)");
    o << line;
    for (const View& v : g_views)
    {
        sprintf_s(line, "  view %-7s draws=%u refused=%u%s%s faults=%u rect=(%.0f %.0f %.0f %.0f)\n", v.root, v.draws,
                  v.refused, *v.why ? " last: " : "", v.why, v.faults, v.px[0], v.px[1], v.px[2], v.px[3]);
        o << line;
    }
    return o.str();
}
static void Walk(uintptr_t c, int depth, std::ostringstream& o, int& budget)
{
    for (int guard = 0; c && guard < 64 && budget-- > 0; ++guard)
    {
        u32 vt = 0, child = 0, sib = 0;
        u8 st = 0;
        char n[33] = {};
        float pos[3] = {};
        if (!debug::SafeRead(c, &vt, 4) || !debug::SafeRead(c + kChild, &child, 4) || !debug::SafeRead(c + kSibling, &sib, 4))
            return;
        debug::SafeRead(c + kName, n, 32);
        debug::SafeRead(c + kState, &st, 1);
        debug::SafeRead(c + 0xA8, pos, 12);
        char line[200];
        sprintf_s(line, "%*s%-28s ctl=%08X vt=%08X state=%02X pos=(%.3f %.3f %.3f)\n", depth * 2, "", n,
                  static_cast<u32>(c), vt, st, pos[0], pos[1], pos[2]);
        o << line;
        if (child && depth < 6)
            Walk(child, depth + 1, o, budget);
        c = sib;
    }
}

std::string Tree(const std::string& label)
{
    if (label == "roots") // research: every top-level control (parent 0) in the game's control list (exe::UiControls)
    {
        std::ostringstream o;
        if (!exe::UiControls.ok())
            return "UiControls unsigned";
        u32 node = 0;
        debug::SafeRead(exe::UiControls.get(), &node, 4);
        for (int guard = 0; node && guard < 4000; ++guard)
        {
            u32 c = 0, next = 0, parent = 1;
            debug::SafeRead(node + 8, &c, 4), debug::SafeRead(node + 4, &next, 4);
            if (c && debug::SafeRead(c + kParent, &parent, 4) && parent == 0)
            {
                char n[33] = {};
                u8 st = 0;
                u32 vt = 0;
                debug::SafeRead(c + kName, n, 32), debug::SafeRead(c + kState, &st, 1), debug::SafeRead(c, &vt, 4);
                char b[96];
                sprintf_s(b, "%08X vt=%08X state=%02X %s\n", c, vt, st, n);
                o << b;
            }
            node = next;
        }
        return o.str();
    }
    for (const Root& r : g_roots)
        if (label == r.label && r.ctl)
        {
            std::ostringstream o;
            int budget = 300;
            u32 child = 0;
            debug::SafeRead(r.ctl + kChild, &child, 4);
            o << r.name << "\n";
            Walk(child, 1, o, budget);
            return o.str();
        }
    if (const uintptr_t c = ui::FindControl(label.c_str())) // research: any named control's tree
    {
        std::ostringstream o;
        int budget = 300;
        u32 child = 0;
        debug::SafeRead(c + kChild, &child, 4);
        o << label << "\n";
        Walk(child, 1, o, budget);
        return o.str();
    }
    return "no such root (or not found yet): " + label;
}
} // namespace hudvis
