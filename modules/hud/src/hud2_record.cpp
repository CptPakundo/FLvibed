// HUD 2's frame recorder (hud2_record.h).
#include "hud2_record.h"

#include "game/fontraster.h"
#include "hud2_state_io.h"
#include "ui/hud2_kit_fldl.h"

#include <istream>
#include <memory>

namespace hud2rec
{
bool g_on = false;

// the recording: what is left, where it goes, what came of it
static int g_left = 0, g_written = 0, g_skipped = 0, g_failed = 0;
static std::string g_folder, g_name, g_build, g_last, g_error;
// the frame being recorded: its record (Update's half done: g_pending), the two passes copied before their End
static std::unique_ptr<hud2state::StateDoc> g_doc;
static bool g_pending = false;
static fldl::Pass g_under, g_over;
static bool g_underCopied = false, g_overCopied = false;
static std::vector<std::unique_ptr<fldl::Texture>> g_textures;
static std::vector<hud2state::LayerPrint> g_underPrints, g_overPrints;
// the game's host the tape's wrappers answer from, and the half being taped
static hud2::FrameHost g_game;
static Half g_half = Half::None;

static std::string Hex(unsigned long long h)
{
    char b[24];
    snprintf(b, sizeof b, "%016llx", h);
    return b;
}

static void Drop()
{
    g_doc.reset();
    g_pending = g_underCopied = g_overCopied = false;
    g_textures.clear();
    g_underPrints.clear(), g_overPrints.clear();
    g_half = Half::None;
}

void Stop(const char* why)
{
    if (!g_on && !g_pending)
        return;
    if (g_left > 0 && why)
        g_error = why;
    g_left = 0, g_on = false;
    Drop();
}

// ---------------------------------------------------------------- the tape
static hud2state::Tape* Taping()
{
    if (!g_doc)
        return nullptr;
    return g_half == Half::Step ? &g_doc->update.tape : g_half == Half::Draw ? &g_doc->render.tape : nullptr;
}
static u32 TickMs()
{
    const u32 v = g_game.tickMs ? g_game.tickMs() : 0;
    if (hud2state::Tape* t = Taping())
        t->tickMs.push_back(v);
    return v;
}
static unsigned long long TickMs64()
{
    const unsigned long long v = g_game.tickMs64 ? g_game.tickMs64() : 0;
    if (hud2state::Tape* t = Taping())
        t->tickMs64.push_back(v);
    return v;
}
static double Seconds()
{
    const double v = g_game.seconds ? g_game.seconds() : 0;
    if (hud2state::Tape* t = Taping())
        t->seconds.push_back(v);
    return v;
}
static bool Typing()
{
    const bool v = g_game.typing && g_game.typing();
    if (hud2state::Tape* t = Taping())
        t->typing.push_back(v ? 1 : 0);
    return v;
}
static int HotHovered()
{
    const int v = g_game.hotHovered ? g_game.hotHovered() : -1;
    if (hud2state::Tape* t = Taping())
        t->hovered.push_back(v);
    return v;
}
static void HotStart(bool on)
{
    if (g_game.hotStart)
        g_game.hotStart(on);
    if (g_doc && g_half == Half::Draw)
        g_doc->render.hotOn = on ? 1 : 0;
}
static void HotRectOf(int slot, float x0, float y0, float x1, float y1)
{
    if (g_game.hotRect)
        g_game.hotRect(slot, x0, y0, x1, y1);
    if (g_doc && g_half == Half::Draw)
        g_doc->render.hot.push_back({slot, x0, y0, x1, y1});
}

hud2::FrameHost Host(const hud2::FrameHost& game)
{
    g_game = game;
    hud2::FrameHost h;
    h.tickMs = TickMs, h.tickMs64 = TickMs64, h.seconds = Seconds, h.typing = Typing;
    h.hotStart = HotStart, h.hotRect = HotRectOf, h.hotHovered = HotHovered;
    return h;
}
void Tape(Half half) { g_half = g_doc ? half : Half::None; }

// ---------------------------------------------------------------- the frame's halves
void BeforeStep(const hud2::FrameIn& in, const hud2::FrameState& st, hud2::Phase phase, const hud2::StartOut& start)
{
    if (g_pending)
        ++g_skipped; // (the last frame's Render never came: dropped)
    Drop();
    g_doc = std::make_unique<hud2state::StateDoc>();
    g_doc->priv = true, g_doc->build = g_build, g_doc->state = g_name, g_doc->frame = static_cast<unsigned>(g_written);
    g_doc->in = in, g_doc->st = st;
    g_doc->update.phase = phase, g_doc->update.start = start;
}

void AfterStep(const hud2::StepOut& out)
{
    if (!g_doc)
        return;
    g_doc->update.step = out;
    g_half = Half::None;
    g_pending = true;
}

// The kit's open pass copied (its textures described) with every layer's kitdump print.
static bool CopyPass(fldl::Pass& to, std::vector<hud2state::LayerPrint>& prints)
{
    std::string err;
    if (!fldl::CopyOpenPass(to, g_textures, err))
    {
        g_error = "pass not copied: " + err;
        return false;
    }
    const hud2kit::OpenPass p = hud2kit::PassNow();
    prints.clear();
    for (int li = 0; li < hud2kit::LCount; ++li)
        prints.push_back({static_cast<unsigned>(p.layers[li].v.size()), Hex(fldl::LayerHash(p.layers[li].v))});
    return true;
}

void Under(bool drawn, int sw, int sh, int px, const bool back[2], const hud2::FrameState& st)
{
    if (!g_pending || !g_doc)
        return;
    hud2state::UnderRec& u = g_doc->under;
    u.drawn = drawn, u.sw = sw, u.sh = sh, u.px = px;
    for (int i = 0; i < 2; ++i)
        u.back[i] = back[i], u.viewBack[i] = st.viewBack[i];
    g_underCopied = drawn && CopyPass(g_under, g_underPrints);
    if (drawn && !g_underCopied)
        ++g_failed, Drop();
}

void BeforeDraw(const hud2::DrawIn& in, const hud2::FrameState& st, int sw, int sh, int askPx, int drawPx, bool iconsReady)
{
    if (!g_pending || !g_doc)
        return;
    g_doc->draw = in;
    hud2state::RenderRec& r = g_doc->render;
    r.drawn = true, r.sw = sw, r.sh = sh, r.askPx = askPx, r.drawPx = drawPx, r.iconsReady = iconsReady;
    r.kitExtra = hud2kit::AddedChars();
    r.st = st;
}

void BeforeEnd()
{
    g_half = Half::None;
    if (!g_pending || !g_doc)
        return;
    g_overCopied = CopyPass(g_over, g_overPrints);
    if (!g_overCopied)
        ++g_failed, Drop();
}

static bool WriteText(const std::string& path, const std::string& text)
{
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "wb") != 0 || !f)
        return false;
    const bool ok = fwrite(text.data(), 1, text.size(), f) == text.size();
    return fclose(f) == 0 && ok;
}

