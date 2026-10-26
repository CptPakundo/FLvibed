// Speedrun mode's clock, split detector and LiveSplit client - see livesplit.h. Layout of the detector: Event() takes
// one observation (an objective shown, a mode change, a marked solar gone, the mission's result) and matches it against
// the next checkpoint row of checkpoints_gen.h; a split moves the index, tells LiveSplit and the event log, and stores
// the clock with newgame (the sidecar). The matching mirrors tools\splits.py our_splits: exact objective ids, the mode
// kinds only against the very next row, kill rows by the count of kills in the leg, a LOOKBACK of 5 s for an objective
// that showed just before the previous checkpoint, and a missed objective / kill row skipped when a later objective
// row's id shows (a softer rule than offline: the offline tool can look back over the whole log).
// (the dev build compiles the logic sources plain, without core\pre.h: the winsock headers first, before windows.h, as
// Winsock requires; include-guarded, so the wrapped build - pre.h already has them - sees them once)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include "livesplit.h"

#include "../events.h"
#include "../fl.h"
#include "checkpoints_gen.h"
#include "host.h"
#include "log.h"
#include "newgame.h"
#include "options_page.h"
#include "settings_bus.h"
#include "game/space.h"

#include <deque>
#include <set>
#include <sstream>

// ------------------------------------------------------------------ the autosplitter struct (docs\livesplit\README.md)
// A plain exported global with a magic, found by autosplitters through the module's export table (FLvibed.dll in the
// release build, FLControllerImpl.dll in dev) or by a signature scan for the magic. Fixed layout, version 1.
extern "C"
{
    struct FLvibedSplitsV1
    {
        char magic[8];        // "FLVSPLIT"
        u32 version;          // 1
        u32 size;             // sizeof(FLvibedSplitsV1)
        u32 mode;             // the session's mode: 0 normal, 1 assisted, 2 speedrun, 0xFFFFFFFF none
        u32 running;          // 1 while the speedrun clock counts (a speedrun session outside the menu)
        u32 runTimeMs;        // game time since the New Game click, loads and menus excluded
        i32 checkpointIndex;  // the last checkpoint reached (-1 none), an index into docs\checkpoints.tsv's rows
        u32 checkpointCount;  // rows in that table (193)
        u32 splitSeq;         // grows with every split
        u32 lastSplitMs;      // runTimeMs at the last split
        u32 inMenu;           // 1 while the game is in its menu (a load: game time paused)
        u32 realTimeMs;       // wall time since the New Game click (menus included)
        char checkpoint[32];  // the last checkpoint's id ("M01a.wing"), NUL-terminated
        char nextCheckpoint[32];
    };
    __declspec(dllexport) FLvibedSplitsV1 FLvibedSplits = {{'F', 'L', 'V', 'S', 'P', 'L', 'I', 'T'}, 1,
                                                            sizeof(FLvibedSplitsV1), 0xFFFFFFFFu, 0, 0, -1,
                                                            static_cast<u32>(checkpoints::kCount), 0, 0, 0, 0, {}, {}};
}

