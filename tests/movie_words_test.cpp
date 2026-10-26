// subtitles\src\impl\movie_words.h (pure): an intro line's word_times.tsv row (movie clock) -> its word starts from the
// line's own start; -1 stays unknown; a word before the line's start counts from 0; a count mismatch or nothing placed
// = no timings (the estimate); the text hash is the aligner's FNV-1a.
#include <cstdlib>
#include <cwctype>

#include "test.h"

#include "../subtitles/src/impl/movie_words.h"

void Test_movie_words()
{
    using moviewords::Relative;
    std::vector<float> w;
    // "It was the 23rd century." at 5.70 s: starts on the movie clock -> from the line's start
    CHECK(Relative("5730,5810,5950,6100,6420", 5.70f, 5, w));
    CHECK(w.size() == 5);
    CHECK_NEAR(w[0], 0.03, 1e-4);
    CHECK_NEAR(w[4], 0.72, 1e-4);
    // an unplaced word stays unknown, the rest still count
    CHECK(Relative("1000,-1,1500", 1.0f, 3, w));
    CHECK(w[1] < 0 && w[0] == 0.0f);
    CHECK_NEAR(w[2], 0.5, 1e-4);
    // a word the aligner placed before the line's (corrected) start counts from 0
    CHECK(Relative("900,1200", 1.0f, 2, w));
    CHECK(w[0] == 0.0f);
    // the text has another word count than the row (changed text): no timings
    CHECK(!Relative("1000,1200", 1.0f, 3, w) && w.empty());
    // nothing placed, or an empty / malformed field: no timings
    CHECK(!Relative("-1,-1", 0.0f, 2, w));
    CHECK(!Relative("", 0.0f, 0, w));
    CHECK(!Relative("100,,200", 0.0f, 3, w));
    // the text hash align_words.py writes (fnv1a of the UTF-8 text): "" and a known value
    CHECK(moviewords::TextFnv("") == 0x811C9DC5u);
    CHECK(moviewords::TextFnv("a") == 0xE40C292Cu);
    CHECK(moviewords::WordCount(L"  Copy that.  Reading green. ") == 4);
}
