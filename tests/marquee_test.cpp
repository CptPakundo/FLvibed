// core\game\marquee.h (pure): the marquee a cut text scrolls by (the owner, 31 Oct 2026, HUD 2 decision 1; UI_HUD2.md
// 5.20) - a text that fits never moves; a cut one starts as the fit rule's cut form (its prefix + the ellipsis), holds
// 1.5 s, scrolls at the speed until its end is right-aligned in the room, holds 1.5 s, fades out, fades back in on the
// start and repeats; the motion is continuous (the offset never steps while the text is visible, the only jump - the
// reset - happens at alpha 0), never draws past its room, and does not depend on the frame rate.
#include "test.h"

#include "game/marquee.h"

void Test_marquee()
{
    using namespace marquee;
    const Timing tm;
    // the chosen values (UI_HUD2.md 5.20: Android's 1.2 s, the owner's ~1.5 s; 0.05 H/s at the floor; the panels' 0.25 s)
    CHECK(tm.startHoldS == 1.5f && tm.endHoldS == 1.5f && tm.fadeS == 0.25f);
    CHECK(tm.speedH == 0.05f);
    // the speed: 0.05 H/s at the floor (4K: 108 px/s for the 51.84 px floor), as many characters a second when larger
    CHECK_NEAR(Speed(tm, 2160, 51.84f, 51.84f), 108.0, 1e-3);
    CHECK_NEAR(Speed(tm, 2160, 2 * 51.84f, 51.84f), 216.0, 1e-3);
    // ---- a text that fits never moves (any time)
    for (double t : {0.0, 1.0, 2.7, 100.0})
    {
        const Frame f = At(tm, 80, 100, 80, 6, 108, t);
        CHECK(!f.moving && f.offset == 0 && f.alpha == 1 && f.ellipsis == 0);
        CHECK_NEAR(f.clip, 80, 1e-6);
    }
    CHECK(!At(tm, 100, 100, 100, 6, 108, 3.0).moving); // exactly the room: fits
    // ---- a cut text: 300 px in a 200 px room, the cut form's prefix 180 px + a 12 px ellipsis
    const float textW = 300, room = 200, prefix = 180, ell = 12, v = 100; // overflow 100 px at 100 px/s: a 1 s scroll
    CHECK_NEAR(Period(tm, textW - room, v), 0.25 + 1.5 + 1.0 + 1.5 + 0.25, 1e-6);
    // the start: the cut form exactly (the prefix, the ellipsis at its end), full alpha from t = 0 (no fade-in first)
    Frame f = At(tm, textW, room, prefix, ell, v, 0);
    CHECK(f.moving && f.phase == 0 && f.offset == 0 && f.alpha == 1);
    CHECK_NEAR(f.clip, prefix, 1e-4);
    CHECK_NEAR(f.ellipsis, 1, 1e-6);
    f = At(tm, textW, room, prefix, ell, v, 1.49);
    CHECK(f.phase == 0 && f.offset == 0);
    // scrolling: after the 1.5 s hold, at the speed
    f = At(tm, textW, room, prefix, ell, v, 1.5 + 0.5);
    CHECK(f.phase == 1);
    CHECK_NEAR(f.offset, 50, 1e-3);
    CHECK(f.clip <= room - ell + 1e-3 && f.ellipsis == 1); // more than an ellipsis still hidden: room kept for it
    // the end: right-aligned in the room, no ellipsis, held 1.5 s
    f = At(tm, textW, room, prefix, ell, v, 1.5 + 1.0 + 0.1);
    CHECK(f.phase == 2);
    CHECK_NEAR(f.offset, textW - room, 1e-3);
    CHECK_NEAR(f.clip, room, 1e-3);
    CHECK(f.ellipsis == 0 && f.alpha == 1);
    // the fade out, then the start fading back in
    f = At(tm, textW, room, prefix, ell, v, 1.5 + 1.0 + 1.5 + 0.125);
    CHECK(f.phase == 3);
    CHECK_NEAR(f.alpha, 0.5, 1e-3);
    CHECK_NEAR(f.offset, textW - room, 1e-3);
    const double P = Period(tm, textW - room, v);
    f = At(tm, textW, room, prefix, ell, v, P - 0.25 + 0.125); // (the next cycle's fade-in: t = P - fade is its start)
    CHECK(f.phase == 4 && f.offset == 0);
    CHECK_NEAR(f.alpha, 0.5, 1e-3);
    // and repeats
    f = At(tm, textW, room, prefix, ell, v, P + 2.0);
    const Frame g = At(tm, textW, room, prefix, ell, v, 2.0);
    CHECK(f.phase == g.phase);
    CHECK_NEAR(f.offset, g.offset, 1e-3);
    // ---- continuous: over three cycles at 1 ms the offset moves at most the speed x dt (+ a hair) while visible; the
    // only jump (the reset) happens with the text invisible; the clip and the ellipsis move without steps; nothing past
    // the room; the visible text never ends left of the prefix at the start
    {
        int jumps = 0, badJump = 0, beyond = 0, clipSteps = 0;
        Frame prev = At(tm, textW, room, prefix, ell, v, 0);
        for (int i = 1; i <= static_cast<int>(3 * P * 1000); ++i)
        {
            const Frame cur = At(tm, textW, room, prefix, ell, v, i / 1000.0);
            const float d = fabsf(cur.offset - prev.offset);
            if (d > v * 0.001f + 1e-3f)
            {
                ++jumps;
                if (prev.alpha > 0.01f || cur.alpha > 0.01f)
                    ++badJump;
            }
            else if (fabsf(cur.clip - prev.clip) > 2 * v * 0.001f + 1e-3f || fabsf(cur.ellipsis - prev.ellipsis) > 0.05f)
                ++clipSteps;
            if (cur.clip > room + 1e-3f || cur.offset < 0 || cur.offset > textW - room + 1e-3f)
                ++beyond;
            prev = cur;
        }
        CHECK(jumps == 3 || jumps == 2); // one reset per cycle (the last may fall past the sampled end)
        CHECK(badJump == 0);
        CHECK(clipSteps == 0);
        CHECK(beyond == 0);
    }
    // ---- frame-rate independent: 30 fps and 144 fps sampling see the same picture at the same moments
    for (double t : {0.0, 1.0, 1.9, 2.3, 3.9, 4.6, 7.25})
    {
        const Frame a = At(tm, textW, room, prefix, ell, v, t), b = At(tm, textW, room, prefix, ell, v, (t * 144) / 144);
        CHECK(a.offset == b.offset && a.alpha == b.alpha && a.clip == b.clip);
    }
    // ---- sub-pixel: 1 ms of scroll moves 0.1 px at 100 px/s (no snapping to whole pixels)
    {
        const Frame a = At(tm, textW, room, prefix, ell, v, 1.75), b = At(tm, textW, room, prefix, ell, v, 1.751);
        CHECK(b.offset - a.offset > 0.05f && b.offset - a.offset < 0.15f);
    }
    // ---- a barely cut text (overflow under an ellipsis): the start is still the cut form, the end still the full end
    {
        const Frame s = AtOffset(205, 200, 190, 12, 0), e = AtOffset(205, 200, 190, 12, 5);
        CHECK_NEAR(s.clip, 190, 1e-4);
        CHECK(s.ellipsis > 0.4f && s.ellipsis <= 1);
        CHECK_NEAR(e.clip, 200, 1e-4);
        CHECK(e.ellipsis == 0);
    }
    // ---- a prefix of nothing (only the ellipsis fit): the clip opens from 0 and reaches the room by the end
    {
        const Frame s = AtOffset(500, 20, 0, 12, 0), e = AtOffset(500, 20, 0, 12, 480);
        CHECK(s.clip == 0 && s.ellipsis == 1);
        CHECK_NEAR(e.clip, 20, 1e-4);
    }
    // ---- no speed (or the still form): the cut form, never moving
    {
        const Frame s = At(tm, textW, room, prefix, ell, 0, 2.5), st = Static(textW, room, prefix, ell);
        CHECK(s.offset == 0 && s.alpha == 1);
        CHECK_NEAR(s.clip, prefix, 1e-4);
        CHECK(st.offset == 0 && st.ellipsis == 1);
    }
    // ---- negative time = the start
    CHECK(At(tm, textW, room, prefix, ell, v, -5).offset == 0);
}
