// FLvibed - the pure geometry of the pad's prompt rows (hold word): factored out of
// modules\controller\src\impl\promptbar.cpp (the bottom lane: modules\hud\docs\UI_HUD2_EXTENDED.md 13) and hud2.cpp ComputeLayout (HUD 2's
// context line: modules\hud\docs\UI_HUD2.md 5.9) without changing their behaviour, so the host-free tests (tests\promptfit_test.cpp)
// drive the very code the game runs across the display matrix. No game, no device: the callers pass the measured widths
// and their keep-out queries. Pixels in, pixels out; every size a fraction of H.
#pragma once
#include "game/deckmode.h"

#include <algorithm>
#include <string>
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

// The pills hold still (GitHub issue 10). Traced frame by frame at 1280x800: the buy button hides for two frames on every
// buy, so a pending prompt bar blinked; while a window slides in, its moving plates sent the lane row -> top -> column ->
// top -> row -> column inside 120 frames. Whatever the cause (the subtitles' block, a plate, a window's slide), the rule is
// the same one, on the clock and not the frame count, so it holds at any frame rate:
//  - shown: a wish to show lasts kPillHoldMs past its end, so a gap shorter than that never reaches the screen;
//  - placed: a new lane replaces the one in use once it stood kLaneSettleMs, or after kLaneMaxWaitMs of changes without
//    rest (a place that never settles still wins: never stuck on a stale lane). Other pills (a new screen, other labels) or
//    another screen size are a new layout, taken at once.
constexpr unsigned kPillHoldMs = 200, kLaneSettleMs = 120, kLaneMaxWaitMs = 400;

struct LaneKey
{
    LaneMode mode = LaneMode::None;
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    bool SamePlace(const LaneKey& o) const { return mode == o.mode && x0 == o.x0 && y0 == o.y0 && x1 == o.x1 && y1 == o.y1; }
};

class PillSteady
{
public:
    bool Shown(bool want, unsigned long long nowMs)
    {
        if (want)
            wanted_ = true, lastWant_ = nowMs;
        return wanted_ && nowMs - lastWant_ < kPillHoldMs;
    }
    // true: `cand` is the lane to use this frame; false: keep the one in use. `layout` = what the pills are (their widths,
    // the screen): a change of it is taken at once.
    bool Accept(const LaneKey& cand, unsigned long long layout, unsigned long long nowMs)
    {
        if (!have_ || layout != layout_ || held_.mode == LaneMode::None)
        {
            Take(cand, layout);
            return true;
        }
        if (cand.SamePlace(held_))
        {
            waiting_ = false;
            return true;
        }
        if (!waiting_)
            waiting_ = true, firstAt_ = nowMs, next_ = cand, nextAt_ = nowMs;
        else if (!cand.SamePlace(next_))
            next_ = cand, nextAt_ = nowMs;
        if (nowMs - nextAt_ >= kLaneSettleMs || nowMs - firstAt_ >= kLaneMaxWaitMs)
        {
            Take(cand, layout);
            return true;
        }
        return false;
    }
    void NewLane() { have_ = false, waiting_ = false; } // nothing in use (the pills were gone): the next lane is taken at once

private:
    void Take(const LaneKey& k, unsigned long long layout) { have_ = true, waiting_ = false, held_ = k, layout_ = layout; }
    bool wanted_ = false, have_ = false, waiting_ = false;
    unsigned long long lastWant_ = 0, firstAt_ = 0, nextAt_ = 0, layout_ = 0;
    LaneKey held_, next_;
};

