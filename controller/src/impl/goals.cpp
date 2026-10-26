#include "goals.h"

#include "../events.h"
#include "autopilot_impl.h" // the autopilot's own state and Start: the panel is its pad UI (AUTOPILOT_UX.md 5)
#include "controller.h"     // ctl::MarkAutopilotUsed
#include "device.h"         // the form: the pad's glyphs and buttons, or the keyboard + mouse's keys and clicks
#include "flightmodal.h"
#include "host.h"
#include "log.h"
#include "options_page.h" // LocalBus: the bus over this DLL's own sections when the host has none
#include "settings_bus.h"
#include "hud2.h" // BottomPanelsTop: HUD 2's bottom panels while it shows
#include "ui.h" // Collect: the HUD's bottom-right panel by its controls' names
#include "ui_meta.h"

#include <algorithm>
#include <sstream>

// (ASCII only: the build reads the sources as cp1252, so non-ASCII text goes in as \u escapes)
namespace goals
{
using namespace flightmodal;

enum Tab
{
    Actions,
    Queue,
    Settings,
    TabCount
};
static const wchar_t* kTabNames[TabCount] = {L"ACTIONS", L"QUEUE", L"SETTINGS"};

struct ActionRow
{
    enum Kind
    {
        StartMode,  // autopilot::Start(mode, target)
        TravelTo,   // Travel with the target as the destination (the fastest way: lanes, gates, dock)
        TravelList, // Travel along the waypoint list as it is
        Objective,  // the objective mode (ap start objective)
    } kind = StartMode;
    autopilot::Mode mode = autopilot::Mode::Off;
    u32 target = 0;
    std::wstring label, detail;
    bool enabled = true;
};

// The player rows of the SETTINGS tab (UI_SPEC.json AUTOPILOT tab; the same registry rows through the bus).
static const char* kSettingKeys[] = {"engage", "auto_fire", "missiles", "countermeasures", "repair", "loot", "caution"};
constexpr int kSettingCount = 7;

static bool g_open = false;
static int g_tab = Actions;
static int g_focus = 0;
static int g_scroll = 0;
static std::vector<ActionRow> g_actions;
static std::vector<std::wstring> g_queue; // the QUEUE tab's lines (rebuilt with the actions)
static float g_rebuildIn = 0;
static Edges g_edges;
constexpr int kVisibleActions = 6;
static int g_visibleActions = kVisibleActions; // as many as the room above the HUD's bottom-right panel allows (Render)

// The keyboard + mouse form (the owner, 26 Oct 2026: the autopilot identical on both devices). The same panel, rows and
// behaviour; its keys are the options page's grammar (uikit: arrows, Enter / Space, Esc / Backspace, Tab / Shift+Tab,
// Page Up / Down), queued by OnRawKey while the panel is open and run in Tick; its clicks come through FLvibed's native
// click box (HostApi v22 SetHudHotspots - the box HUD 2's keyboard form uses, free while a modal is open: HUD 2 stands
// its own set down then), so the game's hit test arbitrates click vs fire. Keycaps instead of glyphs in the panel's foot.
enum HotSlot
{
    kHotTab = 0,     // 0..2 the tabs
    kHotClose = 3,   // the title's close mark
    kHotUp = 4,      // the scroll arrows
    kHotDown = 5,
    kHotRow = 6,     // 6.. the visible rows (actions or settings)
    kHotRows = 8,
    kHotSlots = kHotRow + kHotRows
};
static HudHotspot g_hot[kHotSlots] = {};
static bool g_hotPushed = false; // the host holds our set (hidden once when the panel closes / the pad takes over)
static int g_hotHover = -1;
static int g_keyQueue[16];
static int g_nKeys = 0;
static bool g_keyShift = false; // Shift as the raw keys tell it (a posted key leaves GetKeyState alone)
static int g_clickQueue[8];
static int g_nClicks = 0;

static bool Kbm() { return !device::Pad(); }

static void HotRect(int slot, float x0, float y0, float x1, float y1)
{
    if (slot >= 0 && slot < kHotSlots)
        g_hot[slot] = {std::floor(x0), std::floor(y0), std::ceil(x1), std::ceil(y1), 1};
}

// The top edge of the HUD's bottom-right panel group (the target / status panel with the nanobot / battery / weapon
// group row above it): the lowest y the goal panel may reach, from the HUD's own controls by name (ui::Collect; the
// names in docs\kb\hud-controls.md - HudShift moves them, so never a position). 0 when none is on screen (the HUD
// hidden): the caller keeps its own limit. Cached 250 ms (a control-tree walk).
static float HudPanelTop()
{
    float hud2Top = 0; // HUD 2 shows: its own bottom panels (the game's are hidden)
    if (hud2::BottomPanelsTop(hud2Top))
        return hud2Top;
    static float top = 0;
    static ULONGLONG at = 0;
    const ULONGLONG now = GetTickCount64();
    if (now - at < 250)
        return top;
    at = now;
    top = 0;
    for (const ui::Item& it : ui::Collect())
        for (const char* n : {"HudRepairButton", "UseBatteryButton", "HudWeaponGroupButton", "StatusCloseButton", "TargetCloseButton"})
            if (strcmp(it.name, n) == 0)
            {
                const float edge = it.y - it.hh;
                if (top == 0 || edge < top)
                    top = edge;
            }
    return top;
}

static const SettingsBus* Bus()
{
    if (HostAtLeast(18) && g_host->GetSettingsBus)
        if (const SettingsBus* b = g_host->GetSettingsBus())
            return b;
    const SettingsBus* local = options_page::LocalBus();
    return local && local->Count() ? local : nullptr;
}

static std::string Get(const char* key)
{
    char buf[128] = {};
    const SettingsBus* bus = Bus();
    return bus && bus->Get("Autopilot", key, buf, sizeof(buf)) ? buf : "";
}

static void Set(const char* key, const std::string& v)
{
    const SettingsBus* bus = Bus();
    if (!bus || !bus->Set("Autopilot", key, v.c_str()))
        events::Push("goals: set %s = %s refused (no bus row)", key, v.c_str());
}

static std::wstring NameOf(u32 id)
{
    if (!id)
        return L"";
    const space::Contact c = autopilot::FindContact(id);
    if (c.id)
        return ContactName(c);
    const std::wstring n = Upper(space::SpeakerName(id));
    if (!n.empty())
        return n;
    wchar_t buf[24];
    swprintf_s(buf, L"#%u", id);
    return buf;
}

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

// The objective as the game's banner shows it (hud2::ObjectiveLine: formatted by the game, a job's blank first step
// skipped). The raw resource text showed a job's "FIND YOUR TARGET, %S0 %S1, ..." with its codes (26 Oct 2026).
static std::wstring ObjectiveText()
{
    ObjectiveStep step = {};
    if (!HostObjectiveNow(&step) || !step.textId)
        return L"";
    return Upper(hud2::ObjectiveLine(step.textId));
}

static void Rebuild()
{
    g_actions.clear();
    const u32 tid = fl::TargetId();
    const aim::Target t = aim::Current();
    if (tid)
    {
        const space::Contact c = autopilot::FindContact(tid);
        if (c.id)
        {
            const std::wstring name = ContactName(c), d = Dist(c.distance);
            const bool hostile = (t.valid && t.hostile) || c.preHostile;
            if (hostile)
            {
                ActionRow r;
                r.mode = autopilot::Mode::Combat;
                r.target = tid;
                r.label = L"ATTACK " + name;
                r.detail = std::wstring(L"FIGHT IT") +
                           (autopilot::g_set.engage ? L", THEN THE NEAREST ENEMY" : L", THEN STOP") + Sep() + d;
                g_actions.push_back(r);
            }
            else if (c.ship)
            {
                ActionRow r;
                r.target = tid;
                r.mode = autopilot::Mode::Escort;
                r.label = L"ESCORT " + name;
                r.detail = L"STAY WITH IT, FIGHT WHAT THREATENS IT, REJOIN" + Sep() + d;
                g_actions.push_back(r);
                r.mode = autopilot::Mode::Follow;
                r.label = L"FOLLOW " + name;
                wchar_t buf[64];
                swprintf_s(buf, L"STAY WITH IT WITHIN %.0f M", autopilot::g_set.followDistance);
                r.detail = std::wstring(buf) + Sep() + d;
                g_actions.push_back(r);
                r.mode = autopilot::Mode::Formation;
                r.label = L"FORMATION WITH " + name;
                r.detail = L"JOIN ITS FORMATION (THE GAME'S OWN)" + Sep() + d;
                g_actions.push_back(r);
            }
            if (Dockable(c))
            {
                ActionRow r;
                r.mode = autopilot::Mode::Dock;
                r.target = tid;
                r.label = L"DOCK AT " + name;
                if (c.type & space::kTradelaneRing)
                    r.detail = L"RIDE THE TRADE LANE FROM THIS RING";
                else if (c.type & (space::kJumpGate | space::kJumpHole))
                    r.detail = L"JUMP THROUGH IT";
                else
                    r.detail = autopilot::g_set.fastDock ? L"FAST DOCK: FULL SPEED TO THE BAY, THE REQUEST AT ITS MOUTH"
                                                         : L"THE GAME'S DOCK";
                r.detail += Sep() + d;
                g_actions.push_back(r);
                if (c.base) // a base: the fastest way there (lanes, gates) then the dock
                {
                    ActionRow tr;
                    tr.kind = ActionRow::TravelTo;
                    tr.mode = autopilot::Mode::Travel;
                    tr.target = tid;
                    tr.label = L"TRAVEL TO " + name;
                    tr.detail = L"THE FASTEST WAY: LANES, GATES, THE DOCK AT THE END" + Sep() + d;
                    g_actions.push_back(tr);
                }
            }
            ActionRow g;
            g.mode = autopilot::Mode::Goto;
            g.target = tid;
            g.label = L"FLY TO " + name;
            g.detail = ((c.type & space::kLoot) ? L"TO THE LOOT (THE TRACTOR TAKES WHAT IS IN REACH)" : L"THE GAME'S GOTO, ROUTE ROUND HAZARDS") +
                       Sep() + d;
            g_actions.push_back(g);
        }
    }
    {
        ActionRow r;
        r.kind = ActionRow::Objective;
        r.label = L"MISSION OBJECTIVE";
        const std::wstring text = ObjectiveText();
        r.enabled = HostAtLeast(11);
        if (!r.enabled)
            r.detail = L"NEEDS THE GAME RESTARTED (HOST API 11)";
        else if (text.empty())
            r.detail = L"NO OBJECTIVE NOW - IT FLIES WHAT THE NEXT ONE ASKS";
        else
            r.detail = (autopilot::g_obj.on ? L"RUNNING: " : L"DO WHAT IT ASKS: ") + text;
        g_actions.push_back(r);
    }
    {
        const std::vector<space::Waypoint> wps = space::Waypoints();
        fl::Kinematics k;
        if (!wps.empty() && fl::PlayerKinematics(k))
        {
            ActionRow r;
            r.kind = ActionRow::TravelList;
            r.mode = autopilot::Mode::Travel;
            r.label = L"TRAVEL ALONG THE WAYPOINTS";
            const float d[3] = {wps[0].pos[0] - k.pos[0], wps[0].pos[1] - k.pos[1], wps[0].pos[2] - k.pos[2]};
            wchar_t buf[96];
            swprintf_s(buf, L"%u WAYPOINT%ls (THE NAV MAP'S LIST), THE FIRST %ls AWAY", static_cast<unsigned>(wps.size()),
                       wps.size() == 1 ? L"" : L"S", Dist(sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2])).c_str());
            r.detail = buf;
            if (wps[0].object)
                r.label = L"TRAVEL TO " + NameOf(wps[0].object);
            g_actions.push_back(r);
        }
    }
    {
        ActionRow r;
        r.mode = autopilot::Mode::Hold;
        r.label = L"HOLD HERE";
        r.detail = L"STOP AND KEEP STATION, THE NOSE ON THE SCENE";
        g_actions.push_back(r);
    }
    // the QUEUE tab: what runs now (honest: one goal at a time)
    g_queue.clear();
    if (autopilot::Active())
    {
        const autopilot::Mode m = autopilot::Current();
        std::wstring now = L"NOW";
        if (m != autopilot::Mode::Off)
        {
            now += Sep() + ModeVerb(m);
            const std::wstring n = NameOf(autopilot::g.target);
            if (!n.empty())
                now += L" " + n;
            const space::Contact c = autopilot::FindContact(autopilot::g.target);
            if (c.id)
                now += Sep() + Dist(c.distance);
        }
        else
            now += Sep() + L"DECIDING";
        g_queue.push_back(now);
        if (autopilot::g_obj.on)
        {
            g_queue.push_back(L"MISSION" + Sep() + ObjectiveText());
            std::wstring step = L"   STEP ";
            for (const char* p = autopilot::ActionName(autopilot::g_obj.action); *p; ++p)
                step += static_cast<wchar_t>(towupper(*p));
            g_queue.push_back(step);
        }
        if (m == autopilot::Mode::Travel && autopilot::g.tr.plan.ok)
        {
            const route::Plan& plan = autopilot::g.tr.plan;
            for (size_t i = 0; i < plan.steps.size(); ++i)
            {
                const route::Step& st = plan.steps[i];
                std::wstring line = i == autopilot::g.tr.step ? L" > " : L"   ";
                switch (st.kind)
                {
                case route::Kind::Lane: line += L"LANE " + NameOf(st.object) + L" -> " + NameOf(st.endRing); break;
                case route::Kind::Jump: line += L"JUMP " + NameOf(st.object); break;
                case route::Kind::Dock: line += L"DOCK " + NameOf(st.object); break;
                case route::Kind::Arrive:
                {
                    fl::Kinematics k;
                    line += L"ARRIVE";
                    if (fl::PlayerKinematics(k))
                    {
                        const float d[3] = {st.pos[0] - k.pos[0], st.pos[1] - k.pos[1], st.pos[2] - k.pos[2]};
                        line += L" " + Dist(sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]));
                    }
                    break;
                }
                }
                g_queue.push_back(line);
            }
        }
    }
    else
        g_queue.push_back(L"AUTOPILOT OFF");
    g_queue.push_back(L"");
    g_queue.push_back(L"ONE GOAL AT A TIME: THERE IS NO QUEUE YET.");
    g_queue.push_back(L"DO NOW REPLACES THE RUNNING GOAL; QUEUE AND DO NEXT COME LATER.");
    g_rebuildIn = 0.5f;
    const int n = g_tab == Actions ? static_cast<int>(g_actions.size()) : g_tab == Settings ? kSettingCount : 0;
    g_focus = std::clamp(g_focus, 0, std::max(0, n - 1));
}

