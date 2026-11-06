// modules\hud\src\navmap_labels.h (pure): the game's gate, the transform, the declutter: nothing hidden is revealed, labels never meet, the same scene gives the same answer.
#include "test.h"

#include "../modules/hud/src/navmap_labels.h"

#include <vector>

using namespace navlabels;

namespace
{
Record Base()
{
    Record r;
    r.type = 0, r.flags = 1u << 8, r.visible = true, r.nameIds = 196609, r.radius = 1000;
    return r;
}
} // namespace

void Test_navmap_labels()
{
    // the game's gate, every leg of it: what it hides stays hidden
    Record r = Base();
    CHECK(Named(r) && Rank(r) == 0);
    r.visible = false;
    CHECK(!Named(r) && Rank(r) == -1);
    r = Base(), r.nameIds = 0;
    CHECK(!Named(r));
    r = Base(), r.type = 2;
    CHECK(!Named(r));
    r = Base(), r.flags = 1u << 7; // a lane ring: no pass of the game's names it
    CHECK(!Named(r));

    // bases, gates, then the rest
    r = Base(), r.flags = 2;
    CHECK(Rank(r) == 0);
    r.flags = 0x40;
    CHECK(Rank(r) == 1);
    r.flags = 0x800;
    CHECK(Rank(r) == 1);
    r.flags = 1;
    CHECK(Rank(r) == 2);

    CHECK(!LabelsInLayer(4) && !LabelsInLayer(6) && !LabelsInLayer(7) && LabelsInLayer(0) && LabelsInLayer(1));

    // the transform: the system view fills the viewport, north up
    const navlayout::Rect port = {1000, 200, 2000, 1200};
    View v;
    v.scaleX = 100000, v.scaleZ = 100000;
    float x = 0, y = 0;
    CHECK(ToScreen(v, port, 0, 0, x, y));
    CHECK_NEAR(x, 1500, 0.01);
    CHECK_NEAR(y, 700, 0.01);
    CHECK(ToScreen(v, port, 50000, -50000, x, y));
    CHECK_NEAR(x, 1750, 0.01);
    CHECK_NEAR(y, 450, 0.01);
    CHECK(!ToScreen(v, port, 150000, 0, x, y)); // (outside the viewport: no mark on the panel)
    // the sector view: the game's zoom about its centre, the same numbers as its label code
    v.view = 2, v.zoom = 2, v.halfW = 0.25f, v.halfH = 0.25f, v.centreX = 0.05f, v.centreY = -0.05f;
    CHECK(ToScreen(v, port, 0, 0, x, y));
    CHECK_NEAR(x, 1500 + 0.5f * 1000 * 2 * 0.2f, 0.1);
    CHECK_NEAR(y, 700 + 0.5f * 1000 * 2 * 0.2f, 0.1);
    // no transform, no mark: the universe, a value not read yet
    v.view = 0;
    CHECK(!ToScreen(v, port, 0, 0, x, y));
    v.view = 1, v.scaleX = 0;
    CHECK(!ToScreen(v, port, 0, 0, x, y));

    // the game's 550 rule in the system view alone
    v.view = 1, v.scaleX = 1, v.mapScale = 1;
    r = Base(), r.radius = 549;
    CHECK(!LabelByRule(r, v));
    r.radius = 551;
    CHECK(LabelByRule(r, v));
    v.view = 2;
    r.radius = 1;
    CHECK(LabelByRule(r, v));

    // the declutter: a base beats a gate on the same spot, the pinned beats both; the left side is the second try
    const navlayout::Rect b = {0, 0, 1000, 1000};
    Label l[4];
    int order[4];
    for (Label& a : l)
        a.w = 100, a.h = 20, a.gap = 8;
    l[0].x = 200, l[0].y = 200, l[0].rank = 1;
    l[1].x = 205, l[1].y = 202, l[1].rank = 0;
    l[2].x = 900, l[2].y = 500, l[2].rank = 2;
    l[3].x = 205, l[3].y = 800, l[3].rank = 2;
    const int n = Declutter(l, 4, order, b);
    CHECK(n >= 3);
    CHECK(l[1].shown);
    CHECK(l[2].shown && l[2].left); // (no room right of it)
    CHECK(l[3].shown);
    for (int i = 0; i < 4; ++i)
        for (int j = i + 1; j < 4; ++j)
            if (l[i].shown && l[j].shown)
                CHECK(!l[i].box.Overlaps(l[j].box));
    // stable: the same scene twice
    Label again[4];
    for (int i = 0; i < 4; ++i)
        again[i] = l[i];
    Declutter(again, 4, order, b);
    for (int i = 0; i < 4; ++i)
        CHECK(again[i].shown == l[i].shown && again[i].left == l[i].left);
    // pinned first
    l[0].pinned = true;
    Declutter(l, 4, order, b);
    CHECK(l[0].shown);

    // a dense scene: nothing overlaps and something stands
    std::vector<Label> many(120);
    std::vector<int> ord(120);
    for (int i = 0; i < 120; ++i)
    {
        many[i].x = static_cast<float>((i * 37) % 900 + 50), many[i].y = static_cast<float>((i * 91) % 900 + 50), many[i].w = 140, many[i].h = 24, many[i].gap = 8, many[i].rank = i % 3;
    }
    CHECK(Declutter(many.data(), 120, ord.data(), b) > 5);
    for (int i = 0; i < 120; ++i)
        for (int j = i + 1; j < 120; ++j)
            if (many[i].shown && many[j].shown)
                CHECK(!many[i].box.Overlaps(many[j].box));
}
