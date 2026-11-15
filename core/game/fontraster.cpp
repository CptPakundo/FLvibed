// The font half of the overlay renderer with no device: GDI rasterising, atlas packing, glyph lookup (fontraster.h).
#include "fontraster.h"

#include "glyphset.h"
#include "log.h"

#include <vector>

namespace render
{
bool RasterizeFont(Raster& out, const wchar_t* face, int px, int weight, const std::wstring& chars)
{
    out = Raster();
    if (px < 4)
        return false;
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
    out.ascent = tm.tmAscent;
    out.lineHeight = tm.tmHeight;
    OUTLINETEXTMETRICW otm = {};
    otm.otmSize = sizeof(otm);
    if (GetOutlineTextMetricsW(dc, sizeof(otm), &otm)) // (the fixed part only: no strings asked for)
        out.underPos = otm.otmsUnderscorePosition, out.underSize = static_cast<int>(otm.otmsUnderscoreSize);
    out.metrics = true;

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
    if (!bmp || !bits) // GDI out of resources (or a huge px): no font rather than a write through null
    {
        Log("render: font %ls: no glyph bitmap (CreateDIBSection failed, px %d)", face, px);
        if (bmp)
            DeleteObject(bmp);
        SelectObject(dc, oldFont);
        DeleteObject(hf);
        DeleteDC(dc);
        return false;
    }
    HGDIOBJ oldBmp = SelectObject(dc, bmp);
    SetTextColor(dc, RGB(255, 255, 255));
    SetBkColor(dc, RGB(0, 0, 0));
    SetBkMode(dc, OPAQUE);

    struct Cell
    {
        wchar_t c;
        Glyph g;
        std::vector<u8> a;
    };
    std::vector<Cell> rs;
    std::wstring set = chars;
    set += L'?';
    // The box the face draws for a character neither it nor GDI's fallback holds (U+FFFF: no font's): such a character
    // stays out of the atlas, so GlyphFor gives its stand-in. A character the face lacks but GDI found elsewhere stays.
    const size_t cellBytes = static_cast<size_t>(cellW) * cellH * 4;
    const wchar_t noChar = 0xFFFF;
    memset(bits, 0, cellBytes);
    TextOutW(dc, pad, 0, &noChar, 1);
    GdiFlush();
    const std::vector<u8> box(static_cast<const u8*>(bits), static_cast<const u8*>(bits) + cellBytes);
    std::vector<WORD> index(set.size());
    const bool indexed = GetGlyphIndicesW(dc, set.c_str(), static_cast<int>(set.size()), index.data(),
                                          GGI_MARK_NONEXISTING_GLYPHS) == set.size();
    for (size_t i = 0; i < set.size(); ++i)
    {
        const wchar_t c = set[i];
        bool dup = false;
        for (const Cell& r : rs)
            dup |= r.c == c;
        if (dup)
            continue;
        memset(bits, 0, cellBytes);
        TextOutW(dc, pad, 0, &c, 1);
        GdiFlush();
        if (indexed && index[i] == 0xFFFF && c != L'?' && memcmp(bits, box.data(), cellBytes) == 0)
            continue;
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
        Cell r;
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

    // Shelf packing; a 4x4 white block first (solid boxes use it: the texture's, render::BuildFont).
    const int texW = px > 100 ? 4096 : px > 40 ? 2048 : 1024;
    int x = 6, y = 0, rowH = 4;
    for (Cell& r : rs)
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

    out.texW = texW, out.texH = texH;
    out.coverage.assign(static_cast<size_t>(texW) * texH, 0);
    out.glyphs.reserve(rs.size());
    for (Cell& r : rs)
    {
        const int gx = static_cast<int>(r.g.u0), gy = static_cast<int>(r.g.v0);
        for (int yy = 0; yy < r.g.h; ++yy)
            for (int xx = 0; xx < r.g.w; ++xx)
                out.coverage[static_cast<size_t>(gy + yy) * texW + gx + xx] = r.a[yy * r.g.w + xx];
        r.g.u0 = gx / static_cast<float>(texW);
        r.g.v0 = gy / static_cast<float>(texH);
        r.g.u1 = (gx + r.g.w) / static_cast<float>(texW);
        r.g.v1 = (gy + r.g.h) / static_cast<float>(texH);
        out.glyphs.push_back({r.c, r.g, gx, gy});
    }
    return true;
}

static std::wstring g_unmet;

const std::wstring& Unmet() { return g_unmet; }

const Glyph* GlyphFor(const Font& f, wchar_t c)
{
    const auto own = f.glyphs.find(c);
    if (own != f.glyphs.end())
        return &own->second;
    if (f.glyphs.empty())
        return nullptr;
    if (c >= 32 && g_unmet.size() < 256 && g_unmet.find(c) == std::wstring::npos)
    {
        g_unmet += c;
        Log("render: U+%04X not in the atlas of %ls - added at the next build", static_cast<unsigned>(c), f.face.c_str());
    }
    const wchar_t drawn = glyphset::Resolve(c, [&](wchar_t ch) { return f.glyphs.count(ch) != 0; });
    return drawn ? &f.glyphs.find(drawn)->second : nullptr;
}

// (moved here from render.cpp's BuildFont and FontDump unchanged, bench B2: the bench writes the same image and print)
void AtlasArgb(const Raster& r, u32* dst, size_t pitchPx)
{
    for (int row = 0; row < r.texH; ++row)
        memset(dst + static_cast<size_t>(row) * pitchPx, 0, static_cast<size_t>(r.texW) * 4);
    for (int yy = 0; yy < 4; ++yy)
        for (int xx = 0; xx < 4; ++xx)
            dst[static_cast<size_t>(yy) * pitchPx + xx] = 0xFFFFFFFF;
    // each glyph's box white with its coverage as alpha (the rest of the atlas 0)
    for (const RasterGlyph& g : r.glyphs)
        for (int yy = 0; yy < g.g.h; ++yy)
        {
            u32* d = dst + static_cast<size_t>(g.y + yy) * pitchPx + g.x;
            const u8* a = &r.coverage[static_cast<size_t>(g.y + yy) * r.texW + g.x];
            for (int xx = 0; xx < g.g.w; ++xx)
                d[xx] = (static_cast<u32>(a[xx]) << 24) | 0x00FFFFFF;
        }
}

void FillFont(Font& font, Raster& r)
{
    for (const RasterGlyph& g : r.glyphs)
        font.glyphs[g.c] = g.g;
    font.coverage = std::move(r.coverage); // CPU copy for the software target
    font.texW = r.texW;
    font.texH = r.texH;
    font.underPos = r.underPos, font.underSize = r.underSize;
    font.whiteU = 2.0f / r.texW;
    font.whiteV = 2.0f / r.texH;
}

unsigned long long Fnv(const void* p, size_t n, unsigned long long h)
{
    const u8* b = static_cast<const u8*>(p);
    for (size_t i = 0; i < n; ++i)
        h = (h ^ b[i]) * 1099511628211ull;
    return h;
}

std::string FontPrint(const Font& f, unsigned long long texHash)
{
    std::string face;
    for (wchar_t c : f.face)
        face += c == L' ' ? '_' : c < 128 ? static_cast<char>(c) : '?';
    char b[320];
    snprintf(b, sizeof b, "font %s px=%d weight=%d ascent=%d line=%d atlas=%dx%d white=%.9g,%.9g glyphs=%zu cov=%016llx tex=%016llx\n",
             face.c_str(), f.px, f.weight, f.ascent, f.lineHeight, f.texW, f.texH, f.whiteU, f.whiteV, f.glyphs.size(),
             Fnv(f.coverage.data(), f.coverage.size()), texHash);
    std::string block = b;
    std::vector<wchar_t> cs;
    for (const auto& g : f.glyphs)
        cs.push_back(g.first);
    std::sort(cs.begin(), cs.end());
    for (wchar_t c : cs)
    {
        const Glyph& g = f.glyphs.at(c);
        snprintf(b, sizeof b, "  U+%04X adv=%d w=%d h=%d ox=%d oy=%d uv=%.9g,%.9g,%.9g,%.9g\n", static_cast<unsigned>(c),
                 g.advance, g.w, g.h, g.ox, g.oy, g.u0, g.v0, g.u1, g.v1);
        block += b;
    }
    return block;
}

int TextWidth(const Font& font, const std::wstring& s)
{
    int w = 0;
    for (wchar_t c : s)
        if (const Glyph* g = GlyphFor(font, c))
            w += g->advance;
    return w;
}
} // namespace render
