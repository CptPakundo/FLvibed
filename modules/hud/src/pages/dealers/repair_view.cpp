// FLvibed - the repair page's view (the HUD 2 page framework, audit 70b F3a; basepage.cpp's DrawRepair moved unchanged): the
// damaged items on the left (a tick, the game's icon, the name, the price, the health), the sum and the credits after it
// under them, and the item in focus on the right (its compare block and card; the ship's for the hull). Draws its model
// (core\game\screens\repair_model.h) and nothing of the game: the rows, chips and card are hit boxes, the icons a set, both
// handed on by the runtime. Declared in repair_page.h (a view includes only the draw kits and its model).
#include "../page_kit.h"
#include "../page_view.h"
#include "game/screens/repair_model.h"

namespace repairpage
{
using namespace pagekit;
using namespace repairscreen; // (the actions' ids)

void View(const page::ViewIn& in, const void* model, page::PageState&, page::ViewOut& out)
{
    const repairscreen::Model& mm = *static_cast<const repairscreen::Model*>(model);
    const Frame f = {in.W, in.H, in.x0, in.x1, in.top, in.bottom, in.titleY};
    const float H = f.H, W = f.W, top = in.pageTop;
    GroundBut(0, top, W, H, in.holes);
    out.holes = Holes();
    const float x0 = f.left, x1 = 0.60f * W, rx0 = 0.625f * W, rx1 = f.right;
    float y = top + 0.024f * H;
    const int n = static_cast<int>(mm.rows.size());
    Heading(x0, x1, y, H, L"DAMAGED ITEMS", n ? std::to_wstring(n) + (n == 1 ? L" ITEM" : L" ITEMS") : L"");
    y += 0.036f * H;
    // the sum stands under the rows, the rows take the room there is
    const float foot = f.bottom - 0.116f * H, pitch = n ? std::min(0.104f * H, (foot - y) / n) : 0, cell = std::min(0.066f * H, pitch);
    page::IconSet& icons = out.iconSets[out.nIconSets++];
    icons.format = "RepairSmallIcons%d", icons.base = 0, icons.count = repairscreen::kRows, icons.nth = 1;
    if (!n)
        Text((x0 + x1) / 2, y + 0.10f * H, L"NOTHING ON THIS SHIP NEEDS REPAIR", 0.032f * H, Argb(kDim, 1), Center);
    for (int i = 0; i < n; ++i)
    {
        const repairscreen::Row& m = mm.rows[i];
        const float rowTop = y + i * pitch, box = 0.030f * H, tx = x0 + box + 0.014f * H + cell + 0.012f * H;
        const bool on = i == mm.sel;
        const int hit = HitAt(out, x0, rowTop, x1, rowTop + pitch, kHitRow, i);
        if (on)
            skinkit::Focus(x0, rowTop + 0.002f * H, x1, rowTop + pitch - 0.002f * H);
        else if (hit >= 0 && hit == in.hover)
            Rect(x0, rowTop, x1 - x0, pitch, Argb(kChrome, 0.10f));
        // the tick: a box, filled while the row is in the sum
        const float bx = x0 + 0.008f * H, by = rowTop + (pitch - box) / 2;
        Rect(bx, by, box, box, Argb(kChrome, 0.25f));
        Rect(bx + 0.002f * H, by + 0.002f * H, box - 0.004f * H, box - 0.004f * H, Argb(0x060716, 1));
        if (m.ticked)
            Rect(bx + 0.006f * H, by + 0.006f * H, box - 0.012f * H, box - 0.012f * H, Argb(kFriend, 1));
        const float ix = x0 + box + 0.014f * H;
        if (!m.hull && m.slot >= 0 && m.slot < repairscreen::kRows) // (the hull has no icon of the game's)
        {
            icons.icons[m.slot] = {ix + cell / 2, rowTop + pitch / 2, true};
            out.holes.Add(ix + 1, rowTop + (pitch - cell) / 2, ix + cell - 1, rowTop + (pitch + cell) / 2);
        }
        const std::wstring price = Money(m.price);
        const float pad = 0.012f * H;
        TextMarquee(tx, rowTop + pitch * 0.34f, m.name, 0.030f * H, x1 - tx - pad * 2 - TextWidth(price, 0.030f * H), Argb(kInk, 1));
        Text(x1 - pad, rowTop + pitch * 0.34f, price, 0.030f * H, Argb(on ? kInk : m.ticked ? kActive : kDim, 1), Right);
        if (m.health >= 0)
        {
            wchar_t b[16];
            swprintf_s(b, L"%.0f %%", m.health * 100);
            skinkit::Pips(tx, rowTop + pitch * 0.74f - 0.005f * H, x1 - tx - 0.09f * H, 0.010f * H, m.health, 20, m.health < 0.35f ? kHostile : kAmber);
            Text(x1 - pad, rowTop + pitch * 0.74f, b, 0.024f * H, Argb(on ? kInk : kChrome, 1), Right);
        }
    }
    // the sum and what is left
    float by = std::min(foot, y + n * pitch) + 0.010f * H;
    skinkit::Rule(x0, x1, by);
    by += 0.006f * H;
    skinkit::Row(x0, x1, by, 0.036f * H, L"Total cost", Money(mm.total));
    by += 0.036f * H;
    skinkit::Row(x0, x1, by, 0.036f * H, L"Credits after", Money(mm.credits - mm.total));
    by += 0.040f * H;
    if (!in.pad && n)
    {
        const float cx = ChipAt(in, out, x0, by, 0.048f * H, L"TICK ALL / NONE", kActAll, 0);
        ChipAt(in, out, cx, by, 0.048f * H, L"REPAIR", kActRepair, 0, mm.canMend);
    }
    // the right: what is in focus, what it is and what it can do
    float ry = top + 0.024f * H;
    HitAt(out, rx0, ry, rx1, f.bottom, -1, 0); // (the card's box: the mouse there is on no row)
    if (n && !mm.rows[mm.sel].hull)
    {
        const repairscreen::Row& m = mm.rows[mm.sel];
        TextFit((rx0 + rx1) / 2, ry + 0.012f * H, m.name, 0.038f * H, rx1 - rx0, Argb(kInk, 1), Center);
        ry += 0.040f * H;
        skinkit::Rule(rx0, rx1, ry);
        ry = DrawCompare(rx0, rx1, ry + 0.008f * H, mm.compare, H);
        Card(rx0, rx1, ry, f.bottom, H, mm.cardText, mm.cardFirst, mm.cardLines, mm.cardRoom);
    }
    else
    {
        TextFit((rx0 + rx1) / 2, ry + 0.012f * H, n ? mm.rows[mm.sel].name : mm.shipName, 0.038f * H, rx1 - rx0, Argb(kInk, 1), Center);
        ry += 0.040f * H;
        skinkit::Rule(rx0, rx1, ry);
        ry += 0.008f * H;
        skinkit::Row(rx0, rx1, ry, 0.036f * H, L"Ship", mm.shipName);
        ry += 0.036f * H;
        if (mm.shipOk)
        {
            skinkit::Row(rx0, rx1, ry, 0.036f * H, L"Armor when whole", mm.shipHull); // (docked the hull's share reads nowhere yet: the game's own bar holds none)
            ry += 0.044f * H;
            Card(rx0, rx1, ry, f.bottom, H, mm.cardText, mm.cardFirst, mm.cardLines, mm.cardRoom);
        }
    }
    if (!in.pad)
    {
        ChipAt(in, out, x0, in.laneY, in.laneH, L"CLOSE", kActClose, 0);
        Text(0.358f * W, in.laneMid, L"CLICK A ROW TO TICK IT", 0.026f * H, Argb(kChrome, 1));
    }
}
} // namespace repairpage
