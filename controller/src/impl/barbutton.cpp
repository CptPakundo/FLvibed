// The maneuver bar's fifth button, the logic side (barbutton.h; docs\UI_PLAN.md 2). The host owns the native control
// (controller\src\host\uibtn.cpp); this file owns what the player sees and what a click or the key does.
#include "barbutton.h"

#include "../api.h"
#include "settings_bus.h"
#include "../events.h"
#include "../fl.h"
#include "autopilot.h"
#include "flexe.h"
#include "goals.h"
#include "game/render.h"
#include "host.h"
#include "log.h"
#include "ui.h"

namespace barbutton
{
static bool g_shown = true, g_lit = false, g_enabled = true; // the state last told to the host
static int g_frame = 0;
static std::string g_keyName; // [Controller] autopilot_key as last read
static int g_vk = 'K';
static bool g_keyRefused = false, g_keyDown = false;
static u32 g_keymapChecked = 0;
static render::Image g_glyph;

static const SettingsBus* Bus()
{
    return (HostAtLeast(18) && g_host->GetSettingsBus) ? g_host->GetSettingsBus() : nullptr;
}

static bool BusGet(const char* key, char* out, int len)
{
    const SettingsBus* bus = Bus();
    return bus && bus->Get && bus->Get("Controller", key, out, len);
}

// The key's name as the ini / the page spell it: a letter or digit, F1..F12, or a key name; 0 when unknown.
static int VkFromName(const std::string& name)
{
    if (name.empty())
        return 0;
    if (name.size() == 1)
    {
        const unsigned char c = static_cast<unsigned char>(name[0]);
        if (isalnum(c))
            return toupper(c);
        const SHORT scan = VkKeyScanA(static_cast<char>(c));
        return scan == -1 ? 0 : (scan & 0xFF);
    }
    if ((name[0] == 'F' || name[0] == 'f') && isdigit(static_cast<unsigned char>(name[1])))
    {
        const int n = atoi(name.c_str() + 1);
        return (n >= 1 && n <= 24) ? VK_F1 + n - 1 : 0;
    }
    static const struct
    {
        const char* name;
        int vk;
    } kNames[] = {{"Insert", VK_INSERT},   {"Delete", VK_DELETE}, {"Home", VK_HOME},       {"End", VK_END},
                  {"PageUp", VK_PRIOR},    {"PageDown", VK_NEXT}, {"Tab", VK_TAB},         {"Space", VK_SPACE},
                  {"Backspace", VK_BACK},  {"Pause", VK_PAUSE},   {"ScrollLock", VK_SCROLL}, {"CapsLock", VK_CAPITAL},
                  {"NumLock", VK_NUMLOCK}, {"Apps", VK_APPS},     {"Numpad0", VK_NUMPAD0}, {"Numpad1", VK_NUMPAD1},
                  {"Numpad2", VK_NUMPAD2}, {"Numpad3", VK_NUMPAD3}, {"Numpad4", VK_NUMPAD4}, {"Numpad5", VK_NUMPAD5},
                  {"Numpad6", VK_NUMPAD6}, {"Numpad7", VK_NUMPAD7}, {"Numpad8", VK_NUMPAD8}, {"Numpad9", VK_NUMPAD9}};
    for (const auto& k : kNames)
        if (_stricmp(k.name, name.c_str()) == 0)
            return k.vk;
    return 0;
}

// Does the game's live keymap bind this key (any command, any modifiers)? exe::KeyCommands: the list's sentinel; each
// node's entry (node + 8) = {cmd, ids_name, ids_info, event mask, ?, keys_begin, keys_end}, key = {int key, u32 mods}
// (docs\FREELANCER_RE.md "Keyboard / commands"); mouse buttons are -1 / -2 / -3.
static bool KeymapBinds(int vk, int* cmdOut)
{
    if (!exe::KeyCommands.ok() || vk <= 0)
        return false;
    __try
    {
        const u32 sentinel = Mem<u32>(exe::KeyCommands);
        u32 node = sentinel ? Mem<u32>(sentinel) : 0;
        for (int guard = 0; node && node != sentinel && guard < 1000; ++guard, node = Mem<u32>(node))
        {
            const u32 entry = node + 8;
            const u32 begin = Mem<u32>(entry + 0x14), end = Mem<u32>(entry + 0x18);
            if (!begin || end < begin || end - begin > 64 * 8)
                continue;
            for (u32 k = begin; k + 8 <= end; k += 8)
                if (Mem<int>(k) == vk)
                {
                    if (cmdOut)
                        *cmdOut = Mem<int>(entry);
                    return true;
                }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
    return false;
}

static void RefreshKey(bool force)
{
    char name[96] = {};
    if (!BusGet("autopilot_key", name, sizeof(name)))
        strcpy_s(name, "K"); // no bus / no row yet: the plan's default
    const u32 now = GetTickCount();
    if (!force && g_keyName == name && now - g_keymapChecked < 5000)
        return;
    const bool changed = g_keyName != name;
    g_keyName = name;
    g_vk = VkFromName(g_keyName);
    g_keymapChecked = now;
    int cmd = 0;
    const bool bound = g_vk && KeymapBinds(g_vk, &cmd);
    if (changed || bound != g_keyRefused)
    {
        if (!g_vk)
            Log("autopilot key '%s': unknown key name - the key is off", name);
        else if (bound)
            Log("autopilot key '%s' (vk 0x%02X) is bound by the game's keymap to %s - refused; rebind it on the page",
                name, g_vk, fl::CmdNameRaw(cmd));
        else
            Log("autopilot key '%s' (vk 0x%02X) armed", name, g_vk);
    }
    g_keyRefused = bound;
}

bool KeyRefused() { return g_keyRefused; }
int KeyVk() { return g_vk; }

void Update()
{
    if (!HostAtLeast(18) || !g_host->SetUiButton)
        return;
    ++g_frame;
    if (g_frame % 30 == 1)
        RefreshKey(false);
    char v[16] = {};
    bool wanted = true;
    if (BusGet("maneuver_button", v, sizeof(v)))
        wanted = atof(v) != 0;
    g_shown = wanted;
    g_lit = autopilot::Active();
    // grey when the autopilot cannot start: no player ship (the bar is hidden then anyway) or a cutscene
    g_enabled = fl::PlayerIObj() != nullptr && !hk::Call<int(__cdecl*)()>(exe::IsPlayerInCutscene);
    g_host->SetUiButton(kUiBtnBar, g_shown, g_lit, g_enabled);
}

static void Toggle(const char* reason)
{
    if (autopilot::Active())
    {
        events::Push("autopilot stop reason=%s", reason);
        // the player's own stop, as the pad's View hold: the objective mode ends too (Stop("bar") left the objective
        // supervisor on - it re-decided and flew on in Hold; found by the parity test, 26 Oct 2026)
        autopilot::Stop("player input");
    }
    else
    {
        if (!g_enabled)
        {
            events::Push("autopilot start refused reason=%s (no ship or a cutscene)", reason);
            return;
        }
        events::Push("autopilot start reason=%s", reason);
        autopilot::StartSmart();
    }
}

// Shift held (the keyboard's own state, or the raw Shift keys this logic saw: a posted key in a background test leaves
// GetKeyState alone)
static bool g_rawShift = false;
static bool ShiftHeld() { return g_rawShift || (GetKeyState(VK_SHIFT) & 0x8000) != 0; }

// The goal panel on the keyboard + mouse (the owner, 26 Oct 2026: the autopilot identical on both devices): Shift with
// the autopilot key or Shift + click on the button (the bar's, HUD 2's) opens / closes it - the pad's X / the wheel.
static void TogglePanel(const char* reason)
{
    if (!g_enabled && !goals::IsOpen())
    {
        events::Push("goals open refused reason=%s (no ship or a cutscene)", reason);
        return;
    }
    events::Push("goals toggle reason=%s", reason);
    goals::Toggle();
}

void OnClick()
{
    if (ShiftHeld())
        TogglePanel("bar shift-click");
    else
        Toggle("bar");
}

bool OnKey(int vk, bool down, bool textField)
{
    if (vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT)
    {
        g_rawShift = down;
        return false; // never swallowed
    }
    if (textField || !g_vk || vk != g_vk)
        return false;
    if (g_keyRefused)
        return false; // the game's own command has it
    if (down && !g_keyDown)
    {
        if (ShiftHeld())
            TogglePanel("shift-key");
        else
            Toggle("key");
    }
    g_keyDown = down;
    return true; // ours: the game never sees it (nothing bound anyway)
}

// A ring with a chevron inside (line art like the bar's own icons), white with coverage in alpha; tinted when drawn.
static const render::Image* Glyph(void* device)
{
    if (g_glyph.texture && g_glyph.device == device)
        return &g_glyph;
    const int n = 128;
    std::vector<u32> px(static_cast<size_t>(n) * n);
    auto seg = [](float px0, float py0, float ax, float ay, float bx, float by)
    {
        const float vx = bx - ax, vy = by - ay, wx = px0 - ax, wy = py0 - ay;
        const float t = Clampf((wx * vx + wy * vy) / (vx * vx + vy * vy), 0, 1);
        const float dx = px0 - (ax + t * vx), dy = py0 - (ay + t * vy);
        return std::sqrt(dx * dx + dy * dy);
    };
    const float stroke = 0.075f, aa = 0.03f;
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x)
        {
            const float fx = (x + 0.5f) / n * 2 - 1, fy = (y + 0.5f) / n * 2 - 1;
            const float r = std::sqrt(fx * fx + fy * fy);
            float d = std::fabs(r - 0.78f);                     // the ring
            d = std::min(d, seg(fx, fy, -0.34f, 0.2f, 0.0f, -0.26f)); // the chevron (nose up)
            d = std::min(d, seg(fx, fy, 0.34f, 0.2f, 0.0f, -0.26f));
            const float cov = Clampf((stroke - d) / aa + 0.5f, 0, 1);
            px[static_cast<size_t>(y) * n + x] = (static_cast<u32>(cov * 255 + 0.5f) << 24) | 0xFFFFFF;
        }
    g_glyph = render::Image();
    render::BuildImage(g_glyph, px, n, n);
    return g_glyph.texture ? &g_glyph : nullptr;
}

// The host gives the button FLvibed's own mesh when EXE\FLvibedutopilot.3db is installed (uibtn.cpp): the glyph is
// then the mesh's, drawn by the game (lit through the game's own tint); the overlay glyph is the fallback without it.
static bool MeshInstalled()
{
    static int state = -1;
    if (state < 0)
    {
        const std::string file = std::string(g_host && g_host->dllDir ? g_host->dllDir : "") + "FLvibed\\autopilot.3db";
        state = GetFileAttributesA(file.c_str()) != INVALID_FILE_ATTRIBUTES ? 1 : 0;
        Log("bar button: %s", state ? "the shipped mesh draws the icon" : "no shipped mesh: the overlay glyph draws it");
    }
    return state == 1;
}

void Draw()
{
    if (!HostAtLeast(18) || !g_host->UiButtonRect || !g_shown || MeshInstalled())
        return;
    float cx = 0, cy = 0, hw = 0, hh = 0;
    if (!g_host->UiButtonRect(kUiBtnBar, &cx, &cy, &hw, &hh) || hh <= 1)
        return;
    void* device = g_host->GetD3DDevice();
    if (!device)
        return;
    render::SetDevice(device);
    const render::Image* img = Glyph(device);
    if (!img || !render::BeginPlain())
        return;
    const bool hovered = _stricmp(ui::ControlName(ui::Hovered()), "FLvibedAutopilotButton") == 0;
    // the bar's light blue, the active maneuver's yellow, grey when it cannot start; a hover brightens
    u32 rgb = g_lit ? 0xFFD040 : 0x8CC8F0;
    if (!g_enabled)
        rgb = 0x6A7684;
    u32 a = hovered ? 0xFF : 0xDC;
    const float d = hh * 1.5f; // the bar's icons fill about three quarters of a button's hit box
    render::Sprite(*img, cx, cy, d, d, 0, (a << 24) | rgb, false);
    render::End(false);
}

void Shutdown() { render::FreeImage(g_glyph); }
} // namespace barbutton
