// HUD 2's keyboard-and-mouse clicks (docs\UI_HUD2.md 5.4): the parts record where their clickable things are while they
// draw (HotRect), Update hands the set to the controller host (HostApi v22 SetHudHotspots: invisible native icon buttons
// in a host-created top-level box, so the game's own hit test, hover, click sound and click-vs-fire arbitration apply),
// and a click comes back as OnUiButton(kUiBtnHud2 + slot) -> OnHotClick, run in the next Update through the game's own
// commands (fl::FireCommand: what the pad presets send) or targets (space::SetTarget) - never cursor or key emulation.
// Also here: the keyboard form's maneuver row, the threat / contact list (both forms draw the threat list here) and the
// target card's close / next-enemy icons. The set is placed by the host only when it changes (memcmp there).
#include "hud2_impl.h"

#include "../fl.h"
#include "aim.h"
#include "barbutton.h"
#include "../events.h"
#include "flexe.h"
#include "game/hudinfo.h"
#include "game/space.h"
#include "host.h"
#include "log.h"
#include "text.h"
#include "uiopt.h"

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
static const wchar_t* const kFilters[5] = {L"IMPORTANT", L"SHIPS", L"STATIONS", L"LOOT", L"ALL"};
static const char* const kSlotNames[kHotCount] = {
    "free flight", "goto",   "dock",    "formation", "autopilot", "neuronet", "list header", "filter important",
    "filter ships", "filter stations", "filter loot", "filter all", "row 1", "row 2", "row 3", "row 4", "row 5",
    "row 6", "row 7", "row 8", "row 9", "row 10", "nanobots", "batteries", "countermeasures", "mines", "group 1",
    "group 2", "group 3", "clear target", "next enemy"};

void HotStart(bool on)
{
    for (HudHotspot& s : g_set)
        s.shown = 0;
    g_recording = on;
}
void HotRect(int slot, float x0, float y0, float x1, float y1)
{
    if (!g_recording || slot < 0 || slot >= kHotCount)
        return;
    g_set[slot] = {std::floor(x0), std::floor(y0), std::ceil(x1), std::ceil(y1), 1}; // whole pixels: stable compares
}
int HotHovered() { return g_hover; }

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
            barbutton::OnClick(); // the bar button's own toggle (autopilot start / stop, its events)
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

void HotUpdate(Model& m, bool on)
{
    // the queued clicks first (they came from the game's mouse dispatch since the last frame)
    for (int i = 0; i < g_nQueue; ++i)
        g_refresh |= RunClick(m, g_queue[i]);
    g_nQueue = 0;
    if (!HostAtLeast(22) || !g_host->SetHudHotspots)
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
static flicons::Icon IconOf(const space::Contact& c, bool hostile)
{
    return c.ship ? (hostile ? flicons::Enemy : flicons::Fighter)
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
    m.filter = kFilters[std::clamp(m.listFilter, 0, 4)];
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
    }
    return true;
}

// ---------------------------------------------------------------- drawing
static void AutopilotGlyph(float cx, float cy, float s, u32 c) // the bar button's ring with a chevron
{
    const float lw = std::max(1.5f, s * 0.08f);
    Ring(cx, cy, s * 0.36f, lw, c);
    Line(cx - s * 0.16f, cy + s * 0.08f, cx, cy - s * 0.10f, lw, c);
    Line(cx, cy - s * 0.10f, cx + s * 0.16f, cy + s * 0.08f, lw, c);
}

float DrawManeuverRow(const Model& m, const Layout& L, float x, float y, float a, float& right)
{
    SetAlpha(a);
    const float H = L.H, k = L.k, g = 0.034f * H * k, kh = 0.024f * H * k, gap = 0.016f * H * k;
    static const flicons::Icon icons[4] = {flicons::Free, flicons::Goto, flicons::Dock, flicons::Formation};
    float cx = x;
    for (int i = 0; i < 5; ++i)
    {
        const bool lit = i < 4 ? m.manRun == i : m.ap;
        const bool hov = HotHovered() == kHotMan + i;
        const u32 col = lit ? kActive : hov ? kInk : kChrome;
        const float kw = DrawPrompt(m.manBtn[i], 0, 0, kh, 0, true);
        const float w = g + (kw > 0 ? kw + 0.004f * H * k : 0);
        if (hov)
            Plate(cx - 0.004f * H * k, y - g * 0.62f, cx + w + 0.004f * H * k, y + g * 0.62f, 0.004f * H * k,
                  kTL | kTR | kBR | kBL, 0.5f, kInk, 0.6f);
        if (i < 4)
        {
            if (!Icon(icons[i], cx + g / 2, y, g, Argb(col, 1)))
                Disc(cx + g / 2, y, 0.006f * H * k, Argb(col, 1));
        }
        else
            AutopilotGlyph(cx + g / 2, y, g, Argb(col, 1));
        if (kw > 0)
            DrawPrompt(m.manBtn[i], cx + g + 0.004f * H * k + kw / 2, y + g * 0.18f, kh, Argb(kChrome, 0.9f));
        HotRect(kHotMan + i, cx - 0.004f * H * k, y - g * 0.62f, cx + w + 0.004f * H * k, y + g * 0.62f);
        right = cx + w + 0.004f * H * k;
        cx += w + gap;
    }
    return 0.048f * H * k;
}

