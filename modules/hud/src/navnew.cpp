// The new map behind its seam: navnew.h. The fields: 07_navmap_facts.md and docs\FREELANCER_RE.md "The nav map's labels and records"; the numbers: navmap_layout.h.
//
// Each write is remembered and given back while it still holds FLvibed's value; the window is a new object at every open, so a write's life is one opening. Every part
// falls back to the game's own: a transform that does not read gives the game's labels back, a control that will not move stays where it is.
#include "navnew.h"

#include "flexe.h"
#include "game/deckmode.h"
#include "game/hudinfo.h"
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

namespace navnew
{
using namespace hud2kit;
using namespace winplates;
using Rc = navlayout::Rect;

constexpr u32 kFrameOrigin = 0x40C, kBoxW = 0x418, kBoxH = 0x41C, kScaleX = 0x4A8, kScaleZ = 0x4AC, kObjects = 0x400, kSelected = 0x90C, kZoom = 0x3F8, kCentre = 0x46C, kView = 0x7D0,
              kMapScale = 0xF48, kAnimating = 0x478;
// The record's fields.
constexpr u32 kRecFlags = 0x04, kRecPos = 0x30, kRecRadius = 0x3C, kRecVisible = 0x48, kRecName = 0x4C, kRecId = 0x70;
constexpr int kMaxRecords = 400, kMaxLabels = 128;
// The game's box at its own size: labels follow a grown box by the ratio of the two (12a row 2).
constexpr float kNativeBox = 0.468f;

// Off until the hit-box sweep and a second install have passed (UI_HUD2_SKIN.md 6.1).
static bool g_on = false;

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
    g_names.clear();
}

// +0x4A8 / +0x4AC scale the game's labels to a box of its own size: written every frame the map shows, because it rewrites +0x4A8 on a return from the universe.
static void HoldScale(u32 root, float boxW, float boxH)
{
    float v = 0;
    const u32 at[2] = {root + kScaleX, root + kScaleZ};
    const float box[2] = {boxW, boxH};
    for (int i = 0; i < 2; ++i)
        if (RF(at[i], v) && v > 0)
        {
            if (v != g_w.mineScale[i])
                g_w.baseScale[i] = v;
            g_w.mineScale[i] = g_w.baseScale[i] * kNativeBox / box[i];
            WF(at[i], g_w.mineScale[i]);
        }
}

