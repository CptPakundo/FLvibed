// The virtual mouse from the logic's side: core\dimouse.h, through the host's calls.
#include "dimouse.h"

#include "game/hostcalls.h"

namespace dimouse
{
void Update() {}
void MoveBy(float dx, float dy) { hostcalls::g.MouseMoveBy(dx, dy); }
void MoveTo(float x, float y) { hostcalls::g.MouseMoveTo(x, y); }
void ReleaseTarget() { hostcalls::g.MouseReleaseTarget(); }
void Wheel(int notches) { hostcalls::g.MouseWheel(notches); }
void SetButton(int index, bool down) { hostcalls::g.MouseButton(index, down); }
bool ButtonDown(int index) { return hostcalls::g.MouseButtonDown(index); }
void Reset() { hostcalls::g.MouseReset(); }
bool IsCaptured() { return hostcalls::g.MouseIsCaptured(); }
void Capture() { hostcalls::g.MouseCapture(); }
u32 LastRealTick() { return hostcalls::g.LastRealMouseTick ? hostcalls::g.LastRealMouseTick() : 0; }
std::string Stats()
{
    char buf[512];
    hostcalls::g.MouseStats(buf, sizeof(buf));
    return buf;
}
} // namespace dimouse
