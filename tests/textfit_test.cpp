// core\game\textfit.h (pure): the one text-fit rule - unchanged when it fits; else the size steps down only as far as
// needed, never under the floor; only at the floor the ellipsis cut (trailing spaces / separators dropped); a size
// asked under the floor is never enlarged; the cache keys on string, size, floor, box, track and font.
#include "test.h"

#include "game/textfit.h"

static std::vector<float> Adv(const std::wstring& s, float a) { return std::vector<float>(s.size(), a); }

void Test_textfit()
{
    using textfit::Fit;
    const std::wstring s = L"PLANET MANHATTAN"; // 16 characters, 10 px each at em 20 -> 160 px at size 20
    const auto adv = Adv(s, 10);
    // fits: unchanged
    textfit::Result r = Fit(s, adv, 10, 20, 20, 12, 200, 0);
    CHECK(r.text == s && r.size == 20 && !r.shrunk && !r.cut);
    CHECK_NEAR(r.width, 160, 0.01);
    // a bit too wide: shrunk just enough (150 / 160 x 20 = 18.75), not cut
    r = Fit(s, adv, 10, 20, 20, 12, 150, 0);
    CHECK(r.text == s && r.shrunk && !r.cut);
    CHECK(r.size <= 18.75f && r.size > 18.7f);
    CHECK(r.width <= 150);
    // much too wide: at the floor (12 -> 6 px a character), cut with the ellipsis: 60 px holds 9 characters + ellipsis
    r = Fit(s, adv, 10, 20, 20, 12, 60, 0);
    CHECK(r.cut && r.size == 12);
    CHECK(r.text.size() >= 2 && r.text.back() == textfit::kEllipsis);
    CHECK(r.width <= 60);
    // "PLANET MAN" + ... would be 11 x 6 = 66 > 60: "PLANET MA" is 9 + 1 = 60; a cut never ends in a space
    CHECK(r.text == std::wstring(L"PLANET MA") + textfit::kEllipsis);
    r = Fit(s, adv, 10, 20, 20, 12, 49, 0); // 8 slots: "PLANET " -> "PLANET" + ellipsis (space dropped)
    CHECK(r.text == std::wstring(L"PLANET") + textfit::kEllipsis);
    // asked under the floor: never enlarged, cut at the asked size
    r = Fit(s, adv, 10, 20, 10, 12, 40, 0);
    CHECK(r.size == 10 && r.cut);
    // tracking counts per gap: 16 x 10 + 15 x 0.1 x 20 = 190 at size 20
    r = Fit(s, adv, 10, 20, 20, 12, 190, 0.1f);
    CHECK(!r.shrunk && !r.cut);
    r = Fit(s, adv, 10, 20, 20, 12, 189, 0.1f);
    CHECK(r.shrunk && !r.cut && r.width <= 189);
    // nothing fits: empty, never wider than the box
    r = Fit(s, adv, 10, 20, 20, 12, 2, 0);
    CHECK(r.text.empty() && r.width == 0);
    // a separator before the cut is dropped too (the middle dot of "A  \u00B7  B")
    const std::wstring sep = L"AB \u00B7 CDEF";
    r = Fit(sep, Adv(sep, 10), 10, 10, 10, 10, 50, 0); // 5 slots: "AB \u00B7" -> "AB"
    CHECK(r.text == std::wstring(L"AB") + textfit::kEllipsis);
    // the legibility floor: 0.024 H, never under 14 px (800x600 and below)
    CHECK_NEAR(textfit::MinSize(2160), 51.84, 0.01);
    CHECK_NEAR(textfit::MinSize(720), 17.28, 0.01);
    CHECK_NEAR(textfit::MinSize(480), 14.0, 0.01);
    // the cache: a hit returns the stored result; another box / font misses
    textfit::Cache c;
    CHECK(c.Find(s, 20, 12, 150, 0, 1) == nullptr);
    c.Put(Fit(s, adv, 10, 20, 20, 12, 150, 0));
    const textfit::Result* hit = c.Find(s, 20, 12, 150, 0, 1);
    CHECK(hit && hit->shrunk);
    CHECK(c.Find(s, 20, 12, 151, 0, 1) == nullptr);
    CHECK(c.Find(s, 20, 12, 150, 0, 2) == nullptr);
    CHECK(c.Size() == 1);
    c.Bound(0);
    CHECK(c.Size() == 0);

    // spaces survive the rule at every size (fix-text 3): a shrunk line keeps its text, spaces included, and its width
    // still counts every space; a cut keeps the spaces before the cut
    const std::wstring line = L"is showing the location";
    std::vector<float> la(line.size(), 10.0f);
    for (size_t i = 0; i < line.size(); ++i)
        if (line[i] == L' ')
            la[i] = 4.0f; // a narrow space (Agency FB's is ~0.4 of a letter)
    for (float size : {14.0f, 20.0f, 52.0f, 95.0f, 140.0f})
    {
        const float full = (19 * 10 + 3 * 4) * size / 20; // 19 letters + 3 spaces at em 20
        r = Fit(line, la, 10, 20, size, size * 0.5f, full * 0.8f, 0);
        CHECK(r.text == line && r.shrunk && !r.cut);
        CHECK(r.width <= full * 0.8f);
        r = Fit(line, la, 10, 20, size, size, full * 0.6f, 0); // floor = size: cut, spaces inside kept
        CHECK(r.cut && r.text.find(L"is showing") == 0);
    }

    // the spoken-word pop grows a word in place without closing its gaps (textfit::GrowCap): at every size two
    // neighbours at full growth leave at least 60 % of the space between them
    for (float px : {30.0f, 52.0f, 114.0f, 180.0f})
    {
        const float space = 0.17f * px, longW = 2.9f * px, shortW = 1.3f * px; // "controls", "your" in Agency FB
        const float topL = textfit::GrowCap(longW, longW, space, 1.08f), topS = textfit::GrowCap(shortW, shortW, space, 1.08f);
        CHECK(topL >= 1.0f && topL <= 1.08f && topS >= 1.0f && topS <= 1.08f);
        const float eaten = (longW * (topL - 1) + shortW * (topS - 1)) * 0.5f;
        CHECK(space - eaten >= space * 0.6f - 0.01f);
        CHECK(topL < 1.08f); // the fixed 1.08 would have eaten 0.116 em of a 0.17 em space from the long word alone
    }
    CHECK_NEAR(textfit::GrowCap(10, 10, 100, 1.08f), 1.08, 1e-5); // a short word with room: the full pop
    CHECK_NEAR(textfit::GrowCap(0, 10, 4, 1.08f), 1.0, 1e-5);     // nothing measured: no growth
    CHECK(textfit::GrowCap(100, 104, 3, 1.08f) < 1.0f);           // the grow font already wider: drawn a hair smaller
}
