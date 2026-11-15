// The model slot's game-free half (modules\hud\src\modelslot_rules.h; audit 70b F7): the rectangle and clip maths, the
// distance that fits a model (CamSetFov's vertical half angle, the smaller side), the placement (pitch 0 = the experiment's
// matrix), the keys, the table's state machine (ask, load, draw, hide, grace, drop, the world's change, the 12 places, a failed
// load not retried until its file changes) and the world compare.
#include "../modules/hud/src/modelslot_rules.h"

#include "test.h"

void Test_modelslot()
{
    using namespace modelslot;
    // ---- rectangles
    {
        const Rect vp{0, 0, 1280, 800};
        CHECK(Inside(Rect{0, 0, 1280, 800}, vp));
        CHECK(Inside(Rect{1080, 600, 200, 200}, vp));
        CHECK(!Inside(Rect{1081, 600, 200, 200}, vp)); // one pixel past the right edge
        CHECK(!Inside(Rect{-1, 0, 10, 10}, vp));
        CHECK(!Inside(Rect{10, 10, 0, 10}, vp)); // empty
        CHECK(Drawable(Rect{10, 10, 100, 100}, vp, Rect{}));                 // no clip of the page's own
        CHECK(Drawable(Rect{10, 10, 100, 100}, vp, Rect{0, 0, 200, 200}));
        CHECK(!Drawable(Rect{150, 150, 100, 100}, vp, Rect{0, 0, 200, 200})); // partly out of the clip: not drawn
        CHECK(!Drawable(Rect{1200, 10, 100, 100}, vp, Rect{}));              // partly off the screen
        // the experiment's 12 squares of 200 at 1280x800 centred: exactly the 6 that fit (48_ probe3: drawn=6)
        int fit = 0;
        const int size = 200, x0 = (1280 - size) / 2, y0 = (800 - size) / 2;
        for (int i = 0; i < 12; ++i)
            fit += Drawable(Rect{x0 + (i % 4) * (size + 8), y0 + (i / 4) * (size + 8), size, size}, vp, Rect{});
        CHECK(fit == 6);
        // 4K with a letterboxed viewport
        CHECK(Drawable(Rect{1800, 900, 240, 240}, Rect{0, 120, 3840, 1920}, Rect{}));
        CHECK(!Drawable(Rect{1800, 100, 240, 240}, Rect{0, 120, 3840, 1920}, Rect{}));
        int c[4];
        Corners(Rect{540, 300, 200, 200}, c);
        CHECK(c[0] == 540 && c[1] == 300 && c[2] == 739 && c[3] == 499); // inclusive, as the probe built it
    }
    // ---- distance: the probe's formula for squares; the smaller side for a tall rectangle; clamps
    {
        const float d = Distance(10.0f, 0.85f, 25.0f, 1.0f);
        CHECK_NEAR(d, 10.0f / (0.85f * tanf(25.0f * kPi / 180.0f)), 1e-4f);
        CHECK_NEAR(Distance(10.0f, 0.85f, 25.0f, 2.0f), d, 1e-4f);     // wider than tall: the height decides
        CHECK_NEAR(Distance(10.0f, 0.85f, 25.0f, 0.5f), 2.0f * d, 1e-3f); // taller than wide: the width decides
        CHECK_NEAR(Distance(0.0f, 1.0f, 45.0f, 1.0f), 1.0f, 1e-4f);    // no bounds: radius 1
        CHECK(Distance(1.0f, 0.0f, 25.0f, 1.0f) == Distance(1.0f, 0.05f, 25.0f, 1.0f)); // fill clamped
        CHECK(Distance(1.0f, 1.0f, 0.0f, 1.0f) == Distance(1.0f, 1.0f, 1.0f, 1.0f));    // fov clamped
    }
    // ---- placement: pitch 0 is the experiment's matrix and centre; any turn keeps the centre on the axis at -dist
    {
        const float centre[3] = {1.0f, 2.0f, 3.0f};
        float xf[12];
        const float yaw = 0.7f, c = cosf(yaw), s = sinf(yaw);
        Place(yaw, 0.0f, centre, 50.0f, xf);
        const float want[12] = {c, 0, s, 0, 1, 0, -s, 0, c, -(c * 1 + s * 3), -2.0f, -50.0f - (-s * 1 + c * 3)};
        bool same = true;
        for (int i = 0; i < 12; ++i)
            same = same && fabsf(xf[i] - want[i]) < 1e-5f;
        CHECK(same);
        Place(0.4f, -0.3f, centre, 20.0f, xf);
        const float px = xf[0] * centre[0] + xf[1] * centre[1] + xf[2] * centre[2] + xf[9];
        const float py = xf[3] * centre[0] + xf[4] * centre[1] + xf[5] * centre[2] + xf[10];
        const float pz = xf[6] * centre[0] + xf[7] * centre[1] + xf[8] * centre[2] + xf[11];
        CHECK_NEAR(px, 0.0f, 1e-4f);
        CHECK_NEAR(py, 0.0f, 1e-4f);
        CHECK_NEAR(pz, -20.0f, 1e-4f);
        // the 3x3 stays a rotation (rows of length 1)
        for (int r = 0; r < 3; ++r)
            CHECK_NEAR(xf[r * 3] * xf[r * 3] + xf[r * 3 + 1] * xf[r * 3 + 1] + xf[r * 3 + 2] * xf[r * 3 + 2], 1.0f, 1e-5f);
    }
    // ---- keys
    {
        CHECK(KeyOf("modelprobe", 0) != KeyOf("modelprobe", 1));
        CHECK(KeyOf("modelprobe", 0) != KeyOf("shipdealer", 0));
        CHECK(KeyOf("modelprobe", 3) == KeyOf("modelprobe", 3));
        CHECK(KeyOf(nullptr, 0) != 0);
    }
    // ---- the table: ask, load, draw; not asked: hidden for the grace, then given back
    {
        Table t;
        Ask a;
        strcpy_s(a.file, "ships\\civilian\\cv_starflier\\cv_starflier.cmp");
        const unsigned k = KeyOf("page", 0);
        CHECK(t.Show(0, a, 0.0) == -1); // key 0
        Ask none;
        CHECK(t.Show(k, none, 0.0) == -1); // no file
        const int i = t.Show(k, a, 0.0);
        CHECK(i == 0 && t.slots[i].state == State::Wanted && t.Used() == 1);
        t.BeginPass();
        CHECK(t.Next(i, 0.01) == Act::Load);
        t.Loaded(i, true);
        CHECK(t.slots[i].state == State::Ready && t.Held() == 1 && t.loads == 1);
        // asked again the next frame: drawn
        t.Show(k, a, 0.02);
        t.BeginPass();
        CHECK(t.Next(i, 0.03) == Act::Draw);
        // an ask after the pass in the same frame counts for the next pass (a view in the overlay)
        t.Show(k, a, 0.035);
        t.BeginPass();
        CHECK(t.Next(i, 0.05) == Act::Draw);
        // not asked: nothing drawn, the holder kept for the grace
        t.BeginPass();
        CHECK(t.Next(i, 0.06) == Act::None);
        t.BeginPass();
        CHECK(t.Next(i, 0.035 + kGraceS - 0.01) == Act::None);
        t.BeginPass();
        CHECK(t.Next(i, 0.035 + kGraceS) == Act::Release);
        t.Freed(i);
        t.Released(i);
        CHECK(t.Held() == 0 && t.Used() == 0 && t.frees == 1);
    }
    // ---- drop: given back at the next pass; a same-frame ask after the drop keeps it
    {
        Table t;
        Ask a;
        strcpy_s(a.file, "a.cmp");
        const int i = t.Show(7, a, 1.0);
        t.BeginPass();
        t.Loaded(i, true);
        t.Drop(7);
        t.BeginPass();
        CHECK(t.Next(i, 1.02) == Act::Release);
        t.Drop(7);
        t.Show(7, a, 1.03);
        t.BeginPass();
        CHECK(t.Next(i, 1.04) == Act::Draw);
        t.Drop(12345); // an unknown key: nothing
        CHECK(t.Used() == 1);
    }
    // ---- a new file reloads; a failed load is not retried until the file or the world changes
    {
        Table t;
        Ask a, b;
        strcpy_s(a.file, "good.cmp");
        strcpy_s(b.file, "missing.cmp");
        int i = t.Show(1, a, 0);
        t.BeginPass();
        t.Loaded(i, true);
        t.slots[i].turned = 2.0f;
        t.Show(1, b, 0.01);
        CHECK(t.slots[i].state == State::Wanted && t.slots[i].held && t.slots[i].turned == 0); // the old holder freed by the load
        t.BeginPass();
        CHECK(t.Next(i, 0.02) == Act::Load);
        t.Loaded(i, false);
        CHECK(t.slots[i].state == State::Failed && !t.slots[i].held && t.failures == 1);
        t.Show(1, b, 0.03);
        t.BeginPass();
        CHECK(t.Next(i, 0.04) == Act::None); // not retried
        t.Show(1, a, 0.05);
        t.BeginPass();
        CHECK(t.Next(i, 0.06) == Act::Load); // another file: tried
        t.Loaded(i, true);
        // the world changed: every holder freed, the ask loads again
        t.Freed(i);
        CHECK(t.slots[i].state == State::Wanted && !t.slots[i].held && t.frees == 1);
        t.Show(1, a, 0.07);
        t.BeginPass();
        CHECK(t.Next(i, 0.08) == Act::Load);
        // a failed one is retried after the world changed too: it holds no holder, so the game's half frees nothing for it
        // and Forget turns it back to Wanted (the verifier's find, 13 Nov 2026: Freed alone never reached it)
        t.Loaded(i, false);
        CHECK(t.slots[i].state == State::Failed && !t.slots[i].held);
        t.Forget();
        CHECK(t.slots[i].state == State::Wanted);
        t.Show(1, a, 0.09);
        t.BeginPass();
        CHECK(t.Next(i, 0.10) == Act::Load);
        // a draw that faulted: failed
        t.Loaded(i, true);
        t.DrawFailed(i);
        CHECK(t.slots[i].state == State::Failed);
    }
    // ---- 12 places: the 13th ask refused (counted); a place given back serves the next
    {
        Table t;
        Ask a;
        strcpy_s(a.file, "x.3db");
        for (unsigned k = 1; k <= kSlots; ++k)
            CHECK(t.Show(k, a, 0) >= 0);
        CHECK(t.Show(100, a, 0) == -1 && t.refused == 1);
        CHECK(t.Show(5, a, 0) == 4); // a known key keeps its place
        t.Drop(5);
        t.BeginPass();
        CHECK(t.Next(4, 0.01) == Act::Release);
        t.Freed(4);
        t.Released(4);
        CHECK(t.Show(100, a, 0.02) == 4);
    }
    // ---- the world
    {
        World w{3, 0, 100, 0, 1280, 800, 10.0};
        World n = w;
        n.at = 10.016;
        CHECK(Changed(w, n) == Why::Same);
        n.loads = 4;
        CHECK(Changed(w, n) == Why::Load);
        n = w, n.base = 77;
        CHECK(Changed(w, n) == Why::Base);
        n = w, n.system = 101;
        CHECK(Changed(w, n) == Why::System);
        n = w, n.resets = 1;
        CHECK(Changed(w, n) == Why::Device);
        n = w, n.w = 3840, n.h = 2160;
        CHECK(Changed(w, n) == Why::Screen);
        n = w, n.at = 10.0 + kGapS + 0.01;
        CHECK(Changed(w, n) == Why::Gap);
        CHECK(!strcmp(WhyName(Why::Load), "load") && !strcmp(WhyName(Why::Gap), "gap"));
    }
}
