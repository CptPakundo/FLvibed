// FLvibed - reads the game's target indicators: see indicators.h.
#include "game/indicators.h"

#include "flexe.h"
#include "safemem.h"
#include "ui/ui.h"

namespace indicators
{
uintptr_t Object()
{
    static uintptr_t cached = 0;
    u32 vt = 0;
    if (cached && safemem::Read(cached, &vt, 4) && exe::VtHudIndicators.is(vt))
        return cached;
    cached = 0;
    const uintptr_t control = ui::FindControl("Targetable_Objects");
    if (control > kOwner && safemem::Read(control - kOwner, &vt, 4) && exe::VtHudIndicators.is(vt))
        cached = control - kOwner;
    return cached;
}

int Read(Mark* out, int max)
{
    const uintptr_t o = Object();
    u32 begin = 0, end = 0;
    if (!o || !out || max <= 0 || !safemem::Read(o + kRecBegin, &begin, 4) || !safemem::Read(o + kRecEnd, &end, 4))
        return 0;
    const int n = Count(begin, end);
    int got = 0;
    for (int i = 0; i < n && got < max; ++i)
    {
        u8 rec[kStride];
        if (!safemem::Read(begin + i * kStride, rec, kStride))
            break;
        out[got++] = Decode(rec);
    }
    return got;
}
} // namespace indicators
