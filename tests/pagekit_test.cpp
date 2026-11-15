// modules\hud\src\pages\page_kit.h, its pure parts (audit 70b F0): a hole's box grows to take in each rectangle and starts
// from the first one; money in thousands with its sign before the dollar; upper case; a wrap keeps the text's own lines.
// Drawing itself is the nets' (nn_pages, classic_frames), and a chip's hit record needs the skin's chip drawn (not linked
// here).
#include "test.h"

#include "../modules/hud/src/pages/page_kit.h"

void Test_pagekit()
{
    // A box takes the first rectangle as it is, then grows to take in each one added; nothing added: not ok.
    pagekit::Box b;
    CHECK(!b.ok);
    b.AddRect(10, 20, 30, 40);
    CHECK(b.ok && b.x0 == 10 && b.y0 == 20 && b.x1 == 30 && b.y1 == 40);
    b.AddRect(5, 25, 20, 50);
    CHECK(b.x0 == 5 && b.y0 == 20 && b.x1 == 30 && b.y1 == 50);
    b.AddRect(-100, -100, -90, -90); // (a box left of and above the screen still counts: the game's own place before a move)
    CHECK(b.x0 == -100 && b.y0 == -100 && b.x1 == 30 && b.y1 == 50);

    // Money: thousands, the minus before the dollar, zero plain.
    CHECK(pagekit::Money(0) == L"$0");
    CHECK(pagekit::Money(999) == L"$999");
    CHECK(pagekit::Money(1000) == L"$1,000");
    CHECK(pagekit::Money(1234567) == L"$1,234,567");
    CHECK(pagekit::Money(-25000) == L"-$25,000");

    CHECK(pagekit::Upper(L"Light Fighter mk2") == L"LIGHT FIGHTER MK2");

    // A wrap wider than any line keeps the text's own lines, an empty one too.
    const std::vector<std::wstring> lines = pagekit::Wrap(L"one two\n\nthree", 0.025f * 800, 1e9f);
    CHECK(lines.size() == 3 && lines[0] == L"one two" && lines[1].empty() && lines[2] == L"three");
}
