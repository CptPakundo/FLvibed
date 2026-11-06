// FLvibed - the command registry (commands.h).
#include "all.h"

#include "loader.h"

namespace cmd
{
namespace
{
struct Verb
{
    const char* name;
    size_t length;
    bool args;
    Handler handler;
};

struct Channel
{
    int module;
    const volatile u32* seq;
    volatile u32* ack;
    const char* in;
    size_t inLen;
    char* reply;
    size_t replyLen;
    Handler logic;
    std::vector<Verb> verbs;
};

Channel g_channels[router::kModules];
int g_count = 0;

void ReloadVerb(int, const char*, char* reply, size_t len) // every module's logic, as one
{
    strcpy_s(reply, len, flv_logic_loader()->Reload() ? "reloaded" : "reload FAILED (see log)");
}

void UnloadVerb(int, const char*, char* reply, size_t len)
{
    flv_logic_loader()->Unload();
    strcpy_s(reply, len, "unloaded");
}

// The hooks of the module that asked, the ones it shares with others among them: mode, place, the function's own jump now.
void HooksVerb(int channel, const char*, char* reply, size_t len)
{
    hk::HookReport(reply, len, router::OwnerOf(g_channels[channel].module));
}
} // namespace

int Open(int module, const volatile u32* seq, volatile u32* ack, const char* in, size_t inLen, char* reply, size_t replyLen,
         Handler logic)
{
    if (g_count >= router::kModules)
        return -1;
    Channel& c = g_channels[g_count];
    c.module = module;
    c.seq = seq;
    c.ack = ack;
    c.in = in;
    c.inLen = inLen;
    c.reply = reply;
    c.replyLen = replyLen;
    c.logic = logic;
    const int channel = g_count++;
    Add(channel, "reload", false, ReloadVerb);
    Add(channel, "unload", false, UnloadVerb);
    Add(channel, "hooks", false, HooksVerb);
    return channel;
}

void Add(int channel, const char* verb, bool args, Handler handler)
{
    if (channel >= 0 && channel < g_count)
        g_channels[channel].verbs.push_back({verb, strlen(verb), args, handler});
}

int ChannelOf(int module)
{
    for (int i = 0; i < g_count; ++i)
        if (g_channels[i].module == module)
            return i;
    return -1;
}

void Poll(int channel)
{
    if (channel < 0 || channel >= g_count)
        return;
    Channel& c = g_channels[channel];
    if (*c.seq == *c.ack)
        return;
    char line[1024];
    const size_t n = std::min(sizeof(line), c.inLen);
    memcpy(line, c.in, n);
    line[n - 1] = 0;

    Handler handler = c.logic;
    for (const Verb& v : c.verbs)
        if (strncmp(line, v.name, v.length) == 0 && (line[v.length] == 0 || (v.args && line[v.length] == ' ')))
        {
            handler = v.handler;
            break;
        }
    handler(channel, line, c.reply, c.replyLen);
    MemoryBarrier();
    *c.ack = *c.seq;
}
} // namespace cmd
