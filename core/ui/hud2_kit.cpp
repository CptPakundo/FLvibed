// HUD 2's drawing kit (hud2kit::): a Direct3D 8 batcher with four layers, shapes, icons, text and draw recording.
#include "hud2_kit.h"

#include "game/gamefont.h"
#include "game/glyphset.h"
#include "game/marquee.h"
#include "game/promptfit.h"
#include "game/render.h"
#include "game/textfit.h"
#include "hook.h"
#include "log.h"

#include <sstream>
#include <unordered_map>

// (ASCII only: the build reads the sources as cp1252, non-ASCII text goes in as \u escapes)
namespace hud2kit
{
namespace dev
{
constexpr int BeginScene = 34, EndScene = 35, SetViewport = 40, SetRenderState = 50, ApplyStateBlock = 54,
              CaptureStateBlock = 55, DeleteStateBlock = 56, CreateStateBlock = 57, SetTexture = 61,
              SetTextureStageState = 63, DrawPrimitiveUP = 72, SetVertexShader = 76, SetPixelShader = 88;
}
struct Vertex
{
    float x, y, z, rhw;
    u32 color;
    float u, v;
};
constexpr u32 kFvf = 0x004 | 0x040 | 0x100; // XYZRHW | DIFFUSE | TEX1

enum LayerId
{
    LShape = 0,
    LIconBig,
    LIconSmall,
    LTextSmall,
    LTextBig,
    LTextCaption,
    LCount
};
struct Layer
{
    std::vector<Vertex> v;
    void* tex = nullptr;
};
static Layer g_layers[LCount];

static void* g_dev = nullptr;
static u32 g_stateBlock = 0;
static int g_sw = 0, g_sh = 0;
static bool g_keepDestAlpha = false; // the next End writes colour only (SetKeepDestAlpha)
static render::Font g_small, g_big;
static int g_textPx = 0;
static void* g_fontDev = nullptr;
static bool g_fontsOk = false;
static float g_alpha = 1, g_ox = 0, g_oy = 0;
// the text clip (TextMarquee): glyph quads are cut to x in [g_clipX0, g_clipX1] (before the offset), their texture
// coordinates cut with them - no draw call of its own (D3D8 has no scissor rect; the viewport would split the batch)
static float g_clipX0 = -1e30f, g_clipX1 = 1e30f;
static float g_wu = 0, g_wv = 0; // the white texel (small font)
static u32 g_frames = 0, g_lastVerts = 0, g_lastCalls = 0, g_fontBuilds = 0;

// text widths per string at each font's own size (the HUD's strings change at 10 Hz at most)
struct Widths
{
    float smallW = -1, bigW = -1;
};
static std::unordered_map<std::wstring, Widths> g_widths;
// the fit rule's results (core\game\textfit.h) and its floor: the legibility floor (textfit::MinSize, 0.024 H and never
// under 14 px) whatever text_size says - text_size sets the preferred size, the rule steps down from it as far as needed
static textfit::Cache g_fit;
static float g_floor = 0;
static float g_minPx = 0; // the legibility floor every text is clamped to: max(0.024 H, textfit::kMinPx)
static u32 g_fitMisses = 0;

// The characters the atlas holds: glyphset::Base plus every character met since that was missing (g_extra: added on
// demand, the fonts rebuilt once at the next pass, cached for the session).
static std::wstring g_extra;      // characters added on demand
static bool g_extraPending = false; // a rebuild is due
static const std::wstring& Chars()
{
    static std::wstring c;
    static size_t extra = static_cast<size_t>(-1);
    if (c.empty() || extra != g_extra.size())
    {
        c = glyphset::Base() + g_extra;
        extra = g_extra.size();
    }
    return c;
}

// The glyph of `ch` (render::GlyphFor), and what it met for the next build. A character of the set that the face could
// not show is left out of the atlas on purpose: it has its stand-in, and no rebuild would bring it.
static const render::Glyph* GlyphOf(const render::Font& f, wchar_t ch)
{
    const render::Glyph* g = render::GlyphFor(f, ch);
    static size_t seen = 0;
    const std::wstring& met = render::Unmet();
    for (; seen < met.size(); ++seen)
        if (Chars().find(met[seen]) == std::wstring::npos)
            g_extra += met[seen], g_extraPending = true;
    return g;
}

// The last other font set (fix-text): the kit's callers ask for different sizes in one frame when
// hud_scale is not 100 (HUD 2: text_size x hud_scale; the strip, the pills: text_size alone) - one set rebuilt the atlas
// on every pass (1877 builds in 40 s at hud_scale 130). Two sets swap in O(1) with their width and fit caches.
struct FontSet
{
    render::Font smallFont, bigFont;
    int px = 0;
    void* dev = nullptr;
    bool ok = false;
    u32 id = 0;
    std::unordered_map<std::wstring, Widths> widths;
    textfit::Cache fit;
};
static FontSet g_spare;
static u32 g_setId = 0; // the current set's id (the fit cache's font key)
static void SwapSpare()
{
    std::swap(g_small, g_spare.smallFont), std::swap(g_big, g_spare.bigFont);
    std::swap(g_textPx, g_spare.px), std::swap(g_fontDev, g_spare.dev), std::swap(g_fontsOk, g_spare.ok);
    std::swap(g_setId, g_spare.id), std::swap(g_widths, g_spare.widths), std::swap(g_fit, g_spare.fit);
}

static bool Fonts(int px)
{
    if (g_fontsOk && g_textPx == px && g_fontDev == g_dev && !g_extraPending)
        return true;
    if (g_extraPending)
        g_spare.ok = false; // (a new character: the other set lacks it too)
    else if (g_spare.ok && g_spare.px == px && g_spare.dev == g_dev)
    {
        SwapSpare();
        return true;
    }
    if (g_fontsOk && g_fontDev == g_dev && !g_extraPending)
        SwapSpare(); // keep the current set as the spare; build into the old spare's fonts
    g_extraPending = false;
    g_fontsOk = false;
    const bool game = gamefont::Load();
    const float k = game ? 1.0f : 0.85f; // Segoe UI reads larger than Agency FB
    const int big = static_cast<int>(px * 1.25f + 0.5f);
    if (!render::BuildFont(g_small, gamefont::LabelFace(), std::max(6, static_cast<int>(px * k)), 700, Chars()) ||
        !render::BuildFont(g_big, gamefont::LabelFace(), std::max(6, static_cast<int>(big * k)), 700, Chars()))
        return false;
    g_textPx = px, g_fontDev = g_dev, g_fontsOk = true;
    g_widths.clear();
    g_fit.Clear();
    ++g_fontBuilds;
    g_setId = g_fontBuilds;
    return true;
}

// The marquees' clocks (TextMarquee): when each cut text first showed, by its string, size and room (a hash: no
// allocation per frame once a text is known); a text not drawn for 30 passes starts over the next time it shows (a new
// target, a panel that faded out and back). Pruned at a pass's start once the table grows.
struct MarqueeClock
{
    double t0 = 0;
    u32 seen = 0; // the pass it was last drawn in (g_frames + 1: 0 = never)
};
static std::unordered_map<unsigned long long, MarqueeClock> g_marquees;
static unsigned long long MarqueeKey(const std::wstring& s, float size, float room)
{
    unsigned long long h = 1469598103934665603ull; // FNV-1a
    for (wchar_t c : s)
        h = (h ^ static_cast<unsigned long long>(c)) * 1099511628211ull;
    h = (h ^ static_cast<unsigned long long>(static_cast<long long>(room * 4))) * 1099511628211ull;
    h = (h ^ static_cast<unsigned long long>(static_cast<long long>(size * 8))) * 1099511628211ull;
    return h;
}
static double MarqueeClockOf(const std::wstring& s, float size, float room)
{
    const double now = perf::Now();
    MarqueeClock& c = g_marquees[MarqueeKey(s, size, room)];
    if (c.seen == 0 || g_frames + 1 - c.seen > 30)
        c.t0 = now;
    c.seen = g_frames + 1;
    return now - c.t0;
}
static void PruneMarquees()
{
    if (g_marquees.size() < 64)
        return;
    for (auto it = g_marquees.begin(); it != g_marquees.end();)
        it = g_frames + 1 - it->second.seen > 120 ? g_marquees.erase(it) : std::next(it);
}

bool Begin(void* device, int sw, int sh, int textPx)
{
    if (!device || sw <= 0 || sh <= 0)
        return false;
    if (device != g_dev)
        g_stateBlock = 0, g_fontsOk = false;
    g_dev = device;
    render::SetDevice(device);
    g_sw = sw, g_sh = sh;
    g_minPx = textfit::MinSize(static_cast<float>(sh));
    // the atlas never under the legibility floor (text clamped up to it would be drawn magnified - soft at 720p)
    if (!Fonts(std::max(textPx, static_cast<int>(ceilf(g_minPx)))))
        return false;
    g_floor = g_minPx; // the fit rule steps down to the legibility floor itself (text_size 150 cut where 0.024 H fits)
    g_fit.Bound(); // (no fit result is held across passes)
    for (Layer& l : g_layers)
        l.v.clear(), l.tex = nullptr;
    g_layers[LShape].tex = g_small.texture;
    g_layers[LTextSmall].tex = g_small.texture;
    g_layers[LTextBig].tex = g_big.texture;
    g_wu = g_small.whiteU, g_wv = g_small.whiteV;
    g_alpha = 1, g_ox = g_oy = 0;
    g_clipX0 = -1e30f, g_clipX1 = 1e30f;
    PruneMarquees();
    return true;
}

void SetAlpha(float a) { g_alpha = Clampf(a, 0, 1); }
float Alpha() { return g_alpha; }
void SetOffset(float dx, float dy) { g_ox = dx, g_oy = dy; }

static u32 Mul(u32 argb)
{
    const u32 a = static_cast<u32>((argb >> 24) * g_alpha + 0.5f);
    return (a << 24) | (argb & 0xFFFFFF);
}

static void Push(Layer& l, float x, float y, u32 c, float u, float v)
{
    l.v.push_back({x + g_ox - 0.5f, y + g_oy - 0.5f, 0, 1, c, u, v});
}

// ---------------------------------------------------------------- the lint's recorder (hud2_kit.h Record)
static std::vector<Drawn>* g_rec = nullptr;
static const char* g_recTag = "";
static int g_recDepth = 0; // kit calls inside kit calls: only the outermost records
static int g_recUnit = 0, g_recUnitNext = 0; // the current designed unit (RecUnit), the last id handed out
void Record(std::vector<Drawn>* out) { g_rec = out, g_recTag = "", g_recDepth = 0, g_recUnit = 0, g_recUnitNext = 0; }
void RecordTag(const char* tag) { g_recTag = tag ? tag : ""; }
RecUnit::RecUnit() : prev(g_recUnit)
{
    if (g_rec && !g_recUnit) // (ids only while a pass records: Record restarts them, nothing counts up between)
        g_recUnit = ++g_recUnitNext;
}
RecUnit::~RecUnit() { g_recUnit = prev; }
struct RecScope
{
    bool on, top;
    RecScope() : on(g_rec != nullptr), top(on && g_recDepth == 0)
    {
        if (on)
            ++g_recDepth;
    }
    ~RecScope()
    {
        if (on)
            --g_recDepth;
    }
};
static float AlphaOf(u32 argb) { return ((argb >> 24) / 255.0f) * g_alpha; }
static Drawn* Rec(char kind, float x0, float y0, float x1, float y1, float size, float alpha)
{
    if (!g_rec || alpha <= 0)
        return nullptr;
    Drawn d;
    d.kind = kind, d.x0 = x0 + g_ox, d.y0 = y0 + g_oy, d.x1 = x1 + g_ox, d.y1 = y1 + g_oy, d.size = size, d.alpha = alpha;
    d.tag = g_recTag, d.unit = g_recUnit;
    g_rec->push_back(std::move(d));
    return &g_rec->back();
}
static void RecArc(float cx, float cy, float r, float a0, float a1, float width, float alpha)
{
    if (a1 < a0)
        std::swap(a0, a1);
    // the box from the band (the centre line every <= 6 degrees, +- half the width)
    constexpr float kDeg = 3.14159265f / 180.0f;
    const int n = std::max(1, static_cast<int>(ceilf((a1 - a0) / 6.0f)));
    float x0 = 1e30f, y0 = 1e30f, x1 = -1e30f, y1 = -1e30f;
    for (int i = 0; i <= n; ++i)
    {
        const float t = (a0 + (a1 - a0) * i / n) * kDeg, x = cx + r * cosf(t), y = cy + r * sinf(t);
        x0 = std::min(x0, x), y0 = std::min(y0, y), x1 = std::max(x1, x), y1 = std::max(y1, y);
    }
    const float h = width * 0.5f;
    if (Drawn* d = Rec('A', x0 - h, y0 - h, x1 + h, y1 + h, width, alpha))
        d->cx = cx + g_ox, d->cy = cy + g_oy, d->r = r, d->a0 = a0, d->a1 = a1, d->w = width;
}

// ---------------------------------------------------------------- shapes
void Tri(float x0, float y0, float x1, float y1, float x2, float y2, u32 argb)
{
    const u32 c = Mul(argb);
    if (!(c >> 24))
        return;
    Layer& l = g_layers[LShape];
    Push(l, x0, y0, c, g_wu, g_wv), Push(l, x1, y1, c, g_wu, g_wv), Push(l, x2, y2, c, g_wu, g_wv);
}

void Quad(const float xy[8], u32 argb)
{
    const u32 c = Mul(argb);
    if (!(c >> 24))
        return;
    Layer& l = g_layers[LShape];
    Push(l, xy[0], xy[1], c, g_wu, g_wv), Push(l, xy[2], xy[3], c, g_wu, g_wv), Push(l, xy[4], xy[5], c, g_wu, g_wv);
    Push(l, xy[0], xy[1], c, g_wu, g_wv), Push(l, xy[4], xy[5], c, g_wu, g_wv), Push(l, xy[6], xy[7], c, g_wu, g_wv);
}

void Rect(float x, float y, float w, float h, u32 argb)
{
    if (w <= 0 || h <= 0)
        return;
    const RecScope rs;
    if (rs.top)
        Rec('S', x, y, x + w, y + h, 0, AlphaOf(argb));
    const float q[8] = {x, y, x + w, y, x + w, y + h, x, y + h};
    Quad(q, argb);
}

void Grad(float x, float y, float w, float h, u32 tl, u32 tr, u32 br, u32 bl)
{
    tl = Mul(tl), tr = Mul(tr), br = Mul(br), bl = Mul(bl);
    if (!((tl | tr | br | bl) >> 24) || w <= 0 || h <= 0)
        return;
    Layer& l = g_layers[LShape];
    Push(l, x, y, tl, g_wu, g_wv), Push(l, x + w, y, tr, g_wu, g_wv), Push(l, x + w, y + h, br, g_wu, g_wv);
    Push(l, x, y, tl, g_wu, g_wv), Push(l, x + w, y + h, br, g_wu, g_wv), Push(l, x, y + h, bl, g_wu, g_wv);
}

void Line(float x0, float y0, float x1, float y1, float width, u32 argb)
{
    const float dx = x1 - x0, dy = y1 - y0, len = sqrtf(dx * dx + dy * dy);
    if (len < 0.01f)
        return;
    const RecScope rs;
    if (rs.top)
        Rec('S', std::min(x0, x1) - width * 0.5f, std::min(y0, y1) - width * 0.5f, std::max(x0, x1) + width * 0.5f,
            std::max(y0, y1) + width * 0.5f, 0, AlphaOf(argb));
    const float nx = -dy / len * width * 0.5f, ny = dx / len * width * 0.5f;
    const float q[8] = {x0 + nx, y0 + ny, x1 + nx, y1 + ny, x1 - nx, y1 - ny, x0 - nx, y0 - ny};
    Quad(q, argb);
}

void Poly(const float* xy, int n, u32 argb)
{
    for (int i = 1; i + 1 < n; ++i)
        Tri(xy[0], xy[1], xy[i * 2], xy[i * 2 + 1], xy[i * 2 + 2], xy[i * 2 + 3], argb);
}

void Outline(const float* xy, int n, float width, u32 argb)
{
    const RecScope rs;
    if (rs.top && n > 0)
    {
        float x0 = xy[0], y0 = xy[1], x1 = xy[0], y1 = xy[1];
        for (int i = 1; i < n; ++i)
            x0 = std::min(x0, xy[i * 2]), x1 = std::max(x1, xy[i * 2]), y0 = std::min(y0, xy[i * 2 + 1]),
            y1 = std::max(y1, xy[i * 2 + 1]);
        Rec('S', x0 - width * 0.5f, y0 - width * 0.5f, x1 + width * 0.5f, y1 + width * 0.5f, 0, AlphaOf(argb));
    }
    for (int i = 0; i < n; ++i)
    {
        const int j = (i + 1) % n;
        // extend each edge by half the width so the corners close
        const float dx = xy[j * 2] - xy[i * 2], dy = xy[j * 2 + 1] - xy[i * 2 + 1];
        const float len = sqrtf(dx * dx + dy * dy);
        if (len < 0.01f)
            continue;
        const float ex = dx / len * width * 0.5f, ey = dy / len * width * 0.5f;
        Line(xy[i * 2] - ex, xy[i * 2 + 1] - ey, xy[j * 2] + ex, xy[j * 2 + 1] + ey, width, argb);
    }
}

void Arc(float cx, float cy, float r, float a0, float a1, float width, u32 argb)
{
    if (a1 < a0)
        std::swap(a0, a1);
    const RecScope rs;
    if (rs.top)
        RecArc(cx, cy, r, a0, a1, width, AlphaOf(argb));
    const int steps = std::max(1, static_cast<int>(ceilf((a1 - a0) / 6.0f)));
    const float ri = r - width * 0.5f, ro = r + width * 0.5f;
    constexpr float kDeg = 3.14159265f / 180.0f;
    float c0 = cosf(a0 * kDeg), s0 = sinf(a0 * kDeg);
    for (int i = 1; i <= steps; ++i)
    {
        const float a = (a0 + (a1 - a0) * i / steps) * kDeg, c1 = cosf(a), s1 = sinf(a);
        const float q[8] = {cx + c0 * ri, cy + s0 * ri, cx + c0 * ro, cy + s0 * ro,
                            cx + c1 * ro, cy + s1 * ro, cx + c1 * ri, cy + s1 * ri};
        Quad(q, argb);
        c0 = c1, s0 = s1;
    }
}

void Ring(float cx, float cy, float r, float width, u32 argb) { Arc(cx, cy, r, 0, 360, width, argb); }

void Disc(float cx, float cy, float r, u32 argb)
{
    const int steps = std::max(8, std::min(48, static_cast<int>(r * 0.8f)));
    constexpr float kTau = 6.2831853f;
    for (int i = 0; i < steps; ++i)
    {
        const float a0 = kTau * i / steps, a1 = kTau * (i + 1) / steps;
        Tri(cx, cy, cx + cosf(a0) * r, cy + sinf(a0) * r, cx + cosf(a1) * r, cy + sinf(a1) * r, argb);
    }
}

static u32 EmptyOf(u32 rgb)
{
    const u32 r = ((rgb >> 16) & 0xFF) / 5 + 14, g = ((rgb >> 8) & 0xFF) / 5 + 14, b = (rgb & 0xFF) / 5 + 22;
    return (r << 16) | (g << 8) | b;
}

void SegBar(float x, float y, float w, float h, float frac, u32 rgb, int n, float gap, bool right)
{
    if (n <= 0 || w <= 0)
        return;
    const RecScope rs;
    if (rs.top)
        Rec('S', x, y, x + w, y + h, 0, g_alpha);
    if (gap < 0)
        gap = std::max(2.0f, h * 0.28f);
    const float sw = (w - gap * (n - 1)) / n;
    const float lit = Clampf(frac, 0, 1) * n;
    const u32 empty = Argb(EmptyOf(rgb), 200 / 255.0f), full = Argb(rgb, 1);
    for (int i = 0; i < n; ++i)
    {
        const int k = right ? n - 1 - i : i;
        const float bx = x + k * (sw + gap);
        if (i < static_cast<int>(lit))
            Rect(bx, y, sw, h, full);
        else if (i < lit)
        {
            const float part = lit - static_cast<int>(lit);
            Rect(bx, y, sw, h, empty);
            if (right)
                Rect(bx + sw * (1 - part), y, sw * part, h, full);
            else
                Rect(bx, y, sw * part, h, full);
        }
        else
            Rect(bx, y, sw, h, empty);
    }
}

static float g_keyline = 0;
void SetArcKeyline(float px) { g_keyline = px; }

// The keyline under an arc's blocks: one dark arc, a little wider than they are.
static void Keyline(float cx, float cy, float r, float a0, float a1, float width)
{
    if (g_keyline > 0)
        Arc(cx, cy, r, a0, a1, width + 2 * g_keyline, Argb(kNavy, 0.6f));
}

void SegArcCentred(float cx, float cy, float r, float a0, float a1, float width, float frac, u32 rgb, int n, float gapDeg, u32 emptyRgb,
                   float emptyWidth)
{
    if (n <= 0)
        return;
    const float span = (a1 - a0) / n, ew = emptyWidth > 0 ? emptyWidth : width;
    const RecScope rs;
    if (rs.top)
        RecArc(cx, cy, r, a0 + gapDeg * 0.5f, a1 - gapDeg * 0.5f, std::max(width, ew), g_alpha);
    Keyline(cx, cy, r, a0 + gapDeg * 0.5f, a1 - gapDeg * 0.5f, std::max(width, ew));
    float lo = 0, hi = 0;
    promptfit::CentredLit(a0, a1, frac, lo, hi);
    for (int i = 0; i < n; ++i)
    {
        const float b0 = a0 + i * span + gapDeg * 0.5f, b1 = a0 + (i + 1) * span - gapDeg * 0.5f;
        const float l0 = std::max(b0, lo), l1 = std::min(b1, hi);
        if (l0 >= b1 || l1 <= b0 || l1 <= l0 || l0 > b0 || l1 < b1)
            Arc(cx, cy, r, b0, b1, ew, Argb(emptyRgb, 170 / 255.0f));
        if (l1 > l0)
            Arc(cx, cy, r, l0, l1, width, Argb(rgb, 1));
    }
}

void SegArc(float cx, float cy, float r, float a0, float a1, float width, float frac, u32 rgb, int n, float gapDeg,
            bool reverse, u32 emptyRgb, float emptyWidth)
{
    const float span = (a1 - a0) / n;
    const float ew = emptyWidth > 0 ? emptyWidth : width;
    const RecScope rs;
    Keyline(cx, cy, r, a0 + gapDeg * 0.5f, a1 - gapDeg * 0.5f, std::max(width, ew));
    // (the lint's record: the drawn extent - the first block starts half a gap after a0, the last ends half a gap before
    // a1; recording a0..a1 put two ring arcs 6 degrees apart where the drawn ones are 7.7 apart: UI_HUD2_PRESETS.md 6 item 3)
    if (rs.top && n > 0)
        RecArc(cx, cy, r, a0 + gapDeg * 0.5f, a1 - gapDeg * 0.5f, std::max(width, ew), g_alpha);
    for (int i = 0; i < n; ++i)
    {
        const int k = reverse ? n - 1 - i : i;
        const float b0 = a0 + k * span + gapDeg * 0.5f, b1 = a0 + (k + 1) * span - gapDeg * 0.5f;
        const bool lit = i < frac * n;
        Arc(cx, cy, r, b0, b1, lit ? width : ew, lit ? Argb(rgb, 1) : Argb(emptyRgb, 170 / 255.0f));
    }
}

void Plate(float x0, float y0, float x1, float y1, float c, int corners, float fill, u32 rim, float rimA, char accent,
           u32 accentRgb)
{
    const RecScope rs;
    if (rs.top)
        Rec('P', x0, y0, x1, y1, 0, g_alpha);
    const float tl = (corners & kTL) ? c : 0, tr = (corners & kTR) ? c : 0, br = (corners & kBR) ? c : 0,
                bl = (corners & kBL) ? c : 0;
    const float pts[16] = {x0 + tl, y0, x1 - tr, y0, x1, y0 + tr, x1, y1 - br,
                           x1 - br, y1, x0 + bl, y1, x0, y1 - bl, x0, y0 + tl};
    Poly(pts, 8, Argb(kGlass, fill));
    if (rimA > 0)
        Outline(pts, 8, std::max(1.5f, g_sh * 0.0009f), Argb(rim, rimA));
    if (accent)
    {
        const float aw = g_sh * 0.003f;
        if (accent == 'l')
            Rect(x0, y0 + tl, aw, (y1 - bl) - (y0 + tl), Argb(accentRgb, 1));
        else if (accent == 'r')
            Rect(x1 - aw, y0 + tr, aw, (y1 - br) - (y0 + tr), Argb(accentRgb, 1));
        else if (accent == 't')
            Rect(x0 + tl, y0, (x1 - tr) - (x0 + tl), aw, Argb(accentRgb, 1));
        else if (accent == 'b')
            Rect(x0 + bl, y1 - aw, (x1 - br) - (x0 + bl), aw, Argb(accentRgb, 1));
    }
}

// ---------------------------------------------------------------- icons
bool Icon(flicons::Icon icon, float cx, float cy, float size, u32 argb, float angle)
{
    void* tex = nullptr;
    float uv[4];
    if (!flicons::Uv(icon, size, &tex, uv))
        return false;
    const u32 c = Mul(argb);
    if (!(c >> 24))
        return true;
    if (g_rec && g_recDepth == 0)
        Rec('I', cx - size * 0.5f, cy - size * 0.5f, cx + size * 0.5f, cy + size * 0.5f, size, (c >> 24) / 255.0f);
    Layer& l = g_layers[size <= 64 * 1.1f ? LIconSmall : LIconBig];
    l.tex = tex;
    const float h = size * 0.5f, co = cosf(angle), si = sinf(angle);
    auto at = [&](float dx, float dy, float u, float v) { Push(l, cx + dx * co - dy * si, cy + dx * si + dy * co, c, u, v); };
    at(-h, -h, uv[0], uv[1]), at(h, -h, uv[2], uv[1]), at(h, h, uv[2], uv[3]);
    at(-h, -h, uv[0], uv[1]), at(h, h, uv[2], uv[3]), at(-h, h, uv[0], uv[3]);
    return true;
}

// ---------------------------------------------------------------- text
static bool UseBig(float size) { return size > g_small.px * 1.1f; }

static float RawWidth(const render::Font& f, const std::wstring& s)
{
    float w = 0;
    for (wchar_t ch : s)
    {
        if (const render::Glyph* g = GlyphOf(f, ch))
            w += static_cast<float>(g->advance);
    }
    return w;
}

float TextWidth(const std::wstring& s, float size, float track)
{
    if (!g_fontsOk || s.empty())
        return 0;
    size = std::max(size, g_minPx); // (the legibility floor: Text draws no smaller)
    const bool big = UseBig(size);
    const render::Font& f = big ? g_big : g_small;
    if (g_widths.size() > 2048)
        g_widths.clear();
    Widths& w = g_widths[s];
    float& raw = big ? w.bigW : w.smallW;
    if (raw < 0)
        raw = RawWidth(f, s);
    return raw * size / f.px + track * size * (s.size() - 1);
}

float TextFloor() { return g_floor; }

// The rule over one of the two fonts: each character's advance at the font's em, the ellipsis'.
static textfit::Result FitWith(const render::Font& f, const std::wstring& s, float size, float floor, float maxW,
                               float track)
{
    static std::vector<float> adv; // (grown once, reused)
    adv.resize(s.size());
    auto advance = [&](wchar_t ch)
    {
        const render::Glyph* g = GlyphOf(f, ch);
        return g ? static_cast<float>(g->advance) : 0.0f;
    };
    for (size_t i = 0; i < s.size(); ++i)
        adv[i] = advance(s[i]);
    return textfit::Fit(s, adv, advance(textfit::kEllipsis), static_cast<float>(f.px), size, floor, maxW, track);
}

const textfit::Result& FitText(const std::wstring& s, float size, float maxW, float track, float floor)
{
    if (floor < 0)
        floor = g_floor;
    size = std::max(size, g_minPx), floor = std::max(floor, g_minPx); // (never under the legibility floor)
    const bool big = UseBig(size);
    if (const textfit::Result* r = g_fit.Find(s, size, floor, maxW, track, (g_setId << 1) | (big ? 1 : 0)))
        return *r;
    ++g_fitMisses;
    textfit::Result r;
    if (!g_fontsOk || s.empty())
        r.text = s, r.size = size;
    else
    {
        r = FitWith(big ? g_big : g_small, s, size, floor, maxW, track);
        if (UseBig(r.size) != big) // the size it lands on draws from the other font: measured in that one
            r = FitWith(big ? g_small : g_big, s, size, floor, maxW, track);
    }
    // (Find again: FitWith may have queued a missing character, the key is the last Find's)
    g_fit.Find(s, size, floor, maxW, track, (g_setId << 1) | (big ? 1 : 0));
    return g_fit.Put(std::move(r));
}

float TextFit(float x, float y, const std::wstring& s, float size, float maxW, u32 argb, Align align, float track,
              bool outline, float floor)
{
    const textfit::Result& r = FitText(s, size, maxW, track, floor);
    return Text(x, y, r.text, r.size, argb, align, track, outline);
}

// (the wrapped results per string / size / box / line count; cleared with the fit cache's bound)
struct Wrapped
{
    float size = 0;
    std::vector<std::wstring> lines;
};
static std::unordered_map<std::wstring, Wrapped> g_wrapped;

int FitLines(const std::wstring& s, float size, float maxW, float track, std::wstring* out, int maxLines, float& outSize)
{
    const textfit::Result& one = FitText(s, size, maxW, track);
    outSize = one.size;
    if (!one.cut || maxLines < 2)
    {
        out[0] = one.text;
        return 1;
    }
    const wchar_t key[5] = {static_cast<wchar_t>(0x1F), static_cast<wchar_t>(g_setId & 0xFFFF),
                            static_cast<wchar_t>(static_cast<int>(size * 4) & 0xFFFF),
                            static_cast<wchar_t>(static_cast<int>(maxW * 4) & 0xFFFF), static_cast<wchar_t>(maxLines)};
    if (g_wrapped.size() > 256)
        g_wrapped.clear();
    Wrapped& w = g_wrapped[s + std::wstring(key, 5)];
    if (w.lines.empty() || w.size != one.size)
    {
        // at the floor, word by word: each line takes the whole words that fit, the last line the rest by the rule
        const float z = one.size;
        w.size = z, w.lines.clear();
        size_t at = 0;
        while (static_cast<int>(w.lines.size()) < maxLines - 1)
        {
            size_t best = std::wstring::npos, sp = s.find(L' ', at);
            for (;;)
            {
                const size_t end = sp == std::wstring::npos ? s.size() : sp;
                if (TextWidth(s.substr(at, end - at), z, track) > maxW)
                    break;
                best = end;
                if (sp == std::wstring::npos)
                    break;
                sp = s.find(L' ', sp + 1);
            }
            if (best == std::wstring::npos) // not even the first word fits: the last line cuts it
                break;
            w.lines.push_back(s.substr(at, best - at));
            at = best;
            while (at < s.size() && s[at] == L' ')
                ++at;
            if (at >= s.size())
                break;
        }
        if (at < s.size())
            w.lines.push_back(FitText(s.substr(at), z, maxW, track, z).text);
    }
    const int n = std::min(maxLines, static_cast<int>(w.lines.size()));
    for (int i = 0; i < n; ++i)
        out[i] = w.lines[i];
    return n;
}

std::wstring Fit(const std::wstring& s, float size, float maxW, float track)
{
    return FitText(s, size, maxW, track, size).text; // (the floor = the size: cut only)
}

static void Glyphs(Layer& l, const render::Font& f, float x, float yTop, const std::wstring& s, float k, float track,
                   u32 c)
{
    float pen = x;
    for (wchar_t ch : s)
    {
        const render::Glyph* found = GlyphOf(f, ch);
        if (!found)
            continue;
        const render::Glyph& g = *found;
        if (g.w > 0)
        {
            float x0 = pen + g.ox * k, x1 = x0 + g.w * k, u0 = g.u0, u1 = g.u1;
            const float y0 = yTop + g.oy * k, y1 = y0 + g.h * k;
            if (x1 > g_clipX0 && x0 < g_clipX1) // (the clip: TextMarquee's room; else unbounded)
            {
                const float qx0 = x0, qw = x1 - x0, du = g.u1 - g.u0;
                if (x0 < g_clipX0)
                    u0 = g.u0 + du * (g_clipX0 - qx0) / qw, x0 = g_clipX0;
                if (x1 > g_clipX1)
                    u1 = g.u0 + du * (g_clipX1 - qx0) / qw, x1 = g_clipX1;
                Push(l, x0, y0, c, u0, g.v0), Push(l, x1, y0, c, u1, g.v0), Push(l, x1, y1, c, u1, g.v1);
                Push(l, x0, y0, c, u0, g.v0), Push(l, x1, y1, c, u1, g.v1), Push(l, x0, y1, c, u0, g.v1);
            }
        }
        pen += g.advance * k + track * f.px * k;
    }
}

// Text without its record (Text records it; TextMarquee records its room instead); `snap` = onto the pixel grid at the
// font's own size (a scrolling text keeps its sub-pixel place: linear filtering draws it between pixels).
static float TextDraw(float x, float y, const std::wstring& s, float size, u32 argb, Align align, float track, bool outline,
                      bool snap, bool record)
{
    if (!g_fontsOk || s.empty())
        return 0;
    size = std::max(size, g_minPx); // the legibility floor (text_size / hud_scale at their minimum, small screens)
    const float w = TextWidth(s, size, track);
    const u32 c = Mul(argb);
    if (!(c >> 24))
        return w;
    const bool big = UseBig(size);
    const render::Font& f = big ? g_big : g_small;
    Layer& l = g_layers[big ? LTextBig : LTextSmall];
    const float k = size / f.px;
    if (align == Center)
        x -= w * 0.5f;
    else if (align == Right)
        x -= w;
    if (record && g_rec && g_recDepth == 0) // (a glyph's letter, a keycap's name: inside their call, not recorded)
        if (Drawn* d = Rec('T', x, y - size * 0.5f, x + w, y + size * 0.5f, size, (c >> 24) / 255.0f))
            d->text = s;
    float yTop = y - f.lineHeight * k * 0.5f;
    if (k > 0.98f && k < 1.02f) // at the font's own size: on the pixel grid (sharp as point sampling); a scrolling
    {                           // text only vertically
        if (snap)
            x = floorf(x + 0.5f);
        yTop = floorf(yTop + 0.5f);
    }
    if (outline)
    {
        const float o = std::max(1.0f, size * 0.06f);
        const u32 sc = (static_cast<u32>((c >> 24) * 0.55f) << 24) | kNavy;
        static const float kOff[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
        for (const auto& d : kOff)
            Glyphs(l, f, x + d[0] * o, yTop + d[1] * o, s, k, track, sc);
    }
    Glyphs(l, f, x, yTop, s, k, track, c);
    return w;
}

float Text(float x, float y, const std::wstring& s, float size, u32 argb, Align align, float track, bool outline)
{
    return TextDraw(x, y, s, size, argb, align, track, outline, true, true);
}

// The game's captions, measured on the Options screen's siblings at 4K (the page button's caption, options_page.cpp): Agency FB Regular, 72 px em = 0.0333 H, its capitals centred on the
// button, a dark shadow down-right. Their own atlas, built the first time one is drawn: only the main menu asks.
static render::Font g_caption;
static void* g_captionDev = nullptr;
static size_t g_captionChars = 0;
static bool CaptionFont()
{
    const int px = std::max(8, static_cast<int>(g_sh * 0.0333f + 1.5f)); // (+1.5: the game's rasteriser rounds up at small sizes)
    if (g_caption.px == px && g_caption.texture && g_captionDev == g_dev && g_captionChars == Chars().size())
        return true;
    const bool game = gamefont::Load();
    if (!render::BuildFont(g_caption, game ? gamefont::LabelFace() : L"Segoe UI", px, game ? 400 : 600, Chars()))
        return false;
    g_captionDev = g_dev, g_captionChars = Chars().size();
    return true;
}

float GameCaption(float x, float y, const std::wstring& s, u32 argb, Align align, float size)
{
    if (!g_fontsOk || s.empty())
        return 0;
    if (!CaptionFont())
        return Text(x, y, s, size > 0 ? size : g_sh * 0.0333f, argb, align); // (fails closed: the HUD's face)
    const render::Font& f = g_caption;
    const float k = size > 0 ? size / f.px : 1.0f;
    const float w = RawWidth(f, s) * k;
    const u32 c = Mul(argb);
    if (!(c >> 24))
        return w;
    g_layers[LTextCaption].tex = f.texture;
    if (align == Center)
        x -= w * 0.5f;
    else if (align == Right)
        x -= w;
    float capTop = f.ascent * 0.25f, capH = f.ascent * 0.75f;
    const auto it = f.glyphs.find(L'F');
    if (it != f.glyphs.end() && it->second.h > 0)
        capTop = static_cast<float>(it->second.oy), capH = static_cast<float>(it->second.h);
    const float yTop = y - (capTop + capH * 0.5f) * k;
    if (g_rec && g_recDepth == 0)
        if (Drawn* d = Rec('T', x, y - capH * k * 0.5f, x + w, y + capH * k * 0.5f, f.px * k, (c >> 24) / 255.0f))
            d->text = s;
    const float o = std::max(1.0f, g_sh * 0.0012f);
    Glyphs(g_layers[LTextCaption], f, x + o, yTop + o, s, k, 0, (static_cast<u32>((c >> 24) * 0.75f) << 24));
    Glyphs(g_layers[LTextCaption], f, x, yTop, s, k, 0, c);
    return w;
}

// The marquee (core\game\marquee.h; the owner, 31 Oct 2026, HUD 2 decision 1): the fit rule first; a text it would cut
// draws in full, clipped to its room [bx, bx + maxW], moved by the marquee's offset at this moment (its clock: since the
// text first showed, MarqueeClockOf), the ellipsis at the clip's edge with its alpha. Recorded for the lint as ONE text
// whose box is the room (never the full text: nothing is drawn outside it) - the lint sees the same box at every moment.
void TextClipped(float x, float y, const std::wstring& s, float size, float clipX0, float clipX1, u32 argb)
{
    const float cx0 = g_clipX0, cx1 = g_clipX1;
    g_clipX0 = clipX0, g_clipX1 = clipX1;
    TextDraw(x, y, s, std::max(size, g_minPx), argb, Left, 0, false, false, false);
    g_clipX0 = cx0, g_clipX1 = cx1;
}

static const marquee::Timing kMarquee{};
float TextMarquee(float x, float y, const std::wstring& s, float size, float maxW, u32 argb, Align align, float track,
                  bool outline, float floor)
{
    const textfit::Result& r = FitText(s, size, maxW, track, floor);
    if (!r.cut || !g_fontsOk || maxW < 1.0f || s.empty())
        return Text(x, y, r.text, r.size, argb, align, track, outline);
    const float z = std::max(r.size, g_minPx);
    const float bx = align == Center ? x - maxW * 0.5f : align == Right ? x - maxW : x;
    static const std::wstring kEll(1, textfit::kEllipsis);
    const float textW = TextWidth(s, z, track), ellW = TextWidth(kEll, z, track);
    if (marquee::JustOver(textW, maxW, ellW))
        return Text(x, y, s, z, argb, align, track, outline);
    // the cut form's prefix (its whole characters before the ellipsis): its width without the ellipsis and the track
    // gap before it
    const float prefixW = r.text.size() > 1 ? std::max(0.0f, r.width - ellW - track * z) : 0.0f;
    const marquee::Frame f = marquee::At(kMarquee, textW, maxW, prefixW, ellW,
                                         marquee::Speed(kMarquee, static_cast<float>(g_sh), z, g_floor),
                                         MarqueeClockOf(s, z, maxW));
    const RecScope rs; // (the parts inside draw without records: the room is the record)
    if (rs.top)
        if (Drawn* d = Rec('T', bx, y - z * 0.5f, bx + maxW, y + z * 0.5f, z, AlphaOf(argb)))
            d->text = s;
    auto fade = [&](float a)
    {
        const u32 al = static_cast<u32>((argb >> 24) * Clampf(a, 0, 1) + 0.5f);
        return (al << 24) | (argb & 0xFFFFFF);
    };
    const float cx0 = g_clipX0, cx1 = g_clipX1;
    g_clipX0 = bx, g_clipX1 = bx + f.clip;
    TextDraw(bx - f.offset, y, s, z, fade(f.alpha), Left, track, outline, false, false);
    if (f.ellipsis > 0.004f)
    {
        g_clipX1 = bx + maxW;
        TextDraw(bx + f.clip + track * z, y, kEll, z, fade(f.alpha * f.ellipsis), Left, track, outline, false, false);
    }
    g_clipX0 = cx0, g_clipX1 = cx1;
    return maxW;
}

// A glyph's mark (its letter inside the button's shape): sized by the shape, not text - exempt from the legibility floor
// (clamped up it would leave its ring).
static void Mark(float cx, float cy, const wchar_t* s, float size, u32 argb)
{
    const float keep = g_minPx;
    g_minPx = 0;
    Text(cx, cy, s, size, argb, Center, 0, false);
    g_minPx = keep;
}

// ---------------------------------------------------------------- glyphs and keycaps
static const wchar_t* PillLabel(glyphs::Id id)
{
    const glyphs::Family f = glyphs::Current();
    const bool one = f == glyphs::Family::PlayStation || f == glyphs::Family::Steam; // (the L1 / R1 names)
    switch (id)
    {
    case glyphs::LB: return one ? L"L1" : f == glyphs::Family::Nintendo ? L"L" : L"LB";
    case glyphs::RB: return one ? L"R1" : f == glyphs::Family::Nintendo ? L"R" : L"RB";
    case glyphs::LT: return one ? L"L2" : f == glyphs::Family::Nintendo ? L"ZL" : L"LT";
    case glyphs::RT: return one ? L"R2" : f == glyphs::Family::Nintendo ? L"ZR" : L"RT";
    default: return nullptr;
    }
}

float GlyphWidth(glyphs::Id id, float h) { return PillLabel(id) ? h * 1.3f : h; }

static void Chamfered(float cx, float cy, float w, float h, float c, float width, u32 argb, u32 fill)
{
    const float x0 = cx - w * 0.5f, x1 = cx + w * 0.5f, y0 = cy - h * 0.5f, y1 = cy + h * 0.5f;
    const float pts[16] = {x0 + c, y0, x1 - c, y0, x1, y0 + c, x1, y1 - c, x1 - c, y1, x0 + c, y1, x0, y1 - c, x0, y0 + c};
    if (fill >> 24)
        Poly(pts, 8, fill);
    Outline(pts, 8, width, argb);
}

void Glyph(glyphs::Id id, float cx, float cy, float h, u32 argb, bool hold)
{
    const RecScope rs;
    if (rs.top)
    {
        const float hw = std::max(GlyphWidth(id, h) * 0.5f, hold ? h * 0.65f : 0.0f), hh = hold ? h * 0.65f : h * 0.5f;
        if (Drawn* d = Rec('G', cx - hw, cy - hh, cx + hw, cy + hh, h, AlphaOf(argb)))
            d->glyph = static_cast<int>(id);
    }
    const float a = (argb >> 24) / 255.0f;
    const u32 rgb = argb & 0xFFFFFF;
    const float r = h * 0.46f, lw = std::max(1.5f, h * 0.075f);
    const u32 body = Argb(0x0D0B1A, 0.75f * a);
    const glyphs::Family fam = glyphs::Current();
    // glyph_look: the face marks take their family colour (glyphs::FaceColours), and the coloured look (glyphs::LookFor) also gives the D-pad's
    // pressed arm the atlas's light; sizes, positions and the rest stay HUD 2's
    const bool coloured = glyphs::LookFor(true) == glyphs::Look::Coloured;
    const u32 mark = glyphs::FaceColours(fam) && glyphs::FaceRgb(fam, id) ? Argb(glyphs::FaceRgb(fam, id), a) : argb;
    // the face button's ring shares its mark's colour where its family's buttons have one (glyphs::OutlineRgb), so it
    // dims with the mark; the hold ring outside stays in the caller's colour
    const u32 ring = glyphs::FaceColours(fam) && glyphs::OutlineRgb(fam, id) ? mark : argb;
    const u32 arm = coloured ? Argb(glyphs::kPressedArm, a) : argb;
    auto letter = [&](const wchar_t* s, float k) { Mark(cx, cy, s, h * k, argb); };
    if (const wchar_t* pill = PillLabel(id))
    {
        Chamfered(cx, cy, h * 1.3f, h * 0.78f, h * 0.2f, lw, argb, body);
        letter(pill, 0.46f);
    }
    else if ((id >= glyphs::DU && id <= glyphs::DR) || id == glyphs::COUNT)
    {
        // the D-pad: a plus, the arm lit
        const float t = h * 0.17f, e = h * 0.46f;
        const float pts[24] = {cx - t, cy - e, cx + t, cy - e, cx + t, cy - t, cx + e, cy - t, cx + e, cy + t, cx + t, cy + t,
                               cx + t, cy + e, cx - t, cy + e, cx - t, cy + t, cx - e, cy + t, cx - e, cy - t, cx - t, cy - t};
        Poly(pts, 12, body);
        Outline(pts, 12, lw, argb);
        if (id != glyphs::COUNT)
        {
            const float dx = id == glyphs::DL ? -1.0f : id == glyphs::DR ? 1.0f : 0.0f;
            const float dy = id == glyphs::DU ? -1.0f : id == glyphs::DD ? 1.0f : 0.0f;
            const float ax = cx + dx * (e + t) * 0.5f, ay = cy + dy * (e + t) * 0.5f;
            const float hw = dx != 0 ? (e - t) * 0.5f - lw : t - lw, hh = dy != 0 ? (e - t) * 0.5f - lw : t - lw;
            Rect(ax - hw, ay - hh, hw * 2, hh * 2, arm);
        }
    }
    else
    {
        Disc(cx, cy, r, body);
        Ring(cx, cy, r - lw * 0.5f, lw, ring);
        switch (id)
        {
        case glyphs::A:
        case glyphs::B:
        case glyphs::X:
        case glyphs::Y:
            if (fam == glyphs::Family::PlayStation)
            {
                const float s = r * 0.42f, w2 = std::max(1.5f, h * 0.07f);
                if (id == glyphs::A)
                    Line(cx - s, cy - s, cx + s, cy + s, w2, mark), Line(cx - s, cy + s, cx + s, cy - s, w2, mark);
                else if (id == glyphs::B)
                    Ring(cx, cy, s, w2, mark);
                else if (id == glyphs::X)
                {
                    const float q[8] = {cx - s, cy - s, cx + s, cy - s, cx + s, cy + s, cx - s, cy + s};
                    Outline(q, 4, w2, mark);
                }
                else
                {
                    const float q[6] = {cx, cy - s * 1.1f, cx + s * 1.05f, cy + s * 0.75f, cx - s * 1.05f, cy + s * 0.75f};
                    Outline(q, 3, w2, mark);
                }
            }
            else
            {
                static const wchar_t* kXbox[4] = {L"A", L"B", L"X", L"Y"};
                static const wchar_t* kNin[4] = {L"B", L"A", L"Y", L"X"};
                Mark(cx, cy, (fam == glyphs::Family::Nintendo ? kNin : kXbox)[id], h * 0.56f, mark);
            }
            break;
        case glyphs::LS:
        case glyphs::L3: letter(L"L", 0.52f); break;
        case glyphs::RS:
        case glyphs::R3: letter(L"R", 0.52f); break;
        case glyphs::VIEW:
        {
            const float s = r * 0.36f, w2 = std::max(1.2f, h * 0.06f);
            const float b1[8] = {cx - s * 1.1f, cy - s * 0.9f, cx + s * 0.5f, cy - s * 0.9f, cx + s * 0.5f, cy + s * 0.5f,
                                 cx - s * 1.1f, cy + s * 0.5f};
            Outline(b1, 4, w2, argb);
            Rect(cx - s * 0.5f, cy - s * 0.3f, s * 1.6f, s * 1.3f, argb);
            break;
        }
        case glyphs::MENU:
        {
            const float s = r * 0.42f, w2 = std::max(1.2f, h * 0.065f);
            for (int i = -1; i <= 1; ++i)
                Line(cx - s, cy + i * s * 0.6f, cx + s, cy + i * s * 0.6f, w2, argb);
            break;
        }
        default: break;
        }
    }
    if (hold) // the broken outer ring: hold this button
        for (int k = 0; k < 4; ++k)
            Arc(cx, cy, h * 0.62f, -60.0f + k * 90.0f, -10.0f + k * 90.0f, std::max(1.5f, h * 0.06f), Argb(rgb, a * 0.9f));
    (void)rgb;
}

float Keycap(const std::wstring& name, float cx, float cy, float h, u32 argb, bool measure, bool tight)
{
    if (tight) // the dense panels' keycap (hud2_kit.h): the name at the floor, the cap 1.2 x the name, no lip margin
    {
        const float ts = promptfit::KeycapTightNameSize(h, std::max(0.024f * g_sh, g_minPx)), hc = ts * 1.2f;
        const float w = promptfit::KeycapTightWidth(TextWidth(name, ts, 0.04f), ts); // (the layout tests' numbers)
        if (measure)
            return w;
        const RecScope rs;
        if (rs.top)
            if (Drawn* d = Rec('K', cx - w * 0.5f, cy - hc * 0.5f, cx + w * 0.5f, cy + hc * 0.5f, ts, AlphaOf(argb)))
                d->text = name;
        const float a = (argb >> 24) / 255.0f;
        const float lw = std::max(1.5f, hc * 0.07f);
        Chamfered(cx, cy, w, hc, hc * 0.18f, lw, argb, Argb(0x0D0B1A, 0.75f * a));
        Rect(cx - w * 0.5f + hc * 0.18f, cy + hc * 0.5f - lw * 0.5f, w - hc * 0.36f, lw, Argb(argb & 0xFFFFFF, a * 0.6f));
        Text(cx, cy - hc * 0.03f, name, ts, argb, Center, 0.04f, false);
        return w;
    }
    // the key's name never under the 0.024 H text floor (UI_HUD2.md 1.9; the round 3 critique measured ~10 px keycaps):
    // the cap grows around it (promptfit.h KeycapNameSize / KeycapHeight / KeycapWidth: the layout tests' same numbers)
    const float ts = promptfit::KeycapNameSize(name.size(), h, static_cast<float>(g_sh));
    const float w = promptfit::KeycapWidth(TextWidth(name, ts, 0.04f), name.size(), h, static_cast<float>(g_sh));
    h = promptfit::KeycapHeight(name.size(), h, static_cast<float>(g_sh));
    if (measure)
        return w;
    const RecScope rs;
    if (rs.top)
        if (Drawn* d = Rec('K', cx - w * 0.5f, cy - h * 0.43f, cx + w * 0.5f, cy + h * 0.43f, ts, AlphaOf(argb)))
            d->text = name;
    const float a = (argb >> 24) / 255.0f;
    const float lw = std::max(1.5f, h * 0.07f);
    Chamfered(cx, cy, w, h * 0.86f, h * 0.18f, lw, argb, Argb(0x0D0B1A, 0.75f * a));
    // the key's lower lip (a keycap, not a button)
    Rect(cx - w * 0.5f + h * 0.18f, cy + h * 0.43f - lw * 0.5f, w - h * 0.36f, lw, Argb(argb & 0xFFFFFF, a * 0.6f));
    Text(cx, cy - h * 0.03f, name, ts, argb, Center, 0.04f, false);
    return w;
}

// ---------------------------------------------------------------- the pass
static void SetRS(u32 s, u32 v) { VCall<HRESULT>(g_dev, dev::SetRenderState, s, v); }
static void SetTSS(u32 stage, u32 t, u32 v) { VCall<HRESULT>(g_dev, dev::SetTextureStageState, stage, t, v); }

void SetKeepDestAlpha() { g_keepDestAlpha = true; }

void End()
{
    u32 verts = 0;
    for (const Layer& l : g_layers)
        verts += static_cast<u32>(l.v.size());
    g_lastVerts = verts, g_lastCalls = 0;
    ++g_frames;
    if (!g_dev || !verts)
        return;
    if (!g_stateBlock && FAILED(VCall<HRESULT>(g_dev, dev::CreateStateBlock, 1u /*D3DSBT_ALL*/, &g_stateBlock)))
        g_stateBlock = 0;
    if (g_stateBlock)
        VCall<HRESULT>(g_dev, dev::CaptureStateBlock, g_stateBlock);
    struct
    {
        u32 x, y, w, h;
        float minZ, maxZ;
    } vp = {0, 0, static_cast<u32>(g_sw), static_cast<u32>(g_sh), 0.0f, 1.0f};
    VCall<HRESULT>(g_dev, dev::SetViewport, static_cast<void*>(&vp));
    SetRS(7, 0);     // ZENABLE
    SetRS(14, 0);    // ZWRITEENABLE
    SetRS(8, 3);     // FILLMODE solid
    SetRS(15, 0);    // ALPHATESTENABLE
    SetRS(27, 1);    // ALPHABLENDENABLE
    SetRS(19, 5);    // SRCBLEND srcalpha
    SetRS(20, 6);    // DESTBLEND invsrcalpha
    SetRS(171, 1);   // BLENDOP add
    SetRS(22, 1);    // CULLMODE none
    SetRS(28, 0);    // FOGENABLE
    SetRS(29, 0);    // SPECULARENABLE
    SetRS(52, 0);    // STENCILENABLE
    SetRS(137, 0);   // LIGHTING
    SetRS(152, 0);   // CLIPPLANEENABLE
    SetRS(168, g_keepDestAlpha ? 0x7 : 0xF); // COLORWRITEENABLE (rgb only: the back buffer's alpha left as it was)
    g_keepDestAlpha = false;
    VCall<HRESULT>(g_dev, dev::SetPixelShader, 0u);
    VCall<HRESULT>(g_dev, dev::SetVertexShader, kFvf);
    VCall<HRESULT>(g_dev, dev::SetTexture, 1u, static_cast<void*>(nullptr));
    SetTSS(0, 1, 4);  // COLOROP modulate
    SetTSS(0, 2, 2);  // COLORARG1 texture
    SetTSS(0, 3, 0);  // COLORARG2 diffuse
    SetTSS(0, 4, 4);  // ALPHAOP modulate
    SetTSS(0, 5, 2);  // ALPHAARG1 texture
    SetTSS(0, 6, 0);  // ALPHAARG2 diffuse
    SetTSS(0, 11, 0); // TEXCOORDINDEX
    SetTSS(0, 24, 0); // TEXTURETRANSFORMFLAGS
    SetTSS(0, 13, 3); // ADDRESSU clamp
    SetTSS(0, 14, 3); // ADDRESSV clamp
    SetTSS(0, 16, 2); // MAGFILTER linear
    SetTSS(0, 17, 2); // MINFILTER linear
    SetTSS(0, 18, 0); // MIPFILTER none
    SetTSS(1, 1, 1);  // stage 1 COLOROP disable
    SetTSS(1, 4, 1);  // stage 1 ALPHAOP disable
    for (int li = 0; li < LCount; ++li)
    {
        Layer& l = g_layers[li];
        if (l.v.empty() || !l.tex)
            continue;
        // the flat icons in the HUD tint (the critique, round 3: the maneuver icons read multicoloured): their colour is
        // the vertex colour alone, the shape the texture's alpha; shapes and text modulate (the white texel / the font)
        const bool icon = li == LIconBig || li == LIconSmall;
        SetTSS(0, 1, icon ? 3 : 4); // COLOROP selectarg2 (diffuse) / modulate
        VCall<HRESULT>(g_dev, dev::SetTexture, 0u, l.tex);
        VCall<HRESULT>(g_dev, dev::DrawPrimitiveUP, 4u /*TRIANGLELIST*/, static_cast<UINT>(l.v.size() / 3),
                       static_cast<const void*>(l.v.data()), static_cast<UINT>(sizeof(Vertex)));
        ++g_lastCalls;
    }
    if (g_stateBlock)
        VCall<HRESULT>(g_dev, dev::ApplyStateBlock, g_stateBlock);
}

void OnDeviceLost()
{
    if (g_dev && g_stateBlock)
        VCall<HRESULT>(g_dev, dev::DeleteStateBlock, g_stateBlock);
    g_stateBlock = 0;
}

void Shutdown()
{
    OnDeviceLost();
    render::FreeFont(g_small);
    render::FreeFont(g_big);
    render::FreeFont(g_caption);
    render::FreeFont(g_spare.smallFont);
    render::FreeFont(g_spare.bigFont);
    g_fontsOk = false;
    g_spare.ok = false;
    g_spare.widths.clear();
    g_spare.fit.Clear();
    for (Layer& l : g_layers)
        std::vector<Vertex>().swap(l.v);
    g_widths.clear();
    g_fit.Clear();
}

static std::string FaceA()
{
    std::string s;
    for (wchar_t c : g_small.face)
        s += c == L' ' ? '_' : c < 128 ? static_cast<char>(c) : '?';
    return s.empty() ? "-" : s;
}

std::string Status()
{
    std::ostringstream o;
    o << "kit fonts=" << g_small.px << "/" << g_big.px << "px builds=" << g_fontBuilds << " verts=" << g_lastVerts
      << " calls=" << g_lastCalls << " widths=" << g_widths.size() << " fits=" << g_fit.Size() << " fitMisses=" << g_fitMisses
      << " floor=" << static_cast<int>(g_floor + 0.5f) << "px face=" << FaceA() << " chars=" << Chars().size() << " added=";
    for (wchar_t ch : g_extra)
    {
        char b[12];
        sprintf_s(b, "U+%04X ", static_cast<unsigned>(ch));
        o << b;
    }
    return o.str();
}
} // namespace hud2kit