static int RowCount()
{
    return g_tab == Actions ? static_cast<int>(g_actions.size()) : g_tab == Settings ? kSettingCount : 0;
}

static void KeepVisible()
{
    const int vis = g_tab == Actions ? g_visibleActions : 8;
    if (g_focus < g_scroll)
        g_scroll = g_focus;
    if (g_focus >= g_scroll + vis)
        g_scroll = g_focus - vis + 1;
    g_scroll = std::clamp(g_scroll, 0, std::max(0, RowCount() - vis));
}

static void MoveFocus(int dir)
{
    const int n = RowCount();
    if (!n)
        return;
    g_focus = ((g_focus + dir) % n + n) % n;
    KeepVisible();
}

static void SwitchTab(int dir)
{
    g_tab = ((g_tab + dir) % TabCount + TabCount) % TabCount;
    g_focus = g_scroll = 0;
}

static void DoNow(const ActionRow& r)
{
    if (!r.enabled)
    {
        events::Push("goals: %ls not available", r.label.c_str());
        return;
    }
    bool ok = false;
    switch (r.kind)
    {
    case ActionRow::StartMode: ok = autopilot::Start(r.mode, r.target, nullptr); break;
    case ActionRow::TravelTo:
    {
        autopilot::State::Travel t;
        t.destObject = r.target;
        t.hasDest = space::ObjectPos(r.target, t.dest);
        t.phase = autopilot::State::Travel::Plot;
        ok = t.hasDest && autopilot::Start(autopilot::Mode::Travel, r.target, nullptr);
        if (ok)
            autopilot::g.tr = t;
        break;
    }
    case ActionRow::TravelList: ok = autopilot::StartWaypoints(); break; // the list as it is (the smart start's)
    case ActionRow::Objective: ok = autopilot::StartObjective(); break;  // as `ap start objective` / the smart start
    }
    events::Push("goals do-now %ls -> %s", r.label.c_str(), ok ? "started" : "refused");
    if (ok)
    {
        ctl::MarkAutopilotUsed(); // the AUTOPILOT legend row and the prompts may show (controller.cpp g_apUsed)
        Close(); // one goal at a time: the status line shows it (the panel would only repeat it)
    }
}

