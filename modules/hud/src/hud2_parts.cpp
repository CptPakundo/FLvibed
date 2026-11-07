// HUD 2's components, drawn as tools\uimock\hud2_states.py draws them (iteration 3): the same fractions of H, sizes,
// colours and behaviours; deviations are noted where they are (modules\hud\docs\UI_HUD2.md). Every text >= 0.024 H at scale 100.
#include "hud2_impl.h"
#include "hud2_layout.h"
#include "ui/hud_rows.h"
#include "ui/views.h"
#include "safemem.h"
#include "shieldcue.h"
#include "ui/ui.h"
#include "ui/uiopt.h"

#include <cmath>

// (ASCII only: the build reads the sources as cp1252, non-ASCII text goes in as \u escapes)
namespace hud2
{
using namespace hud2kit;

static float Now() { return GetTickCount() / 1000.0f; }
// A cue's pulse, 0..1 about its rest: under skin_motion's fades and none nothing breathes (UI_HUD2_SKIN.md 6.10), the cue stands at its rest
static float Pulse(float rate) { return hudrows::g.skinMotion ? 0.0f : sinf(Now() * rate); }

float DrawPrompt(const Prompt& p, float cx, float cy, float h, u32 argb, bool measure, bool tight)
{
    if (!p.key.empty())
        return Keycap(p.key, cx, cy, h, argb, measure, tight);
    if (p.btn == glyphs::COUNT)
        return 0;
    if (!measure)
        Glyph(p.btn, cx, cy, h, argb, p.hold);
    return GlyphWidth(p.btn, h);
}

// The bars' low states (UI_HUD2_SKIN.md 6.11; the rules: hud2_bars.h): under 3 Hz, never colour alone, the number and the alert card carry the state too; `shield_cue`
// and `hull_cue` are their off switches. A bar never changes colour: the glow and the bloom stay in its own.
static hud2bars::Breath g_brShield, g_brHull, g_brBoost, g_brTShield, g_brTHull, g_brEscort[3];
static hud2bars::Bloom g_blShield, g_blHull, g_blBoost, g_blCruise, g_blTShield, g_blTHull;
static float g_beat = 0;

void StepBars(Model& m, float dt, int motion)
{
    using namespace hud2bars;
    static bool upWas = true;
    static float hullWas = 1, boostWas = 1, tShieldWas = -1, tHullWas = -1;
    static u32 targetWas = 0;
    BarFx& x = m.fx;
    const bool cueS = shieldcue::Get().on, cueH = shieldcue::Get().hull;
    x.glowShield = cueS ? g_brShield.Step(dt, m.shield, !m.shieldUp, motion) : 0;
    x.glowHull = cueH ? g_brHull.Step(dt, m.hull, false, motion) : 0;
    x.glowBoost = cueH && m.boost >= 0 ? g_brBoost.Step(dt, m.boost, false, motion) : 0; // (the hull's logic exactly, the owner, 6 Nov 2026: its breath, and its bloom at nothing)
    const bool tgt = m.target && m.tBars;
    x.glowTShield = cueS && tgt ? g_brTShield.Step(dt, m.tShield, m.tShield <= 0, motion) : 0;
    x.glowTHull = cueH && tgt ? g_brTHull.Step(dt, m.tHull, false, motion) : 0;
    for (int i = 0; i < 3; ++i)
        x.glowEscort[i] = cueH && i < m.nEscorts ? kEscortDepth * g_brEscort[i].Step(dt, m.escorts[i].hull, false, motion) : 0;
    // the breaks: a shield by its flag falling, not its value; a hull at nothing
    const Bloom* all[5] = {&g_blShield, &g_blHull, &g_blBoost, &g_blTShield, &g_blTHull};
    if (upWas && !m.shieldUp && cueS && BloomFree(all, 5))
        g_blShield.Start();
    if (hullWas > 0 && m.hull == 0 && cueH && BloomFree(all, 5))
        g_blHull.Start();
    if (boostWas > 0 && m.boost == 0 && cueH && BloomFree(all, 5))
        g_blBoost.Start();
    boostWas = m.boost;
    if (m.targetId != targetWas)
        tShieldWas = tHullWas = -1, targetWas = m.targetId;
    if (tgt && tShieldWas > 0 && m.tShield == 0 && cueS && BloomFree(all, 5))
        g_blTShield.Start(kTargetDelay);
    if (tgt && tHullWas > 0 && m.tHull <= 0 && cueH && BloomFree(all, 5))
        g_blTHull.Start(kTargetDelay);
    upWas = m.shieldUp, hullWas = m.hull >= 0 ? m.hull : hullWas, tShieldWas = tgt ? m.tShield : -1, tHullWas = tgt ? m.tHull : -1;
    // the cruise arc: full while it works. A disruption blooms out as a breaking bar does and runs it down on the game's own clock, reaching nothing the moment the engine
    // can charge again; any other end leaves the engine free at once (EngageCruise: the clock to 0), so it just fades (Shockwave, 7 Nov 2026: only a disruption blows out)
    {
        static int cruiseWas = 0;
        static float fill = 0;
        if (cruiseWas != 0 && m.cruise == 0 && m.cruiseWait > 0)
            g_blCruise.Start();
        if (m.cruise != 0)
            fill = m.cruise == 1 ? std::max(0.0f, m.charge) : 1.0f;
        else if (m.cruiseWait > 0)
            fill = m.cruiseWait;
        else
            fill = motion == 2 ? 0.0f : std::max(0.0f, fill - dt / 0.2f);
        cruiseWas = m.cruise, x.cruiseFill = fill;
    }
    for (Bloom* b : {&g_blShield, &g_blHull, &g_blBoost, &g_blCruise, &g_blTShield, &g_blTHull})
        b->Step(dt);
    x.bloomShield = g_blShield.Now(motion), x.bloomHull = g_blHull.Now(motion), x.bloomBoost = g_blBoost.Now(motion), x.bloomCruise = g_blCruise.Now(motion), x.bloomTShield = g_blTShield.Now(motion), x.bloomTHull = g_blTHull.Now(motion);
    // the shield down (Shockwave: the hull stands bare until it is back, and the meter only looked low): from the game's own fail flag to its return, a slow
    // pulse over the whole arc in its own colour, 1 Hz (well under 3); FADES and NONE hold it steady
    {
        static float downPhase = 0, wordPhase = 0;
        if (m.shieldUp || m.shield < -1)
            x.shieldDown = 0, x.downMix = 0, downPhase = wordPhase = 0;
        else
        {
            downPhase += dt;
            downPhase -= floorf(downPhase);
            x.shieldDown = motion == 0 ? 0.3f + 0.7f * (0.5f - 0.5f * cosf(2 * kPi * downPhase)) : 0.7f;
            // SHIELDS DOWN between blue and red as fast as the flash rule allows (the owner: "max frequency"; kRateMax 2.9 Hz, under 3); red under FADES / NONE
            wordPhase += kRateMax * dt;
            wordPhase -= floorf(wordPhase);
            x.downMix = motion == 0 ? 0.5f - 0.5f * cosf(2 * kPi * wordPhase) : 1.0f;
        }
    }
    // the alert cards' glow follows the worst bar's beat, else the slowest rate's
    static Breath own;
    const float g = std::max(g_brShield.on ? x.glowShield : 0.0f, g_brHull.on ? x.glowHull : 0.0f);
    x.beat = g_beat = g_brShield.on || g_brHull.on ? g : own.Step(dt, kOnAt, false, motion);
}

// The glow over a bar's lit part [lo, hi] of the span a0..a1: tight, full over what is left, the same short roll-off at both ends, in two wide layers under the arc.
static void ArcGlow(float cx, float cy, float r, float a0, float a1, float lo, float hi, float w, u32 rgb, float glow)
{
    if (glow <= 0.003f || hi <= lo)
        return;
    const float was = Alpha(), roll = hud2bars::kRollOff * (a1 - a0);
    for (const float layer : {3.6f, 2.3f}) // (2.6 / 1.7 before: more scale, the owner)
    {
        SetAlpha(was * glow * hud2bars::kGlowLayerA);
        Arc(cx, cy, r, lo, hi, w * layer, Argb(rgb, 1));
        for (int i = 1; i <= 3; ++i)
        {
            SetAlpha(was * glow * hud2bars::kGlowLayerA * (1 - i / 4.0f));
            const float s0 = roll * (i - 1) / 3, s1 = roll * i / 3;
            if (hi + s0 < a1)
                Arc(cx, cy, r, hi + s0, std::min(a1, hi + s1), w * layer, Argb(rgb, 1));
            if (lo - s0 > a0)
                Arc(cx, cy, r, std::max(a0, lo - s1), lo - s0, w * layer, Argb(rgb, 1));
        }
    }
    SetAlpha(was);
}

// SHIELDS / DOWN, one word a row at the reticle's lower left (the owner, 7 Nov 2026), between the shield's blue and red by `mix`.
static void ShieldsDownWords(float cx, float cy, float R, float T, float mix)
{
    const auto ch = [mix](u32 a, u32 b, int s) { return static_cast<u32>(((a >> s) & 0xFF) + (static_cast<int>((b >> s) & 0xFF) - static_cast<int>((a >> s) & 0xFF)) * mix) << s; };
    const u32 rgb = ch(kShield, kHostile, 16) | ch(kShield, kHostile, 8) | ch(kShield, kHostile, 0);
    const float x = cx - R * 1.12f, y = cy + R * 0.62f; // (clear of the shield arc's lower end: the owner, 7 Nov)
    Text(x, y, L"SHIELDS", T, Argb(rgb, 1), Right);
    Text(x, y + T * 1.15f, L"DOWN", T, Argb(rgb, 1), Right);
}

// The shield down: its whole arc's track lit by the pulse, in its own colour (a bar never changes colour), two layers as the glow's.
static void ArcDown(float cx, float cy, float r, float a0, float a1, float w, float pulse)
{
    if (pulse <= 0)
        return;
    const float was = Alpha();
    SetAlpha(was * 0.22f * pulse);
    Arc(cx, cy, r, a0, a1, w * 3.0f, Argb(kShield, 1));
    SetAlpha(was * 0.55f * pulse);
    Arc(cx, cy, r, a0, a1, w * 1.1f, Argb(kShield, 1));
    SetAlpha(was);
}

// The bloom of a bar that broke: the whole bar lit, a halo away from the reticle, the empty pips coming up under it at the end. Words and numbers draw above it.
static void ArcBloom(float cx, float cy, float r, float a0, float a1, float w, u32 rgb, const hud2bars::BloomOut& b, float H, float k)
{
    if (!b.on)
        return;
    const float was = Alpha();
    if (b.light > 0)
    {
        SetAlpha(was * b.light);
        Arc(cx, cy, r, a0, a1, w, Argb(rgb, 1));
    }
    if (b.reach > 0)
    {
        const float out = hud2bars::kBloomReachH * H * k * b.reach, in = hud2bars::kBloomInwardH * H * k * std::min(1.0f, b.reach);
        RecordTag("ring.bloom"); // (600 ms: an overlay as the hit flash is, UI_HUD2_SKIN.md 12; the lint's bounding box read its corners against the cue row)
        // a halo, not a slab: four widths on one centre, the sum 0.40 at the bar and less each step out
        for (const float part : {1.0f, 0.75f, 0.5f, 0.25f})
        {
            SetAlpha(was * hud2bars::kBloomHaloA * (1 - b.pips));
            Arc(cx, cy, r + (out - in) / 2 * part, a0, a1, w + (out + in) * part, Argb(rgb, 1));
        }
    }
    SetAlpha(was);
}

// The ring's arcs carry their navy keyline, 1 px at 800 lines, 2 at 4K: legible over a bright planet at no cost in area (R6).
struct ArcKeyScope
{
    explicit ArcKeyScope(float H) { SetArcKeyline(std::max(1.0f, floorf(H * 0.001f + 0.5f))); }
    ~ArcKeyScope() { SetArcKeyline(0); }
};

// ---------------------------------------------------------------- the reticle ring
void DrawRing(const Model& m, const Layout& L, float a, float dim)
{
    const float H = L.H, k = L.k, cx = L.cx, cy = L.cy, R = L.R;
    const float w = hud2layout::kRingArcW * H * k, T = 0.024f * H * k;
    const int numbers = uiopt::Get().hudNumbers;
    SetAlpha(a * dim);
    const ArcKeyScope key(H);
    // (the lint's recorder, hud2_kit.h Record: each element of the centre stack its own tag - a number belongs to its arc)
    const float shield = std::max(0.0f, m.shield);
    RecordTag("ring.shield");
    if (m.shield > -2) // shield left (red at <= 25 %), filling from the bottom as the hull: it drains down (GitHub issue 18)
    {
        ArcDown(cx, cy, R, 138, 222, w, m.fx.shieldDown);
        ArcGlow(cx, cy, R, 138, 222, 138, 138 + 84 * shield, w, kShield, m.fx.glowShield);
        SegArc(cx, cy, R, 138, 222, w, shield, kShield, 14, 1.6f, false);
        ArcBloom(cx, cy, R, 138, 222, w, kHostile, m.fx.bloomShield, H, k); // (a meter zeroed blooms in one red: shield, hull, cruise - the owner, 7 Nov 2026)
    }
    RecordTag("ring.hull");
    if (m.hull >= 0) // hull right
    {
        ArcGlow(cx, cy, R, -42, 42, 42 - 84 * m.hull, 42, w, kHull, m.fx.glowHull);
        SegArc(cx, cy, R, -42, 42, w, m.hull, kHull, 14, 1.6f, true);
        ArcBloom(cx, cy, R, -42, 42, w, kHostile, m.fx.bloomHull, H, k);
    }
    RecordTag("ring.power");
    // power below - not while cruise charges or runs: the cruise readout (the charge seconds, CRUISE ON) holds the ring's
    // bottom then (UI_HUD2.md 5.23; the game takes the weapons offline while the engine charges, FREELANCER_RE "The cruise
    // line"; while cruise runs it is this package's choice - the power arc returns with the drop - open for the owner)
    if (m.cruise == 0 && m.power >= 0) // (draining to the middle: GitHub issue 28; inside the ring's top since the boost took the bottom, the throttle's mirror)
        SegArcCentred(cx, cy, R + hud2layout::kThrottleDR * H * k, 248, 292, w * 0.8f, m.power, kPower, 8, 2.2f);
    auto show = [&](float v) { return v >= 0 && (numbers == 1 || (numbers == 0 && v < 0.4f)); };
    wchar_t buf[32];
    RecordTag("ring.shield");
    if (m.fx.shieldDown > 0) // (whatever hud_numbers says: the state matters more than the value)
        ShieldsDownWords(cx, cy, R, T, m.fx.downMix);
    if (show(shield) && m.shield > -2)
    {
        swprintf_s(buf, L"%d", static_cast<int>(shield * 100 + 0.5f));
        Text(cx - R * 0.72f, cy - R * 0.92f, buf, T, Argb(kShield, 1), Right);
    }
    RecordTag("ring.hull");
    if (show(m.hull))
    {
        swprintf_s(buf, L"%d", static_cast<int>(m.hull * 100 + 0.5f));
        Text(cx + R * 0.72f, cy - R * 0.92f, buf, T, Argb(kHull, 1), Left);
    }
    // the speed and the throttle dim with the arcs in calm flight, but less (they stay readable at a glance: 60 %)
    const float sa = a * std::max(dim, 0.6f);
    SetAlpha(a);
    RecordTag("ring.charge");
    // The boost owns the ring's bottom, always (the owner, 6 Nov 2026; Shockwave's report: how much is left): yellow, draining to the middle, amber when low. The cruise owns the top
    // and only while it works: cyan, its place, colour and motion all its own. Charging it fills and its glow grows with it, no breathing; running it stands full and breathes at 1 Hz
    RecordTag("ring.boost");
    if (m.boost >= 0)
    {
        const u32 bc = m.boost < 0.25f ? kAmber : kActive;
        ArcGlow(cx, cy, R, 48, 132, 90 - 42 * m.boost, 90 + 42 * m.boost, w, bc, m.fx.glowBoost);
        SegArcCentred(cx, cy, R, 48, 132, w, m.boost, bc, 14, 1.8f, 0x283050, w * 0.35f);
        ArcBloom(cx, cy, R, 48, 132, w, bc, m.fx.bloomBoost, H, k);
    }
    RecordTag("ring.charge");
    if (m.cruise != 0 || m.fx.cruiseFill > 0 || m.fx.bloomCruise.on)
    {
        // working (charging, cruising): a quiet steady glow, no halo - since the bars' bigger bloom the halo that breathed with it flashed through normal cruise
        // (Shockwave, 7 Nov 2026). Disrupted: red, the glow breathing (2.5 Hz, under HUD 2's 3 Hz) while it runs down on the game's clock, after the bloom out
        const bool broke = m.cruise == 0 && m.cruiseWait > 0;
        const u32 col = broke ? kHostile : kCyan;
        const float fill = m.fx.cruiseFill;
        const float glow = m.cruise == 1 ? 0.35f * m.charge : m.cruise == 2 ? 0.35f : broke ? 0.45f + 0.35f * Pulse(15.708f) : 0.35f * fill;
        ArcGlow(cx, cy, R, 240, 300, 270 - 30 * fill, 270 + 30 * fill, w, col, glow);
        SegArcCentred(cx, cy, R, 240, 300, w, fill, col, 14, 1.8f, 0x283050, w * 0.35f); // (from the middle out, as the boost: both are level meters)
        ArcBloom(cx, cy, R, 240, 300, w, kHostile, m.fx.bloomCruise, H, k);
    }
    if (m.cruise == 1) // the seconds left
    {
        swprintf_s(buf, L"%.1f s", m.chargeLeft); // inside the ring, over the throttle arc
        Text(cx, cy + hud2layout::kCruiseDY * H * k, buf, T, Argb(kCyan, 1), Center);
    }
    // CRUISE ON in the charge seconds' slot ("The CRUISE ON text still hides the ship ... the
    // lower center of the screen ... devoid of HUD elements"; UI_HUD2.md 5.23): the cruise state reads in ONE place, inside
    // the ring under the speed - "3.2 s" while the engine charges (the arc filling), CRUISE ON while it runs (the power
    // arc yields its slot, above), nothing while off; no word under the ring any more (the reticle lane under it, R + 0.092
    // H, sat on the ship's nose). The hull / shield arcs end at 42 / 138 degrees: the word's box (~0.11 H wide at 0.024) keeps > 0.01 H from their inner corners.
    RecordTag("ring.cruise");
    if (m.cruise == 2)
        Text(cx, cy + hud2layout::kCruiseDY * H * k, L"CRUISE ON", T, Argb(kCyan, 1), Center);
    SetAlpha(m.cruise != 0 ? a : sa);
    RecordTag("ring.speed");
    swprintf_s(buf, L"%d", static_cast<int>(m.speed + 0.5f));
    // under the meter, in every state (form C; the owner's word over the older lower-centre rule: the zone starts below it)
    Text(cx, cy + R + hud2layout::kSpeedDR * H * k, buf, hud2layout::kSpeedSize * H * k, Argb(kInk, 1), Center);
    // (the tractor's spot that stood under the ring beside the cruise word - [X] TRACTOR n - is a context prompt in the
    // right lane since 5.23: hud2.cpp TractorCtx)
    // the throttle inside the ring over the meter: a short segmented arc, centred, filling from the middle out with the throttle, yellow and full with BOOST
    // while the afterburner fires (BOOST, not BURN), empty with KILL while the engine is killed, REV in reverse
    SetAlpha(sa);
    RecordTag("ring.throttle");
    {
        const float rt = R + hud2layout::kThrottleDR * H * k, tw = w * hud2layout::kThrottleW;
        const float thr = m.kill ? 0.0f : m.burner ? 1.0f : std::clamp(fabsf(m.throttle), 0.0f, 1.0f);
        const u32 tc = m.burner ? kActive : m.kill ? kAmber : m.reverse ? kAmber : kChrome;
        SegArcCentred(cx, cy, rt, hud2layout::kThrottleA0, hud2layout::kThrottleA1, tw, thr, tc, hud2layout::kThrottleBlocks, 2.2f, 0x283050, tw * 0.35f);
        // the word right of the speed, at the place it held beside the old arc: > 0.01 H from the power arc's (68 degrees) and the charge arc's (48..132)
        // outer edges and from CRUISE ON's box (up to ~0.12 H wide) - the lint's centre.overlap
        wchar_t boostWord[24];
        swprintf_s(boostWord, L"BOOST %d", static_cast<int>(std::max(0.0f, m.boost) * 100 + 0.5f));
        const wchar_t* drive = m.kill ? L"KILL" : m.burner ? (m.boost >= 0 ? boostWord : L"BOOST") : m.reverse ? L"REV" : nullptr;
        if (drive)
            Text(cx + 0.068f * H * k, cy + R + hud2layout::kDriveDR * H * k + tw * 0.5f - T * 0.5f, drive, T, Argb(tc, 1), Left, 0.08f);
    }
    SetAlpha(a);
    RecordTag("ring.hit");
    // a hit's side: a transient amber flash at 0.13 H - one 0.008 H arc fading over 300 ms, no glow band (the round 4
    // critique: the 0.018 H glow under a 0.006 H core read as a ~0.04 H outlined mark, the heaviest on the screen)
    if (m.hitAge >= 0 && m.hitAge < 0.3f)
    {
        const float f = 1 - m.hitAge / 0.3f, rd = 0.13f * H * k;
        Arc(cx, cy, rd, m.hitAng - 16, m.hitAng + 16, 0.008f * H * k, Argb(kAmber, 0.85f * f));
    }
    DrawTailMark(m, L, a);
    // missiles homing on the player: red chevrons on the ring's outside at their bearing, pulsing
    const float pulse = 0.65f + 0.35f * Pulse(10.0f);
    RecordTag("ring.missiles");
    for (int i = 0; i < m.missiles && i < 3; ++i)
    {
        const float ang = m.missileAng[i] * 3.14159265f / 180.0f, ux = cosf(ang), uy = sinf(ang), vx = -uy, vy = ux;
        // outside the hit flash's band (0.13 H +- 0.009): at its radius the chevrons hid under the amber flash of the hit
        // that usually comes with a missile (instance 5); thicker so they read over the game's red arcs
        const float rr[2] = {0.155f * H * k, 0.18f * H * k};
        const float sz = 0.02f * H * k, lw = 0.0065f * H * k;
        for (int c = 0; c < 2; ++c)
        {
            const float px = cx + rr[c] * ux, py = cy + rr[c] * uy;
            const u32 col = Argb(kHostile, pulse * (c ? 0.69f : 1.0f));
            Line(px + vx * sz, py + vy * sz, px - ux * sz * 0.8f, py - uy * sz * 0.8f, lw, col);
            Line(px - ux * sz * 0.8f, py - uy * sz * 0.8f, px - vx * sz, py - vy * sz, lw, col);
        }
    }
}

void DrawTailMark(const Model& m, const Layout& L, float a)
{
    if (!m.tail)
        return;
    SetAlpha(a);
    RecordTag("ring.tail");
    Arc(L.cx, L.cy, 0.13f * L.H * L.k, m.tailAng - 12, m.tailAng + 12, 0.0065f * L.H * L.k, Argb(kHostile, 0.9f));
}

// ---------------------------------------------------------------- top-left: where you are going
static void Chip(float x, float y, const std::wstring& s, float T, u32 rgb)
{
    const float w = TextWidth(s, T, 0.1f);
    Plate(x, y - 0.02f * T / 0.024f, x + w + T * 0.67f, y + 0.02f * T / 0.024f, T * 0.25f, kTL | kTR | kBR | kBL, 0.82f,
          rgb, 1.0f);
    Text(x + T / 3, y, s, T, Argb(rgb, 1), Left, 0.1f, false);
}

float DrawNav(const Model& m, const Layout& L, float a, const NavStyle& ns, float* bottom)
{
    // Focus's lean nav (PRESETS 2.2 Calm: "headline 1 line + timer"; "chip lines as Standard"): the NeuroNet chip, the
    // standing place line, the info line and the keyboard form's maneuver row are held back (the peek shows them: ns.full),
    // the keyboard form gets a dim MANEUVERS tag instead; the cruise charge's chip goes (its home is the band)
    const bool held = ns.lean && !ns.full;
    SetAlpha(a);
    const float H = L.H, k = L.k, T = 0.024f * H * k;
    const float x = L.mx, top = L.my + 0.02f * H;
    const float tx = x + 0.032f * H * k;
    float right = tx;
    // a nav line to the nav's width cap by the fit rule (smaller to the 0.024 H floor, then cut); `reserve` = room kept
    // right of the text (the protect bar)
    auto fit = [&](float x0, float y0, const std::wstring& s, float size, u32 argb, float track, float reserve = 0)
    { return TextMarquee(x0, y0, s, size, L.navMaxX - x0 - reserve, argb, Left, track); };
    auto ext = [&](float x1) { right = std::max(right, x1); };
    // The fixed rows first (the round 4 critique: NEURONET moved from line 1 to line 4 between states): the NeuroNet chip
    // pinned on the nav's top line, the keyboard form's maneuver row pinned under it; the objective / place / chip /
    // info lines stack below them on their own. In the game's letterbox only the maneuver chip stays (in the upper bar).
    float yy = top;
    if (!m.cine && !held)
    {
        const float g = 0.032f * H * k;
        const bool hov = m.kbm && HotHovered() == kHotNeuro;
        const u32 nc = Argb(hov ? kInk : kChrome, 1);
        const float gw = DrawPrompt(m.neuro, tx + g / 2, yy, g, nc, true);
        float nw = 0;
        {
            const RecUnit unit; // (the chip's glyph and its word)
            DrawPrompt(m.neuro, tx + gw / 2, yy, g, nc);
            nw = Text(tx + gw + g * 0.25f, yy, L"NEURAL NET", T, nc, Left, 0.1f);
        }
        ext(tx + gw + g * 0.25f + nw);
        if (m.news) // the game's NeuroNet flash: a yellow dot beside the chip (the mock-up's)
        {
            const float dx = tx + gw + g * 0.25f + nw + 0.012f * H * k;
            Disc(dx, yy - 0.008f * H * k, 0.005f * H * k, Argb(kActive, 1));
            ext(dx + 0.006f * H * k);
        }
        HotRect(kHotNeuro, tx - 0.006f * H * k, yy - 0.022f * H * k, tx + gw + g * 0.25f + nw + 0.006f * H * k,
                yy + 0.022f * H * k);
        yy += 0.048f * H * k;
        if (m.kbm) // the keyboard form: the maneuver row, each a click (hud2_click.cpp)
        {
            float rowRight = tx;
            yy += DrawManeuverRow(m, L, tx, yy, a, rowRight);
            ext(rowRight);
        }
    }
    const float y = yy; // the text block's first line
    const float chipRight = right;
    right = tx;
    if (ns.lean && !m.cine && (!m.objective.empty() || !m.timer.empty()))
    {
        // the headline on one line (cut by the fit rule) and the mission timer as its own field at its end (1.4 "Its own
        // field on the headline line, TIME 1:42"), the protected ship's bar between them
        const float d = 0.01f * H * k;
        if (!m.objective.empty())
        {
            const float dia[8] = {x + d, y - d, x + 2 * d, y, x + d, y + d, x, y};
            Outline(dia, 4, 0.002f * H * k, Argb(kActive, 1));
        }
        const float barW = m.protect >= 0 && !m.objective.empty() ? 0.096f * H * k : 0.0f;
        const float tw = m.timer.empty() ? 0.0f : TextWidth(m.timer, T, 0.06f) + 0.016f * H * k;
        float ow = 0;
        if (!m.objective.empty())
            ow = TextMarquee(tx, y, m.objective, ns.headline * H * k, L.navMaxX - tx - barW - tw, Argb(kInk, 1));
        if (barW > 0)
            SegBar(tx + ow + 0.016f * H * k, y - 0.005f * H * k, 0.08f * H * k, 0.010f * H * k, m.protect, kFriend, 8, 2);
        float tx2 = tx + ow + barW;
        if (tw > 0)
        {
            tx2 += ow > 0 ? 0.016f * H * k : 0.0f;
            tx2 += Text(tx2, y, m.timer, T, Argb(kChrome, 1), Left, 0.06f);
        }
        ext(tx2);
        yy += 0.05f * H * k;
    }
    else if (!m.objective.empty() && !m.cine)
    {
        const float d = 0.01f * H * k;
        const float dia[8] = {x + d, y - d, x + 2 * d, y, x + d, y + d, x, y};
        Outline(dia, 4, 0.002f * H * k, Argb(kActive, 1));
        const float barW = m.protect >= 0 ? 0.096f * H * k : 0.0f;
        // the headline by the fit rule; where even the floor would cut it, two lines (wrap,
        // step down or cut per box - "Protect the a..." at 4:3 with text_size 150)
        std::wstring lines[2];
        float os = 0;
        const float room = L.navMaxX - tx - barW;
        const int nl = FitLines(m.objective, 0.032f * H * k, room, 0, lines, 2, os);
        // the last line, where FitLines cut it, is a marquee of the rest of the headline (the owner's decision 1, 31 Oct
        // 2026: no permanent "..."): the words the lines before it hold are skipped, the rest scrolls in the room
        auto line = [&](int i, float ly)
        {
            if (i != nl - 1 || lines[i].empty() || lines[i].back() != textfit::kEllipsis)
                return Text(tx, ly, lines[i], os, Argb(kInk, 1));
            size_t at = 0;
            for (int j = 0; j < i; ++j)
            {
                const size_t p = m.objective.find(lines[j], at);
                at = p == std::wstring::npos ? at : p + lines[j].size();
                while (at < m.objective.size() && m.objective[at] == L' ')
                    ++at;
            }
            return TextMarquee(tx, ly, m.objective.substr(at), os, room, Argb(kInk, 1), Left, 0, true, os);
        };
        float ow = line(0, y);
        ext(tx + ow + barW);
        if (m.protect >= 0)
            SegBar(tx + ow + 0.016f * H * k, y - 0.005f * H * k, 0.08f * H * k, 0.010f * H * k, m.protect, kFriend, 8, 2);
        for (int i = 1; i < nl; ++i)
        {
            yy += os * 1.25f;
            ext(tx + line(i, yy));
        }
        yy += 0.05f * H * k;
    }
    // the place: an arrival / a zone edge for 5 s (the game's own line, hidden while HUD 2 shows), else the standing
    // system / zone line - dim, left out in a fight (the headline and the maneuver matter there)
    // (one fight rule, the threat list's: hostiles near - also while the autopilot fights; the round 4 critique)
    const bool event = !m.place.empty() && m.placeLeft > 0;
    if (!m.cine && (event || (!m.where.empty() && !m.fight && !held))) // (Focus: the arrival event only - an event wakes)
    {
        SetAlpha(a * (event ? std::min(1.0f, m.placeLeft) : 1.0f));
        ext(tx + fit(tx, yy, event ? m.place : m.where, T, Argb(kChrome, 1), 0.08f));
        SetAlpha(a);
        yy += 0.04f * H * k;
    }
    // the zone's danger: the next free row, under the place line and pushing nothing, in Focus too; the game's own art beside it (hudvis). Lag stands in the
    // same block, one small word, multiplayer only
    if (ns.artOut)
        ns.artOut[0] = ns.artOut[1] = 0;
    if (!m.cine && m.radiation)
    {
        RecordTag("nav.radiation");
        const float lineW = fit(tx, yy, L"RADIATION \x00B7 LEAVE THE ZONE", T, Argb(kAmber, 1), 0.08f);
        ext(tx + lineW);
        // the art after the line's end: its size is the game's, about 0.0625 H across whatever the HUD's scale (read from a
        // 1280x800 frame), so at the margin it stood on the screen's edge and over the line's first letter
        if (ns.artOut)
            ns.artOut[0] = tx + lineW + 0.008f * H * k + 0.032f * H, ns.artOut[1] = yy;
        yy += std::max(0.04f * H * k, 0.046f * H); // (the art's lower edge clear of the next row, a long one passing under it)
        RecordTag("nav");
    }
    if (!m.cine && m.lag)
    {
        RecordTag("nav.lag");
        ext(tx + Text(tx, yy, L"LAG", T, Argb(kAmber, 1), Left, 0.08f));
        yy += 0.04f * H * k;
        RecordTag("nav");
    }
    const bool manChip = m.man && !(ns.lean && m.manName == L"CRUISE CHARGING");
    if (m.ap || manChip || m.turret)
    {
        float chipX = tx;
        if (m.ap)
        {
            Chip(chipX, yy, L"AUTOPILOT", T, kCyan);
            ext(chipX + TextWidth(L"AUTOPILOT", T, 0.1f) + T * 0.67f); // (the chip's plate: Chip)
            chipX += TextWidth(L"AUTOPILOT", T, 0.1f) + 0.03f * H * k;
        }
        if (m.turret)
        {
            Chip(chipX, yy, L"TURRET VIEW", T, kChrome);
            ext(chipX + TextWidth(L"TURRET VIEW", T, 0.1f) + T * 0.67f);
            chipX += TextWidth(L"TURRET VIEW", T, 0.1f) + 0.03f * H * k;
        }
        if (manChip)
        {
            float mw = 0;
            {
                const RecUnit unit; // (the maneuver's icon and its word)
                if (!Icon(m.manIcon, chipX + 0.016f * H * k, yy, 0.032f * H * k, Argb(m.manCol, 1)))
                    Disc(chipX + 0.016f * H * k, yy, 0.006f * H * k, Argb(m.manCol, 1));
                mw = Text(chipX + 0.038f * H * k, yy, m.manName, T * 1.1f, Argb(m.manCol, 1), Left, 0.06f);
            }
            ext(chipX + 0.038f * H * k + mw);
            if (!m.manDest.empty())
            {
                // handed over to the target card while it names the same object (hud2.cpp Render destOnCard; the lint's
                // value.twice, UI_HUD2_PRESETS.md 1.7): faded with the card, its room kept (the nav's edge and the
                // dialogue panel's topFree do not jump when the card comes and goes)
                const float dx0 = chipX + 0.038f * H * k + mw + 0.012f * H * k;
                SetAlpha(a * (1 - m.destOnCard));
                ext(dx0 + fit(dx0, yy, m.manDest, T * 1.1f, Argb(kInk, 1), 0));
                SetAlpha(a);
            }
        }
        else if (m.ap && !m.apMode.empty())
            ext(chipX + fit(chipX, yy, m.apMode, T * 1.1f, Argb(kInk, 1), 0.04f));
        yy += 0.04f * H * k;
        if (m.progress >= 0)
        {
            SegBar(tx, yy - 0.005f * H * k, 0.24f * H * k, 0.009f * H * k, m.progress, m.man ? m.manCol : kCyan, 24, 2);
            ext(tx + 0.24f * H * k); // (the bar and its ETA count in the content's extent: 0.2.1 round 4)
            if (!m.eta.empty())
                ext(tx + 0.255f * H * k + Text(tx + 0.255f * H * k, yy, m.eta, T, Argb(kInk, 1)));
            yy += 0.04f * H * k;
        }
        else if (!m.eta.empty())
        {
            ext(tx + Text(tx, yy, m.eta, T, Argb(kInk, 1)));
            yy += 0.04f * H * k;
        }
        if (!m.wait.empty())
        {
            ext(tx + fit(tx, yy, m.wait, T, Argb(kCyan, 0.9f), 0.06f));
            yy += 0.04f * H * k;
        }
    }
    // the dock / lane / jump answer, unless the comm surface says it now (the round 4 critique: DOCKING GRANTED in the
    // nav while the comm card said it - one value, one place), then the other contextual information
    // (Focus: the dock / lane / jump answer with its chip - "chip lines as Standard", the queue and the bay - the rest of
    // the info line held back; its timer is the headline's field)
    std::wstring info = m.comm || (held && !manChip && !m.ap) ? std::wstring() : m.event;
    std::wstring more = m.info; // (the lean nav's timer is the headline's field: not twice - Slow puts it first)
    if (ns.lean && !m.timer.empty() && more.compare(0, m.timer.size(), m.timer) == 0)
    {
        more.erase(0, m.timer.size());
        if (more.compare(0, 3, L" \x00B7 ") == 0)
            more.erase(0, 3);
    }
    if (!more.empty() && !held)
        info += (info.empty() ? L"" : L" \x00B7 ") + more;
    if (!info.empty() && !m.cine)
    {
        ext(tx + fit(tx, yy, info, T, Argb(kChrome, 1), 0.06f));
        yy += 0.04f * H * k;
    }
    if (held && m.kbm && !m.cine) // the keyboard form's maneuver row, held back: a dim tag (the peek shows the row)
    {
        ext(tx + Text(tx, yy, L"MANEUVERS", T, Argb(kDim, 0.8f), Left, 0.1f));
        yy += 0.04f * H * k;
    }
    if (bottom)
        *bottom = std::max(top, yy - 0.02f * H * k);
    const bool texts = yy > y + 0.001f;
    if (ns.textOut)
        ns.textOut[0] = texts ? right : 0.0f, ns.textOut[1] = texts ? yy - 0.02f * H * k : 0.0f;
    // the scrim sized to the text block, eased (0.2.1 round 4: hud2.cpp eases ns.scrimR / B; never under this frame's text)
    const float textRight = std::max(right, ns.scrimR), textBottom = std::max(yy - 0.02f * H * k, ns.scrimB);
    right = std::max(right, chipRight);
    // The soft scrim behind the text block (no visible rectangle anywhere): none while the
    // chip is all there is; else the drawn text's extent, feathered on every side (a centre block, four edge ramps and
    // four corner fans: the colour fades to nothing at every outer edge). Shapes lie under the text by the kit's layers.
    // The edge in two ramps (0.30 -> 0.12 over 0.03 H, then -> 0 over 0.05 H; the round 4 critique: the one linear
    // 0.045 H ramp read as an edge over bright clouds) - an eased falloff with the same few quads.
    if (texts)
    {
        const u32 c0 = Argb(0x040410, 0.30f), cm = Argb(0x040410, 0.12f), c1 = Argb(0x040410, 0);
        const float x0 = x - 0.012f * H * k, y0 = y - 0.024f * H * k, x1 = textRight + 0.012f * H * k, y1 = textBottom;
        const float w = std::max(0.0f, x1 - x0), h = std::max(0.0f, y1 - y0);
        Grad(x0, y0, w, h, c0, c0, c0, c0);
        // a ring of edge ramps and corner fans from the rectangle (ax0..ax1, ay0..ay1) outward by f, colour ci -> co
        auto ring = [&](float ax0, float ay0, float ax1, float ay1, float f, u32 ci, u32 co)
        {
            const float rw = ax1 - ax0, rh = ay1 - ay0;
            Grad(ax0, ay0 - f, rw, f, co, co, ci, ci); // top
            Grad(ax0, ay1, rw, f, ci, ci, co, co);     // bottom
            Grad(ax0 - f, ay0, f, rh, co, ci, ci, co); // left
            Grad(ax1, ay0, f, rh, ci, co, co, ci);     // right
            Grad(ax0 - f, ay0 - f, f, f, co, co, ci, co); // corners: only the inner vertex holds the inner colour
            Grad(ax1, ay0 - f, f, f, co, co, co, ci);
            Grad(ax1, ay1, f, f, ci, co, co, co);
            Grad(ax0 - f, ay1, f, f, co, ci, co, co);
        };
        const float f1 = 0.03f * H * k, f2 = 0.05f * H * k;
        ring(x0, y0, x0 + w, y0 + h, f1, c0, cm);
        ring(x0 - f1, y0 - f1, x0 + w + f1, y0 + h + f1, f2, cm, c1);
    }
    return right;
}

// A plate with a window: the game's 3D view (hud_wireframes; hudvis draws it under the HUD) shows through its left
// column x0..wx - there only a faint tint (the wireframe's thin lines lost half their contrast under the full glass),
// the glass from wx on (following the right corners' chamfers), a hairline between; rim and accent as any plate.
static void WindowPlate(float x0, float y0, float x1, float y1, float c, int corners, float fill, u32 rim, float rimA,
                        char accent, u32 accentRgb, float wx)
{
    const float tint = 0.12f;
    Plate(x0, y0, x1, y1, c, corners, tint, rim, rimA, accent, accentRgb);
    const float tr = (corners & kTR) ? c : 0, br = (corners & kBR) ? c : 0;
    const float pts[12] = {wx, y0, x1 - tr, y0, x1, y0 + tr, x1, y1 - br, x1 - br, y1, wx, y1};
    Poly(pts, 6, Argb(kGlass, (fill - tint) / (1 - tint)));
    const float lw = std::max(1.0f, (y1 - y0) * 0.006f);
    Rect(wx - lw / 2, y0 + (y1 - y0) * 0.12f, lw, (y1 - y0) * 0.76f, Argb(rim, rimA * 0.8f));
}

// ---------------------------------------------------------------- top-right: what you are looking at
// The room a card text at x gets so that its end never lands in the `gap` just short of `start`, the left edge of a
// text in the row above or below (diagonal neighbours: the lint's panel.overlap wants 0.01 H between them; the in-game
// lint: "Manhattan" ended 0.0079 H before "PLANET", 0.0088 H diagonally). A text ending at least `gap`
// before `start`, or reaching past it (then the two share the column: lines of one block), keeps its room; one ending
// inside the band is fitted into the room that ends `gap` before `start` (the fit rule: smaller, then cut).
// A text the fit rule would cut is a marquee (TextMarquee, the owner's decision 1): it fills its whole room
// while it scrolls (and the lint records the room), so its end is the room's end, not the cut form's.
static float ClearRoom(const std::wstring& s, float size, float track, float x, float room, float start, float gap)
{
    if (s.empty() || start <= x || start - gap - x <= 0)
        return room;
    const textfit::Result& r = FitText(s, size, room, track);
    const float end = x + (r.cut ? room : r.width);
    return end > start - gap && end < start + 1.0f ? start - gap - x : room;
}

float CardColumn(const Model& m, const Layout& L)
{
    return (m.tViewOk && uiopt::Get().hudWireframes ? hud2layout::std_ov::kCardColView : hud2layout::std_ov::kCardColIcon) * L.H * L.k;
}

void DrawTarget(const Model& m, const Layout& L, float a)
{
    SetAlpha(a);
    const float H = L.H, k = L.k, T = 0.024f * H * k;
    const float x0 = L.tgtX0, y0 = L.tgtY0, x1 = L.tgtX1, y1 = L.tgtY1;
    const float vw = CardColumn(m, L);
    if (m.tView) // the game's view of the target in the card's window (its own wireframe), no flat icon
        WindowPlate(x0, y0, x1, y1, 0.012f * H * k, kTR | kBL, 170 / 255.0f, kChrome, 0.43f, 'l', m.tCol,
                    x0 + 0.012f * H * k + vw + 0.006f * H * k);
    else
        Plate(x0, y0, x1, y1, 0.012f * H * k, kTR | kBL, 170 / 255.0f, kChrome, 0.43f, 'l', m.tCol);
    // no view (none wanted for this kind, not built yet, or stale): the flat type icon at 0.07 H in the window
    if (!m.tView && !Icon(m.tIcon, x0 + 0.012f * H * k + vw / 2, (y0 + y1) / 2, 0.07f * H * k, Argb(m.tCol, 1)))
    {
        const float cx = x0 + 0.012f * H * k + vw / 2, cy = (y0 + y1) / 2, d = vw * 0.2f;
        const float dia[8] = {cx, cy - d, cx + d, cy, cx, cy + d, cx - d, cy};
        Outline(dia, 4, 0.003f * H * k, Argb(m.tCol, 1));
    }
    const float tx = x0 + vw + 0.026f * H * k, right = x1 - 0.018f * H * k;
    float yy = y0 + 0.030f * H * k;
    // the attitude chip, the class and level (the mock's 0.9 T raised to the 0.024 H floor)
    const float cw = TextWidth(m.tAtt, T, 0.1f) + 0.016f * H * k;
    Plate(tx, yy - 0.016f * H * k, tx + cw, yy + 0.016f * H * k, 0.005f * H * k, kTL | kTR | kBR | kBL, 230 / 255.0f,
          m.tCol, 1.0f);
    Text(tx + 0.008f * H * k, yy, m.tAtt, T, Argb(m.tCol, 1), Left, 0.1f, false);
    std::wstring cls = m.tClass;
    if (m.tLevel >= 0)
    {
        wchar_t lv[24];
        swprintf_s(lv, L"  \u00B7  LEVEL %d", m.tLevel);
        cls += lv;
    }
    // the pad's clear-target prompt, only on the card (= only with a target): [RS] x2 CLEAR in the card's footer row
    // (the pad card is a row taller for it, as the keyboard form's is for its icons - hud2_click.cpp DrawCardIcons;
    // was the top row's right end, which cut the class to "F..." beside a HOSTILE chip: the spec recapture)
    // (the 0.17 card has no foot row beside the bars: the prompt shows where there are none, UI_HUD2_BUILD_040.md Q2)
    if (!m.kbm && m.clearBtn.btn != glyphs::COUNT && !m.tBars)
    {
        const float gh = T * 1.2f, gw = DrawPrompt(m.clearBtn, 0, 0, gh, 0, true), fy = y1 - CardFootUp(L);
        const wchar_t* lbl = L"x2 CLEAR";
        const float lw = TextWidth(lbl, T, 0.06f);
        const RecUnit unit; // (the prompt and its own label)
        DrawPrompt(m.clearBtn, right - lw - 0.006f * H * k - gw / 2, fy, gh, Argb(kDim, 1));
        Text(right, fy, lbl, T, Argb(kDim, 1), Right, 0.06f);
    }
    // cut to the card ("FIGHTER - LEVEL 1" ran past its edge beside a wide chip): the short level form first, then cut
    const float clsRoom = right - (tx + cw + 0.012f * H * k);
    if (m.tLevel >= 0 && TextWidth(cls, T, 0.06f) > clsRoom)
    {
        wchar_t lv[24];
        swprintf_s(lv, L"  LV %d", m.tLevel);
        cls = m.tClass + lv;
    }
    if (!cls.empty())
        TextMarquee(tx + cw + 0.012f * H * k, yy, cls, T, clsRoom, Argb(kDim, 1), Left, 0.06f);
    yy += 0.040f * H * k;
    // a long name (a lane's "Colorado Jump Gate->Fort Bush") steps down by the fit rule before it is cut
    // no faction (a waypoint, loot, a lane ring): its row collapses - the distance moves onto the name's row (the round
    // 4 critique: a blank faction row)
    const bool facRow = !m.tFaction.empty();
    const float distW = TextWidth(m.tDist, 0.032f * H * k) + 0.016f * H * k;
    float nameRoom = right - tx - (facRow ? 0 : distW);
    if (facRow) // (the distance on the faction row under it: diagonal neighbours keep 0.01 H - ClearRoom)
        nameRoom = ClearRoom(m.tName, 0.032f * H * k, 0, tx, nameRoom, right - TextWidth(m.tDist, 0.032f * H * k),
                             0.011f * H * k);
    TextMarquee(tx, yy, m.tName, 0.032f * H * k, nameRoom, Argb(kInk, 1));
    if (!facRow)
        Text(right, yy, m.tDist, 0.032f * H * k, Argb(kInk, 1), Right);
    yy += 0.036f * H * k;
    if (facRow)
    {
        TextMarquee(tx, yy, m.tFaction, 0.026f * H * k, right - tx - distW, Argb(m.tCol, 1));
        Text(right, yy, m.tDist, 0.032f * H * k, Argb(kInk, 1), Right);
        yy += 0.034f * H * k;
    }
    if (m.tBars) // only for what can be damaged
    {
        const float bw = right - tx - 0.052f * H * k;
        wchar_t buf[16];
        if (m.tShield >= 0)
        {
            SegBar(tx, yy - 0.005f * H * k, bw, 0.009f * H * k, m.tShield, kShield, 20);
            swprintf_s(buf, L"%d", static_cast<int>(m.tShield * 100 + 0.5f));
            Text(right, yy, buf, T, Argb(kShield, 1), Right);
        }
        yy += std::max(0.024f * H * k, std::max(T, TextFloor())); // (never closer than the numbers' own height)
        if (m.tHull >= 0)
        {
            SegBar(tx, yy - 0.005f * H * k, bw, 0.009f * H * k, m.tHull, kHull, 20);
            swprintf_s(buf, L"%d", static_cast<int>(m.tHull * 100 + 0.5f));
            Text(right, yy, buf, T, Argb(kHull, 1), Right);
        }
    }
    // (the threat list under the card: hud2_click.cpp DrawList; the keyboard form's close / next icons: DrawCardIcons)
}

// The left bumper's two modals where the target card would be, ALWAYS while no target is selected (the pad only):
// [LB] TARGETS  [LB hold] WHEEL, right-aligned on the card's top line - faded in calm flight, full in combat (the
// owner, 26 Oct 2026; kept off the reticle ring: "let's not make the ring TOO busy")
void DrawCalmHints(const Model& m, const Layout& L, float a, float textH)
{
    if (m.kbm || m.target)
        return;
    SetAlpha(a * (m.state == State::Combat ? 1.0f : 0.45f));
    const float H = L.H, k = L.k, T = textH * H * k, gh = T * 1.3f, gap = 0.006f * H * k, sep = 0.03f * H * k;
    const float y = L.tgtY0 + 0.02f * H * k;
    float x = L.tgtX1;
    const struct
    {
        const Prompt* p;
        const wchar_t* s;
    } items[2] = {{&m.wheelBtn, L"WHEEL"}, {&m.pickerBtn, L"TARGETS"}}; // right to left
    for (const auto& it : items)
    {
        if (it.p->btn == glyphs::COUNT)
            continue;
        const RecUnit unit; // (the prompt and its own label)
        const float g = DrawPrompt(*it.p, 0, 0, gh, 0, true);
        x -= TextWidth(it.s, T, 0.1f);
        Text(x, y, it.s, T, Argb(kDim, 1), Left, 0.1f);
        x -= gap + g / 2;
        DrawPrompt(*it.p, x, y, gh, Argb(kDim, 1));
        x -= g / 2 + sep;
    }
}

// The enemies cue (hud2.cpp EnemyCue: the owner): right-aligned under the hints' row, in the card's corner,
// two lines - "ENEMIES NEARBY" with the count in hostile red, then the press that takes the nearest ([RS] TARGET
// NEAREST); no plate (the hints have none). Both rows end at the card's right edge; the bottom row's bottom stays above
// kEnemyCueBottom (the HUD bus's keep-out).
void DrawEnemyCue(const Model& m, const Layout& L, float a)
{
    SetAlpha(a);
    const float H = L.H, k = L.k, T = 0.026f * H * k, gh = T * 1.3f, gap = 0.006f * H * k;
    const float right = L.tgtX1, y1 = L.tgtY0 + 0.068f * H * k, y2 = L.tgtY0 + 0.112f * H * k; // (0.01 H apart: lint)
    wchar_t head[40];
    swprintf_s(head, m.enemies > 1 ? L"%d ENEMIES NEARBY" : L"ENEMY NEARBY", m.enemies);
    Text(right, y1, head, T, Argb(kHostile, 1), Right, 0.1f);
    const RecUnit unit; // (the prompt and its own label)
    const float tw = TextWidth(L"TARGET NEAREST", T, 0.1f), g = DrawPrompt(m.nearestBtn, 0, 0, gh, 0, true);
    Text(right, y2, L"TARGET NEAREST", T, Argb(kInk, 1), Right, 0.1f);
    DrawPrompt(m.nearestBtn, right - tw - gap - g / 2, y2, gh, Argb(kInk, 1));
}

// ---------------------------------------------------------------- bottom-left: your ship
constexpr float kEscortInset = 0.016f; // the escorts' rows from the plate's left edge
void DrawShip(const Model& m, const Layout& L, float a)
{
    SetAlpha(a);
    const float H = L.H, k = L.k, T = 0.024f * H * k;
    const float x0 = L.shipX0, y0 = L.shipY0, x1 = L.shipX1, y1 = L.shipY1;
    const float right = x1 - 0.014f * H * k;
    // the escorts' rows: a slim plate sized to them, the names and bars with no silhouette column (UI_HUD2_SIZE_AUDIT.md 3.4), the rows at a 0.042 pitch from
    // 0.026, each bar 0.018 under its name
    if (m.nEscorts > 0)
    {
        Plate(x0, y0, x1, y1, 0.012f * H * k, kTL | kBR, 200 / 255.0f);
        const float tx = x0 + kEscortInset * H * k;
        float yy = y0 + 0.026f * H * k;
        for (int i = 0; i < m.nEscorts && i < 3; ++i)
        {
            TextMarquee(tx, yy, m.escorts[i].name, T, right - tx, Argb(kInk, 1)); // (a long name: the fit rule)
            SetAlpha(a * (1 - m.fx.glowEscort[i])); // (pips only, at half depth)
            SegBar(tx, yy + 0.018f * H * k, right - tx, 0.007f * H * k, m.escorts[i].hull, kFriend, 20, 2); // (a target's hull bar: the same pips)
            SetAlpha(a);
            yy += 0.042f * H * k;
        }
        return;
    }
    // the ship in a fight: its view with the damage (or the flat icon), its name and class; hold and credits are in the base's strip
    const float vw = hud2layout::std_ov::kShipViewW * H * k;
    const float icon = hud2layout::std_ov::kShipH * 0.5f * H * k;
    if (m.shipView)
        WindowPlate(x0, y0, x1, y1, 0.012f * H * k, kTL | kBR, 200 / 255.0f, kChrome, 0.43f, 0, 0,
                    x0 + 0.012f * H * k + vw + 0.006f * H * k);
    else
        Plate(x0, y0, x1, y1, 0.012f * H * k, kTL | kBR, 200 / 255.0f);
    if (!m.shipView && !Icon(flicons::Fighter, x0 + 0.012f * H * k + vw / 2, (y0 + y1) / 2, icon, Argb(kChrome, 0.9f)))
        Disc(x0 + 0.012f * H * k + vw / 2, (y0 + y1) / 2, icon * 0.16f, Argb(kChrome, 0.9f));
    const float tx = x0 + 0.012f * H * k + vw + 0.010f * H * k, yy = (y0 + y1) / 2;
    if (!m.shipName.empty() || !m.shipClass.empty())
    {
        const float cw = m.shipClass.empty() ? 0 : TextWidth(m.shipClass, T, 0.08f) + 0.012f * H * k;
        TextMarquee(tx, yy, m.shipName, 0.026f * H * k, hud2layout::RowRoom(tx, right, cw, 0, false, 0, 0), Argb(kInk, 1));
        if (cw > 0)
            Text(right, yy, m.shipClass, T, Argb(kDim, 1), Right, 0.08f);
    }
}

// ---------------------------------------------------------------- bottom-right: what you fire and use
// The nanobots: a wrench (the mock-up's repair tool; the round 3 critique: the old repair cross read as the D-pad's cross
// beside it) - a diagonal handle, an open jaw ring at its upper end, a round end below.
static void NanoShape(float cx, float cy, float s, u32 c)
{
    const float lw = std::max(1.5f, s * 0.11f), e = s * 0.34f;
    const float hx = cx + e * 0.62f, hy = cy - e * 0.62f; // the jaw's centre
    Line(cx - e, cy + e, hx - s * 0.10f, hy + s * 0.10f, lw * 1.3f, c);
    Arc(hx, hy, s * 0.15f, -15, 285, lw, c); // the jaw open toward the upper right (0 = 3 o'clock, clockwise)
    Disc(cx - e, cy + e, lw * 0.9f, c);
}
static void BatteryShape(float cx, float cy, float s, u32 c)
{
    const float w = s * 0.42f, h = s * 0.7f, lw = std::max(1.5f, s * 0.07f);
    const float b[8] = {cx - w / 2, cy - h / 2, cx + w / 2, cy - h / 2, cx + w / 2, cy + h / 2, cx - w / 2, cy + h / 2};
    Outline(b, 4, lw, c);
    Rect(cx - w * 0.2f, cy - h / 2 - s * 0.08f, w * 0.4f, s * 0.08f, c);
    // the bolt
    Line(cx + w * 0.12f, cy - h * 0.3f, cx - w * 0.14f, cy + h * 0.02f, lw, c);
    Line(cx - w * 0.14f, cy + h * 0.02f, cx + w * 0.14f, cy - h * 0.02f, lw, c);
    Line(cx + w * 0.14f, cy - h * 0.02f, cx - w * 0.12f, cy + h * 0.3f, lw, c);
}

// A weapon group's numeral in a cell of its own, ch tall at (ccx, yy) (Agency FB's "1" is a bare stroke: ">1" read as
// ">|"; a one is drawn with its flag and foot); lit = the active group (DrawLoadout, DrawFireStrip)
static void GroupCell(float ccx, float yy, float ch, int num, bool lit, float H, float k)
{
    const u32 nc = Argb(lit ? kActive : kDim, 1);
    Plate(ccx - ch * 0.5f, yy - ch * 0.5f, ccx + ch * 0.5f, yy + ch * 0.5f, ch * 0.18f, kTL | kTR | kBR | kBL,
          lit ? 0.35f : 0.2f, lit ? kActive : kDim, lit ? 0.9f : 0.6f);
    if (num == 1)
    {
        const float lw = std::max(1.5f, ch * 0.09f), h = ch * 0.62f;
        Line(ccx + h * 0.08f, yy - h * 0.5f, ccx + h * 0.08f, yy + h * 0.5f, lw, nc);
        Line(ccx + h * 0.08f, yy - h * 0.5f, ccx - h * 0.26f, yy - h * 0.22f, lw, nc);
        Line(ccx - h * 0.26f, yy + h * 0.5f, ccx + h * 0.40f, yy + h * 0.5f, lw, nc);
    }
    else
    {
        wchar_t n[8];
        swprintf_s(n, L"%d", num);
        Text(ccx, yy, n, 0.024f * H * k, nc, Center, 0, false);
    }
}

// One consumable slot (DrawLoadout, DrawFireStrip): its plate, a lit slot's glow (`glow`), the icon and the count - drawn
// as one designed unit with the caller's key (the caller holds the RecUnit). The count at the text floor at the slot's
// right, beside the icon; when its digits leave the icon less room than stacking does (three digits), at the lower right
// under an icon at the upper left. Two digits' room is always kept, so a count crossing 10 does not move the icon. The
// icon never lies under the count (the lint's panel.overlap; the owner, 31 Oct 2026, A1: "it's not fine for the glyphs
// to overlap ... tightening the glyphs themselves so that everything fits nicely" - before, the count at 0.030 hung
// over the slot's lower right edge and over a 0.0285 icon). `iconMax` = the icon's size where it has the room.
static void Slot(const Model& m, int i, int count, float px, float py, float sz, float iconMax, float chamfer, bool glow,
                 float H, float k)
{
    const bool hot = m.hot[i];
    const u32 hc = m.hotCol[i] ? m.hotCol[i] : kHostile;
    const bool none = count < 0; // not fitted / unknown: the slot dims
    Plate(px - sz / 2, py - sz / 2, px + sz / 2, py + sz / 2, chamfer, kTL | kTR | kBR | kBL, hot ? 220 / 255.0f : 200 / 255.0f,
          hot ? hc : kChrome, hot ? 1.0f : 0.43f, hot ? 'b' : 0, hc);
    if (hot && glow) // a lit slot glows in its meaning's colour
    {
        const float g = 0.004f * H * k;
        const float o[8] = {px - sz / 2 - g, py - sz / 2 - g, px + sz / 2 + g, py - sz / 2 - g,
                            px + sz / 2 + g, py + sz / 2 + g, px - sz / 2 - g, py + sz / 2 + g};
        Outline(o, 4, g * 1.5f, Argb(hc, 0.35f + 0.25f * Pulse(6.0f)));
    }
    // a lit slot's icon in the lit colour (amber when its answer is used up or missing: the round 4 critique)
    const u32 ic = hot ? Argb(hc, 1) : Argb(kChrome, none ? 0.35f : 1.0f);
    // an empty or missing answer reads "0" (no dash: the round 4 critique read it as a ~0.004 H mark), amber when used up
    // or when an alert names the missing slot, dim when it is simply not fitted
    wchar_t buf[16];
    swprintf_s(buf, L"%d", std::max(0, count));
    const float cs = std::max(0.024f * H * k, TextFloor()), in = 0.0015f * H * k;
    const float cw = std::max(TextWidth(buf, cs), TextWidth(L"88", cs));
    const float beside = sz - 3 * in - cw, above = sz - 2 * in - cs;
    const bool side = beside >= above;
    const float isz = std::max(0.0f, std::min(iconMax, side ? beside : above));
    const float ix = px - sz / 2 + in + isz / 2, iy = side ? py : py - sz / 2 + in + isz / 2;
    if (i == 0)
        NanoShape(ix, iy, isz, ic);
    else if (i == 1)
        BatteryShape(ix, iy, isz, ic);
    else if (!Icon(i == 2 ? flicons::Star : flicons::Mine, ix, iy, isz, ic))
        Ring(ix, iy, isz * 0.3f, 2, ic);
    const bool warn = count == 0 || (none && hot);
    Text(px + sz / 2 - in, side ? py : py + sz / 2 - cs / 2, buf, cs, warn ? Argb(kWarn, 1) : Argb(kInk, none ? 0.5f : 1.0f),
         Right);
}

// The loadout's drawn gun lines (Standard): the first unused line's offset from the panel's top in H x k units (wraps
// included), and whether the taller pitch is in use (the keyboard form's group keys, or a trigger per group) -
// LoadoutContentHeight reads them (one frame late).
static float g_loGunEnd = 0;
static float g_loPadV = 0.028f; // the air over the first row and under the last, in H x k, as drawn
constexpr float kLoadPadX = 0.018f; // the inset at both ends of the rows
static float g_loRowP = 0.03f; // the rows' pitch in H x k, as drawn

// weapon_groups 2 on the pad: the groups fire from a button each, so a row carries its own glyph.
static bool TwoTriggers(const Model& m) { return !m.kbm && m.fireBtn2.btn != glyphs::COUNT; }
// The button of a group row: game group 1 on the first trigger, 2 on the second; other groups have none.
static const Prompt* RowTrigger(const Model& m, int row)
{
    const int group = m.realGroups ? m.groupNum[row] : row + 1;
    return group == 1 ? &m.fireBtn : group == 2 ? &m.fireBtn2 : nullptr;
}

void DrawLoadout(const Model& m, const Layout& L, float a)
{
    SetAlpha(a);
    const float H = L.H, k = L.k, T = 0.024f * H * k;
    const float x0 = L.loX0, y0 = L.loY0, x1 = L.loX1, y1 = L.loY1;
    Plate(x0, y0, x1, y1, 0.012f * H * k, kTR | kBL, 190 / 255.0f);
    // the consumables' cross in its own plate stacked on the base panel's right end (
    // "LOADOUT too wide because of the consumables" - the base holds the weapons only), the cross itself unchanged,
    // centred in its plate (hud2_layout.h std_ov::kConsW / kConsH)
    // (no plate of its own: each item keeps a small dark cell, which holds over a bright planet where bare marks washed out: UI_HUD2_SIZE_AUDIT.md 3.3)
    const float qx0 = L.consX0, qy0 = L.consY0, qx1 = L.consX1, qy1 = L.consY1;
    const float d = 0.050f * H * k, sz = hud2layout::kConsCell * H * k;
    const float cx = (qx0 + qx1) / 2, cy = (qy0 + qy1) / 2;
    if (!m.kbm)
    {
        const float c = hud2layout::kConsCrossCell * 0.5f * H * k;
        Plate(cx - c, cy - c, cx + c, cy + c, 0.008f * H * k, kTL | kTR | kBR | kBL, 200 / 255.0f, kChrome, 0.43f);
        RecordTag("crossmark"); // (the cross's centre is its mark, not an element of the panel: it stands among its slots by design)
        Glyph(glyphs::COUNT, cx, cy, 0.028f * H * k, Argb(kChrome, 220 / 255.0f));
        RecordTag("loadout");
    }
    static const float kDir[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}}; // nanobots up, batteries down, CM left, mines right
    const int counts[4] = {m.nano, m.batt, m.cm, m.mines};
    // the keyboard form: a 2 x 2 grid of [key] slot (the keys at the 0.024 H floor no longer fit a slot's corner):
    // nanobots / batteries in the left column, countermeasures / mines in the right
    // (the keys tight: hud2kit::Keycap - the owner's A1); 0.018 H x k from the plate's left edge (the
    // plate's width is the grid's: hud2_layout.h kConsWKbm + 2 keys)
    const float kKeyW = SlotKeyWidth(m, L, false);
    const float cellW = kKeyW + 0.008f * H * k + sz, gx0 = qx0 + 0.018f * H * k;
    // (the cross's vertical arm 0.88 d, as in the 0.15 H plate it sat in before: the slots keep their size; the counts
    // inside their slots (Slot) clear the centre glyph by 0.018 H)
    for (int i = 0; i < 4; ++i)
    {
        float px = cx + kDir[i][0] * d * 1.15f, py = cy + kDir[i][1] * d * 0.88f;
        if (m.kbm)
        {
            const int col = i >= 2, row = i & 1;
            px = gx0 + col * (cellW + 0.016f * H * k) + kKeyW + 0.008f * H * k + sz / 2;
            py = cy + (row ? 1.0f : -1.0f) * (sz * 0.5f + 0.006f * H * k);
        }
        const RecUnit unit; // (the slot, its icon, its count and its key: one designed unit)
        Slot(m, i, counts[i], px, py, sz, sz * 0.62f, 0.008f * H * k, true, H, k);
        if (m.kbm && !m.slotBtn[i].key.empty()) // the keyboard form: the key left of the slot
        {
            const float kw = DrawPrompt(m.slotBtn[i], 0, 0, 0.026f * H * k, 0, true, true);
            DrawPrompt(m.slotBtn[i], px - sz / 2 - 0.008f * H * k - kw / 2, py, 0.026f * H * k, Argb(kChrome, 1), false, true);
        }
        if (m.kbm) // a click uses it (hud2_click.cpp)
        {
            if (HotHovered() == kHotSlot + i)
            {
                const float o[8] = {px - sz / 2, py - sz / 2, px + sz / 2, py - sz / 2, px + sz / 2, py + sz / 2, px - sz / 2, py + sz / 2};
                Outline(o, 4, std::max(1.5f, 0.002f * H * k), Argb(kInk, 0.9f));
            }
            HotRect(kHotSlot + i, px - sz / 2, py - sz / 2, px + sz / 2, py + sz / 2);
        }
    }
    // the weapons: the base panel's whole width (no divider: the cross has its own plate since decision 3)
    const float wx = x0 + kLoadPadX * H * k; // (the same inset as the right end: the owner, 6 Nov 2026)
    const float g = 0.036f * H * k;
    // the keyboard form's group keys (USER_WEAPON_GROUPn, tight keycaps at the rows' right end): their column's width
    // (0 = none bound, or the pad)
    const bool click = m.kbm && m.realGroups;
    float keyCol = 0;
    for (int i = 0; click && i < m.nGroups && i < 3; ++i)
        keyCol = std::max(keyCol, DrawPrompt(m.groupBtn[i], 0, 0, 0.024f * H * k, 0, true, true));
    const bool keyed = keyCol > 0;
    // weapon_groups 2: each group row has its own trigger glyph (GitHub issue 16). Two glyphs a row apart need the keyboard
    // form's taller pitch and the lint's 0.028 size, as stacked keycaps do.
    const bool two = TwoTriggers(m), tall = keyed || two;
    // the rows in the 0.15 H plate (hud2.cpp loH; was 0.17 H: first row 0.032, pitch 0.032, launcher 0.140): first row
    // 0.028, pitch 0.030 (0.026 text, 0.028 numeral cells), the launcher row 0.122 (its LOCKED chip 0.012 above the edge).
    // With group keys (the keyboard form; the owner, 31 Oct 2026, A1: the NUM 1 / 2 / 3 keycaps touched each other and
    // the weapon names at the 0.030 pitch - 0.039 H floor-grown caps): first row 0.022, pitch 0.039 (a 0.0288 tight cap +
    // the lint's 0.01 H between stacked caps), the launcher row 0.128; the names end 0.012 H before their key, the
    // launcher row's texts before the key column
    // (under k 1 the caps stand at the text floor, about 0.0288 H: the pitch stops with them, or NUM 1 / 2 / 3 close up, Small and 1280x800)
    const float rowP = tall ? 0.039f * std::max(k, 1.0f) * H : 0.030f * H * k;
    // the same air over the first row and under the last (the owner, 6 Nov 2026: the space below one gun row was waste)
    const int lines = std::min(m.nGroups, 3) + (m.ungrouped.empty() ? 0 : 1) + (m.torpedo.empty() ? 0 : 1) + (m.ammo >= 0 || !m.launcher.empty() ? 1 : 0);
    const float padV = tall ? (lines >= 4 ? 0.022f : 0.026f) : 0.028f; // (a full keyboard panel stands at its cap on the Large HUD: more air would close its rows up)
    float yy = y0 + padV * H * k;
    auto widthAt = [](float size) { return [size](const std::wstring& t) { return TextWidth(t, size); }; };
    if (m.nGroups > 0)
    {
        const float fw = DrawPrompt(m.fireBtn, 0, 0, g, 0, true, true);
        // RT beside the group rows (between the first two, as the mock-up; on the row when there is one)
        if (!two)
            DrawPrompt(m.fireBtn, wx + fw / 2, yy + (m.nGroups > 1 ? (keyed ? rowP * 0.5f : 0.02f * H * k) : 0.0f), g,
                       Argb(kChrome, 1), false, true);
        const float tx = wx + std::max(g, fw) + 0.02f * H * k, rx = x1 - 0.018f * H * k;
        const float ts = 0.026f * H * k, nameX = tx + 0.052f * H * k;
        // Three lines above the launcher: a row whose guns do not fit wraps one gun per line onto a spare line (at most
        // two lines a row) before anything is cut (the round 4 critique: "Azrael..." cut with lines left)
        const int rows = std::min(m.nGroups, 3) + (m.ungrouped.empty() ? 0 : 1) + (m.torpedo.empty() ? 0 : 1);
        int spare = std::max(0, 3 - rows);
        // a row's text over one or two lines: the guns are joined by the loadout's separator (hud2.cpp addName)
        auto drawNames = [&](const std::wstring& s, float room, u32 col)
        {
            static const std::wstring kSep = L"  \x00B7  "; // (an escape: the build reads the source as cp1252)
            if (TextWidth(s, std::min(ts, TextFloor())) <= room || spare <= 0 || s.find(kSep) == std::wstring::npos)
            {
                TextMarquee(nameX, yy, s, ts, room, col);
                return;
            }
            // the first line takes the guns that fit (at least one), the second line the rest (hud2layout::TwoLineSplit)
            const size_t at = hud2layout::TwoLineSplit(s, room, kSep, widthAt(ts));
            if (at == std::wstring::npos)
            {
                TextMarquee(nameX, yy, s, ts, room, col);
                return;
            }
            TextMarquee(nameX, yy, s.substr(0, at - kSep.size()), ts, room, col);
            yy += rowP, --spare;
            TextMarquee(nameX, yy, s.substr(at), ts, room, col);
        };
        for (int i = 0; i < m.nGroups && i < 3; ++i)
        {
            const bool lit = i == m.activeGroup;
            const bool hov = click && HotHovered() == kHotGroup + i;
            const float rowY = yy;
            if (const Prompt* fire = two ? RowTrigger(m, i) : nullptr)
                DrawPrompt(*fire, wx + fw / 2, yy, 0.028f * H * k, Argb(kChrome, 1), false, true);
            // the group's numeral in a cell of its own (Agency FB's "1" is a bare stroke: ">1" read as ">|"); a one is
            // drawn with its flag and foot; the active group's marker a yellow triangle left of the cell
            const float ch = 0.028f * H * k, ccx = tx + 0.012f * H * k + ch * 0.5f;
            GroupCell(ccx, yy, ch, m.realGroups ? m.groupNum[i] : i + 1, lit, H, k);
            if (lit)
            {
                const float s = 0.008f * H * k;
                Tri(tx - s * 0.2f, yy - s, tx - s * 0.2f, yy + s, tx + s * 1.1f, yy, Argb(kActive, 1));
            }
            // the keyboard form: the group's key at the row's end (USER_WEAPON_GROUPn), when one is bound
            const float kw = click ? DrawPrompt(m.groupBtn[i], 0, 0, 0.024f * H * k, 0, true, true) : 0;
            if (kw > 0)
                DrawPrompt(m.groupBtn[i], rx - kw / 2, yy, 0.024f * H * k, Argb(kChrome, 0.9f), false, true);
            const float room = rx - nameX - (kw > 0 ? kw + 0.012f * H * k : 0);
            drawNames(m.groups[i], room, Argb(lit ? kInk : kDim, 1));
            const float hh = std::min(0.015f * H * k, rowP * 0.5f); // (the hover / click row: half a pitch)
            if (hov)
                Plate(tx - 0.008f * H * k, rowY - hh, rx + 0.006f * H * k, yy + hh, 0.004f * H * k, kTL | kTR | kBR | kBL,
                      0.45f, kInk, 0.6f);
            if (click)
                HotRect(kHotGroup + i, tx - 0.008f * H * k, rowY - hh, rx + 0.006f * H * k, yy + hh);
            yy += rowP;
        }
        if (!m.ungrouped.empty()) // the guns in no group, dimmed under the groups (no label: no placeholder text)
        {
            drawNames(m.ungrouped, rx - nameX, Argb(kDim, 0.8f));
            yy += rowP;
        }
    }
    // the torpedo row on the next gun line (hud2.cpp gives it one: two group rows on a torpedo ship; GitHub issue 5,
    // 4 Nov 2026), its name (with how it fires: "HOLD", the key) and count in the launcher row's columns. On the
    // launcher's button (RB: tap missiles, held torpedoes) one glyph between the two rows, as RT beside the gun rows:
    // two 0.036 glyphs do not fit the 0.030 pitch (the lint's panel.overlap)
    const bool ordnanceShare = !m.torpedo.empty() && (m.ammo >= 0 || !m.launcher.empty()) &&
                               m.torpedoBtn.btn != glyphs::COUNT && m.torpedoBtn.btn == m.launchBtn.btn;
    const float torpedoY = yy;
    const float ordW = std::max(DrawPrompt(m.launchBtn, 0, 0, g, 0, true, true),
                                m.torpedo.empty() ? 0.0f : DrawPrompt(m.torpedoBtn, 0, 0, g, 0, true, true));
    if (!m.torpedo.empty())
    {
        if (!ordnanceShare)
            DrawPrompt(m.torpedoBtn, wx + ordW / 2, yy, g, Argb(kChrome, 1), false, true);
        const float rx = x1 - 0.018f * H * k - (keyed ? keyCol + 0.012f * H * k : 0.0f), ls = 0.026f * H * k;
        const std::wstring ammo = std::to_wstring(std::max(0, m.torpedoAmmo));
        const float aw = TextWidth(ammo, ls);
        // the gun names' column; a name too long for it slides left into the group cell's column, empty on this row, only as
        // far as it needs ("OLD - STARKILLER TORPI..." scrolled beside that empty room; alignment
        // gives way to the whole name)
        const float gunCol = wx + std::max(g, ordW) + 0.02f * H * k + 0.052f * H * k;
        const float tightCol = wx + std::max(g, ordW) + 0.012f * H * k;
        const float amGap = std::max(0.010f * H * k, 0.0105f * H); // (the lint's 0.01 H at the floor too)
        const float nx = std::max(tightCol, std::min(gunCol, rx - aw - amGap - TextWidth(m.torpedo, ls)));
        Text(rx, yy, ammo, ls, Argb(m.torpedoAmmo > 0 ? kInk : kAmber, 1), Right);
        TextMarquee(nx, yy, m.torpedo, ls, hud2layout::RowRoom(nx, rx, aw, amGap, false, 0, 0), Argb(kInk, 1));
        yy += rowP;
    }
    // the launcher row where the mock-up puts it (under three rows), whatever the group count
    const float freeY = yy;                       // (the first gun line the rows above left unused)
    yy = y0 + (tall ? 0.128f : 0.122f) * H * k; // (at most three rows above it: groups + the guns in no group)
    // 0.01 H under the last row's numeral and caps, however that row fell, pad and keyboard: the panel grows for it (ContentGrow)
    yy = std::max(yy, freeY - rowP + 0.0355f * std::max(k, 1.0f) * H);
    const float launchNeed = yy;
    // the panel's height fits its rows (0.2.1 round 6; LoadoutContentHeight): Standard's launcher row keeps its pad above
    // the bottom edge, so a shorter panel lifts it under the last gun row (at the full 0.15 the same place as before)
    if (m.presetDrawn != hud2preset::Focus)
    {
        yy = std::min(yy, y1 - padV * H * k);
        // (the height the rows need: the launcher's line where there is one, not one more pitch: that overran S1, p2n)
        g_loGunEnd = ((m.ammo >= 0 || !m.launcher.empty() ? launchNeed : freeY) - y0) / (H * k), g_loPadV = padV, g_loRowP = rowP / (H * k);
    }
    // the gun line above the launcher row is the panel's spare height when the rows did not use it (0.2.1 round 5)
    const bool lineAbove = freeY <= yy - rowP + 0.5f;
    if (m.ammo >= 0 || !m.launcher.empty())
    {
        const float lw = ordW;
        DrawPrompt(m.launchBtn, wx + lw / 2, ordnanceShare ? (torpedoY + yy) * 0.5f : yy, g, Argb(kChrome, 1), false, true);
        const float tx = wx + std::max(g, lw) + 0.02f * H * k;
        // the missile: an arrow (the mock's icon)
        const float ax = tx + 0.002f * H * k, al = 0.028f * H * k, lt = std::max(1.5f, 0.003f * H * k);
        Line(ax, yy, ax + al, yy, lt, Argb(kInk, 1));
        Line(ax, yy, ax + al * 0.3f, yy - al * 0.22f, lt, Argb(kInk, 1));
        Line(ax, yy, ax + al * 0.3f, yy + al * 0.22f, lt, Argb(kInk, 1));
        // three columns (the round 3 critique: the ammo dropped and the name cut when LOCKED showed): the name (cut to
        // its room), the ammo always, LOCKED in its own column at the right edge (its room kept while a launcher is on);
        // with the group keys, the right edge is the key column's left - 0.012 H (nothing under the keys: panel.overlap)
        const float rx = x1 - 0.018f * H * k - (keyed ? keyCol + 0.012f * H * k : 0.0f);
        const float lockW = TextWidth(L"LOCKED", T, 0.06f) + 0.016f * H * k;
        // (0.2.1 round 4, the owner: a label scrolls only past its row's REAL free room - LOCKED's column is taken only
        // while it shows; unlocked, the ammo sits at the right edge and the name gets the room the gun rows above use)
        const float ammoR = rx - (m.locked ? lockW + 0.012f * H * k : 0.0f);
        std::wstring ammo = m.ammo >= 0 ? std::to_wstring(m.ammo) : L"";
        const float aw = ammo.empty() ? 0 : TextWidth(ammo, 0.026f * H * k);
        if (!ammo.empty())
            Text(ammoR, yy, ammo, 0.026f * H * k, Argb(m.ammo > 0 ? kInk : kAmber, 1), Right);
        const float nx = tx + 0.048f * H * k, // (clear of the group numeral's column: 2.6 px at 1280x800, the lint's panel.overlap)
                    room = hud2layout::RowRoom(nx, rx, aw, 0.010f * H * k, m.locked, lockW, 0.012f * H * k);
        // the full name by the fit rule, a marquee where it would be cut (no
        // permanent cut - the mock-up's word drop, "JAVELIN MISSILE" -> "JAVELIN", hid the rest for good)
        // too long for its row: its first words on the free gun line above (the whole width to the right edge: no ammo
        // or LOCKED there), the rest on its own row; a marquee only when it still does not fit (0.2.1 round 5)
        const float ls = 0.026f * H * k;
        const size_t at = lineAbove && TextWidth(m.launcher, ls) > room
                              ? hud2layout::TwoLineSplit(m.launcher, hud2layout::RowRoom(nx, rx, 0, 0, false, 0, 0),
                                                         std::wstring(L" "), widthAt(ls))
                              : std::wstring::npos;
        if (at == std::wstring::npos)
            TextMarquee(nx, yy, m.launcher, ls, room, Argb(kInk, 1));
        else
        {
            std::wstring first = m.launcher.substr(0, at);
            while (!first.empty() && first.back() == L' ')
                first.pop_back();
            TextMarquee(nx, yy - rowP, first, ls, hud2layout::RowRoom(nx, rx, 0, 0, false, 0, 0), Argb(kInk, 1));
            TextMarquee(nx, yy, m.launcher.substr(at), ls, room, Argb(kInk, 1));
        }
        if (m.locked)
        {
            const float lx = rx - lockW;
            Plate(lx, yy - 0.016f * H * k, rx, yy + 0.016f * H * k, 0.005f * H * k, kTL | kTR | kBR | kBL,
                  230 / 255.0f, kHostile, 1.0f);
            Text(lx + 0.008f * H * k, yy, L"LOCKED", T, Argb(kHostile, 1), Left, 0.06f, false);
        }
    }
}

