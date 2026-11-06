// The New Game choice on keyboard and mouse - see startprompt.h.
#include "startprompt.h"

#include "game/fl.h"
#include "game/hostcalls.h"
#include "game/menu.h"
#include "game/render.h"
#include "game/room.h"
#include "game/session.h"
#include "game/space.h"
#include "game/uidraw.h"
#include "log.h"
#include "modules_rules.h"
#include "safemem.h"
#include "ui/ui.h"
#include "ui/uikit.h"
#include "ui/uiopt.h"

#include <sstream>

namespace startprompt
{
constexpr int kBack = 3; // the row after the modes

static bool g_open = false;
static int g_over = -1;       // the row under the pointer
static bool g_mouseWas = false;
static bool g_overButton = false;
static float g_lastX = -1, g_lastY = -1;
static uintptr_t g_taken = 0; // what the hit test leaves out now: NEW GAME, or the whole menu while the list is open
static std::string g_lastWhat;

struct Box
{
    float x = 0, y = 0, w = 0, h = 0;
    bool Holds(float px, float py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};
static Box g_rows[kBack + 1]; // as last drawn

static bool Wanted() { return hostcalls::g.ModuleOn && !hostcalls::g.ModuleOn(modules::kController) && !space::Multiplayer(); }

// Each frame while it holds: the hit test's list has another owner in flight, whose last release may fall in this menu's first frames.
static void Take(uintptr_t control)
{
    if ((control == g_taken && !control) || !hostcalls::g.SetHudHitSuppression)
        return;
    g_taken = control;
    hostcalls::g.SetHudHitSuppression(control != 0, control ? &control : nullptr, control ? 1 : 0);
}

static void Open()
{
    g_open = true;
    g_over = session::NORMAL;
    Log("startprompt: open");
}

static int Next(int row, int by)
{
    do
        row = (row + by + kBack + 1) % (kBack + 1);
    while (row != kBack && !session::Offered(row));
    return row;
}

static void Pick(int row)
{
    g_open = false;
    if (row == kBack)
        return;
    session::Begin(static_cast<session::Mode>(row), &g_lastWhat);
}

void Update()
{
    const u32 button = Wanted() && session::MainMenuVisible() ? session::MainMenuButton(menu::kNewGame) : 0;
    if (!button)
    {
        g_open = false;
        g_overButton = false;
        Take(0);
        return;
    }
    u32 parent = 0;
    safemem::Read(button + 4, &parent, 4); // (FlControl +4 = parent: every button of the menu hangs under it)
    Take(g_open && parent ? parent : button);
    const float px = static_cast<float>(fl::MouseX()), py = static_cast<float>(fl::MouseY());
    const bool down = (fl::MouseButtons() & 1) != 0, pressed = down && !g_mouseWas;
    g_mouseWas = down;
    const bool moved = px != g_lastX || py != g_lastY;
    g_lastX = px, g_lastY = py;
    ui::Item it;
    // (the game's Exit question stands over the menu: its buttons are the mouse's then, and the hit test keeps NEW GAME from it anyway)
    g_overButton = !g_open && !uiclick::FindVisible("ExitButtonBox0") && ui::FrontButtonRect(button, it) && std::fabs(px - it.x) <= it.hw && std::fabs(py - it.y) <= it.hh;
    if (!g_open)
    {
        if (pressed && g_overButton)
            Open();
        return;
    }
    int under = -1;
    for (int i = 0; i <= kBack; ++i)
        if ((i == kBack || session::Offered(i)) && g_rows[i].Holds(px, py))
            under = i;
    if (moved && under >= 0) // (a still pointer leaves the keys' row alone)
        g_over = under;
    if (pressed && under >= 0)
        Pick(under);
}

bool OnKey(int vk, bool down)
{
    if (!g_open)
        return false;
    if (down && vk == VK_ESCAPE)
        g_open = false;
    else if (down && vk == VK_UP)
        g_over = Next(g_over, -1);
    else if (down && vk == VK_DOWN)
        g_over = Next(g_over, 1);
    else if (down && vk == VK_RETURN && g_over >= 0)
        Pick(g_over);
    return true;
}

static void DrawList(const uikit::Fonts& f, float W, float H)
{
    const float w = H * 0.62f, rowH = f.label.lineHeight * 1.7f;
    int rows = 1;
    for (int i = 0; i < kBack; ++i)
        rows += session::Offered(i) ? 1 : 0;
    const int shown = g_over >= 0 && g_over < kBack ? g_over : static_cast<int>(session::SPEEDRUN);
    const std::wstring desc = session::DescOf(shown);
    const float descLines = std::ceil(render::TextWidth(f.body, desc) / (w - H * 0.06f)) + 1;
    const float h = H * 0.03f + f.title.lineHeight * 1.6f + rows * rowH + H * 0.015f + descLines * f.body.lineHeight * 1.15f + H * 0.03f;
    const float x0 = (W - w) * 0.5f, y0 = H * 0.5f - h * 0.5f;
    uidraw::HudPanel(x0, y0, w, h, 0.96f, H, GetTickCount64() / 1000.0);
    float y = y0 + H * 0.03f;
    render::SetFont(f.title);
    uidraw::TextShadow(x0 + (w - render::TextWidth(f.title, L"NEW GAME")) * 0.5f, y, L"NEW GAME", uidraw::kText, 1);
    y += f.title.lineHeight * 1.6f;
    render::SetFont(f.label);
    for (int i = 0; i <= kBack; ++i)
    {
        if (i < kBack && !session::Offered(i))
            continue;
        g_rows[i] = {x0 + H * 0.02f, y, w - H * 0.04f, rowH};
        if (i == g_over)
            uidraw::FocusFrame(g_rows[i].x, y, g_rows[i].w, rowH, H, uidraw::kFrame);
        const u32 col = i == g_over ? uidraw::kSelected : uidraw::kText;
        render::Text(x0 + H * 0.035f, y + (rowH - f.label.lineHeight) * 0.5f, i == kBack ? L"BACK" : session::ChoiceOf(i).name, col);
        if (i < kBack && session::ChoiceOf(i).tag[0])
        {
            render::SetFont(f.body);
            render::Text(x0 + w - H * 0.035f - render::TextWidth(f.body, session::ChoiceOf(i).tag), y + (rowH - f.body.lineHeight) * 0.5f, session::ChoiceOf(i).tag, 0xFFB9C7D2);
            render::SetFont(f.label);
        }
        y += rowH;
    }
    y += H * 0.015f;
    render::SetFont(f.body);
    // the description by words, in the panel's width
    std::wistringstream words(desc);
    std::wstring word, line;
    while (words >> word)
    {
        const std::wstring probe = line.empty() ? word : line + L" " + word;
        if (!line.empty() && render::TextWidth(f.body, probe) > w - H * 0.06f)
        {
            render::Text(x0 + H * 0.03f, y, line, uidraw::kText);
            y += f.body.lineHeight * 1.15f;
            line = word;
        }
        else
            line = probe;
    }
    if (!line.empty())
        render::Text(x0 + H * 0.03f, y, line, uidraw::kText);
}

void Draw()
{
    if (!g_open && !g_overButton)
        return;
    void* device = hostcalls::g.GetD3DDevice ? hostcalls::g.GetD3DDevice() : nullptr;
    if (!device)
        return;
    render::SetDevice(device);
    int sw = 0, sh = 0;
    if (!render::ScreenSize(sw, sh) || sh < 100)
        return;
    const uikit::Fonts& f = uikit::FontsFor(sh, uiopt::Get().textSize);
    if (!f.ok || !render::Begin(f.label))
        return;
    const float H = static_cast<float>(sh);
    if (g_open)
        DrawList(f, static_cast<float>(sw), H);
    else
    {
        ui::Item it;
        if (const u32 button = session::MainMenuButton(menu::kNewGame); button && ui::FrontButtonRect(button, it))
            uidraw::FocusFrame(it.x - it.hw, it.y - it.hh, it.hw * 2, it.hh * 2, H, uidraw::kFrame);
    }
    render::End(false);
}

void Shutdown()
{
    g_open = false;
    g_overButton = false;
    Take(0);
}

static std::string Status()
{
    std::ostringstream o;
    o << "startprompt wanted=" << Wanted() << " open=" << g_open << " over=" << g_over << " taken=" << (g_taken != 0);
    ui::Item it;
    if (const u32 button = session::MainMenuButton(menu::kNewGame); button && ui::FrontButtonRect(button, it))
        o << " button=" << static_cast<int>(it.x) << "," << static_cast<int>(it.y);
    o << " rows=";
    for (const Box& b : g_rows)
        o << static_cast<int>(b.x + b.w * 0.5f) << "," << static_cast<int>(b.y + b.h * 0.5f) << " ";
    o << g_lastWhat << " | " << session::Status();
    return o.str();
}

std::string Command(std::istream& in)
{
    std::string sub;
    in >> sub;
    if (sub == "open")
        Open();
    else if (sub == "close")
        g_open = false;
    else if (sub == "pick")
    {
        int n = 0;
        in >> n;
        if (n < 0 || n > kBack || (n < kBack && !session::Offered(n)))
            return "not offered: " + Status();
        Pick(n);
    }
    return Status();
}
} // namespace startprompt
