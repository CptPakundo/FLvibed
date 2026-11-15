// FLvibed - the STATUS page's view (the HUD 2 page framework, audit 70b F4b; nnpage.cpp's DrawStatus moved unchanged): the
// page's ground round the focused faction's card (its hole: the card is the game's), the pilot's numbers at the left, the
// reputation list in the middle with the focused faction's standing under it. Draws its model
// (core\game\screens\status_model.h) and what its shell hands in (the card's hole) and nothing of the game: the rows and the
// SHOW chip are hit boxes, handed on by the runtime. Declared in status_page.h.
#include "../page_kit.h"
#include "../page_view.h"
#include "game/screens/status_model.h"

namespace statuspage
{
using namespace pagekit;
using statusrules::Band;
using statusrules::BandOf;
using statusrules::BandWord;

void View(const page::ViewIn& in, const void* model, page::PageState&, page::ViewOut& out)
{
    const statusscreen::Model& m = *static_cast<const statusscreen::Model*>(model);
    statusscreen::Focus& fo = *m.focus; // (the list's room is the view's: the rows that stand)
    screens::List& list = fo.list;
    const Frame f = {in.W, in.H, in.x0, in.x1, in.top, in.bottom, in.titleY};
    const float H = f.H, W = f.W;
    Box hole;
    if (in.textOk)
        hole.AddRect(in.text[0], in.text[1], in.text[2], in.text[3]);
    Cover(W, H, in.textReady ? hole : Box());
    Title(f, L"PLAYER STATUS");
    // the pilot: three numbers large, the rest as rows
    const float x0 = f.left, x1 = 0.30f * W;
    float y = f.top + 0.03f * H;
    if (!m.stat[0].empty())
    {
        Text(x0, y, fo.head[0], 0.026f * H, Argb(kChrome, 1));
        Text(x1, y, m.stat[0], 0.044f * H, Argb(kActive, 1), Right);
        y += 0.056f * H;
    }
    if (!m.stat[1].empty())
    {
        Text(x0, y, fo.head[1], 0.026f * H, Argb(kChrome, 1));
        Text(x1, y, m.stat[1], 0.040f * H, Argb(kInk, 1), Right);
        y += 0.050f * H;
    }
    if (!m.stat[2].empty())
    {
        Text(x0, y, fo.head[2], 0.026f * H, Argb(kChrome, 1));
        Text(x1, y, m.stat[2], 0.030f * H, Argb(kInk, 1), Right);
        y += 0.028f * H;
        if (screens::Number(m.stat[2]) > 0) // (the game may say a mission instead of a sum)
        {
            skinkit::Pips(x0, y, x1 - x0, 0.010f * H, statusrules::Share(screens::Number(m.stat[1]), screens::Number(m.stat[2])), 24, kActive);
            y += 0.03f * H;
        }
    }
    y += 0.012f * H;
    skinkit::Rule(x0, x1, y);
    y += 0.012f * H;
    const float rowH = std::min(0.056f * H, std::max(skinrules::kRowMin * H, (f.bottom - y) / 10));
    for (int i = 3; i < statusscreen::kStats; ++i)
        if (!fo.head[i].empty())
        {
            skinkit::Row(x0, x1, y, rowH, fo.head[i], m.stat[i]);
            y += rowH;
        }
    // reputation: the list in the middle, the focused faction's standing under it, its card at the right
    const float rx0 = 0.32f * W, rx1 = (hole.ok ? hole.x0 : f.right) - 0.03f * H;
    float ry = f.top + 0.03f * H;
    Text(rx0, ry, L"REPUTATION", 0.030f * H, Argb(kChrome, 1));
    std::wstring tag = statusrules::FilterWord(fo.filter);
    tag += L"  ";
    tag += std::to_wstring(list.count);
    tag += fo.byName ? L"  BY NAME" : L"  BY STANDING";
    Text(rx1, ry, tag, 0.026f * H, Argb(kDim, 1), Right);
    ry += 0.028f * H;
    skinkit::Rule(rx0, rx1, ry);
    ry += 0.008f * H;
    const float foot = f.bottom - 0.066f * H; // the standing's two lines
    const float pitch = m.shown.empty() ? 0.05f * H : std::min(0.066f * H, std::max(0.05f * H, (foot - ry) / m.shown.size()));
    list.rows = std::max(1, static_cast<int>((foot - ry) / pitch));
    list.Fit();
    // from the right: the band's word, then the bar, the name in what is left (at 16:10 the bar ran under the word, 7 Nov 2026)
    const float wordW = TextWidth(L"UNFRIENDLY", 0.026f * H), half = std::min(0.09f * H, (rx1 - rx0) * 0.14f);
    const float cx = rx1 - 0.012f * H - wordW - 0.024f * H - half, nameW = cx - half - rx0 - 0.036f * H;
    if (m.shown.empty())
        Text(rx0, ry + pitch / 2, m.reps.empty() ? L"READING THE NEURAL NET" : L"NOBODY IN THIS GROUP", 0.030f * H, Argb(kDim, 1));
    for (int i = 0; i < list.rows && list.first + i < list.count; ++i)
    {
        const int row = list.first + i;
        const statusscreen::Rep& r = m.shown[row];
        const float top = ry + i * pitch, cy = top + pitch / 2;
        const int hit = HitAt(out, rx0, top, rx1, top + pitch, statusscreen::kHitRow, row);
        if (row == list.sel)
            skinkit::Focus(rx0, top + 0.002f * H, rx1, top + pitch - 0.002f * H);
        else if (hit >= 0 && hit == in.hover)
            Rect(rx0, top, rx1 - rx0, pitch, Argb(kChrome, 0.10f));
        TextMarquee(rx0 + 0.012f * H, cy, r.name, 0.030f * H, nameW, Argb(kInk, 1));
        // the bar: from the middle, the game's own marks at -0.6 and 0.6
        const Band b = BandOf(r.v);
        const float bh = 0.012f * H;
        Rect(cx - half, cy - bh / 2, 2 * half, bh, Argb(kDim, 0.30f));
        const float len = half * std::max(-1.0f, std::min(1.0f, r.v));
        Rect(len < 0 ? cx + len : cx, cy - bh / 2, fabsf(len), bh, Argb(BandRgb(b), 1));
        for (const float mk : {-0.6f, 0.0f, 0.6f})
            Rect(cx + half * mk - 1, cy - bh, 2, 2 * bh, Argb(kChrome, 0.7f));
        Text(rx1 - 0.012f * H, cy, BandWord(b), 0.026f * H, Argb(row == list.sel ? kInk : BandRgb(b), 1), Right);
    }
    if (list.count > list.rows)
        skinkit::ScrollBar(rx1 + 0.008f * H, ry, ry + list.rows * pitch, static_cast<float>(list.first) / list.count, static_cast<float>(list.rows) / list.count);
    if (list.count > 0)
    {
        // what the number means: the game's own lines, how far they are
        const statusscreen::Rep& r = m.shown[list.sel];
        wchar_t line[96];
        swprintf_s(line, L"STANDING %+.2f", r.v);
        // two lines, the standing over what it means: on one line the meaning ran left over the pilot's rows at 1280x800 (Shockwave, 7 Nov 2026)
        const float ly = foot + 0.017f * H;
        skinkit::Rule(rx0, rx1, foot + 0.002f * H);
        Text(rx0, ly, line, 0.026f * H, Argb(BandRgb(BandOf(r.v)), 1));
        if (r.v > -0.6f)
            swprintf_s(line, L"HOSTILE AT -0.60: %.2f AWAY", r.v + 0.6f);
        else
            swprintf_s(line, L"ITS SHIPS ATTACK, ITS BASES REFUSE YOU: %.2f TO GO", -0.6f - r.v);
        TextMarquee(rx0, ly + 0.030f * H, line, 0.024f * H, rx1 - rx0, Argb(kChrome, 1));
    }
    if (!in.pad)
    {
        ChipAt(in, out, 0.32f * W, in.laneY, in.laneH, L"SHOW " + std::wstring(statusrules::FilterWord(statusrules::NextFilter(fo.filter))), statusscreen::kHitFilter, 0);
        Text(f.left, in.laneMid, L"CLICK A FACTION, WHEEL SCROLLS", 0.026f * H, Argb(kChrome, 1));
    }
}
} // namespace statuspage
