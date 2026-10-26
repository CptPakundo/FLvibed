// FLvibed - the pure geometry of the pad's prompt rows (hold word, 26 Oct 2026): factored out of
// controller\src\impl\promptbar.cpp (the bottom lane: docs\UI_HUD2_EXTENDED.md 13) and hud2.cpp ComputeLayout (HUD 2's
// context line: docs\UI_HUD2.md 5.9) without changing their behaviour, so the host-free tests (tests\promptfit_test.cpp)
// drive the very code the game runs across the display matrix. No game, no device: the callers pass the measured widths
// and their keep-out queries. Pixels in, pixels out; every size a fraction of H.
#pragma once
#include <algorithm>
#include <vector>

namespace promptfit
{
// ---------------------------------------------------------------- the bottom lane (promptbar)
constexpr int kLaneMax = 5; // the prompts a bar takes

enum class LaneMode
{
    None,
    Row,    // rule 1: the bottom row
    Rows,   // the fit rule: two rows at the bottom, the labels at the floor
    Column, // rule 4: stacked right of the colliding window
    Top,    // rule 4's last resort, docked: under the NeuroNet box
    Below   // rule 4's last resort elsewhere: the nearest legal band (under the lowest obstacle, else above the lane)
};
inline const char* LaneModeName(LaneMode m)
{
    switch (m)
    {
    case LaneMode::Row: return "row";
    case LaneMode::Rows: return "rows";
    case LaneMode::Column: return "column";
    case LaneMode::Top: return "top";
    case LaneMode::Below: return "below";
    default: return "-";
    }
}

struct LanePlaced
{
    LaneMode mode = LaneMode::None;
    bool use[kLaneMax] = {};
    float x0[kLaneMax] = {}, cy[kLaneMax] = {}; // each pill's left edge and centre line (pixels)
    float rx0 = 0, ry0 = 0, rx1 = 0, ry1 = 0;   // the pills' area
    float widest = 0, column = 0;               // the widest pill, the free column's width at the bottom slot (status)
    bool atFloor = false;                       // the labels at the fit rule's floor
    float w[kLaneMax] = {};                     // each pill's width as placed
};

// The pills' places: wN = each pill's width with its label at the nominal size, wF with the label at the fit rule's floor
// (equal to wN where the label already is at or under it), ph the pill's height, pitch the gap between pills in a row
// (pixels); `confirm` = the pill that always stays (the A pill, else the first; BACK = the last stays too). `left(y0, y1,
// right)` = the row's left limit in that band (the screen's keep-outs), `nnBottom` = the NeuroNet box's bottom when
// docked (0 = none), `lowest(x0, x1, below)` = the lowest keep-out edge in that x range. `dropped` counts the pills
// dropped. The order (docs\UI_HUD2_EXTENDED.md 13; fix-text 25 Oct 2026): the row; the row with the labels at the floor;
// the column; two rows at the bottom (labels at the floor); the column at the floor; only then the last resorts.
template <class LeftFn, class LowFn>
inline void LaneLayout(float W, float H, int n, int confirm, const float* wN, const float* wF, float ph, float pitch,
                       float nnBottom, LeftFn left, LowFn lowest, LanePlaced& out, int& dropped)
{
    out = LanePlaced();
    n = n < 0 ? 0 : (n > kLaneMax ? kLaneMax : n);
    for (int i = 0; i < n; ++i)
        out.use[i] = true;
    const float* w = wN; // the widths in use (switched to wF by the fit rule's steps)
    auto useFloor = [&]()
    {
        w = wF, out.atFloor = true;
    };
    struct Commit // the widths the drawer uses, whatever step placed the pills
    {
        LanePlaced& o;
        const float*& w;
        int n;
        ~Commit()
        {
            for (int i = 0; i < n; ++i)
                o.w[i] = w[i];
        }
    } commit{out, w, n};
    const float right = W - W * 0.05f;
    const float by0 = H * 0.910f, by1 = H * 0.950f, cy = (by0 + by1) * 0.5f;
    auto kept = [&](int i) { return i == confirm || i == n - 1; };
    auto span = [&](int from, int to)
    {
        float s = 0;
        int k = 0;
        for (int i = from; i < to; ++i)
            if (out.use[i])
                s += w[i], ++k;
        return k ? s + pitch * (k - 1) : 0.0f;
    };
    auto dropOne = [&]() // the one before BACK first; false when only the kept ones are left
    {
        for (int d = n - 2; d >= 0; --d)
            if (out.use[d] && !kept(d))
            {
                out.use[d] = false, ++dropped;
                return true;
            }
        return false;
    };
    auto placeRow = [&](int from, int to, float y)
    {
        float x = right;
        for (int i = to - 1; i >= from; --i)
            if (out.use[i])
                out.x0[i] = x - w[i], out.cy[i] = y, x -= w[i] + pitch;
    };
    // rule 1: one row in the band - at the labels' size, then with the labels at the floor (the fit rule)
    const float roomRow = right - left(by0, by1, right);
    if (span(0, n) > roomRow)
        useFloor();
    if (span(0, n) <= roomRow)
    {
        placeRow(0, n, cy);
        out.mode = LaneMode::Row;
        out.rx0 = right - span(0, n), out.ry0 = by0, out.rx1 = right, out.ry1 = by1;
        return;
    }
    // rule 4: the column right of the window, bottom-up from 0.950 H, as long as each slot is as wide as the widest pill
    // (the labels' size first: the column verified at 4K keeps its look)
    const float slot = ph + H * 0.008f;
    auto column = [&]()
    {
        float maxW = 0;
        for (int i = 0; i < n; ++i)
            maxW = std::max(maxW, w[i]);
        out.widest = maxW, out.column = right - left(by1 - ph, by1, right);
        int free = 0;
        for (; free < n; ++free)
        {
            const float y1 = by1 - free * slot, y0 = y1 - ph;
            if (y0 < H * 0.25f || right - left(y0, y1, right) < maxW)
                break;
        }
        if (free < std::min(n, 2))
            return false;
        int k = n;
        while (k > free && dropOne())
            --k;
        int s = 0; // the last kept pill at the bottom slot
        for (int i = n - 1; i >= 0; --i)
            if (out.use[i])
            {
                out.x0[i] = right - w[i], out.cy[i] = by1 - s * slot - ph * 0.5f;
                ++s;
            }
        out.mode = LaneMode::Column;
        out.rx0 = right - maxW, out.ry0 = by1 - (s - 1) * slot - ph, out.rx1 = right, out.ry1 = by1;
        return true;
    };
    w = wN, out.atFloor = false;
    if (column())
        return;
    useFloor();
    // the fit rule's two rows at the bottom (labels at the floor): the band's row takes the bar's end (BACK and the pills
    // before it) as far as its free room goes, the row above it (0.008 H higher) the rest - both right-aligned, each
    // clear of the keep-outs in its own band
    {
        int from = n;
        while (from > 0 && span(from - 1, n) <= roomRow)
            --from;
        const float uy1 = by0 - H * 0.008f, uy0 = uy1 - ph;
        if (from > 0 && from < n && span(0, from) <= right - left(uy0, uy1, right))
        {
            placeRow(from, n, cy);
            placeRow(0, from, (uy0 + uy1) * 0.5f);
            out.mode = LaneMode::Rows;
            out.rx0 = right - std::max(span(0, from), span(from, n)), out.ry0 = uy0, out.rx1 = right, out.ry1 = by1;
            return;
        }
    }
    if (column())
        return;
    // last resort, docked: the top-right lane under the NeuroNet box - right-aligned rows, filled from the bar's end (BACK
    // and the pills before it on the first row), as many rows as the room under the box holds (each row as wide as the free
    // band at its height; a blocked band is skipped); drops only when even that runs out (critique 4: the 4:3 dealers kept
    // only BUY / CLOSE)
    if (nnBottom > 0)
    {
        const float rowPitch = ph + H * 0.008f, y1st = nnBottom + H * 0.012f + ph * 0.5f;
        struct RowAt
        {
            int from, to;
            float y;
        };
        std::vector<RowAt> rows;
        for (;;)
        {
            rows.clear();
            int end = n;
            for (int r = 0; end > 0; ++r)
            {
                const float y = y1st + r * rowPitch;
                if (y + ph * 0.5f > by1) // (the lane never reaches below the band's foot)
                    break;
                const float room = right - left(y - ph * 0.5f, y + ph * 0.5f, right);
                int from = end;
                while (from > 0 && span(from - 1, end) <= room)
                    --from;
                if (span(from, end) <= 0)
                    continue; // this band is blocked (or no pill fits it): the next one down
                rows.push_back({from, end, y});
                end = from;
            }
            if (end <= 0 || !dropOne())
            {
                for (int i = 0; i < end; ++i) // (nothing left to drop: what found no row is not drawn)
                    if (out.use[i])
                        out.use[i] = false, ++dropped;
                break;
            }
        }
        for (const RowAt& r : rows)
            placeRow(r.from, r.to, r.y);
        out.mode = LaneMode::Top;
        return;
    }
    // last resort elsewhere (critique 4: never below the 0.95 H safe margin): the nearest legal band - just under the
    // lowest obstacle in the row's x range when the row still ends by 0.950 H, else the first free band above the lane,
    // searched upwards in 0.005 H steps (a gap above and below), not above the 0.05 H margin; when none is free, the band
    // itself (the pills over the obstacle's edge rather than off the safe area)
    while (span(0, n) > right - W * 0.05f && dropOne())
        ;
    const float rowW = span(0, n), gapV = H * 0.010f;
    const float low = lowest(right - rowW, right, by0);
    float y = cy;
    if (low + gapV + ph <= by1)
        y = std::max(cy, low + gapV + ph * 0.5f);
    else
        for (float yc = by1 - ph * 0.5f; yc - ph * 0.5f >= H * 0.05f; yc -= H * 0.005f)
            if (rowW <= right - left(yc - ph * 0.5f - gapV, yc + ph * 0.5f + gapV, right))
            {
                y = yc;
                break;
            }
    placeRow(0, n, y);
    out.mode = LaneMode::Below;
    out.rx0 = right - rowW, out.ry0 = y - ph * 0.5f, out.rx1 = right, out.ry1 = y + ph * 0.5f;
}

// HUD 2's prompt pill (promptbar.cpp DrawHud2) without a HOLD word: pad + glyph(s) + 0.010 H + the label + pad. `glyphs` =
// the glyphs' widths (the second one's 0.006 H gap included). A hold's word adds HoldExtra (below).
inline float PillWidth(float H, float glyphs, float label)
{
    const float pad = H * 0.012f;
    return pad + glyphs + H * 0.010f + label + pad;
}

// ---------------------------------------------------------------- HUD 2's context line (hud2.cpp ComputeLayout)
// One prompt's width without a HOLD word: glyph (g tall) + 0.3 g + the label. A hold's word adds HoldExtra (below).
inline float ContextPromptWidth(float glyph, float g, float label) { return glyph + g * 0.3f + label; }

// HUD 2's scale k (hud2.cpp ComputeLayout): hud_scale x text_size, never under 1, the aspect factor, then what the panels
// fit (the bottom row, the right column, the centre column).
inline float Hud2Scale(float W, float H, int hudScale, int textSize, bool kbm)
{
    auto clampf = [](float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); };
    float k = clampf(hudScale / 100.0f, 0.8f, 1.3f) * clampf(textSize / 100.0f, 0.8f, 1.5f);
    k = std::max(k, 1.0f);
    k *= clampf(0.9f * W / (1.6f * H), 0.8f, 1.0f);
    k = std::min(k, std::min((0.9f * W - 0.02f * H) / ((kbm ? 1.06f : 1.02f) * H), (0.88f * H) / ((kbm ? 0.662f : 0.622f) * H)));
    k = std::min(k, 0.426f / 0.36f);
    return k;
}

