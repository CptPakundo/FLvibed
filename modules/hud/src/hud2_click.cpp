// HUD 2's keyboard-and-mouse clicks, the game's side (modules\hud\docs\UI_HUD2.md 5.4): the parts record where their
// clickable things are while they draw (hud2_click_draw.cpp and the parts, through the frame's host: HostHotStart,
// HostHotRect), Update hands the set to the controller host (HostApi v22 SetHudHotspots: invisible native icon buttons
// in a host-created top-level box, so the game's own hit test, hover, click sound and click-vs-fire arbitration apply),
// and a click comes back as OnUiButton(kUiBtnHud2 + slot) -> OnHotClick, run in the next Update through the game's own
// commands (fl::FireCommand: what the pad presets send) or targets (space::SetTarget) - never cursor or key emulation.
// Also here: the contact list mode's rows from the game's contacts (FillList). The set is placed by the host only when
// it changes (memcmp there).
#include "hud2_click.h"
#include "hud2_layout.h"

#include "game/fl.h"
#include "game/aim.h"
#include "ui/views.h"
#include "game/events.h"
#include "flexe.h"
#include "game/hudinfo.h"
#include "hudhost.h"
#include "npcicons.h"
#include "log.h"
#include "text.h"
#include "ui/uiopt.h"

#include <algorithm>
#include <sstream>

