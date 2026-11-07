// The nav map's zoom and pan, pure (docs\MAP_PLAN.md "P2's design"): the game's own transform in its own units, what one frame of input does to it, the touchpad's gestures. The
// transform is read from 0x496600 and 0x492410 (docs\FREELANCER_RE.md "The nav map's zoom and pan"). tests\navview_test.cpp holds it. No game, no system header.
#pragma once
#include <algorithm>
#include <cmath>
#include <string>

namespace navview
{
// A lane's name as the game builds it, "Fort Bush->Norfolk Shipyard": its two ends. False: no such name.
inline bool SplitLane(const std::wstring& lane, std::wstring& from, std::wstring& to)
{
    const size_t at = lane.find(L"->");
    if (at == std::wstring::npos || at == 0 || at + 2 >= lane.size())
        return false;
    from = lane.substr(0, at), to = lane.substr(at + 2);
    return true;
}

// A lane's end names this place: the same words, or the place's name goes on after them ("West Point" is the lane's word for West Point Military Academy).
inline bool EndNames(const std::wstring& end, const std::wstring& place)
{
    return !end.empty() && place.compare(0, end.size(), end) == 0 && (place.size() == end.size() || place[end.size()] == L' ');
}

constexpr float kMinZoom = 1.0f;       // the whole system (view 1)
constexpr float kMaxZoom = 8.6f;       // the game's sector level: 0x49F1A0's +0x4A8 x 0.11627907
constexpr float kFullZoomSeconds = 1.2f; // end to end at a full pull
constexpr float kZoomEase = 0.06f;     // seconds: the zoom follows its target
constexpr float kPanRate = 1.5f;       // viewports a second at full tilt, whatever the zoom
constexpr float kWheelNotch = 0.22f;   // a mouse wheel's notch, natural log of the zoom's factor
constexpr float kGlideEase = 0.12f;    // seconds: a released swipe's speed dies away (about 0.25 s to the eye)
constexpr float kBackToSystem = 1.002f; // a zoom this close to 1 is view 1

// The map's fields: Z = +0x3F8, C = +0x46C / +0x470, in the box's units (+0x418 / +0x41C), positive C moves the map right and up.
struct View
{
    float zoom = 1, cx = 0, cy = 0;
};
struct Box
{
    float w = 0.4684f, h = 0.4684f;
};

// How far C may go, per axis: the map's edge stays on the viewport's edge. (The game's own drag allows half the box at every zoom: past the map's edge.)
inline float MaxCentre(float size, float zoom) { return size * 0.5f * (1 - 1 / std::max(zoom, kMinZoom)); }

inline void Clamp(View& v, const Box& b)
{
    v.zoom = std::min(kMaxZoom, std::max(kMinZoom, v.zoom));
    const float mx = MaxCentre(b.w, v.zoom), my = MaxCentre(b.h, v.zoom);
    v.cx = std::min(mx, std::max(-mx, v.cx));
    v.cy = std::min(my, std::max(-my, v.cy));
}

// A place on the map (0..1 across, 0..1 down) at a place in the viewport (0..1, down from the top), from 0x496600's x = C Z - W Z / 2 + n W Z around the box's centre.
inline float MapU(const View& v, const Box& b, float u) { return (u - 0.5f - v.cx * v.zoom / b.w + v.zoom * 0.5f) / v.zoom; }
inline float MapV(const View& v, const Box& b, float t) { return 1 - ((1 - t) - 0.5f - v.cy * v.zoom / b.h + v.zoom * 0.5f) / v.zoom; }
inline float PortU(const View& v, const Box& b, float mapU) { return 0.5f + v.cx * v.zoom / b.w - v.zoom * 0.5f + mapU * v.zoom; }
inline float PortV(const View& v, const Box& b, float mapV) { return 1 - (0.5f + v.cy * v.zoom / b.h - v.zoom * 0.5f + (1 - mapV) * v.zoom); }

// The zoom changes, and the map's point at (u, t) stays at (u, t).
inline void ZoomAbout(View& v, const Box& b, float u, float t, float zoom)
{
    const float mu = MapU(v, b, u), mv = MapV(v, b, t);
    v.zoom = std::min(kMaxZoom, std::max(kMinZoom, zoom));
    v.cx = b.w / v.zoom * (u - 0.5f + v.zoom * 0.5f - mu * v.zoom);
    v.cy = b.h / v.zoom * ((1 - t) - 0.5f + v.zoom * 0.5f - (1 - mv) * v.zoom);
    Clamp(v, b);
}

// The map moves by this much of the viewport, right and down.
inline void PanBy(View& v, const Box& b, float du, float dt)
{
    v.cx += du * b.w / v.zoom;
    v.cy -= dt * b.h / v.zoom;
    Clamp(v, b);
}

// One frame's input. Sticks and triggers are rates; the touchpad's are this frame's distances.
struct Drive
{
    float zoomRate = 0;        // -1 (out) .. 1 (in)
    float zoomStep = 0;        // natural log of the zoom's factor, this frame
    float panX = 0, panY = 0;  // the stick, +y up: the view moves that way, the map the other
    float dragU = 0, dragT = 0; // the map follows the finger: viewports, right and down
    bool touching = false;     // a finger holds the map: no glide yet
};

struct Motion
{
    float target = 0;        // log of the zoom it heads for
    float glideU = 0, glideT = 0; // viewports a second
};

// A full pull's log zoom a second.
inline float ZoomPerSecond() { return std::log(kMaxZoom / kMinZoom) / kFullZoomSeconds; }

// Quadratic: precise near the centre, as the cursor's.
inline float StickCurve(float a) { return a * std::fabs(a); }

// One frame: (u, t) the place the zoom holds still. True when the view moved.
inline bool Step(Motion& m, View& v, const Box& b, const Drive& d, float u, float t, float dt)
{
    const View before = v;
    const float top = std::log(kMaxZoom);
    m.target = std::min(top, std::max(0.0f, m.target + d.zoomRate * ZoomPerSecond() * dt + d.zoomStep));
    const float now = std::log(std::max(v.zoom, kMinZoom));
    float next = now + (m.target - now) * (1 - std::exp(-dt / kZoomEase));
    if (std::fabs(m.target - next) < 0.001f)
        next = m.target;
    if (next != now)
        ZoomAbout(v, b, u, t, std::exp(next));
    float du = -StickCurve(d.panX) * kPanRate * dt + d.dragU, dv = StickCurve(d.panY) * kPanRate * dt + d.dragT;
    if (d.touching)
    {
        if (dt > 0)
        {
            // the last moments' speed: what a release carries on
            const float k = 1 - std::exp(-dt / 0.05f);
            m.glideU += (d.dragU / dt - m.glideU) * k, m.glideT += (d.dragT / dt - m.glideT) * k;
        }
    }
    else
    {
        du += m.glideU * dt, dv += m.glideT * dt;
        const float k = std::exp(-dt / kGlideEase);
        m.glideU *= k, m.glideT *= k;
        if (std::fabs(m.glideU) + std::fabs(m.glideT) < 0.01f)
            m.glideU = m.glideT = 0;
    }
    if (du != 0 || dv != 0)
        PanBy(v, b, du, dv);
    Clamp(v, b);
    return v.zoom != before.zoom || v.cx != before.cx || v.cy != before.cy;
}

// ---- the touchpad's gestures, on SDL's fingers (0..1 across, 0..1 down)
constexpr int kFingers = 2;
struct Finger
{
    bool down = false;
    float x = 0, y = 0;
};
constexpr float kTouchDead = 0.02f;  // of the pad: a finger that has moved less is a touch, not a drag
constexpr float kTouchZoom = 3.0f;   // log zoom for the pad's whole height, two fingers

struct Gesture
{
    Finger last[kFingers];
    bool moving = false;  // past the dead zone
    bool blocked = false; // the pad was clicked: nothing until every finger is off
    float startX = 0, startY = 0;
    int was = 0;          // fingers down last frame
};
struct GestureOut
{
    float dragU = 0, dragT = 0, zoomStep = 0;
    bool touching = false;
};

// One read of the pad. `clicked`: the touchpad's button is down.
inline GestureOut Read(Gesture& g, const Finger* f, bool clicked)
{
    GestureOut o;
    int n = 0;
    for (int i = 0; i < kFingers; ++i)
        n += f[i].down ? 1 : 0;
    if (clicked)
        g.blocked = true;
    if (n == 0)
        g.blocked = false;
    if (n != g.was)
    {
        // a finger came or went: a new gesture from here, nothing jumps
        g.moving = false;
        for (int i = 0; i < kFingers; ++i)
            if (f[i].down)
            {
                g.startX = f[i].x, g.startY = f[i].y;
                break;
            }
    }
    else if (n > 0 && !g.blocked)
    {
        float dx = 0, dy = 0, sx = 0, sy = 0;
        for (int i = 0; i < kFingers; ++i)
            if (f[i].down && g.last[i].down)
                dx += f[i].x - g.last[i].x, dy += f[i].y - g.last[i].y, sx += f[i].x, sy += f[i].y;
        dx /= n, dy /= n, sx /= n, sy /= n;
        if (!g.moving && n == 1)
            g.moving = std::fabs(sx - g.startX) + std::fabs(sy - g.startY) > kTouchDead;
        else if (n == 2)
            g.moving = true;
        if (g.moving && n == 1)
            o.dragU = dx, o.dragT = dy;
        else if (g.moving)
            o.zoomStep = -dy * kTouchZoom; // (up the pad = in)
    }
    o.touching = n == 1 && g.moving && !g.blocked;
    g.was = n;
    for (int i = 0; i < kFingers; ++i)
        g.last[i] = f[i];
    return o;
}

// ---- the two backgrounds: the system's (its 8 x 8 grid, +0x354) holds up to a few times its size, the sector's (the fine grid, +0x37C) is made for 8.6 and is a blur of lines far below it.
// Below `kFadeFrom` the system's alone, above `kFadeTo` the sector's alone. Frames: docs\MAP_PLAN.md.
constexpr float kFadeFrom = 2.5f, kFadeTo = 6.0f;
inline float SectorAlpha(float zoom)
{
    const float t = std::min(1.0f, std::max(0.0f, (zoom - kFadeFrom) / (kFadeTo - kFadeFrom)));
    return t * t * (3 - 2 * t);
}

// ---- the grid's letters and numbers: 0x49FBF0's formula with the live zoom (it holds 8.6 in view 2, and offsets of 0 in view 1 and 0.027 / -0.0115 in view 2)
inline float GridOffset(float full, float zoom) { return full * (zoom - kMinZoom) / (kMaxZoom - kMinZoom); }
inline float GridLetterX(int i, const View& v) { return (v.cx + GridOffset(0.027f, v.zoom) + i * 0.07f - 0.279f) * v.zoom; }
inline float GridNumberY(int i, const View& v) { return (v.cy + GridOffset(-0.0115f, v.zoom) + 0.252f - i * 0.07f) * v.zoom; }
inline bool GridLetterShown(float x) { return x <= 0.23f && x >= -0.31f; }
inline bool GridNumberShown(float y) { return y <= 0.274f && y >= -0.257f; }
constexpr float kGridLetterY = -0.294f, kGridNumberX = -0.309f;
} // namespace navview