// ---------------------------------------------------------------- the panels' content widths (0.2.1 round 3)
// Each mirrors its Draw* function's columns: the left inset + the icon / view column + the widest row + the right pad.
float CardContentWidth(const Model& m, const Layout& L)
{
    if (!m.target)
        return 0;
    const float H = L.H, k = L.k, T = 0.024f * H * k;
    const float lead = CardColumn(m, L) + 0.026f * H * k, pad = 0.018f * H * k; // DrawTarget's tx, right
    std::wstring cls = m.tClass;
    if (m.tLevel >= 0)
    {
        wchar_t lv[24];
        swprintf_s(lv, L"  \x00B7  LEVEL %d", m.tLevel);
        cls += lv;
    }
    const float chip = TextWidth(m.tAtt, T, 0.1f) + 0.016f * H * k;
    float w = chip + (cls.empty() ? 0.0f : 0.012f * H * k + TextWidth(cls, T, 0.06f));
    const float distW = TextWidth(m.tDist, 0.032f * H * k) + 0.016f * H * k;
    w = std::max(w, TextWidth(m.tName, 0.032f * H * k) + distW); // (the name clear of the distance, on its row or under it)
    if (!m.tFaction.empty())
        w = std::max(w, TextWidth(m.tFaction, 0.026f * H * k) + distW);
    if (m.tBars)
        w = std::max(w, 0.15f * H * k + 0.052f * H * k); // (the bars keep 0.15 H x k)
    if (!m.kbm && m.clearBtn.btn != glyphs::COUNT && !m.tBars)
        w = std::max(w, DrawPrompt(m.clearBtn, 0, 0, T * 1.2f, 0, true) + 0.006f * H * k + TextWidth(L"x2 CLEAR", T, 0.06f));
    float total = lead + w + pad;
    if (m.kbm)
        total = std::max(total, CardIconsContentWidth(m, L));
    return total;
}

