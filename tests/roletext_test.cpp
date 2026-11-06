// core\game\roletext.h (pure): the ladder of fonts, a column's pick, the rich fit. Numbers from the dealer at 1280x800, 5 Nov 2026.
#include "test.h"

#include "game/roletext.h"

using namespace roletext;

static std::vector<Rung> Agency()
{
    return Ladder({{0.0435f, 6}, {0.029f, 1}, {0.035f, 3}, {0.0385f, 4}, {0.04f, 5}, {0.029f, 9}, {0.045f, 7}, {0.03f, 2}, {0.06f, 8}});
}

void Test_roletext()
{
    const std::vector<Rung> l = Agency();
    CHECK(l.size() == 8); // the second 0.029 is the same text
    CHECK(l.front().font == 1 && l.back().font == 8);
    CHECK(RungAt(l, 0.035f) == 2);
    CHECK(RungAt(l, 0.036f) == 2);
    CHECK(RungAt(l, 0.02f) == -1);

    Room room;
    room.width = 206, room.height = 54, room.gap = 8, room.screenH = 800, room.nameBase = 0.035f, room.priceBase = 0.035f;
    const Pick base{RungAt(l, 0.035f), RungAt(l, 0.035f)}, top{RungAt(l, 0.0385f), RungAt(l, 0.045f)};

    // the owner's row: both climb to the top
    Pick p = PickRows({{120, 100, 42}, {108, 100, 40}}, room, l, base, top);
    CHECK(p.price == top.price && p.name == top.name);
    // the class line and a long price: the price stops where it would run into the line (125 % put them over each other)
    p = PickRows({{120, 140, 50}}, room, l, base, top);
    CHECK(l[p.price].height < 0.045f);
    CHECK(140 + 8 + 50 * l[p.price].height / 0.035f * kWidthMargin <= 206);
    // a name as wide as the row stays at its base: it never wraps away from its line
    p = PickRows({{200, 0, 40}}, room, l, base, top);
    CHECK(p.name == base.name && p.price == top.price);
    // one row decides for the column
    p = PickRows({{100, 80, 40}, {200, 80, 40}}, room, l, base, top);
    CHECK(p.name == base.name);
    // a short row: the price takes the height first, the name what is left
    room.height = 48;
    p = PickRows({{100, 80, 40}}, room, l, base, top);
    CHECK(p.price > base.price && p.name == base.name);
    // nothing fits: the base, never under it
    room.height = 10;
    p = PickRows({{100, 80, 40}}, room, l, base, top);
    CHECK(p.price == base.price && p.name == base.name);
    // the cap is honoured: a pick never climbs past the window's last one
    room.height = 54;
    p = PickRows({{100, 80, 40}}, room, l, base, Pick{base.name, RungAt(l, 0.04f)});
    CHECK(p.price == RungAt(l, 0.04f) && p.name == base.name);

    // rich: within the boxes, labels level with their values
}
