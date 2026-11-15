// FLvibed - the game's commodity list by base (NN_CommodityByBase, the BEST PRICES page), its rules, pure (moved from
// modules\hud\src\nnpage_rules.h in audit 70b F3a, whose names stay as using-declarations): a row, the order, the two answers.
// No game include: tests\nnpage_test.cpp holds them.
#pragma once
#include <algorithm>
#include <string>
#include <vector>

namespace screens
{
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
} // namespace screens
