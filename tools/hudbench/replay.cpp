// FLvibed design bench - a recorded frame of HUD 2's flight HUD run again with no game (replay.h).
#include "replay.h"

#include "fldl_write.h"
#include "game/fontraster.h"
#include "game/hudsize.h"
#include "game/textfit.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <tuple>

namespace benchreplay
{
using hud2state::Json;
using hud2state::Tape;

// ---------------------------------------------------------------- the bench's host: the record's tape, in call order
// One half (Step's, Draw's) at a time; a call outside a half has no recorded answer and fails the replay.
namespace
{
struct Player
{
    const Tape* tape = nullptr;
    const char* half = "";
    size_t at[5] = {};
    std::string error;
    int hotOn = -1;
    std::vector<hud2state::HotRect>* hot = nullptr;
    // a variant's tape (ReplayVariant): an answer past the end repeats the last one, answers left are no fault; counted
    bool lenient = false;
    size_t past = 0, left = 0;
};
// A variant's preset tables (--layout): copies of core\ui_meta.h's with numbers changed, given to the frame by the host
// (hud2_frame.h FrameHost::preset).
struct Tables
{
    bool on = false;
    std::vector<uimeta::hud2p::ElemNum> nums[uimeta::hud2p::kPresetCount];
    uimeta::hud2p::Preset presets[uimeta::hud2p::kPresetCount] = {};
};
} // namespace
static Player g_play;
static Tables g_tables;
static const uimeta::hud2p::Preset* BenchPreset(int p)
{
    return g_tables.on && p >= 0 && p < uimeta::hud2p::kPresetCount ? &g_tables.presets[p] : nullptr;
}

static void Fail(const std::string& why)
{
    if (g_play.error.empty())
        g_play.error = why;
}

template <class T> static T Next(std::vector<T> Tape::*kind, int k, const char* name)
{
    if (!g_play.tape)
    {
        Fail(std::string("a host call (") + name + ") outside Step and Draw: the record holds no answer for it");
        return T{};
    }
    const std::vector<T>& v = g_play.tape->*kind;
    size_t& at = g_play.at[k];
    if (at < v.size())
        return v[at++];
    if (g_play.lenient)
    {
        ++at, ++g_play.past;
        return v.empty() ? T{} : v.back();
    }
    Fail(std::string(g_play.half) + "'s tape ran out: " + name + " was asked for answer " + std::to_string(at + 1) +
         ", the record holds " + std::to_string(v.size()));
    ++at;
    return v.empty() ? T{} : v.back();
}
static u32 TickMs() { return Next(&Tape::tickMs, 0, "tickMs"); }
static unsigned long long TickMs64() { return Next(&Tape::tickMs64, 1, "tickMs64"); }
static double Seconds() { return Next(&Tape::seconds, 2, "seconds"); }
static bool Typing() { return Next(&Tape::typing, 3, "typing") != 0; }
static int HotHovered() { return Next(&Tape::hovered, 4, "hovered"); }
static void HotStart(bool on) { g_play.hotOn = on ? 1 : 0; }
static void HotRect(int slot, float x0, float y0, float x1, float y1)
{
    if (g_play.hot)
        g_play.hot->push_back({slot, x0, y0, x1, y1});
}

static hud2::FrameHost Host()
{
    hud2::FrameHost h;
    h.tickMs = TickMs, h.tickMs64 = TickMs64, h.seconds = Seconds, h.typing = Typing;
    h.hotStart = HotStart, h.hotRect = HotRect, h.hotHovered = HotHovered;
    if (g_tables.on)
        h.preset = BenchPreset;
    return h;
}

static void Start(const Tape& tape, const char* half)
{
    g_play.tape = &tape, g_play.half = half;
    for (size_t& a : g_play.at)
        a = 0;
}
// Every answer of the half used, none asked for past the end.
static void Finish()
{
    if (!g_play.tape)
        return;
    const Tape& t = *g_play.tape;
    const size_t have[5] = {t.tickMs.size(), t.tickMs64.size(), t.seconds.size(), t.typing.size(), t.hovered.size()};
    static const char* const kNames[5] = {"tickMs", "tickMs64", "seconds", "typing", "hovered"};
    for (int k = 0; k < 5; ++k)
        if (g_play.at[k] < have[k] && g_play.lenient)
            g_play.left += have[k] - g_play.at[k];
        else if (g_play.at[k] < have[k])
            Fail(std::string(g_play.half) + "'s tape is left over: " + std::to_string(have[k] - g_play.at[k]) + " of " +
                 std::to_string(have[k]) + " " + kNames[k] + " answers not asked for");
    g_play.tape = nullptr, g_play.half = "";
}

void Unbind()
{
    // the kit's marquee table back to its own, emptied (a table set while empty takes over the clocks of the one before:
    // hud2_kit.h SetMarquees), and its clock back to its default
    static hud2kit::Marquees sink;
    hud2kit::SetMarquees(&sink);
    sink.clocks.clear();
    hud2kit::SetMarquees(nullptr);
    hud2kit::SetClock(nullptr);
    g_play = Player{};
    g_tables.on = false;
}

// The host reads the kit still makes, as the game's frame left them for the pass: the text floor (hud_size), the pad
// family and glyph_look of its plain glyph forms, the icon atlas standing.
static void KitInputs(const hud2::PartsIn& p, bool icons)
{
    textfit::g_floorH = p.size.floorH;
    kitlist::In().family = p.family;
    kitlist::In().glyphLook = p.opt.glyphLook;
    kitlist::In().icons = icons;
}

void Replay(const hud2state::StateDoc& doc, Run& run)
{
    Unbind();
    run.ok = false, run.error.clear(), run.underDrawn = false, run.hotOn = -1, run.hot.clear(), run.drawPx = 0;
    run.under = kitlist::Pass{}, run.over = kitlist::Pass{}, run.stepOut = hud2::StepOut{}, run.textures.clear();
    const hud2state::RenderRec& r = doc.render;
    auto fail = [&](const std::string& why)
    {
        kitlist::Take();
        run.error = why;
    };
    if (doc.update.phase != hud2::Phase::Live)
        return fail("the record's Update was not a live frame (phase " + std::to_string(static_cast<int>(doc.update.phase)) + ")");
    if (!r.drawn || r.sw < 100 || r.sh < 100)
        return fail("the record's Render drew no pass (render.drawn false): nothing to replay");
    void* dev = kitlist::Device();
    kitlist::Take();

    // the kit as the game's frame found it: a pass already done at this size (Step measures on the last pass's device and
    // screen: hud2_kit.h MeasureAt), and every character the game's atlases had gained (row 31): a pass that draws them,
    // so the next Begin builds the atlases with them in the game's order
    KitInputs(doc.in.parts, r.iconsReady);
    if (!hud2kit::Begin(dev, r.sw, r.sh, r.askPx))
        return fail("the kit's Begin failed (no font)");
    if (!r.kitExtra.empty())
        hud2kit::Text(0, 0, r.kitExtra, static_cast<float>(r.askPx), 0xFFFFFFFF);
    hud2kit::End();
    kitlist::Take();

    const hud2::FrameHost host = Host();
    // Update's half: Step from the state it took, with its tape
    run.stepped = doc.st;
    Start(doc.update.tape, "Step");
    hud2::Step(host, doc.in, run.stepped, run.stepOut);
    Finish();

    // the under pass: the view backings, as RenderUnderHud drew them
    if (doc.under.drawn)
    {
        if (!hud2kit::Begin(dev, doc.under.sw, doc.under.sh, doc.under.px))
            return fail("the kit's Begin failed for the under pass");
        auto st = std::make_unique<hud2::FrameState>();
        st->viewBack[0] = doc.under.viewBack[0], st->viewBack[1] = doc.under.viewBack[1];
        hud2::DrawUnder(*st, doc.under.back);
        std::string err;
        if (!fldl::CopyOpenPass(run.under, run.textures, err))
            return fail("the under pass not copied: " + err);
        hud2kit::End();
        run.underDrawn = true;
    }

    // Render's half: Draw from the state the game's Draw took, with its tape, into a pass opened as the game opened it
    KitInputs(doc.draw.parts, r.iconsReady);
    if (!hud2kit::Begin(dev, r.sw, r.sh, r.askPx))
        return fail("the kit's Begin failed for the over pass");
    run.drawPx = hud2kit::TextPx();
    run.drawn = r.st;
    Start(r.tape, "Draw");
    g_play.hot = &run.hot;
    hud2::Draw(host, doc.draw, run.drawn);
    Finish();
    g_play.hot = nullptr;
    run.hotOn = g_play.hotOn;
    hud2kit::Mark("comm.end"); // (the game's comm caption goes between parts.end and here: the host's, a hole)
    {
        // copied before End, as the game's recorder does (End submits nothing for an empty pass: lowenergy, disrupt)
        std::string err;
        if (!fldl::CopyOpenPass(run.over, run.textures, err))
            return fail("the over pass not copied: " + err);
    }
    hud2kit::End();
    kitlist::Take();
    run.error = g_play.error;
    if (run.error.empty() && run.drawPx != r.drawPx)
        run.error = "the kit drew at " + std::to_string(run.drawPx) + " px, the game at " + std::to_string(r.drawPx);
    run.ok = run.error.empty();
}

// ---------------------------------------------------------------- reading a list
static const Json* Member(const Json& o, const char* name)
{
    if (o.kind != Json::Object)
        return nullptr;
    for (const auto& m : o.members)
        if (m.first == name)
            return &m.second;
    return nullptr;
}
static double Number(const Json* j) { return j && j->kind == Json::Number ? strtod(j->text.c_str(), nullptr) : 0; }

// A set's name without the bench's build suffix (.2, .3: FORMAT.md point 3).
static std::string BaseSet(const std::string& name)
{
    const size_t dot = name.rfind('.');
    if (dot == std::string::npos || dot + 1 >= name.size())
        return name;
    for (size_t i = dot + 1; i < name.size(); ++i)
        if (name[i] < '0' || name[i] > '9')
            return name;
    return name.substr(0, dot);
}

bool ReadList(const std::string& text, ListRead& out, std::string& error)
{
    Json root;
    hud2state::JsonParser p{text};
    if (!p.Value(root) || root.kind != Json::Object)
        return error = "not a list: " + (p.err.empty() ? std::string("not an object") : p.err), false;
    const Json* schema = Member(root, "schema");
    if (!schema || schema->text != "fldl/0")
        return error = "not an fldl/0 list", false;
    const Json* size = Member(root, "size");
    if (!size || size->items.size() != 2)
        return error = "the list has no size", false;
    out.w = static_cast<int>(Number(&size->items[0])), out.h = static_cast<int>(Number(&size->items[1]));
    // a glyph's set by what makes it (face, px, weight); its fingerprint apart (Compare checks the prints)
    std::vector<std::pair<std::string, std::string>> setKey;
    if (const Json* sets = Member(root, "sets"))
        for (const Json& s : sets->items)
        {
            const Json *n = Member(s, "name"), *f = Member(s, "face"), *print = Member(s, "print");
            if (!n || !f || !print)
                return error = "a set without its name, face or print", false;
            const std::string key = f->text + " " + std::to_string(static_cast<int>(Number(Member(s, "px")))) + " " +
                                    std::to_string(static_cast<int>(Number(Member(s, "weight"))));
            setKey.push_back({n->text, key});
            out.sets.push_back({key, print->text});
        }
    auto texName = [&](const std::string& t)
    {
        if (t.rfind("glyph:", 0) != 0)
            return t;
        const size_t colon = t.find(':', 6);
        const std::string set = t.substr(6, colon == std::string::npos ? std::string::npos : colon - 6);
        for (const auto& k : setKey)
            if (k.first == set)
                return "glyph:" + k.second + t.substr(colon == std::string::npos ? t.size() : colon);
        return "glyph:" + BaseSet(set) + "?" + t.substr(colon == std::string::npos ? t.size() : colon);
    };
    std::vector<std::string> textures;
    if (const Json* tx = Member(root, "textures"))
        for (const Json& t : tx->items)
            textures.push_back(texName(t.text));
    const Json* passes = Member(root, "passes");
    if (!passes)
        return error = "the list has no passes", false;
    for (int pi = 0; pi < 2; ++pi)
    {
        ListPass& lp = pi ? out.over : out.under;
        const Json* pass = Member(*passes, pi ? "over" : "under");
        const Json* layers = pass ? Member(*pass, "layers") : nullptr;
        if (!layers || layers->items.size() != hud2kit::LCount)
            return error = std::string("the ") + (pi ? "over" : "under") + " pass has not six layers", false;
        const Json* wa = Member(*pass, "writeAlpha");
        lp.writeAlpha = wa && wa->text == "true";
        for (int li = 0; li < hud2kit::LCount; ++li)
        {
            const Json* v = Member(layers->items[li], "v");
            const Json* spans = Member(layers->items[li], "spans");
            if (!v || !spans || v->items.size() % 5)
                return error = "a layer without whole vertices", false;
            ListLayer& L = lp.layers[li];
            const size_t n = v->items.size() / 5;
            for (size_t i = 0; i < n; ++i)
            {
                L.x.push_back(static_cast<float>(Number(&v->items[i * 5])));
                L.y.push_back(static_cast<float>(Number(&v->items[i * 5 + 1])));
                L.color.push_back(static_cast<unsigned>(strtoul(v->items[i * 5 + 2].text.c_str(), nullptr, 10)));
                L.u.push_back(static_cast<float>(Number(&v->items[i * 5 + 3])));
                L.v.push_back(static_cast<float>(Number(&v->items[i * 5 + 4])));
            }
            L.tex.assign(n, std::string());
            size_t covered = 0;
            for (const Json& s : spans->items)
            {
                if (s.items.size() != 3)
                    return error = "a span not [first, count, texture]", false;
                const size_t first = static_cast<size_t>(Number(&s.items[0])), count = static_cast<size_t>(Number(&s.items[1]));
                const size_t ti = static_cast<size_t>(Number(&s.items[2]));
                if (first != covered || first + count > n || ti >= textures.size())
                    return error = "spans that do not cover their layer in order", false;
                for (size_t i = first; i < first + count; ++i)
                    L.tex[i] = textures[ti];
                covered += count;
            }
            if (covered != n)
                return error = "spans that do not cover their layer", false;
        }
    }
    if (const Json* marks = Member(root, "marks"))
        for (const Json& m : marks->items)
        {
            const Json *n = Member(m, "name"), *pass = Member(m, "pass"), *counts = Member(m, "counts");
            if (!n || !pass || !counts || counts->items.size() != hud2kit::LCount)
                return error = "a mark without its name, pass or six counts", false;
            std::vector<unsigned> c;
            for (const Json& k : counts->items)
                c.push_back(static_cast<unsigned>(Number(&k)));
            (pass->text == "under" ? out.under : out.over).marks.push_back({n->text, c});
        }
    return true;
}

// ---------------------------------------------------------------- level 1
static const char* const kLayer[hud2kit::LCount] = {"shapes", "iconBig", "iconSmall", "textSmall", "textBig", "caption"};
static constexpr float kPos = 0.01f, kUv = 1e-4f; // the design's 0.01 px; box-relative texture coordinates

static std::string F(double v)
{
    char b[32];
    snprintf(b, sizeof b, "%.6g", v);
    return b;
}
static std::string Hex(unsigned long long h)
{
    char b[24];
    snprintf(b, sizeof b, "%016llx", h);
    return b;
}
static const std::vector<unsigned>* MarkOf(const ListPass& p, const char* name)
{
    for (const auto& m : p.marks)
        if (m.first == name)
            return &m.second;
    return nullptr;
}

// Vertices [from, to) of a layer, the game's against the bench's (same indexes).
static void CompareRange(const char* pass, int li, const ListLayer& g, const ListLayer& b, size_t from, size_t to, Verdict& out)
{
    int shown = 0, differing = 0;
    for (size_t i = from; i < to; ++i)
    {
        const float dx = fabsf(g.x[i] - b.x[i]), dy = fabsf(g.y[i] - b.y[i]);
        out.maxMove = std::max(out.maxMove, std::max(dx, dy));
        std::string what;
        if (dx > kPos || dy > kPos)
            what = "at (" + F(b.x[i]) + ", " + F(b.y[i]) + "), the game's (" + F(g.x[i]) + ", " + F(g.y[i]) + ")";
        else if (g.color[i] != b.color[i])
            what = "colour " + std::to_string(b.color[i]) + ", the game's " + std::to_string(g.color[i]);
        else if (g.tex[i] != b.tex[i])
            what = "texture " + b.tex[i] + ", the game's " + g.tex[i];
        else if (fabsf(g.u[i] - b.u[i]) > kUv || fabsf(g.v[i] - b.v[i]) > kUv)
            what = "texture coordinates (" + F(b.u[i]) + ", " + F(b.v[i]) + "), the game's (" + F(g.u[i]) + ", " + F(g.v[i]) + ")";
        if (what.empty())
            continue;
        ++differing;
        if (shown++ < 3)
            out.lines.push_back(std::string(pass) + " " + kLayer[li] + " vertex " + std::to_string(i) + ": " + what);
    }
    if (differing)
    {
        out.differences += differing;
        if (differing > 3)
            out.lines.push_back(std::string(pass) + " " + kLayer[li] + ": " + std::to_string(differing) + " vertices differ in all");
    }
}

static void ComparePass(const char* name, const ListPass& g, const ListPass& b, bool over, Verdict& out)
{
    const bool gAny = std::any_of(std::begin(g.layers), std::end(g.layers), [](const ListLayer& l) { return !l.x.empty(); });
    if (gAny && g.writeAlpha != b.writeAlpha)
        ++out.differences, out.lines.push_back(std::string(name) + ": writeAlpha differs");
    // the game's parts end at its mark parts.end; between it and comm.end the host's comm caption (a hole); after comm.end
    // whatever else drew into the pass (none today): compared too
    const std::vector<unsigned>*gParts = over ? MarkOf(g, "parts.end") : nullptr, *bParts = over ? MarkOf(b, "parts.end") : nullptr;
    const std::vector<unsigned>*gComm = over ? MarkOf(g, "comm.end") : nullptr, *bComm = over ? MarkOf(b, "comm.end") : nullptr;
    if (over && gAny && (!gParts || !gComm))
        ++out.differences, out.lines.push_back("over: the game's list lacks the marks parts.end / comm.end");
    if (over && (!bParts || !bComm))
        ++out.differences, out.lines.push_back("over: the bench's pass lacks the marks parts.end / comm.end");
    if (over && gParts && gComm)
    {
        unsigned caption = 0;
        for (int li = 0; li < hud2kit::LCount; ++li)
            caption += (*gComm)[li] - (*gParts)[li];
        if (caption)
            out.holes.push_back("comm.caption: " + std::to_string(caption) +
                                " vertices between parts.end and comm.end (the game's window text, drawn by the host: not "
                                "in the record)");
    }
    for (int li = 0; li < hud2kit::LCount; ++li)
    {
        const ListLayer &gl = g.layers[li], &bl = b.layers[li];
        if (!(over && gParts && gComm && bParts && bComm))
        {
            if (gl.x.size() != bl.x.size())
                ++out.differences, out.lines.push_back(std::string(name) + " " + kLayer[li] + ": " + std::to_string(bl.x.size()) +
                                                           " vertices, the game's " + std::to_string(gl.x.size()));
            CompareRange(name, li, gl, bl, 0, std::min(gl.x.size(), bl.x.size()), out);
            continue;
        }
        const size_t gp = (*gParts)[li], bp = (*bParts)[li];
        if (gp != bp)
            ++out.differences, out.lines.push_back(std::string(name) + " " + kLayer[li] + ": the parts drew " + std::to_string(bp) +
                                                       " vertices, the game's " + std::to_string(gp));
        CompareRange(name, li, gl, bl, 0, std::min(gp, bp), out);
        // after comm.end: the game's tail against the bench's (both empty today)
        const size_t gt = gl.x.size() - (*gComm)[li], bt = bl.x.size() - (*bComm)[li];
        if (gt != bt)
            ++out.differences, out.lines.push_back(std::string(name) + " " + kLayer[li] + ": " + std::to_string(gt) +
                                                       " vertices drawn after comm.end in the game, " + std::to_string(bt) +
                                                       " in the bench");
    }
    // the marks themselves: the same names in the same order
    std::string gm, bm;
    for (const auto& m : g.marks)
        gm += m.first + " ";
    for (const auto& m : b.marks)
        bm += m.first + " ";
    if (gAny && gm != bm)
        ++out.differences, out.lines.push_back(std::string(name) + ": marks [" + bm + "], the game's [" + gm + "]");
}

// Two JSON trees member by member: numbers within a relative 1e-5 (an absolute 1e-4 near zero), the rest as text.
static void DiffTree(const Json& a, const Json& b, const std::string& path, int& count, std::vector<std::string>& lines)
{
    auto note = [&](const std::string& what)
    {
        if (count++ < 6)
            lines.push_back("state " + path + ": " + what);
    };
    if (a.kind != b.kind)
        return note("of another kind");
    if (a.kind == Json::Number)
    {
        const double x = strtod(a.text.c_str(), nullptr), y = strtod(b.text.c_str(), nullptr);
        if (a.text != b.text && fabs(x - y) > std::max(1e-4, 1e-5 * std::max(fabs(x), fabs(y))))
            note("the bench's " + b.text + ", the game's " + a.text);
        return;
    }
    if (a.kind == Json::Object)
    {
        if (a.members.size() != b.members.size())
            return note("another member count");
        for (size_t i = 0; i < a.members.size(); ++i)
            DiffTree(a.members[i].second, b.members[i].second, path + "." + a.members[i].first, count, lines);
        return;
    }
    if (a.kind == Json::Array)
    {
        if (a.items.size() != b.items.size())
            return note(std::to_string(b.items.size()) + " items, the game's " + std::to_string(a.items.size()));
        for (size_t i = 0; i < a.items.size(); ++i)
            DiffTree(a.items[i], b.items[i], path + "[" + std::to_string(i) + "]", count, lines);
        return;
    }
    if (a.text != b.text)
        note("the bench's " + b.text + ", the game's " + a.text);
}
static std::string StateText(const hud2::FrameState& st)
{
    hud2state::Writer w;
    w.Open(nullptr, '{');
    w("st", const_cast<hud2::FrameState&>(st)); // (the writer only reads)
    w.Close('}', true);
    return w.out;
}

void Compare(const hud2state::StateDoc& doc, const std::string& gameListText, const Run& run, const std::string& benchList,
             Verdict& out, const std::vector<hud2state::RetiredMember>* retired)
{
    auto differ = [&](const std::string& line) { ++out.differences, out.lines.push_back(line); };
    // the record and its list belong together (the record's fingerprints of the list file and of its own inputs; a record
    // older than a retired member was fingerprinted with it, so it is written back where it stood)
    if (Hex(render::Fnv(gameListText.data(), gameListText.size())) != doc.list.hash)
        differ("the game's list is not the one the record names (its hash is not list.hash)");
    {
        hud2state::Writer w;
        w.retired = retired && !retired->empty() ? retired : nullptr;
        w.Open(nullptr, '{');
        w("draw_in", const_cast<hud2::DrawIn&>(doc.draw)), w("render", const_cast<hud2state::RenderRec&>(doc.render));
        w.Close('}', true);
        if (Hex(render::Fnv(w.out.data(), w.out.size())) != doc.list.inputs)
            differ("the record's draw_in and render do not read back as written (their hash is not list.inputs)");
    }
    ListRead g, b;
    std::string err;
    if (!ReadList(gameListText, g, err))
        return differ("the game's list: " + err);
    if (!ReadList(benchList, b, err))
        return differ("the bench's list: " + err);
    if (g.w != b.w || g.h != b.h)
        differ("size " + std::to_string(b.w) + "x" + std::to_string(b.h) + ", the game's " + std::to_string(g.w) + "x" + std::to_string(g.h));
    // the fonts: each set the bench drew with has the game's fingerprint
    for (const auto& s : b.sets)
        for (const auto& gs : g.sets)
            if (gs.first == s.first && gs.second != s.second)
                ++out.fontDifferences, out.lines.push_back("font set " + s.first + ": print " + s.second + ", the game's " + gs.second);
    ComparePass("under", g.under, b.under, false, out);
    ComparePass("over", g.over, b.over, true, out);

    // the fingerprints: each layer's kitdump hash, bit for bit (the over pass only when the game drew nothing after its parts)
    auto exact = [&](const std::vector<hud2state::LayerPrint>& prints, const kitlist::Pass& p)
    {
        if (prints.size() != hud2kit::LCount)
            return false;
        for (int li = 0; li < hud2kit::LCount; ++li)
            if (prints[li].count != p.v[li].size() || prints[li].hash != Hex(fldl::LayerHash(p.v[li])))
                return false;
        return true;
    };
    out.exactOver = exact(doc.list.over, run.over);
    out.exactUnder = doc.list.under.empty() ? !run.underDrawn : exact(doc.list.under, run.under);
    if (doc.list.under.empty() != !run.underDrawn)
        differ(std::string("the under pass: drawn in ") + (run.underDrawn ? "the bench" : "the game") + " only");

    // the hotspots Draw gave (the keyboard form's clickable rectangles)
    const hud2state::RenderRec& r = doc.render;
    if (run.hotOn != r.hotOn)
        differ("the hotspots' start " + std::to_string(run.hotOn) + ", the game's " + std::to_string(r.hotOn));
    if (run.hot.size() != r.hot.size())
        differ(std::to_string(run.hot.size()) + " hotspots, the game's " + std::to_string(r.hot.size()));
    else
        for (size_t i = 0; i < r.hot.size(); ++i)
        {
            const hud2state::HotRect &x = run.hot[i], &y = r.hot[i];
            if (x.slot != y.slot || fabsf(x.x0 - y.x0) > kPos || fabsf(x.y0 - y.y0) > kPos || fabsf(x.x1 - y.x1) > kPos ||
                fabsf(x.y1 - y.y1) > kPos)
                differ("hotspot " + std::to_string(i) + " (slot " + std::to_string(x.slot) + ") differs from the game's");
        }

    // Step: what it asked of the host, and the state it left against the one the game's Draw took
    const hud2::StepOut& so = doc.update.step;
    if (run.stepOut.pulse != so.pulse)
        differ("Step's target-panel pulse differs from the game's");
    for (int i = 0; i < 2; ++i)
    {
        const hud2::ViewRect &x = run.stepOut.view[i], &y = so.view[i];
        if (x.show != y.show || fabsf(x.cx - y.cx) > kPos || fabsf(x.cy - y.cy) > kPos || fabsf(x.hw - y.hw) > kPos || fabsf(x.hh - y.hh) > kPos)
            differ(std::string("Step's ") + (i ? "target" : "ship") + " view rectangle differs from the game's");
    }
    if (r.sw != static_cast<int>(run.stepped.L.W) || r.sh != static_cast<int>(run.stepped.L.H))
        out.notes.push_back("Render re-laid out at " + std::to_string(r.sw) + "x" + std::to_string(r.sh) +
                            " (host work between Step and Draw): Step's state not compared with Draw's");
    else if (doc.st.sim.empty())
        out.notes.push_back("a live frame: the target's projection is host work between Step and Draw, Step's state not compared");
    else
    {
        const std::string gt = StateText(r.st), bt = StateText(run.stepped);
        if (gt == bt)
            out.notes.push_back("Step's state equals the game's, as written");
        else
        {
            Json ga, ba;
            hud2state::JsonParser pa{gt}, pb{bt};
            int count = 0;
            if (pa.Value(ga) && pb.Value(ba))
                DiffTree(ga, ba, "", count, out.lines);
            if (count)
                out.differences += count, out.lines.push_back("Step's state: " + std::to_string(count) + " members differ from the game's");
            else
                out.notes.push_back("Step's state equals the game's within the tolerance (not as written)");
        }
    }
    if (out.exactOver && out.exactUnder)
        out.notes.push_back("the passes equal the record's fingerprints bit for bit");
    else if (!out.differences)
        out.notes.push_back("the passes equal the game's within the tolerance (largest move " + F(out.maxMove) + " px), not bit for bit");
}

// ---------------------------------------------------------------- a variant: settings, preset numbers, another size
namespace hp = uimeta::hud2p;

static std::string NumName(const hp::Preset& p, const hp::ElemNum& n)
{
    return std::string(p.id) + "." + n.context + "." + n.elem + "." + n.key;
}

std::string LayoutNames()
{
    std::string o;
    for (const hp::Preset& p : hp::kPresets)
        for (int i = 0; i < p.numCount; ++i)
            o += NumName(p, p.nums[i]) + " = " + F(p.nums[i].v) + "\n";
    return o;
}

bool ReadLayout(const std::string& text, std::vector<LayoutNum>& out, std::string& why)
{
    out.clear();
    Json root;
    hud2state::JsonParser p{text};
    const bool read = p.Value(root);
    while (p.at < text.size() && strchr(" \t\r\n", text[p.at]))
        ++p.at;
    if (!read || root.kind != Json::Object || p.at != text.size())
        return why = "not one JSON object" + (p.err.empty() ? std::string() : ": " + p.err), false;
    for (const auto& m : root.members)
    {
        if (!m.first.empty() && m.first[0] == '_') // ("_about": a note for the reader)
            continue;
        if (m.second.kind != Json::Number)
            return why = "\"" + m.first + "\": not a number", false;
        for (const LayoutNum& o : out)
            if (o.name == m.first)
                return why = "\"" + m.first + "\" given twice", false;
        LayoutNum n;
        n.name = m.first;
        bool found = false;
        // (the first of a name in its table: the one hp::Num reads)
        for (int pi = 0; pi < hp::kPresetCount && !found; ++pi)
            for (int i = 0; i < hp::kPresets[pi].numCount && !found; ++i)
                if (NumName(hp::kPresets[pi], hp::kPresets[pi].nums[i]) == m.first)
                    n.preset = pi, n.index = i, n.was = hp::kPresets[pi].nums[i].v, found = true;
        if (!found)
            return why = "\"" + m.first + "\": no number of that name in the presets' tables (flbench layout-names lists them)", false;
        n.value = static_cast<float>(strtod(m.second.text.c_str(), nullptr));
        if (!std::isfinite(n.value))
            return why = "\"" + m.first + "\": not a finite number", false;
        out.push_back(n);
    }
    if (out.empty())
        return why = "no number in it", false;
    return true;
}

bool ParseSet(const std::string& arg, Overrides& o, std::string& why)
{
    const size_t eq = arg.find('=');
    if (eq == std::string::npos || eq == 0 || eq + 1 >= arg.size())
        return why = "--set " + arg + ": not name=value", false;
    o.set.push_back({arg.substr(0, eq), arg.substr(eq + 1)});
    return true;
}

namespace
{
bool WholeNumber(const std::string& s, long long& n)
{
    char* end = nullptr;
    errno = 0;
    n = strtoll(s.c_str(), &end, 10);
    return !s.empty() && end && !*end && errno == 0;
}
std::string Lower(std::string s)
{
    for (char& c : s)
        c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    return s;
}
// Sets every member of a frame's settings copy whose path is the name asked for or ends in it (hud2_state_io.h's names;
// the lists' Fields are the one place the names live).
struct Setter
{
    static constexpr bool kSets = false, kFills = false;
    std::string want, value, path;
    std::vector<std::pair<std::string, std::string>> done; // the path, "old -> new"
    std::string error;