// SETTINGS: dir -1 / +1 steps a picker or sets a toggle, 0 toggles / cycles (A)
static void ActOnSetting(int i, int dir)
{
    if (i < 0 || i >= kSettingCount)
        return;
    const uimeta::Row* m = uimeta::FindRegistryRow("Autopilot", kSettingKeys[i]);
    const std::string v = Get(kSettingKeys[i]);
    if (!m || v.empty())
        return;
    if (m->kind == uimeta::Toggle)
    {
        const bool on = atof(v.c_str()) != 0;
        const bool want = dir == 0 ? !on : dir > 0;
        if (want != on)
            Set(kSettingKeys[i], want ? "1" : "0");
    }
    else if (m->kind == uimeta::Picker && m->optionCount > 0)
    {
        const int n = m->optionCount;
        int idx = atoi(v.c_str()) + (dir == 0 ? 1 : dir);
        idx = ((idx % n) + n) % n;
        Set(kSettingKeys[i], std::to_string(idx));
    }
}

void Open()
{
    if (g_open)
        return;
    g_open = true;
    g_tab = Actions;
    g_focus = g_scroll = 0;
    g_edges.Reset();
    Rebuild();
    events::Push("goals open actions=%u ap=%s", static_cast<unsigned>(g_actions.size()),
                 autopilot::Active() ? autopilot::ModeName(autopilot::Current()) : "off");
}

