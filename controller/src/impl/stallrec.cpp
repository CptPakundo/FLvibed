#include "stallrec.h"

#include "../events.h"
#include "autopilot.h"
#include "device.h"
#include "game/ai.h"
#include "game/hudinfo.h"
#include "game/space.h"
#include "host.h"
#include "log.h"
#include "stallwatch.h"
#include "text.h"
#include "version.h"

#include <algorithm>
#include <sstream>
#include <vector>

// (ASCII only: the build reads the sources as cp1252, so non-ASCII text goes in as \u escapes)
namespace stallrec
{
constexpr float kRangeM = 6000, kHostile = -0.6f, kSampleSec = 0.5f, kNearM = 2500;
constexpr int kMaxRecords = 200;

static Options g_opt;
static stallwatch::Watch g_watch;
static double g_clock = 0;
static float g_nextSample = 0;
static int g_records = 0, g_ends = 0, g_ships = 0;
static bool g_headerDone = false;
static std::string g_last;
static float g_fps = 0;
static u32 g_hold = 0; // debug `stalls hold`: this ship's sample stands still
static bool g_holdSet = false;
static float g_holdPos[3] = {};

Options& Get() { return g_opt; }

static std::string Path() { return DllDir() + "FLvibed_npc_stalls.log"; }

static void Write(const std::string& line)
{
    FILE* f = nullptr;
    if (fopen_s(&f, Path().c_str(), "a") != 0 || !f)
        return;
    if (!g_headerDone)
    {
        g_headerDone = true;
        SYSTEMTIME t;
        GetLocalTime(&t);
        fprintf(f, "\n=== FLvibed %s, %04d-%02d-%02d %02d:%02d ===\n", FLV_VERSION, t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute);
    }
    fputs(line.c_str(), f);
    fputc('\n', f);
    fclose(f);
}

static std::string Clock()
{
    SYSTEMTIME t;
    GetLocalTime(&t);
    char b[16];
    sprintf_s(b, "%02d:%02d:%02d", t.wHour, t.wMinute, t.wSecond);
    return b;
}

// The ship's AI as the game reports it: "op=idle(6) locked=0 prio=0 graph=.. behavior=.."
static std::string AiText(u32 id)
{
    ai::State s;
    if (!ai::ReadShipState(id, s))
        return "op=unread";
    std::ostringstream o;
    o << "op=" << ai::OpName(s.op) << "(" << s.op << ") locked=" << s.locked << " prio=" << s.priority << " graph=" << s.graph
      << " behavior=" << s.behavior;
    return o.str();
}

static std::string Dist(const float a[3], const float b[3])
{
    const float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
    return std::to_string(static_cast<int>(sqrtf(dx * dx + dy * dy + dz * dz)));
}

static void RecordStart(const space::Contact& c, const std::vector<space::Contact>& all, const char* mode, double seconds)
{
    std::ostringstream o;
    o << Clock() << " STALL id=" << c.id << " name=\"" << text::Utf8(hudinfo::DisplayName(c.id)) << "\" level="
      << hudinfo::Level(c.id) << " mode=" << mode << " for=" << seconds << "s dist=" << static_cast<int>(c.distance)
      << " pos=" << static_cast<int>(c.pos[0]) << "," << static_cast<int>(c.pos[1]) << "," << static_cast<int>(c.pos[2])
      << " hp=" << c.health << " att=" << c.attitude << " target=" << c.targetId << " onPlayer=" << c.targetsPlayer << " "
      << AiText(c.id);
    ai::Personality p;
    if (ai::CopyNpcPersonality(c.id, p))
        o << " job[scene=" << p.Job.scene_toughness_threshold << " loot=" << p.Job.loot_preference << "]";
    o << "\n   ships:";
    int n = 0;
    for (const space::Contact& s : all)
        if (s.ship && s.id != c.id && n < 6)
        {
            const std::string d = Dist(c.pos, s.pos);
            if (atoi(d.c_str()) > kNearM)
                continue;
            ++n;
            ai::State st;
            o << " " << s.id << ":" << d << "m:" << (ai::ReadShipState(s.id, st) ? ai::OpName(st.op) : "?") << ":att" << s.attitude;
        }
    o << "\n   loot:";
    n = 0;
    for (const space::Contact& s : all)
        if ((s.type & space::kLoot) && n < 6)
        {
            const std::string d = Dist(c.pos, s.pos);
            if (atoi(d.c_str()) > kNearM)
                continue;
            ++n;
            o << " " << s.id << ":" << d << "m:good=" << s.lootGood;
        }
    wchar_t obj[256] = {};
    o << "\n   context: system=\"" << text::Utf8(hudinfo::SystemName()) << "\" objective=\""
      << (HostObjectiveText(obj, 255) ? text::Utf8(obj) : std::string()) << "\" autopilot=" << autopilot::Active()
      << " device=" << (device::Pad() ? "pad" : "kbm") << " fps=" << static_cast<int>(g_fps);
    Write(o.str());
    events::Push("npc-stall id=%u %s", c.id, AiText(c.id).c_str());
}

void Update(float dt, const char* mode, bool paused)
{
    if (!g_opt.on)
        return;
    const bool flying = strcmp(mode, "space") == 0 || strcmp(mode, "space-ui") == 0 || strcmp(mode, "cutscene") == 0;
    if (!flying)
    {
        g_watch.Reset();
        return;
    }
    if (paused)
        return;
    g_clock += dt;
    g_nextSample -= dt;
    if (dt > 0)
        g_fps = g_fps * 0.9f + (1 / dt) * 0.1f;
    if (g_nextSample > 0)
        return;
    g_nextSample = kSampleSec;
    const std::vector<space::Contact> all = space::Contacts();
    static std::vector<stallwatch::Sample> ships;
    ships.clear();
    for (const space::Contact& c : all)
        if (c.ship && c.attitudeAsked && c.attitude <= kHostile && c.distance <= kRangeM && !c.lane && !c.jumping && c.health > 0)
        {
            stallwatch::Sample s;
            s.id = c.id;
            memcpy(s.pos, c.pos, sizeof(s.pos));
            if (c.id == g_hold) // (debug `stalls hold`)
            {
                if (!g_holdSet)
                    memcpy(g_holdPos, c.pos, sizeof(g_holdPos)), g_holdSet = true;
                memcpy(s.pos, g_holdPos, sizeof(s.pos));
            }
            ships.push_back(s);
        }
    g_ships = static_cast<int>(ships.size());
    static std::vector<stallwatch::Event> events;
    events.clear();
    g_watch.Update(g_clock, ships, events);
    for (const stallwatch::Event& e : events)
    {
        if (e.began)
        {
            if (g_records >= kMaxRecords)
                continue;
            ++g_records;
            const auto it = std::find_if(all.begin(), all.end(), [&](const space::Contact& c) { return c.id == e.id; });
            if (it != all.end())
                RecordStart(*it, all, mode, e.seconds);
        }
        else if (g_records <= kMaxRecords)
        {
            ++g_ends;
            std::ostringstream o;
            o << Clock() << " END id=" << e.id << " after=" << e.seconds << "s how=" << (e.gone ? "gone" : "moved") << " "
              << (e.gone ? std::string() : AiText(e.id));
            Write(o.str());
            g_last = o.str();
        }
    }
}

std::string Command(std::istream& in)
{
    std::string what;
    in >> what;
    if (what == "hold") // an NPC's brain overrides every directive: only the sample stands still
    {
        g_hold = 0;
        in >> g_hold;
        g_holdSet = false;
        return "hold " + std::to_string(g_hold);
    }
    std::ostringstream o;
    o << "stalls on=" << g_opt.on << " ships=" << g_ships << " records=" << g_records << " ends=" << g_ends << " file=" << Path()
      << " last='" << g_last << "'";
    return o.str();
}
} // namespace stallrec
