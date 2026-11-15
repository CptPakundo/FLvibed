// The model slot's game-free half (modelslot.h): the table of slots and its state machine, the rectangle and clip maths, the
// distance and the placement that fit a model in its rectangle, and the compare that tells the world changed under the slots
// (a save load, a base or system change, a device reset, a gap in the frames). Pure: tests\modelslot_test.cpp.
#pragma once
#include <cmath>
#include <cstring>

namespace modelslot
{
// A rectangle in back-buffer pixels.
struct Rect
{
    int x = 0, y = 0, w = 0, h = 0;
};

// `r` lies wholly inside `outer` (and is not empty).
inline bool Inside(const Rect& r, const Rect& outer)
{
    return r.w > 0 && r.h > 0 && r.x >= outer.x && r.y >= outer.y && r.x + r.w <= outer.x + outer.w && r.y + r.h <= outer.y + outer.h;
}

// A model is drawn only when its rectangle lies wholly inside the viewport (the engine keeps its last viewport for one outside
// it: audit 48_ 1) and inside the page's clip (`clip.w` 0 = no clip of the page's own). A partly visible rectangle is not drawn:
// the camera's rectangle is the projection's centre, so cutting it would move the model.
inline bool Drawable(const Rect& r, const Rect& viewport, const Rect& clip) { return Inside(r, viewport) && (clip.w <= 0 || Inside(r, clip)); }

// The camera's rectangle as CamSetRect 0x40FB80 takes it: left, top, right, bottom, inclusive.
inline void Corners(const Rect& r, int out[4])
{
    out[0] = r.x, out[1] = r.y, out[2] = r.x + r.w - 1, out[3] = r.y + r.h - 1;
}

constexpr float kPi = 3.14159265f;

// How far in front of the camera a model stands so that its radius fills `fill` of the rectangle's smaller half side.
// CamSetFov 0x40FA10's degrees are the VERTICAL half angle (its tan is the slope of the top and bottom planes; the horizontal
// half angle is atan(tan x aspect), the aspect width / height from CamSetRect), so the half height spans d x tan(fov) at
// distance d, the half width d x tan(fov) x aspect. The radius is the bounds' first value (Engine vt[0x88] A[0]), which
// undershoots a ship's reach by about 1.4 x (48_ 4): a page sizes with `fill`.
inline float Distance(float radius, float fill, float fovDeg, float aspect)
{
    const float r = radius > 0 ? radius : 1.0f;
    const float f = fill < 0.05f ? 0.05f : fill > 2.0f ? 2.0f : fill;
    const float fov = fovDeg < 1.0f ? 1.0f : fovDeg > 89.0f ? 89.0f : fovDeg;
    const float a = aspect > 0 && aspect < 1.0f ? aspect : 1.0f;
    return r / (f * tanf(fov * kPi / 180.0f) * a);
}

// The model's placement for Engine vt[0x9C]: a 3x3 (rows) and a position. The model turns by `yaw` about the vertical axis,
// then by `pitch` about the camera's horizontal one (M = Rx(pitch) Ry(yaw)); its bounds' centre turned with it is moved onto the
// camera's axis, `dist` in front (the camera stays at the origin looking down -Z: the ship view's way, 48_). Pitch 0 gives
// the experiment's matrix exactly (rows c 0 s / 0 1 0 / -s 0 c).
inline void Place(float yaw, float pitch, const float centre[3], float dist, float xf[12])
{
    const float c = cosf(yaw), s = sinf(yaw), cp = cosf(pitch), sp = sinf(pitch);
    const float m[9] = {c, 0, s, sp * s, cp, -sp * c, -cp * s, sp, cp * c};
    for (int i = 0; i < 9; ++i)
        xf[i] = m[i];
    for (int row = 0; row < 3; ++row)
        xf[9 + row] = -(m[row * 3] * centre[0] + m[row * 3 + 1] * centre[1] + m[row * 3 + 2] * centre[2]);
    xf[11] -= dist;
}

// A key for a slot: the asking page's name and the row's index (FNV-1a), never 0. Keys are the pages' own: two pages that
// can stand at once use two names.
inline unsigned KeyOf(const char* owner, int index)
{
    unsigned h = 2166136261u;
    for (const char* p = owner; p && *p; ++p)
        h = (h ^ static_cast<unsigned char>(*p)) * 16777619u;
    for (int i = 0; i < 4; ++i)
        h = (h ^ static_cast<unsigned>((index >> (8 * i)) & 0xFF)) * 16777619u;
    return h ? h : 1u;
}

constexpr int kSlots = 12;       // the live models at once (25_research_3d_models.md 5: 12 cost about 0.04 ms of CPU a frame, 48_ 7)
constexpr int kFile = 160;       // a model's path under DATA
constexpr double kGraceS = 0.5;  // a key not asked for this long gives its holder and its place back
constexpr double kGapS = 0.25;   // passes this far apart: the frames stopped (a load, a device reset, the window away)

// What a page asks for (modelslot.h Model, copied).
struct Ask
{
    char file[kFile] = {};
    Rect rect, clip;
    float yaw = 0, pitch = 0, spin = 0, fill = 0.6f, fov = 25.0f;
    unsigned flags = 0x300;
    int ambient = 0;
};

enum class State : unsigned char
{
    Empty,  // the place is free
    Wanted, // asked, its holder not loaded (yet, or again after the world changed)
    Ready,  // loaded: drawn while asked
    Failed, // the load failed or a draw faulted: not tried again until the file changes or the world does
};

struct Slot
{
    unsigned key = 0;
    State state = State::Empty;
    bool held = false; // the game's half holds a loaded holder for it (freed before the place is given back)
    Ask ask;
    unsigned askedAt = 0; // the pass count at the last ask
    double lastAsk = 0;   // its time (perf::Now)
    float turned = 0;     // what `spin` has turned it so far
};

// What the pass does with a slot.
enum class Act
{
    None,    // nothing: not asked this frame (kept a while), or failed
    Load,    // asked and not loaded: load (freeing an old holder first), then draw
    Draw,    // asked and loaded
    Release, // not asked for kGraceS (or dropped): its holder freed, its place given back
};

struct Table
{
    Slot slots[kSlots];
    unsigned pass = 1; // passes so far (+1): an ask before the first pass counts for it
    unsigned loads = 0, frees = 0, failures = 0, refused = 0;