float ShipContentWidth(const Model& m, const Layout& L)
{
    const float H = L.H, k = L.k, T = 0.024f * H * k;
    const float lead = 0.012f * H * k + hud2layout::std_ov::kShipViewW * H * k + 0.010f * H * k, pad = 0.014f * H * k;
    float w = 0;
    if (m.nEscorts > 0)
    {
        for (int i = 0; i < m.nEscorts && i < 3; ++i)
            w = std::max(w, TextWidth(m.escorts[i].name, T));
        return kEscortInset * H * k + std::max(w, 0.10f * H * k) + pad;
    }
    if (!m.shipName.empty() || !m.shipClass.empty())
        w = TextWidth(m.shipName, 0.026f * H * k) +
            (m.shipClass.empty() ? 0.0f : TextWidth(m.shipClass, T, 0.08f) + 0.012f * H * k);
    return w > 0 ? lead + w + pad : 0.0f;
}

// The panels' content heights (0.2.1 round 6, the owner: "a loadout with one gun row and no launcher has a big empty
// area below"; UI_HUD2.md 5.29), pixels, floored at hud2layout::kShipMinH / kLoadMinH; the layout caps them at the full
// 0.15 H. The ship panel: its rows' fixed places (DrawShip) + 0.015 H under the last; the loadout: its drawn gun lines
// (DrawLoadout's g_loGunEnd), the launcher row one pitch under the last + its pad, else the last row + the pad.
float CardFootUp(const Layout& L) { return hud2layout::kCardAir * L.H * L.k + 0.0145f * (L.k > 1 ? L.H * L.k : L.H); }

