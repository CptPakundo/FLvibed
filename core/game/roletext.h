// FLvibed - the game's own text by role (modules\hud\docs\UI_HUD2.md 5.36; flight's principle, text takes the room it
// has). A text control can change font live, and the game's fonts of one face already form a ladder of sizes, so a role picks a
// rung: the numbers that decide a deal climb furthest, labels less, headings and buttons not at all, and only while the text fits.
// Pure: the rules. modules\hud\src\windowtext.cpp reads the game and applies them.
#pragma once
#include <algorithm>
#include <cstdint>
#include <vector>

namespace roletext
{
struct Rung
{
    float height = 0; // a fraction of the screen's height (fixed_height)
    uint32_t font = 0;
};

// Sorted, one rung per height: two fonts of one height and face are the same text.
inline std::vector<Rung> Ladder(std::vector<Rung> fonts)
{
    std::stable_sort(fonts.begin(), fonts.end(), [](const Rung& a, const Rung& b) { return a.height < b.height; });
    std::vector<Rung> out;
    for (const Rung& r : fonts)
        if (out.empty() || r.height > out.back().height + 0.0001f)
            out.push_back(r);
    return out;
}

// The rung of a height (the nearest at or under it); -1 when the ladder holds nothing that low.
inline int RungAt(const std::vector<Rung>& ladder, float height)
{
    int at = -1;
    for (int i = 0; i < static_cast<int>(ladder.size()); ++i)
        if (ladder[i].height <= height + 0.0001f)
            at = i;
    return at;
}

// A two line row of the game's lists: the name above, the second line (a quantity, a class) left of the price under it. Widths in
// pixels at the base fonts, as measured.
struct Row
{
    float name = 0, second = 0, price = 0;
};
struct Room
{
    float width = 0;    // from the name's left edge to the price's right edge, pixels
    float height = 0;   // the row, pixels
    float gap = 0;      // the least between the second line and the price, pixels
    float screenH = 0;  // pixels
    float nameBase = 0, priceBase = 0; // the base fonts' heights
};
// A font's cell holds its leading: the part that reads as text, measured from the frames at 1280x800 (a 0.0435 price under a 0.035
// name sits whole in the 0.068 row).
constexpr float kInk = 0.8f;
// Width carries a margin: a font's width is not exactly proportional to its height (hinting).
constexpr float kWidthMargin = 1.03f;

inline bool RowFits(const Row& r, const Room& room, float nameH, float priceH)
{
    const float nameW = r.name * nameH / room.nameBase * kWidthMargin;
    const float priceW = r.price * priceH / room.priceBase * kWidthMargin;
    if (nameW > room.width && nameH > room.nameBase)
        return false;
    if (r.price > 0 && r.second > 0 && r.second + room.gap + priceW > room.width && priceH > room.priceBase)
        return false;
    return (nameH + priceH) * room.screenH * kInk <= room.height || (nameH <= room.nameBase && priceH <= room.priceBase);
}

struct Pick
{
    int name = 0, price = 0; // rungs
};
// The price first (it decides the deal), then the name in what is left, for every row at once: a column keeps one size. Never over
// `cap` (the rungs of the window's last pick or the role's top), never under the base.
inline Pick PickRows(const std::vector<Row>& rows, const Room& room, const std::vector<Rung>& ladder, Pick base, Pick cap)
{
    const auto fits = [&](int name, int price)
    {
        for (const Row& r : rows)
            if (!RowFits(r, room, ladder[name].height, ladder[price].height))
                return false;
        return true;
    };
    Pick out = base;
    for (int p = cap.price; p > base.price; --p)
        if (fits(base.name, p))
        {
            out.price = p;
            break;
        }
    for (int n = cap.name; n > base.name; --n)
        if (fits(n, out.price))
        {
            out.name = n;
            break;
        }
    return out;
}
} // namespace roletext
