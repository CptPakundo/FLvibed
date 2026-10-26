#include "render.h"

#include "hook.h"
#include "log.h"

#include <vector>

namespace render
{
// IDirect3DDevice8 vtable indices
namespace dev
{
constexpr int GetBackBuffer = 16, CreateTexture = 20, BeginScene = 34, EndScene = 35, SetViewport = 40,
              GetViewport = 41, SetRenderState = 50, ApplyStateBlock = 54, CaptureStateBlock = 55,
              DeleteStateBlock = 56, CreateStateBlock = 57, SetTexture = 61, SetTextureStageState = 63,
              DrawPrimitiveUP = 72, SetVertexShader = 76, SetPixelShader = 88;
}
constexpr int Release = 2, Surface_GetDesc = 8, Tex_LockRect = 16, Tex_UnlockRect = 17;

struct Vertex
{
    float x, y, z, rhw;
    u32 color;
    float u, v;
};
constexpr u32 kFvf = 0x004 | 0x040 | 0x100; // XYZRHW | DIFFUSE | TEX1

static void* g_dev = nullptr;
static u32 g_stateBlock = 0;
static const Font* g_font = nullptr;
static std::vector<Vertex> g_verts;

void SetDevice(void* device)
{
    if (device != g_dev)
        g_stateBlock = 0;
    g_dev = device;
}

static void* g_grabSurface = nullptr; // GrabRegion's staging surface (below)
static int g_grabW = 0, g_grabH = 0;

// Before every device Reset: the state block, and GrabRegion's staging surface (CreateImageSurface; kept between grabs,
// recreated on the next one) - nothing of FLvibed's but MANAGED textures outlives a Reset (26 Oct 2026, the Alt+Enter
// freeze audit; the surface is system memory, released anyway so no wrapper's pool mapping can make it count).
void OnDeviceLost()
{
    if (g_dev && g_stateBlock)
        VCall<HRESULT>(g_dev, dev::DeleteStateBlock, g_stateBlock);
    g_stateBlock = 0;
    if (g_grabSurface)
        VCall<ULONG>(g_grabSurface, Release);
    g_grabSurface = nullptr, g_grabW = g_grabH = 0;
}

void OnDeviceReset() {}

// Is the back-buffer readback (GrabRegion) safe on this device? Only on the system's own Direct3D 8 (the device's code in
// the Windows system directory). Under a D3D8 -> D3D9 wrapper in the game folder (HD Edition's DxWrapper / d3d8to9) one
// CopyRects from the back buffer leaves something of D3D9's default pool behind: every later device Reset fails
// D3DERR_INVALIDCALL for good, so a mode change or Alt+Enter would hang the game (26 Oct 2026, bisected with the test
// host's `d3dreset` on HDE 0.7.1: with the subtitles' comm-portrait capture the game's own recovery failed after the
// first grab, 4 of 4 runs; with only CopyRects skipped it recovered 3 of 3, controller alone 10 of 10). Once per device.
bool ReadbackSafe()
{
    static void* checked = nullptr;
    static bool safe = false;
    if (!g_dev)
        return false;
    if (checked == g_dev)
        return safe;
    checked = g_dev;
    safe = false;
    void* code = (*static_cast<void***>(g_dev))[0]; // QueryInterface: the device implementation's module
    HMODULE m = nullptr;
    char path[MAX_PATH] = {}, sys[MAX_PATH] = {}, wow[MAX_PATH] = {};
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           static_cast<LPCSTR>(code), &m) &&
        GetModuleFileNameA(m, path, MAX_PATH))
    {
        if (char* slash = strrchr(path, '\\'))
            *slash = 0;
        GetSystemDirectoryA(sys, MAX_PATH);
        GetSystemWow64DirectoryA(wow, MAX_PATH);
        safe = (sys[0] && _stricmp(path, sys) == 0) || (wow[0] && _stricmp(path, wow) == 0);
    }
    Log("render: back-buffer readback %s (device code in %s)", safe ? "on" : "off - not the system's Direct3D 8",
        path[0] ? path : "?");
    return safe;
}