// (ASCII only: the build reads the sources as cp1252, non-ASCII text goes in as \u escapes)
namespace hud2
{
using namespace hud2kit;

static HudHotspot g_set[kUiBtnHud2Count] = {};
static bool g_recording = false;
static bool g_pushed = false;  // the host has a set from us (hidden once when we stop)
static int g_hover = -1;
static int g_served = -1;      // what the host answered last (-1 not asked, 0 unavailable)
static int g_queue[8];
static int g_nQueue = 0;
static int g_lastWheel = 0;
static bool g_haveWheel = false;
static unsigned g_clicks = 0;
static const char* const kSlotNames[kHotCount] = {
    "free flight", "goto",   "dock",    "formation", "autopilot", "neuronet", "list header", "filter important",
    "filter ships", "filter stations", "filter loot", "filter all", "row 1", "row 2", "row 3", "row 4", "row 5",
    "row 6", "row 7", "row 8", "row 9", "row 10", "nanobots", "batteries", "countermeasures", "mines", "group 1",
    "group 2", "group 3", "clear target", "next enemy"};

void HostHotStart(bool on)
{
    for (HudHotspot& s : g_set)
        s.shown = 0;
    g_recording = on;
}
void HostHotRect(int slot, float x0, float y0, float x1, float y1)
{
    if (!g_recording || slot < 0 || slot >= kHotCount)
        return;
    g_set[slot] = {std::floor(x0), std::floor(y0), std::ceil(x1), std::ceil(y1), 1}; // whole pixels: stable compares
}
int HostHotHovered() { return g_hover; }

static void Tap(int cmd)
{
    const u32 flags = fl::CommandFlags(cmd);
    if (flags & fl::CMDF_KEYDOWN)
        fl::FireCommand(cmd, true);
    if (flags & fl::CMDF_KEYUP)
        fl::FireCommand(cmd, false);
}

void OnHotClick(int slot)
{
    if (slot < 0 || slot >= kHotCount)
        return;
    if (g_nQueue < 8)
        g_queue[g_nQueue++] = slot;
}

void OnHotspot(int slot) { OnHotClick(slot); }

// One click, through the game's own commands / targets. Returns true when the model should refresh at once.
static bool RunClick(Model& m, int slot)
{
    ++g_clicks;
    events::Push("hud2 click %s", kSlotNames[slot]);
    if (slot >= kHotMan && slot < kHotMan + 5)
    {
        static const int cmds[4] = {fl::USER_MANEUVER_FREE_FLIGHT, fl::USER_MANEUVER_GOTO, fl::USER_MANEUVER_DOCK,
                                    fl::USER_MANEUVER_FORMATION};
        if (slot - kHotMan < 4)
            Tap(cmds[slot - kHotMan]);
        else
            views::BarClick(); // the bar button's own toggle (autopilot start / stop, its events)
        return true;
    }
    if (slot == kHotNeuro)
        return Tap(fl::USER_NAV_MAP), false;
    if (slot == kHotListHead)
    {
        m.list = !m.list;
        m.listFirst = 0;
        return true;
    }
    if (slot >= kHotFilter && slot < kHotFilter + 5)
    {
        m.list = true;
        m.listFilter = slot - kHotFilter;
        m.listFirst = 0;
        return true;
    }
    if (slot >= kHotRow && slot < kHotRow + 10)
    {
        const int i = slot - kHotRow;
        if (i < m.nThreats && m.threats[i].id)
            space::SetTarget(m.threats[i].id);
        return true;
    }
    if (slot >= kHotSlot && slot < kHotSlot + 4)
    {
        static const int cmds[4] = {fl::USER_REPAIR_HEALTH, fl::USER_REPAIR_SHIELD, fl::USER_LAUNCH_COUNTERMEASURES,
                                    fl::USER_LAUNCH_MINES};
        Tap(cmds[slot - kHotSlot]);
        return true;
    }
    if (slot >= kHotGroup && slot < kHotGroup + 3)
    {
        const int row = slot - kHotGroup; // a row is the game's group groupNum[row] (only groups with guns are rows)
        if (row < m.nGroups && m.groupNum[row] >= 1 && m.groupNum[row] <= 6)
            Tap(fl::USER_WEAPON_GROUP1 + (m.groupNum[row] - 1));
        return true;
    }
    if (slot == kHotClear)
        return Tap(fl::USER_CLEAR_TARGET), true;
    if (slot == kHotNextEnemy)
        return Tap(fl::USER_NEXT_ENEMY), true;
    return false;
}

static bool g_refresh = false;
bool HotRefreshWanted()
{
    const bool r = g_refresh;
    g_refresh = false;
    return r;
}

void HotGiveBack()
{
    g_nQueue = 0; // (a click queued in HUD 2's last frame is dropped, never run on the classic HUD)
    if (g_pushed && g_host->SetHudHotspots)
    {
        static const HudHotspot none[1] = {};
        g_host->SetHudHotspots(none, 0);
    }
    g_pushed = false, g_hover = -1, g_haveWheel = false;
}

void HotUpdate(Model& m, bool on)
{
    // the queued clicks first (they came from the game's mouse dispatch since the last frame)
    for (int i = 0; i < g_nQueue; ++i)
        g_refresh |= RunClick(m, g_queue[i]);
    g_nQueue = 0;
    if (!g_host->SetHudHotspots)
    {
        g_hover = -1;
        return;
    }
    if (on)
    {
        g_served = g_host->SetHudHotspots(g_set, kHotCount);
        g_pushed = true;
        const int h = g_host->HudHotspotHovered ? g_host->HudHotspotHovered() : 0;
        g_hover = h >= kUiBtnHud2 && h < kUiBtnHud2 + kHotCount ? h - kUiBtnHud2 : -1;
    }
    else
    {
        if (g_pushed) // hidden at once (the host would also hide a set not renewed)
        {
            static const HudHotspot none[1] = {};
            g_host->SetHudHotspots(none, 0);
            g_pushed = false;
        }
        g_hover = -1;
    }
    // the wheel over the contact list scrolls it (the game's own wheel position: a frame's turn = its change)
    if (exe::MouseWheel.ok())
    {
        const int w = Mem<int>(exe::MouseWheel);
        const int turn = g_haveWheel ? w - g_lastWheel : 0;
        g_lastWheel = w, g_haveWheel = true;
        if (turn && on && m.list && g_hover >= kHotListHead && g_hover < kHotRow + 10)
        {
            const int first = std::max(0, std::min(m.listFirst - turn * 3, std::max(0, m.listTotal - m.listMax)));
            if (first != m.listFirst)
                m.listFirst = first, g_refresh = true;
        }
    }
}

// ---------------------------------------------------------------- the contact list mode (10 Hz)
static flicons::Icon IconOf(const space::Contact& c, bool)
{
    return c.ship ? npcicons::ClassIcon(hudinfo::ClassName(c.id)) // (hostility is the colour: GitHub 85)
           : (c.type & space::kPlanet)                          ? flicons::Planet
           : (c.type & space::kTradelaneRing)                   ? flicons::Lane
           : (c.type & (space::kJumpGate | space::kJumpHole))   ? flicons::JumpGate
           : (c.type & space::kLoot)                            ? flicons::Loot
           : (c.type & (space::kStation | space::kDockingRing)) ? flicons::Station
                                                                : flicons::Waypoint;
}

static std::wstring DistText(float d)
{
    wchar_t b[32];
    if (d >= 1000)
        swprintf_s(b, L"%.1f km", d / 1000);
    else
        swprintf_s(b, L"%.0f m", d);
    return b;
}

bool FillList(Model& m, const std::vector<space::Contact>& all, u32 targetId)
{
    if (!m.list)
        return false;
    static std::vector<std::pair<float, int>> rows; // reused (no allocation once grown)
    rows.clear();
    u32 mission[16];
    const int nMission = m.listFilter == 0 ? HostMissionObjects(mission, 16) : 0;
    for (int i = 0; i < static_cast<int>(all.size()); ++i)
    {
        const space::Contact& c = all[i];
        if (c.id == space::PlayerShipId())
            continue;
        const bool hostile = c.ship && c.attitudeAsked && c.attitude <= aim::kHostileAttitude;
        bool in = false;
        switch (m.listFilter)
        {
        case 0: // important: hostiles, attackers, the target, what the mission points at
            in = hostile || c.targetsPlayer || (targetId && c.id == targetId);
            for (int k = 0; k < nMission && !in; ++k)
                in = mission[k] == c.id;
            break;
        case 1: in = c.ship; break;
        case 2:
            in = !c.ship && (c.type & (space::kStation | space::kDockingRing | space::kPlanet | space::kJumpGate |
                                       space::kJumpHole)) != 0;
            break;
        case 3: in = (c.type & space::kLoot) != 0; break;
        default: in = c.ship || c.targetable || c.dock; break;
        }
        if (in)
            rows.push_back({c.distance, i});
    }
    std::sort(rows.begin(), rows.end());
    m.listTotal = static_cast<int>(rows.size());
    m.listFirst = std::max(0, std::min(m.listFirst, m.listTotal - m.listMax));
    m.filter = kListFilters[std::clamp(m.listFilter, 0, 4)];
    m.nThreats = 0;
    for (int r = m.listFirst; r < m.listTotal && m.nThreats < m.listMax; ++r)
    {
        const space::Contact& c = all[rows[r].second];
        Model::Threat& t = m.threats[m.nThreats++];
        float att = c.attitude;
        bool known = c.attitudeAsked;
        if (!c.ship)
            known = space::AttitudeToPlayer(c.id, att);
        const bool hostile = known && att <= aim::kHostileAttitude;
        t.id = c.id;
        t.name = hudinfo::DisplayName(c.id); // as the game's contact list names it ("Unknown Jump Gate" unvisited)
        if (t.name.empty() && !c.ship)
            t.name = text::Ids(c.nameIds);
        t.dist = DistText(c.distance);
        t.col = !known ? kNeutral : hostile ? kHostile : att >= -aim::kHostileAttitude ? kFriend : kNeutral;
        t.icon = IconOf(c, hostile);
        // a ship: its own outline (its class icon meanwhile) and the pilot's rank (GitHub 84 + 85; an NPC's only)
        t.arch = c.ship && !c.player ? c.arch : 0;
        npcicons::ShownName(t.arch, t.name); // (the test verb's name fixture; nothing without one)
        npcicons::Want(t.arch);
        t.rankLen = 0;
        t.rank = c.ship && !c.player ? npcicons::Rank(t.name, &t.rankLen) : 0;
    }
    return true;
}

// ---------------------------------------------------------------- debug
std::string HotCommand(const std::string& what, std::istream& in)
{
    std::ostringstream o;
    if (what == "click") // hud2 click <slot>: the click's action as if the host had routed it (the logic half)
    {
        int slot = -1;
        in >> slot;
        if (slot < 0 || slot >= kHotCount)
            return "hud2 click <0.." + std::to_string(kHotCount - 1) + ">";
        OnHotClick(slot);
        return std::string("queued: ") + kSlotNames[slot];
    }
    o << "hud2 hot: recording=" << g_recording << " served=" << g_served
      << " pushed=" << g_pushed << " hovered=" << g_hover << " clicks=" << g_clicks << "\n";
    for (int i = 0; i < kHotCount; ++i)
        if (g_set[i].shown)
            o << "  " << i << " " << kSlotNames[i] << " (" << g_set[i].x0 << "," << g_set[i].y0 << ")-(" << g_set[i].x1
              << "," << g_set[i].y1 << ")\n";
    return o.str();
}
} // namespace hud2
