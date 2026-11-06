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
// Degrees a second for one unit of it. The game's unit is not traced: 10 gives half a turn a second, and the four arcs repeat at 2 Hz, under HUD 2's 3 Hz.
constexpr float kSpinDegPerUnit = 18.0f;

void DrawReticle(float x, float y, float s, u32 rgb, unsigned form, float turn)
{
    const float r = s * 0.38f, w = std::max(2.0f, s * 0.06f);
    for (int q = 0; q < 4; ++q) // four arcs on the diagonals
        Arc(x, y, r, turn - 70.0f + 90.0f * static_cast<float>(q), turn - 20.0f + 90.0f * static_cast<float>(q), w, Argb(rgb, 1));
    if (!(form & (FormSpokes | FormBar)))
        Disc(x, y, std::max(1.5f, s * 0.05f), Argb(rgb, 1));
    if (form & FormSpokes) // firing: spokes from the gaps towards the middle
    {
        const float a = s * 0.46f, b = s * 0.14f;
        for (int q = 0; q < 4; ++q)
        {
            const float rad = (turn + 90.0f * static_cast<float>(q)) * 3.14159265f / 180.0f, c = cosf(rad), n = sinf(rad);
            Line(x + c * b, y + n * b, x + c * a, y + n * a, w, Argb(rgb, 1));
        }
    }
    if (form & FormBar) // cannot fire: one bar across
        Line(x - r * 0.72f, y + r * 0.72f, x + r * 0.72f, y - r * 0.72f, w, Argb(rgb, 1));
    if (form & FormCorners) // hostile: corners round the arcs
    {
        const float c = s * 0.52f, l = s * 0.16f;
        for (int sx = -1; sx <= 1; sx += 2)
            for (int sy = -1; sy <= 1; sy += 2)
            {
                const float cx = x + c * static_cast<float>(sx), cy = y + c * static_cast<float>(sy);
                Line(cx, cy, cx - l * static_cast<float>(sx), cy, w, Argb(rgb, 1));
                Line(cx, cy, cx, cy - l * static_cast<float>(sy), w, Argb(rgb, 1));
            }
    }
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
        DrawReticle(x, y, s, rgb, g_row->form, fmodf(static_cast<float>(GetTickCount64() % 3600000) / 1000.0f * rate, 360.0f));
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
    if (!device || (!drawCursor && !drawTip))
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
    End();
}

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
