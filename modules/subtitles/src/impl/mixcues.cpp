// Subtitles for the scene mixes: loads mixes.tsv, starts a track on the mix's sound and shows its lines on its clock.
#include "mixcues.h"

#include "flexe.h"
#include "hook.h"
#include "log.h"
#include "data.h"
#include "display.h"
#include "game/ini.h"
#include "game/room.h"
#include "mix_cues.h"
#include "str.h"
#include "movie_words.h"
#include "speakers.h"

#include <algorithm>
#include <fstream>
#include <map>
#include <vector>

namespace mixcues
{
struct Line
{
    std::wstring speaker, text;
    unsigned textFnv = 0;
    std::vector<float> wordStarts; // seconds from the line's start (< 0 unknown); empty: estimated pages
};
struct Mix
{
    std::string id;
    std::vector<Window> windows; // by start
    std::vector<Line> lines;     // the same order
    float lastEnd = 0;
};
static std::map<u32, Mix> g_mixes; // by the nickname id the game starts the sound with (CreateID)
static const Mix* g_cur = nullptr;
static double g_t0 = 0;
static size_t g_next = 0;
static Track g_track;
static u32 g_base = 0; // the base the track started in (another base, space or the menu ends it)
static std::string g_last = "none";

static double Now()
{
    static LARGE_INTEGER f = {};
    if (!f.QuadPart)
        QueryPerformanceFrequency(&f);
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return static_cast<double>(t.QuadPart) / f.QuadPart;
}

// The rows after the header, which must start with the columns `col0` `col1` (another layout fails closed: no rows,
// logged - a column order changed by the review would put the wrong text on screen).
static bool ReadLines(const std::string& path, std::vector<std::vector<std::string>>& rows, const char* col0,
                      const char* col1)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return false;
    std::string l;
    bool header = false;
    while (std::getline(in, l))
    {
        if (!l.empty() && l.back() == '\r')
            l.pop_back();
        if (l.compare(0, 3, "\xEF\xBB\xBF") == 0) // an editor's BOM
            l.erase(0, 3);
        if (l.empty() || l[0] == '#')
            continue;
        if (!header)
        {
            header = true;
            const std::vector<std::string> h = data::SplitTsv(l);
            if (h.size() < 2 || h[0] != col0 || h[1] != col1)
            {
                Log("mixes: unexpected header in %s", path.c_str());
                return false;
            }
            continue;
        }
        rows.push_back(data::SplitTsv(l));
    }
    return true;
}

// The recordings the lines were written for (research S7): a mix is started by its sound nickname alone,
// so a mod that ships its own recording under a vanilla nickname (Crossfire ships its own mixes) would get the vanilla
// lines over a different track. Each nickname's file - resolved the way the game does, through the [Sound] entries of
// the inis EXE\freelancer.ini [Data] `sounds` lists - must have the size of the vanilla 1.1 recording (identical in HD
// Edition 0.7.1; measured on both). A different size drops that mix's lines (logged); a nickname the inis do not
// resolve is not judged (vanilla's binary BINI gf_sounds.ini is decoded by ini::Read).
struct MixFile
{
    const char* nickname;
    long long bytes;
};
static const MixFile kMixFiles[] = {
    {"mix_s002xe", 928459},  {"mix_s010x", 1821340},  {"mix_s019a", 795610},   {"mix_s019b", 1496656},
    {"mix_s019c", 140761},   {"mix_s019d", 95869},    {"mix_s019e", 95347},    {"mix_s027a", 603511},
    {"mix_s027b", 1385731},  {"mix_s027c", 461530},   {"mix_s051a", 1045648},  {"mix_s051b", 1465336},
    {"mix_s051c", 89605},    {"mix_s069x", 1740169},  {"mix_s072aa", 401239},  {"mix_s072ab", 2045539},
    {"mix_s072ba", 251686},  {"mix_s072bb", 197398},  {"mix_s072c", 86995},    {"mix_s072d", 56458},
    {"mix_s072e", 89866},
};

