// FLvibed design bench - `flbench serve`: the bench as a program that stays running and answers its window (the owner's
// requirement of 13 Nov 2026: started with a double-click, closed with the window, no script, no prompt). A small HTTP
// server bound to 127.0.0.1 only, on a port the system picks, single-threaded: it serves the bench's page
// (tools\hudbench\web\bench.html and the replayer's scripts) and answers its requests in memory with the same replay as
// `flbench replay` (replay.h): the recorded states it finds, the settings a record carries, the layout numbers, a draw
// (the unchanged frame and the changed one, what changed and what cannot be true), and the saved variants. The launcher
// (tools\hudbench\app\bench_app.ps1) starts it and ends it when the window closes; the server also ends by itself,
// whichever comes first, when the launcher is gone (--parent), the window's process is gone (<portfile>.window), the
// page said goodbye (/api/bye) or stopped saying it is there (no /api/ping for 15 s): closing does not depend on a
// browser's process name.
//
//   flbench serve [--root <FLvibed folder>] [--install <game folder>] [--parent <pid>] [--portfile <file>] [--port <n>]
//
// Nothing leaves the machine: the socket listens on 127.0.0.1, the page's own policy allows only its own address.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

#include "fldl_write.h"
#include "kitlist.h"
#include "replay.h"

#include "log.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <map>
#include <memory>
#include <string>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

// bench.cpp's: a variant list's hotspots, holes and relayout
bool AddHolesAndHot(std::string& list, const benchreplay::Run& run, const benchreplay::VariantOut& vo);

