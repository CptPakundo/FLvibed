// The movie lines of the Subtitles module: read from EXE\FLSubtitles\movies.tsv, shown on the movie clock over core's window
// (core\game\moviescreen.h), word-timed where the review aligned them. The fit, the intro and the skip are the Movies module's.
#include "movie.h"

#include "../api.h"
#include "game/moviescreen.h"
#include "log.h"
#include "data.h"
#include "display.h"
#include "movie_words.h"

extern const HostApi* g_host;

#include <fstream>
#include <vector>

namespace movie
{
struct Line
{
    std::string movie;
    float start, end;
    std::wstring speaker, text;
    unsigned textFnv = 0;          // FNV-1a of the UTF-8 text (the word_times.tsv row must match it)
    std::vector<float> wordStarts; // per word, seconds from the line's start (< 0 unknown); empty: estimated pages
};
static std::vector<Line> g_lines;
static void LoadWordTimes(const std::string& path);
static size_t g_next = 0; // next line to show
static bool g_on = false; // a movie runs for these lines

static double Now() { return moviescreen::Seconds(); }

bool Load(const std::string& path)
{
    g_lines.clear();
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        Log("movie: cannot open %s (no movie subtitles)", path.c_str());
        return false;
    }
    std::string l;
    bool header = false;
    size_t skipped = 0;
    while (std::getline(in, l))
    {
        if (!l.empty() && l.back() == '\r')
            l.pop_back();
        if (l.compare(0, 3, "\xEF\xBB\xBF") == 0) // a UTF-8 BOM (an editor's save) must not break the header
            l.erase(0, 3);
        if (l.empty() || l[0] == '#')
            continue;
        if (!header)
        {
            header = true; // movie start end speaker text note - checked: another column order would show wrong text
            const std::vector<std::string> h = data::SplitTsv(l);
            if (h.size() < 5 || h[0] != "movie" || h[1] != "start" || h[2] != "end" || h[3] != "speaker" ||
                h[4] != "text")
            {
                Log("movie: unexpected header in %s (no movie subtitles)", path.c_str());
                return false;
            }
            continue;
        }
        const std::vector<std::string> f = data::SplitTsv(l);
        if (f.size() < 5 || f[1].empty())
        {
            ++skipped;
            continue;
        }
        Line ln{f[0], static_cast<float>(atof(f[1].c_str())), static_cast<float>(atof(f[2].c_str())),
                data::Widen(f[3]), data::Widen(f[4])};
        ln.textFnv = moviewords::TextFnv(f[4]);
        g_lines.push_back(std::move(ln));
    }
    Log("movie: %zu lines from %s (%zu malformed rows skipped)", g_lines.size(), path.c_str(), skipped);
    const std::string dir = path.substr(0, path.find_last_of("\\/") + 1);
    LoadWordTimes(dir + "word_times.tsv");
    return true;
}

// The intro lines' word starts (word_times.tsv rows with voice "movie": the review's forced alignment, times on the
// movie clock): a line pages on its words like dialogue (display.cpp); a line without a matching row (no row, or its
// text changed since the alignment) keeps the estimate.
static void LoadWordTimes(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return;
    size_t timed = 0, stale = 0;
    std::string l;
    bool head = false;
    while (std::getline(in, l))
    {
        if (!l.empty() && l.back() == '\r')
            l.pop_back();
        if (l.compare(0, 3, "\xEF\xBB\xBF") == 0) // a UTF-8 BOM (an editor's save) must not break the header
            l.erase(0, 3);
        if (l.empty() || l[0] == '#')
            continue;
        if (!head) // id voice text_fnv audio_sha1 end_ms starts_ms collapsed
        {
            head = true;
            const std::vector<std::string> h = data::SplitTsv(l);
            if (h.size() < 6 || h[0] != "id" || h[1] != "voice" || h[2] != "text_fnv" || h[5] != "starts_ms")
            {
                Log("movie: unexpected header in %s (movie lines keep estimated timing)", path.c_str());
                return;
            }
            continue;
        }
        const size_t t1 = l.find('\t');
        if (t1 == std::string::npos || l.compare(t1 + 1, 6, "movie\t") != 0)
            continue; // a dialogue row (data.cpp)
        const std::vector<std::string> f = data::SplitTsv(l);
        if (f.size() < 6)
            continue;
        const unsigned fnv = static_cast<unsigned>(strtoul(f[2].c_str(), nullptr, 16));
        bool found = false;
        for (Line& ln : g_lines)
        {
            if (ln.movie != f[0] || ln.textFnv != fnv || !ln.wordStarts.empty())
                continue;
            found = true;
            if (moviewords::Relative(f[5], ln.start, moviewords::WordCount(ln.text), ln.wordStarts))
                ++timed;
            else
                ++stale;
            break;
        }
        stale += found ? 0 : 1;
    }
    Log("movie: word timings for %zu of %zu lines (%zu rows stale, ignored) from %s", timed, g_lines.size(), stale,
        path.c_str());
}

