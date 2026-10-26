#include "uikit.h"

#include "../fl.h"
#include "game/gamefont.h"
#include "game/uidraw.h"
#include "gamepad.h"
#include "log.h"

#include <algorithm>

namespace uikit
{
// ------------------------------------------------------------------ fonts
static Fonts g_fonts;
static std::wstring g_chars; // the characters the fonts hold (ASCII + what the rows needed so far)

static std::wstring BaseChars()
{
    std::wstring s;
    for (wchar_t c = 32; c < 127; ++c)
        s += c;
    s += L"\x00B0\x00B7\x2013\x2014\x2026"; // degree, middle dot, dashes, ellipsis
    return s;
}

const Fonts& FontsFor(int sh, int percent)
{
    percent = std::clamp(percent, 50, 200);
    if (g_fonts.ok && g_fonts.sh == sh && g_fonts.percent == percent && g_fonts.label.texture)
        return g_fonts;
    if (g_chars.empty())
        g_chars = BaseChars();
    const float k = percent / 100.0f;
    int titlePx = std::max(8, static_cast<int>(sh * 0.028f * k + 0.5f));
    int labelPx = std::max(8, static_cast<int>(sh * 0.0235f * k + 0.5f));
    const int bodyPx0 = std::max(8, static_cast<int>(sh * 0.0155f * k + 0.5f));
    // the legibility floor (textfit::MinSize: 0.024 H, never under 14 px) for the label faces whatever text_size says;
    // the body face (Arial Unicode MS, the rollover help) keeps its 0.0155 H but never under the 14 px floor
    const int minPx = static_cast<int>(textfit::MinSize(static_cast<float>(sh)) + 0.5f);
    const int bodyPx = std::max(bodyPx0, static_cast<int>(textfit::kMinPx));
    titlePx = std::max(titlePx, minPx), labelPx = std::max(labelPx, minPx);
    const bool game = gamefont::Load();
    const wchar_t* labelFace = gamefont::LabelFace(); // (the game's face from its files or Windows, else Segoe UI)
    const int labelWeight = game ? 400 : 600;
    bool ok = render::BuildFont(g_fonts.title, labelFace, titlePx, labelWeight, g_chars) &&
              render::BuildFont(g_fonts.label, labelFace, labelPx, labelWeight, g_chars);
    if (!ok && game) // the face did not resolve: Segoe UI
        ok = render::BuildFont(g_fonts.title, L"Segoe UI", titlePx, 600, g_chars) &&
             render::BuildFont(g_fonts.label, L"Segoe UI", labelPx, 600, g_chars);
    ok = ok && (render::BuildFont(g_fonts.body, gamefont::TextFace(), bodyPx, 400, g_chars) ||
                render::BuildFont(g_fonts.body, L"Segoe UI", bodyPx, 400, g_chars));
    g_fonts.sh = sh;
    g_fonts.percent = percent;
    g_fonts.ok = ok;
    if (ok)
        Log("uikit: fonts %ls %d / %d px, body %d px (H %d, %d %%)", g_fonts.label.face.c_str(), titlePx, labelPx,
            bodyPx, sh, percent);
    else
        Log("uikit: fonts FAILED (%ls, H %d: no device or face)", labelFace, sh);
    return g_fonts;
}

void FreeFonts()
{
    render::FreeFont(g_fonts.title);
    render::FreeFont(g_fonts.label);
    render::FreeFont(g_fonts.body);
    g_fonts.ok = false;
}

std::wstring Widen(const char* utf8)
{
    if (!utf8 || !*utf8)
        return L"";
    const int n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
    std::wstring w(n > 0 ? n - 1 : 0, L'\0');
    if (n > 1)
        MultiByteToWideChar(CP_UTF8, 0, utf8, -1, &w[0], n);
    return w;
}

std::string Narrow(const std::wstring& s)
{
    if (s.empty())
        return "";
    const int n = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string a(n > 0 ? n - 1 : 0, '\0');
    if (n > 1)
        WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, &a[0], n, nullptr, nullptr);
    return a;
}

