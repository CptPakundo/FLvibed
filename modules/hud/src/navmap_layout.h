// HUD 2's NeuroNet screens, pure: where the strip of words stands, and how the nav map and its panels share the screen (modules\hud\docs\
// UI_HUD2_SKIN.md 6.1, "Changed 6 Nov 2026"). Fractions of H unless named; pixels in the rectangles. Text widths come in through a functor,
// so tests\navmap_layout_test.cpp walks the display matrix with no game and no font.
#pragma once
#include <algorithm>

namespace navlayout
{
struct Rect
{
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    float W() const { return x1 - x0; }
    float H() const { return y1 - y0; }
    bool Overlaps(const Rect& o) const { return x0 < o.x1 && o.x0 < x1 && y0 < o.y1 && o.y0 < y1; }
    bool Holds(float x, float y) const { return x >= x0 && x < x1 && y >= y0 && y < y1; }
};

// ---- the strip: one row of words in the bottom lane's left half, at the side margin
constexpr float kWordH = 0.030f;   // a chip's word (UI_HUD2_SKIN.md 4)
constexpr float kWordGap = 0.030f; // between words, at its widest
constexpr float kWordGapMin = 0.016f;
constexpr float kStripPad = 0.014f;
constexpr int kMaxTabs = 7;

struct StripIn
{
    float W = 0, H = 0;
    float side = 0.05f;             // the margin, fractions of W (deckmode::SideW)
    float laneTop = 0.91f, laneBottom = 0.95f; // fractions of H (deckmode::LaneTopH / LaneBottomH)
    float floorH = 0.024f;          // the text floor, fractions of H
    int tabs = 5;
    bool bumpers = false;           // the pad: LB / RB at its ends
    float bumperW = 0;              // one bumper glyph's width, pixels
};
struct StripOut
{
    Rect box;               // the plate
    Rect word[kMaxTabs];    // each word's hit area, the plate's height
    float size = 0;         // the words' em, pixels
    float lbX = 0, rbX = 0; // the bumpers' centres
    Rect close;             // K&M's CLOSE chip, at the lane's right end
    bool fits = false;      // it stays in the lane's left half
};

// `measure(i, size)`: tab i's word at this em, pixels. `closeW`: the CLOSE word's width at the strip's size.
template <class Measure> StripOut Strip(const StripIn& in, Measure measure, float closeW)
{
    StripOut o;
    const float H = in.H, W = in.W;
    const float x0 = in.side * W, half = W * 0.5f;
    const float ends = in.bumpers ? 2 * (in.bumperW + kStripPad * H) : 2 * kStripPad * H;
    auto total = [&](float size, float gap)
    {
        float w = ends + gap * H * (in.tabs - 1);
        for (int i = 0; i < in.tabs; ++i)
            w += measure(i, size);
        return w;
    };
    // the room first: the gap gives way, then the size, down to the floor; a word is never shortened
    float size = kWordH * H, gap = kWordGap;
    const float room = half - x0;
    if (total(size, gap) > room)
        gap = std::max(kWordGapMin, gap - (total(size, gap) - room) / (H * std::max(1, in.tabs - 1)));
    const float floorPx = in.floorH * H;
    while (total(size, gap) > room && size > floorPx)
        size = std::max(floorPx, size - 0.001f * H);
    o.size = size;
    const float w = total(size, gap);
    o.fits = w <= room;
    o.box = {x0, in.laneTop * H, x0 + w, in.laneBottom * H};
    float x = x0 + (in.bumpers ? in.bumperW + kStripPad * H : kStripPad * H);
    o.lbX = x0 + kStripPad * H * 0.5f + in.bumperW * 0.5f;
    o.rbX = o.box.x1 - kStripPad * H * 0.5f - in.bumperW * 0.5f;
    for (int i = 0; i < in.tabs; ++i)
    {
        const float ww = measure(i, size);
        const float g = gap * H * 0.5f;
        o.word[i] = {std::max(x - g, o.box.x0), o.box.y0, std::min(x + ww + g, o.box.x1), o.box.y1};
        x += ww + gap * H;
    }
    const float pad = kStripPad * H, right = W * (1 - in.side);
    o.close = {right - closeW - 2 * pad, o.box.y0, right, o.box.y1};
    return o;
}

// ---- the map and its panels
constexpr float kMapH = 0.785f, kMapTop = 0.115f, kMapBottom = 0.900f;
constexpr float kPanelMin = 0.38f; // a panel gives way to this before a margin does
constexpr float kRowNumbers = 0.02f; // the native map's row numbers, left of its viewport (a 1280x800 frame: they touched the VIEW panel)
constexpr float kPanelGap = 0.018f;
constexpr float kGutterW = 0.0125f; // the narrowest margin, fractions of W
constexpr float kColumnMapH = 0.70f;
constexpr float kColumnMin = 0.10f; // the narrowest column of tabs that keeps its words

struct MapOut
{
    Rect map, left, right;
    float size = 0;        // the map's side, fractions of H
    float panel = 0;       // a panel's width, fractions of H
    float side = 0;        // the margin in force, fractions of W
    bool column = false;   // 4:3: one column of tabs in place of the left panel
};

// One solver for every aspect: the map is min(0.785 H, W - panels - margins). The room goes first to the map, then the panels give way to
// 0.38 H, then the margins to the gutter, and only then the map shrinks; where two panels would leave it under 0.70 H the left one becomes a column.
inline MapOut Solve(float W, float H, float side)
{
    MapOut o;
    const float aspect = W / H;
    o.side = side;
    float room = aspect - 2 * side * aspect - 2 * kPanelGap - kMapH; // for the two panels
    float panel = room / 2;
    if (panel < kPanelMin)
    {
        panel = kPanelMin;
        const float wanted = (aspect - 2 * kPanelGap - kMapH - 2 * panel) / (2 * aspect);
        o.side = std::max(std::min(side, kGutterW), std::min(side, wanted));
    }
    float size = std::min(kMapH, aspect - 2 * o.side * aspect - 2 * kPanelGap - 2 * panel);
    float leftW = panel;
    if (size < kColumnMapH)
    {
        o.column = true;
        size = kColumnMapH;
        o.side = std::min(side, kGutterW);
        leftW = aspect - 2 * o.side * aspect - 2 * kPanelGap - size - panel;
        if (leftW < kColumnMin)
        {
            size = std::max(0.2f, size - (kColumnMin - leftW));
            leftW = kColumnMin;
        }
    }
    o.size = size, o.panel = panel;
    const float y1 = kMapBottom * H, y0 = y1 - size * H;
    // centred between the panels: the left margin + the left panel + a gap
    const float mx0 = o.side * W + leftW * H + kPanelGap * H;
    o.map = {mx0, y0, mx0 + size * H, y1};
    o.left = {o.side * W, y0, o.side * W + leftW * H, y1};
    o.right = {o.map.x1 + kPanelGap * H, y0, o.map.x1 + kPanelGap * H + panel * H, y1};
    return o;
}

// The panels beside a map of the game's own size: the frame and the words around the native map (6.1, the first step). `map` is its viewport.
inline MapOut Around(float W, float H, float side, const Rect& map)
{
    MapOut o = Solve(W, H, side);
    o.map = map;
    o.size = map.H() / H;
    const float gap = kPanelGap * H, rows = kRowNumbers * H; // (the game's numbers 1-8 stand just left of the viewport)
    const float leftRoom = map.x0 - gap - rows - o.side * W, rightRoom = W * (1 - o.side) - map.x1 - gap;
    const float panel = std::max(0.0f, std::min(o.panel * H, std::min(leftRoom, rightRoom)));
    o.panel = panel / H;
    o.column = false;
    o.left = {map.x0 - gap - rows - panel, map.y0, map.x0 - gap - rows, map.y1};
    o.right = {map.x1 + gap, map.y0, map.x1 + gap + panel, map.y1};
    return o;
}

// ---- a panel's chips, top to bottom
constexpr float kChipH = 0.048f, kChipGap = 0.010f, kPanelPad = 0.018f, kSectionH = 0.040f;

// `n` chips in `cols` columns from `y`, into out[0..n). Returns the y under them.
inline float Grid(const Rect& panel, float H, float y, int n, int cols, Rect* out)
{
    const float pad = kPanelPad * H, gap = kChipGap * H, h = kChipH * H;
    const float w = (panel.W() - 2 * pad - gap * (cols - 1)) / cols;
    for (int i = 0; i < n; ++i)
    {
        const int r = i / cols, c = i % cols;
        const float x = panel.x0 + pad + c * (w + gap), yy = y + r * (h + gap);
        out[i] = {x, yy, x + w, yy + h};
    }
    const int rows = (n + cols - 1) / cols;
    return rows ? y + rows * (h + gap) - gap : y;
}
// Keeps a panel of this height on the screen, its bottom at or above `limit`, its top where it can stand: it moves, it never clips.
inline float PanelTop(float top, float height, float limit) { return std::max(0.0f, std::min(top, limit - height)); }
} // namespace navlayout