// Copies a region of the back buffer into 0xAARRGGBB pixels (research captures at the game's own frame rate: every
// Present, out of focus). D3D8: GetBackBuffer -> CreateImageSurface (system memory) -> CopyRects -> LockRect.
// Only while the device is operational (TestCooperativeLevel D3D_OK). Players' features ask ReadbackSafe() first.
bool GrabRegion(int x, int y, int w, int h, std::vector<u32>& out)
{
    if (!g_dev || w <= 0 || h <= 0)
        return false;
    constexpr int TestCooperativeLevel = 3;
    if (FAILED(VCall<HRESULT>(g_dev, TestCooperativeLevel)))
        return false;
    constexpr int CreateImageSurface = 27, CopyRects = 28, Surf_LockRect = 9, Surf_UnlockRect = 10;
    void*& sys = g_grabSurface;
    int &sysW = g_grabW, &sysH = g_grabH;
    void* bb = nullptr;
    if (FAILED(VCall<HRESULT>(g_dev, dev::GetBackBuffer, 0u, 0u /*MONO*/, &bb)) || !bb)
        return false;
    u32 desc[8] = {};
    VCall<HRESULT>(bb, Surface_GetDesc, static_cast<void*>(desc));
    bool ok = false;
    if (desc[0] == 21 /*A8R8G8B8*/ || desc[0] == 22 /*X8R8G8B8*/)
    {
        if (!sys || sysW != w || sysH != h)
        {
            if (sys)
                VCall<ULONG>(sys, Release);
            sys = nullptr;
            VCall<HRESULT>(g_dev, CreateImageSurface, static_cast<UINT>(w), static_cast<UINT>(h), desc[0], &sys);
            sysW = w, sysH = h;
        }
        const RECT r = {x, y, x + w, y + h};
        const POINT p = {0, 0};
        if (sys && SUCCEEDED(VCall<HRESULT>(g_dev, CopyRects, bb, &r, 1u, sys, &p)))
        {
            struct
            {
                int pitch;
                u8* bits;
            } lr = {};
            if (SUCCEEDED(VCall<HRESULT>(sys, Surf_LockRect, static_cast<void*>(&lr), static_cast<const RECT*>(nullptr),
                                         0x10u /*READONLY*/)))
            {
                out.resize(static_cast<size_t>(w) * h);
                for (int yy = 0; yy < h; ++yy)
                    memcpy(&out[static_cast<size_t>(yy) * w], lr.bits + static_cast<size_t>(yy) * lr.pitch,
                           static_cast<size_t>(w) * 4);
                VCall<HRESULT>(sys, Surf_UnlockRect);
                ok = true;
            }
        }
    }
    VCall<ULONG>(bb, Release);
    return ok;
}

bool Viewport(int& x, int& y, int& w, int& h)
{
    x = y = w = h = 0;
    if (!g_dev)
        return false;
    struct
    {
        u32 x, y, w, h;
        float minZ, maxZ;
    } vp = {};
    if (FAILED(VCall<HRESULT>(g_dev, dev::GetViewport, static_cast<void*>(&vp))))
        return false;
    x = static_cast<int>(vp.x), y = static_cast<int>(vp.y), w = static_cast<int>(vp.w), h = static_cast<int>(vp.h);
    return true;
}

bool ScreenSize(int& w, int& h)
{
    w = h = 0;
    if (!g_dev)
        return false;
    void* surf = nullptr;
    if (FAILED(VCall<HRESULT>(g_dev, dev::GetBackBuffer, 0u, 0u, &surf)) || !surf)
        return false;
    u32 desc[8] = {}; // D3DSURFACE_DESC (d3d8): Format, Type, Usage, Pool, Size, MultiSample, Width, Height
    const HRESULT hr = VCall<HRESULT>(surf, Surface_GetDesc, static_cast<void*>(desc));
    VCall<ULONG>(surf, Release);
    if (FAILED(hr))
        return false;
    w = static_cast<int>(desc[6]);
    h = static_cast<int>(desc[7]);
    return w > 0 && h > 0;
}

void FreeFont(Font& font)
{
    if (font.texture)
        VCall<ULONG>(font.texture, Release);
    font.texture = nullptr;
    font.glyphs.clear();
}

