// The [Modules] rows (core\modules_rules.h): an older ini behaves as it did, a present row wins, the save writes only what the
// next start would otherwise change.
#include "modules_rules.h"

#include "test.h"

using namespace modules;

static void Rows(int subtitles, int controller, int hud, int movies, bool (&on)[kRows])
{
    int ini[kRows];
    for (int& v : ini)
        v = kAbsent;
    ini[kSubtitles] = subtitles;
    ini[kController] = controller;
    ini[kHud] = hud;
    ini[kMovies] = movies;
    Resolve(ini, on);
}

void Test_modules_rules()
{
    bool on[kRows];

    // a missing file or key: everything on
    Rows(kAbsent, kAbsent, kAbsent, kAbsent, on);
    for (bool row : on)
        CHECK(row);

    // an older ini, `controller = 0`: everything the controller carried goes with it, the subtitles and movies stay
    Rows(1, 0, kAbsent, kAbsent, on);
    CHECK(on[kSubtitles] && on[kMovies]);
    CHECK(!on[kController] && !on[kHud] && !on[kAutopilot] && !on[kSpeedrun] && !on[kDisplay] && !on[kNpc]);

    // an older ini, `subtitles = 0`
    Rows(0, 1, kAbsent, kAbsent, on);
    CHECK(!on[kSubtitles] && !on[kMovies]);
    CHECK(on[kController] && on[kHud] && on[kAutopilot] && on[kSpeedrun] && on[kDisplay] && on[kNpc]);

    // both at 0
    Rows(0, 0, kAbsent, kAbsent, on);
    for (bool row : on)
        CHECK(!row);

    // a present row wins: HUD without Controller, Subtitles without Movies
    Rows(1, 0, 1, 0, on);
    CHECK(!on[kController] && on[kHud] && !on[kAutopilot]);
    CHECK(on[kSubtitles] && !on[kMovies]);

    // any other number in a row is on, as GetPrivateProfileInt's 2 always was
    Rows(2, 1, kAbsent, kAbsent, on);
    CHECK(on[kSubtitles]);

    // the save: nothing changed, nothing written, and an inherited row stays absent
    int ini[kRows];
    bool want[kRows], write[kRows];
    for (int r = 0; r < kRows; ++r)
        ini[r] = kAbsent, want[r] = true;
    ToWrite(ini, want, write);
    for (bool w : write)
        CHECK(!w);
    // an older ini with `controller = 0`: its inherited zeros stay inherited
    ini[kController] = 0;
    for (int r : {kController, kHud, kAutopilot, kSpeedrun, kDisplay, kNpc})
        want[r] = false;
    ToWrite(ini, want, write);
    for (bool w : write)
        CHECK(!w);
    // the controller turned off on the page, the rest left on: it writes itself and keeps the rows it would take along
    for (int r = 0; r < kRows; ++r)
        ini[r] = kAbsent, want[r] = true;
    want[kController] = false;
    ToWrite(ini, want, write);
    CHECK(write[kController] && write[kHud] && write[kAutopilot] && write[kSpeedrun] && write[kDisplay] && write[kNpc]);
    CHECK(!write[kSubtitles] && !write[kMovies]);
    // the controller off and the HUD off too: the HUD inherits that, only the rest are written
    want[kHud] = false;
    ToWrite(ini, want, write);
    CHECK(write[kController] && !write[kHud] && write[kAutopilot]);
    // an older ini with `controller = 0`, the controller turned on: the rows that followed it come back with it, none written
    for (int r = 0; r < kRows; ++r)
        ini[r] = kAbsent, want[r] = true;
    ini[kController] = 0;
    for (int r : {kHud, kAutopilot, kSpeedrun, kDisplay, kNpc})
        want[r] = false;
    Follow(ini, kController, want);
    CHECK(want[kHud] && want[kNpc]);
    ToWrite(ini, want, write);
    CHECK(write[kController]);
    for (int r : {kHud, kAutopilot, kSpeedrun, kDisplay, kNpc})
        CHECK(!write[r]);
    // ... but a present row keeps its word
    ini[kHud] = 0, want[kHud] = false;
    Follow(ini, kController, want);
    CHECK(!want[kHud]);
    // a present row that matches stays as it is; one that differs is written
    ini[kController] = 0, ini[kHud] = 1;
    for (int r = 0; r < kRows; ++r)
        want[r] = true;
    want[kController] = false;
    ToWrite(ini, want, write);
    CHECK(!write[kController] && !write[kHud] && write[kAutopilot]);

    // each row's parent comes first, or Resolve reads a row it has not set yet
    for (int r = 0; r < kRows; ++r)
        CHECK(kInherits[r] < r);
}
