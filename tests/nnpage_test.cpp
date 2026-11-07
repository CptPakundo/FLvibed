// modules\hud\src\nnpage_rules.h (pure): the game's standings, the reputation filter, a list's window, the numbers in the game's text.
#include "../modules/hud/src/nnpage_rules.h"

#include "test.h"

void Test_nnpage()
{
    using namespace nnrules;
    // best prices: the highest first; sell at the top, buy at the lowest of those that sell it
    {
        std::vector<Quote> q(3);
        q[0].price = 100, q[0].buyHere = true;
        q[1].price = 300;
        q[2].price = 200, q[2].buyHere = true;
        SortQuotes(q);
        CHECK(q[0].price == 300 && q[2].price == 100);
        const Picks p = Pick(q);
        CHECK(p.sell == 0 && p.buy == 2);
        q[2].buyHere = q[1].buyHere = false;
        CHECK(Pick(q).buy == -1);
        CHECK(Pick(std::vector<Quote>()).sell == -1);
    }
    // the bands are the game's (0x45A6E0)
    CHECK(BandOf(-0.6f) == Hostile && BandOf(-0.59f) == Unfriendly && BandOf(-0.2f) == Unfriendly);
    CHECK(BandOf(0) == Neutral && BandOf(0.2f) == Neutral && BandOf(0.21f) == Warm && BandOf(0.6f) == Friendly);
    // the filter
    CHECK(Passes(All, -1));
    CHECK(Passes(Enemies, -0.3f) && !Passes(Enemies, 0));
    CHECK(Passes(Indifferent, 0) && !Passes(Indifferent, 0.5f));
    CHECK(Passes(Friends, 0.3f) && !Passes(Friends, 0));
    // a list keeps a row of lead and stops at its ends
    List l;
    l.count = 48, l.rows = 13;
    l.Fit();
    CHECK(l.first == 0);
    for (int i = 0; i < 12; ++i)
        l.Step(1);
    CHECK(l.sel == 12 && l.first == 1);
    CHECK(l.Step(100) && l.sel == 47 && l.first == 35);
    CHECK(!l.Step(1));
    l.Step(-100);
    CHECK(l.sel == 0 && l.first == 0);
    // short and empty
    List s;
    s.count = 3, s.rows = 13, s.sel = 9;
    s.Fit();
    CHECK(s.sel == 2 && s.first == 0);
    s.count = 0;
    s.Fit();
    CHECK(s.sel == 0 && s.first == 0 && !s.Step(1));
    // the wheel keeps the focus inside the window
    List w;
    w.count = 48, w.rows = 10;
    w.Fit();
    w.Scroll(5);
    CHECK(w.first == 5 && w.sel == 6);
    w.Fit();
    CHECK(w.first == 5);
    w.Scroll(100);
    CHECK(w.first == 38 && w.sel == 39);
    // ...and a Fit after each step leaves the window where the wheel put it, both ways (BEST PRICES stuck after one step)
    List p;
    p.count = 20, p.rows = 8;
    p.Fit();
    for (int i = 1; i <= 12; ++i)
    {
        p.Scroll(1), p.Fit();
        CHECK(p.first == i);
    }
    for (int i = 11; i >= 0; --i)
    {
        p.Scroll(-1), p.Fit();
        CHECK(p.first == i);
    }
    // numbers and headings
    CHECK(Number(L"$1,250,000") == 1250000 && Number(L"") == 0);
    CHECK(Share(500, 1000) > 0.49f && Share(500, 1000) < 0.51f && Share(5, 0) == 0 && Share(5000, 1000) == 1);
    CHECK(Heading(L"Current Level: ") == L"Current Level");
}
