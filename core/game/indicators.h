// FLvibed - the game's target indicators as data (docs\research\target-info-display.md 3.1, 3.3): the object behind `Targetable_Objects` and its per-contact records,
// read at the game's own rate. Record layout decoded from 0x4ED180 and 0x4EF030 (decompiled; probe T4 owes the agreement with a projection): a vector at +0x370,
// stride 0x34. Fails closed: a vtable or a bound that does not match reads as no records. Decode is pure for the tests.
#pragma once
#include "common.h"

#include <cstring>

namespace indicators
{
constexpr u32 kOwner = 0x38;      // the object = the control - 0x38
constexpr u32 kRecBegin = 0x374, kRecEnd = 0x378, kStride = 0x34;
constexpr u32 kCorners = 0x7A0, kHats = 0x7B0, kDiamond = 0x7B4; // the art the draw tests before use
constexpr u32 kTextControl = 0x40C;
constexpr int kMaxRecords = 256;

struct Mark
{
    u32 flags = 0;
    float x = 0, y = 0, z = 0; // on the screen, pixels and depth
    float world[3] = {};
    u32 id = 0;
    float s = 0;       // the bracket's size term
    float distance = 0;
    bool onScreen = false;
};

// One record's fields out of its 0x34 bytes.
inline Mark Decode(const u8* rec)
{
    Mark m;
    memcpy(&m.flags, rec, 4);
    memcpy(&m.x, rec + 4, 4), memcpy(&m.y, rec + 8, 4), memcpy(&m.z, rec + 0xC, 4);
    memcpy(m.world, rec + 0x10, 12);
    memcpy(&m.id, rec + 0x1C, 4);
    memcpy(&m.s, rec + 0x20, 4);
    memcpy(&m.distance, rec + 0x28, 4);
    m.onScreen = rec[0x2D] != 0;
    return m;
}

// How many records a vector's two ends hold: 0 for anything that is not a whole, sane count.
inline int Count(u32 begin, u32 end)
{
    if (!begin || end < begin || (end - begin) % kStride)
        return 0;
    const u32 n = (end - begin) / kStride;
    return n > static_cast<u32>(kMaxRecords) ? 0 : static_cast<int>(n);
}

// The object, 0 when it is not there (cached by its vtable, looked up again after a rebuild).
uintptr_t Object();
// Up to `max` records of the frame the game last built; the count read.
int Read(Mark* out, int max);
} // namespace indicators