bool BuildFont(Font& font, const wchar_t* face, int px, int weight, const std::wstring& chars)
{
    FreeFont(font);
    if (!g_dev || px < 4)
        return false;
    font.face = face;
    font.px = px;
    font.weight = weight;

    HDC dc = CreateCompatibleDC(nullptr);
    HFONT hf = CreateFontW(-px, 0, 0, 0, weight, 0, 0, 0, DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                           ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS, face);
    if (!dc || !hf)
    {
        if (dc)
            DeleteDC(dc);
        return false;
    }
    TEXTMETRICW tm = {};
    HGDIOBJ oldFont = SelectObject(dc, hf);
    // GDI substitutes a missing face silently: refuse it, so the caller can fall back on purpose
    wchar_t got[LF_FACESIZE] = {};
    GetTextFaceW(dc, LF_FACESIZE, got);
    if (_wcsicmp(got, face) != 0)
    {
        Log("render: font %ls not available (GDI gave %ls)", face, got);
        SelectObject(dc, oldFont);
        DeleteObject(hf);
        DeleteDC(dc);
        return false;
    }
    GetTextMetricsW(dc, &tm);
    font.ascent = tm.tmAscent;
    font.lineHeight = tm.tmHeight;

    // One glyph cell, rendered white on black; coverage = red channel.
    const int pad = px, cellW = px * 4, cellH = tm.tmHeight + 4;
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = cellW;
    bi.bmiHeader.biHeight = -cellH;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HGDIOBJ oldBmp = SelectObject(dc, bmp);
    SetTextColor(dc, RGB(255, 255, 255));
    SetBkColor(dc, RGB(0, 0, 0));
    SetBkMode(dc, OPAQUE);

    struct Raster
    {
        wchar_t c;
        Glyph g;
        std::vector<u8> a;
    };
    std::vector<Raster> rs;
    std::wstring set = chars;
    set += L'?';
    for (wchar_t c : set)
    {
        bool dup = false;
        for (const Raster& r : rs)
            dup |= r.c == c;
        if (dup)
            continue;
        memset(bits, 0, static_cast<size_t>(cellW) * cellH * 4);
        TextOutW(dc, pad, 0, &c, 1);
        GdiFlush();
        SIZE sz = {};
        GetTextExtentPoint32W(dc, &c, 1, &sz);
        int x0 = cellW, y0 = cellH, x1 = -1, y1 = -1;
        const u32* px32 = static_cast<const u32*>(bits);
        for (int y = 0; y < cellH; ++y)
            for (int x = 0; x < cellW; ++x)
                if (px32[y * cellW + x] & 0xFF)
                {
                    x0 = std::min(x0, x), x1 = std::max(x1, x);
                    y0 = std::min(y0, y), y1 = std::max(y1, y);
                }
        Raster r;
        r.c = c;
        r.g = {};
        r.g.advance = sz.cx;
        if (x1 >= x0)
        {
            r.g.w = x1 - x0 + 1;
            r.g.h = y1 - y0 + 1;
            r.g.ox = x0 - pad;
            r.g.oy = y0;
            r.a.resize(static_cast<size_t>(r.g.w) * r.g.h);
            for (int y = 0; y < r.g.h; ++y)
                for (int x = 0; x < r.g.w; ++x)
                    r.a[y * r.g.w + x] = static_cast<u8>(px32[(y0 + y) * cellW + x0 + x] & 0xFF);
        }
        rs.push_back(std::move(r));
    }
    SelectObject(dc, oldBmp);
    SelectObject(dc, oldFont);
    DeleteObject(bmp);
    DeleteObject(hf);
    DeleteDC(dc);

    // Shelf packing; a 4x4 white block first (solid boxes use it).
    const int texW = px > 100 ? 4096 : px > 40 ? 2048 : 1024;
    int x = 6, y = 0, rowH = 4;
    for (Raster& r : rs)
    {
        if (x + r.g.w + 2 > texW)
            x = 0, y += rowH + 2, rowH = 0;
        r.g.u0 = static_cast<float>(x); // pixel coordinates for now
        r.g.v0 = static_cast<float>(y);
        x += r.g.w + 2;
        rowH = std::max(rowH, r.g.h);
    }
    int texH = 64;
    while (texH < y + rowH + 2)
        texH *= 2;
    if (texH > 4096)
    {
        Log("render: font %d px does not fit the atlas", px);
        return false;
    }

    void* tex = nullptr;
    HRESULT hr = VCall<HRESULT>(g_dev, dev::CreateTexture, static_cast<UINT>(texW), static_cast<UINT>(texH), 1u, 0u,
                                21u /*A8R8G8B8*/, 1u /*MANAGED*/, &tex);
    if (FAILED(hr) || !tex)
    {
        Log("render: CreateTexture %dx%d failed 0x%08X", texW, texH, hr);
        return false;
    }
    struct
    {
        int pitch;
        u8* bits;
    } lr = {};
    if (FAILED(VCall<HRESULT>(tex, Tex_LockRect, 0u, static_cast<void*>(&lr), static_cast<const RECT*>(nullptr), 0u)))
    {
        VCall<ULONG>(tex, Release);
        return false;
    }
    for (int row = 0; row < texH; ++row)
        memset(lr.bits + static_cast<size_t>(row) * lr.pitch, 0, static_cast<size_t>(texW) * 4);
    for (int yy = 0; yy < 4; ++yy)
        for (int xx = 0; xx < 4; ++xx)
            reinterpret_cast<u32*>(lr.bits + static_cast<size_t>(yy) * lr.pitch)[xx] = 0xFFFFFFFF;
    font.coverage.assign(static_cast<size_t>(texW) * texH, 0); // CPU copy for the software target
    for (Raster& r : rs)
    {
        const int gx = static_cast<int>(r.g.u0), gy = static_cast<int>(r.g.v0);
        for (int yy = 0; yy < r.g.h; ++yy)
        {
            u32* dst = reinterpret_cast<u32*>(lr.bits + static_cast<size_t>(gy + yy) * lr.pitch) + gx;
            for (int xx = 0; xx < r.g.w; ++xx)
            {
                const u8 a = r.a[yy * r.g.w + xx];
                dst[xx] = (static_cast<u32>(a) << 24) | 0x00FFFFFF;
                font.coverage[static_cast<size_t>(gy + yy) * texW + gx + xx] = a;
            }
        }
        r.g.u0 = gx / static_cast<float>(texW);
        r.g.v0 = gy / static_cast<float>(texH);
        r.g.u1 = (gx + r.g.w) / static_cast<float>(texW);
        r.g.v1 = (gy + r.g.h) / static_cast<float>(texH);
        font.glyphs[r.c] = r.g;
    }
    VCall<HRESULT>(tex, Tex_UnlockRect, 0u);
    font.texture = tex;
    font.texW = texW;
    font.texH = texH;
    font.whiteU = 2.0f / texW;
    font.whiteV = 2.0f / texH;
    Log("render: font %ls %d px (weight %d), %zu glyphs, atlas %dx%d", face, px, weight, font.glyphs.size(), texW,
        texH);
    return true;
}

