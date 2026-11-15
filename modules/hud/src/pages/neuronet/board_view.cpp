// FLvibed - the view of the job board's and the news's page (the HUD 2 page framework, audit 70b F3d / F3e; basepage.cpp's
// DrawBoard moved unchanged): a panel in the room, the room showing beside it; the rows on its left (the game's icon, a job's
// kind, reward, title, destination and difficulty, or a story's headline), the chosen one's head over the game's own text and
// picture, which show through the panel's holes, the job list's bar, the news's ticker, the text's bar. Draws its model
// (core\game\screens\board_model.h) and nothing of the game: the rows and chips are hit boxes, the icons a set, both handed on
// by the runtime. Declared in board_page.h (a view includes only the draw kits and its model).
#include "../page_kit.h"
#include "../page_view.h"
#include "game/screens/board_model.h"

#include <cmath>

namespace boardpage
{
using namespace pagekit;
using namespace boardscreen; // (the actions' ids, the panel)

// The ground of the panel but for `cut`, under the game's pass, as rectangles the runtime draws whole: the bands GroundBut draws
// for one hole, each band's pieces left and right of it (the same rectangles, so the same pixels).
static void CutBands(Holes& out, float x0, float y0, float x1, float y1, float cx0, float cy0, float cx1, float cy1)
{
    float ys[4] = {y0, y1, std::max(y0, std::min(y1, cy0)), std::max(y0, std::min(y1, cy1))};
    std::sort(ys, ys + 4);
    for (int b = 0; b + 1 < 4; ++b)
    {
        const float a = ys[b], c = ys[b + 1];
        if (c <= a)
            continue;
        float x = x0;
        if (cy0 <= a && cy1 >= c)
        {
            if (cx0 > x)
                out.Add(x, a, cx0, c);
            x = std::max(x, cx1);
        }
        if (x1 > x)
            out.Add(x, a, x1, c);
    }
}

void View(const page::ViewIn& in, const void* model, page::PageState&, page::ViewOut& out)
{
    const boardscreen::Model& m = *static_cast<const boardscreen::Model*>(model);
    const bool jobs = m.Jobs();
    const float H = in.H;
    const Panel p = PanelOf(in.W, in.H, in.pageTop, jobs);
    // the text and the picture show through, on the panel's ground under the game's pass
    Holes h;
    h.Add(p.px0, p.py0, p.px1, p.py1);
    h.Add(p.tx0 - 2, p.ty0 - 2, p.tx1 + 2, p.ty1 + 2);
    const float cell = 0.082f * H, list0 = p.y0 + 0.07f * H, pitch = std::min(0.112f * H, (p.y1 - list0 - 0.02f * H) / kRows);
    page::IconSet& icons = out.iconSets[out.nIconSets++];
    icons.format = jobs ? "MissionVendorSummaryIcon%d" : "NewsHeadlineIcon%d", icons.base = 0, icons.count = kRows, icons.nth = 1;
    const float x0 = p.x0 + 0.02f * H, x1 = p.mid - 0.01f * H;
    for (int i = 0; i < kRows; ++i)
        if (m.rows[i].box)
        {
            const float top = list0 + i * pitch;
            icons.icons[i] = {x0 + cell / 2, top + pitch / 2, true};
            h.Add(x0 + 1, top + (pitch - cell) / 2, x0 + cell - 1, top + (pitch + cell) / 2);
        }
    GroundBut(p.x0, p.y0, p.x1, p.y1, h);
    out.holes = Holes();
    CutBands(out.holes, p.x0, p.y0, p.x1, p.y1, p.px0, p.py0, p.px1, p.py1);
    skinkit::Plate(p.x0, p.y0, p.x1, p.y1, kTL | kBR, 0.0f);
    Text(x0, p.y0 + 0.034f * H, jobs ? L"JOB BOARD" : L"NEWS", 0.036f * H, Argb(kInk, 1));
    skinkit::Rule(x0, x1, p.y0 + 0.060f * H);
    for (int i = 0; i < kRows; ++i)
    {
        const Row& r = m.rows[i];
        if (!r.box)
            continue;
        const float top = list0 + i * pitch, a = x0 + cell + 0.012f * H;
        const int hit = HitAt(out, x0, top, x1, top + pitch, kActPick, i);
        const bool on = i == m.sel;
        if (on && !(in.pad && m.textSide))
            skinkit::Focus(a - 0.006f * H, top + 0.003f * H, x1, top + pitch - 0.003f * H);
        else if (on)
            Rect(a - 0.006f * H, top, x1 - a + 0.006f * H, pitch, Argb(kChrome, 0.06f)); // (chosen: the pad is on the text)
        else if (hit >= 0 && hit == in.hover)
            Rect(a - 0.006f * H, top, x1 - a + 0.006f * H, pitch, Argb(kChrome, 0.10f));
        if (jobs)
        {
            const float rw = TextWidth(r.reward, 0.032f * H), pad = 0.012f * H;
            TextMarquee(a + pad, top + pitch * 0.34f, r.type.empty() ? r.title : r.type, 0.032f * H, x1 - a - rw - 3 * pad, Argb(kInk, 1));
            float dw = 0;
            if (r.difficulty > 0)
            {
                const std::wstring dword = L"DIFFICULTY " + std::to_wstring(r.difficulty);
                dw = TextWidth(dword, 0.024f * H) + 2 * pad;
                Text(x1 - pad, top + pitch * 0.72f, dword, 0.024f * H, Argb(kInk, 1), Right); // (white: what the row adds, beside the blue it repeats)
            }
            TextMarquee(a + pad, top + pitch * 0.72f, r.title + L"   " + r.dest, 0.024f * H, x1 - a - 2 * pad - dw, Argb(on ? kInk : kChrome, 1));
            Text(x1 - pad, top + pitch * 0.34f, r.reward, 0.032f * H, Argb(on ? kInk : kFriend, 1), Right);
        }
        else
            TextMarquee(a + 0.012f * H, top + pitch / 2, r.headline, 0.030f * H, x1 - a - 0.024f * H, Argb(on ? kInk : kChrome, 1));
    }
    // more jobs than rows: a bar beside them says how many and where the rows stand (Shockwave, 7 Nov 2026: no sign of more)
    if (m.jobTotal > m.jobRows && m.jobRows > 0)
        skinkit::ScrollBar(x1 + 0.008f * H, list0, list0 + m.jobRows * pitch, static_cast<float>(m.jobFirst) / m.jobTotal, static_cast<float>(m.jobRows) / m.jobTotal);
    // the chosen one's head, over its text
    const float hy = p.ty0 - 0.036f * H;
    if (jobs)
    {
        if (!m.reward.empty())
        {
            const std::wstring type = m.sel >= 0 ? m.rows[m.sel].type : std::wstring();
            const float rw = TextWidth(m.reward, 0.034f * H);
            TextMarquee(p.tx0, hy, type + L"   " + m.title + L"   " + m.dest, 0.028f * H, p.tx1 - p.tx0 - rw - 0.02f * H, Argb(kInk, 1));
            Text(p.tx1, hy, m.reward, 0.034f * H, Argb(kFriend, 1), Right);
        }
        else if (!m.rejected.empty()) // (a refused job: the game's own caption, "Mission Rejected", in the head's place, the reason under it: audit 109_ 2d)
            Text(p.tx0, hy, m.rejected, 0.030f * H, Argb(kAmber, 1));
    }
    else
    {
        TextMarquee(p.tx0, hy, m.headline, 0.030f * H, p.tx1 - p.tx0, Argb(kInk, 1));
        // the market's ticker across the panel, the whole of what the base trades
        if (!m.tickerText.empty() && m.ticker)
        {
            const float size = 0.028f * H, a = p.x0 + 0.03f * H, b = p.x1 - 0.03f * H, w = std::max(TextWidth(m.tickerText, size), 1.0f), y = p.y1 - 0.034f * H;
            *m.ticker = fmodf(*m.ticker, w);
            for (float x = a - *m.ticker; x < b; x += w)
                TextClipped(x, y, m.tickerText, size, a, b, Argb(kChrome, 1));
        }
    }
    // its bar, the game's own state
    if (m.bar && m.barHi > m.barLo)
    {
        const float room = p.ty1 - p.ty0, whole = room + m.barHi - m.barLo;
        skinkit::ScrollBar(p.tx1 + 0.012f * H, p.ty0, p.ty1, (m.barPos - m.barLo) / whole, room / whole);
    }
    if (in.pad && m.textSide) // the pad on the text: framed as the ship dealer's ship is
    {
        const float e = std::max(1.0f, 0.003f * H), fx0 = p.tx0 - 0.010f * H, fy0 = p.ty0 - 0.008f * H, fx1 = p.tx1 + 0.026f * H, fy1 = p.ty1 + 0.008f * H;
        Rect(fx0, fy0, fx1 - fx0, e, Argb(kActive, 0.85f));
        Rect(fx0, fy1 - e, fx1 - fx0, e, Argb(kActive, 0.85f));
        Rect(fx0, fy0, e, fy1 - fy0, Argb(kActive, 0.85f));
        Rect(fx1 - e, fy0, e, fy1 - fy0, Argb(kActive, 0.85f));
    }
    if (!in.pad)
    {
        float cx = ChipAt(in, out, p.x0, in.laneY, in.laneH, L"CLOSE", kActClose, 0);
        if (jobs && m.accept && m.sel >= 0) // (as the pad's ACCEPT: a job chosen)
            cx = ChipAt(in, out, cx, in.laneY, in.laneH, L"ACCEPT JOB", kActAccept, 0);
        Text(cx + 0.02f * H, in.laneMid, L"WHEEL SCROLLS THE TEXT", 0.026f * H, Argb(kChrome, 1));
    }
}
} // namespace boardpage
