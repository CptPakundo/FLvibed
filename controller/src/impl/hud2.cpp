// HUD 2: state, data at its own rate, the settings, the transition, the HUD bus - see hud2.h, docs\UI_HUD2.md.
#include "hud2.h"

#include "../dimouse.h"
#include "../fl.h"
#include "aim.h"
#include "autopilot.h"
#include "controller.h" // ctl::NearbyEnemy
#include "autopilot_impl.h" // read-only: the objective run's action (the wait reason, as padhud's status line)
#include "flexe.h"
#include "flufaware.h" // FLUF's windows: the parts under them step back
#include "game/ai.h"
#include "game/flicons.h"
#include "game/flight.h"
#include "game/hudinfo.h"
#include "game/render.h"
#include "game/shipname.h"
#include "game/space.h"
#include "game/promptshow.h"
#include "game/hudwake.h"
#include "host.h"
#include "hud2_impl.h"
#include "hud2_layout.h"
#include "hud2_sim.h"
#include "hud_bus.h"
#include "hudvis.h"
#include "promptbar.h"
#include "log.h"
#include "settings_bus.h"
#include "text.h"
#include "ui.h"
#include "uiopt.h"

#include <sstream>

// (ASCII only: the build reads the sources as cp1252, non-ASCII text goes in as \u escapes)
namespace hud2
{
using namespace hud2kit;
static const wchar_t kDot[] = L" \u00B7 ";

const char* StateName(State s)
{
    switch (s)
    {
    case State::Calm: return "calm";
    case State::Combat: return "combat";
    case State::CruiseCharge: return "cruise";
    case State::Maneuver: return "maneuver";
    case State::Autopilot: return "autopilot";
    case State::Turret: return "turret";
    }
    return "?";
}

static Context g_ctx;
static Model g_m;
static Layout g_L;
static hud2layout::Out g_core; // the layout core's last frame (hud2_layout.h): g_L is its copy for the parts
// Focus's reveal (hud2_layout.h Reveal: what it holds back in calm, the wakes, the peek, the fight form): Update steps
// it while Focus draws; g_rv = its last output. g_entrySeen: this space entry's auto-peek moment has passed.
static hud2layout::Reveal g_reveal;
static hud2layout::RevealOut g_rv;
static bool g_entrySeen = false;
static bool g_focusReadout = false; // the lever rests partial and the setup needs its reading (the band's tick + %)
static float g_navBottom = 0; // the nav's drawn bottom (last frame; Focus's nav grows with its lines)
static Fades g_f;
static int g_force = -1;         // debug: -1 auto (hud_style), 0 off, 1 on
// the backing under a 3D view (RequestViews -> RenderUnderHud): the plate's window column, its corners, its glass alpha
// (0.82, at least 3/4 of it in the calm fade: the view's thin lines must read over a bright station)
struct ViewBack
{
    bool on;
    float x0, y0, x1, y1, ch;
    int corners;
    float alpha;
};
static ViewBack g_viewBack[2] = {};
static bool g_viewBackOn = true; // debug: hud2 viewback on|off
static u32 g_viewBackDrawn = 0;
static u32 g_viewPulses = 0; // frames the target panel ran shown to build its view (RequestViews)
static float kViewZoom = 1.5f; // the views' rectangles over their windows (debug: hud2 viewzoom <f>)
static std::string g_sim;        // debug: a mock-up state painted over the live data (its name: hud2_sim.h)
static bool g_simComm = false;   // debug: `hud2 sim <name>+comm` - the comm surface (the subtitles' dialogue panel) shows
static bool g_active = false;    // the game's panels are hidden this frame
static bool g_menuOver = false;  // an Extended HUD 2 modal menu is over the flight (SetMenuOver): HUD 2 not drawn
static bool g_visible = false;   // HUD 2 draws (fading in or shown)
static bool g_wasInSpace = false;
static float g_t = 0;            // the transition, 0 = the game's HUD .. 1 = HUD 2
static float g_slow = 1;         // seconds since the 10 Hz refresh
static u32 g_switches = 0;
static struct
{
    float update = 0, render = 0; // ms, smoothed
    int drawn = 0;
} g_perf;

static std::wstring Upper(std::wstring s)
{
    for (wchar_t& c : s)
        c = static_cast<wchar_t>(towupper(c));
    return s;
}
static std::wstring Dist(float m)
{
    wchar_t b[32];
    if (m >= 1000)
        swprintf_s(b, L"%.1f km", m / 1000);
    else
        swprintf_s(b, L"%.0f m", m);
    return b;
}
static std::wstring Clock(float s)
{
    if (s < 0)
        return L"";
    wchar_t b[32];
    const int t = static_cast<int>(s + 0.5f);
    swprintf_s(b, L"%d:%02d", t / 60, t % 60);
    return b;
}

// ---------------------------------------------------------------- the keyboard's keys (the K&M form's keycaps)
// The game's live keymap (exe::KeyCommands, as barbutton.cpp reads it): command -> its first key's name. Rebuilt every
// 5 s (the player may rebind in the Options screen).
static std::map<int, std::wstring> g_keys;
static DWORD g_keysAt = 0;
static int g_fireVk[32] = {}, g_fireVkN = 0; // the keymap's fire commands' keys (FireKeyPressed)
static std::wstring VkName(int vk, u32 mods)
{
    std::wstring s;
    if (mods & 4)
        s += L"CTRL+";
    if (mods & 0x10)
        s += L"ALT+";
    if (mods & 1)
        s += L"SHIFT+";
    if (vk == -1)
        return s + L"LMB";
    if (vk == -2)
        return s + L"RMB";
    if (vk == -3)
        return s + L"MMB";
    if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9'))
        return s + static_cast<wchar_t>(vk);
    if (vk >= VK_F1 && vk <= VK_F24)
        return s + L"F" + std::to_wstring(vk - VK_F1 + 1);
    switch (vk)
    {
    case VK_SPACE: return s + L"SPACE";
    case VK_TAB: return s + L"TAB";
    case VK_RETURN: return s + L"ENTER";
    case VK_BACK: return s + L"BKSP";
    case VK_ESCAPE: return s + L"ESC";
    case VK_SHIFT: return s + L"SHIFT";
    case VK_CONTROL: return s + L"CTRL";
    case VK_MENU: return s + L"ALT";
    case VK_UP: return s + L"UP";
    case VK_DOWN: return s + L"DOWN";
    case VK_LEFT: return s + L"LEFT";
    case VK_RIGHT: return s + L"RIGHT";
    default: break;
    }
    wchar_t name[32] = {};
    const UINT sc = MapVirtualKeyW(static_cast<UINT>(vk), 0 /*MAPVK_VK_TO_VSC*/);
    if (sc && GetKeyNameTextW(static_cast<LONG>(sc << 16), name, 32) > 0)
    {
        // a key whose name is a lone small mark reads as an EMPTY keycap at the HUD's size: the game's free flight
        // (USER_MANEUVER_FREE_FLIGHT, key 192 = VK_OEM_3 in SAVE\UserKeyMap.ini) is "`" on a US layout - ABORT DOCK /
        // STOP / the K&M maneuver row's first key showed a ~0.004 H tick in an empty cap (the owner's frames, 31 Oct
        // 2026). The grave key shows its larger mark "~" (the same 1-character cap: "TILDE" widened LEAVE FORMATION's
        // pill to 0.28 H, past the K&M loadout's edge - the context line fell back above the panels into zone S1);
        // the rarer marks get their word; a letter-like name (a German layout's "\x00D6") stays as it is.
        if (!name[1])
            switch (name[0])
            {
            case L'`':
            case L'~': return s + L"~";
            case 0x00B4: return s + L"ACUTE";
            case L'^': return s + L"CARET";
            case L'\'': return s + L"QUOTE";
            case L',': return s + L"COMMA";
            case L'.': return s + L"PERIOD";
            case L'-': return s + L"MINUS";
            case 0x00A8: return s + L"DIAERESIS";
            default: break;
            }
        return s + Upper(name);
    }
    return L"";
}
struct RawKey
{
    int cmd, vk;
    u32 mods;
};
// The keymap's (command, first key) pairs (guarded: no C++ objects in this frame); returns the count.
static int ReadKeymap(RawKey* out, int max)
{
    int n = 0;
    __try
    {
        const u32 sentinel = Mem<u32>(exe::KeyCommands);
        u32 node = sentinel ? Mem<u32>(sentinel) : 0;
        for (int guard = 0; node && node != sentinel && guard < 1000 && n < max; ++guard, node = Mem<u32>(node))
        {
            const u32 entry = node + 8;
            const u32 begin = Mem<u32>(entry + 0x14), end = Mem<u32>(entry + 0x18);
            if (!begin || end <= begin || end - begin > 64 * 8)
                continue;
            out[n++] = {Mem<int>(entry), Mem<int>(begin), Mem<u32>(begin + 4)};
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
    return n;
}
static void RefreshKeys()
{
    const DWORD now = GetTickCount();
    if (now - g_keysAt < 5000 && !g_keys.empty())
        return;
    g_keysAt = now;
    g_keys.clear();
    if (!exe::KeyCommands.ok())
        return;
    static RawKey raw[512];
    const int n = ReadKeymap(raw, 512);
    g_fireVkN = 0;
    for (int i = 0; i < n; ++i)
    {
        if (g_keys.find(raw[i].cmd) == g_keys.end())
            g_keys[raw[i].cmd] = VkName(raw[i].vk, raw[i].mods);
        const int c = raw[i].cmd;
        const bool fire = (c >= fl::USER_FIRE_WEAPON1 && c <= fl::USER_FIRE_WEAPON_GROUP6) || c == fl::USER_FIRE_FORWARD ||
                          c == fl::USER_LAUNCH_MISSILES || c == fl::USER_LAUNCH_MINES ||
                          c == fl::USER_LAUNCH_COUNTERMEASURES || c == fl::USER_LAUNCH_TORPEDOS ||
                          c == fl::USER_LAUNCH_CRUISE_DISRUPTORS || c == fl::USER_FIRE_WEAPONS;
        const int vk = raw[i].vk == -1 ? VK_LBUTTON : raw[i].vk == -2 ? VK_RBUTTON : raw[i].vk == -3 ? VK_MBUTTON : raw[i].vk;
        if (fire && vk > 0 && vk < 256 && g_fireVkN < 32)
            g_fireVk[g_fireVkN++] = vk;
    }
}
// Any of the keymap's fire keys / buttons went down this frame (the keyboard and mouse's fire input, 0.2.1 round 6:
// core\game\hudwake.h FireInput) - only while the game's window is in front (a background game hears nothing).
static bool FireKeyPressed()
{
    static bool was[32] = {};
    RefreshKeys();
    DWORD pid = 0;
    const HWND fg = GetForegroundWindow();
    if (fg)
        GetWindowThreadProcessId(fg, &pid);
    const bool front = pid == GetCurrentProcessId();
    bool pressed = false;
    for (int i = 0; i < 32; ++i)
    {
        const bool down = front && i < g_fireVkN && (GetAsyncKeyState(g_fireVk[i]) & 0x8000) != 0;
        pressed |= down && !was[i];
        was[i] = down;
    }
    return pressed;
}
static std::wstring KeyOf(int cmd)
{
    const auto it = g_keys.find(cmd);
    return it == g_keys.end() ? L"" : it->second;
}
static std::wstring AutopilotKey()
{
    const SettingsBus* bus = HostAtLeast(18) && g_host->GetSettingsBus ? g_host->GetSettingsBus() : nullptr;
    char v[32] = {};
    if (!bus || !bus->Get || !bus->Get("Controller", "autopilot_key", v, sizeof(v)) || !v[0])
        strcpy_s(v, "K");
    std::wstring s;
    for (const char* p = v; *p; ++p)
        s += static_cast<wchar_t>(toupper(static_cast<unsigned char>(*p)));
    return s;
}

// The command an action sends from the keyboard (the K&M form's keycap); `loot`: the context's tractor form (one item).
static int CmdOf(presets::Action a, bool dock, bool loot = false)
{
    using A = presets::Action;
    switch (a)
    {
    case A::Context: return loot ? fl::USER_TRACTOR_BEAM : dock ? fl::USER_MANEUVER_DOCK : fl::USER_MANEUVER_GOTO;
    case A::Goto: return fl::USER_MANEUVER_GOTO;
    case A::FreeFlight: return fl::USER_MANEUVER_FREE_FLIGHT;
    case A::Formation: return fl::USER_MANEUVER_FORMATION;
    case A::Cruise: return fl::USER_CRUISE;
    case A::Hub: return fl::USER_NAV_MAP;
    case A::Nanobot: return fl::USER_REPAIR_HEALTH;
    case A::Battery: return fl::USER_REPAIR_SHIELD;
    case A::Cm: return fl::USER_LAUNCH_COUNTERMEASURES;
    case A::Mine: return fl::USER_LAUNCH_MINES;
    case A::Missile: return fl::USER_LAUNCH_MISSILES;
    case A::Torpedo: return fl::USER_LAUNCH_TORPEDOS;
    case A::CruiseDisruptor: return fl::USER_LAUNCH_CRUISE_DISRUPTORS;
    case A::Fire: return fl::USER_FIRE_WEAPONS;
    case A::Reverse: return fl::USER_MANEUVER_BRAKE_REVERSE;
    case A::Turret: return fl::USER_AUTO_TURRET;
    case A::Tractor: return fl::USER_COLLECT_LOOT; // X: everything in reach (the targeted item: Context's loot form)
    case A::TargetSmart:
    case A::NextEnemy: return fl::USER_NEXT_ENEMY;
    case A::TargetNearest: return fl::USER_CLOSEST_ENEMY;
    case A::TargetPicker: return fl::USER_CONTACT_LIST;
    case A::Afterburner: return fl::USER_AFTERBURN;
    default: return -1;
    }
}

// A prompt for an action in the current form: the preset's button (pad), or the keymap's key (K&M). False when the
// action has no button / key: the prompt is not offered.
static bool MakePrompt(Prompt& p, presets::Action a, const wchar_t* label, bool dock = false, bool loot = false)
{
    p = Prompt();
    p.label = label ? label : L"";
    if (g_m.kbm)
    {
        if (a == presets::Action::Autopilot)
            p.key = AutopilotKey();
        else if (a == presets::Action::GoalPanel) // the goal panel's keyboard twin: Shift + the autopilot key (barbutton)
            p.key = L"SHIFT+" + AutopilotKey();
        else
        {
            const int cmd = CmdOf(a, dock, loot);
            if (cmd >= 0)
                p.key = KeyOf(cmd);
        }
        return !p.key.empty();
    }
    const presets::Table& t = g_ctx.table;
    if (a == presets::Action::Handback)
        for (glyphs::Id s : {glyphs::LS, glyphs::RS})
            if (t.b[s].tap == a)
                return p.btn = s, true;
    int slot = 0;
    if (!presets::Find(t, a, p.btn, slot))
        return p.btn = glyphs::COUNT, false;
    p.hold = slot == 1;
    return true;
}

// The lane a context prompt takes (the owner, 31 Oct 2026, decision 4: the prompts hug the bottom-left and the bottom-
// right, the centre bottom clear): by the corners' jobs (UI_HUD2_PRESETS.md 1.1: bottom-left your ship, bottom-right
// what you fire and use) - the ship's movement and control (dock, go to, cruise, free flight and the aborts, reverse, the
// autopilot and its goals, the map) left beside the ship panel; what you fire, use or target (the turret, the tractor,
// the targeting prompts) right beside the loadout.
static int CtxSide(presets::Action a, const wchar_t* label)
{
    using A = presets::Action;
    switch (a)
    {
    case A::Turret:
    case A::Tractor: // ([X] TRACTOR n: TractorCtx, 5.23)
    case A::TargetSmart:
    case A::TargetPicker: return 1;
    case A::Context: return label && wcsncmp(label, L"TRACTOR", 7) == 0 ? 1 : 0;
    default: return 0;
    }
}

static void AddCtx(presets::Action a, const wchar_t* label, bool dock = false, bool loot = false)
{
    if (g_m.nCtx >= 3)
        return;
    Prompt p;
    if (!MakePrompt(p, a, label, dock, loot))
        return;
    p.side = CtxSide(a, label);
    for (int i = 0; i < g_m.nCtx; ++i)
        if (p.key.empty() ? g_m.ctx[i].btn == p.btn : g_m.ctx[i].key == p.key)
            return; // one prompt per button (the round 3 critique: X TURRET (hold) beside X LOOT (tap) - one glyph, two meanings)
    g_m.ctx[g_m.nCtx++] = p;
}

// The torpedo row's prompt: the pad's glyph and "HOLD" in the row's text when it is a hold (RB: not a tap, GitHub issue
// 5), the keyboard form's key in the text (a keycap does not fit the 0.030 pitch beside the launcher's).
static void TorpedoPrompt(Model& m, presets::Action a)
{
    MakePrompt(m.torpedoBtn, a, nullptr);
    const std::wstring sep = std::wstring(L"  ") + static_cast<wchar_t>(0x00B7) + L"  ";
    if (!m.torpedoBtn.key.empty())
        m.torpedo = m.torpedoBtn.key + sep + m.torpedo, m.torpedoBtn = Prompt();
    else if (m.torpedoBtn.hold)
        m.torpedo = L"HOLD" + sep + m.torpedo;
}

// The tractor's prompts in the right lane (CtxSide: what you use; they left the spot under the reticle ring on 31 Oct
// 2026, UI_HUD2.md 5.23). The owner, 4 Nov 2026: "the tractor prompts at the bottom of the screen [should] note the
// object's name and range. Also, it's not prompted anywhere that the A button also tractors something. Prompting should
// clarify which prompt does what (tractor specific, tractor all, etc)". So: [A] TRACTOR <item> . <range> - one item, the
// targeted loot in reach or, with no target, the nearest loot in reach (ContextAction; `aFree`: the state leaves A to
// the context, and not in ASSISTED, whose A starts the autopilot; the keyboard's key tractors only a target). [X] TRACTOR
// ALL n - everything in reach; with one item in reach that A does not name, [X] names it. Focus: A's prompt only (its
// band shows LOOT n with X's glyph, PRESETS 2.2). Within the line's three prompts and its one-prompt-per-button rule
// (combat's X-hold TURRET keeps the X: the round 3 critique); idempotent.
static std::wstring TractorItem(const std::wstring& name, const std::wstring& dist)
{
    return L"TRACTOR " + Upper(name) + L" " + static_cast<wchar_t>(0x00B7) + L" " + dist;
}

static void TractorCtx(bool aFree)
{
    std::wstring one;
    if (aFree && uiopt::Get().padPreset != presets::ASSISTED)
    {
        if (g_m.target && g_m.tLootReach)
            one = TractorItem(g_m.tName, g_m.tDist);
        else if (!g_m.target && !g_m.kbm && g_m.loot > 0 && !g_m.lootName.empty())
            one = TractorItem(g_m.lootName, g_m.lootDist);
    }
    if (!one.empty())
        AddCtx(presets::Action::Context, one.c_str(), false, true);
    if (g_m.loot <= 0 || g_m.presetDrawn == hud2preset::Focus)
        return;
    std::wstring all;
    if (g_m.loot > 1)
        all = L"TRACTOR ALL " + std::to_wstring(g_m.loot);
    else if (one.empty() && !g_m.lootName.empty())
        all = TractorItem(g_m.lootName, g_m.lootDist);
    else if (one.empty())
        all = L"TRACTOR ALL";
    if (!all.empty())
        AddCtx(presets::Action::Tractor, all.c_str());
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

// ---------------------------------------------------------------- the data
static float Bearing(const float pos[3], const fl::Kinematics& k)
{
    const float r[3] = {pos[0] - k.pos[0], pos[1] - k.pos[1], pos[2] - k.pos[2]};
    // columns of rot = the ship's right / up / back axes
    const float x = r[0] * k.rot[0] + r[1] * k.rot[3] + r[2] * k.rot[6];
    const float y = r[0] * k.rot[1] + r[1] * k.rot[4] + r[2] * k.rot[7];
    return atan2f(-y, x) * 180.0f / 3.14159265f;
}

static ai::Self g_self;
static bool g_haveSelf = false;
static fl::Kinematics g_kin;
static bool g_haveKin = false;
static u32 g_hitSeq = 0;
static u32 g_laneKey[2] = {}, g_laneFinal = 0; // the lane ride fallback (Slow)
static std::vector<u32> g_laneRings, g_laneEnds[2];
static float g_move[3] = {}; // the ship's last 10 Hz step (server position)
static float g_navRight = 0; // the nav's drawn right edge (last frame)
// the nav's scrim: [0] [1] its text block's drawn right / bottom (last frame), [2] [3] as ComputeLayout eased them
static float g_navScrim[4] = {};
static float g_posSpeed = 0; // the ship's speed from its server position (10 Hz; the lane ride)
static u32 g_objSeq = 0xFFFFFFFF;
static std::vector<u32> g_objRefs;
static u32 g_system = 0;
static bool g_combat = false;
static bool g_letterbox = false; // the game's cinematic bars are up (HUD 2 collapses to the nav chip and the ring)
// the running maneuver's start distance (progress)
static int g_manKind = 0;
static u32 g_manObj = 0;
static float g_manD0 = 0;
// alerts' edges
static float g_prevShield = 1, g_prevHull = 1, g_prevFree = 1;
static int g_prevMissiles = 0;
static double g_spaceSince = 0; // when the ship last entered space (a load, a launch): the alerts arm after it

// hud_prompts (0.2.1): the context prompts shown on a change or an action (core\game\promptshow.h)
static promptshow::State g_promptShow;
static unsigned g_ctxSig = promptshow::kSeed; // the prompt set's signature as the model built it (before the layout's trim)
static bool g_promptAction = false;           // NoteAction since the last Update
static u32 g_promptTarget = 0;                // the target last frame (a switch / clear is an action, by any input)
// hud_fade 1 CONTEXTUAL (0.2.1, core\game\hudwake.h): each element's wake timer, this frame's triggers so far
static hudwake::State g_wake;
static unsigned g_wakeTrig = 0, g_wakeShown = hudwake::All;
void NoteAction(unsigned wake) { g_promptAction = true, g_wakeTrig |= wake; }
static bool g_cursorEligible = false, g_cursorHiddenNote = false; // the idle cursor (CursorIdleEligible)
bool CursorIdleEligible() { return g_cursorEligible; }
void NoteCursorHidden(bool hidden) { g_cursorHiddenNote = hidden; }
static unsigned CtxSig(const Model& m)
{
    unsigned h = promptshow::Mix(promptshow::kSeed, m.nCtx);
    for (int i = 0; i < m.nCtx && i < 3; ++i)
        h = promptshow::Mix(promptshow::Mix(promptshow::Mix(h, static_cast<int>(m.ctx[i].btn)), m.ctx[i].key), m.ctx[i].label);
    return h;
}

static void Alert(const wchar_t* title, u32 col)
{
    if (!uiopt::Get().hudAlerts)
        return;
    g_m.alert = title, g_m.alertCol = col, g_m.alertLeft = 3.0f, g_m.nAlertPrompts = 0, g_m.alertNone.clear();
    g_m.alertSlots = 0;
}
// The maneuver lock's answer (0.2.1; a player report: in M01 the mission's Act_LockManeuvers made B / free flight do
// nothing, which read as a malfunction): the card in the warning amber, glyph-free, 2.5 s - not gated by hud_alerts,
// since it answers the player's own press
bool LockNotice()
{
    if (!Active())
        return false;
    g_m.alert = L"LOCKED BY THE MISSION", g_m.alertCol = kWarn, g_m.alertLeft = 2.5f, g_m.nAlertPrompts = 0;
    g_m.alertNone.clear(), g_m.alertSlots = 0;
    return true;
}

// The alert's answer when it is used up or not fitted: an amber word in the card instead of a glyph (the round 3
// critique: every alert carries its answer or says there is none)
static void AlertNone(const wchar_t* s)
{
    if (g_m.nAlertPrompts == 0 && g_m.alertNone.empty())
        g_m.alertNone = s;
}
static void AlertPrompt(presets::Action a, const wchar_t* label)
{
    if (g_m.nAlertPrompts < 2 && MakePrompt(g_m.alertPrompts[g_m.nAlertPrompts], a, label))
        ++g_m.nAlertPrompts;
}

static void Fast(float dt)
{
    Model& m = g_m;
    g_haveSelf = ai::ReadSelf(g_self);
    m.hull = g_haveSelf ? g_self.hull : -1;
    m.shield = g_haveSelf ? g_self.shield : -1;
    m.power = g_haveSelf ? g_self.power : -1;
    g_haveKin = fl::PlayerKinematics(g_kin);
    m.speed = g_m.man && g_m.manName == L"TRADE LANE" ? g_posSpeed : 0;
    m.speed = g_haveKin ? std::max(m.speed, sqrtf(g_kin.vel[0] * g_kin.vel[0] + g_kin.vel[1] * g_kin.vel[1] + g_kin.vel[2] * g_kin.vel[2])) : m.speed;
    flight::Model fm;
    m.cruise = 0;
    if (flight::ReadModel(fm) && fm.cruiseEngaged)
    {
        const bool charging = fm.cruiseChargeTime > 0 && fm.cruiseChargeElapsed < fm.cruiseChargeTime;
        m.cruise = charging ? 1 : 2;
        m.charge = charging ? fm.cruiseChargeElapsed / fm.cruiseChargeTime : 1;
        m.chargeLeft = std::max(0.0f, fm.cruiseChargeTime - fm.cruiseChargeElapsed);
    }
    m.burner = hudinfo::AfterburnerOn();
    m.kill = hudinfo::EngineKilled();
    m.throttle = fl::GetThrottle();
    m.reverse = m.throttle < -0.01f;
    // a hit's side (the game's own damage indicator input, host v12) - only for real damage: the indicator's inputs also
    // come in a jump's tunnel and from contacts that cost nothing (the round 4 critique: the flash showed in the
    // tunnel). A hit report flashes when the hull or the shield fell within 0.5 s of it (either order), never while
    // the letterbox is up.
    {
        static float lastHull = -1, lastShield = -1;
        static double damageAt = -10, pendingAt = -10;
        static float pendingAng = 0;
        static u32 rejected = 0;
        const double now = perf::Now();
        if ((lastHull >= 0 && m.hull >= 0 && m.hull < lastHull - 1e-5f) ||
            (lastShield >= 0 && m.shield >= 0 && m.shield < lastShield - 1e-5f))
            damageAt = now;
        lastHull = m.hull, lastShield = m.shield;
        const bool cine = exe::LetterboxAmount.ok() && Mem<float>(exe::LetterboxAmount) > 0.0f;
        float hp[3];
        u32 kind = 0, tick = 0;
        const u32 seq = HostPlayerHit(hp, &kind, &tick);
        if (seq && seq != g_hitSeq)
        {
            if (g_hitSeq && g_haveKin && !cine)
                pendingAng = Bearing(hp, g_kin), pendingAt = now;
            else if (g_hitSeq && cine && rejected++ < 5)
                Log("hud2: hit report kind %u in the letterbox: no flash", kind);
            g_hitSeq = seq;
        }
        if (now - pendingAt < 0.5 && fabs(damageAt - pendingAt) < 0.5 && !cine)
            m.hitAng = pendingAng, m.hitAge = 0, pendingAt = -10;
        else if (now - pendingAt >= 0.5 && pendingAt > 0 && rejected++ < 5)
            Log("hud2: hit report without damage (kind %u): no flash", kind), pendingAt = -10;
        else if (m.hitAge >= 0)
            m.hitAge = m.hitAge + dt > 1 ? -1 : m.hitAge + dt;
    }
    if (m.alertLeft > 0)
        m.alertLeft -= dt;
    if (m.placeLeft > 0)
        m.placeLeft -= dt;
    // the dock reply for the readers (hudinfo::Docking's queue)
    u32 ship = 0, rk = 0, resp = 0;
    const u32 dseq = HostDockReply(&ship, &rk, &resp);
    hudinfo::SetDockReply(dseq, rk, resp);
}

// The place line: what the game's own arrival line (NN_ArrivalText, hidden while HUD 2 shows: hudvis::SetHideArrival)
// says, in the nav. The game shows a line at a system arrival / a launch ("New York System, Liberty Space.": the house
// from the game's own table, hudinfo::ArrivalLine) and, with HDE's Zone.dll (Jason Hood's plugin, "Entered %Zv1." /
// "Left %Zv1.", typed through the same control), at every named zone's edge. HUD 2 shows each line 5 s (+ a 1 s fade).
// The events are HUD 2's own readings (the game's control is reused for a line that comes while another runs and keeps
// no readable text): the system line once the ship is in a new system (a jump's arrival, a load, a launch) and the jump
// chip has gone; a zone edge from the 10 Hz zone reading, not within 3 s of a system change (the zone of the system left
// still read for a moment and showed as "Left the Pittsburgh Debris Field" at a New York arrival).
static void Place(Model& m, bool jumping)
{
    static std::wstring zoneWas;
    static double sysAt = -1; // the system changed then: its line is due
    static double sysChangedAt = -10;
    static HMODULE zoneDll = nullptr;
    static DWORD zoneDllAt = 0;
    const double now = perf::Now();
    if (!zoneDll && GetTickCount() - zoneDllAt > 5000)
        zoneDllAt = GetTickCount(), zoneDll = GetModuleHandleA("Zone.dll");
    const u32 sys = fl::PlayerSystem();
    const std::wstring zone = Upper(hudinfo::ZoneName());
    auto the = [](const std::wstring& z) { return z.compare(0, 4, L"THE ") == 0 ? z : L"THE " + z; };
    if (sys != g_system)
    {
        g_system = sys, zoneWas = zone, sysChangedAt = now;
        sysAt = sys ? now : -1;
    }
    else if (zone != zoneWas)
    {
        if (zoneDll && now - sysChangedAt > 3.0 && (!zone.empty() || !zoneWas.empty()))
        {
            m.place = !zone.empty() ? L"ENTERED " + the(zone) + L"." : L"LEFT " + the(zoneWas) + L".";
            m.placeLeft = 5;
        }
        zoneWas = zone;
    }
    if (sysAt >= 0 && (!jumping || now - sysAt > 15))
    {
        const std::wstring line = Upper(hudinfo::ArrivalLine());
        if (!line.empty())
            m.place = line, m.placeLeft = 5;
        sysAt = -1;
    }
    // The standing line under the headline (the mock-up's "NEW YORK - LIBERTY SPACE" with a middle dot; the round 3 critique: no system /
    // zone line): the system and the named zone the ship is in, else the house - both from the game's own arrival line
    // ("New York System, Liberty Space.": the system part without its " System" word, the house without the stop), the
    // system's display name when that line has no comma.
    static std::wstring lineWas, sysShort, house;
    const std::wstring line = jumping ? lineWas : hudinfo::ArrivalLine();
    if (line != lineWas || sysShort.empty())
    {
        lineWas = line;
        const size_t comma = line.find(L", ");
        sysShort = comma == std::wstring::npos ? hudinfo::SystemName() : line.substr(0, comma);
        house = comma == std::wstring::npos ? L"" : line.substr(comma + 2);
        while (!house.empty() && (house.back() == L'.' || house.back() == L' '))
            house.pop_back();
        const std::wstring word = L" System";
        if (sysShort.size() > word.size() && sysShort.compare(sysShort.size() - word.size(), word.size(), word) == 0)
            sysShort.resize(sysShort.size() - word.size());
        while (!sysShort.empty() && sysShort.back() == L'.')
            sysShort.pop_back();
    }
    m.where = Upper(sysShort);
    const std::wstring second = !zone.empty() ? zone : Upper(house);
    if (!m.where.empty() && !second.empty())
        m.where += kDot + second;
}

// The D-pad slots the alerts light (Slow, 10 Hz): nanobots with a critical hull, batteries with a failing shield,
// countermeasures with missiles homing; and every slot the alert card names while the card shows. A lit slot whose
// answer is used up (0) or not fitted (-1) lights amber; a used-up slot stays lit amber while it is empty.
static void LightSlots(Model& m, bool shieldLow)
{
    const int counts[4] = {m.nano, m.batt, m.cm, m.mines};
    const bool cond[4] = {m.hull >= 0 && m.hull <= 0.25f, shieldLow, m.missiles > 0, false};
    static const u32 kMeaning[4] = {kFriend, kShield, kHostile, kChrome};
    const bool card = m.alertLeft > 0 && !m.alert.empty();
    for (int i = 0; i < 4; ++i)
    {
        const bool named = cond[i] || (card && (m.alertSlots & (1 << i)));
        m.hot[i] = named || counts[i] == 0;
        m.hotCol[i] = counts[i] > 0 ? kMeaning[i] : kWarn;
    }
}

// The objective line as the game shows it: the host's copy formatted by the game's own formatter (host v24: values in,
// codes without a handler dropped - vanilla 1.1 writes "Destroy the Hostile Ships%M", 21830); with an older host the
// raw string with every format code dropped the way the formatter drops an unfilled one ('%', a letter, its digits,
// an optional 'v' digit), never shown raw.
static std::wstring ObjectiveLineOf(u32 textId)
{
    if (!textId)
        return L"";
    wchar_t buf[512];
    if (HostObjectiveText(buf, 512))
        return buf;
    const std::wstring raw = text::Ids(textId);
    std::wstring s;
    for (size_t i = 0; i < raw.size(); ++i)
    {
        if (raw[i] != L'%' || i + 1 >= raw.size())
        {
            s += raw[i];
            continue;
        }
        ++i; // the code letter
        while (i + 1 < raw.size() && iswdigit(raw[i + 1]))
            ++i;
        if (i + 2 < raw.size() && raw[i + 1] == L'v' && iswdigit(raw[i + 2]))
            i += 2;
    }
    std::wstring out; // spaces a dropped code left doubled / trailing
    for (wchar_t c : s)
        if (c != L' ' || (!out.empty() && out.back() != L' '))
            out += c;
    while (!out.empty() && out.back() == L' ')
        out.pop_back();
    return out;
}

std::wstring ObjectiveLine(u32 textId) { return ObjectiveLineOf(textId); }

static void Slow()
{
    Model& m = g_m;
    const uiopt::UiOptions& o = uiopt::Get();
    RefreshKeys();
    const std::vector<space::Contact> all = space::Contacts();
    const u32 tid = fl::TargetId();
    // ---- the target
    m.target = false;
    if (!tid) // (the card's texts were kept: `hud2 state` showed the last target's name with target=0 after a load)
        m.tName.clear();
    m.targetId = tid;
    bool tDock = false, tLoot = false, tHostile = false;
    u32 tType = 0;
    float lootRange = g_haveSelf ? g_self.tractorRange : 0;
    int loot = 0;
    bool hostileNear = false;
    m.nThreats = 0;
    m.enemies = 0, m.tLootReach = false;
    m.lootName.clear(), m.lootDist.clear();
    float nearestLoot = 0;
    for (const space::Contact& c : all)
    {
        const bool hostile = c.ship && c.attitudeAsked && c.attitude <= aim::kHostileAttitude;
        if (hostile && c.distance < 4000 && (c.targetsPlayer || c.distance < 2000))
            hostileNear = true;
        if (ctl::NearbyEnemy(c))
            ++m.enemies;
        // (loot the hold cannot take - CShip::get_space_for_cargo_type 0 - is not offered: nothing would come in)
        if ((c.type & space::kLoot) && lootRange > 0 && c.distance <= lootRange && c.lootFits != 0)
        {
            ++loot;
            if (c.id == tid)
                m.tLootReach = true;
            if (m.lootName.empty() || c.distance < nearestLoot)
            {
                nearestLoot = c.distance;
                m.lootName = hudinfo::DisplayName(c.id);
                if (m.lootName.empty())
                    m.lootName = text::Ids(c.nameIds);
                m.lootDist = Dist(c.distance);
            }
        }
        if (hostile && c.id != tid && m.nThreats < 2 && c.distance < 10000)
        {
            Model::Threat& t = m.threats[m.nThreats++];
            t.name = hudinfo::DisplayName(c.id); // as the game names it ("Unknown ..." for an unvisited solar)
            t.dist = Dist(c.distance);
            t.col = kHostile;
            t.id = c.id;
            t.icon = flicons::Enemy;
        }
        if (!tid || c.id != tid)
            continue;
        m.target = true;
        tType = c.type;
        tDock = c.dock || (c.type & (space::kTradelaneRing | space::kJumpGate | space::kJumpHole | space::kDockingRing));
        tLoot = (c.type & space::kLoot) != 0;
        float att = c.attitude;
        bool known = c.attitudeAsked;
        if (!c.ship)
            known = space::AttitudeToPlayer(c.id, att);
        tHostile = known && att <= aim::kHostileAttitude;
        m.tCol = !known ? kNeutral : tHostile ? kHostile : att >= -aim::kHostileAttitude ? kFriend : kNeutral;
        m.tAtt = !known ? L"NEUTRAL" : tHostile ? L"HOSTILE" : att >= -aim::kHostileAttitude ? L"FRIENDLY" : L"NEUTRAL";
        // the name exactly as the game's target panel shows it: an unvisited gate is "Unknown Jump Gate" (no knowledge leak)
        m.tName = hudinfo::DisplayName(c.id);
        if (m.tName.empty() && !c.ship)
            m.tName = text::Ids(c.nameIds);
        m.tDist = Dist(c.distance);
        // (another player's ship too: the game's own 3D target label reads "Navy - FLvibedTester", its affiliation)
        m.tFaction = hudinfo::FactionName(c.id);
        m.tClass = hudinfo::ClassName(c.id);
        if (m.tClass.empty())
            m.tClass = c.ship ? L"SHIP"
                       : (c.type & space::kPlanet)         ? L"PLANET"
                       : (c.type & space::kTradelaneRing)  ? L"TRADE LANE"
                       : (c.type & space::kJumpGate)       ? L"JUMP GATE"
                       : (c.type & space::kJumpHole)       ? L"JUMP HOLE"
                       : (c.type & space::kLoot)           ? L"LOOT"
                       : (c.type & (space::kStation | space::kDockingRing)) ? L"STATION"
                                                            : L"";
        m.tLevel = c.ship ? hudinfo::Level(c.id) : -1;
        m.tViewOk = c.ship || (c.type & (space::kStation | space::kDockingRing | space::kPlanet | space::kJumpGate |
                                         space::kJumpHole)) != 0;
        m.tHull = c.health;
        float cur = 0, mx = 0;
        bool up = false;
        m.tShield = space::ShieldOf(c.id, cur, mx, up) && mx > 0 ? (up ? cur / mx : 0) : -1;
        m.tBars = c.ship || (c.health >= 0 && c.health < 0.999f && !(c.type & space::kPlanet));
        const std::wstring cls = m.tClass;
        m.tIcon = c.ship ? (tHostile ? flicons::Enemy
                                     : (cls.find(L"FREIGHT") != std::wstring::npos || cls.find(L"TRANSPORT") != std::wstring::npos)
                                           ? flicons::Transport
                                           : flicons::Fighter)
                  : (c.type & space::kPlanet)                          ? flicons::Planet
                  : (c.type & space::kTradelaneRing)                   ? flicons::Lane
                  : (c.type & (space::kJumpGate | space::kJumpHole))   ? flicons::JumpGate
                  : (c.type & space::kLoot)                            ? flicons::Loot
                  : (c.type & (space::kStation | space::kDockingRing)) ? flicons::Station
                                                                       : flicons::Waypoint;
    }
    if (!m.target)
        m.targetId = 0;
    g_combat = tHostile || hostileNear;
    m.fight = g_combat;
    m.loot = loot; // (was the game's own count, hudinfo::LootInReach: it counts what the hold cannot take)
    m.filter = L"HOSTILES";
    MakePrompt(m.threatsBtn, presets::Action::TargetPicker, nullptr);
    if (!MakePrompt(m.nextBtn, presets::Action::TargetSmart, nullptr))
        MakePrompt(m.nextBtn, presets::Action::NextEnemy, nullptr);
    if (!MakePrompt(m.nearestBtn, presets::Action::TargetSmart, nullptr))
        MakePrompt(m.nearestBtn, presets::Action::TargetNearest, nullptr);
    // the contact list mode replaces the threats (hud2_click.cpp): the keyboard form's header click, or the pad's action
    // wheel (CONTACT FILTER, CycleListFilter below); out of combat no threat rows
    if (g_L.H > 0) // the rows that fit between the list's header and the loadout (the last frame's layout) - its cross
    {              // stacked on its right end, under the card's column, since the owner's decision 3 (31 Oct 2026)
        const float H = g_L.H, k = g_L.k;
        const float lo = g_L.consY1 > g_L.consY0 && g_L.consX0 < g_L.tgtX1 && g_L.consX1 > g_L.tgtX0
                             ? std::min(g_L.loY0, g_L.consY0)
                             : g_L.loY0;
        const float room = lo - 0.02f * H - (g_L.thrY0 + (0.034f + 0.022f + 0.022f + 0.03f) * H * k);
        m.listMax = std::clamp(static_cast<int>(room / (0.042f * H * k)), 3, 10);
    }
    if (!FillList(m, all, tid) && m.kbm && !g_combat)
        m.nThreats = 0;
    if (m.kbm)
    {
        auto key = [](Prompt& p, int cmd)
        {
            p = Prompt();
            p.key = KeyOf(cmd);
        };
        key(m.manBtn[0], fl::USER_MANEUVER_FREE_FLIGHT);
        key(m.manBtn[1], fl::USER_MANEUVER_GOTO);
        key(m.manBtn[2], fl::USER_MANEUVER_DOCK);
        key(m.manBtn[3], fl::USER_MANEUVER_FORMATION);
        m.manBtn[4] = Prompt();
        m.manBtn[4].key = AutopilotKey();
        key(m.clearBtn, fl::USER_CLEAR_TARGET);
        key(m.nextEnemyBtn, fl::USER_NEXT_ENEMY);
        for (int g = 0; g < 3; ++g) // (the rows' group numbers: the last 10 Hz pass's)
            key(m.groupBtn[g], fl::USER_WEAPON_GROUP1 + std::clamp(m.groupNum[g], 1, 6) - 1);
    }

    // ---- the ship panel
    m.nEscorts = 0;
    hudinfo::Formation form;
    const u32 me = space::PlayerShipId();
    // The escorts by call sign in a fight / formation (else the hold and credits): the formation's members first, then
    // the friendly ships the mission marks or names in its objective (a convoy, a wingman), then in a fight the
    // friendly ships fighting beside the player (nearest, within 3 km) - each tier by distance, three rows.
    u32 mission[32];
    const int nMission = std::max(0, autopilot::MissionObjectIds(mission, 32, all));
    auto friendly = [](const space::Contact& c)
    { return c.ship && c.attitudeAsked && c.attitude >= -aim::kHostileAttitude; };
    auto addEscort = [&](const space::Contact& c)
    {
        for (int i = 0; i < m.nEscorts; ++i)
            if (m.escortIds[i] == c.id)
                return;
        if (m.nEscorts < 3 && c.id != me)
            m.escortIds[m.nEscorts] = c.id,
            m.escorts[m.nEscorts++] = {hudinfo::DisplayName(c.id), std::max(0.0f, c.health)};
    };
    const bool haveForm = hudinfo::PlayerFormation(form);
    {
        static std::vector<const space::Contact*> byDist;
        byDist.clear();
        for (const space::Contact& c : all)
            if (c.ship && c.id != me)
                byDist.push_back(&c);
        std::sort(byDist.begin(), byDist.end(), [](const space::Contact* a, const space::Contact* b) { return a->distance < b->distance; });
        if (haveForm)
            for (const space::Contact* c : byDist)
                if (std::find(form.members.begin(), form.members.end(), c->id) != form.members.end())
                    addEscort(*c);
        if (g_combat || form.leader)
        {
            for (const space::Contact* c : byDist)
                if (friendly(*c) && std::find(mission, mission + nMission, c->id) != mission + nMission)
                    addEscort(*c);
            if (g_combat)
                for (const space::Contact* c : byDist)
                    if (friendly(*c) && c->distance < 3000)
                        addEscort(*c);
        }
    }
    if (!g_combat && !form.leader)
        m.nEscorts = 0;
    if (g_haveSelf && g_self.holdSize > 0 && g_self.holdFree >= 0)
        m.holdUsed = static_cast<int>(g_self.holdSize - g_self.holdFree + 0.5f), m.holdCap = static_cast<int>(g_self.holdSize + 0.5f);
    else
        m.holdUsed = -1;
    m.credits = hudinfo::Credits();
    m.shipName = g_haveSelf ? shipname::Name(g_self.shipArch) : L"";
    m.shipClass = me ? hudinfo::ClassName(me) : L"";

    // ---- the loadout
    m.nano = g_haveSelf ? g_self.nanobots : -1;
    m.batt = g_haveSelf ? g_self.batteries : -1;
    m.cm = space::PlayerCountermeasures();
    m.mines = space::PlayerMines();
    MakePrompt(m.slotBtn[0], presets::Action::Nanobot, nullptr);
    MakePrompt(m.slotBtn[1], presets::Action::Battery, nullptr);
    MakePrompt(m.slotBtn[2], presets::Action::Cm, nullptr);
    MakePrompt(m.slotBtn[3], presets::Action::Mine, nullptr);
    MakePrompt(m.fireBtn, presets::Action::Fire, nullptr);
    if (!MakePrompt(m.launchBtn, presets::Action::Missile, nullptr))
        MakePrompt(m.launchBtn, presets::Action::Torpedo, nullptr);
    m.nGroups = 0;
    // the torpedo hardpoints' launcher has a row of its own and is not one of the missiles: its rounds were added to the
    // launcher's count under the missile's name (GitHub issue 5, 4 Nov 2026). Those hardpoints also take cruise
    // disruptors: a torpedo wins the row, a disruptor shows when alone, with its own button (the wheel's: none on the row)
    m.torpedo.clear();
    m.torpedoAmmo = -1;
    bool torpedoIsRound = false;
    for (const space::Weapon& w : space::PlayerWeapons())
    {
        if (!w.homing || _strnicmp(w.hardpoint, "HpTorpedo", 9) != 0 || (torpedoIsRound && w.cruiseDisruptor))
            continue;
        if (!w.cruiseDisruptor && !torpedoIsRound)
            m.torpedo.clear(), m.torpedoAmmo = -1, torpedoIsRound = true;
        if (m.torpedo.empty())
        {
            m.torpedo = Upper(hudinfo::EquipName(w.ammoArch));
            if (m.torpedo.empty())
                m.torpedo = w.cruiseDisruptor ? L"CRUISE DISRUPTOR" : L"TORPEDOES";
        }
        m.torpedoAmmo = std::max(0, m.torpedoAmmo) + std::max(0, w.ammo);
    }
    if (!m.torpedo.empty())
        TorpedoPrompt(m, torpedoIsRound ? presets::Action::Torpedo : presets::Action::CruiseDisruptor);
    // the lines above the launcher row (three): the torpedo row takes one
    const int groupRows = m.torpedo.empty() ? 3 : 2;
    const std::vector<hudinfo::WeaponInfo> wi = hudinfo::Weapons();
    // a row's guns: each kind once in mount order, identical guns as one entry with their count ("Justice Mk I \u00D72";
    // fix-text 25 Oct 2026: the vanilla run showed one "Justice Mk I" for two, and the old de-duplication by substring
    // also swallowed a "Mk I" into a row holding a "Mk II")
    using Guns = std::vector<std::pair<std::wstring, int>>;
    auto addName = [](Guns& v, const std::wstring& n)
    {
        if (n.empty())
            return;
        for (auto& e : v)
            if (e.first == n)
            {
                ++e.second;
                return;
            }
        v.push_back({n, 1});
    };
    auto joined = [](const Guns& v)
    {
        std::wstring s;
        for (const auto& e : v)
        {
            s += (s.empty() ? L"" : L"  \u00B7  ") + e.first; // (the separator the weapon rows wrap at)
            if (e.second > 1)
                s += L" \u00D7" + std::to_wstring(e.second);
        }
        return s;
    };
    bool anyGroups = false;
    for (const hudinfo::WeaponInfo& w : wi)
        anyGroups |= !w.launcher && w.groups != 0;
    m.launcher.clear();
    m.ammo = -1;
    m.realGroups = anyGroups;
    m.ungrouped.clear();
    const int ag = hudinfo::ActiveWeaponGroup();
    const int activeNum = ag > 0 ? ag : g_ctx.weaponGroup;
    if (anyGroups)
    {
        // the player has guns in groups: a row per group that has guns (its own number; no "-" placeholder rows - the
        // round 3 critique), the active group's row kept when more than three have guns; the guns in no group in a
        // dimmed row under them (no "(no group)" label: they fire as the game toggles them)
        std::wstring names[6];
        for (int g = 0; g < 6; ++g)
        {
            Guns row;
            for (const hudinfo::WeaponInfo& w : wi)
                if (!w.launcher && (w.groups & (1u << g)))
                    addName(row, w.name.empty() ? std::wstring(L"GUN") : w.name);
            names[g] = joined(row);
        }
        int nums[6], n = 0;
        for (int g = 0; g < 6; ++g)
            if (!names[g].empty())
                nums[n++] = g + 1;
        bool activeIn = false;
        for (int i = 0; i < n && i < groupRows; ++i)
            activeIn |= nums[i] == activeNum;
        if (n > groupRows && !activeIn && activeNum >= 1 && activeNum <= 6 && !names[activeNum - 1].empty())
            nums[groupRows - 1] = activeNum;
        m.nGroups = std::min(n, groupRows);
        for (int i = 0; i < m.nGroups; ++i)
            m.groups[i] = names[nums[i] - 1], m.groupNum[i] = nums[i];
        Guns loose;
        for (const hudinfo::WeaponInfo& w : wi)
            if (!w.launcher && w.groups == 0)
                addName(loose, w.name.empty() ? std::wstring(L"GUN") : w.name);
        m.ungrouped = joined(loose);
        if (m.nGroups >= groupRows)
            m.ungrouped.clear(); // three rows are all the panel holds (the guns in no group are rarer than a third group)
    }
    // the guns and launchers as the game mounts them (names from hudinfo when it has them)
    Guns guns;
    int nGuns = 0;
    for (const space::Weapon& w : space::PlayerWeapons())
    {
        if (w.homing)
        {
            if (_strnicmp(w.hardpoint, "HpTorpedo", 9) == 0)
                continue; // (the torpedo row above)
            if (w.ammo >= 0)
                m.ammo = std::max(0, m.ammo) + w.ammo;
            if (m.launcher.empty())
            {
                m.launcher = Upper(hudinfo::EquipName(w.ammoArch));
                if (m.launcher.empty())
                    m.launcher = L"MISSILES";
            }
        }
        else
        {
            ++nGuns;
            addName(guns, hudinfo::EquipName(w.archId));
        }
    }
    if (!anyGroups && nGuns)
    {
        m.groups[0] = guns.empty() ? std::to_wstring(nGuns) + (nGuns == 1 ? L" GUN" : L" GUNS") : joined(guns);
        m.groupNum[0] = 1;
        m.nGroups = 1;
    }
    m.activeGroup = anyGroups ? -1 : 0; // the row of the active group (none of the rows: none lit)
    for (int i = 0; anyGroups && i < m.nGroups; ++i)
        if (m.groupNum[i] == activeNum)
            m.activeGroup = i;
    m.locked = m.target && m.tBars && hudinfo::MissileLock() >= 1.0f; // a lock is on a ship (the reader reads 1 on a station too)

    // ---- missiles homing on the player
    m.missiles = 0;
    if (g_haveKin)
        for (const space::Missile& mi : space::Missiles())
            if (mi.targetsPlayer && m.missiles < 3)
                m.missileAng[m.missiles++] = Bearing(mi.pos, g_kin);

    // ---- the objective
    ObjectiveStep st = {};
    const u32 oseq = HostObjectiveNow(&st);
    // the text read at every pass, not only on a new sequence (a load restores the objective with the same sequence the
    // last session ended on: the headline stayed empty after loading a mission save, 25 Oct 2026)
    {
        static u32 textWas = 0xFFFFFFFF;
        if (oseq != g_objSeq || st.textId != textWas)
        {
            g_objSeq = oseq, textWas = st.textId;
            m.objective = ObjectiveLineOf(st.textId);
            g_objRefs.assign(st.refs, st.refs + std::clamp(st.refCount, 0, 4));
        }
    }
    // the protected ship's hull beside the headline: the friendly ship the objective names (its refs), else the friendly
    // ships the mission marks (a convoy: the weakest one's hull)
    m.protect = -1;
    m.tMission = false;
    for (u32 ref : g_objRefs)
    {
        const u32 obj = space::ObjectiveObject(ref, all);
        if (obj && m.target && obj == m.targetId) // the objective names the target (Focus's lane rule)
            m.tMission = true;
        for (const space::Contact& c : all)
            if (obj && c.id == obj && friendly(c))
                m.protect = std::max(0.0f, c.health);
    }
    if (m.protect < 0 && !m.objective.empty())
    {
        u32 marks[32]; // the mission's objects as the autopilot's objective run reads them (marks + objective refs)
        const int nMarks = autopilot::MissionObjectIds(marks, 32, all);
        for (const space::Contact& c : all)
            if (friendly(c) && c.id != me && std::find(marks, marks + std::max(0, nMarks), c.id) != marks + std::max(0, nMarks))
                m.protect = m.protect < 0 ? std::max(0.0f, c.health) : std::min(m.protect, std::max(0.0f, c.health));
    }

    // ---- the maneuver
    space::Pilot p;
    const bool havePilot = space::ReadPilot(p);
    int kind = 0; // 1 goto 2 dock 3 formation 4 lane 5 jump 6 cruise charge
    hudinfo::LaneRun lane;
    hudinfo::Jump jump;
    // A jump is a dock with the gate (docs\FREELANCER_RE.md "HUD 2 readers"): its sequence, tunnel and jump-in are
    // hudinfo::Jumping; the approach to the gate (requested / queued / granted) shows as JUMP toward the gate's system
    // too, and the far gate's dock tail after the jump-in (Docking() false) shows nothing - never "DOCK" at an arrival.
    hudinfo::Dock dock;
    const bool docking = hudinfo::Docking(dock);
    const bool jumping = hudinfo::Jumping(jump) || (havePilot && p.jumping);
    bool gateDock = false;
    if (jumping)
        kind = 5;
    else if (havePilot && p.lane)
        kind = 4;
    else if (havePilot && p.op == 7 && docking)
    {
        jump.toward = hudinfo::JumpToward(tid);
        gateDock = !jump.toward.empty();
        kind = gateDock ? 5 : 2;
    }
    else if (havePilot && p.op == 2)
        kind = 1;
    else if (havePilot && p.op == 10)
        kind = 3;
    else if (m.cruise == 1)
        kind = 6;
    // the jump's parts follow each other with a gap of a few frames (the effect times run out a moment before the
    // jump-in reads is_jumping: the chip blinked off for ~1 s at 0:00, 25 Oct 2026): held 2 s over such a gap
    {
        static double jumpAt = -10;
        static std::wstring jumpToward;
        const double now = perf::Now();
        if (kind == 5)
            jumpAt = now, jumpToward = jump.toward;
        else if (kind == 0 && now - jumpAt < 2.0)
            kind = 5, jump.toward = jumpToward, jump.secondsLeft = 0;
    }
    m.man = kind != 0;
    // the keyboard form's maneuver row: free flight when none runs (cruise charging included), none on a lane / jump
    m.manRun = kind == 0 || kind == 6 ? 0 : kind <= 3 ? kind : -1;
    m.progress = -1;
    m.eta.clear();
    m.manDest.clear();
    u32 obj = 0;
    switch (kind)
    {
    case 1: m.manName = L"GOTO", m.manIcon = flicons::Goto, m.manCol = kActive, obj = tid; break;
    case 2: m.manName = L"DOCK", m.manIcon = flicons::Dock, m.manCol = kActive, obj = tid; break;
    case 3: m.manName = L"FORMATION", m.manIcon = flicons::Formation, m.manCol = kActive, obj = p.leader; break;
    case 4: m.manName = L"TRADE LANE", m.manIcon = flicons::Lane, m.manCol = kActive, obj = p.laneExit; break;
    case 5: m.manName = L"JUMP", m.manIcon = flicons::JumpGate, m.manCol = kCyan, obj = gateDock ? tid : 0; break;
    case 6: m.manName = L"CRUISE CHARGING", m.manIcon = flicons::Cruise, m.manCol = kCyan; break;
    default: m.manName.clear(); break; // (was kept: `hud2 state` showed "TRADE LANE" long after the ride)
    }
    if (kind == 1 || kind == 2)
        m.manDest = Upper(m.tName);
    // a dock with a trade lane ring is the way onto the lane: LANE and the lane's far end (the known bug "DOCK TRADE
    // LANE RING"); the far end is what the game's own lane name puts after its arrow
    if (kind == 2 && tid && (tType & space::kTradelaneRing))
    {
        m.manName = L"LANE", m.manIcon = flicons::Lane;
        const std::wstring ln = hudinfo::LaneName(tid);
        const size_t arrow = ln.find(L"->");
        if (arrow != std::wstring::npos)
            m.manDest = Upper(ln.substr(arrow + 2));
    }
    else if (kind == 3 && p.leader)
        m.manDest = Upper(space::SpeakerName(p.leader));
    // the ship's position as the server keeps it (on a lane the client's kinematics stand still: 2 m/s read on a
    // ride) and the speed from its change at this 10 Hz rate
    float me3[3] = {};
    const bool haveMe = space::ObjectPos(me, me3) || (g_haveKin && (memcpy(me3, g_kin.pos, sizeof(me3)), true));
    {
        static float last[3] = {};
        static double lastT = 0;
        const double now = perf::Now();
        if (haveMe && lastT > 0 && now - lastT > 0.02)
        {
            const float dx = me3[0] - last[0], dy = me3[1] - last[1], dz = me3[2] - last[2];
            g_posSpeed = sqrtf(dx * dx + dy * dy + dz * dz) / static_cast<float>(now - lastT);
        }
        if (haveMe)
        {
            for (int i = 0; i < 3; ++i)
                g_move[i] = me3[i] - last[i];
            memcpy(last, me3, sizeof(last)), lastT = now;
        }
    }
    // A lane ride (until the readers have the lane run, hudinfo::Lane): the whole lane from the game's own links (the
    // system file's next_ring / prev_ring) walked both ways from the ride's entry ring to both ends; the ride ends at
    // the end the ship moves toward (its server position's change: on a lane the entry stays, the exit field names a
    // leg and was seen equal to the entry at the ride's start). Cached per entry ring.
    const bool laneRead = kind == 4 && hudinfo::Lane(lane);
    if (kind == 4 && !laneRead && p.laneEntry && haveMe)
    {
        if (g_laneKey[0] != p.laneEntry)
        {
            g_laneKey[0] = p.laneEntry, g_laneFinal = 0;
            for (int dir = 0; dir < 2; ++dir)
            {
                g_laneEnds[dir].assign(1, p.laneEntry);
                u32 r = p.laneEntry;
                for (int guard = 0; guard < 64; ++guard)
                {
                    u32 next = 0, prev = 0;
                    if (!space::LaneLinks(r, next, prev))
                        break;
                    const u32 step = dir ? next : prev;
                    if (!step)
                        break;
                    r = step;
                    g_laneEnds[dir].push_back(r);
                }
            }
        }
        // which way: the end the last 10 Hz step moved toward
        int best = -1;
        float bestDot = 0;
        for (int dir = 0; dir < 2; ++dir)
        {
            float ep[3];
            if (g_laneEnds[dir].size() < 2 || !space::ObjectPos(g_laneEnds[dir].back(), ep))
                continue;
            const float dot = (ep[0] - me3[0]) * g_move[0] + (ep[1] - me3[1]) * g_move[1] + (ep[2] - me3[2]) * g_move[2];
            if (dot > bestDot)
                bestDot = dot, best = dir;
        }
        if (best >= 0)
        {
            g_laneFinal = g_laneEnds[best].back();
            g_laneRings = g_laneEnds[best];
        }
        if (g_laneFinal)
            obj = g_laneFinal;
    }
    u32 laneFinal = g_laneFinal;
    std::vector<u32>& laneRings = g_laneRings;
    if (obj && haveMe)
    {
        float pos[3];
        if (space::ObjectPos(obj, pos))
        {
            const float dx = pos[0] - me3[0], dy = pos[1] - me3[1], dz = pos[2] - me3[2];
            const float d = sqrtf(dx * dx + dy * dy + dz * dz);
            if (kind != g_manKind || obj != g_manObj || d > g_manD0)
                g_manKind = kind, g_manObj = obj, g_manD0 = std::max(d, 1.0f);
            m.progress = Clampf(1 - d / g_manD0, 0, 1);
            const float v = kind == 4 ? std::max(m.speed, g_posSpeed) : m.speed;
            const float eta = v > 5 ? d / v : -1;
            m.eta = Clock(eta);
            if (kind == 4 && !laneRead && obj == laneFinal) // the rings still ahead: nearer the end than the ship
            {
                int n = 0;
                for (u32 r : laneRings)
                {
                    float rp[3];
                    if (space::ObjectPos(r, rp) &&
                        sqrtf((pos[0] - rp[0]) * (pos[0] - rp[0]) + (pos[1] - rp[1]) * (pos[1] - rp[1]) +
                              (pos[2] - rp[2]) * (pos[2] - rp[2])) < d)
                        ++n;
                }
                lane.ringsLeft = n + 1; // the last ring itself
            }
        }
    }
    else
        g_manKind = kind, g_manObj = 0;
    if (kind == 5 && gateDock) // the approach to the gate: the distance's progress (an ordinary dock's), no countdown yet
        jump.secondsLeft = -1;
    if (kind == 4 && lane.ringsLeft >= 0 && !laneRead)
    {
        wchar_t b[64];
        swprintf_s(b, lane.ringsLeft == 1 ? L"%d RING" : L"%d RINGS", lane.ringsLeft);
        m.eta = b + (m.eta.empty() ? std::wstring() : kDot + m.eta);
    }
    if (laneRead)
    {
        // the bar fills over the whole ride (the reader's progress along the lane from the entry ring); the distance to
        // the exit ring restarted at every ring and read as an empty bar
        m.progress = lane.progress >= 0 ? lane.progress : -1;
        wchar_t b[64];
        if (lane.ringsLeft >= 0)
        {
            swprintf_s(b, lane.ringsLeft == 1 ? L"%d RING" : L"%d RINGS", lane.ringsLeft);
            m.eta = b + (lane.eta >= 0 ? kDot + Clock(lane.eta) : (m.eta.empty() ? L"" : kDot + m.eta));
        }
        if (!lane.toward.empty())
            m.manDest = Upper(lane.toward);
    }
    if (kind == 5)
    {
        m.manDest = Upper(jump.toward);
        if (jump.secondsLeft >= 0)
            m.eta = Clock(jump.secondsLeft);
    }
    if (kind == 6)
        m.manDest.clear();
    // the autopilot
    m.ap = g_ctx.autopilot;
    m.apMode.clear();
    m.wait.clear();
    if (m.ap)
    {
        const autopilot::Mode am = autopilot::Current();
        if (am != autopilot::Mode::Off)
        {
            m.apMode = ModeVerb(am);
            if (autopilot::g.target)
            {
                const std::wstring n = Upper(hudinfo::DisplayName(autopilot::g.target));
                if (!n.empty())
                    m.apMode += L" " + n;
            }
        }
        else if (autopilot::g_obj.on)
            m.apMode = L"MISSION";
        if (const wchar_t* why = WaitReason())
            m.wait = why;
    }
    m.turret = g_ctx.turret;

    // ---- contextual information (hud_info)
    m.info.clear();
    m.event.clear();
    m.timer.clear();
    if (o.hudInfo)
    {
        const hudinfo::Dock& dk = dock;
        if (docking && dk.state > 0)
        {
            static const wchar_t* kDock[] = {L"", L"DOCK REQUESTED", L"DOCK QUEUE", L"DOCKING GRANTED", L"DOCKING"};
            static const wchar_t* kGate[] = {L"", L"JUMP REQUESTED", L"JUMP QUEUE", L"JUMP GRANTED", L"JUMPING"};
            static const wchar_t* kLane[] = {L"", L"LANE REQUESTED", L"LANE QUEUE", L"LANE GRANTED", L"ENTERING LANE"};
            const bool laneDock = !gateDock && (tType & space::kTradelaneRing);
            m.event = (gateDock ? kGate : laneDock ? kLane : kDock)[std::clamp(dk.state, 0, 4)];
            if (dk.state == 2 && dk.queue >= 0)
                m.event += L" " + std::to_wstring(dk.queue);
            if (dk.bay >= 0)
                m.event += kDot + std::wstring(L"BAY ") + std::to_wstring(dk.bay);
        }
        const float timer = hudinfo::MissionTimer();
        if (timer >= 0)
        {
            m.timer = std::wstring(L"TIME ") + Clock(timer); // (first in info: the lean nav takes it out there)
            m.info += (m.info.empty() ? L"" : kDot) + m.timer;
        }
        if (hudinfo::TractorActive())
            m.info += (m.info.empty() ? L"" : kDot) + std::wstring(L"TRACTOR ACTIVE");
        if (kind != 3 && form.leader && form.leader != me)
            m.info += (m.info.empty() ? L"" : kDot) + std::wstring(L"FORMATION ") + Upper(space::SpeakerName(form.leader));
    }
    Place(m, kind == 5);
    MakePrompt(m.neuro, presets::Action::Hub, nullptr);
    // the tractor's glyph for Focus's band (LOOT n); Standard's tractor is the right lane's TRACTOR n (TractorCtx, 5.23)
    MakePrompt(m.tractorBtn, presets::Action::Tractor, nullptr);
    MakePrompt(m.pickerBtn, presets::Action::TargetPicker, nullptr);
    MakePrompt(m.wheelBtn, presets::Action::ActionWheel, nullptr);
    if (!m.kbm) // the pad's clear-target button for the card's top row (the keyboard form's key is set with the card)
        MakePrompt(m.clearBtn, presets::Action::ClearTarget, nullptr);

    // ---- the state
    m.state = g_ctx.turret        ? State::Turret
              : g_ctx.autopilot   ? State::Autopilot
              : g_combat          ? State::Combat
              : m.cruise == 1     ? State::CruiseCharge
              : (kind && kind != 6) ? State::Maneuver
                                  : State::Calm;

    // ---- the context line (at most three; never one a panel shows; contextual). Not while the pad drives one of
    // FLUF's windows (flufaware::OwnsPad): its buttons mean CLICK / CLOSE there, the prompt bar says so (A3, 26 Oct 2026:
    // "B REVERSE" showed under FLUF's nav map while B closed it)
    m.nCtx = 0;
    using A = presets::Action;
    // The cruise prompt lives in this line, in the left lane (CtxSide: the ship's movement) - the teaching lane at the
    // screen's bottom edge (UI_HUD2_PRESETS.md 1.6: the context line holds the teaching glyphs, "CRUISE in calm" among
    // them) - in every preset: Focus's left its band (the owner, 31 Oct 2026, B13: "the cruise prompt itself can live at
    // the edge of the screen rather than at the center. It's crowded."), Standard's left its reticle lane above the
    // ship's nose the same day ("Yes, move it"; UI_HUD2.md 5.21): [Y] CRUISE in calm after an actionable target's DOCK /
    // TRACTOR, [Y] CANCEL CRUISE first while it charges; Focus's band / Standard's ring interior keep the state without a
    // glyph (Standard: the charge seconds, then CRUISE ON in the same slot under the speed - nothing under the ring, 5.23).
    if (o.hudContext == 0 && !flufaware::OwnsPad())
    {
        switch (m.state)
        {
        case State::Turret: AddCtx(A::Turret, L"EXIT TURRET"); break;
        case State::Autopilot:
            AddCtx(A::Autopilot, L"TAKE CONTROL"); // a verb (the round 4 critique: "AUTOPILOT OFF" read as a state)
            AddCtx(A::GoalPanel, L"GOALS");
            break;
        case State::Combat:
            AddCtx(A::Reverse, L"REVERSE");
            AddCtx(A::Turret, L"TURRET");
            break;
        case State::CruiseCharge: // (the cancel first: one prompt, one place - the ring / band show only the state word)
            AddCtx(A::Cruise, L"CANCEL CRUISE");
            if (tDock)
                AddCtx(A::Context, L"DOCK", true);
            break;
        case State::Maneuver:
            if (kind == 4 || kind == 5)
                AddCtx(A::Hub, L"MAP");
            else
                AddCtx(A::FreeFlight, kind == 2 ? (m.manName == L"LANE" ? L"ABORT LANE" : L"ABORT DOCK") : kind == 3 ? L"LEAVE FORMATION" : L"STOP");
            break;
        case State::Calm:
            if (m.cruise == 2)
                AddCtx(A::FreeFlight, L"DROP CRUISE");
            if (m.target && tDock)
                AddCtx(A::Context, L"DOCK", true);
            TractorCtx(true); // (loot in reach: before the teaching prompts, as its ring spot had precedence)
            if (m.cruise == 0) // (the cruise prompt, above)
                AddCtx(A::Cruise, L"CRUISE");
            if (m.target && !tDock && !m.tLootReach) // (loot out of reach: A flies to it)
                AddCtx(A::Context, L"GO TO");
            else if (!m.target) // (RS: enemies first, else what is ahead - controller.cpp SmartTarget)
                AddCtx(A::TargetSmart, m.enemies > 0 ? L"NEAREST ENEMY" : L"TARGET AHEAD");
            if (m.kbm) // (the pad's LB TARGETS / WHEEL sit under the reticle ring in calm flight)
                AddCtx(A::TargetPicker, L"TARGETS");
            break;
        }
        // the tractor's prompts in every other state (calm placed them above), after the state's own prompts - they left
        // their spot under the reticle ring on 31 Oct 2026 (5.23); A's one-item form only where A is the context's (a
        // fight: B / X are REVERSE / TURRET); idempotent
        TractorCtx(m.state == State::Combat);
    }

    // ---- alerts (edges) and the slots they light
    const float shield = m.shield < 0 ? 0 : m.shield;
    // armed 1.5 s after entering space, and only on a live hull: a load read the hull / shield as 0 for a moment and
    // fired HULL CRITICAL - NO NANOBOTS on a whole ship (FIX2 release captures); unarmed, the edges just follow the values
    if (perf::Now() - g_spaceSince < 1.5 || m.hull <= 0)
    {
        if (g_haveSelf && g_self.shieldMax > 0)
            g_prevShield = shield;
        g_prevHull = m.hull >= 0 ? m.hull : 1;
        g_prevMissiles = m.missiles;
        g_prevFree = g_haveSelf ? g_self.holdFree : 1;
    }
    if (g_haveSelf && g_self.shieldMax > 0 && shield <= 0.25f && g_prevShield > 0.25f)
    {
        Alert(L"SHIELD FAILING", kHostile);
        m.alertSlots |= 2;
        if (m.batt > 0)
            AlertPrompt(A::Battery, L"BATTERY");
        if (m.missiles > 0 && m.cm > 0)
            AlertPrompt(A::Cm, L"FLARE"), m.alertSlots |= 4;
        AlertNone(L"NO BATTERIES \x00B7 BREAK OFF");
    }
    if (m.hull >= 0 && m.hull <= 0.25f && g_prevHull > 0.25f)
    {
        Alert(L"HULL CRITICAL", kHostile);
        m.alertSlots |= 1;
        if (m.nano > 0)
            AlertPrompt(A::Nanobot, L"NANOBOTS");
        AlertNone(L"NO NANOBOTS \x00B7 BREAK OFF");
    }
    if (m.missiles > g_prevMissiles)
    {
        Alert(L"MISSILE INCOMING", kHostile);
        m.alertSlots |= 4;
        if (m.cm > 0)
            AlertPrompt(A::Cm, L"FLARE");
        AlertNone(m.cm < 0 ? L"NO COUNTERMEASURES \x00B7 EVADE" : L"NO FLARES \x00B7 EVADE");
    }
    if (hudinfo::CruiseDisrupted(0.2f))
    {
        Alert(L"CRUISE DISRUPTED", kAmber);
        AlertPrompt(A::Cruise, L"RECHARGE");
    }
    const float free = g_haveSelf ? g_self.holdFree : 1;
    if (g_haveSelf && free >= 0 && free < 1 && g_prevFree >= 1)
    {
        Alert(L"CARGO HOLD FULL", kChrome);
        AlertNone(L"NO ROOM FOR LOOT");
    }
    if (g_haveSelf && g_self.shieldMax > 0)
        g_prevShield = shield;
    g_prevHull = m.hull >= 0 ? m.hull : 1;
    g_prevMissiles = m.missiles;
    g_prevFree = free;
    // a slot lights in its meaning's colour while its alert's condition holds and while the card naming it shows (the
    // round 4 critique: the empty countermeasure slot stayed dark under NO COUNTERMEASURES); an empty or missing answer
    // lights amber (#E8A33A, border and icon); a used-up slot (count 0) stays lit amber while it is empty
    LightSlots(m, g_haveSelf && g_self.shieldMax > 0 && shield <= 0.25f);
}

// ---------------------------------------------------------------- the debug states (the mock-up's, over the live HUD)
static void Simulate(Model& m)
{
    if (g_sim.empty())
        return;
    const bool combat = g_sim == "combat" || g_sim == "torpedo" || g_sim == "critical";
    m.hull = combat ? 0.88f : 1, m.shield = combat ? 0.22f : 1, m.power = combat ? 0.64f : 1;
    if (g_sim == "critical") // (the ring's low states: the flashes, the hull's bloom)
        m.hull = 0.18f, m.shield = 0.10f;
    m.speed = combat ? 84.0f : g_sim == "dock" ? 300.0f : g_sim == "lane" ? 2600.0f : 80.0f;
    m.cruise = g_sim == "cruise" ? 1 : g_sim == "dock" ? 2 : 0;
    m.charge = 0.58f, m.chargeLeft = 2.1f;
    m.nano = 3, m.batt = 3, m.cm = 12, m.mines = 8, m.ammo = 36, m.launcher = L"JAVELIN";
    m.groups[0] = L"Azrael  \u00B7  Lavablade Mk II", m.groups[1] = L"Azrael", m.groups[2] = L"Lavablade Mk II";
    m.nGroups = 3, m.activeGroup = 0, m.realGroups = true, m.ungrouped.clear();
    m.torpedo.clear(), m.torpedoAmmo = -1;
    if (g_sim == "torpedo") // (the torpedo row takes the third group's line)
        m.nGroups = 2, m.torpedo = L"TORPEDO MK I", m.torpedoAmmo = 6, TorpedoPrompt(m, presets::Action::Torpedo);
    m.holdUsed = 12, m.holdCap = 40, m.credits = 4250;
    m.nEscorts = 0;
    m.man = false, m.ap = false, m.progress = -1, m.eta.clear(), m.wait.clear(), m.info.clear(), m.event.clear();
    m.timer.clear(), m.tMission = false;
    m.target = true;
    m.tBars = false, m.tLevel = -1;
    m.missiles = 0;
    m.nThreats = 0;
    m.alertLeft = 0;
    m.hot[0] = m.hot[1] = m.hot[2] = m.hot[3] = false;
    m.locked = false;
    m.tName = L"Fort Bush", m.tFaction = L"Liberty Police", m.tClass = L"POLICE BASE", m.tDist = L"3.4 km";
    m.tAtt = L"FRIENDLY", m.tCol = kFriend, m.tIcon = flicons::Station;
    m.objective = L"Dock at Fort Bush", m.protect = -1;
    m.state = State::Calm;
    m.nCtx = 0;
    using A = presets::Action;
    if (combat)
    {
        m.state = State::Combat;
        m.objective = L"Protect the armored transport", m.protect = 0.62f;
        m.tName = L"Dustin Leinenbach", m.tFaction = L"Rogues", m.tClass = L"FIGHTER", m.tLevel = 3, m.tDist = L"427 m";
        m.tAtt = L"HOSTILE", m.tCol = kHostile, m.tIcon = flicons::Enemy, m.tBars = true, m.tShield = 0.30f, m.tHull = 0.74f;
        m.threats[0] = {L"Charles Emerson", L"460 m", kHostile}, m.threats[1] = {L"Bart Kessler", L"1.2 km", kHostile};
        m.nThreats = 2;
        m.escorts[0] = {L"Escort 1 \u00B7 Charackli", 1.0f}, m.escorts[1] = {L"Escort 2 \u00B7 Cazamias", 0.7f};
        m.escorts[2] = {L"Escort 3 \u00B7 Carlson", 0.35f};
        m.nEscorts = 3;
        m.missiles = 1, m.missileAng[0] = 28, m.hitAng = 200, m.hitAge = 0.2f;
        m.alert = L"SHIELD FAILING", m.alertCol = kHostile, m.alertLeft = 3, m.nAlertPrompts = 0;
        AlertPrompt(A::Battery, L"BATTERY");
        AlertPrompt(A::Cm, L"FLARE");
        m.hot[1] = true, m.hotCol[1] = kShield, m.hot[2] = true, m.hotCol[2] = kHostile;
        m.locked = true;
        AddCtx(A::Reverse, L"REVERSE");
        AddCtx(A::Turret, L"TURRET");
    }
    else if (g_sim == "travel")
    {
        AddCtx(A::Context, L"DOCK", true); // (the LB hints have their own spots since 26 Oct 2026)
        AddCtx(A::Cruise, L"CRUISE"); // (the cruise prompt in this line, every preset - Slow's rule, B13 / 5.21)
        m.place = L"NEW YORK  \u00B7  LIBERTY SPACE", m.placeLeft = 5;
    }
    else if (g_sim == "cruise")
    {
        m.state = State::CruiseCharge;
        m.man = true, m.manName = L"CRUISE CHARGING", m.manCol = kCyan, m.manIcon = flicons::Cruise, m.manDest.clear();
        AddCtx(A::Cruise, L"CANCEL CRUISE"); // (the cancel in this line, every preset - Slow's rule, B13 / 5.21)
        AddCtx(A::Context, L"DOCK", true);
    }
    else if (g_sim == "dock")
    {
        m.state = State::Autopilot;
        m.man = true, m.manName = L"DOCK", m.manCol = kActive, m.manIcon = flicons::Dock, m.manDest = L"FORT BUSH";
        m.ap = true, m.progress = 0.62f, m.eta = L"0:12";
        AddCtx(A::FreeFlight, L"ABORT DOCK");
        AddCtx(A::Autopilot, L"TAKE CONTROL");
    }
    else if (g_sim == "lane")
    {
        m.state = State::Maneuver;
        m.objective = L"Dock at Manhattan";
        m.man = true, m.manName = L"TRADE LANE", m.manCol = kActive, m.manIcon = flicons::Lane, m.manDest = L"MANHATTAN";
        m.progress = 0.35f, m.eta = L"9 rings  \u00B7  0:48";
        m.tName = L"Manhattan", m.tFaction = L"Liberty", m.tClass = L"PLANET", m.tDist = L"38 km", m.tIcon = flicons::Planet;
        AddCtx(A::Hub, L"MAP");
    }
    // ---- the presets' other contexts (UI_HUD2_PRESETS.md 6 step 2; UI_SPEC.json hud2_presets contexts). Each fills only
    // what Slow fills in that real situation (the code path named per branch); the fight rule (g_combat: the threat
    // list, the calm fades) and m.fight are the sim's own here - Slow recomputes them live every 10 Hz pass.
    else
    {
        auto calm = [&]() { g_combat = false, m.fight = false, m.loot = 0, m.turret = false, m.enemies = 0; };
        // a fight as Slow reads it: hostiles near (g_combat), the threat rows (hostiles other than the target, 2 rows),
        // the Combat state's context line (REVERSE, TURRET)
        auto fight = [&]()
        {
            g_combat = true, m.fight = true, m.loot = 0, m.turret = false, m.enemies = 0;
            m.state = State::Combat;
            m.hull = 0.88f, m.shield = 0.55f, m.power = 0.64f, m.speed = 84.0f, m.cruise = 0;
            m.objective = L"Destroy the Hostile Ships";
            m.threats[0] = {L"Charles Emerson", L"460 m", kHostile}, m.threats[1] = {L"Bart Kessler", L"1.2 km", kHostile};
            m.nThreats = 2;
        };
        auto hostileFighter = [&]()
        {
            m.target = true;
            m.tName = L"Dustin Leinenbach", m.tFaction = L"Rogues", m.tClass = L"FIGHTER", m.tLevel = 3, m.tDist = L"427 m";
            m.tAtt = L"HOSTILE", m.tCol = kHostile, m.tIcon = flicons::Enemy, m.tBars = true, m.tShield = 0.30f, m.tHull = 0.74f;
        };
        if (g_sim == "ap") // the autopilot travelling (m.ap = g_ctx.autopilot, ModeVerb(Travel) + its target's name; no
        {                  // maneuver chip: it steers itself), cruising - calm flight (the calm fades)
            calm();
            m.state = State::Autopilot;
            m.ap = true, m.apMode = L"TRAVEL TO MANHATTAN";
            m.cruise = 2, m.speed = 300.0f;
            m.objective = L"Dock at Manhattan";
            m.tName = L"Manhattan", m.tFaction = L"Liberty", m.tClass = L"PLANET", m.tDist = L"38 km", m.tIcon = flicons::Planet;
            AddCtx(A::Autopilot, L"TAKE CONTROL");
            AddCtx(A::GoalPanel, L"GOALS");
        }
        else if (g_sim == "apwait") // the objective run waiting (Mode Off with g_obj.on: "MISSION"; WaitReason LeadStall)
        {
            calm();
            m.state = State::Autopilot;
            m.ap = true, m.apMode = L"MISSION", m.wait = L"WAITING FOR THE LEADER";
            m.speed = 0;
            m.objective = L"Follow Walker to the New York Jump Gate";
            m.target = false;
            AddCtx(A::Autopilot, L"TAKE CONTROL");
            AddCtx(A::GoalPanel, L"GOALS");
        }
        else if (g_sim == "turret") // turret view in a fight (g_ctx.turret: the TURRET VIEW chip, EXIT TURRET)
        {
            fight();
            hostileFighter();
            m.state = State::Turret;
            m.turret = true;
            AddCtx(A::Turret, L"EXIT TURRET");
        }
        else if (g_sim == "formation") // the game's formation directive (pilot op 10: FORMATION + the leader's call sign,
        {                               // the bar toward the leader), the members as escorts (form.leader), cruising
            calm();
            m.state = State::Maneuver;
            m.man = true, m.manName = L"FORMATION", m.manIcon = flicons::Formation, m.manCol = kActive, m.manDest = L"WALKER";
            m.progress = 0.94f, m.eta = L"0:00"; // (the leader a few tens of metres ahead at cruise speed)
            m.cruise = 2, m.speed = 300.0f;
            m.objective = L"Follow Walker to the New York Jump Gate";
            m.target = false;
            m.escorts[0] = {L"Walker", 1.0f}, m.nEscorts = 1;
            AddCtx(A::FreeFlight, L"LEAVE FORMATION");
        }
        else if (g_sim == "jump" || g_sim == "letterbox") // the gate's sequence (hudinfo::Jumping: JUMP toward the far
        {                                                  // system, the countdown, no bar); letterbox = its tunnel (Update)
            calm();
            m.state = State::Maneuver;
            m.man = true, m.manName = L"JUMP", m.manIcon = flicons::JumpGate, m.manCol = kCyan, m.manDest = L"NEW YORK";
            m.eta = g_sim == "jump" ? L"0:04" : L"0:01";
            m.speed = g_sim == "jump" ? 40.0f : 0.0f;
            m.objective = L"Follow Walker to the New York Jump Gate";
            m.tName = L"New York Jump Gate", m.tFaction = L"Liberty", m.tClass = L"JUMP GATE", m.tDist = L"320 m";
            m.tIcon = flicons::JumpGate;
            AddCtx(A::Hub, L"MAP");
        }
        else if (g_sim == "loot") // loot in the tractor's reach (m.loot: TRACTOR 3 in the right lane, 5.23), the loot
        {                         // targeted (class LOOT, no attitude: neutral, no bars), the beam pulling (TRACTOR ACTIVE)
            calm();
            m.speed = 20.0f;
            m.loot = 3;
            m.tName = L"Scrap Metal", m.tFaction.clear(), m.tClass = L"LOOT", m.tDist = L"180 m";
            m.tAtt = L"NEUTRAL", m.tCol = kNeutral, m.tIcon = flicons::Loot;
            m.info = L"TRACTOR ACTIVE";
            m.tLootReach = true;
            m.lootName = L"Scrap Metal", m.lootDist = L"180 m";
            TractorCtx(true); // ([A] TRACTOR SCRAP METAL . 180 m, [X] TRACTOR ALL 3)
        }
        else if (g_sim == "capital") // a fight with a capital ship targeted (ClassName: BATTLESHIP); no sub-target row:
        {                            // HUD 2 draws none yet (UI_HUD2_PRESETS.md 1.4, step 6)
            fight();
            m.objective = L"Take out the Nomad Ships";
            m.target = true;
            m.tName = L"Nomad Battleship", m.tFaction = L"Nomads", m.tClass = L"BATTLESHIP", m.tLevel = -1, m.tDist = L"2.4 km";
            m.tAtt = L"HOSTILE", m.tCol = kHostile, m.tIcon = flicons::Enemy, m.tBars = true, m.tShield = 0.80f, m.tHull = 0.95f;
            m.threats[0] = {L"Nomad Fighter", L"640 m", kHostile}, m.threats[1] = {L"Nomad Fighter", L"1.1 km", kHostile};
            AddCtx(A::Reverse, L"REVERSE");
            AddCtx(A::Turret, L"TURRET");
        }
        else if (g_sim == "notarget") // a fight with nothing targeted (hostiles near, no target: the pad's LB hints at full
        {                             // alpha in the card's corner, no threat list - ListShown wants a target on the pad;
            fight();                  // under them the enemies cue, DrawEnemyCue: the owner, 4 Nov 2026)
            m.target = false;
            m.enemies = 3;
            AddCtx(A::Reverse, L"REVERSE");
            AddCtx(A::Turret, L"TURRET");
        }
        else if (g_sim == "offscreen") // the destination targeted behind the ship: HUD 2 draws the card as on screen (it
        {                              // keeps no on / off-screen state yet); the direction is the game's native arrow
            calm();
            m.cruise = 2, m.speed = 300.0f;
            m.objective = L"Dock at Manhattan";
            m.tName = L"Manhattan", m.tFaction = L"Liberty", m.tClass = L"PLANET", m.tDist = L"38 km", m.tIcon = flicons::Planet;
            AddCtx(A::FreeFlight, L"DROP CRUISE");
            AddCtx(A::Context, L"DOCK", true);
        }
        else if (g_sim == "comm") // a dock approach under a mission timer while a comm line shows (the presets' comms:
        {                         // the event yields to the comm surface, TIME stays): g_simComm forces the surface
            calm();
            m.speed = 120.0f;
            m.state = State::Maneuver;
            m.man = true, m.manName = L"DOCK", m.manCol = kActive, m.manIcon = flicons::Dock, m.manDest = L"FORT BUSH";
            m.progress = 0.62f, m.eta = L"0:12";
            m.event = std::wstring(L"DOCKING GRANTED") + kDot + L"BAY 3";
            m.info = m.timer = L"TIME 1:42";
            AddCtx(A::FreeFlight, L"ABORT DOCK");
        }
    }
}

// ---------------------------------------------------------------- layout, fades, the bus
// The threat list under the target card: the pad's in combat (with a target, as built); the keyboard form's header
// always (its contact list toggle), its rows in combat or in the contact list mode.
// The enemies cue (the owner, 4 Nov 2026: "Even when the target panel is hidden, when there are enemies nearby the top
// right corner of the screen should mention there are enemies nearby, and prompt the player to press the right stick to
// target nearest enemy"): the pad with nothing targeted (a target brings the card, which says it), hostiles within the
// targeting press's reach, the player flying (not the autopilot, which picks its own targets; not the turret view).
static bool EnemyCue()
{
    return !g_m.kbm && !g_m.target && g_m.enemies > 0 && g_m.state != State::Autopilot && g_m.state != State::Turret &&
           g_m.nearestBtn.btn != glyphs::COUNT;
}

static bool ListShown()
{
    if (!uiopt::Get().hudThreats)
        return false;
    if (g_m.kbm || g_m.list)
        return true;
    return g_m.target && g_combat && g_m.nThreats > 0;
}

bool CycleListFilter(std::wstring& now)
{
    if (!g_active)
        return false;
    if (!g_m.list)
        g_m.list = true, g_m.listFilter = 0;
    else if (g_m.listFilter < 4)
        ++g_m.listFilter;
    else
        g_m.list = false;
    static const wchar_t* kNames[] = {L"IMPORTANT", L"SHIPS", L"STATIONS", L"LOOT", L"ALL"};
    now = g_m.list ? kNames[g_m.listFilter] : L"OFF";
    g_slow = 1; // refill the list at once
    return true;
}

bool BottomPanelsTop(float& y)
{
    if (!g_visible || g_L.H <= 0)
        return false;
    y = std::min(g_L.loY0, g_L.shipY0);
    if (g_L.consY1 > g_L.consY0) // (the cross stacked on the loadout: the goal panel stops above it - decision 3)
        y = std::min(y, g_L.consY0);
    return y > 0;
}

// The frame's rectangles from the layout core (hud2_layout.h, UI_HUD2_PRESETS.md 6 step 5; UI_HUD2.md 5.17): this is its
// impure half - the scale for the texts' measures, the context line's prompt widths and the alert's width (the kit), the
// subtitles' dialogue panel (the HUD bus) - then the core places everything and the result is copied into g_L for the
// parts. The history of each rule (the aspect factor, the fit caps, the nav's 0.42 H for the dialogue panel, the
// loadout's 0.15, the context line's one baseline and hold cascade, the alert's places) is in the core's comments and
// UI_HUD2.md 5.9-5.16.
static void ComputeLayout(float W, float H)
{
    const uiopt::UiOptions& o = uiopt::Get();
    hud2layout::Frame f;
    f.W = W, f.H = H, f.hudScale = o.hudScale, f.textSize = o.textSize, f.kbm = g_m.kbm;
    f.combat = g_combat, f.letterbox = g_letterbox;
    f.target = g_m.target, f.list = ListShown();
    f.listRows = g_m.kbm ? g_m.nThreats : 2;
    f.listMore = g_m.list && g_m.listTotal > g_m.nThreats;
    f.alert = g_m.alertLeft > 0 && !g_m.alert.empty();
    // the nav sized to its content (0.2.1 round 4, UI_HUD2.md 5.27): its box's right edge and bottom and its scrim's extent
    // follow the drawn lines - at once when they grow (the box always holds the text), gently and only past the
    // hysteresis when they shrink (hud2_layout.h EaseContent: a ticking value or a 5 s place line never breathes it)
    {
        static float r = 0, b = 0, sr = 0, sb = 0, hWas = 0;
        static ULONGLONG at = 0;
        const ULONGLONG now = GetTickCount64();
        const float ndt = at ? Clampf((now - at) / 1000.0f, 0.0f, 0.25f) : 0.0f;
        at = now;
        if (H != hWas)
            r = b = sr = sb = 0, hWas = H;
        const float hyst = hud2layout::kContentHyst * H;
        auto ease = [&](float& cur, float raw)
        {
            cur = raw > 0 ? std::max(raw, hud2layout::EaseContent(cur, raw, ndt, hyst)) : 0.0f;
            return cur;
        };
        f.navRight = ease(r, g_navRight), f.navBottom = ease(b, g_navBottom);
        g_navScrim[2] = ease(sr, g_navScrim[0]), g_navScrim[3] = ease(sb, g_navScrim[1]);
    }
    if (g_m.presetDrawn == hud2preset::Focus) // (Focus: its reveal's form and held parts)
        f.form = g_rv.form, f.held = g_rv.held;
    // the measures need this frame's scale (the parts size their texts by L.H x L.k)
    Layout& L = g_L;
    L.W = W, L.H = H, L.k = hud2layout::Scale(f);
    f.m.nCtx = g_m.nCtx;
    f.m.ctxMeasured = ContextWidths(g_m, L, f.m.base, f.m.glyph, f.m.label, f.m.hold, f.m.dims);
    if (f.m.ctxMeasured)
        ContextLabelMins(g_m, L, f.m.labelMin); // (a squeezed prompt's label wraps, never scrolls: 0.2.1)
    for (int i = 0; i < g_m.nCtx && i < 3; ++i) // (each prompt's lane: CtxSide, the owner's decision 4)
        f.m.side[i] = g_m.ctx[i].side;
    f.m.ctxGap = ContextGap(L), f.m.ctxWord = ContextWordSize(L);
    f.m.slotKeyW = SlotKeyWidth(g_m, L, g_m.presetDrawn == hud2preset::Focus); // (the stacked cross's width: decision 3)
    f.m.alertW = g_m.alert.empty() ? 0 : DrawAlert(g_m, L, 1, true);
    // Standard's panels sized to their content (0.2.1 round 3, UI_HUD2.md 5.26): measured, floored, eased with hysteresis
    // (hud2_layout.h EaseContent); the layout caps them at the spec's widths. A new scale or screen snaps them.
    if (g_m.presetDrawn != hud2preset::Focus)
    {
        static float cardW = 0, shipW = 0, loadW = 0, kWas = 0, hWas = 0;
        static ULONGLONG at = 0;
        const ULONGLONG now = GetTickCount64();
        const float cdt = at ? Clampf((now - at) / 1000.0f, 0.0f, 0.25f) : 0.0f;
        at = now;
        static float shipH = 0, loadH = 0; // (round 6: the heights, the same ease / hysteresis)
        if (L.k != kWas || H != hWas)
            cardW = shipW = loadW = shipH = loadH = 0, kWas = L.k, hWas = H;
        shipH = hud2layout::EaseContent(shipH, ShipContentHeight(g_m, L), cdt, hud2layout::kContentHyst * H * L.k);
        loadH = hud2layout::EaseContent(loadH, LoadoutContentHeight(g_m, L), cdt, hud2layout::kContentHyst * H * L.k);
        f.m.shipH = shipH, f.m.loadH = loadH;
        const float hk = H * L.k, hyst = hud2layout::kContentHyst * hk;
        const float cw = std::max(g_m.target ? CardContentWidth(g_m, L) : 0.0f, f.list ? ListContentWidth(g_m, L) : 0.0f);
        const float sw = ShipContentWidth(g_m, L), lw = LoadoutContentWidth(g_m, L);
        cardW = hud2layout::EaseContent(cardW, cw > 0 ? std::max(cw, hud2layout::kCardMinW * hk) : 0.0f, cdt, hyst);
        shipW = hud2layout::EaseContent(shipW, sw > 0 ? std::max(sw, hud2layout::kShipMinW * hk) : 0.0f, cdt, hyst);
        loadW = hud2layout::EaseContent(loadW, lw > 0 ? std::max(lw, hud2layout::kLoadMinW * hk) : 0.0f, cdt, hyst);
        f.m.cardW = cardW, f.m.shipW = shipW, f.m.loadW = loadW;
    }
    // the subtitles' dialogue panel (the alert goes under it, or to the first clear place)
    HudSubtitles subs = {};
    const HudBus* bus = HostAtLeast(21) && g_host->GetHudBus ? g_host->GetHudBus() : nullptr;
    // (the comm surface shows a line: the nav leaves its dock / lane / jump answer to it - one value, one place)
    // Fresh = the subtitles' record changed within the last 2 of our publishes: its `frame` is the subtitles' own counter,
    // not the bus's (the compare of the two counters read every record as stale - the alert never moved under the
    // dialogue panel, 25 Oct 2026, FIX2)
    static u32 subsWas = 0, subsSeenAt = 0;
    const bool read = bus && bus->ReadSubtitles && bus->Frame && bus->ReadSubtitles(&subs);
    if (read && subs.frame != subsWas)
        subsWas = subs.frame, subsSeenAt = bus->Frame();
    g_m.comm = read && bus->Frame() - subsSeenAt <= 2 && subs.dialogue.x1 > subs.dialogue.x0 && subs.dialogue.y0 < 0.3f;
    if (!g_sim.empty() && g_simComm) // debug `hud2 sim <name>+comm` (the subtitles' panel itself: their `test` command)
        g_m.comm = true;
    f.dlg.fresh = read && bus->Frame() - subsSeenAt <= 2 && subs.dialogue.x1 > subs.dialogue.x0;
    f.dlg.comm = g_m.comm;
    f.dlg.x0 = subs.dialogue.x0, f.dlg.y0 = subs.dialogue.y0, f.dlg.x1 = subs.dialogue.x1, f.dlg.y1 = subs.dialogue.y1;
    // the core (the drawn preset's data and rules)
    const hud2layout::Out& c = g_core = hud2layout::Compute(f, hud2preset::Data(g_m.presetDrawn));
    L.aspect = c.aspect, L.k = c.k;
    L.mx = c.mx, L.my = c.my, L.cx = c.cx, L.cy = c.cy, L.R = c.R;
    L.navX0 = c.nav.x0, L.navY0 = c.nav.y0, L.navX1 = c.nav.x1, L.navY1 = c.nav.y1, L.navMaxX = c.navMaxX;
    L.tgtX0 = c.card.x0, L.tgtY0 = c.card.y0, L.tgtX1 = c.card.x1, L.tgtY1 = c.card.y1;
    L.thrY0 = c.thrY0, L.thrY1 = c.thrY1;
    L.shipX0 = c.ship.x0, L.shipY0 = c.ship.y0, L.shipX1 = c.ship.x1, L.shipY1 = c.ship.y1;
    L.loX0 = c.loadout.x0, L.loY0 = c.loadout.y0, L.loX1 = c.loadout.x1, L.loY1 = c.loadout.y1;
    L.consX0 = c.cons.x0, L.consY0 = c.cons.y0, L.consX1 = c.cons.x1, L.consY1 = c.cons.y1;
    L.ctxY = c.ctxY, L.ctxX0 = c.ctxX0, L.ctxX1 = c.ctxX1, L.ctxTop = c.ctxTop, L.ctxPlan = c.ctxPlan;
    for (int i = 0; i < 3; ++i)
        L.ctxW[i] = c.ctxW[i], L.ctxX[i] = c.ctxX[i], L.ctxPY[i] = c.ctxPY[i], L.ctxWrap[i] = c.ctxWrap[i];
    L.ctxTall = c.ctxTall;
    L.alertY = c.alertY, L.alertX0 = c.alertX0, L.alertX1 = c.alertX1, L.alertY1 = c.alertY1;
    L.listInCard = c.listInCard, L.form = c.form, L.bandY = c.bandY, L.holdY = c.holdY, L.clearY0 = c.clearY0;
    g_m.nCtx = c.nCtx; // (the last prompts that did not fit gave way; a preset's pill cap too)
}

static void Publish(bool active)
{
    const HudBus* bus = HostAtLeast(21) && g_host->GetHudBus ? g_host->GetHudBus() : nullptr;
    if (!bus || !bus->PublishLayout)
        return;
    const Layout& L = g_L;
    const float W = L.W > 0 ? L.W : 1, H = L.H > 0 ? L.H : 1;
    auto R = [&](float x0, float y0, float x1, float y1) { return HudRect{x0 / W, y0 / H, x1 / W, y1 / H}; };
    const HudRect none = {0, 0, 0, 0};
    HudLayout l = {};
    l.active = active ? 1 : 0;
    l.kbm = g_m.kbm ? 1 : 0;
    l.alerts = active && uiopt::Get().hudAlerts ? 1 : 0;
    l.alpha = g_t * uiopt::Get().hudOpacity / 100.0f;
    l.scale = uiopt::Get().hudScale / 100.0f;
    if (active)
    {
        l.nav = R(L.navX0, L.navY0, L.navX1, L.navY1);
        const bool list = ListShown();
        l.target = g_m.target ? R(L.tgtX0, L.tgtY0, L.tgtX1, list ? L.thrY1 : L.tgtY1)
                   : list     ? R(L.tgtX0, L.thrY0, L.tgtX1, L.thrY1)
                              : none;
        l.ship = g_f.ship > 0.01f ? R(L.shipX0, L.shipY0, L.shipX1, L.shipY1) : none;
        // (the loadout with the cross stacked on it, as one rectangle: the bus has one; decision 3)
        l.loadout = g_f.loadout > 0.01f ? R(std::min(L.loX0, L.consX1 > L.consX0 ? L.consX0 : L.loX0),
                                            std::min(L.loY0, L.consY1 > L.consY0 ? L.consY0 : L.loY0), L.loX1, L.loY1)
                                        : none;
        // (the ring's, the top strip's and the comm feed's rectangles: the layout core's, hud2_layout.h)
        const hud2layout::Out& c = g_core;
        l.ring = R(c.ringBus.x0, c.ringBus.y0, c.ringBus.x1, c.ringBus.y1);
        l.context = g_m.nCtx ? R(L.ctxX0, L.ctxTop, L.ctxX1, L.ctxY + 0.03f * H) : none; // (with a word above its glyph)
        l.alert = g_m.alertLeft > 0 && !g_m.alert.empty() ? R(L.alertX0, L.alertY, L.alertX1, L.alertY1) : none;
        l.topFree = R(c.topFree.x0, c.topFree.y0, c.topFree.x1, c.topFree.y1); // from the screen's top edge (the panel may hang flush)
        l.chatFree = R(c.chatFree.x0, c.chatFree.y0, c.chatFree.x1, c.chatFree.y1);
    }
    bus->PublishLayout(&l);
}

// hud_wireframes: the game's own 3D views (the target's, the player's ship's) in the card's and the ship panel's left
// column, requested each frame for hudvis to draw under the HUD (the next OnRenderUnderHud); only while HUD 2 is fully
// in (a view cannot fade or slide) and its part is not faded down, else the flat icons. The rectangles are the ones
// DrawTarget / DrawShip leave as their windows.
static void RequestViews(const Context& c)
{
    const Layout& L = g_L;
    // (Focus: none - its compact card and its free-text bottom-left have no window for a view; UI_HUD2.md 5.18)
    const bool on = uiopt::Get().hudWireframes && g_active && c.space && g_t >= 1.0f && L.H > 0 && g_sim.empty() &&
                    g_m.presetDrawn != hud2preset::Focus;
    const float H = L.H, k = L.k, opacity = Clampf(uiopt::Get().hudOpacity / 100.0f, 0.4f, 1.0f);
    {
        const float vw = 0.13f * H * k, hh = (L.tgtY1 - L.tgtY0) / 2 - 0.012f * H * k;
        // the panel's view is built by its own update for the target it last saw while shown: a stale one (another
        // object) is never drawn; the panel is let run its frame (shown until Apply hides it before the draw)
        const u32 viewObj = hudvis::TargetViewObject();
        const bool stale = g_m.targetId && viewObj != g_m.targetId;
        if (on && stale && g_m.tViewOk)
            hudvis::PulseTargetPanel(), ++g_viewPulses;
        // only for what has a body worth a wireframe (ships, stations, planets, gates): a trade lane ring's view showed stray
        // silhouettes, loot / waypoints nothing (the round 3 critique) - those take the flat icon
        const bool body = g_m.tViewOk;
        const bool show = on && g_m.target && body && !stale && g_f.target > 0.5f;
        // the rectangle 1.5x the window: the view's object fills ~45 % of its rectangle (FREELANCER_RE.md "UI"), ~2/3 of
        // the window then (the view draws only the object, nothing of the rectangle outside it)
        hudvis::SetViewRect(1, L.tgtX0 + 0.012f * H * k + vw / 2, (L.tgtY0 + L.tgtY1) / 2, vw / 2 * kViewZoom, hh * kViewZoom,
                            show);
        g_viewBack[1] = {show, L.tgtX0, L.tgtY0, L.tgtX0 + 0.012f * H * k + vw + 0.006f * H * k, L.tgtY1, 0.012f * H * k,
                         kTR | kBL, 0.82f * std::max(g_f.target, 0.75f) * opacity};
    }
    {
        // the ship panel's icon column (hud2_parts.cpp DrawShip; 0.80 x h = 0.12 until 5.22's narrower panel)
        const float h = L.shipY1 - L.shipY0, vw = hud2layout::std_ov::kShipViewW * H * k;
        const bool show = on && g_f.ship > 0.3f;
        hudvis::SetViewRect(0, L.shipX0 + 0.012f * H * k + vw / 2, (L.shipY0 + L.shipY1) / 2, vw / 2 * kViewZoom,
                            (h / 2 - 0.012f * H * k) * kViewZoom, show);
        g_viewBack[0] = {show, L.shipX0, L.shipY0, L.shipX0 + 0.012f * H * k + vw + 0.006f * H * k, L.shipY1, 0.012f * H * k,
                         kTL | kBR, 0.82f * std::max(g_f.ship, 0.75f) * opacity};
    }
}

static void Ease(float& v, float target, float dt, float tau = 0.25f)
{
    const float k = tau > 0 ? std::min(1.0f, dt / tau) : 1;
    v += (target - v) * k;
    if (fabsf(v - target) < 0.004f)
        v = target;
}

void Update(float dt, const Context& c)
{
    perf::Scope ps("hud2.Update");
    const double t0 = perf::Now();
    g_ctx = c;
    g_cursorEligible = false; // (set by the CONTEXTUAL wake below when it runs)
    const uiopt::UiOptions& o = uiopt::Get();
    // the layout preset and the controller setup it adapts to (UI_HUD2_PRESETS.md 6 step 4, hud2_preset.h): kept current
    // in and out of flight, so `hud2 state` and the lint read the live choice; step 5 lays out from it
    {
        const int p = hud2preset::Clamp(o.hudPreset);
        if (p != g_m.preset)
            Log("hud2: preset %s", hud2preset::Label(p).c_str());
        g_m.preset = p;
        g_m.presetDrawn = hud2preset::Drawn(p);
        g_m.presetData = &hud2preset::Data(p);
        hud2preset::Setup s;
        s.padPreset = o.padPreset, s.weaponGroups = o.weaponGroups, s.holdsAsToggles = o.holdsAsToggles, s.kbm = !c.pad;
        g_m.adapt = hud2preset::AdaptTo(p, s);
    }
    const bool inSpace = c.space || c.spaceUi;
    bool want = o.hudStyle == 2 || (o.hudStyle == 1 && c.pad);
    if (g_force >= 0)
        want = g_force == 1;
    const bool active = want && inSpace;
    if (active != g_active)
    {
        ++g_switches;
        Log("hud2: %s (style %d, %s)", active ? "on: the game's panels hidden" : "off: the game's panels back",
            o.hudStyle, c.pad ? "pad" : "keyboard and mouse");
    }
    g_active = active;
    g_viewBack[0].on = g_viewBack[1].on = false; // (RequestViews sets them while the views are wanted)
    if (!inSpace)
    {
        if (g_wasInSpace)
            hudvis::Invalidate(); // the HUD goes with the flight (a load, a dock, the menu): drop the roots
        g_wasInSpace = false;
        g_entrySeen = false;
        g_visible = false;
        g_t = 0;
        HotUpdate(g_m, false);
        return;
    }
    if (!g_wasInSpace)
        g_spaceSince = perf::Now(); // (the alerts arm 1.5 s later: Slow)
    g_wasInSpace = true;
    const bool show = active && c.space;
    const float ms = static_cast<float>(o.hudTransition);
    if (ms <= 0)
        g_t = show ? 1.0f : 0.0f;
    else
        g_t = Clampf(g_t + (show ? 1 : -1) * dt * 1000.0f / ms, 0, 1);
    g_visible = g_t > 0;
    if (!active && !g_visible)
    {
        HotUpdate(g_m, false);
        Publish(false);
        return;
    }
    g_m.kbm = !c.pad;
    // the keyboard form's clicks (queued by OnUiButton) and its hotspots for the host (placed there only on a change)
    HotUpdate(g_m, g_visible && g_m.kbm && c.space && !c.modal && !g_menuOver && g_t > 0.5f);
    Fast(dt);
    g_slow += dt;
    if (HotRefreshWanted())
        g_slow = 1;
    bool built = !g_sim.empty(); // (the debug states build the model every frame)
    if (g_slow >= 0.1f)
    {
        g_slow = 0;
        Slow();
        built = true;
    }
    Simulate(g_m);
    if (built) // the prompt set's signature as built (the layout trims nCtx every frame after: not a change)
        g_ctxSig = CtxSig(g_m);
    if (c.modal)
        g_m.nCtx = 0; // the context line steps aside while a modal shows its prompt bar
    // the fades
    const bool calm = g_m.state == State::Calm || g_m.state == State::CruiseCharge || g_m.state == State::Maneuver ||
                      (g_m.state == State::Autopilot && !g_combat) || // the autopilot travelling is calm flight too
                      (g_m.state == State::Turret && !g_combat);      // turret view out of a fight (0.2.1 round 6)
    const int fade = o.hudFade <= 0 ? 0 : 1; // (0.2.1: 0 FULL, 1 CONTEXTUAL; the retired 2 MINIMAL falls back to 1)
    Fades tgt;
    tgt.ring = tgt.ship = tgt.loadout = tgt.nav = 1; // (CONTEXTUAL's wake below)
    bool targetChanged = false;
    tgt.target = g_m.target ? (g_m.state == State::Maneuver && g_m.manName == L"TRADE LANE" ? 0.75f : 1.0f) : 0.0f;
    tgt.context = g_m.nCtx ? 1.0f : 0.0f;
    // hud_prompts 1 (0.2.1, core\game\promptshow.h): the lanes show on a change of their prompts or after a player action
    // (NoteAction; a target switch / clear by any input), 4 s, then fade out with the context's ease
    {
        const u32 tid = fl::TargetId();
        targetChanged = tid != g_promptTarget;
        const bool action = g_promptAction || targetChanged;
        g_promptAction = false, g_promptTarget = tid;
        if (!promptshow::Step(g_promptShow, o.hudPrompts, g_ctxSig, action, c.modal, dt))
            tgt.context = 0;
    }
    tgt.alert = g_m.alertLeft > 0 ? 1.0f : 0.0f;
    // The game's cinematics (a jump's tunnel, a lane's entry, a dock's approach: the letterbox, exe::LetterboxAmount): HUD 2
    // collapses to the nav's chip (in the upper bar) and the ring - the side panels and the context line go (the round 3
    // critique: the panels straddled the bars and read through over the gate's stripes)
    // (debug `hud2 sim letterbox`: the bars' HUD without the bars - the game draws them only in a real cinematic)
    const bool simCine = g_sim == "letterbox";
    g_letterbox = (exe::LetterboxAmount.ok() && Mem<float>(exe::LetterboxAmount) > 0.0f) || simCine;
    g_m.cine = g_letterbox && (g_sim.empty() || simCine);
    const bool focus = g_m.presetDrawn == hud2preset::Focus;
    // CONTEXTUAL (hud_fade 1, the default since 0.2.1; core\game\hudwake.h): HUD 2 fades out while idle / in simple
    // flying, each element wakes on its triggers and lingers 4 s; a fight, a modal, low shield / hull and incoming
    // missiles keep theirs up; the alert is always shown, the prompts follow hud_prompts (Focus: its own reveal, below)
    {
        static long long loSig = 0;
        static unsigned objSig = 0, manSig = 0;
        static int cruiseWas = 0;
        static bool killWas = false;
        unsigned trig = g_wakeTrig;
        g_wakeTrig = 0;
        if (c.steer > 0.15f)
            trig |= hudwake::Steer;
        // the keyboard-and-mouse player's steering wakes the ring as the stick does (0.2.1 round 3; HUD 2 is their default):
        // the game's mouse flight engaged (exe::MouseFlightEngaged, the u8 its steering sets each frame) with the cursor
        // off the centre, or the game's cursor moving while in space (the free cursor aims the guns)
        {
            static int mxWas = -1, myWas = -1;
            const int mx = fl::MouseX(), my = fl::MouseY();
            const float Hs = static_cast<float>(std::max(1, fl::ScreenHeight())), Ws = static_cast<float>(std::max(1, fl::ScreenWidth()));
            const bool engaged = exe::MouseFlightEngaged.ok() && Mem<u8>(exe::MouseFlightEngaged) != 0;
            const float off = sqrtf((mx - Ws / 2) * (mx - Ws / 2) + (my - Hs / 2) * (my - Hs / 2));
            const bool moved = mxWas >= 0 && abs(mx - mxWas) + abs(my - myWas) > 0.004f * Hs;
            if (c.space && !c.pad && ((engaged && off > 0.03f * Hs) || moved))
                trig |= hudwake::Steer;
            mxWas = mx, myWas = my;
        }
        if (g_m.burner)
            trig |= hudwake::Boost;
        if (c.firing)
            trig |= hudwake::Fire;
        if (c.space && !c.pad && FireKeyPressed()) // a K&M fire input's press: everything (the pad's: controller.cpp)
            trig |= hudwake::FireInput;
        // turret view (0.2.1 round 6, the owner: the contextual HUD stayed up there): calm flight like any other, its
        // entry / exit wakes the panels that say so (the TURRET VIEW chip), then the HUD fades by the same rules
        {
            static bool turretWas = false;
            if (c.turret != turretWas)
                trig |= hudwake::Maneuver;
            turretWas = c.turret;
        }
        if (targetChanged)
            trig |= hudwake::TargetChg;
        // the state's own edges (any input: the keyboard, the mouse, the autopilot, the mission)
        const long long lo = static_cast<long long>(g_m.nano) * 1000003LL + g_m.batt * 1009LL + g_m.cm * 31LL + g_m.mines +
                             g_m.ammo * 7LL + g_m.torpedoAmmo * 524287LL + g_m.activeGroup * 131071LL;
        if (lo != loSig)
            loSig = lo, trig |= hudwake::Fire;
        const unsigned obj = promptshow::Mix(promptshow::kSeed, g_m.objective);
        if (obj != objSig)
            objSig = obj, trig |= hudwake::Objective;
        const unsigned man = promptshow::Mix(promptshow::Mix(promptshow::kSeed, g_m.man ? 1 : 0), g_m.manName);
        if (man != manSig)
            manSig = man, trig |= hudwake::Maneuver;
        if (g_m.cruise != cruiseWas || g_m.kill != killWas || g_m.cruise == 1) // (the charge's seconds stay up)
            cruiseWas = g_m.cruise, killWas = g_m.kill, trig |= hudwake::Cruise;
        if (g_m.hitAge >= 0 && g_m.hitAge < 0.3f)
            trig |= hudwake::Hit;
        hudwake::In in;
        in.trig = trig, in.dt = dt, in.combat = !calm || g_combat, in.modal = c.modal || c.spaceUi;
        in.lowShield = g_m.shield >= 0 && g_m.shield < 0.25f, in.lowHull = g_m.hull > 0 && g_m.hull < 0.5f;
        in.missiles = g_m.missiles > 0;
        g_wakeShown = hudwake::Step(g_wake, focus ? 0 : fade, in);
        g_cursorEligible = g_active && c.space && !c.spaceUi && !c.modal && !g_menuOver && !g_m.cine;
        if (!focus && fade == 1)
        {
            tgt.ring = (g_wakeShown & hudwake::Ring) ? 1.0f : 0.0f;
            tgt.ship = (g_wakeShown & hudwake::Ship) ? 1.0f : 0.0f;
            tgt.loadout = (g_wakeShown & hudwake::Loadout) ? 1.0f : 0.0f;
            tgt.nav = (g_wakeShown & hudwake::Nav) ? 1.0f : 0.0f;
            if (!(g_wakeShown & hudwake::Target))
                tgt.target = 0;
        }
    }
    // Focus (UI_HUD2_PRESETS.md 2.2; hud2_layout.h Reveal): its bundle's reveal decides what shows (hud_fade is not read:
    // Focus's bundle is hud_fade 2's reveal, written by the bundles of step 4 - until then the preset applies it itself)
    if (focus)
    {
        namespace hl = hud2layout;
        static int rulesOf = -1;
        static hl::RevealRules rr;
        if (rulesOf != g_m.presetDrawn)
            rr = hl::RevealRulesOf(hud2preset::Data(g_m.presetDrawn)), rulesOf = g_m.presetDrawn;
        const Model& m = g_m;
        hl::RevealIn in;
        in.dt = dt, in.combat = g_combat, in.letterbox = g_letterbox;
        // the auto-peek: once per space entry, when HUD 2 is fully in and no letterbox is up
        in.spaceEntry = !g_entrySeen && g_t >= 1.0f && !g_letterbox && c.space;
        if (in.spaceEntry)
            g_entrySeen = true;
        // (no hold signature: the hold strip is gone - the owner's B9, 31 Oct 2026)
        in.loadSig = static_cast<long long>(m.nano) * 1000003LL + m.batt * 1009LL + m.cm * 31LL + m.mines + m.ammo * 7LL +
                     m.torpedoAmmo * 524287LL + m.activeGroup * 131071LL;
        in.escortSig = m.nEscorts;
        for (int i = 0; i < m.nEscorts && i < 3; ++i)
            in.escortSig = in.escortSig * 101 + static_cast<int>(m.escorts[i].hull * 20 + 0.5f);
        in.escorts = m.nEscorts > 0;
        in.slotLit = m.hot[0] || m.hot[1] || m.hot[2] || m.hot[3];
        // (a hit no longer wakes the arcs to full in calm: each meter shows at 40 % while it is below full, a full one
        // never - the owner's B1 / B10-B12, 31 Oct 2026; the hit's direction is the arcs' circle segment, drawn on top)
        in.hull = m.hull, in.shield = m.shield, in.charging = m.cruise == 1;
        in.speed = m.speed;
        in.throttleState = m.burner ? 1 : m.kill ? 2 : m.reverse ? 3 : m.cruise ? 4 : 0;
        // "continuously during protect, formation, dock and goto, and while the throttle lever rests between 1 and 99 %
        // (K&M, ACE)" (PRESETS 2.2; 4 the setups' readout need)
        // The tick + % show whenever the lever sits partial (the lint's analog rule: an analog control in use never lacks
        // its reading - FREELANCER's spring stick held part-way too); the speed stays on only for the setups whose lever
        // rests there (PRESETS 4: ACE, K&M)
        const float lever = fabsf(m.throttle);
        const bool partial = lever > uimeta::hud2p::kLeverPartialLo && lever < uimeta::hud2p::kLeverPartialHi;
        in.speedSteady = m.protect >= 0 || (partial && m.adapt.needs.throttleReadout) ||
                         (m.man && (m.manName == L"FORMATION" || m.manName == L"DOCK" || m.manName == L"GOTO"));
        g_rv = g_reveal.Step(rr, in);
        g_focusReadout = partial;
        tgt.ring = 1; // (the arcs' and the band's own alphas below)
        tgt.ship = (g_rv.held & hl::Bit(hl::Ship)) ? 0.0f : 1.0f;
        tgt.loadout = (g_rv.held & hl::Bit(hl::Loadout)) ? 0.0f : 1.0f;
        // a lane ride: "the card hides unless hostile or a mission object"
        if (m.man && m.manName == L"TRADE LANE" && !(m.tCol == kHostile || m.tMission))
            tgt.target = 0;
        Ease(g_f.arcShield, g_rv.arcShield, dt), Ease(g_f.arcHull, g_rv.arcHull, dt), Ease(g_f.arcPower, g_rv.arcPower, dt);
        Ease(g_f.band, g_letterbox ? 0.0f : 1.0f, dt), Ease(g_f.speed, g_rv.speed ? 1.0f : 0.0f, dt);
        Ease(g_f.escorts, g_rv.escorts ? 1.0f : 0.0f, dt);
    }
    else
        g_rv = hud2layout::RevealOut(), g_reveal = hud2layout::Reveal(), g_focusReadout = false;
    // what the drawn preset suppresses (the layout core's rules, hud2_layout.h): the letterbox's collapse (Standard: the
    // card, the list, the ship, the loadout and the context line go; the nav's chip, the ring and the alert stay), and
    // its budget rule from the last frame's layout (Standard has none) - the parts fade out instead of vanishing
    // (after the MINIMAL wake, which never wakes a part in the letterbox: the same targets as before the core)
    {
        namespace hl = hud2layout;
        const u32 off = hl::ContextSuppressed(hl::RulesOf(g_m.presetDrawn), g_letterbox) | g_core.budgetOff;
        if (off & hl::Bit(hl::Card))
            tgt.target = 0;
        if (off & hl::Bit(hl::Ship))
            tgt.ship = 0;
        if (off & hl::Bit(hl::Loadout))
            tgt.loadout = 0;
        if (off & hl::Bit(hl::Context))
            tgt.context = 0;
        if (off & hl::Bit(hl::Alert))
            tgt.alert = 0;
    }
    Ease(g_f.ring, tgt.ring, dt), Ease(g_f.ship, tgt.ship, dt), Ease(g_f.loadout, tgt.loadout, dt);
    Ease(g_f.nav, tgt.nav, dt);
    {
        const bool ctxMode = fade == 1 && g_m.presetDrawn != hud2preset::Focus;
        Ease(g_f.wakeTarget, !ctxMode || (g_wakeShown & hudwake::Target) ? 1.0f : 0.0f, dt);
        Ease(g_f.wakeHints, !ctxMode || tgt.context > 0 ? 1.0f : 0.0f, dt, 0.12f);
    }
    Ease(g_f.target, tgt.target, dt, 0.12f), Ease(g_f.context, tgt.context, dt, 0.12f), Ease(g_f.alert, tgt.alert, dt, 0.15f);
    Ease(g_f.enemyCue, EnemyCue() ? 1.0f : 0.0f, dt, 0.12f);
    g_f.slide = 1 - g_t;
    ComputeLayout(static_cast<float>(fl::ScreenWidth()), static_cast<float>(fl::ScreenHeight()));
    RequestViews(c);
    Publish(active);
    g_perf.update = g_perf.update * 0.95f + static_cast<float>((perf::Now() - t0) * 1000.0) * 0.05f;
}

void RenderUnderHud(void* device)
{
    hudvis::SetHideArrival(true); // the nav's place line says what the game's arrival line says (Place)
    hudvis::SetKeyboardForm(g_m.kbm); // the mouse-over label stays in the keyboard form only
    // the game's hover help only for an intentional hover (owner, 25 Oct 2026): in flight after a real mouse move (motion,
    // wheel, buttons: the host's physical-mouse tick) within the last 3 s - not for a cursor resting on a HUD button while
    // the stick, the autopilot or nothing flies; the NeuroNet windows (Mode::SpaceUi), the bases, the menus and a modal
    // menu over the flight or the paused game (the pause / load / save menus) keep the game's own; both HUD styles
    {
        const u32 mt = dimouse::LastRealTick();
        hudvis::SetHoverHelp(!g_ctx.space || g_menuOver || fl::GamePaused() || (mt != 0 && GetTickCount() - mt < 3000));
    }
    // the views' backing: the panel's own glass under the view (as the game's panel draws its background before its
    // view), so the wireframe keeps its contrast over a bright station; only where a view was drawn last frame and is
    // requested again (a missing view leaves no dark patch: the overlay's plate is whole then)
    const bool back[2] = {!g_menuOver && g_viewBackOn && g_viewBack[0].on && hudvis::ViewShown(0),
                          !g_menuOver && g_viewBackOn && g_viewBack[1].on && hudvis::ViewShown(1)};
    int sw = 0, sh = 0;
    if (device && (back[0] || back[1]))
    {
        render::SetDevice(device);
        if (render::ScreenSize(sw, sh) && sh >= 100 &&
            hud2kit::Begin(device, sw, sh, std::max(8, static_cast<int>(0.026f * g_L.H * g_L.k + 0.5f))))
        {
            hud2kit::SetAlpha(1), hud2kit::SetOffset(0, 0);
            for (int i = 0; i < 2; ++i)
                if (back[i])
                {
                    // the window column of the plate: its left corners chamfered as the plate's
                    const ViewBack& b = g_viewBack[i];
                    const float tl = (b.corners & kTL) ? b.ch : 0, bl = (b.corners & kBL) ? b.ch : 0;
                    const float poly[12] = {b.x0 + tl, b.y0, b.x1, b.y0, b.x1, b.y1, b.x0 + bl, b.y1, b.x0, b.y1 - bl, b.x0, b.y0 + tl};
                    hud2kit::Poly(poly, 6, Argb(kGlass, b.alpha));
                }
            hud2kit::SetKeepDestAlpha();
            hud2kit::End();
            ++g_viewBackDrawn;
        }
    }
    hudvis::Apply(g_active);
}

bool Active() { return g_active; }

void SetMenuOver(bool on)
{
    if (on != g_menuOver)
        Log("hud2: %s", on ? "a modal menu over the flight: the flight HUD hidden under its scrim" : "the modal menu closed: the flight HUD back");
    g_menuOver = on;
    hudvis::SetMenuOver(on);
}

bool MenuOver() { return g_menuOver; }

bool TargetCardAnchor(float& right, float& bottom)
{
    if (!g_visible || g_t < 0.5f || g_menuOver)
        return false;
    right = g_L.tgtX1;
    // (no target on the pad: under the left bumper's hints, DrawCalmHints, and the enemies cue under them, DrawEnemyCue)
    const bool hints = !g_m.target && !g_m.kbm;
    const float below = hints ? (EnemyCue() ? kEnemyCueBottom : 0.04f) * g_L.H * g_L.k : 0;
    bottom = (ListShown() ? g_L.thrY1 : g_m.target ? g_L.tgtY1 : g_L.my + below) + 0.012f * g_L.H;
    return true;
}

float RingRadius() { return g_L.R > 0 ? g_L.R : 0.10f * fl::ScreenHeight(); }

const Model& CurModel() { return g_m; }
const Layout& CurLayout() { return g_L; }
const Context& CurContext() { return g_ctx; }

void Render(void* device)
{
    perf::Scope ps("hud2.Render");
    g_perf.drawn = 0;
    if (!g_visible || !device || g_menuOver)
        return;
    const double t0 = perf::Now();
    render::SetDevice(device);
    int sw = 0, sh = 0;
    if (!render::ScreenSize(sw, sh) || sh < 100)
        return;
    if (sw != static_cast<int>(g_L.W) || sh != static_cast<int>(g_L.H))
        ComputeLayout(static_cast<float>(sw), static_cast<float>(sh));
    const Layout& L = g_L;
    const int textPx = std::max(8, static_cast<int>(0.026f * L.H * L.k + 0.5f));
    if (!hud2kit::Begin(device, sw, sh, textPx))
        return;
    LintPassBegin(); // (debug `hud2 lint`: the kit records this pass's items while armed; hud2_lint_live.cpp)
    flicons::Ready(device);
    const float base = g_t * Clampf(uiopt::Get().hudOpacity / 100.0f, 0.4f, 1.0f);
    // FLUF's windows over a part (flufaware.h, fluf_windows): the part steps back under them - FLUF's ImGui draws over
    // FLvibed's overlay and takes the mouse there; eased; 1 everywhere without FLUF
    using flufaware::Dim;
    const float fNav = Dim(flufaware::SlotNav, L.navX0, L.navY0, L.navX1, L.navY1, L.W, L.H);
    const float fTgt = Dim(flufaware::SlotTarget, L.tgtX0, L.tgtY0, L.tgtX1, ListShown() ? L.thrY1 : L.tgtY1, L.W, L.H);
    const float fShip = Dim(flufaware::SlotShip, L.shipX0, L.shipY0, L.shipX1, L.shipY1, L.W, L.H);
    const float fLo = Dim(flufaware::SlotLoadout, L.loX0, L.consY1 > L.consY0 ? std::min(L.loY0, L.consY0) : L.loY0,
                          L.loX1, L.loY1, L.W, L.H); // (with the cross stacked on it: decision 3)
    const float fRing = Dim(flufaware::SlotRing, L.cx - L.R, L.cy - L.R, L.cx + L.R, L.cy + L.R, L.W, L.H);
    const float fCtx = Dim(flufaware::SlotContext, L.ctxX0, L.ctxTop, L.ctxX1, L.ctxY + 0.03f * L.H, L.W, L.H);
    const float fAlert = Dim(flufaware::SlotAlert, L.alertX0, L.alertY, L.alertX1, L.alertY1, L.W, L.H);
    const float s = g_f.slide * 0.01f * L.H; // the transition's slide toward each corner
    auto part = [&](float dx, float dy) { SetOffset(dx * s, dy * s); };
    // the keyboard form's hotspots are recorded while the parts draw (hud2_click.cpp; handed to the host by Update)
    HotStart(g_m.kbm && g_ctx.space && !g_ctx.modal && g_t > 0.5f);
    g_m.tView = hudvis::ViewShown(1), g_m.shipView = hudvis::ViewShown(0); // drawn this frame under the HUD
    // the goal panel / picker / wheel open over the right half: the threat / contact list under them steps back (its rows
    // read through the goal panel's glass, 25 Oct 2026)
    const float listDim = g_ctx.modal ? 0.12f : 1.0f;
    // One value, one place (the lint's value.twice; UI_HUD2_PRESETS.md 1.7: "a value may move (handover), never show in
    // two visible places at once"): a goto / dock chip's object IS the selected target (manDest = Upper(tName)), a lane's
    // far end may be - while the card names it, the chip's object hands over to the card (the card's fade: 1 = the card
    // alone, 0 = the chip's again, e.g. in the letterbox where only the chip stays). The card is the target's home (1.7
    // "identity ... at the thing"; 2.1 Other contexts: "Dock: chip + queue / bay", "lane: chip + bar + ETA").
    g_m.destOnCard = g_m.target && !g_m.manDest.empty() && Upper(g_m.tName) == g_m.manDest
                         ? Clampf(g_f.target / 0.75f, 0.0f, 1.0f) // (0.75 = the card's level on a lane ride)
                         : 0.0f;
    // ...and to the ship panel's escort row naming the same ship (a formation's leader is an escort row - its home, with
    // its hull: the lint's value.twice on `hud2 sim formation`, 31 Oct 2026)
    if (!g_m.manDest.empty())
        for (int i = 0; i < g_m.nEscorts && i < 3; ++i)
            if (Upper(g_m.escorts[i].name) == g_m.manDest)
                g_m.destOnCard = 1.0f;
    // Focus (UI_HUD2_PRESETS.md 2.2): its own nav form, card, bottom-left text, fire strip and reticle (hud2_parts.cpp's
    // Focus parts); the context line, the alert and the keyboard form's list as Standard's
    const bool focus = g_m.presetDrawn == hud2preset::Focus;
    part(-1, -1);
    RecordTag("nav");
    {
        NavStyle ns;
        if (focus)
            ns.lean = true, ns.full = g_rv.navFull,
            ns.headline = uimeta::hud2p::Num(hud2preset::Data(hud2preset::Focus), "calm", "nav", "text_h", 0.030f);
        ns.scrimR = g_navScrim[2], ns.scrimB = g_navScrim[3], ns.textOut = g_navScrim;
        g_navRight = DrawNav(g_m, L, base * fNav * g_f.nav, ns, &g_navBottom); // (the slide is an offset: the edge is without it)
    }
    if (g_f.target > 0.01f)
    {
        part(1, -1);
        RecordTag("card");
        if (focus) // (the pad's threat rows inside it, faded in from the fight form's 80 %)
            DrawTargetCompact(g_m, L, base * g_f.target * fTgt,
                              ListShown() ? listDim * Clampf((L.form - 0.8f) / 0.2f, 0.0f, 1.0f) : 0.0f, g_rv.fight);
        else
            DrawTarget(g_m, L, base * g_f.target * fTgt);
        RecordTag("card.icons");
        if (g_m.kbm)
            DrawCardIcons(g_m, L, base * g_f.target * fTgt, focus);
        RecordTag("list");
        if (!g_m.kbm && ListShown() && !focus)
            DrawList(g_m, L, base * g_f.target * listDim * fTgt);
    }
    if (!g_m.target && g_f.target < 0.99f) // the card's corner without a card: the left bumper's hints (pad)
    {
        part(1, -1);
        RecordTag("hints");
        DrawCalmHints(g_m, L, base * (1 - g_f.target) * g_f.wakeHints * fTgt,
                      focus ? uimeta::hud2p::Num(hud2preset::Data(hud2preset::Focus), "calm", "hints", "text_h", 0.026f)
                            : 0.024f);
    }
    if (g_f.enemyCue > 0.01f) // (not under the contextual fade: it shows for as long as enemies are near)
    {
        part(1, -1);
        RecordTag("enemycue");
        DrawEnemyCue(g_m, L, base * g_f.enemyCue * (1 - g_f.target) * fTgt);
    }
    if (g_m.kbm && ListShown()) // the keyboard form's list stands without a target too
    {
        part(1, -1);
        RecordTag("list");
        DrawList(g_m, L, base * (g_m.target ? std::max(g_f.target, 0.5f) : 1.0f) * g_f.wakeTarget * listDim * fTgt);
    }
    if (g_f.ship > 0.01f)
    {
        part(-1, 1);
        RecordTag("ship");
        if (focus)
            DrawShipStrip(g_m, L, base * g_f.ship * fShip, g_f.escorts);
        else
            DrawShip(g_m, L, base * g_f.ship * fShip);
    }
    if (g_f.loadout > 0.01f)
    {
        part(1, 1);
        RecordTag("loadout");
        const float lo = base * g_f.loadout * (promptbar::Visible() ? 0.25f : 1.0f) * fLo; // the prompt bar sits over it
        if (focus) // (in a fight the launcher's count is the band's MSL n: one home, 1.7 pair 5)
            DrawFireStrip(g_m, L, lo, !g_rv.fight);
        else
            DrawLoadout(g_m, L, lo);
    }
    part(0, 0);
    RecordTag("ring"); // (DrawRing tags its elements itself: ring.shield, ring.speed, ring.cruise, ...)
    if (focus)
    {
        FocusVis v;
        v.shield = g_f.arcShield, v.hull = g_f.arcHull, v.power = g_f.arcPower, v.band = g_f.band, v.speed = g_f.speed;
        v.fight = g_rv.fight, v.readout = g_focusReadout;
        DrawSplitArcs(g_m, L, base * fRing, v);
    }
    else if (g_f.ring > 0.01f || true)
        DrawRing(g_m, L, base * fRing * (g_m.presetDrawn == hud2preset::Focus || uiopt::Get().hudFade <= 0 ? 1.0f : g_f.ring),
                 g_f.ring); // (CONTEXTUAL, 0.2.1: the speed and the cruise readout go with the ring - DrawRing keeps
                            // them at 60 % of a dimmed ring, the old CALM DIMS)
    if (g_f.context > 0.01f && g_m.nCtx)
    {
        part(0, 1);
        RecordTag("context");
        DrawContext(g_m, L, base * g_f.context * fCtx, false);
    }
    if (g_f.alert > 0.01f && !g_m.alert.empty())
    {
        part(0, -1);
        RecordTag("alert");
        DrawAlert(g_m, L, base * g_f.alert * fAlert, false);
    }
    LintPassEnd();
    hud2kit::End();
    g_perf.drawn = 1;
    g_perf.render = g_perf.render * 0.95f + static_cast<float>((perf::Now() - t0) * 1000.0) * 0.05f;
}

std::string Command(std::istream& in)
{
    std::string what;
    in >> what;
    if (what == "on" || what == "off" || what == "auto")
    {
        g_force = what == "on" ? 1 : what == "off" ? 0 : -1;
        return "hud2 " + what;
    }
    if (what == "hit") // hud2 hit <x> <y>: the control the game's hit test finds at a pixel (the hotspot question)
    {
        float x = 0, y = 0;
        in >> x >> y;
        const uintptr_t c = ui::HitTest(x, y);
        char b[96];
        sprintf_s(b, "hit %.0f %.0f -> %s (%08X)", x, y, c ? ui::ControlName(c) : "-", static_cast<u32>(c));
        return b;
    }
    if (what == "hot" || what == "click") // the keyboard form's hotspots / a click's action (hud2_click.cpp)
        return HotCommand(what, in);
    if (what == "lint") // hud2 lint [off]: the layout lint over the last drawn frame (hud2_lint_live.cpp)
        return LintCommand(in);
    if (what == "list") // hud2 list [on|off|0..4]: the keyboard form's contact list mode and its filter
    {
        std::string v;
        in >> v;
        if (v == "on" || v == "off")
            g_m.list = v == "on";
        else if (!v.empty())
            g_m.list = true, g_m.listFilter = std::clamp(atoi(v.c_str()), 0, 4);
        g_m.listFirst = 0, g_slow = 1;
        return "hud2 list=" + std::to_string(g_m.list) + " filter=" + std::to_string(g_m.listFilter);
    }
    if (what == "viewzoom") // hud2 viewzoom <f>: the views' rectangles over their windows (research)
    {
        float f = 0;
        in >> f;
        if (f > 0.5f && f < 4)
            kViewZoom = f;
        return "hud2 viewzoom=" + std::to_string(kViewZoom);
    }
    if (what == "viewback") // hud2 viewback on|off: the dark backing under the game's 3D views (research)
    {
        std::string v;
        in >> v;
        if (!v.empty())
            g_viewBackOn = v != "off";
        return "hud2 viewback=" + std::to_string(g_viewBackOn) + " drawn=" + std::to_string(g_viewBackDrawn) +
               " pulses=" + std::to_string(g_viewPulses) + " viewObj=" + std::to_string(hudvis::TargetViewObject()) +
               " target=" + std::to_string(g_m.targetId);
    }
    if (what == "tree") // hud2 tree <root label>: the native panel's controls (the wireframe question)
    {
        std::string label;
        in >> label;
        return hudvis::Tree(label);
    }
    if (what == "peek") // hud2 peek [pin|off]: Focus's manual peek (UI_HUD2_PRESETS.md 1.6 2; the inputs are step 7's)
    {
        std::string v;
        in >> v;
        if (g_m.presetDrawn != hud2preset::Focus)
            return "hud2 peek: n/a: the drawn layout (" + std::string(hud2preset::Data(g_m.presetDrawn).name) +
                   ") holds nothing back (hud_preset 1 = Focus)";
        if (v == "off")
            g_reveal.pinned = false, g_reveal.peekLeft = 0;
        else if (v == "pin")
            g_reveal.pinned = true;
        else
            g_reveal.Peek(hud2layout::RevealRulesOf(hud2preset::Data(g_m.presetDrawn)));
        char b[96];
        snprintf(b, sizeof b, "hud2 peek: %s (%.1f s left)", g_reveal.pinned ? "pinned" : g_reveal.peekLeft > 0 ? "on" : "off",
                 g_reveal.peekLeft);
        return b;
    }
    if (what == "sim")
    {
        std::string s;
        in >> s;
        if (s == "list")
            return "hud2 sim " + hud2sim::Names() + " [+comm on any: the comm surface shows] | off (docs\\UI_HUD2_PRESETS.md 6 step 2)";
        // <name>[+comm]: an unknown name or modifier is refused (the sim stays as it was)
        const hud2sim::Parsed p = hud2sim::Parse(s);
        if (!p.ok)
            return "hud2 sim: unknown '" + s + "' (names: " + hud2sim::Names() + ", +comm; off)";
        g_sim = p.base, g_simComm = p.comm;
        g_objSeq = 0xFFFFFFFF, g_m.place.clear(), g_m.placeLeft = 0, g_m.alertLeft = 0; // the live values again
        // (Focus: the reveal starts over - a sim's own values are not a change that wakes a part, the lint samples settled)
        g_reveal = hud2layout::Reveal();
        g_slow = 1;
        return "hud2 sim " + (g_sim.empty() ? std::string("off (live data)") : hud2sim::Label(g_sim, g_simComm));
    }
    std::ostringstream o;
    const uiopt::UiOptions& u = uiopt::Get();
    o << "hud2 active=" << g_active << " visible=" << g_visible << " t=" << g_t << " force=" << g_force
      << " sim=" << hud2sim::Label(g_sim, g_simComm) << " switches=" << g_switches << " state=" << StateName(g_m.state)
      << " kbm=" << g_m.kbm << " combat=" << g_combat << " cursor=" << (g_cursorHiddenNote ? "hidden" : "shown")
      << "\n";
    o << "settings style=" << u.hudStyle << " fade=" << u.hudFade << " opacity=" << u.hudOpacity << " scale="
      << u.hudScale << " numbers=" << u.hudNumbers << " threats=" << u.hudThreats << " context=" << u.hudContext
      << " prompts=" << u.hudPrompts << (g_promptShow.left > 0 ? "(shown)" : "(hidden)") << " alerts=" << u.hudAlerts << " info=" << u.hudInfo << " wireframes=" << u.hudWireframes
      << " transition=" << u.hudTransition << "\n";
    // the layout preset (UI_HUD2_PRESETS.md 6 step 4): asked / drawn, its data's size, the controller setup's read
    o << "preset=" << hud2preset::Label(g_m.preset) << " hud_preset=" << g_m.preset << " drawn=" << g_m.presetDrawn
      << " zones=" << g_m.presetData->zoneCount << " budgets=" << g_m.presetData->budgetCount
      << " pick_r=" << g_m.presetData->pickR << " | " << hud2preset::Describe(g_m.adapt) << "\n";
    o << "ring hull=" << g_m.hull << " shield=" << g_m.shield << " power=" << g_m.power << " speed=" << g_m.speed
      << " cruise=" << g_m.cruise << " charge=" << g_m.charge << " missiles=" << g_m.missiles << " hitAge=" << g_m.hitAge
      << "\n";
    o << "nav objective='" << text::Utf8(g_m.objective) << "' man=" << g_m.man << " '" << text::Utf8(g_m.manName) << " "
      << text::Utf8(g_m.manDest) << "' progress=" << g_m.progress << " eta='" << text::Utf8(g_m.eta) << "' ap=" << g_m.ap
      << " '" << text::Utf8(g_m.apMode) << "' wait='" << text::Utf8(g_m.wait) << "' info='" << text::Utf8(g_m.info)
      << "' event='" << text::Utf8(g_m.event) << "' comm=" << g_m.comm << " cine=" << g_m.cine << " fight=" << g_m.fight
      << " place='" << text::Utf8(g_m.place) << "' protect=" << g_m.protect << " refs=";
    for (u32 r : g_objRefs)
        o << r << ",";
    o << "\n";
    o << "target=" << g_m.target << " '" << text::Utf8(g_m.tName) << "' " << text::Utf8(g_m.tAtt) << " class='"
      << text::Utf8(g_m.tClass) << "' faction='" << text::Utf8(g_m.tFaction) << "' level=" << g_m.tLevel << " dist="
      << text::Utf8(g_m.tDist) << " bars=" << g_m.tBars << " shield=" << g_m.tShield << " hull=" << g_m.tHull
      << " threats=" << g_m.nThreats << " list=" << g_m.list << " filter=" << text::Utf8(g_m.filter) << " total="
      << g_m.listTotal << " first=" << g_m.listFirst << "\n";
    o << "ship name='" << text::Utf8(g_m.shipName) << "' class='" << text::Utf8(g_m.shipClass) << "' realGroups="
      << g_m.realGroups << " ungrouped='" << text::Utf8(g_m.ungrouped) << "' manRun=" << g_m.manRun << "\n";
    o << "ship escorts=" << g_m.nEscorts << " hold=" << g_m.holdUsed << "/" << g_m.holdCap << " credits=" << g_m.credits
      << " | loadout nano=" << g_m.nano << " batt=" << g_m.batt << " cm=" << g_m.cm << " mines=" << g_m.mines
      << " groups=" << g_m.nGroups << " active=" << g_m.activeGroup + 1 << " '" << text::Utf8(g_m.groups[0])
      << "' launcher='" << text::Utf8(g_m.launcher) << "' ammo=" << g_m.ammo << " torpedo='" << text::Utf8(g_m.torpedo)
      << "' torpedoAmmo=" << g_m.torpedoAmmo << " locked=" << g_m.locked << "\n";
    o << "context:";
    for (int i = 0; i < g_m.nCtx; ++i)
        o << " [" << (g_m.ctx[i].key.empty() ? glyphs::Name(g_m.ctx[i].btn) : text::Utf8(g_m.ctx[i].key))
          << (g_m.ctx[i].hold ? "(hold)" : "") << "] " << text::Utf8(g_m.ctx[i].label);
    o << " | hold=" << promptfit::HoldStepName(g_L.ctxPlan.step);
    o << " | alert='" << text::Utf8(g_m.alert) << "' left=" << g_m.alertLeft << " loot=" << g_m.loot << " lootNearest='"
      << text::Utf8(g_m.lootName) << "' " << text::Utf8(g_m.lootDist) << " enemies=" << g_m.enemies
      << " enemyCue=" << (EnemyCue() ? 1 : 0) << " prompts=";
    for (int i = 0; i < g_m.nCtx; ++i)
        o << (i ? " | " : "") << (g_m.ctx[i].btn != glyphs::COUNT ? glyphs::Name(g_m.ctx[i].btn) : "")
          << text::Utf8(g_m.ctx[i].key) << " "
          << text::Utf8(g_m.ctx[i].label);
    o << "\n";
    o << "lane entry=" << g_laneKey[0] << " exit=" << g_laneKey[1] << " final=" << g_laneFinal << " rings="
      << g_laneRings.size() << "\n";
    o << "wake shown=" << g_wakeShown << " (1 ring 2 ship 4 loadout 8 target 16 nav)\n";
    o << "fades nav=" << g_f.nav << " ring=" << g_f.ring << " target=" << g_f.target << " ship=" << g_f.ship << " loadout=" << g_f.loadout
      << " context=" << g_f.context << " alert=" << g_f.alert << "\n";
    {
        // the layout's zones and whether any two meet (the corners, the ring between them): the aspect check's evidence
        const Layout& L = g_L;
        struct Z
        {
            const char* n;
            float x0, y0, x1, y1;
        } z[] = {{"nav", L.navX0, L.navY0, L.navX1, L.navY1},
                 {"card", L.tgtX0, L.tgtY0, L.tgtX1, std::max(L.tgtY1, L.thrY1)},
                 {"ring", L.cx - L.R, L.cy - L.R, L.cx + L.R, L.cy + L.R},
                 {"ship", L.shipX0, L.shipY0, L.shipX1, L.shipY1},
                 {"loadout", L.loX0, L.loY0, L.loX1, L.loY1},
                 {"cons", L.consX0, L.consY0, L.consX1, L.consY1}}; // (the cross stacked on the loadout: decision 3)
        o << "layout " << L.W << "x" << L.H << " k=" << L.k << " aspect=" << L.aspect;
        for (const Z& a : z)
            o << " " << a.n << "=" << static_cast<int>(a.x0) << "," << static_cast<int>(a.y0) << ","
              << static_cast<int>(a.x1) << "," << static_cast<int>(a.y1);
        // the context prompts' lanes (decision 4): each kept prompt's lane / row and its place, squeezed = less than asked
        o << " ctx=" << (g_core.ctxFallback ? "fallback" : "lanes");
        for (int i = 0; i < g_core.nCtx && i < 3; ++i)
            o << " [" << (g_core.ctxSide[i] < 0 ? "C" : g_core.ctxSide[i] ? "R" : "L")
              << (g_core.ctxPY[i] < g_core.ctxY - 1 ? "1" : "0") << " " << static_cast<int>(g_core.ctxX[i]) << "+"
              << static_cast<int>(g_core.ctxW[i]) << (g_core.ctxWrap[i] ? " wrap" : "") << "]";
        o << " overlaps=";
        int n = 0;
        for (size_t i = 0; i < std::size(z); ++i)
            for (size_t j = i + 1; j < std::size(z); ++j)
                if (z[i].x0 < z[j].x1 && z[j].x0 < z[i].x1 && z[i].y0 < z[j].y1 && z[j].y0 < z[i].y1)
                    o << (n++ ? "," : "") << z[i].n << "/" << z[j].n;
        o << (n ? "" : "none") << " gaps top=" << static_cast<int>(L.tgtX0 - L.navMaxX)
          << " bottom=" << static_cast<int>(L.loX0 - L.shipX1) << "\n";
        // the layout core (hud2_layout.h): the drawn preset, what its rules suppress, the boxed area against its cap
        const hud2layout::Out& core = g_core;
        o << "core preset=" << hud2preset::Data(g_m.presetDrawn).name << " suppressed=";
        int nOff = 0;
        for (int e = 0; e < hud2layout::kElemCount; ++e)
            if (core.suppressed & hud2layout::Bit(e))
                o << (nOff++ ? "," : "") << hud2layout::ElemName(e)
                  << ((core.budgetOff & hud2layout::Bit(e)) ? "(budget)" : "");
        char coreLine[96];
        snprintf(coreLine, sizeof coreLine, " boxed=%.1f%% cap=%.1f%% overrides=%d", core.boxed * 100, core.budget * 100,
                 g_m.presetDrawn == hud2preset::Standard ? hud2layout::kStandardOverrides : 0);
        o << (nOff ? "" : "-") << coreLine << "\n";
        if (g_m.presetDrawn == hud2preset::Focus) // Focus's reveal (hud2_layout.h Reveal)
        {
            char fl[200];
            snprintf(fl, sizeof fl,
                     "focus held=%s%s peek=%d%s fight=%d form=%.2f arcs=%.2f/%.2f/%.2f speed=%d escorts=%d "
                     "navFull=%d readout=%d\n",
                     (g_rv.held & hud2layout::Bit(hud2layout::Ship)) ? "ship," : "",
                     (g_rv.held & hud2layout::Bit(hud2layout::Loadout)) ? "loadout" : (g_rv.held ? "" : "-"),
                     g_rv.peek ? 1 : 0, g_reveal.pinned ? "(pinned)" : "", g_rv.fight ? 1 : 0, g_rv.form, g_rv.arcShield,
                     g_rv.arcHull, g_rv.arcPower, g_rv.speed ? 1 : 0, g_rv.escorts ? 1 : 0, g_rv.navFull ? 1 : 0,
                     g_focusReadout ? 1 : 0);
            o << fl;
        }
    }
    o << "perf update=" << g_perf.update << "ms render=" << g_perf.render << "ms (smoothed) drawn=" << g_perf.drawn
      << " | " << hud2kit::Status() << " | " << flicons::Status() << "\n";
    o << hudvis::Status();
    return o.str();
}

void OnDeviceLost() { hud2kit::OnDeviceLost(); }

void Shutdown()
{
    SetMenuOver(false);
    hudvis::Apply(false);
    hudvis::Invalidate();
    hud2kit::Shutdown();
    flicons::Shutdown();
}
} // namespace hud2
