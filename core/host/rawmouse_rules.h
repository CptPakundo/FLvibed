// FLvibed - the raw mouse's per-read decision (setting `mouse_raw`, core\host\dimouse.cpp): which motion one read of the game's DirectInput
// mouse carries. The game's DirectInput mouse moves its cursor 3-4 px per raw count at slow speed and 5.7-7 at fast speed (measured
// 14 Nov 2026, docs\FREELANCER_RE.md "## Mouse"); the host's Raw Input listener counts the mouse's own motion, and a read takes those
// counts in place of DirectInput's, 1 raw count = 1 DirectInput count (the game's sensitivity slider stays the scale).
// Header only and free of system headers: dimouse.cpp includes it, tests\rawmouse_test.cpp tests it host-free.
#pragma once

namespace rawmouse
{
// Fail open: DirectInput's own motion passes through after this long with no raw relative motion event while DirectInput moved on
// this many reads (the two sources race by a frame, so one read with DirectInput's motion and no raw counts is no sign).
constexpr unsigned kSilentMs = 250;
constexpr int kSilentReads = 3;

// What a read carries, and why (debug `state`: dimouse's counters)
enum class Source : int
{
    Nothing = 0,     // the DirectInput call failed or the game is behind: no real motion reaches the game
    Raw = 1,         // the raw counts since the last read
    Off = 2,         // DirectInput's: raw mode off (the setting, or the listener could not run)
    Absolute = 3,    // DirectInput's: an absolute event since the last read (a tablet, remote desktop, a VM's mouse)
    Silent = 4       // DirectInput's: the listener went silent while DirectInput reported motion (kSilentMs / kSilentReads)
};

// Carried from read to read (the caller owns it)
struct State
{
    int diReadsWithoutRaw = 0; // reads with DirectInput motion since the last raw relative motion event
    bool passThrough = false;  // the silent rule tripped: DirectInput's motion until the next raw relative hardware event
};

struct ReadIn
{
    bool rawOn = false;        // the listener runs and holds the mouse's registration
    bool acquired = false;     // the DirectInput call itself succeeded and the game is in front
    bool dropInjected = false; // a mapper's motion is dropped for this read (the pad filter): hardware counts only
    bool absoluteSeen = false; // an absolute event since the last read
    int hwEvents = 0;          // raw relative motion events since the last read: hardware (RAWINPUTHEADER.hDevice != 0)
    int injEvents = 0;         //   and injected (hDevice == 0)
    unsigned msSinceRaw = ~0u; // since the last raw relative motion event of either kind (~0 = none yet)
    long hwX = 0, hwY = 0;     // raw counts since the last read
    long injX = 0, injY = 0;
    long diX = 0, diY = 0;     // the device's own lX / lY (or its buffered X / Y events' sum)
};

struct ReadOut
{
    long x = 0, y = 0;
    Source source = Source::Nothing;
};

inline ReadOut Decide(const ReadIn& in, State& s)
{
    ReadOut out;
    if (!in.acquired)
        return out; // (the caller clears the raw sums: nothing of the real mouse reaches a game behind)
    if (!in.rawOn)
    {
        out.x = in.diX, out.y = in.diY, out.source = Source::Off;
        return out;
    }
    if (in.hwEvents > 0 || in.injEvents > 0)
        s.diReadsWithoutRaw = 0;
    else if ((in.diX || in.diY) && ++s.diReadsWithoutRaw >= kSilentReads && in.msSinceRaw >= kSilentMs)
        s.passThrough = true;
    if (in.hwEvents > 0)
        s.passThrough = false;
    if (in.absoluteSeen || s.passThrough)
    {
        out.x = in.diX, out.y = in.diY, out.source = in.absoluteSeen ? Source::Absolute : Source::Silent;
        return out;
    }
    out.x = in.hwX + (in.dropInjected ? 0 : in.injX);
    out.y = in.hwY + (in.dropInjected ? 0 : in.injY);
    out.source = Source::Raw;
    return out;
}

inline const char* SourceName(Source s)
{
    switch (s)
    {
    case Source::Raw:
        return "raw";
    case Source::Off:
        return "off";
    case Source::Absolute:
        return "absolute";
    case Source::Silent:
        return "silent";
    default:
        return "nothing";
    }
}
} // namespace rawmouse