struct ContextPlaced
{
    int n = 0;              // the prompts kept (the last ones give way first)
    float y = 0;            // the line's centre
    float x0 = 0, x1 = 0;   // its extent (with the 0.03 H x k margin)
    bool above = false;     // moved above the panels (only a first prompt that alone does not fit)
};

// The line under the ship (docs\UI_HUD2.md 5.9 "one baseline"): at 0.915 H; the last prompts give way until the line fits
// between the ship panel (right edge shipX1) and the loadout (left edge loX0); only a line whose first prompt alone does
// not fit goes above the panels (their top panelsY0, 0.04 H x k above it). `w` = each prompt's width (nullptr = the line
// measures 0: the player is typing), `gap` the space between prompts.
inline ContextPlaced PlaceContext(const float* w, int n, float gap, float W, float H, float k, float shipX1, float loX0,
                                  float panelsY0)
{
    ContextPlaced c;
    c.n = n;
    c.y = 0.915f * H;
    const float cx = W / 2;
    auto measure = [&](int m)
    {
        float t = 0;
        if (w)
            for (int i = 0; i < m; ++i)
                t += w[i] + (i ? gap : 0);
        return t + 0.03f * H * k;
    };
    float cw = measure(c.n);
    while (c.n > 1 && (cx - cw / 2 < shipX1 || cx + cw / 2 > loX0))
        cw = measure(--c.n);
    c.x0 = cx - cw / 2, c.x1 = cx + cw / 2;
    if (c.n && (c.x0 < shipX1 || c.x1 > loX0))
        c.y = panelsY0 - 0.04f * H * k, c.above = true;
    return c;
}