static void CheckFiles()
{
    std::map<std::string, std::string> files; // nickname (lower case) -> DATA-relative file
    for (const std::string& f : ini::DataFiles("sounds"))
        for (const ini::Section& s : ini::Read(f))
            if (s.type == "sound")
                if (const std::string *nick = s.Get("nickname"), *file = s.Get("file"); nick && file)
                    files[str::Lower(*nick)] = *file;
    int checked = 0, dropped = 0;
    for (auto it = g_mixes.begin(); it != g_mixes.end();)
    {
        const std::string nick = str::Lower(it->second.id);
        const auto f = files.find(nick);
        const MixFile* want = nullptr;
        for (const MixFile& m : kMixFiles)
            if (nick == m.nickname)
                want = &m;
        if (f == files.end() || !want)
        {
            ++it;
            continue;
        }
        ++checked;
        WIN32_FILE_ATTRIBUTE_DATA a = {};
        const std::string full = ini::DataPath(f->second);
        const long long have = GetFileAttributesExA(full.c_str(), GetFileExInfoStandard, &a)
                                   ? (static_cast<long long>(a.nFileSizeHigh) << 32) | a.nFileSizeLow
                                   : -1;
        if (have == want->bytes)
        {
            ++it;
            continue;
        }
        Log("mixes: %s plays %s (%lld bytes, the lines were written for %lld): not the recording the lines belong to - "
            "its lines are off", it->second.id.c_str(), full.c_str(), have, want->bytes);
        ++dropped;
        it = g_mixes.erase(it);
    }
    Log("mixes: %d recordings checked against the vanilla files, %d dropped", checked, dropped);
}

bool Load(const std::string& path)
{
    Stop();
    g_mixes.clear();
    std::vector<std::vector<std::string>> rows;
    if (!ReadLines(path, rows, "mix", "start")) // mix start end speaker text note reviewed
    {
        Log("mixes: no %s (scene mixes show no lines)", path.c_str());
        return false;
    }
    struct Row
    {
        std::string mix;
        Window w;
        Line ln;
    };
    std::vector<Row> all;
    for (const auto& f : rows)
    {
        if (f.size() < 5 || f[0].empty() || f[1].empty() || f[4].empty())
            continue;
        Row r{f[0], {static_cast<float>(atof(f[1].c_str())), static_cast<float>(atof(f[2].c_str()))},
              {data::Widen(f[3]), data::Widen(f[4])}};
        if (r.w.end <= r.w.start)
            continue;
        r.ln.textFnv = moviewords::TextFnv(f[4]);
        all.push_back(std::move(r));
    }
    std::stable_sort(all.begin(), all.end(), [](const Row& a, const Row& b) { return a.w.start < b.w.start; });
    for (Row& r : all)
    {
        Mix& m = g_mixes[data::CreateID(r.mix.c_str())];
        m.id = r.mix;
        m.windows.push_back(r.w);
        m.lines.push_back(std::move(r.ln));
        m.lastEnd = std::max(m.lastEnd, r.w.end);
    }
    // the lines' word starts: word_times.tsv rows with voice "mix" (the review's aligner; times on the mix clock),
    // found by mix + text hash like the movies' (a changed text keeps the estimate)
    const std::string dir = path.substr(0, path.find_last_of("\\/") + 1);
    std::vector<std::vector<std::string>> wt;
    size_t timed = 0;
    if (ReadLines(dir + "word_times.tsv", wt, "id", "voice")) // id voice text_fnv audio_sha1 end_ms starts_ms collapsed
        for (const auto& f : wt)
        {
            if (f.size() < 6 || f[1] != "mix")
                continue;
            const auto it = g_mixes.find(data::CreateID(f[0].c_str()));
            if (it == g_mixes.end())
                continue;
            const unsigned fnv = static_cast<unsigned>(strtoul(f[2].c_str(), nullptr, 16));
            Mix& m = it->second;
            for (size_t i = 0; i < m.lines.size(); ++i)
                if (m.lines[i].textFnv == fnv && m.lines[i].wordStarts.empty() &&
                    moviewords::Relative(f[5], m.windows[i].start, moviewords::WordCount(m.lines[i].text),
                                         m.lines[i].wordStarts))
                {
                    ++timed;
                    break;
                }
        }
    CheckFiles();
    Log("mixes: %zu lines in %zu scene mixes from %s, %zu word-timed", all.size(), g_mixes.size(), path.c_str(), timed);
    return true;
}