namespace livesplit
{
// ------------------------------------------------------------------ state
static bool g_speedrun = false; // the session is a speedrun
static double g_runMs = 0, g_realMs = 0;
static ULONGLONG g_startTick = 0;
static std::string g_mode = "menu";
static int g_cp = -1;
static int g_seq = 0;
static u32 g_lastSplitMs = 0, g_lastSegMs = 0;
static int g_legKills = 0;
static u32 g_objSeq = 0;
static u32 g_resultSeq = 0;           // the host's mission results seen (HostApi v20 MissionResult)
static ULONGLONG g_lastClearTick = 0; // the last single objectives-cleared call (a result clears twice; the fallback)
static std::deque<std::pair<ULONGLONG, u32>> g_recentObj; // (tick, text id) of the objectives shown lately
struct Mark
{
    u32 id, system;
};
static std::vector<Mark> g_marked;
static std::set<u32> g_killed;
static float g_killPoll = 0, g_setPoll = 0;
// settings (the bus, cached)
static int g_enabled = 1, g_port = 16834, g_gametime = 1, g_timer = 1;
// the socket
static SOCKET g_sock = INVALID_SOCKET;
enum class Net
{
    Off,
    Idle,
    Connecting,
    Connected
};
static Net g_net = Net::Off;
static float g_retry = 0, g_backoff = 2;
static bool g_wsa = false;
static u32 g_sent = 0, g_dropped = 0;
static std::string g_lastSent, g_netErr;
// The client connects only while a speedrun session runs (g_speedrun) or the debug `livesplit connect` / `test` asked
// (g_manual, until that session ends); outside that the socket stays closed - with `livesplit` 1 (the default) every
// game once tried 127.0.0.1:16834 from its start and retried with the backoff for the whole session (log noise,
// release acceptance 25 Oct 2026). Commands sent while the connection is being made wait in g_pending (a few seconds:
// the New Game click's `starttimer` arrives before the connect completes) and go out as one when it is up.
static bool g_manual = false;
static std::vector<std::pair<ULONGLONG, std::string>> g_pending;
constexpr ULONGLONG kPendingMs = 6000;
static bool Wanted() { return g_speedrun || g_manual; }

static void ReadSettings()
{
    const SettingsBus* bus = HostAtLeast(18) && g_host->GetSettingsBus ? g_host->GetSettingsBus() : nullptr;
    if (!bus)
        bus = options_page::LocalBus();
    if (!bus)
        return;
    char v[96];
    if (bus->Get("Controller", "livesplit", v, sizeof(v)))
        g_enabled = atoi(v);
    if (bus->Get("Controller", "livesplit_port", v, sizeof(v)))
        g_port = atoi(v);
    if (bus->Get("Controller", "livesplit_gametime", v, sizeof(v)))
        g_gametime = atoi(v);
    if (bus->Get("Controller", "speedrun_timer", v, sizeof(v)))
        g_timer = atoi(v);
}
bool TimerEnabled() { return g_timer != 0; }

// ------------------------------------------------------------------ the TCP client
// (the FD_SET / FD_ISSET macros cast with FAR, which common.h's #undef far breaks: the fd_set filled by hand)
static fd_set One(SOCKET s)
{
    fd_set f;
    f.fd_count = 1;
    f.fd_array[0] = s;
    return f;
}
static bool Has(const fd_set& f, SOCKET s)
{
    for (u_int i = 0; i < f.fd_count; ++i)
        if (f.fd_array[i] == s)
            return true;
    return false;
}

static void CloseSock(const char* why)
{
    if (g_sock != INVALID_SOCKET)
    {
        closesocket(g_sock);
        Log("livesplit: disconnected (%s)", why);
    }
    g_sock = INVALID_SOCKET;
    g_net = g_enabled ? Net::Idle : Net::Off;
    g_retry = g_backoff;
    g_backoff = g_backoff * 2 > 30 ? 30 : g_backoff * 2;
}

static void Connect()
{
    if (!g_wsa)
        return;
    g_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_sock == INVALID_SOCKET)
    {
        g_netErr = "socket " + std::to_string(WSAGetLastError());
        CloseSock("no socket");
        return;
    }
    u_long nb = 1;
    ioctlsocket(g_sock, FIONBIO, &nb);
    sockaddr_in a = {};
    a.sin_family = AF_INET;
    a.sin_port = htons(static_cast<u_short>(g_port));
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    const int r = connect(g_sock, reinterpret_cast<sockaddr*>(&a), sizeof(a));
    if (r == 0)
    {
        g_net = Net::Connected;
        g_backoff = 2;
        Log("livesplit: connected to 127.0.0.1:%d", g_port);
        return;
    }
    const int err = WSAGetLastError();
    if (err == WSAEWOULDBLOCK)
    {
        g_net = Net::Connecting;
        g_retry = 5; // the connect's own timeout
        return;
    }
    g_netErr = "connect " + std::to_string(err);
    CloseSock("connect failed");
}

