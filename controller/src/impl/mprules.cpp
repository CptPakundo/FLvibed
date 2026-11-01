#include "mprules.h"

#include "autopilot_impl.h"
#include "game/space.h"
#include "host.h"
#include "log.h"
#include "mprules_tag.h"
#include "text.h"

#include <sstream>

namespace mprules
{
namespace
{
bool g_mp = false;      // the last frame's mode
bool g_said = false;    // the server said something this session
Allow g_word = Allow::Off;
u32 g_seen = 0;         // chat lines read so far (the host's count)
std::wstring g_lastTag; // the line it said it in
u32 g_lastSender = 0;
struct Seen
{
    u32 sender;
    std::wstring text;
};
Seen g_recent[8];
u32 g_recentCount = 0;

// A server's line, never a player's. Freelancer's chat senders: player client ids are small (1..the server's player
// limit); the server's console and system messages (FLHook's, the server's own) come from 0 or the console ids
// (0x10000 and up). Verified on the local server (docs\MULTIPLAYER.md).
bool FromServer(u32 sender) { return sender == 0 || sender >= 0x10000; }
} // namespace

// The whole line only (core\mprules_tag.h: FLHook relays players' chat as server lines, "Name: text")
bool ParseTag(const std::wstring& text, Allow& out)
{
    int v = 0;
    if (!ParseMpTag(text, v))
        return false;
    out = static_cast<Allow>(v);
    return true;
}

// The tag somewhere in the line but not as the whole of it: logged, never obeyed
static bool MentionsTag(const std::wstring& text)
{
    std::wstring t;
    for (wchar_t c : text)
        t += c >= L'A' && c <= L'Z' ? static_cast<wchar_t>(c + 32) : c;
    return t.find(L"[flvibed autopilot=") != std::wstring::npos;
}

void Update()
{
    const bool mp = space::Multiplayer();
    if (mp != g_mp)
    {
        g_mp = mp;
        g_said = false;
        g_lastTag.clear();
        Log(mp ? "multiplayer: the story's scripts run on the server - missions, splits / speedrun mode and the New "
                 "Game prompt stand down; the autopilot follows the server's word (mprules)"
               : "single player: every FLvibed feature as usual");
        events::Push("multiplayer %s", mp ? "on" : "off");
    }
    if (!HostAtLeast(30) || !g_host->ChatLineAt)
        return;
    const u32 total = g_host->ChatLineAt(0, nullptr, nullptr, 0);
    // The first read of this logic (a fresh start or a dev reload) in a session: the session's tag line as the host kept
    // it (v31) - a reload lost the word whenever the login's tag line had left the 32-line ring (28 Oct 2026)
    static bool restored = false;
    if (!restored && g_mp && HostAtLeast(31) && g_host->ServerTag)
    {
        restored = true;
        u32 sender = 0;
        wchar_t buf[256] = {};
        Allow a = Allow::Off;
        const u32 at = g_host->ServerTag(&sender, buf, 256);
        if (at && at <= total && total - at >= 32 && ParseTag(buf, a)) // (still in the ring: read below as it was)
        {
            g_said = true, g_word = a, g_lastTag = buf, g_lastSender = sender;
            Log("mprules: the server's word kept by the host (line %u of %u): autopilot=%s (sender %u)", at, total,
                a == Allow::Full ? "on" : a == Allow::Travel ? "travel" : "off", sender);
        }
    }
    if (total == g_seen)
        return;
    const u32 from = total - g_seen > 32 ? total - 31 : g_seen + 1;
    for (u32 n = from; n <= total; ++n)
    {
        u32 sender = 0;
        wchar_t buf[256] = {};
        g_host->ChatLineAt(n, &sender, buf, 256);
        g_recent[g_recentCount++ % 8] = {sender, buf};
        Allow a = Allow::Off;
        if (!g_mp)
            continue;
        if (!ParseTag(buf, a))
        {
            if (MentionsTag(buf))
                Log("mprules: a line mentions the tag but is not the tag alone (sender %u: a player's relayed chat, or "
                    "a banner with more text) - ignored",
                    sender);
            continue;
        }
        if (!FromServer(sender))
        {
            Log("mprules: a player's line carries the tag (sender %u) - ignored", sender);
            continue;
        }
        g_said = true;
        g_word = a;
        g_lastTag = buf;
        g_lastSender = sender;
        Log("mprules: the server says autopilot=%s (sender %u)",
            a == Allow::Full ? "on" : a == Allow::Travel ? "travel" : "off", sender);
        events::Push("mp-permission autopilot=%s", a == Allow::Full ? "on" : a == Allow::Travel ? "travel" : "off");
    }
    g_seen = total;
}

Allow Autopilot()
{
    if (!space::Multiplayer())
        return Allow::Full;
    if (g_said)
        return g_word;
    const int v = autopilot::g_set.mpAutopilot;
    return v <= 0 ? Allow::Off : v == 1 ? Allow::Travel : Allow::Full;
}

std::string Source()
{
    if (!space::Multiplayer())
        return "single player";
    const Allow a = Autopilot();
    const char* name = a == Allow::Full ? "on" : a == Allow::Travel ? "travel" : "off";
    return g_said ? std::string("the server says ") + name
                  : std::string("the server is silent: mp_autopilot ") + std::to_string(autopilot::g_set.mpAutopilot) +
                        " (" + name + ")";
}

std::string Describe()
{
    std::ostringstream o;
    o << "multiplayer=" << (space::Multiplayer() ? 1 : 0) << " autopilot: " << Source() << "\n";
    if (g_said)
        o << "tag line (sender " << g_lastSender << "): " << text::Utf8(g_lastTag) << "\n";
    o << "chat lines seen " << g_seen << "; the last ones:\n";
    for (u32 i = g_recentCount > 8 ? g_recentCount - 8 : 0; i < g_recentCount; ++i)
        o << "  sender " << g_recent[i % 8].sender << (FromServer(g_recent[i % 8].sender) ? " (server)" : " (player)")
          << ": " << text::Utf8(g_recent[i % 8].text) << "\n";
    return o.str();
}
} // namespace mprules
