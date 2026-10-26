#include "padhud.h"

#include "../fl.h"
#include "aim.h"
#include "autopilot.h"
#include "autopilot_impl.h" // read-only: the objective run's action and the state (the status line's wait reason)
#include "game/ai.h"
#include "game/flight.h"
#include "game/gamefont.h"
#include "game/render.h"
#include "game/space.h"
#include "glyphs.h"
#include "hud2.h"
#include "host.h"
#include "log.h"
#include "text.h"
#include "ui.h"
#include "uiopt.h"

#include <algorithm>
#include <sstream>

// (ASCII only: the build reads the sources as cp1252, so non-ASCII text goes in as \u escapes)
namespace padhud
{
static bool g_enabled = true; // debug `padhud off`
static bool g_modal = false;
static Context g_ctx;

constexpr wchar_t kDot = L'\u00B7'; // the status line's separator

// ---------------------------------------------------------------- what the layer shows (read in Update)
struct Legend
{
    const wchar_t* title = nullptr;
    struct Row
    {
        presets::Action action;
        glyphs::Id btn;
        bool hold; // the action sits in a hold slot (marked)
        std::wstring label;
        int count = -1; // -1 none
    };
    std::vector<Row> rows;
};
struct Shown
{
    bool combat = false;
    bool targetValid = false, targetDockable = false, targetHostile = false, targetLoot = false;
    float targetDist = 0;
    std::wstring targetName;
    float targetPos[3] = {};
    bool dockInRange = false;
    bool lootInReach = false;
    float lootPos[3] = {};
    bool cruiseCharging = false, cruising = false;
    float chargeLeft = 0;
    int maneuverOp = -1; // the player's running maneuver (space::Pilot op): the bar lights that button
    int missiles = -1, nanobots = -1, batteries = -1, cms = -1, mines = -1;
    std::wstring status; // the status line (empty: none)
    Legend legend;
};
static Shown g_s;

static std::wstring Upper(std::wstring s)
{
    for (wchar_t& c : s)
        c = static_cast<wchar_t>(towupper(c));
    return s;
}

static std::wstring Dist(float m)
{
    wchar_t buf[32];
    if (m >= 1000)
        swprintf_s(buf, L"%.1f KM", m / 1000);
    else
        swprintf_s(buf, L"%.0f M", m);
    return buf;
}

static std::wstring Sep() { return std::wstring(L" ") + kDot + L" "; }

static const wchar_t* ModeVerb(autopilot::Mode m)
{
    switch (m)
    {
    case autopilot::Mode::Goto: return L"FLY TO";
    case autopilot::Mode::Dock: return L"DOCK AT";
    case autopilot::Mode::Follow: return L"FOLLOW";
    case autopilot::Mode::Formation: return L"FORMATION WITH";
    case autopilot::Mode::Face: return L"FACE";
    case autopilot::Mode::Escort: return L"ESCORT";
    case autopilot::Mode::Combat: return L"FIGHT";
    case autopilot::Mode::Travel: return L"TRAVEL TO";
    case autopilot::Mode::Hold: return L"HOLD";
    case autopilot::Mode::Shoot: return L"SHOOT";
    default: return L"";
    }
}

// The wait reason the autopilot exposes through its objective run's sub-action (autopilot_impl.h Action).
static const wchar_t* WaitReason()
{
    if (!autopilot::g_obj.on)
        return nullptr;
    switch (autopilot::g_obj.action)
    {
    case autopilot::Action::DockWait: return L"WAITING FOR CLEARANCE";
    case autopilot::Action::LaneWait: return L"WAITING FOR THE LANE";
    case autopilot::Action::LeadStall: return L"WAITING FOR THE LEADER";
    case autopilot::Action::PreStage: return L"WAITING FOR THE NEXT STEP";
    case autopilot::Action::Rest: return L"RESTING";
    case autopilot::Action::SurviveHold:
    case autopilot::Action::SurviveStay: return L"HOLDING";
    case autopilot::Action::Survive: return L"SURVIVING";
    case autopilot::Action::Leash: return L"KEEPING THE LEASH";
    default: return nullptr;
    }
}

// The button an action is shown with: the stick for the hand-back (the spec's "LS HAND BACK"; B does it too), else
// the first binding of the table.
static bool Button(presets::Action a, glyphs::Id& btn, bool& hold)
{
    const presets::Table& t = g_ctx.table;
    if (a == presets::Action::Handback)
        for (glyphs::Id s : {glyphs::LS, glyphs::RS})
            if (t.b[s].tap == a)
                return btn = s, hold = false, true;
    int slot;
    if (!presets::Find(t, a, btn, slot))
        return false;
    hold = slot == 1;
    return true;
}

static void AddRow(Legend& l, presets::Action a, const std::wstring& label, int count = -1)
{
    glyphs::Id btn;
    bool hold;
    if (!Button(a, btn, hold)) // not bound in this preset: not offered
        return;
    for (const Legend::Row& r : l.rows)
        if (r.btn == btn && r.hold == hold)
            return; // one row per button and slot (a preset may bind two of the sheet's actions to one button)
    if (l.rows.size() >= 9)
        return;
    l.rows.push_back({a, btn, hold, label, count});
}

void Update(float, const Context& c)
{
    g_ctx = c;
    if (!c.active)
        return;
    const uiopt::UiOptions& o = uiopt::Get();

    // The target and the loot around (space::Contacts is memoised in the frame)
    g_s = Shown();
    const aim::Target t = aim::Current();
    const u32 tid = fl::TargetId();
    ai::Self self;
    const bool haveSelf = ai::ReadSelf(self);
    g_s.nanobots = haveSelf ? self.nanobots : -1;
    g_s.batteries = haveSelf ? self.batteries : -1;
    g_s.cms = space::PlayerCountermeasures();
    g_s.mines = space::PlayerMines(); // (INT-HOST: the mine droppers' ammo, as the countermeasures)
    g_s.missiles = -1;
    for (const space::Weapon& w : space::PlayerWeapons())
        if (w.homing && w.ammo >= 0)
            g_s.missiles = std::max(0, g_s.missiles) + w.ammo;
    const float dockRange = space::DockRange();
    float nearestLoot = 1e30f;
    bool anyHostileNear = false;
    for (const space::Contact& k : space::Contacts())
    {
        if (k.id == tid && tid)
        {
            g_s.targetValid = true;
            g_s.targetDockable =
                k.dock || (k.type & (space::kTradelaneRing | space::kJumpGate | space::kJumpHole)) != 0;
            g_s.targetLoot = (k.type & space::kLoot) != 0;
            g_s.targetDist = k.distance;
            memcpy(g_s.targetPos, k.pos, sizeof(k.pos));
            g_s.targetName = Upper(k.ship ? space::SpeakerName(k.id) : text::Ids(k.nameIds));
            g_s.dockInRange = g_s.targetDockable && dockRange > 0 && k.distance <= dockRange;
        }
        if (k.ship && k.attitude <= aim::kHostileAttitude && k.attitudeAsked && k.distance < 4000 &&
            (k.targetsPlayer || k.distance < 2000))
            anyHostileNear = true;
        if ((k.type & space::kLoot) && k.distance < nearestLoot && haveSelf && self.tractorRange > 0 &&
            k.distance <= self.tractorRange)
        {
            nearestLoot = k.distance;
            g_s.lootInReach = true;
            memcpy(g_s.lootPos, k.pos, sizeof(k.pos));
        }
    }
    g_s.targetHostile = t.valid && t.hostile;
    g_s.combat = g_s.targetHostile || anyHostileNear;
    flight::Model fm;
    if (flight::ReadModel(fm) && fm.cruiseEngaged)
    {
        g_s.cruiseCharging = fm.cruiseChargeTime > 0 && fm.cruiseChargeElapsed < fm.cruiseChargeTime;
        g_s.cruising = !g_s.cruiseCharging;
        g_s.chargeLeft = std::max(0.0f, fm.cruiseChargeTime - fm.cruiseChargeElapsed);
    }
    space::Pilot pilot;
    if (space::ReadPilot(pilot))
        g_s.maneuverOp = pilot.op;

    // The status line (ap_status_line)
    if (o.apStatusLine)
    {
        std::wstring s;
        if (c.autopilot)
        {
            s = L"AUTOPILOT";
            const autopilot::Mode m = autopilot::Current();
            if (m != autopilot::Mode::Off)
            {
                s += Sep() + ModeVerb(m);
                if (autopilot::g.target && autopilot::g.target == tid && !g_s.targetName.empty())
                    s += L" " + g_s.targetName;
                else if (autopilot::g.target)
                {
                    const std::wstring n = Upper(space::SpeakerName(autopilot::g.target));
                    if (!n.empty())
                        s += L" " + n;
                }
                if (autopilot::g.target && autopilot::g.target == tid && g_s.targetValid)
                    s += Sep() + Dist(g_s.targetDist);
            }
            else if (autopilot::g_obj.on)
                s += Sep() + L"MISSION";
            if (const wchar_t* why = WaitReason())
                s += Sep() + why;
        }
        else if (c.turret)
            s = L"TURRET VIEW";
        else if (g_s.cruiseCharging)
        {
            s = L"CRUISE" + Sep() + L"CHARGING";
            if (g_s.targetValid && !g_s.targetName.empty())
                s += Sep() + g_s.targetName + Sep() + Dist(g_s.targetDist);
        }
        g_s.status = s;
    }

    // The legend (button_legend): the state's items as the spec lists them, each resolved to its button in the
    // active preset (an action the preset does not bind is left out)
    if (o.buttonLegend == 0 && c.flying)
    {
        Legend& l = g_s.legend;
        if (c.autopilot)
        {
            l.title = L"AUTOPILOT";
            AddRow(l, presets::Action::Handback, L"HAND BACK");
            AddRow(l, presets::Action::GoalPanel, L"GOALS");
            AddRow(l, presets::Action::Autopilot, L"AUTOPILOT OFF");
            AddRow(l, presets::Action::Fire, L"FIRE");
        }
        else if (c.turret)
        {
            l.title = L"TURRET VIEW";
            AddRow(l, presets::Action::Aim, L"AIM");
            AddRow(l, presets::Action::Steer, L"AIM");
            AddRow(l, presets::Action::Fire, L"FIRE");
            AddRow(l, presets::Action::Throttle, L"THROTTLE / STRAFE");
            AddRow(l, presets::Action::Turret, L"EXIT");
        }
        else if (g_s.combat)
        {
            l.title = L"COMBAT";
            if (o.weaponGroups == 2)
            {
                AddRow(l, presets::Action::FireGroup1, L"FIRE GROUP 1");
                AddRow(l, presets::Action::FireGroup2, L"FIRE GROUP 2");
            }
            else
            {
                wchar_t fire[32];
                if (o.weaponGroups == 1)
                    swprintf_s(fire, L"FIRE GROUP %d", c.weaponGroup);
                else
                    wcscpy_s(fire, L"FIRE");
                AddRow(l, presets::Action::Fire, fire);
            }
            AddRow(l, presets::Action::Afterburner, L"AFTERBURNER");
            AddRow(l, presets::Action::Missile, L"MISSILE", g_s.missiles);
            AddRow(l, presets::Action::TargetSmart, L"NEXT ENEMY");
            AddRow(l, presets::Action::NextEnemy, L"NEXT ENEMY");
            AddRow(l, presets::Action::TargetNearest, L"NEAREST ENEMY");
            AddRow(l, presets::Action::Nanobot, L"NANOBOT", g_s.nanobots);
            AddRow(l, presets::Action::Battery, L"BATTERY", g_s.batteries);
            AddRow(l, presets::Action::Cm, L"COUNTERMEASURE", g_s.cms);
            AddRow(l, presets::Action::Mine, L"MINE", g_s.mines);
            AddRow(l, presets::Action::Reverse, L"REVERSE");
        }
        else if (g_s.cruising)
        {
            AddRow(l, presets::Action::FreeFlight, L"DROP CRUISE");
            if (g_s.targetDockable)
                AddRow(l, presets::Action::Context, L"DOCK AT " + g_s.targetName);
            AddRow(l, presets::Action::TargetPicker, L"TARGETS");
        }
        else if (g_s.targetValid)
        {
            if (g_s.targetDockable)
                AddRow(l, presets::Action::Context, L"DOCK AT " + g_s.targetName);
            else if (g_s.targetLoot)
                AddRow(l, presets::Action::Context, L"TRACTOR " + g_s.targetName);
            else
                AddRow(l, presets::Action::Context, L"GO TO " + g_s.targetName);
            AddRow(l, presets::Action::Cruise, L"CRUISE");
            AddRow(l, presets::Action::TargetPicker, L"TARGETS");
            AddRow(l, presets::Action::TargetSmart, L"TARGET AHEAD");
            AddRow(l, presets::Action::Hub, L"MAP");
        }
        else
        {
            AddRow(l, presets::Action::TargetSmart, L"TARGET AHEAD / NEAREST");
            AddRow(l, presets::Action::TargetNearest, L"NEAREST ENEMY");
            AddRow(l, presets::Action::TargetPicker, L"TARGETS");
            AddRow(l, presets::Action::Cruise, L"CRUISE");
            AddRow(l, presets::Action::Hub, L"MAP");
        }
        // the autopilot's own prompts only for players who use it (ap_hints / used once)
        if (!c.autopilot && c.apKnown && !c.turret)
            AddRow(l, presets::Action::Autopilot, L"AUTOPILOT");
    }
}

// ---------------------------------------------------------------- fonts (the game's own: core\game\gamefont)
static render::Font g_font, g_small;
static int g_fontPx = 0, g_smallPx = 0;
static void* g_fontDev = nullptr;

static const std::wstring& Chars()
{
    static std::wstring c;
    if (c.empty())
    {
        for (wchar_t ch = 32; ch < 127; ++ch)
            c += ch;
        c += L"\u00B7\u2212\u00C9\u00E9\u00DC\u00FC\u00D6\u00F6\u00C4\u00E4";
    }
    return c;
}

static bool Fonts(int px, int smallPx, void* dev)
{
    if (g_font.texture && g_fontPx == px && g_smallPx == smallPx && g_fontDev == dev)
        return true;
    // Agency FB (the HUD's labels) when the game's fonts load, else Segoe UI, a little smaller to read alike
    const bool game = gamefont::Load();
    const float k = game ? 1.0f : 0.85f;
    if (!render::BuildFont(g_font, gamefont::LabelFace(), static_cast<int>(px * k), 700, Chars()) ||
        !render::BuildFont(g_small, gamefont::LabelFace(), static_cast<int>(smallPx * k), game ? 700 : 600, Chars()))
        return false;
    g_fontPx = px, g_smallPx = smallPx, g_fontDev = dev;
    return true;
}

// ---------------------------------------------------------------- drawing
constexpr u32 kHud = 0x82C9F3;  // the HUD's chrome (130, 201, 243)
constexpr u32 kDim = 0x6E8EA8;  // secondary text
constexpr u32 kBack = 0x0D0B1A; // the panel base (13, 11, 26)
static u32 Argb(u32 rgb, float a) { return (static_cast<u32>(Clampf(a, 0, 1) * 255 + 0.5f) << 24) | (rgb & 0xFFFFFF); }

struct Badge
{
    glyphs::Id btn;
    bool hold;
    bool lit;       // the control it badges is lit: the glyph's chrome goes the game's active yellow
    float x, y, h;  // centre, height
};

// The HUD's controls by name (ui::Collect: visible, enabled, on screen), refreshed every 100 ms.
static std::vector<ui::Item> g_items;
static DWORD g_itemsAt = 0;

static const ui::Item* Item(const char* name, int nth = 0)
{
    int n = 0;
    for (const ui::Item& it : g_items)
        if (strcmp(it.name, name) == 0 && n++ == nth)
            return &it;
    return nullptr;
}

// Size and placement (TASK_GLYPHS_HUD.md 5): a badge is scaled to its control - 0.45 of the control's height, at least
// 0.022 H and at most 0.036 H, text_size scaling all of it (the maneuver bar's 156 px buttons at 4K get 70 px glyphs,
// the 78 px header icons 47 px) - and sits where the HUD's own frames put their tabs: centred on the control, half
// across the frame line of the control's panel that faces the open screen (the top panels' bottom line: the maneuver
// bar, the NeuroNet box, the cruise box; the bottom panels' header top line: the consumables, the weapon groups). In a
// header with room beside the control (the contact list's next-enemy icon and its expander) the badge sits beside it
// inside the band. Never on a corner, never a fixed pixel size. The frame lines are measured from the controls' rects
// at 4K (research\experience\runs\ui_glyphs_2026-10-25, probe_4k): the maneuver bar's line 0.0088 H under its buttons'
// bottom edge (buttons hh 78 at y 85, the line at ~182), the NeuroNet box's line the same under its buttons (hh 40 at
// y 52, line ~110), the cruise box's bottom line 2 hh under its centre (y 248, hh 39, line ~326), the bottom panels'
// header line 0.025 H above their controls' centres (y 1559 / 1558, line ~1505). HudShift moves the panels whole, so
// the offsets hold in its layouts; the controls themselves come by name from ui::Collect (visible ones only).
static void Badges(std::vector<Badge>& out, float H, float ts, int sw, int sh)
{
    auto size = [&](float hh) { return Clampf(hh * 2 * 0.45f, H * 0.022f, H * 0.036f) * ts; };
    auto push = [&](glyphs::Id btn, bool hold, bool lit, float x, float y, float h)
    {
        x = Clampf(x, h * 0.5f, sw - h * 0.5f);
        y = Clampf(y, h * 0.5f, sh - h * 0.5f);
        out.push_back({btn, hold, lit, x, y, h});
    };
    enum Anchor
    {
        TopPanel,    // half across the panel's bottom line under the control
        UnderBox,    // half across the cruise box's bottom line
        BottomPanel, // half across the header's top line over the control
        BesideLeft,  // inside the header band, left of the control
        BesideRight, // inside the header band, right of the control
    };
    auto on = [&](const char* name, presets::Action a, Anchor an, bool lit = false, int nth = 0)
    {
        glyphs::Id btn;
        bool hold;
        const ui::Item* it = Item(name, nth);
        if (!it || !Button(a, btn, hold))
            return;
        const float h = size(it->hh);
        switch (an)
        {
        case TopPanel: push(btn, hold, lit, it->x, it->y + it->hh + H * 0.0088f, h); break;
        case UnderBox: push(btn, hold, lit, it->x, it->y + it->hh * 2, h); break;
        case BottomPanel: push(btn, hold, lit, it->x, it->y - H * 0.025f, h); break;
        case BesideLeft: push(btn, hold, lit, it->x - it->hw - h * 0.7f, it->y, h); break;
        case BesideRight: push(btn, hold, lit, it->x + it->hw + h * 0.7f, it->y, h); break;
        }
    };
    // the maneuver bar: ManeuverButton0..3 are, left to right, free flight, goto, dock, formation (keymap.ini
    // IDR_SPACE order; the A badge sits on whichever of goto / dock applies to the target); lit = the running maneuver
    // (space::Pilot op: FreeFlight 19, Goto 2, Dock 7 / InstantTradeLane 9, Formation 10)
    const int op = g_s.maneuverOp;
    on("ManeuverButton0", presets::Action::FreeFlight, TopPanel, op == 19);
    if (g_s.targetValid && !g_s.targetDockable && !g_s.targetLoot)
        on("ManeuverButton1", presets::Action::Context, TopPanel, op == 2);
    else if (g_s.targetValid && g_s.targetDockable)
        on("ManeuverButton2", presets::Action::Context, TopPanel, op == 7 || op == 9);
    on("ManeuverButton3", presets::Action::Formation, TopPanel, op == 10);
    on("ManeuverCruiseButton", presets::Action::Cruise, UnderBox, g_s.cruising || g_s.cruiseCharging);
    on("NNNavmapButtonRoot", presets::Action::Hub, TopPanel);
    // the contact list's header: LB left of the expander (collapsed and expanded alike), R3 right of the next-enemy icon
    on("SwitchToContactList", presets::Action::TargetPicker, BesideLeft);
    on("NextEnemyButton", presets::Action::TargetSmart, BesideRight);
    on("NextEnemyButton", presets::Action::NextEnemy, BesideRight);
    // the status panel's header: the D-pad over each consumable, Y (hold) over the active weapon group (lit)
    on("HudRepairButton", presets::Action::Nanobot, BottomPanel);
    on("UseBatteryButton", presets::Action::Battery, BottomPanel);
    if (uiopt::Get().weaponGroups == 1)
        on("HudWeaponGroupButton", presets::Action::WeaponGroup, BottomPanel, true, std::clamp(g_ctx.weaponGroup - 1, 0, 2));
    // the FLvibed bar button (the HOST track's native button): View held, lit while the autopilot flies
    float cx, cy, hw, hh;
    if (HostAtLeast(18) && g_host->UiButtonRect && g_host->UiButtonRect(kUiBtnBar, &cx, &cy, &hw, &hh))
    {
        glyphs::Id btn;
        bool hold;
        if (Button(presets::Action::Autopilot, btn, hold) && (g_ctx.apKnown || g_ctx.autopilot))
            push(btn, hold, g_ctx.autopilot, cx, cy + hh + H * 0.0088f, size(hh));
    }
}

// The hold mark: a thin bar in the glyph's chrome under it (the legend spells it out as HOLD beside the row).
static void HoldMark(float cx, float cy, float h, float alpha, u32 rgb = glyphs::kChrome)
{
    const float t = std::max(1.5f, h * 0.06f);
    render::Box(cx - h * 0.28f, cy + h * 0.5f + t * 0.6f, h * 0.56f, t, Argb(rgb, alpha));
}

static struct
{
    int badges = 0, legendRows = 0, prompts = 0;
    bool status = false;
    float h = 0;
} g_last;

void Render(void* device)
{
    g_last = decltype(g_last)();
    if (!g_enabled || !g_ctx.active || !device)
        return;
    if (hud2::Active()) // HUD 2 replaces the game's HUD: it has its own prompts, chips and lines (CLASSIC unchanged)
        return;
    const uiopt::UiOptions& o = uiopt::Get();
    render::SetDevice(device);
    glyphs::SetDevice(device);
    int sw = 0, sh = 0;
    if (!render::ScreenSize(sw, sh) || sh < 100)
        return;
    const float H = static_cast<float>(sh), W = static_cast<float>(sw);
    const float ts = o.textSize / 100.0f;
    // the HUD's own label size (fonts.ini HudSmall 0.029 H) for the legend, the status line and the world prompts
    const int px = std::max(10, static_cast<int>(H * 0.029f * ts + 0.5f));
    const int smallPx = std::max(8, static_cast<int>(H * 0.021f * ts + 0.5f));
    if (!Fonts(px, smallPx, device))
        return;
    const DWORD now = GetTickCount();
    if (now - g_itemsAt > 100)
    {
        g_items = ui::Collect();
        g_itemsAt = now;
    }
    if (!render::Begin(g_font))
        return;
    g_last.h = H;

    // (a) the badges
    if (o.hudGlyphs)
    {
        std::vector<Badge> badges;
        Badges(badges, H, ts, sw, sh);
        for (const Badge& b : badges)
        {
            const u32 rgb = b.lit ? glyphs::kLit : glyphs::kChrome;
            glyphs::Draw(b.btn, b.x, b.y, b.h, 0.95f, rgb);
            if (b.hold)
                HoldMark(b.x, b.y, b.h, 0.95f, rgb);
        }
        g_last.badges = static_cast<int>(badges.size());
    }

    // (c) the status line under the maneuver bar, right of the cruise box that hangs under the bar's left end
    if (!g_s.status.empty() && g_ctx.flying)
    {
        float barBottom = 0, barLeft = 1e9f, barRight = 0;
        int n = 0;
        for (int i = 0; i < 4; ++i)
        {
            char name[20];
            sprintf_s(name, "ManeuverButton%d", i);
            if (const ui::Item* it = Item(name))
            {
                barBottom = std::max(barBottom, it->y + it->hh);
                barLeft = std::min(barLeft, it->x - it->hw);
                barRight = std::max(barRight, it->x + it->hw);
                ++n;
            }
        }
        if (n)
        {
            float x0 = barLeft;
            if (const ui::Item* cruise = Item("ManeuverCruiseButton"))
                if (cruise->y > barBottom - cruise->hh) // it hangs under the bar: start right of it
                    x0 = std::max(x0, cruise->x + cruise->hw + H * 0.012f);
            const float tw = static_cast<float>(render::TextWidth(g_font, g_s.status));
            const float y = barBottom + H * 0.01f, x = Clampf(x0, H * 0.01f, W - tw - H * 0.01f);
            render::Gradient(x - H * 0.01f, y - H * 0.003f, tw + H * 0.02f, g_font.lineHeight + H * 0.006f,
                             Argb(kBack, 0.0f), Argb(kBack, 0.55f), Argb(kBack, 0.55f), Argb(kBack, 0.0f));
            render::Text(x, y, g_s.status, Argb(kHud, 0.95f));
            g_last.status = true;
        }
    }

    // (b) the legend in the empty right-middle (steps aside while a modal has its own prompt bar)
    if (o.buttonLegend == 0 && g_ctx.flying && !g_modal && !g_s.legend.rows.empty())
    {
        // the glyphs at the HUD font's height (0.029 H, the labels' em), the rows 1.3 of it, the labels in the HUD font
        const Legend& l = g_s.legend;
        const float gh = H * 0.029f * ts, rowH = gh * 1.3f, gap = gh * 0.35f;
        const float right = W - W * 0.02f;
        float maxW = 0;
        for (const Legend::Row& r : l.rows)
        {
            float w = gh + gap + static_cast<float>(render::TextWidth(g_font, r.label));
            if (r.count >= 0)
                w += static_cast<float>(render::TextWidth(g_font, L" 00"));
            if (r.hold)
                w += static_cast<float>(render::TextWidth(g_small, L" HOLD")) + gap;
            maxW = std::max(maxW, w);
        }
        const float total = rowH * l.rows.size() + (l.title ? g_small.lineHeight * 1.3f : 0.0f);
        float y = H * 0.5f - total * 0.5f;
        // a long legend (combat: nine rows) keeps clear of the bottom panels' header line (0.025 H above its icons)
        float limit = H;
        for (const char* name : {"HudRepairButton", "NextEnemyButton", "SwitchToContactList"})
            if (const ui::Item* it = Item(name))
                limit = std::min(limit, it->y - H * 0.025f - H * 0.012f);
        if (y + total + H * 0.008f > limit)
            y = std::max(H * 0.06f, limit - total - H * 0.008f);
        const float x0 = right - maxW;
        render::Gradient(x0 - H * 0.015f, y - H * 0.008f, maxW + H * 0.03f + W * 0.02f, total + H * 0.016f,
                         Argb(kBack, 0.0f), Argb(kBack, 0.5f), Argb(kBack, 0.5f), Argb(kBack, 0.0f));
        // a thin edge line in the HUD's frame colour along the left, subtle (the panels' line (118, 151, 180))
        render::Box(x0 - H * 0.012f, y - H * 0.004f, std::max(1.0f, H * 0.001f), total + H * 0.008f,
                    Argb(0x7697B4, 0.45f));
        if (l.title)
        {
            render::SetFont(g_small);
            render::Text(x0, y, l.title, Argb(kDim, 0.95f));
            render::SetFont(g_font);
            y += g_small.lineHeight * 1.3f;
        }
        for (const Legend::Row& r : l.rows)
        {
            const float cy = y + rowH * 0.5f;
            glyphs::Draw(r.btn, x0 + gh * 0.5f, cy, gh, 0.95f);
            if (r.hold)
                HoldMark(x0 + gh * 0.5f, cy, gh, 0.9f);
            float x = x0 + gh + gap;
            render::Text(x, cy - g_font.lineHeight * 0.5f, r.label, Argb(kHud, 0.95f));
            x += static_cast<float>(render::TextWidth(g_font, r.label));
            if (r.count >= 0)
            {
                wchar_t buf[16];
                swprintf_s(buf, L" %d", r.count);
                render::Text(x, cy - g_font.lineHeight * 0.5f, buf, Argb(0xF0F4F8, 0.95f));
                x += static_cast<float>(render::TextWidth(g_font, buf));
            }
            if (r.hold)
            {
                render::SetFont(g_small);
                render::Text(x + gap, cy - g_small.lineHeight * 0.5f, L"HOLD", Argb(kDim, 0.95f));
                render::SetFont(g_font);
            }
            y += rowH;
        }
        g_last.legendRows = static_cast<int>(l.rows.size());
    }

    // (d) the world prompts: by the target when a dock is possible now, by loot in the tractor's reach
    if (o.worldPrompts == 0 && g_ctx.flying && !g_modal && !g_ctx.autopilot)
    {
        auto prompt = [&](const float world[3], presets::Action a, const wchar_t* label)
        {
            glyphs::Id btn;
            bool hold;
            float s[3];
            if (!Button(a, btn, hold) || !fl::WorldToScreen(world, s))
                return;
            if (s[0] < 0 || s[0] > W || s[1] < 0 || s[1] > H)
                return;
            std::wstring text = L"{";
            for (const char* p = glyphs::Name(btn); *p; ++p)
                text += static_cast<wchar_t>(*p);
            text += L"} ";
            text += label;
            const float tw = glyphs::Text(g_font, 0, 0, text, 0, true);
            const float x = Clampf(s[0] - tw * 0.5f, 0.0f, W - tw), y = std::min(s[1] + H * 0.07f, H * 0.8f);
            render::Gradient(x - H * 0.008f, y - H * 0.003f, tw + H * 0.016f, g_font.lineHeight + H * 0.006f,
                             Argb(kBack, 0.45f), Argb(kBack, 0.45f), Argb(kBack, 0.45f), Argb(kBack, 0.45f));
            glyphs::Text(g_font, x, y, text, Argb(kHud, 0.95f));
            ++g_last.prompts;
        };
        if (g_s.dockInRange && !g_s.cruising && !g_s.cruiseCharging)
            prompt(g_s.targetPos, presets::Action::Context, L"DOCK");
        if (g_s.lootInReach)
            prompt(g_s.lootPos, g_s.targetLoot ? presets::Action::Context : presets::Action::Tractor, L"LOOT");
    }
    render::End(false);
}

void SetModal(bool on) { g_modal = on; }
bool Modal() { return g_modal; }

std::string Command(std::istream& in)
{
    std::string what;
    in >> what;
    if (what == "on" || what == "off")
    {
        g_enabled = what == "on";
        return "padhud " + what;
    }
    std::ostringstream o;
    o << "padhud enabled=" << g_enabled << " active=" << g_ctx.active << " flying=" << g_ctx.flying
      << " modal=" << g_modal << " preset=" << presets::PresetName(g_ctx.preset) << " autopilot=" << g_ctx.autopilot
      << " turret=" << g_ctx.turret << " group=" << g_ctx.weaponGroup << "\n";
    o << "state combat=" << g_s.combat << " target=" << g_s.targetValid << " dockable=" << g_s.targetDockable
      << " dockInRange=" << g_s.dockInRange << " lootInReach=" << g_s.lootInReach << " cruise="
      << (g_s.cruiseCharging ? "charging" : g_s.cruising ? "on" : "off") << " missiles=" << g_s.missiles
      << " nano=" << g_s.nanobots << " batt=" << g_s.batteries << " cm=" << g_s.cms << "\n";
    o << "status='" << text::Utf8(g_s.status) << "'\n";
    o << "legend" << (g_s.legend.title ? std::string(" ") + text::Utf8(g_s.legend.title) : std::string("")) << ":";
    for (const Legend::Row& r : g_s.legend.rows)
    {
        o << " " << glyphs::Name(r.btn) << (r.hold ? "(hold)" : "") << "=" << text::Utf8(r.label);
        if (r.count >= 0)
            o << " " << r.count;
    }
    o << "\n";
    o << "drawn badges=" << g_last.badges << " legendRows=" << g_last.legendRows << " prompts=" << g_last.prompts
      << " status=" << g_last.status << " H=" << g_last.h << " font=" << g_fontPx << "px\n";
    o << glyphs::Status();
    return o.str();
}

void Shutdown()
{
    render::FreeFont(g_font);
    render::FreeFont(g_small);
    glyphs::Shutdown();
}
} // namespace padhud