std::wstring Text()
{
    std::wstring all;
    for (const Line& ln : g_lines)
        all += ln.speaker + ln.text;
    return all;
}

static void* g_basicVideo = nullptr; // IBasicVideo of the movie, for the research reads only (moviesample)
// The frame reads are OFF: with them the renderer drew 8 frames of the intro and its audio had 50 discontinuities, without them 2,257
// and 0 (GetCurrentImage stalls playback). Lines inside the picture keep the full box; debug `moviesample on` brings them back.
static bool g_noSample = true;
static bool g_translucent = false; // the layered window blends the box; the child can only show it or not

static void ReleaseBasicVideo()
{
    if (g_basicVideo)
        VCall<ULONG>(g_basicVideo, 2);
    g_basicVideo = nullptr;
}

static void EnsureBasicVideo()
{
    if (g_basicVideo)
        return;
    void* graph = g_host && g_host->version >= 3 ? g_host->GetMovieGraph() : nullptr;
    static const GUID kIidBasicVideo = {0x56A868B5, 0x0AD4, 0x11CE, {0xB0, 0x3A, 0x00, 0x20, 0xAF, 0x0B, 0xA7, 0x70}};
    if (!graph || FAILED(VCall<HRESULT>(graph, 0, &kIidBasicVideo, &g_basicVideo)))
        g_basicVideo = nullptr;
}

// ------------------------------------------------------------------ the box: from the picture under the line
// A line whose box lies inside the picture gets a box only over busy picture (none on black / dark /
// flat colour; on the bars display.cpp leaves it out by the picture rectangle alone). The video's current frame
// (IBasicVideo::GetCurrentImage, vt[36]: a packed DIB) is read once a second on a worker thread (it is a read-back from
// the renderer: ~0.1 s at 720x480, up to ~0.6 s at 1440x960 measured - never on the movie loop's thread) into a 64 x 36
// luminance grid of the shown picture (the fit's source rectangle); the previous grid is kept for the change. Each line
// is judged from the grid cells under its box at once (a new line too): spread = standard deviation of the cells, change
// = their mean difference to the previous grid; busy = max(spread / 40, change / 20); on at 0.55 (0.45 for a line's
// first look), off at 0.35 (hysteresis: borderline content does not flicker). The layered window blends the box by how
// busy (0.4 .. 1 of the full box); the child window can only show it or not. No grid (a renderer without the image, an
// unknown format): no target - the full box (legibility first).
constexpr int kGw = 64, kGh = 36;
struct Grid
{
    float v[kGh][kGw];
};
static Grid g_grid, g_prevGrid, g_workGrid;
static bool g_haveGrid = false, g_havePrev = false;
static double g_lastSample = 0, g_imageMs = 0;
static u32 g_imageFails = 0, g_imageReads = 0;
static std::vector<std::pair<double, bool>> g_boxOn; // each line's box state (hysteresis)
// the worker: one read per request; the main thread hands it the IBasicVideo (AddRef'd) and the source rectangle
static HANDLE g_sampler = nullptr, g_sampleGo = nullptr;
static volatile LONG g_sampleBusy = 0, g_sampleReady = 0, g_samplerStop = 0;
static void* g_sampleVideo = nullptr;
static RECT g_sampleSrc = {};
static double g_sampleMs = 0;

static bool ReadGrid(void* bv, const RECT& src, Grid& out)
{
    static std::vector<u8> image; // (worker thread only)
    long size = 0;
    if (FAILED(VCall<HRESULT>(bv, 36, &size, static_cast<long*>(nullptr))) || size <= 40 || size > 64 << 20)
        return false;
    image.resize(static_cast<size_t>(size));
    if (FAILED(VCall<HRESULT>(bv, 36, &size, reinterpret_cast<long*>(image.data()))))
        return false;
    const auto* bi = reinterpret_cast<const BITMAPINFOHEADER*>(image.data());
    if (bi->biSize < 40 || bi->biWidth <= 0 || bi->biHeight == 0 || (bi->biBitCount != 32 && bi->biBitCount != 24) ||
        (bi->biCompression != BI_RGB && bi->biCompression != BI_BITFIELDS))
        return false;
    const int iw = bi->biWidth, ih = std::abs(bi->biHeight), bpp = bi->biBitCount / 8, stride = (iw * bpp + 3) & ~3;
    const size_t off = bi->biSize + (bi->biCompression == BI_BITFIELDS ? 12 : 0);
    if (off + static_cast<size_t>(stride) * ih > image.size())
        return false;
    const u8* px = image.data() + off;
    RECT s = src;
    if (s.right <= s.left || s.right > iw || s.bottom > ih)
        s = RECT{0, 0, iw, ih};
    for (int gy = 0; gy < kGh; ++gy)
        for (int gx = 0; gx < kGw; ++gx)
        {
            const int vx = std::clamp(static_cast<int>(s.left + (s.right - s.left) * (gx + 0.5f) / kGw), 0, iw - 1);
            const int vy = std::clamp(static_cast<int>(s.top + (s.bottom - s.top) * (gy + 0.5f) / kGh), 0, ih - 1);
            const u8* p = px + static_cast<size_t>(bi->biHeight > 0 ? ih - 1 - vy : vy) * stride + static_cast<size_t>(vx) * bpp;
            out.v[gy][gx] = 0.114f * p[0] + 0.587f * p[1] + 0.299f * p[2];
        }
    return true;
}