void Close()
{
    if (!g_open)
        return;
    g_open = false;
    g_nKeys = g_nClicks = 0;
    events::Push("goals close");
}

void Toggle()
{
    if (g_open)
        Close();
    else
        Open();
}

bool IsOpen() { return g_open; }

bool OwnsButton(glyphs::Id b)
{
    if (!g_open)
        return false;
    switch (b)
    {
    case glyphs::A:
    case glyphs::B:
    case glyphs::X:
    case glyphs::Y:
    case glyphs::LB:
    case glyphs::RB:
    case glyphs::DU:
    case glyphs::DD:
    case glyphs::DL:
    case glyphs::DR: return true;
    default: return false;
    }
}

// The keyboard's grammar (uikit's, the options page's): the queued keys of this frame.
static void RunKeys()
{
    for (int i = 0; i < g_nKeys && g_open; ++i)
    {
        switch (g_keyQueue[i])
        {
        case VK_ESCAPE:
        case VK_BACK: Close(); break;
        case VK_UP: MoveFocus(-1); break;
        case VK_DOWN: MoveFocus(+1); break;
        case VK_TAB: SwitchTab(g_keyShift || (GetKeyState(VK_SHIFT) & 0x8000) ? -1 : +1); break;
        case VK_PRIOR: SwitchTab(-1); break;
        case VK_NEXT: SwitchTab(+1); break;
        case VK_LEFT:
        case VK_RIGHT:
            if (g_tab == Settings)
                ActOnSetting(g_focus, g_keyQueue[i] == VK_LEFT ? -1 : +1);
            break;
        case VK_RETURN:
        case VK_SPACE:
            if (g_tab == Actions && g_focus < static_cast<int>(g_actions.size()))
                DoNow(g_actions[g_focus]);
            else if (g_tab == Settings)
                ActOnSetting(g_focus, 0);
            break;
        }
    }
    g_nKeys = 0;
}

