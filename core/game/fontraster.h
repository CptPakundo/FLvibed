// FLvibed - the font half of core\game\render that needs no Direct3D device: the glyph and font types, the rasteriser
// (a face drawn by Windows' GDI at an exact pixel size, each character's inked box packed into one atlas of coverage
// bytes) and the glyph lookup. render::BuildFont rasterises here and uploads the atlas as a texture; a program with no
// game and no device (the design bench, ..\..\..\audit_2026-11-12\71b_bench_design_v2.md section 3) rasterises the same
// way and gets the same boxes, advances and coverage. Split out of render.cpp unchanged (bench B1a, 12 Nov 2026).
#pragma once
#include "common.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace render
{
struct Glyph
{
    float u0, v0, u1, v1; // atlas coordinates
    int w, h;             // bitmap size in pixels
    int ox, oy;           // bitmap offset from the pen position (oy from the line top)
    int advance;
};

struct Font
{
    std::wstring face;
    int px = 0; // em height in pixels
    int weight = 0;
    int ascent = 0, lineHeight = 0;
    // The face's underline (its outline metrics, as GDI draws lfUnderline): the rule's offset above the baseline in pixels
    // (negative: below) and its thickness; 0 / 0 when the face gave none (the kit then takes 1/8 and 1/16 of the em).
    int underPos = 0, underSize = 0;
    std::unordered_map<wchar_t, Glyph> glyphs;
    void* texture = nullptr; // IDirect3DTexture8 (managed pool: survives Reset)
    int texW = 0, texH = 0;
    float whiteU = 0, whiteV = 0; // a fully covered texel, for solid boxes
    std::vector<u8> coverage;     // CPU copy of the atlas alpha (software target)
};

// One rasterised font, before any texture: what render::BuildFont uploads.
struct RasterGlyph
{
    wchar_t c;
    Glyph g;  // its UVs final (in the texW x texH atlas)
    int x, y; // its box's top-left texel in the atlas
};
struct Raster
{
    bool metrics = false; // the face resolved: ascent and lineHeight are set (also when a later step failed)
    int ascent = 0, lineHeight = 0;
    int underPos = 0, underSize = 0; // Font's: set with the metrics where the face has outline metrics
    int texW = 0, texH = 0;
    std::vector<RasterGlyph> glyphs; // in the order rasterised (the characters' order, '?' last, each once)
    std::vector<u8> coverage;        // texW x texH: each glyph's coverage at its box; 0 elsewhere (the 4 x 4 white block
                                     // at (0, 0) that the texture carries is not in it)
};

// Rasterises `chars` (plus '?') of `face` at `px` (the em) and `weight`, ANTIALIASED_QUALITY, coverage from the red
// channel of white on black; shelf-packs the inked boxes into an atlas 1024 / 2048 / 4096 wide (by px) after a 6-texel
// gap for the white block, 2 texels between boxes. A character neither the face nor GDI's fallback holds (it draws the
// face's empty box) is left out, so GlyphFor gives its stand-in. False (logged) when GDI substitutes another face, a
// glyph bitmap cannot be made, or the atlas would pass 4096 tall.
bool RasterizeFont(Raster& out, const wchar_t* face, int px, int weight, const std::wstring& chars);

// The atlas image render::BuildFont uploads (A8R8G8B8, rows `pitchPx` pixels apart): 0 everywhere, the 4 x 4 white
// block at (0, 0), each glyph's box white with its coverage as alpha. A program with no device keeps the same image.
void AtlasArgb(const Raster& r, u32* dst, size_t pitchPx);
// The font's glyph table, atlas size, white texel and coverage copy from `r` (its coverage moved out).
void FillFont(Font& font, Raster& r);
// FNV-1a 64 over bytes (the fingerprints' hashes).
unsigned long long Fnv(const void* p, size_t n, unsigned long long h = 1469598103934665603ull);
// One font's fingerprint block, the format of `hud2 fontdump` (render::FontDump): a line of the face, px, weight, metrics,
// atlas size, white texel, glyph count, the coverage's hash and `texHash` (the atlas image's), then one line per glyph
// (advance, box, offset, atlas UVs) in code point order. Equal blocks = the same raster.
std::string FontPrint(const Font& f, unsigned long long texHash);

int TextWidth(const Font& font, const std::wstring& s);
// The glyph drawn for `c`: its own, else glyphset::Resolve's stand-in; nullptr when the font holds neither. A character
// not in the atlas joins Unmet(), the characters each font's owner adds at its next build (at most 256, each once).
const Glyph* GlyphFor(const Font& font, wchar_t c);
const std::wstring& Unmet();
} // namespace render
