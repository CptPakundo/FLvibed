// HUD 2's drawing kit (modules\hud\docs\UI_HUD2.md 5.2 "one render pass"): its own small Direct3D 8 batcher over the same device
// and vertex format as core\game\render (that one starts a new draw call for every textured quad - an arc of 14
// segments would be 28 draw calls). Everything HUD 2 draws goes into four layers, each one draw call at the end of the
// pass, in order: shapes (plates, bars, arcs, glyph rings: the small font's white texel), icons (the game's flat icon
// atlas, core\game\flicons), small text, large text - plates lie under icons and text by construction. Text is the
// game's Agency FB (core\game\gamefont) rasterised at two sizes and drawn scaled (linear filtering), with the navy
// outline free text needs over a bright sky (a 4-offset shadow). The controller glyphs are drawn here too, in the HUD
// tint (UI_HUD2.md 1.7: the shape in the chrome; glyph_style's family decides the letters / shapes; the face marks take the family's
// colours as the classic atlas does, glyphs::FaceColours; glyph_look 2 keeps them in the tint), and the
// keyboard-and-mouse form's keycaps. Coordinates in back-buffer pixels; sizes by the caller (fractions of H). Nothing
// is allocated per frame once the vertex lists have grown; nothing is drawn at alpha 0. The lists are built with no
// device (hud2_kit.cpp); the device, the fonts' textures, the faces and the icons are behind hud2_kit_backend.h, in the
// game hud2_kit_d3d8.cpp (bench B1b, 12 Nov 2026).
#pragma once
#include "common.h"
#include "game/flicons.h"
#include "game/textfit.h"
#include "glyphs.h"

#include <string>
#include <unordered_map>

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
// The small font's em the kit measures and draws with now (the last Begin's, after its legibility floor); 0 before any.
int TextPx();
// Outside a pass: the fonts a Begin with `textPx` would draw with become the ones TextWidth / FitText measure with, on
// the last pass's device (nothing drawn, no pass begun). For a caller that measures before its own pass (HUD 2's layout
// in its Update): otherwise it measures with the size the last module's Begin left. False before the first pass.
bool MeasureAt(int textPx);
void End(); // the four draw calls
// The next End writes the colour channels only, the back buffer's alpha kept (a pass under the game's 3D views: the
// target view drew nothing over a backing that had written the alpha).
void SetKeepDestAlpha();
// A cut mark in the pass (`name` a string literal): the vertex count of every layer now, so the design bench's lists say
// which ranges are whose (HUD 2's parts, the comm caption, ...). Drawing ignores them; a pass's Begin clears them.
void Mark(const char* name);
// The clock the marquees run on (TextMarquee: when a cut text first showed), in seconds; nullptr = perf::Now, the
// game's. HUD 2's parts set their host's (modules\hud\src\hud2_frame.h), a program with no game its own.
void SetClock(double (*seconds)());
// The marquees' table (TextMarquee: when each cut text first showed, keyed by its string, size and room; a text not drawn
// for 30 passes starts over). Its caller owns it so a program with no game can start, save and restore it: HUD 2's
// parts keep theirs in their state and set it with their host (modules\hud\src\hud2_frame.h); nullptr = the kit's own.
// Every TextMarquee after the call uses it, whoever draws. A table set while it is empty takes over the clocks the last
// one held (so the switch to HUD 2's table at its first bind restarts no text another caller is scrolling).
struct MarqueeClock
{
    double t0 = 0;
    u32 seen = 0; // the pass it was last drawn in (the kit's pass count + 1: 0 = never)
};
struct Marquees
{
    std::unordered_map<unsigned long long, MarqueeClock> clocks;
};
void SetMarquees(Marquees* table);
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
// A centred meter (GitHub issue 28): the same blocks, the lit part `frac` of the span about its middle, trimmed continuously at both ends, so it drains
// to the middle and fills from it. A block cut by the trim keeps its empty form under the lit part.
void SegArcCentred(float cx, float cy, float r, float a0, float a1, float width, float frac, u32 rgb, int n, float gapDeg,
                   u32 emptyRgb = 0x283050, float emptyWidth = -1);
// The arcs that follow carry a navy keyline this many pixels wide along both edges, under their blocks, so they read over a bright planet (0 = none). It adds
// no area: a scope around the ring's arcs.
void SetArcKeyline(float px);
// A glass plate: chamfered corners where flagged (tl, tr, br, bl), hairline rim, an accent edge (side 'l' / 't' /
// 'b', 0 none) in a colour. Alphas 0..1 (the mock's alpha / 255).
void Plate(float x0, float y0, float x1, float y1, float chamfer, int corners, float fill = 0.6f, u32 rim = kChrome,
           float rimA = 0.43f, char accent = 0, u32 accentRgb = 0);
