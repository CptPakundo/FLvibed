// HUD 2's layout core (modules\hud\src\hud2_layout.h, UI_HUD2_PRESETS.md 6 step 5; UI_HUD2.md 5.17): Standard's
// rectangles equal today's HUD (the ComputeLayout of 2b6062ac, frozen below as Ref - with the owner's deliberate moves:
// A5's 0.15 ship panel, 5.19; decision 3's narrow loadout and its stacked cross and decision 4's context lanes, 5.20;
// the lanes above the panels instead of the centred fallback line and the lower centre's top Out::clearY0, 5.23 -
// the lanes' own geometry tested in tests\promptfit_test.cpp) at 3840x2160, 1920x1080, 4:3, 5:4,
// 16:10 and 21:9, pad and keyboard-and-mouse, large scales, a typing player, an alert under the dialogue panel and the
// context line stepping aside (within 1 px, and bit for bit: the core keeps today's float expressions); the numbers it
// takes from the spec's data are today's (the card's 0.215, the loadout's 0.15, the context line's 0.915, the ring's
// 0.10, the margins, the aspect factors); Standard's plates stay out of its zones (the lint's own zone rule,
// hud2_lint.h ZonesOf / InZone); the letterbox suppresses what it did before; a preset rule with budget suppression drops
// the next element of its order while the plates are over the cap, and Standard's rules (none) never do.
#include "../modules/hud/src/hud2_layout.h"
#include "../modules/hud/src/hud2_lint.h"

#include "test.h"