// ------------------------------------------------------------------ keys
struct NamedKey
{
    int vk;
    const char* name;
};
static const NamedKey kNamedKeys[] = {
    {VK_INSERT, "Insert"},    {VK_DELETE, "Delete"},   {VK_HOME, "Home"},       {VK_END, "End"},
    {VK_PRIOR, "Page Up"},    {VK_NEXT, "Page Down"},  {VK_SPACE, "Space"},     {VK_TAB, "Tab"},
    {VK_PAUSE, "Pause"},      {VK_SCROLL, "Scroll Lock"}, {VK_CAPITAL, "Caps Lock"}, {VK_BACK, "Backspace"},
    {VK_RETURN, "Enter"},     {VK_ESCAPE, "Escape"},   {VK_UP, "Up"},           {VK_DOWN, "Down"},
    {VK_LEFT, "Left"},        {VK_RIGHT, "Right"},     {VK_OEM_4, "["},         {VK_OEM_6, "]"},
    {VK_OEM_1, ";"},          {VK_OEM_7, "'"},         {VK_OEM_COMMA, ","},     {VK_OEM_PERIOD, "."},
    {VK_OEM_2, "/"},          {VK_OEM_5, "\\"},        {VK_OEM_MINUS, "-"},     {VK_OEM_PLUS, "="},
    {VK_OEM_3, "`"},          {VK_MULTIPLY, "Num *"},  {VK_ADD, "Num +"},       {VK_SUBTRACT, "Num -"},
    {VK_DECIMAL, "Num ."},    {VK_DIVIDE, "Num /"},    {VK_LSHIFT, "Shift"},    {VK_RSHIFT, "Right Shift"},
    {VK_LCONTROL, "Ctrl"},    {VK_RCONTROL, "Right Ctrl"}, {VK_LMENU, "Alt"},   {VK_RMENU, "Right Alt"},
};

std::string KeyName(int vk)
{
    if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9'))
        return std::string(1, static_cast<char>(vk));
    if (vk >= VK_F1 && vk <= VK_F24)
        return "F" + std::to_string(vk - VK_F1 + 1);
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9)
        return "Num " + std::to_string(vk - VK_NUMPAD0);
    for (const NamedKey& k : kNamedKeys)
        if (k.vk == vk)
            return k.name;
    return "";
}

int KeyFromName(const std::string& name)
{
    if (name.empty())
        return 0;
    if (name.size() == 1)
    {
        const char c = static_cast<char>(toupper(static_cast<unsigned char>(name[0])));
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
            return c;
    }
    for (const NamedKey& k : kNamedKeys)
        if (_stricmp(k.name, name.c_str()) == 0)
            return k.vk;
    if ((name[0] == 'F' || name[0] == 'f') && name.size() <= 3)
    {
        const int n = atoi(name.c_str() + 1);
        if (n >= 1 && n <= 24)
            return VK_F1 + n - 1;
    }
    if (_strnicmp(name.c_str(), "Num ", 4) == 0 && name.size() == 5 && isdigit(static_cast<unsigned char>(name[4])))
        return VK_NUMPAD0 + (name[4] - '0');
    return 0;
}

// ------------------------------------------------------------------ input
static Gamepad* Pad(void* p) { return static_cast<Gamepad*>(p); }

void Input::Init()
{
    if (!pad_)
    {
        pad_ = new Gamepad();
        Pad(pad_)->Init();
    }
}

void Input::Reset()
{
    keys_.clear();
    memset(keyDown_, 0, sizeof(keyDown_));
    for (float& r : repeat_)
        r = 0;
    prevClick_ = true; // the click that opened the page is not a click inside it
}