    int Find(unsigned key) const
    {
        for (int i = 0; i < kSlots; ++i)
            if (key && slots[i].key == key)
                return i;
        return -1;
    }
    int Held() const
    {
        int n = 0;
        for (const Slot& s : slots)
            n += s.held;
        return n;
    }
    int Used() const
    {
        int n = 0;
        for (const Slot& s : slots)
            n += s.key != 0;
        return n;
    }

    // A page's ask for this frame. A new file (or a first ask) makes the slot Wanted; the same file keeps its state. -1: key 0,
    // no file, or every place taken (counted in `refused`; the page shows its flat fallback).
    int Show(unsigned key, const Ask& a, double now)
    {
        if (!key || !a.file[0])
            return -1;
        int i = Find(key);
        if (i < 0)
        {
            for (int j = 0; j < kSlots && i < 0; ++j)
                if (!slots[j].key && !slots[j].held)
                    i = j;
            if (i < 0)
            {
                ++refused;
                return -1;
            }
            slots[i] = Slot{};
            slots[i].key = key;
            slots[i].state = State::Wanted;
        }
        Slot& s = slots[i];
        if (strcmp(s.ask.file, a.file) != 0)
            s.state = State::Wanted, s.turned = 0;
        s.ask = a;
        s.askedAt = pass;
        s.lastAsk = now;
        return i;
    }

    // A page closing: given back at the next pass.
    void Drop(unsigned key)
    {
        const int i = Find(key);
        if (i >= 0)
            slots[i].askedAt = 0, slots[i].lastAsk = -1e9;
    }

    // The pass begins: what an ask from now on counts for.
    void BeginPass() { ++pass; }

    // This pass's act for slot i (after BeginPass): asked since the previous pass began, or not.
    Act Next(int i, double now) const
    {
        const Slot& s = slots[i];
        if (!s.key)
            return s.held ? Act::Release : Act::None;
        const bool asked = s.askedAt + 1 >= pass;
        if (!asked)
            return now - s.lastAsk >= kGraceS ? Act::Release : Act::None;
        if (s.state == State::Wanted)
            return Act::Load;
        return s.state == State::Ready ? Act::Draw : Act::None;
    }

    // The game's half reports.
    void Loaded(int i, bool ok)
    {
        Slot& s = slots[i];
        if (ok)
            ++loads;
        else
            ++failures;
        s.held = ok;
        s.state = ok ? State::Ready : State::Failed;
    }
    void DrawFailed(int i)
    {
        ++failures;
        slots[i].state = State::Failed;
    }
    // Its holder freed (the place kept: the next ask loads again).
    void Freed(int i)
    {
        Slot& s = slots[i];
        if (s.held)
            ++frees;
        s.held = false;
        if (s.key && (s.state == State::Ready || s.state == State::Failed))
            s.state = State::Wanted;
    }
    // Its place given back (after Freed).
    void Released(int i) { slots[i] = Slot{}; }
    // The world changed (after the game's half freed every holder): every ask loads again at its next pass, a failed one
    // included (it held no holder, so Freed never saw it).
    void Forget()
    {
        for (Slot& s : slots)
            if (s.key && !s.held && (s.state == State::Ready || s.state == State::Failed))
                s.state = State::Wanted;
    }
};

// What the slots' holders depend on, read at each pass; a difference frees every holder (the holders stay valid in the engine:
// it destroys an instance only through its owner, docs\FREELANCER_RE.md "A model drawn with no game control"; the free is
// the slot's own rule so no holder outlives the scene it was asked in).
struct World
{
    unsigned loads = 0, base = 0, system = 0, resets = 0;
    int w = 0, h = 0;
    double at = 0; // the pass's time
};

enum class Why
{
    Same,
    Load,   // a game load (a save, New Game, the load after a death)
    Base,   // docked at another base, launched or landed
    System, // another system
    Device, // a device Reset
    Screen, // another back-buffer size
    Gap,    // the passes stopped for kGapS (a load screen, a Reset in progress, the window in the background)
};

inline Why Changed(const World& was, const World& now)
{
    if (now.loads != was.loads)
        return Why::Load;
    if (now.base != was.base)
        return Why::Base;
    if (now.system != was.system)
        return Why::System;
    if (now.resets != was.resets)
        return Why::Device;
    if (now.w != was.w || now.h != was.h)
        return Why::Screen;
    if (now.at - was.at > kGapS)
        return Why::Gap;
    return Why::Same;
}

inline const char* WhyName(Why w)
{
    switch (w)
    {
    case Why::Load: return "load";
    case Why::Base: return "base";
    case Why::System: return "system";
    case Why::Device: return "device";
    case Why::Screen: return "screen";
    case Why::Gap: return "gap";
    default: return "same";
    }
}
} // namespace modelslot
