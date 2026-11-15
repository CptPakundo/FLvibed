// HUD 2's draw kit: its passes as display lists (hud2_kit_fldl.h).
#include "hud2_kit_fldl.h"

#include "game/flicons_decode.h"
#include "game/fontraster.h"

#include <algorithm>
#include <map>

namespace fldl
{
std::string Marker() { return std::string("flbench") + "-" + "private"; }

static const char* const kLayerNames[hud2kit::LCount] = {"shapes", "iconBig", "iconSmall", "textSmall", "textBig", "caption"};

static std::string Num(double v)
{
    char b[40];
    snprintf(b, sizeof b, "%.9g", v);
    return b;
}

std::string Utf8(const std::wstring& w)
{
    if (w.empty())
        return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), &s[0], n, nullptr, nullptr);
    return s;
}

std::string Quote(const std::string& s)
{
    std::string o = "\"";
    for (unsigned char c : s)
    {
        if (c == '"' || c == '\\')
            o += '\\', o += static_cast<char>(c);
        else if (c < 0x20)
        {
            char b[8];
            snprintf(b, sizeof b, "\\u%04x", c);
            o += b;
        }
        else
            o += static_cast<char>(c);
    }
    return o + "\"";
}

std::string SetName(const std::wstring& face, int px, int weight)
{
    std::string f;
    for (wchar_t c : face)
        if ((c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z') || (c >= L'0' && c <= L'9'))
            f += static_cast<char>(c);
    if (f.empty())
        f = "face";
    char b[96];
    snprintf(b, sizeof b, "%s-%d-%d", f.c_str(), px, weight);
    return b;
}

void IconAtlas(int cell, Texture& out)
{
    out = Texture();
    out.set = "icons-" + std::to_string(cell);
    out.w = flicons::kCols * cell, out.h = flicons::kRows * cell;
    for (int i = 0; i < flicons::Count; ++i)
        out.boxes.push_back({std::string("icon:") + flicons::kIconNames[i] + "." + std::to_string(cell), (i % flicons::kCols) * cell,
                             (i / flicons::kCols) * cell, cell, cell});
}

unsigned long long LayerHash(const std::vector<hud2kit::Vertex>& v)
{
    return render::Fnv(v.data(), v.size() * sizeof(hud2kit::Vertex));
}

bool CopyOpenPass(Pass& out, std::vector<std::unique_ptr<Texture>>& textures, std::string& error)
{
    const hud2kit::OpenPass p = hud2kit::PassNow();
    out = Pass();
    out.sw = p.sw, out.sh = p.sh, out.keepDestAlpha = p.keepDestAlpha;
    std::vector<std::pair<const void*, const Texture*>> known; // (one description per texture: the shapes share the small font's)
    for (int li = 0; li < hud2kit::LCount; ++li)
    {
        const hud2kit::Layer& l = p.layers[li];
        if (l.v.empty() || !l.tex)
            continue; // (as End: a layer without a texture is not drawn)
        const Texture* t = nullptr;
        for (const auto& k : known)
            if (k.first == l.tex)
                t = k.second;
        if (!t)
        {
            auto d = std::make_unique<Texture>();
            if (!hud2kit::backend::Describe(l.tex, *d))
                return error = std::string("layer ") + kLayerNames[li] + ": a texture the kit's back end does not know", false;
            t = d.get();
            known.push_back({l.tex, t});
            textures.push_back(std::move(d));
        }
        out.v[li] = l.v;
        out.tex[li] = t;
    }
    out.marks = hud2kit::Marks();
    return true;
}

// ---------------------------------------------------------------- naming a triangle's texture
// The box a triangle samples: its texture coordinates' centre in texels, inside exactly one box (the boxes keep two
// empty texels between them); a font's white block (4 x 4 at the origin) is "white". Every corner must lie in that box
// too (a triangle across two boxes would sample both: no list can say that).
struct Named
{
    std::string name;
    double bx = 0, by = 0, bw = 1, bh = 1; // the box in texels (white: unused)
    bool white = false;
};
static bool NameOf(const Texture& t, const hud2kit::Vertex* v, Named& out, std::string& error)
{
    double cu = 0, cv = 0;
    for (int k = 0; k < 3; ++k)
        cu += v[k].u * t.w / 3.0, cv += v[k].v * t.h / 3.0;
    const double eps = 0.01;
    auto inside = [&](double x, double y, double bx, double by, double bw, double bh)
    { return x >= bx - eps && x <= bx + bw + eps && y >= by - eps && y <= by + bh + eps; };
    auto corners = [&](double bx, double by, double bw, double bh)
    {
        for (int k = 0; k < 3; ++k)
            if (!inside(v[k].u * static_cast<double>(t.w), v[k].v * static_cast<double>(t.h), bx, by, bw, bh))
                return false;
        return true;
    };
    if (t.font && inside(cu, cv, 0, 0, 4, 4))
    {
        if (!corners(0, 0, 4, 4))
            return error = "a triangle of " + t.set + " reaches out of the white block", false;
        out.name = "white", out.white = true;
        return true;
    }
    const Box* hit = nullptr;
    for (const Box& b : t.boxes)
        if (inside(cu, cv, b.x, b.y, b.w, b.h))
        {
            if (hit)
                return error = "a triangle of " + t.set + " falls in two boxes (" + hit->name + ", " + b.name + ")", false;
            hit = &b;
        }
    if (!hit)
        return error = "a triangle of " + t.set + " samples no glyph, icon or white texel (texel " + Num(cu) + ", " +
                       Num(cv) + ")",
               false;
    if (!corners(hit->x, hit->y, hit->w, hit->h))
        return error = "a triangle of " + hit->name + " reaches past its box", false;
    out.name = hit->name, out.bx = hit->x, out.by = hit->y, out.bw = hit->w, out.bh = hit->h;
    return true;
}

static bool PassJson(const Pass* p, bool writeAlpha, std::vector<std::string>& textures, std::map<std::string, int>& index,
                     std::string& o, std::string& error)
{
    if (p)
        writeAlpha = !p->keepDestAlpha; // (the pass's own flag: SetKeepDestAlpha = colour writes only)
    o += "{\"writeAlpha\": ";
    o += writeAlpha ? "true" : "false";
    o += ", \"layers\": [";
    for (int li = 0; li < hud2kit::LCount; ++li)
    {
        const bool icon = li == hud2kit::LIconBig || li == hud2kit::LIconSmall;
        o += li ? ",\n  " : "\n  ";
        o += "{\"name\": \"" + std::string(kLayerNames[li]) + "\", \"op\": \"" + (icon ? "icon" : "modulate") + "\", \"v\": [";
        std::string spans;
        if (p && p->tex[li] && !p->v[li].empty())
        {
            const std::vector<hud2kit::Vertex>& v = p->v[li];
            if (v.size() % 3)
                return error = std::string("layer ") + kLayerNames[li] + ": not whole triangles", false;
            int spanFirst = 0, spanTex = -1;
            for (size_t i = 0; i < v.size(); i += 3)
            {
                Named nm;
                if (!NameOf(*p->tex[li], &v[i], nm, error))
                    return false;
                auto it = index.find(nm.name);
                const int ti = it != index.end() ? it->second : (index[nm.name] = static_cast<int>(textures.size()));
                if (it == index.end())
                    textures.push_back(nm.name);
                if (ti != spanTex)
                {
                    if (spanTex >= 0)
                        spans += (spans.empty() ? "" : ", ") + std::string("[") + std::to_string(spanFirst) + ", " +
                                 std::to_string(static_cast<int>(i) - spanFirst) + ", " + std::to_string(spanTex) + "]";
                    spanFirst = static_cast<int>(i), spanTex = ti;
                }
                for (size_t k = i; k < i + 3; ++k)
                {
                    const double u = nm.white ? 0 : (v[k].u * static_cast<double>(p->tex[li]->w) - nm.bx) / nm.bw;
                    const double w = nm.white ? 0 : (v[k].v * static_cast<double>(p->tex[li]->h) - nm.by) / nm.bh;
                    o += (k ? ", " : "") + Num(v[k].x) + ", " + Num(v[k].y) + ", " + std::to_string(v[k].color) + ", " +
                         Num(u) + ", " + Num(w);
                }
            }
            spans += (spans.empty() ? "" : ", ") + std::string("[") + std::to_string(spanFirst) + ", " +
                     std::to_string(static_cast<int>(v.size()) - spanFirst) + ", " + std::to_string(spanTex) + "]";
        }
        o += "], \"spans\": [" + spans + "]}";
    }
    o += "]}";
    return true;
}

bool ListJson(const Meta& meta, const Pass* under, const Pass& over, const std::vector<hud2kit::Drawn>& items,
              std::string& out, std::string& error)
{
    std::vector<std::string> textures;
    std::map<std::string, int> index;
    std::string passes = "{\"under\": ";
    if (!PassJson(under, false, textures, index, passes, error))
        return false;
    passes += ",\n\"over\": ";
    if (!PassJson(&over, true, textures, index, passes, error))
        return false;
    passes += "}";
    if (textures.empty())
        textures.push_back("white"); // (the format wants a non-empty table)

    std::string o = "{\"schema\": \"fldl/0\", \"privacy\": " + Quote(meta.priv ? Marker() : "public") +
                    ", \"build\": {\"game\": " + Quote(meta.buildGame) + ", \"bench\": " + Quote(meta.buildBench) + "},\n" +
                    "\"size\": [" + std::to_string(over.sw) + ", " + std::to_string(over.sh) + "], \"state\": " +
                    Quote(meta.state) + ", \"variant\": " + Quote(meta.variant) + ", \"relayout\": false,\n";
    if (!meta.record.empty() || !meta.inputs.empty())
        o += "\"record\": {\"file\": " + Quote(meta.record) + ", \"inputs\": " + Quote(meta.inputs) + "},\n";
    o += "\"sets\": [";
    // the font sets this list draws with, each named once (by name: the game's recorder describes each pass's textures
    // apart, so the under and over passes' small font are two descriptions of one set)
    std::vector<const Texture*> sets;
    for (const Pass* p : {under, &over})
        for (int li = 0; p && li < hud2kit::LCount; ++li)
            if (p->tex[li] && p->tex[li]->font &&
                std::none_of(sets.begin(), sets.end(), [&](const Texture* t) { return t->set == p->tex[li]->set; }))
                sets.push_back(p->tex[li]);
    for (size_t i = 0; i < sets.size(); ++i)
    {
        const Texture& t = *sets[i];
        char print[24];
        snprintf(print, sizeof print, "%016llx", render::Fnv(t.print.data(), t.print.size()));
        o += (i ? ",\n " : "\n ") + std::string("{\"name\": ") + Quote(t.set) + ", \"face\": " + Quote(Utf8(t.face)) +
             ", \"source\": " + std::to_string(t.source) + ", \"px\": " + std::to_string(t.px) + ", \"weight\": " +
             std::to_string(t.weight) + ", \"ascent\": " + std::to_string(t.ascent) + ", \"line\": " +
             std::to_string(t.lineHeight) + ", \"glyphs\": " + std::to_string(t.boxes.size()) + ", \"print\": \"" + print + "\"}";
    }
    o += "],\n\"textures\": [";
    for (size_t i = 0; i < textures.size(); ++i)
        o += (i ? ", " : "") + Quote(textures[i]);
    o += "],\n\"passes\": " + passes + ",\n\"marks\": [";
    bool first = true;
    for (const Pass* p : {under, &over})
        for (size_t i = 0; p && i < p->marks.size(); ++i)
        {
            const hud2kit::MarkAt& m = p->marks[i];
            o += (first ? "" : ", ") + std::string("{\"name\": ") + Quote(m.name) + ", \"pass\": \"" +
                 (p == under ? "under" : "over") + "\", \"counts\": [";
            for (int li = 0; li < hud2kit::LCount; ++li)
                o += (li ? ", " : "") + std::to_string(m.counts[li]);
            o += "]}";
            first = false;
        }
    o += "],\n\"items\": [";
    for (size_t i = 0; i < items.size(); ++i)
    {
        const hud2kit::Drawn& d = items[i];
        o += (i ? ",\n " : "\n ") + std::string("{\"tag\": ") + Quote(d.tag ? d.tag : "") + ", \"kind\": " +
             Quote(std::string(1, d.kind ? d.kind : '?')) + ", \"rect\": [" + Num(d.x0) + ", " + Num(d.y0) + ", " +
             Num(std::max(0.0f, d.x1 - d.x0)) + ", " + Num(std::max(0.0f, d.y1 - d.y0)) + "]" +
             (d.text.empty() ? "" : ", \"text\": " + Quote(Utf8(d.text))) + "}";
    }
    o += "],\n\"hot\": [], \"holes\": []}\n";
    out = std::move(o);
    return true;
}
} // namespace fldl
