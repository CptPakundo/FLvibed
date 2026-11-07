// The pad on the nav map: drives the game's map window buttons, waypoint list and route natively; owns its drawing.
#include "padmap.h"

#include "dimouse.h"
#include "game/deckmode.h"
#include "game/events.h"
#include "game/fl.h"
#include "game/hostcalls.h"
#include "game/hudinfo.h"
#include "game/navview.h"
#include "game/nnwin.h"
#include "ui/views.h"
#include "device.h"
#include "game/render.h"
#include "game/room.h"
#include "game/space.h"
#include "game/uidraw.h"
#include "gamepad.h"
#include "ui/glyphs.h"
#include "log.h"
#include "padmap_zones.h"
#include "promptbar.h"
#include "safemem.h"
#include "str.h"
#include "text.h"
#include "ui/ui.h"
#include "ui/uikit.h"
#include "ui/uiopt.h"
#include "ui/winplates.h"

#include <algorithm>
#include <sstream>

// (ASCII only: the build reads the sources as cp1252, so non-ASCII text goes in as \u escapes)
namespace padmap
{
using glyphs::Id;

struct Dest
{
    u32 id = 0, system = 0;
    std::wstring name;
    std::wstring kind; // BASE / PLANET / GATE / HOLE / STATION
    float dist = 0;
    float pos[3] = {};
};

static bool g_open = false;
static float g_refresh = 0;
static u32 g_owned = 0;
static bool g_list = false; // the destination list is shown
static std::vector<Dest> g_dests;
static int g_sel = 0;
static float g_xHeld = 0;
static bool g_xFired = false;
static float g_trigRepeat[2] = {};
static bool g_trigWas[2] = {};
constexpr float kXHold = 0.6f;
static int g_pointedWp = -1; // the waypoint under the cursor
static int g_stepAt = -1;    // D-pad up / down: where in the objects by distance, -1 not stepping
static float g_stepX = 0, g_stepY = 0;
static struct XAsk
{
    enum What
    {
        None,
        Add,
        Remove
    } what = None;
    float left = 0, waited = 0;
} g_xAsk; // X's press, waiting for the popup of the map's own click
static float g_widestHeld = 0; // seconds LT has pushed against the widest zoom (docs\MAP_PLAN.md: the detent before the universe)
constexpr float kUniverseDetent = 0.35f;
static navview::Gesture g_gesture; // the touchpad's
static float g_mapAx = 0, g_mapAy = 0; // where the last A over the map stood (the same spot = the selection's own place)
static struct
{
    bool route = false; // the press asks for the best path, waiting for its button
    float check = 0;    // seconds left to read the press's outcome
} g_wp;
static bool g_findCursor = false; // the cursor to (g_goX, g_goZ), once the view stands on it
static float g_goX = 0, g_goZ = 0;

static u32 Vis(const char* name);
// What the last A / X did, shown for a moment in place of that button's prompt (the owner, 7 Nov 2026: X "needs a prompt")
static struct
{
    glyphs::Id btn = glyphs::COUNT;
    std::wstring label;
    float left = 0;
} g_note;
static void Note(glyphs::Id btn, const wchar_t* label) { g_note.btn = btn, g_note.label = label, g_note.left = 1.6f; }

// The nav map's selection as the game shows it: its popup stands. 0x49EA50 shows a zoom button in every popup, and hides them with the rest (0x49F0E0). Not Set Waypoint: a click
// that hits an object's radius gets Info, Select Object and Best Path and no Set Waypoint, one that misses gets Set Waypoint, so the big bases read as "nothing there" and the
// small depots as selected (the owner's Newark, 7 Nov 2026: FREELANCER_RE "The nav map's selection").
static bool MapSelected() { return Vis("NeuroNetNavMap") && (Vis("NavMapOutZoom") || Vis("NavMapInZoom")); }
static padzones::Zones g_zones; // the skin's two zones; with no chips one zone, as ever
static float g_homePending = 0; // on the way to the ship's system through the universe view
static std::string g_lastAction;
static struct
{
    int rows = 0;
} g_drawn;

static u32 Vis(const char* name) { return uiclick::FindVisible(name); }
static bool ClickName(const char* name, const char* why)
{
    const u32 ctl = Vis(name);
    const bool ok = ctl && uiclick::Click(ctl);
    events::Push("pad map: %s %s %s", why, ok ? "clicked" : "no click on", name);
    g_lastAction = std::string(why) + (ok ? " ok" : " failed");
    return ok;
}

static std::wstring Upper(std::wstring s) { return str::Upper(std::move(s)); }

// The map's zoom buttons exist only in the popup of a selection (never with nothing selected: 0x49EA50 shows them), and
// each view has one: the sector view the out button, the others the in button. So a trigger first selects what lies
// under the cursor, the way the map's own click does; zooming out of a system's map is the universe button.
static void Zoom(bool in)
{
    const char* name = in ? "NavMapInZoom" : "NavMapOutZoom";
    const char* why = in ? "zoom in" : "zoom out";
    if (!Vis(name))
        uiclick::MapClick(Vis("NeuroNetNavMap"));
    if (Vis(name))
        ClickName(name, why);
    else if (!in && Vis("NavMapUniverseButton"))
        ClickName("NavMapUniverseButton", why);
    else
        events::Push("pad map: %s: nothing to zoom at the cursor", why), g_lastAction = std::string(why) + " none";
}

// This system's destinations from the game's object registry (space::Contacts): bases and planets with a dock, stations,
// gates and holes the player has found, nearest first. What the game still calls "Unknown ..." stays off the list: its name, and that it is there at all, is what finding it gives (Shockwave, 7 Nov 2026).
static void BuildDests()
{
    g_dests.clear();
    space::Pilot p;
    const bool inSpace = space::ReadPilot(p);
    for (const space::Contact& c : space::Contacts())
    {
        if (c.ship || c.id == 0)
            continue;
        const bool gate = (c.type & space::kJumpGate) != 0, hole = (c.type & space::kJumpHole) != 0;
        const bool ring = (c.type & space::kTradelaneRing) != 0;
        if (ring || (!c.dock && !gate && !hole && !c.base) || hudinfo::Undiscovered(c.id))
            continue;
        Dest d;
        d.id = c.id;
        d.system = inSpace ? p.system : 0;
        d.name = Upper(text::Ids(c.nameIds));
        if (d.name.empty())
            continue;
        d.kind = hole ? L"HOLE" : gate ? L"GATE" : (c.type & space::kPlanet) ? L"PLANET" : c.base ? L"BASE" : L"STATION";
        d.dist = c.distance;
        memcpy(d.pos, c.pos, sizeof(d.pos));
        g_dests.push_back(d);
    }
    std::sort(g_dests.begin(), g_dests.end(), [](const Dest& a, const Dest& b) { return a.dist < b.dist; });
    if (g_dests.size() > 40)
        g_dests.resize(40);
    g_sel = std::clamp(g_sel, 0, std::max(0, static_cast<int>(g_dests.size()) - 1));
}

static void NoteOver(promptbar::Prompt* p, int n)
{
    if (g_note.left > 0)
        for (int i = 0; i < n; ++i)
            if (p[i].btn == g_note.btn && !p[i].hold) // (the note is what a press did: never under a hold's HOLD, which then read as an offer to hold it, the owner, 7 Nov 2026)
                p[i].label = g_note.label.c_str(), p[i].enabled = true;
}

static void Prompts(bool inSpace, bool strip, bool panel)
{
    using promptbar::Prompt;
    Prompt p[5];
    int n = 0;
    if (panel && !g_list)
    {
        p[n++] = {Id::A, L"CHOOSE"};
        p[n++] = {Id::DU, L"NEXT", false, Id::DD};
        p[n++] = {Id::B, strip ? L"CLOSE LAYERS" : L"BACK"};
        promptbar::Set("navmap.panel", p, n, false);
        return;
    }
    if (g_list)
    {
        p[n++] = {Id::A, L"SET ROUTE"};
        p[n++] = {Id::DU, L"DESTINATION", false, Id::DD};
        p[n++] = {Id::Y, L"TRAVEL"};
        p[n++] = {strip ? Id::DL : Id::VIEW, L"CLOSE LIST"};
        p[n++] = {Id::B, L"CLOSE"};
        promptbar::Set("navmap.dest", p, n, true);
        return;
    }
    // Each prompt says what the press does right here, right now (the owner, 7 Nov 2026: "prompts that are really self-explanatory"; removing a waypoint had none): A says PLOT ROUTE
    // only where a press would plot one, X names the waypoint action the selection offers, and what cannot happen now is not offered. Five at most (promptbar.h), in this order.
    const float mx = static_cast<float>(fl::MouseX()), my = static_cast<float>(fl::MouseY()), tol = static_cast<float>(fl::ScreenHeight()) * 0.02f;
    const bool selected = MapSelected(), universe = !navview::Movable() && Vis("NavMapShipButton") && !Vis("NavMapUniverseButton");
    const bool again = selected && fabsf(mx - g_mapAx) < tol && fabsf(my - g_mapAy) < tol;
    const bool route = !space::Waypoints().empty();
    // (one fixed bar, in a fixed order: what cannot happen now dims and stays where it is; moving the map has its own line on the map, navskin; map-screens-rebuild.md 4)
    const float xFill = g_xFired ? 0 : Clampf(g_xHeld / kXHold, 0, 1);
    Prompt add = {Id::X, L"ADD WAYPOINT"}, hold = {Id::X, g_pointedWp >= 0 ? L"REMOVE WAYPOINT" : L"CLEAR ALL WAYPOINTS", true};
    add.enabled = !universe && g_pointedWp < 0;
    hold.enabled = !universe && route, hold.progress = hold.enabled ? xFill : 0;
    p[n++] = {Id::A, again ? L"PLOT ROUTE" : L"SELECT"};
    p[n++] = add;
    p[n++] = hold;
    if (inSpace)
    {
        Prompt fly = {Id::Y, L"FLY ROUTE"};
        fly.enabled = route;
        p[n++] = fly;
    }
    p[n++] = {Id::B, L"CLOSE"};
    NoteOver(p, n);
    promptbar::Set(inSpace ? "navmap" : "navmap.docked", p, n, g_note.left > 0);
}

// The skin's chips as the zones see them; a press and a step go to the HUD by the chip's id. No chips, no zones: the map's one zone.
static padzones::Out Zones(const Gamepad& pad)
{
    constexpr int kMax = 40;
    views::MapChip chips[kMax];
    padzones::Cell cells[kMax];
    const int n = g_list ? 0 : views::HudMapChips(chips, kMax);
    for (int i = 0; i < n; ++i)
        cells[i] = {chips[i].x0, chips[i].y0, chips[i].x1, chips[i].y1, chips[i].id, chips[i].enabled, chips[i].stepper};
    padzones::In in;
    in.dpad = pad.Pressed(XINPUT_GAMEPAD_DPAD_UP)      ? padzones::Up
              : pad.Pressed(XINPUT_GAMEPAD_DPAD_DOWN)  ? padzones::Down
              : pad.Pressed(XINPUT_GAMEPAD_DPAD_LEFT)  ? padzones::Left
              : pad.Pressed(XINPUT_GAMEPAD_DPAD_RIGHT) ? padzones::Right
                                                       : padzones::None;
    // beside the strip every direction has its own job (docs\MAP_PLAN.md): only D-pad right walks into the layers' card, and the D-pad walks inside it
    if (views::HudNNStrip() && g_zones.zone != padzones::Zone::Panel && in.dpad != padzones::Right)
        in.dpad = padzones::None;
    const PadState& s = pad.Cur();
    in.stick = s.lx != 0 || s.ly != 0;
    in.a = pad.Pressed(XINPUT_GAMEPAD_A), in.b = pad.Pressed(XINPUT_GAMEPAD_B);
    const padzones::Out o = g_zones.Step(in, cells, n);
    if (n)
        g_owned |= XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_DPAD_DOWN | XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_DPAD_RIGHT;
    if (o.ownsA)
        g_owned |= XINPUT_GAMEPAD_A;
    if (o.ownsB)
        g_owned |= XINPUT_GAMEPAD_B;
    views::HudMapChipFocus(g_zones.focus);
    if (o.press >= 0)
        views::HudMapChipPress(o.press);
    if (o.stepId >= 0)
        views::HudMapChipStep(o.stepId, o.stepDir);
    return o;
}

// The map's middle on a world point, the cursor on it once the view stands there.
static void GoTo(const float* pos)
{
    navview::CentreOn(pos[0], pos[2]);
    g_goX = pos[0], g_goZ = pos[2], g_findCursor = true;
}

static void FindShip()
{
    space::Pilot me;
    if (space::ReadPilot(me))
        GoTo(me.pos);
}

// One press for SYSTEM / SIRIUS, one back to the ship's system (6.1). The game shows one of its two buttons at a time, so from another system the way home is
// the universe first: the second click waits for the view to change (its zoom takes 1.5 s), and gives up rather than arrive late.
static void ViewPresses(const Gamepad& pad, float dt)
{
    g_owned |= XINPUT_GAMEPAD_RIGHT_THUMB | XINPUT_GAMEPAD_LEFT_THUMB;
    if (pad.Pressed(XINPUT_GAMEPAD_RIGHT_THUMB))
        ClickName(Vis("NavMapUniverseButton") ? "NavMapUniverseButton" : "NavMapShipButton", "system / sirius");
    const u32 root = Vis("NeuroNetNavMap");
    if (pad.Pressed(XINPUT_GAMEPAD_LEFT_THUMB))
    {
        // FIND ME: the ship's system, and in it the ship in the middle. Which system the map shows decides, not which button stands: NavMapShipButton can stand over the ship's own
        // system, and every press then clicked it and moved nothing (the owner's log, 7 Nov 2026: "my system clicked" x 7)
        if (navview::ShowsShipSystem(root))
            FindShip();
        else
            ClickName("NavMapShipButton", "my system"), g_homePending = 3;
    }
    if (g_homePending > 0)
    {
        g_homePending -= dt;
        if (navview::Movable() && navview::ShowsShipSystem(root))
            g_homePending = 0, FindShip();
    }
}

bool Update(float dt, const Gamepad& pad, bool mayOpen)
{
    g_owned = 0;
    g_refresh -= dt;
    if (g_refresh <= 0)
    {
        const bool open = mayOpen && Vis("NeuroNetNavMap") != 0; // (the root's name in game; NN_NavMap is the class)
        if (open != g_open)
        {
            Log("pad map %s", open ? "open" : "closed");
            g_list = false;
            views::HudMapList(false);
            g_xHeld = 0, g_xFired = false, g_homePending = 0, g_wp.route = false, g_wp.check = 0, g_widestHeld = 0, g_xAsk.what = XAsk::None, g_findCursor = false;
            g_zones.Reset();
            navview::Closed();
        }
        g_open = open;
        g_refresh = 0.1f;
    }
    if (!g_open)
        return false;
    g_note.left -= dt;
    const bool inSpace = !fl::PlayerBase() && fl::PlayerIObj() != nullptr; // (docked: the base, as the mode detector)

    // View: the destination list beside the map (in space; docked the bumpers, X and the triggers stay the map's and
    // the room's ring never sees them)
    g_owned |= XINPUT_GAMEPAD_X | XINPUT_GAMEPAD_LEFT_SHOULDER | XINPUT_GAMEPAD_RIGHT_SHOULDER;
    if (inSpace)
        g_owned |= XINPUT_GAMEPAD_BACK | XINPUT_GAMEPAD_Y;
    else
        g_list = false;
    const bool strip = views::HudNNStrip();
    // beside the strip the list is D-pad left, on the side it opens; the NeuroNet's button closes the NeuroNet (the owner, 7 Nov 2026), and the list with it
    if (strip && pad.Pressed(XINPUT_GAMEPAD_BACK))
    {
        g_owned |= XINPUT_GAMEPAD_BACK;
        g_list = false;
        ClickName("NavMapCloseButton", "close");
    }
    const bool listPress = strip ? g_zones.zone != padzones::Zone::Panel && pad.Pressed(XINPUT_GAMEPAD_DPAD_LEFT) : pad.Pressed(XINPUT_GAMEPAD_BACK);
    if (inSpace && listPress)
    {
        g_list = !g_list;
        if (g_list)
            BuildDests();
        events::Push("pad map: destination list %s (%d)", g_list ? "open" : "closed", static_cast<int>(g_dests.size()));
    }
    const padzones::Out zone = Zones(pad);
    const bool panel = g_zones.zone == padzones::Zone::Panel;
    if (strip)
        ViewPresses(pad, dt);
    if (g_list)
    {
        g_owned |= XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_DPAD_DOWN | XINPUT_GAMEPAD_A;
        const int n = static_cast<int>(g_dests.size());
        if (n)
        {
            const int step = pad.Pressed(XINPUT_GAMEPAD_DPAD_DOWN) ? 1 : pad.Pressed(XINPUT_GAMEPAD_DPAD_UP) ? -1 : 0;
            if (step)
            {
                g_sel = (g_sel + step + n) % n;
                GoTo(g_dests[g_sel].pos); // (the map shows the row the list stands on)
            }
            if (pad.Pressed(XINPUT_GAMEPAD_A))
            {
                const Dest& d = g_dests[g_sel];
                // the game's own best path (what the map's "Best path" button does): the waypoint list follows
                const bool ok = space::PlotBestPath(d.pos, d.id, d.system);
                events::Push("pad map: route to %s (%u) %s", text::Utf8(d.name).c_str(), d.id, ok ? "plotted" : "refused");
                g_lastAction = ok ? "best path plotted" : "best path refused";
            }
        }
    }
    else if (!zone.ownsA)
    {
        // A over the map area, as modern games' maps (docs\MAP_PLAN.md; the owner, 7 Nov 2026: "Go for research best practices"): a press selects the object at the cursor by the map's own
        // click, and that click alone: the free cursor's button clicked too, and a second click took the selection off again (the owner's Newark). A again on the selected object (the same
        // spot as the press that selected it) plots the best path to it: the map's own Best Path button. Waypoints are X's. Off the map A is the free cursor's, on whatever stands there.
        const float mx = static_cast<float>(fl::MouseX()), my = static_cast<float>(fl::MouseY());
        // (the game's own hit test at the cursor, not its hovered pointer: out of focus the hover is not recomputed)
        const uintptr_t hov = ui::HitTest(mx, my);
        const char* hn = ui::ControlName(hov);
        // (the root names itself NeuroNetNavMap in game, its class NN_NavMap: the check knew only the class, so no A over the map ever counted - 7 Nov 2026)
        const bool onMap = !hov || strcmp(hn, "NeuroNetNavMap") == 0 || strcmp(hn, "NN_NavMap") == 0 || strncmp(hn, "NavMapLeftWindow", 16) == 0 || strncmp(hn, "NavMapRightWindow", 17) == 0;
        if (onMap)
            g_owned |= XINPUT_GAMEPAD_A;
        if (onMap && pad.Pressed(XINPUT_GAMEPAD_A))
        {
            const float H = static_cast<float>(fl::ScreenHeight());
            const bool same = MapSelected() && fabsf(mx - g_mapAx) < H * 0.02f && fabsf(my - g_mapAy) < H * 0.02f; // (same = A again: the route)
            g_mapAx = mx, g_mapAy = my;
            g_wp.route = same, g_wp.check = 0.6f; // (the popup, and so the selection's buttons, comes a frame or more after the click)
            // (a plotted route hides Best Path in every popup: A again replaces it, so the old one goes and the map's click opens a popup that has the button)
            if (same && !Vis("NavMapBestPathButton"))
                space::ClearWaypoints();
            if (!same || !Vis("NavMapBestPathButton"))
                uiclick::MapClick(Vis("NeuroNetNavMap"));
            events::Push("pad map: A %s at %.0f, %.0f", same ? "again: route" : "the map's own click", mx, my);
        }
        if (g_wp.check > 0)
        {
            g_wp.check -= dt;
            const bool selected = MapSelected();
            if (g_wp.route && Vis("NavMapBestPathButton"))
            {
                g_wp.check = 0, g_wp.route = false;
                Note(Id::A, ClickName("NavMapBestPathButton", "best path") ? L"ROUTE PLOTTED" : L"NO ROUTE");
            }
            else if (!g_wp.route && selected)
                g_wp.check = 0, Note(Id::A, L"SELECTED");
            else if (g_wp.check <= 0)
            {
                Note(Id::A, g_wp.route ? L"NO ROUTE" : L"NOTHING HERE");
                events::Push("pad map: A %s", g_wp.route ? "no Best Path for this selection" : "selected nothing");
                g_wp.route = false;
            }
        }
    }
    // X (docs\MAP_PLAN.md; the owner, 7 Nov 2026): a tap makes a waypoint at the cursor, chained after the others, selection or none; a hold removes the waypoint the cursor points at,
    // and with none there clears the route. The map's own buttons: Set Waypoint and Clear Waypoint stand with a selection, and the map's own click at the cursor makes one
    const u32 root = Vis("NeuroNetNavMap");
    const float cx = static_cast<float>(fl::MouseX()), cy = static_cast<float>(fl::MouseY()), reach = static_cast<float>(fl::ScreenHeight()) * 0.025f;
    g_pointedWp = root ? navview::WaypointAt(root, cx, cy, reach) : -1;
    // D-pad up / down: the next destination the player has found, nearest the ship first, the map centred on it and the cursor on it (the owner, 7 Nov 2026), the list's own rows without the
    // list; a push of the stick starts over. Docked there are no destinations: the next object on the screen, nearest the cursor first
    if (strip && !panel && !g_list)
    {
        g_owned |= XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_DPAD_DOWN;
        if (pad.Cur().lx != 0 || pad.Cur().ly != 0)
            g_stepAt = -1;
        const int step = pad.Pressed(XINPUT_GAMEPAD_DPAD_DOWN) ? 1 : pad.Pressed(XINPUT_GAMEPAD_DPAD_UP) ? -1 : 0;
        if (step && g_stepAt < 0 && inSpace)
            BuildDests();
        const int dests = inSpace ? static_cast<int>(g_dests.size()) : 0;
        float xy[2 * 64];
        const int count = step && !dests ? navview::Objects(root, xy, 64) : 0;
        if (step && dests)
        {
            g_sel = g_stepAt < 0 ? (step > 0 ? 0 : dests - 1) : (g_sel + step + dests) % dests;
            g_stepAt = g_sel;
            GoTo(g_dests[g_sel].pos);
            events::Push("pad map: next destination %s", text::Utf8(g_dests[g_sel].name).c_str());
        }
        else if (count > 0)
        {
            if (g_stepAt < 0)
                g_stepX = cx, g_stepY = cy;
            int order[64];
            for (int i = 0; i < count; ++i)
                order[i] = i;
            const float ax = g_stepX, ay = g_stepY;
            std::sort(order, order + count, [&](int a, int b)
                      {
                          const float da = (xy[2 * a] - ax) * (xy[2 * a] - ax) + (xy[2 * a + 1] - ay) * (xy[2 * a + 1] - ay);
                          const float db = (xy[2 * b] - ax) * (xy[2 * b] - ax) + (xy[2 * b + 1] - ay) * (xy[2 * b + 1] - ay);
                          return da < db;
                      });
            g_stepAt = g_stepAt < 0 ? 0 : (g_stepAt + step + count) % count;
            dimouse::MoveBy(xy[2 * order[g_stepAt]] - cx, xy[2 * order[g_stepAt] + 1] - cy);
        }
    }
    const bool xDown = !panel && !g_list && pad.Down(XINPUT_GAMEPAD_X);
    const float xBefore = g_xHeld;
    g_xHeld = xDown ? g_xHeld + dt : 0;
    if (!xDown)
    {
        if (xBefore > 0 && xBefore < kXHold && !g_xFired)
        {
            const bool sameSpot = MapSelected() && fabsf(cx - g_mapAx) < reach && fabsf(cy - g_mapAy) < reach;
            float at[3] = {};
            u32 object = 0;
            if (g_pointedWp >= 0)
                Note(Id::X, L"HOLD TO REMOVE");
            else if (!navview::OnMap(root, cx, cy))
                Note(Id::X, L"POINT AT THE MAP");
            else if (!Vis("NavMapShipButton") && navview::ObjectAt(root, cx, cy, reach, at, object)) // (the game's popup offers no Set Waypoint on an object it hits: the list's own call, in the ship's system)
                Note(Id::X, space::AddWaypoint(at, object) ? L"WAYPOINT ADDED" : L"NOT ADDED");
            else if (sameSpot && Vis("NavMapSetWaypointButton"))
                Note(Id::X, ClickName("NavMapSetWaypointButton", "waypoint") ? L"WAYPOINT ADDED" : L"NOT ADDED");
            else if (uiclick::MapClick(root))
                g_xAsk = {XAsk::Add, 0.7f, 0}; // (the popup comes a frame or more after the click)
        }
        g_xFired = false;
    }
    else if (!g_xFired && g_xHeld >= kXHold)
    {
        g_xFired = true;
        if (g_pointedWp >= 0)
        {
            if (!Vis("NavMapClearWaypointButton"))
                uiclick::MapClick(root);
            g_xAsk = {XAsk::Remove, 0.7f, 0};
        }
        else
        {
            if (!ClickName("NavMapClearAllWaypointsButton", "clear"))
            {
                space::ClearWaypoints();
                events::Push("pad map: waypoints cleared natively");
            }
            Note(Id::X, L"WAYPOINTS CLEARED");
        }
    }
    if (g_xAsk.what != XAsk::None)
    {
        g_xAsk.left -= dt, g_xAsk.waited += dt;
        const char* button = g_xAsk.what == XAsk::Add ? "NavMapSetWaypointButton" : "NavMapClearWaypointButton";
        if (g_xAsk.waited >= 0.12f && Vis(button)) // (an older selection's buttons stand until the click's popup replaces them)
        {
            const bool add = g_xAsk.what == XAsk::Add, ok = ClickName(button, add ? "waypoint" : "remove");
            Note(Id::X, add ? (ok ? L"WAYPOINT ADDED" : L"NOT ADDED") : (ok ? L"WAYPOINT REMOVED" : L"NOT REMOVED"));
            g_xAsk.what = XAsk::None;
        }
        else if (g_xAsk.left <= 0)
        {
            Note(Id::X, g_xAsk.what == XAsk::Add ? L"NOT ADDED" : L"NOT REMOVED");
            events::Push("pad map: X found no button for the cursor's selection");
            g_xAsk.what = XAsk::None;
        }
    }
    // Y: travel the route with the autopilot (the list as it is)
    if (inSpace && !panel && pad.Pressed(XINPUT_GAMEPAD_Y))
    {
        if (space::Waypoints().empty()) // (7 Nov 2026: with no route the press closed the map and started a travel to nowhere)
            Note(Id::Y, L"NO ROUTE YET");
        else
        {
            std::istringstream in("start travel");
            const std::string r = views::ApCommand(in, "");
            events::Push("pad map: travel -> %s", r.substr(0, 80).c_str());
            g_lastAction = "travel: " + r.substr(0, 60);
            if (r.find("refused") == std::string::npos)
                fl::TapKey(VK_ESCAPE); // the map closes as the player's Escape closes it; the ship flies
        }
    }
    // the triggers and RS: the map's continuous zoom toward the cursor and its pan (docs\MAP_PLAN.md "P2's design"; core\game\navview.*). LT pushed on against the widest zoom
    // goes to the universe, the game's own fly-out; the universe does not scale, so there RT is the game's zoom into the system under the cursor, as it was
    const PadState& ps = pad.Cur();
    const u32 trig[2] = {PAD_LT, PAD_RT};
    navview::Drive drive;
    // (the panels' zone and the destination list have the D-pad, A and B: the map keeps its zoom and pan beside them, the owner, 7 Nov 2026)
    {
        drive.zoomRate = ps.rt - ps.lt;
        drive.panX = ps.rx, drive.panY = ps.ry;
        // the cursor pushed on against the screen's edge moves the map on under it (map-screens-rebuild.md 7), as if the stick were the right one
        const float W = static_cast<float>(fl::ScreenWidth()), H = static_cast<float>(fl::ScreenHeight()), edge = 0.06f;
        if ((cx < W * edge && ps.lx < 0) || (cx > W * (1 - edge) && ps.lx > 0))
            drive.panX = Clampf(drive.panX + ps.lx, -1, 1);
        if ((cy < H * edge && ps.ly > 0) || (cy > H * (1 - edge) && ps.ly < 0))
            drive.panY = Clampf(drive.panY + ps.ly, -1, 1);
        // the mouse's wheel, whichever device the frame calls the player's: only moving the mouse says K&M, and a wheel that waits for that does nothing
        if (hostcalls::g.RealMouseWheel)
            drive.zoomStep = static_cast<float>(hostcalls::g.RealMouseWheel()) * navview::kWheelNotch;
        // the touchpad: an extra, every gesture has its stick route above; `map_touchpad` turns it off. The click stays View (a finger on it moves nothing until it lifts)
        SdlPadTouch touch;
        if (uiopt::Get().mapTouchpad && pad.Touch(touch))
        {
            navview::Finger f[navview::kFingers];
            for (int i = 0; i < navview::kFingers; ++i)
                f[i] = {touch.down[i] != 0, touch.x[i], touch.y[i]};
            const navview::GestureOut g = navview::Read(g_gesture, f, touch.clicked != 0);
            drive.dragU = g.dragU, drive.dragT = g.dragT, drive.zoomStep += g.zoomStep, drive.touching = g.touching;
        }
        else
            g_gesture = navview::Gesture();
    }
    if (navview::Frame(Vis("NeuroNetNavMap"), drive, dt, static_cast<float>(fl::MouseX()), static_cast<float>(fl::MouseY())))
        g_mapAx = g_mapAy = -1e6f; // (what stands under the cursor has changed: the next A is a first press)
    if (g_findCursor)
    {
        g_findCursor = false;
        float sx = 0, sy = 0;
        if (navview::ProjectWorld(root, g_goX, g_goZ, sx, sy))
            dimouse::MoveBy(sx - static_cast<float>(fl::MouseX()), sy - static_cast<float>(fl::MouseY()));
    }
    views::HudMapList(g_list);
    g_widestHeld = !g_list && navview::Movable() && navview::AtWidest() && pad.Down(PAD_LT) && !pad.Down(PAD_RT) ? g_widestHeld + dt : 0;
    if (g_widestHeld >= kUniverseDetent)
    {
        g_widestHeld = 0;
        if (Vis("NavMapUniverseButton"))
            ClickName("NavMapUniverseButton", "zoom out");
    }
    for (int i = 0; i < 2; ++i)
    {
        const bool down = !panel && !navview::Movable() && pad.Down(trig[i]);
        g_trigRepeat[i] -= dt;
        if (down && (!g_trigWas[i] || g_trigRepeat[i] <= 0) && i == 1)
        {
            Zoom(true);
            g_trigRepeat[i] = g_trigWas[i] ? 0.25f : 0.45f;
        }
        g_trigWas[i] = down;
    }
    // the bumpers: the strip's tabs while it stands (the system and the universe have ViewPresses); else the ship's system / the universe
    if (pad.Pressed(XINPUT_GAMEPAD_LEFT_SHOULDER))
    {
        if (strip)
            nnwin::Step(-1);
        else
            ClickName("NavMapShipButton", "system");
    }
    if (pad.Pressed(XINPUT_GAMEPAD_RIGHT_SHOULDER))
    {
        if (strip)
            nnwin::Step(+1);
        else
            ClickName("NavMapUniverseButton", "universe");
    }
    // B: the map's own close button (else the game's Escape through the free cursor's B)
    if (Vis("NavMapCloseButton"))
    {
        g_owned |= XINPUT_GAMEPAD_B;
        if (pad.Pressed(XINPUT_GAMEPAD_B) && !zone.ownsB)
        {
            if (g_list)
                g_list = false;
            else
                ClickName("NavMapCloseButton", "close");
        }
    }
    Prompts(inSpace, strip, g_zones.zone == padzones::Zone::Panel);
    return true;
}

u32 Owned() { return g_open ? g_owned : 0; }
bool IsOpen() { return g_open; }
bool OwnsRightStick() { return g_open && navview::Movable(); }

constexpr u32 kHud = 0x82C9F3;
constexpr u32 kDim = 0x6E8EA8;
constexpr u32 kBack = 0x0D0B1A;
static u32 Argb(u32 rgb, float a) { return (static_cast<u32>(Clampf(a, 0, 1) * 255 + 0.5f) << 24) | (rgb & 0xFFFFFF); }

void Draw(void* device)
{
    g_drawn = decltype(g_drawn)();
    if (!g_open || !g_list || !device || !device::Pad())
        return;
    render::SetDevice(device);
    int sw = 0, sh = 0;
    if (!render::ScreenSize(sw, sh) || sh < 100)
        return;
    const float H = static_cast<float>(sh);
    const uiopt::UiOptions& o = uiopt::Get();
    const uikit::Fonts& f = uikit::FontsFor(sh, o.textSize);
    if (!f.ok || !render::Begin(f.label))
        return;
    // the list beside the map: a title, the rows around the selection. Under HUD 2 it stands on the left panel, solid, from that panel's top down to the prompts (the owner's frame,
    // 7 Nov 2026: at the screen's left it lay half over the VIEW panel, which showed through it); the classic map has no panel there, so it stands at the screen's left
    const float rowH = f.label.lineHeight * 1.25f;
    float x0 = H * 0.03f, w = H * 0.34f, panelTop = -1;
    if (views::HudNNStrip())
        x0 = sw * deckmode::SideW() + H * 0.012f, w = H * 0.40f - H * 0.024f, panelTop = H * (deckmode::EdgeH() + 0.078f);
    const float titleH = f.title.lineHeight * 1.4f;
    const float bottom = promptbar::Top(H, f.label) - H * 0.075f; // (clear of the line over the lane)
    const int maxRows = panelTop >= 0 ? std::max(6, static_cast<int>((bottom - panelTop - titleH) / rowH)) : 18;
    const int n = static_cast<int>(g_dests.size());
    const int first = std::clamp(g_sel - maxRows / 2, 0, std::max(0, n - maxRows));
    const int shown = std::min(maxRows, n - first);
    const float top = panelTop >= 0 ? panelTop : H * 0.5f - (shown * rowH + titleH) * 0.5f;
    const float listH = panelTop >= 0 ? bottom - top : shown * rowH + titleH; // (on the panel: all the way down, so nothing of it shows under a short list)
    uidraw::HudPanel(x0 - H * 0.012f, top - H * 0.012f, w + H * 0.024f, listH + H * 0.024f, 1.0f, H, GetTickCount64() / 1000.0);
    render::SetFont(f.title);
    render::Text(x0, top, L"DESTINATIONS", uidraw::kText);
    render::SetFont(f.label);
    float y = top + f.title.lineHeight * 1.4f;
    if (!n)
        render::Text(x0, y, L"NOTHING FOUND HERE YET", Argb(kDim, 0.95f));
    for (int i = first; i < first + shown; ++i, y += rowH)
    {
        const Dest& d = g_dests[i];
        const bool sel = i == g_sel;
        if (sel)
            uidraw::FocusFrame(x0 - H * 0.006f, y - (rowH - f.label.lineHeight) * 0.5f, w + H * 0.012f, rowH, H);
        wchar_t dist[32];
        if (d.dist >= 1000)
            swprintf_s(dist, L"%.1f KM", d.dist / 1000);
        else
            swprintf_s(dist, L"%.0f M", d.dist);
        render::Text(x0, y, d.name, sel ? uidraw::kSelected : uidraw::kText);
        render::SetFont(f.body);
        const float dw = static_cast<float>(render::TextWidth(f.body, dist));
        render::Text(x0 + w - dw, y + (f.label.lineHeight - f.body.lineHeight) * 0.5f, dist, Argb(kDim, 0.95f));
        const float kw = static_cast<float>(render::TextWidth(f.body, d.kind));
        const float nameW = static_cast<float>(render::TextWidth(f.label, d.name));
        if (x0 + nameW + H * 0.012f < x0 + w - dw - H * 0.012f - kw) // (a long name keeps the row: the kind gives way, the name stays whole)
            render::Text(x0 + w - dw - H * 0.012f - kw, y + (f.label.lineHeight - f.body.lineHeight) * 0.5f, d.kind, Argb(kDim, 0.8f));
        render::SetFont(f.label);
        ++g_drawn.rows;
    }
    render::End(false);
}

std::string Status()
{
    std::ostringstream o;
    const u32 map = g_open ? Vis("NeuroNetNavMap") : 0;
    o << "map open=" << g_open << " view=" << (map ? static_cast<int>(Mem<u32>(map + 0x7D0)) : -1) << " list=" << g_list
      << " zone=" << (g_zones.zone == padzones::Zone::Panel ? "panel" : "canvas") << " focus=" << g_zones.focus << " dests=" << g_dests.size() << " sel=" << g_sel
      << " owned=0x" << std::hex << g_owned << std::dec << " drawnRows=" << g_drawn.rows << " last='" << g_lastAction
      << "' waypoints=" << space::Waypoints().size();
    return o.str();
}

std::string Command(std::istream& in)
{
    std::string what;
    in >> what;
    if (what == "view")
        return navview::Command(in);
    if (what == "dest")
    {
        if (g_dests.empty())
            BuildDests();
        std::ostringstream o;
        for (size_t i = 0; i < g_dests.size(); ++i)
            o << (static_cast<int>(i) == g_sel ? "* " : "  ") << i << " " << text::Utf8(g_dests[i].name) << " "
              << text::Utf8(g_dests[i].kind) << " " << g_dests[i].id << " " << g_dests[i].dist << " m\n";
        return o.str() + Status();
    }
    return Status() + "\n" + promptbar::Status();
}

void Shutdown() {}
} // namespace padmap