// A click on one of the panel's hotspots (the same actions as the keys / the pad's buttons).
static void RunClicks()
{
    for (int i = 0; i < g_nClicks && g_open; ++i)
    {
        const int slot = g_clickQueue[i];
        events::Push("goals click %d", slot);
        if (slot >= kHotTab && slot < kHotTab + TabCount)
        {
            g_tab = slot - kHotTab;
            g_focus = g_scroll = 0;
            Rebuild();
        }
        else if (slot == kHotClose)
            Close();
        else if (slot == kHotUp)
            MoveFocus(-1);
        else if (slot == kHotDown)
            MoveFocus(+1);
        else if (slot >= kHotRow && slot < kHotRow + kHotRows)
        {
            const int row = g_scroll + slot - kHotRow;
            if (row < RowCount())
            {
                g_focus = row;
                if (g_tab == Actions)
                    DoNow(g_actions[row]);
                else if (g_tab == Settings)
                    ActOnSetting(row, 0);
            }
        }
    }
    g_nClicks = 0;
}

// The click box: our set while open on the keyboard + mouse (renewed each frame: the host hides a set not renewed),
// hidden once otherwise; the hovered slot moves the focus as the mouse does on the options page.
static void PushHotspots()
{
    const bool want = g_open && Kbm() && HostAtLeast(22) && g_host->SetHudHotspots;
    if (want)
    {
        g_host->SetHudHotspots(g_hot, kHotSlots);
        g_hotPushed = true;
        const int h = g_host->HudHotspotHovered ? g_host->HudHotspotHovered() : 0;
        const int hover = h >= kUiBtnHud2 && h < kUiBtnHud2 + kHotSlots ? h - kUiBtnHud2 : -1;
        if (hover != g_hotHover && hover >= kHotRow && hover < kHotRow + kHotRows && g_scroll + hover - kHotRow < RowCount())
            g_focus = g_scroll + hover - kHotRow;
        g_hotHover = hover;
    }
    else
    {
        if (g_hotPushed && HostAtLeast(22) && g_host->SetHudHotspots)
        {
            static const HudHotspot none[1] = {};
            g_host->SetHudHotspots(none, 0);
        }
        g_hotPushed = false;
        g_hotHover = -1;
    }
    for (HudHotspot& s : g_hot) // Render records this frame's rectangles
        s.shown = 0;
}

void Tick(float dt)
{
    if (g_open && !fl::PlayerIObj())
        Close();
    if (g_open)
    {
        g_rebuildIn -= dt;
        if (g_rebuildIn <= 0)
            Rebuild();
        RunKeys();
        RunClicks();
    }
    g_nKeys = g_nClicks = 0;
    PushHotspots();
}

bool OnRawKey(int vk, bool down, bool textField)
{
    if (vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT)
        g_keyShift = down;
    if (!g_open || textField)
        return false;
    switch (vk)
    {
    case VK_UP: case VK_DOWN: case VK_LEFT: case VK_RIGHT: case VK_RETURN: case VK_SPACE: case VK_TAB:
    case VK_PRIOR: case VK_NEXT: case VK_ESCAPE: case VK_BACK:
        if (down && g_nKeys < 16)
            g_keyQueue[g_nKeys++] = vk;
        return true; // the open panel's key (its release too): the game never sees it while the panel is open
    default: return false;
    }
}

bool OwnsHotspots() { return g_hotPushed; }

void OnHotspot(int slot)
{
    if (g_nClicks < 8)
        g_clickQueue[g_nClicks++] = slot;
}

void Update(float dt, const bool down[glyphs::COUNT])
{
    if (!g_open)
        return;
    g_edges.Update(dt, down);
    const bool* p = g_edges.pressed;
    if (p[glyphs::B])
    {
        Close();
        return;
    }
    if (p[glyphs::LB])
        SwitchTab(-1);
    if (p[glyphs::RB])
        SwitchTab(+1);
    if (p[glyphs::DU])
        MoveFocus(-1);
    if (p[glyphs::DD])
        MoveFocus(+1);
    if (g_tab == Actions)
    {
        if (p[glyphs::A] && g_focus < static_cast<int>(g_actions.size()))
            DoNow(g_actions[g_focus]);
        if (p[glyphs::X] || p[glyphs::Y])
            events::Push("goals: no queue - %s is not available yet (DO NOW replaces the running goal)",
                         p[glyphs::X] ? "QUEUE" : "DO NEXT");
    }
    else if (g_tab == Settings)
    {
        if (p[glyphs::A])
            ActOnSetting(g_focus, 0);
        if (p[glyphs::DL])
            ActOnSetting(g_focus, -1);
        if (p[glyphs::DR])
            ActOnSetting(g_focus, +1);
    }
}

