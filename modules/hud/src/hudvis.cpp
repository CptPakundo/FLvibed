// The native HUD switch: hides and restores the game's flight HUD panels by their root controls while HUD 2 shows.
#include "hudvis.h"

#include "game/fl.h"
#include "flexe.h"
#include "game/hostcalls.h"
#include "game/hudinfo.h"
#include "game/indicators.h"
#include "hud2_roots.h"
#include "log.h"
#include "safemem.h"
#include "ui/ui.h"

#include <algorithm>
#include <sstream>
#include <vector>

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
    uintptr_t art[8] = {}; // the art of a root kept for a window under it (the registry's KeepNoArt)
    int nArt = 0;
    bool artHidden = false;
    DWORD seenAt = 0;      // the game last showed it (an alert: AlertSeen)
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
// stays shown - the NeuroNet box with the window - until it closes. Five cached reads a frame. A button no longer under
// the root is a stale pointer: a save load rebuilds the HUD, and with the new root at the old one's address the root
// passes Valid while its buttons' blocks hold anything, a set bit 1 among it - the classic NeuroNet
// box beside HUD 2 after a load (not reproduced in 44 harness loads; the line below names the button if it comes back).
static bool NeuroNetWindowOpen(const Root& r)
{
    static uintptr_t logged = 0;
    for (int i = 0; i < r.nAux; ++i)
    {
        u8 st = 0;
        u32 parent = 0;
        if (safemem::Read(r.aux[i] + kParent, &parent, 4) && parent == r.ctl &&
            safemem::Read(r.aux[i] + kState, &st, 1) && (st & 2))
        {
            if (logged != r.aux[i])
            {
                char n[33] = {};
                safemem::Read(r.aux[i] + kName, n, 32);
                Log("hudvis: the NeuroNet box stays shown: %s is open (state %02X)", n, st);
                logged = r.aux[i];
            }
            return true;
        }
    }
    logged = 0;
    return false;
}

// The switch buttons under the NeuroNet root (direct children named NN...Button), and its art: the box's background and the five
// icon buttons (NN...ButtonRoot). A window hangs under its switch button, so those are never touched.
static void FindNeuroNetButtons(Root& r)
{
    r.nAux = 0;
    r.nArt = 0;
    u32 c = 0;
    safemem::Read(r.ctl + kChild, &c, 4);
    for (int guard = 0; c && guard < 32 && r.nAux < 8; ++guard)
    {
        char n[33] = {};
        if (safemem::Read(c + kName, n, 32))
        {
            const size_t len = strlen(n);
            if (len > 8 && strncmp(n, "NN", 2) == 0 && strcmp(n + len - 6, "Button") == 0)
                r.aux[r.nAux++] = c;
            else if (r.nArt < 8 && (strcmp(n, "NNBackground") == 0 || (len > 10 && strcmp(n + len - 10, "ButtonRoot") == 0)))
                r.art[r.nArt++] = c;
        }
        if (!safemem::Read(c + kSibling, &c, 4))
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
        cache = c && safemem::Read(c + kState, &st, 1) && (st & 2);
    }
    return cache;
}

// The game's hover help - the mouse-over label (Rollover: "Rogues - Ken Norris" beside the cursor) and the mouse-over box
// (ToolTip: "NeuroNet", "Police Base / Fort Bush") - only for an intentional hover (SetHoverHelp,
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
static bool AlwaysKept(const Root&) { return true; }

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
    // the other three alerts of the same parent (UI_HUD2_SKIN.md 6.8), each with its home: the nav's amber line, the ring's tail mark, the
    // nav's small word. While they are hidden the game goes on showing them, and each frame's re-hide reads that: AlertSeen
    {"RadiationAlert", "radiation", nullptr, true},
    {"TrailAlert", "tail", nullptr, true},
    {"LagAlert", "lag", nullptr, true},
    // the game's comm window (HUD_Receiver: the portrait, its outline, the caption): HUD 2 keeps it, only a modal menu over the
    // flight hides it - its pieces stood bare around the pause menu with no plate (UI_HUD2.md 5.34)
    {"CommText", "comm", AlwaysKept},
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

