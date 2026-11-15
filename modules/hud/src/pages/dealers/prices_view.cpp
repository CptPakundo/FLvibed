// FLvibed - the BEST PRICES page's view (the HUD 2 page framework, audit 70b F3a; basepage.cpp's DrawPrices moved unchanged):
// the window's rows in the order that answers the question, the highest first, under the window's own column words; on the
// right the two answers named (sell it highest at, buy it cheapest at) and what the difference brings. It lists the bases the
// player has been to. Draws its model (core\game\screens\prices_model.h) and nothing of the game. Declared in prices_page.h.
#include "../page_kit.h"
#include "../page_view.h"
#include "game/screens/prices_model.h"

namespace pricespage
{
using namespace pagekit;
using pricesscreen::kActClose;

void View(const page::ViewIn& in, const void* model, page::PageState&, page::ViewOut& out)
{
    const pricesscreen::Model& m = *static_cast<const pricesscreen::Model*>(model);
    const Frame f = {in.W, in.H, in.x0, in.x1, in.top, in.bottom, in.titleY};
    const float H = f.H, W = f.W, top = m.docked ? in.pageTop : 0;
    GroundBut(0, top, W, H, Holes());
    out.holes = Holes();
    if (!top)
        Title(f, L"BEST PRICES");
    const float x0 = f.left, x1 = 0.66f * W, rx0 = 0.685f * W, rx1 = f.right;
    float y = (top ? top : f.top) + 0.024f * H;
    const std::vector<screens::Quote>& quotes = m.quotes;
    Heading(x0, x1, y, H, m.title, std::to_wstring(quotes.size()) + (quotes.size() == 1 ? L" BASE KNOWN" : L" BASES KNOWN"));
    y += 0.040f * H;
    const float c1 = x0 + (x1 - x0) * 0.46f, c2 = x0 + (x1 - x0) * 0.78f;
    Text(x0, y, m.colBase, 0.024f * H, Argb(kChrome, 1));
    Text(c1, y, m.colSystem, 0.024f * H, Argb(kChrome, 1));
    Text(c2, y, m.colBuySell, 0.024f * H, Argb(kChrome, 1));
    Text(x1 - 0.012f * H, y, m.colPrice, 0.024f * H, Argb(kChrome, 1), Right);
    y += 0.020f * H;
    const float foot = f.bottom, pitch = 0.052f * H;
    screens::List& l = m.list;
    l.rows = std::max(1, static_cast<int>((foot - y) / pitch));
    l.Fit();
    const screens::Picks picks = screens::Pick(quotes);
    for (int i = 0; i < l.rows && l.first + i < l.count; ++i)
    {
        const screens::Quote& q = quotes[l.first + i];
        const float rowTop = y + i * pitch, cy = rowTop + pitch / 2;
        const bool on = l.first + i == l.sel && in.pad;
        if (on)
            skinkit::Focus(x0, rowTop + 0.002f * H, x1, rowTop + pitch - 0.002f * H);
        const u32 ink = l.first + i == picks.sell ? kFriend : q.base == m.here ? kActive : kInk;
        TextMarquee(x0 + 0.012f * H, cy, q.base, 0.030f * H, c1 - x0 - 0.024f * H, Argb(on ? kInk : ink, 1));
        TextMarquee(c1, cy, q.system, 0.028f * H, c2 - c1 - 0.012f * H, Argb(on ? kInk : kChrome, 1));
        Text(c2, cy, q.sells, 0.028f * H, Argb(on ? kInk : q.buyHere ? kInk : kDim, 1));
        Text(x1 - 0.012f * H, cy, Money(q.price), 0.030f * H, Argb(on ? kInk : ink, 1), Right);
    }
    if (l.count > l.rows)
        skinkit::ScrollBar(x1 + 0.006f * H, y, y + l.rows * pitch, static_cast<float>(l.first) / l.count, static_cast<float>(l.rows) / l.count);
    // the answers
    float ry = (top ? top : f.top) + 0.024f * H;
    Heading(rx0, rx1, ry, H, L"BEST DEALS", L"");
    ry += 0.046f * H;
    auto block = [&](const wchar_t* word, int at, u32 rgb)
    {
        Text(rx0, ry, word, 0.026f * H, Argb(kChrome, 1));
        ry += 0.034f * H;
        if (at < 0)
        {
            Text(rx0, ry, L"NO BASE KNOWN YET", 0.028f * H, Argb(kDim, 1));
            ry += 0.052f * H;
            return;
        }
        const screens::Quote& q = quotes[at];
        const std::wstring money = Money(q.price);
        TextMarquee(rx0, ry, q.base, 0.032f * H, rx1 - rx0 - TextWidth(money, 0.034f * H) - 0.012f * H, Argb(kInk, 1));
        Text(rx1, ry, money, 0.034f * H, Argb(rgb, 1), Right);
        ry += 0.030f * H;
        Text(rx0, ry, q.system, 0.024f * H, Argb(kChrome, 1));
        ry += 0.050f * H;
    };
    block(L"SELL IT HIGHEST AT", picks.sell, kFriend);
    block(L"BUY IT CHEAPEST AT", picks.buy, kActive);
    if (picks.sell >= 0 && picks.buy >= 0 && quotes[picks.sell].price > quotes[picks.buy].price)
    {
        const long long each = quotes[picks.sell].price - quotes[picks.buy].price;
        skinkit::Rule(rx0, rx1, ry - 0.012f * H);
        skinkit::Row(rx0, rx1, ry, 0.036f * H, L"Profit each", Money(each));
        ry += 0.036f * H;
        if (m.cargo && m.cap > 0)
            skinkit::Row(rx0, rx1, ry, 0.036f * H, L"Your hold full", Money(each * m.cap));
    }
    if (!in.pad)
    {
        ChipAt(in, out, x0, in.laneY, in.laneH, L"CLOSE", kActClose, 0);
        if (l.count > l.rows)
            Text(0.358f * W, in.laneMid, L"WHEEL SCROLLS", 0.026f * H, Argb(kChrome, 1));
    }
}
} // namespace pricespage
