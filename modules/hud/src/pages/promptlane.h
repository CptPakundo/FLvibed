// FLvibed - the pages' prompt rule, pure (the HUD 2 page framework: audit 70b 2.2 "Prompts"). The owner's rule
// (12 Nov 2026, audit owner_notes entry 44: "Use free width, then try for a second row if that's possible, rule 2 is an
// absolute last resort"), for every HUD 2 page:
//  1. the prompts fit by their measured widths into the row's free width: no count cap (today's lane takes five,
//     core\game\promptfit.h kLaneMax; this rule replaces it for the pages);
//  2. when they do not fit, a second row, if the page's layout has room for one;
//  3. only then does a prompt give way: the action with the lowest priority (ties: the later one), one at a time, until
//     the rest fit; never the one that closes or goes back (Role::Close / Back), which keeps its place.
// The order stays the page's: a row holds its prompts left to right as given; the second row takes the ones that did not
// fit the first. Pixels in, rows out. tests\promptlane_test.cpp holds every rule. No game include.
#pragma once
#include "pageinput.h"

#include <cstdint>

namespace promptlane
{
constexpr int kMax = 16; // the storage, not a cap: more than this is the caller's error (Fit refuses it)

struct Prompt
{
    float width;            // measured: glyph, word and the pill's padding
    pageinput::Role role;   // Close / Back never give way
    int priority;           // among the actions, the lowest gives way first (ties: the later one)
};

struct Space
{
    float row1 = 0;          // the free width of the first row
    float row2 = 0;          // of the second, when it exists
    bool secondRow = false;  // the page's layout has room for a second row
    float gap = 0;           // between two prompts
};

struct Fit
{
    bool ok = false;         // false: the input was beyond kMax (nothing placed)
    int rows = 0;            // 0 (no prompts), 1 or 2
    int row[kMax] = {};      // each prompt's row: 0, 1, or -1 when it gave way
    float x[kMax] = {};      // its left edge within its row
    int gaveWay[kMax] = {};  // the prompts that gave way, in the order they did
    int nGave = 0;
    bool overflow = false;   // even the close / back prompts alone do not fit: they stay, past the free width
};

// The width of the prompts `use` marks, in order, on one row.
inline float RowWidth(const Prompt* p, int n, const bool* use, float gap)
{
    float w = 0;
    int k = 0;
    for (int i = 0; i < n; ++i)
        if (use[i])
            w += (k++ ? gap : 0) + p[i].width;
    return w;
}

// Lays the used prompts on one row, or two (the first row filled in order, the rest to the second): true when they fit.
inline bool Place(const Prompt* p, int n, const bool* use, const Space& s, bool twoRows, Fit& f)
{
    float x[2] = {0, 0};
    int count[2] = {0, 0};
    int r = 0;
    for (int i = 0; i < n; ++i)
    {
        if (!use[i])
        {
            f.row[i] = -1;
            continue;
        }
        float at = x[r] + (count[r] ? s.gap : 0);
        if (r == 0 && at + p[i].width > s.row1 && twoRows && count[0])
            r = 1, at = 0;
        f.row[i] = r, f.x[i] = at;
        x[r] = at + p[i].width, ++count[r];
    }
    f.rows = count[1] ? 2 : count[0] ? 1 : 0;
    return x[0] <= s.row1 && x[1] <= (twoRows ? s.row2 : 0);
}

inline Fit FitPrompts(const Prompt* p, int n, const Space& s)
{
    Fit f;
    if (n < 0 || n > kMax)
        return f;
    f.ok = true;
    bool use[kMax];
    for (int i = 0; i < n; ++i)
        use[i] = true;
    for (;;)
    {
        // 1. one row by width; 2. two rows when the page has room
        if (Place(p, n, use, s, false, f))
            return f;
        if (s.secondRow && Place(p, n, use, s, true, f))
            return f;
        // 3. the last resort: the lowest-priority action gives way (ties: the later one); never close / back
        int drop = -1;
        for (int i = 0; i < n; ++i)
            if (use[i] && p[i].role == pageinput::Role::Action && (drop < 0 || p[i].priority <= p[drop].priority))
                drop = i;
        if (drop < 0)
        {
            // only the way out is left and it does not fit: it stays (the caller shrinks or reports it)
            f.overflow = true;
            Place(p, n, use, s, s.secondRow, f);
            return f;
        }
        use[drop] = false;
        f.gaveWay[f.nGave++] = drop;
    }
}
} // namespace promptlane
