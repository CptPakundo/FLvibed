// HUD 2's layout lint (modules\hud\docs\UI_HUD2_PRESETS.md 6 step 3; docs\research\IDEAS.md U35 / T8): one frame's drawn items,
// the zones, the bindings' reach, the analog controls and the latched toggles in; findings (FAIL / WARN, rule id, the
// items, a short reason) and a PASS / WARN / FAIL verdict out. Pure: no game call, no globals, no system headers (core\pre.h
// / common.h), so tests\hud2_lint_test.cpp runs the same code as the game. The impure adapter (the kit's recording of the
// last drawn pass, the model, the layout, the bindings) is hud2_lint_live.cpp (debug `hud2 lint`).
//
// The rules are the doc's item 3 as written: FAIL - a plate inside a zone; text under the floor; centre-stack overlaps;
// one value id visible twice (except declared second channels); a bound action with no reachable prompt / keycap /
// wheel entry, or a wheel action with no wheel bind; an analog control in use without its readout; a latched toggle
// without a state word. Plus (UI_HUD2_PRESETS.md 7 A1 / B4-B7) panel.overlap: two drawn items of
// one panel that overlap or sit closer than 0.01 H (PanelOverlap below). WARN - budgets, prompt counts, the ship
// keep-out, and title-safe 90 % (U35's rule 1; the presets doc names no severity for it, so WARN). Contrast is not here
// (it needs frames): the named hook below.
// Every number carries its source. Units: pixels of the back buffer, H = its height.
//
// The numbers are the presets doc's DATA (6 step 1): docs\UI_SPEC.json "hud2_presets" -> core\ui_meta.h uimeta::hud2p
// (tools\ui_meta.py), each with its section there; nothing here copies one. The aliases below keep the lint's names.
#pragma once
#include "common.h"
#include "game/textfit.h"
#include "ui_meta.h"

