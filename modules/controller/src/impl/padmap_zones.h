// The pad on the skin's nav map, pure: its two zones (modules\hud\docs\UI_HUD2_SKIN.md 6.1 "The pad's inputs close by two zones"). Canvas: the left stick, the triggers, A / X / Y,
// no chip focused. Panel: the D-pad enters and walks every chip, A presses, the left stick returns: the owner's Destiny rule. So one press never means two things. With
// no chips (the HUD off, the old map) there is one zone, as there always was. tests\padmap_zones_test.cpp holds it.
#pragma once
#include <cmath>

namespace padzones
{
enum class Zone
{
    Canvas,
    Panel
};
enum Dir
{
    None = -1,
    Up,
    Down,
    Left,
    Right
};
struct Cell
{
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    int id = -1;
    bool enabled = true, stepper = false;
    float Cx() const { return (x0 + x1) / 2; }
    float Cy() const { return (y0 + y1) / 2; }
};

inline int Find(const Cell* c, int n, int id)
{
    for (int i = 0; i < n; ++i)
        if (c[i].id == id)
            return i;
    return -1;
}

// The nearest chip that way from `from` (an index), -1 none. Straight lines first: distance across counts double. A disabled chip takes the focus too: it says why it
// does nothing by its look, and a row with a gap must not trap the walk.
inline int Walk(const Cell* c, int n, int from, Dir d)
{
    int best = -1;
    float bestScore = 1e30f;
    const float x = c[from].Cx(), y = c[from].Cy();
    for (int i = 0; i < n; ++i)
    {
        if (i == from)
            continue;
        const float dx = c[i].Cx() - x, dy = c[i].Cy() - y;
        const float along = d == Left ? -dx : d == Right ? dx : d == Up ? -dy : dy;
        const float across = d == Left || d == Right ? std::fabs(dy) : std::fabs(dx);
        if (along <= 1.0f)
            continue;
        const float score = along + 2 * across;
        if (score < bestScore)
            bestScore = score, best = i;
    }
    return best;
}

// The chip the D-pad lands on from the canvas: left the left side's top, the rest the right side's top.
inline int Enter(const Cell* c, int n, Dir d)
{
    float lo = 1e30f, hi = -1e30f;
    for (int i = 0; i < n; ++i)
        lo = std::fmin(lo, c[i].Cx()), hi = std::fmax(hi, c[i].Cx());
    const float mid = (lo + hi) / 2;
    int best = -1;
    for (int i = 0; i < n; ++i)
    {
        const bool side = d == Left ? c[i].Cx() <= mid : c[i].Cx() >= mid;
        if (side && (best < 0 || c[i].y0 < c[best].y0 - 0.5f || (std::fabs(c[i].y0 - c[best].y0) <= 0.5f && c[i].x0 < c[best].x0)))
            best = i;
    }
    return best;
}

struct In
{
    Dir dpad = None;   // the D-pad's press this frame
    bool stick = false; // the left stick moved
    bool a = false, b = false;
};
struct Out
{
    int press = -1;      // the chip to press
    int stepId = -1;     // the chip to step ...
    int stepDir = 0;     // ... this way
    bool ownsA = false;  // the zone took the press: the canvas must not see it
    bool ownsB = false;
};

struct Zones
{
    Zone zone = Zone::Canvas;
    int focus = -1; // the chip's id; -1 on the canvas
    float lastX = 0, lastY = 0;

    void Reset() { zone = Zone::Canvas, focus = -1; }

    Out Step(const In& in, const Cell* c, int n)
    {
        Out o;
        if (n <= 0)
        {
            Reset();
            return o;
        }
        if (zone == Zone::Canvas)
        {
            if (in.dpad != None)
            {
                const int e = Enter(c, n, in.dpad);
                if (e >= 0)
                    zone = Zone::Panel, focus = c[e].id, Note(c[e]);
            }
            return o;
        }
        // the focused chip went (a selection's chips leave with the selection): the nearest one takes over
        int at = Find(c, n, focus);
        if (at < 0)
        {
            at = Nearest(c, n);
            focus = c[at].id;
        }
        Note(c[at]);
        o.ownsA = true, o.ownsB = true;
        if (in.stick || in.b)
        {
            Reset();
            o.ownsA = false; // (the same frame's A is the canvas's: the stick has left the panel)
            return o;
        }
        if (in.a && c[at].enabled)
            o.press = c[at].id;
        if (in.dpad != None)
        {
            if (c[at].stepper && (in.dpad == Left || in.dpad == Right))
                o.stepId = c[at].id, o.stepDir = in.dpad == Left ? -1 : 1;
            else
            {
                const int w = Walk(c, n, at, in.dpad);
                if (w >= 0)
                    focus = c[w].id, Note(c[w]);
            }
        }
        return o;
    }

private:
    void Note(const Cell& c) { lastX = c.Cx(), lastY = c.Cy(); }
    int Nearest(const Cell* c, int n) const
    {
        int best = 0;
        float d = 1e30f;
        for (int i = 0; i < n; ++i)
        {
            const float s = std::fabs(c[i].Cx() - lastX) + std::fabs(c[i].Cy() - lastY);
            if (s < d)
                d = s, best = i;
        }
        return best;
    }
};
} // namespace padzones