static DWORD WINAPI SamplerThread(LPVOID)
{
    while (WaitForSingleObject(g_sampleGo, INFINITE) == WAIT_OBJECT_0 && !g_samplerStop)
    {
        const double t0 = Now();
        const bool ok = g_sampleVideo && ReadGrid(g_sampleVideo, g_sampleSrc, g_workGrid);
        g_sampleMs = (Now() - t0) * 1000.0;
        if (g_sampleVideo)
            VCall<ULONG>(g_sampleVideo, 2);
        g_sampleVideo = nullptr;
        InterlockedExchange(&g_sampleReady, ok ? 1 : -1);
        InterlockedExchange(&g_sampleBusy, 0);
    }
    return 0;
}

// Ends the worker (before the graph goes: the movie's stop) - it may be inside a read: waited for, at most 2 s.
static void StopSampler()
{
    if (!g_sampler)
        return;
    InterlockedExchange(&g_samplerStop, 1);
    SetEvent(g_sampleGo);
    if (WaitForSingleObject(g_sampler, 2000) != WAIT_OBJECT_0)
        Log("movie: the frame sampler did not end within 2 s");
    CloseHandle(g_sampler), CloseHandle(g_sampleGo);
    g_sampler = g_sampleGo = nullptr;
    InterlockedExchange(&g_samplerStop, 0);
    InterlockedExchange(&g_sampleBusy, 0);
    InterlockedExchange(&g_sampleReady, 0);
}

static void JudgeBoxes(const display::MovieFrame& f, const RECT& dest)
{
    // no box on movie lines: the text's outline and shadow carry it; with the research
    // reads on (`moviesample on`) the old judged box comes back, the full box where no grid is read yet
    display::SetMovieBoxDefault(g_noSample ? 0.0f : 1.0f);
    if (g_noSample)
        return;
    const double now = Now();
    // a finished read: its grid becomes the current one
    const LONG ready = InterlockedExchange(&g_sampleReady, 0);
    if (ready == 1)
    {
        if (g_haveGrid)
            g_prevGrid = g_grid, g_havePrev = true;
        g_grid = g_workGrid, g_haveGrid = true;
        ++g_imageReads;
        g_imageMs = std::max(g_imageMs, g_sampleMs);
    }
    else if (ready == -1 && g_imageFails++ == 0)
        Log("movie: no video frame to sample (the lines keep the full box inside the picture)");
    // a new read once a second while a line's box reaches into the picture (lines on the bars need none; a line's
    // first look uses the newest grid at once)
    bool inPicture = false;
    for (const RECT& b : f.boxes)
        inPicture |= dest.right <= dest.left ||
                     (b.right > dest.left && b.left < dest.right && b.bottom > dest.top && b.top < dest.bottom);
    EnsureBasicVideo();
    if (inPicture && !g_noSample && g_basicVideo && now - g_lastSample >= 1.0 && !g_sampleBusy)
    {
        if (!g_sampler)
        {
            g_sampleGo = CreateEventW(nullptr, FALSE, FALSE, nullptr);
            g_sampler = g_sampleGo ? CreateThread(nullptr, 0, SamplerThread, nullptr, 0, nullptr) : nullptr;
        }
        if (g_sampler)
        {
            g_lastSample = now;
            VCall<ULONG>(g_basicVideo, 1); // AddRef: the worker's own reference
            g_sampleVideo = g_basicVideo, g_sampleSrc = moviescreen::Source();
            InterlockedExchange(&g_sampleBusy, 1);
            SetEvent(g_sampleGo);
        }
    }
    if (!g_haveGrid)
        return;
    RECT d = dest;
    if (d.right <= d.left)
        GetClientRect(moviescreen::Game(), &d);
    std::vector<std::pair<double, bool>> next;
    for (size_t i = 0; i < f.ids.size(); ++i)
    {
        const RECT& b = f.boxes[i];
        const LONG l = std::max(b.left, d.left), t = std::max(b.top, d.top), r = std::min(b.right, d.right),
                   btm = std::min(b.bottom, d.bottom);
        if (r <= l || btm <= t)
            continue; // on the bars
        const int gx0 = std::clamp(static_cast<int>((l - d.left) * kGw / (d.right - d.left)), 0, kGw - 1),
                  gx1 = std::clamp(static_cast<int>((r - 1 - d.left) * kGw / (d.right - d.left)), gx0, kGw - 1),
                  gy0 = std::clamp(static_cast<int>((t - d.top) * kGh / (d.bottom - d.top)), 0, kGh - 1),
                  gy1 = std::clamp(static_cast<int>((btm - 1 - d.top) * kGh / (d.bottom - d.top)), gy0, kGh - 1);
        double sum = 0, sum2 = 0, change = 0, n = 0;
        for (int gy = gy0; gy <= gy1; ++gy)
            for (int gx = gx0; gx <= gx1; ++gx)
            {
                const float v = g_grid.v[gy][gx];
                sum += v, sum2 += v * v, n += 1;
                if (g_havePrev)
                    change += std::fabs(v - g_prevGrid.v[gy][gx]);
            }
        const double mean = sum / n, spread = std::sqrt(std::max(0.0, sum2 / n - mean * mean));
        const float busy = static_cast<float>(std::max(spread / 40.0, g_havePrev ? change / n / 20.0 : 0.0));
        int known = -1;
        for (size_t k = 0; k < g_boxOn.size(); ++k)
            if (g_boxOn[k].first == f.ids[i])
                known = static_cast<int>(k);
        const bool on = known < 0 ? busy >= 0.45f : g_boxOn[known].second ? busy > 0.35f : busy >= 0.55f;
        next.push_back({f.ids[i], on});
        display::SetMovieBox(f.ids[i], !on ? 0.0f
                                       : g_translucent ? std::clamp((busy - 0.35f) / 0.4f, 0.4f, 1.0f)
                                                                 : 1.0f);
    }
    g_boxOn.swap(next);
}

