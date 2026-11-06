// FLvibed - the command registry: who answers what on a debug channel. Each module keeps its own channel (shared memory,
// Local\FLControllerShm and Local\FLSubtitlesShm, its layout its own), so a verb both modules answer (`state`, `config`, `set`,
// `trace`, `perf`) goes on reaching the module the tool asked. A verb in the table is answered in the host; every other line
// goes to the channel's logic handler.
#pragma once

namespace cmd
{
// The whole line, the reply buffer. `channel`: the one that asked.
typedef void (*Handler)(int channel, const char* line, char* reply, size_t len);

// Opens a module's channel over its shared block and adds what every channel answers (`reload`, `unload`, `hooks`). Returns the
// channel for Add and Poll.
int Open(int module, const volatile u32* seq, volatile u32* ack, const char* in, size_t inLen, char* reply, size_t replyLen,
         Handler logic);
// `args`: the verb may have arguments after a space; otherwise only the exact line matches.
void Add(int channel, const char* verb, bool args, Handler handler);
// The channel of a module, or -1.
int ChannelOf(int module);
// Answers the waiting line, if any. `channel` < 0 (the block could not be mapped): nothing.
void Poll(int channel);
} // namespace cmd