// DrawTarget's rows: the chip 0.030, the name 0.070, the faction 0.106, then the bars, each row only if it shows
float CardContentHeight(const Model& m, const Layout& L)
{
    if (!m.target)
        return 0;
    const float H = L.H, k = L.k, hk = H * k, T = 0.024f * hk, line = std::max(T, TextFloor());
    const float rows = m.tFaction.empty() ? 0.070f : 0.106f;
    float h = rows * hk + std::max(0.016f * hk, TextFloor() / 2); // (the name's or the faction's row)
    if (m.tBars)
        h = (rows + 0.034f + (m.tFaction.empty() ? 0.002f : 0.0f)) * hk + (m.tHull >= 0 ? std::max(0.024f * hk, line) : 0.0f) + line / 2;
    if (m.kbm || (m.clearBtn.btn != glyphs::COUNT && !m.tBars)) // the foot row, 0.01 H under them
        h += 0.01f * H + 0.029f * (k > 1 ? hk : H);
    return h + hud2layout::kCardAir * hk;
}

float ShipContentHeight(const Model& m, const Layout& L)
{
    if (m.nEscorts > 0) // (its rows alone: the floor below is the ship icon's)
        return (0.026f + (std::min(m.nEscorts, 3) - 1) * 0.042f + 0.0215f + 0.018f) * L.H * L.k;
    return std::max(0.058f, hud2layout::kShipMinH) * L.H * L.k;
}