static void SendNow(const char* cmd);

// The commands held back while the connection was being made: out in order once it is up, dropped (logged) when
// their wait is over.
static void FlushPending(bool connected)
{
    const ULONGLONG now = GetTickCount64();
    std::vector<std::pair<ULONGLONG, std::string>> keep;
    for (const auto& p : g_pending)
    {
        if (connected)
            SendNow(p.second.c_str());
        else if (now - p.first <= kPendingMs)
            keep.push_back(p);
        else
        {
            ++g_dropped;
            Log("livesplit: %s dropped (no connection within %u s)", p.second.c_str(), static_cast<unsigned>(kPendingMs / 1000));
        }
    }
    g_pending = keep;
}

static void PumpNet(float dt)
{
    if (!g_enabled)
    {
        if (g_net != Net::Off)
            CloseSock("off");
        g_net = Net::Off;
        g_pending.clear();
        return;
    }
    if (!Wanted())
    {
        // no speedrun session and nobody asked: the socket stays closed (an open one from a session that ended goes)
        if (g_net == Net::Connecting || g_net == Net::Connected)
            CloseSock("session ended");
        g_pending.clear();
        return;
    }
    switch (g_net)
    {
    case Net::Off: g_net = Net::Idle, g_retry = 0; break;
    case Net::Idle:
        g_retry -= dt;
        if (g_retry <= 0)
            Connect();
        FlushPending(false);
        break;
    case Net::Connecting:
    {
        fd_set w = One(g_sock), e = One(g_sock);
        timeval tv = {0, 0};
        const int n = select(0, nullptr, &w, &e, &tv);
        if (n > 0 && Has(w, g_sock))
        {
            g_net = Net::Connected;
            g_backoff = 2;
            Log("livesplit: connected to 127.0.0.1:%d", g_port);
            FlushPending(true);
        }
        else if (n > 0 && Has(e, g_sock))
        {
            g_netErr = "refused";
            CloseSock("refused (is LiveSplit Server started?)");
            FlushPending(false);
        }
        else
        {
            g_retry -= dt;
            if (g_retry <= 0)
                g_netErr = "timeout", CloseSock("connect timeout");
            FlushPending(false);
        }
        break;
    }
    case Net::Connected:
    {
        // a closed peer shows as readable with 0 bytes
        fd_set r = One(g_sock);
        timeval tv = {0, 0};
        if (select(0, &r, nullptr, nullptr, &tv) > 0 && Has(r, g_sock))
        {
            char buf[64];
            const int got = recv(g_sock, buf, sizeof(buf), 0);
            if (got == 0 || (got < 0 && WSAGetLastError() != WSAEWOULDBLOCK))
                CloseSock("peer closed");
        }
        break;
    }
    }
}

static void Send(const char* cmd)
{
    g_lastSent = cmd;
    if (g_net != Net::Connected)
    {
        // a connection on its way (the session just started, a reconnect pending): the command waits for it
        if (g_enabled && Wanted() && (g_net == Net::Connecting || g_net == Net::Idle))
        {
            g_pending.push_back({GetTickCount64(), cmd});
            Log("livesplit: %s waits for the connection", cmd);
            return;
        }
        ++g_dropped;
        if (g_enabled)
            Log("livesplit: %s dropped (not connected)", cmd);
        return;
    }
    SendNow(cmd);
}

static void SendNow(const char* cmd)
{
    const std::string line = std::string(cmd) + "\r\n";
    const int r = send(g_sock, line.c_str(), static_cast<int>(line.size()), 0);
    if (r == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)
    {
        g_netErr = "send " + std::to_string(WSAGetLastError());
        CloseSock("send failed");
        ++g_dropped;
        return;
    }
    ++g_sent;
    Log("livesplit: sent %s", cmd);
}