static bool ReadState(uintptr_t ctl, u8& state) { return safemem::Read(ctl + kState, &state, 1); }

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
    if (!safemem::Read(r.ctl, &vt, 4) || vt != r.vtable)
        return false;
    if (name)
    {
        char n[33] = {};
        if (!safemem::Read(r.ctl + kName, n, 32) || strcmp(n, r.name) != 0)
            return false;
    }
    return true;
}

static void Drop(Root& r)
{
    r.ctl = 0, r.vtable = 0, r.hidden = false, r.name[0] = 0, r.nAux = 0, r.nArt = 0, r.artHidden = false;
    ++g_drops;
}

static void Find(Root& r)
{
    uintptr_t c = ui::FindControl(r.child);
    for (int guard = 0; c && guard < 16 && !r.self; ++guard)
    {
        u32 parent = 0;
        if (!safemem::Read(c + kParent, &parent, 4))
            return;
        if (!parent)
            break;
        c = parent;
    }
    if (!c || !safemem::Read(c, &r.vtable, 4) || !safemem::Read(c + kName, r.name, 32))
        return;
    r.name[32] = 0;
    r.ctl = c;
    r.hidden = false;
    r.nAux = 0, r.nArt = 0, r.artHidden = false;
    if (r.keep == NeuroNetWindowOpen)
        FindNeuroNetButtons(r);
    ++g_finds;
}

// A kept root's art, hidden by bit 1 alone, every frame: the game shows it again by itself, and bit 0 carries the box's update.
static void HideKeptArt(Root& r)
{
    for (int i = 0; i < r.nArt; ++i)
    {
        u8 st = 0;
        u32 parent = 0;
        if (safemem::Read(r.art[i] + kParent, &parent, 4) && parent == r.ctl && ReadState(r.art[i], st) && (st & 2))
            WriteState(r.art[i], static_cast<u8>(st & ~2));
    }
    r.artHidden = r.nArt > 0;
}

static void RestoreKeptArt(Root& r)
{
    if (!r.artHidden)
        return;
    r.artHidden = false;
    for (int i = 0; i < r.nArt; ++i)
    {
        u8 st = 0;
        u32 parent = 0;
        if (safemem::Read(r.art[i] + kParent, &parent, 4) && parent == r.ctl && ReadState(r.art[i], st) && (st & 1) && !(st & 2))
            WriteState(r.art[i], static_cast<u8>(st | 2));
    }
}

static void Restore(Root& r)
{
    RestoreKeptArt(r);
    if (!r.hidden)
        return;
    u8 st = 0;
    // (not an alert: it blinks, so the game's next tick shows it anyway, and a scene that stops the tick - the death -
    // kept the restored icons standing over it)
    if (!r.self && Valid(r, true) && ReadState(r.ctl, st))
        WriteState(r.ctl, static_cast<u8>(st | r.saved));
    r.hidden = false;
}

// ---- the arrival line
static uintptr_t ArrivalNow()
{
    if (!exe::ArrivalText.ok())
        return 0;
    u32 p = 0;
    safemem::Read(exe::ArrivalText.get(), &p, 4);
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

// ---- the game's 3D views without their panels (docs\FREELANCER_RE.md "UI", "HUD 2: the 3D views")
// The target panel's owner (the root control - 0x38) draws its 3D view in its vt[0x38] = exe::TargetViewDraw: the
// object its watch holder (+0x398) refers to, through its view camera (+0x3A0 / +0x3B8), into its rectangle (+0x4B4
// centre x, y, z = -1 in UI camera space, +0x4C0 / +0x4C4 half extents; 0x4E70E0 builds the viewport from them every
// draw) - only while its view mode (+0x648, the game's setter 0x4E5460) is 1. The panel's own draw calls it after its
// background child; called directly on the owner while the panel is hidden, it draws the view alone.
// ONE mechanism for both views on every build (FIX-VIEWS): the target's view = that draw as the panel has
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
static u16 g_shipPart = 0;
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
    return p && safemem::Read(reinterpret_cast<uintptr_t>(p), &v, 2);
}