static const Glyph* GlyphOf(const Font& f, wchar_t c)
{
    auto it = f.glyphs.find(c);
    if (it == f.glyphs.end())
        it = f.glyphs.find(L'?');
    return it == f.glyphs.end() ? nullptr : &it->second;
}

int TextWidth(const Font& font, const std::wstring& s)
{
    int w = 0;
    for (wchar_t c : s)
        if (const Glyph* g = GlyphOf(font, c))
            w += g->advance;
    return w;
}

// Software target: premultiplied BGRA pixels (for UpdateLayeredWindow), drawn directly by Box/Text.
static u32* g_soft = nullptr;
static int g_softW = 0, g_softH = 0;

// Draw batches: each font's atlas is one texture; a SetFont or Sprite starts a new batch (drawn in order at End).
struct Batch
{
    void* texture;
    size_t first;          // first vertex
    bool additive = false; // SRCALPHA / ONE
    bool linear = false;   // linear filtering (sprites); point for 1:1 text
};
static std::vector<Batch> g_batches;

bool Begin(const Font& font)
{
    if (!g_dev || !font.texture)
        return false;
    g_font = &font;
    g_soft = nullptr;
    g_verts.clear();
    g_batches.assign(1, {font.texture, 0});
    return true;
}

static Image MakeImage(const u32* px, int w, int h)
{
    Image img;
    void* tex = nullptr;
    if (!g_dev ||
        FAILED(VCall<HRESULT>(g_dev, dev::CreateTexture, static_cast<UINT>(w), static_cast<UINT>(h), 1u, 0u,
                              21u /*A8R8G8B8*/, 1u /*MANAGED*/, &tex)) ||
        !tex)
        return img;
    struct
    {
        int pitch;
        u8* bits;
    } lr = {};
    if (FAILED(VCall<HRESULT>(tex, Tex_LockRect, 0u, static_cast<void*>(&lr), static_cast<const RECT*>(nullptr), 0u)))
    {
        VCall<ULONG>(tex, Release);
        return img;
    }
    for (int y = 0; y < h; ++y)
        memcpy(lr.bits + static_cast<size_t>(y) * lr.pitch, px + static_cast<size_t>(y) * w,
               static_cast<size_t>(w) * 4);
    VCall<HRESULT>(tex, Tex_UnlockRect, 0u);
    img.texture = tex, img.device = g_dev, img.w = w, img.h = h;
    return img;
}