void FrameDone()
{
    if (!g_pending || !g_doc || !g_overCopied)
        return;
    hud2state::StateDoc& d = *g_doc;
    char base[160];
    // (not FORMAT.md's <state>@<W>x<H>: a name@... reads as an e-mail address to the commit hook, and a fixture is committed)
    snprintf(base, sizeof base, "%s_%dx%d_f%u", g_name.c_str(), d.render.sw, d.render.sh, d.frame);
    const std::string recordFile = std::string(base) + ".flstate.json", listFile = std::string(base) + ".fldl.json";
    // the inputs' fingerprint: Render's input and record as the writer writes them, alone in one object
    hud2state::Writer w;
    w.Open(nullptr, '{');
    w("draw_in", d.draw), w("render", d.render);
    w.Close('}', true);
    d.list.inputs = Hex(render::Fnv(w.out.data(), w.out.size()));
    fldl::Meta meta;
    meta.state = g_name, meta.buildGame = d.build, meta.priv = true, meta.record = recordFile, meta.inputs = d.list.inputs;
    std::string list, err;
    if (!fldl::ListJson(meta, g_underCopied ? &g_under : nullptr, g_over, {}, list, err))
    {
        g_error = std::string(base) + ": the list: " + err;
        ++g_failed, Drop();
        return;
    }
    d.list.file = listFile, d.list.hash = Hex(render::Fnv(list.data(), list.size()));
    for (const fldl::Pass* p : {g_underCopied ? &g_under : nullptr, &g_over})
        for (int li = 0; p && li < hud2kit::LCount; ++li)
        {
            const fldl::Texture* t = p->tex[li];
            if (!t || !t->font)
                continue;
            bool seen = false;
            for (const hud2state::SetPrint& s : d.list.sets)
                seen = seen || s.name == t->set;
            if (!seen)
                d.list.sets.push_back({t->set, Hex(render::Fnv(t->print.data(), t->print.size()))});
        }
    d.list.under = g_underCopied ? g_underPrints : std::vector<hud2state::LayerPrint>();
    d.list.over = g_overPrints;
    const std::string record = hud2state::Write(d);
    // the list first: a record names its list, and a record is never left without one
    if (!WriteText(g_folder + "\\" + listFile, list) || !WriteText(g_folder + "\\" + recordFile, record))
    {
        g_error = "cannot write " + g_folder + "\\" + std::string(base) + ".*";
        ++g_failed, Drop();
        Stop(nullptr);
        return;
    }
    g_last = recordFile;
    ++g_written;
    Drop();
    if (--g_left <= 0)
        g_left = 0, g_on = false;
}