// ---------------------------------------------------------------- where a hold prompt's word HOLD goes
// The owner (26 Oct 2026, follow-up): HOLD BEFORE the glyph (the word, then the glyph with its broken ring, then the
// label); where that would cost a prompt (or move the context line above the panels where it fitted before) the first
// step of a cascade that keeps every prompt the line / lane showed without any word:
//   (a) Above   - the word stacked above the glyph, small, centred on it (only its excess width over the glyph counts);
//   (b) Ring    - the word in the broken ring's top gap, only when it fits there at the text floor (else skipped: never
//                 shrunk under the floor);
//   (b2) Over the label - the word stacked above the label's start (this build's addition, the owner invited other
//                 placements: a label is wider than the word, so it costs no width where (a)'s glyph slot still does -
//                 the 4:3 TAKE CONTROL line; the word keeps its one meaning, the ring still marks the button);
//   (c) Shared  - one HOLD before a run of two or more consecutive hold prompts (the run's first carries it);
//   (d) Partial - the word dropped from the last hold prompts until everything fits; Outline = from all of them.
// Deterministic: the same widths and room always give the same step.
enum class HoldStep
{
    NoHold, // no hold prompt in the set
    Before,
    Above,
    Ring,
    OverLabel,
    Shared,
    Partial,
    Outline
};
inline const char* HoldStepName(HoldStep s)
{
    switch (s)
    {
    case HoldStep::NoHold: return "no hold";
    case HoldStep::Before: return "before";
    case HoldStep::Above: return "(a) above";
    case HoldStep::Ring: return "(b) ring";
    case HoldStep::OverLabel: return "(b2) over the label";
    case HoldStep::Shared: return "(c) shared";
    case HoldStep::Partial: return "(d) partial";
    case HoldStep::Outline: return "(d) outline";
    default: return "-";
    }
}
enum class WordAt : unsigned char
{
    None,
    Before,
    Above,
    Ring,
    OverLabel
};
inline bool Raised(WordAt a) { return a == WordAt::Above || a == WordAt::OverLabel; } // the word over the prompt's top
struct HoldPlan
{
    HoldStep step = HoldStep::NoHold;
    WordAt at[kLaneMax] = {};
};
// The word's width at its size, the gap between the word and the glyph (Before), and the broken ring's top gap: its chord
// at the ring's radius (hud2kit::Glyph: arcs from -60 + k x 90 to -10 + k x 90 degrees, radius 0.62 h - the top gap spans
// 40 degrees: chord = 2 r sin 20 deg).
struct HoldDims
{
    float word = 0, gapBefore = 0, ringChord = 0;
};
inline float RingChord(float glyphH) { return 2.0f * 0.62f * glyphH * 0.34202f; }
// The gap between the word and its glyph (Before): the broken ring reaches ~0.15 h past the glyph's slot (radius 0.62 h
// + its line), then a clear 0.2 h (0.2 h alone let the ring touch the word's D: in game at 4K, 26 Oct 2026).
inline float GapBefore(float glyphH) { return glyphH * 0.35f; }
// What a prompt's word adds to its width (glyph = the width of the glyph slot, label = the label's width at its smallest).
inline float HoldExtra(WordAt a, float glyph, float label, const HoldDims& d)
{
    switch (a)
    {
    case WordAt::Before: return d.gapBefore + d.word;
    case WordAt::Above: return std::max(0.0f, d.word - glyph);
    case WordAt::OverLabel: return std::max(0.0f, d.word - label);
    default: return 0.0f;
    }
}