// The pills' places: wN = each pill's width with its label at the nominal size, wF with the label at the fit rule's floor
// (equal to wN where the label already is at or under it), ph the pill's height, pitch the gap between pills in a row
// (pixels); `confirm` = the pill that always stays (the A pill, else the first; BACK = the last stays too). `left(y0, y1,
// right)` = the row's left limit in that band (the screen's keep-outs), `nnBottom` = the NeuroNet box's bottom when
// docked (0 = none), `lowest(x0, x1, below)` = the lowest keep-out edge in that x range. `dropped` counts the pills
// dropped. The order (modules\hud\docs\UI_HUD2_EXTENDED.md 13; fix-text 25 Oct 2026): the row; the row with the labels at the floor;
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
    const float right = W - W * deckmode::SideW();
    const float by0 = H * deckmode::LaneTopH(), by1 = H * deckmode::LaneBottomH(), cy = (by0 + by1) * 0.5f;
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
    while (span(0, n) > right - W * deckmode::SideW() && dropOne())
        ;
    const float rowW = span(0, n), gapV = H * 0.010f;
    const float low = lowest(right - rowW, right, by0);
    float y = cy;
    if (low + gapV + ph <= by1)
        y = std::max(cy, low + gapV + ph * 0.5f);
    else
        for (float yc = by1 - ph * 0.5f; yc - ph * 0.5f >= H * deckmode::EdgeH(); yc -= H * 0.005f)
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

// The line under the ship (modules\hud\docs\UI_HUD2.md 5.9 "one baseline"): at 0.915 H; the last prompts give way until the line fits
// between the ship panel (right edge shipX1) and the loadout (left edge loX0); only a line whose first prompt alone does
// not fit goes above the panels (their top panelsY0, 0.04 H x k above it). `w` = each prompt's width (nullptr = the line
// measures 0: the player is typing), `gap` the space between prompts. `lineY` = the baseline in H (HUD 2's layout core
// passes its preset's context y: UI_SPEC.json hud2_presets, Standard 0.915).
inline ContextPlaced PlaceContext(const float* w, int n, float gap, float W, float H, float k, float shipX1, float loX0,
                                  float panelsY0, float lineY = 0.915f)
{
    ContextPlaced c;
    c.n = n;
    c.y = lineY * H;
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
// The owner (follow-up): HOLD BEFORE the glyph (the word, then the glyph with its broken ring, then the
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
// + its line), then a clear 0.2 h (0.2 h alone let the ring touch the word's D: in game at 4K).
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
                                    float panelsY0, float lineY = 0.915f)
{
    ContextHold r;
    n = n < 0 ? 0 : (n > 3 ? 3 : n);
    const ContextPlaced none = PlaceContext(base, n, gap, W, H, k, shipX1, loX0, panelsY0, lineY);
    auto widths = [&](const HoldPlan& p, float* w)
    {
        for (int i = 0; i < n; ++i)
            w[i] = base[i] + HoldExtra(p.at[i], glyph[i], label[i], d);
    };
    r.plan = ChooseHold(n, hold, d, [&](const HoldPlan& p)
                        {
                            float w[3] = {};
                            widths(p, w);
                            const ContextPlaced c = PlaceContext(w, n, gap, W, H, k, shipX1, loX0, panelsY0, lineY);
                            return c.n == none.n && c.above == none.above;
                        });
    widths(r.plan, r.w);
    r.c = PlaceContext(r.w, n, gap, W, H, k, shipX1, loX0, panelsY0, lineY);
    return r;
}

// ---------------------------------------------------------------- HUD 2's keycap (the keyboard form's prompt)
// hud2kit::Keycap's measure (its regular, non-tight cap), pure so the layout tests size a K&M prompt by the same numbers
// the HUD draws: the key's name never under the 0.024 H text floor - names over 2 characters at 0.42 h, else 0.5 h - the
// cap grows around it (its height h, or the name / 0.42 (/ 0.5) x 0.8), the width the name's (at KeycapNameSize, track
// 0.04) + half the cap's height, never under the height. `len` the name's characters, `H` the screen height.
inline float KeycapNameSize(size_t len, float h, float H)
{
    const float a = len > 2 ? h * 0.42f : h * 0.5f, b = 0.024f * H;
    return a > b ? a : b;
}
inline float KeycapHeight(size_t len, float h, float H)
{
    const float g = KeycapNameSize(len, h, H) * (len > 2 ? 1.0f / 0.42f : 2.0f) * 0.8f;
    return h > g ? h : g;
}
inline float KeycapWidth(float nameW, size_t len, float h, float H)
{
    const float hc = KeycapHeight(len, h, H), w = nameW + hc * 0.5f;
    return w > hc ? w : hc;
}
// The tight cap (hud2kit::Keycap(..., tight): the dense panels' and, since UI_HUD2.md 5.22, the context prompts'): the
// name at 0.62 h, never under the legibility floor `minPx` (textfit::MinSize), the cap 1.2 x the name, its width the
// name's (track 0.04) + 0.3 x the cap, never under the cap.
inline float KeycapTightNameSize(float h, float minPx)
{
    const float a = h * 0.62f;
    return a > minPx ? a : minPx;
}
inline float KeycapTightWidth(float nameW, float ts)
{
    const float hc = ts * 1.2f, w = nameW + hc * 0.3f;
    return w > hc ? w : hc;
}