bool BuildImage(Image& img, const std::vector<u32>& argb, int w, int h)
{
    FreeImage(img);
    if (w <= 0 || h <= 0 || argb.size() < static_cast<size_t>(w) * h)
        return false;
    img = MakeImage(argb.data(), w, h);
    return img.texture != nullptr;
}

void FreeImage(Image& img)
{
    if (img.texture && img.device == g_dev)
        VCall<ULONG>(img.texture, Release);
    img = Image();
}

// The plain pass: a white 4x4 "font" without glyphs (Box / Gradient need a white texel).
static Font g_plain;
static void* g_plainDev = nullptr;
bool BeginPlain()
{
    if (!g_dev)
        return false;
    if (!g_plain.texture || g_plainDev != g_dev)
    {
        const u32 white[16] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
                               0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
                               0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
        const Image img = MakeImage(white, 4, 4);
        if (!img.texture)
            return false;
        g_plain.texture = img.texture, g_plain.texW = g_plain.texH = 4;
        g_plain.whiteU = g_plain.whiteV = 0.5f;
        g_plainDev = g_dev;
    }
    return Begin(g_plain);
}

void Sprite(const Image& img, float cx, float cy, float w, float h, float angle, u32 argb, bool additive)
{
    if (!g_font || g_soft || !img.texture || img.device != g_dev)
        return;
    g_batches.push_back({img.texture, g_verts.size(), additive, true});
    const float c = std::cos(angle), s = std::sin(angle), hw = w * 0.5f, hh = h * 0.5f;
    auto at = [&](float dx, float dy, float u, float v)
    { return Vertex{cx + dx * c - dy * s - 0.5f, cy + dx * s + dy * c - 0.5f, 0, 1, argb, u, v}; };
    const Vertex a = at(-hw, -hh, 0, 0), b = at(hw, -hh, 1, 0), d = at(hw, hh, 1, 1), e = at(-hw, hh, 0, 1);
    g_verts.insert(g_verts.end(), {a, b, d, a, d, e});
    g_batches.push_back({g_font->texture, g_verts.size()}); // following Box / Text calls
}

void Quad(const Image& img, const float xy[8], const float uv[8], const u32 argb[4], bool additive)
{
    if (!g_font || g_soft || !img.texture || img.device != g_dev)
        return;
    g_batches.push_back({img.texture, g_verts.size(), additive, true});
    auto at = [&](int i)
    { return Vertex{xy[i * 2] - 0.5f, xy[i * 2 + 1] - 0.5f, 0, 1, argb[i], uv[i * 2], uv[i * 2 + 1]}; };
    const Vertex a = at(0), b = at(1), c = at(2), d = at(3);
    g_verts.insert(g_verts.end(), {a, b, c, a, c, d});
    g_batches.push_back({g_font->texture, g_verts.size()}); // following Box / Text calls
}

bool SetFont(const Font& font)
{
    if (!g_font || (g_soft ? font.coverage.empty() : !font.texture))
        return false;
    g_font = &font;
    if (!g_soft)
    {
        if (g_batches.back().first == g_verts.size())
            g_batches.back().texture = font.texture;
        else
            g_batches.push_back({font.texture, g_verts.size()});
    }
    return true;
}

bool BeginSoftware(const Font& font, u32* pixels, int w, int h)
{
    if (font.coverage.empty() || !pixels)
        return false;
    g_font = &font;
    g_soft = pixels, g_softW = w, g_softH = h;
    g_verts.clear();
    return true;
}

