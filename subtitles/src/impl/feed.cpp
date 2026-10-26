#include "feed.h"

#include "data.h"

#include <deque>

namespace feed
{
struct Entry
{
    u32 seq;
    std::string line;
};
static std::deque<Entry> g_ring;
static u32 g_seq = 0;

void Push(const char* channel, const char* what, const std::wstring& speaker, const std::wstring& text)
{
    char head[96];
    sprintf_s(head, "%u %u %s %s | ", ++g_seq, GetTickCount(), channel, what);
    g_ring.push_back({g_seq, head + data::Narrow(speaker) + " | " + data::Narrow(text)});
    if (g_ring.size() > 200)
        g_ring.pop_front();
}

std::string Since(u32 after)
{
    std::string out = "last " + std::to_string(g_seq) + "\n";
    for (const Entry& e : g_ring)
        if (e.seq > after && out.size() + e.line.size() < 50000)
            out += e.line + "\n";
    return out;
}
} // namespace feed
