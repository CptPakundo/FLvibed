// FLvibed design bench (build.cmd bench -> build\bench\flbench.exe): HUD 2's draw kit run with no game and no device,
// its passes written as display lists for the replayer (tools\hudbench\web, FORMAT.md). The design:
// ..\..\..\audit_2026-11-12\71b_bench_design_v2.md. Three commands: `card`, the kit's test card (B2); `replay`, frames
// the game recorded run again through HUD 2's Step and Draw and compared with the game's lists, or drawn with settings,
// preset numbers or the size changed (B5a: replay.h, ReplayMain and VariantMain below; README.md); `layout-names`, the
// preset numbers a layout file can change; `diff`, two lists compared as sets of vertices (replay.h DiffLists); `serve`,
// the bench's window's server (serve.cpp; the owner starts it with the shortcut `HUD 2 Bench`, tools\hudbench\app).
//
//   flbench card (--install <game folder> | --synthetic) [--size WxH]... [--out <folder>] [--px <n>]
//                [--family xbox|playstation|nintendo|steam] [--look 0|1|2] [--prints <a game's hud2 fontdump file>]
//
// --install: the fonts from the install's DATA (the game's own resolution, gamefont::LoadFrom) and its flat icons; the
//   output is private (the marker) and goes by default to build\bench\cache\card. --synthetic: block glyphs and disc
//   icons, no game file read, public output in build\bench\synthetic\card. Default sizes 1280x800 and 3840x2160; --px
//   the kit's text size (default HUD 2's 0.026 H).
// Writes per size card@<W>x<H>.fldl.json and card@<W>x<H>.probes.json (pixels the replay must show), and once card_atlas.png /
// .json and card_fontprint.txt (every font set's fingerprint in the game's fontdump format). --prints compares those
// fingerprints with a game's `hud2 fontdump` of the same faces and sizes.
// Exit: 0 written; 1 an error (bad arguments, a list that cannot be named, a file not written); 2 the install refused (no
// game font: the bench never draws with a fallback face); 3 a fingerprint differs from the game's, or none could be
// compared.
#include "fldl_write.h"
#include "kitlist.h"
#include "replay.h"

#include "log.h"

#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

static FILE* g_log = nullptr;
static void LogSink(const char* line)
{
    if (g_log)
        fprintf(g_log, "%s\n", line);
}

static std::string ExeDir()
{
    char p[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, p, MAX_PATH);
    std::string s = p;
    return s.substr(0, s.find_last_of("\\/") + 1);
}

static std::string FirstLine(const std::string& path)
{
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "rb") != 0 || !f)
        return "unknown";
    char b[256] = {};
    const bool got = fgets(b, sizeof b, f) != nullptr;
    fclose(f);
    std::string s = got ? b : "unknown";
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r'))
        s.pop_back();
    return s;
}

static bool ReadText(const std::string& path, std::string& out)
{
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "rb") != 0 || !f)
        return false;
    char b[65536];
    size_t n;
    out.clear();
    while ((n = fread(b, 1, sizeof b, f)) > 0)
        out.append(b, n);
    fclose(f);
    return true;
}

// ---------------------------------------------------------------- the test card
// Every kind of primitive the kit has, in FLvibed's own words (no game text): shapes, a gradient, lines and arcs, the
// two text fonts, a fitted and a clipped text, the game caption's font, big and small icons, a pad glyph, a keycap; an
// under pass with colour writes only; two cut marks. Each probe is a pixel whose colour follows from the kit's blend
// rule over the replay's black, at the centre of a recorded primitive (the kit's own record of where it drew).
struct Card
{
    std::vector<kitlist::Pass> passes;
    std::vector<hud2kit::Drawn> items;
    std::vector<fldl::Probe> probes;
};

static constexpr u32 kRed = 0xFFE02020, kUnder = 0xFF2040A0, kText = 0xFFF0F0F0, kBig = 0xFF40E080, kIcon = 0xFF30C0F0;

