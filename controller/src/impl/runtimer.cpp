// The HUD timer of speedrun mode - see runtimer.h.
#include "runtimer.h"

#include "host.h"
#include "hud2.h"
#include "livesplit.h"
#include "log.h"
#include "ui.h"
#include "uikit.h"
#include "uiopt.h"
#include "game/render.h"
#include "game/uidraw.h"

#include <fstream>
#include <map>
#include <sstream>

namespace livesplit
{
bool TimerEnabled(); // (livesplit.cpp reads speedrun_timer with its other rows)
}

namespace runtimer
{
static std::map<std::string, u32> g_best; // checkpoint id -> best segment ms
static std::string g_file;
static int g_lastSeq = 0;
static int g_deltaMs = 0;   // the last split's segment minus its best (0 = none yet / first time)
static bool g_deltaValid = false;
static float g_deltaFor = 0; // seconds the last delta stays up
static std::string g_deltaCp;
static ULONGLONG g_itemsAt = 0;
static float g_nnBottom = 0, g_nnRight = 0; // the NeuroNet box (its five buttons) from the UI tree
static bool g_drawn = false;

static void LoadBest()
{
    g_best.clear();
    std::ifstream in(g_file);
    std::string line;
    while (std::getline(in, line))
    {
        const size_t eq = line.find('=');
        if (line.empty() || line[0] == ';' || line[0] == '[' || eq == std::string::npos)
            continue;
        std::string k = line.substr(0, eq);
        while (!k.empty() && k.back() == ' ')
            k.pop_back();
        g_best[k] = static_cast<u32>(strtoul(line.c_str() + eq + 1, nullptr, 10));
    }
}
static void SaveBest()
{
    const size_t slash = g_file.find_last_of('\\');
    if (slash != std::string::npos)
        CreateDirectoryA(g_file.substr(0, slash).c_str(), nullptr);
    std::ofstream out(g_file, std::ios::trunc);
    out << "; FLvibed speedrun mode: the player's best segment per story checkpoint (ms of game time, loads excluded)\n"
           "[Best]\n";
    for (const auto& kv : g_best)
        out << kv.first << " = " << kv.second << '\n';
}

void Init()
{
    g_file = std::string(g_host && g_host->dllDir ? g_host->dllDir : "") + "FLvibed\\splits_best.ini";
    LoadBest();
    g_lastSeq = livesplit::SplitSeq();
}
void Shutdown() {}

void Update(float dt)
{
    if (g_deltaFor > 0)
        g_deltaFor -= dt;
    const int seq = livesplit::SplitSeq();
    if (seq == g_lastSeq)
        return;
    g_lastSeq = seq;
    const int cp = livesplit::CheckpointIndex();
    const char* name = livesplit::CheckpointName(cp);
    if (!name)
        return;
    const u32 seg = livesplit::LastSegmentMs();
    const auto it = g_best.find(name);
    // a segment of 0 ms is a skipped / lookback split: no best, no delta
    if (seg == 0)
        return;
    g_deltaValid = it != g_best.end();
    g_deltaMs = g_deltaValid ? static_cast<int>(seg) - static_cast<int>(it->second) : 0;
    g_deltaFor = 8.0f;
    g_deltaCp = name;
    if (it == g_best.end() || seg < it->second)
    {
        g_best[name] = seg;
        SaveBest();
        Log("timer: %s best segment %u ms%s", name, seg, it == g_best.end() ? " (first)" : "");
    }
}

static std::wstring Clock(u32 ms)
{
    wchar_t buf[32];
    const u32 h = ms / 3600000, m = ms / 60000 % 60, s = ms / 1000 % 60, t = ms / 100 % 10;
    swprintf_s(buf, L"%u:%02u:%02u.%u", h, m, s, t);
    return buf;
}
static std::wstring Delta(int ms)
{
    wchar_t buf[32];
    const int a = ms < 0 ? -ms : ms;
    swprintf_s(buf, L"%s%d.%d", ms < 0 ? L"-" : L"+", a / 1000, a / 100 % 10);
    return buf;
}
static std::wstring Label(const char* cp)
{
    // "M01a.wing" -> "M01A - WING" (ASCII only: the sources compile as ANSI and the UI fonts' glyph set is narrow)
    std::wstring w;
    for (const char* p = cp; *p; ++p)
        w += *p == '.' ? std::wstring(L" - ") : std::wstring(1, static_cast<wchar_t>(toupper(static_cast<unsigned char>(*p))));
    return w;
}

void Draw()
{
    g_drawn = false;
    if (!livesplit::Running() || !livesplit::TimerEnabled())
        return;
    void* device = g_host && g_host->GetD3DDevice ? g_host->GetD3DDevice() : nullptr;
    if (!device)
        return;
    render::SetDevice(device);
    int sw = 0, sh = 0;
    if (!render::ScreenSize(sw, sh) || sh < 100)
        return;
    const float H = static_cast<float>(sh), W = static_cast<float>(sw);
    const ULONGLONG now = GetTickCount64();
    if (now - g_itemsAt > 500)
    {
        g_itemsAt = now;
        g_nnBottom = g_nnRight = 0;
        for (const ui::Item& it : ui::Collect())
            if (strncmp(it.name, "NN", 2) == 0 && strstr(it.name, "ButtonRoot"))
                g_nnBottom = std::max(g_nnBottom, it.y + it.hh), g_nnRight = std::max(g_nnRight, it.x + it.hw);
    }
    const uikit::Fonts& f = uikit::FontsFor(sh, uiopt::Get().textSize);
    if (!f.ok || !render::Begin(f.title))
        return;
    float right = g_nnRight > 0 ? g_nnRight : W - W * 0.02f;
    float y = (g_nnBottom > 0 ? g_nnBottom : H * 0.03f) + H * 0.012f;
    float r2 = 0, b2 = 0;
    if (hud2::TargetCardAnchor(r2, b2)) // HUD 2 shows: under its target card (and threat list), its right edge
        right = r2, y = b2;
    const std::wstring time = Clock(livesplit::RunMs());
    const int next = livesplit::CheckpointIndex() + 1;
    const char* nextName = livesplit::CheckpointName(next);
    const std::wstring split = nextName ? Label(nextName) : L"END";
    // the delta: the last split's for 8 s, else the live segment against its best once it is behind
    std::wstring delta;
    u32 col = uidraw::kText;
    if (g_deltaFor > 0 && g_deltaValid)
        delta = Delta(g_deltaMs), col = g_deltaMs <= 0 ? 0xFF6EE07A : 0xFFE86A6A;
    else if (g_deltaFor > 0)
        delta = L"NEW", col = 0xFFE0C060;
    else if (nextName)
    {
        const auto it = g_best.find(nextName);
        if (it != g_best.end())
        {
            const int live = static_cast<int>(livesplit::RunMs() - livesplit::LastSplitMs()) - static_cast<int>(it->second);
            if (live > 0)
                delta = Delta(live), col = 0xFFE86A6A;
        }
    }
    const float w1 = static_cast<float>(render::TextWidth(f.title, time));
    render::SetFont(f.body);
    const float w2 = static_cast<float>(render::TextWidth(f.body, split)),
                w3 = delta.empty() ? 0 : static_cast<float>(render::TextWidth(f.body, delta)) + H * 0.012f;
    const float w = std::max(w1, w2 + w3) + H * 0.02f, h = f.title.lineHeight + f.body.lineHeight * 1.2f + H * 0.012f;
    render::Gradient(right - w - H * 0.02f, y - H * 0.006f, w + H * 0.02f, h, 0x00000000, 0x80000000, 0x80000000,
                     0x00000000);
    render::SetFont(f.title);
    render::Text(right - w1 - H * 0.01f, y, time, 0xFFF0F4F8);
    y += f.title.lineHeight;
    render::SetFont(f.body);
    float x = right - H * 0.01f;
    if (!delta.empty())
    {
        x -= static_cast<float>(render::TextWidth(f.body, delta));
        render::Text(x, y, delta, col);
        x -= H * 0.012f;
    }
    render::Text(x - w2, y, split, uidraw::kText);
    render::End(false);
    g_drawn = true;
}

std::string Status()
{
    std::ostringstream o;
    o << "timer enabled=" << livesplit::TimerEnabled() << " running=" << livesplit::Running() << " drawn=" << g_drawn
      << " t=" << livesplit::RunMs() << "ms next=" << (livesplit::CheckpointName(livesplit::CheckpointIndex() + 1)
                                                            ? livesplit::CheckpointName(livesplit::CheckpointIndex() + 1)
                                                            : "end")
      << " lastDelta=" << (g_deltaValid ? std::to_string(g_deltaMs) : std::string("-")) << " for=" << g_deltaFor
      << " bests=" << g_best.size() << " nn=(" << g_nnRight << "," << g_nnBottom << ") file=" << g_file;
    return o.str();
}

std::string Command(std::istream& in)
{
    std::string sub;
    in >> sub;
    if (sub == "reset-best")
    {
        g_best.clear();
        SaveBest();
        return Status();
    }
    if (sub == "bests")
    {
        std::ostringstream o;
        for (const auto& kv : g_best)
            o << kv.first << " = " << kv.second << "\n";
        return o.str() + Status();
    }
    return Status() + "\nusage: timer state|bests|reset-best";
}
} // namespace runtimer
