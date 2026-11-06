// FLvibed - the [Modules] rows of EXE\FLvibed.ini, pure (docs\MODULES_PLAN.md section 2; tests\modules_rules_test.cpp).
// A row that is present wins; a row that is absent inherits, so an older ini behaves as it did and "HUD without Controller"
// is `controller = 0`, `hud = 1`. subtitles and controller are the first two rows and inherit nothing: absent = on.
// The other rows are read and logged from P2 on and take effect when their module is cut out (P5-P9). `movies` runs beside
// `subtitles` or alone: one of `subtitles` / `controller` must be on, or nothing starts.
// `safe = 1` is the troubleshooting state: nothing starts, whatever the rows say. The options page sets every row; it keeps one of `subtitles` and `controller` on.
#pragma once

namespace modules
{
enum Row : int
{
    kSubtitles,
    kController,
    kHud,
    kAutopilot,
    kSpeedrun,
    kDisplay,
    kNpc,
    kMovies,
    kRows
};

inline constexpr const char* kRowNames[kRows] = {"subtitles", "controller", "hud",     "autopilot",
                                                 "speedrun",  "display",    "npc",     "movies"};
inline constexpr int kInherits[kRows] = {-1, -1, kController, kController, kController, kController, kController, kSubtitles};
inline constexpr int kAbsent = -1; // what GetPrivateProfileInt is told to give for a row that is not there

// `ini`: each row as read, kAbsent = no such row. A parent comes before the rows that inherit from it.
inline void Resolve(const int (&ini)[kRows], bool (&on)[kRows])
{
    for (int r = 0; r < kRows; ++r)
        on[r] = ini[r] != kAbsent ? ini[r] != 0 : kInherits[r] < 0 ? true : on[kInherits[r]];
}

// `parent` was just turned on: the rows that follow it and are absent read on again, as they will at the next start, so the page shows it and the save writes none of them.
inline void Follow(const int (&ini)[kRows], int parent, bool (&want)[kRows])
{
    for (int r = 0; r < kRows; ++r)
        if (kInherits[r] == parent && ini[r] == kAbsent)
            want[r] = true;
}

// The save: the rows to write so the next start resolves to `want`, what the page shows. An absent row stays absent while it would inherit the value anyway, so a
// first save freezes nothing; a parent turned off writes its children that stay on, so it does not take them along.
inline void ToWrite(const int (&ini)[kRows], const bool (&want)[kRows], bool (&write)[kRows])
{
    for (int r = 0; r < kRows; ++r)
    {
        const bool next = ini[r] != kAbsent ? ini[r] != 0 : kInherits[r] < 0 ? true : want[kInherits[r]];
        write[r] = next != want[r];
    }
}
} // namespace modules