float LoadoutContentHeight(const Model& m, const Layout& L)
{
    if (g_loGunEnd <= 0)
        return 0; // not drawn yet: the maximum
    const float pad = g_loPadV, rowP = g_loRowP;
    const bool launcher = m.ammo >= 0 || !m.launcher.empty();
    const float h = launcher ? g_loGunEnd + pad : (m.nGroups > 0 || !m.ungrouped.empty() ? g_loGunEnd - rowP + pad : 0.0f);
    return std::max(h, hud2layout::kLoadMinH) * L.H * L.k;
}

float LoadoutContentWidth(const Model& m, const Layout& L)
{
    const float H = L.H, k = L.k, T = 0.024f * H * k, ts = 0.026f * H * k, g = 0.036f * H * k;
    const float wx = kLoadPadX * H * k, pad = kLoadPadX * H * k;
    const bool click = m.kbm && m.realGroups;
    float keyCol = 0;
    for (int i = 0; click && i < m.nGroups && i < 3; ++i)
        keyCol = std::max(keyCol, DrawPrompt(m.groupBtn[i], 0, 0, 0.024f * H * k, 0, true, true));
    float right = 0; // the widest row's right end from the panel's left edge
    if (m.nGroups > 0)
    {
        const float fw = DrawPrompt(m.fireBtn, 0, 0, g, 0, true, true);
        const float nameX = wx + std::max(g, fw) + 0.02f * H * k + 0.052f * H * k;
        // a row's names on one line (0.2.1 round 5, the owner: the panel's allotted width is used first - DrawLoadout's
        // drawNames wraps onto a spare line only past the width the panel keeps anyway, its maximum)
        auto namesW = [&](const std::wstring& s) { return TextWidth(s, ts); };
        for (int i = 0; i < m.nGroups && i < 3; ++i)
        {
            const float kw = click ? DrawPrompt(m.groupBtn[i], 0, 0, 0.024f * H * k, 0, true, true) : 0;
            right = std::max(right, nameX + namesW(m.groups[i]) + (kw > 0 ? kw + 0.012f * H * k : 0.0f));
        }
        if (!m.ungrouped.empty())
            right = std::max(right, nameX + namesW(m.ungrouped));
    }
    const float ordW = std::max(DrawPrompt(m.launchBtn, 0, 0, g, 0, true, true),
                                m.torpedo.empty() ? 0.0f : DrawPrompt(m.torpedoBtn, 0, 0, g, 0, true, true));
    if (!m.torpedo.empty())
        right = std::max(right, wx + std::max(g, ordW) + 0.02f * H * k + 0.052f * H * k + TextWidth(m.torpedo, ts) +
                                    0.010f * H * k + TextWidth(std::to_wstring(std::max(0, m.torpedoAmmo)), ts) +
                                    (keyCol > 0 ? keyCol + 0.012f * H * k : 0.0f));
    if (m.ammo >= 0 || !m.launcher.empty())
    {
        const float nx = wx + std::max(g, ordW) + 0.02f * H * k + 0.048f * H * k;
        const float aw = m.ammo >= 0 ? TextWidth(std::to_wstring(m.ammo), ts) : 0.0f;
        const float lockW = TextWidth(L"LOCKED", T, 0.06f) + 0.016f * H * k;
        right = std::max(right, nx + TextWidth(m.launcher, ts) + 0.010f * H * k + aw +
                                    (m.locked ? 0.012f * H * k + lockW : 0.0f) + // (LOCKED's column only while it shows)
                                    (keyCol > 0 ? keyCol + 0.012f * H * k : 0.0f));
    }
    return right > 0 ? right + pad : 0.0f;
}