// ---------------------------------------------------------------- a prompt's label: never a marquee (0.2.1)
// The owner: a button prompt's label is always readable whole - a label too long for its room wraps onto two
// lines in a somewhat smaller font (never under the legibility floor) instead of scrolling (panel names keep their
// marquee). The order: (1) one line at the nominal size; (2) one line shrunk, down to the two-line size; (3) two lines
// split at the word boundary that makes the wider line narrowest, at the two-line size (0.85 x nominal, never under the
// floor), shrunk to the floor where needed; (4) the last resort: at the floor, the line(s) cut with the ellipsis.
// `width(text, size)` = the caller's measure (linear in size: hud2kit::TextWidth); `floor` = textfit::MinSize.
constexpr float kWrapScale = 0.85f; // the two-line size against the nominal one
constexpr float kWrapTall = 0.008f; // a two-line prompt's plate grows upward by this (H x k): 0.060 instead of 0.052 tall
constexpr float kWrapPitch = 1.0f;  // the two lines' centres this many ems apart (their text boxes touch: the lint's
                                    // panel.overlap minimum; 0.95 overlapped by 2.6 px at 4K)
struct LabelFit
{
    int lines = 1;         // 1 or 2
    float size = 0;        // every line's text size
    std::wstring line[2];  // the line(s); a cut one ends in the ellipsis (textfit::kEllipsis)
    float width = 0;       // the widest line at `size`
    bool cut = false;
};
inline float WrapSize(float nominal, float floor)
{
    const float z = nominal * kWrapScale, f = floor < nominal ? floor : nominal;
    return z > f ? z : f;
}
// The best two-line split: the space whose halves' wider one is narrowest at `size` (false: no space - one word).
template <class WidthFn>
inline bool SplitLabel(const std::wstring& s, float size, WidthFn width, std::wstring& a, std::wstring& b, float& wide)
{
    bool any = false;
    for (size_t sp = s.find(L' '); sp != std::wstring::npos; sp = s.find(L' ', sp + 1))
    {
        size_t e = sp, st = sp;
        while (e > 0 && s[e - 1] == L' ')
            --e;
        while (st < s.size() && s[st] == L' ')
            ++st;
        if (e == 0 || st >= s.size())
            continue;
        const std::wstring x = s.substr(0, e), y = s.substr(st);
        const float wx = width(x, size), wy = width(y, size), m = wx > wy ? wx : wy;
        if (!any || m < wide)
            a = x, b = y, wide = m, any = true;
    }
    return any;
}
// The longest prefix + the ellipsis that fits `room` at `size` (trailing spaces dropped; "" when not even the ellipsis).
template <class WidthFn>
inline std::wstring CutLabel(const std::wstring& s, float size, float room, WidthFn width)
{
    if (width(s, size) <= room + 0.5f)
        return s;
    const std::wstring ell(1, static_cast<wchar_t>(0x2026));
    size_t best = 0;
    for (size_t k = 1; k < s.size(); ++k)
    {
        if (width(s.substr(0, k) + ell, size) > room + 0.5f)
            break;
        best = k;
    }
    while (best > 0 && s[best - 1] == L' ')
        --best;
    if (best == 0)
        return width(ell, size) <= room + 0.5f ? ell : std::wstring();
    return s.substr(0, best) + ell;
}
template <class WidthFn>
inline LabelFit FitLabel(const std::wstring& s, float nominal, float floor, float room, WidthFn width)
{
    LabelFit r;
    r.size = nominal, r.line[0] = s;
    if (s.empty() || nominal <= 0)
        return r;
    const float lo = floor < nominal ? floor : nominal; // never larger than asked
    const float w1 = width(s, nominal), z2 = WrapSize(nominal, lo);
    r.width = w1;
    if (w1 <= room + 0.5f) // (1)
        return r;
    const float z1 = room / (w1 / nominal) * 0.999f; // (2) one line shrunk (the measure is linear in size)
    if (z1 >= z2)
    {
        r.size = z1, r.width = width(s, z1);
        return r;
    }
    std::wstring a, b;
    float wide = 0;
    if (SplitLabel(s, z2, width, a, b, wide)) // (3) two lines
    {
        float z = z2;
        if (wide > room + 0.5f)
            z = z2 * room / wide * 0.999f;
        if (z >= lo)
        {
            if (z != z2)
                SplitLabel(s, z, width, a, b, wide);
            r.lines = 2, r.size = z, r.line[0] = a, r.line[1] = b, r.width = wide;
            return r;
        }
        // (4) at the floor, two lines cut
        SplitLabel(s, lo, width, a, b, wide);
        r.lines = 2, r.size = lo, r.cut = true;
        r.line[0] = CutLabel(a, lo, room, width), r.line[1] = CutLabel(b, lo, room, width);
        const float wa = width(r.line[0], lo), wb = width(r.line[1], lo);
        r.width = wa > wb ? wa : wb;
        return r;
    }
    // (4) one word: one line at the floor, cut
    r.size = lo, r.line[0] = CutLabel(s, lo, room, width), r.cut = r.line[0] != s, r.width = width(r.line[0], lo);
    return r;
}
// The narrowest room a label reads whole in: its best two-line split's wider line at the floor (one word: its width at
// the floor) - a lane squeezes a prompt no further (PlaceContextLanes' minW).
template <class WidthFn>
inline float LabelMinWidth(const std::wstring& s, float nominal, float floor, WidthFn width)
{
    const float lo = floor < nominal ? floor : nominal;
    std::wstring a, b;
    float wide = 0;
    if (SplitLabel(s, lo, width, a, b, wide))
        return wide;
    return width(s, lo);
}

