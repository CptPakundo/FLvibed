// The game clock (core\game\gameclock.h): Advance / NowSec / NowMs64 / NowMs, what Advance ignores, the start past the
// callers' "0 = never" windows, and the u32 difference across the wrap (the GetTickCount idiom the callers keep).
#include "game/gameclock.h"

#include "test.h"

void Test_gameclock()
{
    // the start: one day, so a 0 stamp reads "long ago" against every window the callers use (the longest 30 min)
    gameclock::Reset();
    CHECK_NEAR(gameclock::NowSec(), gameclock::kStartSec, 0);
    CHECK(gameclock::NowMs64() == 86400000ull);
    CHECK(gameclock::NowMs() == 86400000u);
    CHECK(gameclock::NowMs64() - 0ull > 30ull * 60 * 1000);
    CHECK(gameclock::NowMs() != 0);

    // paused (0), negative, NaN and absurd dt add nothing
    gameclock::Advance(0);
    gameclock::Advance(-0.5);
    gameclock::Advance(std::nan(""));
    gameclock::Advance(7200);
    CHECK(gameclock::NowMs64() == 86400000ull);

    // exact steps: 4 x 0.25 s = 1 s
    for (int i = 0; i < 4; ++i)
        gameclock::Advance(0.25);
    CHECK(gameclock::NowMs64() == 86401000ull);
    CHECK_NEAR(gameclock::NowSec(), gameclock::kStartSec + 1.0, 0);

    // fixed dt 1/60 (the debug `fixeddt 60`): 60 frames = 1 s of game time, however fast they run; 10 min of frames
    // (36000) stays within a microsecond
    gameclock::Reset();
    const unsigned long long t0 = gameclock::NowMs64();
    for (int i = 0; i < 60; ++i)
        gameclock::Advance(1.0 / 60);
    CHECK_NEAR(gameclock::NowSec(), gameclock::kStartSec + 1.0, 1e-9);
    const unsigned long long d = gameclock::NowMs64() - t0;
    CHECK(d >= 999 && d <= 1000);
    for (int i = 0; i < 36000 - 60; ++i)
        gameclock::Advance(1.0 / 60);
    CHECK_NEAR(gameclock::NowSec(), gameclock::kStartSec + 600.0, 1e-6);

    // the callers' idioms: `stamp && now - stamp < window` and `now >= next`
    gameclock::Reset();
    const unsigned long long stamp = gameclock::NowMs64();
    gameclock::Advance(2.5);
    CHECK(stamp && gameclock::NowMs64() - stamp < 5000);
    gameclock::Advance(2.5);
    CHECK(!(gameclock::NowMs64() - stamp < 5000)); // 5000 ms on the dot: the window is over
    u32 next = gameclock::NowMs() + 1000;
    gameclock::Advance(0.5);
    CHECK(!(gameclock::NowMs() >= next));
    gameclock::Advance(0.5);
    CHECK(gameclock::NowMs() >= next);

    // u32 wrap (49.7 days of game time after the start): the unsigned difference stays right across it
    gameclock::Reset(4294966.5); // 2^32 ms - 796 ms (exact in binary)
    const u32 before = gameclock::NowMs();
    CHECK(before == 4294966500u);
    gameclock::Advance(1.0);
    const u32 after = gameclock::NowMs();
    CHECK(after == 204u); // wrapped
    CHECK(after < before);
    CHECK(static_cast<u32>(after - before) == 1000u);
    CHECK(gameclock::NowMs64() == 4294967500ull); // the 64-bit view does not wrap

    gameclock::Reset();
}