// ---------------------------------------------------------------- the context line under the player's ship
// The line steps aside while the player types: the game's chat input `CTID` is open (it exists only then; its state
// bit 1 = drawn - hudvis reads it the same way) or FLvibed's on-screen keyboard is up - the line sat on the Console
// field and under the OSK's prompt bar (Extended HUD 2 verify). CTID looked up at 4 Hz.
static bool Typing()
{
    if (views::OskOpen())
        return true;
    static bool cache = false;
    static DWORD at = 0;
    const DWORD now = GetTickCount();
    if (now - at >= 250)
    {
        at = now;
        u8 st = 0;
        const uintptr_t c = ui::FindControl("CTID");
        cache = c && safemem::Read(c + 0x6C, &st, 1) && (st & 2);
    }
    return cache;
}

// The line's sizes (fractions of H x k): glyph 0.036, label 0.026, the HOLD word 0.024 (the kit clamps every text to
// the 0.024 H floor), 0.03 between prompts.
static float CtxGlyph(const Layout& L) { return 0.036f * L.H * L.k; }
static float CtxLabel(const Layout& L) { return 0.026f * L.H * L.k; }
static float CtxHold(const Layout& L) { return 0.024f * L.H * L.k; }
float ContextGap(const Layout& L) { return 0.03f * L.H * L.k; }
float ContextWordSize(const Layout& L) { return std::max(CtxHold(L), TextFloor()); }

bool ContextWidths(const Model& m, const Layout& L, float* widths, float* glyphW, float* labelW, bool* hold,
                   promptfit::HoldDims& d)
{
    if (Typing())
        return false;
    const float g = CtxGlyph(L);
    // the word HOLD before a hold action's glyph, small and dim; where it would cost a prompt the
    // cascade of promptfit.h places it (hud2.cpp ComputeLayout)
    d.word = TextWidth(L"HOLD", CtxHold(L), 0.06f), d.gapBefore = promptfit::GapBefore(g), d.ringChord = promptfit::RingChord(g);
    for (int i = 0; i < m.nCtx && i < 3; ++i)
    {
        // (the keyboard form's keycap tight, as DrawContext draws it: UI_HUD2.md 5.22)
        const float gw = DrawPrompt(m.ctx[i], 0, 0, g, 0, true, true), lw = TextWidth(m.ctx[i].label, CtxLabel(L), 0.06f);
        widths[i] = promptfit::ContextPromptWidth(gw, g, lw);
        glyphW[i] = gw, labelW[i] = lw;
        hold[i] = m.ctx[i].hold && m.ctx[i].key.empty(); // (the keyboard form's keycaps: no hold mark)
    }
    return true;
}

void ContextLabelMins(const Model& m, const Layout& L, float* out)
{
    for (int i = 0; i < 3; ++i)
        out[i] = i < m.nCtx ? promptfit::LabelMinWidth(m.ctx[i].label, CtxLabel(L), TextFloor(),
                                                       [](const std::wstring& s, float z) { return TextWidth(s, z, 0.06f); })
                            : 0.0f;
}

// The keyboard form's consumables keys as the cross draws them (the stacked cross's width: hud2_layout.h slotKeyW)
float SlotKeyWidth(const Model& m, const Layout& L, bool focus)
{
    if (!m.kbm)
        return 0;
    float w = 0;
    for (int i = 0; i < 4; ++i)
        w = std::max(w, DrawPrompt(m.slotBtn[i], 0, 0, (focus ? 0.024f : 0.026f) * L.H * L.k, 0, true, true));
    return w;
}

// Draws the prompts ComputeLayout placed in their lanes (L.ctxX / ctxPY / ctxW,
// L.ctxPlan); a prompt placed narrower than it asked (a lane's room) draws its label as a marquee in what is left (the
// owner's decision 1). measure = the prompts' summed width alone.
float DrawContext(const Model& m, const Layout& L, float a, bool measure)
{
    const float H = L.H, k = L.k, g = CtxGlyph(L), lab = CtxLabel(L), gap = ContextGap(L), ws = ContextWordSize(L);
    if (Typing())
        return 0;
    float total = 0;
    const int n = std::min(m.nCtx, 3);
    for (int i = 0; i < n; ++i)
        total += L.ctxW[i] + (i ? gap : 0);
    if (measure || !n)
        return total;
    SetAlpha(a);
    const u32 wordArgb = Argb(kChrome, 0.85f); // the small dim word: the chrome, not the label's ink
    for (int i = 0; i < n; ++i)
    {
        const float x = L.ctxX[i], y = L.ctxPY[i];
        const RecUnit unit; // (a pill: its HOLD word, its glyph and its label)
        const promptfit::WordAt at = L.ctxPlan.at[i];
        // the glyph's width and where the label starts (the same walk as below, measured first: the label's fit decides
        // the plate's height)
        const float gw = DrawPrompt(m.ctx[i], 0, 0, g, 0, true, true);
        const float hw = TextWidth(L"HOLD", CtxHold(L), 0.06f);
        const float extra = at == promptfit::WordAt::Above ? std::max(0.0f, hw - gw) : 0.0f;
        const float lx = x + (at == promptfit::WordAt::Before ? hw + promptfit::GapBefore(g) : 0.0f) + gw + extra + g * 0.3f;
        // the label in the pill's room (its own width, or less where the lane squeezed the pill): never a marquee (the
        // owner, 2 Nov 2026, 0.2.1) - one line, shrunk, else two lines at a smaller size >= the floor, the ellipsis only
        // as the last resort (promptfit FitLabel); + 0.5 px: the measure's own rounding never cuts a label that fits
        const float room = x + L.ctxW[i] - lx + 0.5f;
        const promptfit::LabelFit lf = promptfit::FitLabel(
            m.ctx[i].label, lab, TextFloor(), room, [](const std::wstring& s, float z) { return TextWidth(s, z, 0.06f); });
        const float tall = lf.lines == 2 ? L.ctxTall : 0.0f, top = y - 0.026f * H * k - tall, yc = y - tall * 0.5f;
        Plate(x - 0.012f * H * k, top, x + L.ctxW[i] + 0.012f * H * k, y + 0.026f * H * k, 0.026f * H * k,
              kTL | kTR | kBR | kBL, 150 / 255.0f, kChrome, 0);
        float cx = x;
        if (at == promptfit::WordAt::Before)
            cx += Text(cx, yc, L"HOLD", CtxHold(L), wordArgb, Left, 0.06f) + promptfit::GapBefore(g);
        // the keyboard form's keycap is the TIGHT cap (UI_HUD2.md 5.22: the regular SHIFT+W cap, 0.10 H, pushed "[SHIFT+W]
        // CANCEL CRUISE" past the left lane at 16:9 and its label scrolled; the tight one is 0.086 H)
        if (at == promptfit::WordAt::Above) // stacked above the glyph, centred on it, just over the plate
            Text(cx + (gw + extra) / 2, top - 0.001f * H - ws / 2, L"HOLD", CtxHold(L), wordArgb, Center, 0.06f);
        else if (at == promptfit::WordAt::Ring) // (b): in the broken ring's top gap (only where it fits at the floor)
            Text(cx + gw / 2, yc - g * 0.62f, L"HOLD", CtxHold(L), wordArgb, Center, 0.06f);
        cx += extra / 2;
        DrawPrompt(m.ctx[i], cx + gw / 2, yc, g, Argb(kChrome, 1), false, true);
        cx += gw + extra / 2;
        if (at == promptfit::WordAt::OverLabel) // (b2): over the label's start, just over the plate
            Text(cx + g * 0.3f, top - 0.001f * H - ws / 2, L"HOLD", CtxHold(L), wordArgb, Left, 0.06f);
        if (lf.lines == 2)
        {
            const float d = lf.size * promptfit::kWrapPitch * 0.5f;
            Text(lx, yc - d, lf.line[0], lf.size, Argb(kInk, 1), Left, 0.06f);
            Text(lx, yc + d, lf.line[1], lf.size, Argb(kInk, 1), Left, 0.06f);
        }
        else
            Text(lx, yc, lf.line[0], lf.size, Argb(kInk, 1), Left, 0.06f);
    }
    return total;
}

// ---------------------------------------------------------------- the transient alert
constexpr float kAlertCardSec = 3.0f; // (hud2.cpp Alert)
constexpr int kAlertBloomLayers = 10;
constexpr float kAlertBloomReach = 0.024f, kAlertBloomAlpha = 0.055f; // the outer edge (H x k); each layer's alpha, they add up inward

