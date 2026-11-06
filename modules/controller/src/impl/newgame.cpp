// The New Game prompt and the mode of every save - see newgame.h.
#include "newgame.h"
#include "ui/views.h"

#include "game/events.h"
#include "game/fl.h"
#include "controller.h"
#include "host.h"
#include "game/modslots.h"
#include "log.h"
#include "promptbar.h"
#include "ui/ui.h"
#include "ui/uiopt.h"
#include "game/menu.h"
#include "game/session.h"
#include "game/render.h"
#include "game/room.h"
#include "game/uidraw.h"

#include <fstream>
#include <map>
#include <sstream>

namespace newgame
{
using uikit::Widen;

static bool g_prompt = false;
static int g_pick = 0;
static std::string g_lastWhat; // the last New Game click's reply
static int g_tagsDrawn = 0;

static bool Rd(u32 addr, void* out, size_t n)
{
    __try
    {
        memcpy(out, reinterpret_cast<const void*>(addr), n);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}
template <typename T> static T RdOr(u32 addr, T def)
{
    T v;
    return Rd(addr, &v, sizeof(T)) ? v : def;
}

// ------------------------------------------------------------------ the prompt
void OpenPrompt()
{
    g_prompt = true;
    g_pick = 0;
    Log("newgame: prompt open");
}
void ClosePrompt() { g_prompt = false; }
bool PromptOpen() { return g_prompt; }

static void Start(int pick)
{
    session::Begin(static_cast<session::Mode>(pick), &g_lastWhat);
    g_prompt = false;
}

void PromptInput(const uikit::UiInput& in, const uikit::Input::PadExtra&)
{
    if (!g_prompt)
        return;
    if (in.up)
        do
            g_pick = (g_pick + 2) % 3;
        while (!session::Offered(g_pick));
    if (in.down)
        do
            g_pick = (g_pick + 1) % 3;
        while (!session::Offered(g_pick));
    if (in.accept)
        Start(g_pick);
    else if (in.back)
        ClosePrompt();
    static const promptbar::Prompt prompts[] = {{glyphs::A, L"START"}, {glyphs::B, L"BACK"}};
    promptbar::Set("mainmenu.newgame", prompts, 2, true, 10);
}

static std::vector<std::wstring> Wrap(const render::Font& f, const std::wstring& s, float width)
{
    std::vector<std::wstring> lines;
    std::wstring line, word;
    std::wistringstream ws(s);
    while (ws >> word)
    {
        const std::wstring probe = line.empty() ? word : line + L" " + word;
        if (!line.empty() && render::TextWidth(f, probe) > width)
            lines.push_back(line), line = word;
        else
            line = probe;
    }
    if (!line.empty())
        lines.push_back(line);
    return lines;
}

static void DrawPrompt(const uikit::Fonts& f, float W, float H)
{
    const float w = H * 0.62f, rowH = f.label.lineHeight * 1.7f;
    const float descW = w - H * 0.06f;
    render::SetFont(f.body);
    const std::vector<std::wstring> desc = Wrap(f.body, session::DescOf(g_pick), descW);
    const float h = H * 0.03f + f.title.lineHeight * 1.6f + 3 * rowH + H * 0.015f + desc.size() * f.body.lineHeight * 1.15f +
                    H * 0.03f;
    const float x0 = (W - w) * 0.5f, y0 = H * 0.5f - h * 0.5f;
    const double now = GetTickCount64() / 1000.0;
    uidraw::HudPanel(x0, y0, w, h, 0.96f, H, now);
    float y = y0 + H * 0.03f;
    render::SetFont(f.title);
    uidraw::TextShadow(x0 + (w - render::TextWidth(f.title, L"NEW GAME")) * 0.5f, y, L"NEW GAME", uidraw::kText, 1);
    y += f.title.lineHeight * 1.6f;
    render::SetFont(f.label);
    for (int i = 0; i < 3; ++i)
    {
        if (!session::Offered(i))
            continue;
        const bool sel = i == g_pick;
        if (sel)
            uidraw::FocusFrame(x0 + H * 0.02f, y, w - H * 0.04f, rowH, H, uidraw::kFrame);
        const u32 col = sel ? uidraw::kSelected : uidraw::kText;
        render::Text(x0 + H * 0.035f, y + (rowH - f.label.lineHeight) * 0.5f, session::ChoiceOf(i).name, col);
        if (session::ChoiceOf(i).tag[0])
        {
            render::SetFont(f.body);
            render::Text(x0 + w - H * 0.035f - render::TextWidth(f.body, session::ChoiceOf(i).tag),
                         y + (rowH - f.body.lineHeight) * 0.5f, session::ChoiceOf(i).tag, 0xFFB9C7D2);
            render::SetFont(f.label);
        }
        y += rowH;
    }
    y += H * 0.015f;
    render::SetFont(f.body);
    for (const std::wstring& l : desc)
    {
        render::Text(x0 + H * 0.03f, y, l, uidraw::kText);
        y += f.body.lineHeight * 1.15f;
    }
}

// ------------------------------------------------------------------ the tags on the load screens' rows
static void DrawTags(const uikit::Fonts& f, float H)
{
    g_tagsDrawn = 0;
    u32 list = 0, bar = 0;
    bool front = false;
    // the lists come from the hit-test probe, not the tree; the shared cache (was: a fresh probe every drawn frame)
    const std::vector<ui::Item>& items = ui::CollectAllCached();
    if (const u32 cs = session::CharSelect())
    {
        list = RdOr<u32>(cs + 0xD8, 0u);
        bar = uiclick::FindVisible("CharScroll");
        front = true;
    }
    else if (const u32 screen = menu::LoadScreen())
    {
        // the widest game-list child of the screen (the description column)
        float best = 0;
        for (const ui::Item& it : items)
            if (it.root == screen && exe::VtGameList.is(RdOr<u32>(static_cast<u32>(it.ctl), 0u)) && it.hw > best)
                best = it.hw, list = static_cast<u32>(it.ctl);
        bar = uiclick::FindVisible("LoadSave_Scroll");
    }
    if (!list)
        return;
    // the drawn rows (measured 25 Oct 2026 at 4K, HDE 0.7.1): a pitch of 0.029 H on both lists; the first row's text
    // centre 0.2486 H above the screen's middle on the front list, 0.2246 H in game (ui::ListRows' 0.0318 H front pitch
    // is the hit cell's of another layout: it drifts a row after a dozen rows here)
    const float rowH = H * 0.029f;
    const float firstY = H * 0.5f - H * (front ? 0.2486f : 0.2246f);
    const int first = bar ? static_cast<int>(RdOr<float>(bar + 0x3C8, 0.0f) + 0.5f) : 0;
    const std::vector<menu::SaveEntry>& saves = session::CachedSaves();
    // the list's rectangle (its screen item) for the right edge and the bottom
    float right = 0, bottom = 0;
    for (const ui::Item& it : items)
        if (it.ctl == list)
            right = it.x + it.hw, bottom = it.y + it.hh;
    if (right <= 0)
        return;
    render::SetFont(f.body);
    const float textDy = 0;
    for (int r = 0; r < 64; ++r)
    {
        const float y = firstY + r * rowH;
        if (y + rowH * 0.5f > bottom)
            break;
        const int idx = first + r;
        if (idx < 0 || idx >= static_cast<int>(saves.size()))
            continue;
        const session::Mode m = session::OfSave(saves[idx].id);
        if (m != session::ASSISTED && m != session::SPEEDRUN)
            continue;
        const wchar_t* tag = m == session::SPEEDRUN ? L"SPEEDRUN" : L"ASSISTED";
        const float tw = static_cast<float>(render::TextWidth(f.body, tag));
        const float x = right - tw - H * 0.012f, ty = y + textDy - f.body.lineHeight * 0.5f;
        render::Box(x - H * 0.004f, ty, tw + H * 0.008f, static_cast<float>(f.body.lineHeight),
                    m == session::SPEEDRUN ? 0xC0603010 : 0xC0104060);
        render::Text(x, ty, tag, m == session::SPEEDRUN ? 0xFFFFC860 : 0xFF8ACEF1);
        ++g_tagsDrawn;
    }
}

void Draw()
{
    const bool loadScreen = session::CharSelect() || menu::LoadScreen(); // (the in-game one opens from the pause menu, docked or in space)
    if (!g_prompt && !loadScreen)
        return;
    void* device = g_host && g_host->GetD3DDevice ? g_host->GetD3DDevice() : nullptr;
    if (!device)
        return;
    render::SetDevice(device);
    int sw = 0, sh = 0;
    if (!render::ScreenSize(sw, sh) || sh < 100)
        return;
    const uikit::Fonts& f = uikit::FontsFor(sh, uiopt::Get().textSize);
    if (!f.ok || !render::Begin(f.label))
        return;
    if (loadScreen)
        DrawTags(f, static_cast<float>(sh));
    if (g_prompt)
        DrawPrompt(f, static_cast<float>(sw), static_cast<float>(sh));
    render::End(false);
}

std::string Status()
{
    std::ostringstream o;
    o << "newgame " << session::Status() << " prompt=" << g_prompt << " pick=" << g_pick << " tags=" << g_tagsDrawn;
    return o.str();
}

std::string Command(std::istream& in)
{
    std::string sub;
    in >> sub;
    if (sub == "open")
    {
        OpenPrompt();
        return Status();
    }
    if (sub == "close")
    {
        ClosePrompt();
        return Status();
    }
    if (sub == "pick") // newgame pick <0..2>: as A on that entry (the game's New Game runs)
    {
        int n = 0;
        in >> n;
        if (n < 0 || n > 2)
            return "pick 0 normal | 1 assisted | 2 speedrun";
        g_prompt = true;
        Start(n);
        return g_lastWhat + " | " + Status();
    }
    if (sub == "mode") // newgame mode <0..2>: force the session's mode (tests)
    {
        int n = 0;
        in >> n;
        session::Force(static_cast<session::Mode>(n < 0 ? -1 : n > 2 ? 2 : n));
        return Status();
    }
    if (sub == "records")
    {
        return session::Records() + Status();
    }
    if (sub == "rescan")
    {
        session::Rescan();
        return Status();
    }
    return Status() + "\nusage: newgame open|close|pick <n>|mode <n>|records|rescan|state";
}
} // namespace newgame

