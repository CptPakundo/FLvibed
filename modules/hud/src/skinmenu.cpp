// The main menu's CONTINUE - see skinmenu.h. Facts: docs\FREELANCER_RE.md "Main menu and load screen": the update 0x566510 rewrites every button through its vt[0x14]
// during the slide and leaves them alone at rest, so a place is checked each frame and written again whenever the game wrote its own.
#include "skinmenu.h"

#include "flexe.h"
#include "game/fl.h"
#include "game/menu.h"
#include "game/session.h"
#include "log.h"
#include "skin_rules.h"
#include "skinkit.h"
#include "ui/hud2_kit.h"
#include "ui/hud_rows.h"
#include "ui/winplates.h"
#include "uiskin.h"
#include "version.h"

#include <algorithm>
#include <sstream>
#include <vector>

namespace skinmenu
{
namespace
{
using namespace hud2kit;
using namespace skinrules;
using namespace winplates;

constexpr u32 kPos = 0x60;

struct Place
{
    u32 ctl = 0;
    float native = 0, mine = 0; // y, in the UI's units
};
std::vector<Place> g_places;
bool g_have = false;       // a save to load
bool g_placed = false;     // the five stand at their new places
std::string g_file;
std::wstring g_label;      // beside the chip
Box g_chip;
bool g_chipUp = false, g_hover = false, g_padFocus = false;
u32 g_mouseWas = 0;
float g_wait = -1;         // >= 0: a load under way
float g_retry = 0;
u32 g_listGen = 0xFFFFFFFFu;

// The manager's list, newest first by the file time it read at the game's start or wrote since.
void ReadSaves()
{
    g_have = false, g_file.clear(), g_label.clear();
    u32 head = 0, node = 0;
    if (!exe::SaveList.ok() || !R32(static_cast<u32>(exe::SaveList.get()), head) || !head || !R32(head, node))
        return;
    struct Entry
    {
        char file[64];
        u32 record;
    };
    std::vector<Entry> entries;
    std::vector<SaveStamp> stamps;
    entries.reserve(512);
    for (int guard = 0; node && node != head && guard < 512; ++guard)
    {
        u32 record = 0, next = 0;
        if (!R32(node + 8, record) || !R32(node, next) || !record)
            break;
        Entry e = {};
        u8 hidden = 0;
        u32 lo = 0, hi = 0;
        if (RBytes(record, e.file, sizeof(e.file) - 1) && R8(record + 0x200, hidden) && R32(record + 0x228, lo) && R32(record + 0x22C, hi))
        {
            e.record = record;
            entries.push_back(e);
            SaveStamp s;
            s.time = (static_cast<uint64_t>(hi) << 32) | lo, s.hidden = hidden != 0;
            stamps.push_back(s);
        }
        node = next;
    }
    for (size_t i = 0; i < stamps.size(); ++i)
        stamps[i].file = entries[i].file;
    const int best = LatestSave(stamps.data(), static_cast<int>(stamps.size()));
    if (best < 0)
        return;
    g_have = true;
    g_file = entries[static_cast<size_t>(best)].file;
    u32 text = 0;
    if (R32(entries[static_cast<size_t>(best)].record + 0x218, text) && text)
        for (int i = 0; i < 48; ++i)
        {
            wchar_t c = 0;
            if (!RBytes(text + static_cast<u32>(i) * 2, &c, 2) || c < 0x20)
                break;
            g_label.push_back(c);
        }
    const session::Mode mode = session::OfSave(g_file);
    if (mode == session::ASSISTED || mode == session::SPEEDRUN) // never into another mode unsaid
    {
        const std::string m = session::ModeName(mode);
        g_label = std::wstring(m.begin(), m.end()) + (g_label.empty() ? std::wstring() : L"  /  " + g_label);
    }
}

bool SetY(const Place& p, float y)
{
    u32 vt = 0, set = 0;
    float pos[3] = {};
    if (!R32(p.ctl, vt) || !exe::VtFrontButton.is(vt) || !R32(vt + 0x14, set) || !set || !RBytes(p.ctl + kPos, pos, sizeof(pos)))
        return false;
    pos[1] = y;
    __try
    {
        reinterpret_cast<void(__thiscall*)(u32, const float*)>(set)(p.ctl, pos);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

void Place5(const std::vector<u32>& buttons)
{
    if (g_places.size() != buttons.size())
    {
        g_places.clear();
        for (u32 b : buttons)
        {
            Place p;
            p.ctl = b;
            g_places.push_back(p);
        }
    }
    // the game's y where the game wrote it last: a y that is not this file's is the game's
    for (Place& p : g_places)
    {
        float pos[3] = {};
        if (!RBytes(p.ctl + kPos, pos, sizeof(pos)))
            return;
        if (fabsf(pos[1] - p.mine) > 0.0005f || p.mine == 0)
            p.native = pos[1];
    }
    std::sort(g_places.begin(), g_places.end(), [](const Place& a, const Place& b) { return a.native > b.native; }); // UI y falls down the screen
    const float first = g_places.front().native, last = g_places.back().native;
    bool ok = true;
    for (size_t i = 0; i < g_places.size(); ++i)
    {
        Place& p = g_places[i];
        const float want = MenuSlotY(first, last, static_cast<int>(i) + 1);
        float pos[3] = {};
        RBytes(p.ctl + kPos, pos, sizeof(pos));
        if (fabsf(pos[1] - want) > 0.0005f)
            ok = SetY(p, want) && ok;
        p.mine = want;
    }
    g_placed = ok;
    if (!ok)
        GiveBack();
}
} // namespace

void GiveBack()
{
    for (const Place& p : g_places)
    {
        float pos[3] = {};
        if (p.mine != 0 && RBytes(p.ctl + kPos, pos, sizeof(pos)) && fabsf(pos[1] - p.mine) <= 0.0005f)
            SetY(p, p.native);
    }
    g_places.clear();
    g_placed = false;
    g_chipUp = false;
}

void Layout(u32 menuRoot, const std::vector<u32>& buttons, bool on)
{
    if (!on || !menuRoot || NativeListed(hudrows::g.skinNative, "continue"))
    {
        if (!g_places.empty())
            GiveBack();
        g_chipUp = g_padFocus = false;
        return;
    }
    if (g_listGen != g_gen)
        ReadSaves(), g_listGen = g_gen;
    if (!g_have || buttons.size() != 5)
    {
        if (!g_places.empty())
            GiveBack();
        return;
    }
    Place5(buttons);
    Box a, b;
    if (!g_placed || g_places.size() != 5 || !RectOf(g_places[0].ctl, a) || !RectOf(g_places[4].ctl, b) || !a.box)
    {
        g_chipUp = false;
        return;
    }
    const float pitch = (b.cy - a.cy) / 4.0f;
    g_chip = a;
    g_chip.cy = a.cy - pitch, g_chip.y0 = a.y0 - pitch, g_chip.y1 = a.y1 - pitch;
    g_chipUp = true;
}

void Frame(float dt, bool mouse)
{
    const float mx = static_cast<float>(fl::MouseX()), my = static_cast<float>(fl::MouseY());
    g_hover = g_chipUp && mouse && mx >= g_chip.x0 && mx <= g_chip.x1 && my >= g_chip.y0 && my <= g_chip.y1 &&
              !uiskin::ModalOpen(); // (a dialog over the menu owns the mouse)
    const u32 buttons = fl::MouseButtons() & 1u;
    if (g_hover && g_mouseWas && !buttons) // on release, as the game's own buttons
        Go();
    g_mouseWas = buttons;
    if (g_wait < 0)
        return;
    g_wait += dt, g_retry -= dt;
    if (g_wait > kContinueTimeoutS)
    {
        Log("skin: CONTINUE gave up on %s: the load screen stays open", g_file.c_str());
        g_wait = -1;
        return;
    }
    if (g_retry > 0 || !session::CharSelect())
        return;
    g_retry = 0.25f;
    std::string what;
    if (menu::Load(g_file, &what))
    {
        session::NoteLoading(g_file);
        Log("skin: CONTINUE %s", what.c_str());
        g_wait = -1;
    }
}

void Draw(u32 menuRoot)
{
    if (menuRoot)
        if (const u32 v = Child(menuRoot, "Version"))
        {
            Box b;
            std::wstring s;
            if (RectOf(v, b) && TextOf(v, s))
            {
                const float size = skinkit::Px(0.024f);
                Text(b.cx - TextWidth(s, size) / 2 - skinkit::Px(0.020f), b.cy, L"FLVIBED " FLV_VERSION, size, Argb(kDim, 1), Right);
            }
        }
    if (!g_chipUp)
        return;
    skinkit::ChipState st;
    st.focused = g_hover || g_padFocus;
    skinkit::Chip(g_chip.x0, g_chip.y0, g_chip.x1, g_chip.y1, L"CONTINUE", st);
    if (!g_label.empty())
        Text(g_chip.x1 + skinkit::Px(0.024f), g_chip.cy, g_label, skinkit::Px(kSection), Argb(kChrome, 1));
}

bool Offered() { return g_chipUp; }
bool Focused() { return g_chipUp && (g_hover || g_padFocus); }
void Focus(bool on) { g_padFocus = on; }

void Go()
{
    std::string what;
    if (!g_chipUp || g_wait >= 0 || uiskin::ModalOpen() || !menu::ClickMainButton(menu::kLoadGame, &what))
        return;
    g_wait = 0, g_retry = 0;
}

std::string Status()
{
    std::ostringstream o;
    o << "skinmenu save=" << (g_have ? g_file : std::string("none")) << " chip=" << g_chipUp << " placed=" << g_placed << " wait=" << g_wait << " rect=" << g_chip.x0 << "," << g_chip.y0 << "," << g_chip.x1 << "," << g_chip.y1;
    return o.str();
}
} // namespace skinmenu
