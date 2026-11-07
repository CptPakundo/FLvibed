// modules\hud\src\hud2_target.h (pure): the targeting display's timing, sizes, seat and steps, over the display matrix: the block never leaves the screen.
#include "test.h"

#include "../modules/hud/src/hud2_target.h"

using namespace hud2target;

namespace
{
In Base(float W, float H, float x, float y)
{
    In in;
    in.dt = 1 / 60.0f, in.id = 7, in.onScreen = true, in.x = x, in.y = y, in.r = 0.02f * H, in.W = W, in.H = H, in.k = 1, in.R = 0.10f * H;
    in.shield = 1, in.hull = 1;
    return in;
}
Out Run(State& s, In in, float seconds)
{
    Out o;
    for (float t = 0; t < seconds; t += in.dt)
        o = s.Step(in);
    return o;
}
} // namespace

void Test_hud2_target()
{
    const float W = 3840, H = 2160;
    // sizes follow the object, floored and capped
    CHECK_NEAR(MarkSize(1, H, 1), 0.030 * H, 0.01);
    CHECK_NEAR(MarkSize(0.03f * H, H, 1), 0.06 * H, 0.01);
    CHECK_NEAR(MarkSize(H, H, 1), 0.10 * H, 0.01);
    // nothing without a target
    {
        State s;
        In in = Base(W, H, 2800, 500);
        in.id = 0;
        CHECK(!s.Step(in).show);
    }
    // a new target: the card 2.0 s, the change 0.35 s, the class line first; settled under the ship
    {
        State s;
        const In in = Base(W, H, 2800, 500);
        Out o = Run(s, in, 1.9f);
        CHECK(o.show && o.settle == 0 && o.classA == 1 && o.plateA == 1);
        CHECK(o.side == 1 && o.cardX0 > in.x);
        o = Run(s, in, 0.3f);
        CHECK(o.settle > 0.2f && o.settle < 1);
        CHECK(o.classA < o.plateA);
        o = Run(s, in, 0.3f);
        CHECK_NEAR(o.settle, 1, 1e-5);
        CHECK(o.plateA == 0);
        CHECK(o.caretY < in.y && o.blockY > in.y);
        // the four carets (7 Nov 2026): the block and the card clear the lower and the side ones
        CHECK(o.blockY >= in.y + o.mark / 2 + QuadReach(o.mark, H, 1) - 0.5f);
        CHECK(o.cardX0 >= in.x + o.mark / 2 + QuadReach(o.mark, H, 1) - 0.5f);
        CHECK(QuadReach(o.mark, H, 1) > 0 && CaretSize(o.mark, H, 1) >= 0.008f * H - 1e-3f);
        // a new target starts it again
        In next = in;
        next.id = 8;
        CHECK(s.Step(next).settle == 0);
    }
    // skin_motion: FADES fades in place, NONE cuts
    {
        State s;
        In in = Base(W, H, 2800, 500);
        in.motion = MotionNone;
        Run(s, in, 1.99f);
        const Out o = Run(s, in, 0.02f);
        CHECK(o.settle == 1);
        CHECK(Breathe(0.3f, 1, MotionFades) == 1 && Breathe(0.3f, 1, MotionNone) == 1);
        CHECK(Breathe(0.5f, 1, MotionFull) > 0.99f && Breathe(0.0f, 1, MotionFull) < 0.01f);
    }
    // the seat: enter and leave with hysteresis, on the cue's row, side changes only past the middle
    {
        State s;
        const float cx = W / 2, cy = H / 2;
        In in = Base(W, H, cx + 0.12f * H, cy);
        Out o = Run(s, in, 3.0f); // (past the card, which keeps its place)
        CHECK(o.seat && o.side == 1);
        CHECK(o.blockX == cx && o.blockY + State::BlockH(H, 1) <= cy - in.R); // on top of the reticle, clear of it
        in.x = cx + 0.15f * H; // between enter and leave: it stays
        CHECK(Run(s, in, 0.1f).seat);
        in.x = cx + 0.20f * H; // out, after the seat's hold
        CHECK(!Run(s, in, 0.7f).seat);
        in.x = cx + 0.05f * H;
        CHECK(Run(s, in, 0.7f).seat);
        { // the hold: a target that dips out and back inside it does not move the seat
            State h;
            In q = Base(W, H, cx + 0.12f * H, cy);
            Run(h, q, 3.0f);
            q.x = cx + 0.20f * H;
            CHECK(Run(h, q, 0.3f).seat == false);
            q.x = cx + 0.12f * H;
            CHECK(Run(h, q, 0.1f).seat == false); // (just left: it holds)
        }
        in.x = cx - 0.01f * H; // inside the dead band: the side holds
        CHECK(Run(s, in, 0.1f).side == 1);
        Run(s, in, 1.0f);
        in.x = cx - 0.07f * H;
        CHECK(Run(s, in, 0.1f).side == -1);
        in.x = cx + 0.07f * H; // straight back: not within the hold
        CHECK(Run(s, in, 0.1f).side == -1);
        CHECK(Run(s, in, 1.0f).side == 1);
    }
    // a jump travels whatever made it: the target crossing the middle beside the ring swaps the block's side of it
    {
        State s;
        In in = Base(W, H, W / 2 - 0.30f * H, H / 2 + 0.20f * H);
        in.ring = {W / 2 - 0.2f * H, H / 2 - 0.25f * H, W / 2 + 0.2f * H, H / 2 + 0.25f * H}, in.hasRing = true;
        Run(s, in, 3.0f);
        float last = s.Step(in).blockX, worst = 0;
        for (int i = 0; i < 60; ++i)
        {
            in.x += 0.012f * H;
            const float x = s.Step(in).blockX;
            worst = std::max(worst, fabsf(x - last));
            last = x;
        }
        CHECK(worst < 0.15f * H); // (the swap is 0.5 H and more across the ring)
    }
    // with the ring faded there is no seat: the words stay with the target, and the seat comes back with the ring
    {
        State s;
        In in = Base(W, H, W / 2 + 0.05f * H, H / 2);
        CHECK(Run(s, in, 3.0f).seat);
        in.ringGone = true;
        Out o = Run(s, in, 1.0f);
        CHECK(!o.seat && o.blockY > in.y);
        CHECK_NEAR(o.blockX, in.x, 1.0);
        in.ringGone = false;
        CHECK(Run(s, in, 1.0f).seat);
    }
    // a card over the ring moves the seat up over it, with its words whole
    {
        State s;
        In in = Base(W, H, W / 2 + 0.05f * H, H / 2);
        const float y0 = Run(s, in, 3.0f).blockY;
        in.panels[0] = {W / 2 - 0.3f * H, y0 - 0.01f * H, W / 2 + 0.3f * H, y0 + 0.04f * H}, in.nPanels = 1;
        Out o = Run(s, in, 1.0f);
        CHECK(o.seat && o.blockY + State::BlockH(H, 1) <= in.panels[0].y0 && o.wordsA == 1);
    }
    // the seat stands over the reticle: a side change does not move it
    {
        State s;
        In in = Base(W, H, W / 2 + 0.05f * H, H / 2);
        Run(s, in, 3.0f);
        const float before = s.Step(in).blockX;
        in.x = W / 2 - 0.07f * H;
        Run(s, in, 1.0f); // (past the hold)
        CHECK_NEAR(s.Step(in).blockX, before, 1.0);
    }
    // the card stands beside the ship: at the screen's edge on its other side, never over it, and it does not hop back
    {
        State s;
        In in = Base(W, H, W - 0.05f * H, 500);
        const Out o = s.Step(in);
        CHECK(o.cardX0 + kCardW * H <= in.x);
        in.x = W - 0.50f * H;
        CHECK(s.Step(in).cardX0 + kCardW * H <= in.x);
    }
    // the seat waits for the change: a ship flying into the ring in its first seconds does not take it yet
    {
        State s;
        In in = Base(W, H, W / 2 + 0.40f * H, H / 2);
        Run(s, in, 0.5f);
        in.x = W / 2 + 0.05f * H;
        CHECK(!Run(s, in, 0.5f).seat);
        CHECK(Run(s, in, 2.0f).seat);
    }
    // no room for the card: at its opening the settled form at once, later the change starts
    {
        State s;
        In in = Base(W, H, 2800, 500);
        in.panels[0] = {2700, 400, 3400, 600}, in.nPanels = 1;
        CHECK(s.Step(in).settle > 0.99f);
        State l;
        In f = Base(W, H, 2800, 500);
        Run(l, f, 1.0f);
        f.panels[0] = {2700, 400, 3400, 600}, f.nPanels = 1;
        Run(l, f, 0.1f);
        CHECK(Run(l, f, 0.5f).settle > 0.99f);
    }
    // a block that opens with no room opens faded, not whole
    {
        State s;
        In in = Base(W, H, 2800, 500);
        in.panels[0] = {0, 0, W, H}, in.nPanels = 1;
        CHECK(s.Step(in).wordsA < 0.01f);
    }
    // off screen: the block stands where it stood, and fades; a target never on screen has none
    {
        State s;
        In in = Base(W, H, 2800, 500);
        const Out on = Run(s, in, 3.0f);
        in.onScreen = false, in.x = -5000;
        const Out off = s.Step(in);
        CHECK(off.show);
        CHECK_NEAR(off.blockX, on.blockX, 1.0);
        CHECK_NEAR(off.blockY, on.blockY, 1.0);
        State n;
        In never = Base(W, H, -5000, 500);
        never.onScreen = false;
        CHECK(!n.Step(never).show);
    }
    // off the seat the block keeps off the ring, above the ship; its time on the card is seen time
    {
        State s;
        In in = Base(W, H, W / 2, H / 2 - 0.30f * H);
        in.ring = {W / 2 - 0.2f * H, H / 2 - 0.25f * H, W / 2 + 0.2f * H, H / 2 + 0.2f * H}, in.hasRing = true;
        const Out o = Run(s, in, 3.5f);
        CHECK(!o.seat && o.blockY < in.y);
    }
    // an alert card has the place: the block goes above the ship; with no room there the words go
    {
        State s;
        In in = Base(W, H, 2800, 1200);
        const Out free = Run(s, in, 3.0f);
        in.panels[0] = {free.blockX - 50, free.blockY - 5, free.blockX + 50, free.blockY + 50}, in.nPanels = 1;
        const Out moved = Run(s, in, 1.0f);
        CHECK(moved.blockY < in.y && moved.wordsA > 0.99f);
        in.panels[1] = {0, 0, W, in.y}, in.nPanels = 2;
        CHECK(Run(s, in, 1.0f).wordsA < 0.01f);
    }
    // after a hit: numbers 3 s, a 0.4 s fade, white pips for what was lost; another hit starts it again
    {
        State s;
        In in = Base(W, H, 2800, 500);
        Run(s, in, 0.1f);
        in.hull = 0.8f;
        Out o = s.Step(in);
        CHECK(o.hitHullA == 1 && o.hitShieldA == 0);
        CHECK_NEAR(o.lostHull, 0.2, 1e-4);
        // the white trail (7 Nov 2026): held a moment, then drained, long before the numbers go
        o = Run(s, in, 0.3f);
        CHECK_NEAR(o.lostHull, 0.2, 1e-4);
        o = Run(s, in, 0.2f);
        CHECK(o.lostHull > 0 && o.lostHull < 0.2f);
        o = Run(s, in, 0.6f);
        CHECK(o.lostHull == 0 && o.hitHullA == 1);
        o = Run(s, in, 2.1f);
        CHECK(o.hitHullA > 0 && o.hitHullA < 1);
        o = Run(s, in, 0.3f);
        CHECK(o.hitHullA == 0 && o.lostHull == 0);
        in.hull = 0.7f;
        CHECK(s.Step(in).hitHullA == 1);
    }
    // off screen: no caret, the words wait 0.5 s, then fade
    {
        State s;
        In in = Base(W, H, 3700, 500);
        Run(s, in, 0.1f);
        in.onScreen = false;
        Out o = Run(s, in, 0.3f);
        CHECK(o.show && o.wordsA == 1 && o.caretX < 0);
        o = Run(s, in, 0.4f);
        CHECK(o.wordsA < 1);
        o = Run(s, in, 0.3f);
        CHECK(!o.show);
    }
    // the matrix: wherever the target stands, the block and the card stay on screen
    const float screens[][2] = {{3840, 2160}, {1280, 800}, {1920, 1080}, {1024, 768}, {2560, 1080}};
    for (const auto& sc : screens)
        for (float fx = 0; fx <= 1.0f; fx += 0.1f)
            for (float fy = 0; fy <= 1.0f; fy += 0.1f)
                for (const float r : {2.0f, 0.3f * sc[1]})
                {
                    State s;
                    In in = Base(sc[0], sc[1], fx * sc[0], fy * sc[1]);
                    in.r = r;
                    const Out o = Run(s, in, 0.05f);
                    CHECK(o.blockX - kBarW * sc[1] / 2 >= 0 && o.blockX + kBarW * sc[1] / 2 <= sc[0]);
                    CHECK(o.blockY >= 0 && o.blockY + State::BlockH(sc[1], 1) <= sc[1]);
                    CHECK(o.cardX0 >= 0 && o.cardX0 + kCardW * sc[1] <= sc[0]);
                    CHECK(o.cardY0 >= 0 && o.cardY0 + kCardH * sc[1] <= sc[1]);
                }
    // close ships: the richest form that touches nothing
    {
        Box forms[4] = {{0, 0, 100, 20}, {0, 0, 50, 20}, {0, 0, 30, 10}, {0, 0, 10, 10}};
        CHECK(FitForm(forms, nullptr, 0) == CaretName);
        const Box wide = {60, 0, 200, 20};
        CHECK(FitForm(forms, &wide, 1) == CaretDistance);
        const Box mid = {40, 0, 200, 20};
        CHECK(FitForm(forms, &mid, 1) == CaretPips);
        const Box near = {5, 0, 200, 20};
        CHECK(FitForm(forms, &near, 1) == CaretBare);
    }
}
