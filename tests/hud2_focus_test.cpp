// HUD 2's Focus layout (controller\src\impl\hud2_layout.h FocusCorners / RulesOf / Reveal; docs\UI_HUD2_PRESETS.md 2.2,
// UI_HUD2.md 5.18): its rectangles from the spec's data at 4K, 1080p, 4:3, 5:4, 16:10 and 21:9 / 32:9, pad and keyboard
// and mouse - the compact card 0.34 wide (calm 0.095, the fight form 0.167 with the pad's 2 threat rows inside it, the
// morph between them; the pad + 0.012 for its lane row, K&M + its 0.040 click row: the owner's B4-B7), the fire strip
// 0.36 / 0.40 x 0.14, the arcs' radius 0.12, the band at + 0.14, the escort rows' last line at 0.93 (B9: no hold strip),
// the nav's 0.48 / 0.36 cap; no plate in its clear field (x 0.30-0.70 W, from topFree's bottom to 0.82 H) or the
// centre core; its boxed budgets (calm <= 3 %, combat <= its cap); its pill caps (1 calm, 2 combat); the letterbox
// collapse; the bottom-left free text and the held parts outside the boxed area. Then the reveal's rules: what calm holds
// back, the wakes (the changed element only, hud_wake_s), the arcs' rules (each meter on its own in calm: below full at
// 40 %, full hidden, B1; the power arc in a fight or a cruise charge), the speed's (a throttle-state change, a > 20 % change held 0.5 s, the steady
// contexts), the fight's lingers (opacity 2 s, the form 6 s, eased), the peek (3 s, a second pins, a third ends; the
// auto-peek at space entry, never in combat), hud_reveal 0 / 2, the letterbox.
#include "../controller/src/impl/hud2_layout.h"
#include "../controller/src/impl/hud2_lint.h"

#include "test.h"

namespace
{
using namespace hud2layout;
namespace hp = uimeta::hud2p;
const hp::Preset& F() { return hp::kPresets[kFocusIndex]; }
// 4K's edges inside the margins (the spec's: 3 % since 0.2.0, 5 % before - these were 192 / 108 / 3648 / 2052)
const double MX = hp::kMarginSideW * 3840.0, MY = hp::kMarginTop * 2160.0, RX = 3840 - MX,
             BY = 2160 - hp::kMarginBottom * 2160.0;

// A context line of n prompts, each `w` H wide (as tests\hud2_layout_test.cpp's).
void Prompts(Frame& f, int n, float w)
{
    const float k = Scale(f), H = f.H;
    f.m.ctxMeasured = true, f.m.nCtx = n;
    for (int i = 0; i < 3; ++i)
    {
        f.m.glyph[i] = 0.036f * H * k, f.m.label[i] = w * H * k - 0.036f * H * k * 1.3f;
        f.m.base[i] = promptfit::ContextPromptWidth(f.m.glyph[i], 0.036f * H * k, f.m.label[i]);
        f.m.hold[i] = false;
    }
    f.m.dims.word = 0.05f * H * k, f.m.dims.gapBefore = promptfit::GapBefore(0.036f * H * k),
    f.m.dims.ringChord = promptfit::RingChord(0.036f * H * k);
    f.m.ctxGap = 0.03f * H * k, f.m.ctxWord = std::max(0.024f * H * k, std::max(0.024f * H, 14.0f));
}

Frame Base(float W, float H, bool kbm, bool combat)
{
    Frame f;
    f.W = W, f.H = H, f.kbm = kbm, f.combat = combat;
    f.target = true;
    f.list = kbm || combat; // (ListShown: the keyboard form's header always; the pad's rows in a fight)
    f.listRows = kbm && !combat ? 0 : 2; // (Slow fills 2 threat rows; the keyboard form's calm list is its header alone)
    return f;
}

// A plate's rectangle in the clear field or the core (the lint's InZone; the clear field is the spec's zone with
// "lint": false - checked here, not live)
bool InClear(const Out& o, const Frame& f, const char* comp, const Rect& r)
{
    hud2lint::Item it;
    it.kind = hud2lint::Kind::Plate, it.comp = comp;
    it.x0 = r.x0, it.y0 = r.y0, it.x1 = r.x1, it.y1 = r.y1;
    hud2lint::Zone clear;
    clear.name = "clear";
    clear.x0 = 0.30f * f.W, clear.x1 = 0.70f * f.W, clear.y0 = o.topFree.y1, clear.y1 = 0.82f * f.H;
    bool in = hud2lint::InZone(it, clear);
    for (const hud2lint::Zone& z : hud2lint::ZonesOf(F(), f.W, f.H, o.card.x0, o.card.x1, f.kbm, 0, 0, o.clearY0))
        in |= !hud2lint::Exempt(it.comp, z.exempt) && hud2lint::InZone(it, z);
    if (in)
        fprintf(stderr, "hud2_focus: %s in the clear field / core at %gx%g %s (%g,%g-%g,%g)\n", comp, f.W, f.H,
                f.kbm ? "K&M" : "pad", r.x0, r.y0, r.x1, r.y1);
    return in;
}

RevealIn Calm(float dt = 0.1f)
{
    RevealIn in;
    in.dt = dt, in.hull = 1, in.shield = 1, in.speed = 80;
    return in;
}
RevealOut Run(Reveal& r, const RevealRules& rr, RevealIn in, float seconds)
{
    RevealOut o;
    for (float t = 0; t < seconds - 1e-4f; t += in.dt)
        o = r.Step(rr, in);
    return o;
}
} // namespace

