// Forwarders so the logic code can use the same dimouse/shm/fl APIs as the host.
#include "../dimouse.h"
#include "../fl.h"
#include "../shm.h"
#include "host.h"

const HostApi* g_host = nullptr;

namespace dimouse
{
void Update() {}
void MoveBy(float dx, float dy) { g_host->MouseMoveBy(dx, dy); }
void MoveTo(float x, float y) { g_host->MouseMoveTo(x, y); }
void ReleaseTarget() { g_host->MouseReleaseTarget(); }
void Wheel(int notches) { g_host->MouseWheel(notches); }
void SetButton(int index, bool down) { g_host->MouseButton(index, down); }
bool ButtonDown(int index) { return g_host->MouseButtonDown(index); }
void Reset() { g_host->MouseReset(); }
bool IsCaptured() { return g_host->MouseIsCaptured(); }
void Capture() { g_host->MouseCapture(); }
u32 LastRealTick() { return HostAtLeast(2) ? g_host->LastRealMouseTick() : 0; }
std::string Stats()
{
    char buf[512];
    g_host->MouseStats(buf, sizeof(buf));
    return buf;
}
} // namespace dimouse

namespace shm
{
bool Init() { return g_host && g_host->shm; }
ShmBlock* Get() { return g_host ? g_host->shm : nullptr; }
} // namespace shm

namespace fl
{
void SetHeld(int cmd, bool held) { g_host->SetHeld(cmd, held); }
void ClearAllHeld() { g_host->ClearAllHeld(); }
} // namespace fl

#include "../events.h"
#include "text.h"

#include <cstdarg>

namespace events
{
void Update() {}
void Push(const char* fmt, ...)
{
    if (!HostAtLeast(3))
        return;
    char text[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf_s(text, _TRUNCATE, fmt, ap);
    va_end(ap);
    g_host->PushEvent(text);
}
std::string Since(u32, int) { return ""; } // tools read events from the host
u32 LastSeq() { return 0; }
// The host's events::IdsText without the round trip (audit 24 Oct 2026): core\text.cpp is compiled into this logic too
// and holds no host state (it loads the resource DLLs itself, as space::SpeakerName here already does) - the same text,
// line breaks as spaces (one line per event); no host version needed.
std::string IdsText(u32 ids)
{
    std::string s = text::IdsUtf8(ids);
    for (char& c : s)
        if (c == '\n' || c == '\r')
            c = ' ';
    return s;
}
} // namespace events