bool OnSound(u32 id)
{
    const auto it = g_mixes.find(id);
    if (it == g_mixes.end())
        return false;
    g_cur = &it->second;
    g_t0 = Now(), g_next = 0, g_track = Track{};
    g_base = Mem<u32>(exe::PlayerBase);
    g_last = "playing " + g_cur->id;
    Log("mix %s starts: %zu lines, %.1f s", g_cur->id.c_str(), g_cur->lines.size(), g_cur->lastEnd);
    return true;
}

static void End(const char* why, bool retire)
{
    if (!g_cur)
        return;
    Log("mix %s ends at %.1f s: %s", g_cur->id.c_str(), Now() - g_t0, why);
    g_last = g_cur->id + " ended: " + why;
    g_cur = nullptr;
    if (retire)
        display::EndScene(); // its lines on screen fade now (a skipped scene's speech is gone)
}

void Tick()
{
    if (!g_cur)
        return;
    const double t = Now() - g_t0;
    // the place left: another base, space (a ship), the menu (no base) - whatever the scene state reads
    const u32 base = Mem<u32>(exe::PlayerBase);
    if (base != g_base)
        return End(base ? "another base" : "left the base", true);
    static std::vector<size_t> due;
    Due(g_cur->windows, g_next, t, due);
    for (const size_t i : due)
    {
        const Line& ln = g_cur->lines[i];
        const Window& w = g_cur->windows[i];
        const display::Relation rel = speakers::Hostile(ln.speaker, g_cur->id) ? display::Relation::Hostile
                                      : ln.speaker == L"Trent"                    ? display::Relation::Player
                                                                                  : display::Relation::Ally;
        // a late start (a hitch past the window's start) shortens it; the word starts stay on the mix clock
        const float late = static_cast<float>(t) - w.start;
        std::vector<float> starts = ln.wordStarts;
        for (float& s : starts)
            s = s < 0 ? s : std::max(0.0f, s - late);
        display::Show(display::Category::Story, ln.speaker, rel, ln.text, w.end - static_cast<float>(t), 0, false,
                      starts.empty() ? nullptr : &starts);
    }
    bool skipped = false;
    if (g_track.Ended(t, g_cur->lastEnd, room::InScene(), skipped))
        End(skipped ? "the scene ended early (skipped)" : t > g_cur->lastEnd + 1.0 ? "its last line is over"
                                                                                 : "the scene ended",
            skipped);
}

void Stop()
{
    End("stopped", false);
}

std::string Describe()
{
    size_t n = 0;
    for (const auto& m : g_mixes)
        n += m.second.lines.size();
    std::string s = "mix lines " + std::to_string(n) + " in " + std::to_string(g_mixes.size()) + " mixes; ";
    if (g_cur)
        s += "playing " + g_cur->id + " at " + std::to_string(Now() - g_t0).substr(0, 5) + " s, line " +
             std::to_string(g_next) + "/" + std::to_string(g_cur->lines.size()) +
             (g_track.sawScene ? ", scene seen" : ", no scene seen");
    else
        s += g_last;
    return s;
}

std::wstring Text()
{
    std::wstring all;
    for (const auto& m : g_mixes)
        for (const Line& ln : m.second.lines)
            all += ln.speaker + ln.text;
    return all;
}
} // namespace mixcues