// A keyboard key in the panel's foot / title (the keyboard form's twin of a glyph): a thin frame round its name in the
// current font; returns its width.
static float KeyCap(const render::Font& font, float x, float cy, float h, const std::wstring& name, u32 argb, float H)
{
    render::SetFont(font);
    const float tw = static_cast<float>(render::TextWidth(font, name));
    const float w = std::max(h, tw + h * 0.6f);
    uidraw::Frame(x, cy - h * 0.5f, w, h, std::max(1.0f, H * 0.0012f), argb);
    render::Text(x + (w - tw) * 0.5f, cy - font.lineHeight * 0.5f, name, argb);
    return w;
}

void Render(void* device)
{
    if (!g_open)
        return;
    Screen s;
    if (!BeginScreen(device, s))
        return;
    const float H = s.H, W = s.W, ts = s.ts;
    const uikit::Fonts& f = *s.f;
    const bool kbm = Kbm();
    std::vector<Prompt> prompts;
    prompts.push_back({GlyphToken(glyphs::LB) + GlyphToken(glyphs::RB) + L" TAB"});
    if (g_tab == Actions)
    {
        prompts.push_back({GlyphToken(glyphs::A) + L" DO NOW"});
        prompts.push_back({GlyphToken(glyphs::X) + L" QUEUE", false});
        prompts.push_back({GlyphToken(glyphs::Y) + L" DO NEXT", false});
    }
    else if (g_tab == Settings)
    {
        prompts.push_back({GlyphToken(glyphs::A) + L" CHANGE"});
        prompts.push_back({GlyphToken(glyphs::DL) + GlyphToken(glyphs::DR) + L" VALUE"});
    }
    prompts.push_back({GlyphToken(glyphs::B) + L" CLOSE"});
    const float barTop = PromptBar(s, prompts);
    // the panel, right-middle (one column; the settings rows need the kit's control column: 0.34 H). Its bottom stays
    // above the HUD's bottom-right panel group (the consumables / weapon group row over the target panel; the strip at
    // ~0.70 H at every aspect, release acceptance 25 Oct 2026: the panel's bottom at ~0.73 H covered it) and above the
    // prompt bar; the action rows shrink when the room between the top margin and that edge is short.
    const float pad = H * 0.012f;
    const float w = std::min(H * 0.6f * ts, W * 0.48f);
    const float headH = H * 0.05f * ts, tabH = H * 0.038f;
    const float actionPitch = (f.label.lineHeight + f.body.lineHeight) * 1.15f;
    const float settingPitch = H * 0.0454f, linePitch = f.label.lineHeight * 1.35f;
    const float hudTop = HudPanelTop();
    const float limit = std::min(barTop - H * 0.01f, hudTop > 0 ? hudTop - H * 0.012f : H);
    const float top = H * 0.02f, room = limit - top - (headH + tabH + pad * 3.5f);
    g_visibleActions = std::clamp(static_cast<int>(room / actionPitch), 3, kVisibleActions);
    const float footH = kbm ? f.body.lineHeight * 1.9f : 0; // the keyboard form's keys
    const float bodyH = std::max(g_visibleActions * actionPitch, std::max(kSettingCount * settingPitch, 8 * linePitch));
    const float h = headH + tabH + bodyH + footH + pad * 3.5f;
    const float x1 = W - W * 0.02f, x0 = x1 - w;
    float y0 = H * 0.5f - h * 0.5f;
    if (y0 + h > limit)
        y0 = limit - h;
    y0 = std::max(y0, top);
    const double now = GetTickCount64() / 1000.0;
    uidraw::HudPanel(x0, y0, w, h, 0.92f, H, now);
    // the title
    float y = y0 + pad;
    render::SetFont(f.title);
    const std::wstring title = L"AUTOPILOT GOALS";
    uidraw::TextShadow(x0 + (w - render::TextWidth(f.title, title)) * 0.5f, y + (headH - f.title.lineHeight) * 0.5f, title,
                       Argb(kHud, 0.95f), std::max(1.0f, H * 0.001f));
    if (kbm) // the close mark: its key, clickable
    {
        const float kh = f.body.lineHeight * 1.2f;
        const float kw = KeyCap(f.body, 0, 0, kh, L"ESC", 0, H); // (measure: drawn transparent)
        const float kx = x1 - pad - kw, cy = y + headH * 0.5f;
        KeyCap(f.body, kx, cy, kh, L"ESC", Argb(g_hotHover == kHotClose ? kWhite : kHud, 0.9f), H);
        HotRect(kHotClose, kx - pad * 0.3f, cy - kh * 0.7f, kx + kw + pad * 0.3f, cy + kh * 0.7f);
    }
    y += headH;
    if (kbm) // the tabs across the width, each clickable (Tab / Shift+Tab step them)
    {
        const float tabW = (w - 2 * pad) / TabCount - pad * 0.5f;
        for (int t = 0; t < TabCount; ++t)
        {
            const float tx = x0 + pad + t * (tabW + pad * 0.5f);
            uikit::TabButton(f, tx, y, tabW, tabH, H, kTabNames[t], t == g_tab, g_hotHover == kHotTab + t);
            HotRect(kHotTab + t, tx, y, tx + tabW, y + tabH);
        }
    }
    else // the tabs with the bumpers at their sides
    {
        const float gh = tabH * 0.6f;
        glyphs::Draw(glyphs::LB, x0 + pad + glyphs::Width(glyphs::LB, gh) * 0.5f, y + tabH * 0.5f, gh, 0.9f);
        glyphs::Draw(glyphs::RB, x1 - pad - glyphs::Width(glyphs::RB, gh) * 0.5f, y + tabH * 0.5f, gh, 0.9f);
        const float inner = w - 2 * (pad + glyphs::Width(glyphs::LB, gh) + pad);
        const float tabW = inner / TabCount - pad * 0.5f;
        for (int t = 0; t < TabCount; ++t)
            uikit::TabButton(f, x0 + pad + glyphs::Width(glyphs::LB, gh) + pad + t * (tabW + pad * 0.5f), y, tabW, tabH, H,
                             kTabNames[t], t == g_tab, false);
    }
    y += tabH + pad;
    const float left = x0 + pad * 1.6f, right = x1 - pad * 1.6f;
    if (g_tab == Actions)
    {
        const int n = static_cast<int>(g_actions.size());
        for (int i = g_scroll, vi = 0; i < n && vi < g_visibleActions; ++i, ++vi)
        {
            const ActionRow& r = g_actions[i];
            const float ry = y + vi * actionPitch;
            if (i == g_focus)
                uidraw::FocusFrame(x0 + pad * 0.7f, ry, w - pad * 1.4f, actionPitch, H, Argb(uidraw::kFrame & 0xFFFFFF, 0.9f));
            render::SetFont(f.label);
            uidraw::TextShadow(left, ry + actionPitch * 0.08f, Fit(f.label, r.label, right - left),
                               Argb(r.enabled ? kHud : kGrey, 0.95f), std::max(1.0f, H * 0.001f));
            render::SetFont(f.body);
            render::Text(left + H * 0.01f, ry + actionPitch * 0.08f + f.label.lineHeight, Fit(f.body, r.detail, right - left - H * 0.01f),
                         Argb(r.enabled ? kWhite : kGrey, 0.85f));
            if (vi < kHotRows) // a click = DO NOW (the keyboard form; the mouse over a row focuses it)
                HotRect(kHotRow + vi, x0 + pad * 0.7f, ry, x1 - pad * 1.6f, ry + actionPitch);
        }
        if (n > g_visibleActions)
        {
            const float ax = x1 - pad * 0.9f, a = H * 0.009f;
            uidraw::Triangle(ax, y + a, a, 0, g_scroll > 0 ? uidraw::kText : uidraw::kDisabled);
            uidraw::Triangle(ax, y + g_visibleActions * actionPitch - a, a, 1,
                             g_scroll + g_visibleActions < n ? uidraw::kText : uidraw::kDisabled);
            HotRect(kHotUp, ax - a * 1.6f, y, ax + a * 1.6f, y + a * 3);
            HotRect(kHotDown, ax - a * 1.6f, y + g_visibleActions * actionPitch - a * 3, ax + a * 1.6f,
                    y + g_visibleActions * actionPitch);
        }
    }
    else if (g_tab == Queue)
    {
        render::SetFont(f.label);
        for (size_t i = 0; i < g_queue.size() && i < 10; ++i)
        {
            const bool note = i + 2 >= g_queue.size();
            render::Text(left, y + i * linePitch, Fit(f.label, g_queue[i], right - left), Argb(note ? kDim : kHud, 0.95f));
        }
    }
    else
    {
        const SettingsBus* bus = Bus();
        for (int i = 0; i < kSettingCount; ++i)
        {
            const uimeta::Row* m = uimeta::FindRegistryRow("Autopilot", kSettingKeys[i]);
            uikit::RowGeom g;
            g.y = y + i * settingPitch;
            g.h = settingPitch;
            g.labelX = left + H * 0.03f;
            g.controlRight = right - H * 0.012f; // (the kit's focus frame reaches 0.015 H past the control column)
            g.controlLeft = right - 0.36f * H;
            g.sh = H;
            uikit::RowStyle st;
            st.focused = i == g_focus;
            const std::string v = Get(kSettingKeys[i]);
            st.enabled = bus && m && !v.empty();
            std::wstring label = m ? uikit::Widen(m->label) : uikit::Widen(kSettingKeys[i]);
            uikit::RowLabel(f, g, st, label);
            if (i < kHotRows) // a click = CHANGE (the next value)
                HotRect(kHotRow + i, x0 + pad * 0.7f, g.y, x1 - pad * 0.7f, g.y + g.h);
            if (!m || !st.enabled)
                uikit::RowPicker(f, g, st, L"NOT AVAILABLE");
            else if (m->kind == uimeta::Toggle)
                uikit::RowToggle(f, g, st, atof(v.c_str()) != 0);
            else
            {
                const int idx = atoi(v.c_str());
                uikit::RowPicker(f, g, st,
                                 idx >= 0 && idx < m->optionCount ? uikit::Widen(m->options[idx]) : uikit::Widen(v.c_str()));
            }
        }
        // the help line of the focused row
        if (const uimeta::Row* m = uimeta::FindRegistryRow("Autopilot", kSettingKeys[std::clamp(g_focus, 0, kSettingCount - 1)]))
        {
            render::SetFont(f.body);
            render::Text(left, y + kSettingCount * settingPitch + pad * 0.5f, Fit(f.body, uikit::Widen(m->help), right - left),
                         Argb(kDim, 0.95f));
        }
    }
    if (kbm) // the keyboard form's foot: the pad's prompts as keys (QUEUE / DO NEXT have no key: there is no queue yet)
    {
        struct Item
        {
            const wchar_t* key;
            const wchar_t* label;
        };
        std::vector<Item> items;
        if (g_tab == Actions)
            items = {{L"ENTER", L"DO NOW"}, {L"UP/DOWN", L"ROW"}, {L"TAB", L"NEXT TAB"}};
        else if (g_tab == Settings)
            items = {{L"ENTER", L"CHANGE"}, {L"LEFT/RIGHT", L"VALUE"}, {L"TAB", L"NEXT TAB"}};
        else
            items = {{L"TAB", L"NEXT TAB"}};
        const float kh = f.body.lineHeight * 1.2f, cy = y0 + h - pad - footH * 0.5f;
        float kx = left;
        for (const Item& it : items)
        {
            kx += KeyCap(f.body, kx, cy, kh, it.key, Argb(kHud, 0.9f), H) + pad * 0.5f;
            render::SetFont(f.body);
            render::Text(kx, cy - f.body.lineHeight * 0.5f, it.label, Argb(kWhite, 0.85f));
            kx += static_cast<float>(render::TextWidth(f.body, it.label)) + pad * 1.4f;
        }
        render::SetFont(f.body);
        const std::wstring click = L"CLICK A ROW";
        const float cw = static_cast<float>(render::TextWidth(f.body, click));
        if (kx + cw < right)
            render::Text(right - cw, cy - f.body.lineHeight * 0.5f, click, Argb(kDim, 0.9f));
    }
    render::End(false);
}