static bool DrawCard(int W, int H, int px, Card& card, std::string& error)
{
    const float u = H / 100.0f;
    void* dev = kitlist::Device();
    // the under pass: a view backing, colour writes only (as HUD 2's under pass, hud2.cpp)
    if (!hud2kit::Begin(dev, W, H, px))
        return error = "the kit's Begin failed (no font)", false;
    hud2kit::SetKeepDestAlpha();
    hud2kit::Rect(2 * u, 80 * u, 12 * u, 12 * u, kUnder);
    hud2kit::End();
    // the over pass
    if (!hud2kit::Begin(dev, W, H, px))
        return error = "the kit's Begin failed (no font)", false;
    std::vector<hud2kit::Drawn>& rec = card.items;
    hud2kit::Record(&rec);
    hud2kit::RecordTag("card.rect");
    hud2kit::Rect(2 * u, 2 * u, 10 * u, 10 * u, kRed);
    hud2kit::RecordTag("card.shapes");
    hud2kit::Grad(14 * u, 2 * u, 10 * u, 10 * u, 0xFFFF0000, 0xFF00FF00, 0xFF0000FF, 0xFFFFFFFF);
    hud2kit::Line(26 * u, 2 * u, 36 * u, 12 * u, 0.6f * u, 0xFFFFC040);
    hud2kit::Arc(44 * u, 7 * u, 4 * u, 0, 270, 0.8f * u, 0xFF80C0FF);
    hud2kit::Ring(56 * u, 7 * u, 4 * u, 0.5f * u, 0xFFFF80C0);
    hud2kit::Disc(68 * u, 7 * u, 4 * u, 0xFFC0FF80);
    hud2kit::SegBar(76 * u, 3 * u, 20 * u, 3 * u, 0.6f, 0x82C9F3, 8);
    hud2kit::Mark("card.shapes");
    hud2kit::RecordTag("card.text.small");
    // (an odd count of characters: the run's centre lies inside its middle glyph, not on the seam between two)
    hud2kit::Text(2 * u, 20 * u, L"BENCH CARD 01", static_cast<float>(px), kText);
    hud2kit::RecordTag("card.text.big");
    hud2kit::Text(2 * u, 28 * u, L"MMM", px * 1.6f, kBig);
    hud2kit::RecordTag("card.text.fit");
    hud2kit::TextFit(2 * u, 36 * u, L"A LINE TOO LONG FOR ITS BOX, CUT", static_cast<float>(px), 14 * u, kText);
    hud2kit::RecordTag("card.text.clipped");
    hud2kit::TextClipped(2 * u, 42 * u, L"CLIPPED TICKER TEXT", static_cast<float>(px), 4 * u, 12 * u, kText);
    hud2kit::RecordTag("card.caption");
    hud2kit::GameCaption(2 * u, 48 * u, L"TEST CARD CAPTION", 0xFFE0E0E0);
    hud2kit::RecordTag("card.icon.big");
    hud2kit::Icon(flicons::Cruise, 60 * u, 30 * u, 0.12f * H, kIcon);
    hud2kit::RecordTag("card.icon.small");
    hud2kit::Icon(flicons::Dock, 80 * u, 30 * u, 0.03f * H, kIcon);
    hud2kit::RecordTag("card.glyph");
    hud2kit::Glyph(glyphs::A, 60 * u, 50 * u, 4 * u, 0xFF82C9F3);
    hud2kit::RecordTag("card.keycap");
    hud2kit::Keycap(L"F1", 70 * u, 50 * u, 4 * u, 0xFF82C9F3);
    hud2kit::Mark("card.end");
    hud2kit::Record(nullptr);
    hud2kit::End();
    card.passes = kitlist::Take();
    if (card.passes.size() != 2)
        return error = "the card made " + std::to_string(card.passes.size()) + " passes, not 2", false;

    // probes: an opaque primitive's centre shows its colour; text and icons only on the synthetic source (block glyphs,
    // disc icons: solid at their centre); the under pass writes no alpha, the over pass does
    auto probe = [&](const char* name, float x, float y, u32 argb, int a)
    { card.probes.push_back({name, static_cast<int>(floorf(x)), static_cast<int>(floorf(y)), argb & 0xFFFFFF, 3, a}); };
    probe("under: the backing, no alpha written", 8 * u, 86 * u, kUnder, 0);
    const bool synth = kitlist::Current() == kitlist::Source::Synthetic;
    for (const hud2kit::Drawn& d : rec)
    {
        const float cx = (d.x0 + d.x1) * 0.5f, cy = (d.y0 + d.y1) * 0.5f;
        const std::string tag = d.tag ? d.tag : "";
        if (tag == "card.rect" && d.kind == 'S')
            probe("over: the opaque rectangle", cx, cy, kRed, 255);
        else if (synth && tag == "card.text.small" && d.kind == 'T')
            probe("small text: a block glyph", cx, cy, kText, 255);
        else if (synth && tag == "card.text.big" && d.kind == 'T')
            probe("big text: a block glyph", cx, cy, kBig, 255);
        else if (synth && tag == "card.icon.big" && d.kind == 'I')
            probe("big icon: the disc's centre", cx, cy, kIcon, 255);
        else if (synth && tag == "card.icon.small" && d.kind == 'I')
            probe("small icon: the disc's centre", cx, cy, kIcon, 255);
    }
    const size_t want = synth ? 6 : 2;
    if (card.probes.size() != want)
        return error = "the card recorded " + std::to_string(card.probes.size()) + " probe places, not " + std::to_string(want), false;
    return true;
}