    bool Match(const char* k) const
    {
        const std::string full = path + k;
        return full == want || (full.size() > want.size() + 1 && full.compare(full.size() - want.size(), want.size(), want) == 0 &&
                                full[full.size() - want.size() - 1] == '.');
    }
    void Bad(const char* k, const char* wanted)
    {
        if (error.empty())
            error = "--set " + want + "=" + value + ": " + path + k + " takes " + wanted;
    }
    void operator()(const char* k, bool& x)
    {
        if (!Match(k))
            return;
        const std::string v = Lower(value);
        long long n = -1;
        const bool on = v == "true" || v == "on" || (WholeNumber(v, n) && n == 1);
        if (!on && !(v == "false" || v == "off" || n == 0))
            return Bad(k, "0 / 1");
        done.push_back({path + k, std::string(x ? "1" : "0") + " -> " + (on ? "1" : "0")});
        x = on;
    }
    template <class T> std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>> operator()(const char* k, T& x)
    {
        if (!Match(k))
            return;
        long long n = 0;
        if (!WholeNumber(value, n) || n < static_cast<long long>(std::numeric_limits<T>::min()) ||
            (n > 0 && static_cast<unsigned long long>(n) > static_cast<unsigned long long>(std::numeric_limits<T>::max())))
            return Bad(k, "a whole number");
        done.push_back({path + k, std::to_string(static_cast<long long>(x)) + " -> " + std::to_string(n)});
        x = static_cast<T>(n);
    }
    template <class T> std::enable_if_t<std::is_floating_point_v<T>> operator()(const char* k, T& x)
    {
        if (!Match(k))
            return;
        char* end = nullptr;
        const double d = strtod(value.c_str(), &end);
        if (value.empty() || !end || *end || !std::isfinite(d))
            return Bad(k, "a number");
        done.push_back({path + k, F(x) + " -> " + F(d)});
        x = static_cast<T>(d);
    }
    template <class T> std::enable_if_t<std::is_enum_v<T>> operator()(const char* k, T& x)
    {
        if (!Match(k))
            return;
        long long n = 0;
        if constexpr (std::is_same_v<T, glyphs::Family>)
        {
            static const char* const kNames[] = {"xbox", "playstation", "nintendo", "steam"};
            for (int i = 0; i < 4; ++i)
                if (Lower(value) == kNames[i])
                    n = i + 100; // (named: below)
            if (n >= 100)
                n -= 100;
            else if (!WholeNumber(value, n) || n < 0 || n > 3)
                return Bad(k, "xbox, playstation, nintendo or steam");
        }
        else if (!WholeNumber(value, n))
            return Bad(k, "a whole number");
        done.push_back({path + k, std::to_string(static_cast<long long>(x)) + " -> " + std::to_string(n)});
        x = static_cast<T>(n);
    }
    void operator()(const char* k, std::string& x)
    {
        if (!Match(k))
            return;
        done.push_back({path + k, "\"" + x + "\" -> \"" + value + "\""});
        x = value;
    }
    void operator()(const char* k, std::wstring&)
    {
        if (Match(k))
            Bad(k, "nothing: wide text is not a setting the bench changes");
    }
    template <class T> std::enable_if_t<std::is_class_v<T> && !hud2state::kIsText<T>> operator()(const char* k, T& x)
    {
        const std::string was = path;
        path += std::string(k) + ".";
        hud2state::Fields(*this, x);
        path = was;
    }
    template <class T, size_t N> void operator()(const char* k, T (&a)[N])
    {
        for (size_t i = 0; i < N; ++i)
            (*this)((std::string(k) + "." + std::to_string(i)).c_str(), a[i]);
    }
    template <class T> void operator()(const char*, std::vector<T>&) {}
    template <class T, class Fn> void group(const char* k, T& x, Fn f)
    {
        const std::string was = path;
        path += std::string(k) + ".";
        f(*this, x);
        path = was;
    }
    template <class T> void note(T&) {}
    void fail(const char*, const char*) {}
};
} // namespace

bool ApplySet(const std::vector<std::pair<std::string, std::string>>& set, float H, hud2::PartsIn& p,
              std::vector<std::string>& lines, std::string& why)
{
    for (const auto& kv : set)
    {
        if (kv.first == "hud_size")
        {
            // the one size choice as the game's frame resolves it (core\game\hudsize.h Apply), at the height drawn
            const std::string v = Lower(kv.second);
            const int preset = v == "small" || v == "1" ? hudsize::Small : v == "normal" || v == "2" ? hudsize::Normal
                               : v == "large" || v == "3"                 ? hudsize::Large
                                                                          : -1;
            if (preset < 0)
                return why = "--set hud_size=" + kv.second + ": small, normal or large (automatic is the machine's answer)", false;
            // the player's text_size: Large's 120 % in a record drawn at Large is taken as its 100 %
            const int chosen = p.size.deck && p.opt.textSize == deckmode::kTextSize ? deckmode::kTextSizeDefault : p.opt.textSize;
            hud2::SizeIn was = p.size;
            const int textWas = p.opt.textSize;
            p.size.deck = preset == hudsize::Large;
            p.size.baseK = hudsize::BaseK(preset, H), p.size.floorH = hudsize::FloorH(preset, H);
            p.opt.textSize = deckmode::TextSize(p.size.deck, chosen);
            lines.push_back("set hud_size " + v + " at a height of " + F(H) + ": size.deck " + (was.deck ? "1" : "0") + " -> " +
                            (p.size.deck ? "1" : "0") + ", size.base_k " + F(was.baseK) + " -> " + F(p.size.baseK) +
                            ", size.floor_h " + F(was.floorH) + " -> " + F(p.size.floorH) + ", opt.text_size " +
                            std::to_string(textWas) + " -> " + std::to_string(p.opt.textSize) + " (the player's text_size taken as " +
                            std::to_string(chosen) + " %)");
            continue;
        }
        Setter s;
        s.want = kv.first, s.value = kv.second;
        hud2state::Fields(s, p);
        if (!s.error.empty())
            return why = s.error, false;
        if (s.done.empty())
            return why = "--set " + kv.first + ": no setting of that name in the frame's copies (hud2_state_io.h: opt.*, rows.*, "
                         "family, size.*; or hud_size)",
                   false;
        for (const auto& d : s.done)
        {
            std::string line = "set " + d.first + " " + d.second;
            // the options page's range (core\ui_meta.h's rows): a value the player cannot choose is drawn, and said
            const std::string leaf = d.first.substr(d.first.find_last_of('.') + 1);
            const uimeta::Row* row = uimeta::FindRow(leaf.c_str());
            char* end = nullptr;
            const double v = strtod(kv.second.c_str(), &end);
            if (row && end && !*end)
            {
                if (row->kind == uimeta::Slider && (v * row->scale < row->min - 1e-6 || v * row->scale > row->max + 1e-6))
                    line += " (OUTSIDE the options page's " + F(row->min / row->scale) + ".." + F(row->max / row->scale) +
                            ": no player can choose it)";
                else if (row->kind == uimeta::Picker && !row->values && (v < 0 || v >= row->optionCount))
                    line += " (OUTSIDE the options page's 0.." + std::to_string(row->optionCount - 1) + ": no player can choose it)";
            }
            lines.push_back(line);
        }
    }
    return true;
}

namespace
{
// Lists every member ApplySet's Setter can reach, by its full path, its value and its kind (the same walk as Setter).
struct Lister
{
    static constexpr bool kSets = false, kFills = false;
    std::string path;
    struct Item
    {
        std::string path, value, kind;
    };
    std::vector<Item> items;

