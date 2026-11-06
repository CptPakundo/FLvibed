// The new map's words and marks, pure (modules\hud\docs\UI_HUD2_SKIN.md 6.1, step 10): which of the game's map records may be named, where a record stands in the
// viewport, and the labels' declutter: the mark survives, the label drops, the positions stay stable. The gate and the transform are the game's own, read from 0x4999D0 and
// 0x499C20 (docs\FREELANCER_RE.md "The nav map's labels and records"). tests\navmap_labels_test.cpp holds them.
#pragma once
#include "navmap_layout.h"

#include <algorithm>
#include <cmath>

namespace navlabels
{
using u32_t = unsigned int;

// One object of the map's list +0x400.
struct Record
{
    int type = -1;         // +0x00
    u32_t flags = 0;       // +0x04: the object's type bits
    float x = 0, z = 0;    // +0x30, +0x38
    float radius = 0;      // +0x3C
    bool visible = false;  // +0x48
    u32_t nameIds = 0;     // +0x4C
};

// The game's three passes, in its order: planets, the gates, then stations, moons and the rest it names.
constexpr u32_t kPassPlanet = 2, kPassGates = 0xC40, kPassRest = 0x200101;

// The game names a record only through this gate, and this is all the overlay draws: what the game hides stays hidden. An undiscovered object has its byte +0x48 clear.
inline bool Named(const Record& r) { return r.type == 0 && r.nameIds != 0 && r.visible && (r.flags & (kPassPlanet | kPassGates | kPassRest)) != 0; }

// The declutter's rank, 0 first: bases, gates, then the rest (6.1). -1: not named.
inline int Rank(const Record& r)
{
    if (!Named(r))
        return -1;
    constexpr u32_t kStation = 1u << 8;
    if (r.flags & (kPassPlanet | kStation))
        return 0;
    return (r.flags & kPassGates) ? 1 : 2;
}

// The map's layers with no labels of the game's: the legend, the briefing, the base list.
inline bool LabelsInLayer(int layer) { return layer != 4 && layer != 6 && layer != 7; }

// What the transform reads each frame.
struct View
{
    int view = 1;                  // +0x7D0: 1 system, 2 sector
    float scaleX = 0, scaleZ = 0;  // +0x4A8 / +0x4AC, the game's value, not the skin's
    float zoom = 1;                // +0x3F8
    float centreX = 0, centreY = 0; // +0x46C / +0x470
    float halfW = 0, halfH = 0;    // half of +0x418 / +0x41C
    float mapScale = 1;            // +0xF48
};

// A world point's place in the viewport, pixels. False where the view has no such transform, or the point falls outside the viewport.
inline bool ToScreen(const View& v, const navlayout::Rect& port, float x, float z, float& sx, float& sy)
{
    if (!(v.scaleX > 0) || !(v.scaleZ > 0) || (v.view != 1 && v.view != 2))
        return false;
    float nx = x / v.scaleX, ny = z / v.scaleZ;
    if (v.view == 2)
    {
        if (!(v.halfW > 0) || !(v.halfH > 0))
            return false;
        nx = v.zoom * (nx + v.centreX / v.halfW);
        ny = v.zoom * (ny - v.centreY / v.halfH);
    }
    sx = port.x0 + (nx + 1) * 0.5f * port.W();
    sy = port.y0 + (ny + 1) * 0.5f * port.H();
    return sx >= port.x0 && sx <= port.x1 && sy >= port.y0 && sy <= port.y1;
}

// The game's start: at the system's scale a small object loses its label before it collides.
constexpr float kMinScaledRadius = 550.0f;
inline bool LabelByRule(const Record& r, const View& v) { return v.view != 1 || r.radius * v.mapScale >= kMinScaledRadius; }

// One label to place: the mark at (x, y), the text w x h beside it.
struct Label
{
    float x = 0, y = 0, w = 0, h = 0;
    float gap = 0;  // the mark's edge to the text
    int rank = 2;
    bool pinned = false; // the selection, the route's next point: first, whatever its rank
    bool shown = false;
    bool left = false;   // the text stands left of the mark
    navlayout::Rect box;
};

// Places the labels by rank, then by their order in the list, so the same scene always gives the same answer: right of the mark, else left, else it drops. Every mark's
// own circle is kept clear too. Fills order[0..n) with a scratch of ints the caller owns; returns how many stand.
inline int Declutter(Label* l, int n, int* order, const navlayout::Rect& bounds)
{
    for (int i = 0; i < n; ++i)
        order[i] = i, l[i].shown = false;
    std::stable_sort(order, order + n, [&](int a, int b)
                     {
                         const int ra = l[a].pinned ? -1 : l[a].rank, rb = l[b].pinned ? -1 : l[b].rank;
                         return ra < rb;
                     });
    int shown = 0;
    for (int k = 0; k < n; ++k)
    {
        Label& a = l[order[k]];
        for (int side = 0; side < 2 && !a.shown; ++side)
        {
            navlayout::Rect b = side == 0 ? navlayout::Rect{a.x + a.gap, a.y - a.h / 2, a.x + a.gap + a.w, a.y + a.h / 2}
                                          : navlayout::Rect{a.x - a.gap - a.w, a.y - a.h / 2, a.x - a.gap, a.y + a.h / 2};
            if (b.x0 < bounds.x0 || b.x1 > bounds.x1 || b.y0 < bounds.y0 || b.y1 > bounds.y1)
                continue;
            bool clear = true;
            for (int j = 0; j < n && clear; ++j)
            {
                const Label& o = l[j];
                if (&o == &a)
                    continue;
                if (o.shown && b.Overlaps(o.box))
                    clear = false;
                const navlayout::Rect mark = {o.x - a.gap, o.y - a.gap, o.x + a.gap, o.y + a.gap};
                if (b.Overlaps(mark))
                    clear = false;
            }
            if (clear)
                a.shown = true, a.left = side == 1, a.box = b, ++shown;
        }
    }
    return shown;
}
} // namespace navlabels
