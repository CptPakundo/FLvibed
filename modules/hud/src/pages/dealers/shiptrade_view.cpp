// FLvibed - the ship dealer page's view (the HUD 2 page framework, audit 70b F3c; basepage.cpp's DrawShips moved unchanged): the
// ships on sale on the left (the game's icon, the name, the price, the class, a refusal), the deal under them (your ship, its
// trade-in, what the chosen one costs you, the credits after, its need), the game's 3D ship in the middle through a hole with
// the chosen one's name and card under it, and its numbers against your ship on the right. Draws its model
// (core\game\screens\shiptrade_model.h) and nothing of the game: the rows and chips are hit boxes, the icons a set, both handed
// on by the runtime. Declared in shiptrade_page.h (a view includes only the draw kits and its model).
#include "../page_kit.h"
#include "../page_view.h"
#include "game/screens/shiptrade_model.h"

namespace shiptradepage
{
using namespace pagekit;
using namespace shiptradescreen; // (the actions' ids, the ship's hole)

// One number of the chosen ship against the player's: its label, its value, the difference coloured by its verdict.
static float StatRow(float x0, float x1, float y, float H, const Stat& s, bool show)
{
    const float pitch = 0.036f * H, c1 = x0 + (x1 - x0) * 0.62f;
    Text(x0, y + pitch / 2, s.label, 0.025f * H, Argb(kChrome, 1));
    Text(c1, y + pitch / 2, s.mine, 0.028f * H, Argb(kInk, 1), Right);
    if (show)
        Text(x1, y + pitch / 2, s.diff, 0.026f * H, Argb(s.verdict == kBetter ? kFriend : s.verdict == kWorse ? kHostile : kChrome, 1), Right);
    return y + pitch;
}

void View(const page::ViewIn& in, const void* model, page::PageState&, page::ViewOut& out)
{
    const shiptradescreen::Model& m = *static_cast<const shiptradescreen::Model*>(model);
    const Frame f = {in.W, in.H, in.x0, in.x1, in.top, in.bottom, in.titleY};
    const float H = f.H, W = f.W, top = in.pageTop, view = 0.165f * H; // (view: the game's 3D ship stands where the game has it)
    const float ly0 = f.left, ly1 = 0.30f * W, cx0 = 0.325f * W, cx1 = 0.675f * W, ry0 = 0.70f * W, ry1 = f.right;
    // the view stands where the game has it, in the screen's middle
    out.holes = Holes();
    out.holes.Add(W / 2 - kShipHoleHalfW * H, kShipHoleTop * H, W / 2 + kShipHoleHalfW * H, kShipHoleFoot * H);
    GroundBut(0, top, W, H, in.holes);
    float y = top + 0.024f * H;
    Heading(ly0, ly1, y, H, L"SHIPS HERE", std::to_wstring(m.offers.size()) + L" ON SALE");
    y += 0.036f * H;
    const float pitch = 0.100f * H, cell = 0.092f * H;
    // the pad on the ship's side: its view framed as the focus, the stick's glyph and TURN under it (the owner: say plainly the stick turns the ship now)
    const bool shipSide = m.focus == 1 && m.devicePad && m.sel >= 0;
    if (shipSide)
    {
        const float hx0 = W / 2 - kShipHoleHalfW * H, hy0 = kShipHoleTop * H, hx1 = W / 2 + kShipHoleHalfW * H, hy1 = kShipHoleFoot * H, gh = 0.030f * H;
        const float e = std::max(1.0f, 0.003f * H); // (a whole rectangle in the focus's yellow: the list's style left it three lines - the owner)
        Rect(hx0, hy0, hx1 - hx0, e, Argb(kActive, 0.85f));
        Rect(hx0, hy1 - e, hx1 - hx0, e, Argb(kActive, 0.85f));
        Rect(hx0, hy0, e, hy1 - hy0, Argb(kActive, 0.85f));
        Rect(hx1 - e, hy0, e, hy1 - hy0, Argb(kActive, 0.85f));
        const std::wstring word = L"TURN";
        const float tw = TextWidth(word, 0.026f * H), gw = GlyphWidth(glyphs::RS, gh), x = W / 2 - (gw + 0.008f * H + tw) / 2, ty = hy1 - 0.026f * H;
        Glyph(glyphs::RS, x + gw / 2, ty, gh, Argb(kInk, 1));
        Text(x + gw + 0.008f * H, ty, word, 0.026f * H, Argb(kInk, 1));
    }
    page::IconSet& icons = out.iconSets[out.nIconSets++];
    icons.format = "ShipTraderShipMesh%d", icons.base = 0, icons.count = 3, icons.nth = 1;
    for (size_t i = 0; i < m.offers.size(); ++i, y += pitch)
    {
        const Offer& o = m.offers[i];
        const bool on = static_cast<int>(i) == m.sel;
        const int hit = HitAt(out, ly0, y, ly1, y + pitch, kHitRow, static_cast<int>(i));
        if (i < 3)
            icons.icons[i] = {ly0 + cell / 2, y + pitch / 2, true};
        out.holes.Add(ly0 + 1, y + (pitch - cell) / 2, ly0 + cell - 1, y + (pitch + cell) / 2);
        if (on && !shipSide)
            skinkit::Focus(ly0 + cell, y + 0.002f * H, ly1, y + pitch - 0.002f * H);
        else if (on)
            Rect(ly0 + cell, y, ly1 - ly0 - cell, pitch, Argb(kChrome, 0.06f)); // (chosen: the focus is on the ship)
        else if (hit >= 0 && hit == in.hover)
            Rect(ly0 + cell, y, ly1 - ly0 - cell, pitch, Argb(kChrome, 0.10f));
        const float pad = 0.012f * H, tx = ly0 + cell + pad;
        const std::wstring price = Money(o.price);
        TextMarquee(tx, y + pitch * 0.32f, o.name, 0.034f * H, ly1 - tx - 2 * pad - TextWidth(price, 0.030f * H), Argb(kInk, 1));
        Text(ly1 - pad, y + pitch * 0.32f, price, 0.030f * H, Argb(on ? kInk : kActive, 1), Right);
        Text(tx, y + pitch * 0.72f, o.kind, 0.024f * H, Argb(on ? kInk : kChrome, 0.9f));
        if (o.refusal)
        {
            wchar_t b[32];
            if (o.refusal == 1)
                swprintf_s(b, L"NEEDS LEVEL %d", o.level);
            else
                swprintf_s(b, o.refusal == 2 ? L"REFUSED" : L"TOO DEAR");
            Text(ly1 - pad, y + pitch * 0.72f, b, 0.024f * H, Argb(on ? kInk : kAmber, 1), Right);
        }
    }
    // the deal
    y += 0.014f * H;
    skinkit::Rule(ly0, ly1, y);
    y += 0.008f * H;
    const long long tradeIn = m.budget - m.credits;
    const float rowH = 0.038f * H;
    skinkit::Row(ly0, ly1, y, rowH, L"Your ship", m.own);
    y += rowH;
    skinkit::Row(ly0, ly1, y, rowH, L"Trade-in, with its equipment", Money(tradeIn));
    y += rowH;
    const Offer* chosen = m.Chosen();
    if (chosen)
    {
        const long long net = chosen->price - tradeIn;
        skinkit::Row(ly0, ly1, y, rowH, L"Costs you", Money(net));
        y += rowH;
        skinkit::Row(ly0, ly1, y, rowH, L"Credits after", Money(m.credits - net));
        y += rowH;
        if (!m.need.empty())
            y = skinkit::Body(ly0, y + 0.008f * H, ly1 - ly0, m.need, 2, kAmber);
        if (!in.pad)
            ChipAt(in, out, ly0, y + 0.012f * H, 0.048f * H, L"BUY THIS SHIP", kActBuy, 0, m.canBuy);
    }
    // the middle: the ship and its card
    if (chosen)
    {
        Text((cx0 + cx1) / 2, top + 0.040f * H, chosen->name, 0.044f * H, Argb(kInk, 1), Center);
        Text((cx0 + cx1) / 2, top + 0.082f * H, chosen->kind, 0.028f * H, Argb(kChrome, 1), Center);
        const float cardTop = view + 0.47f * H, size = 0.025f * H, cp = 0.031f * H;
        const std::vector<std::wstring> lines = Wrap(m.cardText, size, cx1 - cx0 - 0.016f * H);
        m.cardLines = static_cast<int>(lines.size());
        m.cardRoom = std::max(1, static_cast<int>((f.bottom - cardTop) / cp));
        m.cardFirst = std::max(0, std::min(m.cardFirst, m.cardLines - m.cardRoom));
        for (int i = 0; i < m.cardRoom && m.cardFirst + i < m.cardLines; ++i)
            Text(cx0, cardTop + cp * (i + 0.5f), lines[m.cardFirst + i], size, Argb(kInk, 0.9f));
        if (m.cardLines > m.cardRoom)
            skinkit::ScrollBar(cx1 - 0.004f * H, cardTop, f.bottom, static_cast<float>(m.cardFirst) / m.cardLines, static_cast<float>(m.cardRoom) / m.cardLines);
        // the numbers against the player's
        float ry = top + 0.024f * H;
        Heading(ry0, ry1, ry, H, L"AGAINST YOUR SHIP", m.own);
        ry += 0.040f * H;
        for (int i = 0; i < m.nStats; ++i)
            ry = StatRow(ry0, ry1, ry, H, m.stats[i], m.compare);
    }
    else
        Text((cx0 + cx1) / 2, view + 0.27f * H, in.pad ? L"CHOOSE A SHIP" : L"CLICK A SHIP", 0.032f * H, Argb(kDim, 1), Center);
    if (!in.pad)
        ChipAt(in, out, ly0, in.laneY, in.laneH, L"CLOSE", kActClose, 0);
}
} // namespace shiptradepage
