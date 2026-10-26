#include "padmap.h"

#include "../dimouse.h"
#include "../events.h"
#include "../fl.h"
#include "autopilot.h"
#include "device.h"
#include "game/render.h"
#include "game/room.h"
#include "game/space.h"
#include "game/uidraw.h"
#include "gamepad.h"
#include "glyphs.h"
#include "log.h"
#include "promptbar.h"
#include "text.h"
#include "ui.h"
#include "uikit.h"
#include "uiopt.h"

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
static float g_sinceMapA = 1e9f; // seconds since the last A over the map area (the "again = waypoint" rule)
static float g_mapAx = 0, g_mapAy = 0;
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

static std::wstring Upper(std::wstring s)
{
    for (wchar_t& c : s)
        c = static_cast<wchar_t>(towupper(c));
    return s;
}

// This system's destinations from the game's object registry (space::Contacts): bases and planets with a dock, stations,
// gates and holes; nearest first. (The player's "known" flag per object is not readable here: every object of the
// current system is listed - reported.)
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
        if (ring || (!c.dock && !gate && !hole && !c.base))
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

static void Prompts(bool inSpace)
{
    using promptbar::Prompt;
    Prompt p[5];
    int n = 0;
    if (!inSpace)
    {
        // docked: the same map window, without the flight's travel and the destination list (this system's objects
        // are space objects)
        p[n++] = {Id::A, L"SELECT \u00b7 AGAIN = WAYPOINT"};
        p[n++] = {Id::X, L"REMOVE \u00b7 HOLD CLEAR"};
        p[n++] = {Id::LT, L"ZOOM", false, Id::RT};
        p[n++] = {Id::LB, L"SYSTEM / UNIVERSE", false, Id::RB};
        p[n++] = {Id::B, L"CLOSE"};
        promptbar::Set("navmap.docked", p, n, false);
        return;
    }
    if (g_list)
    {
        p[n++] = {Id::A, L"SET ROUTE"};
        p[n++] = {Id::DU, L"DESTINATION", false, Id::DD};
        p[n++] = {Id::Y, L"TRAVEL"};
        p[n++] = {Id::VIEW, L"CLOSE LIST"};
        p[n++] = {Id::B, L"CLOSE"};
        promptbar::Set("navmap.dest", p, n, true);
        return;
    }
    p[n++] = {Id::A, L"SELECT \u00B7 AGAIN = WAYPOINT"};
    p[n++] = {Id::X, L"REMOVE \u00B7 HOLD CLEAR"};
    Prompt y = {Id::Y, L"TRAVEL"};
    p[n++] = y;
    p[n++] = {Id::LT, L"ZOOM", false, Id::RT};
    p[n++] = {Id::VIEW, L"DESTINATIONS"};
    promptbar::Set("navmap", p, n, false);
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
            g_xHeld = 0, g_xFired = false, g_sinceMapA = 1e9f;
        }
        g_open = open;
        g_refresh = 0.1f;
    }
    if (!g_open)
        return false;
    g_sinceMapA += dt;
    const bool inSpace = !fl::PlayerBase() && fl::PlayerIObj() != nullptr; // (docked: the base, as the mode detector)

    // View: the destination list beside the map (in space; docked the bumpers, X and the triggers stay the map's and
    // the room's ring never sees them)
    g_owned |= XINPUT_GAMEPAD_X | XINPUT_GAMEPAD_LEFT_SHOULDER | XINPUT_GAMEPAD_RIGHT_SHOULDER;
    if (inSpace)
        g_owned |= XINPUT_GAMEPAD_BACK | XINPUT_GAMEPAD_Y;
    else
        g_list = false;
    if (inSpace && pad.Pressed(XINPUT_GAMEPAD_BACK))
    {
        g_list = !g_list;
        if (g_list)
            BuildDests();
        events::Push("pad map: destination list %s (%d)", g_list ? "open" : "closed", static_cast<int>(g_dests.size()));
    }
    if (g_list)
    {
        g_owned |= XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_DPAD_DOWN | XINPUT_GAMEPAD_A;
        const int n = static_cast<int>(g_dests.size());
        if (n)
        {
            if (pad.Pressed(XINPUT_GAMEPAD_DPAD_UP))
                g_sel = (g_sel - 1 + n) % n;
            if (pad.Pressed(XINPUT_GAMEPAD_DPAD_DOWN))
                g_sel = (g_sel + 1) % n;
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
    else
    {
        // A over the map area: the game's own click selects (the free cursor's button); a second A at the same spot
        // within 2 s plots the selection as a waypoint (the map's own Set Waypoint button)
        if (pad.Pressed(XINPUT_GAMEPAD_A))
        {
            // (the game's own hit test at the cursor, not its hovered pointer: out of focus the hover is not recomputed)
            const uintptr_t hov = ui::HitTest(static_cast<float>(fl::MouseX()), static_cast<float>(fl::MouseY()));
            const char* hn = ui::ControlName(hov);
            const bool onMap = !hov || strcmp(hn, "NN_NavMap") == 0 || strncmp(hn, "NavMapLeftWindow", 16) == 0 ||
                               strncmp(hn, "NavMapRightWindow", 17) == 0;
            const float mx = static_cast<float>(fl::MouseX()), my = static_cast<float>(fl::MouseY());
            const float H = static_cast<float>(fl::ScreenHeight());
            if (onMap && g_sinceMapA < 2.0f && fabsf(mx - g_mapAx) < H * 0.02f && fabsf(my - g_mapAy) < H * 0.02f)
            {
                ClickName("NavMapSetWaypointButton", "waypoint");
                g_sinceMapA = 1e9f;
            }
            else if (onMap)
                g_sinceMapA = 0, g_mapAx = mx, g_mapAy = my;
        }
    }
    // X: remove the last waypoint; held: clear the route
    const bool xDown = pad.Down(XINPUT_GAMEPAD_X);
    const float xBefore = g_xHeld;
    g_xHeld = xDown ? g_xHeld + dt : 0;
    if (!xDown)
    {
        if (xBefore > 0 && xBefore < 0.6f && !g_xFired)
        {
            // the map's own button when it shows one (a waypoint selected), else the game's remove-one routine on the
            // last entry (exe::RemoveWaypoint, space::RemoveWaypoint; the HOST integration, 25 Oct 2026)
            if (!ClickName("NavMapClearWaypointButton", "remove"))
            {
                const int n = static_cast<int>(space::Waypoints().size());
                const bool ok = n > 0 && space::RemoveWaypoint(n - 1);
                events::Push("pad map: last waypoint %s (%d left)", ok ? "removed natively" : "not removed",
                             ok ? n - 1 : n);
                g_lastAction = ok ? "last waypoint removed" : "no waypoint to remove";
            }
        }
        g_xFired = false;
    }
    else if (!g_xFired && g_xHeld >= 0.6f)
    {
        g_xFired = true;
        if (!ClickName("NavMapClearAllWaypointsButton", "clear"))
        {
            space::ClearWaypoints();
            events::Push("pad map: waypoints cleared natively");
        }
    }
    // Y: travel the route with the autopilot (the list as it is)
    if (inSpace && pad.Pressed(XINPUT_GAMEPAD_Y))
    {
        if (!inSpace)
            events::Push("pad map: travel refused (not in space)");
        else
        {
            std::istringstream in("start travel");
            const std::string r = autopilot::Command(in, "");
            events::Push("pad map: travel -> %s", r.substr(0, 80).c_str());
            g_lastAction = "travel: " + r.substr(0, 60);
            if (r.find("refused") == std::string::npos)
                fl::TapKey(VK_ESCAPE); // the map closes as the player's Escape closes it; the ship flies
        }
    }
    // the triggers: zoom (repeat while held); the bumpers: the ship's system / the universe
    const u32 trig[2] = {PAD_LT, PAD_RT};
    for (int i = 0; i < 2; ++i)
    {
        const bool down = pad.Down(trig[i]);
        g_trigRepeat[i] -= dt;
        if (down && (!g_trigWas[i] || g_trigRepeat[i] <= 0))
        {
            ClickName(i == 0 ? "NavMapOutZoom" : "NavMapInZoom", i == 0 ? "zoom out" : "zoom in");
            g_trigRepeat[i] = g_trigWas[i] ? 0.25f : 0.45f;
        }
        g_trigWas[i] = down;
    }
    if (pad.Pressed(XINPUT_GAMEPAD_LEFT_SHOULDER))
        ClickName("NavMapShipButton", "system");
    if (pad.Pressed(XINPUT_GAMEPAD_RIGHT_SHOULDER))
        ClickName("NavMapUniverseButton", "universe");
    // B: the map's own close button (else the game's Escape through the free cursor's B)
    if (Vis("NavMapCloseButton"))
    {
        g_owned |= XINPUT_GAMEPAD_B;
        if (pad.Pressed(XINPUT_GAMEPAD_B))
        {
            if (g_list)
                g_list = false;
            else
                ClickName("NavMapCloseButton", "close");
        }
    }
    Prompts(inSpace);
    return true;
}

u32 Owned() { return g_open ? g_owned : 0; }
bool IsOpen() { return g_open; }

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
    // the list beside the map, at the screen's left: a title, up to 18 rows around the selection
    const float rowH = f.label.lineHeight * 1.25f, x0 = H * 0.03f, w = H * 0.34f;
    const int maxRows = 18;
    const int n = static_cast<int>(g_dests.size());
    const int first = std::clamp(g_sel - maxRows / 2, 0, std::max(0, n - maxRows));
    const int shown = std::min(maxRows, n - first);
    const float top = H * 0.5f - (shown * rowH + f.title.lineHeight * 1.4f) * 0.5f;
    uidraw::HudPanel(x0 - H * 0.012f, top - H * 0.012f, w + H * 0.024f, shown * rowH + f.title.lineHeight * 1.4f + H * 0.024f,
                     0.92f, H, GetTickCount64() / 1000.0);
    render::SetFont(f.title);
    render::Text(x0, top, L"DESTINATIONS", uidraw::kText);
    render::SetFont(f.label);
    float y = top + f.title.lineHeight * 1.4f;
    if (!n)
        render::Text(x0, y, L"NOTHING HERE (NOT IN SPACE?)", Argb(kDim, 0.95f));
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
        render::Text(x0 + w - dw - H * 0.012f - kw, y + (f.label.lineHeight - f.body.lineHeight) * 0.5f, d.kind,
                     Argb(kDim, 0.8f));
        render::SetFont(f.label);
        ++g_drawn.rows;
    }
    render::End(false);
}

std::string Status()
{
    std::ostringstream o;
    o << "map open=" << g_open << " list=" << g_list << " dests=" << g_dests.size() << " sel=" << g_sel
      << " owned=0x" << std::hex << g_owned << std::dec << " drawnRows=" << g_drawn.rows << " last='" << g_lastAction
      << "' waypoints=" << space::Waypoints().size();
    return o.str();
}

std::string Command(std::istream& in)
{
    std::string what;
    in >> what;
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
