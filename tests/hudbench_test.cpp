// The design bench's list back end (tools\hudbench\kitlist.*, fldl_write.*) over HUD 2's real draw kit
// (core\ui\hud2_kit.cpp, compiled here with no game and no device): the kit's passes become display lists whose every
// triangle names the texture box it samples; a triangle that samples none fails the list; the under pass writes no
// alpha; the marks keep the layers' counts; an install with no game font is refused; game text's italic and underline
// (decision 56) are sheared quads and one rule. Synthetic source only (block glyphs,
// disc icons): no game text, art or font metric.
#include "test.h"

#include "../tools/hudbench/fldl_write.h"
#include "../tools/hudbench/kitlist.h"

static int Count(const std::string& s, const std::string& what)
{
    int n = 0;
    for (size_t at = s.find(what); at != std::string::npos; at = s.find(what, at + what.size()))
        ++n;
    return n;
}

void Test_hudbench()
{
    // an install without DATA\FONTS\fonts.ini: refused (exit 2), never drawn with a fallback face
    std::string why;
    const std::string empty = g_test.temp + "\\bench_empty";
    CreateDirectoryA(empty.c_str(), nullptr);
    CHECK(kitlist::Open(kitlist::Source::Install, std::wstring(empty.begin(), empty.end()), why) == 2 && !why.empty());

    CHECK(kitlist::Open(kitlist::Source::Synthetic, L"", why) == 0);
    void* dev = kitlist::Device();
    const int W = 1280, H = 800, px = 21;
    CHECK(hud2kit::Begin(dev, W, H, px));
    hud2kit::SetKeepDestAlpha();
    hud2kit::Rect(10, 10, 40, 40, 0xFF102030);
    hud2kit::End();
    CHECK(hud2kit::Begin(dev, W, H, px));
    hud2kit::Rect(100, 100, 50, 30, 0xFFFF0000);
    hud2kit::Mark("t.shapes");
    hud2kit::Text(100, 200, L"ABC", static_cast<float>(px), 0xFFFFFFFF, hud2kit::Left, 0, false);
    hud2kit::TextClipped(100, 260, L"CLIPPED", static_cast<float>(px), 104, 130, 0xFFFFFFFF);
    hud2kit::Icon(flicons::Cruise, 400, 400, 100, 0xFF00FF00);
    hud2kit::Icon(flicons::Dock, 500, 400, 30, 0xFF00FF00);
    hud2kit::Mark("t.end");
    hud2kit::End();
    std::vector<kitlist::Pass> passes = kitlist::Take();
    CHECK(passes.size() == 2);
    if (passes.size() != 2)
        return;
    const kitlist::Pass &under = passes[0], &over = passes[1];
    CHECK(under.keepDestAlpha && !over.keepDestAlpha);
    CHECK(under.v[hud2kit::LShape].size() == 6 && over.v[hud2kit::LShape].size() >= 6);
    // the kit's -0.5: a rectangle at whole pixels has its corners on halves
    CHECK_NEAR(over.v[hud2kit::LShape][0].x, 99.5, 1e-6);
    CHECK(over.v[hud2kit::LTextSmall].size() >= 18); // ABC's three glyphs, then the clipped text's
    CHECK(!over.v[hud2kit::LIconBig].empty() && !over.v[hud2kit::LIconSmall].empty());
    // marks: in order, the shapes' count at the first, everything at the last
    CHECK(over.marks.size() == 2);
    if (over.marks.size() == 2)
    {
        CHECK(over.marks[0].counts[hud2kit::LShape] == 6 && over.marks[0].counts[hud2kit::LTextSmall] == 0);
        CHECK(over.marks[1].counts[hud2kit::LTextSmall] == over.v[hud2kit::LTextSmall].size());
    }

    fldl::Meta meta;
    meta.state = "unit", meta.buildGame = "none", meta.buildBench = "test", meta.priv = false;
    std::string list, error;
    CHECK(fldl::ListJson(meta, &under, over, {}, list, error));
    CHECK(error.empty());
    CHECK(list.find("\"schema\": \"fldl/0\"") != std::string::npos && list.find("\"privacy\": \"public\"") != std::string::npos);
    CHECK(list.find(fldl::Marker()) == std::string::npos);
    CHECK(list.find("\"white\"") != std::string::npos);
    CHECK(list.find("glyph:synthetic-21-700:U+0041") != std::string::npos); // 'A' in the small set
    CHECK(list.find("icon:Cruise.128") != std::string::npos && list.find("icon:Dock.64") != std::string::npos);
    CHECK(Count(list, "\"name\": \"t.") == 2);
    CHECK(list.find("\"writeAlpha\": false") != std::string::npos && list.find("\"writeAlpha\": true") != std::string::npos);
    meta.priv = true;
    std::string priv;
    CHECK(fldl::ListJson(meta, &under, over, {}, priv, error) && priv.find(fldl::Marker()) != std::string::npos);

    // fail closed: a triangle sampling between the boxes, or across two, names no texture
    kitlist::Pass bad = over;
    for (hud2kit::Vertex& v : bad.v[hud2kit::LTextSmall])
        v.u = 1.0f, v.v = 1.0f; // the atlas's far corner: no glyph there
    std::string none;
    CHECK(!fldl::ListJson(meta, nullptr, bad, {}, none, error) && error.find("samples no") != std::string::npos);
    kitlist::Pass across = over;
    across.v[hud2kit::LTextSmall][0].u = 0.99f; // one corner far out of its glyph's box
    error.clear();
    CHECK(!fldl::ListJson(meta, nullptr, across, {}, none, error) && !error.empty());

    // the fingerprints: one block per font set, the game's fontdump format
    const std::string prints = fldl::Prints();
    CHECK(prints.rfind("font synthetic px=", 0) == 0);
    std::string diff, missing;
    int differing = 0;
    CHECK(fldl::ComparePrints(prints, diff, missing, differing) >= 2 && differing == 0 && missing.empty());
    std::string changed = prints;
    changed[changed.find("adv=") + 4] = changed[changed.find("adv=") + 4] == '9' ? '8' : '9';
    CHECK(fldl::ComparePrints(changed, diff, missing, differing) >= 1 && differing >= 1 && !diff.empty());

    // game text's styles (hud2_kit.h kStyleItalic / kStyleUnderline, audit decision 56): italic is the same glyphs sheared
    // about the baseline (the same vertices' count, texture coordinates and heights; each quad's top moved right of its foot
    // by kItalicSlant x its height; the width as plain); underline one rule in the shapes layer under the whole run, on the
    // white texel; a styled pass still samples only named boxes
    {
        const float size = static_cast<float>(px);
        CHECK(hud2kit::Begin(dev, W, H, px));
        const float wPlain = hud2kit::Text(100, 200, L"AB C", size, 0xFFFFFFFF, hud2kit::Left, 0, false);
        hud2kit::End();
        CHECK(hud2kit::Begin(dev, W, H, px));
        const float wItalic = hud2kit::Text(100, 200, L"AB C", size, 0xFFFFFFFF, hud2kit::Left, 0, false, hud2kit::kStyleItalic);
        hud2kit::End();
        CHECK(hud2kit::Begin(dev, W, H, px));
        hud2kit::Text(100, 200, L"AB C", size, 0xFFFFFFFF, hud2kit::Left, 0, false, hud2kit::kStyleUnderline);
        hud2kit::TextClipped(100, 300, L"AB C", size, 110, 1000, 0xFFFFFFFF, hud2kit::kStyleUnderline);
        hud2kit::End();
        std::vector<kitlist::Pass> sp = kitlist::Take();
        CHECK(sp.size() == 3 && wPlain > 0 && wItalic == wPlain);
        if (sp.size() == 3)
        {
            const std::vector<hud2kit::Vertex>&up = sp[0].v[hud2kit::LTextSmall], &it = sp[1].v[hud2kit::LTextSmall];
            CHECK(!up.empty() && up.size() == it.size() && sp[1].v[hud2kit::LShape].empty());
            bool same = up.size() == it.size(), sheared = !up.empty();
            for (size_t q = 0; same && q + 5 < up.size(); q += 6)
            {
                // a quad: 0 top-left, 1 top-right, 2 foot-right, 3 top-left, 4 foot-right, 5 foot-left
                for (size_t k = 0; k < 6; ++k)
                    same &= up[q + k].u == it[q + k].u && up[q + k].v == it[q + k].v && up[q + k].y == it[q + k].y;
                const float top = it[q].x - up[q].x, foot = it[q + 5].x - up[q + 5].x, h = up[q + 5].y - up[q].y;
                sheared &= fabsf((top - foot) - hud2kit::kItalicSlant * h) < 1e-3f && top > foot;
                sheared &= fabsf((it[q + 1].x - up[q + 1].x) - top) < 1e-4f && fabsf((it[q + 2].x - up[q + 2].x) - foot) < 1e-4f;
            }
            CHECK(same && sheared);
            // the underline: one rule per run (6 vertices), from the pen to the run's end (the second cut to its clip), below
            // the glyphs' tops, at least a pixel thick, on the shapes layer's white texel
            const std::vector<hud2kit::Vertex>& rule = sp[2].v[hud2kit::LShape];
            CHECK(rule.size() == 12);
            if (rule.size() == 12)
            {
                CHECK_NEAR(rule[0].x, 99.5, 1e-4);
                CHECK_NEAR(rule[1].x, 99.5 + wPlain, 1e-3);
                CHECK_NEAR(rule[6].x, 109.5, 1e-4);
                CHECK(rule[2].y - rule[0].y >= 1.0f && rule[0].y > up[0].y);
                CHECK(rule[0].u == over.v[hud2kit::LShape][0].u && rule[0].v == over.v[hud2kit::LShape][0].v); // (a Rect's texel)
            }
            std::string styled;
            error.clear();
            CHECK(fldl::ListJson(meta, nullptr, sp[1], {}, styled, error) && error.empty());
            CHECK(fldl::ListJson(meta, nullptr, sp[2], {}, styled, error) && error.empty());
        }
        // bold (kStyleBold, the owner's "Synthetic emphasis" of 13 Nov 2026): the plain glyphs, then the same glyphs again
        // kBoldSmear em (at least 1 px) to the right, nothing else moved; the width as plain
        CHECK(hud2kit::Begin(dev, W, H, px));
        const float wBold = hud2kit::Text(100, 200, L"AB C", size, 0xFFFFFFFF, hud2kit::Left, 0, false, hud2kit::kStyleBold);
        hud2kit::End();
        std::vector<kitlist::Pass> bp = kitlist::Take();
        CHECK(bp.size() == 1 && wBold == wPlain);
        if (bp.size() == 1 && sp.size() == 3)
        {
            const std::vector<hud2kit::Vertex>&up = sp[0].v[hud2kit::LTextSmall], &bo = bp[0].v[hud2kit::LTextSmall];
            CHECK(bo.size() == 2 * up.size() && bp[0].v[hud2kit::LShape].empty());
            const float dx = std::max(1.0f, hud2kit::kBoldSmear * size);
            bool twice = bo.size() == 2 * up.size();
            for (size_t k = 0; twice && k < up.size(); ++k)
                twice &= bo[k].x == up[k].x && bo[k].y == up[k].y && bo[k].u == up[k].u && fabsf(bo[up.size() + k].x - (up[k].x + dx)) < 1e-3f &&
                         bo[up.size() + k].y == up[k].y && bo[up.size() + k].u == up[k].u && bo[up.size() + k].v == up[k].v;
            CHECK(twice);
        }
    }
}