// src over dst in premultiplied ARGB; argb is straight colour with alpha, scaled by coverage (0..255).
static void Blend(int x, int y, u32 argb, u32 cov)
{
    if (x < 0 || y < 0 || x >= g_softW || y >= g_softH)
        return;
    const u32 a = ((argb >> 24) * cov + 127) / 255;
    if (!a)
        return;
    u32& d = g_soft[static_cast<size_t>(y) * g_softW + x];
    const u32 inv = 255 - a;
    const u32 r = (((argb >> 16) & 0xFF) * a + ((d >> 16) & 0xFF) * inv + 127) / 255;
    const u32 g = (((argb >> 8) & 0xFF) * a + ((d >> 8) & 0xFF) * inv + 127) / 255;
    const u32 b = ((argb & 0xFF) * a + (d & 0xFF) * inv + 127) / 255;
    const u32 oa = a + ((d >> 24) * inv + 127) / 255;
    d = (oa << 24) | (r << 16) | (g << 8) | b;
}

static void Quad(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, u32 c)
{
    x0 -= 0.5f, y0 -= 0.5f, x1 -= 0.5f, y1 -= 0.5f; // texel centers (D3D8 rasterization rules)
    const Vertex a = {x0, y0, 0, 1, c, u0, v0}, b = {x1, y0, 0, 1, c, u1, v0}, d = {x1, y1, 0, 1, c, u1, v1},
                 e = {x0, y1, 0, 1, c, u0, v1};
    g_verts.insert(g_verts.end(), {a, b, d, a, d, e});
}

void Box(float x, float y, float w, float h, u32 argb)
{
    if (!g_font)
        return;
    if (g_soft)
    {
        for (int yy = static_cast<int>(y); yy < static_cast<int>(y + h); ++yy)
            for (int xx = static_cast<int>(x); xx < static_cast<int>(x + w); ++xx)
                Blend(xx, yy, argb, 255);
        return;
    }
    Quad(x, y, x + w, y + h, g_font->whiteU, g_font->whiteV, g_font->whiteU, g_font->whiteV, argb);
}

void Gradient(float x, float y, float w, float h, u32 tl, u32 tr, u32 br, u32 bl)
{
    if (!g_font)
        return;
    if (g_soft)
    {
        auto mix = [](u32 a, u32 b, float t)
        {
            u32 o = 0;
            for (int s = 0; s < 32; s += 8)
                o |= static_cast<u32>(((a >> s) & 0xFF) * (1 - t) + ((b >> s) & 0xFF) * t + 0.5f) << s;
            return o;
        };
        for (int yy = static_cast<int>(y); yy < static_cast<int>(y + h); ++yy)
        {
            const float v = h > 1 ? (yy - y) / (h - 1) : 0;
            const u32 l = mix(tl, bl, v), r = mix(tr, br, v);
            for (int xx = static_cast<int>(x); xx < static_cast<int>(x + w); ++xx)
                Blend(xx, yy, mix(l, r, w > 1 ? (xx - x) / (w - 1) : 0), 255);
        }
        return;
    }
    const float u = g_font->whiteU, v = g_font->whiteV;
    const float x0 = x - 0.5f, y0 = y - 0.5f, x1 = x + w - 0.5f, y1 = y + h - 0.5f;
    const Vertex a = {x0, y0, 0, 1, tl, u, v}, b = {x1, y0, 0, 1, tr, u, v}, d = {x1, y1, 0, 1, br, u, v},
                 e = {x0, y1, 0, 1, bl, u, v};
    g_verts.insert(g_verts.end(), {a, b, d, a, d, e});
}

void Text(float x, float y, const std::wstring& s, u32 argb)
{
    if (!g_font)
        return;
    float pen = std::floor(x);
    y = std::floor(y);
    for (wchar_t c : s)
    {
        const Glyph* g = GlyphOf(*g_font, c);
        if (!g)
            continue;
        if (g->w > 0)
        {
            if (g_soft)
            {
                const int gx = static_cast<int>(g->u0 * g_font->texW + 0.5f),
                          gy = static_cast<int>(g->v0 * g_font->texH + 0.5f);
                for (int yy = 0; yy < g->h; ++yy)
                    for (int xx = 0; xx < g->w; ++xx)
                        Blend(static_cast<int>(pen) + g->ox + xx, static_cast<int>(y) + g->oy + yy, argb,
                              g_font->coverage[static_cast<size_t>(gy + yy) * g_font->texW + gx + xx]);
            }
            else
                Quad(pen + g->ox, y + g->oy, pen + g->ox + g->w, y + g->oy + g->h, g->u0, g->v0, g->u1, g->v1, argb);
        }
        pen += g->advance;
    }
}