void DrawCardIcons(const Model& m, const Layout& L, float a)
{
    if (!m.target)
        return;
    SetAlpha(a);
    // a row at the card's foot (the card is taller in the keyboard form), right-aligned: [key] icon NEXT  [key] x CLEAR
    const float H = L.H, k = L.k, s = 0.028f * H * k, kh = 0.026f * H * k, T = 0.024f * H * k;
    const float y = L.tgtY1 - 0.026f * H * k;
    float x = L.tgtX1 - 0.018f * H * k; // right to left: clear, then next enemy
    // the row stays right of the view's column (the round 3 bug: NEXT ENEMY's keycap sat in the view): the long labels
    // when they fit, else the short ones, else the icons and keys alone
    const float minX = L.tgtX0 + 0.13f * H * k + 0.03f * H * k;
    static const wchar_t* kLabels[3][2] = {{L"CLEAR", L"NEXT ENEMY"}, {L"CLEAR", L"NEXT"}, {L"", L""}};
    int form = 0;
    for (; form < 2; ++form)
    {
        float w = 0;
        for (int i = 0; i < 2; ++i)
            w += TextWidth(kLabels[form][i], T, 0.08f) + 0.006f * H * k + s +
                 DrawPrompt(i == 0 ? m.clearBtn : m.nextEnemyBtn, 0, 0, kh, 0, true) + 0.006f * H * k + 0.024f * H * k;
        if (x - w >= minX)
            break;
    }
    for (int i = 0; i < 2; ++i)
    {
        const int slot = i == 0 ? kHotClear : kHotNextEnemy;
        const Prompt& p = i == 0 ? m.clearBtn : m.nextEnemyBtn;
        const wchar_t* label = kLabels[form][i];
        const bool hov = HotHovered() == slot;
        const u32 col = Argb(hov ? kInk : kChrome, 1);
        const float x1 = x, lw0 = TextWidth(label, T, 0.08f);
        Text(x1, y, label, T, col, Right, 0.08f);
        const float cx = x1 - lw0 - 0.006f * H * k - s / 2;
        if (i == 0)
        {
            const float d = s * 0.26f, lw = std::max(1.5f, s * 0.09f);
            Line(cx - d, y - d, cx + d, y + d, lw, col);
            Line(cx - d, y + d, cx + d, y - d, lw, col);
        }
        else if (!Icon(flicons::Next, cx, y, s, col))
            Ring(cx, y, s * 0.3f, 2, col);
        float x0 = cx - s / 2;
        const float kw = DrawPrompt(p, 0, 0, kh, 0, true);
        if (kw > 0)
        {
            DrawPrompt(p, x0 - 0.006f * H * k - kw / 2, y, kh, Argb(kChrome, 0.9f));
            x0 -= kw + 0.006f * H * k;
        }
        if (hov)
            Plate(x0 - 0.004f * H * k, y - 0.018f * H * k, x1 + 0.004f * H * k, y + 0.018f * H * k, 0.004f * H * k,
                  kTL | kTR | kBR | kBL, 0.4f, kInk, 0.6f);
        HotRect(slot, x0 - 0.004f * H * k, y - 0.018f * H * k, x1 + 0.004f * H * k, y + 0.018f * H * k);
        x = x0 - 0.024f * H * k;
    }
}