// ------------------------------------------------------------------ the struct
static void Publish()
{
    FLvibedSplits.mode = static_cast<u32>(newgame::Session() < 0 ? 0xFFFFFFFFu : static_cast<u32>(newgame::Session()));
    FLvibedSplits.running = Running() ? 1 : 0;
    FLvibedSplits.runTimeMs = static_cast<u32>(g_runMs);
    FLvibedSplits.checkpointIndex = g_cp;
    FLvibedSplits.splitSeq = static_cast<u32>(g_seq);
    FLvibedSplits.lastSplitMs = g_lastSplitMs;
    FLvibedSplits.inMenu = g_mode == "menu" ? 1 : 0;
    FLvibedSplits.realTimeMs = static_cast<u32>(g_realMs);
    strncpy_s(FLvibedSplits.checkpoint, CheckpointName(g_cp) ? CheckpointName(g_cp) : "", _TRUNCATE);
    strncpy_s(FLvibedSplits.nextCheckpoint, CheckpointName(g_cp + 1) ? CheckpointName(g_cp + 1) : "", _TRUNCATE);
}

// ------------------------------------------------------------------ the detector
static void Split(int index, const char* how)
{
    g_cp = index;
    ++g_seq;
    const u32 now = static_cast<u32>(g_runMs);
    g_lastSegMs = now - g_lastSplitMs;
    g_lastSplitMs = now;
    const checkpoints::Row& r = checkpoints::kRows[index];
    // kills are counted per leg: a new leg's first row resets them
    if (index == 0 || strcmp(checkpoints::kRows[index - 1].leg, r.leg) != 0 || r.kind == checkpoints::Control)
        g_legKills = r.kind == checkpoints::Kill ? g_legKills : 0;
    events::Push("split %s t=%u seg=%u by=%s", r.cp, now, g_lastSegMs, how);
    Log("livesplit: split %d %s at %u ms (segment %u ms, %s)", index, r.cp, now, g_lastSegMs, how);
    Send("split");
    newgame::SetRunState(now, g_cp);
    Publish();
}

// An observation against the next rows.
static void Event(checkpoints::Kind kind, u32 value, const char* how)
{
    if (!g_speedrun)
        return;
    const int next = g_cp + 1;
    if (next >= checkpoints::kCount)
        return;
    using namespace checkpoints;
    const Row& row = kRows[next];
    if (kind == Kill)
    {
        ++g_legKills;
        if (row.kind == Kill && static_cast<u32>(g_legKills) >= row.value)
            Split(next, how);
        return;
    }
    if (row.kind == kind && (kind != Obj || row.value == value))
    {
        Split(next, how);
    }
    else if (kind == Obj)
    {
        // a missed soft row (an objective the script replaced in the same instant, a kill the log did not see): the
        // exact id of a later objective row within reach says where the story is
        for (int j = next + 1; j < kCount && j <= next + 3; ++j)
        {
            const Row& r = kRows[j];
            if (r.kind != Obj && r.kind != Kill)
                break;
            if (r.kind == Obj && r.value == value)
            {
                for (int k = next; k < j; ++k)
                    Split(k, "skipped");
                Split(j, how);
                break;
            }
        }
    }
    // LOOKBACK: the next row's objective showed shortly before this checkpoint (the race's "Beat Hovis" banner 3 s
    // before the HUD returns) - it counts at this checkpoint's time
    bool more = true;
    while (more && g_cp + 1 < kCount)
    {
        more = false;
        const Row& r = kRows[g_cp + 1];
        if (r.kind != Obj)
            break;
        const ULONGLONG now = GetTickCount64();
        for (const auto& o : g_recentObj)
            if (o.second == r.value && now - o.first <= 5000)
            {
                Split(g_cp + 1, "lookback");
                more = true;
                break;
            }
    }
}

static void Reset(const char* why)
{
    if (g_speedrun || g_cp >= 0 || g_runMs > 0)
        Log("livesplit: reset (%s)", why);
    g_speedrun = false;
    g_runMs = g_realMs = 0;
    g_cp = -1;
    g_lastSplitMs = g_lastSegMs = 0;
    g_legKills = 0;
    g_marked.clear();
    g_killed.clear();
    g_recentObj.clear();
    Publish();
}