static bool NavKey(int vk)
{
    switch (vk)
    {
    case VK_UP: case VK_DOWN: case VK_LEFT: case VK_RIGHT: case VK_RETURN: case VK_SPACE: case VK_TAB:
    case VK_PRIOR: case VK_NEXT: case VK_HOME: case VK_END: case VK_ESCAPE: case VK_BACK:
        return true;
    }
    return false;
}

bool Input::PushKey(int vk, bool down, bool capturing)
{
    if (vk <= 0 || vk >= 256)
        return false;
    if (!capturing && !NavKey(vk))
        return false;
    keys_.push_back({vk, down});
    return true;
}

bool Input::PadConnected() const { return pad_ && Pad(pad_)->Cur().connected; }

UiInput Input::Poll(float dt, bool capturing)
{
    UiInput in;
    // the pad (its own reader: controller.cpp's g_pad is private to the flight code; the virtual pad is merged by both)
    if (pad_)
    {
        Gamepad& p = *Pad(pad_);
        p.ignoreReal = GetForegroundWindow() != fl::MainWindow();
        p.Update(dt);
        const PadState& s = p.Cur();
        in.anyPad = p.AnyInput();
        in.accept |= p.Pressed(XINPUT_GAMEPAD_A);
        in.back |= p.Pressed(XINPUT_GAMEPAD_B);
        in.tabPrev |= p.Pressed(XINPUT_GAMEPAD_LEFT_SHOULDER);
        in.tabNext |= p.Pressed(XINPUT_GAMEPAD_RIGHT_SHOULDER);
        in.pageUp |= p.Pressed(PAD_LT);
        in.pageDown |= p.Pressed(PAD_RT);
        // D-pad and the left stick as four held directions with repeat
        const bool dir[4] = {p.Down(XINPUT_GAMEPAD_DPAD_UP) || s.ly > 0.6f, p.Down(XINPUT_GAMEPAD_DPAD_DOWN) || s.ly < -0.6f,
                             p.Down(XINPUT_GAMEPAD_DPAD_LEFT) || s.lx < -0.6f,
                             p.Down(XINPUT_GAMEPAD_DPAD_RIGHT) || s.lx > 0.6f};
        static bool prevDir[4] = {};
        bool* edges[4] = {&in.up, &in.down, &in.left, &in.right};
        for (int i = 0; i < 4; ++i)
        {
            if (dir[i] && !prevDir[i])
                *edges[i] = true, repeat_[i] = 0.45f;
            else if (dir[i])
            {
                repeat_[i] -= dt;
                if (repeat_[i] <= 0)
                    *edges[i] = true, repeat_[i] = 0.09f;
            }
            prevDir[i] = dir[i];
        }
    }
    // the keyboard: the queued raw keys (edges), held arrows repeat like the pad's
    for (const KeyEvent& e : keys_)
    {
        const bool was = keyDown_[e.vk];
        keyDown_[e.vk] = e.down;
        if (!e.down || was)
            continue;
        in.anyKey = true;
        if (capturing && e.vk != VK_ESCAPE)
        {
            in.capturedVk = e.vk;
            continue;
        }
        switch (e.vk)
        {
        case VK_UP: in.up = true; repeat_[0] = 0.45f; break;
        case VK_DOWN: in.down = true; repeat_[1] = 0.45f; break;
        case VK_LEFT: in.left = true; repeat_[2] = 0.45f; break;
        case VK_RIGHT: in.right = true; repeat_[3] = 0.45f; break;
        case VK_RETURN: case VK_SPACE: in.accept = true; break;
        case VK_ESCAPE: case VK_BACK: in.back = true; break;
        case VK_PRIOR: in.pageUp = true; break;
        case VK_NEXT: in.pageDown = true; break;
        case VK_TAB: (GetKeyState(VK_SHIFT) & 0x8000) ? in.tabPrev = true : in.tabNext = true; break;
        }
    }
    keys_.clear();
    {
        const int vks[4] = {VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT};
        bool* edges[4] = {&in.up, &in.down, &in.left, &in.right};
        for (int i = 0; i < 4; ++i)
            if (keyDown_[vks[i]] && !*edges[i])
            {
                repeat_[i] -= dt;
                if (repeat_[i] <= 0)
                    *edges[i] = true, repeat_[i] = 0.09f;
            }
    }
    // the pointer: the game's cursor (the physical and the virtual mouse alike) and its left button
    in.px = static_cast<float>(fl::MouseX());
    in.py = static_cast<float>(fl::MouseY());
    in.pointer = true;
    in.held = (fl::MouseButtons() & 1) != 0;
    in.click = in.held && !prevClick_;
    prevClick_ = in.held;
    return in;
}