// The grid's and titles' texts stay where the game built them (12a row 2): moved by the distance the map moved, through the control's own signed setter.
static void MoveText(u32 ctl, float dx, float dy, float camX, float camY)
{
    float pos[3] = {};
    u32 vt = 0, set = 0;
    if (!ctl || !safemem::Read(ctl + 0x60, pos, sizeof(pos)) || !R32(ctl, vt) || !R32(vt + 0x14, set) || !exe::ControlSetPosition.is(set) || !(pos[2] < 0))
        return;
    pos[0] += dx * -pos[2] / camX, pos[1] += dy * -pos[2] / camY;
    __try
    {
        reinterpret_cast<void(__thiscall*)(u32, const float*)>(set)(ctl, pos);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

static void PlaceTexts(u32 root, const Rc& from, const Rc& to)
{
    if (!exe::UiCamera.ok())
        return;
    float camX = 0, camY = 0;
    __try
    {
        camX = Mem<float>(exe::UiCamera, 0x78), camY = Mem<float>(exe::UiCamera, 0x7C);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return;
    }
    if (std::fabs(camX) < 1 || std::fabs(camY) < 1)
        return;
    const float gx = to.W() / 8, gy = to.H() / 8, ox = from.W() / 8, oy = from.H() / 8;
    for (int i = 0; i < 8; ++i)
    {
        char n[32];
        sprintf_s(n, "NavMapGridLetter%c", 'A' + i);
        MoveText(Child(root, n), (to.x0 + gx * (i + 0.5f)) - (from.x0 + ox * (i + 0.5f)), to.y1 - from.y1, camX, camY);
        sprintf_s(n, "NavMapGridNumber%d", i + 1);
        MoveText(Child(root, n), to.x0 - from.x0, (to.y0 + gy * (i + 0.5f)) - (from.y0 + oy * (i + 0.5f)), camX, camY);
    }
}

// The growth: once the window has stopped sliding, the box at the solver's size. The viewport's pixels per box unit are read, not assumed.
static void Grow(u32 root, const Rc& port)
{
    float boxW = 0, boxH = 0, origin[3] = {};
    if (!RF(root + kBoxW, boxW) || !RF(root + kBoxH, boxH) || !safemem::Read(root + kFrameOrigin, origin, sizeof(origin)) || !(boxW > 0.05f) || !(boxH > 0.05f))
        return;
    if (!g_w.grown)
    {
        if (room::NNWindowBusy())
            return;
        const float kx = port.W() / boxW, ky = port.H() / boxH;
        const Rc& t = g_lay.map;
        g_w.box[0] = boxW, g_w.box[1] = boxH;
        memcpy(g_w.origin, origin, sizeof(origin));
        g_w.mineBox[0] = t.W() / kx, g_w.mineBox[1] = t.H() / ky;
        g_w.mineOrigin[0] = origin[0] + (t.x0 - port.x0) / kx;
        g_w.mineOrigin[1] = origin[1] - (t.y1 - port.y1) / ky; // (the box's y runs up)
        g_w.mineOrigin[2] = origin[2];
        WF(root + kFrameOrigin, g_w.mineOrigin[0]);
        WF(root + kFrameOrigin + 4, g_w.mineOrigin[1]);
        WF(root + kBoxW, g_w.mineBox[0]);
        WF(root + kBoxH, g_w.mineBox[1]);
        g_w.grown = true;
        PlaceTexts(root, port, t);
        Log("navnew: the map grown to %.0f x %.0f px at %.0f, %.0f", t.W(), t.H(), t.x0, t.y0);
    }
    else if (!room::NNWindowBusy())
    {
        // a window moved after the growth (windowtext's drop, a mod): the map goes back to the solver's place, its texts with it
        const Rc& t = g_lay.map;
        const float kx = port.W() / boxW, ky = port.H() / boxH, ex = t.x0 - port.x0, ey = t.y1 - port.y1;
        if (std::fabs(ex) > 1 || std::fabs(ey) > 1)
        {
            g_w.mineOrigin[0] += ex / kx, g_w.mineOrigin[1] -= ey / ky;
            WF(root + kFrameOrigin, g_w.mineOrigin[0]);
            WF(root + kFrameOrigin + 4, g_w.mineOrigin[1]);
            PlaceTexts(root, port, {port.x0 + ex, port.y0 + ey, port.x1 + ex, port.y1 + ey});
        }
    }
    HoldScale(root, g_w.mineBox[0], g_w.mineBox[1]);
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
    if (g_w.grown)
        v.scaleX = g_w.baseScale[0], v.scaleZ = g_w.baseScale[1];
    else
        RF(root + kScaleX, v.scaleX), RF(root + kScaleZ, v.scaleZ);
    g_view = v;
    g_transform = (v.view == 1 || v.view == 2) && v.scaleX > 0 && v.scaleZ > 0;
}

void Frame(u32 root, const Rc& map)
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
    Grow(root, map);
    ReadView(root);
    // the game's labels back wherever HUD 2's have no transform: the universe's view, a value not read
    LabelsOff(g_transform);
}

bool Grown() { return g_on && g_w.grown; }

// ---- the panels, from the game's data: the hidden texts have no reader yet (probe 3), so nothing here depends on one
static std::wstring Km(float metres)
{
    wchar_t b[32];
    swprintf_s(b, metres >= 1000 ? L"%.1f K" : L"%.0f M", metres >= 1000 ? metres / 1000 : metres);
    return b;
}

static float PanelFont(float H) { return 0.030f * H; }

float LeftHeight(const Rc&, float H)
{
    if (!Grown())
        return 0;
    const int rows = std::min<int>(4, static_cast<int>(space::Waypoints().size()));
    return (0.060f + 0.040f + 0.08f + (rows ? 0.040f + rows * 0.036f : 0)) * H;
}

float DrawLeft(const Rc& p, float H)
{
    if (!Grown())
        return p.y0;
    const float x = p.x0 + 0.018f * H, w = p.W() - 0.036f * H, f = PanelFont(H);
    float y = p.y0 + 0.034f * H;
    TextFit(x, y, hudinfo::SystemName(), 0.040f * H, w, Argb(kInk, 1));
    y += 0.040f * H;
    Text(x, y, L"OBJECTIVE", TextFloor(), Argb(kChrome, 1));
    y += 0.030f * H;
    wchar_t obj[256] = {};
    if (HostObjectiveText(obj, 256))
    {
        std::wstring lines[2];
        float size = f;
        const int n = FitLines(obj, f, w, 0, lines, 2, size);
        for (int i = 0; i < n; ++i)
            Text(x, y + i * 0.034f * H, lines[i], size, Argb(kInk, 1));
    }
    y += 0.07f * H;
    const std::vector<space::Waypoint> wp = space::Waypoints();
    if (!wp.empty())
    {
        Text(x, y, L"ROUTE", TextFloor(), Argb(kChrome, 1));
        y += 0.032f * H;
        space::Pilot me;
        const bool have = space::ReadPilot(me);
        float from[3] = {me.pos[0], me.pos[1], me.pos[2]};
        for (size_t i = 0; i < wp.size() && i < 4; ++i)
        {
            const float dx = wp[i].pos[0] - from[0], dy = wp[i].pos[1] - from[1], dz = wp[i].pos[2] - from[2];
            const std::wstring name = wp[i].object ? hudinfo::DisplayName(wp[i].object) : std::wstring(L"Waypoint");
            TextFit(x, y, std::to_wstring(i + 1) + L"  " + name, TextFloor(), w * 0.72f, Argb(kInk, 1));
            if (have)
                Text(x + w, y, Km(std::sqrt(dx * dx + dy * dy + dz * dz)), TextFloor(), Argb(kChrome, 1), Right);
            memcpy(from, wp[i].pos, sizeof(from));
            y += 0.036f * H;
        }
    }
    return y;
}

// The record the map has selected, if +0x90C really holds one of its own records.
static u32 SelectedRecord(u32 root)
{
    u32 want = 0, head = 0, node = 0;
    if (!R32(root + kSelected, want) || !want || !R32(root + kObjects, head) || !head || !R32(head, node))
        return 0;
    for (int i = 0; node && node != head && i < kMaxRecords; ++i)
    {
        u32 rec = 0;
        if (!R32(node + 8, rec))
            return 0;
        if (rec == want)
            return rec;
        if (!R32(node, node))
            return 0;
    }
    return 0;
}

float RightHeight(const Rc&, float H)
{
    if (!Grown() || !SelectedRecord(g_w.root))
        return 0;
    return 0.130f * H;
}

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

static void DrawRoute(const Rc& port, float H)
{
    const std::vector<space::Waypoint> wp = space::Waypoints();
    space::Pilot me;
    if (wp.empty() || !space::ReadPilot(me))
        return;
    float px = 0, py = 0;
    bool have = navlabels::ToScreen(g_view, port, me.pos[0], me.pos[2], px, py);
    for (const space::Waypoint& w : wp)
    {
        float x = 0, y = 0;
        if (w.system != me.system || !navlabels::ToScreen(g_view, port, w.pos[0], w.pos[2], x, y))
        {
            have = false;
            continue;
        }
        if (have)
        {
            // dashes, as a plotted thing that is not there: the game's own lines stay solid
            const float dx = x - px, dy = y - py, len = std::sqrt(dx * dx + dy * dy), dash = 0.010f * H;
            for (float t = 0; t + dash < len; t += 2 * dash)
                Line(px + dx * t / len, py + dy * t / len, px + dx * (t + dash) / len, py + dy * (t + dash) / len, std::max(2.0f, 0.002f * H), Argb(kActive, 1));
        }
        px = x, py = y, have = true;
    }
}

static void DrawYou(const Rc& port, float H)
{
    space::Pilot me;
    float x = 0, y = 0, pos[3] = {}, rot[9] = {};
    if (!space::ReadPilot(me) || !navlabels::ToScreen(g_view, port, me.pos[0], me.pos[2], x, y))
        return;
    const float r = 0.012f * H;
    Ring(x, y, r, std::max(2.0f, 0.002f * H), Argb(kInk, 1));
    if (space::ObjectPose(space::PlayerShipId(), pos, rot))
    {
        // forward = -third column of the rows (space.h); on the map x runs right and z down
        const float fx = -rot[2], fz = -rot[8], len = std::sqrt(fx * fx + fz * fz);
        if (len > 0.01f)
            Line(x, y, x + fx / len * r * 1.8f, y + fz / len * r * 1.8f, std::max(2.0f, 0.002f * H), Argb(kInk, 1));
    }
    Text(x + r * 1.6f, y, L"YOU", TextFloor(), Argb(kActive, 1));
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
        if (rank < 0 || !navlabels::ToScreen(g_view, port, d.x, d.z, x, y))
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
    DrawYou(port, H);
}

void Closed()
{
    if (g_w.root)
        GiveBack();
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
    o << "navnew on=" << g_on << " grown=" << g_w.grown << " transform=" << g_transform << " view=" << g_view.view << " scale=" << g_view.scaleX << "," << g_view.scaleZ
      << " zoom=" << g_view.zoom << " records=" << g_records << " labels=" << g_drawn << " dropped=" << g_dropped << " gameLabelsOff=" << g_w.labelsOff << " show=" << g_w.showLabels;
    return o.str();
}

std::string Status()
{
    std::istringstream none;
    return Command(none);
}
} // namespace navnew