float DrawCard(const AlertCard& c, float x0, float y0, const Layout& L, float a, bool measure)
{
    const float H = L.H, k = L.k, T = 0.024f * H * k, g = 0.032f * H * k;
    const float tw = TextWidth(c.text, 0.028f * H * k, 0.1f);
    float pw = 0;
    for (int i = 0; i < c.n; ++i)
        pw += DrawPrompt(c.prompts[i], 0, 0, g, 0, true) + g * 0.25f + TextWidth(c.prompts[i].label, T) + 0.03f * H * k;
    if (!c.none.empty())
        pw += TextWidth(c.none, T, 0.06f) + 0.03f * H * k;
    // beside a meter (GitHub issue 21): two lines, the event over its answers, so it fits every screen and hides less of the view
    const bool stacked = c.side != 0;
    const float w = stacked ? std::max(0.056f * H * k + tw, 0.02f * H * k + pw) + 0.02f * H * k
                            : 0.07f * H * k + tw + 0.03f * H * k + pw;
    if (measure || c.text.empty())
        return w;
    SetAlpha(a);
    // (a card with no answers has one line: no second line's room, and it stays centred where the two-line card stands)
    const bool twoLines = stacked && (c.n > 0 || !c.none.empty());
    const float h = (twoLines ? hud2layout::kAlertStackH : 0.05f) * H * k;
    if (stacked && !twoLines)
        y0 += (hud2layout::kAlertStackH - 0.05f) * 0.5f * H * k;
    const float yc = y0 + (twoLines ? 0.026f * H * k : h / 2);
    // The urgency, all of it under the card's words so they stay clear: a burst as it arrives, then
    // a bloom around the plate on the low meters' pulse (1.5 Hz, under the 3 Hz limit, one beat for every danger) with their
    // off switch (HUD_CUES.md)
    const float chamfer = 0.012f * H * k;
    auto shape = [](float a0, float b0, float a1, float b1, float q, u32 argb) {
        const float p[16] = {a0 + q, b0, a1 - q, b0, a1, b0 + q, a1, b1 - q, a1 - q, b1, a0 + q, b1, a0, b1 - q, a0, b0 + q};
        Poly(p, 8, argb);
    };
    float burst = 0;
    if (stacked && shieldcue::Get().on)
    {
        burst = std::clamp(1 - (kAlertCardSec - c.left) / 0.4f, 0.0f, 1.0f);
        const float glow = std::max(burst, 0.3f + 0.7f * g_beat);
        for (int i = kAlertBloomLayers; i >= 1; --i) // (many faint layers: three read as hard bands)
        {
            const float d = kAlertBloomReach * H * k * i / kAlertBloomLayers;
            shape(x0 - d, y0 - d, x0 + w + d, y0 + h + d, chamfer + d * 0.5f, Argb(c.col, kAlertBloomAlpha * glow));
        }
    }
    Plate(x0, y0, x0 + w, y0 + h, chamfer, kTL | kTR | kBR | kBL, 205 / 255.0f, kChrome, 0.43f,
          c.side < 0 ? 'r' : 'l', c.col); // (the accent on the edge that faces the ring)
    if (burst > 0)
        shape(x0, y0, x0 + w, y0 + h, chamfer, Argb(c.col, 0.3f * burst));
    {
        const RecUnit unit; // (the alert's mark and its words)
        if (!Icon(flicons::Important, x0 + 0.03f * H * k, yc, 0.03f * H * k, Argb(c.col, 1)))
            Ring(x0 + 0.03f * H * k, yc, 0.012f * H * k, 0.002f * H * k, Argb(c.col, 1));
        Text(x0 + 0.056f * H * k, yc, c.text, 0.028f * H * k, Argb(c.col, 1), Left, 0.1f, false);
    }
    float x = stacked ? x0 + 0.02f * H * k : x0 + 0.056f * H * k + tw + 0.03f * H * k;
    // the answers' line, 0.01 H under the event (the lint's gap): at 4:3 k 0.8 the text and glyphs sit at their floors, so a floor here too
    const float pyc = stacked ? y0 + std::max(0.072f * k, 0.063f) * H : yc;
    for (int i = 0; i < c.n; ++i)
    {
        const RecUnit unit; // (the answer's prompt and its own label)
        const float pw1 = DrawPrompt(c.prompts[i], 0, 0, g, 0, true);
        DrawPrompt(c.prompts[i], x + pw1 / 2, pyc, g, Argb(kChrome, 1));
        x += pw1 + g * 0.25f;
        x += Text(x, pyc, c.prompts[i].label, T, Argb(kInk, 1), Left, 0, false) + 0.03f * H * k;
    }
    if (!c.none.empty()) // the answer used up / not fitted: amber words instead of a glyph
        Text(x, pyc, c.none, T, Argb(kWarn, 1), Left, 0.06f, false); // the lit slot's amber: one meaning, one colour
    return w;
}

float DrawAlert(const Model& m, const Layout& L, float a, bool measure)
{
    AlertCard c;
    c.text = m.alert, c.none = m.alertNone, c.col = m.alertCol, c.n = m.nAlertPrompts, c.side = m.alertSide, c.left = m.alertLeft;
    for (int i = 0; i < c.n && i < 2; ++i)
        c.prompts[i] = m.alertPrompts[i];
    return DrawCard(c, L.alertX0, m.alertSide && m.alertYNow >= 0 ? m.alertYNow : L.alertY, L, a, measure);
}

// ================================================================ Focus (modules\hud\docs\UI_HUD2_PRESETS.md 2.2 "glass and edges";
// UI_HUD2.md 5.18): the parts Standard's cannot draw. Geometry from the layout core (hud2_layout.h FocusCorners), the
// reticle's numbers from the data (ui_meta.h "reticle" rows), visibility from its reveal (hud2_layout.h Reveal).
namespace
{
const uimeta::hud2p::Preset& FocusData() { return uimeta::hud2p::kPresets[hud2preset::Focus]; }
float FocusNum(const char* elem, const char* key, float fallback)
{
    return uimeta::hud2p::Num(FocusData(), "reticle", elem, key, fallback);
}
} // namespace

// The split arcs (shield left, hull right: 50 degrees each on the arcs' circle R, the drawn preset's reticle radius 0.12),
// their numbers outside each arc's middle (below 40 %, hud_numbers as Standard's), the power / cruise charge arc at the
// bottom inside the circle (it must keep the centre stack's 0.01 H from the band), the hit segment on the circle, the
// missile chevrons at 0.15 / 0.17; then the band at the centre + 0.078 (inside the circle since 5.23, the owner's lower
// centre rule - it was + 0.14, over the ship; its rows pass through the gaps between the split arcs and the bottom arc,
// > 0.01 H from each): the speed in the middle, the throttle (tick, the lever's % while it rests partial, BOOST / KILL /
// REV) to its left, the cruise state, LOOT n and in combat MSL n to its right - fixed anchors, so nothing slides when
// another item comes or goes.
void DrawSplitArcs(const Model& m, const Layout& L, float a, const FocusVis& v)
{
    const float H = L.H, k = L.k, cx = L.cx, cy = L.cy, R = L.R;
    const float w = hud2layout::kRingArcW * H * k, T = 0.024f * H * k;
    const float half = FocusNum("reticle", "arc_deg", 50.0f) * 0.5f;
    const int numbers = uiopt::Get().hudNumbers;
    auto show = [&](float x) { return x >= 0 && (numbers == 1 || (numbers == 0 && x < 0.4f)); };
    wchar_t buf[40];
    const float shield = std::max(0.0f, m.shield);
    RecordTag("ring.shield");
    const ArcKeyScope key(H);
    if (v.shield > 0.004f)
    {
        SetAlpha(a * v.shield);
        ArcDown(cx, cy, R, 180 - half, 180 + half, w, m.fx.shieldDown);
        ArcGlow(cx, cy, R, 180 - half, 180 + half, 180 - half, 180 - half + 2 * half * shield, w, kShield, m.fx.glowShield);
        SegArc(cx, cy, R, 180 - half, 180 + half, w, shield, kShield, 8, 1.6f, false);
        ArcBloom(cx, cy, R, 180 - half, 180 + half, w, kHostile, m.fx.bloomShield, H, k);
        if (m.fx.shieldDown > 0)
            ShieldsDownWords(cx, cy, R, T, m.fx.downMix);
        if (show(shield) && m.shield >= 0)
        {
            swprintf_s(buf, L"%d", static_cast<int>(shield * 100 + 0.5f));
            Text(cx - R - 0.014f * H * k, cy, buf, T, Argb(kShield, 1), Right);
        }
    }
    RecordTag("ring.hull");
    if (v.hull > 0.004f && m.hull >= 0)
    {
        SetAlpha(a * v.hull);
        ArcGlow(cx, cy, R, -half, half, half - 2 * half * m.hull, half, w, kHull, m.fx.glowHull);
        SegArc(cx, cy, R, -half, half, w, m.hull, kHull, 8, 1.6f, true);
        ArcBloom(cx, cy, R, -half, half, w, kHostile, m.fx.bloomHull, H, k);
        if (show(m.hull))
        {
            swprintf_s(buf, L"%d", static_cast<int>(m.hull * 100 + 0.5f));
            Text(cx + R + 0.014f * H * k, cy, buf, T, Argb(kHull, 1), Left);
        }
    }
    // the bottom arc on the arcs' circle (the ring's lowest drawn edge, hud2layout::RingBottom): the band moved inside the
    // circle above it (5.23 - at the centre + 0.14 it sat in the lower centre over the ship; the arc was 0.012 inside then)
    const float rp = R;
    if (m.cruise == 1)
    {
        RecordTag("ring.charge");
        SetAlpha(a);
        SegArc(cx, cy, rp, 66, 114, w, m.charge, kCyan, 10, 1.8f, true);
    }
    else if (v.power > 0.004f && m.power >= 0)
    {
        RecordTag("ring.power");
        SetAlpha(a * v.power);
        SegArcCentred(cx, cy, rp, 70, 110, w * 0.8f, m.power, kPower, 6, 2.2f);
    }
    SetAlpha(a);
    RecordTag("ring.hit"); // "a segment on the arc circle at the hit bearing" (an overlay, 1.3)
    if (m.hitAge >= 0 && m.hitAge < 0.3f)
        Arc(cx, cy, R, m.hitAng - 16, m.hitAng + 16, 0.008f * H * k, Argb(kAmber, 0.85f * (1 - m.hitAge / 0.3f)));
    RecordTag("ring.missiles");
    {
        const float pulse = 0.65f + 0.35f * Pulse(10.0f);
        const float rr[2] = {FocusNum("reticle", "chevron_r_0", 0.15f) * H * k, FocusNum("reticle", "chevron_r_1", 0.17f) * H * k};
        const float sz = 0.02f * H * k, lw = 0.0065f * H * k;
        for (int i = 0; i < m.missiles && i < 3; ++i)
        {
            const float ang = m.missileAng[i] * 3.14159265f / 180.0f, ux = cosf(ang), uy = sinf(ang), vx = -uy, vy = ux;
            for (int c = 0; c < 2; ++c)
            {
                const float px = cx + rr[c] * ux, py = cy + rr[c] * uy;
                const u32 col = Argb(kHostile, pulse * (c ? 0.69f : 1.0f));
                Line(px + vx * sz, py + vy * sz, px - ux * sz * 0.8f, py - uy * sz * 0.8f, lw, col);
                Line(px - ux * sz * 0.8f, py - uy * sz * 0.8f, px - vx * sz, py - vy * sz, lw, col);
            }
        }
    }
    // ---- the band
    if (v.band <= 0.004f || L.bandY <= 0)
        return;
    const float by = L.bandY, bt = 0.026f * H * k, gap = 0.012f * H * k, sep = 0.03f * H * k, gh = bt * 1.3f;
    const float side = 0.045f * H * k; // from the centre to each side's near edge (a 4-digit speed at 0.030 is ~0.055 wide)
    RecordTag("ring.band");
    if (v.speed > 0.004f)
    {
        SetAlpha(a * v.band * v.speed);
        swprintf_s(buf, L"%d", static_cast<int>(m.speed + 0.5f));
        Text(cx, by, buf, 0.030f * H * k, Argb(kInk, 1), Center);
    }
    // left of the speed, right to left: the throttle word (a state word: always while it holds), the lever's %, the tick
    float lx = cx - side;
    SetAlpha(a * v.band);
    const wchar_t* drive = m.kill ? L"KILL" : m.burner ? L"BOOST" : m.reverse ? L"REV" : nullptr;
    const u32 tc = m.burner ? kActive : (m.kill || m.reverse) ? kAmber : kChrome;
    if (drive)
        lx -= Text(lx, by, drive, bt, Argb(tc, 1), Right, 0.08f) + gap;
    const float thr = m.kill ? 0.0f : m.burner ? 1.0f : std::clamp(fabsf(m.throttle), 0.0f, 1.0f);
    if (v.readout && !drive)
    {
        swprintf_s(buf, L"%d %%", static_cast<int>(thr * 100 + 0.5f));
        lx -= Text(lx, by, buf, bt, Argb(kChrome, 1), Right, 0.04f) + gap;
    }
    const float tickA = std::max(v.speed, v.readout ? 1.0f : 0.0f);
    if (tickA > 0.004f) // the 3-segment throttle tick (the lever's reading: the lint's analog readout)
    {
        RecordTag("ring.throttle");
        SetAlpha(a * v.band * tickA);
        const float tw = 0.036f * H * k, th = 0.008f * H * k;
        SegBar(lx - tw, by - th / 2, tw, th, thr, tc, static_cast<int>(FocusNum("reticle", "throttle_tick_segments", 3)), 2);
        RecordTag("ring.band");
    }
    // right of the speed: the cruise state word (CRUISE 3.2 while it charges, CRUISE ON: state words, no glyph - the
    // prompt itself lives in the context line at the screen's bottom edge, the owner, 31 Oct 2026, B13: "the cruise
    // prompt itself can live at the edge of the screen rather than at the center. It's crowded."; hud2.cpp's context
    // line), LOOT n with the tractor's glyph, in combat the active launcher's count (1.7 pair 5: its one home there)
    float rx = cx + side;
    auto item = [&](const Prompt* p, const wchar_t* s, u32 col, float alpha)
    {
        const RecUnit unit; // (a prompt and its own label)
        SetAlpha(a * v.band * alpha);
        const float g = p ? DrawPrompt(*p, 0, 0, gh, 0, true) : 0;
        if (g > 0)
            DrawPrompt(*p, rx + g / 2, by, gh, col), rx += g + 0.006f * H * k;
        rx += Text(rx, by, s, bt, col, Left, 0.08f) + sep;
    };
    if (m.cruise == 1)
    {
        swprintf_s(buf, L"CRUISE %.1f", m.chargeLeft);
        item(nullptr, buf, Argb(kCyan, 1), 1);
    }
    else if (m.cruise == 2)
        item(nullptr, L"CRUISE ON", Argb(kCyan, 1), 1);
    if (m.loot > 0)
    {
        swprintf_s(buf, L"LOOT %d", m.loot);
        item(&m.tractorBtn, buf, Argb(kInk, 1), 1);
    }
    if (v.fight && m.ammo >= 0 && !m.launcher.empty())
    {
        swprintf_s(buf, L"MSL %d", m.ammo);
        item(nullptr, buf, Argb(m.ammo > 0 ? kInk : kAmber, 1), 1);
    }
}

