// HUD 2's cursor and tooltip - see skinover.h. The cursor's shapes are the sheet's, in fractions of its size.
#include "skinover.h"

#include "flexe.h"
#include "game/baseinfo.h"
#include "game/deckmode.h"
#include "game/fl.h"
#include "game/ini.h"
#include "game/hostcalls.h"
#include "game/render.h"
#include "hook.h"
#include "log.h"
#include "navnew.h"
#include "navskin.h"
#include "nnstrip.h"
#include "skin_rules.h"
#include "skinkit.h"
#include "str.h"
#include "text.h"
#include "uistrip.h"
#include "ui/hud2_kit.h"
#include "ui/hud_rows.h"
#include "ui/ui.h"
#include "ui/views.h"
#include "ui/winplates.h"

#include <map>
#include <set>
#include <sstream>
#include <string>

namespace skinover
{
namespace
{
using namespace hud2kit;
using namespace skinrules;
using winplates::R32;
using winplates::RBytes;

// The game's two hover help controls as read before the UI pass (FREELANCER_RE "The hover help"): 0 the ToolTip box, 1 the Rollover line.
struct Help
{
    std::wstring text;
    float x = 0, y = 0; // the game's place, pixels
    bool live = false;
    TipRect drawn;      // where HUD 2's stood this frame (state report)
    bool moved = false; // it stands away from the game's place: something HUD 2 draws over the game's UI was there
};
Help g_help[2];
// A HUD 2 word's help (WordHelp): its control's texts, read when the cursor comes onto the word
struct WordTip
{
    u32 ctl = 0;
    std::wstring word, text;
    TipRect box;    // the word's click box
    float rest = 0; // seconds the cursor stayed on it
    bool live = false;
    TipRect drawn;
};
WordTip g_word;
std::vector<TipRect> g_blocks; // what the help kept clear of, the last frame it stood (state report)
std::set<std::string> g_unknown;
const CursorRow* g_row = nullptr;
bool g_mapTag = false; // the last Draw stood the ToolTip as the full-screen map's name tag
bool g_sheet = false; // debug `uiskin cursorsheet 1`: every flight reticle drawn in a row (the owner judges the set from one frame)

u32 Tint(CursorTint t)
{
    switch (t)
    {
    case CursorTint::Neutral: return kNeutral;
    case CursorTint::Hostile: return kHostile;
    case CursorTint::Friend: return kFriend;
    case CursorTint::Amber: return kAmber;
    default: return kChrome;
    }
}

void DrawArrow(float x, float y, float s, u32 rgb)
{
    // the tip on the game's hot spot: tip, right wing, notch, lower wing
    const float pts[8] = {x, y, x + 0.95f * s, y + 0.60f * s, x + 0.46f * s, y + 0.56f * s, x + 0.30f * s, y + 1.05f * s};
    Tri(pts[0], pts[1], pts[2], pts[3], pts[4], pts[5], Argb(kGlass, 0.9f));
    Tri(pts[0], pts[1], pts[4], pts[5], pts[6], pts[7], Argb(kGlass, 0.9f));
    Outline(pts, 4, std::max(2.0f, s * 0.05f), Argb(rgb, 1));
}

void DrawStretch(float x, float y, float s, u32 rgb)
{
    const float h = s * 0.5f, a = s * 0.2f, w = std::max(2.0f, s * 0.05f);
    Line(x - h + a, y, x + h - a, y, w, Argb(rgb, 1));
    const float left[6] = {x - h, y, x - h + a, y - a, x - h + a, y + a};
    const float right[6] = {x + h, y, x + h - a, y - a, x + h - a, y + a};
    Outline(left, 3, w, Argb(rgb, 1));
    Outline(right, 3, w, Argb(rgb, 1));
}

// The game's own `spin` of a cursor (DATA\mouse.ini, a mod's too), read once: the fire cursors turn, as on the classic HUD (the owner, 6 Nov 2026: HUD 2 no more static than it).
float GameSpin(const char* name)
{
    static std::map<std::string, float> spin;
    static bool read = false;
    if (!read)
    {
        read = true;
        for (const ini::Section& s : ini::Read(ini::DataPath("mouse.ini")))
        {
            const std::string *nick = s.Get("nickname"), *v = s.Get("spin");
            if (s.type == "cursor" && nick && v)
                spin[*nick] = static_cast<float>(atof(v->c_str()));
        }
    }
    const auto it = spin.find(name);
    return it == spin.end() ? 0.0f : it->second;
}
// Degrees a second for one unit of it: the game's unit is radians a second (traced 7 Nov 2026: 0x41DAE0 sets the cursor's angle +0x58 = the frame clock's seconds x
// spin +0x5C, and 0x41D9C0 feeds it to fsin / fcos), so the fire cursors' spin 10 is 573 degrees a second, 1.6 turns - as vanilla (the owner, 7 Nov 2026: "I want the
// new cursor to rotate faster. How fast does vanilla cursor rotate?"). Was 18 (a guess: 3.2 times too slow). The four darts repeat at 6.4 Hz, over HUD 2's 3 Hz:
// the owner's call for the game's own rate
constexpr float kSpinDegPerUnit = 57.29578f;

// One of the reticle's inward triangles at angle `deg`: its point at rTip from the centre, its base at rBase, `half` across. Its shape says the attitude as the
// target's carets do (hud2_target.cpp CaretQuad): 0 hostile solid, 1 neutral hollow, 2 friendly open (the base left out); the navy keyline keeps it read on a bright sky
void InTri(float x, float y, float deg, float rTip, float rBase, float half, int attitude, float w, u32 rgb)
{
    const float rad = deg * 3.14159265f / 180.0f, c = cosf(rad), n = sinf(rad);
    const float t[2] = {x + c * rTip, y + n * rTip}, b[2] = {x + c * rBase, y + n * rBase};
    const float l[2] = {b[0] - n * half, b[1] + c * half}, r[2] = {b[0] + n * half, b[1] - c * half};
    const u32 col = Argb(rgb, 1), navy = Argb(kNavy, 0.7f);
    Line(l[0], l[1], t[0], t[1], w + 2, navy);
    Line(r[0], r[1], t[0], t[1], w + 2, navy);
    if (attitude != 2)
        Line(l[0], l[1], r[0], r[1], w + 2, navy);
    if (attitude == 0)
        Tri(l[0], l[1], r[0], r[1], t[0], t[1], col);
    Line(l[0], l[1], t[0], t[1], w, col);
    Line(r[0], r[1], t[0], t[1], w, col);
    if (attitude != 2)
        Line(l[0], l[1], r[0], r[1], w, col);
}

// The flight reticle, a bit closer to the game's own (DATA\screens\cursor.TXM: one thin ring, a dot, four darts on the diagonals pointing in; the owner, 7 Nov
// 2026: "don't just make it spin, change it to include inwards-pointing triangles. Also, make the cursor look just a bit closer to vanilla FL"): four long arcs
// on the axes that read as the ring, the dot, and over a ship four triangles in the diagonal gaps pointing at the middle, their shape the attitude (hostile
// solid, neutral hollow, friendly open, as the target's carets); firing pushes them in and draws the ring bolder (the game's hostile_fire); cannot fire = the
// bar across, no triangles. Free aim (`cross`) is the ring and the dot alone, as the game's. Everything turns with the game's spin.
void DrawReticle(float x, float y, float s, u32 rgb, CursorTint tint, unsigned form, float turn)
{
    const bool firing = (form & FormSpokes) != 0, noFire = (form & FormBar) != 0;
    const float r = s * 0.38f, w = std::max(2.0f, s * (firing ? 0.075f : 0.06f));
    for (int q = 0; q < 4; ++q) // four arcs on the axes, the gaps on the diagonals
        Arc(x, y, r, turn - 30.0f + 90.0f * static_cast<float>(q), turn + 30.0f + 90.0f * static_cast<float>(q), w, Argb(rgb, 1));
    if (noFire) // cannot fire: one bar across
    {
        Line(x - r * 0.72f, y + r * 0.72f, x + r * 0.72f, y - r * 0.72f, w, Argb(rgb, 1));
        return;
    }
    Disc(x, y, std::max(1.5f, s * 0.05f), Argb(rgb, 1));
    const int attitude = tint == CursorTint::Hostile ? 0 : tint == CursorTint::Neutral ? 1 : tint == CursorTint::Friend ? 2 : -1;
    if (attitude < 0)
        return;
    const float tip = firing ? r * 0.42f : r * 0.68f, base = firing ? r * 1.42f : r * 1.40f, half = s * (firing ? 0.072f : 0.058f); // (darts, as the game's: longer than wide)
    const float tw = std::max(1.5f, s * 0.04f);
    for (int q = 0; q < 4; ++q)
        InTri(x, y, turn + 45.0f + 90.0f * static_cast<float>(q), tip, base, half, attitude, tw, rgb);
}

void DrawBubble(float x, float y, float s, u32 rgb, const wchar_t* word)
{
    // the tail's tip on the game's hot spot, the bubble over it
    const float size = std::max(TextFloor(), s * 0.55f), tw = TextWidth(word, size), w = std::max(s, tw + s * 0.5f), h = s * 0.85f, tail = s * 0.28f;
    const float x0 = x - w * 0.32f, y1 = y - tail, y0 = y1 - h;
    hud2kit::Plate(x0, y0, x0 + w, y1, s * 0.12f, kTL | kBR, 0.92f, kChrome, 0);
    skinkit::Rim(x0, y0, x0 + w, y1, s * 0.12f, kTL | kBR, 1);
    Tri(x, y, x - tail * 0.2f, y1, x + tail * 0.9f, y1, Argb(kGlass, 0.92f));
    Line(x, y, x - tail * 0.2f, y1, std::max(2.0f, s * 0.05f), Argb(rgb, 1));
    Line(x, y, x + tail * 0.9f, y1, std::max(2.0f, s * 0.05f), Argb(rgb, 1));
    Text(x0 + w / 2, (y0 + y1) / 2, word, size, Argb(kInk, 1), Center);
}

void DrawCursor(float W)
{
    const float x = static_cast<float>(fl::MouseX()), y = static_cast<float>(fl::MouseY()), s = CursorSize(W);
    const u32 rgb = Tint(g_row->tint);
    switch (g_row->shape)
    {
    case CursorShape::Arrow: DrawArrow(x, y, s, rgb); break;
    case CursorShape::Stretch: DrawStretch(x, y, s, rgb); break;
    case CursorShape::Reticle:
    {
        const float rate = hudrows::g.skinMotion == 2 ? 0.0f : GameSpin(g_row->name) * kSpinDegPerUnit;
        DrawReticle(x, y, s, rgb, g_row->tint, g_row->form, fmodf(static_cast<float>(GetTickCount64() % 3600000) / 1000.0f * rate, 360.0f));
        break;
    }
    case CursorShape::Bubble: DrawBubble(x, y, s, rgb, g_row->word); break;
    }
}

// The game's name of the moment: a known row, or the game's own cursor with one log line per name.
const CursorRow* CurrentRow()
{
    const char* name = fl::CursorName();
    if (!name || !*name)
        return nullptr;
    const CursorRow* row = CursorFor(name);
    if (!row && g_unknown.insert(name).second)
        Log("skin: cursor '%s' is not in the set: it stays the game's", name);
    return row;
}
} // namespace

bool HelpRead(int which, u32 ctl)
{
    if (which < 0 || which > 1)
        return false;
    Help& h = g_help[which];
    h.live = false;
    u8 state = 0;
    u32 text = 0;
    float pos[3] = {};
    // drawn while state bits 0 and 1 stand; the text object at +0x7C (the class's vt[0x70] sets it) holds the string's pointer first
    if (!ctl || !winplates::R8(ctl + 0x6C, state) || (state & 3) != 3 || !R32(ctl + 0x7C, text) || !text || !RBytes(ctl + 0x60, pos, sizeof(pos)))
        return false;
    std::wstring s;
    for (int i = 0; i < 400; ++i)
    {
        wchar_t c = 0;
        if (!RBytes(text + static_cast<u32>(i) * 2, &c, 2))
            return false;
        if (!c)
            break;
        if (c < 0x20 && c != L'\n')
            return false; // not text: the field is not what the decompiler said
        s.push_back(c);
    }
    if (s.empty() || !ui::Project(pos[0], pos[1], pos[2], h.x, h.y))
        return false;
    h.text.swap(s);
    h.live = true;
    return true;
}

void HelpClear(int which)
{
    if (which >= 0 && which <= 1)
        g_help[which].live = false;
}

bool MapTag(float r[4], std::wstring& text)
{
    if (!g_mapTag || !g_help[0].live)
        return false;
    const TipRect& d = g_help[0].drawn;
    r[0] = d.x0, r[1] = d.y0, r[2] = d.x1, r[3] = d.y1;
    text = g_help[0].text;
    return true;
}

namespace
{
std::wstring Trim(std::wstring s)
{
    while (!s.empty() && iswspace(s.back()))
        s.pop_back();
    size_t i = 0;
    while (i < s.size() && iswspace(s[i]))
        ++i;
    return s.substr(i);
}

// The game's UI option `name` on, as the help's own draw tests it (>= 0.999: 0x4BDEA0 "enable_tooltips", 0x4B5CA0 "enable_rollover"); off when unreadable.
bool OptionOn(const char* name)
{
    const u32 o = exe::UiOptionFind.ok() ? hk::Call<u32(__cdecl*)(const char*)>(exe::UiOptionFind, name) : 0;
    float v = 0;
    return o && RBytes(o + 0x14, &v, sizeof(v)) && v >= 0.999f;
}

// The ToolTip's text for a control, as 0x4BD880 makes it from what the control's help hands it (FREELANCER_RE "The hover help's fields"): the hint IDS +0xCC,
// and on an icon button (vt[0x88] 0x55BDE0 hands its command +0x34C too) " (<key>)" while a key is bound to the command - "Neural Net Log (F5)".
std::wstring ShortTip(u32 ctl)
{
    std::wstring s = baseinfo::HintText(ctl);
    u32 vt = 0, cmd = 0;
    int key[3] = {};
    if (s.empty() || !R32(ctl, vt) || !exe::VtIconButton.is(vt) || !R32(ctl + 0x34C, cmd) || !cmd || !exe::CommandKey.ok() || !exe::CommandKeyText.ok() ||
        !hk::Call<bool(__cdecl*)(int, int*)>(exe::CommandKey, static_cast<int>(cmd), key) || !key[1])
        return s;
    wchar_t name[256] = {};
    hk::Call<void(__cdecl*)(const int*, wchar_t*, u32)>(exe::CommandKeyText, key, name, 256u);
    name[255] = 0;
    return name[0] ? s + L" (" + name + L")" : s;
}

// The Rollover's text for a control: its map at +0x150 from hint IDS to long hint IDS (head +0x154, nil +0x158; node +0 left, +8 right, +0xC the key, +0x10
// the long hint's IDS), looked up as 0x4B5CA0 does (0x432240: the lower bound, then the key itself). Read in game 11 Nov 2026: 1057 -> 2057, 6506 -> 2306.
std::wstring LongHint(u32 ctl)
{
    static u32 roll = 0;
    u32 vt = 0, ids = 0, head = 0, nil = 0, node = 0, best = 0, key = 0, hint = 0;
    if (!roll || !R32(roll, vt) || !exe::VtRollover.is(vt))
        roll = static_cast<u32>(ui::FindControl("Rollover"));
    if (!roll || !R32(roll, vt) || !exe::VtRollover.is(vt) || !R32(ctl + 0xCC, ids) || !ids || !R32(roll + 0x154, head) || !R32(roll + 0x158, nil) ||
        !head || !R32(head + 4, node))
        return L"";
    for (int guard = 0; node && node != nil && node != head && guard < 64; ++guard)
    {
        if (!R32(node + 0xC, key))
            return L"";
        if (key < ids)
        {
            if (!R32(node + 8, node))
                return L"";
        }
        else
        {
            best = node;
            if (!R32(node, node))
                return L"";
        }
    }
    if (!best || !R32(best + 0xC, key) || key != ids || !R32(best + 0x10, hint) || !hint)
        return L"";
    return Trim(text::Ids(hint));
}
} // namespace

void WordHelp(u32 ctl, const std::wstring& word, const float r[4], float dt, bool tip, bool rollover)
{
    if (!ctl)
    {
        g_word = WordTip();
        return;
    }
    if (ctl != g_word.ctl || word != g_word.word)
    {
        g_word = WordTip();
        g_word.ctl = ctl, g_word.word = word;
        const std::wstring s = tip && OptionOn("enable_tooltips") ? ShortTip(ctl) : std::wstring();
        const std::wstring l = rollover && OptionOn("enable_rollover") ? LongHint(ctl) : std::wstring();
        g_word.text = skinrules::WordHelpText(word, s, l);
    }
    g_word.box = {r[0], r[1], r[2], r[3]};
    g_word.rest += dt;
    g_word.live = g_word.rest >= skinrules::kHelpRest && !g_word.text.empty();
}

namespace
{
// What HUD 2 draws over the game's UI this frame, where a help box must not stand (the game's own box sat under it, GitHub issue 71): the strip and the
// character card, the room words and the NeuroNet box, a NeuroNet window's tabs, the full-screen map's top row and chips, what was kept out over the UI pass (the
// map's cards and lane), the prompt lane.
// Not the windows' own plates: they lie under the UI pass, and a row's help belongs on its window.
std::vector<TipRect> Blocks(float W, float H)
{
    std::vector<TipRect> b;
    for (const views::HudRect& r : uistrip::Drawn())
        b.push_back({r.x0, r.y0, r.x1, r.y1});
    const winplates::DockedGeo& d = winplates::g_docked;
    if (d.live)
    {
        b.push_back({d.bx0, 0, d.bx1, d.by1});
        if (d.sub)
            b.push_back({d.sx0, 0, d.sx1, d.sy1});
        if (d.nn)
            b.push_back({d.nx0, 0, d.nx1, d.ny1});
    }
    float s[4];
    if (nnstrip::Rect(s))
    {
        b.push_back({s[0], s[1], s[2], s[3]});
        if (navnew::Grown()) // the full-screen map's top row: the system's name, the tabs, LAYERS (navskin DrawFull) - one band down to the tabs' foot
            b.push_back({0, 0, W, s[3]});
    }
    views::MapChip chips[32]; // the map's chips (LAYERS, CLOSE, the cards' actions)
    for (int i = 0, n = navskin::Chips(chips, 32); i < n; ++i)
        b.push_back({chips[i].x0, chips[i].y0, chips[i].x1, chips[i].y1});
    for (size_t i = winplates::g_keepUnder; i < winplates::g_keep.size(); ++i)
    {
        const winplates::KeepOut& k = winplates::g_keep[i];
        b.push_back({k.x0, k.y0, k.x1, k.y1});
    }
    if (views::LaneWanted() || views::DevicePromptVisible()) // (this late in the overlay the lane has drawn: its "wanted" is spent, its shown stands)
        b.push_back({0, H * deckmode::LaneTopH(), W, H});
    return b;
}
} // namespace

void Draw(void* device, bool on, bool pad)
{
    const bool cursorOn = on && !skinrules::NativeListed(hudrows::g.skinNative, "cursor") && exe::CursorShown.ok();
    g_row = cursorOn ? CurrentRow() : nullptr;
    // (hidden the frame before it is drawn, and the game's shows at once for anything this pass cannot draw)
    hostcalls::CursorHide(hostcalls::kCursorHideSkin, g_row != nullptr);
    const bool drawCursor = g_row && !hostcalls::CursorHiddenBesides(hostcalls::kCursorHideSkin) && Mem<u8>(exe::CursorShown) != 0;
    const bool drawTip = on && (g_help[0].live || g_help[1].live || g_word.live);
    g_mapTag = false;
    if (!device || (!drawCursor && !drawTip && !g_sheet))
        return;
    int sw = 0, sh = 0;
    render::SetDevice(device);
    if (!render::ScreenSize(sw, sh) || sh < 100 || !Begin(device, sw, sh, std::max(8, static_cast<int>(0.026f * sh + 0.5f))))
        return;
    SetAlpha(1), SetOffset(0, 0);
    const float W = static_cast<float>(sw), H = static_cast<float>(sh);
    skinkit::SetScreen(W, H);
    if (drawTip)
    {
        // the box first (it stands by what is hovered), then the line, each clear of what HUD 2 draws over the game's UI and of the one before
        std::vector<TipRect> blocks = Blocks(W, H);
        g_blocks = blocks;
        const float gap = H * 0.008f;
        for (Help& h : g_help)
        {
            skinkit::TooltipLines t;
            h.moved = false;
            if (!h.live || !skinkit::TooltipFit(h.text, t))
                continue;
            float x = h.x, y = h.y;
            if (&h == &g_help[0] && navnew::Grown())
            {
                // the full-screen map: the game places its box by its own map's size, which no longer stands (GitHub issue 75: "Alsterfeld" well away from
                // the field). What it names is under the cursor - the game's hit test found it there, the pad's map cursor too - so the tag stands below
                // right of the arrow
                const float s = CursorSize(W);
                x = static_cast<float>(fl::MouseX()) + s * 0.6f, y = static_cast<float>(fl::MouseY()) + s * 0.6f;
            }
            else if (pad) // no cursor for the help to stand by: a fixed line over the prompt lane
                x = W * deckmode::SideW(), y = H * (deckmode::LaneTopH() - 0.06f);
            TipRect want, at;
            want.x0 = x, want.y0 = y, want.x1 = x + t.w, want.y1 = y + t.h;
            skinrules::TipPlace(want, skinkit::SafeArea(), blocks.data(), static_cast<int>(blocks.size()), gap, at);
            skinkit::TooltipAt(at.x0, at.y0, t);
            h.drawn = at, h.moved = fabsf(at.x0 - x) > 1 || fabsf(at.y0 - y) > 1;
            g_mapTag |= &h == &g_help[0] && navnew::Grown();
            blocks.push_back(at);
        }
        skinkit::TooltipLines t;
        if (on && g_word.live && skinkit::TooltipFit(g_word.text, t)) // a word's help: under the word, at the free place nearest to it (mouse and pad alike)
        {
            TipRect want;
            want.x0 = g_word.box.x0, want.y0 = g_word.box.y1 + gap, want.x1 = want.x0 + t.w, want.y1 = want.y0 + t.h;
            skinrules::TipPlace(want, skinkit::SafeArea(), blocks.data(), static_cast<int>(blocks.size()), gap, g_word.drawn);
            skinkit::TooltipAt(g_word.drawn.x0, g_word.drawn.y0, t);
        }
    }
    if (drawCursor)
        DrawCursor(W);
    if (g_sheet) // debug: every flight reticle in a row across the middle, at the cursor's size, turning as the game spins it
    {
        const CursorRow* rows = CursorRows();
        const float s = CursorSize(W) * 1.5f, step = s * 1.6f;
        int n = 0;
        for (int i = 0; i < kCursorCount; ++i)
            n += rows[i].shape == CursorShape::Reticle ? 1 : 0;
        float x = W / 2 - step * (n - 1) / 2;
        const float t = static_cast<float>(GetTickCount64() % 3600000) / 1000.0f;
        for (int i = 0; i < kCursorCount; ++i)
            if (rows[i].shape == CursorShape::Reticle)
            {
                const float rate = hudrows::g.skinMotion == 2 ? 0.0f : GameSpin(rows[i].name) * kSpinDegPerUnit;
                DrawReticle(x, H * 0.42f, s, Tint(rows[i].tint), rows[i].tint, rows[i].form, fmodf(t * rate, 360.0f));
                Text(x, H * 0.42f + s * 0.8f, std::wstring(rows[i].name, rows[i].name + strlen(rows[i].name)), std::max(TextFloor(), s * 0.22f), Argb(kInk, 1), Center);
                x += step;
            }
    }
    End();
}

void Sheet(bool on) { g_sheet = on; }

void Shutdown()
{
    hostcalls::CursorHide(hostcalls::kCursorHideSkin, false);
    g_help[0].live = g_help[1].live = false;
    g_word = WordTip();
    g_row = nullptr;
}

std::string Status()
{
    std::ostringstream o;
    o << "skinover cursor=" << (g_row ? g_row->name : "game's") << " unknown=" << g_unknown.size() << " tip=" << (g_help[0].live ? 1 : 0)
      << " rollover=" << (g_help[1].live ? 1 : 0);
    static const char* const kName[2] = {"tooltip", "rollover"};
    for (int i = 0; i < 2; ++i)
        if (g_help[i].live)
        {
            char line[160];
            sprintf_s(line, "\n  %s game=%.0f,%.0f drawn=%.0f,%.0f-%.0f,%.0f%s \"", kName[i], g_help[i].x, g_help[i].y, g_help[i].drawn.x0, g_help[i].drawn.y0,
                      g_help[i].drawn.x1, g_help[i].drawn.y1, g_help[i].moved ? " moved" : "");
            o << line;
            for (wchar_t c : g_help[i].text.substr(0, 60))
                o << (c >= 0x20 && c < 0x7F ? static_cast<char>(c) : '?');
            o << "\"";
        }
    if (g_word.ctl)
    {
        char line[200];
        sprintf_s(line, "\n  word \"%s\" ctl=%08X rest=%.2f live=%d drawn=%.0f,%.0f-%.0f,%.0f \"", str::Narrow(g_word.word).c_str(), g_word.ctl, g_word.rest,
                  g_word.live ? 1 : 0, g_word.drawn.x0, g_word.drawn.y0, g_word.drawn.x1, g_word.drawn.y1);
        o << line;
        for (wchar_t c : g_word.text.substr(0, 160))
            o << (c >= 0x20 && c < 0x7F ? static_cast<char>(c) : '|');
        o << "\"";
    }
    if (g_help[0].live || g_help[1].live || g_word.live)
        for (const TipRect& b : g_blocks)
        {
            char line[96];
            sprintf_s(line, "\n  help keeps clear of %.0f,%.0f-%.0f,%.0f", b.x0, b.y0, b.x1, b.y1);
            o << line;
        }
    return o.str();
}
} // namespace skinover
