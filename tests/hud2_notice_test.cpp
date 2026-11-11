// HUD 2's flight notice (modules\hud\src\hud2_notice.h, its place hud2_layout.h PlaceNotice; GitHub issues 70 and 69): the
// card's time (4 s from the last event, faded in 0.15 s and out 0.4 s, skin_motion none on and off at once), payouts that
// add up while it shows, item counts merged by kind and the "+n MORE" line past four kinds, the credits watch (a rise in
// flight only, primed after a dock or a load, unarmed reads only prime, no fall), the money's words; and its place: the
// bottom-left corner when the ship panel is held back, above the panel when it shows, never over a reserved area, the
// clear fields or the lower centre, none in the letterbox, its own area registered.
#include "../modules/hud/src/hud2_layout.h"
#include "../modules/hud/src/hud2_notice.h"

#include "test.h"

void Test_hud2_notice()
{
    using namespace hud2notice;
    // ---- the time and the fade
    {
        Notice n;
        CHECK(!n.Up() && n.Empty());
        n.Credit(50000, 16000);
        CHECK(n.credits && n.cash == 50000 && n.gain == 16000 && n.left == kHoldS);
        n.Step(kFadeInS / 2, 0);
        CHECK_NEAR(n.alpha, 0.5f, 0.01f);
        n.Step(kFadeInS, 0);
        CHECK(n.alpha == 1.0f);
        n.Step(kHoldS - 1.5f * kFadeInS - 0.01f, 0); // (just before its time is up: still full)
        CHECK(n.alpha == 1.0f && n.left > 0);
        n.Step(0.02f, 0); // time up: fading out
        CHECK(n.left == 0 && n.alpha < 1.0f && n.Up() && n.credits);
        n.Step(kFadeOutS, 0); // faded: emptied
        CHECK(!n.Up() && n.Empty() && n.gain == 0);
    }
    {
        Notice n; // skin_motion none: on and off at once
        n.Credit(10, 5);
        n.Step(0.01f, 2);
        CHECK(n.alpha == 1.0f);
        n.Step(kHoldS, 2);
        CHECK(n.alpha == 0.0f && n.Empty());
    }
    // ---- payouts add up while the card shows (and while it fades); a new card after it has gone
    {
        Notice n;
        n.Credit(50000, 16000);
        n.Step(3.0f, 0);
        n.Credit(55000, 5000);
        CHECK(n.cash == 55000 && n.gain == 21000 && n.left == kHoldS);
        n.Step(kHoldS - 0.01f, 0);
        n.Step(0.1f, 0); // fading
        CHECK(n.left == 0 && n.alpha > 0 && n.alpha < 1);
        n.Credit(56000, 1000);
        CHECK(n.gain == 22000);
        n.Step(kHoldS + kFadeOutS + 0.1f, 0);
        CHECK(n.Empty());
        n.Credit(57000, 1000);
        CHECK(n.gain == 1000);
        n.Credit(57000, 0); // (no rise: nothing)
        n.Credit(56000, -1000);
        CHECK(n.gain == 1000 && n.cash == 57000);
    }
    // ---- items merged by kind; past four kinds one "+n MORE" line, each extra kind counted once
    {
        Notice n;
        n.Add(7, L"SHIELD BATTERY", 5);
        n.Step(2.0f, 0);
        n.Add(7, L"SHIELD BATTERY", 3);
        CHECK(n.nItems == 1 && n.items[0].count == 8 && n.left == kHoldS && n.Loot());
        n.Add(9, L"NANOBOTS", 2);
        n.Add(0, L"WATER", 1), n.Add(0, L"WATER", 2), n.Add(0, L"OXYGEN", 1); // (unknown kinds merge by name)
        CHECK(n.nItems == 4 && n.items[1].count == 2 && n.items[2].count == 3 && n.items[3].name == L"OXYGEN");
        n.Add(11, L"JUSTICE MK I", 1), n.Add(12, L"PIRATE GUN", 1), n.Add(11, L"JUSTICE MK I", 1);
        CHECK(n.nItems == 4 && n.nExtra == 2);
        n.Add(7, L"SHIELD BATTERY", 0); // (no units: nothing)
        CHECK(n.items[0].count == 8);
        n.Credit(100, 50); // a payout and loot share the one card
        CHECK(n.credits && n.Loot());
        n.Clear();
        CHECK(n.Empty() && !n.Up());
    }
    // ---- the credits watch
    {
        CashWatch w;
        CHECK(w.Step(1000, true) == 0);    // (the first read primes)
        CHECK(w.Step(17000, true) == 16000);
        CHECK(w.Step(16000, true) == 0);   // (a fall: nothing)
        CHECK(w.Step(17000, true) == 1000);
        CHECK(w.Step(-1, true) == 0);      // (unread: kept)
        CHECK(w.Step(18000, true) == 1000);
        CHECK(w.Step(50000, false) == 0);  // (unarmed: a load's first reads prime only)
        CHECK(w.Step(50001, true) == 1);
        w.Forget();                        // (docked: a purchase or a sale never shows)
        CHECK(w.Step(10, true) == 0 && w.Step(10, true) == 0);
    }
    // ---- a game load, also from space into space (0.4.6's final check: $400,000 in flight, a save with $500,000 loaded
    // from the pause menu showed CREDITS $500,000 +$100,000): the load is seen once, the card goes, the watch forgets and
    // its first reads after the load (unarmed for the settle) only prime; a payout after it shows as before
    {
        LoadWatch lw;
        CHECK(!lw.Step(3));                // (the first read primes: the loads before this logic ran are not news)
        CHECK(!lw.Step(3));
        CashWatch w;
        Notice n;
        CHECK(w.Step(400000, true) == 0);
        n.Credit(400000, w.Step(401000, true)); // (a card standing at the load)
        CHECK(n.Up());
        CHECK(lw.Step(4) && !lw.Step(4)); // the load, once
        n.Clear(), w.Forget();
        CHECK(!n.Up() && n.Empty());
        CHECK(w.Step(500000, false) == 0 && w.Step(500000, false) == 0); // (the settle: prime only)
        CHECK(w.Step(500000, true) == 0);  // armed: no rise, no card
        CHECK(w.Step(516000, true) == 16000); // a payout after the load: shown
        CHECK(lw.Step(5));                 // (New Game, the death screen's loads: each a new count)
    }
    CHECK(Money(50000) == L"$50,000" && Money(1234567) == L"$1,234,567" && Money(999) == L"$999" && Money(-16000) == L"-$16,000");

    // ---- its place (Standard and Focus, 4K and 1280x800)
    using namespace hud2layout;
    for (int preset = 0; preset < 2; ++preset)
        for (int size = 0; size < 2; ++size)
        {
            const hp::Preset& P = hp::kPresets[preset];
            Frame f;
            f.W = size ? 1280.0f : 3840.0f, f.H = size ? 800.0f : 2160.0f;
            f.m.noticeW = 0.30f * f.H, f.m.noticeH = 0.11f * f.H;
            // calm: the bottom-left panel held back - the corner
            f.held = Bit(Ship);
            Out o = Compute(f, P);
            CHECK(o.noticeShow);
            CHECK_NEAR(o.notice.x0, o.mx, 0.5f);
            CHECK_NEAR(o.notice.y1, o.ship.y1, 0.5f);
            CHECK(o.res.Placed(ANotice) && o.res.at[ANotice].x0 == o.notice.x0);
            CHECK(o.res.Free(o.notice, kAreasAll & ~o.res.quiet & ~(1u << ANotice)));
            CHECK(o.notice.x1 < kBottomBandX0W * f.W);
            // ... but while the held panel is still fading out it keeps its area: the notice keeps off it (it stood over it, 0.4.6)
            f.drawn = Bit(Ship);
            o = Compute(f, P);
            CHECK(!(o.res.quiet & (1u << AShip)) && (!o.noticeShow || o.res.Free(o.notice, 1u << AShip)));
            f.drawn = 0;
            // the panel shows (a fight, escorts): above it, clear of it
            f.held = 0;
            o = Compute(f, P);
            CHECK(o.noticeShow);
            CHECK(o.notice.y1 <= o.ship.y0 - kNoticeGap * f.H * o.k + 0.5f);
            // Focus: the area holds the three escort rows as drawn (hud2_parts.cpp DrawShipStrip; 0.4.6's final check: the
            // first row's top stood above it and the notice touched it), and the notice keeps off them
            if (preset == 1)
            {
                const float rowTop = o.holdY - 2 * 0.042f * f.H * o.k - 0.013f * f.H * o.k;
                CHECK(o.shipTop > 0 && o.res.at[AShip].y0 <= rowTop + 0.01f);
                CHECK(o.notice.y1 <= rowTop - kNoticeGap * f.H * o.k + 0.5f);
            }
            else
                CHECK(o.shipTop == 0);
            CHECK(o.res.Free(o.notice, kAreasAll & ~o.res.quiet & ~(1u << ANotice)));
            // the subtitles' panel over the place above the ship panel: it keeps off it (higher up, or nowhere)
            f.dlg.fresh = true, f.dlg.x0 = 0.0f, f.dlg.y0 = 0.55f, f.dlg.x1 = 0.30f, f.dlg.y1 = 0.78f;
            o = Compute(f, P);
            if (o.noticeShow)
                CHECK(o.notice.y1 < 0.55f * f.H || o.notice.y0 > 0.78f * f.H || o.notice.x0 > 0.30f * f.W);
            f.dlg.fresh = false;
            // the letterbox: no place; no notice: no place
            f.letterbox = true;
            CHECK(!Compute(f, P).noticeShow);
            f.letterbox = false, f.m.noticeW = 0;
            o = Compute(f, P);
            CHECK(!o.noticeShow && !o.res.Placed(ANotice));
        }

    // ---- the subtitles' comm feed in the chat band (GitHub issues 77 / 78, the lead's 1280x800 frame: a card over TRACTOR
    // ALL): a reserved area while its cards show; the left lane's rows it meets close (their prompts take the other lane or
    // give way), the notice keeps off it, the alert cards do not (the feed narrows for them)
    {
        const hp::Preset& S = hp::kPresets[0];
        Frame f;
        f.W = 1280.0f, f.H = 800.0f, f.held = Bit(Ship);
        const float k = Scale(f), H = f.H;
        f.m.ctxMeasured = true, f.m.nCtx = 2;
        for (int i = 0; i < 2; ++i)
        {
            f.m.glyph[i] = 0.036f * H * k, f.m.label[i] = 0.12f * H * k;
            f.m.base[i] = promptfit::ContextPromptWidth(f.m.glyph[i], 0.036f * H * k, f.m.label[i]);
            f.m.side[i] = 0; // (both in the left lane: they stack)
        }
        f.m.dims.word = 0.05f * H * k, f.m.dims.gapBefore = promptfit::GapBefore(0.036f * H * k),
        f.m.dims.ringChord = promptfit::RingChord(0.036f * H * k);
        f.m.ctxGap = 0.03f * H * k, f.m.ctxWord = 0.024f * H * k;
        Out o = Compute(f, S);
        CHECK(o.nCtx == 2 && !o.res.Placed(AChat));
        // the feed's card where the lead's frame had it: over the left lane's upper row
        const Rect up = o.res.at[ACtx0 + 1].y0 < o.res.at[ACtx0].y0 ? o.res.at[ACtx0 + 1] : o.res.at[ACtx0];
        f.dlg.cx0 = 0.0125f, f.dlg.cy0 = (up.y0 - 0.06f * H) / H, f.dlg.cx1 = 0.30f, f.dlg.cy1 = (up.y0 + up.y1) * 0.5f / H;
        o = Compute(f, S);
        CHECK(o.res.Placed(AChat) && std::string(AreaPart(AChat)) == "chat");
        for (int i = 0; i < o.nCtx; ++i)
            CHECK(o.res.Free(o.res.at[ACtx0 + i], 1u << AChat));
        CHECK(o.nCtx >= 1); // (the lower row stays)
        // the notice keeps off it
        f.m.noticeW = 0.25f * H, f.m.noticeH = 0.12f * H;
        o = Compute(f, S);
        if (o.noticeShow)
            CHECK(o.res.Free(o.notice, 1u << AChat, kNoticeGap * H * o.k - 0.5f));
        // an alert card beside the ring is not moved by it
        Frame a = f;
        a.m.noticeW = 0, a.dlg.cx0 = 0, a.dlg.cy0 = 0.40f, a.dlg.cx1 = 0.50f, a.dlg.cy1 = 0.60f; // (a wide feed card level with the ring)
        a.alert = true, a.m.alertW = 0.2f * H, a.m.alertSide = -1;
        Frame b = a;
        b.dlg.cx1 = b.dlg.cx0 = 0;
        const Out oa = Compute(a, S), ob = Compute(b, S);
        CHECK(oa.alertX0 == ob.alertX0 && oa.alertY == ob.alertY);
    }
}
