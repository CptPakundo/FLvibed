// HUD 2's cursor and tooltip - see skinover.h. The cursor's shapes are the sheet's, in fractions of its size.
#include "skinover.h"

#include "flexe.h"
#include "game/deckmode.h"
#include "game/fl.h"
#include "game/ini.h"
#include "game/hostcalls.h"
#include "game/render.h"
#include "log.h"
#include "skin_rules.h"
#include "skinkit.h"
#include "ui/hud2_kit.h"
#include "ui/hud_rows.h"
#include "ui/ui.h"
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

std::wstring g_tipText;
float g_tipX = 0, g_tipY = 0;
bool g_tipLive = false;
std::set<std::string> g_unknown;
const CursorRow* g_row = nullptr;
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

bool TipRead(u32 tip)
{
    g_tipLive = false;
    u8 showing = 0;
    u32 text = 0;
    float pos[3] = {};
    if (!tip || !winplates::R8(tip + 0x159, showing) || !showing || !R32(tip + 0x104, text) || !text || !RBytes(tip + 0x60, pos, sizeof(pos)))
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
    if (s.empty() || !ui::Project(pos[0], pos[1], pos[2], g_tipX, g_tipY))
        return false;
    g_tipText.swap(s);
    g_tipLive = true;
    return true;
}

void Draw(void* device, bool on, bool pad)
{
    const bool cursorOn = on && !skinrules::NativeListed(hudrows::g.skinNative, "cursor") && exe::CursorShown.ok();
    g_row = cursorOn ? CurrentRow() : nullptr;
    // (hidden the frame before it is drawn, and the game's shows at once for anything this pass cannot draw)
    hostcalls::CursorHide(hostcalls::kCursorHideSkin, g_row != nullptr);
    const bool drawCursor = g_row && !hostcalls::CursorHiddenBesides(hostcalls::kCursorHideSkin) && Mem<u8>(exe::CursorShown) != 0;
    const bool drawTip = on && g_tipLive;
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
        if (pad) // no cursor for the help to stand by: a fixed line over the prompt lane
            skinkit::Tooltip(W * deckmode::SideW(), H * (deckmode::LaneTopH() - 0.06f), g_tipText);
        else
            skinkit::Tooltip(g_tipX, g_tipY, g_tipText);
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
    g_tipLive = false;
    g_row = nullptr;
}

std::string Status()
{
    std::ostringstream o;
    o << "skinover cursor=" << (g_row ? g_row->name : "game's") << " unknown=" << g_unknown.size() << " tip=" << (g_tipLive ? 1 : 0);
    return o.str();
}
} // namespace skinover