    void Add(const char* k, const std::string& v, const char* kind) { items.push_back({path + k, v, kind}); }
    void operator()(const char* k, bool& x) { Add(k, x ? "1" : "0", "bool"); }
    template <class T> std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>> operator()(const char* k, T& x)
    {
        Add(k, std::to_string(static_cast<long long>(x)), "int");
    }
    template <class T> std::enable_if_t<std::is_floating_point_v<T>> operator()(const char* k, T& x) { Add(k, F(x), "float"); }
    template <class T> std::enable_if_t<std::is_enum_v<T>> operator()(const char* k, T& x)
    {
        Add(k, std::to_string(static_cast<long long>(x)), std::is_same_v<T, glyphs::Family> ? "family" : "int");
    }
    void operator()(const char* k, std::string& x) { Add(k, x, "text"); }
    void operator()(const char*, std::wstring&) {}
    template <class T> std::enable_if_t<std::is_class_v<T> && !hud2state::kIsText<T>> operator()(const char* k, T& x)
    {
        const std::string was = path;
        path += std::string(k) + ".";
        hud2state::Fields(*this, x);
        path = was;
    }
    template <class T, size_t N> void operator()(const char* k, T (&a)[N])
    {
        for (size_t i = 0; i < N; ++i)
            (*this)((std::string(k) + "." + std::to_string(i)).c_str(), a[i]);
    }
    template <class T> void operator()(const char*, std::vector<T>&) {}
    template <class T, class Fn> void group(const char* k, T& x, Fn f)
    {
        const std::string was = path;
        path += std::string(k) + ".";
        f(*this, x);
        path = was;
    }
    template <class T> void note(T&) {}
    void fail(const char*, const char*) {}
};
} // namespace

void SettingList(const hud2::PartsIn& in, std::vector<SettingInfo>& out)
{
    out.clear();
    hud2::PartsIn p = in; // (the walk takes members by reference; nothing is written)
    Lister l;
    hud2state::Fields(l, p);
    auto endsIn = [](const std::string& full, const std::string& s)
    { return full == s || (full.size() > s.size() + 1 && full.compare(full.size() - s.size(), s.size(), s) == 0 && full[full.size() - s.size() - 1] == '.'); };
    std::vector<bool> taken(l.items.size(), false);
    for (size_t i = 0; i < l.items.size(); ++i)
    {
        if (taken[i])
            continue;
        const Lister::Item& it = l.items[i];
        // the name without its first part ("opt.hud_scale" -> "hud_scale") when every copy it would set is <one part>.<name>
        // and they hold one value: one entry sets them all, as `--set hud_scale=` does
        std::string name = it.path;
        std::vector<size_t> group{i};
        const size_t dot = it.path.find('.');
        if (dot != std::string::npos)
        {
            const std::string s = it.path.substr(dot + 1);
            std::vector<size_t> g;
            bool ok = true;
            for (size_t j = 0; j < l.items.size() && ok; ++j)
                if (endsIn(l.items[j].path, s))
                {
                    const size_t d = l.items[j].path.find('.');
                    ok = d != std::string::npos && l.items[j].path.substr(d + 1) == s && l.items[j].value == it.value &&
                         l.items[j].kind == it.kind;
                    g.push_back(j);
                }
            if (ok)
                name = s, group = g;
        }
        for (size_t j : group)
            taken[j] = true;
        SettingInfo si;
        si.name = name, si.value = it.value, si.kind = it.kind, si.copies = static_cast<int>(group.size());
        if (si.kind == "family")
            si.options = {{"xbox", "Xbox"}, {"playstation", "PlayStation"}, {"nintendo", "Nintendo"}, {"steam", "Steam"}},
            si.value = it.value == "1" ? "playstation" : it.value == "2" ? "nintendo" : it.value == "3" ? "steam" : "xbox";
        const std::string leaf = it.path.substr(it.path.find_last_of('.') + 1);
        if (const uimeta::Row* row = uimeta::FindRow(leaf.c_str()))
        {
            si.label = row->label ? row->label : "";
            if (row->kind == uimeta::Slider && row->scale > 0)
                si.range = true, si.min = row->min / row->scale, si.max = row->max / row->scale, si.step = row->step / row->scale;
            else if (row->kind == uimeta::Picker && si.kind == "int" && !row->values) // (the member holds the option's index)
                for (int k = 0; k < row->optionCount; ++k)
                    si.options.push_back({std::to_string(k), row->options ? row->options[k] : std::to_string(k)});
        }
        out.push_back(si);
    }
}

// The preset and the controller setup it adapts to, as StepStart derives them every frame (hud2_frame.cpp; the bench
// starts after StepStart: a variant of hud_preset or the pad setup must reach the model the same way).
static void DerivePreset(const hud2::FrameIn& in, hud2::Model& m)
{
    const uiopt::UiOptions& o = in.parts.opt;
    const int p = hud2preset::Clamp(o.hudPreset);
    m.preset = p, m.presetDrawn = hud2preset::Drawn(p), m.presetData = &hud2preset::Data(p);
    hud2preset::Setup s;
    s.padPreset = o.padPreset, s.weaponGroups = o.weaponGroups, s.holdsAsToggles = o.holdsAsToggles, s.kbm = !in.ctx.pad;
    m.adapt = hud2preset::AdaptTo(p, s);
}

namespace
{
// Scales the float members of a struct named in `xs` by sx and in `ys` by sy (the targeting display's state lists its
// private members only through Fields).
struct Scaler
{
    static constexpr bool kSets = false, kFills = false;
    std::vector<std::string> xs, ys;
    float sx = 1, sy = 1;
    void operator()(const char* k, float& x)
    {
        if (std::find(xs.begin(), xs.end(), k) != xs.end())
            x *= sx;
        else if (std::find(ys.begin(), ys.end(), k) != ys.end())
            x *= sy;
    }
    template <class T> void operator()(const char*, T&) {}
    template <class T, class Fn> void group(const char*, T&, Fn) {}
    template <class T> void note(T&) {}
    void fail(const char*, const char*) {}
};
} // namespace

// The values the record carries in pixels of its own size (last frame's drawn edges, positions in travel, the cursor,
// the world's marks on the screen, the targeting display's places), scaled to the size drawn: one frame after a resize,
// not a settled one.
static void ScaleCarried(hud2::FrameIn& in, hud2::FrameState& st, float sx, float sy)
{
    {
        Scaler s;
        s.sx = sx, s.sy = sy;
        s.xs = {"last_x", "rel_x", "off_x", "shown_x", "card_off", "shown_card_x"}, s.ys = {"last_y", "rel_y", "off_y", "shown_y", "last_mark"};
        hud2state::Fields(s, st.parts.form);
        s.xs = {"caret_x", "block_x", "card_x0"}, s.ys = {"caret_y", "block_y", "card_y0", "mark"};
        hud2state::Fields(s, st.parts.formOut);
    }
    st.navRight *= sx, st.navBottom *= sy;
    st.navScrim[0] *= sx, st.navScrim[1] *= sy, st.navScrim[2] *= sx, st.navScrim[3] *= sy;
    st.navEase.right *= sx, st.navEase.scrimRight *= sx, st.navEase.bottom *= sy, st.navEase.scrimBottom *= sy;
    if (st.noticeX >= 0)
        st.noticeX *= sx;
    if (st.noticeY >= 0)
        st.noticeY *= sy;
    st.radArt[0] *= sx, st.radArt[1] *= sy;
    for (int i = 0; i < 4; ++i)
        st.box[i] *= (i % 2 ? sy : sx), in.box[i] *= (i % 2 ? sy : sx);
    in.mouseX = static_cast<int>(lroundf(in.mouseX * sx)), in.mouseY = static_cast<int>(lroundf(in.mouseY * sy));
    if (st.mouseXWas >= 0)
        st.mouseXWas = static_cast<int>(lroundf(st.mouseXWas * sx)), st.mouseYWas = static_cast<int>(lroundf(st.mouseYWas * sy));
    hud2::Model& m = st.m;
    m.tMark.x *= sx, m.tMark.y *= sy, m.tMark.r *= sy;
    for (auto& mk : m.marks)
        mk.x *= sx, mk.y *= sy;
    if (m.alertYNow >= 0)
        m.alertYNow *= sy;
    for (auto& h : m.held)
        if (h.yNow >= 0)
            h.yNow *= sy;
}

void ReplayVariant(const hud2state::StateDoc& doc, const Overrides& o, Run& run, VariantOut& out)
{
    Unbind();
    run.ok = false, run.error.clear(), run.underDrawn = false, run.hotOn = -1, run.hot.clear(), run.drawPx = 0;
    run.under = kitlist::Pass{}, run.over = kitlist::Pass{}, run.stepOut = hud2::StepOut{}, run.textures.clear();
    out = VariantOut();
    const hud2state::RenderRec& r = doc.render;
    auto fail = [&](const std::string& why)
    {
        kitlist::Take();
        Unbind();
        run.error = why;
    };
    if (doc.update.phase != hud2::Phase::Live)
        return fail("the record's Update was not a live frame (phase " + std::to_string(static_cast<int>(doc.update.phase)) + ")");
    if (!r.drawn || r.sw < 100 || r.sh < 100)
        return fail("the record's Render drew no pass (render.drawn false): nothing to replay");
    const int W = o.w > 0 ? o.w : r.sw, H = o.h > 0 ? o.h : r.sh;
    if (W < 100 || H < 100 || W > 16384 || H > 16384)
        return fail("a size of " + std::to_string(W) + "x" + std::to_string(H) + " (100..16384 a side)");
    out.w = W, out.h = H, out.relayout = W != r.sw || H != r.sh;

    // the settings: both halves' copies
    auto in = std::make_unique<hud2::FrameIn>(doc.in);
    auto draw = std::make_unique<hud2::DrawIn>(doc.draw);
    std::string why;
    std::vector<std::string> again;
    if (!ApplySet(o.set, static_cast<float>(H), in->parts, out.lines, why) ||
        !ApplySet(o.set, static_cast<float>(H), draw->parts, again, why))
        return fail(why);
    for (const auto& kv : o.set)
        if (kv.first == "hud_style" || kv.first == "hud_transition" || kv.first == "opt.hud_style" || kv.first == "rows.hud_style" ||
            kv.first == "opt.hud_transition" || kv.first == "rows.hud_transition")
            out.notes.push_back(kv.first + " is read by StepStart, before the recorded state: it does not change this frame");
    // the preset numbers: copies of the spec's tables with the run's numbers, given to the frame by the host
    if (!o.layout.empty())
    {
        for (int i = 0; i < hp::kPresetCount; ++i)
        {
            g_tables.presets[i] = hp::kPresets[i];
            g_tables.nums[i].assign(hp::kPresets[i].nums, hp::kPresets[i].nums + hp::kPresets[i].numCount);
        }
        for (const LayoutNum& n : o.layout)
        {
            if (n.preset < 0 || n.preset >= hp::kPresetCount || n.index < 0 || n.index >= hp::kPresets[n.preset].numCount)
                return fail("layout number " + n.name + " is not in its table");
            g_tables.nums[n.preset][n.index].v = n.value;
            out.lines.push_back("layout " + n.name + " " + F(n.was) + " -> " + F(n.value));
        }
        for (int i = 0; i < hp::kPresetCount; ++i)
            g_tables.presets[i].nums = g_tables.nums[i].data();
        g_tables.on = true;
    }
    run.stepped = doc.st;
    DerivePreset(*in, run.stepped.m);
    run.stepped.revealRulesOf = -1; // (Focus's reveal rules again from the run's numbers)
    if (out.relayout)
    {
        in->screenW = W, in->screenH = H; // (Step lays out at the size drawn)
        ScaleCarried(*in, run.stepped, static_cast<float>(W) / r.sw, static_cast<float>(H) / r.sh);
    }
    void* dev = kitlist::Device();
    kitlist::Take();
    // the kit as the game's frame would find it at this size: a pass done (Step measures on the last pass's screen), every
    // character the game's atlases had gained
    // (at the recorded size the game's own text size, so the atlases fill in the game's order)
    KitInputs(in->parts, r.iconsReady);
    const int primePx = out.relayout ? hud2::KitTextPx(static_cast<float>(H), doc.st.L.k > 0 ? doc.st.L.k : 1.0f) : r.askPx;
    if (!hud2kit::Begin(dev, W, H, primePx))
        return fail("the kit's Begin failed (no font)");
    if (!r.kitExtra.empty())
        hud2kit::Text(0, 0, r.kitExtra, static_cast<float>(r.askPx), 0xFFFFFFFF);
    hud2kit::End();
    kitlist::Take();

    g_play.lenient = true;
    const hud2::FrameHost host = Host();
    Start(doc.update.tape, "Step");
    hud2::Step(host, *in, run.stepped, run.stepOut);
    Finish();
    out.views = run.stepOut;
    // the host's work between Step and Draw: the target's projection on the screen (a live frame: the game's, scaled; a
    // simulated state's own), the relayout to the back buffer when it is not the layout's
    run.drawn = run.stepped;
    if (doc.st.sim.empty())
    {
        run.drawn.m.tMark = r.st.m.tMark;
        run.drawn.m.tMark.x *= static_cast<float>(W) / r.sw, run.drawn.m.tMark.y *= static_cast<float>(H) / r.sh;
        run.drawn.m.tMark.r *= static_cast<float>(H) / r.sh;
    }
    if (static_cast<int>(run.drawn.L.W) != W || static_cast<int>(run.drawn.L.H) != H)
        hud2::Relayout(host, draw->parts, in->subs, static_cast<float>(W), static_cast<float>(H), draw->combat, run.drawn);
    const int px = hud2::KitTextPx(run.drawn.L.H, run.drawn.L.k);

    // the under pass where the game drew one, from this run's backings
    if (doc.under.drawn)
    {
        const bool back[2] = {doc.under.back[0] && run.drawn.viewBack[0].on, doc.under.back[1] && run.drawn.viewBack[1].on};
        if (back[0] || back[1])
        {
            if (!hud2kit::Begin(dev, W, H, px))
                return fail("the kit's Begin failed for the under pass");
            hud2::DrawUnder(run.drawn, back);
            std::string err;
            if (!fldl::CopyOpenPass(run.under, run.textures, err))
                return fail("the under pass not copied: " + err);
            hud2kit::End();
            run.underDrawn = true;
        }
    }
    KitInputs(draw->parts, r.iconsReady);
    if (!hud2kit::Begin(dev, W, H, px))
        return fail("the kit's Begin failed for the over pass");
    run.drawPx = hud2kit::TextPx();
    Start(r.tape, "Draw");
    g_play.hot = &run.hot;
    hud2::Draw(host, *draw, run.drawn);
    Finish();
    g_play.hot = nullptr;
    run.hotOn = g_play.hotOn;
    hud2kit::Mark("comm.end");
    {
        std::string err;
        if (!fldl::CopyOpenPass(run.over, run.textures, err))
            return fail("the over pass not copied: " + err);
    }
    hud2kit::End();
    kitlist::Take();
    run.error = g_play.error;
    run.ok = run.error.empty();

    // what cannot be true of this picture
    if (g_play.past || g_play.left)
        out.notes.push_back("the clocks: the changed frame asked the host " + std::to_string(g_play.past) +
                            " time(s) past the recorded answers (the last one repeated) and left " + std::to_string(g_play.left) +
                            " unasked: what runs on time (pulses, eases, marquees) stands as at the recorded moment");
    if (out.relayout)
    {
        out.notes.push_back("another size: recorded at " + std::to_string(r.sw) + "x" + std::to_string(r.sh) + ", drawn at " +
                            std::to_string(W) + "x" + std::to_string(H) +
                            ". The text is measured and rasterised anew at this size; the values the record carries in its "
                            "own pixels (the nav's last drawn edge and scrim, the notice's travel, the radiation art's place, the "
                            "cursor, the cargo box, the target's and the world marks' screen places, the alerts' slide, the "
                            "targeting display's places) are scaled by the size ratio, so this is the first frame after a resize, "
                            "not a settled one; the panels' content eases snap to their targets (the game's rule at a new size); "
                            "what the host's model refresh decided from its screen (the list's row count) is the record's");
        out.notes.push_back("the hotspots are the bench's at this size (" + std::to_string(run.hot.size()) +
                            "), not the game's: the record's are of its own size");
        bool sized = false;
        for (const auto& kv : o.set)
            sized = sized || kv.first == "hud_size";
        if (!sized)
            out.notes.push_back("hud_size as the record resolved it at a height of " + std::to_string(r.sh) + " (size.deck " +
                                (doc.in.parts.size.deck ? "1" : "0") + ", base_k " + F(doc.in.parts.size.baseK) + ", floor_h " +
                                F(doc.in.parts.size.floorH) + "): Small at a height of 800 or less reads as Normal there; "
                                "--set hud_size=small|normal|large resolves it at this height");
    }
    for (int i = 0; i < 2; ++i)
    {
        const hud2::ViewRect& v = run.stepOut.view[i];
        if (v.show && v.hw > 0 && v.hh > 0)
            out.notes.push_back(std::string("hole: the game's 3D view of ") + (i ? "the target" : "the ship") + " at (" + F(v.cx - v.hw) +
                                ", " + F(v.cy - v.hh) + ") " + F(2 * v.hw) + " x " + F(2 * v.hh) +
                                ": the game draws it, the bench does not (the list's holes)");
    }
    if (run.drawn.m.comm)
        out.notes.push_back("hole: the comm caption (the game's window text over a talking contact) is not drawn");
    if (doc.under.drawn && !run.underDrawn)
        out.notes.push_back("the game drew an under pass (a view's backing); this variant asks for none");
    Unbind();
}

void DiffLists(const ListRead& before, const ListRead& after, std::vector<std::string>& lines, size_t& changed)
{
    changed = 0;
    if (before.w != after.w || before.h != after.h)
    {
        lines.push_back("the sizes differ (" + std::to_string(before.w) + "x" + std::to_string(before.h) + ", " +
                        std::to_string(after.w) + "x" + std::to_string(after.h) + "): not compared");
        changed = 1;
        return;
    }
    // a vertex by what it is: its place to 0.01 px, its colour, its texture and the place in it (1e-4)
    using Key = std::tuple<long, long, unsigned, std::string, long, long>;
    for (int pi = 0; pi < 2; ++pi)
        for (int li = 0; li < hud2kit::LCount; ++li)
        {
            const ListLayer &a = (pi ? before.over : before.under).layers[li], &b = (pi ? after.over : after.under).layers[li];
            std::map<Key, long> count;
            auto add = [&](const ListLayer& l, long d)
            {
                for (size_t i = 0; i < l.x.size(); ++i)
                    count[Key(lroundf(l.x[i] * 100), lroundf(l.y[i] * 100), l.color[i], l.tex[i], lroundf(l.u[i] * 1e4f),
                              lroundf(l.v[i] * 1e4f))] += d;
            };
            add(a, 1), add(b, -1);
            size_t onlyBefore = 0, onlyAfter = 0;
            float x0 = 1e30f, y0 = 1e30f, x1 = -1e30f, y1 = -1e30f;
            for (const auto& kc : count)
                if (kc.second)
                {
                    (kc.second > 0 ? onlyBefore : onlyAfter) += static_cast<size_t>(kc.second > 0 ? kc.second : -kc.second);
                    const float x = std::get<0>(kc.first) / 100.0f, y = std::get<1>(kc.first) / 100.0f;
                    x0 = std::min(x0, x), y0 = std::min(y0, y), x1 = std::max(x1, x), y1 = std::max(y1, y);
                }
            if (!onlyBefore && !onlyAfter)
                continue;
            changed += onlyBefore + onlyAfter;
            lines.push_back(std::string(pi ? "over" : "under") + " " + kLayer[li] + ": " + std::to_string(onlyBefore) +
                            " vertices only before, " + std::to_string(onlyAfter) + " only after (of " + std::to_string(a.x.size()) +
                            " / " + std::to_string(b.x.size()) + "), within x " + F(x0) + ".." + F(x1) + ", y " + F(y0) + ".." + F(y1));
        }
}
} // namespace benchreplay