namespace
{
using hud2layout::Frame;
using hud2layout::Out;
namespace hp = uimeta::hud2p;

// Today's ComputeLayout (hud2.cpp at 2b6062ac), the rectangle maths verbatim over the core's inputs: the reference
// Standard must keep.
struct RefOut
{
    float k, mx, my, cx, cy, R;
    float tgtX0, tgtY0, tgtX1, tgtY1, navMaxX, navX0, navY0, navX1, navY1, thrY0, thrY1;
    float shipX0, shipY0, shipX1, shipY1, loX0, loY0, loX1, loY1;
    float consX0, consY0, consX1, consY1;
    int nCtx;
    float ctxY, ctxX0, ctxX1, ctxTop, ctxW[3], ctxX[3], ctxPY[3];
    bool fallback;
    promptfit::HoldPlan ctxPlan;
    float alertY, alertX0, alertX1, alertY1;
};
RefOut Ref(const Frame& f)
{
    RefOut L = {};
    const float W = f.W, H = f.H;
    L.k = promptfit::Hud2Scale(W, H, f.hudScale, f.textSize, f.kbm);
    const float k = L.k;
    // the margins: the spec's (0.05 until 0.2.0; top = bottom)
    L.mx = hp::kMarginSideW * W, L.my = hp::kMarginTop * H, L.cx = W / 2, L.cy = H / 2, L.R = 0.10f * H * k;
    L.tgtX1 = W - L.mx, L.tgtX0 = L.tgtX1 - 0.44f * H * k, L.tgtY0 = L.my, L.tgtY1 = L.my + (f.kbm ? 0.22f : 0.215f) * H * k;
    L.navMaxX = std::max(L.mx + 0.25f * H * k, std::min(L.mx + 0.62f * H * k, L.tgtX0 - 0.42f * H));
    L.navX0 = L.mx - 0.02f * H, L.navY0 = L.my - 0.01f * H, L.navY1 = L.my + (f.kbm ? 0.338f : 0.29f) * H * k;
    L.navX1 = f.navRight > 0 ? std::min(f.navRight + 0.01f * H, L.navMaxX) : L.navMaxX;
    L.thrY0 = f.target ? L.tgtY1 : L.tgtY0 - 0.02f * H * k;
    const int rows = f.listRows;
    L.thrY1 = L.thrY0 + 0.034f * H * k + 0.022f * H * k + (0.022f + 0.042f * rows) * H * k + (f.listMore ? 0.03f * H * k : 0.0f);
    if (f.kbm && rows == 0)
        L.thrY1 = L.thrY0 + 0.06f * H * k;
    // (the ship panel 0.15 tall, was 0.17: the owner's A5, 31 Oct 2026 - "keep these panels at identical height", the
    // loadout's 0.15; the one deliberate change to today's Standard here, UI_HUD2.md 5.19)
    // (0.31 wide, was 0.40: the owner's "wasted space", 31 Oct 2026 - the icon column sized to its content, UI_HUD2.md 5.22)
    L.shipX0 = L.mx, L.shipX1 = L.mx + 0.31f * H * k, L.shipY1 = H - L.my, L.shipY0 = L.shipY1 - 0.15f * H * k;
    // (the loadout's base panel 0.40 / K&M 0.44 with the consumables' cross stacked on its right end, 0.20 x 0.15 / K&M
    // 0.16 + 2 keys x 0.13, 0.012 above it: the owner's decision 3, 31 Oct 2026 - was 0.62 / 0.66 with the cross inside)
    const float loH = 0.15f;
    L.loX1 = W - L.mx, L.loX0 = L.loX1 - (f.kbm ? 0.44f : 0.40f) * H * k, L.loY1 = H - L.my, L.loY0 = L.loY1 - loH * H * k;
    {
        const float consW = f.kbm ? 0.16f * H * k + 2 * (f.m.slotKeyW > 0 ? f.m.slotKeyW : 0.03f * H * k) : 0.20f * H * k;
        L.consX1 = L.loX1, L.consX0 = L.consX1 - consW, L.consY1 = L.loY0 - 0.012f * H * k,
        L.consY0 = L.consY1 - (f.kbm ? 0.13f : 0.15f) * H * k;
    }
    // the context prompts in two lanes beside the bottom panels, never in the bottom-centre band 0.40-0.60 W (the owner's
    // decision 4, 31 Oct 2026; was one line centred between the panels): promptfit.h's lanes and their hold cascade
    // (their own tests: tests\promptfit_test.cpp), squeezed to two floor ems of label; both lanes closed = the same two
    // lanes above the panels (5.23: the centred line there crossed the lower centre), else no prompt
    {
        const int n = std::min(f.m.nCtx, 3);
        L.nCtx = 0, L.ctxY = 0.915f * H, L.fallback = false, L.ctxX0 = L.ctxX1 = 0;
        const promptfit::LaneGeom g = promptfit::ContextLanes(W, H, k, L.shipX1, L.loX0, 0.915f, 0.40f, 0.60f);
        float minW[3];
        for (int i = 0; i < 3; ++i)
            minW[i] = f.m.base[i] - f.m.label[i] + std::min(f.m.label[i], 2.0f * std::max(0.024f * H, 14.0f));
        promptfit::LanesPlaced c;
        float w[3] = {};
        auto place = [&](const promptfit::LaneGeom& lanes)
        {
            L.ctxPlan = promptfit::HoldPlan();
            if (f.m.ctxMeasured)
            {
                const promptfit::LanesHold h = promptfit::PlaceLanesHold(f.m.base, minW, f.m.glyph, f.m.label, f.m.hold,
                                                                         f.m.side, n, f.m.dims, f.m.ctxGap, lanes, H, k,
                                                                         f.m.ctxWord);
                c = h.c, L.ctxPlan = h.plan;
                for (int i = 0; i < 3; ++i)
                    w[i] = h.w[i];
            }
            else
                c = promptfit::PlaceLanes(nullptr, nullptr, f.m.side, n, f.m.ctxGap, lanes, H, k);
        };
        place(g);
        if (c.none)
        {
            L.fallback = true;
            const float top = std::min(L.shipY0, L.loY0);
            const promptfit::LaneGeom up = promptfit::LanesAbove(W, H, k, L.mx, top, 0.40f, 0.60f);
            CHECK_NEAR(up.y0, top - 0.04f * H * k, 0.01);  // (the old centred line's baseline)
            CHECK(up.lx1 <= 0.40f * W && up.rx0 >= 0.60f * W); // never in the lower centre's columns
            L.ctxY = up.y0;
            place(up);
        }
        if (!c.none)
        {
            L.nCtx = c.n, L.ctxX0 = c.x0, L.ctxX1 = c.x1;
            for (int i = 0; i < c.n; ++i)
                L.ctxX[i] = c.x[i], L.ctxPY[i] = c.y[i], L.ctxW[i] = w[i];
        }
        else
            L.ctxPlan = promptfit::HoldPlan();
        L.ctxTop = L.ctxY - 0.03f * H;
        for (int i = 0; i < L.nCtx && i < 3; ++i) // (0.2.1: a squeezed prompt's label wraps, its plate rises by tall)
        {
            const float tall = c.wrap[i] ? g.tall : 0.0f;
            L.ctxTop = std::min(L.ctxTop, promptfit::Raised(L.ctxPlan.at[i])
                                              ? L.ctxPY[i] - 0.026f * H * k - tall - 0.001f * H - f.m.ctxWord
                                              : L.ctxPY[i] - 0.03f * H - tall);
        }
    }
    L.alertY = L.my;
    if (f.dlg.comm)
        L.alertY = std::max(L.alertY, f.dlg.y1 * H + 0.012f * H);
    const float aw = f.m.alertW;
    L.alertX0 = L.cx - aw / 2, L.alertX1 = L.cx + aw / 2;
    if (aw > 0 && (L.alertX1 > L.tgtX0 - 0.01f * H || L.alertX0 < L.navX1))
        L.alertY = std::max(L.alertY, (f.list ? L.thrY1 : L.tgtY1) + 0.012f * H);
    const float ringY0 = L.cy - L.R - 0.03f * H * k, ringY1 = L.cy + L.R + 0.04f * H * k;
    if (aw > 0 && L.alertY < ringY1 && L.alertY + 0.05f * H * k > ringY0 && L.alertX0 < L.cx + L.R && L.alertX1 > L.cx - L.R)
        L.alertY = ringY1 + 0.006f * H;
    if (aw > 0 && f.dlg.fresh && f.dlg.x1 > f.dlg.x0)
    {
        const float ah = 0.05f * H * k, gap = 0.012f * H;
        const float dx0 = f.dlg.x0 * W, dy0 = f.dlg.y0 * H, dx1 = f.dlg.x1 * W, dy1 = f.dlg.y1 * H;
        auto meets = [&](float x0, float y0, float x1, float y1, float y) {
            return x1 > x0 && L.alertX0 < x1 && L.alertX1 > x0 && y < y1 && y + ah > y0;
        };
        const bool list = f.list;
        auto clear = [&](float y) {
            return y >= L.my * 0.5f && y + ah <= std::min(L.shipY0, L.loY0) - 0.006f * H && !meets(dx0, dy0, dx1, dy1, y) &&
                   !meets(L.cx - L.R, ringY0, L.cx + L.R, ringY1, y) && !meets(L.navX0, L.navY0, L.navX1, L.navY1, y) &&
                   !meets(L.consX0, L.consY0, L.consX1, L.consY1, y) &&
                   !((f.target || list) && meets(L.tgtX0, f.target ? L.tgtY0 : L.thrY0, L.tgtX1, list ? L.thrY1 : L.tgtY1, y));
        };
        if (meets(dx0, dy0, dx1, dy1, L.alertY))
            for (float y : {dy1 + gap, dy0 - gap - ah, ringY1 + 0.006f * H})
                if (clear(y))
                {
                    L.alertY = y;
                    break;
                }
    }
    L.alertY1 = L.alertY + 0.05f * H * k;
    // (the prompts step aside while the alert meets one of them: decision 4 - the centred line moved down before)
    if (aw > 0 && L.nCtx)
    {
        const float ch = 0.026f * H * k, pad = 0.012f * H * k;
        bool meet = false;
        for (int i = 0; i < L.nCtx && i < 3; ++i)
        {
            const float top = promptfit::Raised(L.ctxPlan.at[i]) ? L.ctxPY[i] - ch - 0.001f * H - f.m.ctxWord : L.ctxPY[i] - ch;
            meet |= L.ctxX[i] - pad < L.alertX1 && L.ctxX[i] + L.ctxW[i] + pad > L.alertX0 && L.ctxPY[i] + ch > L.alertY &&
                    top < L.alertY1;
        }
        if (meet)
            L.nCtx = 0;
    }
    return L;
}

int g_exact = 0, g_compared = 0; // bit-identical values among the compared ones (expected: all)
void Same(float a, float b)
{
    CHECK_NEAR(a, b, 1.0f); // the task's 1 px
    ++g_compared;
    g_exact += a == b;
}
void Compare(const Frame& f)
{
    const Out o = hud2layout::Compute(f, hp::kPresets[0]);
    const RefOut r = Ref(f);
    Same(o.k, r.k), Same(o.mx, r.mx), Same(o.my, r.my), Same(o.cx, r.cx), Same(o.cy, r.cy), Same(o.R, r.R);
    Same(o.card.x0, r.tgtX0), Same(o.card.y0, r.tgtY0), Same(o.card.x1, r.tgtX1), Same(o.card.y1, r.tgtY1);
    Same(o.navMaxX, r.navMaxX), Same(o.nav.x0, r.navX0), Same(o.nav.y0, r.navY0), Same(o.nav.x1, r.navX1),
        Same(o.nav.y1, r.navY1);
    Same(o.thrY0, r.thrY0), Same(o.thrY1, r.thrY1);
    Same(o.ship.x0, r.shipX0), Same(o.ship.y0, r.shipY0), Same(o.ship.x1, r.shipX1), Same(o.ship.y1, r.shipY1);
    Same(o.loadout.x0, r.loX0), Same(o.loadout.y0, r.loY0), Same(o.loadout.x1, r.loX1), Same(o.loadout.y1, r.loY1);
    Same(o.cons.x0, r.consX0), Same(o.cons.y0, r.consY0), Same(o.cons.x1, r.consX1), Same(o.cons.y1, r.consY1);
    CHECK(o.nCtx == r.nCtx);
    CHECK(o.ctxFallback == r.fallback);
    Same(o.ctxY, r.ctxY), Same(o.ctxX0, r.ctxX0), Same(o.ctxX1, r.ctxX1), Same(o.ctxTop, r.ctxTop);
    for (int i = 0; i < 3; ++i)
    {
        if (i < r.nCtx)
            Same(o.ctxW[i], r.ctxW[i]), Same(o.ctxX[i], r.ctxX[i]), Same(o.ctxPY[i], r.ctxPY[i]);
        CHECK(o.ctxPlan.at[i] == r.ctxPlan.at[i]);
    }
    CHECK(o.ctxPlan.step == r.ctxPlan.step);
    // no pill's plate in the lower centre's columns at any height (5.23: the lanes beside or above the panels)
    for (int i = 0; i < o.nCtx && i < 3; ++i)
        CHECK(o.ctxX[i] + o.ctxW[i] + 0.012f * f.H * o.k <= 0.40f * f.W + 0.5f ||
              o.ctxX[i] - 0.012f * f.H * o.k >= 0.60f * f.W - 0.5f);
    Same(o.alertY, r.alertY), Same(o.alertX0, r.alertX0), Same(o.alertX1, r.alertX1), Same(o.alertY1, r.alertY1);
    // the bus's rectangles (hud2.cpp Publish before the core)
    Same(o.ringBus.x0, r.cx - r.R * 1.6f), Same(o.ringBus.y1, r.cy + r.R * 1.6f);
    Same(o.topFree.x0, r.navX1), Same(o.topFree.x1, r.tgtX0), Same(o.topFree.y1, 0.30f * f.H);
    Same(o.chatFree.x0, r.mx), Same(o.chatFree.y0, r.navY1 + 0.02f * f.H), Same(o.chatFree.x1, r.mx + 0.30f * f.W),
        Same(o.chatFree.y1, r.shipY0 - 0.02f * f.H);
}

// A context line of n prompts, each `w` H wide (glyph 0.036 H x k, the rest the label), the first `holds` holds.
void Prompts(Frame& f, int n, float w, int holds)
{
    const float k = hud2layout::Scale(f), H = f.H;
    f.m.ctxMeasured = true, f.m.nCtx = n;
    for (int i = 0; i < 3; ++i)
    {
        f.m.glyph[i] = 0.036f * H * k, f.m.label[i] = w * H * k - 0.036f * H * k * 1.3f;
        f.m.base[i] = promptfit::ContextPromptWidth(f.m.glyph[i], 0.036f * H * k, f.m.label[i]);
        f.m.hold[i] = i < holds;
        f.m.side[i] = i % 2; // (left, right, left: both lanes - hud2.cpp CtxSide gives the real ones)
    }
    f.m.dims.word = 0.05f * H * k, f.m.dims.gapBefore = promptfit::GapBefore(0.036f * H * k),
    f.m.dims.ringChord = promptfit::RingChord(0.036f * H * k);
    f.m.ctxGap = 0.03f * H * k, f.m.ctxWord = std::max(0.024f * H * k, std::max(0.024f * H, 14.0f));
}

Frame Base(float W, float H, bool kbm)
{
    Frame f;
    f.W = W, f.H = H, f.kbm = kbm;
    f.target = true;
    f.listRows = kbm ? 4 : 2;
    return f;
}
} // namespace

