// HUD 2 map's influence cells (mapcells.h). Inputs come only from what the game loaded: the shown system's id from the map (+0x7E0), the player's
// reputation vibe from the zone build (core\host\uihooks.cpp), the faction feelings from Common.dll's Reputation::Vibe::GetGroupFeelingsTowards, and the
// system's zones, encounters, formations, factions and ships from the files EXE\freelancer.ini names, read from the install the game runs from
// (spawnsim_ini.*): the zone objects in memory give no density, encounter chance or formation (docs\kb\spawning.md 1, the issue's dump of 15 Nov 2026), so
// the loader reads the files the game read. The work runs on one worker thread; the game's thread only starts jobs, reads the feelings and draws.
#include "mapcells.h"

#include "flexe.h"
#include "game/hostcalls.h"
#include "game/influence.h"
#include "game/ini.h"
#include "game/navview.h"
#include "game/spawnsim_ini.h"
#include "log.h"
#include "safemem.h"
#include "ui/hud2_kit.h"

#include <atomic>
#include <cmath>
#include <memory>
#include <sstream>

namespace mapcells
{
using namespace hud2kit;
using Rc = navlayout::Rect;
namespace
{
constexpr int kZoneCells = 1;           // uihooks::kZoneCells (core\host\uihooks.h)
constexpr DWORD kRelationCheckMs = 1000; // the feelings are read again this often while the map shows
constexpr int kMaxIds = 1024;

// ---- the worker (no game call: files and arithmetic)
struct Job
{
    int kind = 0; // 1 load the system, 2 compute the cells
    u32 seq = 0, system = 0;
    std::shared_ptr<spawnsim::World> world; // kind 2: with the player's standings
};
struct Loaded
{
    u32 seq = 0;
    std::shared_ptr<spawnsim::World> world; // null: failed
    std::string error;
    double ms = 0;
};
struct Computed
{
    u32 seq = 0;
    std::shared_ptr<influence::Field> field;
    std::vector<int> relations;
    double ms = 0;
};
SRWLOCK g_lock = SRWLOCK_INIT;
Job g_job;                 // under g_lock: the next job (the latest replaces an unstarted one)
bool g_hasJob = false;     // under g_lock
Loaded g_loadedOut;        // under g_lock: the worker's results
bool g_hasLoaded = false;
Computed g_computedOut;
bool g_hasComputed = false;
HANDLE g_thread = nullptr, g_wake = nullptr;
volatile LONG g_stop = 0;
std::atomic<bool> g_cancel{false};

double Ms(LARGE_INTEGER a, LARGE_INTEGER b)
{
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    return 1000.0 * (b.QuadPart - a.QuadPart) / f.QuadPart;
}

DWORD WINAPI Worker(void*)
{
    while (!g_stop)
    {
        WaitForSingleObject(g_wake, INFINITE);
        while (!g_stop)
        {
            Job job;
            AcquireSRWLockExclusive(&g_lock);
            const bool has = g_hasJob;
            if (has)
                job = g_job, g_hasJob = false, g_cancel = false;
            ReleaseSRWLockExclusive(&g_lock);
            if (!has)
                break;
            LARGE_INTEGER t0, t1;
            QueryPerformanceCounter(&t0);
            if (job.kind == 1)
            {
                Loaded out;
                out.seq = job.seq;
                std::string data = ini::DataPath("");
                const std::string nick = spawnsim::SystemNickname(data, job.system);
                auto w = std::make_shared<spawnsim::World>();
                if (nick.empty())
                    out.error = "the system is not in the universe file";
                else if (spawnsim::LoadSystem(data, nick, {}, *w, out.error))
                    out.world = w;
                QueryPerformanceCounter(&t1);
                out.ms = Ms(t0, t1);
                if (out.world)
                    out.error = nick;
                AcquireSRWLockExclusive(&g_lock);
                g_loadedOut = out, g_hasLoaded = true;
                ReleaseSRWLockExclusive(&g_lock);
            }
            else if (job.kind == 2 && job.world)
            {
                Computed out;
                out.seq = job.seq;
                out.relations = influence::Relations(*job.world);
                auto f = std::make_shared<influence::Field>(influence::Compute(*job.world, influence::Options(), &g_cancel));
                QueryPerformanceCounter(&t1);
                out.ms = Ms(t0, t1);
                if (!f->cancelled)
                    out.field = f;
                AcquireSRWLockExclusive(&g_lock);
                g_computedOut = out, g_hasComputed = true;
                ReleaseSRWLockExclusive(&g_lock);
            }
        }
    }
    return 0;
}

bool Post(const Job& job)
{
    if (!g_thread)
    {
        g_stop = 0;
        g_wake = CreateEventA(nullptr, FALSE, FALSE, nullptr);
        g_thread = g_wake ? CreateThread(nullptr, 0, Worker, nullptr, 0, nullptr) : nullptr;
        if (!g_thread)
        {
            if (g_wake)
                CloseHandle(g_wake), g_wake = nullptr;
            return false;
        }
    }
    AcquireSRWLockExclusive(&g_lock);
    g_job = job, g_hasJob = true;
    ReleaseSRWLockExclusive(&g_lock);
    SetEvent(g_wake);
    return true;
}

// ---- the game's thread
u32 g_system = 0, g_seq = 0;
std::shared_ptr<spawnsim::World> g_world;   // the shown system's data, standings 0
std::vector<u32> g_groups;                  // per faction of g_world: its reputation group id (0 unknown)
std::shared_ptr<influence::Field> g_field;  // the cells for g_system
std::vector<int> g_fieldRelations, g_pendingRelations;
bool g_computing = false, g_loading = false, g_failed = false;
DWORD g_lastCheck = 0;
std::string g_status = "idle";
// the draw's per-cell visibility (a cell shows where the map shows one of its zones) and its cost
std::vector<char> g_visible;
u32 g_idsHash = 0;
const influence::Field* g_visibleFor = nullptr;
double g_drawMs = 0;
int g_drawFrames = 0, g_drawn = 0;
bool g_drawLogged = false;

struct RepCalls
{
    int(__cdecl* group)(u32&, const char*) = nullptr;               // pub::Reputation::GetReputationGroup (Server.dll)
    int(__cdecl* feelings)(const int&, const u32&, float&) = nullptr; // Reputation::Vibe::GetGroupFeelingsTowards (Common.dll)
};
const RepCalls& Rep()
{
    static RepCalls r;
    static bool done = false;
    if (!done)
    {
        done = true;
        if (HMODULE s = GetModuleHandleA("server.dll"))
            r.group = reinterpret_cast<decltype(r.group)>(GetProcAddress(s, "?GetReputationGroup@Reputation@pub@@YAHAAIPBD@Z"));
        if (HMODULE c = GetModuleHandleA("common.dll"))
            r.feelings = reinterpret_cast<decltype(r.feelings)>(GetProcAddress(c, "?GetGroupFeelingsTowards@Vibe@Reputation@@YAHABHABIAAM@Z"));
    }
    return r;
}

// The player's feelings towards the world's factions (the same call the zone build makes); false when the vibe or the calls are not there yet.
bool Standings(std::vector<double>& out)
{
    const RepCalls& r = Rep();
    int vibe = 0;
    if (!r.feelings || !hostcalls::g.MapZoneIds)
        return false;
    hostcalls::g.MapZoneIds(nullptr, 0, &vibe, nullptr);
    if (!vibe)
        return false;
    if (g_groups.size() != g_world->factions.size())
    {
        g_groups.assign(g_world->factions.size(), 0);
        for (size_t i = 0; i < g_groups.size() && r.group; ++i)
            r.group(g_groups[i], g_world->factions[i].nick.c_str());
    }
    out.assign(g_world->factions.size(), 0);
    for (size_t i = 0; i < out.size(); ++i)
    {
        float f = 0;
        if (g_groups[i] && r.feelings(vibe, g_groups[i], f) == 0)
            out[i] = f;
    }
    return true;
}

bool PoliticalAll()
{
    int layer = 0, filter = -1;
    return exe::NavMapMode.ok() && exe::NavMapZoneFilter.ok() && safemem::Read(static_cast<u32>(exe::NavMapMode.get()), &layer, 4) &&
           safemem::Read(static_cast<u32>(exe::NavMapZoneFilter.get()), &filter, 4) && layer == 1 && filter == 0;
}
} // namespace

int Frame(u32 root)
{
    const u32 sys = navview::ShownSystemId(root);
    if (!sys)
        return 0;
    if (sys != g_system)
    {
        g_system = sys, ++g_seq;
        g_world.reset(), g_field.reset(), g_groups.clear(), g_fieldRelations.clear();
        g_computing = false, g_failed = false, g_visibleFor = nullptr, g_drawLogged = false, g_drawFrames = 0, g_drawMs = 0;
        g_cancel = true;
        Job j;
        j.kind = 1, j.seq = g_seq, j.system = sys;
        g_loading = Post(j);
        g_status = g_loading ? "loading" : "no worker";
    }
    // the worker's results
    Loaded loaded;
    Computed computed;
    bool hasLoaded = false, hasComputed = false;
    AcquireSRWLockExclusive(&g_lock);
    if (g_hasLoaded)
        loaded = g_loadedOut, hasLoaded = true, g_hasLoaded = false;
    if (g_hasComputed)
        computed = g_computedOut, hasComputed = true, g_hasComputed = false;
    ReleaseSRWLockExclusive(&g_lock);
    if (hasLoaded && loaded.seq == g_seq)
    {
        g_loading = false;
        g_world = loaded.world;
        g_failed = !g_world;
        char b[256];
        sprintf_s(b, "system %08X %s: %s in %.0f ms (%zu zones, %zu encounters, %zu factions)", g_system, g_world ? "loaded" : "not loaded", loaded.error.c_str(), loaded.ms,
                  g_world ? g_world->zones.size() : 0, g_world ? g_world->encounters.size() : 0, g_world ? g_world->factions.size() : 0);
        g_status = b;
        Log("map cells: %s", b);
    }
    if (hasComputed && computed.seq == g_seq)
    {
        g_computing = false;
        if (computed.field && computed.relations == g_pendingRelations)
        {
            g_field = computed.field, g_fieldRelations = computed.relations, g_visibleFor = nullptr;
            char b[256];
            sprintf_s(b, "system %08X: %zu cells (spacing %.0f m, %d simulations) computed in %.0f ms off the game's thread", g_system, g_field->cells.size(),
                      g_field->spacing, g_field->signatures, computed.ms);
            g_status = b;
            Log("map cells: %s", b);
        }
        else
            g_lastCheck = 0; // a relation flipped meanwhile: again at once
    }
    // the player's relations: the first computation, and again when one flips
    const DWORD now = GetTickCount();
    if (g_world && !g_computing && now - g_lastCheck >= kRelationCheckMs)
    {
        g_lastCheck = now;
        std::vector<double> standing;
        if (Standings(standing))
        {
            std::vector<int> rel(standing.size());
            for (size_t i = 0; i < rel.size(); ++i)
                rel[i] = influence::RelationOf(standing[i]);
            if (!g_field || rel != g_fieldRelations)
            {
                auto w = std::make_shared<spawnsim::World>(*g_world);
                for (size_t i = 0; i < standing.size(); ++i)
                    w->factions[i].standing = standing[i];
                Job j;
                j.kind = 2, j.seq = g_seq, j.world = w;
                g_pendingRelations = rel;
                g_computing = Post(j);
                if (g_field)
                    Log("map cells: a relation flipped: computing again");
            }
        }
    }
    return g_field && PoliticalAll() ? kZoneCells : 0;
}

void Draw(u32 root, const Rc& port, float H, const Rc& legend)
{
    if (!g_field || !PoliticalAll())
        return;
    LARGE_INTEGER t0, t1;
    QueryPerformanceCounter(&t0);
    const influence::Field& f = *g_field;
    // the zones the map shows (the game's own discovery rule: a cell shows where one of its zones is drawn)
    static u32 ids[kMaxIds];
    u32 colour[3] = {0xFF00FF00u, 0xFFFFFFFFu, 0xFFFF0000u};
    int vibe = 0;
    const int n = hostcalls::g.MapZoneIds ? hostcalls::g.MapZoneIds(ids, kMaxIds, &vibe, colour) : 0;
    u32 hash = 2166136261u;
    for (int i = 0; i < n; ++i)
        hash = (hash ^ ids[i]) * 16777619u;
    if (g_visibleFor != &f || hash != g_idsHash)
    {
        g_visibleFor = &f, g_idsHash = hash;
        g_visible.assign(f.cells.size(), 0);
        for (size_t c = 0; c < f.cells.size(); ++c)
            for (int k = 0; k < f.cells[c].nZones && !g_visible[c]; ++k)
                for (int i = 0; i < n; ++i)
                    if (ids[i] == f.cells[c].zones[k])
                    {
                        g_visible[c] = 1;
                        break;
                    }
    }
    // the map's projection is linear in the plane (a top-down camera): three points give it, a fourth checks it; else every cell is projected
    const float D = 20000;
    float ox = 0, oy = 0, axx = 0, axy = 0, azx = 0, azy = 0, cx = 0, cy = 0;
    if (!navview::ProjectWorld(root, 0, 0, ox, oy) || !navview::ProjectWorld(root, D, 0, axx, axy) || !navview::ProjectWorld(root, 0, D, azx, azy))
        return;
    axx = (axx - ox) / D, axy = (axy - oy) / D, azx = (azx - ox) / D, azy = (azy - oy) / D;
    const bool linear = navview::ProjectWorld(root, -3 * D, 4 * D, cx, cy) && std::fabs(ox - 3 * D * axx + 4 * D * azx - cx) < 1 && std::fabs(oy - 3 * D * axy + 4 * D * azy - cy) < 1;
    const float spacingPx = static_cast<float>(f.spacing) * std::sqrt(axx * axx + axy * axy);
    const float hexR = spacingPx / std::sqrt(3.0f) * 0.94f; // (a hair's gap between neighbours)
    int drawn = 0;
    if (hexR >= 0.75f)
        for (size_t c = 0; c < f.cells.size(); ++c)
        {
            if (!g_visible[c])
                continue;
            const influence::Cell& cell = f.cells[c];
            float sx = 0, sy = 0;
            if (linear)
                sx = ox + cell.x * axx + cell.z * azx, sy = oy + cell.x * axy + cell.z * azy;
            else if (!navview::ProjectWorld(root, cell.x, cell.z, sx, sy))
                continue;
            if (!port.Holds(sx, sy))
                continue;
            const float t = influence::StrengthT(influence::CellShips(cell));
            const float r = hexR * influence::CellScale(t);
            float xy[12];
            for (int k = 0; k < 6; ++k) // pointy-top: corners at 30 + 60 k degrees
            {
                const float a = (30.0f + 60.0f * k) * 0.0174532925f;
                xy[k * 2] = sx + r * std::cos(a), xy[k * 2 + 1] = sy + r * std::sin(a);
            }
            Poly(xy, 6, Argb(colour[influence::CellRelation(cell)] & 0xFFFFFFu, influence::CellAlpha(t)));
            ++drawn;
        }
    // the legend (the owner's v2B): what the colour, the size and the alternation say
    const float size = TextFloor(), line = size * 1.25f, pad = 0.012f * H;
    const float w = legend.W(), h = pad * 2 + line * 7; // (the panel's width: at 0.34 H the first rule was cut at 1280x800 and 4K, frames of 15 Nov 2026)
    const float x = legend.x0, y = legend.y0;
    if (w > 0.15f * H && y + h <= legend.y1)
    {
        Rect(x, y, w, h, Argb(0x0A1424, 0.82f));
        const float box[8] = {x, y, x + w, y, x + w, y + h, x, y + h};
        Outline(box, 4, std::max(1.0f, 0.0012f * H), Argb(kChrome, 0.6f));
        float ty = y + pad + size;
        TextFit(x + pad, ty, L"WHO OPERATES HERE", size, w - 2 * pad, Argb(kChrome, 1));
        static const wchar_t* names[3] = {L"FRIENDLY", L"NEUTRAL", L"HOSTILE"};
        for (int r = 0; r < 3; ++r)
        {
            ty += line;
            Rect(x + pad, ty - size * 0.75f, size * 1.3f, size * 0.7f, Argb(colour[r] & 0xFFFFFFu, 1));
            TextFit(x + pad + size * 1.8f, ty, names[r], size, w - 2 * pad - size * 1.8f, Argb(kInk, 1));
        }
        static const wchar_t* rules[3] = {L"COLOUR = YOUR RELATION TO THE STRONGEST", L"BIGGER, BRIGHTER CELL = STRONGER", L"ALTERNATING CELLS = CONTESTED"};
        for (int r = 0; r < 3; ++r)
            ty += line, TextFit(x + pad, ty, rules[r], size, w - 2 * pad, Argb(kChrome, 0.9f));
    }
    QueryPerformanceCounter(&t1);
    g_drawMs += Ms(t0, t1), ++g_drawFrames, g_drawn = drawn;
    if (g_drawFrames == 300 && !g_drawLogged)
    {
        g_drawLogged = true;
        Log("map cells: draw %.3f ms a frame (mean of 300 frames, %d of %zu cells drawn, hex %.1f px, %s projection)", g_drawMs / g_drawFrames, drawn, f.cells.size(), hexR,
            linear ? "linear" : "per-cell");
    }
}

void Shutdown()
{
    if (g_thread)
    {
        g_cancel = true;
        InterlockedExchange(&g_stop, 1);
        SetEvent(g_wake);
        WaitForSingleObject(g_thread, INFINITE);
        CloseHandle(g_thread);
        g_thread = nullptr;
    }
    if (g_wake)
        CloseHandle(g_wake), g_wake = nullptr;
    AcquireSRWLockExclusive(&g_lock);
    g_hasJob = g_hasLoaded = g_hasComputed = false;
    g_loadedOut = Loaded(), g_computedOut = Computed();
    ReleaseSRWLockExclusive(&g_lock);
    g_system = 0, g_world.reset(), g_field.reset(), g_groups.clear(), g_fieldRelations.clear(), g_pendingRelations.clear();
    g_computing = g_loading = g_failed = false, g_visibleFor = nullptr, g_visible.clear();
    g_status = "idle";
}

std::string Status()
{
    std::ostringstream o;
    o << "cells: " << g_status << (g_loading ? " [loading]" : "") << (g_computing ? " [computing]" : "") << (g_failed ? " [failed]" : "");
    if (g_drawFrames)
        o << "; draw " << g_drawMs / g_drawFrames << " ms mean over " << g_drawFrames << " frames, " << g_drawn << " drawn";
    return o.str();
}
} // namespace mapcells
