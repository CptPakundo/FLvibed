// The on-screen keyboard - see osk.h.
#include "osk.h"

#include "../fl.h"
#include "host.h"
#include "log.h"
#include "padhud.h"
#include "promptbar.h"
#include "uiopt.h"
#include "game/render.h"
#include "game/uidraw.h"

#include <sstream>

namespace osk
{
// Five rows of ten keys; the shift layer swaps the letters' case and the digits for symbols. Row 4 holds the
// punctuation that save descriptions and chat need; the bottom row's wide keys are SPACE, BACKSPACE, DONE.
static const wchar_t* kRows[5] = {L"1234567890", L"qwertyuiop", L"asdfghjkl-", L"zxcvbnm,.'", L"!?:;()/&@_"};
static const wchar_t* kRowsShift[5] = {L"!@#$%^&*()", L"QWERTYUIOP", L"ASDFGHJKL_", L"ZXCVBNM<>\"", L"+=[]{}\\|~`"};
constexpr int kCols = 10, kKeyRows = 5, kWideRow = 5; // row 5: SPACE | BACKSPACE | DONE
static bool g_open = false;
static Purpose g_purpose = Purpose::Other;
static int g_row = 1, g_col = 0;
static bool g_shift = false;
static int g_typed = 0;
static bool g_sticky = false; // shift stays after a key (a second press of LB / RB made it sticky)

void Open(Purpose p)
{
    g_open = true;
    g_purpose = p;
    g_row = 1, g_col = 0, g_shift = false, g_sticky = false;
    padhud::SetModal(true); // the flight legend steps aside
    Log("osk: open (%s)", p == Purpose::Save ? "save" : p == Purpose::Chat ? "chat" : "other");
}
void Close()
{
    if (g_open)
    {
        Log("osk: closed (%d typed)", g_typed);
        padhud::SetModal(false);
    }
    g_open = false;
}
bool IsOpen() { return g_open; }
Purpose Current() { return g_purpose; }

static wchar_t KeyAt(int row, int col) { return (g_shift ? kRowsShift : kRows)[row][col]; }

int Type(const std::wstring& s)
{
    int n = 0;
    for (wchar_t c : s)
        n += fl::SendChar(c) ? 1 : 0;
    g_typed += n;
    return n;
}

static void Backspace() { fl::TapKey(VK_BACK); }
static void Done()
{
    if (g_purpose == Purpose::Save)
    {
        Close(); // the description stays in the game's edit; A on the screen saves
        return;
    }
    fl::TapKey(VK_RETURN); // the chat sends and closes its field; another edit notifies its owner (0xF003)
    Close();
}

void Input(const uikit::UiInput& in, const uikit::Input::PadExtra& x)
{
    if (!g_open)
        return;
    if (in.up)
        g_row = g_row > 0 ? g_row - 1 : kWideRow;
    if (in.down)
        g_row = g_row < kWideRow ? g_row + 1 : 0;
    if (g_row == kWideRow)
    {
        // the wide row has three cells; map the column to them
        if (in.left)
            g_col = g_col >= 4 ? (g_col >= 7 ? 4 : 0) : 7;
        if (in.right)
            g_col = g_col < 4 ? 4 : (g_col < 7 ? 7 : 0);
    }
    else
    {
        if (in.left)
            g_col = (g_col + kCols - 1) % kCols;
        if (in.right)
            g_col = (g_col + 1) % kCols;
    }
    if (x.lb || x.rb)
    {
        if (g_shift && !g_sticky)
            g_sticky = true;
        else
            g_shift = !g_shift, g_sticky = false;
    }
    if (in.accept)
    {
        if (g_row == kWideRow)
        {
            if (g_col < 4)
                Type(L" ");
            else if (g_col < 7)
                Backspace();
            else
                Done();
        }
        else
        {
            Type(std::wstring(1, KeyAt(g_row, g_col)));
            if (g_shift && !g_sticky)
                g_shift = false;
        }
    }
    if (x.x)
        Type(L" ");
    if (in.back)
        Backspace();
    if (x.menu)
        Done();
    static const promptbar::Prompt prompts[] = {{glyphs::A, L"TYPE"},
                                                {glyphs::X, L"SPACE"},
                                                {glyphs::B, L"BACKSPACE"},
                                                {glyphs::LB, L"SHIFT", false, glyphs::RB},
                                                {glyphs::MENU, g_purpose == Purpose::Chat ? L"SEND" : L"DONE"}};
    promptbar::Set(g_purpose == Purpose::Chat ? "chat" : "save.keyboard", prompts, 5, true, 10);
}

void Draw()
{
    if (!g_open)
        return;
    void* device = g_host && g_host->GetD3DDevice ? g_host->GetD3DDevice() : nullptr;
    if (!device)
        return;
    render::SetDevice(device);
    int sw = 0, sh = 0;
    if (!render::ScreenSize(sw, sh) || sh < 100)
        return;
    const float H = static_cast<float>(sh), W = static_cast<float>(sw);
    const uiopt::UiOptions& o = uiopt::Get();
    const uikit::Fonts& f = uikit::FontsFor(sh, o.textSize);
    if (!f.ok || !render::Begin(f.label))
        return;
    const float ts = o.textSize / 100.0f;
    const float cell = H * 0.042f * ts, gap = cell * 0.12f, pitch = cell + gap;
    const float panelW = kCols * pitch + gap + cell * 0.8f, panelH = (kKeyRows + 1) * pitch + gap + cell * 0.8f;
    // the spec places the keyboard in the lower middle of the screen (save: 0.52 H; chat: 0.65 H) - the chat field sits at
    // 0.89 H in HDE at 16:9, so both use 0.50 H and the keyboard ends above the field
    const float top = 0.50f * H, left = (W - panelW) * 0.5f;
    const double now = GetTickCount64() / 1000.0;
    uidraw::HudPanel(left, top, panelW, panelH, 0.94f, H, now);
    const float x0 = left + cell * 0.4f + gap, y0 = top + cell * 0.4f + gap;
    const u32 key = 0xFF8ACEF1, keyBack = 0x60203060, focusBack = 0xFF2A5F8A;
    for (int r = 0; r < kKeyRows; ++r)
        for (int c = 0; c < kCols; ++c)
        {
            const float x = x0 + c * pitch, y = y0 + r * pitch;
            const bool focused = r == g_row && c == g_col;
            render::Box(x, y, cell, cell, focused ? focusBack : keyBack);
            uidraw::Frame(x, y, cell, cell, 1, focused ? uidraw::kSelected : uidraw::kFrame);
            const std::wstring ch(1, KeyAt(r, c));
            render::Text(x + (cell - render::TextWidth(f.label, ch)) * 0.5f, y + (cell - f.label.lineHeight) * 0.5f,
                         ch, focused ? uidraw::kSelected : key);
        }
    // the wide row: SPACE (4 cells), BACKSPACE (3), DONE (3)
    struct Wide
    {
        int c0, n;
        const wchar_t* label;
    } wide[3] = {{0, 4, L"SPACE"}, {4, 3, L"BACKSPACE"}, {7, 3, g_purpose == Purpose::Chat ? L"SEND" : L"DONE"}};
    for (int i = 0; i < 3; ++i)
    {
        const float x = x0 + wide[i].c0 * pitch, y = y0 + kKeyRows * pitch, w = wide[i].n * pitch - gap;
        const bool focused = g_row == kWideRow && g_col >= wide[i].c0 && g_col < wide[i].c0 + wide[i].n;
        render::Box(x, y, w, cell, focused ? focusBack : keyBack);
        uidraw::Frame(x, y, w, cell, 1, focused ? uidraw::kSelected : uidraw::kFrame);
        render::Text(x + (w - render::TextWidth(f.label, wide[i].label)) * 0.5f, y + (cell - f.label.lineHeight) * 0.5f,
                     wide[i].label, focused ? uidraw::kSelected : key);
    }
    // the shift state, small, at the panel's top right
    render::SetFont(f.body);
    const std::wstring st = g_shift ? (g_sticky ? L"SHIFT LOCK" : L"SHIFT") : L"";
    if (!st.empty())
        render::Text(left + panelW - render::TextWidth(f.body, st) - cell * 0.4f, top + cell * 0.1f, st, uidraw::kSelected);
    render::End(false);
}

std::string Status()
{
    std::ostringstream s;
    s << "osk open=" << g_open << " purpose=" << static_cast<int>(g_purpose) << " focus=" << g_row << "," << g_col
      << " shift=" << g_shift << " typed=" << g_typed;
    return s.str();
}

std::string Command(std::istream& in)
{
    std::string sub;
    in >> sub;
    if (sub == "open")
    {
        std::string p;
        in >> p;
        Open(p == "chat" ? Purpose::Chat : p == "save" ? Purpose::Save : Purpose::Other);
        return Status();
    }
    if (sub == "close")
    {
        Close();
        return Status();
    }
    if (sub == "type")
    {
        std::string rest;
        std::getline(in, rest);
        if (!rest.empty() && rest[0] == ' ')
            rest.erase(0, 1);
        return "typed " + std::to_string(Type(uikit::Widen(rest.c_str()))) + " of " + std::to_string(rest.size());
    }
    if (sub == "shift")
    {
        g_shift = !g_shift;
        return Status();
    }
    return Status() + "\nusage: osk open [save|chat]|close|type <text>|shift|state";
}
} // namespace osk
