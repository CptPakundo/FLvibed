// The NeuroNet pages' rules, pure (docs\NEURONET_PLAN.md): the game's five standings, the filter of the reputation list, a list's window, a number out of the game's text.
// Host-free: tests\nnpage_test.cpp holds them.
#pragma once
#include <algorithm>
#include <string>
#include <vector>

namespace nnrules
{
// The game's own five steps (0x45A6E0): at or under -0.6, at or under -0.2, up to 0.2, under 0.6, from 0.6.
enum Band
{
    Hostile,
    Unfriendly,
    Neutral,
    Warm,
    Friendly
};
inline Band BandOf(float v) { return v <= -0.6f ? Hostile : v <= -0.2f ? Unfriendly : v >= 0.6f ? Friendly : v > 0.2f ? Warm : Neutral; }

// The reputation list's filter, one press each: all, then who shoots, who does not care, who likes the player.
enum Filter
{
    All,
    Enemies,
    Indifferent,
    Friends,
    kFilters
};
inline bool Passes(Filter f, float v)
{
    const Band b = BandOf(v);
    return f == All || (f == Enemies && b <= Unfriendly) || (f == Indifferent && b == Neutral) || (f == Friends && b >= Warm);
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
// A row of the game's commodity list by base: its price there, and whether the base sells the good (its own word beside it).
struct Quote
{
    long long price = 0;
    bool buyHere = false;
    std::wstring sells, base, system;
};
// Highest price first, the game's order among equals.
inline void SortQuotes(std::vector<Quote>& q)
{
    std::stable_sort(q.begin(), q.end(), [](const Quote& a, const Quote& b) { return a.price > b.price; });
}
// The two answers, as rows of a sorted list: where to sell (the highest price, any base buys), where to buy (the lowest of those that sell it); -1 none.
struct Picks
{
    int sell = -1, buy = -1;
};
inline Picks Pick(const std::vector<Quote>& q)
{
    Picks p;
    for (int i = 0; i < static_cast<int>(q.size()); ++i)
    {
        if (p.sell < 0 && q[i].price > 0)
            p.sell = i;
        if (q[i].buyHere && q[i].price > 0 && (p.buy < 0 || q[i].price <= q[p.buy].price))
            p.buy = i;
    }
    return p;
}

// How far the player's worth is on the way to the next level: 0 .. 1; 0 when the game names no next level.
inline float Share(long long worth, long long next) { return next <= 0 ? 0 : std::max(0.0f, std::min(1.0f, static_cast<float>(worth) / static_cast<float>(next))); }

// A heading of the game's without its colon.
inline std::wstring Heading(std::wstring s)
{
    while (!s.empty() && (s.back() == L':' || s.back() == L' '))
        s.pop_back();
    return s;
}
} // namespace nnrules