void DrawList(const Model& m, const Layout& L, float a)
{
    SetAlpha(a);
    const float H = L.H, k = L.k, T = 0.024f * H * k;
    const float x0 = L.tgtX0, x1 = L.tgtX1, right = x1 - 0.018f * H * k;
    float yy = L.thrY0 + 0.034f * H * k;
    const float g = 0.036f * H * k * 0.9f;
    float gx = x0 + g * 0.2f;
    if (m.kbm) // the list toggle: the game's list icon, lit while the contact list mode is on
    {
        const bool hov = HotHovered() == kHotListHead;
        const float s = 0.028f * H * k;
        if (!Icon(flicons::List, gx + s / 2, yy, s, Argb(m.list ? kActive : hov ? kInk : kChrome, 1)))
            Ring(gx + s / 2, yy, s * 0.3f, 2, Argb(m.list ? kActive : kChrome, 1));
        float hx1 = gx + s;
        if (!m.list && m.nThreats == 0) // calm: the header alone, a way into the contact list
            hx1 += 0.008f * H * k + Text(gx + s + 0.008f * H * k, yy, L"CONTACTS", T, Argb(hov ? kInk : kDim, 1), Left, 0.08f);
        HotRect(kHotListHead, gx - 0.004f * H * k, yy - 0.02f * H * k, hx1 + 0.004f * H * k, yy + 0.02f * H * k);
        gx += s + 0.012f * H * k;
    }
    if (m.list)
    {
        // the game's filters as words, the current one lit
        for (int i = 0; i < 5; ++i)
        {
            const bool cur = i == m.listFilter, hov = HotHovered() == kHotFilter + i;
            const float w = Text(gx, yy, kFilters[i], T, Argb(cur ? kActive : hov ? kInk : kDim, 1), Left, 0.02f);
            HotRect(kHotFilter + i, gx - 0.004f * H * k, yy - 0.02f * H * k, gx + w + 0.004f * H * k, yy + 0.02f * H * k);
            gx += w + 0.014f * H * k;
        }
    }
    else if (m.nThreats > 0 || !m.kbm)
    {
        // the threat list in combat: LB TARGETS, R3 NEXT, the filter (the keyboard: its keys)
        const float w1 = DrawPrompt(m.threatsBtn, 0, 0, g, 0, true);
        DrawPrompt(m.threatsBtn, gx + w1 / 2, yy, g, Argb(kChrome, 1));
        gx += w1 + g * 0.3f;
        if (w1 > 0) // (the keyboard has no key for the picker: no TARGETS label without its button)
            gx += Text(gx, yy, L"TARGETS", T, Argb(kChrome, 1), Left, 0.08f) + 0.03f * H * k;
        else if (!m.kbm)
            gx = x0;
        // NEXT: not in the keyboard form while the card shows its NEXT ENEMY icon - the same command and key (the round 4
        // critique saw CTRL+R twice: one binding, USER_NEXT_ENEMY, labelled in two places - not a duplicate binding)
        if (!(m.kbm && m.target))
        {
            const float w2 = DrawPrompt(m.nextBtn, 0, 0, g, 0, true);
            DrawPrompt(m.nextBtn, gx + w2 / 2, yy, g, Argb(kChrome, 1));
            // (fit to the room left of the filter word: at 4:3 "NEXT" ran into "HOSTILES" - the fit rule)
            const float fw = TextWidth(m.filter, T, 0.08f) + 0.012f * H * k;
            TextFit(gx + w2 + g * 0.35f, yy, L"NEXT", T, right - fw - (gx + w2 + g * 0.35f), Argb(kChrome, 1), Left, 0.08f);
        }
        Text(right, yy, m.filter, T, Argb(kDim, 1), Right, 0.08f);
    }
    yy += 0.044f * H * k;
    for (int i = 0; i < m.nThreats && i < 10; ++i)
    {
        const bool hov = m.kbm && HotHovered() == kHotRow + i, tgt = m.threats[i].id && m.threats[i].id == m.targetId;
        Plate(x0, yy - 0.019f * H * k, x1, yy + 0.019f * H * k, 0, 0, hov ? 0.75f : 120 / 255.0f, kChrome, hov ? 0.6f : 0);
        if (!Icon(m.threats[i].icon, x0 + 0.024f * H * k, yy, 0.028f * H * k, Argb(m.threats[i].col, 1)))
            Disc(x0 + 0.024f * H * k, yy, 0.006f * H * k, Argb(m.threats[i].col, 1));
        const float dw = TextWidth(m.threats[i].dist, 0.026f * H * k);
        TextFit(x0 + 0.05f * H * k, yy, m.threats[i].name, 0.026f * H * k, right - dw - 0.02f * H * k - (x0 + 0.05f * H * k),
                Argb(tgt ? kActive : kInk, 1));
        Text(right, yy, m.threats[i].dist, 0.026f * H * k, Argb(kInk, 1), Right);
        HotRect(kHotRow + i, x0, yy - 0.019f * H * k, x1, yy + 0.019f * H * k);
        yy += 0.042f * H * k;
    }
    if (m.list && m.listTotal > m.nThreats) // where the wheel has scrolled to
    {
        wchar_t b[48];
        swprintf_s(b, L"%d-%d OF %d", m.listFirst + 1, m.listFirst + m.nThreats, m.listTotal);
        Text(right, yy - 0.008f * H * k, b, T, Argb(kDim, 1), Right, 0.06f);
    }
    else if (m.list && m.listTotal == 0)
        Text(x0 + 0.05f * H * k, yy, L"NONE", T, Argb(kDim, 1), Left, 0.08f);
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
    o << "hud2 hot: recording=" << g_recording << " host v22=" << HostAtLeast(22) << " served=" << g_served
      << " pushed=" << g_pushed << " hovered=" << g_hover << " clicks=" << g_clicks << "\n";
    for (int i = 0; i < kHotCount; ++i)
        if (g_set[i].shown)
            o << "  " << i << " " << kSlotNames[i] << " (" << g_set[i].x0 << "," << g_set[i].y0 << ")-(" << g_set[i].x1
              << "," << g_set[i].y1 << ")\n";
    return o.str();
}
} // namespace hud2
