// FLvibed - the crash recorder's pure half (core\host\crashlog.cpp): a text block built in a fixed buffer with no allocation
// and no C runtime formatting, which exceptions it leaves to the game, and the test that a stack value is a return address.
// Host-free: tests\crashlog_test.cpp.
#pragma once
#include <cstddef>
#include <cstdint>

namespace crashrules
{
// A line of text in a buffer the caller owns. Never allocates, never writes past `cap - 1`, always ends in '\0'.
struct Text
{
    char* buf;
    size_t cap;
    size_t len;

    Text(char* b, size_t c) : buf(b), cap(c), len(0)
    {
        if (cap)
            buf[0] = '\0';
    }
    Text& Put(const char* s)
    {
        while (s && *s && len + 1 < cap)
            buf[len++] = *s++;
        if (cap)
            buf[len] = '\0';
        return *this;
    }
    Text& Char(char c)
    {
        const char s[2] = {c, '\0'};
        return Put(s);
    }
    // "0x" then exactly `digits` hex digits (1..8)
    Text& Hex(uint32_t v, int digits = 8)
    {
        char s[11] = {'0', 'x'};
        if (digits < 1 || digits > 8)
            digits = 8;
        for (int i = 0; i < digits; ++i)
            s[2 + i] = "0123456789ABCDEF"[(v >> (4 * (digits - 1 - i))) & 0xF];
        s[2 + digits] = '\0';
        return Put(s);
    }
    Text& Dec(uint32_t v)
    {
        char s[11];
        int n = 0;
        do
            s[n++] = static_cast<char>('0' + v % 10);
        while ((v /= 10) && n < 10);
        while (n)
            Char(s[--n]);
        return *this;
    }
    // two digits, leading zero (times)
    Text& Dec2(uint32_t v) { return Char(static_cast<char>('0' + v / 10 % 10)).Char(static_cast<char>('0' + v % 10)); }
};

// The game's own filter (Freelancer.exe 0x5A54E0, docs\FREELANCER_RE.md "The crash filter") passes a breakpoint and a single
// step on (EXCEPTION_CONTINUE_SEARCH): a debugger's, not a crash. The recorder does the same.
inline bool PassedOn(uint32_t code) { return code == 0x80000003u || code == 0x80000004u; }

// The codes the recorder also takes at their first chance (a vectored handler, ahead of every frame's handler): faults no game
// code recovers from, which can end the process without reaching any filter. An unwind to a frame that is not on the stack
// (0xC0000029, RtlUnwind on a broken handler chain: the dispatch finds no valid frame, so no frame's handler and no filter
// runs; instance 5, 13 Nov 2026), a stack overflow, a heap the system found corrupt, a /GS failure raised as an exception.
// Never a C++ exception (0xE06D7363) or any other code the game and its DLLs throw and catch in normal play.
inline bool FatalFirstChance(uint32_t code)
{
    return code == 0xC0000029u || code == 0xC00000FDu || code == 0xC0000374u || code == 0xC0000409u;
}

// What an access violation's first parameter says (EXCEPTION_RECORD::ExceptionInformation[0]).
inline const char* AccessKind(uint32_t p0) { return p0 == 0 ? "read" : p0 == 1 ? "write" : p0 == 8 ? "execute" : "access"; }

// True when the bytes just before a stack value end in an x86 call, so the value is likely a return address: `before[n - k]`
// is the byte k places before it (n = how many bytes the caller could read, at most 7). Covers call rel32 (E8), and call
// through a register or memory (FF /2) in each of its lengths: 2 (FF D0+r / FF 10+r), 3 (disp8, or a SIB byte), 4 (SIB +
// disp8), 6 (disp32, FF 15 absolute), 7 (SIB + disp32).
inline bool LooksLikeReturn(const uint8_t* before, size_t n)
{
    auto at = [&](size_t k) -> int { return k <= n ? before[n - k] : -1; };
    if (at(5) == 0xE8)
        return true;
    auto callModrm = [&](size_t k, int wantMod, bool sib) {
        if (at(k) != 0xFF || at(k - 1) < 0)
            return false;
        const int m = at(k - 1);
        const int mod = m >> 6, reg = (m >> 3) & 7, rm = m & 7;
        if (reg != 2 || mod != wantMod)
            return false;
        if (mod == 3)
            return !sib;
        if (sib != (rm == 4))
            return false;
        if (mod == 0 && rm == 5)
            return false; // disp32 absolute: 6 bytes, checked below
        return true;
    };
    if (callModrm(2, 3, false) || callModrm(2, 0, false))
        return true;
    if (callModrm(3, 1, false) || callModrm(3, 0, true))
        return true;
    if (callModrm(4, 1, true))
        return true;
    if (at(6) == 0xFF && at(5) == 0x15)
        return true;
    if (callModrm(6, 2, false))
        return true;
    if (callModrm(7, 2, true))
        return true;
    return false;
}
} // namespace crashrules