void TextScaled(float x, float y, const std::wstring& s, u32 argb, float scale)
{
    if (!g_font)
        return;
    if (g_soft || scale == 1.0f) // the software target draws at the font's own size
    {
        Text(x, y, s, argb);
        return;
    }
    // linear filtering, no pixel snapping: any size between the atlas's (the glyphs keep 2 texels apart in it)
    g_batches.push_back({g_font->texture, g_verts.size(), false, true});
    float pen = x;
    for (wchar_t c : s)
    {
        const Glyph* g = GlyphOf(*g_font, c);
        if (!g)
            continue;
        if (g->w > 0)
        {
            const float x0 = pen + g->ox * scale, y0 = y + g->oy * scale;
            Quad(x0, y0, x0 + g->w * scale, y0 + g->h * scale, g->u0, g->v0, g->u1, g->v1, argb);
        }
        pen += g->advance * scale;
    }
    g_batches.push_back({g_font->texture, g_verts.size()}); // following Text calls: point-filtered again
}

static void SetRS(u32 state, u32 value) { VCall<HRESULT>(g_dev, dev::SetRenderState, state, value); }
static void SetTSS(u32 stage, u32 type, u32 value)
{
    VCall<HRESULT>(g_dev, dev::SetTextureStageState, stage, type, value);
}

void End(bool ownScene)
{
    if (g_soft) // software target: already drawn
    {
        g_soft = nullptr;
        g_font = nullptr;
        return;
    }
    if (!g_dev || !g_font || g_verts.empty())
    {
        g_font = nullptr;
        return;
    }
    if (!g_stateBlock && FAILED(VCall<HRESULT>(g_dev, dev::CreateStateBlock, 1u /*D3DSBT_ALL*/, &g_stateBlock)))
        g_stateBlock = 0;
    if (g_stateBlock)
        VCall<HRESULT>(g_dev, dev::CaptureStateBlock, g_stateBlock);

    int sw = 0, sh = 0;
    ScreenSize(sw, sh);
    struct
    {
        u32 x, y, w, h;
        float minZ, maxZ;
    } vp = {0, 0, static_cast<u32>(sw), static_cast<u32>(sh), 0.0f, 1.0f};

    if (!ownScene || SUCCEEDED(VCall<HRESULT>(g_dev, dev::BeginScene)))
    {
        if (sw > 0)
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
        SetRS(168, 0xF); // COLORWRITEENABLE
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
        SetTSS(0, 16, 1); // MAGFILTER point (1:1 texels)
        SetTSS(0, 17, 1); // MINFILTER point
        SetTSS(0, 18, 0); // MIPFILTER none
        SetTSS(1, 1, 1);  // stage 1 COLOROP disable
        SetTSS(1, 4, 1);  // stage 1 ALPHAOP disable
        for (size_t i = 0; i < g_batches.size(); ++i)
        {
            const size_t first = g_batches[i].first;
            const size_t last = i + 1 < g_batches.size() ? g_batches[i + 1].first : g_verts.size();
            if (last <= first)
                continue;
            SetRS(20, g_batches[i].additive ? 2 : 6);   // DESTBLEND one / invsrcalpha
            SetTSS(0, 16, g_batches[i].linear ? 2 : 1); // MAGFILTER linear / point
            SetTSS(0, 17, g_batches[i].linear ? 2 : 1); // MINFILTER
            VCall<HRESULT>(g_dev, dev::SetTexture, 0u, g_batches[i].texture);
            VCall<HRESULT>(g_dev, dev::DrawPrimitiveUP, 4u /*TRIANGLELIST*/, static_cast<UINT>((last - first) / 3),
                           static_cast<const void*>(g_verts.data() + first), static_cast<UINT>(sizeof(Vertex)));
        }
        if (ownScene)
            VCall<HRESULT>(g_dev, dev::EndScene);
    }
    if (g_stateBlock)
        VCall<HRESULT>(g_dev, dev::ApplyStateBlock, g_stateBlock);
    g_font = nullptr;
}
} // namespace render
