// FLvibed controller host - the DualSense grips' stream (modules\controller\docs\HAPTICS.md 8): WASAPI on a thread of its own, the back pair of the pad's speaker endpoint. In the
// host because it must outlive a logic reload. Never opened in a test: the coils can be heard. The synth and the rules are pure: modules\controller\src\wave_synth.h.
#pragma once
#include "../api.h"

namespace hapticwave
{
constexpr int kOff = 0, kOpening = 1, kRunning = 2, kFailed = -1;

// HostApi v41 PadWave: open != 0 starts the stream for the pad at `sdlpad::Path` (once; a failure stands until a close), 0 stops it. Returns the state.
int Open(int open);
// HostApi v41 PadWaveFeed: the newest wavesynth::WaveFrame. With none for 200 ms the stream falls silent by itself.
void Feed(const void* frame);
// HostApi v41 PadWaveWhy: why the stream refused or failed ("" if it did not), for the log and `haptics status`.
void Why(char* out, int len);
} // namespace hapticwave
