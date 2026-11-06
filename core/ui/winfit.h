// FLvibed - a window's panels fitted to what they hold. Pure: the numbers come from the game's own controls as they stand (winplates.cpp measures them), so a text that wraps, a
// list that is short, a slider the game hides or a button that appears moves the panel's foot by itself, with no number per state. Rules (the owner, 6 Nov 2026): no empty room
// but a small even margin, nothing outside its panel, panels side by side end level, a panel under them takes the room they leave.
#pragma once

#include <algorithm>
#include <vector>

namespace winfit
{
struct Rect
{
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
};

enum class Kind
{
    Keep,  // stands as it was asked: its content is not ours to read (a 3D view, a page drawn by others, a collapsed panel)
    Hug,   // ends under what it holds
    Level, // Keep, but the panels beside it end level with it
    Under, // its top follows the panels over it, its foot stays
    Match, // not all it holds can be read (the ship's 3D view): it ends with the row, and the row holds what it can read
};

struct Panel
{
    Rect plate; // as the table asks, then as fitted
    Kind kind = Kind::Keep;
    bool has = false; // holds something
    float top = 0, foot = 0, left = 0, right = 0; // what it holds
    float least = 0; // it ends no higher than this: a heading's rule, with the title over it, is part of the panel whatever else is read
};

struct Rules
{
    float air = 0;  // under the content
    float gap = 0;  // between a panel and the one under it
    float snap = 0; // panels side by side that end within this end level; further apart, each ends at its own content
    float reach = 0; // a control this far under a panel is still that panel's: it stood out of it
    float least = 0; // what an Under panel keeps of its height
};

// The panel a control's centre stands in, else the nearest one above it in its column within `reach`; -1 none
inline int Owner(const std::vector<Panel>& ps, float cx, float cy, float reach)
{
    for (size_t i = 0; i < ps.size(); ++i)
        if (cx >= ps[i].plate.x0 && cx <= ps[i].plate.x1 && cy >= ps[i].plate.y0 && cy <= ps[i].plate.y1)
            return static_cast<int>(i);
    int best = -1;
    float least = reach;
    for (size_t i = 0; i < ps.size(); ++i)
    {
        const float d = cy - ps[i].plate.y1;
        if (cx >= ps[i].plate.x0 && cx <= ps[i].plate.x1 && d > 0 && d <= least)
            best = static_cast<int>(i), least = d;
    }
    return best;
}

inline void Add(std::vector<Panel>& ps, const Rect& item, float reach)
{
    const int i = Owner(ps, (item.x0 + item.x1) / 2, (item.y0 + item.y1) / 2, reach);
    if (i < 0)
        return;
    Panel& p = ps[static_cast<size_t>(i)];
    p.top = p.has ? std::min(p.top, item.y0) : item.y0;
    p.foot = p.has ? std::max(p.foot, item.y1) : item.y1;
    p.left = p.has ? std::min(p.left, item.x0) : item.x0;
    p.right = p.has ? std::max(p.right, item.x1) : item.x1;
    p.has = true;
}

inline void Solve(std::vector<Panel>& ps, const Rules& r)
{
    // each panel on its content; a Hug with nothing in it keeps what the table asks
    float tall = -1e9f;
    for (Panel& p : ps)
    {
        if (p.kind == Kind::Hug && p.has)
            p.plate.y1 = std::max(std::max(p.foot + r.air, p.plate.y0 + r.air * 2), p.least);
        if (p.kind == Kind::Hug || p.kind == Kind::Level)
            tall = std::max(tall, p.plate.y1);
        if (p.kind == Kind::Match && p.has) // (what it can read counts: SELECT SHIP hung under the panel)
            tall = std::max(tall, p.foot + r.air);
    }
    for (Panel& p : ps)
        if ((p.kind == Kind::Hug || p.kind == Kind::Level) && tall - p.plate.y1 <= r.snap)
            p.plate.y1 = tall;
    for (Panel& p : ps)
        if (p.kind == Kind::Match && tall > -1e8f)
            p.plate.y1 = tall;
    for (Panel& p : ps)
    {
        if (p.kind != Kind::Under)
            continue;
        float over = -1e9f;
        for (const Panel& q : ps)
            if (q.kind != Kind::Under && q.kind != Kind::Keep && q.plate.x1 > p.plate.x0 && q.plate.x0 < p.plate.x1 && q.plate.y0 < p.plate.y0)
                over = std::max(over, q.plate.y1);
        // (the row ending just short of it leaves it be: a few pixels down took the stats' last line, and put a scroll bar over the values)
        if (over > -1e8f && !(over < p.plate.y0 && over + r.gap > p.plate.y0))
            p.plate.y0 = std::min(over + r.gap, p.plate.y1 - r.least);
    }
}

// One step of a panel's edge towards where it should stand: it travels, never cuts
inline float Toward(float now, float want, float step)
{
    return now < want ? std::min(now + step, want) : std::max(now - step, want);
}
} // namespace winfit