// ---------------------------------------------------------------- the verb
static std::string BuildId()
{
    wchar_t exe[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    std::wstring path = exe;
    path = path.substr(0, path.find_last_of(L"\\/") + 1) + L"FLvibed.build.txt";
    FILE* f = nullptr;
    std::string id;
    if (_wfopen_s(&f, path.c_str(), L"rb") == 0 && f)
    {
        char b[128] = {};
        if (fgets(b, sizeof b, f))
            for (const char* c = b; *c && *c != ' ' && *c != '\r' && *c != '\n'; ++c)
                id += *c;
        fclose(f);
    }
    return id.empty() ? "unknown" : id; // (deploy.ps1 writes FLvibed.build.txt beside the DLL)
}

std::string Command(std::istream& in, const std::string& defaultName)
{
    std::string n, folder, name;
    in >> n;
    if (n.empty())
    {
        char b[160];
        snprintf(b, sizeof b, "bench record left=%d written=%d skipped=%d failed=%d pending=%d last=", g_left, g_written,
                 g_skipped, g_failed, g_pending ? 1 : 0);
        return b + (g_last.empty() ? std::string("-") : g_last) + " error=" + (g_error.empty() ? std::string("-") : g_error);
    }
    const int frames = atoi(n.c_str());
    if (frames <= 0)
    {
        Stop("stopped");
        return "bench record off";
    }
    in >> std::ws;
    if (in.peek() == '"')
        in.get(), std::getline(in, folder, '"');
    else
        in >> folder;
    in >> name;
    if (frames > 200 || folder.empty())
        return "hud2 bench record <1..200> <folder> [<name>] | 0 | (bare: the state)";
    for (size_t at = folder.find_first_of("\\/", 3); ; at = folder.find_first_of("\\/", at + 1)) // (each folder on the way)
    {
        CreateDirectoryA(folder.substr(0, at).c_str(), nullptr);
        if (at == std::string::npos)
            break;
    }
    const DWORD attr = GetFileAttributesA(folder.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY))
        return "bench record: cannot make " + folder;
    Stop(nullptr);
    g_folder = folder, g_name = name.empty() ? defaultName : name, g_build = BuildId();
    g_written = g_skipped = g_failed = 0, g_last.clear(), g_error.clear();
    g_left = frames, g_on = true;
    return "bench record: the next " + std::to_string(frames) + " frames of " + g_name + " -> " + folder + " (build " + g_build + ")";
}
} // namespace hud2rec
