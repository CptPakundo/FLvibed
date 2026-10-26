// HUD 2's drawing kit (docs\UI_HUD2.md 5.2 "one render pass"): its own small Direct3D 8 batcher over the same device
// and vertex format as core\game\render (that one starts a new draw call for every textured quad - an arc of 14
// segments would be 28 draw calls). Everything HUD 2 draws goes into four layers, each one draw call at the end of the
// pass, in order: shapes (plates, bars, arcs, glyph rings: the small font's white texel), icons (the game's flat icon
// atlas, core\game\flicons), small text, large text - plates lie under icons and text by construction. Text is the
// game's Agency FB (core\game\gamefont) rasterised at two sizes and drawn scaled (linear filtering), with the navy
// outline free text needs over a bright sky (a 4-offset shadow). The controller glyphs are drawn here too, in the HUD
// tint (UI_HUD2.md 1.7: shape + letter in the chrome; glyph_style's family decides the letters / shapes; glyph_look 1
// colours the face marks as the classic atlas does, glyphs::LookFor), and the
// keyboard-and-mouse form's keycaps. Coordinates in back-buffer pixels; sizes by the caller (fractions of H). Nothing
// is allocated per frame once the vertex lists have grown; nothing is drawn at alpha 0.
#pragma once
#include "common.h"
#include "game/flicons.h"
#include "game/textfit.h"
#include "glyphs.h"

#include <string>