// ------------------------------------------------------------------ row renderers
// The Options screen's columns at 4K (FREELANCER_RE "Options screen"): label x 1619 with the panel's left edge at
// ~1520; toggle centre 3082 (half 42), ENABLED text ending ~3010; slider centre 2781 half 351 x 27 with its caption to
// the left; the picker box 2458..3140. As fractions of H (2160) from the control column's right edge (controlRight =
// the toggle box's right edge, 3124): ENABLED ends at controlRight - 0.052 H, the slider track runs from controlRight -
// 0.291 H to controlRight - 0.023 H (its arrows outside it), the picker box from controlRight - 0.308 H to controlRight +
// 0.008 H (re-measured against the game's pages in the hosted page, 25 Oct 2026, research\experience\ui_2026-10-25\int_page).
static float TextY(const render::Font& f, const RowGeom& g) { return g.y + (g.h - f.lineHeight) * 0.5f; }

// The fit rule's floor for these fonts: 0.024 H at the player's text_size (HUD 2's text floor; uidraw::FitText).
static float Floor(const Fonts& f)
{
    return textfit::MinSize(static_cast<float>(f.sh));
}

void RowLabel(const Fonts& f, const RowGeom& g, const RowStyle& s, const std::wstring& label)
{
    const u32 col = s.enabled ? uidraw::kText : uidraw::kDisabled;
    if (s.focused)
        uidraw::FocusFrame(g.labelX - g.sh * 0.03f, g.y, g.controlRight - g.labelX + g.sh * 0.045f, g.h, g.sh,
                           uidraw::Fade(uidraw::kFrame, 0.9f));
    if (s.changed)
        uidraw::Dot(g.labelX - g.sh * 0.012f, g.y + g.h * 0.5f, std::max(2.0f, g.sh * 0.0035f), col);
    render::SetFont(f.label);
    // the game's row labels are upper case (RESOLUTION, MOUSE SENSITIVITY): the spec's labels drawn the same way
    std::wstring upper = label;
    for (wchar_t& c : upper)
        if (c >= L'a' && c <= L'z')
            c = static_cast<wchar_t>(c - L'a' + L'A');
    // fitted to the room left of the control column (the fit rule)
    const float room = g.controlLeft > g.labelX ? g.controlLeft - g.labelX - g.sh * 0.01f : 1e9f;
    uidraw::TextShadowFit(g.labelX, TextY(f.label, g), f.label, uidraw::FitText(f.label, upper, room, Floor(f)), col,
                          std::max(1.0f, g.sh * 0.001f));
}

static void Note(const Fonts& f, const RowGeom& g, const RowStyle& s, float rightOfControlText)
{
    if (!s.note)
        return;
    render::SetFont(f.body);
    const std::wstring n = s.note;
    const float w = static_cast<float>(render::TextWidth(f.body, n));
    render::Text(rightOfControlText - w, TextY(f.body, g), n, uidraw::Fade(uidraw::kText, 0.7f));
}