namespace
{
std::string g_root, g_web, g_tmp, g_benchBuild;
int g_atlasGen = 0;
std::string g_atlasText;

bool ReadFile(const std::string& path, std::string& out)
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

std::string Json(const std::string& s)
{
    std::string o = "\"";
    for (unsigned char c : s)
    {
        if (c == '"' || c == '\\')
            o += '\\', o += static_cast<char>(c);
        else if (c == '\n')
            o += "\\n";
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
std::string JsonList(const std::vector<std::string>& v)
{
    std::string o = "[";
    for (size_t i = 0; i < v.size(); ++i)
        o += (i ? ", " : "") + Json(v[i]);
    return o + "]";
}

std::string UrlDecode(const std::string& s)
{
    std::string o;
    for (size_t i = 0; i < s.size(); ++i)
        if (s[i] == '%' && i + 2 < s.size())
            o += static_cast<char>(strtol(s.substr(i + 1, 2).c_str(), nullptr, 16)), i += 2;
        else
            o += s[i] == '+' ? ' ' : s[i];
    return o;
}
std::string Query(const std::string& q, const std::string& key)
{
    size_t at = 0;
    while (at < q.size())
    {
        size_t amp = q.find('&', at);
        if (amp == std::string::npos)
            amp = q.size();
        const std::string kv = q.substr(at, amp - at);
        const size_t eq = kv.find('=');
        if (kv.substr(0, eq) == key)
            return eq == std::string::npos ? "" : UrlDecode(kv.substr(eq + 1));
        at = amp + 1;
    }
    return "";
}

long long MTime(const WIN32_FIND_DATAA& fd)
{
    ULARGE_INTEGER u;
    u.LowPart = fd.ftLastWriteTime.dwLowDateTime, u.HighPart = fd.ftLastWriteTime.dwHighDateTime;
    return static_cast<long long>(u.QuadPart / 10000000ULL) - 11644473600LL; // (seconds since 1970)
}

// ---------------------------------------------------------------- the recorded states it finds
struct Rec
{
    std::string rel, run, size, state;
    int frame = 0;
    long long runTime = 0, crowd = 0;
};
void FindRecords(const std::string& base, const std::string& relBase, const std::string& run, long long runTime, std::vector<Rec>& out,
                 int depth)
{
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA((g_root + base + "\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE)
        return;
    std::vector<std::pair<std::string, long long>> dirs;
    std::map<std::string, long long> sizes;
    std::vector<std::string> recs;
    do
    {
        const std::string n = fd.cFileName;
        if (n == "." || n == "..")
            continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            dirs.push_back({n, MTime(fd)});
        else
        {
            sizes[n] = static_cast<long long>(fd.nFileSizeLow) + (static_cast<long long>(fd.nFileSizeHigh) << 32);
            if (n.size() > 13 && n.compare(n.size() - 13, 13, ".flstate.json") == 0)
                recs.push_back(n);
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    for (const std::string& n : recs)
    {
        // <state>_<W>x<H>_f<N>.flstate.json
        Rec r;
        r.rel = relBase + "\\" + n, r.run = run, r.runTime = runTime;
        std::string stem = n.substr(0, n.size() - 13);
        const size_t f = stem.rfind("_f");
        if (f != std::string::npos)
            r.frame = atoi(stem.c_str() + f + 2), stem = stem.substr(0, f);
        const size_t u = stem.rfind('_');
        if (u != std::string::npos && stem.find('x', u) != std::string::npos)
            r.size = stem.substr(u + 1), stem = stem.substr(0, u);
        r.state = stem;
        // how crowded: the game's list of the frame (its size in bytes), else the record's
        const std::string game = n.substr(0, n.size() - 13) + ".fldl.json";
        r.crowd = sizes.count(game) ? sizes[game] : sizes[n];
        out.push_back(r);
    }
    if (depth < 3)
        for (const auto& d : dirs)
            FindRecords(base + "\\" + d.first, relBase + "\\" + d.first, run.empty() ? d.first : run, run.empty() ? d.second : runTime, out,
                        depth + 1);
}
std::string RecordsJson()
{
    std::vector<Rec> recs;
    FindRecords("research\\experience\\bench\\record", "research\\experience\\bench\\record", "", 0, recs, 0);
    std::vector<Rec> tests;
    FindRecords("tests\\bench", "tests\\bench", "", 0, tests, 0);
    for (Rec& r : tests)
        if (r.run.empty())
            r.run = "tests\\bench", r.runTime = 0;
    recs.insert(recs.end(), tests.begin(), tests.end());
    std::string o = "[";
    for (size_t i = 0; i < recs.size(); ++i)
    {
        const Rec& r = recs[i];
        o += std::string(i ? ",\n" : "") + "{\"rel\": " + Json(r.rel) + ", \"run\": " + Json(r.run) + ", \"runTime\": " + std::to_string(r.runTime) +
             ", \"size\": " + Json(r.size) + ", \"state\": " + Json(r.state) + ", \"frame\": " + std::to_string(r.frame) +
             ", \"crowd\": " + std::to_string(r.crowd) + "}";
    }
    return o + "]";
}

// ---------------------------------------------------------------- a record (read once, kept)
std::map<std::string, std::unique_ptr<hud2state::StateDoc>> g_docs;
bool SafeRel(const std::string& rel)
{
    return !rel.empty() && rel.find("..") == std::string::npos && rel.find(':') == std::string::npos && rel[0] != '\\' && rel[0] != '/' &&
           rel.size() > 13 && rel.compare(rel.size() - 13, 13, ".flstate.json") == 0;
}
const hud2state::StateDoc* Doc(const std::string& rel, std::string& why)
{
    if (!SafeRel(rel))
        return why = "not a recorded state: " + rel, nullptr;
    auto it = g_docs.find(rel);
    if (it != g_docs.end())
        return it->second.get();
    std::string text;
    auto doc = std::make_unique<hud2state::StateDoc>();
    if (!ReadFile(g_root + rel, text))
        return why = "cannot read " + rel, nullptr;
    if (!hud2state::Read(text, *doc, &why))
        return why = rel + " does not read: " + why, nullptr;
    return (g_docs[rel] = std::move(doc)).get();
}

std::string SettingsJson(const hud2state::StateDoc& doc)
{
    std::vector<benchreplay::SettingInfo> list;
    benchreplay::SettingList(doc.in.parts, list);
    std::string o = "[";
    for (size_t i = 0; i < list.size(); ++i)
    {
        const auto& s = list[i];
        o += std::string(i ? ",\n" : "") + "{\"name\": " + Json(s.name) + ", \"value\": " + Json(s.value) + ", \"kind\": " + Json(s.kind) +
             ", \"label\": " + Json(s.label) + ", \"copies\": " + std::to_string(s.copies);
        if (s.range)
        {
            char b[128];
            snprintf(b, sizeof b, ", \"min\": %g, \"max\": %g, \"step\": %g", s.min, s.max, s.step);
            o += b;
        }
        if (!s.options.empty())
        {
            o += ", \"options\": [";
            for (size_t k = 0; k < s.options.size(); ++k)
                o += std::string(k ? ", " : "") + "[" + Json(s.options[k].first) + ", " + Json(s.options[k].second) + "]";
            o += "]";
        }
        o += "}";
    }
    return o + "]";
}

std::string LayoutJson()
{
    // `flbench layout-names`' lines: "<name> = <value>"
    const std::string text = benchreplay::LayoutNames();
    std::string o = "[";
    size_t at = 0;
    bool first = true;
    while (at < text.size())
    {
        size_t nl = text.find('\n', at);
        if (nl == std::string::npos)
            nl = text.size();
        const std::string line = text.substr(at, nl - at);
        at = nl + 1;
        const size_t eq = line.find(" = ");
        if (eq == std::string::npos)
            continue;
        o += std::string(first ? "" : ",\n") + "[" + Json(line.substr(0, eq)) + ", " + Json(line.substr(eq + 3)) + "]";
        first = false;
    }
    return o + "]";
}

// ---------------------------------------------------------------- a draw
// The request's lines: "rec <path>", "size WxH", "set <name>=<value>", "layout <name>=<value>" (the same lines a saved
// variant holds).
struct Ask
{
    std::string rec;
    benchreplay::Overrides ov;
    std::vector<std::pair<std::string, std::string>> layoutKv;
};
bool ParseAsk(const std::string& body, Ask& a, std::string& why)
{
    size_t at = 0;
    while (at < body.size())
    {
        size_t nl = body.find('\n', at);
        if (nl == std::string::npos)
            nl = body.size();
        std::string line = body.substr(at, nl - at);
        at = nl + 1;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
            line.pop_back();
        if (line.empty() || line[0] == '#')
            continue;
        const size_t sp = line.find(' ');
        const std::string k = line.substr(0, sp), v = sp == std::string::npos ? "" : line.substr(sp + 1);
        if (k == "rec")
            a.rec = v;
        else if (k == "size")
        {
            if (sscanf_s(v.c_str(), "%dx%d", &a.ov.w, &a.ov.h) != 2 || a.ov.w < 100 || a.ov.h < 100 || a.ov.w > 16384 || a.ov.h > 16384)
                return why = "size " + v + ": WxH, 100..16384 a side", false;
        }
        else if (k == "set")
        {
            if (!benchreplay::ParseSet(v, a.ov, why))
                return false;
        }
        else if (k == "layout")
        {
            const size_t eq = v.find('=');
            char* end = nullptr;
            const std::string num = eq == std::string::npos ? "" : v.substr(eq + 1);
            strtod(num.c_str(), &end);
            if (num.empty() || !end || *end)
                return why = "layout " + v + ": <name>=<number>", false;
            a.layoutKv.push_back({v.substr(0, eq), num});
        }
        else
            return why = "unknown line: " + line, false;
    }
    if (!a.layoutKv.empty())
    {
        std::string text = "{";
        for (size_t i = 0; i < a.layoutKv.size(); ++i)
            text += (i ? ", " : "") + Json(a.layoutKv[i].first) + ": " + a.layoutKv[i].second;
        text += "}";
        if (!benchreplay::ReadLayout(text, a.ov.layout, why))
            return false;
    }
    return true;
}

std::string ListOf(const hud2state::StateDoc& doc, const std::string& rel, const benchreplay::Run& run, const benchreplay::VariantOut& vo,
                   const std::string& variant, bool patch, std::string& err)
{
    fldl::Meta meta;
    meta.state = doc.state, meta.variant = variant, meta.buildGame = doc.build, meta.buildBench = g_benchBuild, meta.priv = true;
    meta.record = rel.substr(rel.find_last_of('\\') + 1), meta.inputs = doc.list.inputs;
    std::string list;
    if (!fldl::ListJson(meta, run.underDrawn ? &run.under : nullptr, run.over, {}, list, err))
        return "";
    if (patch && !AddHolesAndHot(list, run, vo))
        return err = "the list writer's form is not the one the bench patches", "";
    return list;
}

std::string Draw(const std::string& body)
{
    const auto t0 = std::chrono::steady_clock::now();
    Ask a;
    std::string why;
    if (!ParseAsk(body, a, why))
        return "{\"ok\": false, \"error\": " + Json(why) + "}";
    const hud2state::StateDoc* doc = Doc(a.rec, why);
    if (!doc)
        return "{\"ok\": false, \"error\": " + Json(why) + "}";
    static auto before = std::make_unique<benchreplay::Run>(), after = std::make_unique<benchreplay::Run>();
    const int W = a.ov.w > 0 ? a.ov.w : doc->render.sw, H = a.ov.h > 0 ? a.ov.h : doc->render.sh;
    const bool recordedSize = W == doc->render.sw && H == doc->render.sh;
    const bool changes = !a.ov.set.empty() || !a.ov.layout.empty();
    std::vector<std::string> lines, notes, diff;
    std::string verdict, listBefore, listAfter, err;
    benchreplay::VariantOut voBefore, vo;
    // before: the frame unchanged (at the recorded size the exact replay, judged against the game's list)
    if (recordedSize)
    {
        benchreplay::Replay(*doc, *before);
        if (before->ok)
        {
            std::string plain = ListOf(*doc, a.rec, *before, voBefore, "", false, err), gameList;
            benchreplay::Verdict v;
            const std::string dir = a.rec.substr(0, a.rec.find_last_of('\\') + 1);
            if (plain.empty())
                ++v.differences, v.lines.push_back("the bench's list: " + err);
            else if (!ReadFile(g_root + dir + doc->list.file, gameList))
                v.notes.push_back("no game list beside the record: not compared");
            else
                benchreplay::Compare(*doc, gameList, *before, plain, v);
            verdict = v.differences ? "DIFFERS from what the game drew" : v.fontDifferences ? "its font differs from the game's"
                      : gameList.empty()                                ? "not compared (no game list)"
                                                                        : "EQUAL to what the game drew";
            for (size_t i = 0; i < v.lines.size() && i < 6; ++i)
                lines.push_back(v.lines[i]);
            voBefore.views = before->stepOut;
            listBefore = ListOf(*doc, a.rec, *before, voBefore, "", true, err);
            if (listBefore.empty())
                listBefore = plain;
        }
        benchreplay::Unbind();
        if (!before->ok)
        {
            verdict = "NOT EQUAL: this record does not replay exactly with today's code (" + before->error + "); drawn leniently, record again";
            benchreplay::ReplayVariant(*doc, benchreplay::Overrides(), *before, voBefore);
            if (before->ok)
                listBefore = ListOf(*doc, a.rec, *before, voBefore, "unchanged", true, err);
        }
    }
    else
    {
        benchreplay::Overrides sizeOnly;
        sizeOnly.w = W, sizeOnly.h = H;
        benchreplay::ReplayVariant(*doc, sizeOnly, *before, voBefore);
        if (before->ok)
            listBefore = ListOf(*doc, a.rec, *before, voBefore, "unchanged", true, err);
        verdict = "not compared (drawn at another size than recorded)";
        for (const std::string& s : voBefore.notes)
            notes.push_back(s);
    }
    if (!before->ok || listBefore.empty())
        return "{\"ok\": false, \"error\": " + Json("the unchanged frame: " + (before->ok ? err : before->error)) + "}";
    if (changes)
    {
        benchreplay::ReplayVariant(*doc, a.ov, *after, vo);
        if (!after->ok)
            return "{\"ok\": false, \"error\": " + Json(after->error) + "}";
        listAfter = ListOf(*doc, a.rec, *after, vo, "variant", true, err);
        if (listAfter.empty())
            return "{\"ok\": false, \"error\": " + Json("the changed frame: " + err) + "}";
        for (const std::string& s : vo.lines)
            lines.push_back(s);
        notes = vo.notes;
        benchreplay::ListRead ra, rb;
        size_t changed = 0;
        if (benchreplay::ReadList(listBefore, ra, err) && benchreplay::ReadList(listAfter, rb, err))
            benchreplay::DiffLists(ra, rb, diff, changed);
        else
            diff.push_back("not compared: " + err);
        if (!changed && diff.empty())
            diff.push_back("NOTHING changed: the change is not read in this state, or it equals the recorded value");
    }
    // the atlas: written when the kit made new pictures (a new text size, new glyphs), so the page reloads it only then
    if (!fldl::WriteAtlas(g_tmp + "\\replay_atlas", true, err))
        return "{\"ok\": false, \"error\": " + Json("the atlas: " + err) + "}";
    std::string atlasText;
    ReadFile(g_tmp + "\\replay_atlas.json", atlasText);
    if (atlasText != g_atlasText)
        g_atlasText = atlasText, ++g_atlasGen;
    const long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
    return "{\"ok\": true, \"w\": " + std::to_string(W) + ", \"h\": " + std::to_string(H) + ", \"recorded\": " +
           Json(std::to_string(doc->render.sw) + "x" + std::to_string(doc->render.sh)) + ", \"relayout\": " + (recordedSize ? "false" : "true") +
           ", \"verdict\": " + Json(verdict) + ", \"lines\": " + JsonList(lines) + ", \"diff\": " + JsonList(diff) + ", \"notes\": " +
           JsonList(notes) + ", \"atlas\": " + std::to_string(g_atlasGen) + ", \"ms\": " + std::to_string(ms) + ",\n\"before\": " + listBefore +
           ",\n\"after\": " + (changes ? listAfter : std::string("null")) + "}";
}

// ---------------------------------------------------------------- saved variants: research\experience\bench\variants\<name>.variant.txt
std::string VariantDir() { return g_root + "research\\experience\\bench\\variants"; }
bool SafeName(const std::string& n)
{
    if (n.empty() || n.size() > 80)
        return false;
    for (char c : n)
        if (!isalnum(static_cast<unsigned char>(c)) && c != ' ' && c != '-' && c != '_')
            return false;
    return n[0] != ' ';
}
std::string VariantsJson()
{
    std::vector<std::pair<long long, std::string>> v;
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA((VariantDir() + "\\*.variant.txt").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE)
    {
        do
            v.push_back({MTime(fd), std::string(fd.cFileName).substr(0, strlen(fd.cFileName) - 12)});
        while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    std::sort(v.begin(), v.end(), [](const auto& x, const auto& y) { return x.first > y.first; });
    std::string o = "[";
    for (size_t i = 0; i < v.size(); ++i)
        o += std::string(i ? ", " : "") + "{\"name\": " + Json(v[i].second) + ", \"time\": " + std::to_string(v[i].first) + "}";
    return o + "]";
}

// ---------------------------------------------------------------- HTTP
struct Reply
{
    int code = 200;
    std::string type = "application/json; charset=utf-8", body;
};
std::chrono::steady_clock::time_point g_lastPing, g_byeAt;
bool g_pinged = false, g_bye = false;

Reply Handle(const std::string& method, const std::string& target, const std::string& body)
{
    Reply r;
    const size_t q = target.find('?');
    const std::string path = target.substr(0, q), query = q == std::string::npos ? "" : target.substr(q + 1);
    auto fail = [&](int code, const std::string& why)
    {
        r.code = code, r.body = "{\"ok\": false, \"error\": " + Json(why) + "}";
        return r;
    };
    if (path == "/api/ping")
    {
        g_pinged = true, g_lastPing = std::chrono::steady_clock::now(), g_bye = false;
        r.body = "{\"ok\": true}";
        return r;
    }
    if (path == "/api/bye") // (the page is closing: the end in 3 s unless a ping says it was a reload)
    {
        g_bye = true, g_byeAt = std::chrono::steady_clock::now();
        r.body = "{\"ok\": true}";
        return r;
    }
    if (path == "/api/records")
        return r.body = RecordsJson(), r;
    if (path == "/api/layout")
        return r.body = LayoutJson(), r;
    if (path == "/api/settings")
    {
        std::string why;
        const hud2state::StateDoc* doc = Doc(Query(query, "rec"), why);
        if (!doc)
            return fail(404, why);
        return r.body = SettingsJson(*doc), r;
    }
    if (path == "/api/draw" && method == "POST")
        return r.body = Draw(body), r;
    if (path == "/api/variants")
        return r.body = VariantsJson(), r;
    if (path == "/api/variant")
    {
        const std::string name = Query(query, "name");
        std::string text;
        if (!SafeName(name) || !ReadFile(VariantDir() + "\\" + name + ".variant.txt", text))
            return fail(404, "no saved variant " + name);
        r.type = "text/plain; charset=utf-8", r.body = text;
        return r;
    }
    if (path == "/api/save" && method == "POST")
    {
        const std::string name = Query(query, "name");
        if (!SafeName(name))
            return fail(400, "a name of letters, digits, spaces, - and _ (up to 80)");
        Ask a;
        std::string why;
        if (!ParseAsk(body, a, why) || !SafeRel(a.rec))
            return fail(400, why.empty() ? "no recorded state" : why);
        CreateDirectoryA((g_root + "research\\experience\\bench").c_str(), nullptr);
        CreateDirectoryA(VariantDir().c_str(), nullptr);
        if (!fldl::WriteFile(VariantDir() + "\\" + name + ".variant.txt",
                             "# a design bench variant (flbench serve): the recorded state, the screen size, the settings and the layout numbers changed\n" + body))
            return fail(500, "cannot write the variant");
        r.body = "{\"ok\": true}";
        return r;
    }
    if (path == "/replay_atlas.json" || path == "/replay_atlas.png")
    {
        if (!ReadFile(g_tmp + "\\" + path.substr(1), r.body))
            return fail(404, "no atlas yet");
        r.type = path.back() == 'g' ? "image/png" : "application/json";
        return r;
    }
    // the page's own files (tools\hudbench\web): a plain name, no folder
    std::string file = path == "/" ? "bench.html" : path.substr(1);
    for (char c : file)
        if (!(isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '.' || c == '-'))
            return fail(404, "not found");
    const size_t dot = file.rfind('.');
    const std::string ext = dot == std::string::npos ? "" : file.substr(dot);
    if (ext == ".html")
        r.type = "text/html; charset=utf-8";
    else if (ext == ".js")
        r.type = "text/javascript; charset=utf-8";
    else if (ext == ".css")
        r.type = "text/css; charset=utf-8";
    else
        return fail(404, "not found");
    if (file.find("..") != std::string::npos || !ReadFile(g_web + file, r.body))
        return fail(404, "not found");
    return r;
}

struct Client
{
    SOCKET s = INVALID_SOCKET;
    std::string in;
};
// a whole request in `in`: method, target, body (false: not yet)
bool Complete(const std::string& in, std::string& method, std::string& target, std::string& body)
{
    const size_t end = in.find("\r\n\r\n");
    if (end == std::string::npos)
        return false;
    const std::string head = in.substr(0, end);
    size_t len = 0;
    for (size_t at = head.find("\r\n"); at != std::string::npos; at = head.find("\r\n", at + 2))
    {
        std::string line = head.substr(at + 2, head.find("\r\n", at + 2) - at - 2);
        for (size_t i = 0; i < line.size() && line[i] != ':'; ++i)
            line[i] = static_cast<char>(tolower(static_cast<unsigned char>(line[i])));
        if (line.rfind("content-length:", 0) == 0)
            len = static_cast<size_t>(strtoul(line.c_str() + 15, nullptr, 10));
    }
    if (in.size() < end + 4 + len)
        return false;
    const size_t sp1 = head.find(' '), sp2 = head.find(' ', sp1 + 1);
    method = head.substr(0, sp1), target = head.substr(sp1 + 1, sp2 - sp1 - 1), body = in.substr(end + 4, len);
    return true;
}
void Send(SOCKET s, const Reply& r)
{
    const char* text = r.code == 200 ? "OK" : r.code == 400 ? "Bad Request" : r.code == 404 ? "Not Found" : "Error";
    std::string head = "HTTP/1.1 " + std::to_string(r.code) + " " + text + "\r\nContent-Type: " + r.type + "\r\nContent-Length: " +
                       std::to_string(r.body.size()) + "\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n";
    const std::string all = head + r.body;
    u_long blocking = 0;
    ioctlsocket(s, FIONBIO, &blocking);
    size_t sent = 0;
    while (sent < all.size())
    {
        const int n = send(s, all.data() + sent, static_cast<int>(std::min<size_t>(all.size() - sent, 1 << 20)), 0);
        if (n <= 0)
            break;
        sent += static_cast<size_t>(n);
    }
    shutdown(s, SD_SEND);
}
} // namespace

int ServeMain(int argc, char** argv)
{
    std::string install, portfile;
    int port = 0;
    DWORD parent = 0;
    {
        char p[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, p, MAX_PATH);
        std::string s = p;
        s = s.substr(0, s.find_last_of("\\/")); // build\bench
        g_root = s.substr(0, s.find_last_of("\\/"));
        g_root = g_root.substr(0, g_root.find_last_of("\\/") + 1); // the FLvibed folder
        g_tmp = s + "\\serve_" + std::to_string(GetCurrentProcessId());
        std::string b;
        if (ReadFile(s + "\\build_info.txt", b))
            g_benchBuild = b.substr(0, b.find_first_of("\r\n"));
    }
    for (int i = 2; i < argc; ++i)
    {
        const std::string a = argv[i];
        if (a == "--root" && i + 1 < argc)
            g_root = std::string(argv[++i]) + "\\";
        else if (a == "--install" && i + 1 < argc)
            install = argv[++i];
        else if (a == "--parent" && i + 1 < argc)
            parent = static_cast<DWORD>(strtoul(argv[++i], nullptr, 10));
        else if (a == "--portfile" && i + 1 < argc)
            portfile = argv[++i];
        else if (a == "--port" && i + 1 < argc)
            port = atoi(argv[++i]);
        else
            return fprintf(stderr, "usage: flbench serve [--root <FLvibed folder>] [--install <game folder>] [--parent <pid>] [--portfile <file>] [--port <n>]\n"), 1;
    }
    if (install.empty())
        install = g_root + "Install\\Freelancer HD Edition";
    g_web = g_root + "tools\\hudbench\\web\\";
    CreateDirectoryA(g_tmp.c_str(), nullptr);
    LogSetSink([](const char*) {}, (g_tmp + "\\").c_str()); // (the kit's log lines are not the window's business)
    std::string why;
    const std::wstring winstall(install.begin(), install.end());
    if (const int opened = kitlist::Open(kitlist::Source::Install, winstall, why))
        return fprintf(stderr, "flbench serve: refused: %s\n", why.c_str()), opened;
    HANDLE parentHandle = parent ? OpenProcess(SYNCHRONIZE, FALSE, parent) : nullptr, windowHandle = nullptr;

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        return fprintf(stderr, "flbench serve: no sockets\n"), 1;
    SOCKET ls = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<u_short>(port));
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr); // (this machine only)
    if (ls == INVALID_SOCKET || bind(ls, reinterpret_cast<sockaddr*>(&addr), sizeof addr) != 0 || listen(ls, 16) != 0)
        return fprintf(stderr, "flbench serve: cannot listen on 127.0.0.1:%d\n", port), 1;
    int alen = sizeof addr;
    getsockname(ls, reinterpret_cast<sockaddr*>(&addr), &alen);
    port = ntohs(addr.sin_port);
    printf("flbench serve: http://127.0.0.1:%d/ (root %s)\n", port, g_root.c_str());
    fflush(stdout);
    if (!portfile.empty() && !fldl::WriteFile(portfile, std::to_string(port) + "\n"))
        return fprintf(stderr, "flbench serve: cannot write %s\n", portfile.c_str()), 1;

    const auto start = std::chrono::steady_clock::now();
    std::vector<Client> clients;
    for (;;)
    {
        // ends, whichever comes first: the launcher gone; the window's process gone (its pid in <portfile>.window, written by
        // the launcher once it started the window); the page said goodbye 3 s ago with no ping since (not a reload); the
        // page silent for 15 s (it pings every 3 s; never seen within three minutes of the start)
        if (parentHandle && WaitForSingleObject(parentHandle, 0) == WAIT_OBJECT_0)
            break;
        if (!windowHandle && !portfile.empty())
        {
            std::string w;
            if (ReadFile(portfile + ".window", w) && atoi(w.c_str()) > 0)
                windowHandle = OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(atoi(w.c_str())));
        }
        if (windowHandle && WaitForSingleObject(windowHandle, 0) == WAIT_OBJECT_0)
            break;
        const auto now = std::chrono::steady_clock::now();
        if (g_bye && now - g_byeAt > std::chrono::seconds(3))
            break;
        if (g_pinged ? now - g_lastPing > std::chrono::seconds(15) : now - start > std::chrono::seconds(180))
            break;
        fd_set rd; // (filled by hand: core\common.h undefines `far`, which the FD_ macros' FAR needs)
        rd.fd_count = 0;
        rd.fd_array[rd.fd_count++] = ls;
        for (const Client& c : clients)
            rd.fd_array[rd.fd_count++] = c.s;
        timeval tv = {1, 0};
        if (select(0, &rd, nullptr, nullptr, &tv) <= 0)
            continue;
        auto isSet = [&](SOCKET s) { return std::find(rd.fd_array, rd.fd_array + rd.fd_count, s) != rd.fd_array + rd.fd_count; };
        if (isSet(ls) && clients.size() < 60)
        {
            SOCKET s = accept(ls, nullptr, nullptr);
            if (s != INVALID_SOCKET)
            {
                u_long nb = 1;
                ioctlsocket(s, FIONBIO, &nb);
                clients.push_back({s, {}});
            }
        }
        for (size_t i = 0; i < clients.size();)
        {
            Client& c = clients[i];
            bool done = false;
            if (isSet(c.s))
            {
                char b[65536];
                const int n = recv(c.s, b, sizeof b, 0);
                if (n <= 0 || c.in.size() > (64u << 20))
                    done = true;
                else
                {
                    c.in.append(b, static_cast<size_t>(n));
                    std::string method, target, body;
                    if (Complete(c.in, method, target, body))
                    {
                        Send(c.s, Handle(method, target, body));
                        done = true;
                    }
                }
            }
            if (done)
            {
                closesocket(c.s);
                clients.erase(clients.begin() + static_cast<long>(i));
            }
            else
                ++i;
        }
    }
    for (Client& c : clients)
        closesocket(c.s);
    closesocket(ls);
    WSACleanup();
    // the run's atlas folder goes with it
    DeleteFileA((g_tmp + "\\replay_atlas.png").c_str());
    DeleteFileA((g_tmp + "\\replay_atlas.json").c_str());
    RemoveDirectoryA(g_tmp.c_str());
    if (!portfile.empty())
        DeleteFileA(portfile.c_str());
    printf("flbench serve: ended\n");
    return 0;
}