constexpr int kTL = 1, kTR = 2, kBR = 4, kBL = 8;

// ---- icons (layer 1): the game's flat icon, tinted; false when the atlas is not ready (the caller draws a shape)
bool Icon(flicons::Icon icon, float cx, float cy, float size, u32 argb, float angleRad = 0);
// An NPC ship's own top-down outline (GitHub 85), tinted, a `size` px square: false while it has none (no source - the
// bench -, no archetype, not baked yet), and the caller draws the class icon. The source answers a ship archetype and a pixel
// size with the atlas and the cell's UV (modules\hud\src\npcicons.*: it bakes on demand); cleared with nullptr.
using ShipUvFn = bool (*)(u32 arch, int px, void** texture, float uv[4]);
void SetShipSource(ShipUvFn fn);
bool ShipIcon(u32 arch, float cx, float cy, float size, u32 argb);
// A pilot's rank mark (GitHub 84): tier 1..3 open chevrons stacked in a `size` square centred at (cx, cy); 0 draws nothing.
void RankMark(int tier, float cx, float cy, float size, u32 argb);

// ---- text (layers 2 / 3)
enum Align
{
    Left,
    Center,
    Right
};
// Game text's styles (the owner, 12 Nov 2026, audit decision 56: the infocards' italic and underline drawn, one text kit
// for every page that shows the game's text). The game draws a styled run with a GDI font of that style (Common.dll
// DeviceTRC: one font per bold / italic / underline combination, docs\FREELANCER_RE.md "Rich text"); its faces have no
// italic file, so its italic is GDI's synthesized oblique: the upright glyphs sheared about the baseline, the advances
// unchanged (measured 13 Nov 2026 on the game's AGENCYB.TTF and AGENCYR.TTF: 0.34 px across per px up). The kit draws the
// same from its one atlas: kStyleItalic shears each glyph's quad by kItalicSlant (the width stays TextWidth's, so a layout
// measures styled text as plain), kStyleUnderline adds the face's underline (its outline metrics, as GDI's lfUnderline)
// under the whole run in the text's colour, its spaces too. HUD 2's text is already the bold face, so a bold run gets the
// game's other bold: GDI's synthesized emboldening (the game's slot-0 face, Arial Unicode MS, has no bold file; measured
// 13 Nov 2026 at 400 against 700 on its ARIALUNI.TTF, 16 to 200 px: each glyph's ink widened to the right, its left edge
// kept). kStyleBold draws the run a second time kBoldSmear em to the right (at least 1 px; the owner, 13 Nov 2026,
// "Synthetic emphasis"); the advances stay TextWidth's (GDI adds 1 px each), so the layout holds.
constexpr u32 kStyleItalic = 1, kStyleUnderline = 2, kStyleBold = 4;
constexpr float kItalicSlant = 0.34f;
constexpr float kBoldSmear = 0.025f;
// Text `size` px tall (the em), vertically centred on y (the mock's 'm' anchor); `track` = extra letter spacing in ems;
// `outline` = the navy shadow; `style` kStyle* bits. Returns the width.
float Text(float x, float y, const std::wstring& s, float size, u32 argb, Align align = Left, float track = 0,
           bool outline = true, u32 style = 0);
float TextWidth(const std::wstring& s, float size, float track = 0);
// `s` from character `from` on (a name whose rank word gave way), kept per caller's `slot` (0..kTailSlots-1) and composed
// again only when `s` or `from` changes: no string built per frame. A slot outside the range shares one scratch entry.
constexpr int kTailSlots = 24;
const std::wstring& TextFrom(int slot, const std::wstring& s, int from);
// Words that stand beside the game's own captions, in their face and weight (Agency FB Regular; HUD 2's text is its bold) with their shadow, capitals centred on y. `size` 0: the game's caption size. Returns the width.
float GameCaption(float x, float y, const std::wstring& s, u32 argb, Align align = Left, float size = 0);
// THE text-fit rule (core\game\textfit.h; every box FLvibed draws text into): unchanged when it fits maxW, else the size
// steps down - only as far as needed - to the floor (default: TextFloor(), the legibility floor: 0.024 H, never under
// 14 px; every text the kit draws is clamped up to it as well), and only then the text is cut with an ellipsis. Cached
// per string / size / box (valid for the pass). TextFit draws the result and returns its width.
const textfit::Result& FitText(const std::wstring& s, float size, float maxW, float track = 0, float floor = -1);
float TextFit(float x, float y, const std::wstring& s, float size, float maxW, u32 argb, Align align = Left,
              float track = 0, bool outline = true, float floor = -1);