void Test_hud2_focus()
{
    const hp::Preset& P = F();
    // ---- the rules and the data the layout reads (UI_SPEC.json "focus"; the generator's reticle / bundle rows)
    {
        const Rules& r = RulesOf(kFocusIndex);
        CHECK(r.corners == 1 && r.listInCard && r.cardOffForm && r.capPills && r.unplated == Bit(Ship));
        CHECK(!r.budgetSuppress && r.letterboxKeep == (Bit(Nav) | Bit(Ring) | Bit(Alert)));
        CHECK(&RulesOf(0) != &r && RulesOf(0).corners == 0 && !RulesOf(0).capPills); // Standard's untouched
        CHECK(P.reticleR == 0.12f && P.pickR == 0.12f); // 2.2: arcs at r 0.12, pick radius 0.12 (RingRadius)
        const char* need[][3] = {{"calm", "card", "w"},          {"calm", "card", "h_off_screen"},
                                 {"combat", "card", "h_off_screen"}, {"combat", "loadout", "w"},
                                 {"combat", "loadout", "w_kbm"}, {"combat", "loadout", "h"},
                                 {"calm", "nav", "w_max"},       {"calm", "nav", "w_max_4_3"},
                                 {"calm", "nav", "h"},           {"calm", "hold", "y"},
                                 {"calm", "comm", "chat_free_x1_w"}, {"reticle", "reticle", "band_dy"},
                                 {"reticle", "reticle", "arc_deg"}, {"reticle", "cruise_row", "alpha_calm"},
                                 {"bundle", "new", "hud_reveal"}, {"bundle", "new", "hud_wake_s"},
                                 {"calm", "context", "pills_max"}, {"combat", "context", "pills_max"}};
        for (const auto& n : need)
        {
            const bool has = hp::HasNum(P, n[0], n[1], n[2]);
            if (!has)
                fprintf(stderr, "hud2_focus: no %s.%s.%s in the data\n", n[0], n[1], n[2]);
            CHECK(has);
        }
        CHECK(hp::Num(P, "reticle", "reticle", "band_dy", 0) == 0.078f); // (inside the arcs' circle since 5.23; was 0.14)
        CHECK(hp::Num(P, "bundle", "new", "hud_calm_ring", -1) == 0.0f); // Focus's calm ring: none
    }
    // ---- the anchors at 4K (pad): calm, then the fight form
    {
        Frame f = Base(3840, 2160, false, false);
        Out o = Compute(f, P);
        CHECK_NEAR(o.k, 1.0, 1e-6);
        CHECK_NEAR(o.R, 0.12 * 2160, 0.01);
        CHECK_NEAR(o.card.x1, RX, 0.01);
        CHECK_NEAR(o.card.x0, RX - 0.34 * 2160, 0.01);
        CHECK_NEAR(o.card.y0, MY, 0.01);
        // the calm compact card (its off-screen form) + the pad's lane row (B4-B7: focus_ns::kCardLaneRow 0.012)
        CHECK_NEAR(o.card.y1, MY + (0.095 + 0.012) * 2160, 0.01);
        CHECK(o.listInCard && o.form == 0 && o.thrY0 == o.thrY1);
        // the fire strip's base the weapons only, the cross stacked on its right end (the owner's decision 3, 31 Oct
        // 2026: was 0.36 wide with the cross inside)
        CHECK_NEAR(o.loadout.x0, RX - 0.22 * 2160, 0.01);
        CHECK_NEAR(o.loadout.y0, BY - 0.14 * 2160, 0.01);
        CHECK_NEAR(o.cons.x1, RX, 0.01);
        CHECK_NEAR(o.cons.x0, RX - 0.16 * 2160, 0.01);
        CHECK_NEAR(o.cons.y1, o.loadout.y0 - 0.012 * 2160, 0.01);
        CHECK_NEAR(o.cons.y0, o.cons.y1 - 0.14 * 2160, 0.01);
        CHECK_NEAR(o.bandY, 1080 + 0.078 * 2160, 0.01);
        // the lower centre's top (5.23): the split arcs' circle (its bottom arc on it) + 0.01 H; the old band at + 0.14
        // (its 0.026 texts from + 0.127) would FAIL there, the band at + 0.078 (texts to + 0.091, the speed to + 0.093)
        // passes - checked with the lint itself below
        CHECK_NEAR(o.clearY0, 1080 + (0.12 + 0.5 * 0.0065) * 2160 + 0.01 * 2160, 0.01);
        {
            std::vector<hud2lint::Zone> zones = hud2lint::ZonesOf(P, 3840, 2160, o.card.x0, o.card.x1, false, 0, 0, o.clearY0);
            auto bandFails = [&](float dy)
            {
                hud2lint::Input in;
                in.W = 3840, in.H = 2160, in.zones = zones;
                const float y = 1080 + dy * 2160;
                auto text = [&](const char* comp, float x0, float x1, float h)
                {
                    hud2lint::Item it;
                    it.kind = hud2lint::Kind::Text, it.comp = comp, it.centre = true, it.textH = h * 2160;
                    it.x0 = 1920 + x0 * 2160, it.x1 = 1920 + x1 * 2160, it.y0 = y - h * 1080, it.y1 = y + h * 1080;
                    in.items.push_back(it);
                };
                text("ring.band", -0.0275f, 0.0275f, 0.030f); // the speed
                text("ring.band", 0.045f, 0.155f, 0.026f);    // CRUISE ON right of it
                text("ring.band", -0.105f, -0.045f, 0.026f);  // BOOST left of it
                hud2lint::Item hull; // the hull arc (-25..25) and the bottom (charge) arc on the circle
                hull.kind = hud2lint::Kind::Arc, hull.comp = "ring.hull", hull.centre = true, hull.cx = 1920, hull.cy = 1080,
                hull.r = 0.12f * 2160, hull.a0 = -25, hull.a1 = 25, hull.w = 0.0065f * 2160;
                hud2lint::FitArcBox(hull);
                hud2lint::Item bottom = hull;
                bottom.comp = "ring.charge", bottom.a0 = 66, bottom.a1 = 114;
                hud2lint::FitArcBox(bottom);
                in.items.push_back(hull), in.items.push_back(bottom);
                const hud2lint::Result r = hud2lint::Lint(in);
                int zone = 0, overlap = 0;
                for (const hud2lint::Finding& fd : r.findings)
                    zone += fd.rule == "zone", overlap += fd.rule == "centre.overlap";
                if (overlap && dy < 0.1f)
                    fprintf(stderr, "hud2_focus: the band at + %g:\n%s\n", dy, hud2lint::Format(r).c_str());
                CHECK(overlap == 0 || dy > 0.1f); // (the band inside keeps the centre stack's 0.01 H from the arcs)
                return zone;
            };
            CHECK(bandFails(0.14f) == 3); // as drawn until 5.23: the speed, CRUISE ON and BOOST in the lower centre
            CHECK(bandFails(0.078f) == 0);
        }
        CHECK_NEAR(o.holdY, 0.93 * 2160, 0.01);
        CHECK_NEAR(o.navMaxX, MX + 0.48 * 2160, 0.01);
        CHECK_NEAR(o.nav.y1, MY + 0.08 * 2160, 0.01);
        CHECK_NEAR(o.chatFree.x1, 0.30 * 3840, 0.01);
        CHECK(o.unplated == Bit(Ship));
        f.combat = f.list = true;
        o = Compute(f, P);
        CHECK_NEAR(o.form, 1.0, 1e-6);
        CHECK_NEAR(o.card.y1, MY + (0.167 + 0.012) * 2160, 0.01); // card + 2 threat rows (+ the lane row)
        CHECK_NEAR(o.thrY0, MY + (0.095 + 0.012) * 2160, 0.01);
        CHECK_NEAR(o.thrY1, o.card.y1, 0.01);
        CHECK_NEAR(o.thrY1 - o.thrY0, (focus_ns::kRowSep + 2 * 0.032f) * 2160, 0.05); // the rows' 0.072 H
        // the morph: half way, half the growth (the form is the caller's eased value)
        f.form = 0.5f;
        o = Compute(f, P);
        CHECK_NEAR(o.card.y1, MY + (0.107 + 0.036) * 2160, 0.01);
        // the nav grows with its drawn lines (a chip, a bar, a wait line)
        f.navBottom = 0.30f * 2160;
        o = Compute(f, P);
        CHECK_NEAR(o.nav.y1, 0.31 * 2160, 0.01);
        // K&M: the card + its click row; the contact list block under it (Standard's geometry); the 0.40 strip
        Frame g = Base(3840, 2160, true, true);
        o = Compute(g, P);
        CHECK(!o.listInCard && o.form == 0);
        CHECK_NEAR(o.card.y1, MY + (0.095 + focus_ns::kCardFooterKbm) * 2160, 0.01); // (no lane row: the click row)
        CHECK_NEAR(focus_ns::kCardFooterKbm, 0.040, 1e-6);                          // (B4-B7: was 0.034)
        // the bottom-left block: 3 escort rows, the last on 0.93 (B9: no hold strip)
        CHECK_NEAR(o.ship.y0, BY - 0.12 * 2160, 0.01);
        CHECK_NEAR(o.thrY0, o.card.y1, 0.01);
        CHECK_NEAR(o.loadout.x0, RX - 0.22 * 2160, 0.01); // (decision 3: was the spec's 0.40 with the key grid inside)
        // the key grid stacked on it: 0.134 + its two key columns (0.03 each while unmeasured), 0.11 tall
        CHECK_NEAR(o.cons.x0, RX - (0.134 + 0.06) * 2160, 0.01);
        CHECK_NEAR(o.cons.y1 - o.cons.y0, 0.11 * 2160, 0.01);
        g.m.slotKeyW = 0.05f * 2160;
        CHECK_NEAR(Compute(g, P).cons.x0, RX - 0.134 * 2160 - 2 * 0.05 * 2160, 0.01); // (the measured keys)
        // 4:3: the nav's 0.36 cap (x k 0.8)
        Frame n = Base(1600, 1200, false, false);
        o = Compute(n, P);
        CHECK_NEAR(o.navMaxX, hp::kMarginSideW * 1600 + 0.36 * 1200 * 0.8, 0.01);
        Frame w = Base(1920, 1200, false, false); // 16:10 keeps 0.48 (x k 0.9)
        o = Compute(w, P);
        CHECK_NEAR(o.navMaxX, hp::kMarginSideW * 1920 + 0.48 * 1200 * 0.9, 0.01);
    }
    // ---- across the display matrix: inside the screen, out of the clear field and the core, under the budgets
    const float screens[][2] = {{3840, 2160}, {1920, 1080}, {1280, 720}, {1600, 1200}, {1280, 1024}, {1920, 1200},
                                {2560, 1080}, {3840, 1600}, {5120, 1440}};
    for (const auto& s : screens)
        for (int kbm = 0; kbm < 2; ++kbm)
            for (int combat = 0; combat < 2; ++combat)
                for (int alert = 0; alert < 2; ++alert)
                {
                    Frame f = Base(s[0], s[1], kbm != 0, combat != 0);
                    Prompts(f, 3, 0.16f);
                    if (alert)
                        f.alert = true, f.m.alertW = 0.5f * f.H;
                    if (!combat) // calm: what the reveal holds back with nothing woken (the strip, the bottom-left)
                        f.held = Bit(Loadout) | Bit(Ship);
                    const Out o = Compute(f, P);
                    // (an alert is transient - 3 s, the budget's peak case; calm names no alert case: it is left out there)
                    const float alertA = alert ? (o.alertX1 - o.alertX0) * (o.alertY1 - o.alertY) / (f.W * f.H) : 0.0f;
                    const float steady = combat ? o.boxed : o.boxed - alertA;
                    // the pill cap (calm 1, combat 2)
                    CHECK(o.nCtx <= (combat ? 2 : 1) && o.nCtx >= 1);
                    // the corners inside the margins
                    CHECK(o.card.x1 <= f.W - o.mx + 0.5f && o.card.y0 >= o.my - 0.5f);
                    CHECK(o.loadout.x1 <= f.W - o.mx + 0.5f && o.loadout.y1 <= f.H - o.my + 0.5f);
                    CHECK(o.ship.x0 >= o.mx - 0.5f && o.ship.y1 <= f.H - o.my + 0.5f);
                    CHECK(o.nav.x1 <= o.card.x0); // the nav never reaches the card
                    // no plate in the clear field or the core (the card, the K&M list, the strip, the line's pills)
                    CHECK(!InClear(o, f, "card", o.card));
                    if (f.kbm)
                        CHECK(!InClear(o, f, "list", {o.card.x0, o.thrY0, o.card.x1, o.thrY1}));
                    CHECK(!InClear(o, f, "loadout", o.loadout));
                    CHECK(!InClear(o, f, "loadout", o.cons)); // (the cross stacked on it: decision 3)
                    CHECK(o.cons.y1 < o.loadout.y0 && o.cons.x1 <= o.loadout.x1 + 0.5f && o.cons.x0 >= o.loadout.x0 - 0.5f);
                    // the context prompts in their lanes (decision 4), each pill's plate out of the clear field, the
                    // core and the bottom-centre band (ZonesOf's "bottom")
                    for (int i = 0; i < o.nCtx; ++i)
                        CHECK(!InClear(o, f, "context",
                                       {o.ctxX[i] - 0.012f * f.H * o.k, o.ctxPY[i] - 0.026f * f.H * o.k,
                                        o.ctxX[i] + o.ctxW[i] + 0.012f * f.H * o.k, o.ctxPY[i] + 0.026f * f.H * o.k}));
                    CHECK(!o.ctxFallback);
                    // the band inside the arcs' circle (5.23; was below it, 2.2's 0.63-0.65), its tallest text (the speed,
                    // 0.030) above the lower centre's top and clear of the core (r 0.045)
                    CHECK(o.bandY < o.cy + o.R && o.bandY - 0.015f * f.H * o.k > o.cy + 0.045f * f.H * o.k);
                    CHECK(o.bandY + 0.015f * f.H * o.k < o.clearY0);
                    // the budgets (PRESETS 2.2 Budgets: calm with a target <= 3 %; combat <= 9 % pad (the card's
                    // off-screen form = the peak case), K&M <= 10 %): the pad always; K&M in combat without an alert.
                    // K&M calm carries the click-pinned contact list's header and its card's click row (~4 %: over
                    // the calm 3 %, the lint's WARN - an owner decision, UI_HUD2.md 5.18), K&M combat + an alert 11 %
                    const float cap = CapOf(P, f.combat, f.kbm, true, true);
                    if (!f.kbm)
                    {
                        if (steady > cap)
                            fprintf(stderr, "hud2_focus: boxed %.2f %% > cap %.2f %% at %gx%g pad combat=%d alert=%d\n",
                                    steady * 100, cap * 100, f.W, f.H, combat, alert);
                        CHECK(steady <= cap);
                    }
                    else if (combat && !alert)
                        CHECK(o.boxed <= cap);
                    else if (!combat)
                        CHECK(steady <= 0.045f); // the known K&M calm overage, bounded
                    CHECK(o.budgetOff == 0 && o.suppressed == 0);
                    if (g_test.verbose && s[0] == 3840 && s[1] == 2160)
                        fprintf(stderr, "hud2_focus: 4K %s %s%s boxed %.2f %% (steady %.2f %%, cap %.1f %%)\n",
                                f.kbm ? "K&M" : "pad", combat ? "combat" : "calm", alert ? " + alert" : "",
                                o.boxed * 100, steady * 100, cap * 100);
                }
    // the boxed budget's parts: the bottom-left free text never counts, a held part neither
    {
        Frame f = Base(3840, 2160, false, true);
        const Out a = Compute(f, P);
        f.held = Bit(Loadout);
        const Out b = Compute(f, P);
        CHECK(b.held == Bit(Loadout));
        CHECK_NEAR(a.boxed - b.boxed, (a.loadout.Area() + a.cons.Area()) / (f.W * f.H), 1e-6); // (+ the stacked cross)
        CHECK(a.ship.Area() > 0); // (a rectangle for the alert / the context line / the bus, no plate)
        Frame c = Base(3840, 2160, false, false);
        c.target = false, c.list = false;
        c.held = Bit(Loadout) | Bit(Ship);
        CHECK(Compute(c, P).boxed == 0); // calm, no target, nothing woken: ~0 % (2.2 / 3 "no target ~0")
    }
    // ---- the letterbox: the nav's chip, the ring and the alert stay (2.2 "Letterbox: the chip, arcs only below nominal")
    {
        Frame f = Base(3840, 2160, false, true);
        f.letterbox = true;
        const Out o = Compute(f, P);
        CHECK(o.suppressed == (Bit(Card) | Bit(List) | Bit(Ship) | Bit(Loadout) | Bit(Context)));
    }

    // ================================================================ the reveal (PRESETS 1.6; 2.2)
    const RevealRules rr = RevealRulesOf(P);
    CHECK(rr.level == 1 && rr.autoPeek && rr.wakeS == 4.0f && rr.fightLingerS == 2.0f && rr.formLingerS == 6.0f);
    CHECK(rr.hullBelowFull == 0.4f && rr.speedShowS == 3.0f && rr.speedChangeFrac == 0.2f && rr.speedChangeHoldS == 0.5f);
    // ---- calm holds back the bottom-left and the fire strip; the arcs are off at full hull / shield
    {
        Reveal r;
        RevealOut o = Run(r, rr, Calm(), 1);
        CHECK(o.held == (Bit(Ship) | Bit(Loadout)));
        CHECK(o.arcHull == 0 && o.arcShield == 0 && o.arcPower == 0);
        CHECK(!o.speed && !o.peek && !o.fight && !o.navFull && o.form == 0);
        // below full: the hull arc at 40 %, the recharging shield too
        RevealIn in = Calm();
        in.hull = 0.9f, in.shield = 0.5f;
        o = r.Step(rr, in);
        CHECK(o.arcHull == 0.4f && o.arcShield == 0.4f && o.arcPower == 0);
        // a cruise charge: the bottom arc (the charge)
        in.charging = true;
        CHECK(r.Step(rr, in).arcPower == 1);
    }
    // ---- each meter on its own in calm (the owner's B1 / B10-B12, 31 Oct 2026): a full meter hidden, one below full at
    // 40 %, whichever the other is; a hit wakes nothing to full (the hit segment is the event)
    {
        Reveal r;
        RevealIn in = Calm();
        Run(r, rr, in, 1);
        in.shield = 0.6f; // the shield recharging, the hull full
        RevealOut o = r.Step(rr, in);
        CHECK(o.arcShield == 0.4f && o.arcHull == 0);
        in.shield = 1, in.hull = 0.8f; // the hull damaged, the shield full again
        o = r.Step(rr, in);
        CHECK(o.arcShield == 0 && o.arcHull == 0.4f);
        in.hull = 1; // (a hit with both meters full again: nothing shows)
        o = Run(r, rr, in, 5);
        CHECK(o.arcShield == 0 && o.arcHull == 0 && o.arcPower == 0);
    }
    // ---- the wakes: only the changed element, for hud_wake_s (no hold strip since B9: the bottom-left holds the escort
    // rows alone)
    {
        Reveal r;
        RevealIn in = Calm();
        Run(r, rr, in, 1);
        RevealOut o = r.Step(rr, in);
        CHECK((o.held & Bit(Ship)) && (o.held & Bit(Loadout)));
        in.loadSig = 3; // a nanobot used
        o = r.Step(rr, in);
        CHECK(!(o.held & Bit(Loadout)) && (o.held & Bit(Ship)));
        o = Run(r, rr, in, 4.2f);
        CHECK(o.held & Bit(Loadout));
        in.slotLit = true; // an alert's lit slot: the strip while it holds
        CHECK(!(r.Step(rr, in).held & Bit(Loadout)));
        in.slotLit = false;
        // escorts: hidden in calm, a hull change shows them
        in.escorts = true;
        o = r.Step(rr, in);
        CHECK(!o.escorts && (o.held & Bit(Ship)));
        in.escortSig = 99;
        o = r.Step(rr, in);
        CHECK(o.escorts && !(o.held & Bit(Ship)));
        // the first step only primes: a fresh reveal does not wake on its first signatures
        Reveal fresh;
        RevealIn first = Calm();
        first.escortSig = 5, first.loadSig = 6, first.escorts = true;
        CHECK(fresh.Step(rr, first).held == (Bit(Ship) | Bit(Loadout)));
    }
    // ---- the speed
    {
        Reveal r;
        RevealIn in = Calm();
        Run(r, rr, in, 1);
        in.throttleState = 1; // the afterburner: shown 3 s
        CHECK(r.Step(rr, in).speed);
        CHECK(Run(r, rr, in, 2.8f).speed);
        CHECK(!Run(r, rr, in, 0.3f).speed);
        in.speed = 104; // +30 % from 80: shown after 0.5 s held
        CHECK(!Run(r, rr, in, 0.3f).speed);
        CHECK(Run(r, rr, in, 0.3f).speed);
        CHECK(!Run(r, rr, in, 3.1f).speed); // ... for 3 s, the reference now 104
        in.speed = 110; // +6 %: nothing
        CHECK(!Run(r, rr, in, 1).speed);
        in.speedSteady = true; // protect / formation / dock / goto / the lever resting partial
        CHECK(r.Step(rr, in).speed);
    }
    // ---- the fight: at once; the opacity lingers 2 s, the form 6 s and eases
    {
        Reveal r;
        RevealIn in = Calm();
        Run(r, rr, in, 1);
        in.combat = true;
        RevealOut o = r.Step(rr, in);
        // (the fire strip shows; the bottom-left only with escort rows or a woken hold line: none here)
        CHECK(o.fight && o.held == Bit(Ship) && o.arcHull == 1 && o.arcPower == 1 && o.form > 0 && o.form < 1);
        in.escorts = true;
        CHECK(r.Step(rr, in).held == 0 && r.Step(rr, in).escorts); // escorts in a fight
        in.escorts = false;
        o = Run(r, rr, in, 1.5f);
        CHECK(o.form == 1);
        in.combat = false;
        o = Run(r, rr, in, 1.9f);
        CHECK(o.fight && !(o.held & Bit(Loadout)));
        o = Run(r, rr, in, 0.3f);
        CHECK(!o.fight && o.held == (Bit(Ship) | Bit(Loadout)) && o.form == 1); // the form still lingers
        o = Run(r, rr, in, 3.6f);
        CHECK(o.form == 1);
        o = Run(r, rr, in, 0.3f);
        CHECK(o.form < 1);
        o = Run(r, rr, in, 1.5f);
        CHECK(o.form == 0);
    }
    // ---- the peek: 3 s; a second inside the window pins; the next ends it; the auto-peek at space entry, not in combat
    {
        Reveal r;
        RevealIn in = Calm();
        Run(r, rr, in, 1);
        r.Peek(rr);
        RevealOut o = r.Step(rr, in);
        // (the peek: everything held back - the strip; the bottom-left only with escort rows (B9), none here)
        CHECK(o.peek && o.navFull && o.held == Bit(Ship) && o.arcHull == 1 && o.speed);
        o = Run(r, rr, in, 3.0f);
        CHECK(!o.peek && o.held != 0);
        r.Peek(rr);
        Run(r, rr, in, 1);
        r.Peek(rr); // a second inside the window: pinned
        CHECK(Run(r, rr, in, 10).peek && r.pinned);
        r.Peek(rr); // the next ends it
        CHECK(!r.Step(rr, in).peek);
        RevealIn entry = Calm();
        entry.spaceEntry = true;
        Reveal a;
        CHECK(a.Step(rr, entry).peek);
        entry.combat = true;
        Reveal b;
        CHECK(!b.Step(rr, entry).peek); // never in combat
    }
    // ---- hud_reveal 0 (always full) and 2 (peek only: nothing wakes by itself but an alert's slot)
    {
        RevealRules full = rr, peekOnly = rr;
        full.level = 0, peekOnly.level = 2;
        Reveal r0;
        RevealIn esc = Calm();
        esc.escorts = true;
        RevealOut o = Run(r0, full, esc, 1);
        CHECK(o.held == 0 && o.navFull && o.speed && o.arcHull == 1);
        Reveal r2;
        RevealIn in = Calm();
        Run(r2, peekOnly, in, 1);
        in.escortSig = 1, in.loadSig = 1, in.throttleState = 1;
        o = r2.Step(peekOnly, in);
        CHECK(o.held == (Bit(Ship) | Bit(Loadout)) && !o.speed);
        in.slotLit = true;
        CHECK(!(r2.Step(peekOnly, in).held & Bit(Loadout)));
        r2.Peek(peekOnly);
        CHECK(r2.Step(peekOnly, in).peek);
    }
    // ---- the letterbox: the arcs only below nominal, no band speed
    {
        Reveal r;
        RevealIn in = Calm();
        in.combat = true, in.letterbox = true, in.hull = 0.7f, in.speedSteady = true;
        const RevealOut o = Run(r, rr, in, 1);
        CHECK(o.arcHull == 0.4f && o.arcShield == 0 && o.arcPower == 0 && !o.speed);
    }
}
