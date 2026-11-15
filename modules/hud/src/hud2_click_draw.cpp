// The keyboard-and-mouse form's clickable parts as they draw (modules\hud\docs\UI_HUD2.md 5.4): the maneuver row under the
// nav's chips, the threat / contact list under the card (both forms draw the threat list here) and the target card's
// close / next-enemy icons. Each records its clickable rectangles (HotRect) and brightens the one under the cursor
// (HotHovered) through the frame's host (hud2_frame.h FrameHost); the clicks themselves are the game's side, hud2_click.cpp.
// Part of HUD 2's game-free core (hud2_frame.h).
#include "hud2_impl.h"
#include "hud2_layout.h"

#include <algorithm>

// (ASCII only: the build reads the sources as cp1252, non-ASCII text goes in as \u escapes)
namespace hud2
{
using namespace hud2kit;

float DrawManeuverRow(const Model& m, const Layout& L, float x, float y, float a, float& right)
{
    SetAlpha(a);
    // words where the row had pictures (R4): the running maneuver alone, one lit chip with its key; the whole row in words on a peek, or while the mouse is on
    // it, so every click keeps its place (the 0.4.0 HUD 2 build record Q3, private)
    const float H = L.H, k = L.k, T = 0.024f * H * k, kh = 0.024f * H * k, gap = 0.016f * H * k, hh = 0.021f * H * k;
    static const wchar_t* const kWords[5] = {L"FREE FLIGHT", L"GOTO", L"DOCK", L"FORMATION", L"AUTOPILOT"};
    const int run = m.ap ? 4 : m.manRun >= 0 && m.manRun < 4 ? m.manRun : 0;
    const int hover = HotHovered();
    const bool whole = m.peek || (hover >= kHotMan && hover < kHotMan + 5);
    float cx = x;
    for (int i = 0; i < 5; ++i)
    {
        if (!whole && i != run)
            continue;
        const bool lit = i == run, hov = hover == kHotMan + i;
        const u32 col = lit ? kActive : hov ? kInk : kChrome;
        const RecUnit unit; // (the maneuver's key and its word)
        // (tight keycaps: the floor-grown 0.033 H cap, 0.006 H below the row, came 0.0094 H from the headline's box under the row - the lint's panel.overlap)
        const float kw = DrawPrompt(m.manBtn[i], 0, 0, kh, 0, true, true);
        const float tw = TextWidth(kWords[i], T, 0.06f), w = (kw > 0 ? kw + 0.008f * H * k : 0) + tw;
        Plate(cx - 0.006f * H * k, y - hh, cx + w + 0.006f * H * k, y + hh, 0.004f * H * k, kTL | kTR | kBR | kBL,
              hov ? 0.5f : lit ? 0.35f : 0.2f, hov ? kInk : kChrome, hov ? 0.6f : 0);
        if (kw > 0)
            DrawPrompt(m.manBtn[i], cx + kw / 2, y, kh, Argb(kChrome, 0.9f), false, true);
        Text(cx + (kw > 0 ? kw + 0.008f * H * k : 0), y, kWords[i], T, Argb(col, 1), Left, 0.06f);
        HotRect(kHotMan + i, cx - 0.006f * H * k, y - hh, cx + w + 0.006f * H * k, y + hh);
        right = cx + w + 0.006f * H * k;
        cx += w + gap;
    }
    return 0.048f * H * k;
}

void DrawCardIcons(const Model& m, const Layout& L, float a, bool compact)
{
    if (!m.target)
        return;
    SetAlpha(a);
    // a row at the card's foot (the card is taller in the keyboard form), right-aligned: [key] NEXT  [key] CLEAR, the pad's words (R4). `compact` = Focus's card:
    // its click row 0.040 under its rows, right of its 0.046 icon column
    const float H = L.H, k = L.k, kh = 0.026f * H * k, T = 0.024f * H * k;
    // (0.0085 H down from the spec's line: the card's numbers above it stand at the text floor)
    const float y = L.tgtY1 - (compact ? 0.020f * H * k : CardFootUp(L));
    float x = L.tgtX1 - (compact ? 0.016f : 0.018f) * H * k; // right to left: clear, then next
    for (int i = 0; i < 2; ++i)
    {
        const int slot = i == 0 ? kHotClear : kHotNextEnemy;
        const Prompt& p = i == 0 ? m.clearBtn : m.nextEnemyBtn;
        const wchar_t* label = i == 0 ? L"CLEAR" : L"NEXT";
        const bool hov = HotHovered() == slot;
        const u32 col = Argb(hov ? kInk : kChrome, 1);
        const float x1 = x, lw0 = TextWidth(label, T, 0.08f);
        const RecUnit unit; // (the key and its label: one click)
        Text(x1, y, label, T, col, Right, 0.08f);
        float x0 = x1 - lw0;
        const float kw = DrawPrompt(p, 0, 0, kh, 0, true, true);
        if (kw > 0)
        {
            DrawPrompt(p, x0 - 0.006f * H * k - kw / 2, y, kh, Argb(kChrome, 0.9f), false, true);
            x0 -= kw + 0.006f * H * k;
        }
        if (hov)
            Plate(x0 - 0.004f * H * k, y - 0.018f * H * k, x1 + 0.004f * H * k, y + 0.018f * H * k, 0.004f * H * k,
                  kTL | kTR | kBR | kBL, 0.4f, kInk, 0.6f);
        HotRect(slot, x0 - 0.004f * H * k, y - 0.018f * H * k, x1 + 0.004f * H * k, y + 0.018f * H * k);
        x = x0 - 0.024f * H * k;
    }
}

// The K&M card's click row right of the card's column (DrawCardIcons, compact false)
float CardIconsContentWidth(const Model& m, const Layout& L)
{
    const float H = L.H, k = L.k, kh = 0.026f * H * k, T = 0.024f * H * k;
    float w = 0;
    for (int i = 0; i < 2; ++i)
        w += TextWidth(i == 0 ? L"CLEAR" : L"NEXT", T, 0.08f) + DrawPrompt(i == 0 ? m.clearBtn : m.nextEnemyBtn, 0, 0, kh, 0, true, true) + 0.006f * H * k +
             0.024f * H * k;
    return CardColumn(m, L) + 0.03f * H * k + w + 0.018f * H * k;
}

// The list's width (DrawList): its header (the filter words, or TARGETS / NEXT and the filter) and its rows
float ListContentWidth(const Model& m, const Layout& L)
{
    const float H = L.H, k = L.k, T = 0.024f * H * k, g = 0.036f * H * k * 0.9f;
    float head = g * 0.2f;
    if (m.kbm)
        head += 0.028f * H * k + 0.012f * H * k + (!m.list && m.nThreats == 0 ? TextWidth(L"CONTACTS", T, 0.08f) : 0.0f);
    if (m.list)
        for (int i = 0; i < 5; ++i)
            head += TextWidth(kListFilters[i], T, 0.02f) + 0.014f * H * k;
    else if (m.nThreats > 0 || !m.kbm)
    {
        const float w1 = DrawPrompt(m.threatsBtn, 0, 0, g, 0, true);
        if (w1 > 0)
            head += w1 + g * 0.3f + TextWidth(L"TARGETS", T, 0.08f) + 0.03f * H * k;
        if (!(m.kbm && m.target))
            head += DrawPrompt(m.nextBtn, 0, 0, g, 0, true) + g * 0.35f + TextWidth(L"NEXT", T, 0.08f);
        head += 0.012f * H * k + TextWidth(m.filter, T, 0.08f);
    }
    float rows = 0;
    for (int i = 0; i < m.nThreats && i < 10; ++i)
        rows = std::max(rows, 0.05f * H * k + TextWidth(m.threats[i].name, m.target ? 0.026f * H * k : T) + 0.02f * H * k +
                                  TextWidth(m.threats[i].dist, m.target ? 0.026f * H * k : T));
    return std::max(head, rows) + 0.018f * H * k;
}

void DrawList(const Model& m, const Layout& L, float a)
{
    SetAlpha(a);
    const float H = L.H, k = L.k, T = 0.024f * H * k;
    const float x0 = L.tgtX0, x1 = L.tgtX1, right = x1 - 0.018f * H * k;
    float yy = L.thrY0 + 0.034f * H * k;
    const float g = 0.036f * H * k * 0.9f;
    float gx = x0 + g * 0.2f;
    if (m.kbm) // the list toggle: the game's list icon, lit while the contact list mode is on
    {
        const bool hov = HotHovered() == kHotListHead;
        const float s = 0.028f * H * k;
        const RecUnit unit; // (the list toggle's icon and its word)
        if (!Icon(flicons::List, gx + s / 2, yy, s, Argb(m.list ? kActive : hov ? kInk : kChrome, 1)))
            Ring(gx + s / 2, yy, s * 0.3f, 2, Argb(m.list ? kActive : kChrome, 1));
        float hx1 = gx + s;
        if (!m.list && m.nThreats == 0) // calm: the header alone, a way into the contact list
            hx1 += 0.008f * H * k + Text(gx + s + 0.008f * H * k, yy, L"CONTACTS", T, Argb(hov ? kInk : kDim, 1), Left, 0.08f);
        HotRect(kHotListHead, gx - 0.004f * H * k, yy - 0.02f * H * k, hx1 + 0.004f * H * k, yy + 0.02f * H * k);
        gx += s + 0.012f * H * k;
    }
    if (m.list)
    {
        // the game's filters as words, the current one lit
        for (int i = 0; i < 5; ++i)
        {
            const bool cur = i == m.listFilter, hov = HotHovered() == kHotFilter + i;
            const float w = Text(gx, yy, kListFilters[i], T, Argb(cur ? kActive : hov ? kInk : kDim, 1), Left, 0.02f);
            HotRect(kHotFilter + i, gx - 0.004f * H * k, yy - 0.02f * H * k, gx + w + 0.004f * H * k, yy + 0.02f * H * k);
            gx += w + 0.014f * H * k;
        }
    }
    else if (m.nThreats > 0 || !m.kbm)
    {
        // the threat list in combat: LB TARGETS, R3 NEXT, the filter (the keyboard: its keys)
        const float w1 = DrawPrompt(m.threatsBtn, 0, 0, g, 0, true);
        {
            const RecUnit unit; // (the prompt and its own label)
            DrawPrompt(m.threatsBtn, gx + w1 / 2, yy, g, Argb(kChrome, 1));
            // (the keyboard has no key for the picker: no TARGETS label, and no gap, without its button - the gap alone,
            // uncounted by ListContentWidth, left NEXT short of room: it scrolled as "|EXT" at 4K, "NE" at 1280x800)
            if (w1 > 0)
                gx += w1 + g * 0.3f, gx += Text(gx, yy, L"TARGETS", T, Argb(kChrome, 1), Left, 0.08f) + 0.03f * H * k;
            else if (!m.kbm)
                gx = x0;
        }
        // NEXT: not in the keyboard form while the card shows its NEXT ENEMY icon - the same command and key (the round 4
        // critique saw CTRL+R twice: one binding, USER_NEXT_ENEMY, labelled in two places - not a duplicate binding)
        if (!(m.kbm && m.target))
        {
            const float w2 = DrawPrompt(m.nextBtn, 0, 0, g, 0, true);
            const RecUnit unit; // (the prompt and its own label)
            DrawPrompt(m.nextBtn, gx + w2 / 2, yy, g, Argb(kChrome, 1));
            // (fit to the room left of the filter word: at 4:3 "NEXT" ran into "HOSTILES" - the fit rule)
            const float fw = TextWidth(m.filter, T, 0.08f) + 0.012f * H * k;
            TextMarquee(gx + w2 + g * 0.35f, yy, L"NEXT", T, right - fw - (gx + w2 + g * 0.35f), Argb(kChrome, 1), Left, 0.08f);
        }
        Text(right, yy, m.filter, T, Argb(kDim, 1), Right, 0.08f);
    }
    yy += std::max(0.044f * H * k, 0.0455f * H); // (the header's keycap stands at the floor: 0.01 H to the first row)
    for (int i = 0; i < m.nThreats && i < 10; ++i)
    {
        const bool hov = m.kbm && HotHovered() == kHotRow + i, tgt = m.threats[i].id && m.threats[i].id == m.targetId;
        Plate(x0, yy - 0.019f * H * k, x1, yy + 0.019f * H * k, 0, 0, hov ? 0.75f : 120 / 255.0f, kChrome, hov ? 0.6f : 0);
        if (!Icon(m.threats[i].icon, x0 + 0.032f * H * k, yy, 0.028f * H * k, Argb(m.threats[i].col, 1))) // (0.018 H from the edge, as the distance on the right)
            Disc(x0 + 0.032f * H * k, yy, 0.006f * H * k, Argb(m.threats[i].col, 1));
        const float rowT = m.target ? 0.026f * H * k : T; // (no target held: the rows stand alone, at the floor)
        const float dw = TextWidth(m.threats[i].dist, rowT);
        TextMarquee(x0 + 0.058f * H * k, yy, m.threats[i].name, rowT,
                    hud2layout::RowRoom(x0 + 0.058f * H * k, right, dw, 0.02f * H * k, false, 0, 0), // (real room)
                Argb(tgt ? kActive : kInk, 1));
        Text(right, yy, m.threats[i].dist, rowT, Argb(kInk, 1), Right);
        HotRect(kHotRow + i, x0, yy - 0.019f * H * k, x1, yy + 0.019f * H * k);
        yy += 0.042f * H * k;
    }
    if (m.list && m.listTotal > m.nThreats) // where the wheel has scrolled to
    {
        wchar_t b[48];
        swprintf_s(b, L"%d-%d OF %d", m.listFirst + 1, m.listFirst + m.nThreats, m.listTotal);
        Text(right, yy - 0.008f * H * k, b, T, Argb(kDim, 1), Right, 0.06f);
    }
    else if (m.list && m.listTotal == 0)
        Text(x0 + 0.058f * H * k, yy, L"NONE", T, Argb(kDim, 1), Left, 0.08f);
}
} // namespace hud2