// The lines on the movie clock: from each tick of moviescreen.
static void Lines(double t)
{
    const std::string& current = moviescreen::Current().id;
    for (; g_next < g_lines.size(); ++g_next)
    {
        const Line& ln = g_lines[g_next];
        if (ln.movie != current)
            continue;
        if (ln.start > t)
            break;
        if (ln.end > t && display::Get().movieSubtitles) // not already over (a late first update skips past lines)
            display::Show(display::Category::Story, ln.speaker,
                          ln.speaker == L"Computer" ? display::Relation::Computer : display::Relation::Neutral, ln.text,
                          ln.end - ln.start, 0, false, ln.wordStarts.empty() ? nullptr : &ln.wordStarts);
    }
    display::Update(0);
}

static void Paint(u32* pixels, int w, int h, const RECT& picture, bool translucent, moviescreen::Painted& out)
{
    g_translucent = translucent;
    display::SetMoviePicture(picture, translucent);
    display::DrawMovie(pixels, w, h);
    const display::MovieFrame& f = display::LastMovieFrame();
    JudgeBoxes(f, picture);
    out.rects = &f.rects, out.box = &f.box, out.key = f.key, out.animating = f.animating;
}

void Attach() { moviescreen::content = {Lines, Paint}; }

void OnSettingApplied(const settings::Def* d)
{
    if (d && strcmp(d->name, "movie_intro") == 0 && moviescreen::settingsChanged)
        moviescreen::settingsChanged();
}

void OnStart(HWND hwnd, const char* path)
{
    moviescreen::Begin(hwnd, path);
    g_on = true;
    ReleaseBasicVideo();
    g_next = 0;
    g_boxOn.clear(), g_lastSample = 0, g_imageFails = g_imageReads = 0, g_imageMs = 0, g_haveGrid = g_havePrev = false;
    display::Clear();
}

void OnUpdate()
{
    if (g_on)
        moviescreen::Pass();
}

void OnStop()
{
    if (!g_on)
        return;
    g_on = false;
    StopSampler();
    if (!g_noSample)
        Log("movie: %u frame samples, slowest %.1f ms", g_imageReads, g_imageMs);
    ReleaseBasicVideo();
    display::Clear();
    moviescreen::End();
}

void Shutdown()
{
    OnStop();
    moviescreen::content = {};
    moviescreen::Shutdown();
}

std::string SetSampling(bool on)
{
    g_noSample = !on;
    return std::string("movie frame sampling ") + (on ? "on" : "off");
}

} // namespace movie