void OnNewGame(bool speedrun)
{
    Reset("new game");
    g_startTick = GetTickCount64();
    if (speedrun)
    {
        g_speedrun = true;
        ReadSettings();
        g_retry = 0, g_backoff = 2; // the session starts: connect now (the next PumpNet); starttimer waits for it
        if (g_net == Net::Off && g_enabled)
            g_net = Net::Idle;
        Send("starttimer");
        events::Push("speedrun start");
    }
    Publish();
}

void Init()
{
    WSADATA wd;
    g_wsa = WSAStartup(MAKEWORD(2, 2), &wd) == 0;
    if (!g_wsa)
        Log("livesplit: WSAStartup failed (%d): the client stays off", WSAGetLastError());
    ReadSettings();
    g_net = g_enabled ? Net::Idle : Net::Off;
    Publish();
}

void Shutdown()
{
    if (g_sock != INVALID_SOCKET)
        closesocket(g_sock);
    g_sock = INVALID_SOCKET;
    g_net = Net::Off;
    if (g_wsa)
        WSACleanup();
    g_wsa = false;
}

void Update(float dt, const char* modeName)
{
    const std::string mode = modeName ? modeName : "";
    g_setPoll -= dt;
    if (g_setPoll <= 0)
    {
        g_setPoll = 1.0f;
        ReadSettings();
        if (g_speedrun)
            newgame::SetRunState(static_cast<u32>(g_runMs), g_cp); // the sidecar's state for the next save written
    }
    PumpNet(dt);

    // the session (newgame): a speedrun save loaded restores its clock; the session's end resets
    {
        u32 ms;
        int cp;
        newgame::Mode m;
        if (newgame::TakeLoadedRunState(ms, cp, m))
        {
            Reset("save loaded");
            if (m == newgame::SPEEDRUN)
            {
                g_speedrun = true;
                g_runMs = ms;
                g_cp = cp;
                g_lastSplitMs = ms;
                g_startTick = GetTickCount64();
                Log("livesplit: speedrun save loaded: clock %u ms, checkpoint %d (%s)", ms, cp,
                    CheckpointName(cp) ? CheckpointName(cp) : "none");
            }
        }
        const newgame::Mode s = newgame::Session();
        if (s == newgame::UNKNOWN && g_speedrun)
        {
            Send("reset");
            Reset("main menu");
            g_manual = false; // the session ended: the next PumpNet closes the socket (a debug connect ends with it)
        }
        else if (s == newgame::SPEEDRUN && !g_speedrun && mode != "menu")
        {
            g_speedrun = true; // a session that became speedrun without a load record (debug `newgame mode 2`)
            g_startTick = GetTickCount64();
            g_retry = 0, g_backoff = 2;
        }
        if (g_speedrun && g_net == Net::Off && g_enabled)
            g_net = Net::Idle; // (a speedrun save loaded: the session's connection)
    }

    // the clock and the game-time pause around loads
    if (g_speedrun)
    {
        if (mode != "menu")
            g_runMs += dt * 1000.0;
        g_realMs = static_cast<double>(GetTickCount64() - g_startTick);
        if (mode != g_mode && g_gametime)
        {
            if (mode == "menu")
                Send("pausegametime");
            else if (g_mode == "menu")
                Send("unpausegametime");
        }
    }

    // the detector
    if (g_speedrun)
    {
        using namespace checkpoints;
        // mode transitions
        if (mode != g_mode)
        {
            const std::string& p = g_mode;
            if (mode == "docked" && p != "menu")
                Event(Land, 0, "land");
            else if (p == "docked" && mode != "menu")
                Event(Launch, 0, "launch");
            if (p == "space" && mode == "cutscene")
                Event(Cutscene, 0, "cutscene");
            if ((mode == "space" || mode == "space-ui") && (p == "cutscene" || p == "docked"))
                Event(Control, 0, "control");
            if (mode == "menu" || p == "menu")
                g_marked.clear(); // a load: the marks of the previous state are not kills
        }
        // the mission's result: the game's own SUCCESS message (HostApi v20 MissionResult, the client's
        // DisplayMissionMessage of the success kinds - the same message every full PASS log shows at the leg's end)
        const bool hostResult = HostAtLeast(20);
        if (hostResult)
        {
            u32 type = 0;
            const u32 rseq = HostMissionResult(&type);
            if (rseq != g_resultSeq)
            {
                g_resultSeq = rseq;
                if (type == 1)
                    Event(Success, 0, "success");
            }
        }
        // the objective step. Without the host's result (an older host): a mission's result clears the objectives
        // TWICE in one instant (every full PASS log of the 14 legs, 25 Oct 2026: two SetObjectives count=0 at the
        // SUCCESS message's millisecond), while a script's own mid-mission clear (M03 after the Willard battle, M04 at
        // the Magellan gate, a save's load) is a single call: `success` = a second clear within the same poll (the
        // sequence jumps by two) or within 50 ms.
        ObjectiveStep step;
        const u32 seq = HostObjectiveNow(&step);
        if (seq != g_objSeq)
        {
            const u32 jump = seq - g_objSeq;
            g_objSeq = seq;
            if (step.textId == 0)
            {
                const ULONGLONG now = GetTickCount64();
                if (hostResult)
                    g_lastClearTick = 0; // the host's message decides
                else if (jump >= 2 || (g_lastClearTick && now - g_lastClearTick <= 50))
                    Event(Success, 0, "success"), g_lastClearTick = 0;
                else
                    g_lastClearTick = now;
            }
            else if (step.textId != 21660) // "Awaiting Mission Objective"
            {
                g_recentObj.push_back({GetTickCount64(), step.textId});
                while (g_recentObj.size() > 8)
                    g_recentObj.pop_front();
                Event(Obj, step.textId, "objective");
            }
        }
        // marked solars that vanish (kill:<n>): in space, twice a second, within one system
        g_killPoll -= dt;
        if ((mode == "space" || mode == "space-ui") && g_killPoll <= 0)
        {
            g_killPoll = 0.5f;
            const u32 system = fl::PlayerSystem();
            u32 marks[32];
            const int n = HostMissionMarks(marks, 32);
            for (int i = 0; i < n; ++i)
            {
                if (marks[i] <= 100000)
                    continue; // ships: not the solars kill rows count
                bool known = false;
                for (const Mark& m : g_marked)
                    known |= m.id == marks[i];
                if (!known)
                    g_marked.push_back({marks[i], system});
            }
            if (!g_marked.empty())
            {
                const std::vector<space::Contact> all = space::Contacts();
                for (const Mark& m : g_marked)
                {
                    if (m.system != system || g_killed.count(m.id))
                        continue;
                    bool present = false;
                    for (const space::Contact& c : all)
                        present |= c.id == m.id;
                    if (!present)
                    {
                        g_killed.insert(m.id);
                        Event(Kill, 0, "kill");
                    }
                }
            }
        }
    }
    else if (mode != g_mode)
        g_objSeq = HostObjectiveNow(nullptr); // keep in step so a later session does not replay an old objective
    g_mode = mode;
    Publish();
}