void RowToggle(const Fonts& f, const RowGeom& g, const RowStyle& s, bool on)
{
    const u32 col = s.enabled ? uidraw::kText : uidraw::kDisabled;
    const float box = g.sh * 0.039f; // half 42 at 2160
    const float bx = g.controlRight - box, by = g.y + (g.h - box) * 0.5f;
    uidraw::Checkbox(bx, by, box, on, s.enabled);
    render::SetFont(f.label);
    const std::wstring word = on ? L"ENABLED" : L"DISABLED";
    const float w = static_cast<float>(render::TextWidth(f.label, word));
    const float tx = g.controlRight - g.sh * 0.052f - w; // the game's ENABLED ends at 3010 (4K; measured 25 Oct 2026)
    uidraw::TextShadow(tx, TextY(f.label, g), word, col, std::max(1.0f, g.sh * 0.001f));
    Note(f, g, s, tx - g.sh * 0.02f);
}

PickerHit PickerArea(const RowGeom& g)
{
    PickerHit h;
    h.x = g.controlRight - g.sh * 0.308f; // the game's resolution picker: x 2458..3140 at 4K (its arrows past the toggles)
    h.w = g.sh * 0.316f;
    h.cell = std::max(10.0f, g.h * 0.72f * 0.6f);
    return h;
}

void RowPicker(const Fonts& f, const RowGeom& g, const RowStyle& s, const std::wstring& word)
{
    const PickerHit a = PickerArea(g);
    const float bh = g.h * 0.72f, by = g.y + (g.h - bh) * 0.5f;
    const float cell = uidraw::PickerBox(a.x, by, a.w, bh, g.sh, s.enabled);
    render::SetFont(f.label);
    const float room = a.w - cell - g.sh * 0.02f;
    const textfit::Result w = uidraw::FitText(f.label, word, room, Floor(f)); // a long word: the fit rule
    const u32 col = s.enabled ? uidraw::kText : uidraw::kDisabled;
    uidraw::TextShadowFit(a.x + (a.w - cell - w.width) * 0.5f, TextY(f.label, g), f.label, w, col,
                          std::max(1.0f, g.sh * 0.001f));
    Note(f, g, s, a.x - g.sh * 0.015f);
}

SliderHit SliderArea(const RowGeom& g)
{
    SliderHit h;
    h.x = g.controlRight - g.sh * 0.291f; // the game's track 2495..3075 at 4K, its arrows at 2472 / 3105 (measured 25 Oct 2026)
    h.w = g.sh * 0.268f;
    return h;
}

void RowSlider(const Fonts& f, const RowGeom& g, const RowStyle& s, float t, const std::wstring& caption)
{
    const SliderHit a = SliderArea(g);
    const float cy = g.y + g.h * 0.5f;
    uidraw::SliderTrack(a.x, cy, a.w, g.sh, t, s.enabled);
    render::SetFont(f.body);
    const float w = static_cast<float>(render::TextWidth(f.body, caption));
    const float tx = a.x - g.sh * 0.022f - w;
    render::Text(tx, TextY(f.body, g), caption, s.enabled ? uidraw::kText : uidraw::kDisabled);
    Note(f, g, s, tx - g.sh * 0.015f);
}

void RowKey(const Fonts& f, const RowGeom& g, const RowStyle& s, const std::wstring& keyName, bool capturing)
{
    const PickerHit a = PickerArea(g);
    const float bh = g.h * 0.72f, by = g.y + (g.h - bh) * 0.5f;
    render::Box(a.x, by, a.w, bh, 0x40000000);
    uidraw::Frame(a.x, by, a.w, bh, std::max(1.0f, g.sh * 0.0014f),
                  capturing ? uidraw::kSelected : (s.enabled ? uidraw::kFrame : uidraw::kDisabled));
    render::SetFont(f.label);
    const textfit::Result w = uidraw::FitText(f.label, capturing ? L"PRESS A KEY" : keyName, a.w - g.sh * 0.02f, Floor(f));
    uidraw::TextShadowFit(a.x + (a.w - w.width) * 0.5f, TextY(f.label, g), f.label, w,
                          capturing ? uidraw::kSelected : (s.enabled ? uidraw::kText : uidraw::kDisabled),
                          std::max(1.0f, g.sh * 0.001f));
    Note(f, g, s, a.x - g.sh * 0.015f);
}

