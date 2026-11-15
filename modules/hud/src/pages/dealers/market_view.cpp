// FLvibed - the dealer page's view (the HUD 2 page framework, audit 70b F3b; basepage.cpp's DrawDealer, DrawList, DrawMiddle and
// DrawDeal moved unchanged): the player's list on the left (the equipment dealer's filters over it, the ship's firepower or the
// hold under it), the dealer's on the right, and the item in focus in the middle (the game's own mesh, the trade, the compare
// block or the good's facts, its card); at the final stage the new ship's items left, what goes to the dealer right and the
// deal over the middle. Draws its model (core\game\screens\market_model.h) and nothing of the game: the rows, tabs, chips and
// card are hit boxes, the two lists game lists (their meshes the runtime's), the transfer mesh an icon set. Declared in
// market_page.h (a view includes only the draw kits and its model).
#include "../page_kit.h"
#include "../page_view.h"
#include "game/screens/market_model.h"

namespace marketpage
{
using namespace pagekit;
using namespace marketscreen; // (the actions' ids, the sides)

static const wchar_t* DealWord(int d, bool selling)
{
    switch (d)
    {
    case kDealGood: return selling ? L"GOOD OFFER" : L"GOOD PRICE";
    case kDealFair: return L"FAIR";
    case kDealHigh: return L"HIGH PRICE";
    case kDealLow: return L"LOW OFFER";
    default: return L"";
    }
}
static u32 DealRgb(int d) { return d == kDealGood ? kFriend : d == kDealFair ? kAmber : kHostile; }

static void DrawList(const page::ViewIn& in, page::ViewOut& out, const Model& mm, int side, float x0, float x1, float y, float foot)
{
    const float H = in.H;
    screens::List& l = mm.list[side];
    const std::vector<Row>& rows = mm.rows[side];
    // five rows, as the game's: the game's own icons stand in them
    const float pitch = std::min(0.124f * H, (foot - y) / 5), cell = std::min(0.092f * H, pitch);
    l.rows = 5;
    l.Fit();
    if (out.nLists < page::ViewOut::kLists) // (the runtime gives the shown rows their meshes and holes: page_view.h GameList)
    {
        page::GameList& gl = out.lists[out.nLists++];
        gl.window = side == kYours ? &mm.inv : &mm.dealer;
        gl.partner = side == kYours ? &mm.dealer : &mm.inv;
        gl.right = side == kTheirs;
        gl.list = &l;
        gl.x0 = x0, gl.cell = cell, gl.top = y, gl.pitch = pitch;
        gl.rows.resize(rows.size());
        for (size_t i = 0; i < rows.size(); ++i)
            gl.rows[i] = {rows[i].row, rows[i].arch};
    }
    const float pad = 0.012f * H, tx = x0 + cell + pad;
    // a row's second line: its word on the left, the slot's (or the compare tag) on the right
    auto WordsOf = [&](const Row& r, std::wstring& under, u32& underRgb, std::wstring& slotWord, bool& tagged) {
        tagged = side == kYours && r.hp < 0 && r.mounted && r.arch == mm.tagged;
        slotWord = tagged ? std::wstring(L"TAGGED TO COMPARE") : r.slot; // (Shockwave, 7 Nov: in the class's place, clear of the row's word)
        if (r.hp >= 0)
            return;
        if (side == kYours)
        {
            under = r.mounted ? L"MOUNTED" : r.count > 1 ? L"x" + std::to_wstring(r.count) : L"IN HOLD";
            if (r.mounted)
                underRgb = kFriend; // (green, as the inventory's)
        }
        else
            under = r.slot;
        if (r.noSlot)
            under = L"NO HARDPOINT TAKES IT", underRgb = kAmber;
        if (r.deal != kDealUnknown)
            under = (side == kYours ? L"x" + std::to_wstring(r.count) + L"   " : L"") + DealWord(r.deal, side == kYours), underRgb = DealRgb(r.deal);
    };
    // The durability meter stands beside the row's word, clear of the slot's. One width for the whole list (issue 55: each meter took its own
    // row's gap, so the meters of a list differed): the inventory's 0.13 H, or less when a row's gap is smaller - the text is wider against H on
    // some screens (at 3840x2160 no gun row had 0.13 H). A row with less than 0.03 H shows none and does not narrow the others, as before.
    auto Metered = [&](const Row& r) { return r.mounted && r.health > 0 && r.health < 0.995f; };
    auto MeterRoom = [&](const Row& r) {
        std::wstring under, slotWord;
        u32 rgb = kChrome;
        bool tagged = false;
        WordsOf(r, under, rgb, slotWord, tagged);
        return x1 - pad - (side == kYours ? TextWidth(slotWord, 0.022f * H) + pad : 0) - (tx + TextWidth(under, 0.024f * H) + pad);
    };
    float meterW = 0.13f * H;
    for (const Row& r : rows)
        if (Metered(r))
            if (const float room = MeterRoom(r); room > 0.03f * H)
                meterW = std::min(meterW, room);
    if (rows.empty())
        Text(x0, y + pitch / 2, mm.Final() ? (side == kYours ? L"NOTHING OF THIS KIND" : L"ALL OF IT GOES ON THE NEW SHIP") : side == kYours ? L"NOTHING TO SELL HERE" : L"NOTHING ON SALE HERE", 0.028f * H, Argb(kDim, 1));
    for (int i = 0; i < l.rows && l.first + i < l.count; ++i)
    {
        const int at = l.first + i;
        const Row& r = rows[at];
        const float top = y + i * pitch, y1 = top + pitch * 0.36f, y2 = top + pitch * 0.76f;
        const bool on = at == l.sel && side == mm.side, chosen = at == l.sel;
        const int slot = HitAt(out, x0, top, x1, top + pitch, kHitRow, side * 1000 + at);
        if (on)
            skinkit::Focus(x0 + cell, top + 0.002f * H, x1, top + pitch - 0.002f * H);
        else if (slot >= 0 && slot == in.hover)
            Rect(x0 + cell, top, x1 - x0 - cell, pitch, Argb(kChrome, 0.10f));
        else if (chosen)
            Rect(x0 + cell, top, x1 - x0 - cell, pitch, Argb(kChrome, 0.06f));
        if (slot >= 0 && slot == in.hover)
            mm.wheelSide = static_cast<float>(side);
        std::wstring right, under, slotWord;
        u32 underRgb = kChrome;
        bool tagged = false;
        WordsOf(r, under, underRgb, slotWord, tagged);
        if (r.hp >= 0)
            right = r.slot;
        else
            right = r.price; // (the game's own number on its row of five: a launcher's price, a damaged item's, a sale's; off its five the last one read, issue 93)
        const float rw = TextWidth(right, 0.028f * H);
        TextMarquee(tx, y1, r.name, 0.032f * H, x1 - tx - rw - 2 * pad, Argb(r.hp >= 0 ? kDim : kInk, 1));
        Text(x1 - pad, y1, right, r.hp >= 0 ? 0.024f * H : 0.028f * H, Argb(r.hp >= 0 ? kChrome : on ? kInk : kActive, 1), Right);
        if (!under.empty())
            Text(tx, y2, under, 0.024f * H, Argb(on && underRgb == kChrome ? kInk : underRgb, 0.9f));
        if (r.hp < 0 && !slotWord.empty() && side == kYours)
            Text(x1 - pad, y2, slotWord, 0.022f * H, Argb(on ? kInk : tagged ? kActive : kChrome, tagged ? 1.0f : 0.8f), Right);
        if (r.hp < 0 && !mm.Final())
        {
            if (!r.need.empty())
                Text(x1 - pad, y2, r.need, 0.022f * H, Argb(kAmber, 1), Right);
            else if (!r.mounted && r.forMounted && (side == kTheirs || r.slot.empty()))
                Text(x1 - pad, y2, L"FOR MOUNTED", 0.022f * H, Argb(on ? kInk : kFriend, 1), Right); // (ammo for a launcher you carry)
        }
        if (Metered(r) && MeterRoom(r) >= meterW)
            skinkit::Pips(tx + TextWidth(under, 0.024f * H) + pad, y2 - 0.003f * H, meterW, 0.006f * H, r.health, 10, r.health < 0.35f ? kHostile : kAmber);
    }
    if (l.count > l.rows)
        skinkit::ScrollBar(x1 + 0.006f * H, y, y + l.rows * pitch, static_cast<float>(l.first) / l.count, static_cast<float>(l.rows) / l.count);
}

// The final stage's middle head: the game's price, what the old ship brings, what the rows at the dealer's bring, what is left to pay; BUY SHIP, or the game's own refusal. The y after it.
static float DrawDeal(const page::ViewIn& in, page::ViewOut& out, const Model& mm, float x0, float x1, float y, float H)
{
    const float rowH = 0.034f * H;
    const long long net = mm.shipNet, tradeIn = mm.tradeIn;
    long long sold = 0;
    bool whole = mm.rows[kTheirs].size() <= 5;
    for (const Row& r : mm.rows[kTheirs])
    {
        if (!r.priceRead) // (off the game's five, or its number not read)
            whole = false;
        sold += screens::Number(r.price);
    }
    if (whole)
    {
        skinkit::Row(x0, x1, y, rowH, L"New ship", Money(net + sold));
        y += rowH;
    }
    skinkit::Row(x0, x1, y, rowH, L"Your ship traded in", Money(-tradeIn));
    y += rowH;
    if (whole && sold > 0)
    {
        skinkit::Row(x0, x1, y, rowH, L"Sold to the dealer", Money(-sold));
        y += rowH;
    }
    skinkit::Row(x0, x1, y, rowH, net - tradeIn < 0 ? L"You receive" : L"You pay", Money(std::llabs(net - tradeIn)));
    y += rowH + 0.006f * H;
    if (mm.canBuyFinal)
    {
        const float axis = (x0 + x1) / 2, ch = 0.048f * H, w = TextWidth(L"BUY SHIP", skinrules::kChipWord * ch / skinrules::kChipLow) + 0.03f * H;
        if (mm.buyStop && in.pad)
            skinkit::Focus(axis - w / 2 - 0.01f * H, y - 0.004f * H, axis + w / 2 + 0.01f * H, y + ch + 0.004f * H);
        ChipAt(in, out, axis - w / 2, y, ch, L"BUY SHIP", kActFinalBuy, 0);
        y += ch + 0.010f * H;
    }
    else
    {
        if (!mm.why.empty())
            y = skinkit::Body(x0, y, x1 - x0, mm.why, 2, kAmber) + 0.006f * H;
    }
    skinkit::Rule(x0, x1, y);
    return y + 0.010f * H;
}

static void DrawMiddle(const page::ViewIn& in, page::ViewOut& out, const Model& mm, float x0, float x1, float y, float bottom)
{
    const float H = in.H;
    if (mm.Final())
        y = DrawDeal(in, out, mm, x0, x1, y, H);
    const Row* r = mm.Focus();
    page::IconSet* mesh = out.nIconSets < page::ViewOut::kIconSets ? &out.iconSets[out.nIconSets] : nullptr; // (the game's TransferItemMesh)
    auto placeMesh = [&](bool on, float cx, float cy)
    {
        if (!mesh)
            return;
        mesh->format = "TransferItemMesh", mesh->base = 0, mesh->count = 1, mesh->nth = 1;
        mesh->icons[0] = {cx, cy, on};
        ++out.nIconSets;
    };
    if (!r)
    {
        placeMesh(false, 0, 0);
        Text((x0 + x1) / 2, y + 0.06f * H, in.pad ? L"CHOOSE AN ITEM" : L"CLICK AN ITEM", 0.030f * H, Argb(kDim, 1), Center);
        return;
    }
    if (r->hp >= 0)
    {
        Text(x0, y + 0.02f * H, L"EMPTY HARDPOINT", 0.036f * H, Argb(kInk, 1));
        Text(x0, y + 0.066f * H, r->slot, 0.028f * H, Argb(kActive, 1));
        float cy = y + 0.12f * H;
        Text(x0, cy, mm.fits.empty() ? L"NOTHING IN YOUR HOLD FITS IT" : L"IN YOUR HOLD, IT TAKES", 0.026f * H, Argb(kChrome, 1));
        cy += 0.036f * H;
        for (size_t i = 0; i < mm.fits.size() && cy < bottom; ++i, cy += 0.036f * H)
            TextMarquee(x0, cy, mm.fits[i], 0.028f * H, x1 - x0, Argb(kInk, 1));
        return;
    }
    // the game's own mesh of the selected item, at the column's head
    const float big = 0.104f * H, axis = (x0 + x1) / 2;
    if (mm.transferMesh && mm.canTrade)
    {
        placeMesh(true, axis, y + big / 2);
        out.holes.Add(axis - big / 2, y, axis + big / 2, y + big);
        y += big + 0.004f * H;
    }
    else if (mm.transferMesh)
        placeMesh(false, 0, 0);
    TextFit(axis, y + 0.02f * H, r->name, 0.038f * H, x1 - x0, Argb(kInk, 1), Center);
    y += 0.050f * H;
    // the trade: the game's own numbers, or its own refusal
    const bool selling = mm.side == kYours;
    if (mm.canTrade)
    {
        const std::wstring& price = mm.priceText;
        const std::wstring& total = mm.totalText;
        const int qty = mm.qty, most = mm.most;
        wchar_t b[64];
        const bool s = mm.slider;
        if (s)
            swprintf_s(b, L"%d OF %d", qty, most);
        else
            swprintf_s(b, L"1");
        const float cx = x0, cw = x1 - x0;
        // one line on the column's axis: the deal, then what it comes to
        const std::wstring word = mm.Final() ? (selling ? L"SELL TO DEALER" : L"TAKE BACK") : selling ? L"SELL" : L"BUY", sum = total.empty() ? price : total;
        const float gap = 0.024f * H, ww = TextWidth(word, 0.026f * H), qw = TextWidth(b, 0.030f * H), sw = TextWidth(sum, 0.034f * H);
        float lx = axis - (ww + qw + sw + 2 * gap) / 2;
        Text(lx, y + 0.016f * H, word, 0.026f * H, Argb(kChrome, 1));
        lx += ww + gap;
        Text(lx, y + 0.016f * H, b, 0.030f * H, Argb(kInk, 1));
        lx += qw + gap;
        Text(lx, y + 0.016f * H, sum, 0.034f * H, Argb(selling ? kFriend : kActive, 1));
        y += 0.040f * H;
        if (s && most > 0)
        {
            skinkit::Pips(cx, y, cw, 0.010f * H, static_cast<float>(qty) / most, 24, kActive);
            y += 0.020f * H;
        }
        if (!in.pad)
        {
            const float ch = 0.044f * H, pad = 0.03f * H + 0.012f * H, cs = skinrules::kChipWord * ch / skinrules::kChipLow;
            float rowW = TextWidth(word, cs) + pad;
            if (s)
                rowW += TextWidth(L"-", cs) + TextWidth(L"+", cs) + TextWidth(L"MAX", cs) + 3 * pad;
            if (mm.Mountable())
                rowW += TextWidth(r->mounted ? L"UNMOUNT" : L"MOUNT", cs) + pad;
            float bx = ChipAt(in, out, axis - rowW / 2, y, ch, word, kHitTrade, 0);
            if (s)
            {
                bx = ChipAt(in, out, bx, y, ch, L"-", kActLess, 0);
                bx = ChipAt(in, out, bx, y, ch, L"+", kActMore, 0);
                bx = ChipAt(in, out, bx, y, ch, L"MAX", kActMax, 0);
            }
            if (mm.Mountable())
                ChipAt(in, out, bx, y, ch, r->mounted ? L"UNMOUNT" : L"MOUNT", kHitMount, 0);
            y += ch + 0.012f * H;
        }
    }
    else
    {
        if (!mm.errorHelp.empty() && mm.selected)
            y = skinkit::Body(x0, y, x1 - x0, mm.errorHelp, 2, kAmber) + 0.008f * H;
        // the need against the player's own value, under the game's refusal: that names the need at most (or another reason, a full ship), never the player's own value (issue 56)
        if (!mm.need.empty())
            y = skinkit::Body(x0, y, x1 - x0, mm.need + L" \xB7 " + mm.yours, 2, kAmber) + 0.008f * H;
        if (!in.pad && mm.Mountable())
        {
            ChipAt(in, out, x0, y, 0.044f * H, r->mounted ? L"UNMOUNT" : L"MOUNT", kHitMount, 0);
            y += 0.056f * H;
        }
    }
    // the compare tag (issue 44), on either card: the mouse's chip; the pad's click in words (the lane is full at 1280 and drops it - ITEM, side, SELL,
    // FILTER, UNMOUNT)
    if (mm.canTag && !in.pad)
    {
        ChipAt(in, out, x0, y, 0.044f * H, mm.tagOn ? L"UNTAG" : L"COMPARE THE STORE WITH THIS", kHitTag, 0);
        y += 0.056f * H;
    }
    else if (mm.canTag)
    {
        const float gh = 0.028f * H, gw = GlyphWidth(glyphs::R3, gh);
        Glyph(glyphs::R3, x0 + gw / 2, y + 0.018f * H, gh, Argb(kInk, 1));
        Text(x0 + gw + 0.010f * H, y + 0.018f * H, mm.tagOn ? L"CLICK: UNTAG" : L"CLICK: COMPARE THE STORE WITH THIS", 0.024f * H, Argb(mm.tagOn ? kActive : kChrome, 1));
        y += 0.046f * H;
    }
    skinkit::Rule(x0, x1, y);
    y += 0.008f * H;
    if (mm.compare)
        y = DrawCompare(x0, x1, y, mm.block, H);
    else
    {
        wchar_t b[64];
        if (mm.volume >= 0)
        {
            swprintf_s(b, L"%.0f", mm.volume);
            skinkit::Row(x0, x1, y, 0.036f * H, L"Cargo space each", b);
            y += 0.036f * H;
        }
        if (mm.basePrice >= 0)
        {
            skinkit::Row(x0, x1, y, 0.036f * H, L"Usual price", Money(static_cast<long long>(mm.basePrice + 0.5f)));
            y += 0.036f * H;
        }
        for (const Row& mine : mm.rows[kYours])
            if (mine.arch == r->arch && !selling)
            {
                skinkit::Row(x0, x1, y, 0.036f * H, L"In your hold", std::to_wstring(mine.count));
                y += 0.036f * H;
            }
        y += 0.008f * H;
    }
    const float cardTop = y;
    const int slot = HitAt(out, x0, cardTop, x1, bottom, -1, 0); // (the card's box: the mouse there is on no row)
    if (slot >= 0 && slot == in.hover)
        mm.wheelSide = -1;
    Card(x0, x1, cardTop, bottom, H, mm.cardText, mm.cardFirst, mm.cardLines, mm.cardRoom);
}

void View(const page::ViewIn& in, const void* model, page::PageState&, page::ViewOut& out)
{
    const Model& mm = *static_cast<const Model*>(model);
    const Frame f = {in.W, in.H, in.x0, in.x1, in.top, in.bottom, in.titleY};
    const float H = f.H, W = f.W;
    // the strip of the base and the room buttons stand: the player goes on from here as from the game's window
    const float top = in.pageTop;
    GroundBut(0, top, W, H, in.holes);
    const float ly0 = f.left, ly1 = 0.335f * W, cx0 = 0.358f * W, cx1 = 0.642f * W, ry0 = 0.665f * W, ry1 = f.right;
    float y = top + 0.024f * H;
    std::wstring tag;
    if (mm.cargoRead && mm.cap > 0)
        tag = L"CARGO " + std::to_wstring(mm.used) + L" / " + std::to_wstring(mm.cap);
    if (mm.Final())
    {
        Heading(ly0, ly1, y, H, L"ON THE NEW SHIP", L"");
        Heading(ry0, ry1, y, H, L"GOES TO THE DEALER", mm.list[kTheirs].count ? std::to_wstring(mm.list[kTheirs].count) + L" ITEMS" : L"");
    }
    else
    {
        Heading(ly0, ly1, y, H, L"YOUR SHIP", tag);
        Heading(ry0, ry1, y, H, L"DEALER", std::to_wstring(mm.list[kTheirs].count) + L" ON SALE");
    }
    y += 0.036f * H;
    float ty = y;
    if (mm.mode == kEquipment || mm.Final())
    {
        // the game's filters, in its own words
        float tx = ly0;
        const float size = 0.026f * H, tabH = 0.040f * H;
        for (int i = 0; i < mm.Filters(); ++i)
        {
            const float w = TextWidth(mm.filterWord[i], size) + 0.020f * H;
            const navlayout::Rect r{tx, y, tx + w, y + tabH};
            const int slot = HitAt(out, r.x0, r.y0, r.x1, r.y1, kHitTab, i);
            skinchip::Tab(r, mm.filterWord[i], size, i == mm.filterNow, slot >= 0 && slot == in.hover);
            tx += w + 0.004f * H;
        }
        ty = y + tabH + 0.006f * H;
    }
    mm.wheelSide = -2;
    // the left column's foot: what the ship does now, under its list
    float foot = f.bottom;
    if (mm.mode == kEquipment)
    {
        foot -= 0.076f * H;
        skinkit::Rule(ly0, ly1, foot + 0.006f * H);
        wchar_t b[96];
        swprintf_s(b, L"%d GUNS", mm.guns);
        Text(ly0, foot + 0.026f * H, L"FIREPOWER", 0.024f * H, Argb(kChrome, 1));
        Text(ly1, foot + 0.026f * H, b, 0.024f * H, Argb(kInk, 1), Right);
        swprintf_s(b, L"HULL %.0f/S   SHIELD %.0f/S   ENERGY %.0f/S", mm.hull, mm.shield, mm.energy);
        TextFit((ly0 + ly1) / 2, foot + 0.056f * H, b, 0.024f * H, ly1 - ly0, Argb(kInk, 1), Center);
    }
    else if (mm.cap > 0)
    {
        foot -= 0.050f * H;
        skinkit::Rule(ly0, ly1, foot + 0.006f * H);
        Text(ly0, foot + 0.030f * H, L"HOLD", 0.024f * H, Argb(kChrome, 1));
        skinkit::Pips(ly0 + 0.07f * H, foot + 0.024f * H, ly1 - ly0 - 0.07f * H, 0.012f * H, static_cast<float>(mm.used) / mm.cap, 20, mm.used >= mm.cap ? kWarn : kChrome);
    }
    DrawList(in, out, mm, kYours, ly0, ly1, ty, foot);
    DrawList(in, out, mm, kTheirs, ry0, ry1, y, f.bottom);
    DrawMiddle(in, out, mm, cx0, cx1, top + 0.024f * H, f.bottom);
    if (!in.pad)
    {
        float cx = ChipAt(in, out, ly0, in.laneY, in.laneH, L"CLOSE", kActClose, 0);
        if (mm.Final() && mm.back)
            cx = ChipAt(in, out, cx, in.laneY, in.laneH, L"BACK TO SHIPS", kActFinalBack, 0);
        if (mm.mode == kCommodity && mm.routes)
            cx = ChipAt(in, out, cx, in.laneY, in.laneH, L"BEST PRICES", kActRoutes, 0);
        Text(std::max(0.358f * W, cx), in.laneMid, L"CLICK AN ITEM, WHEEL SCROLLS", 0.026f * H, Argb(kChrome, 1));
    }
}
} // namespace marketpage
