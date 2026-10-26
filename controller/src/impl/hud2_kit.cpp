#include "hud2_kit.h"

#include "game/gamefont.h"
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

// The characters the atlas holds: what the game's names use (ASCII, Latin-1 Supplement for Rheinland / Bretonia / Kusari
// names, General Punctuation's dashes, quotes, bullet and ellipsis - the gates' and lanes' "New York \u2013> California" -,
// the arrows, the minus) plus every character met since that was missing (g_extra: added on demand, the fonts rebuilt
// once at the next pass, cached for the session; never a '?' for long).
static std::wstring g_extra;      // characters added on demand
static bool g_extraPending = false; // a rebuild is due
static const std::wstring& Chars()
{
    static std::wstring c;
    static size_t extra = static_cast<size_t>(-1);
    if (c.empty() || extra != g_extra.size())
    {
        c.clear();
        for (wchar_t ch = 32; ch < 127; ++ch)
            c += ch;
        for (wchar_t ch = 0xA0; ch <= 0xFF; ++ch) // Latin-1 Supplement (accented letters, the middle dot, the degree)
            c += ch;
        for (wchar_t ch = 0x2010; ch <= 0x2027; ++ch) // dashes, quotes, bullet, ellipsis
            c += ch;
        for (wchar_t ch = 0x2190; ch <= 0x2193; ++ch) // arrows
            c += ch;
        c += L"\u2212\u0152\u0153\u0160\u0161\u0178\u017D\u017E";
        c += g_extra;
        extra = g_extra.size();
    }
    return c;
}

// A character the atlas lacks: remembered (once) for the next build.
static void Missing(wchar_t ch)
{
    if (ch < 32 || g_extra.size() >= 256 || g_extra.find(ch) != std::wstring::npos)
        return;
    g_extra += ch;
    g_extraPending = true;
    Log("hud2kit: U+%04X not in the atlas - added (the fonts rebuild once)", static_cast<unsigned>(ch));
}

// The last other font set (fix-text, 25 Oct 2026): the kit's callers ask for different sizes in one frame when
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

void SegArc(float cx, float cy, float r, float a0, float a1, float width, float frac, u32 rgb, int n, float gapDeg,
            bool reverse, u32 emptyRgb, float emptyWidth)
{
    const float span = (a1 - a0) / n;
    const float ew = emptyWidth > 0 ? emptyWidth : width;
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
        auto it = f.glyphs.find(ch);
        if (it == f.glyphs.end())
            Missing(ch), it = f.glyphs.find(L'?');
        if (it != f.glyphs.end())
            w += static_cast<float>(it->second.advance);
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
        auto it = f.glyphs.find(ch);
        if (it == f.glyphs.end())
            Missing(ch), it = f.glyphs.find(L'?');
        return it != f.glyphs.end() ? static_cast<float>(it->second.advance) : 0.0f;
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
        auto it = f.glyphs.find(ch);
        if (it == f.glyphs.end())
            Missing(ch), it = f.glyphs.find(L'?');
        if (it == f.glyphs.end())
            continue;
        const render::Glyph& g = it->second;
        if (g.w > 0)
        {
            const float x0 = pen + g.ox * k, y0 = yTop + g.oy * k, x1 = x0 + g.w * k, y1 = y0 + g.h * k;
            Push(l, x0, y0, c, g.u0, g.v0), Push(l, x1, y0, c, g.u1, g.v0), Push(l, x1, y1, c, g.u1, g.v1);
            Push(l, x0, y0, c, g.u0, g.v0), Push(l, x1, y1, c, g.u1, g.v1), Push(l, x0, y1, c, g.u0, g.v1);
        }
        pen += g.advance * k + track * f.px * k;
    }
}

float Text(float x, float y, const std::wstring& s, float size, u32 argb, Align align, float track, bool outline)
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
    float yTop = y - f.lineHeight * k * 0.5f;
    if (k > 0.98f && k < 1.02f) // at the font's own size: on the pixel grid (sharp as point sampling)
        x = floorf(x + 0.5f), yTop = floorf(yTop + 0.5f);
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
    switch (id)
    {
    case glyphs::LB: return f == glyphs::Family::PlayStation ? L"L1" : f == glyphs::Family::Nintendo ? L"L" : L"LB";
    case glyphs::RB: return f == glyphs::Family::PlayStation ? L"R1" : f == glyphs::Family::Nintendo ? L"R" : L"RB";
    case glyphs::LT: return f == glyphs::Family::PlayStation ? L"L2" : f == glyphs::Family::Nintendo ? L"ZL" : L"LT";
    case glyphs::RT: return f == glyphs::Family::PlayStation ? L"R2" : f == glyphs::Family::Nintendo ? L"ZR" : L"RT";
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
    const float a = (argb >> 24) / 255.0f;
    const u32 rgb = argb & 0xFFFFFF;
    const float r = h * 0.46f, lw = std::max(1.5f, h * 0.075f);
    const u32 body = Argb(0x0D0B1A, 0.75f * a);
    const glyphs::Family fam = glyphs::Current();
    // glyph_look (glyphs::LookFor): the coloured look gives the face marks their family colour and the D-pad's pressed
    // arm the atlas's light; sizes, positions and the rest stay HUD 2's
    const bool coloured = glyphs::LookFor(true) == glyphs::Look::Coloured;
    const u32 mark = coloured && glyphs::FaceRgb(fam, id) ? Argb(glyphs::FaceRgb(fam, id), a) : argb;
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
        Ring(cx, cy, r - lw * 0.5f, lw, argb);
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

float Keycap(const std::wstring& name, float cx, float cy, float h, u32 argb, bool measure)
{
    // the key's name never under the 0.024 H text floor (UI_HUD2.md 1.9; the round 3 critique measured ~10 px keycaps):
    // the cap grows around it
    const float ts = std::max(name.size() > 2 ? h * 0.42f : h * 0.5f, 0.024f * g_sh);
    h = std::max(h, ts * (name.size() > 2 ? 1.0f / 0.42f : 2.0f) * 0.8f);
    const float w = std::max(h, TextWidth(name, ts, 0.04f) + h * 0.5f);
    if (measure)
        return w;
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
