// Guarded reads of game memory: a copy under SEH that answers false instead of crashing on an unreadable address (a
// control freed in the same frame, a pointer of another build). Header only, in core since 29 Oct 2026 (it was
// controller\src\impl\debug.cpp's SafeRead, which release UI code included debug.h for: SWEEP_2026-10-29 #4).
#pragma once
#include "common.h"

#include <cstring>

namespace safemem
{
// Copies `len` bytes at `addr` into `out`; false when the memory cannot be read (`out` may then be partly written).
inline bool Read(uintptr_t addr, void* out, size_t len)
{
    __try
    {
        memcpy(out, reinterpret_cast<const void*>(addr), len);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}
// One byte into `addr`; false when the memory cannot be written.
inline bool Write(uintptr_t addr, u8 value)
{
    __try
    {
        *reinterpret_cast<volatile u8*>(addr) = value;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}
} // namespace safemem