namespace hud2kit
{
// Colours (0xRRGGBB; the mock-up's palette, tools\uimock\hud2.py)
constexpr u32 kInk = 0xE2F0FC;     // primary text
constexpr u32 kChrome = 0x82C9F3;  // lines, labels, glyphs
constexpr u32 kDim = 0x5C80A0;     // secondary text, empty segments
constexpr u32 kGlass = 0x10122C;   // plate fill
constexpr u32 kCyan = 0x18ACE2;    // cruise, the autopilot
constexpr u32 kActive = 0xFCF344;  // the game's active yellow
constexpr u32 kHostile = 0xEC4034; // hostile red
constexpr u32 kFriend = 0x60D660;  // friendly green
constexpr u32 kNeutral = 0xE8E8E8;
constexpr u32 kShield = 0x4E84FF;
constexpr u32 kHull = 0xD63E3E;
constexpr u32 kPower = 0xE8D25C;
constexpr u32 kAmber = 0xFFC478;
constexpr u32 kWarn = 0xE8A33A;  // a used-up / missing answer: the D-pad slot an alert names, its "0" (the round 4 critique)
constexpr u32 kNavy = 0x060614; // the text outline

inline u32 Argb(u32 rgb, float a)
{
    const float c = a < 0 ? 0 : (a > 1 ? 1 : a);
    return (static_cast<u32>(c * 255 + 0.5f) << 24) | (rgb & 0xFFFFFF);
}

// The pass: false when there is no device / font (nothing drawn this frame). `textPx` = the small font's em (the big
// one is 1.25 of it); fonts are rebuilt only when it or the device changes.
bool Begin(void* device, int screenW, int screenH, int textPx);
void End(); // the four draw calls
// The next End writes the colour channels only, the back buffer's alpha kept (a pass under the game's 3D views: the
// target view drew nothing over a backing that had written the alpha, 25 Oct 2026).
void SetKeepDestAlpha();
void OnDeviceLost();
void Shutdown();

// Global alpha multiplier for everything that follows (the fades): every colour's alpha is scaled by it.
void SetAlpha(float a);
float Alpha();
// Global offset added to every position that follows (the transition's slide).
void SetOffset(float dx, float dy);

// ---- shapes (layer 0)
void Tri(float x0, float y0, float x1, float y1, float x2, float y2, u32 argb);
void Quad(const float xy[8], u32 argb);
void Rect(float x, float y, float w, float h, u32 argb);
void Grad(float x, float y, float w, float h, u32 tl, u32 tr, u32 br, u32 bl);
void Line(float x0, float y0, float x1, float y1, float width, u32 argb);
// A convex polygon (fan) and its outline.
void Poly(const float* xy, int n, u32 argb);
void Outline(const float* xy, int n, float width, u32 argb);
// An annular sector from a0 to a1 degrees (0 = 3 o'clock, clockwise on screen), radius r at its centre line.
void Arc(float cx, float cy, float r, float a0, float a1, float width, u32 argb);
void Ring(float cx, float cy, float r, float width, u32 argb);
void Disc(float cx, float cy, float r, u32 argb);
// The mock-up's segmented bar (the native gauge's blocks) and arc: `frac` lit from the left (align right: from the
// right), the empty blocks dim.
void SegBar(float x, float y, float w, float h, float frac, u32 rgb, int n, float gap = -1, bool right = false);
void SegArc(float cx, float cy, float r, float a0, float a1, float width, float frac, u32 rgb, int n, float gapDeg,
            bool reverse, u32 emptyRgb = 0x283050, float emptyWidth = -1); // emptyWidth: the empty blocks' (-1 = width)
// A glass plate: chamfered corners where flagged (tl, tr, br, bl), hairline rim, an accent edge (side 'l' / 't' /
// 'b', 0 none) in a colour. Alphas 0..1 (the mock's alpha / 255).
void Plate(float x0, float y0, float x1, float y1, float chamfer, int corners, float fill = 0.6f, u32 rim = kChrome,
           float rimA = 0.43f, char accent = 0, u32 accentRgb = 0);
constexpr int kTL = 1, kTR = 2, kBR = 4, kBL = 8;

// ---- icons (layer 1): the game's flat icon, tinted; false when the atlas is not ready (the caller draws a shape)
bool Icon(flicons::Icon icon, float cx, float cy, float size, u32 argb, float angleRad = 0);

// ---- text (layers 2 / 3)
enum Align
{
    Left,
    Center,
    Right
};
// Text `size` px tall (the em), vertically centred on y (the mock's 'm' anchor); `track` = extra letter spacing in ems;
// `outline` = the navy shadow. Returns the width.
float Text(float x, float y, const std::wstring& s, float size, u32 argb, Align align = Left, float track = 0,
           bool outline = true);
float TextWidth(const std::wstring& s, float size, float track = 0);
// THE text-fit rule (core\game\textfit.h; every box FLvibed draws text into): unchanged when it fits maxW, else the size
// steps down - only as far as needed - to the floor (default: TextFloor(), the legibility floor: 0.024 H, never under
// 14 px; every text the kit draws is clamped up to it as well), and only then the text is cut with an ellipsis. Cached
// per string / size / box (valid for the pass). TextFit draws the result and returns its width.
const textfit::Result& FitText(const std::wstring& s, float size, float maxW, float track = 0, float floor = -1);
float TextFit(float x, float y, const std::wstring& s, float size, float maxW, u32 argb, Align align = Left,
              float track = 0, bool outline = true, float floor = -1);
float TextFloor();
// The rule for a box that can take more lines: when one line would be cut even at the floor, the text wraps by whole
// words onto up to maxLines lines at the floor, the last line cut by the rule. Fills out[0..n), returns n and the size.
int FitLines(const std::wstring& s, float size, float maxW, float track, std::wstring* out, int maxLines, float& outSize);
// The text cut with an ellipsis to fit maxW at this size (no step down: a caller that cannot change the size).
std::wstring Fit(const std::wstring& s, float size, float maxW, float track = 0);

// ---- controller glyphs and keycaps in the HUD tint
// The glyph of a pad button centred on (cx, cy), h tall: face buttons a ring with the family's letter / shape, bumpers
// and triggers a pill with their name, sticks a ring with L / R, the D-pad a cross (an arm lit for DU..DR), View / Menu
// a ring with the mark; `hold` adds the broken outer ring.
void Glyph(glyphs::Id id, float cx, float cy, float h, u32 argb, bool hold = false);
float GlyphWidth(glyphs::Id id, float h);
// A keyboard key: a rounded keycap with its name ("K", "F1", "SPACE"); returns its width.
float Keycap(const std::wstring& name, float cx, float cy, float h, u32 argb, bool measure = false);

std::string Status();
} // namespace hud2kit