// The cascade over `n` prompts (hold[i] = a hold action, glyph[i] = its glyph slot's width); `fits(plan)` = the caller's
// layout with this plan keeps what the layout without any word kept. Returns the first step that fits.
template <class Fits>
inline HoldPlan ChooseHold(int n, const bool* hold, const HoldDims& d, Fits fits)
{
    n = n < 0 ? 0 : (n > kLaneMax ? kLaneMax : n);
    HoldPlan p;
    int holds = 0;
    for (int i = 0; i < n; ++i)
        holds += hold[i] ? 1 : 0;
    if (!holds)
        return p;
    auto all = [&](HoldStep s, WordAt a)
    {
        p.step = s;
        for (int i = 0; i < n; ++i)
            p.at[i] = hold[i] ? a : WordAt::None;
    };
    all(HoldStep::Before, WordAt::Before);
    if (fits(p))
        return p;
    all(HoldStep::Above, WordAt::Above);
    if (fits(p))
        return p;
    if (d.word <= d.ringChord) // (b) only where the word stands in the ring's gap at the floor
    {
        all(HoldStep::Ring, WordAt::Ring);
        if (fits(p))
            return p;
    }
    all(HoldStep::OverLabel, WordAt::OverLabel);
    if (fits(p))
        return p;
    bool run = false; // (c) a run of two or more consecutive holds
    for (int i = 0; i + 1 < n; ++i)
        run |= hold[i] && hold[i + 1];
    if (run)
    {
        all(HoldStep::Shared, WordAt::Before);
        for (int i = 1; i < n; ++i)
            if (hold[i] && hold[i - 1])
                p.at[i] = WordAt::None;
        if (fits(p))
            return p;
    }
    all(HoldStep::Partial, WordAt::Before); // (d) the words go from the last hold prompts first
    for (int i = n - 1; i >= 0; --i)
        if (hold[i])
        {
            p.at[i] = WordAt::None;
            if (--holds == 0)
                break;
            if (fits(p))
                return p;
        }
    all(HoldStep::Outline, WordAt::None);
    return p;
}