bool Running() { return g_speedrun && g_mode != "menu"; }
u32 RunMs() { return static_cast<u32>(g_runMs); }
int CheckpointIndex() { return g_cp; }
int SplitSeq() { return g_seq; }
u32 LastSplitMs() { return g_lastSplitMs; }
u32 LastSegmentMs() { return g_lastSegMs; }
const char* CheckpointName(int i) { return i >= 0 && i < checkpoints::kCount ? checkpoints::kRows[i].cp : nullptr; }
const char* SegmentOf(int i) { return i >= 0 && i < checkpoints::kCount ? checkpoints::kRows[i].segment : nullptr; }
bool Connected() { return g_net == Net::Connected; }

std::string Status()
{
    std::ostringstream o;
    o << "livesplit enabled=" << g_enabled << " port=" << g_port << " gametime=" << g_gametime << " net="
      << (g_net == Net::Connected ? "connected" : g_net == Net::Connecting ? "connecting" : g_net == Net::Idle ? "idle" : "off")
      << " wanted=" << Wanted() << " manual=" << g_manual << " pending=" << g_pending.size()
      << " sent=" << g_sent << " dropped=" << g_dropped << " last=" << (g_lastSent.empty() ? "-" : g_lastSent)
      << (g_netErr.empty() ? "" : " err=" + g_netErr) << "\nrun speedrun=" << g_speedrun << " running=" << Running()
      << " t=" << static_cast<u32>(g_runMs) << "ms real=" << static_cast<u32>(g_realMs) << "ms cp=" << g_cp << " ("
      << (CheckpointName(g_cp) ? CheckpointName(g_cp) : "none") << ") next="
      << (CheckpointName(g_cp + 1) ? CheckpointName(g_cp + 1) : "end") << " seq=" << g_seq << " legKills=" << g_legKills
      << " marked=" << g_marked.size() << " objSeq=" << g_objSeq << " mode=" << g_mode;
    return o.str();
}

