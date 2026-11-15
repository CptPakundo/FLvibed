// FLvibed - rules the screens' models share, pure (the HUD 2 page framework, audit 70b 2.4; moved from
// modules\hud\src\nnpage_rules.h in F3a, whose names stay as using-declarations): a list's window on its rows, and a number
// out of the game's text. No game include: tests\nnpage_test.cpp holds them.
#pragma once
#include <algorithm>
#include <cwctype>
#include <string>

namespace screens
{
// A text in capitals, letter by letter (towupper): the pages' words of the game (moved from modules\hud\src\pages\page_kit.h
// in F3d, whose name stays as a using-declaration, so a screen's read and a view say it the same way).
inline std::wstring Upper(std::wstring s)
{
    for (wchar_t& c : s)
        c = towupper(c);
    return s;
}

// A list's window on `count` rows: `sel` the row in focus, `first` the first shown, `rows` how many stand. The focus keeps a row of lead, so the next one is always seen.
struct List
{
    int count = 0, sel = 0, first = 0, rows = 1;
    void Fit()
    {
        sel = count > 0 ? std::max(0, std::min(sel, count - 1)) : 0;
        const int lead = rows > 2 ? 1 : 0;
        if (sel - lead < first)
            first = sel - lead;
        if (sel + lead >= first + rows)
            first = sel + lead - rows + 1;
        first = std::max(0, std::min(first, std::max(0, count - rows)));
    }
    // The focus moves; the ends stop it. True when it moved.
    bool Step(int by)
    {
        const int was = sel;
        sel += by;
        Fit();
        return sel != was;
    }
    // The wheel: the window moves, the focus stays inside it - inside Fit's lead too, or the next Fit pulls the window back
    // (Shockwave, 7 Nov 2026: BEST PRICES scrolled once, then stuck both ways).
    void Scroll(int by)
    {
        first = std::max(0, std::min(first + by, std::max(0, count - rows)));
        const int lead = rows > 2 ? 1 : 0;
        const int lo = first > 0 ? first + lead : 0, hi = first + rows < count ? first + rows - 1 - lead : count - 1;
        sel = std::max(lo, std::min(sel, hi));
        sel = count > 0 ? std::max(0, std::min(sel, count - 1)) : 0;
    }
};

// The digits of the game's text: "$1,250,000" is 1250000, "" and words are 0.
inline long long Number(const std::wstring& s)
{
    long long n = 0;
    for (wchar_t c : s)
        if (c >= L'0' && c <= L'9' && n < 100000000000000LL)
            n = n * 10 + (c - L'0');
    return n;
}

// A standing as the dealers' rows say it: 0.25 is "25%", rounded half away from zero (moved from basepage.cpp in F3b).
inline std::wstring RepWord(float rep) { return std::to_wstring(static_cast<int>(rep * 100 + (rep >= 0 ? 0.5f : -0.5f))) + L"%"; }

// A list window of the game's (NN_Inventory, NN_Dealer) shows five rows with an item mesh each: mesh k shows its row first + k
// (moved from modules\hud\src\nnpage_rules.h in F3b, whose names stay as using-declarations).
constexpr int kMeshes = 5;
// Where the game's window of five should start so its own meshes show the most of the page's shown rows (game rows, -1 none): the first row of
// the best start; -1 when no shown row has a game row. (Every row it misses borrows a mesh, which the game's fill undoes each frame: a load each.)
inline int BestWindowStart(const int* gameRow, int n)
{
    int best = -1, most = 0;
    for (int i = 0; i < n; ++i)
    {
        if (gameRow[i] < 0)
            continue;
        int covered = 0;
        for (int j = 0; j < n; ++j)
            covered += gameRow[j] >= gameRow[i] && gameRow[j] < gameRow[i] + kMeshes;
        if (covered > most || (covered == most && gameRow[i] < best))
            best = gameRow[i], most = covered;
    }
    return best;
}
} // namespace screens
