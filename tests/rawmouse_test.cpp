// The raw mouse's per-read decision (core\host\rawmouse_rules.h, setting `mouse_raw`): raw replaces DirectInput's motion; the pad
// filter keeps the hardware counts only; an absolute event, raw off or a silent listener give DirectInput's; a game behind gets nothing.
#include "../core/host/rawmouse_rules.h"

#include "test.h"

void Test_rawmouse()
{
    using namespace rawmouse;
    const auto read = [](long hwX, long hwY, long diX, long diY) {
        ReadIn in;
        in.rawOn = in.acquired = true;
        in.hwEvents = (hwX || hwY) ? 1 : 0;
        in.msSinceRaw = in.hwEvents ? 0 : 1000;
        in.hwX = hwX, in.hwY = hwY, in.diX = diX, in.diY = diY;
        return in;
    };

    // raw replaces: 1 raw count = 1 DirectInput count, DirectInput's own (accelerated) motion is dropped
    {
        State s;
        const ReadOut o = Decide(read(3, -2, 11, -7), s);
        CHECK(o.source == Source::Raw && o.x == 3 && o.y == -2);
        ReadIn in = read(0, 0, 0, 0);
        CHECK(Decide(in, s).source == Source::Raw && Decide(in, s).x == 0); // still: nothing moves
    }
    // hardware and injected counts add up; with the filter on only the hardware's
    {
        State s;
        ReadIn in = read(4, 1, 9, 2);
        in.injEvents = 1, in.injX = 5, in.injY = 1;
        ReadOut o = Decide(in, s);
        CHECK(o.source == Source::Raw && o.x == 9 && o.y == 2);
        in.dropInjected = true;
        o = Decide(in, s);
        CHECK(o.source == Source::Raw && o.x == 4 && o.y == 1);
        ReadIn mapper = read(0, 0, 6, 0); // a mapper's motion alone, dropped: nothing
        mapper.injEvents = 1, mapper.injX = 6, mapper.msSinceRaw = 0, mapper.dropInjected = true;
        o = Decide(mapper, s);
        CHECK(o.source == Source::Raw && o.x == 0 && o.y == 0);
    }
    // an absolute event since the last read: that read keeps DirectInput's motion, the next is raw again
    {
        State s;
        ReadIn in = read(0, 0, 12, 3);
        in.absoluteSeen = true, in.msSinceRaw = 0;
        ReadOut o = Decide(in, s);
        CHECK(o.source == Source::Absolute && o.x == 12 && o.y == 3);
        o = Decide(read(2, 2, 5, 5), s);
        CHECK(o.source == Source::Raw && o.x == 2);
    }
    // raw off: DirectInput's
    {
        State s;
        ReadIn in = read(2, 2, 7, 8);
        in.rawOn = false;
        const ReadOut o = Decide(in, s);
        CHECK(o.source == Source::Off && o.x == 7 && o.y == 8);
    }
    // not acquired (the call failed, the game behind): nothing, whatever either source says
    {
        State s;
        ReadIn in = read(5, 5, 7, 8);
        in.acquired = false;
        const ReadOut o = Decide(in, s);
        CHECK(o.source == Source::Nothing && o.x == 0 && o.y == 0);
        in.rawOn = false;
        CHECK(Decide(in, s).source == Source::Nothing);
    }
    // the silent rule
    {
        State s;
        // the race of a frame: DirectInput's motion one read after the raw event's, raw counts 0 - not silence, no double count
        ReadIn late = read(0, 0, 4, 0);
        late.msSinceRaw = 8;
        for (int i = 0; i < 10; ++i)
        {
            const ReadOut o = Decide(late, s);
            CHECK(o.source == Source::Raw && o.x == 0);
        }
        CHECK(!s.passThrough);
        // raw silent for 250 ms with DirectInput motion on 3 reads: DirectInput's motion passes
        State t;
        ReadIn quiet = read(0, 0, 4, 1);
        quiet.msSinceRaw = 300;
        CHECK(Decide(quiet, t).source == Source::Raw);
        CHECK(Decide(quiet, t).source == Source::Raw);
        ReadOut o = Decide(quiet, t);
        CHECK(o.source == Source::Silent && o.x == 4 && o.y == 1);
        ReadIn still = read(0, 0, 0, 0); // (a still read keeps the pass-through)
        CHECK(Decide(still, t).source == Source::Silent);
        // an injected event alone does not end it; the next hardware event does
        ReadIn inj = read(0, 0, 3, 0);
        inj.injEvents = 1, inj.injX = 3, inj.msSinceRaw = 0;
        CHECK(Decide(inj, t).source == Source::Silent);
        o = Decide(read(2, 0, 6, 0), t);
        CHECK(o.source == Source::Raw && o.x == 2 && !t.passThrough && t.diReadsWithoutRaw == 0);
        // under 250 ms of silence it never trips, however many reads moved
        State u;
        ReadIn soon = read(0, 0, 4, 1);
        soon.msSinceRaw = 200;
        for (int i = 0; i < 10; ++i)
            CHECK(Decide(soon, u).source == Source::Raw);
        soon.msSinceRaw = 260; // (and the reads counted meanwhile count)
        CHECK(Decide(soon, u).source == Source::Silent);
        // a listener that never heard a thing (msSinceRaw ~0): three moving reads
        State v;
        ReadIn never = read(0, 0, 1, 0);
        never.msSinceRaw = ~0u;
        Decide(never, v), Decide(never, v);
        CHECK(Decide(never, v).source == Source::Silent);
    }
    CHECK(SourceName(Source::Silent)[0] == 's' && SourceName(Source::Nothing)[0] == 'n');
}