namespace hud2lint
{
namespace hp = uimeta::hud2p;
// ---- the numbers (all from ui_meta.h; their sources there and in UI_SPEC.json)
// Text floor: "floor 0.024 H / 14 px [C]" (1.2 Text) - the same floor core\game\textfit.h MinSize clamps every HUD 2 text
// to (kMinPx 14). The kit's text size is its em (hud2_kit.h Text), taken as the body height.
constexpr float kTextFloorH = hp::kTextFloor;
constexpr float kTextFloorPx = hp::kTextFloorPx;
// The centre stack's gap (6 step 3 "centre-stack overlaps"; the 0.01 H: controller_ui.md "HUD rules" 8 / U35).
constexpr float kGapH = hp::kCentreGap;
// The gap between separate elements inside one panel (panel.overlap): the same rule's "no overlap between HUD boxes, a
// minimum gap (proposed 0.01 H)" (docs\research\controller_ui.md "HUD rules" 8 / IDEAS U35); the owner, 31 Oct 2026
// (UI_HUD2_PRESETS.md 7, A1 / B4-B7): "it's not fine for the glyphs to overlap", "HUD elements inside cards [must] not
// overlap, neither glyphs nor contextual data".
constexpr float kPanelGapH = hp::kCentreGap;
// The boxed area's cap where a preset names none for the context (controller_ui.md "HUD rules" 8 / U35, ~10 %).
constexpr float kBudget = hp::kBudgetDefault;
// Title-safe 90 % (6 step 3: WARN; controller_ui.md "HUD rules" 1 / U35).
constexpr float kTitleSafe = hp::kTitleSafe;
// The ship keep-out for new text (1.3, WARN).
constexpr float kKeepX0 = hp::kKeepX0W, kKeepX1 = hp::kKeepX1W, kKeepY0 = hp::kKeepY0, kKeepY1 = hp::kKeepY1;
// The centre core (1.3 "Inside r 0.045 only native markers").
constexpr float kCoreR = hp::kCoreR;
// The lower centre's clear zone (decision 4 - "the very centre bottom clear for aesthetic
// purposes", UI_HUD2.md 5.20; then 5.23 - "The CRUISE ON text still hides the ship. We should really make sure the lower
// center of the screen is devoid of HUD elements (unless they're diegetic like an on-screen ship/solar being a target,
// etc)"): x 0.40-0.60 W = the ship keep-out's columns (1.3 "Ship keep-out for new text: x 0.40-0.60 W, y 0.63-0.92"; the
// ship starts at 0.667-0.685 H), from the drawn ring's lowest edge + the centre stack's 0.01 H (hud2_layout.h
// Out::clearY0: the caller's `clearY0`) to the screen's bottom - 0.85 H until 5.23, which let the reticle lane's CRUISE ON
// at 0.692 H pass. Unlike a sightline zone it FAILs ANY recorded HUD 2 item in it (Zone::texts): plates, texts, glyphs,
// keycaps, icons, arcs and shapes - only the transient overlays (Item::overlay: the 0.3 s hit flash and the missile
// chevrons at the threat's bearing, 1.3) are let through. Diegetic marks (the game's brackets, its reticle) are the game's
// own draws: the kit never records them, so they never reach this zone. Every preset; the context lanes stop at its columns.
constexpr float kBottomX0W = hp::kKeepX0W, kBottomX1W = hp::kKeepX1W;
constexpr const char* kBottomZone = "bottom";
// The zone's top when the caller has no layout (clearY0 <= 0): the preset's ring circle at scale 1 + the gap - stricter
// than any drawn ring (Standard's throttle arc lies below its circle), so a missing layout fails closed.
inline float FallbackClearY0(const hp::Preset& p, float H) { return H * (0.5f + p.reticleR + kGapH); }
// hud_preset 0 = Standard (the generator keeps the presets in hud_preset order: tools\ui_meta.py HUD2_PRESET_IDS).
static_assert(hp::kPresetCount == 4 && hp::kPresets[0].index == 0 && hp::kPresets[3].index == 3,
              "uimeta::hud2p::kPresets in hud_preset order");
inline const hp::Preset& Standard() { return hp::kPresets[0]; }

// A preset's limits for one frame of a context: the boxed-area cap and the prompt caps.
struct Limits
{
    float budget = kBudget;
    int pills = -1, pairs = -1; // -1 = no cap
};
// The budget: the largest cap the preset names for the context on the device - one frame's lint does not know the doc's
// case (no target / target on screen = steady / off screen + alert = peak), so the most permissive one (Standard's
// combat: "peak 15.9 % (<= 16)", K&M "16.9 % (<= 17)", 2.1 Budgets); the keyboard-and-mouse rows when the preset gives
// any, else the pad's; none for the context = kBudget (Standard's calm names no cap). Prompts: "<= n pills" and "n
// pairs ... plus answers" per context (2.1 "Prompts and reveal"; the other presets' sections likewise).
inline Limits LimitsOf(const hp::Preset& p, bool combat, bool kbm)
{
    Limits l;
    const char* ctx = combat ? "combat" : "calm";
    for (int pass = kbm ? 0 : 1; pass < 2; ++pass)
    {
        float best = -1;
        for (int i = 0; i < p.budgetCount; ++i)
            if (strcmp(p.budgets[i].context, ctx) == 0 && strcmp(p.budgets[i].device, pass == 0 ? "kbm" : "pad") == 0)
                best = std::max(best, p.budgets[i].cap);
        if (best > 0)
        {
            l.budget = best;
            break;
        }
    }
    if (const hp::PromptCap* c = hp::Prompts(p, ctx))
        l.pills = c->pills, l.pairs = c->pairs;
    return l;
}
// This package's choice, not the doc's: a copy fading under 10 % alpha does not count as a second visible place (a
// handover morph, 1.2 / 1.7, crosses both places for a moment).
constexpr float kVisibleAlpha = 0.10f;
// Sub-pixel tolerance for the edges (a box ending exactly on a margin passes).
constexpr float kEdgePx = 0.5f;

// ---- the input
enum class Kind : u8
{
    Plate,
    Text,
    Glyph,
    Keycap,
    Arc,
    Icon,
    Shape, // a bar, a line, an outline
};
enum class Lane : u8
{
    None,
    Pill,   // a context-line pill (the teaching lane)
    Answer, // an answer (the alert's prompts): outside the caps ("plus answers")
};
struct Item
{
    Kind kind = Kind::Shape;
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0;       // its box
    float cx = 0, cy = 0, r = 0, a0 = 0, a1 = 0; // an arc: centre, radius at its centre line, degrees (0 = 3 o'clock,
    float w = 0;                                 // clockwise on screen), its width
    float textH = 0;     // text / a keycap's name: the body height (the kit's em)
    float alpha = 1;     // as drawn (0 = not drawn)
    bool plated = false; // text on a plate (the 1.2 "unplated >= 0.026" rule is [D], not an item-3 rule: not checked)
    bool centre = false; // part of the centre stack (the ring, its numbers, the speed, the reticle lane)
    // a transient event mark drawn ON TOP of the centre stack, not spaced from it: UI_HUD2_PRESETS.md 1.3 "Where chevrons
    // cross the reticle lane or the throttle word, the chevrons draw on top ... The hit flash draws above the throttle
    // arc" (the missile chevrons, the 0.3 s hit flash at any bearing). centre.overlap skips a pair with one (its "text
    // dims to 30 %" half is not checked here)
    bool overlay = false;
    bool secondChannel = false; // a declared second channel of a critical state (1.7: the FLARE mirror, the hull flash)
    bool fades = false; // gone from the steady picture: hud_fade takes it out in calm, it stands only while awake
    int unit = 0; // a designed unit (hud2kit::RecUnit: a prompt and its own label, a slot's icon and count): 0 none
    Lane lane = Lane::None;
    std::string comp;  // the component ("nav", "card", "ring.speed", ...)
    std::string value; // the value id it shows ("" none)
    std::string label; // for the findings (the text, the glyph's name)
};
struct Zone
{
    std::string name;
    bool circle = false;
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0; // a rectangle
    float cx = 0, cy = 0, r = 0;          // or a circle
    std::vector<std::string> exempt;      // components exempt by name (1.3 "Exemptions by name"; "x" covers "x.*")
    bool texts = false;                   // every recorded item inside it fails (the lower centre, 5.23: plates, texts,
                                          // glyphs, keycaps, icons, arcs, shapes - overlays apart), not only a plate
};
struct Action
{
    std::string name;
    bool prompt = false, keycap = false, wheelEntry = false; // how it is reachable
};
struct Analog
{
    std::string name;
    bool inUse = false, readout = false;
};
struct Toggle
{
    std::string name;
    bool latched = false, stateWord = false;
};
struct Input
{
    float W = 0, H = 0, scale = 1;
    std::vector<Item> items;
    std::vector<Zone> zones;
    std::vector<Action> actions;
    bool wheelBound = true; // the action wheel has a binding (ACE has none: UI_HUD2_PRESETS.md 4)
    std::vector<Analog> analogs;
    std::vector<Toggle> toggles;
    float budget = kBudget;   // boxed area cap, fraction of the screen
    float dlgX0 = 0, dlgY0 = 0, dlgX1 = 0, dlgY1 = 0; // the subtitles' dialogue panel (pixels), 0 none
    int pillCap = LimitsOf(Standard(), false, false).pills; // the caps (-1 none): LimitsOf(preset, combat, kbm)
    int pairCap = LimitsOf(Standard(), false, false).pairs;
    bool keepOut = true, titleSafe = true;
    bool air = false; // panel.air: a measure of the live HUD, off for the fixtures
    // Rules the caller cannot feed: {rule id, why}; the lint skips them and prints "n/a: <rule>: <why>". An id that names
    // no rule (e.g. "analog.readout:steer") is only printed.
    std::vector<std::pair<std::string, std::string>> na;
};

// ---- the output
enum class Sev : u8
{
    Warn,
    Fail,
};
enum class Verdict : u8
{
    Pass,
    Warn,
    Fail,
};
struct Finding
{
    Sev sev = Sev::Fail;
    std::string rule, items, why;
};
struct Result
{
    std::vector<Finding> findings;
    std::vector<std::pair<std::string, std::string>> na;
    Verdict verdict = Verdict::Pass;
    float boxed = 0;  // the plates' union area / the screen's: the awake peak, everything that can stand at once
    float steady = 0; // ... without what hud_fade takes out: what stands on screen once the frame has settled
    int pills = 0, pairs = 0;
    int fails = 0, warns = 0;
};

// Contrast hook (U35 rule 5: 4.5:1 text, 3:1 large text, against the WORST background over frames; UI_HUD2_PRESETS.md 6
// "Verification"): NOT built - it needs frames (each text rectangle's pixels behind it), which this lint does not see.
// TODO(contrast): a per-text-item luminance input (WCAG relative luminance of the text colour and of the frame's darkest /
// brightest pixels in its box) and a rule "contrast" here; until then Lint() reports it n/a.
constexpr const char* kContrastNa = "needs frames (WCAG 4.5:1 / 3:1 against the worst background, U35 rule 5): not in this lint";

// ---- geometry
inline float Sq(float v) { return v * v; }
// The distance between two boxes (0 when they touch or overlap; negative = the overlap's smaller depth).
inline float BoxGap(float ax0, float ay0, float ax1, float ay1, float bx0, float by0, float bx1, float by1)
{
    const float sx = std::max(ax0 - bx1, bx0 - ax1), sy = std::max(ay0 - by1, by0 - ay1);
    if (sx > 0 && sy > 0)
        return sqrtf(sx * sx + sy * sy);
    return std::max(sx, sy);
}
inline float PointBoxDist(float px, float py, float x0, float y0, float x1, float y1)
{
    const float dx = std::max(std::max(x0 - px, 0.0f), px - x1), dy = std::max(std::max(y0 - py, 0.0f), py - y1);
    return sqrtf(dx * dx + dy * dy);
}
// The arc's centre line sampled every <= 2 degrees (both ends included).
inline int ArcSamples(const Item& a, float* xy, int max)
{
    const float lo = std::min(a.a0, a.a1), hi = std::max(a.a0, a.a1);
    int n = std::max(2, static_cast<int>(ceilf((hi - lo) / 2.0f)) + 1);
    n = std::min(n, max);
    constexpr float kDeg = 3.14159265f / 180.0f;
    for (int i = 0; i < n; ++i)
    {
        const float t = (lo + (hi - lo) * i / (n - 1)) * kDeg;
        xy[i * 2] = a.cx + a.r * cosf(t), xy[i * 2 + 1] = a.cy + a.r * sinf(t);
    }
    return n;
}
// An arc item's box from its band (the samples +- half its width).
inline void FitArcBox(Item& a)
{
    float p[182 * 2];
    const int n = ArcSamples(a, p, 182);
    a.x0 = a.y0 = 1e30f, a.x1 = a.y1 = -1e30f;
    for (int i = 0; i < n; ++i)
        a.x0 = std::min(a.x0, p[i * 2]), a.x1 = std::max(a.x1, p[i * 2]), a.y0 = std::min(a.y0, p[i * 2 + 1]),
        a.y1 = std::max(a.y1, p[i * 2 + 1]);
    a.x0 -= a.w * 0.5f, a.y0 -= a.w * 0.5f, a.x1 += a.w * 0.5f, a.y1 += a.w * 0.5f;
}
inline float PointSegDist(float px, float py, float ax, float ay, float bx, float by)
{
    const float vx = bx - ax, vy = by - ay, l2 = vx * vx + vy * vy;
    const float t = l2 > 0 ? std::max(0.0f, std::min(1.0f, ((px - ax) * vx + (py - ay) * vy) / l2)) : 0.0f;
    return sqrtf(Sq(px - ax - t * vx) + Sq(py - ay - t * vy));
}
// An arc's end face at `deg`: the radial segment across its band (x0, y0, x1, y1).
inline void EndFace(const Item& a, float deg, float* s)
{
    constexpr float kDeg = 3.14159265f / 180.0f;
    const float c = cosf(deg * kDeg), n = sinf(deg * kDeg), ri = a.r - a.w * 0.5f, ro = a.r + a.w * 0.5f;
    s[0] = a.cx + c * ri, s[1] = a.cy + n * ri, s[2] = a.cx + c * ro, s[3] = a.cy + n * ro;
}
// Two segments that do not cross (radial faces at different angles never do): the least of the four end-to-segment
// distances.
inline float FaceDist(const float* p, const float* q)
{
    return std::min(std::min(PointSegDist(p[0], p[1], q[0], q[1], q[2], q[3]), PointSegDist(p[2], p[3], q[0], q[1], q[2], q[3])),
                    std::min(PointSegDist(q[0], q[1], p[0], p[1], p[2], p[3]), PointSegDist(q[2], q[3], p[0], p[1], p[2], p[3])));
}
inline float Norm360(float d)
{
    d = fmodf(d, 360.0f);
    return d < 0 ? d + 360.0f : d;
}
inline bool Concentric(const Item& a, const Item& b)
{
    return fabsf(a.cx - b.cx) <= kEdgePx && fabsf(a.cy - b.cy) <= kEdgePx;
}
// Two arcs on one centre (the ring's arcs, the throttle arc, the hit flash) exactly: their bands are polar rectangles
// with RADIAL ends (hud2_kit Arc: quads from the inner to the outer radius), so with overlapping angle ranges the gap is
// the radial one, else the distance between the facing end faces (moving away from an end only widens the angle).
// The sampled form (ItemGap's fallback) gives the ends round caps half a width long: it read the ring's shield and
// charge arcs, 6 degrees apart (22 px at 4K), as 8.6 px apart (the baseline's cruise FAIL).
inline float ConcentricArcGap(const Item& a, const Item& b)
{
    const float sa = std::min(a.a0, a.a1), la = fabsf(a.a1 - a.a0), sb = std::min(b.a0, b.a1), lb = fabsf(b.a1 - b.a0);
    const float d = Norm360(sb - sa); // b's start from a's start, clockwise
    if (la >= 360 || lb >= 360 || d <= la || d + lb >= 360)
        return fabsf(a.r - b.r) - (a.w + b.w) * 0.5f;
    float fa[4], fb[4], ga[4], gb[4];
    EndFace(a, sa + la, fa), EndFace(b, sb, fb); // a's end to b's start
    EndFace(b, sb + lb, gb), EndFace(a, sa, ga); // b's end to a's start
    return std::min(FaceDist(fa, fb), FaceDist(gb, ga));
}
// The gap between two items' drawn shapes (arcs by their band, the rest by their box); negative = overlapping.
inline float ItemGap(const Item& a, const Item& b)
{
    constexpr int kMax = 182;
    float pa[kMax * 2], pb[kMax * 2];
    if (a.kind == Kind::Arc && b.kind == Kind::Arc && Concentric(a, b))
        return ConcentricArcGap(a, b);
    if (a.kind == Kind::Arc && b.kind == Kind::Arc)
    {
        const int na = ArcSamples(a, pa, kMax), nb = ArcSamples(b, pb, kMax);
        float best = 1e30f;
        for (int i = 0; i < na; ++i)
            for (int j = 0; j < nb; ++j)
                best = std::min(best, Sq(pa[i * 2] - pb[j * 2]) + Sq(pa[i * 2 + 1] - pb[j * 2 + 1]));
        return sqrtf(best) - a.w * 0.5f - b.w * 0.5f;
    }
    if (a.kind == Kind::Arc || b.kind == Kind::Arc)
    {
        const Item& arc = a.kind == Kind::Arc ? a : b;
        const Item& box = a.kind == Kind::Arc ? b : a;
        const int n = ArcSamples(arc, pa, kMax);
        float best = 1e30f;
        for (int i = 0; i < n; ++i)
            best = std::min(best, PointBoxDist(pa[i * 2], pa[i * 2 + 1], box.x0, box.y0, box.x1, box.y1));
        return best - arc.w * 0.5f;
    }
    return BoxGap(a.x0, a.y0, a.x1, a.y1, b.x0, b.y0, b.x1, b.y1);
}
// A plate inside a zone: a rectangle zone overlapping it by more than the edge tolerance on both axes, a circle zone
// reaching into it.
inline bool InZone(const Item& p, const Zone& z)
{
    if (z.circle)
        return PointBoxDist(z.cx, z.cy, p.x0, p.y0, p.x1, p.y1) < z.r;
    const float ox = std::min(p.x1, z.x1) - std::max(p.x0, z.x0), oy = std::min(p.y1, z.y1) - std::max(p.y0, z.y0);
    return ox > kEdgePx && oy > kEdgePx;
}
inline bool Exempt(const std::string& comp, const std::vector<std::string>& names)
{
    for (const std::string& e : names)
        if (comp == e || (comp.size() > e.size() && comp.compare(0, e.size(), e) == 0 && comp[e.size()] == '.'))
            return true;
    return false;
}
// The union of the rectangles' areas (clipped to the screen): coordinate compression, strip by strip.
inline float UnionArea(const std::vector<const Item*>& rs, float W, float H)
{
    std::vector<float> xs;
    for (const Item* r : rs)
        xs.push_back(Clampf(r->x0, 0, W)), xs.push_back(Clampf(r->x1, 0, W));
    std::sort(xs.begin(), xs.end());
    xs.erase(std::unique(xs.begin(), xs.end()), xs.end());
    float area = 0;
    std::vector<std::pair<float, float>> ys;
    for (size_t i = 0; i + 1 < xs.size(); ++i)
    {
        const float a = xs[i], b = xs[i + 1];
        if (b <= a)
            continue;
        ys.clear();
        for (const Item* r : rs)
            if (r->x0 <= a && r->x1 >= b)
            {
                const float y0 = Clampf(r->y0, 0, H), y1 = Clampf(r->y1, 0, H);
                if (y1 > y0)
                    ys.push_back({y0, y1});
            }
        std::sort(ys.begin(), ys.end());
        float cover = 0, s = 0, e = -1;
        for (const auto& y : ys)
        {
            if (y.first > e)
            {
                if (e > s)
                    cover += e - s;
                s = y.first, e = y.second;
            }
            else
                e = std::max(e, y.second);
        }
        if (e > s)
            cover += e - s;
        area += cover * (b - a);
    }
    return area;
}

// ---- the zones: a preset's zone rules (ui_meta.h, from UI_SPEC.json "hud2_presets") placed on the screen and the target
// card's column (cardX0 / cardX1: the layout's card edges). Standard (2.1 Zones): S1 the sightline field = the centre
// +- min(0.42, the card's left edge - 0.02 short), narrower than 16:9 0.01 short (1.8: "S1's half-width = the gap from the
// centre to the card's left edge - 0.01"; wider, the corners anchor to a centred 16:9 frame, so S1 keeps its 0.42); S2 the
// right rail from S1's right edge to the card's right edge, empty; the centre core r 0.045 (1.3). Every zone exempts
// 1.3's names (the alert card, a peek, the subtitles' panel); a zone's keyboard-and-mouse names (S2: the click-pinned
// contact list, "(S2 only)") when `kbm`.
// The consumables' cross stacked on the loadout's right end (consX0 / consY0 its
// plate's left / top edge, 0 = none) lies in the rail's lower end: the rail is then two rectangles - left of the cross
// as before, above the cross down to its top (the owner put it there; the rail stays empty above it). Every preset gets
// the lower centre's clear zone (kBottom*: decision 4, 5.23) from `clearY0` (pixels: the layout's Out::clearY0; <= 0 =
// FallbackClearY0) down, a zone that fails every recorded item, not only plates.
inline std::vector<Zone> ZonesOf(const hp::Preset& p, float W, float H, float cardX0, float cardX1, bool kbm,
                                 float consX0 = 0, float consY0 = 0, float clearY0 = 0)
{
    const float cx = W * 0.5f, cy = H * 0.5f;
    const bool narrow = W / H < 16.0f / 9.0f - 0.01f;
    std::vector<Zone> out;
    float prevX1 = cx;
    for (int i = 0; i < p.zoneCount; ++i)
    {
        const hp::Zone& d = p.zones[i];
        Zone z;
        z.name = d.name;
        for (int e = 0; e < hp::kZoneExemptCount; ++e)
            z.exempt.push_back(hp::kZoneExempt[e]);
        if (kbm)
            for (int e = 0; e < d.exemptKbmCount; ++e)
                z.exempt.push_back(d.exemptKbm[e]);
        switch (d.anchor)
        {
        case hp::Anchor::CentreToCard:
        {
            const float half =
                std::max(0.0f, std::min(d.halfMax * H, cardX0 - cx - (narrow ? d.cardGapNarrow : d.cardGap) * H));
            z.x0 = cx - half, z.x1 = cx + half, z.y0 = d.y0 * H, z.y1 = d.y1 * H;
            break;
        }
        case hp::Anchor::Rail:
            z.x0 = prevX1, z.x1 = std::max(prevX1, cardX1), z.y0 = d.y0 * H, z.y1 = d.y1 * H;
            if (consY0 > 0 && consX0 > z.x0 && consX0 < z.x1 && consY0 < z.y1)
            {
                Zone above = z; // over the cross: down to its top
                above.x0 = consX0, above.y1 = std::max(above.y0, consY0);
                z.x1 = consX0; // left of it: as before
                out.push_back(std::move(above));
            }
            else if (consY0 > 0 && consX0 <= z.x0 && consY0 < z.y1) // (the cross spans the whole rail)
                z.y1 = std::max(z.y0, consY0);
            break;
        case hp::Anchor::CentreOffset:
            z.x0 = cx + d.x0 * H, z.x1 = cx + d.x1 * H, z.y0 = cy + d.y0 * H, z.y1 = cy + d.y1 * H;
            break;
        case hp::Anchor::Centre:
            z.circle = true, z.cx = cx, z.cy = cy, z.r = d.r * H;
            break;
        }
        if (!z.circle)
            prevX1 = std::max(z.x1, d.anchor == hp::Anchor::Rail ? std::max(prevX1, cardX1) : z.x1);
        out.push_back(std::move(z));
    }
    Zone b; // the lower centre's clear zone (decision 4, 5.23)
    b.name = kBottomZone, b.texts = true;
    b.x0 = kBottomX0W * W, b.x1 = kBottomX1W * W, b.y0 = clearY0 > 0 ? clearY0 : FallbackClearY0(p, H), b.y1 = H;
    for (int e = 0; e < hp::kZoneExemptCount; ++e)
        b.exempt.push_back(hp::kZoneExempt[e]);
    out.push_back(std::move(b));
    return out;
}
inline std::vector<Zone> StandardZones(float W, float H, float cardX0, float cardX1, bool kbm, float clearY0 = 0)
{
    return ZonesOf(Standard(), W, H, cardX0, cardX1, kbm, 0, 0, clearY0);
}

// ---- the lint
inline bool Skipped(const Input& in, const char* rule)
{
    for (const auto& n : in.na)
        if (n.first == rule)
            return true;
    return false;
}
inline std::string Px(float v)
{
    char b[24];
    snprintf(b, sizeof b, "%.0f", v);
    return b;
}
inline std::string Who(const Item& it)
{
    std::string s = it.comp.empty() ? std::string("?") : it.comp;
    if (!it.label.empty())
        s += " '" + it.label + "'";
    return s + " [" + Px(it.x0) + "," + Px(it.y0) + "-" + Px(it.x1) + "," + Px(it.y1) + "]";
}
inline void Add(Result& r, Sev sev, const char* rule, std::string items, std::string why)
{
    Finding f;
    f.sev = sev, f.rule = rule, f.items = std::move(items), f.why = std::move(why);
    r.findings.push_back(std::move(f));
}
inline bool IsPrompt(const Item& it) { return it.kind == Kind::Glyph || it.kind == Kind::Keycap; }

// ---- panel.overlap (UI_HUD2_PRESETS.md 7 A1 / B4-B7; the gap kPanelGapH, U35 / controller_ui
// "HUD rules" 8). The part an item belongs to: its component's first segment ("card.icons" -> "card"; the threat list is
// the card's - Focus draws its rows inside the card's plate), each centre-stack component its own ("ring.band",
// "ring.cruise": centre.overlap spaces them from one another).
inline std::string PartOf(const std::string& comp)
{
    if (comp.compare(0, 5, "ring.") == 0)
        return comp;
    const std::string p = comp.substr(0, comp.find('.'));
    return p == "list" ? std::string("card") : p;
}
// The items the rule weighs: the ones that carry information - text, glyphs, keycaps, icons. Plates hold them, bars /
// lines / arcs are their tracks (the centre stack's arcs: centre.overlap).
inline bool PanelItem(const Item& it)
{
    return (it.kind == Kind::Text || it.kind == Kind::Glyph || it.kind == Kind::Keycap || it.kind == Kind::Icon) &&
           it.alpha >= kVisibleAlpha;
}
// The least gap two items of one part must keep (in px), how they are told apart:
//  - one designed unit (Item::unit, hud2kit::RecUnit - the part draws a prompt and its own label, a slot's icon and count
//    with its key, a chip's icon and word inside one scope; the design pairs them): they may touch, never overlap (0);
//  - two texts stacked in one column (their x ranges overlap): lines of one block - the line pitch spaces them, they
//    may touch, never overlap (0); U35's line spacing is its own rule (9), not this one;
//  - anything else (texts side by side, a glyph / keycap / icon beside or above anything not of its unit): separate
//    elements, kPanelGapH.
inline float PanelGapPx(const Item& a, const Item& b, float H)
{
    if (a.unit && a.unit == b.unit)
        return 0;
    if (a.kind == Kind::Text && b.kind == Kind::Text && std::min(a.x1, b.x1) - std::max(a.x0, b.x0) > kEdgePx)
        return 0;
    return kPanelGapH * H;
}

inline Result Lint(const Input& in)
{
    Result r;
    r.na = in.na;
    r.na.push_back({"contrast", kContrastNa});
    const float W = in.W, H = in.H;
    if (W <= 0 || H <= 0)
    {
        Add(r, Sev::Fail, "input", "-", "no screen size");
        r.fails = 1, r.verdict = Verdict::Fail;
        return r;
    }
    char why[200];
    // FAIL: a plate inside a zone (UI_HUD2_PRESETS.md 1.3 "a plate inside a zone fails the lint"); in a zone that keeps
    // everything out (the lower centre, decision 4 / 5.23) any recorded item but a transient overlay
    if (!Skipped(in, "zone"))
        for (const Item& it : in.items)
        {
            const bool plate = it.kind == Kind::Plate;
            if (it.alpha <= 0)
                continue;
            for (const Zone& z : in.zones)
                // (the targeting display stands where the target does, the 31 Oct rule's diegetic exception, the owner's word: UI_HUD2_SKIN.md 6.10)
                if ((plate || (z.texts && !it.overlay)) && !Exempt(it.comp, z.exempt) && it.comp.compare(0, 7, "target.") != 0 && InZone(it, z))
                {
                    const bool mark = it.kind == Kind::Text || it.kind == Kind::Glyph || it.kind == Kind::Keycap ||
                                      it.kind == Kind::Icon;
                    snprintf(why, sizeof why, "%s inside zone %s%s",
                             plate ? "a plate" : mark ? "a prompt / text" : "a drawn mark (arc / shape)", z.name.c_str(),
                             z.texts ? " (the lower centre: nothing HUD 2 draws there, UI_HUD2.md 5.23)" : "");
                    Add(r, Sev::Fail, "zone", Who(it), why);
                }
        }
    // FAIL: text under the floor (max(0.024 H, 14 px), above); glyph letters are sized by their shape (hud2_kit Mark)
    if (!Skipped(in, "text.floor"))
    {
        const float floor = textfit::MinSize(H); // (hud_size moves it)
        for (const Item& it : in.items)
            if ((it.kind == Kind::Text || it.kind == Kind::Keycap) && it.alpha > 0 && it.textH > 0 &&
                it.textH < floor - kEdgePx)
            {
                snprintf(why, sizeof why, "text %.1f px (%.4f H) under the floor %.1f px", it.textH, it.textH / H, floor);
                Add(r, Sev::Fail, "text.floor", Who(it), why);
            }
    }
    // FAIL: centre-stack overlaps - two drawn items of different components, one of them in the centre stack, closer than
    // 0.01 H (inside one component a label sits on its own arc / glyph by design; an overlay - 1.3's chevrons and hit
    // flash - draws on top instead of keeping the gap)
    if (!Skipped(in, "centre.overlap"))
    {
        const float gap = kGapH * H;
        for (size_t i = 0; i < in.items.size(); ++i)
        {
            const Item& a = in.items[i];
            if (a.alpha <= 0)
                continue;
            for (size_t j = i + 1; j < in.items.size(); ++j)
            {
                const Item& b = in.items[j];
                if (b.alpha <= 0 || !(a.centre || b.centre) || a.comp == b.comp)
                    continue;
                if (a.overlay || b.overlay) // drawn on top by design (1.3; Item::overlay)
                    continue;
                // (a cheap reject on the boxes first: the arcs' boxes hold their bands)
                if (BoxGap(a.x0, a.y0, a.x1, a.y1, b.x0, b.y0, b.x1, b.y1) >= gap)
                    continue;
                const float g = ItemGap(a, b);
                if (g < gap)
                {
                    snprintf(why, sizeof why, "%s %.1f px (%.4f H), the minimum is %g H", g < 0 ? "overlap" : "gap",
                             g < 0 ? -g : g, g / H, kGapH);
                    Add(r, Sev::Fail, "centre.overlap", Who(a) + " / " + Who(b), why);
                }
            }
        }
    }
    // FAIL: a context prompt on the subtitles' panel: the prompts move clear, they may not touch it
    if (in.dlgX1 > in.dlgX0 && !Skipped(in, "prompt.subtitles"))
        for (const Item& a : in.items)
            if (a.alpha > 0 && a.comp.compare(0, 7, "context") == 0 && BoxGap(a.x0, a.y0, a.x1, a.y1, in.dlgX0, in.dlgY0, in.dlgX1, in.dlgY1) <= 0)
                Add(r, Sev::Fail, "prompt.subtitles", Who(a), "a prompt on the subtitles' panel");
    // FAIL: an alert card over any other part of the HUD, at any size (GitHub issue 35: the card across a long contact
    // list), and its words out of its own plate. The plate is the card's widest shape; overlays draw on top by design
    if (!Skipped(in, "alert.overlap"))
    {
        const Item* plate = nullptr;
        for (const Item& a : in.items)
            if (a.alpha > 0 && a.comp == "alert" && a.kind == Kind::Shape && (!plate || a.x1 - a.x0 > plate->x1 - plate->x0))
                plate = &a;
        for (const Item& a : in.items)
        {
            if (a.alpha <= 0 || a.comp != "alert")
                continue;
            if (plate && a.kind == Kind::Text && (a.x0 < plate->x0 - 0.5f || a.x1 > plate->x1 + 0.5f))
                Add(r, Sev::Fail, "alert.overlap", Who(a), "the words stand out of the card's plate");
            for (const Item& b : in.items)
            {
                if (b.alpha <= 0 || b.comp == "alert" || b.overlay)
                    continue;
                if (BoxGap(a.x0, a.y0, a.x1, a.y1, b.x0, b.y0, b.x1, b.y1) < 0 && ItemGap(a, b) < 0)
                    Add(r, Sev::Fail, "alert.overlap", Who(a) + " / " + Who(b), "an alert card over another part");
            }
        }
    }
    // FAIL: inside one panel, two drawn items that overlap or sit closer than their gap (PanelGapPx: 0.01 H for separate
    // elements; a designed unit and the lines of one text block may touch). The edge tolerance keeps a box that ends
    // exactly on the gap passing.
    if (!Skipped(in, "panel.overlap"))
    {
        std::vector<std::string> part(in.items.size());
        for (size_t i = 0; i < in.items.size(); ++i)
            if (PanelItem(in.items[i]))
                part[i] = PartOf(in.items[i].comp);
        for (size_t i = 0; i < in.items.size(); ++i)
        {
            const Item& a = in.items[i];
            if (part[i].empty())
                continue;
            for (size_t j = i + 1; j < in.items.size(); ++j)
            {
                const Item& b = in.items[j];
                if (part[j] != part[i])
                    continue;
                const float need = PanelGapPx(a, b, H);
                const float g = BoxGap(a.x0, a.y0, a.x1, a.y1, b.x0, b.y0, b.x1, b.y1);
                if (g < need - kEdgePx)
                {
                    snprintf(why, sizeof why, "%s %.1f px (%.4f H) in %s, the minimum is %s", g < 0 ? "overlap" : "gap",
                             g < 0 ? -g : g, g / H, part[i].c_str(),
                             need > 0 ? "0.01 H (separate elements)" : "touching (one unit / one text block)");
                    Add(r, Sev::Fail, "panel.overlap", Who(a) + " / " + Who(b), why);
                }
            }
        }
    }
    // WARN: a plate whose content does not sit evenly in it (the owner, 6 Nov 2026: equal space on both axes, none wasted, everywhere): the air on the two sides of an axis apart by more than
    // 0.008 H. Text boxes carry the font's line, so a small difference is not read as a fault
    if (in.air && !Skipped(in, "panel.air"))
    {
        const float tol = 0.008f * H;
        for (const Item& p : in.items)
        {
            if (p.kind != Kind::Plate || p.alpha < kVisibleAlpha)
                continue;
            float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
            const std::string part = PartOf(p.comp);
            for (const Item& c : in.items)
            {
                const float mx = (c.x0 + c.x1) / 2, my = (c.y0 + c.y1) / 2;
                if ((!PanelItem(c) && !(c.kind == Kind::Shape && c.alpha >= kVisibleAlpha)) || PartOf(c.comp) != part || mx < p.x0 || mx > p.x1 || my < p.y0 || my > p.y1)
                    continue;
                x0 = std::min(x0, c.x0), y0 = std::min(y0, c.y0), x1 = std::max(x1, c.x1), y1 = std::max(y1, c.y1);
            }
            if (x1 < x0)
                continue;
            const float l = x0 - p.x0, rr = p.x1 - x1, t = y0 - p.y0, b = p.y1 - y1;
            if (fabsf(l - rr) > tol || fabsf(t - b) > tol)
            {
                snprintf(why, sizeof why, "air left %.3f right %.3f top %.3f bottom %.3f H, plate %.3f x %.3f H", l / H, rr / H, t / H, b / H, (p.x1 - p.x0) / H, (p.y1 - p.y0) / H);
                Add(r, Sev::Warn, "panel.air", Who(p), why);
            }
        }
    }
    // FAIL: one value id visible twice (1.7 "one value, one place at a time"), except declared second channels
    if (!Skipped(in, "value.twice"))
    {
        std::vector<bool> done(in.items.size(), false);
        for (size_t i = 0; i < in.items.size(); ++i)
        {
            const Item& a = in.items[i];
            if (done[i] || a.value.empty() || a.alpha < kVisibleAlpha)
                continue;
            std::string who;
            int places = 0;
            for (size_t j = i; j < in.items.size(); ++j)
            {
                const Item& b = in.items[j];
                if (b.value != a.value || b.alpha < kVisibleAlpha)
                    continue;
                done[j] = true;
                if (b.secondChannel)
                    continue;
                ++places;
                who += (who.empty() ? "" : " / ") + Who(b);
            }
            if (places > 1)
            {
                snprintf(why, sizeof why, "value '%s' visible in %d places", a.value.c_str(), places);
                Add(r, Sev::Fail, "value.twice", who, why);
            }
        }
    }
    // FAIL: a bound action with no reachable prompt / keycap / wheel entry, or a wheel action with no wheel bind
    if (!Skipped(in, "action.reach"))
        for (const Action& a : in.actions)
        {
            if (a.wheelEntry && !in.wheelBound)
                Add(r, Sev::Fail, "action.wheel", a.name, "a wheel action, but the wheel has no binding");
            else if (!a.prompt && !a.keycap && !a.wheelEntry)
                Add(r, Sev::Fail, "action.reach", a.name, "bound, but no prompt, keycap or wheel entry reaches it");
        }
    // FAIL: an analog control in use without its readout (UI_HUD2_PRESETS.md 4 rule 4)
    if (!Skipped(in, "analog.readout"))
        for (const Analog& a : in.analogs)
            if (a.inUse && !a.readout)
                Add(r, Sev::Fail, "analog.readout", a.name, "in use without its readout");
    // FAIL: a latched toggle without a state word (4: "a toggle without one is invisible state")
    if (!Skipped(in, "toggle.state"))
        for (const Toggle& t : in.toggles)
            if (t.latched && !t.stateWord)
                Add(r, Sev::Fail, "toggle.state", t.name, "latched without a state word");

    // WARN: the boxed budget (plates only, their union)
    {
        std::vector<const Item*> plates;
        for (const Item& it : in.items)
            if (it.kind == Kind::Plate && it.alpha > 0)
                plates.push_back(&it);
        r.boxed = UnionArea(plates, W, H) / (W * H);
        std::vector<const Item*> standing;
        for (const Item* p : plates)
            if (!p->fades)
                standing.push_back(p);
        r.steady = UnionArea(standing, W, H) / (W * H);
        if (!Skipped(in, "budget") && in.budget > 0 && r.steady > in.budget)
        {
            snprintf(why, sizeof why, "boxed %.1f %% of the screen, the budget is %.1f %%", r.steady * 100, in.budget * 100);
            Add(r, Sev::Warn, "budget", std::to_string(standing.size()) + " plates", why);
        }
        else if (!Skipped(in, "budget") && in.budget > 0 && r.boxed > in.budget)
        {
            snprintf(why, sizeof why, "awake peak %.1f %% of the screen, steady %.1f %%, the budget is %.1f %%", r.boxed * 100, r.steady * 100, in.budget * 100);
            Add(r, Sev::Warn, "budget.awake", std::to_string(plates.size()) + " plates", why);
        }
    }
    // WARN: prompt counts (pills on the context line; every glyph / keycap but the answers)
    for (const Item& it : in.items)
        if (IsPrompt(it) && it.alpha > 0)
        {
            r.pills += it.lane == Lane::Pill;
            r.pairs += it.lane != Lane::Answer;
        }
    if (!Skipped(in, "prompts"))
    {
        if (in.pillCap >= 0 && r.pills > in.pillCap)
        {
            snprintf(why, sizeof why, "%d context pills, the cap is %d", r.pills, in.pillCap);
            Add(r, Sev::Warn, "prompts.pills", "context", why);
        }
        if (in.pairCap >= 0 && r.pairs > in.pairCap)
        {
            snprintf(why, sizeof why, "%d prompts (answers apart), the cap is %d", r.pairs, in.pairCap);
            Add(r, Sev::Warn, "prompts.pairs", "-", why);
        }
    }
    // WARN: text in the ship keep-out
    if (in.keepOut && !Skipped(in, "keepout"))
    {
        const float kx0 = kKeepX0 * W, kx1 = kKeepX1 * W, ky0 = kKeepY0 * H, ky1 = kKeepY1 * H;
        snprintf(why, sizeof why, "text inside the ship keep-out (x %.2f-%.2f W, y %.2f-%.2f H)", kKeepX0, kKeepX1,
                 kKeepY0, kKeepY1);
        for (const Item& it : in.items)
            if (it.kind == Kind::Text && it.alpha > 0 && std::min(it.x1, kx1) - std::max(it.x0, kx0) > kEdgePx &&
                std::min(it.y1, ky1) - std::max(it.y0, ky0) > kEdgePx)
                Add(r, Sev::Warn, "keepout", Who(it), why);
    }
    // WARN: outside title-safe 90 %
    if (in.titleSafe && !Skipped(in, "title-safe"))
    {
        const float mx = (1 - kTitleSafe) * 0.5f * W, my = (1 - kTitleSafe) * 0.5f * H;
        for (const Item& it : in.items)
            if (it.alpha > 0 && (it.x0 < mx - kEdgePx || it.y0 < my - kEdgePx || it.x1 > W - mx + kEdgePx ||
                                 it.y1 > H - my + kEdgePx))
                Add(r, Sev::Warn, "title-safe", Who(it), "outside the inner 90 % (title-safe)");
    }
    for (const Finding& f : r.findings)
        (f.sev == Sev::Fail ? r.fails : r.warns)++;
    r.verdict = r.fails ? Verdict::Fail : r.warns ? Verdict::Warn : Verdict::Pass;
    return r;
}

inline const char* VerdictName(Verdict v) { return v == Verdict::Fail ? "FAIL" : v == Verdict::Warn ? "WARN" : "PASS"; }

// One line per finding, the n/a lines, then the verdict.
inline std::string Format(const Result& r)
{
    std::string s;
    for (const Finding& f : r.findings)
        s += std::string(f.sev == Sev::Fail ? "FAIL " : "WARN ") + f.rule + ": " + f.items + ": " + f.why + "\n";
    for (const auto& n : r.na)
        s += "n/a: " + n.first + ": " + n.second + "\n";
    char b[160];
    snprintf(b, sizeof b, "verdict %s (%d FAIL, %d WARN) boxed=%.1f%% steady=%.1f%% pills=%d prompts=%d", VerdictName(r.verdict),
             r.fails, r.warns, r.boxed * 100, r.steady * 100, r.pills, r.pairs);
    return s + b;
}
} // namespace hud2lint
