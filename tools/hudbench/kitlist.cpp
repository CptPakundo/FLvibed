// FLvibed design bench - the draw kit's list back end (kitlist.h).
#include "kitlist.h"

#include "game/flicons_decode.h"
#include "game/fontraster.h"
#include "game/gamefont.h"

namespace kitlist
{
static Source g_source = Source::Synthetic;
static Inputs g_in;
static bool g_gameFont = false;
static std::vector<std::unique_ptr<Texture>> g_textures;
static std::vector<Pass> g_passes;
static Texture* g_icons[2] = {}; // big, small
static int g_device = 0;

Source Current() { return g_source; }
Inputs& In() { return g_in; }
void* Device() { return &g_device; }
const std::vector<std::unique_ptr<Texture>>& Textures() { return g_textures; }
std::vector<Pass> Take() { return std::move(g_passes); }

static std::string Hex4(wchar_t c)
{
    char b[16];
    snprintf(b, sizeof b, "U+%04X", static_cast<unsigned>(c));
    return b;
}

// A set's name from what makes it: the face (letters and digits), px and weight; a later set of the same three (the kit
// rebuilds a set when a character is added) gets .2, .3, ... so a name always means one texture.
static std::string SetName(const std::wstring& face, int px, int weight)
{
    const std::string b = fldl::SetName(face, px, weight);
    int same = 0;
    for (const auto& t : g_textures)
        if (t->font && (t->set == b || t->set.rfind(std::string(b) + ".", 0) == 0))
            ++same;
    if (!same)
        return b;
    return std::string(b) + "." + std::to_string(same + 1);
}

// ---------------------------------------------------------------- the synthetic source
// Block glyphs: every character a solid box the width of its advance and the height of the line (so text reads as bars
// and a probe anywhere inside a run's box lands on ink), a space none; packed in rows after the 4 x 4 white block as
// fontraster packs (6 texels in, 2 between boxes). No font file, no real metric.
static bool SynthRaster(render::Raster& out, int px, const std::wstring& chars)
{
    const int adv = std::max(1, static_cast<int>(px * 0.55f + 0.5f)), line = std::max(1, static_cast<int>(px * 1.2f + 0.5f));
    out.metrics = true;
    out.ascent = static_cast<int>(px * 0.95f + 0.5f), out.lineHeight = line;
    std::wstring cs;
    for (wchar_t c : chars + L"?")
        if (cs.find(c) == std::wstring::npos)
            cs += c;
    const int texW = px > 100 ? 4096 : px > 40 ? 2048 : 1024;
    int x = 6, y = 0, rowH = 4;
    struct At
    {
        wchar_t c;
        int x, y, w, h;
    };
    std::vector<At> at;
    for (wchar_t c : cs)
    {
        const int w = c == L' ' ? 0 : adv, h = c == L' ' ? 0 : line;
        if (x + w + 2 > texW)
            x = 0, y += rowH + 2, rowH = 0;
        at.push_back({c, x, y, w, h});
        x += w + 2;
        rowH = std::max(rowH, h);
    }
    int texH = 64;
    while (texH < y + rowH + 2)
        texH *= 2;
    if (texH > 4096)
        return false;
    out.texW = texW, out.texH = texH;
    out.coverage.assign(static_cast<size_t>(texW) * texH, 0);
    for (const At& a : at)
    {
        render::RasterGlyph r;
        r.c = a.c, r.x = a.x, r.y = a.y;
        r.g.w = a.w, r.g.h = a.h, r.g.ox = 0, r.g.oy = 0, r.g.advance = adv;
        r.g.u0 = a.x / static_cast<float>(texW), r.g.v0 = a.y / static_cast<float>(texH);
        r.g.u1 = (a.x + a.w) / static_cast<float>(texW), r.g.v1 = (a.y + a.h) / static_cast<float>(texH);
        for (int yy = 0; yy < a.h; ++yy)
            memset(&out.coverage[static_cast<size_t>(a.y + yy) * texW + a.x], 255, static_cast<size_t>(a.w));
        out.glyphs.push_back(r);
    }
    return true;
}

// Disc icons: every cell an opaque white disc (radius 0.45 of the cell) on transparent, in the layout of
// flicons_decode's atlases (icon i at cell (i % kCols, i / kCols)).
static void SynthIcons(int cell, std::vector<u32>& px)
{
    const int w = flicons::kCols * cell, h = flicons::kRows * cell;
    px.assign(static_cast<size_t>(w) * h, 0);
    const float r = cell * 0.45f;
    for (int i = 0; i < flicons::Count; ++i)
    {
        const int cx = (i % flicons::kCols) * cell, cy = (i / flicons::kCols) * cell;
        for (int yy = 0; yy < cell; ++yy)
            for (int xx = 0; xx < cell; ++xx)
            {
                const float dx = xx + 0.5f - cell * 0.5f, dy = yy + 0.5f - cell * 0.5f;
                if (dx * dx + dy * dy <= r * r)
                    px[static_cast<size_t>(cy + yy) * w + cx + xx] = 0xFFFFFFFF;
            }
    }
}

static void MakeIcons(const std::vector<u32>& bigPx, const std::vector<u32>& smallPx)
{
    for (int k = 0; k < 2; ++k)
    {
        const int cell = k ? flicons::kSmall : flicons::kBig;
        auto t = std::make_unique<Texture>();
        fldl::IconAtlas(cell, *t);
        t->argb = k ? smallPx : bigPx;
        g_icons[k] = t.get();
        g_textures.push_back(std::move(t));
    }
}

int Open(Source source, const std::wstring& install, std::string& why)
{
    g_source = source;
    if (source == Source::Synthetic)
    {
        g_gameFont = true;
        std::vector<u32> bigPx, smallPx;
        SynthIcons(flicons::kBig, bigPx), SynthIcons(flicons::kSmall, smallPx);
        MakeIcons(bigPx, smallPx);
        return 0;
    }
    std::wstring dir = install;
    if (!dir.empty() && dir.back() != L'\\' && dir.back() != L'/')
        dir += L'\\';
    const std::wstring data = dir + L"DATA\\";
    if (GetFileAttributesW((data + L"FONTS\\fonts.ini").c_str()) == INVALID_FILE_ATTRIBUTES)
    {
        why = "no DATA\\FONTS\\fonts.ini under the install: not a Freelancer install";
        return 2;
    }
    g_gameFont = gamefont::LoadFrom(data);
    if (!g_gameFont || gamefont::LabelSource() == 0)
    {
        why = "the game's label face is missing (neither the install's font files nor Windows hold it): the game would "
              "draw with Segoe UI, the bench refuses";
        return 2;
    }
    flicons::Atlas a;
    bool icons = false;
    for (const wchar_t* f : flicons::kSheetFiles)
        if (flicons::ReadAtlas(data + L"INTERFACE\\" + f, a))
        {
            icons = true;
            break;
        }
    if (!icons)
    {
        why = "no flat icon sheet under DATA\\INTERFACE could be read";
        return 2;
    }
    MakeIcons(a.bigPx, a.smallPx);
    return 0;
}
} // namespace kitlist

