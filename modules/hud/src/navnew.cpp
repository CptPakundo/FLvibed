// The new map behind its seam: navnew.h. The fields: 07_navmap_facts.md and docs\FREELANCER_RE.md "The nav map's labels and records"; the numbers: navmap_layout.h.
//
// Each write is remembered and given back while it still holds FLvibed's value; the window is a new object at every open, so a write's life is one opening. Every part
// falls back to the game's own: a transform that does not read gives the game's labels back, a control that will not move stays where it is.
#include "navnew.h"

#include "flexe.h"
#include "game/deckmode.h"
#include "game/fl.h"
#include "game/hostcalls.h"
#include "game/hudinfo.h"
#include "game/navview.h"
#include "game/room.h"
#include "game/space.h"
#include "hudhost.h"
#include "log.h"
#include "navmap_labels.h"
#include "safemem.h"
#include "text.h"
#include "ui/hud2_kit.h"
#include "ui/winplates.h"

#include <sstream>
#include <unordered_map>

namespace navnew
{
using namespace hud2kit;
using namespace winplates;
using Rc = navlayout::Rect;

constexpr u32 kFrameOrigin = 0x40C, kBoxW = 0x418, kBoxH = 0x41C, kScaleX = 0x4A8, kScaleZ = 0x4AC, kObjects = 0x400, kSelected = 0x90C, kZoom = 0x3F8, kCentre = 0x46C, kView = 0x7D0,
              kMapScale = 0xF48, kAnimating = 0x478, kPopupWatch = 0x92C; // (the last: the frame's 0x494920 closes the popup when the cursor leaves it)
// The record's fields.
constexpr u32 kRecFlags = 0x04, kRecPos = 0x30, kRecRadius = 0x3C, kRecVisible = 0x48, kRecName = 0x4C, kRecId = 0x70;
constexpr int kMaxRecords = 400, kMaxLabels = 128;
// The map's objects stand this far in from the box's edge at +0x4A8: the game's own labels say so (0x499C20: 0.2 and 0.18 over half its box, 0.2342), and frames agree (planet and ship, 7 Nov 2026).
constexpr float kFit = 0.2f / 0.2342f; // (= 112000 / 131072: the sectors' 8 x 8 run past +0x4A8 to 131072)
// The backgrounds' alpha fields: the system's and the sector's mesh, 0 under HUD 2's ground (the universe's keeps its art).
constexpr u32 kSystemAlpha = 0x374, kSectorAlpha = 0x39C;

// The full-screen map is the skin's map (the owner, 7 Nov 2026: phase 1, docs\MAP_PLAN.md); `navnew off` gives the game's size back for a comparison.
static bool g_on = true;
static bool g_selected = false;

struct Written
{
    u32 root = 0;
    bool grown = false;
    float origin[3] = {}, box[2] = {};     // as the game built them
    float mineOrigin[3] = {}, mineBox[2] = {};
    float baseScale[2] = {}, mineScale[2] = {}; // the game's +0x4A8 / +0x4AC, and what stands there now
    bool labelsOff = false;
    u8 labelsBefore = 0;
    bool showLabels = true;
};
static Written g_w;
static navlayout::MapOut g_lay;
static navlabels::View g_view;
static Rc g_port;
static float g_frameW = 0, g_frameH = 0;
static bool g_transform = false;
static u32 g_records = 0, g_drawn = 0, g_dropped = 0;

struct Named
{
    std::wstring text;
    u32 colour = kChrome;
    float nextAsk = 0;
};
static std::unordered_map<u32, Named> g_names; // by name ids and object: one opening's

static bool RF(u32 a, float& v) { return safemem::Read(a, &v, 4); }

static void WF(u32 a, float v)
{
    __try
    {
        *reinterpret_cast<volatile float*>(a) = v;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

// The game's label byte, off while HUD 2's stand; a press on the native LABELS turns it on, and that press is the player's.
static void LabelsOff(bool off)
{
    if (!exe::NavMapLabelsOn.ok())
        return;
    const u32 at = static_cast<u32>(exe::NavMapLabelsOn.get());
    u8 now = 0;
    if (!R8(at, now))
        return;
    if (off)
    {
        if (!g_w.labelsOff)
            g_w.labelsBefore = now, g_w.labelsOff = true, g_w.showLabels = now != 0;
        else if (now)
            g_w.showLabels = !g_w.showLabels;
        if (now)
            W8(at, 0);
    }
    else if (g_w.labelsOff)
    {
        if (!now)
            W8(at, g_w.labelsBefore);
        g_w.labelsOff = false;
    }
}

static void GiveBack()
{
    LabelsOff(false);
    if (g_w.root && Valid(g_w.root, exe::VtNavMap.get()))
    {
        float v = 0;
        if (g_w.grown)
        {
            for (int i = 0; i < 3; ++i)
                if (RF(g_w.root + kFrameOrigin + 4 * i, v) && v == g_w.mineOrigin[i])
                    WF(g_w.root + kFrameOrigin + 4 * i, g_w.origin[i]);
            if (RF(g_w.root + kBoxW, v) && v == g_w.mineBox[0])
                WF(g_w.root + kBoxW, g_w.box[0]);
            if (RF(g_w.root + kBoxH, v) && v == g_w.mineBox[1])
                WF(g_w.root + kBoxH, g_w.box[1]);
            if (RF(g_w.root + kScaleX, v) && v == g_w.mineScale[0])
                WF(g_w.root + kScaleX, g_w.baseScale[0]);
            if (RF(g_w.root + kScaleZ, v) && v == g_w.mineScale[1])
                WF(g_w.root + kScaleZ, g_w.baseScale[1]);
        }
    }
    g_w = Written();
    g_frameW = g_frameH = 0;
    navview::SetFrame(0, 0);
    g_names.clear();
}

// +0x4A8 / +0x4AC are the world's half size inside the box. The map's camera keeps its field of view, so a box unit is port W / the game's own box W pixels on BOTH axes, whatever the box
// (read from frames, 7 Nov 2026: planets 5.9 x from the centre, the same on both axes, at the first try). So the world has to fill more of the box: one scale on both axes, the system in the
// square. Written every frame, because the game rewrites +0x4A8 on a return from the universe. (Every part of the map reads them: the records, the lanes, the hit test.) True when they changed.
static bool HoldScale(u32 root, float ratioX, float ratioZ)
{
    float v = 0;
    bool changed = false;
    const u32 at[2] = {root + kScaleX, root + kScaleZ};
    const float ratio[2] = {ratioX, ratioZ};
    for (int i = 0; i < 2; ++i)
        if (RF(at[i], v) && v > 0)
        {
            if (v != g_w.mineScale[i])
                g_w.baseScale[i] = v;
            const float mine = g_w.baseScale[i] * ratio[i];
            changed |= std::fabs(mine - v) > 1;
            g_w.mineScale[i] = mine;
            WF(at[i], mine);
        }
    return changed;
}

// The growth: once the window has stopped sliding, the box at `want`. The viewport's pixels per box unit are read, not assumed.
static void Grow(u32 root, const Rc& port, const Rc& want, const Rc& square)
{
    float boxW = 0, boxH = 0, origin[3] = {};
    if (!RF(root + kBoxW, boxW) || !RF(root + kBoxH, boxH) || !safemem::Read(root + kFrameOrigin, origin, sizeof(origin)) || !(boxW > 0.05f) || !(boxH > 0.05f))
        return;
    if (room::NNWindowBusy())
        return;
    const float kx = port.W() / boxW, ky = port.H() / boxH;
    if (!g_w.grown)
    {
        g_w.box[0] = boxW, g_w.box[1] = boxH;
        memcpy(g_w.origin, origin, sizeof(origin));
        g_w.grown = true;
        Log("navnew: the map grown to %.0f x %.0f px at %.0f, %.0f", want.W(), want.H(), want.x0, want.y0);
    }
    // to `want` from where the viewport stands, whatever moved it (a change of view, windowtext's drop, a mod); the game's rectangle follows a frame later, so a move waits for it
    const float ex = want.x0 - port.x0, ey = want.y1 - port.y1, ew = want.W() - port.W(), eh = want.H() - port.H();
    if (std::fabs(ex) > 1 || std::fabs(ey) > 1 || std::fabs(ew) > 1 || std::fabs(eh) > 1)
    {
        origin[0] += ex / kx, origin[1] -= ey / ky; // (the box's y runs up)
        g_w.mineOrigin[0] = origin[0], g_w.mineOrigin[1] = origin[1], g_w.mineOrigin[2] = origin[2];
        g_w.mineBox[0] = want.W() / kx, g_w.mineBox[1] = want.H() / ky;
        WF(root + kFrameOrigin, origin[0]);
        WF(root + kFrameOrigin + 4, origin[1]);
        WF(root + kBoxW, g_w.mineBox[0]);
        WF(root + kBoxH, g_w.mineBox[1]);
    }
    // the camera's own pixels for a box unit (read, not assumed): the sectors' world across the solver's square, at one scale on both axes; the rest of the map works in what the viewport shows
    const float f = navview::PixelsPerBox(root);
    if (f > 0)
    {
        const float over = square.W() * kFit;
        if (HoldScale(root, boxW * f / over, boxH * f / over))
            navview::Replace(root);
        g_frameW = want.W() / f, g_frameH = want.H() / f;
        navview::SetFrame(g_frameW, g_frameH);
    }
    navview::Settle(root);
}

// HUD 2's ground stands where the game's backgrounds did: their alpha, every frame (the game writes it at each zoom). The universe's is the game's.
static void ArtOff(u32 root)
{
    WF(root + kSystemAlpha, 0);
    WF(root + kSectorAlpha, 0);
}

static void ReadView(u32 root)
{
    navlabels::View v;
    u32 view = 0;
    R32(root + kView, view);
    v.view = static_cast<int>(view);
    float boxW = 0, boxH = 0;
    RF(root + kBoxW, boxW), RF(root + kBoxH, boxH);
    v.halfW = boxW / 2, v.halfH = boxH / 2;
    RF(root + kZoom, v.zoom), RF(root + kCentre, v.centreX), RF(root + kCentre + 4, v.centreY), RF(root + kMapScale, v.mapScale);
    RF(root + kScaleX, v.scaleX), RF(root + kScaleZ, v.scaleZ);
    g_view = v;
    g_transform = (v.view == 1 || v.view == 2) && v.scaleX > 0 && v.scaleZ > 0;
}

static u32 SelectedRecord(u32 root);

// The mouse on the map: the wheel zooms toward the cursor, the game's own drag moves it.
// The game's marks that have no size of their own (the ship, waypoints, the mission's: record types 2, 3, 4; +0x3C the size, read by 0x496600) grow with the map's scale and with Z up
// to 2 (Shockwave via the owner, 7 Nov 2026: "big McLargeHuge"). Held at about the game's own size on the screen, whatever the zoom: the square stands 1.6 x the game's map, Z / max(1, Z / 2) is the rest.
struct Mark
{
    float base, mine;
};
static std::unordered_map<u32, Mark> g_marks;
static void HoldMarks(u32 root)
{
    float z = 1;
    RF(root + kZoom, z);
    const float k = 0.7f * std::max(1.0f, z * 0.5f) / std::max(1.0f, z);
    u32 head = 0;
    if (!safemem::Read(root + kObjects, &head, 4) || !head)
        return;
    u32 node = 0;
    safemem::Read(head, &node, 4);
    for (int guard = 0; node && node != head && guard < 4096; ++guard)
    {
        u32 rec = 0, type = 0, next = 0;
        safemem::Read(node + 8, &rec, 4);
        safemem::Read(node, &next, 4);
        if (rec && safemem::Read(rec, &type, 4) && type >= 2 && type <= 4)
        {
            float r = 0;
            if (RF(rec + 0x3C, r) && r > 0)
            {
                Mark& m = g_marks[rec];
                if (r != m.mine)
                    m.base = r;
                m.mine = m.base * k;
                WF(rec + 0x3C, m.mine);
            }
        }
        node = next;
    }
}

// The right button: a waypoint at the cursor in one click (the game's took a click and its popup's button). On an object the list's own call; on open space the game's click, then its Set Waypoint.
static float g_setWaypoint = 0;
static void RightClick(u32 root, float dt)
{
    static bool was = false;
    const bool down = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0 && GetForegroundWindow() == fl::MainWindow();
    const bool pressed = down && !was;
    was = down;
    if (g_setWaypoint > 0)
    {
        g_setWaypoint -= dt;
        if (const u32 set = uiclick::FindVisible("NavMapSetWaypointButton"))
            uiclick::Click(set), g_setWaypoint = 0;
    }
    const float mx = static_cast<float>(fl::MouseX()), my = static_cast<float>(fl::MouseY());
    if (!pressed || !navview::OnMap(root, mx, my))
        return;
    float at[3] = {};
    u32 object = 0;
    if (!uiclick::FindVisible("NavMapShipButton") && navview::ObjectAt(root, mx, my, 0.02f * g_H, at, object))
        space::AddWaypoint(at, object);
    else if (uiclick::MapClick(root))
        g_setWaypoint = 0.6f;
}

static void MouseFrame(u32 root)
{
    static unsigned long long last = 0;
    const unsigned long long now = GetTickCount64();
    const float dt = std::min(0.05f, static_cast<float>(now - last) / 1000.0f);
    last = now;
    navview::Drive d;
    const int wheel = hostcalls::g.RealMouseWheel ? hostcalls::g.RealMouseWheel() : 0;
    d.zoomStep = static_cast<float>(wheel) * navview::kWheelNotch;
    navview::Frame(root, d, dt, static_cast<float>(fl::MouseX()), static_cast<float>(fl::MouseY()));
    RightClick(root, dt);
}

void Frame(u32 root, const Rc& map, bool pad)
{
    if (!g_on)
    {
        if (g_w.root)
            GiveBack();
        return;
    }
    if (g_w.root != root)
        GiveBack(), g_w.root = root;
    g_port = map;
    g_lay = navlayout::Solve(g_W, g_H, deckmode::SideW());
    u32 view = 1;
    R32(root + kView, view);
    u8 animating = 0;
    R8(root + kAnimating, animating);
    // (the universe's art and links have no second scale: it keeps the square; the game's zoom in and out of it runs there too)
    const bool universe = view == 0 || animating;
    Grow(root, map, universe ? g_lay.map : navlayout::Bleed(g_W, g_H, g_lay.map), g_lay.map);
    // the screen is the map: what the player presses here is the map's, not the ship's, on keys and mouse as on the pad (the owner, 7 Nov 2026)
    if (g_w.grown && hostcalls::g.SetFlightLock)
        hostcalls::g.SetFlightLock();
    if (g_w.grown && !universe)
        HoldMarks(root);
    if (!pad && g_w.grown)
        MouseFrame(root);
    if (view != 0)
        ArtOff(root);
    ReadView(root);
    // the selection stays until another takes its place: the game drops it when the cursor leaves its popup, which stood beside the object; HUD 2's buttons stand elsewhere (the owner, 7 Nov 2026)
    W8(root + kPopupWatch, 0);
    // a selection by an object takes the object: the game's own radius is smaller than the mark the player aims at, so Set Waypoint and Best Path get the object, not the point beside it
    if (const u32 rec = SelectedRecord(root))
    {
        float pos[3] = {};
        if (safemem::Read(rec + kRecPos, pos, sizeof(pos)))
        {
            WF(root + kSelected, pos[0]);
            WF(root + kSelected + 4, pos[1]);
            WF(root + kSelected + 8, pos[2]);
        }
    }
    // the game's labels back wherever HUD 2's have no transform: the universe's view, a value not read
    LabelsOff(g_transform);
}

bool Grown() { return g_on && g_w.grown; }
bool On() { return g_on; }

// ---- the panels, from the game's data: the hidden texts have no reader yet (probe 3), so nothing here depends on one
static std::wstring Km(float metres)
{
    wchar_t b[32];
    swprintf_s(b, metres >= 1000 ? L"%.1f K" : L"%.0f M", metres >= 1000 ? metres / 1000 : metres);
    return b;
}

static float PanelFont(float H) { return 0.030f * H; }
constexpr int kRouteRows = 8;

float LeftHeight(const Rc&, float H)
{
    if (!Grown())
        return 0;
    const int rows = std::min<int>(kRouteRows, static_cast<int>(space::Waypoints().size()));
    return (0.060f + 0.040f + 0.08f + (rows ? 0.040f + rows * 0.036f : 0)) * H;
}

bool HasInfo()
{
    wchar_t obj[256] = {};
    return Grown() && (!space::Waypoints().empty() || (HostObjectiveText(obj, 256) && obj[0]));
}

float DrawLeft(const Rc& p, float H, bool title)
{
    if (!Grown())
        return p.y0;
    // (each block only with something to say: the next one takes its place, navskin sizes the plate to where this ends)
    const float pad = 0.018f * H, x = p.x0 + pad, w = p.W() - 2 * pad, f = PanelFont(H);
    float y = p.y0 + pad - 0.014f * H; // (the first block's own step brings it to the plate's air)
    if (title)
    {
        y = p.y0 + pad + 0.022f * H;
        TextFit(x, y, hudinfo::SystemName(), 0.040f * H, w, Argb(kInk, 1));
        y += 0.022f * H;
    }
    wchar_t obj[256] = {};
    if (HostObjectiveText(obj, 256) && obj[0])
    {
        y += 0.026f * H;
        Text(x, y, L"OBJECTIVE", TextFloor(), Argb(kChrome, 1));
        y += 0.030f * H;
        std::wstring lines[2];
        float size = f;
        const int n = FitLines(obj, f, w, 0, lines, 2, size);
        for (int i = 0; i < n; ++i)
            Text(x, y + i * 0.034f * H, lines[i], size, Argb(kInk, 1));
        y += (n - 1) * 0.034f * H + 0.017f * H;
    }
    const std::vector<space::Waypoint> wp = space::Waypoints();
    if (!wp.empty())
    {
        y += 0.026f * H;
        Text(x, y, L"ROUTE", TextFloor(), Argb(kChrome, 1));
        y += 0.032f * H;
        space::Pilot me;
        const bool have = space::ReadPilot(me);
        float from[3] = {me.pos[0], me.pos[1], me.pos[2]};
        for (size_t i = 0; i < wp.size() && i < static_cast<size_t>(kRouteRows); ++i)
        {
            const float dx = wp[i].pos[0] - from[0], dy = wp[i].pos[1] - from[1], dz = wp[i].pos[2] - from[2];
            const std::wstring name = wp[i].object ? hudinfo::RouteName(wp[i].object) : std::wstring(L"Waypoint");
            TextFit(x, y, std::to_wstring(i + 1) + L"  " + name, TextFloor(), w * 0.72f, Argb(kInk, 1));
            if (have)
                Text(x + w, y, Km(std::sqrt(dx * dx + dy * dy + dz * dz)), TextFloor(), Argb(kChrome, 1), Right);
            memcpy(from, wp[i].pos, sizeof(from));
            y += 0.036f * H;
        }
        y -= 0.018f * H;
    }
    return y;
}

void SetSelected(bool on) { g_selected = on; }

// The record at the map's selected point: +0x90C is a Vector, and the game finds the record by distance the same way (0x496D40): within its radius, the nearest.
static u32 SelectedRecord(u32 root)
{
    float at[3] = {};
    u32 head = 0, node = 0, best = 0;
    float bestD = 1e30f;
    if (!g_selected || !safemem::Read(root + kSelected, at, sizeof(at)) || !R32(root + kObjects, head) || !head || !R32(head, node))
        return 0;
    for (int i = 0; node && node != head && i < kMaxRecords; ++i)
    {
        u32 rec = 0;
        int type = -1;
        float pos[3] = {}, radius = 0;
        if (!R32(node + 8, rec))
            return 0;
        if (R32(rec, reinterpret_cast<u32&>(type)) && type == 0 && safemem::Read(rec + kRecPos, pos, sizeof(pos)) && RF(rec + kRecRadius, radius))
        {
            // (the selected point is where the cursor stood: a small station's radius is less than a mark on the screen, so the reach is the mark's too)
            const float zoom = g_view.view == 2 ? g_view.zoom : 1, sq = g_lay.map.W(), reach = sq > 0 ? 0.03f * g_H * 2 * (g_w.baseScale[0] / kFit) / (sq * zoom) : 0;
            const float dx = pos[0] - at[0], dz = pos[2] - at[2], d = dx * dx + dz * dz, r = std::max(radius * 1.05f, reach);
            u32 ids = 0;
            if (d <= r * r && d < bestD && R32(rec + kRecName, ids) && ids)
                bestD = d, best = rec;
        }
        if (!R32(node, node))
            return 0;
    }
    return best;
}

float RightHeight(const Rc&, float H)
{
    if (!Grown() || !SelectedRecord(g_w.root))
        return 0;
    return 0.130f * H;
}

bool HasCard() { return Grown() && SelectedRecord(g_w.root) != 0; }

float DrawRight(const Rc& p, float H)
{
    if (!Grown())
        return p.y0;
    const u32 rec = SelectedRecord(g_w.root);
    u32 ids = 0, id = 0;
    if (!rec || !R32(rec + kRecName, ids) || !ids)
        return p.y0;
    R32(rec + kRecId, id);
    const float x = p.x0 + 0.030f * H, w = p.W() - 0.048f * H;
    float att = 0;
    const bool known = id && space::AttitudeToPlayer(id, att);
    const u32 edge = !known ? kChrome : att < -0.3f ? kHostile : att > 0.3f ? kFriend : kNeutral;
    hud2kit::Rect(p.x0, p.y0 + 0.012f * H, std::max(2.0f, 0.004f * H), 0.106f * H, Argb(edge, 1)); // (the accent edge faces the reading start: R12)
    TextFit(x, p.y0 + 0.040f * H, text::Ids(ids), 0.040f * H, w, Argb(kInk, 1));
    if (id)
    {
        float pos[3] = {};
        space::Pilot me;
        std::wstring line = hudinfo::FactionName(id);
        if (space::ObjectPos(id, pos) && space::ReadPilot(me))
        {
            const float dx = pos[0] - me.pos[0], dy = pos[1] - me.pos[1], dz = pos[2] - me.pos[2];
            line += (line.empty() ? L"" : L"  \u00B7  ") + Km(std::sqrt(dx * dx + dy * dy + dz * dz));
        }
        TextFit(x, p.y0 + 0.086f * H, line, TextFloor(), w, Argb(kChrome, 1));
    }
    return p.y0 + 0.130f * H;
}

// ---- over the map: one mark per object, HUD 2's labels, the route, the player
static u32 AttitudeColour(u32 id)
{
    Named& n = g_names[id ^ 0x80000000u];
    const float now = static_cast<float>(GetTickCount64() % 100000000ULL) / 1000.0f;
    if (now >= n.nextAsk || now + 10 < n.nextAsk)
    {
        n.nextAsk = now + 1;
        float att = 0;
        n.colour = id && space::AttitudeToPlayer(id, att) ? (att < -0.3f ? kHostile : att > 0.3f ? kFriend : kNeutral) : kChrome;
    }
    return n.colour;
}

static void Mark(float x, float y, float r, int rank, u32 colour)
{
    if (rank == 1) // a gate: a diamond
    {
        const float pts[8] = {x, y - r, x + r, y, x, y + r, x - r, y};
        Outline(pts, 4, std::max(1.5f, r * 0.3f), Argb(colour, 1));
    }
    else
        Ring(x, y, r, std::max(1.5f, r * 0.3f), Argb(colour, 1));
}

// Where the map shows a point of the records' coordinates: the game's camera, so a mark stands on what it marks at every zoom (the transform's sum missed the camera's scale: the owner's
// "parallax", 7 Nov 2026). False off the viewport.
static bool At(const Rc& port, float x, float z, float& sx, float& sy) { return navview::Project(g_w.root, x, z, sx, sy) && port.Holds(sx, sy); }

static void DrawRoute(const Rc& port, float H)
{
    const std::vector<space::Waypoint> wp = space::Waypoints();
    space::Pilot me;
    if (wp.empty() || !space::ReadPilot(me))
        return;
    float px = 0, py = 0;
    const float ms = g_view.mapScale > 0 ? g_view.mapScale : 1; // (the records stand in the map's units)
    bool have = At(port, me.pos[0] * ms, me.pos[2] * ms, px, py);
    for (const space::Waypoint& w : wp)
    {
        float x = 0, y = 0;
        if (w.system != me.system || !At(port, w.pos[0] * ms, w.pos[2] * ms, x, y))
        {
            have = false;
            continue;
        }
        // one accent, used for nothing else on the map, the strongest line there (map-screens-rebuild.md 2)
        if (have)
            Line(px, py, x, y, std::max(3.0f, 0.0028f * H), Argb(kActive, 1));
        px = x, py = y, have = true; // (the waypoints' numbered diamonds are the game's own records)
    }
}

// Where the sectors stand, in place of the game's grid: small ticks at the corners, the letters and numbers dim, kept on the screen while their row or column shows.
static void DrawSectors(const Rc& port, float H)
{
    const float sx = g_w.baseScale[0] / kFit, sz = g_w.baseScale[1] / kFit;
    const float tick = 0.007f * H, width = std::max(1.0f, 0.001f * H);
    const u32 ink = Argb(kChrome, 0.30f), word = Argb(kChrome, 0.55f);
    float px[9] = {}, py[9] = {};
    for (int i = 0; i < 9; ++i)
    {
        float unused = 0;
        navview::Project(g_w.root, -sx + i * sx / 4, 0, px[i], unused);
        navview::Project(g_w.root, 0, -sz + i * sz / 4, unused, py[i]);
    }
    for (int i = 0; i < 9; ++i)
        for (int j = 0; j < 9; ++j)
            if (port.Holds(px[i], py[j]))
            {
                Line(px[i] - tick, py[j], px[i] + tick, py[j], width, ink);
                Line(px[i], py[j] - tick, px[i], py[j] + tick, width, ink);
            }
    const float size = TextFloor() * 0.9f;
    const float foot = std::max(py[0] - size, g_lay.map.y0 - size * 0.2f), side = std::max(px[0] - size, g_lay.map.x0 + size * 0.2f); // (the letters along the top: the foot is the prompts')
    for (int i = 0; i < 8; ++i)
    {
        const float cx = (px[i] + px[i + 1]) / 2, cy = (py[i] + py[i + 1]) / 2;
        if (cx > g_lay.map.x0 && cx < g_lay.map.x1)
            Text(cx, foot, std::wstring(1, static_cast<wchar_t>(L'A' + i)), size, word, Center);
        if (cy > g_lay.map.y0 && cy < g_lay.map.y1)
            Text(side, cy, std::to_wstring(i + 1), size, word, Center);
    }
}

void DrawOverMap(const Rc& port)
{
    if (!g_on || !g_transform)
        return;
    const float H = g_H;
    int layer = 0;
    if (exe::NavMapMode.ok())
        safemem::Read(static_cast<u32>(exe::NavMapMode.get()), &layer, 4);
    if (!navlabels::LabelsInLayer(layer))
        return;
    static navlabels::Label labels[kMaxLabels];
    static int order[kMaxLabels];
    static struct
    {
        u32 ids, id;
    } meta[kMaxLabels];
    u32 head = 0, node = 0;
    if (!R32(g_w.root + kObjects, head) || !head || !R32(head, node))
        return;
    int n = 0;
    g_records = 0;
    const float size = TextFloor(), r = 0.007f * H;
    for (int i = 0; node && node != head && i < kMaxRecords && n < kMaxLabels; ++i)
    {
        u32 rec = 0;
        if (!R32(node + 8, rec) || !R32(node, node))
            break;
        ++g_records;
        navlabels::Record d;
        u8 visible = 0;
        float pos[3] = {};
        if (!R32(rec, reinterpret_cast<u32&>(d.type)) || !R32(rec + kRecFlags, d.flags) || !safemem::Read(rec + kRecPos, pos, sizeof(pos)) || !RF(rec + kRecRadius, d.radius) ||
            !R8(rec + kRecVisible, visible) || !R32(rec + kRecName, d.nameIds))
            continue;
        d.visible = visible != 0, d.x = pos[0], d.z = pos[2];
        const int rank = navlabels::Rank(d);
        float x = 0, y = 0;
        if (rank < 0 || !At(port, d.x, d.z, x, y))
            continue;
        u32 id = 0;
        R32(rec + kRecId, id);
        Named& name = g_names[d.nameIds];
        if (name.text.empty())
            name.text = text::Ids(d.nameIds);
        navlabels::Label& l = labels[n];
        l = navlabels::Label();
        l.x = x, l.y = y, l.rank = rank, l.gap = r * 1.6f, l.h = size * 1.1f, l.w = TextWidth(name.text, size);
        l.pinned = rec == SelectedRecord(g_w.root);
        if (!g_w.showLabels || !navlabels::LabelByRule(d, g_view))
            l.w = 1e6f; // (no room for it anywhere: its mark stays)
        meta[n] = {d.nameIds, id};
        ++n;
    }
    const int shown = navlabels::Declutter(labels, n, order, port);
    g_drawn = static_cast<u32>(shown), g_dropped = static_cast<u32>(n - shown);
    DrawSectors(port, H);
    DrawRoute(port, H);
    for (int i = 0; i < n; ++i)
    {
        const navlabels::Label& l = labels[i];
        Mark(l.x, l.y, r, l.rank, AttitudeColour(meta[i].id));
        if (l.shown)
        {
            const Named& name = g_names[meta[i].ids];
            Text(l.left ? l.box.x1 : l.box.x0, l.y, name.text, size, Argb(kInk, 1), l.left ? Right : Left);
        }
    }
}

void Closed()
{
    g_marks.clear();
    if (g_w.root)
        GiveBack();
    navview::Closed();
}

void Shutdown()
{
    GiveBack();
    g_on = false;
}

std::string Command(std::istream& in)
{
    std::string what;
    in >> what;
    if (what == "on" || what == "off")
        g_on = what == "on";
    std::ostringstream o;
    float at[3] = {};
    if (g_w.root)
        safemem::Read(g_w.root + kSelected, at, sizeof(at));
    o << "navnew sel=" << g_selected << " at=" << at[0] << "," << at[2] << " rec=" << (g_w.root ? SelectedRecord(g_w.root) : 0) << " on=" << g_on << " grown=" << g_w.grown << " transform=" << g_transform << " view=" << g_view.view << " scale=" << g_view.scaleX << "," << g_view.scaleZ
      << " zoom=" << g_view.zoom << " records=" << g_records << " labels=" << g_drawn << " dropped=" << g_dropped << " gameLabelsOff=" << g_w.labelsOff << " show=" << g_w.showLabels;
    return o.str();
}

std::string Status()
{
    std::istringstream none;
    return Command(none);
}
} // namespace navnew