// HUD 2's context line with the hold cascade: base[i] = each prompt's width without a word (ContextPromptWidth with
// hold 0), glyph[i] its glyph's width, label[i] its label's; out w[i] = the widths with the chosen plan. The line without
// any word decides what must stay: the same number of prompts, and not above the panels unless it was already.
struct ContextHold
{
    ContextPlaced c;
    HoldPlan plan;
    float w[3] = {};
};
inline ContextHold PlaceContextHold(const float* base, const float* glyph, const float* label, const bool* hold, int n,
                                    const HoldDims& d, float gap, float W, float H, float k, float shipX1, float loX0,
                                    float panelsY0)
{
    ContextHold r;
    n = n < 0 ? 0 : (n > 3 ? 3 : n);
    const ContextPlaced none = PlaceContext(base, n, gap, W, H, k, shipX1, loX0, panelsY0);
    auto widths = [&](const HoldPlan& p, float* w)
    {
        for (int i = 0; i < n; ++i)
            w[i] = base[i] + HoldExtra(p.at[i], glyph[i], label[i], d);
    };
    r.plan = ChooseHold(n, hold, d, [&](const HoldPlan& p)
                        {
                            float w[3] = {};
                            widths(p, w);
                            const ContextPlaced c = PlaceContext(w, n, gap, W, H, k, shipX1, loX0, panelsY0);
                            return c.n == none.n && c.above == none.above;
                        });
    widths(r.plan, r.w);
    r.c = PlaceContext(r.w, n, gap, W, H, k, shipX1, loX0, panelsY0);
    return r;
}