std::string Command(std::istream& in)
{
    std::string sub;
    in >> sub;
    if (sub == "test") // livesplit test [cmd]: sends a split (or the given command) now (connecting first if need be)
    {
        std::string cmd;
        in >> cmd;
        g_manual = true;
        if (g_net == Net::Off && g_enabled)
            g_net = Net::Idle;
        if (g_net == Net::Idle)
            g_retry = 0, g_backoff = 2;
        Send(cmd.empty() ? "split" : cmd.c_str());
        return Status();
    }
    if (sub == "connect") // livesplit connect: (re)connect now, and keep the connection until the session ends
    {
        g_manual = true;
        CloseSock("reconnect");
        g_retry = 0, g_backoff = 2;
        return Status();
    }
    if (sub == "reset")
    {
        Send("reset");
        Reset("debug");
        return Status();
    }
    if (sub == "start") // livesplit start: a speedrun clock from now (tests without a New Game)
    {
        OnNewGame(true);
        return Status();
    }
    if (sub == "event") // livesplit event obj <id>|land|launch|control|cutscene|success|kill: a synthetic observation
    {
        std::string k;
        in >> k;
        u32 v = 0;
        in >> v;
        using namespace checkpoints;
        const Kind kind = k == "obj" ? Obj : k == "land" ? Land : k == "launch" ? Launch : k == "control" ? Control
                        : k == "cutscene" ? Cutscene : k == "success" ? Success : Kill;
        if (kind == Obj)
            g_recentObj.push_back({GetTickCount64(), v});
        Event(kind, v, "debug");
        return Status();
    }
    if (sub == "cp") // livesplit cp <index>: place the detector (tests from a checkpoint save)
    {
        int n = -1;
        in >> n;
        g_cp = n < -1 ? -1 : n >= checkpoints::kCount ? checkpoints::kCount - 1 : n;
        g_legKills = 0;
        g_lastSplitMs = static_cast<u32>(g_runMs);
        Publish();
        return Status();
    }
    if (sub == "rows")
    {
        std::ostringstream o;
        for (int i = std::max(0, g_cp - 2); i < checkpoints::kCount && i <= g_cp + 6; ++i)
            o << (i == g_cp ? "> " : "  ") << i << " " << checkpoints::kRows[i].cp << " kind=" << checkpoints::kRows[i].kind
              << " v=" << checkpoints::kRows[i].value << "\n";
        return o.str();
    }
    return Status() + "\nusage: livesplit state|test [cmd]|connect|reset|start|event <kind> [v]|rows";
}
} // namespace livesplit