void Test_hud2_layout()
{
    using namespace hud2layout;
    // ---- the spec's numbers the core takes are today's (UI_SPEC.json hud2_presets Standard; PRESETS 1.1, 1.5, 1.8, 2.1)
    const hp::Preset& S = hp::kPresets[0];
    CHECK(hp::HasNum(S, "combat", "card", "h_off_screen") && hp::Num(S, "combat", "card", "h_off_screen", 0) == 0.215f);
    CHECK(hp::HasNum(S, "combat", "loadout", "h") && hp::Num(S, "combat", "loadout", "h", 0) == 0.15f);
    CHECK(hp::HasNum(S, "calm", "context", "y") && hp::Num(S, "calm", "context", "y", 0) == 0.915f);
    CHECK(!hp::HasNum(S, "calm", "nope", "x") && hp::Num(S, "calm", "nope", "x", 7.0f) == 7.0f); // fail closed
    CHECK(S.reticleR == 0.10f);
    CHECK(hp::kMarginSideW == 0.03f && hp::kMarginTop == 0.03f && hp::kMarginBottom == 0.03f); // 0.05 until 0.2.0
    CHECK(hp::kAlertPlateH == 0.05f);
    // ---- the alert stacks beside the ring (GitHub issue 21, the owner): the newest level, the older pushed down, up again
    // when the one above goes; each side its own; the dialogue pushes a side's whole stack by the known gap
    {
        Frame f = Base(3840, 2160, false);
        const float H = 2160, k = Scale(f), card = kAlertStackH * H * k, step = card + kAlertStackGap * H * k;
        f.m.alertW = 700, f.m.alertSide = 1;
        const Out one = Compute(f, S);
        const float level = one.cy - card / 2;
        CHECK_NEAR(one.alertY, level, 0.01);
        CHECK(one.alertX0 >= one.cx + kAlertSideR * H * k - 0.01 && !one.heldShow[0]);
        f.m.heldW[0] = 600, f.m.heldSide[0] = 1; // the older one, pushed down
        const Out two = Compute(f, S);
        CHECK(two.heldShow[0]);
        CHECK_NEAR(two.alertY, level, 0.01);
        CHECK_NEAR(two.heldY[0], level + step, 0.01);
        f.m.heldW[1] = 500, f.m.heldSide[1] = -1; // the other side's stack starts level too
        const Out sides = Compute(f, S);
        CHECK(sides.heldShow[1]);
        CHECK_NEAR(sides.heldY[1], level, 0.01);
        CHECK(sides.heldX[1] + 500 <= sides.cx - kAlertSideR * H * k + 0.01);
        Frame gone = f; // the higher card dropped: the lower one goes back up
        gone.m.alertW = 0, gone.m.alertSide = 0;
        const Out up = Compute(gone, S);
        CHECK(up.heldShow[0]);
        CHECK_NEAR(up.heldY[0], level, 0.01);
        Frame sub = f; // a dialogue over the left side only: that stack under it by the gap, the right one stays
        sub.dlg.fresh = true, sub.dlg.x0 = 0.05f, sub.dlg.x1 = 0.45f, sub.dlg.y0 = 0.42f, sub.dlg.y1 = 0.55f;
        sub.m.alertW = 700, sub.m.alertSide = -1, sub.m.heldW[1] = 0, sub.m.heldSide[0] = -1;
        const Out pushed = Compute(sub, S);
        CHECK_NEAR(pushed.alertY, 0.55f * H + kAlertDlgGap * H, 0.01);
        CHECK(pushed.heldShow[0]);
        CHECK_NEAR(pushed.heldY[0], pushed.alertY + step, 0.01); // (the whole stack follows)
        sub.m.alertSide = 1, sub.m.heldSide[0] = 1;
        CHECK_NEAR(Compute(sub, S).alertY, level, 0.01);
    }
    // Hud2Scale's aspect factor is the spec's (1.8: 16:10 k 0.9, 4:3 / 5:4 k 0.8; 16:9 and wider 1)
    CHECK_NEAR(promptfit::Hud2Scale(1920, 1200, 100, 100, false), hp::kAspectK16x10, 1e-5);
    CHECK_NEAR(promptfit::Hud2Scale(1600, 1200, 100, 100, false), hp::kAspectK4x3, 1e-5);
    CHECK_NEAR(promptfit::Hud2Scale(1280, 1024, 100, 100, false), hp::kAspectK4x3, 1e-5);
    CHECK_NEAR(promptfit::Hud2Scale(2560, 1080, 100, 100, false), 1.0, 1e-5);
    // ---- the anchors at 4K (pad, a target): the corners at the margins (3 % since 0.2.0), today's sizes
    {
        const Out o = Compute(Base(3840, 2160, false), S);
        const double MX = hp::kMarginSideW * 3840.0, MY = hp::kMarginTop * 2160.0, MB = hp::kMarginBottom * 2160.0;
        const double RX = 3840 - MX, BY = 2160 - MB; // the right edge, the bottom edge inside the margins
        CHECK_NEAR(o.k, 1.0, 1e-6);
        CHECK_NEAR(o.mx, MX, 0.01);
        CHECK_NEAR(o.my, MY, 0.01);
        CHECK_NEAR(o.R, 216, 0.01);
        CHECK_NEAR(o.card.x1, RX, 0.01);
        CHECK_NEAR(o.card.x0, RX - 950.4, 0.01);
        CHECK_NEAR(o.card.y1, MY + 464.4, 0.01);
        CHECK_NEAR(o.ship.x0, MX, 0.01);
        CHECK_NEAR(o.ship.x1, MX + 0.31 * 2160, 0.01); // 0.31 H (5.22; was 1056, 0.40)
        CHECK_NEAR(o.ship.y1, BY, 0.01);
        CHECK_NEAR(o.ship.y0, BY - 0.15 * 2160, 0.01); // unmeasured: both at their 0.15 H maximum (A5's equal height
        CHECK_NEAR(o.ship.y0, o.loadout.y0, 0.01);     //   dropped in 0.2.1 round 6: each fits its own rows)
        {
            // the heights to content (0.2.1 round 6): measured below the maximum wins, anchored to the bottom margin;
            // the cross stacks on the loadout's new top; above the maximum the 0.15 H
            Frame fh = Base(3840, 2160, false);
            fh.m.loadH = 0.086f * 2160, fh.m.shipH = 0.107f * 2160;
            const Out h = Compute(fh, S);
            CHECK_NEAR(h.loadout.y1, BY, 0.01);
            CHECK_NEAR(h.loadout.y0, BY - 0.086 * 2160, 0.05);
            CHECK_NEAR(h.ship.y0, BY - 0.107 * 2160, 0.05);
            CHECK_NEAR(h.cons.y1, h.loadout.y0 - 0.012 * 2160, 0.05);
            fh.m.loadH = 0.2f * 2160;
            CHECK_NEAR(Compute(fh, S).loadout.y0, BY - 0.15 * 2160, 0.01);
        }
        CHECK_NEAR(o.loadout.x0, RX - 0.40 * 2160, 0.01); // the base: the weapons (decision 3; was 2308.8, 0.62)
        CHECK_NEAR(o.loadout.y0, BY - 0.15 * 2160, 0.01); // 0.80 H: S1's bottom (5.16)
        CHECK_NEAR(o.cons.x1, RX, 0.01);    // the cross stacked on its right end, right-aligned
        CHECK_NEAR(o.cons.x0, RX - 0.20 * 2160, 0.01);
        CHECK_NEAR(o.cons.y1, BY - (0.15 + 0.012) * 2160, 0.01);
        CHECK_NEAR(o.cons.y0, BY - (0.15 + 0.012 + 0.15) * 2160, 0.01);
        CHECK_NEAR(o.ctxY, 0.915 * 2160, 0.01);
        CHECK(o.suppressed == 0 && o.budgetOff == 0);
        // the lanes (decision 4): the left from the ship panel + 0.024 H to the band's 0.40 W - 0.012 H, the right from
        // 0.60 W + 0.012 H to the loadout - 0.024 H, rows 0.06 H apart
        const promptfit::LaneGeom g = promptfit::ContextLanes(3840, 2160, 1, o.ship.x1, o.loadout.x0);
        CHECK_NEAR(g.lx0, o.ship.x1 + 0.024 * 2160, 0.01);
        CHECK_NEAR(g.lx1, 0.40 * 3840 - 0.012 * 2160, 0.01);
        CHECK_NEAR(g.rx0, 0.60 * 3840 + 0.012 * 2160, 0.01);
        CHECK_NEAR(g.rx1, o.loadout.x0 - 0.024 * 2160, 0.01);
        CHECK_NEAR(g.pitch, 0.06 * 2160, 0.01);
        CHECK(hud2layout::kBottomBandX0W == 0.40f && hud2layout::kBottomBandX1W == 0.60f &&
              hud2lint::kBottomX0W == hud2layout::kBottomBandX0W && hud2lint::kBottomX1W == hud2layout::kBottomBandX1W);
        // the lower centre's top (5.23): the throttle arc's outer edge (R + 0.022 + 0.75 x 0.0065 / 2) + 0.01 H - 0.6344 H
        // at k 1, the reticle lane's 0.692 (CRUISE ON) far inside; it scales with the ring (k), the gap does not
        CHECK_NEAR(o.clearY0, 1080 + (0.10 + 0.022 + 0.5 * 0.75 * 0.0065) * 2160 + 0.01 * 2160, 0.01);
        CHECK(o.clearY0 < 0.692f * 2160 - 0.012f * 2160);
        // the panels sized to their content (0.2.1 round 3, UI_HUD2.md 5.26): a measured width below the spec's wins,
        // anchored to the corners; above it the spec's maximum; the lanes hug the narrower panels
        {
            Frame fit = Base(3840, 2160, false);
            fit.m.cardW = 0.32f * 2160, fit.m.shipW = 0.24f * 2160, fit.m.loadW = 0.90f * 2160; // (the loadout over its max)
            const Out c = Compute(fit, S);
            CHECK_NEAR(c.card.x1, o.card.x1, 0.01);
            CHECK_NEAR(c.card.x0, c.card.x1 - 0.32 * 2160, 0.01);
            CHECK_NEAR(c.ship.x0, MX, 0.01);
            CHECK_NEAR(c.ship.x1, MX + 0.24 * 2160, 0.01);
            CHECK_NEAR(c.loadout.x0, RX - 0.40 * 2160, 0.01);
            CHECK(c.nCtx == o.nCtx);
            // the easing: snaps from 0, grows fast, holds inside the hysteresis, shrinks past it
            CHECK(hud2layout::EaseContent(0, 500, 0.016f, 26) == 500);
            CHECK(hud2layout::EaseContent(500, 600, 1.0f, 26) == 600);
            CHECK(hud2layout::EaseContent(500, 490, 0.016f, 26) == 500);
            const float s = hud2layout::EaseContent(500, 400, 0.016f, 26);
            CHECK(s < 500 && s > 400);
            CHECK(hud2layout::EaseContent(500, 0, 0.016f, 26) == 500); // (nothing measured: keeps its width)
            // a row label's real room (0.2.1 round 4): the tag's column only while it shows, the value's when it has one
            CHECK_NEAR(hud2layout::RowRoom(100, 900, 60, 20, false, 150, 25), 900 - 80 - 100, 0.01);
            CHECK_NEAR(hud2layout::RowRoom(100, 900, 60, 20, true, 150, 25), 900 - 175 - 80 - 100, 0.01);
            CHECK_NEAR(hud2layout::RowRoom(100, 900, 0, 20, false, 150, 25), 800, 0.01);
            CHECK(hud2layout::RowRoom(100, 150, 60, 20, true, 150, 25) == 0);
            // the second line in a panel's spare height (round 5): 10 px a character
            {
                auto w10 = [](const std::wstring& t) { return 10.0f * static_cast<float>(t.size()); };
                const std::wstring npos = L"", sp = L" ";
                using hud2layout::TwoLineSplit;
                CHECK(TwoLineSplit(L"JAVELIN MISSILE", 150, sp, w10) == std::wstring::npos); // fits: one line
                CHECK(TwoLineSplit(L"JAVELIN MISSILE", 100, sp, w10) == 8);                  // "JAVELIN" / "MISSILE"
                CHECK(TwoLineSplit(L"A BB CCC DDDD", 80, sp, w10) == 9);                     // most parts that fit
                CHECK(TwoLineSplit(L"LONGWORDONLY", 50, sp, w10) == std::wstring::npos);     // no break: it scrolls
                CHECK(TwoLineSplit(L"VERYLONGFIRST X", 50, sp, w10) == 14);                  // at least one part
                CHECK(TwoLineSplit(L"A  .  B", 30, L"  .  ", w10) == 6);                     // the guns' separator
                CHECK(TwoLineSplit(L"", 10, sp, w10) == std::wstring::npos);
                CHECK(TwoLineSplit(L"AB ", 20, sp, w10) == std::wstring::npos); // (nothing after the break)
                (void)npos;
            }
            // the nav's height sized to its content (round 4): its drawn bottom + the edge, floored, capped at today's
            {
                Frame nv = Base(3840, 2160, false);
                nv.navBottom = static_cast<float>(MY + 0.12 * 2160);
                const Out cn = Compute(nv, S);
                CHECK_NEAR(cn.nav.y1, MY + 0.12 * 2160 + 0.01 * 2160, 0.5);
                nv.navBottom = static_cast<float>(MY + 0.01 * 2160); // (under the floor)
                CHECK_NEAR(Compute(nv, S).nav.y1, MY + 0.10 * 2160, 0.5);
                nv.navBottom = static_cast<float>(MY + 0.60 * 2160); // (over today's 0.29: the maximum)
                CHECK_NEAR(Compute(nv, S).nav.y1, o.nav.y1, 0.01);
            }
        }
        {
            Frame big = Base(3840, 2160, false);
            big.hudScale = 130;
            const Out b = Compute(big, S);
            CHECK_NEAR(b.clearY0, b.cy + hud2layout::RingBottom(false, b.R, 2160, b.k) + 0.01 * 2160, 0.01);
            CHECK(b.k > 1.0f && b.clearY0 > o.clearY0);
        }
        // a left and a right prompt: each hugs its panel, nothing in the band
        Frame p = Base(3840, 2160, false);
        Prompts(p, 2, 0.12f, 0);
        const Out q = Compute(p, S);
        CHECK(q.nCtx == 2 && !q.ctxFallback);
        CHECK_NEAR(q.ctxX[0], g.lx0, 0.01);                       // left: from the ship panel
        CHECK_NEAR(q.ctxX[1] + q.ctxW[1], g.rx1, 0.01);           // right: to the loadout
        CHECK(q.ctxSide[0] == 0 && q.ctxSide[1] == 1);
        CHECK_NEAR(q.ctxPY[0], 0.915 * 2160, 0.01);
        for (int i = 0; i < 2; ++i)
            CHECK(q.ctxX[i] + q.ctxW[i] + 0.012f * 2160 <= 0.40f * 3840 + 0.5f || q.ctxX[i] - 0.012f * 2160 >= 0.60f * 3840 - 0.5f);
        // the boxed area counts the pills' plates, not the band between them
        CHECK_NEAR(q.ctxX1 - q.ctxX0, (g.rx1 + 0.015 * 2160) - (g.lx0 - 0.015 * 2160), 0.05);
    }
    // ---- Standard = today, across the display matrix x device x scale x the frame's states
    const float screens[][2] = {{3840, 2160}, {1920, 1080}, {1280, 720}, {1600, 1200}, {1280, 1024}, {1920, 1200},
                                {2560, 1080}, {3840, 1600}, {5120, 1440}};
    const int scales[][2] = {{100, 100}, {130, 150}, {80, 80}, {100, 130}};
    int cases = 0;
    for (const auto& s : screens)
        for (int kbm = 0; kbm < 2; ++kbm)
            for (const auto& sc : scales)
                for (int state = 0; state < 8; ++state)
                {
                    Frame f = Base(s[0], s[1], kbm != 0);
                    f.hudScale = sc[0], f.textSize = sc[1];
                    const float H = f.H;
                    switch (state)
                    {
                    case 0: // calm, no target, nothing measured
                        f.target = false;
                        break;
                    case 1: // combat: the list, 3 prompts with a hold, the nav drawn short
                        f.combat = f.list = true, f.navRight = 0.4f * H;
                        Prompts(f, 3, 0.16f, 1);
                        break;
                    case 2: // the player types (the line measures 0)
                        f.m.nCtx = 2, f.m.ctxMeasured = false, f.m.ctxGap = 0.03f * H;
                        break;
                    case 3: // an alert over the context line's column (it steps down or aside)
                        f.combat = f.list = f.alert = true;
                        Prompts(f, 2, 0.20f, 2);
                        f.m.alertW = 0.70f * H;
                        break;
                    case 4: // an alert under a fresh dialogue panel at the top (comm)
                        f.alert = true, f.dlg.fresh = f.dlg.comm = true;
                        f.dlg.x0 = 0.30f, f.dlg.y0 = 0.04f, f.dlg.x1 = 0.70f, f.dlg.y1 = 0.18f;
                        f.m.alertW = 0.45f * H;
                        Prompts(f, 1, 0.12f, 0);
                        break;
                    case 5: // the dialogue panel low (not comm): the alert searches its first clear place
                        f.alert = true, f.dlg.fresh = true;
                        f.dlg.x0 = 0.25f, f.dlg.y0 = 0.02f, f.dlg.x1 = 0.75f, f.dlg.y1 = 0.10f;
                        f.m.alertW = 0.5f * H;
                        break;
                    case 6: // the K&M contact list with more rows / the pad list with no target
                        f.list = true, f.target = false, f.listMore = true, f.listRows = f.kbm ? 10 : 2;
                        break;
                    case 7: // the K&M header alone; a wide line that loses prompts
                        f.list = true, f.listRows = f.kbm ? 0 : 2;
                        Prompts(f, 3, 0.45f, 3);
                        break;
                    }
                    Compare(f);
                    ++cases;
                }
    CHECK(cases == 9 * 2 * 4 * 8);
    CHECK(g_compared > 0 && g_exact == g_compared); // bit for bit: the core keeps today's float expressions
    if (g_exact != g_compared)
        fprintf(stderr, "hud2_layout: %d of %d values differ from today's in the last bits\n", g_compared - g_exact,
                g_compared);
    // ---- zones respected: Standard's plates outside its zones (the lint's rule and zones), at default scale
    for (const auto& s : screens)
        for (int kbm = 0; kbm < 2; ++kbm)
        {
            Frame f = Base(s[0], s[1], kbm != 0);
            f.combat = f.list = true;
            Prompts(f, 2, 0.16f, 0);
            const Out o = Compute(f, S);
            // (the rail cut at the stacked cross's top - decision 3 - and the bottom-centre band - decision 4)
            const std::vector<hud2lint::Zone> zones =
                hud2lint::ZonesOf(S, f.W, f.H, o.card.x0, o.card.x1, f.kbm, o.cons.x0, o.cons.y0, o.clearY0);
            CHECK(zones.size() == 5 && zones.back().name == "bottom" && zones.back().texts);
            if (zones.size() < 5)
                continue;
            // the cross lies in the rail's lower end, the rail split there: left of it to 0.78 H, above it to its top
            CHECK(zones[1].name == "S2" && zones[2].name == "S2");
            CHECK_NEAR(zones[1].x0, o.cons.x0, 0.01);
            CHECK_NEAR(zones[1].y1, o.cons.y0, 0.01);
            CHECK_NEAR(zones[2].x1, o.cons.x0, 0.01);
            CHECK_NEAR(zones[2].y1, 0.78 * f.H, 0.01);
            // the list as DrawList draws it (hud2_click.cpp: the header 0.034 + 0.044 H x k, rows every 0.042, plates
            // +- 0.019): on the pad its 2nd row's plate ends at 0.404 H at 16:9 and wider - 0.004 H inside S2 (y0 0.40).
            // Today's HUD, kept (Standard must not move): the lint's live baseline ran on K&M, where the list is exempt
            // in S2, so `hud2 lint` on the pad will flag it (UI_HUD2.md 5.17). Checked here as a known, bounded intrusion.
            const float rowsBottom = o.thrY0 + (0.034f + 0.044f + 0.042f * (f.listRows - 1) + 0.019f) * f.H * o.k;
            if (!f.kbm)
            {
                const float s2y0 = zones[1].y0;
                CHECK(rowsBottom - s2y0 < 0.005f * f.H); // at most 0.004 H (the known intrusion), never more
            }
            struct P
            {
                const char* comp;
                Rect r;
            };
            std::vector<P> plates = {{"card", o.card},
                                     {f.kbm ? "list" : "list.known", {o.card.x0, o.thrY0, o.card.x1, f.kbm ? o.thrY1 : o.thrY0}},
                                     {"ship", o.ship},
                                     {"loadout", o.loadout},
                                     {"loadout", o.cons}};
            // each pill's plate (the lanes, decision 4): the zones S1 / S2 / core and the bottom-centre band
            CHECK(!o.ctxFallback); // (at default scale every screen of the matrix has its lanes)
            for (int i = 0; i < o.nCtx; ++i)
                plates.push_back({"context",
                                  {o.ctxX[i] - 0.012f * f.H * o.k, o.ctxPY[i] - 0.026f * f.H * o.k,
                                   o.ctxX[i] + o.ctxW[i] + 0.012f * f.H * o.k, o.ctxPY[i] + 0.026f * f.H * o.k}});
            for (const P& p : plates)
            {
                hud2lint::Item it;
                it.kind = hud2lint::Kind::Plate, it.comp = p.comp;
                it.x0 = p.r.x0, it.y0 = p.r.y0, it.x1 = p.r.x1, it.y1 = p.r.y1;
                for (const hud2lint::Zone& z : zones)
                {
                    const bool in = !hud2lint::Exempt(it.comp, z.exempt) && hud2lint::InZone(it, z);
                    if (in)
                        fprintf(stderr, "hud2_layout: %s in zone %s at %gx%g %s (%g,%g-%g,%g; line n=%d y=%.3f H)\n", p.comp,
                                z.name.c_str(), f.W, f.H, f.kbm ? "K&M" : "pad", p.r.x0, p.r.y0, p.r.x1, p.r.y1, o.nCtx,
                                o.ctxY / f.H);
                    CHECK(!in);
                }
            }
        }
    // ---- suppression: the letterbox (Standard: the nav's chip, the ring and the alert stay)
    {
        Frame f = Base(3840, 2160, false);
        f.letterbox = true;
        const Out o = Compute(f, S);
        CHECK(o.suppressed == (Bit(Card) | Bit(List) | Bit(Ship) | Bit(Loadout) | Bit(Context)));
        CHECK(o.budgetOff == 0);
        CHECK(ContextSuppressed(RulesOf(0), false) == 0);
        CHECK(ContextSuppressed(RulesOf(3), true) == ContextSuppressed(RulesOf(0), true)); // unbuilt = Standard's
    }
    // ---- suppression: the budget (a preset rule that drops the ship panel, then the loadout, over its cap)
    {
        Frame f = Base(3840, 2160, false);
        f.combat = f.list = f.alert = true;
        Prompts(f, 2, 0.16f, 0);
        f.m.alertW = 0.5f * f.H;
        const float cap = CapOf(S, true, false, true, true);
        CHECK_NEAR(cap, 0.16, 1e-6);                                  // Standard combat pad peak (2.1 Budgets: <= 16)
        CHECK_NEAR(CapOf(S, true, true, true, true), 0.17, 1e-6);     // K&M peak (<= 17)
        CHECK_NEAR(CapOf(S, false, false, false, false), hp::kBudgetDefault, 1e-6); // calm names none: the default
        CHECK_NEAR(CapOf(hp::kPresets[1], false, false, true, false), 0.03, 1e-6); // Focus calm target (<= 3)
        CHECK_NEAR(CapOf(hp::kPresets[1], true, false, true, false), 0.07, 1e-6);  // Focus combat steady (<= 7)
        // Standard itself: over the cap (today's HUD, the lint's WARN) and nothing dropped
        const Out std0 = Compute(f, S);
        CHECK(std0.boxed > cap && std0.budgetOff == 0 && std0.suppressed == 0);
        // a rule that suppresses by the budget (ship, then the loadout, then the list): 20.5 % boxed at 4K (card 5.3,
        // list 4.0, ship 3.4, loadout 5.2, line 1.1, alert 1.4) - the ship alone leaves 17.1 %, the loadout too 11.9 %:
        // both go, in order, the list stays
        Rules r = {Bit(Nav) | Bit(Ring) | Bit(Alert), true, {Ship, Loadout, List}, 3};
        const Out o = ComputeWith(f, S, r);
        CHECK(o.budgetOff == (Bit(Ship) | Bit(Loadout)) && o.suppressed == o.budgetOff);
        CHECK(o.boxed <= cap && o.boxed < std0.boxed);
        CHECK_NEAR(std0.boxed - o.boxed, (o.ship.Area() + o.loadout.Area() + o.cons.Area()) / (f.W * f.H), 1e-6);
        // another preset's data and cap (Speedrun: its combat card 0.15 tall, loadout 0.17; combat pad peak 10 %): the
        // drops are a prefix of the order and end under the cap; a rule that names only the ship stays over it. (The
        // whole order since the lanes, decision 4: they keep both prompts - card 3.7 + list 4.0 + 2 pills 1.1 + alert 1.4
        // = 10.2 % after the ship and the loadout - where the centred line between the old 0.62 loadout and the ship kept
        // one, 9.6 %)
        const Out o1 = ComputeWith(f, hp::kPresets[2], r);
        CHECK(o1.budgetOff == (Bit(Ship) | Bit(Loadout) | Bit(List)) && o1.boxed <= o1.budget && o1.nCtx == 2);
        if (!(o1.budgetOff == (Bit(Ship) | Bit(Loadout) | Bit(List)) && o1.boxed <= o1.budget))
            fprintf(stderr, "hud2_layout: speedrun budgetOff=%u boxed=%.4f budget=%.4f nCtx=%d\n", o1.budgetOff, o1.boxed,
                    o1.budget, o1.nCtx);
        Rules shipOnly = {Bit(Nav) | Bit(Ring) | Bit(Alert), true, {Ship}, 1};
        const Out o3 = ComputeWith(f, hp::kPresets[2], shipOnly);
        CHECK(o3.budgetOff == Bit(Ship) && o3.boxed > o3.budget); // what the rule does not name is never dropped
        // under the cap: nothing (combat steady, no list / line / alert: card 5.3 + ship 3.4 + loadout 5.2 = 13.9 %
        // against the 16 % Standard names for its combat on the pad)
        Frame lean = f;
        lean.alert = false, lean.m.alertW = 0, lean.m.nCtx = 0, lean.list = false;
        const Out o2 = ComputeWith(lean, S, r);
        CHECK(o2.boxed <= o2.budget && o2.budgetOff == 0);
        // the rectangles do not move for a suppression (a part fades, the others keep their places)
        CHECK(o.loadout.x0 == std0.loadout.x0 && o.card.y1 == std0.card.y1 && o.ctxY == std0.ctxY);
        // a tighter cap drops more, in order; never what the context already suppressed
        Frame g = f;
        g.letterbox = true;
        const Out lb = ComputeWith(g, S, r);
        CHECK(lb.budgetOff == 0); // the letterbox already took ship / loadout / list: under the cap
        // under the cap: nothing (calm, no target: the ship and the loadout 8.6 % against the default 10 %)
        Frame calm = Base(3840, 2160, false);
        calm.target = false;
        const Out c0 = ComputeWith(calm, S, r);
        CHECK(c0.boxed < c0.budget && c0.budgetOff == 0);
        // a cap the first drop cannot meet: the order goes on (ship, then the loadout, then the list)
        Frame big = f;
        big.hudScale = 130, big.textSize = 150;
        const Out ob = ComputeWith(big, S, r);
        CHECK((ob.budgetOff & Bit(Ship)) != 0);
        CHECK(ob.boxed <= ob.budget || ob.budgetOff == (Bit(Ship) | Bit(Loadout) | Bit(List)));
    }
    // ---- fail closed: no screen
    {
        Frame f;
        const Out o = Compute(f, S);
        CHECK(o.W == 0 && o.card.Area() == 0 && o.suppressed == 0);
    }
}