// The bottom lane with the hold cascade (promptbar.cpp DrawHud2): baseN / baseF = each pill's width without a word at the
// label's nominal / floor size, glyph[i] = its glyph slot's width, label[i] = its label's width at the floor, ph the
// pill's height, wordH the word's height. The lane without any word decides what must stay: the same pills shown in the
// same mode. A raised word (above the glyph / over the label) also needs its place above its pill free: inside the
// screen's 0.05 H margin, clear of the keep-outs (`left`) and of the other pills.
template <class LeftFn, class LowFn>
inline HoldPlan LaneHold(float W, float H, int n, int confirm, const float* baseN, const float* baseF, const float* glyph,
                         const float* label, const bool* hold, const HoldDims& d, float ph, float pitch, float wordH,
                         float nnBottom, LeftFn left, LowFn lowest, LanePlaced& out, int& dropped, float* wN, float* wF)
{
    n = n < 0 ? 0 : (n > kLaneMax ? kLaneMax : n);
    int dropNone = 0;
    LanePlaced none;
    LaneLayout(W, H, n, confirm, baseN, baseF, ph, pitch, nnBottom, left, lowest, none, dropNone);
    auto widths = [&](const HoldPlan& p)
    {
        for (int i = 0; i < n; ++i)
        {
            const float e = HoldExtra(p.at[i], glyph[i], label[i], d);
            wN[i] = baseN[i] + e, wF[i] = baseF[i] + e;
        }
    };
    auto run = [&](const HoldPlan& p, LanePlaced& o, int& dr)
    {
        widths(p);
        dr = 0;
        LaneLayout(W, H, n, confirm, wN, wF, ph, pitch, nnBottom, left, lowest, o, dr);
    };
    const HoldPlan plan = ChooseHold(n, hold, d, [&](const HoldPlan& p)
                                     {
                                         LanePlaced o;
                                         int dr = 0;
                                         run(p, o, dr);
                                         if (o.mode != none.mode)
                                             return false;
                                         for (int i = 0; i < n; ++i)
                                             if (o.use[i] != none.use[i])
                                                 return false;
                                         for (int i = 0; i < n; ++i) // a raised word's place above its pill
                                             if (o.use[i] && Raised(p.at[i]))
                                             {
                                                 const float y1 = o.cy[i] - ph * 0.5f - H * 0.001f, y0 = y1 - wordH;
                                                 const float x0 = o.x0[i], x1 = o.x0[i] + o.w[i];
                                                 if (y0 < H * 0.05f || left(y0, y1, x1) > x0 + 0.5f)
                                                     return false;
                                                 for (int j = 0; j < n; ++j)
                                                     if (j != i && o.use[j] && o.x0[j] < x1 && o.x0[j] + o.w[j] > x0 &&
                                                         o.cy[j] - ph * 0.5f < y1 && o.cy[j] + ph * 0.5f > y0)
                                                         return false;
                                             }
                                         return true;
                                     });
    run(plan, out, dropped);
    return plan;
}
} // namespace promptfit