// The compact card (PRESETS 2.2 Calm 0.34 x 0.095 / Combat 0.34 x 0.167): the flat type icon in a narrow column (no 3D
// view: a compact card has no window for one), row 1 the name and the distance (the off-screen form's line, drawn
// always until step 6's on / off-screen state), row 2 the attitude chip and the class / level, row 3 the faction in the
// reputation's colour and the pad's target lane at its right end ([RS] x2 CLEAR; in a fight [RS] NEXT before it, CLEAR
// dropped first when the faction would lose its room); in the fight form the pad's 2 threat rows under a hairline.
void DrawTargetCompact(const Model& m, const Layout& L, float a, float rowsA, bool fight)
{
    SetAlpha(a);
    const float H = L.H, k = L.k, T = 0.024f * H * k;
    const float x0 = L.tgtX0, y0 = L.tgtY0, x1 = L.tgtX1, y1 = L.tgtY1;
    Plate(x0, y0, x1, y1, 0.012f * H * k, kTR | kBL, 170 / 255.0f, kChrome, 0.43f, 'l', m.tCol);
    const float iw = 0.046f * H * k, icx = x0 + 0.012f * H * k + iw / 2, icy = y0 + 0.037f * H * k;
    if (!Icon(m.tIcon, icx, icy, 0.040f * H * k, Argb(m.tCol, 1)))
    {
        const float d = iw * 0.3f;
        const float dia[8] = {icx, icy - d, icx + d, icy, icx, icy + d, icx - d, icy};
        Outline(dia, 4, 0.003f * H * k, Argb(m.tCol, 1));
    }
    const float tx = x0 + 0.012f * H * k + iw + 0.010f * H * k, right = x1 - 0.016f * H * k;
    // the columns the rows share: row 2's class starts after the attitude chip, row 1's distance ends at the right; a
    // text of the next row ending just short of one is fitted to end 0.011 H before it (ClearRoom: diagonal neighbours,
    // the lint's panel.overlap - the in-game lint: "Manhattan" / "PLANET", "POLICE BASE" / "Liberty Police"
    // 0.0088 H apart)
    const float cw = TextWidth(m.tAtt, T, 0.1f) + 0.014f * H * k, clsX = tx + cw + 0.010f * H * k, colGap = 0.011f * H * k;
    // row 1
    float yy = y0 + 0.022f * H * k;
    const float ds = 0.026f * H * k, distW = m.tDist.empty() ? 0 : TextWidth(m.tDist, ds) + 0.012f * H * k;
    TextMarquee(tx, yy, m.tName, 0.030f * H * k, ClearRoom(m.tName, 0.030f * H * k, 0, tx, right - tx - distW, clsX, colGap),
            Argb(kInk, 1));
    if (distW > 0)
        Text(right, yy, m.tDist, ds, Argb(kInk, 1), Right);
    // row 2
    yy = y0 + 0.052f * H * k;
    Plate(tx, yy - 0.013f * H * k, tx + cw, yy + 0.013f * H * k, 0.005f * H * k, kTL | kTR | kBR | kBL, 230 / 255.0f,
          m.tCol, 1.0f);
    Text(tx + 0.007f * H * k, yy, m.tAtt, T, Argb(m.tCol, 1), Left, 0.1f, false);
    std::wstring cls = m.tClass;
    if (m.tLevel >= 0)
    {
        wchar_t lv[24];
        swprintf_s(lv, L"  LV %d", m.tLevel);
        cls += lv;
    }
    if (!cls.empty())
    {
        const float room = distW > 0 ? ClearRoom(cls, T, 0.06f, clsX, right - clsX, right - TextWidth(m.tDist, ds), colGap)
                                     : right - clsX;
        TextMarquee(clsX, yy, cls, T, room, Argb(kDim, 1), Left, 0.06f);
    }
    // row 3: the faction and the target lane (the pad). The owner, 31 Oct 2026, B4-B7: "It is important for HUD elements
    // inside cards to not overlap, neither glyphs nor contextual data" - the pad's lane glyph (now 0.0264, still above
    // the 0.024 text beside it) at 0.089 keeps the lint's 0.01 H under row 2's texts (was 0.078 with a 0.0288 glyph: its
    // top 0.0636 met row 2's 0.064; the card grows 0.012 for it: hud2_layout.h focus_ns::kCardLaneRow); the keyboard form
    // keeps 0.078 (its row 3 is text only) and its click row under it (DrawCardIcons compact: focus_ns::kCardFooterKbm),
    // where the faction line ran over the CTRL+R / CTRL+T keycaps
    yy = y0 + (m.kbm ? 0.078f : 0.089f) * H * k;
    float laneX = right;
    if (!m.kbm)
    {
        const float gh = T * 1.1f;
        const struct
        {
            const Prompt* p;
            const wchar_t* s;
        } lane[2] = {{&m.clearBtn, L"x2 CLEAR"}, {fight ? &m.nextBtn : nullptr, L"NEXT"}}; // right to left
        auto width = [&](int i)
        {
            if (!lane[i].p || lane[i].p->btn == glyphs::COUNT)
                return 0.0f;
            return DrawPrompt(*lane[i].p, 0, 0, gh, 0, true) + 0.005f * H * k + TextWidth(lane[i].s, T, 0.06f);
        };
        const float w0 = width(0), w1 = width(1), minFaction = m.tFaction.empty() ? 0.0f : 0.06f * H * k;
        const bool both = w1 > 0 && w0 > 0 && (right - tx) - (w0 + w1 + 0.02f * H * k) >= minFaction;
        for (int i = 0; i < 2; ++i)
        {
            const float wi = i == 0 ? w0 : w1;
            if (wi <= 0 || (i == 0 && w1 > 0 && !both)) // (CLEAR first to go)
                continue;
            const float lw = TextWidth(lane[i].s, T, 0.06f), g = DrawPrompt(*lane[i].p, 0, 0, gh, 0, true);
            const RecUnit unit; // (the prompt and its own label)
            Text(laneX, yy, lane[i].s, T, Argb(kDim, 1), Right, 0.06f);
            DrawPrompt(*lane[i].p, laneX - lw - 0.005f * H * k - g / 2, yy, gh, Argb(kDim, 1));
            laneX -= wi + 0.02f * H * k;
        }
    }
    if (!m.tFaction.empty()) // (the keyboard form's row 3 sits 0.002 H under row 2: clear of the class's column; the
    {                        // pad's 0.089 row is 0.013 H under it)
        const float room = laneX - tx - 0.004f * H * k;
        TextMarquee(tx, yy, m.tFaction, T, m.kbm ? ClearRoom(m.tFaction, T, 0, tx, room, clsX, colGap) : room, Argb(m.tCol, 1));
    }
    // the threat rows (the pad's fight form; faded in once the plate has grown to 80 %: PRESETS 1.2)
    if (L.listInCard && rowsA > 0.004f && m.nThreats > 0 && L.thrY1 > L.thrY0)
    {
        RecordTag("list");
        SetAlpha(a * rowsA);
        const float sepY = L.thrY0 + 0.004f * H * k, pitch = 0.032f * H * k;
        Rect(x0 + 0.016f * H * k, sepY, (x1 - x0) - 0.032f * H * k, std::max(1.0f, 0.0015f * H), Argb(kChrome, 0.35f));
        for (int i = 0; i < m.nThreats && i < 2; ++i)
        {
            const float ry = sepY + 0.004f * H * k + pitch * (i + 0.5f);
            // (0.021, was 0.024: the two rows' icons stacked at the 0.032 pitch keep the lint's 0.01 H - B4-B7)
            if (!Icon(m.threats[i].icon, x0 + 0.030f * H * k, ry, 0.021f * H * k, Argb(m.threats[i].col, 1)))
                Disc(x0 + 0.030f * H * k, ry, 0.005f * H * k, Argb(m.threats[i].col, 1));
            const float dw = TextWidth(m.threats[i].dist, T) + 0.012f * H * k;
            TextMarquee(tx, ry, m.threats[i].name, T, right - tx - dw, Argb(kInk, 1));
            Text(right, ry, m.threats[i].dist, T, Argb(kInk, 1), Right);
        }
        RecordTag("card");
    }
}

// The bottom-left free text (PRESETS 2.2: no ship panel; Combat "escorts as free text, 0.042 rows with inline bars"):
// the escort rows - a formation's leader and members, the protected ship - the last on the old hold strip's line (0.93).
// The owner, 31 Oct 2026, B9: cargo hold / credits are not needed in flight - the hold strip ("CARGO 12/40 . CREDITS
// 4 250", 4 s on a change, on a peek) is gone; the block holds the escort rows alone (in a fight, on an escort's hull
// change, on a peek) and is held back without them.
void DrawShipStrip(const Model& m, const Layout& L, float a, float escortsA)
{
    const float H = L.H, k = L.k, T = 0.026f * H * k;
    const float x = L.shipX0, right = L.shipX1;
    if (escortsA > 0.004f && m.nEscorts > 0)
    {
        SetAlpha(a * escortsA);
        const float pitch = uimeta::hud2p::Num(FocusData(), "combat", "ship", "row_h", 0.042f) * H * k;
        const float barW = 0.07f * H * k;
        const int n = std::min(m.nEscorts, 3);
        for (int i = 0; i < n; ++i)
        {
            const float y = L.holdY - pitch * (n - 1 - i);
            TextMarquee(x, y, m.escorts[i].name, T, right - x - barW - 0.012f * H * k, Argb(kInk, 1));
            SegBar(right - barW, y - 0.004f * H * k, barW, 0.008f * H * k, m.escorts[i].hull, kFriend, 8, 2);
        }
    }
}

// The fire strip (PRESETS 2.2 Combat: 0.36 x 0.14, K&M 0.40; "cross 0.12, slots 0.04, icons 0.02; the active group as
// numeral + short name; launcher name and lock, torpedo"): the D-pad cross (the keyboard form: its 2 x 2 grid of keys),
// then ALL the weapons (B16: "show all weapons incl. launchers - launchers are often in no
// active group; the player wants armament status even when focused - with a smaller font and adapted spacing so all
// fit; active group highlighted"; was the active group's first gun + the launcher): every group's row (its numeral
// cell, its guns; the active group lit, the others dim), the guns in no group (dim), the launcher's row (name, count out
// of a fight, LOCKED). Text at the 0.024 H floor (was 0.026); the rows share the plate's 0.12 H inner height at up to
// 0.040 apart, centred. No torpedo row: HUD 2 has no torpedo reader yet (1.4, step 6).
void DrawFireStrip(const Model& m, const Layout& L, float a, bool countHere)
{
    SetAlpha(a);
    const float H = L.H, k = L.k, T = 0.024f * H * k;
    const float x0 = L.loX0, y0 = L.loY0, x1 = L.loX1, y1 = L.loY1;
    Plate(x0, y0, x1, y1, 0.012f * H * k, kTR | kBL, 190 / 255.0f);
    // the cross in its own plate stacked on the strip's right end (Standard's
    // loadout likewise): slots 0.042 at 0.044 (was 0.038 at 0.040: a slot holds its icon beside a floor-sized count
    // without the two overlapping - Slot; the lint's panel.overlap), 0.130 tall in its 0.14 plate
    const float qx0 = L.consX0, qy0 = L.consY0, qx1 = L.consX1, qy1 = L.consY1;
    if (qx1 > qx0 && qy1 > qy0)
        Plate(qx0, qy0, qx1, qy1, 0.012f * H * k, kTR | kBL, 190 / 255.0f);
    const float cell = 0.044f * H * k, sz = 0.042f * H * k, pad = 0.014f * H * k;
    const float cx = (qx0 + qx1) / 2, cy = (qy0 + qy1) / 2;
    if (!m.kbm)
        Glyph(glyphs::COUNT, cx, cy, 0.022f * H * k, Argb(kChrome, 220 / 255.0f));
    static const float kDir[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}}; // nanobots up, batteries down, CM left, mines right
    const int counts[4] = {m.nano, m.batt, m.cm, m.mines};
    const float keyW = SlotKeyWidth(m, L, true);
    const float cellW = keyW + 0.006f * H * k + sz, gx0 = qx0 + pad;
    const float leftRight = x0; // (the weapons start at the strip's left edge: the cross is no longer beside them)
    const float isz = uimeta::hud2p::Num(FocusData(), "combat", "loadout", "icon", 0.02f) * H * k;
    for (int i = 0; i < 4; ++i)
    {
        float px = cx + kDir[i][0] * cell, py = cy + kDir[i][1] * cell;
        if (m.kbm)
        {
            const int col = i >= 2, row = i & 1;
            px = gx0 + col * (cellW + 0.010f * H * k) + keyW + 0.006f * H * k + sz / 2;
            py = cy + (row ? 1.0f : -1.0f) * (sz * 0.5f + 0.004f * H * k);
        }
        const RecUnit unit; // (the slot, its icon, its count and its key: one designed unit)
        Slot(m, i, counts[i], px, py, sz, isz, 0.007f * H * k, false, H, k);
        if (m.kbm)
        {
            if (!m.slotBtn[i].key.empty())
            {
                const float kw = DrawPrompt(m.slotBtn[i], 0, 0, T, 0, true, true);
                DrawPrompt(m.slotBtn[i], px - sz / 2 - 0.006f * H * k - kw / 2, py, T, Argb(kChrome, 1), false, true);
            }
            if (HotHovered() == kHotSlot + i)
            {
                const float o[8] = {px - sz / 2, py - sz / 2, px + sz / 2, py - sz / 2, px + sz / 2, py + sz / 2, px - sz / 2, py + sz / 2};
                Outline(o, 4, std::max(1.5f, 0.002f * H * k), Argb(kInk, 0.9f));
            }
            HotRect(kHotSlot + i, px - sz / 2, py - sz / 2, px + sz / 2, py + sz / 2);
        }
    }
    // the weapons: every group, the guns in no group, the launcher - the strip's whole width (no divider: decision 3)
    const float wx = leftRight + 0.020f * H * k, rx = x1 - 0.014f * H * k, g = 0.028f * H * k, ts = T;
    const int ng = std::min(m.nGroups, 3);
    const bool ug = !m.ungrouped.empty(), ln = m.ammo >= 0 || !m.launcher.empty(), tp = !m.torpedo.empty();
    const int rows = ng + (ug ? 1 : 0) + (tp ? 1 : 0) + (ln ? 1 : 0);
    if (rows == 0)
        return;
    // the pitch: the inner height shared, at most 0.040 (two prompts a row apart keep the lint's 0.01 H: 0.028 glyphs,
    // 0.0288 tight keycaps); the block centred in the plate
    const float p = std::min(0.040f * H * k, ((y1 - y0) - 0.020f * H * k) / rows);
    const float top = (y0 + y1) / 2 - p * rows * 0.5f; // (the strip's centre: the cross has its own plate)
    auto rowY = [&](int j) { return top + p * (j + 0.5f); };
    const float fw = ng > 0 ? DrawPrompt(m.fireBtn, 0, 0, g, 0, true, true) : 0.0f;
    const bool share = tp && ln && m.torpedoBtn.btn != glyphs::COUNT && m.torpedoBtn.btn == m.launchBtn.btn; // (one glyph)
    const float lw = std::max(ln ? DrawPrompt(m.launchBtn, 0, 0, g, 0, true, true) : 0.0f,
                              tp ? DrawPrompt(m.torpedoBtn, 0, 0, g, 0, true, true) : 0.0f);
    const float ch = std::min(0.026f * H * k, p - 0.004f * H * k); // the numeral cells
    const float cellX = wx + std::max(g, std::max(fw, lw)) + 0.010f * H * k, nameX = cellX + ch + 0.010f * H * k;
    if (ng > 0)
    {
        // RT beside the group rows, centred on them; a trigger per row with weapon_groups 2 (GitHub issue 16)
        const bool two = TwoTriggers(m);
        if (!two)
            DrawPrompt(m.fireBtn, wx + fw / 2, (rowY(0) + rowY(ng - 1)) * 0.5f, g, Argb(kChrome, 1), false, true);
        for (int i = 0; i < ng; ++i)
        {
            const bool lit = i == m.activeGroup;
            if (const Prompt* fire = two ? RowTrigger(m, i) : nullptr)
                DrawPrompt(*fire, wx + fw / 2, rowY(i), std::min(0.028f * H * k, p - 0.010f * H * k), Argb(kChrome, 1), false,
                           true);
            GroupCell(cellX + ch * 0.5f, rowY(i), ch, m.realGroups ? m.groupNum[i] : i + 1, lit, H, k);
            TextMarquee(nameX, rowY(i), m.groups[i], ts, rx - nameX, Argb(lit ? kInk : kDim, 1));
        }
    }
    if (ug) // the guns in no group, dimmed under the groups (no label: no placeholder text)
        TextMarquee(nameX, rowY(ng), m.ungrouped, ts, rx - nameX, Argb(kDim, 0.8f));
    if (tp) // the torpedo row (GitHub issue 5): as the launcher's, without a lock
    {
        const float r1 = rowY(ng + (ug ? 1 : 0));
        const RecUnit unit; // (the torpedo's prompt and its row)
        const float kh = std::min(0.024f * H * k, p - 0.004f * H * k);
        if (!share)
            DrawPrompt(m.torpedoBtn, wx + lw / 2, r1, g, Argb(kChrome, 1), false, true);
        // (always: the band's MSL n is the launcher's, the torpedoes' count has no other home in a fight)
        Text(cellX + ch, r1, std::to_wstring(std::max(0, m.torpedoAmmo)), ts, Argb(m.torpedoAmmo > 0 ? kInk : kAmber, 1), Right);
        TextMarquee(nameX, r1, m.torpedo, ts, rx - nameX, Argb(kInk, 1));
    }
    if (ln)
    {
        const float r2 = rowY(rows - 1);
        const RecUnit unit; // (the launcher's prompt and its row)
        DrawPrompt(m.launchBtn, wx + lw / 2, share ? r2 - p * 0.5f : r2, g, Argb(kChrome, 1), false, true);
        const float lockW = TextWidth(L"LOCKED", T, 0.06f) + 0.014f * H * k;
        // the count in the numeral cells' column, right-aligned to its edge (the launcher has no cell): one column with
        // the group digits above it, 0.010 H from the names' column - right-aligned before LOCKED it sat diagonally
        // 0.006 H from the last group's digit (the in-game lint, Focus combat at 4K K&M)
        std::wstring ammo = countHere && m.ammo >= 0 ? std::to_wstring(m.ammo) : L"";
        if (!ammo.empty())
            Text(cellX + ch, r2, ammo, ts, Argb(m.ammo > 0 ? kInk : kAmber, 1), Right);
        // the name in the names' column, before LOCKED; left out where it would be a stub (a lock on the narrow strip)
        const float nx = nameX, room = rx - (m.locked ? lockW + 0.010f * H * k : 0.0f) - nx;
        if (room >= 0.03f * H * k) // (the full name, a marquee where it would be cut: decision 1)
            TextMarquee(nx, r2, m.launcher, ts, room, Argb(kInk, 1));
        if (m.locked)
        {
            const float lx = rx - lockW, lh = std::min(0.015f * H * k, p * 0.5f); // (the chip within its row)
            Plate(lx, r2 - lh, rx, r2 + lh, 0.005f * H * k, kTL | kTR | kBR | kBL, 230 / 255.0f, kHostile, 1.0f);
            Text(lx + 0.007f * H * k, r2, L"LOCKED", T, Argb(kHostile, 1), Left, 0.06f, false);
        }
    }
}
} // namespace hud2