static int Usage()
{
    fprintf(stderr, "usage: flbench card (--install <game folder> | --synthetic) [--size WxH]... [--out <folder>] [--px <n>]\n"
                    "                    [--family xbox|playstation|nintendo|steam] [--look 0|1|2] [--prints <fontdump file>]\n"
                    "       flbench replay --install <game folder> [--out <folder>] [--brief]\n"
                    "                      [--set <setting>=<value>]... [--layout <file.layout.json>] [--size WxH] [--name <variant>]\n"
                    "                      <record.flstate.json | folder>...\n"
                    "       flbench layout-names\n"
                    "       flbench serve [--root <FLvibed folder>] [--install <game folder>] [--parent <pid>] [--portfile <file>] [--port <n>]\n"
                    "       flbench diff <before.fldl.json> <after.fldl.json>\n");
    return 1;
}

// ---------------------------------------------------------------- replay: recorded frames run again, level 1
// Every record named (a folder: every *.flstate.json in it and its subfolders) is replayed (replay.h) and compared with
// the game's list beside it. --out: the bench's lists (<record>.bench.fldl.json), one atlas of every texture the run made
// (replay_atlas.png / .json: the replayer page draws the lists with it) and the report level1.txt.
// Exit: 0 every list equal (holes named); 1 an error (arguments, a file not read or not written); 2 the install refused;
// 3 a font set's fingerprint differs from the game's; 4 a list, a hotspot or Step's state differs; 5 a replay failed (a
// tape that ran out or was left over, a record that drew nothing, a pass not opened).
//
// A variant (--set, --layout, --size: replay.h ReplayVariant): per record the unchanged frame at the size drawn (at the
// recorded size the exact replay, still judged by level 1 against the game's list) and the changed one, both written
// (<record>.unchanged@<W>x<H>.fldl.json, <record>.<variant>@<W>x<H>.fldl.json) with one atlas for both, and what changed between
// them (DiffLists), what cannot be true of the picture and its holes named in the report variant.txt. Exit: 0 drawn (the
// unchanged frame equal to the game's where it is the recorded size); 1, 2 as above; 4 the unchanged frame differs from
// the game's; 5 a replay failed or an override cannot apply.
static void Records(const std::string& path, std::vector<std::string>& out)
{
    const DWORD a = GetFileAttributesA(path.c_str());
    if (a == INVALID_FILE_ATTRIBUTES)
        return;
    if (!(a & FILE_ATTRIBUTE_DIRECTORY))
    {
        out.push_back(path);
        return;
    }
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA((path + "\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE)
        return;
    std::vector<std::string> dirs;
    do
    {
        const std::string n = fd.cFileName;
        if (n == "." || n == "..")
            continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            dirs.push_back(path + "\\" + n);
        else if (n.size() > 13 && n.compare(n.size() - 13, 13, ".flstate.json") == 0)
            out.push_back(path + "\\" + n);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    for (const std::string& d : dirs)
        Records(d, out);
}

// The list's hotspots and holes, which the kit's writer leaves empty (core\ui\hud2_kit_fldl.cpp), and its `relayout`, for
// a variant's list (FORMAT.md): the page shows the 3D views' places hatched and the hotspots on request.
bool AddHolesAndHot(std::string& list, const benchreplay::Run& run, const benchreplay::VariantOut& vo) // (serve.cpp's too)
{
    auto num = [](float f)
    {
        char b[32];
        snprintf(b, sizeof b, "%.2f", f);
        return std::string(b);
    };
    std::string hot, holes;
    for (const auto& h : run.hot)
        hot += std::string(hot.empty() ? "" : ", ") + "{\"name\": \"slot " + std::to_string(h.slot) + "\", \"rect\": [" + num(h.x0) +
               ", " + num(h.y0) + ", " + num(h.x1 - h.x0) + ", " + num(h.y1 - h.y0) + "]}";
    for (int i = 0; i < 2; ++i)
    {
        const hud2::ViewRect& v = vo.views.view[i];
        if (v.hw > 0 && v.hh > 0)
            holes += std::string(holes.empty() ? "" : ", ") + "{\"kind\": \"" + (i ? "view.target" : "view.ship") + "\", \"rect\": [" +
                     num(v.cx - v.hw) + ", " + num(v.cy - v.hh) + ", " + num(2 * v.hw) + ", " + num(2 * v.hh) + "], \"shown\": " +
                     (v.show ? "true" : "false") + "}";
    }
    const std::string tail = "\"hot\": [], \"holes\": []}\n", rel = "\"relayout\": false";
    const size_t at = list.rfind(tail), r = list.find(rel);
    if (at == std::string::npos || at + tail.size() != list.size() || r == std::string::npos)
        return false; // (the writer's form changed: fail rather than write a list half patched)
    list.replace(at, tail.size(), "\"hot\": [" + hot + "], \"holes\": [" + holes + "]}\n");
    if (vo.relayout)
        list.replace(r, rel.size(), "\"relayout\": true");
    return true;
}

// The variant run (--set, --layout, --size): see above ReplayMain.
static int VariantMain(const std::vector<std::string>& records, const benchreplay::Overrides& ov, const std::string& name,
                       const std::string& out, bool brief, const std::string& benchBuild)
{
    auto doc = std::make_unique<hud2state::StateDoc>();
    auto before = std::make_unique<benchreplay::Run>(), after = std::make_unique<benchreplay::Run>();
    int failed = 0, drawn = 0, differing = 0;
    std::string report;
    auto line = [&](const std::string& s)
    {
        report += s + "\n";
        if (!brief || s.rfind("  ", 0) != 0)
            printf("%s\n", s.c_str());
    };
    // one run's list (a variant's with its hotspots, holes and relayout)
    std::string file;
    auto listOf = [&](const benchreplay::Run& run, const benchreplay::VariantOut* vo, const std::string& variant, std::string& list,
                      std::string& err)
    {
        fldl::Meta meta;
        meta.state = doc->state, meta.variant = variant, meta.buildGame = doc->build, meta.buildBench = benchBuild, meta.priv = true;
        meta.record = file, meta.inputs = doc->list.inputs;
        if (!fldl::ListJson(meta, run.underDrawn ? &run.under : nullptr, run.over, {}, list, err))
            return false;
        if (vo && !AddHolesAndHot(list, run, *vo))
            return err = "the list writer's form is not the one the bench patches (hot, holes, relayout)", false;
        return true;
    };
    for (const std::string& path : records)
    {
        const std::string dir = path.substr(0, path.find_last_of("\\/") + 1);
        file = path.substr(dir.size());
        const std::string base = file.substr(0, file.size() - 13);
        std::string text, err, gameList;
        std::vector<hud2state::RetiredMember> retired;
        if (!ReadText(path, text) || !hud2state::Read(text, *doc, &err, &retired))
        {
            ++failed;
            line("FAILED " + file + ": the record does not read: " + (err.empty() ? "cannot open" : err));
            continue;
        }
        const int W = ov.w > 0 ? ov.w : doc->render.sw, H = ov.h > 0 ? ov.h : doc->render.sh;
        const std::string at = "@" + std::to_string(W) + "x" + std::to_string(H);
        const bool recordedSize = W == doc->render.sw && H == doc->render.sh;
        // before: the frame unchanged at this size (the recorded size: the exact replay, judged against the game's list)
        benchreplay::VariantOut voBefore;
        std::string listBefore, listAfter, verdict;
        if (recordedSize)
        {
            benchreplay::Replay(*doc, *before);
            if (before->ok && listOf(*before, nullptr, "", listBefore, err))
            {
                benchreplay::Verdict v;
                if (!ReadText(dir + doc->list.file, gameList))
                    ++v.differences, v.lines.push_back("the game's list " + doc->list.file + " is not beside the record");
                else
                    benchreplay::Compare(*doc, gameList, *before, listBefore, v, &retired);
                verdict = v.differences ? "DIFFERS from the game's (level 1)" : v.fontDifferences ? "FONT differs from the game's" : "EQUAL to the game's";
                differing += v.differences > 0 || v.fontDifferences > 0;
                for (size_t i = 0; i < v.lines.size() && i < 6; ++i)
                    verdict += "\n    " + v.lines[i];
            }
            else if (before->ok)
                before->ok = false, before->error = "the bench's list: " + err;
            benchreplay::Unbind();
            if (!before->ok)
            {
                // the record no longer replays exactly with this code (it asks the host otherwise): drawn leniently, said
                verdict = "NOT EQUAL: the record does not replay exactly with this code (" + before->error +
                          "): drawn leniently; record again (tests\\bench_record.ps1) before trusting the picture";
                ++differing;
                benchreplay::ReplayVariant(*doc, benchreplay::Overrides(), *before, voBefore);
                if (before->ok && !listOf(*before, &voBefore, "unchanged", listBefore, err))
                    before->ok = false, before->error = "the bench's list: " + err;
            }
        }
        else
        {
            benchreplay::Overrides sizeOnly;
            sizeOnly.w = W, sizeOnly.h = H;
            benchreplay::ReplayVariant(*doc, sizeOnly, *before, voBefore);
            if (before->ok && !listOf(*before, &voBefore, "unchanged", listBefore, err))
                before->ok = false, before->error = "the bench's list: " + err;
            verdict = "not judged (another size: no game list to compare)";
        }
        if (!before->ok)
        {
            ++failed;
            line("FAILED " + file + " (unchanged" + at + "): " + before->error);
            continue;
        }
        // after: the variant (a size alone: the unchanged frame is the variant)
        const bool changes = !ov.set.empty() || !ov.layout.empty();
        benchreplay::VariantOut vo;
        if (changes)
        {
            benchreplay::ReplayVariant(*doc, ov, *after, vo);
            if (after->ok && !listOf(*after, &vo, name, listAfter, err))
                after->ok = false, after->error = "the bench's list: " + err;
            if (!after->ok)
            {
                ++failed;
                line("FAILED " + file + " (" + name + at + "): " + after->error);
                continue;
            }
        }
        else
            vo = voBefore;
        if (!out.empty())
        {
            const std::string fb = out + "\\" + base + ".unchanged" + at + ".fldl.json", fa = out + "\\" + base + "." + name + at + ".fldl.json";
            if (!fldl::WriteFile(fb, listBefore) || (changes && !fldl::WriteFile(fa, listAfter)))
            {
                fprintf(stderr, "flbench: cannot write the lists into %s\n", out.c_str());
                return 1;
            }
        }
        ++drawn;
        line("DRAWN " + file + " as " + (changes ? name : std::string("unchanged")) + " at " + std::to_string(W) + "x" + std::to_string(H));
        line("  the unchanged frame: " + verdict);
        for (const std::string& s : vo.lines)
            line("  " + s);
        if (changes)
        {
            benchreplay::ListRead a, b;
            std::vector<std::string> diff;
            size_t changed = 0;
            if (!benchreplay::ReadList(listBefore, a, err) || !benchreplay::ReadList(listAfter, b, err))
                diff.push_back("not compared: " + err), changed = 1;
            else
                benchreplay::DiffLists(a, b, diff, changed);
            line(changed ? "  changed from the unchanged frame:" : "  changed from the unchanged frame: NOTHING (the override is not read in this state, or it equals the spec)");
            for (const std::string& s : diff)
                line("    " + s);
        }
        for (const std::string& s : vo.notes)
            line("  " + s);
    }
    char sum[256];
    snprintf(sum, sizeof sum, "flbench replay (variant %s): %zu records: %d drawn, %d failed%s", name.c_str(), records.size(), drawn,
             failed, differing ? ", the unchanged frame differs from the game's in some" : "");
    report += std::string(sum) + "\n";
    printf("%s\n", sum);
    if (!out.empty())
    {
        std::string error;
        if (!fldl::WriteFile(out + "\\variant.txt", "# " + fldl::Marker() + "\n" + report) || !fldl::WriteAtlas(out + "\\replay_atlas", true, error))
        {
            fprintf(stderr, "flbench: %s\n", error.empty() ? "cannot write the report" : error.c_str());
            return 1;
        }
        printf("flbench: the lists, replay_atlas.png / .json and variant.txt in %s\n", out.c_str());
    }
    return failed ? 5 : differing ? 4 : 0;
}

static int ReplayMain(int argc, char** argv)
{
    std::wstring install;
    std::string out, variant;
    bool brief = false;
    std::vector<std::string> records;
    benchreplay::Overrides ov;
    for (int i = 2; i < argc; ++i)
    {
        const std::string a = argv[i];
        std::string why;
        if (a == "--install" && i + 1 < argc)
        {
            const std::string s = argv[++i];
            install.assign(s.begin(), s.end());
        }
        else if (a == "--out" && i + 1 < argc)
            out = argv[++i];
        else if (a == "--brief")
            brief = true;
        else if (a == "--set" && i + 1 < argc)
        {
            if (!benchreplay::ParseSet(argv[++i], ov, why))
                return fprintf(stderr, "flbench: %s\n", why.c_str()), 1;
        }
        else if (a == "--layout" && i + 1 < argc)
        {
            if (!ov.layout.empty())
                return fprintf(stderr, "flbench: one --layout file a run\n"), 1;
            std::string text;
            const std::string file = argv[++i];
            if (!ReadText(file, text))
                return fprintf(stderr, "flbench: cannot read %s\n", file.c_str()), 1;
            if (!benchreplay::ReadLayout(text, ov.layout, why))
                return fprintf(stderr, "flbench: %s: %s\n", file.c_str(), why.c_str()), 1;
        }
        else if (a == "--size" && i + 1 < argc)
        {
            if (sscanf_s(argv[++i], "%dx%d", &ov.w, &ov.h) != 2 || ov.w < 100 || ov.h < 100 || ov.w > 16384 || ov.h > 16384)
                return fprintf(stderr, "flbench: --size WxH, 100..16384 a side\n"), 1;
        }
        else if (a == "--name" && i + 1 < argc)
        {
            variant = argv[++i];
            for (char c : variant)
                if (!isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_' && c != '.')
                    return fprintf(stderr, "flbench: --name: letters, digits, - _ . only\n"), 1;
            if (variant == "unchanged" || variant == "bench")
                return fprintf(stderr, "flbench: --name %s is the bench's own word for the unchanged frame\n", variant.c_str()), 1;
        }
        else if (a.rfind("--", 0) == 0)
            return Usage();
        else
        {
            const size_t before = records.size();
            Records(a, records);
            if (records.size() == before)
            {
                fprintf(stderr, "flbench: no record at %s\n", a.c_str());
                return 1;
            }
        }
    }
    if (install.empty() || records.empty())
        return Usage();
    if (!out.empty())
    {
        CreateDirectoryA(out.c_str(), nullptr);
        if (fopen_s(&g_log, (out + "\\bench.log").c_str(), "wb") != 0)
            g_log = nullptr;
    }
    LogSetSink(LogSink, ExeDir().c_str());
    std::string why;
    if (const int opened = kitlist::Open(kitlist::Source::Install, install, why))
    {
        fprintf(stderr, "flbench: refused: %s\n", why.c_str());
        return opened;
    }
    const std::string benchBuild = FirstLine(ExeDir() + "build_info.txt");
    if (ov.Any())
        return VariantMain(records, ov, variant.empty() ? "variant" : variant, out, brief, benchBuild);
    auto doc = std::make_unique<hud2state::StateDoc>();
    auto run = std::make_unique<benchreplay::Run>();
    int failed = 0, differing = 0, fonts = 0, equal = 0, exact = 0;
    std::string report;
    std::vector<std::string> holesMet;
    for (const std::string& path : records)
    {
        const std::string dir = path.substr(0, path.find_last_of("\\/") + 1), file = path.substr(dir.size());
        std::string text, err, gameList;
        auto line = [&](const std::string& s)
        {
            report += s + "\n";
            if (!brief || s.rfind("  ", 0) != 0)
                printf("%s\n", s.c_str());
        };
        std::vector<hud2state::RetiredMember> retired;
        if (!ReadText(path, text) || !hud2state::Read(text, *doc, &err, &retired))
        {
            ++failed;
            line("FAILED " + file + ": the record does not read: " + (err.empty() ? "cannot open" : err));
            continue;
        }
        benchreplay::Replay(*doc, *run);
        std::string list;
        fldl::Meta meta;
        meta.state = doc->state, meta.buildGame = doc->build, meta.buildBench = benchBuild, meta.priv = true;
        meta.record = file, meta.inputs = doc->list.inputs;
        if (run->ok && !fldl::ListJson(meta, run->underDrawn ? &run->under : nullptr, run->over, {}, list, err))
            run->ok = false, run->error = "the bench's list: " + err;
        if (!run->ok)
        {
            ++failed;
            line("FAILED " + file + ": " + run->error);
            benchreplay::Unbind();
            continue;
        }
        if (!out.empty())
        {
            const std::string name = file.substr(0, file.size() - 13) + ".bench.fldl.json";
            if (!fldl::WriteFile(out + "\\" + name, list))
            {
                fprintf(stderr, "flbench: cannot write %s\\%s\n", out.c_str(), name.c_str());
                return 1;
            }
        }
        benchreplay::Verdict v;
        if (!ReadText(dir + doc->list.file, gameList))
            ++v.differences, v.lines.push_back("the game's list " + doc->list.file + " is not beside the record");
        else
            benchreplay::Compare(*doc, gameList, *run, list, v, &retired);
        benchreplay::Unbind();
        const char* verdict = v.differences ? "DIFFERS" : v.fontDifferences ? "FONT" : "EQUAL";
        differing += v.differences > 0, fonts += !v.differences && v.fontDifferences > 0, equal += !v.differences && !v.fontDifferences;
        exact += !v.differences && !v.fontDifferences && v.exactOver && v.exactUnder;
        line(std::string(verdict) + " " + file + (v.exactOver && v.exactUnder ? " (bit for bit)" : ""));
        for (const std::string& s : v.lines)
            line("  " + s);
        for (const std::string& s : v.holes)
        {
            line("  hole " + s);
            if (std::find(holesMet.begin(), holesMet.end(), s.substr(0, s.find(':'))) == holesMet.end())
                holesMet.push_back(s.substr(0, s.find(':')));
        }
        for (const std::string& s : v.notes)
            line("  " + s);
    }
    char sum[256];
    snprintf(sum, sizeof sum, "flbench replay: %zu records: %d equal (%d bit for bit), %d differ, %d font, %d failed", records.size(),
             equal, exact, differing, fonts, failed);
    std::string holes;
    for (const std::string& h : holesMet)
        holes += " " + h;
    report += std::string(sum) + "\nholes met:" + (holes.empty() ? " none" : holes) + "\n";
    printf("%s\nholes met:%s\n", sum, holes.empty() ? " none" : holes.c_str());
    if (!out.empty())
    {
        std::string error;
        if (!fldl::WriteFile(out + "\\level1.txt", "# " + fldl::Marker() + "\n" + report) || !fldl::WriteAtlas(out + "\\replay_atlas", true, error))
        {
            fprintf(stderr, "flbench: %s\n", error.empty() ? "cannot write the report" : error.c_str());
            return 1;
        }
        printf("flbench: the bench's lists, replay_atlas.png / .json and level1.txt in %s\n", out.c_str());
    }
    return failed ? 5 : differing ? 4 : fonts ? 3 : 0;
}

int ServeMain(int argc, char** argv); // serve.cpp: the bench's window's server

int main(int argc, char** argv)
{
    if (argc >= 2 && std::string(argv[1]) == "serve")
        return ServeMain(argc, argv);
    if (argc >= 2 && std::string(argv[1]) == "replay")
        return ReplayMain(argc, argv);
    if (argc == 2 && std::string(argv[1]) == "layout-names")
        return printf("%s", benchreplay::LayoutNames().c_str()), 0;
    if (argc == 4 && std::string(argv[1]) == "diff")
    {
        // two lists of one size as sets of vertices: what is only in one of them, per pass and layer, and where
        std::string ta, tb, err;
        benchreplay::ListRead a, b;
        if (!ReadText(argv[2], ta) || !ReadText(argv[3], tb))
            return fprintf(stderr, "flbench: cannot read the lists\n"), 1;
        if (!benchreplay::ReadList(ta, a, err) || !benchreplay::ReadList(tb, b, err))
            return fprintf(stderr, "flbench: %s\n", err.c_str()), 1;
        std::vector<std::string> lines;
        size_t changed = 0;
        benchreplay::DiffLists(a, b, lines, changed);
        for (const std::string& s : lines)
            printf("%s\n", s.c_str());
        printf("flbench diff: %s\n", changed ? "the pictures differ" : "the same picture");
        return changed ? 4 : 0;
    }
    if (argc < 2 || std::string(argv[1]) != "card")
        return Usage();
    std::wstring install;
    bool synthetic = false;
    std::vector<std::pair<int, int>> sizes;
    std::string out, prints;
    int px = 0;
    for (int i = 2; i < argc; ++i)
    {
        const std::string a = argv[i];
        const bool more = i + 1 < argc;
        if (a == "--synthetic")
            synthetic = true;
        else if (a == "--install" && more)
        {
            const std::string s = argv[++i];
            install.assign(s.begin(), s.end()); // (an ASCII path; the harness's installs are)
        }
        else if (a == "--size" && more)
        {
            int w = 0, h = 0;
            if (sscanf_s(argv[++i], "%dx%d", &w, &h) != 2 || w < 64 || h < 64 || w > 16384 || h > 16384)
                return Usage();
            sizes.push_back({w, h});
        }
        else if (a == "--out" && more)
            out = argv[++i];
        else if (a == "--prints" && more)
            prints = argv[++i];
        else if (a == "--px" && more)
            px = atoi(argv[++i]);
        else if (a == "--look" && more)
            kitlist::In().glyphLook = atoi(argv[++i]);
        else if (a == "--family" && more)
        {
            const std::string f = argv[++i];
            kitlist::In().family = f == "playstation" ? glyphs::Family::PlayStation
                                   : f == "nintendo"  ? glyphs::Family::Nintendo
                                   : f == "steam"     ? glyphs::Family::Steam
                                                      : glyphs::Family::Xbox;
        }
        else
            return Usage();
    }
    if (synthetic == !install.empty())
        return Usage();
    if (sizes.empty())
        sizes = {{1280, 800}, {3840, 2160}};
    if (out.empty())
        out = ExeDir() + (synthetic ? "synthetic\\card" : "cache\\card");
    CreateDirectoryA(out.substr(0, out.find_last_of("\\/")).c_str(), nullptr);
    CreateDirectoryA(out.c_str(), nullptr);
    if (fopen_s(&g_log, (out + "\\bench.log").c_str(), "wb") != 0)
        g_log = nullptr;
    LogSetSink(LogSink, ExeDir().c_str());

    std::string why;
    const int opened = kitlist::Open(synthetic ? kitlist::Source::Synthetic : kitlist::Source::Install, install, why);
    if (opened)
    {
        fprintf(stderr, "flbench: refused: %s\n", why.c_str());
        return opened;
    }
    fldl::Meta meta;
    meta.state = "card";
    meta.buildGame = "none";
    meta.buildBench = FirstLine(ExeDir() + "build_info.txt");
    meta.priv = !synthetic;
    for (const auto& s : sizes)
    {
        Card card;
        std::string error, list;
        const int textPx = px > 0 ? px : static_cast<int>(0.026f * s.second + 0.5f);
        if (!DrawCard(s.first, s.second, textPx, card, error) ||
            !fldl::ListJson(meta, &card.passes[0], card.passes[1], card.items, list, error))
        {
            fprintf(stderr, "flbench: %dx%d: %s\n", s.first, s.second, error.c_str());
            return 1;
        }
        const std::string base = out + "\\card@" + std::to_string(s.first) + "x" + std::to_string(s.second);
        if (!fldl::WriteFile(base + ".fldl.json", list) || !fldl::WriteFile(base + ".probes.json", fldl::ProbesJson(card.probes)))
        {
            fprintf(stderr, "flbench: cannot write %s.*\n", base.c_str());
            return 1;
        }
        printf("flbench: %s.fldl.json (%zu probes)\n", base.c_str(), card.probes.size());
    }
    std::string error;
    // (the fingerprints are font-derived: an install's carry the private marker on a first line --prints ignores)
    const std::string printsText = (meta.priv ? "# " + fldl::Marker() + "\n" : std::string()) + fldl::Prints();
    if (!fldl::WriteAtlas(out + "\\card_atlas", meta.priv, error) || !fldl::WriteFile(out + "\\card_fontprint.txt", printsText))
    {
        fprintf(stderr, "flbench: %s\n", error.empty() ? "cannot write the fingerprints" : error.c_str());
        return 1;
    }
    printf("flbench: %s\\card_atlas.png / .json, card_fontprint.txt\n", out.c_str());
    if (!prints.empty())
    {
        std::string dump, diff, missing;
        int differing = 0;
        if (!ReadText(prints, dump))
        {
            fprintf(stderr, "flbench: cannot read %s\n", prints.c_str());
            return 1;
        }
        const int compared = fldl::ComparePrints(dump, diff, missing, differing);
        printf("flbench: fingerprints: %d set(s) compared with the game's, %d differ\n", compared, differing);
        if (!missing.empty())
            printf("  not in the game's dump (not compared):\n%s", missing.c_str());
        if (differing)
            printf("  first differences:\n%s", diff.c_str());
        if (differing || !compared)
            return 3;
    }
    return 0;
}
