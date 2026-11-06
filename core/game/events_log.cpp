// The logic's side of the event log (core\game\events.h): the record itself is the host's (core\host\events.cpp).
#include "events.h"

#include "hostcalls.h"
#include "text.h"

#include <cstdarg>

namespace events
{
void Update() {}
void Push(const char* fmt, ...)
{
    if (!hostcalls::g.PushEvent)
        return;
    char text[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf_s(text, _TRUNCATE, fmt, ap);
    va_end(ap);
    hostcalls::g.PushEvent(text);
}
std::string Since(u32, int) { return ""; } // tools read events from the host
u32 LastSeq() { return 0; }
// The host's events::IdsText without the round trip (audit 24 Oct 2026): core\text.cpp is compiled into this logic too and holds
// no host state (it loads the resource DLLs itself, as space::SpeakerName does) - the same text, line breaks as spaces (one line
// per event).
std::string IdsText(u32 ids)
{
    std::string s = text::IdsUtf8(ids);
    for (char& c : s)
        if (c == '\n' || c == '\r')
            c = ' ';
    return s;
}
} // namespace events
