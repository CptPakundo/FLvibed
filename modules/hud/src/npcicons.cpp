// HUD 2's NPC icons (npcicons.h): the ship table, the outline worker, the atlas and the kit's source, the rank words.
#include "npcicons.h"

#include "npcicons_rules.h"

#include "game/flhash.h"
#include "game/ini.h"
#include "game/render.h"
#include "game/utf.h"
#include "log.h"
#include "str.h"
#include "text.h"
#include "ui/hud2_kit.h"

#include <deque>
#include <map>
#include <memory>
#include <sstream>

namespace npcicons
{
namespace
{
constexpr int kMaskPx = 256;            // the outline's mask (146_ traced at 512; 256 holds a 128 px cell at 2x)
constexpr int kAtlas = 1024, kCell = 128; // the atlas: 8 x 8 cells, each baked at the size it is drawn (up to 128 px)
constexpr int kCells = (kAtlas / kCell) * (kAtlas / kCell);
constexpr int kBakesPerFrame = 2; // new cells a frame may bake (the rest wait a frame: a few tenths of a millisecond each)

struct ShipRow
{
    std::string file; // DATA\<DA_archetype>, full path
    float floor = 0;  // the width floor by its type (npcicons_rules.h WidthFloor's rule, from the ini's word)
};
std::map<u32, ShipRow> g_ships; // archetype id -> its model
bool g_shipsRead = false;
double g_shipsMs = 0;

// ---- the worker: outlines built off the game's thread (a file read and a raster: no game call)
struct Job
{
    u32 arch;
    std::string file;
    float floor;
};
struct Done
{
    std::unique_ptr<Mask> mask; // null: failed (no file, no mesh)
    double ms = 0;
    size_t tris = 0;
};
SRWLOCK g_lock = SRWLOCK_INIT;
std::deque<Job> g_jobs;            // under g_lock
std::map<u32, Done> g_done;        // under g_lock: written by the worker, read by the game's thread
std::map<u32, bool> g_asked;       // game thread: archetypes queued once
HANDLE g_thread = nullptr, g_wake = nullptr;
volatile LONG g_stop = 0;

DWORD WINAPI Worker(void*)
{
    while (!g_stop)
    {
        Job job{};
        bool have = false;
        AcquireSRWLockExclusive(&g_lock);
        if (!g_jobs.empty())
            job = g_jobs.front(), g_jobs.pop_front(), have = true;
        ReleaseSRWLockExclusive(&g_lock);
        if (!have)
        {
            WaitForSingleObject(g_wake, 500);
            continue;
        }
        const double t0 = perf::Now();
        Done d;
        const int wn = MultiByteToWideChar(CP_ACP, 0, job.file.c_str(), -1, nullptr, 0);
        std::wstring wfile(wn > 0 ? wn - 1 : 0, L'\0');
        if (wn > 1)
            MultiByteToWideChar(CP_ACP, 0, job.file.c_str(), -1, &wfile[0], wn);
        const std::vector<utf::Leaf> leaves = utf::Leaves(wfile);
        const std::vector<Tri2> tris = TopDown(leaves);
        d.tris = tris.size();
        auto m = std::make_unique<Mask>();
        if (Silhouette(tris, kMaskPx, job.floor, *m))
            d.mask = std::move(m);
        d.ms = (perf::Now() - t0) * 1000;
        AcquireSRWLockExclusive(&g_lock);
        g_done[job.arch] = std::move(d);
        ReleaseSRWLockExclusive(&g_lock);
    }
    return 0;
}

// ---- the atlas
render::Image g_atlas;
u32 g_atlasGen = 0;        // render::DeviceResets() when the atlas was built: a cached cell is dropped after a loss / Reset
std::map<u32, int> g_cell; // (arch << 8 | px) -> cell
int g_used = 0;
double g_window = 0;
u32 g_frameBakes = 0;
struct Stats
{
    unsigned built = 0, failed = 0, bakes = 0, atlasResets = 0;
    double maskMsLast = 0, maskMsMax = 0, maskMsSum = 0;
    double bakeMsLast = 0, bakeMsMax = 0, bakeMsSum = 0;
    size_t trisLast = 0;
    std::string lastFile, lastFailed;
} g_st;

// the shiparch rows, once (every [Data] ships file the game loads: a mod's own too)
void ReadShips()
{
    if (g_shipsRead)
        return;
    g_shipsRead = true;
    const double t0 = perf::Now();
    for (const std::string& f : ini::DataFiles("ships"))
        for (const ini::Section& s : ini::Read(f))
        {
            if (s.type != "ship")
                continue;
            const std::string* nick = s.Get("nickname");
            const std::string* da = s.Get("da_archetype");
            if (!nick || !da || da->empty())
                continue;
            ShipRow r;
            r.file = ini::DataPath(*da);
            const std::string* type = s.Get("type");
            const std::string t = type ? Lower(*type) : std::string();
            r.floor = (t == "freighter" || t == "transport" || t == "cruiser" || t == "capital") ? 0.5f : 0.0f;
            g_ships[flhash::CreateId(*nick)] = r;
        }
    g_shipsMs = (perf::Now() - t0) * 1000;
    Log("npcicons: %zu ship archetypes read (%.1f ms)", g_ships.size(), g_shipsMs);
}

// ---- the test verb's fixtures (game thread; empty unless a test sets them)
std::map<u32, std::wstring> g_nameFix; // archetype -> the name its NPC ships are shown under
std::map<u32, bool> g_failFix;         // archetypes drawn as if their model gave no outline

// The kit's source: the cell of (arch, px), baked now when its outline is ready.
bool Uv(u32 arch, int px, void** texture, float uv[4])
{
    perf::Scope ps("hud.npcicons");
    if (!g_failFix.empty() && g_failFix.count(arch))
        return false; // (the forced fallback: as a model with no outline)
    if (px > kCell)
        px = kCell;
    const u32 key = (arch << 8) ^ static_cast<u32>(px);
    if (g_atlas.texture && (!render::ImageOnDevice(g_atlas) || g_atlasGen != render::DeviceResets()))
    {
        render::FreeImage(g_atlas); // (another device, or a loss / Reset since the build: every cell baked again)
        g_cell.clear(), g_used = 0;
    }
    auto it = g_cell.find(key);
    if (it == g_cell.end())
    {
        Want(arch);
        const Mask* mask = nullptr;
        AcquireSRWLockShared(&g_lock);
        const auto d = g_done.find(arch);
        if (d != g_done.end())
            mask = d->second.mask.get(); // (written once by the worker, never changed after)
        ReleaseSRWLockShared(&g_lock);
        if (!mask)
            return false;
        const double now = perf::Now(); // (a window of 5 ms stands for the frame: the kit keeps no frame number for a source)
        if (now - g_window > 0.005)
            g_window = now, g_frameBakes = 0;
        if (g_frameBakes >= kBakesPerFrame)
            return false;
        perf::Scope pb("hud.npcicons.bake");
        const double t0 = perf::Now();
        if (!g_atlas.texture || g_used >= kCells)
        {
            if (!g_atlas.texture)
            {
                std::vector<u32> clear(size_t(kAtlas) * kAtlas, 0x00FFFFFFu);
                if (!render::BuildImage(g_atlas, clear, kAtlas, kAtlas))
                    return false;
                g_atlasGen = render::DeviceResets();
            }
            else
                ++g_st.atlasResets; // full: every cell baked again as it is asked (rare: 64 cells)
            g_cell.clear(), g_used = 0;
        }
        const std::vector<byte> a = Bake(*mask, px);
        std::vector<u32> argb(size_t(px) * px);
        for (size_t i = 0; i < argb.size(); ++i)
            argb[i] = (static_cast<u32>(a[i]) << 24) | 0x00FFFFFFu;
        const int cell = g_used, cx = (cell % (kAtlas / kCell)) * kCell, cy = (cell / (kAtlas / kCell)) * kCell;
        if (!render::UpdateImage(g_atlas, cx, cy, px, px, argb.data()))
        {
            render::FreeImage(g_atlas); // (another device: built again at the next ask)
            g_cell.clear(), g_used = 0;
            return false;
        }
        ++g_used, ++g_frameBakes, ++g_st.bakes;
        it = g_cell.emplace(key, cell).first;
        g_st.bakeMsLast = (perf::Now() - t0) * 1000;
        g_st.bakeMsMax = std::max(g_st.bakeMsMax, g_st.bakeMsLast), g_st.bakeMsSum += g_st.bakeMsLast;
    }
    const int cell = it->second, cx = (cell % (kAtlas / kCell)) * kCell, cy = (cell / (kAtlas / kCell)) * kCell;
    *texture = g_atlas.texture;
    uv[0] = static_cast<float>(cx) / kAtlas, uv[1] = static_cast<float>(cy) / kAtlas;
    uv[2] = static_cast<float>(cx + px) / kAtlas, uv[3] = static_cast<float>(cy + px) / kAtlas;
    return true;
}

// ---- the rank words (faction_prop.ini rank_desig: low, mid, high ids, then the two level thresholds), once
RankWords g_ranks;
bool g_ranksRead = false;
void ReadRanks()
{
    if (g_ranksRead)
        return;
    g_ranksRead = true;
    for (const ini::Section& s : ini::Read(ini::DataPath("MISSIONS\\faction_prop.ini")))
    {
        if (s.type != "factionprops")
            continue;
        const std::string* rd = s.Get("rank_desig");
        if (!rd)
            continue;
        const std::vector<std::string> f = ini::Fields(*rd);
        if (f.size() < 3)
            continue;
        std::wstring w[3];
        for (int i = 0; i < 3; ++i)
            w[i] = text::Ids(static_cast<u32>(strtoul(f[i].c_str(), nullptr, 10)));
        g_ranks.AddFaction(w);
    }
    Log("npcicons: %zu rank words, %zu at different tiers in two factions (no mark)", g_ranks.Size(), g_ranks.Ambiguous());
}
} // namespace

flicons::Icon ClassIcon(const std::wstring& cls)
{
    return (cls.find(L"FREIGHT") != std::wstring::npos || cls.find(L"TRANSPORT") != std::wstring::npos) ? flicons::Transport
                                                                                                         : flicons::Fighter;
}

int Rank(const std::wstring& name, int* wordLen)
{
    ReadRanks();
    size_t len = 0;
    int tier = g_ranks.TierOf(name, &len);
    if (!tier && name.compare(0, 7, L"Escort ") == 0) // the escort list's cached prefix "Escort 1 " (core\game\namecut.h): the rank after it
    {
        size_t at = 7;
        while (at < name.size() && iswdigit(name[at]))
            ++at;
        if (at > 7 && at < name.size() && name[at] == L' ')
            tier = g_ranks.TierOf(name.substr(at + 1), &len), len = 0; // (the word stays: the prefix stands before it)
    }
    if (wordLen)
        *wordLen = static_cast<int>(len);
    return tier;
}

void Want(u32 arch)
{
    if (!arch || g_asked.count(arch))
        return;
    hud2kit::SetShipSource(&Uv);
    ReadShips();
    g_asked[arch] = true;
    const auto it = g_ships.find(arch);
    if (it == g_ships.end())
    {
        ++g_st.failed, g_st.lastFailed = "arch " + std::to_string(arch) + ": not in shiparch";
        return;
    }
    if (!g_thread)
    {
        g_stop = 0;
        g_wake = CreateEventA(nullptr, FALSE, FALSE, nullptr);
        g_thread = g_wake ? CreateThread(nullptr, 0, Worker, nullptr, 0, nullptr) : nullptr;
        if (!g_thread)
            return; // (no worker: the class icons stay)
    }
    AcquireSRWLockExclusive(&g_lock);
    g_jobs.push_back({arch, it->second.file, it->second.floor});
    ReleaseSRWLockExclusive(&g_lock);
    SetEvent(g_wake);
}

void ShownName(u32 arch, std::wstring& name)
{
    if (g_nameFix.empty() || !arch)
        return;
    const auto it = g_nameFix.find(arch);
    if (it != g_nameFix.end())
        name = it->second;
}

void Shutdown()
{
    hud2kit::SetShipSource(nullptr);
    if (g_thread)
    {
        // Joined with no timeout: the worker holds g_lock only for a queue or result swap and never waits on the game's
        // thread (a file read and a raster, no game or D3D call), so it ends after its current job; a timeout would leave
        // it running in code a hot reload unloads.
        AcquireSRWLockExclusive(&g_lock);
        g_jobs.clear();
        ReleaseSRWLockExclusive(&g_lock);
        InterlockedExchange(&g_stop, 1);
        SetEvent(g_wake);
        WaitForSingleObject(g_thread, INFINITE);
        CloseHandle(g_thread);
        g_thread = nullptr;
    }
    if (g_wake)
        CloseHandle(g_wake), g_wake = nullptr;
    render::FreeImage(g_atlas);
    g_cell.clear(), g_used = 0;
    AcquireSRWLockExclusive(&g_lock);
    g_jobs.clear(), g_done.clear();
    ReleaseSRWLockExclusive(&g_lock);
    g_asked.clear();
}

std::string Command(std::istream& in)
{
    std::string op;
    in >> op;
    if (op == "clear")
    {
        g_nameFix.clear(), g_failFix.clear();
        return "npcicons: fixtures cleared";
    }
    if (op == "name" || op == "fail")
    {
        std::string nick;
        in >> nick;
        if (nick.empty())
            return "usage: npcicons name <ship nickname> <text...> | fail <ship nickname> | clear";
        const u32 arch = flhash::CreateId(nick);
        if (op == "fail")
        {
            g_failFix[arch] = true;
            return "npcicons: " + nick + " (" + std::to_string(arch) + ") draws its class icon";
        }
        std::string rest;
        std::getline(in, rest);
        const size_t b = rest.find_first_not_of(' ');
        rest = b == std::string::npos ? std::string() : rest.substr(b);
        if (rest.empty())
            g_nameFix.erase(arch);
        else
            g_nameFix[arch] = str::Widen(rest);
        return "npcicons: " + nick + " (" + std::to_string(arch) + ") shown as '" + rest + "'";
    }
    // fold the worker's results into the counters (game thread)
    unsigned built = 0, failed = 0;
    double sum = 0, mx = 0;
    std::ostringstream fails;
    AcquireSRWLockShared(&g_lock);
    for (const auto& [arch, d] : g_done)
    {
        (d.mask ? built : failed) += 1;
        sum += d.ms, mx = std::max(mx, d.ms);
        if (!d.mask)
            fails << " " << arch;
    }
    const size_t queued = g_jobs.size();
    ReleaseSRWLockShared(&g_lock);
    std::ostringstream o;
    o.setf(std::ios::fixed);
    o.precision(2);
    o << "npcicons: ships=" << g_ships.size() << " ships_ms=" << g_shipsMs << " asked=" << g_asked.size()
      << " queued=" << queued << " outlines=" << built << " outline_failed=" << failed
      << " outline_ms_avg=" << (built + failed ? sum / (built + failed) : 0) << " outline_ms_max=" << mx
      << " bakes=" << g_st.bakes << " bake_ms_last=" << g_st.bakeMsLast << " bake_ms_max=" << g_st.bakeMsMax
      << " bake_ms_avg=" << (g_st.bakes ? g_st.bakeMsSum / g_st.bakes : 0) << " cells=" << g_used << "/" << kCells
      << " atlas_resets=" << g_st.atlasResets << " ranks=" << g_ranks.Size() << " ranks_ambiguous=" << g_ranks.Ambiguous()
      << " no_row=" << g_st.failed << " name_fixtures=" << g_nameFix.size() << " fail_fixtures=" << g_failFix.size();
    if (failed)
        o << " failed_archs=" << fails.str();
    if (!g_st.lastFailed.empty())
        o << " last_no_row=" << g_st.lastFailed;
    return o.str();
}
} // namespace npcicons