std::string Command(std::istream& in)
{
    std::string what;
    in >> what;
    if (what == "open")
        Open();
    else if (what == "close")
        Close();
    else if (what == "toggle")
        Toggle();
    else if (what == "tab")
    {
        int t = 0;
        in >> t;
        g_tab = std::clamp(t, 0, TabCount - 1);
        g_focus = g_scroll = 0;
        Rebuild();
    }
    else if (what == "up")
        MoveFocus(-1);
    else if (what == "down")
        MoveFocus(+1);
    else if (what == "a")
    {
        if (g_tab == Actions && g_focus < static_cast<int>(g_actions.size()))
            DoNow(g_actions[g_focus]);
        else if (g_tab == Settings)
            ActOnSetting(g_focus, 0);
    }
    else if (what == "left")
        ActOnSetting(g_focus, -1);
    else if (what == "right")
        ActOnSetting(g_focus, +1);
    else if (what == "x" || what == "y")
        events::Push("goals: no queue - %s is not available yet", what == "x" ? "QUEUE" : "DO NEXT");
    else if (what == "rebuild")
        Rebuild();
    else if (what == "click") // goals click <slot>: a click as the host routes it (the keyboard + mouse form's logic half)
    {
        int slot = -1;
        in >> slot;
        OnHotspot(slot);
    }
    else if (what != "state" && !what.empty())
        return "goals: open | close | toggle | tab <0-2> | up | down | a | x | y | left | right | rebuild | click <slot> | state";
    std::ostringstream o;
    o << "goals open=" << g_open << " tab=" << text::Utf8(kTabNames[g_tab]) << " focus=" << g_focus
      << " visibleActions=" << g_visibleActions << " hudTop=" << HudPanelTop()
      << " ap=" << (autopilot::Active() ? autopilot::ModeName(autopilot::Current()) : "off")
      << " objective=" << autopilot::g_obj.on << " bus=" << (Bus() ? Bus()->Count() : -1) << " form="
      << (Kbm() ? "kbm" : "pad") << " clickbox=" << g_hotPushed << " hover=" << g_hotHover << "\n";
    if (g_hotPushed)
    {
        o << "hotspots:";
        for (int i = 0; i < kHotSlots; ++i)
            if (g_hot[i].shown)
                o << " " << i << "(" << g_hot[i].x0 << "," << g_hot[i].y0 << "-" << g_hot[i].x1 << "," << g_hot[i].y1 << ")";
        o << "\n";
    }
    o << "actions:";
    for (size_t i = 0; i < g_actions.size(); ++i)
        o << (g_tab == Actions && static_cast<int>(i) == g_focus ? "\n > " : "\n   ") << text::Utf8(g_actions[i].label)
          << " | " << text::Utf8(g_actions[i].detail) << (g_actions[i].enabled ? "" : " (greyed)");
    o << "\nqueue:";
    for (const std::wstring& l : g_queue)
        o << "\n   " << text::Utf8(l);
    o << "\nsettings:";
    for (int i = 0; i < kSettingCount; ++i)
        o << (g_tab == Settings && i == g_focus ? "\n > " : "\n   ") << kSettingKeys[i] << " = " << Get(kSettingKeys[i]);
    return o.str() + "\n";
}
} // namespace goals
