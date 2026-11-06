// modules\subtitles\src\impl\mix_cues.h (pure): a scene mix's lines come due on the mix clock (a line already over is passed
// over); the track ends with its last line, or when the scene, once seen, has been gone for 0.5 s (skipped when that is
// before the last line's end) - a scene without letterbox bars that never reads as a scene ends with its lines.
#include "test.h"

#include "../modules/subtitles/src/impl/mix_cues.h"

void Test_mix_cues()
{
    using namespace mixcues;
    const std::vector<Window> w = {{1.0f, 3.0f}, {3.5f, 6.0f}, {4.0f, 4.5f}, {9.0f, 12.0f}};
    std::vector<size_t> show;
    size_t next = 0;
    Due(w, next, 0.5, show);
    CHECK(show.empty() && next == 0);
    Due(w, next, 1.0, show);
    CHECK(show.size() == 1 && show[0] == 0 && next == 1);
    Due(w, next, 1.2, show); // nothing new
    CHECK(show.empty() && next == 1);
    Due(w, next, 4.7, show); // a hitch: line 2 (4.0-4.5) is over, line 1 still runs
    CHECK(show.size() == 1 && show[0] == 1 && next == 3);
    Due(w, next, 20.0, show); // past everything: nothing shown late
    CHECK(show.empty() && next == 4);

    bool skipped = false;
    // no scene ever seen (no bars, no hidden room buttons): ends 1 s after the last line
    Track a;
    CHECK(!a.Ended(5.0, 12.0f, false, skipped));
    CHECK(!a.Ended(12.9, 12.0f, false, skipped));
    CHECK(a.Ended(13.1, 12.0f, false, skipped) && !skipped);
    // a scene seen, then gone for 0.5 s before the last line: skipped
    Track b;
    CHECK(!b.Ended(1.0, 12.0f, true, skipped));
    CHECK(!b.Ended(5.0, 12.0f, false, skipped)); // gone: the clock starts
    CHECK(!b.Ended(5.3, 12.0f, true, skipped));  // back (a flicker): not an end
    CHECK(!b.Ended(6.0, 12.0f, false, skipped));
    CHECK(!b.Ended(6.4, 12.0f, false, skipped));
    CHECK(b.Ended(6.6, 12.0f, false, skipped) && skipped);
    // the scene ends after the last line: an ordinary end
    Track c;
    CHECK(!c.Ended(1.0, 4.0f, true, skipped));
    CHECK(!c.Ended(4.2, 4.0f, false, skipped));
    CHECK(c.Ended(4.8, 4.0f, false, skipped) && !skipped);
}