static bool UiCam(float& cx, float& cy, float& kx, float& ky)
{
    if (!exe::UiCamera.ok())
        return false;
    const uintptr_t c = exe::UiCamera.get();
    return safemem::Read(c + 0x68, &cx, 4) && safemem::Read(c + 0x6C, &cy, 4) && safemem::Read(c + 0x78, &kx, 4) &&
           safemem::Read(c + 0x7C, &ky, 4) && kx != 0 && ky != 0;
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
    if (!safemem::Read(owner, &vt, 4) || !safemem::Read(vt + kViewDraw, &slot, 4) || slot != exe::TargetViewDraw.get())
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
    if (!safemem::Read(owner + kHolder, &holder, 4) || !holder)
    {
        ++v.refused, v.why = "no object in the status panel";
        return;
    }
    if (!safemem::Read(owner + kViewRect, saveRect, sizeof(saveRect)) || saveRect[2] > -0.01f)
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
            // the part the game pulses in this view (0x4E6EC0: the equipment whose id is the short at +0x3B4, its own subsystem highlight)
            *reinterpret_cast<u16*>(owner + 0x3B4) = g_shipPart;
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
    if (!safemem::Read(src + kHolder, &holder, 4) || !holder)
        return refuse("no object in the panel's holder");
    if (!safemem::Read(owner + kHolder, &saveHolder, 4) || !safemem::Read(owner + kViewMode, &saveMode, 4) ||
        !safemem::Read(owner + kSubTarget, &saveSub, 2) || !safemem::Read(owner + kViewRect, saveRect, sizeof(saveRect)))
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

// The ship's own view at any size, right now, over what is drawn (a NeuroNet page: the pass under the UI lies beneath its ground). False: no panel to draw from.
bool DrawShipAt(float cx, float cy, float halfW, float halfH, u16 part)
{
    g_shipPart = part;
    View& v = g_views[0];
    Root* r = RootByLabel(v.root);
    float ucx, ucy, kx, ky;
    if (!r || !r->ctl || !ShipViewSymbols() || !UiCam(ucx, ucy, kx, ky))
        return false;
    v.px[0] = cx, v.px[1] = cy, v.px[2] = halfW, v.px[3] = halfH;
    DrawShipView(v, r->ctl - kOwner, ucx, ucy, kx, ky);
    return true;
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
    if (!safemem::Read(r->ctl - kOwner + 0x398, &p, 4) || !p || !safemem::Read(p + 8, &obj, 4) || !obj ||
        !safemem::Read(obj + 0xB0, &id, 4))
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
        else if (nameCheck && r.keep == NeuroNetWindowOpen) // (a root that outlived a rebuild: its buttons found again)
            FindNeuroNetButtons(r);
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

// The game steps its Neural Net queue from an update that stops with the NeuroNet box (exe::NNPump, its one caller): with
// the box hidden the first line played and stayed at the head, so every later one waited, voice and subtitle, until the
// box opened (GitHub issue 8). Stepped here instead while the box is hidden. Not in a cutscene, under a modal menu or in a
// pause: a line asked to play in a cutscene plays nothing and would be dropped; the queue waits for them.
static void StepNeuralNet()
{
    __try
    {
        if (exe::NNPump.ok() && exe::IsPlayerInCutscene.ok() && !hk::Call<int(__cdecl*)()>(exe::IsPlayerInCutscene))
            hk::Call<void(__cdecl*)(float)>(exe::NNPump, 0.0f);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

// The throttle keys only set a rate (HUD_Maneuvers part + 0x3E8, per second); the game adds it to the throttle in the maneuver bar's own
// update (0x4D85C0), which stops with the bar: with the bar hidden the keys did nothing, and the ship kept the throttle it had. Applied
// here instead while the bar is hidden, under the same rules as the Neural Net above. docs\FREELANCER_RE.md "Flight".
static void StepThrottleKeys()
{
    __try
    {
        const uintptr_t part = exe::PlayerManeuvers.ok() ? Mem<uintptr_t>(exe::PlayerManeuvers) : 0;
        if (!part || !exe::FrameDt.ok() || !exe::IsPlayerInCutscene.ok() || hk::Call<int(__cdecl*)()>(exe::IsPlayerInCutscene))
            return;
        const float rate = *reinterpret_cast<const float*>(part + 0x3E8);
        if (rate != 0)
            fl::SetThrottle(Clampf(fl::GetThrottle() + rate * static_cast<float>(Mem<double>(exe::FrameDt)), 0.0f, 1.0f));
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

// The contact list's rows are built in its draw, and a hidden root does not draw: under HUD 2 the list stayed empty, so the native target
// keys found nothing. Built here instead while the list is hidden, as often as its draw does. Refused unless the root is that window.
static void StepContactList(uintptr_t window)
{
    static float due = 0;
    __try
    {
        due -= static_cast<float>(Mem<double>(exe::FrameDt));
        u8* dirty = reinterpret_cast<u8*>(window + 0x404);
        if ((due > 0 && !*dirty) || !exe::ContactListBuild.ok() || !exe::ContactListDraw.ok() ||
            !exe::ContactListDraw.is(*reinterpret_cast<const u32*>(*reinterpret_cast<const uintptr_t*>(window) + 0x34)))
            return;
        due = 1.5f;
        *dirty = 0;
        hk::Call<void(__thiscall*)(uintptr_t)>(exe::ContactListBuild, window - 0x38);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

// ---- the radiation art beside the nav's line (UI_HUD2_SKIN.md 6.8): the game's own control, moved there and held steady, so nothing of the game
// ships. Its place goes back when the line goes. Fails closed: a slot that is not the signed setter leaves the line without its art.
struct RadiationArt
{
    bool want = false;
    float px = 0, py = 0;
    uintptr_t ctl = 0; // the control moved
    u32 setter = 0;     // its signed vt[0x14]
    float home[3] = {}; // its own place, as that setter takes it
    bool shown = false;
    const char* why = "not asked"; // `hud2 state`: what the last frame decided
} g_radArt;

const char* RadiationArtWhy() { return g_radArt.why; }

void SetRadiationArt(float px, float py, bool show)
{
    g_radArt.want = show, g_radArt.px = px, g_radArt.py = py;
}

static void CallSetPosition(u32 setter, uintptr_t ctl, const float* pos)
{
    __try
    {
        reinterpret_cast<void(__thiscall*)(u32, const float*)>(setter)(static_cast<u32>(ctl), pos);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

static void RadiationArtBack()
{
    RadiationArt& a = g_radArt;
    if (a.ctl)
    {
        const Root* r = RootByLabel("radiation");
        if (r && r->ctl == a.ctl)
        {
            CallSetPosition(a.setter, a.ctl, a.home);
            u8 st = 0;
            if (a.shown && ReadState(a.ctl, st)) // (dark: the game's next blink shows it if the zone still bites)
                WriteState(a.ctl, static_cast<u8>(st & ~3));
        }
    }
    a.ctl = 0, a.shown = false;
}

// True: the control stands at the line this frame, shown.
static bool RadiationArtStep(Root& r)
{
    RadiationArt& a = g_radArt;
    const auto no = [&a](const char* why) { a.why = why; return false; };
    // (the flag first: this step forces the control shown, so the control itself can no longer say the zone ended)
    if (!a.want)
        return no("not asked");
    if (!r.ctl || !exe::RadiationZone.ok())
        return no("unsigned");
    u32 vt = 0, slot = 0;
    float cx, cy, kx, ky, pos[3] = {};
    if (!safemem::Read(r.ctl, &vt, 4) || !safemem::Read(vt + 0x14, &slot, 4))
        return no("unreadable");
    if (!exe::ImageSetPosition.is(slot))
        return no("another setter");
    // an image control's art stands at +0x60 + the offset the constructor kept at +0xA8 (0x588540): for the alerts +0x60 is
    // zero and the whole place is the offset, the depth in it
    float off[3] = {};
    if (!safemem::Read(r.ctl + 0x60, pos, sizeof(pos)) || !safemem::Read(r.ctl + 0xA8, off, sizeof(off)))
        return no("unreadable");
    const float depth = pos[2] + off[2];
    if (depth > -0.01f)
        return no("no depth");
    if (!UiCam(cx, cy, kx, ky))
        return no("no camera");
    if (a.ctl != r.ctl)
        a.ctl = r.ctl, a.setter = slot, a.home[0] = pos[0], a.home[1] = pos[1], a.home[2] = pos[2];
    const float want[3] = {(a.px - cx) / kx * -depth - off[0], (a.py - cy) / ky * -depth - off[1], pos[2]};
    if (fabsf(want[0] - pos[0]) > 1e-5f || fabsf(want[1] - pos[1]) > 1e-5f) // (a move marks the art for rebuilding)
        CallSetPosition(slot, r.ctl, want);
    u8 st = 0;
    if (ReadState(r.ctl, st) && (st & 3) != 3)
        WriteState(r.ctl, static_cast<u8>(st | 3));
    r.hidden = false;
    a.shown = true;
    a.why = "shown";
    return true;
}

bool AlertSeen(int which)
{
    static const char* const kLabels[] = {"radiation", "tail", "lag"};
    if (which < 0 || which >= 3)
        return false;
    if (which == kRadiation && exe::RadiationZone.ok()) // the game's own flag: the art's forced show would latch the control's word
        return Mem<u8>(exe::RadiationZone) != 0;
    const Root* r = RootByLabel(kLabels[which]);
    return r && r->ctl && r->seenAt && GetTickCount() - r->seenAt < kAlertHoldMs;
}

// The game's top-level controls against the registry: a shown root with no row stays shown and gets one line, once per name. On the hide's start and
// on a new UI generation only: never a walk per frame.
static void AuditRoots()
{
    static std::vector<std::string> logged;
    if (!exe::UiControls.ok())
        return;
    u32 node = 0;
    safemem::Read(exe::UiControls.get(), &node, 4);
    for (int guard = 0; node && guard < 4000; ++guard)
    {
        u32 c = 0, next = 0, parent = 1, vt = 0;
        u8 st = 0;
        char n[33] = {};
        safemem::Read(node + 8, &c, 4), safemem::Read(node + 4, &next, 4);
        if (c && safemem::Read(c + kParent, &parent, 4) && parent == 0 && ReadState(c, st) && (st & 2) && safemem::Read(c + kName, n, 32) && n[0] &&
            !hudroots::Find(n) && std::find(logged.begin(), logged.end(), n) == logged.end() && logged.size() < 64)
        {
            safemem::Read(c, &vt, 4);
            logged.push_back(n);
            Log("hudvis: unknown root %s vt 0x%08X stays shown", n, vt);
        }
        node = next;
    }
}

// ---- the game's bracket under HUD 2's targeting display (UI_HUD2_SKIN.md 6.10): the corners and the waypoint diamond by nulling the indicators' art, which the draw
// tests before use; label and bars by the host's mute. All or nothing: the form draws only while this stands.
struct BracketOff
{
    uintptr_t obj = 0;
    u32 corners = 0, diamond = 0;
    bool on = false;
} g_bracket;
bool g_formWanted = false;

void SetTargetForm(bool want) { g_formWanted = want; }
bool TargetFormOn() { return g_bracket.on; }

static void WriteField(uintptr_t a, u32 v)
{
    __try
    {
        *reinterpret_cast<volatile u32*>(a) = v;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

static void BracketBack()
{
    if (hostcalls::g.SetTargetMute)
        hostcalls::g.SetTargetMute(false);
    if (g_bracket.on && indicators::Object() == g_bracket.obj)
    {
        u32 v = 1;
        if (safemem::Read(g_bracket.obj + indicators::kCorners, &v, 4) && !v)
            WriteField(g_bracket.obj + indicators::kCorners, g_bracket.corners);
        v = 1;
        if (safemem::Read(g_bracket.obj + indicators::kDiamond, &v, 4) && !v)
            WriteField(g_bracket.obj + indicators::kDiamond, g_bracket.diamond);
    }
    g_bracket = BracketOff();
}

static void BracketStep(bool hide)
{
    const bool can = hide && g_formWanted && hostcalls::g.TargetMuteReady && hostcalls::g.TargetMuteReady() && hostcalls::g.SetTargetMute;
    if (!can)
    {
        if (g_bracket.on)
            BracketBack();
        return;
    }
    const uintptr_t obj = indicators::Object();
    if (g_bracket.on && obj != g_bracket.obj) // a rebuilt HUD: the old object is gone, the new one starts whole
        g_bracket = BracketOff();
    if (!obj)
        return;
    if (!g_bracket.on)
    {
        u32 corners = 0, diamond = 0;
        if (!safemem::Read(obj + indicators::kCorners, &corners, 4) || !safemem::Read(obj + indicators::kDiamond, &diamond, 4) || !corners)
            return;
        WriteField(obj + indicators::kCorners, 0);
        WriteField(obj + indicators::kDiamond, 0);
        g_bracket.obj = obj, g_bracket.corners = corners, g_bracket.diamond = diamond, g_bracket.on = true;
        hostcalls::g.SetTargetMute(true);
    }
}

static bool g_holdChat = false; // HoldChat
static void HideChat();

void GiveBack()
{
    BracketBack();
    RadiationArtBack();
    for (Root& r : g_roots)
        Restore(r);
    RestoreArrival();
}

void Apply(bool hide)
{
    const DWORD now = GetTickCount();
    const bool menu = g_menuOver;
    hide = hide || menu;
    if (menu)
        ++g_menuFrames;
    // the first hide of a flight: the roots are looked up on this very pass, not at the next 2 Hz try, or the native HUD draws once
    static bool wasHide = false;
    static u32 auditGen = 0;
    const bool started = hide && !wasHide;
    if (started)
        g_lastFind = 0;
    wasHide = hide;
    ApplyArrival(hide);
    BracketStep(hide);
    if (!hide)
    {
        RadiationArtBack();
        DrawViews(false);
        for (Root& r : g_roots)
            if (!g_holdChat || strcmp(r.label, "chat") != 0)
                Restore(r);
        if (g_holdChat)
            HideChat();
        if (!g_hoverAllowed)
        {
            Revalidate(now, true);
            ApplyHoverHelp(false);
        }
        return;
    }
    Revalidate(now, false);
    {
        const u32 gen = hostcalls::g.UiGeneration ? hostcalls::g.UiGeneration() : 0;
        if (started || gen != auditGen)
            auditGen = gen, AuditRoots();
    }
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
            if (r.nArt && !menu) // (a kept root loses its art all the same: 3.1's rule)
                HideKeptArt(r);
            continue;
        }
        RestoreKeptArt(r); // (only a kept root hides art: a hidden root takes it with it)
        u8 st = 0;
        if (!r.ctl || !ReadState(r.ctl, st))
            continue;
        const bool radiation = strcmp(r.label, "radiation") == 0;
        if (st & 3)
            r.seenAt = now;
        if (radiation)
        {
            if (RadiationArtStep(r))
                continue;
            if (g_radArt.ctl)
                RadiationArtBack();
        }
        if (!(st & 3))
            continue;
        if (r.hidden)
            ++r.rewrites;
        r.saved = st & 3; // what the game wants it to be (it showed it)
        if (WriteState(r.ctl, static_cast<u8>(st & ~3)))
            r.hidden = true;
    }
    DrawViews(!menu); // no 3D view under the menu (its requests lapse: the game's rectangles go back)
    const Root* nn = RootByLabel("neuronet");
    if (!menu && nn && nn->hidden && !fl::GamePaused())
        StepNeuralNet();
    const Root* bar = RootByLabel("maneuvers");
    if (!menu && bar && bar->hidden && !fl::GamePaused())
        StepThrottleKeys();
    const Root* list = RootByLabel("contactlist");
    if (!menu && list && list->hidden && list->ctl && !fl::GamePaused())
        StepContactList(list->ctl);
}

void HoldChat(bool on)
{
    if (g_holdChat && !on) // (HideChat re-found the roots in the cutscene: they go with it, as at the flight's end)
        Invalidate();
    g_holdChat = on;
}

static void HideChat()
{
    Revalidate(GetTickCount(), false);
    Root* r = RootByLabel("chat");
    if (!r || !r->ctl)
        return;
    if (r->keep && r->keep(*r)) // (the player typing: the window stays theirs)
    {
        Restore(*r);
        return;
    }
    u8 st = 0;
    if (!ReadState(r->ctl, st) || !(st & 3))
        return;
    if (r->hidden)
        ++r->rewrites;
    r->saved = st & 3;
    if (WriteState(r->ctl, static_cast<u8>(st & ~3)))
        r->hidden = true;
}

void Invalidate()
{
    BracketBack();
    RadiationArtBack();
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
        if (!safemem::Read(c, &vt, 4) || !safemem::Read(c + kChild, &child, 4) || !safemem::Read(c + kSibling, &sib, 4))
            return;
        safemem::Read(c + kName, n, 32);
        safemem::Read(c + kState, &st, 1);
        safemem::Read(c + 0xA8, pos, 12);
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
        safemem::Read(exe::UiControls.get(), &node, 4);
        for (int guard = 0; node && guard < 4000; ++guard)
        {
            u32 c = 0, next = 0, parent = 1;
            safemem::Read(node + 8, &c, 4), safemem::Read(node + 4, &next, 4);
            if (c && safemem::Read(c + kParent, &parent, 4) && parent == 0)
            {
                char n[33] = {};
                u8 st = 0;
                u32 vt = 0;
                safemem::Read(c + kName, n, 32), safemem::Read(c + kState, &st, 1), safemem::Read(c, &vt, 4);
                char b[128];
                sprintf_s(b, "%08X vt=%08X state=%02X %-28s %s\n", c, vt, st, n, hudroots::ActionName(hudroots::Find(n)));
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
            u32 child = 0, vt = 0, slot = 0, parent = 0;
            safemem::Read(r.ctl + kChild, &child, 4);
            safemem::Read(r.ctl, &vt, 4), safemem::Read(vt + 0x14, &slot, 4), safemem::Read(r.ctl + kParent, &parent, 4);
            float a[3] = {}, b[3] = {};
            safemem::Read(r.ctl + 0x60, a, sizeof(a)), safemem::Read(r.ctl + 0xA8, b, sizeof(b));
            char line[160];
            sprintf_s(line, " vt=%08X set=%08X parent=%08X +60=(%g %g %g) +A8=(%g %g %g)", vt, slot, parent, a[0], a[1], a[2], b[0], b[1], b[2]);
            o << r.name << line << "\n";
            Walk(child, 1, o, budget);
            return o.str();
        }
    if (const uintptr_t c = ui::FindControl(label.c_str())) // research: any named control's tree
    {
        std::ostringstream o;
        int budget = 300;
        u32 child = 0;
        safemem::Read(c + kChild, &child, 4);
        o << label << "\n";
        Walk(child, 1, o, budget);
        return o.str();
    }
    return "no such root (or not found yet): " + label;
}
} // namespace hudvis
