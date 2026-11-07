// HUD 2's targeting display drawn (modules\hud\docs\UI_HUD2_SKIN.md 6.10; the rules: hud2_target.h): the attitude caret, the bars, the words, the mini card, the marks
// of the other ships. Draw only, pad and keyboard alike: clicks stay the game's records. All or nothing with hudvis::TargetFormOn: while the game's bracket stands,
// nothing here draws.
#include "hud2_impl.h"
#include "hud2_target.h"
#include "ui/hud_rows.h"

#include <cmath>

// (ASCII only: the build reads the sources as cp1252)
namespace hud2
{
using namespace hud2kit;
namespace ht = hud2target;

namespace
{
ht::State g_state;
ht::Out g_out;

// The caret over the ship, its shape says the attitude as well as its colour: hostile solid, neutral hollow, friendly open.
void Caret(float x, float y, float s, int attitude, u32 rgb)
{
    const u32 c = Argb(rgb, 1), navy = Argb(kNavy, 0.7f);
    const float lw = std::max(1.5f, s * 0.16f), key = lw + 2;
    const float l[2] = {x - s, y - s}, r[2] = {x + s, y - s}, b[2] = {x, y + s * 0.6f};
    Line(l[0], l[1], b[0], b[1], key, navy);
    Line(r[0], r[1], b[0], b[1], key, navy);
    if (attitude != ht::Friendly)
        Line(l[0], l[1], r[0], r[1], key, navy);
    if (attitude == ht::Hostile)
        Tri(l[0], l[1], r[0], r[1], b[0], b[1], c);
    Line(l[0], l[1], b[0], b[1], lw, c);
    Line(r[0], r[1], b[0], b[1], lw, c);
    if (attitude != ht::Friendly)
        Line(l[0], l[1], r[0], r[1], lw, c);
}

// The target's four carets (hud2_target.h kQuadGap): on the mark's diagonals, each pointing at the centre, the attitude in the shape as Caret's (hostile solid,
// neutral hollow, friendly open: the base left out); the navy keyline under each keeps them read on a bright nebula or a station's lights
void CaretQuad(float x, float y, float half, float s, float gapQ, int attitude, u32 rgb)
{
    const u32 c = Argb(rgb, 1), navy = Argb(kNavy, 0.7f);
    const float lw = std::max(1.5f, s * 0.16f), key = lw + 2;
    for (const float dx : {-1.0f, 1.0f})
        for (const float dy : {-1.0f, 1.0f})
        {
            const float ux = dx * 0.70711f, uy = dy * 0.70711f;                 // out along the diagonal
            const float tx = x + dx * half + ux * gapQ, ty = y + dy * half + uy * gapQ; // the point, by the mark's corner
            const float bx = tx + ux * s * ht::kQuadLen, by = ty + uy * s * ht::kQuadLen; // the base's middle, further out
            const float l[2] = {bx - uy * s, by + ux * s}, r[2] = {bx + uy * s, by - ux * s};
            Line(l[0], l[1], tx, ty, key, navy);
            Line(r[0], r[1], tx, ty, key, navy);
            if (attitude != ht::Friendly)
                Line(l[0], l[1], r[0], r[1], key, navy);
            if (attitude == ht::Hostile)
                Tri(l[0], l[1], r[0], r[1], tx, ty, c);
            Line(l[0], l[1], tx, ty, lw, c);
            Line(r[0], r[1], tx, ty, lw, c);
            if (attitude != ht::Friendly)
                Line(l[0], l[1], r[0], r[1], lw, c);
        }
}

// The soft bed behind bars and words: low, always on, feathered on every side; a plate would read as a box near the aim point.
void SoftBed(float x0, float y0, float x1, float y1, float f)
{
    const u32 in = Argb(kNavy, 0.34f), out = Argb(kNavy, 0);
    Grad(x0, y0, x1 - x0, y1 - y0, in, in, in, in);
    Grad(x0, y0 - f, x1 - x0, f, out, out, in, in);
    Grad(x0, y1, x1 - x0, f, in, in, out, out);
    Grad(x0 - f, y0, f, y1 - y0, out, in, in, out);
    Grad(x1, y0, f, y1 - y0, in, out, out, in);
    Grad(x0 - f, y0 - f, f, f, out, out, in, out);
    Grad(x1, y0 - f, f, f, out, out, out, in);
    Grad(x1, y1, f, f, in, out, out, out);
    Grad(x0 - f, y1, f, f, out, in, out, out);
}

// One bar of the form: its pips, the glow held inside its row, white pips for what was just lost, its number after a hit.
void FormBar(float x, float y, float w, float h, float v, u32 rgb, float glow, const hud2bars::BloomOut& bloom, float lost, float numberA, float numberX, float numberY, float H, float k, float a)
{
    if (v < 0)
        return;
    SegBar(x, y, w, h, v, rgb, 20, 2);
    if (lost > 0)
        Rect(x + w * v, y, w * std::min(lost, 1 - v), h, Argb(0xFFFFFF, 0.85f));
    if (glow > 0.003f) // (more scale, the owner: 1.5 x the row at 0.35 before)
        Rect(x, y - h * 0.5f, w * v, h * 2.0f, Argb(rgb, 0.50f * glow));
    if (bloom.on)
    {
        Rect(x, y, w, h, Argb(rgb, bloom.light));
        const float reach = 0.018f * H * k * bloom.reach; // (near its row: 6.11; 0.01 H at 0.25 before)
        if (reach > 0)
            Rect(x - reach, y - reach * 0.5f, w + 2 * reach, h + reach, Argb(rgb, 0.40f * (1 - bloom.pips)));
    }
    if (numberA > 0)
    {
        wchar_t b[8];
        swprintf_s(b, L"%d", static_cast<int>(v * 100 + 0.5f));
        SetAlpha(a * numberA);
        Text(numberX, numberY, b, 0.024f * H * k, Argb(rgb, 1), Left);
        SetAlpha(a);
    }
}

int AttitudeOf(u32 col) { return col == kHostile ? ht::Hostile : col == kFriend ? ht::Friendly : ht::Neutral; }
} // namespace

void StepTargetForm(const Model& m, const Layout& L, float dt, int motion, bool ringUp)
{
    ht::In in;
    in.dt = dt, in.id = m.target ? m.targetId : 0, in.onScreen = m.tMark.onScreen, in.x = m.tMark.x, in.y = m.tMark.y, in.r = m.tMark.r;
    in.W = L.W, in.H = L.H, in.k = L.k, in.R = L.R;
    in.shield = m.tBars ? m.tShield : -1, in.hull = m.tBars ? m.tHull : -1;
    in.motion = motion, in.cueRow = m.cueShow && m.nCue > 0;
    for (int i = 0; i < 2; ++i)
        if (m.held[i].left > 0 && L.heldShow[i] && L.alertX1 > L.alertX0)
            in.panels[in.nPanels++] = {L.heldX[i], L.heldY[i], L.heldX[i] + (L.alertX1 - L.alertX0), L.heldY[i] + (L.alertY1 - L.alertY)};
    in.ring = {L.cx - L.R - 0.02f * L.H * L.k, L.cy - L.R - hud2target::kRingTop * L.H * L.k, L.cx + L.R + 0.02f * L.H * L.k, L.cy + L.R + hud2target::kRingTop * L.H * L.k}, in.hasRing = ringUp, in.ringGone = !ringUp;
    in.panels[in.nPanels++] = {L.navX0, L.navY0, L.navX1, L.navY1};
    in.panels[in.nPanels++] = {L.tgtX0, L.tgtY0, L.tgtX1, std::max(L.tgtY1, L.thrY1)};
    in.panels[in.nPanels++] = {L.shipX0, L.shipY0, L.shipX1, L.shipY1};
    in.panels[in.nPanels++] = {L.loX0, L.loY0, L.loX1, L.loY1};
    in.panels[in.nPanels++] = {L.consX0, L.consY0, L.consX1, L.consY1};
    if (L.ctxX1 > L.ctxX0)
        in.panels[in.nPanels++] = {L.ctxX0, L.ctxTop, L.ctxX1, L.ctxY + 0.03f * L.H};
    if (!m.alert.empty() && m.alertLeft > 0 && L.alertX1 > L.alertX0) // (only a card that stands: the text outlives it, and a wide card gone hid the block for good: the owner's peek)
        in.panels[in.nPanels++] = {L.alertX0, L.alertY, L.alertX1, L.alertY1};
    if (L.dlgX1 > L.dlgX0)
        in.panels[in.nPanels++] = {L.dlgX0, L.dlgY0, L.dlgX1, L.dlgY1};
    std::wstring words = m.tName;
    words += L"  ";
    words += m.tDist;
    in.wordsW = TextWidth(words, 0.024f * L.H * L.k);
    in.cardW = std::max(ht::kCardW * L.H * L.k, in.wordsW + 0.024f * L.H * L.k);
    g_out = g_state.Step(in);
    // T8's record: the seat and its side, on a change only
    static bool seat = false;
    static int side = 0;
    static unsigned id = 0;
    if (g_out.show && (g_out.seat != seat || g_out.side != side || in.id != id))
    {
        seat = g_out.seat, side = g_out.side, id = in.id;
        Log("hud2 form: seat %d side %d target %u at (%.0f, %.0f), R %.0f", seat ? 1 : 0, side, in.id, in.x, in.y, in.R);
    }
}

// debug `hud2 form`: what the step gave, and the panels it read
std::string TargetFormState()
{
    const ht::Out& o = g_out;
    char line[200];
    snprintf(line, sizeof(line), "show=%d seat=%d side=%d settle=%.2f wordsA=%.2f plateA=%.2f block=(%.0f,%.0f) caret=(%.0f,%.0f) cardX=%.0f", o.show ? 1 : 0, o.seat ? 1 : 0, o.side, o.settle,
             o.wordsA, o.plateA, o.blockX, o.blockY, o.caretX, o.caretY, o.cardX0);
    return line;
}

void DrawTargetForm(const Model& m, const Layout& L, float a, int motion)
{
    const ht::Out& o = g_out;
    if (!m.target || !o.show || a <= 0.01f)
        return;
    const float H = L.H, k = L.k, T = 0.024f * H * k;
    const int att = AttitudeOf(m.tCol);
    SetAlpha(a);
    RecordTag("target.form");
    // bars and words: from the card's place to the settled block, with the change
    const float bw = ht::kBarW * H * k, bh = ht::kBarH * H * k, gap = ht::kBarGap * H * k;
    const float ch = ht::kCardH * H * k;
    const float t = o.settle, cardInX = o.cardX0 + 0.012f * H * k, cardInY = o.cardY0 + 0.040f * H * k;
    const float bx = cardInX + (o.blockX - bw / 2 - cardInX) * t, by = cardInY + (o.blockY - cardInY) * t;
    std::wstring words = m.tName;
    words += L"  ";
    words += m.tDist;
    const float ww = TextWidth(words, T);
    const float wordsY = by + (m.tBars ? 2 * (bh + gap) : 0.0f) + T * 0.6f;
    const float cw = std::max(ht::kCardW * H * k, ww + 0.024f * H * k);
    const float centred = std::max(0.012f * H * k, std::min(bx + bw / 2 - ww / 2, L.W - ww - 0.012f * H * k));
    const float wx = cardInX + (centred - cardInX) * t; // (inside the card's plate, from its left edge, until the change)
    if (o.plateA > 0.01f)
    {
        RecordTag("target.card");
        SetAlpha(a * o.plateA);
        Plate(o.cardX0, o.cardY0, o.cardX0 + cw, o.cardY0 + ch, 0.008f * H * k, kTR | kBL, 200 / 255.0f, kChrome, 0.43f, 'l', m.tCol);
        SetAlpha(a * o.classA);
        Text(o.cardX0 + 0.012f * H * k, o.cardY0 + 0.020f * H * k, m.tAtt + L"  " + m.tClass, T, Argb(m.tCol, 1), Left, 0.06f);
        SetAlpha(a);
        RecordTag("target.form");
    }
    else
    {
        SetAlpha(a * o.wordsA);
        SoftBed(std::min(wx, bx), by - bh, std::max(wx + ww, bx + bw), wordsY + T * 0.6f, 0.02f * H * k);
    }
    SetAlpha(a * o.wordsA);
    if (m.tBars)
    {
        // the two numbers side by side, on the pair's middle: at the bars' own pitch they stood on each other and on the words (padmissile lint)
        const float nx = bx + bw + 0.008f * H * k, ny = by + bh + gap / 2;
        const float nx2 = o.hitShieldA > 0 ? nx + TextWidth(L"100", 0.024f * H * k) + 0.008f * H * k : nx;
        FormBar(bx, by, bw, bh, m.tShield, kShield, m.fx.glowTShield, m.fx.bloomTShield, o.lostShield, o.hitShieldA, nx, ny, H, k, a * o.wordsA);
        FormBar(bx, by + bh + gap, bw, bh, m.tHull, kHull, m.fx.glowTHull, m.fx.bloomTHull, o.lostHull, o.hitHullA, nx2, ny, H, k, a * o.wordsA);
    }
    Text(wx, wordsY, words, T, Argb(kInk, 1));
    SetAlpha(a);
    // the carets stay on the ship, inside the ring too: four, pointing in from its diagonals (7 Nov 2026); a waypoint target adds a small diamond over them, no more
    if (o.caretY >= 0)
    {
        const float s = ht::CaretSize(o.mark, H, k); // (a ship's radius reaches the marks' cap at 300 m: the carets stay marks)
        float cy = o.caretY;
        if (m.tWaypoint)
        {
            // a thin chrome outline with a navy edge, the game's waypoint shape made small; its lower point stays clear of the object's box and its top carets, so
            // a small ship's diamond rises. Only the glow breathes: the line stands whole at the bottom of the breath, and the breath rests at 60 % under FADES and NONE
            const float d = ht::kDiamondHalf * H * k, lw = std::max(2.0f, ht::kDiamondLine * H * k);
            cy = std::min(cy, m.tMark.y - o.mark / 2 - ht::QuadReach(o.mark, H, k) - ht::kDiamondClear * H * k - d);
            const float dia[8] = {o.caretX, cy - d, o.caretX + d, cy, o.caretX, cy + d, o.caretX - d, cy};
            const float br = motion == ht::MotionFull ? ht::Breathe(GetTickCount() / 1000.0f, ht::kWaypointHz, motion) : 0.6f;
            for (const float g : {3.0f, 2.0f})
                Outline(dia, 4, lw * g, Argb(kChrome, 0.10f * br));
            Outline(dia, 4, lw + 2, Argb(kNavy, 0.6f));
            Outline(dia, 4, lw, Argb(kChrome, 1));
        }
        CaretQuad(m.tMark.x, m.tMark.y, o.mark / 2, s, ht::kQuadGap * H * k, att, m.tCol);
    }
    // the other ships: each the richest form that touches nothing
    ht::Box taken[12 + kMaxMarks];
    int nTaken = 0;
    taken[nTaken++] = {std::min(wx, bx), by, std::max(wx + ww, bx + bw), wordsY + T * 0.6f};
    // ... and off the HUD's reading parts: a name stood on the subtitles and a prompt (p2r)
    taken[nTaken++] = {L.cx - L.R, L.cy - L.R, L.cx + L.R, L.cy + L.R}; // (the ring: 6.10 "falls a step when it would touch")
    taken[nTaken++] = {L.navX0, L.navY0, L.navX1, L.navY1};
    taken[nTaken++] = {L.tgtX0, L.tgtY0, L.tgtX1, std::max(L.tgtY1, L.thrY1)};
    taken[nTaken++] = {L.shipX0, L.shipY0, L.shipX1, L.shipY1};
    taken[nTaken++] = {L.loX0, L.loY0, L.loX1, L.loY1};
    taken[nTaken++] = {L.consX0, L.consY0, L.consX1, L.consY1};
    taken[nTaken++] = {L.ctxX0, L.ctxTop, L.ctxX1, L.ctxY + 0.03f * H};
    if (L.dlgX1 > L.dlgX0)
        taken[nTaken++] = {L.dlgX0, L.dlgY0, L.dlgX1, L.dlgY1};
    if (!m.alert.empty() && m.alertLeft > 0 && L.alertX1 > L.alertX0)
        taken[nTaken++] = {L.alertX0, L.alertY, L.alertX1, L.alertY1};
    RecordTag("target.others");
    SetAlpha(a);
    for (int i = 0; i < m.nMarks && i < kMaxMarks; ++i)
    {
        const Model::Mark& s = m.marks[i];
        const float hm = 0.015f * H * k, y0 = s.y + hm;
        const float nw = TextWidth(s.name, T), dw = TextWidth(s.dist, T);
        const ht::Box forms[4] = {{s.x - nw / 2, y0, s.x + nw / 2, y0 + T}, {s.x - dw / 2, y0, s.x + dw / 2, y0 + T}, {s.x - bw / 4, y0, s.x + bw / 4, y0 + bh}, {s.x, y0, s.x, y0}};
        const int f = ht::FitForm(forms, taken, nTaken);
        Caret(s.x, s.y - hm, 0.008f * H * k, AttitudeOf(s.col), s.col);
        if (f == ht::CaretName)
            Text(s.x, y0 + T / 2, s.name, T, Argb(kInk, 1), Center);
        else if (f == ht::CaretDistance)
            Text(s.x, y0 + T / 2, s.dist, T, Argb(kInk, 1), Center);
        else if (f == ht::CaretPips && s.hull >= 0)
            SegBar(s.x - bw / 4, y0, bw / 2, bh, s.hull, kHull, 10, 2);
        if (f != ht::CaretBare)
            taken[nTaken++] = forms[f];
    }
}
} // namespace hud2
