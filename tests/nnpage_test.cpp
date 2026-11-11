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
    // the rows' item meshes (issue 58): the game's window of five starts at its row 2; the page shows its rows 0, EMPTY, 4, 9, 11
    {
        const int game[5] = {0, -1, 4, 9, 11};
        const bool wants[5] = {true, false, true, true, false}; // (row 11: no icon of its own)
        int meshOf[5];
        bool borrowed[5];
        // nothing holds a model yet: row 4 keeps the game's mesh 2, rows 0 and 9 borrow the lowest free ones, 0 and 1
        AssignMeshes(2, game, wants, 5, [](int, int) { return false; }, meshOf, borrowed);
        CHECK(meshOf[2] == 2 && !borrowed[2]);
        CHECK(meshOf[0] == 0 && borrowed[0] && meshOf[3] == 1 && borrowed[3]);
        CHECK(meshOf[1] == -1 && meshOf[4] == -1);
        // a free mesh that holds the row's model already is taken first (no load)
        AssignMeshes(2, game, wants, 5, [](int row, int mesh) { return row == 3 && mesh == 4; }, meshOf, borrowed);
        CHECK(meshOf[3] == 4 && meshOf[0] == 0);
        // none holds it: a mesh the game does not blank each frame (`keeps`: its own row has a model) before the lowest free one
        AssignMeshes(2, game, wants, 5, [](int, int) { return false; }, [](int mesh) { return mesh == 3; }, meshOf, borrowed);
        CHECK(meshOf[0] == 3 && borrowed[0] && meshOf[3] == 0 && borrowed[3]);
        // holding the model still comes first
        AssignMeshes(2, game, wants, 5, [](int row, int mesh) { return row == 0 && mesh == 1; }, [](int mesh) { return mesh == 3; }, meshOf, borrowed);
        CHECK(meshOf[0] == 1 && meshOf[3] == 3);
        // every shown row inside the window: nothing borrowed
        const int inside[3] = {2, 3, 6};
        const bool all[3] = {true, true, true};
        AssignMeshes(2, inside, all, 3, [](int, int) { return false; }, meshOf, borrowed);
        CHECK(meshOf[0] == 0 && meshOf[1] == 1 && meshOf[2] == 4 && !borrowed[0] && !borrowed[1] && !borrowed[2]);
        // five rows outside the window, five meshes: each gets one
        const int far[5] = {10, 11, 12, 13, 14};
        const bool five[5] = {true, true, true, true, true};
        AssignMeshes(0, far, five, 5, [](int, int) { return false; }, meshOf, borrowed);
        CHECK(meshOf[0] == 0 && meshOf[4] == 4 && borrowed[4]);
    }
    // the game's window for the shown rows: the start that covers the most (issue 58: an EMPTY top row left the window where it was)
    {
        const int shown[5] = {-1, 5, 6, 7, 8};
        CHECK(BestWindowStart(shown, 5) == 5);
        const int spread[5] = {1, 2, -1, 9, 10}; // (1..5 covers two, 9..13 two: the lower)
        CHECK(BestWindowStart(spread, 5) == 1);
        const int three[4] = {0, 7, 8, 11}; // (7..11 covers three)
        CHECK(BestWindowStart(three, 4) == 7);
        const int none[2] = {-1, -1};
        CHECK(BestWindowStart(none, 2) == -1 && BestWindowStart(none, 0) == -1);
    }
    // numbers and headings
    CHECK(Number(L"$1,250,000") == 1250000 && Number(L"") == 0);
    CHECK(Share(500, 1000) > 0.49f && Share(500, 1000) < 0.51f && Share(5, 0) == 0 && Share(5000, 1000) == 1);
    CHECK(Heading(L"Current Level: ") == L"Current Level");
}