// ---------------------------------------------------------------- the kit's back end (hud2_kit_backend.h)
namespace hud2kit
{
namespace backend
{
using kitlist::Texture;

bool GameFont() { return kitlist::g_gameFont; }
const wchar_t* LabelFace() { return kitlist::g_source == kitlist::Source::Synthetic ? L"synthetic" : gamefont::LabelFace(); }

void FreeFont(render::Font& font)
{
    // the texture stays (Textures(): a recorded pass names it); the font forgets it, as render::FreeFont
    font.texture = nullptr;
    font.glyphs.clear();
    font.coverage.clear();
}

bool BuildFont(render::Font& font, const wchar_t* face, int px, int weight, const std::wstring& chars)
{
    // render::BuildFont's steps, the upload replaced by an image in memory
    backend::FreeFont(font);
    if (px < 4)
        return false;
    font.face = face;
    font.px = px;
    font.weight = weight;
    render::Raster ras;
    const bool ok = kitlist::g_source == kitlist::Source::Synthetic ? kitlist::SynthRaster(ras, px, chars)
                                                                     : render::RasterizeFont(ras, face, px, weight, chars);
    if (ras.metrics)
        font.ascent = ras.ascent, font.lineHeight = ras.lineHeight;
    if (!ok)
        return false;
    auto t = std::make_unique<Texture>();
    t->font = true;
    t->set = kitlist::SetName(face, px, weight);
    t->w = ras.texW, t->h = ras.texH;
    t->argb.assign(static_cast<size_t>(t->w) * t->h, 0);
    render::AtlasArgb(ras, t->argb.data(), static_cast<size_t>(t->w));
    for (const render::RasterGlyph& r : ras.glyphs)
        if (r.g.w > 0 && r.g.h > 0)
            t->boxes.push_back({"glyph:" + t->set + ":" + kitlist::Hex4(r.c), r.x, r.y, r.g.w, r.g.h});
    render::FillFont(font, ras);
    t->face = face, t->px = px, t->weight = weight, t->ascent = font.ascent, t->lineHeight = font.lineHeight;
    t->source = kitlist::g_source == kitlist::Source::Synthetic ? 0 : gamefont::LabelSource();
    t->print = render::FontPrint(font, render::Fnv(t->argb.data(), t->argb.size() * 4));
    font.texture = t.get();
    kitlist::g_textures.push_back(std::move(t));
    return true;
}

bool IconUv(flicons::Icon icon, float sizePx, void** texture, float uv[4])
{
    if (icon < 0 || icon >= flicons::Count || !kitlist::g_icons[0] || !kitlist::g_in.icons)
        return false;
    const bool smallAtlas = flicons::SmallFor(sizePx);
    flicons::CellUv(icon, smallAtlas, uv);
    *texture = kitlist::g_icons[smallAtlas ? 1 : 0];
    return true;
}

bool Describe(const void* texture, fldl::Texture& out)
{
    for (const auto& t : kitlist::g_textures)
        if (t.get() == texture)
            return out = *t, true; // (the description only: the pixels stay)
    return false;
}

void Bind(void*) {}

void Submit(const Layer* layers, int count, int sw, int sh, bool keepDestAlpha, u32& calls)
{
    kitlist::Pass p;
    p.sw = sw, p.sh = sh, p.keepDestAlpha = keepDestAlpha;
    for (int li = 0; li < count && li < LCount; ++li)
    {
        const Layer& l = layers[li];
        if (l.v.empty() || !l.tex)
            continue; // (as the game: a layer without a texture is not drawn)
        p.v[li] = l.v;
        p.tex[li] = static_cast<const Texture*>(l.tex);
        ++calls;
    }
    p.marks = Marks();
    kitlist::g_passes.push_back(std::move(p));
}

void DeviceLost() {}
} // namespace backend
} // namespace hud2kit

// ---------------------------------------------------------------- the host reads the kit makes, as inputs
namespace glyphs
{
Family Current() { return kitlist::g_in.family; }
Look LookFor(bool hud2Surface) { return LookOf(kitlist::g_in.glyphLook, hud2Surface); }
bool FaceColours(Family f) { return FaceColoursOf(kitlist::g_in.glyphLook, f); }
} // namespace glyphs