// HUD 2's long names (decision 1; core\game\marquee.h; modules\hud\docs\UI_HUD2.md 5.20): the fit rule as
// TextFit, but a text it would cut is a marquee instead of a permanent "..." - its cut form (the start + the ellipsis)
// held 1.5 s, then scrolling continuously (sub-pixel, by the clock: frame-rate independent) until its end shows in the
// room, held 1.5 s, a fade back to the start, repeated. Drawn clipped to its room (x .. x + maxW for Left; Center /
// Right about x), recorded for the lint as one text with the room as its box. A text that fits is TextFit's, still.
// Returns the drawn width (maxW for a marquee).
// A line drawn at x, cut to [clipX0, clipX1]: a ticker's, a page's game text (`style` kStyle* bits).
void TextClipped(float x, float y, const std::wstring& s, float size, float clipX0, float clipX1, u32 argb, u32 style = 0);
float TextMarquee(float x, float y, const std::wstring& s, float size, float maxW, u32 argb, Align align = Left,
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
// The pad family and `glyph_look` a caller hands in (HUD 2's frame: its copies of glyphs::Current() and the setting, so
// a program with no game draws any family); the forms above read the host's (glyphs::Current, glyphs::LookFor).
struct GlyphStyle
{
    glyphs::Family family = glyphs::Family::Xbox;
    int look = 0; // glyph_look (glyphs::LookOf)
};
void Glyph(const GlyphStyle& style, glyphs::Id id, float cx, float cy, float h, u32 argb, bool hold = false);
float GlyphWidth(const GlyphStyle& style, glyphs::Id id, float h);
// A keyboard key: a rounded keycap with its name ("K", "F1", "SPACE"); returns its width. `tight` = the dense panels'
// keycap (the loadout, the fire strip, the K&M maneuver row, Focus's card click row; the owner, 31 Oct 2026, A1:
// "tightening the glyphs themselves so that everything fits"): the name at the 0.024 H floor (or 0.62 h) and the cap
// 1.2 x the name tall (0.0288 H at the floor) instead of the floor-grown 0.033-0.039 H cap - rows of keycaps can keep
// the lint's 0.01 H between them (panel.overlap).
float Keycap(const std::wstring& name, float cx, float cy, float h, u32 argb, bool measure = false, bool tight = false);

std::string Status();
// Research (debug `hud2 kitdump <passes> <file>`): the next `passes` passes as End submits them - every caller's pass,
// in order - to `file` (per layer: vertex count, texture role, a hash of its vertices) and `file`.bin (the vertices), to
// compare two builds on the same state. 0 stops. The lists hold the drawn text's glyph boxes: a git-ignored folder only.
std::string DumpPasses(const std::string& path, int passes);
// Research (the design bench's recorder): the characters the atlases gained on demand this session, beyond
// glyphset::Base (a program with no game seeds the same before its first pass, or its text draws stand-ins).
const std::wstring& AddedChars();

// ---- the layout lint's recorder (debug `hud2 lint`: hud2_lint.h, hud2_lint_live.cpp). While a list is set, every
// primitive the caller draws appends one record - not the ones a kit call draws inside another (a glyph's letter, a
// keycap's name, a segmented arc's blocks, a plate's outline): its box in pixels with the offset applied, its alpha as
// drawn, the current tag. Not recorded: Tri / Quad / Poly / Grad / Disc called directly (fills, not HUD elements of
// their own). Off (nullptr, the default) = one pointer test per call, nothing allocated.
struct Drawn
{
    char kind = 0; // 'P' plate, 'T' text, 'G' glyph, 'K' keycap, 'A' arc, 'I' icon, 'S' shape (rect, line, bar, outline)
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    float size = 0;  // text / a keycap's name: the em in px; a glyph: its height; an icon: its size
    float alpha = 0; // 0..1 as drawn (the global alpha applied)
    float cx = 0, cy = 0, r = 0, a0 = 0, a1 = 0, w = 0; // an arc: centre, radius at its centre line, degrees, width
    int glyph = -1;       // a glyph: its glyphs::Id
    const char* tag = ""; // RecordTag's
    int unit = 0;         // the designed unit it belongs to (RecUnit; 0 none)
    std::wstring text;    // text / a keycap: the string drawn
};
void Record(std::vector<Drawn>* out);
void RecordTag(const char* tag); // a string literal (kept by pointer); Record resets it to ""
// A designed unit: a prompt and its own label, a slot's icon and count with its key, a chip's icon and word. The items
// drawn inside one RecUnit's scope share a unit id in the records; the lint's panel.overlap lets them touch (gap >= 0)
// but never overlap, where separate elements keep 0.01 H (hud2_lint.h). Scopes nest (the outer unit holds); costs one
// int store when nothing records.
struct RecUnit
{
    RecUnit();
    ~RecUnit();
    RecUnit(const RecUnit&) = delete;
    RecUnit& operator=(const RecUnit&) = delete;
    int prev;
};
} // namespace hud2kit
