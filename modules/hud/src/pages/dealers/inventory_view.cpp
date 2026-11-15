// FLvibed - the INVENTORY page's view (the HUD 2 page framework, audit 70b F4 unit b2; nnpage.cpp's DrawInventory and
// DrawJettison moved unchanged): the window's five filters over five rows (the game's own meshes in them), the ship in the
// middle (its 3D view in space, its facts docked), its firepower and the focus's compare block, the purse and the hold under
// the title, the item's card at the right (the game's, through the hole the shell cut); while the game's jettison panel
// stands, the panel as the page's: the item with its mesh, the count on a bar. Draws its model (core\game\screens\
// inventory_model.h) and what its shell hands in (the card's hole, the number field's box) and nothing of the game: the
// tabs, rows, chips and bar cells are hit boxes, the rows a game list sent under the UI pass (GameList::atSend). Declared
// in inventory_page.h.
#include "../page_kit.h"
#include "../page_view.h"
#include "game/screens/inventory_model.h"

namespace inventorypage
{
using namespace pagekit;
using namespace inventoryscreen; // (the actions' ids)

// The mouse's line in the lane: what its clicks and wheel do.
static void Mouse(const page::ViewIn& in, const Frame& f, const wchar_t* line)
{
    if (!in.pad)
        Text(f.left, in.laneMid, line, 0.026f * f.H, Argb(kChrome, 1));
}

// The jettison panel as the page's: the item with its mesh, the game's own count on a bar of the page's. The game's slider and buttons stand off the screen for as
// long as it stands (the shell's) and go on answering by name; its number field comes into the page, it can still be typed in.
static void DrawJettison(const page::ViewIn& in, const Model& m, const Frame& f, page::ViewOut& out)
{
    const Kept& k = *m.k;
    const float H = f.H, W = f.W, axis = W / 2;
    const int qty = m.qty, most = m.most;
    float y = f.top + 0.09f * H;
    // (the game's window stands behind the page here, so what shows through a hole stands below it: the item's mesh, and for the keyboard the game's number field)
    const float cell = 0.092f * H, shown = 0.715f * H;
    Holes h;
    const Item* it = k.list.sel < static_cast<int>(k.items.size()) ? &k.items[k.list.sel] : nullptr;
    int mesh = -1;
    if (it && out.nLists < page::ViewOut::kLists)
    {
        // (the item's mesh placed and its model sent under the UI pass; the hole when the last pass gave it one)
        const page::ViewIn::Sent& sent = in.sent[out.nLists];
        page::GameList& gl = out.lists[out.nLists++];
        gl.window = &m.root;
        gl.atSend = true;
        gl.fixed = 1;
        gl.fixedRows[0] = {{it->row, it->arch}, axis, shown + cell / 2};
        gl.rows.resize(k.items.size());
        for (size_t i = 0; i < k.items.size(); ++i)
            gl.rows[i] = {k.items[i].row, k.items[i].arch};
        if (sent.first == 0 && sent.n > 0)
            mesh = sent.meshOf[0];
    }
    if (mesh >= 0)
        h.Add(axis - cell / 2, shown, axis + cell / 2, shown + cell);
    if (!in.pad && in.textOk) // (the number field, placed by the shell)
        h.Add(in.text[0], in.text[1], in.text[2], in.text[3]);
    GroundBut(0, 0, W, H, h);
    Title(f, L"JETTISON");
    if (it)
        TextFit(axis, y, it->name, 0.044f * H, 0.6f * W, Argb(kInk, 1), Center);
    Text(axis, y + 0.044f * H, L"HOW MANY GO OVERBOARD", 0.026f * H, Argb(kChrome, 1), Center);
    y += 0.104f * H;
    Text(axis, y, std::to_wstring(qty), 0.066f * H, Argb(kActive, 1), Center);
    y += 0.056f * H;
    // the bar: the mouse puts the quantity where it clicks
    const float bw = 0.42f * W, bx = axis - bw / 2, bh = 0.022f * H;
    Rect(bx, y, bw, bh, Argb(kChrome, 0.18f));
    if (most > 0)
        Rect(bx, y, bw * std::min(1.0f, static_cast<float>(qty) / most), bh, Argb(kActive, 1));
    for (int i = 1; i < 10; ++i)
        Rect(bx + bw * i / 10, y, std::max(1.0f, 0.001f * H), bh, Argb(0x060716, 0.8f));
    for (int i = 0; i < kJetCells && !in.pad; ++i)
        HitAt(out, bx + bw * i / kJetCells, y - 0.012f * H, bx + bw * (i + 1) / kJetCells, y + bh + 0.012f * H, kActBar, i);
    Text(bx, y + bh + 0.022f * H, L"1", 0.026f * H, Argb(kChrome, 1));
    Text(bx + bw, y + bh + 0.022f * H, std::to_wstring(most), 0.026f * H, Argb(kChrome, 1), Right);
    y += bh + 0.052f * H;
    if (!in.pad)
    {
        const float ch = 0.052f * H, cs = skinrules::kChipWord * ch / skinrules::kChipLow, gap = 0.03f * H + 0.012f * H;
        const float rowW = TextWidth(L"-", cs) + TextWidth(L"+", cs) + TextWidth(L"MAX", cs) + TextWidth(L"JETTISON", cs) + TextWidth(L"CANCEL", cs) + 5 * gap - 0.012f * H;
        float cx = axis - rowW / 2;
        cx = ChipAt(in, out, cx, y, ch, L"-", kHitJetLess, 0);
        cx = ChipAt(in, out, cx, y, ch, L"+", kHitJetMore, 0);
        cx = ChipAt(in, out, cx, y, ch, L"MAX", kHitJetMax, 0);
        cx = ChipAt(in, out, cx, y, ch, L"JETTISON", kActAccept, 0);
        ChipAt(in, out, cx, y, ch, L"CANCEL", kActCancel, 0);
        Mouse(in, f, L"WHEEL, BAR OR CHIPS SET THE NUMBER");
    }
}

void View(const page::ViewIn& in, const void* model, page::PageState&, page::ViewOut& out)
{
    const Model& m = *static_cast<const Model*>(model);
    Kept& k = *m.k; // (the list's room, the cells and the ship view's place are the view's)
    screens::List& list = k.list;
    const std::vector<Item>& items = k.items;
    const Frame f = {in.W, in.H, in.x0, in.x1, in.top, in.bottom, in.titleY};
    const float H = f.H, W = f.W;
    out.holes = in.holes; // (the runtime's holes go on as they were but on a frame of the list: its cells)
    if (!m.visible)
        return;
    if (m.jettison)
    {
        DrawJettison(in, m, f, out);
        return;
    }
    Box hole;
    if (in.textOk)
        hole.AddRect(in.text[0], in.text[1], in.text[2], in.text[3]);
    // (an empty hardpoint has no card of the game's: what stood there was the last row's)
    const bool empty = m.empty;
    {
        Holes h;
        for (int i = 0; i < k.nCells; ++i)
            h.Add(k.cells[i][0], k.cells[i][1], k.cells[i][2], k.cells[i][3]);
        if (!empty && in.textReady && hole.ok)
            h.Add(hole.x0, hole.y0, hole.x1, hole.y1);
        GroundBut(0, 0, W, H, h);
    }
    if (empty && hole.ok)
    {
        const equiprules::Hp& hp = k.free[items[list.sel].hp];
        const float cx = hole.x0 + 0.02f * H, cw = hole.x1 - hole.x0 - 0.04f * H;
        float cy = hole.y0 + 0.03f * H;
        Text(cx, cy, L"EMPTY HARDPOINT", 0.034f * H, Argb(kInk, 1));
        cy += 0.046f * H;
        Text(cx, cy, equiprules::SlotWord(equiprules::BestOf(hp)), 0.028f * H, Argb(kActive, 1));
        cy += 0.040f * H;
        skinkit::Rule(cx, cx + cw, cy);
        cy += 0.030f * H;
        Text(cx, cy, m.fits.empty() ? L"NOTHING IN YOUR HOLD FITS IT" : L"IN YOUR HOLD, IT TAKES", 0.026f * H, Argb(kChrome, 1));
        cy += 0.036f * H;
        for (size_t i = 0; i < m.fits.size() && cy < hole.y1 - 0.03f * H; ++i, cy += 0.036f * H)
            TextMarquee(cx, cy, m.fits[i], 0.028f * H, cw, Argb(kInk, 1));
    }
    Title(f, m.ship.empty() ? L"INVENTORY" : m.ship);
    const float x0 = f.left, x1 = 0.405f * W;
    // the filters: the game's five, in its words
    const int filter = m.filter;
    float tx = x0, ty = f.top;
    const float size = 0.030f * H, tabH = 0.046f * H;
    for (int i = 0; i < 5; ++i)
    {
        const float w = TextWidth(k.filterWord[i], size) + 0.024f * H;
        const navlayout::Rect r{tx, ty, tx + w, ty + tabH};
        const int slot = HitAt(out, r.x0, r.y0, r.x1, r.y1, kActFilter, i);
        skinchip::Tab(r, k.filterWord[i], size, i == filter, slot >= 0 && slot == in.hover);
        tx += w + 0.008f * H;
    }
    // five rows, as the game's: its own icons stand in them, its list scrolled with the page's
    const float y = ty + tabH + 0.012f * H, foot = f.bottom, pitch = std::min(0.126f * H, (foot - y) / 5), cell = std::min(0.092f * H, pitch);
    list.rows = 5;
    list.Fit();
    Holes cells;
    const int n = std::min(list.rows, std::max(0, list.count - list.first));
    // the icons: the under-pass places the meshes and sends their models from the list as it stands then (the frame of a step); a row gets a cell when that
    // pass gave it its own model this frame
    const int* meshOf = nullptr;
    if (out.nLists < page::ViewOut::kLists)
    {
        const page::ViewIn::Sent& sent = in.sent[out.nLists];
        if (sent.window == m.root && sent.first == list.first && sent.n > 0)
            meshOf = sent.meshOf;
        page::GameList& gl = out.lists[out.nLists++];
        gl.window = &m.root;
        gl.atSend = true;
        gl.list = &list;
        gl.x0 = x0, gl.cell = cell, gl.top = y, gl.pitch = pitch;
        gl.rows.resize(items.size());
        for (size_t i = 0; i < items.size(); ++i)
            gl.rows[i] = {items[i].row, items[i].arch};
    }
    if (items.empty())
        Text(x0, y + pitch / 2, L"NOTHING HERE", 0.030f * H, Argb(kDim, 1));
    for (int i = 0; i < n; ++i)
    {
        const int row = list.first + i;
        const Item& it = items[row];
        const float top = y + i * pitch, cy = top + pitch * 0.36f;
        const int slot = HitAt(out, x0, top, x1, top + pitch, kHitRow, row);
        if (meshOf && meshOf[i] >= 0)
            cells.Add(x0 + 1, top + (pitch - cell) / 2, x0 + cell - 1, top + (pitch + cell) / 2);
        if (row == list.sel)
            skinkit::Focus(x0 + cell, top + 0.002f * H, x1, top + pitch - 0.002f * H);
        else if (slot >= 0 && slot == in.hover)
            Rect(x0 + cell, top, x1 - x0 - cell, pitch, Argb(kChrome, 0.10f));
        const bool tagged = it.hp < 0 && it.mounted && it.arch == m.taggedArch;
        const std::wstring right = it.hp >= 0 ? equiprules::SlotWord(equiprules::BestOf(k.free[it.hp])) : tagged ? L"MOUNTED, TAGGED" : it.mounted ? L"MOUNTED" : (it.count > 1 ? L"x" + std::to_wstring(it.count) : L"IN HOLD");
        const float rw = TextWidth(right, 0.026f * H);
        const float tx2 = x0 + cell + 0.012f * H;
        TextMarquee(tx2, cy, it.name, 0.032f * H, x1 - tx2 - rw - 0.05f * H, Argb(it.hp >= 0 ? kDim : kInk, 1));
        Text(x1 - 0.012f * H, cy, right, 0.026f * H, Argb(row == list.sel ? kInk : tagged ? kActive : it.mounted ? kFriend : kChrome, 1), Right);
        const std::wstring& line = row < static_cast<int>(m.gunLine.size()) ? m.gunLine[row] : std::wstring();
        if (!line.empty())
            TextMarquee(tx2, top + pitch * 0.76f, line, 0.024f * H, x1 - tx2 - 0.012f * H, Argb(row == list.sel ? kInk : kChrome, 0.9f));
        if (it.health > 0 && it.health < 0.995f && it.mounted)
            skinkit::Pips(x1 - 0.14f * H, top + pitch * 0.76f, 0.13f * H, 0.006f * H, it.health, 10, it.health < 0.35f ? kHostile : kAmber);
    }
    if (list.count > list.rows)
        skinkit::ScrollBar(x1 + 0.006f * H, y, y + list.rows * pitch, static_cast<float>(list.first) / list.count, static_cast<float>(list.rows) / list.count);
    k.nCells = cells.n < screens::kMeshes ? cells.n : screens::kMeshes;
    for (int i = 0; i < k.nCells; ++i)
        for (int c = 0; c < 4; ++c)
            k.cells[i][c] = cells.r[i][c];
    out.holes = cells;
    // the middle: the ship, what it carries and what it can do
    const float mx0 = 0.425f * W, mx1 = (hole.ok ? hole.x0 : f.right) - 0.03f * H;
    const bool inSpace = m.inSpace;
    float my = y + 0.008f * H;
    k.shipView = inSpace && m.shipViewOk;
    if (k.shipView)
    {
        // no wider than tall: the view fits the model by its width, and a long ship (Shockwave's Anubis) ran out of a wide, low box at its nose (issue 46)
        k.shipRect[0] = (mx0 + mx1) / 2, k.shipRect[1] = my + 0.125f * H, k.shipRect[2] = std::min((mx1 - mx0) / 2, 0.125f * H), k.shipRect[3] = 0.125f * H;
        my += 0.262f * H;
    }
    wchar_t b[48];
    // docked no view of the ship stands (the status panel's lives in space; the window's own ShipView_Control is hidden by its builder, BASE_ROOMS_PLAN): the column carries what the ship is, two numbers a line
    if (!inSpace)
    {
        const ShipFacts& s = m.facts;
        if (s.ok)
        {
            const struct
            {
                const wchar_t* word;
                float value;
            } facts[] = {{L"Armor", s.hull}, {L"Cargo space", s.hold}, {L"Gun mounts", static_cast<float>(s.guns)}, {L"Turret mounts", static_cast<float>(s.turrets)}, {L"Best gun class", static_cast<float>(s.gunClass)}, {L"Best shield class", static_cast<float>(s.shieldClass)}, {L"Nanobots", s.nanobots}, {L"Shield batteries", s.batteries}};
            const float half = (mx1 - mx0 - 0.03f * H) / 2, rowPitch = 0.034f * H;
            for (int i = 0; i < 8; ++i)
            {
                const float a = i % 2 ? mx0 + half + 0.03f * H : mx0;
                skinkit::Row(a, a + half, my + (i / 2) * rowPitch, rowPitch, facts[i].word, equiprules::ValueWord(facts[i].value));
            }
            my += 4 * rowPitch + 0.012f * H;
            skinkit::Rule(mx0, mx1, my);
            my += 0.008f * H;
        }
    }
    const Loadout& load = k.load;
    if (load.guns > 0)
    {
        swprintf_s(b, L"%d GUNS", load.guns);
        Text(mx0, my + 0.014f * H, L"FIREPOWER", 0.026f * H, Argb(kChrome, 1));
        Text(mx1, my + 0.014f * H, b, 0.026f * H, Argb(kInk, 1), Right);
        my += 0.032f * H;
        swprintf_s(b, L"HULL %.0f/S   SHIELD %.0f/S   ENERGY %.0f/S", load.hull, load.shield, load.energy);
        TextFit((mx0 + mx1) / 2, my + 0.014f * H, b, 0.026f * H, mx1 - mx0, Argb(kInk, 1), Center);
        my += 0.040f * H;
    }
    // the selected item against what it would stand beside
    if (m.compare)
    {
        skinkit::Rule(mx0, mx1, my);
        my = DrawCompare(mx0, mx1, my + 0.008f * H, m.block, H);
        if (in.pad && m.canTag) // the click in words under the numbers (the lane holds FILTER, ITEM, MOUNT, JETTISON, CARD already)
        {
            const float gh = 0.028f * H, gw = GlyphWidth(glyphs::R3, gh);
            Glyph(glyphs::R3, mx0 + gw / 2, my + 0.020f * H, gh, Argb(kInk, 1));
            Text(mx0 + gw + 0.010f * H, my + 0.020f * H, m.tagged ? L"CLICK: UNTAG" : L"CLICK: COMPARE THE OTHERS WITH THIS", 0.024f * H, Argb(m.tagged ? kActive : kChrome, 1));
            my += 0.044f * H;
        }
    }
    // the purse and the hold under the ship's name, in the docked strip's words and sizes (uistrip DrawStrip's second line, which stands down over a NeuroNet
    // window): at the column's foot the compare block pushed them under the prompts (issue 67)
    {
        float lx = f.left;
        const float ly = f.titleY + 0.043f * H; // (between the title and the filters: at 1280x800 0.050 H left the filters 8 px under it)
        const auto item = [&](const wchar_t* label, const std::wstring& value, u32 rgb)
        {
            lx += Text(lx, ly, label, 0.024f * H, Argb(kChrome, 1), Left, 0.06f) + 0.010f * H;
            lx += Text(lx, ly, value, 0.030f * H, Argb(rgb, 1)) + 0.032f * H;
        };
        item(L"CREDITS", Money(m.credits), kInk);
        if (m.cargo && m.cap > 0)
            item(L"CARGO", std::to_wstring(m.used) + L" / " + std::to_wstring(m.cap), m.used >= m.cap ? kWarn : kInk);
    }
    if (!in.pad && list.count > 0)
    {
        float cx = 0.425f * W;
        const float y2 = in.laneY, ch = in.laneH;
        if (m.canMount)
            cx = ChipAt(in, out, cx, y2, ch, items[list.sel].mounted ? L"UNMOUNT" : L"MOUNT", kActMount, 0);
        if (m.jetButton && !empty)
            cx = ChipAt(in, out, cx, y2, ch, k.jettisonArmed ? L"CLICK AGAIN TO JETTISON" : L"JETTISON", kActJettison, 0);
        if (m.trade)
            cx = ChipAt(in, out, cx, y2, ch, L"BEST PRICES", kActRoutes, 0);
        if (m.canTag) // (the mouse's way to RS's click, issue 44)
            ChipAt(in, out, cx, y2, ch, m.tagged ? L"UNTAG" : L"COMPARE THE OTHERS WITH THIS", kHitTag, 0);
        Mouse(in, f, L"CLICK AN ITEM, WHEEL SCROLLS");
    }
}
} // namespace inventorypage
