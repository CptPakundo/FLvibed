// HUD 2's model: the data at its own rates (Fast, Slow), the key names, prompts and alerts, the mock-ups. hud2.cpp runs the frame.
#include "hud2.h"

#include "dimouse.h"
#include "game/fl.h"
#include "game/aim.h"
#include "game/apmode.h"
#include "ui/views.h"
#include "flexe.h"
#include "ui/flufaware.h" // FLUF's windows: the parts under them step back
#include "game/ai.h"
#include "game/flicons.h"
#include "game/flight.h"
#include "game/hudinfo.h"
#include "hud2_cue.h"
#include "ui/hud_rows.h"
#include "game/indicators.h"
#include "game/render.h"
#include "game/shipname.h"
#include "game/space.h"
#include "game/tractorfail.h"
#include "game/promptshow.h"
#include "game/hudwake.h"
#include "hudhost.h"
#include "hud2_impl.h"
#include "hud2_layout.h"
#include "hud2_sim.h"
#include "hud2_data.h"
#include "hud_bus.h"
#include "hudvis.h"
#include "log.h"
#include "settings_bus.h"
#include "str.h"
#include "text.h"
#include "ui/ui.h"
#include "ui/uiopt.h"

#include <sstream>

// (ASCII only: the build reads the sources as cp1252, non-ASCII text goes in as \u escapes)
namespace hud2
{
using namespace hud2kit;
static const wchar_t kDot[] = L" \u00B7 ";
std::wstring Upper(std::wstring s) { return str::Upper(std::move(s)); }
static std::wstring Dist(float m)
{
    wchar_t b[32];
    if (m >= 1000)
        swprintf_s(b, L"%.1f km", m / 1000);
    else
        swprintf_s(b, L"%.0f m", m);
    return b;
}
// The distance the game's own label reads, so card, bracket and cue agree on one number: to the surface, clamped at 0 (docs\FREELANCER_RE.md).
static std::wstring SurfaceDist(const space::Contact& c) { return Dist(std::max(0.0f, c.distance - hudinfo::ObjectRadius(c.id))); }
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
bool FireKeyPressed()
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
        strcpy_s(v, *views::ApKeyName() ? views::ApKeyName() : "K"); // (the controller off: the row is in its file, read by the bar button)
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

long long g_creditsTest = -1; // debug `hud2 credits <n>`: the ship panel's number (-1: the player's)

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

// The lane a context prompt takes (decision 4: the prompts hug the bottom-left and the bottom-
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
    case A::TargetNearest:
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
// 2026, UI_HUD2.md 5.23). The "the tractor prompts at the bottom of the screen [should] note the
// object's name and range. Also, it's not prompted anywhere that the A button also tractors something. Prompting should
// clarify which prompt does what (tractor specific, tractor all, etc)". So: [A] TRACTOR <item> . <range> - one item, the
// targeted loot in reach or, with no target, the nearest loot in reach (ContextAction; `aFree`: the state leaves A to
// the context, and not in ASSISTED, whose A starts the autopilot; the keyboard's key tractors only a target). [X] TRACTOR
// ALL n - everything in reach; with one item in reach that A does not name, [X] names it. Focus: A's prompt only (its
// band shows LOOT n with X's glyph, PRESETS 2.2). Within the line's three prompts and its one-prompt-per-button rule
// (combat's X-hold TURRET keeps the X: the round 3 critique). Once a frame: a second call with `aFree` false would add
// [X] naming the item [A] already names.
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

static const wchar_t* ModeVerb(apmode::Mode m)
{
    switch (m)
    {
    case apmode::Mode::Goto: return L"FLY TO";
    case apmode::Mode::Dock: return L"DOCK AT";
    case apmode::Mode::Follow: return L"FOLLOW";
    case apmode::Mode::Formation: return L"FORMATION WITH";
    case apmode::Mode::Face: return L"FACE";
    case apmode::Mode::Escort: return L"ESCORT";
    case apmode::Mode::Combat: return L"FIGHT";
    case apmode::Mode::Travel: return L"TRAVEL TO";
    case apmode::Mode::Hold: return L"HOLD";
    case apmode::Mode::Shoot: return L"SHOOT";
    default: return L"";
    }
}
static const wchar_t* WaitReason() { return views::ApWaitReason(); }

// ---------------------------------------------------------------- the data
static float Bearing(const float pos[3], const fl::Kinematics& k)
{
    const float r[3] = {pos[0] - k.pos[0], pos[1] - k.pos[1], pos[2] - k.pos[2]};
    // columns of rot = the ship's right / up / back axes
    const float x = r[0] * k.rot[0] + r[1] * k.rot[3] + r[2] * k.rot[6];
    const float y = r[0] * k.rot[1] + r[1] * k.rot[4] + r[2] * k.rot[7];
    return atan2f(-y, x) * 180.0f / 3.14159265f;
}

// The weapon group cue's marks (UI_HUD2_GROUP_CUE.md): the groups that have guns, then M and T while the ship carries them. A cruise disruptor on the torpedo
// hardpoint gets no letter: T says torpedo, and it does no damage to aim for. Any read failing leaves a mark unlit, an empty row hides.
static void FillCue(Model& m, bool flying)
{
    namespace hc = hud2cue;
    m.nCue = 0, m.cueDigits = 0, m.cueShow = false;
    if (!hudrows::g.groupCue || !flying)
        return;
    const aim::Target t = aim::Current();
    const float W = static_cast<float>(fl::ScreenWidth()), Hs = static_cast<float>(fl::ScreenHeight());
    // the aim point: the screen's centre on the pad's gimbal, the cursor on keyboard and mouse
    const float ax = m.kbm ? static_cast<float>(fl::MouseX()) : W / 2, ay = m.kbm ? static_cast<float>(fl::MouseY()) : Hs / 2;
    const float ring = g_L.R > 0 ? g_L.R : 0.10f * Hs;
    const bool onTarget = m.target && hud2cue::InCentre(t.screenX, t.screenY, t.onScreen, m.tMark.x, m.tMark.y, m.tMark.r, m.tMark.onScreen, ax, ay, ring);
    const float dist = std::max(0.0f, t.dist - t.radius);
    space::GunFire fire[16];
    const int nFire = space::GunFireAt(t.world, fire, 16);
    const std::vector<space::Weapon> pw = space::PlayerWeapons();
    const std::vector<hudinfo::WeaponInfo> wi = hudinfo::Weapons();
    auto fireOf = [&](const space::Weapon& w, hc::Gun& g)
    {
        g.ammo = w.ammo;
        for (int i = 0; i < nFire; ++i)
            if (_stricmp(fire[i].hardpoint, w.hardpoint) == 0)
                g.reach = fire[i].reach, g.fire = fire[i].fire;
    };
    const int ag = hudinfo::ActiveWeaponGroup();
    const int activeNum = ag > 0 ? ag : g_ctx.weaponGroup;
    const bool two = uiopt::Get().weaponGroups == 2;
    bool hide = false;
    int nCue = 0;
    auto add = [&](wchar_t ch, const hc::Gun* guns, int n, bool selected, bool locked, float reload)
    {
        const hc::Verdict v = hc::Judge(guns, n, onTarget, dist);
        hide = hide || v == hc::Hide;
        Model::CueMark& c = m.cue[nCue];
        c.ch = ch, c.selected = selected, c.amber = v == hc::Amber, c.locked = locked, c.reload = reload, c.want = v == hc::Lit;
        ++nCue;
    };
    // the groups: the rows the loadout shows, each by the game's own number
    for (int r = 0; r < m.nGroups && r < 3; ++r)
    {
        const int num = m.realGroups ? m.groupNum[r] : 0;
        hc::Gun guns[16];
        int n = 0;
        for (const space::Weapon& w : pw)
        {
            if (w.homing || n >= 16)
                continue;
            bool member = !m.realGroups;
            for (const hudinfo::WeaponInfo& info : wi)
                if (m.realGroups && !info.launcher && info.arch == w.archId && (info.groups & (1u << (num - 1))))
                    member = true;
            if (member)
                fireOf(w, guns[n++]);
        }
        const bool selected = two ? num == 1 || num == 2 : r == m.activeGroup || num == activeNum;
        add(static_cast<wchar_t>(L'0' + (num ? num : r + 1)), guns, n, selected, false, 0);
    }
    m.cueDigits = nCue;
    // M and T: rounds by ammo (the active flag reads 0 for launchers), the reach a LOCK seeker's
    for (const bool torpedo : {false, true})
    {
        hc::Gun guns[8];
        int n = 0;
        float reload = 0;
        for (const space::Weapon& w : pw)
        {
            const bool onTorpedo = _strnicmp(w.hardpoint, "HpTorpedo", 9) == 0;
            if (!w.homing || w.cruiseDisruptor || onTorpedo != torpedo || n >= 8)
                continue;
            fireOf(w, guns[n++]);
            if (w.refire > 0 && w.elapsed < w.refire)
                reload = std::max(reload, w.elapsed / w.refire);
        }
        if (n && nCue < hc::kMaxMarks)
            add(torpedo ? L'T' : L'M', guns, n, false, m.locked, reload);
    }
    m.nCue = nCue;
    // shown with a target or in a fight; never in calm travel, a cruise or a lane
    const bool want = m.target || g_combat;
    m.cueShow = want && !hide && m.cruise == 0 && !m.cine && !(m.man && (m.manName == L"TRADE LANE" || m.manName == L"LANE" || m.manName == L"DOCK"));
    if (m.presetDrawn == hud2preset::Focus)
        m.cueShow = m.cueShow && (m.tCol == kHostile || g_combat);
}

static ai::Self g_self;
static bool g_haveSelf = false;
static fl::Kinematics g_kin;
static bool g_haveKin = false;
static u32 g_hitSeq = 0;
u32 g_laneKey[2] = {}, g_laneFinal = 0; // the lane ride fallback (Slow)
std::vector<u32> g_laneRings, g_laneEnds[2];
static float g_move[3] = {}; // the ship's last 10 Hz step (server position)
static float g_posSpeed = 0; // the ship's speed from its server position (10 Hz; the lane ride)
u32 g_objSeq = 0xFFFFFFFF;
std::vector<u32> g_objRefs;
static u32 g_system = 0;
bool g_combat = false;
// the running maneuver's start distance (progress)
static int g_manKind = 0;
static u32 g_manObj = 0;
static float g_manD0 = 0;
// alerts' edges
static float g_prevShield = 1, g_prevHull = 1, g_prevFree = 1;
static int g_prevMissiles = 0;
double g_spaceSince = 0; // when the ship last entered space (a load, a launch): the alerts arm after it

void ClearAlerts()
{
    g_m.alertLeft = 0;
    for (AlertCard& h : g_m.held)
        h.left = 0;
}

// A running card beside the ring stays when a new alert comes: the dangers are independent, and an alert fires once at its
// edge, so a replaced card never came back. The newest takes the top of its side's stack, the
// others go down a place (hud2_layout.h); the same alert again replaces its old card.
static bool KeepCard(const wchar_t* title)
{
    for (AlertCard& h : g_m.held)
        if (h.left > 0 && h.text == title)
            h.left = 0;
    if (g_m.held[0].left <= 0 && g_m.held[1].left > 0)
        g_m.held[0] = g_m.held[1], g_m.held[1].left = 0;
    if (g_m.alertLeft <= 0 || g_m.alert.empty() || !g_m.alertSide || g_m.alert == title)
        return false;
    if (g_m.held[0].left > 0)
        g_m.held[1] = g_m.held[0];
    AlertCard& h = g_m.held[0];
    h.text = g_m.alert, h.none = g_m.alertNone, h.col = g_m.alertCol, h.n = g_m.nAlertPrompts;
    h.left = g_m.alertLeft, h.side = g_m.alertSide, h.yNow = g_m.alertYNow;
    for (int i = 0; i < h.n && i < 2; ++i)
        h.prompts[i] = g_m.alertPrompts[i];
    return true;
}

void Alert(const wchar_t* title, u32 col, int side)
{
    if (!uiopt::Get().hudAlerts)
        return;
    const bool kept = KeepCard(title);
    g_m.alert = title, g_m.alertCol = col, g_m.alertLeft = 3.0f, g_m.nAlertPrompts = 0, g_m.alertNone.clear();
    if (!kept)
        g_m.alertSlots = 0; // (a kept card still names its slots)
    g_m.alertSide = side, g_m.alertYNow = -1;
}

// The stacks slide (continuous motion, never a step): each card eases to the place the layout gives it.
void SlideAlerts(float dt)
{
    // (skin_motion's fades and none: no travel, a card stands at its place at once)
    const bool still = hudrows::g.skinMotion != 0;
    auto slide = [dt, still](float& y, float to) { y = y < 0 || still ? to : y + (to - y) * std::min(1.0f, dt / 0.08f); };
    if (g_m.alertSide)
        slide(g_m.alertYNow, g_L.alertY);
    for (int i = 0; i < 2; ++i)
        if (g_m.held[i].left > 0 && g_L.heldShow[i])
            slide(g_m.held[i].yNow, g_L.heldY[i]);
}
// The maneuver lock's answer (0.2.1; a player report: in M01 the mission's Act_LockManeuvers made B / free flight do
// nothing, which read as a malfunction): the card in the warning amber, glyph-free, 2.5 s - not gated by hud_alerts,
// since it answers the player's own press
bool LockNotice()
{
    if (!Active())
        return false;
    KeepCard(L"LOCKED BY THE MISSION");
    g_m.alert = L"LOCKED BY THE MISSION", g_m.alertCol = kWarn, g_m.alertLeft = 2.5f, g_m.nAlertPrompts = 0;
    g_m.alertNone.clear(), g_m.alertSlots = 0, g_m.alertSide = 0;
    return true;
}

// The alert's answer when it is used up or not fitted: an amber word in the card instead of a glyph (the round 3
// critique: every alert carries its answer or says there is none)
void AlertNone(const wchar_t* s)
{
    if (g_m.nAlertPrompts == 0 && g_m.alertNone.empty())
        g_m.alertNone = s;
}
void AlertPrompt(presets::Action a, const wchar_t* label)
{
    if (g_m.nAlertPrompts < 2 && MakePrompt(g_m.alertPrompts[g_m.nAlertPrompts], a, label))
        ++g_m.nAlertPrompts;
}

// The game's "Tractor Failure" with its reason in the game's own words (GitHub issue 33): no text, no card, and the subtitles' line stays as it was.
bool AlertTractor(int reason)
{
    const std::wstring why = tractorfail::Shout(text::Ids(tractorfail::TextIds(reason)));
    if (why.empty())
        return false;
    Alert(L"TRACTOR FAILURE", kChrome);
    AlertNone(why.c_str());
    return true;
}
// A refusal since the last look: one compare in the slow pass while there is none.
static void TractorCard()
{
    static u32 seen = 0;
    const HudBus* bus = g_host->GetHudBus ? g_host->GetHudBus() : nullptr;
    if (!bus || bus->version < 6 || !bus->ReadTractorFail)
        return;
    int reason = 0;
    u32 age = 0;
    const u32 serial = bus->ReadTractorFail(&reason, &age);
    if (serial == seen)
        return;
    seen = serial;
    if (tractorfail::Fresh(age))
        AlertTractor(reason);
}

// The target on the screen. Again at the draw (RenderUnderHud): from the frame's start the caret trailed the ship by about a frame (motion p2l).
void ProjectTarget(Model& m)
{
    float c[3] = {}, radius = 0, s[3] = {}, e[3] = {}, rot[9] = {};
    m.tMark.onScreen = false;
    if (m.target && space::TargetBody(c, radius) && fl::WorldToScreen(c, s))
    {
        const float W = static_cast<float>(fl::ScreenWidth()), Hs = static_cast<float>(fl::ScreenHeight());
        m.tMark.x = s[0], m.tMark.y = s[1], m.tMark.onScreen = s[0] >= 0 && s[0] <= W && s[1] >= 0 && s[1] <= Hs;
        m.tMark.r = 0;
        if (fl::FlightScreenAxes(rot))
        {
            const float edge[3] = {c[0] + rot[0] * radius, c[1] + rot[3] * radius, c[2] + rot[6] * radius};
            if (fl::WorldToScreen(edge, e))
                m.tMark.r = sqrtf((e[0] - s[0]) * (e[0] - s[0]) + (e[1] - s[1]) * (e[1] - s[1]));
        }
    }
}

void Fast(float dt)
{
    Model& m = g_m;
    g_haveSelf = ai::ReadSelf(g_self);
    m.hull = g_haveSelf ? g_self.hull : -1;
    m.shield = g_haveSelf ? g_self.shield : -1;
    m.power = g_haveSelf ? g_self.power : -1;
    g_haveKin = fl::PlayerKinematics(g_kin);
    // the cue's fill: the verdict comes at 10 Hz, its hold and ease run per frame
    {
        static hud2cue::Hold holds[hud2cue::kMaxMarks];
        m.cueAnyLit = false;
        for (int i = 0; i < m.nCue && i < hud2cue::kMaxMarks; ++i)
        {
            m.cue[i].litA = holds[i].Step(m.cue[i].want, dt);
            m.cueAnyLit = m.cueAnyLit || m.cue[i].litA > 0.5f;
        }
    }
    // the targeting display's anchors, per frame: 10 Hz is too slow for a mark on a moving ship (docs\research\target-info-display.md 3.3)
    {
        ProjectTarget(m);
        indicators::Mark rec[64];
        const int n = m.target || m.nThreats ? indicators::Read(rec, 64) : 0;
        m.tWaypoint = false;
        int marks = 0;
        for (int i = 0; i < n; ++i)
        {
            if (rec[i].id == m.targetId && (rec[i].flags & 0x200))
                m.tWaypoint = true;
            for (int t = 0; t < m.nThreats && t < 4 && marks < kMaxMarks; ++t)
                if (rec[i].id && rec[i].id == m.threats[t].id && rec[i].id != m.targetId && rec[i].onScreen)
                {
                    Model::Mark& k = m.marks[marks++];
                    k.x = rec[i].x, k.y = rec[i].y, k.name = m.threats[t].name, k.dist = m.threats[t].dist, k.col = m.threats[t].col, k.hull = -1;
                }
        }
        m.nMarks = marks;
    }
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
    m.boost = hudinfo::ThrustGauge();
    m.cruiseWait = hudinfo::CruiseDisruptLeft();
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
    for (AlertCard& h : m.held)
        if (h.left > 0)
            h.left -= dt;
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
    const bool card = (m.alertLeft > 0 && !m.alert.empty()) || m.held[0].left > 0 || m.held[1].left > 0;
    for (int i = 0; i < 4; ++i)
    {
        const bool named = cond[i] || (card && (m.alertSlots & (1 << i)));
        m.hot[i] = named || counts[i] == 0;
        m.hotCol[i] = counts[i] > 0 ? kMeaning[i] : kWarn;
    }
}

void Slow()
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
        if (aim::NearbyEnemy(c))
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
        if (hostile && c.id != tid && m.nThreats < (m.kbm && !tid ? 4 : 2) && c.distance < 10000)
        {
            Model::Threat& t = m.threats[m.nThreats++];
            t.name = hudinfo::DisplayName(c.id); // as the game names it ("Unknown ..." for an unvisited solar)
            t.dist = SurfaceDist(c);
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
        m.tDist = SurfaceDist(c);
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
    {              // stacked on its right end, under the card's column, since the owner's decision 3
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
    const int nMission = std::max(0, views::ApMissionObjects(mission, 32, all));
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
    m.credits = g_creditsTest >= 0 ? g_creditsTest : hudinfo::Credits();
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
    m.fireBtn2 = Prompt();
    m.fireBtn2.btn = glyphs::COUNT;
    if (!MakePrompt(m.fireBtn, presets::Action::Fire, nullptr) && !m.kbm) // weapon_groups 2: a button per group
    {
        MakePrompt(m.fireBtn, presets::Action::FireGroup1, nullptr);
        MakePrompt(m.fireBtn2, presets::Action::FireGroup2, nullptr);
    }
    if (!MakePrompt(m.launchBtn, presets::Action::Missile, nullptr))
        MakePrompt(m.launchBtn, presets::Action::Torpedo, nullptr);
    m.nGroups = 0;
    // the torpedo hardpoints' launcher has a row of its own and is not one of the missiles: its rounds were added to the
    // launcher's count under the missile's name (GitHub issue 5). Those hardpoints also take cruise
    // disruptors: a torpedo wins the row, a disruptor shows when alone; the pad's hold fires either (HoldTorpedoSlot), the
    // keyboard has a key for each
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
        TorpedoPrompt(m, torpedoIsRound || !m.kbm ? presets::Action::Torpedo : presets::Action::CruiseDisruptor);
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

    {
        float cur = 0, mx = 0;
        bool up = true;
        m.shieldUp = !space::ShieldOf(me, cur, mx, up) || up;
    }
    FillCue(m, g_ctx.space && !g_ctx.modal);

    // ---- the game's own warnings, read from the alerts themselves (hudvis): the value the game uses, at the moment it uses it
    m.radiation = hudvis::AlertSeen(hudvis::kRadiation);
    m.lag = hudvis::AlertSeen(hudvis::kLag);
    m.tail = hudvis::AlertSeen(hudvis::kTail);
    // the game names no ship for the tail: the nearest ship astern within its 500 m, else straight behind, the bottom of the ring
    if (m.tail)
    {
        m.tailAng = 90;
        float best = 600;
        if (g_haveKin)
            for (const space::Contact& c : all)
            {
                const float back = (c.pos[0] - g_kin.pos[0]) * g_kin.rot[2] + (c.pos[1] - g_kin.pos[1]) * g_kin.rot[5] + (c.pos[2] - g_kin.pos[2]) * g_kin.rot[8];
                if (c.ship && c.id != me && c.distance < best && back > 0)
                    best = c.distance, m.tailAng = Bearing(c.pos, g_kin);
            }
    }

    // ---- the objective
    ObjectiveStep st = {};
    const u32 oseq = HostObjectiveNow(&st);
    // the text read at every pass, not only on a new sequence (a load restores the objective with the same sequence the
    // last session ended on: the headline stayed empty after loading a mission save)
    {
        static u32 textWas = 0xFFFFFFFF;
        if (oseq != g_objSeq || st.textId != textWas)
        {
            g_objSeq = oseq, textWas = st.textId;
            m.objective = hudinfo::ObjectiveLine(st.textId);
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
        const int nMarks = views::ApMissionObjects(marks, 32, all);
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
    // jump-in reads is_jumping: the chip blinked off for ~1 s at 0:00): held 2 s over such a gap
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
        const apmode::Mode am = views::ApMode();
        if (am != apmode::Mode::Off)
        {
            m.apMode = ModeVerb(am);
            if (views::ApTarget())
            {
                const std::wstring n = Upper(hudinfo::DisplayName(views::ApTarget()));
                if (!n.empty())
                    m.apMode += L" " + n;
            }
        }
        else if (views::ApObjectiveOn())
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
    // FLUF's windows (flufaware::OwnsPad): its buttons mean CLICK / CLOSE there, the prompt bar says so (A3:
    // "B REVERSE" showed under FLUF's nav map while B closed it)
    m.nCtx = 0;
    using A = presets::Action;
    // The cruise prompt lives in this line, in the left lane (CtxSide: the ship's movement) - the teaching lane at the
    // screen's bottom edge (UI_HUD2_PRESETS.md 1.6: the context line holds the teaching glyphs, "CRUISE in calm" among
    // them) - in every preset: Focus's left its band (B13: "the cruise prompt itself can live at
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
            else if (!m.target) // (RS: enemies first, else what is ahead - controller.cpp SmartTarget; the keyboard has
                                // no SmartTarget: its nearest-enemy key is the game's own)
                AddCtx(m.kbm && m.enemies > 0 ? A::TargetNearest : A::TargetSmart,
                       m.enemies > 0 ? L"NEAREST ENEMY" : L"TARGET AHEAD");
            if (m.kbm) // (the pad's LB TARGETS / WHEEL sit under the reticle ring in calm flight)
                AddCtx(A::TargetPicker, L"TARGETS");
            break;
        }
        // the tractor's prompts in every other state (calm placed them above), after the state's own prompts - they left
        // their spot under the reticle ring on 31 Oct 2026 (5.23); A's one-item form only where A is the context's (a
        // fight: B / X are REVERSE / TURRET)
        if (m.state != State::Calm)
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
        Alert(L"SHIELD FAILING", kHostile, -1);
        m.alertSlots |= 2;
        if (m.batt > 0)
            AlertPrompt(A::Battery, L"BATTERY");
        AlertNone(L"NO BATTERIES \x00B7 BREAK OFF");
    }
    if (m.hull >= 0 && m.hull <= 0.25f && g_prevHull > 0.25f)
    {
        Alert(L"HULL CRITICAL", kHostile, 1);
        m.alertSlots |= 1;
        if (m.nano > 0)
            AlertPrompt(A::Nanobot, L"NANOBOTS");
        AlertNone(L"NO NANOBOTS \x00B7 BREAK OFF");
    }
    if (m.missiles > g_prevMissiles)
    {
        Alert(L"MISSILE INCOMING", kHostile, 1);
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
    // entering a damaging zone: a card once, the nav's line after it
    {
        static bool radiationWas = false;
        if (m.radiation && !radiationWas && perf::Now() - g_spaceSince >= 1.5)
        {
            Alert(L"RADIATION", kAmber, 1);
            AlertNone(L"LEAVE THE ZONE");
        }
        radiationWas = m.radiation;
    }
    const float free = g_haveSelf ? g_self.holdFree : 1;
    if (g_haveSelf && free >= 0 && free < 1 && g_prevFree >= 1)
    {
        Alert(L"CARGO HOLD FULL", kChrome);
        AlertNone(L"NO ROOM FOR LOOT");
    }
    TractorCard();
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
void Simulate(Model& m)
{
    if (g_sim.empty())
        return;
    const bool longList = g_sim == "alertlist" || g_sim == "alertlistpad"; // (GitHub issue 35's scene, both forms)
    const bool combat = g_sim == "combat" || g_sim == "torpedo" || g_sim == "critical" || longList;
    m.hull = combat ? 0.88f : 1, m.shield = combat ? 0.22f : 1, m.power = combat ? 0.64f : 1, m.boost = combat ? 0.45f : 1;
    if (g_sim == "critical") // (the ring's low states: the flashes, the hull's bloom)
        m.hull = 0.18f, m.shield = 0.10f;
    m.shieldUp = g_sim != "shielddown";
    if (!m.shieldUp) // (the shield failed and rebuilds: its arc's pulse and DOWN)
        m.hull = 0.55f, m.shield = 0.08f;
    if (g_sim == "lowenergy") // (the energy meter near empty: both ends drained towards the middle, GitHub issue 28)
        m.power = 0.12f;
    m.speed = combat ? 84.0f : g_sim == "dock" ? 300.0f : g_sim == "lane" ? 2600.0f : 80.0f;
    m.cruise = g_sim == "cruise" ? 1 : g_sim == "dock" ? 2 : 0;
    m.cruiseWait = g_sim == "disrupt" ? 0.6f : 0; // (from "dock" to "disrupt": cruise broken, its bloom out and the red run-down)
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
    m.tail = g_sim == "hazard", m.tailAng = 130, m.radiation = g_sim == "hazard", m.lag = g_sim == "hazard";
    // the cue's states (UI_HUD2_GROUP_CUE.md 8), every one on the row at once
    m.nCue = 0, m.cueShow = false;
    if (g_sim == "groups")
    {
        const wchar_t chars[5] = {L'1', L'2', L'3', L'M', L'T'};
        for (int i = 0; i < 5; ++i)
            m.cue[i] = Model::CueMark(), m.cue[i].ch = chars[i];
        m.cue[0].selected = true, m.cue[0].litA = 1, m.cue[1].litA = 1, m.cue[3].litA = 1, m.cue[3].locked = true, m.cue[4].reload = 0.33f;
        m.nCue = 5, m.cueDigits = 3, m.cueShow = true, m.cueAnyLit = true;
    }
    m.nThreats = 0;
    m.alertLeft = 0, m.held[0].left = m.held[1].left = 0;
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
        m.alert = L"SHIELD FAILING", m.alertCol = kHostile, m.alertLeft = 3, m.nAlertPrompts = 0, m.alertSide = -1;
        m.alertNone.clear();
        AlertPrompt(A::Battery, L"BATTERY");
        m.hot[1] = true, m.hotCol[1] = kShield, m.hot[2] = true, m.hotCol[2] = kHostile;
        m.locked = true;
        // the targeting display on a target that stands still: the lint reads the form, its numbers as a live one's
        m.targetId = 424242, m.tMark.onScreen = true, m.tMark.x = 0.62f * g_L.W, m.tMark.y = 0.38f * g_L.H, m.tMark.r = 0.02f * g_L.H;
        AddCtx(A::Reverse, L"REVERSE");
        AddCtx(A::Turret, L"TURRET");
        if (longList)
        {
            static const wchar_t* const kNames[7] = {L"Von Claussen", L"Bernhard Trautermann", L"Wolf Vorkeuffer", L"Norman Dethmar",
                                                     L"Ulrich Roeser", L"Markus Semmerling", L"Arno Schroder"};
            for (int i = 0; i < 7; ++i)
                m.threats[i] = {kNames[i], L"458 m", kHostile};
            m.nThreats = 7, m.list = true, m.listTotal = 7, m.listFirst = 0, m.kbm = g_sim == "alertlist";
            m.alert = L"MISSILE INCOMING", m.alertSide = 1, m.nAlertPrompts = 0;
            AlertPrompt(A::Cm, L"FLARE");
        }
    }
    else if (g_sim == "travel")
    {
        AddCtx(A::Context, L"DOCK", true); // (the LB hints have their own spots)
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
            fight();                  // under them the enemies cue, DrawEnemyCue: the owner)
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
} // namespace hud2