// ---------------------------------------------------------------- HUD 2's context lanes
// The owner's decision 4 (modules\hud\docs\UI_HUD2.md 5.20): the prompts tight to the bottom-left and the bottom-right, the very centre
// bottom clear "for aesthetic purposes" (the centred line's cruise / dock / context pills hid the ship). Two lanes beside
// the bottom panels: the LEFT lane from the ship panel's right edge towards the centre, the RIGHT lane from the loadout's
// left edge towards the centre, neither past the bottom-centre clear band (x 0.40-0.60 W: the ship keep-out's columns,
// UI_HUD2_PRESETS.md 1.3 - the lint's zone "bottom"). Each lane has two rows: the baseline (the preset's context y) and
// one above it. Which lane a prompt takes is the caller's (HUD 2: by the corners' jobs, PRESETS 1.1 - your ship's
// movement left, what you fire / use / target right). The order is the priority: a lane's first prompt hugs its panel.
struct LaneGeom
{
    float lx0 = 0, lx1 = 0; // the left lane's content range (pixels): the prompts' left edges from lx0, ends <= lx1
    float rx0 = 0, rx1 = 0; // the right lane's: the prompts' right edges from rx1 leftwards, starts >= rx0
    float y0 = 0;           // the baseline row's centre line
    float pitch = 0;        // the rows' pitch (the row above at y0 - pitch)
    int rows = 2;
    float tall = 0;         // what a wrapped prompt's plate grows upward (two label lines: FitLabel, 0.2.1) - the row
                            // above moves up by it where its lane's baseline row holds a wrapped prompt
};
// The lanes for a frame: `lineY` the baseline (H), `shipX1` / `loX0` the panels' facing edges, the band x0 / x1 (W), the
// pill's plate pad 0.012 H x k and 0.012 H x k between a plate and its panel (HUD 2's gap), a plate may reach the band's
// edge (the lint's zone tolerates an edge); rows 0.06 H x k apart (the 0.052 pill + 0.008).
inline LaneGeom ContextLanes(float W, float H, float k, float shipX1, float loX0, float lineY = 0.915f,
                             float bandX0W = 0.40f, float bandX1W = 0.60f)
{
    LaneGeom g;
    const float pad = 0.012f * H * k, panelGap = 0.012f * H * k;
    g.lx0 = shipX1 + panelGap + pad, g.lx1 = bandX0W * W - pad;
    g.rx1 = loX0 - panelGap - pad, g.rx0 = bandX1W * W + pad;
    g.y0 = lineY * H, g.pitch = 0.06f * H * k, g.rows = 2, g.tall = kWrapTall * H * k;
    return g;
}
// The lanes above the bottom panels: where the beside-the-panels lanes have no room for even the first prompt (4:3 / 5:4
// at large scales: the panels reach the band). The left from the side margin to the band's 0.40 W, the right from the
// band's 0.60 W to the other margin (the same pads), the baseline 0.04 H x k above the panels' top `panelsTop` (pixels),
// two rows. Replaces the old centred line above the panels, which crossed the lower centre (the
// lower centre devoid of HUD elements; UI_HUD2.md 5.23).
inline LaneGeom LanesAbove(float W, float H, float k, float mx, float panelsTop, float bandX0W = 0.40f,
                           float bandX1W = 0.60f)
{
    LaneGeom g;
    const float pad = 0.012f * H * k;
    g.lx0 = mx + pad, g.lx1 = bandX0W * W - pad;
    g.rx1 = W - mx - pad, g.rx0 = bandX1W * W + pad;
    g.y0 = panelsTop - 0.04f * H * k, g.pitch = 0.06f * H * k, g.rows = 2, g.tall = kWrapTall * H * k;
    return g;
}
struct LanesPlaced
{
    int n = 0;                             // the prompts kept (a prefix: the last give way first)
    float x[3] = {}, y[3] = {}, w[3] = {}; // each kept prompt's left edge, centre line and width as placed
    float asked[3] = {};                   // ... and as asked (w < asked: squeezed, its label scrolls: TextMarquee)
    int side[3] = {}, row[3] = {};         // 0 the left lane, 1 the right; the row (0 the baseline, 1 above it)
    bool moved[3] = {};                    // placed in the other lane (its own was full)
    bool wrap[3] = {};                     // squeezed: its label may take two lines (FitLabel) - its plate g.tall taller
    bool none = false;                     // prompts asked but not even the first one had a place (both lanes closed)
    float x0 = 0, x1 = 0;                  // the kept prompts' extent with the line's 0.015 H x k margin
};
// Places n prompts (w = their widths, nullptr = the line measures 0: the player types; minW = the narrowest each may be
// squeezed to, nullptr = no squeeze; side = 0 left / 1 right, nullptr = all left) with `gap` between two in a row:
// for each in order, its own lane - the first row it fits whole, else an empty row it fits squeezed (>= minW, its label
// then scrolls) - else the other lane the same way; one that finds no place ends the line there.
inline LanesPlaced PlaceLanes(const float* w, const float* minW, const int* side, int n, float gap, const LaneGeom& g,
                              float H, float k)
{
    LanesPlaced o;
    n = n < 0 ? 0 : (n > 3 ? 3 : n);
    const int rows = g.rows < 1 ? 1 : (g.rows > 2 ? 2 : g.rows);
    float cur[2][2] = {{g.lx0, g.lx0}, {g.rx1, g.rx1}}; // the left rows' next left edge, the right rows' next right edge
    bool used[2][2] = {}, tall[2] = {};
    for (int i = 0; i < n; ++i)
    {
        const float wi = w ? w[i] : 0.0f, mi = minW ? std::min(minW[i], wi) : wi;
        const int own = side && side[i] == 1 ? 1 : 0;
        bool placed = false;
        for (int pass = 0; pass < 2 && !placed; ++pass)
        {
            const int s = pass == 0 ? own : 1 - own;
            auto room = [&](int r)
            {
                const float g0 = used[s][r] ? gap : 0.0f;
                return s == 0 ? g.lx1 - (cur[s][r] + g0) : (cur[s][r] - g0) - g.rx0;
            };
            auto put = [&](int r, float width)
            {
                const float g0 = used[s][r] ? gap : 0.0f;
                if (s == 0)
                    o.x[i] = cur[s][r] + g0, cur[s][r] = o.x[i] + width;
                else
                    o.x[i] = cur[s][r] - g0 - width, cur[s][r] = o.x[i];
                o.y[i] = g.y0 - r * (g.pitch + (tall[s] ? g.tall : 0.0f)), o.w[i] = width, o.asked[i] = wi;
                o.side[i] = s, o.row[i] = r, o.wrap[i] = width < wi - 0.5f;
                if (r == 0 && o.wrap[i])
                    tall[s] = true; // (a wrapped prompt is alone in its row: the row above is still empty here)
                o.moved[i] = s != own, used[s][r] = true, placed = true;
            };
            for (int r = 0; r < rows && !placed; ++r) // whole
                if (wi <= room(r) + 0.5f)
                    put(r, wi);
            for (int r = 0; r < rows && !placed; ++r) // squeezed, alone in its row
                if (!used[s][r] && room(r) >= mi - 0.5f && room(r) > 0)
                    put(r, std::min(wi, room(r)));
        }
        if (!placed)
            break;
        o.n = i + 1;
    }
    o.none = n > 0 && o.n == 0;
    const float m = 0.015f * H * k;
    for (int i = 0; i < o.n; ++i)
    {
        const float a = o.x[i] - m, b = o.x[i] + o.w[i] + m;
        o.x0 = i ? std::min(o.x0, a) : a, o.x1 = i ? std::max(o.x1, b) : b;
    }
    return o;
}
// The lanes with the hold cascade (as PlaceContextHold): the lanes without any word decide what must stay - the same
// prompts in the same lanes and rows, none squeezed more than it was - and a word raised over its prompt (above the
// glyph, over the label) needs its place free of the other prompts' plates (the row above). wordH = the word's height.
struct LanesHold
{
    LanesPlaced c;
    HoldPlan plan;
    float w[3] = {};
};
inline LanesHold PlaceLanesHold(const float* base, const float* minW, const float* glyph, const float* label,
                                const bool* hold, const int* side, int n, const HoldDims& d, float gap,
                                const LaneGeom& g, float H, float k, float wordH)
{
    LanesHold r;
    n = n < 0 ? 0 : (n > 3 ? 3 : n);
    const LanesPlaced none = PlaceLanes(base, minW, side, n, gap, g, H, k);
    auto widths = [&](const HoldPlan& p, float* w, float* mw)
    {
        for (int i = 0; i < n; ++i)
        {
            const float e = HoldExtra(p.at[i], glyph[i], label[i], d);
            w[i] = base[i] + e, mw[i] = (minW ? minW[i] : base[i]) + e;
        }
    };
    const float pad = 0.012f * H * k, half = 0.026f * H * k;
    r.plan = ChooseHold(n, hold, d, [&](const HoldPlan& p)
                        {
                            float w[3] = {}, mw[3] = {};
                            widths(p, w, mw);
                            const LanesPlaced c = PlaceLanes(w, mw, side, n, gap, g, H, k);
                            if (c.n != none.n)
                                return false;
                            for (int i = 0; i < c.n; ++i)
                            {
                                if (c.side[i] != none.side[i] || c.row[i] != none.row[i] ||
                                    c.asked[i] - c.w[i] > none.asked[i] - none.w[i] + 0.5f)
                                    return false;
                                if (!Raised(p.at[i]))
                                    continue;
                                const float y1 = c.y[i] - half - (c.wrap[i] ? g.tall : 0.0f) - 0.001f * H,
                                            y0 = y1 - wordH;
                                const float x0 = c.x[i] - pad, x1 = c.x[i] + c.w[i] + pad;
                                for (int j = 0; j < c.n; ++j)
                                    if (j != i && c.x[j] - pad < x1 && c.x[j] + c.w[j] + pad > x0 &&
                                        c.y[j] - half - (c.wrap[j] ? g.tall : 0.0f) < y1 && c.y[j] + half > y0)
                                        return false;
                            }
                            return true;
                        });
    float mw[3] = {};
    widths(r.plan, r.w, mw);
    r.c = PlaceLanes(r.w, mw, side, n, gap, g, H, k);
    for (int i = 0; i < r.c.n; ++i) // (each prompt's width as placed: a squeezed one draws in its room)
        r.w[i] = r.c.w[i];
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
                                                 if (y0 < H * deckmode::EdgeH() || left(y0, y1, x1) > x0 + 0.5f)
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