void Button(const Fonts& f, float x, float y, float w, float h, float sh, const std::wstring& caption, bool lit,
            bool enabled)
{
    const u32 frame = !enabled ? uidraw::kDisabled : uidraw::kFrame;
    render::Box(x, y, w, h, lit ? 0x60203050 : 0x50000000);
    uidraw::Frame(x, y, w, h, std::max(1.0f, sh * 0.0014f), lit ? uidraw::kText : frame);
    render::SetFont(f.label);
    const textfit::Result c = uidraw::FitText(f.label, caption, w - sh * 0.01f, Floor(f));
    uidraw::TextShadowFit(x + (w - c.width) * 0.5f, y + (h - f.label.lineHeight) * 0.5f, f.label, c,
                          !enabled ? uidraw::kDisabled : uidraw::kText, std::max(1.0f, sh * 0.001f));
}

void TabButton(const Fonts& f, float x, float y, float w, float h, float sh, const std::wstring& caption,
               bool selected, bool lit)
{
    render::Box(x, y, w, h, lit ? 0x60203050 : 0x50000000);
    uidraw::Frame(x, y, w, h, std::max(1.0f, sh * 0.0014f), lit ? uidraw::kText : uidraw::kFrame);
    render::SetFont(f.label);
    const textfit::Result c = uidraw::FitText(f.label, caption, w - sh * 0.01f, Floor(f));
    uidraw::TextShadowFit(x + (w - c.width) * 0.5f, y + (h - f.label.lineHeight) * 0.5f, f.label, c,
                          selected ? uidraw::kSelected : uidraw::kText, std::max(1.0f, sh * 0.001f));
}
} // namespace uikit

// ------------------------------------------------------------------ (D-MENUS, 25 Oct 2026) the pad's other buttons
namespace uikit
{
Input::PadExtra Input::Extra() const
{
    PadExtra e;
    if (!pad_)
        return e;
    const Gamepad& p = *Pad(pad_);
    const PadState& s = p.Cur();
    e.connected = s.connected;
    e.x = p.Pressed(XINPUT_GAMEPAD_X), e.y = p.Pressed(XINPUT_GAMEPAD_Y);
    e.menu = p.Pressed(XINPUT_GAMEPAD_START), e.view = p.Pressed(XINPUT_GAMEPAD_BACK);
    e.lb = p.Pressed(XINPUT_GAMEPAD_LEFT_SHOULDER), e.rb = p.Pressed(XINPUT_GAMEPAD_RIGHT_SHOULDER);
    e.l3 = p.Pressed(XINPUT_GAMEPAD_LEFT_THUMB), e.r3 = p.Pressed(XINPUT_GAMEPAD_RIGHT_THUMB);
    e.xDown = p.Down(XINPUT_GAMEPAD_X), e.yDown = p.Down(XINPUT_GAMEPAD_Y);
    e.lbDown = p.Down(XINPUT_GAMEPAD_LEFT_SHOULDER), e.rbDown = p.Down(XINPUT_GAMEPAD_RIGHT_SHOULDER);
    e.aDown = p.Down(XINPUT_GAMEPAD_A), e.bDown = p.Down(XINPUT_GAMEPAD_B);
    e.menuDown = p.Down(XINPUT_GAMEPAD_START), e.viewDown = p.Down(XINPUT_GAMEPAD_BACK);
    e.xHeld = p.HeldTime(XINPUT_GAMEPAD_X);
    e.lx = s.lx, e.ly = s.ly, e.rx = s.rx, e.ry = s.ry, e.lt = s.lt, e.rt = s.rt;
    return e;
}
} // namespace uikit
