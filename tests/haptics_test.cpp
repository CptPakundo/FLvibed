// The haptics skeleton's pure pieces (controller\src\impl\haptics_rules.h, IDEAS U30, WORK_QUEUE Q5 slice 3): the
// effect table, the envelopes, the mixer (max per motor, ducking, rate limit + merge, voices, fatigue, the master
// percent), the silence rules, the renderer's send policy and the hit detector.
#include "test.h"

#include "../controller/src/impl/haptics_rules.h"

#include <algorithm>
#include <cstring>

void Test_haptics()
{
    using namespace haptics;

    // ---- the table: every row named, unique, with its other cue, a sane shape and range
    for (int i = 0; i < kEffectCount; ++i)
    {
        const EffectDef& d = kEffects[i];
        CHECK(d.name && *d.name && EffectByName(d.name) == i);
        CHECK(d.cue && strlen(d.cue) > 10); // never the only cue: each names its counterpart
        CHECK(d.low >= 0 && d.low <= 1 && d.high >= 0 && d.high <= 1 && (d.low > 0 || d.high > 0));
        CHECK(d.attack >= 0 && d.hold >= 0 && d.release >= 0 && d.attack + d.hold + d.release > 0);
        CHECK(Duration(d) > 0 && Duration(d) <= 0.5f); // short effects only: nothing buzzes on
        CHECK(d.prio >= 1 && d.prio <= 7 && d.minGap >= Duration(d) * 0.5f);
    }
    CHECK(EffectByName("nope") == -1);
    CHECK(kEffects[kHitShield].high > kEffects[kHitShield].low); // the shield: sharp, the high motor
    CHECK(kEffects[kHitHull].low > kEffects[kHitHull].high);     // the hull: heavy, the low motor
    CHECK(kEffects[kConfirm].cat == kPrompt && kEffects[kHandBack].cat == kAssist && kEffects[kHitHull].cat == kWorld);

    // ---- the hand-back is today's pulse: 0.6 / 0.36 at full strength, flat for 0.18 s, then over
    const EffectDef& hb = kEffects[kHandBack];
    CHECK_NEAR(hb.low, 0.6, 1e-6);
    CHECK_NEAR(hb.high, 0.6 * 0.6, 1e-6);
    CHECK_NEAR(Duration(hb), 0.18, 1e-6);
    CHECK(Envelope(hb, 0) == 1 && Envelope(hb, 0.1f) == 1 && Envelope(hb, 0.179f) == 1 && Envelope(hb, 0.181f) < 0);

    // ---- envelopes: attack ramp, hold, release, repeats
    {
        const EffectDef d = {"t", kWorld, 1, 1, 0.1f, 0.2f, 0.1f, 1, 0, 3, 0.1f, "test cue text"};
        CHECK_NEAR(Envelope(d, 0.05f), 0.5, 1e-5);
        CHECK(Envelope(d, 0.15f) == 1);
        CHECK_NEAR(Envelope(d, 0.35f), 0.5, 1e-5);
        CHECK(Envelope(d, 0.41f) < 0);
        CHECK(Envelope(d, -1) == 0);
        const EffectDef& c = kEffects[kConfirm]; // two ticks: on, gap, on, over
        CHECK(Envelope(c, 0.01f) == 1);
        CHECK(Envelope(c, 0.10f) == 0);
        CHECK(Envelope(c, 0.16f) == 1);
        CHECK(Envelope(c, Duration(c) + 0.001f) < 0);
    }

    // ---- a hit's weight -> strength: a moderate floor, the top kept for heavy blows
    CHECK(HitStrength(0) == 0 && HitStrength(-1) == 0);
    CHECK_NEAR(HitStrength(0.01f), 0.28, 1e-5);
    CHECK(HitStrength(0.05f) < HitStrength(0.1f) && HitStrength(0.25f) == 1 && HitStrength(3) == 1);

    // ---- silence
    Gates g;
    for (int c = kWorld; c <= kPrompt; ++c)
        CHECK(!*Silence(g, static_cast<Category>(c))); // in flight, pad in hand, in front: everything may play
    {
        Gates s = g;
        s.rumble = 0;
        CHECK(!strcmp(Silence(s, kPrompt), "rumble 0"));
        s = g, s.enabled = false;
        CHECK(*Silence(s, kWorld) && *Silence(s, kPrompt));
        s = g, s.focus = false; // the one focus gate
        CHECK(!strcmp(Silence(s, kWorld), "focus") && !strcmp(Silence(s, kPrompt), "focus"));
        s = g, s.pad = false; // the keyboard and mouse in use: a pad on the desk never buzzes
        CHECK(!strcmp(Silence(s, kAssist), "device kbm"));
        s = g, s.paused = true; // paused: the world and the autopilot silent, the game's own (pausing) boxes not
        CHECK(!strcmp(Silence(s, kWorld), "paused") && !strcmp(Silence(s, kAssist), "paused") && !*Silence(s, kPrompt));
        s = g, s.where = Where::Cutscene;
        CHECK(!strcmp(Silence(s, kWorld), "cutscene") && !strcmp(Silence(s, kPrompt), "cutscene"));
        s = g, s.where = Where::Docked;
        CHECK(!strcmp(Silence(s, kWorld), "docked") && !*Silence(s, kPrompt));
        s = g, s.where = Where::Menu;
        CHECK(!strcmp(Silence(s, kWorld), "menu") && !strcmp(Silence(s, kPrompt), "menu"));
    }

    // ---- the mixer: one effect, its envelope, the master percent
    {
        Mixer m;
        Gates full = g;
        full.rumble = 100;
        CHECK(m.Trigger(kHandBack, 1, 10.0, full) == Mixer::kStarted);
        Out o = m.Update(10.0, full);
        CHECK_NEAR(o.low, 0.6, 1e-6);
        CHECK_NEAR(o.high, 0.36, 1e-6);
        Gates sixty = g; // the default 60 %: the hand-back's 0.36 / 0.216 (slice 2's read-back: 0.36 / 0.22)
        o = m.Update(10.1, sixty);
        CHECK_NEAR(o.low, 0.36, 1e-6);
        CHECK_NEAR(o.high, 0.216, 1e-6);
        o = m.Update(10.19, sixty);
        CHECK(o.low == 0 && o.high == 0 && m.Playing() == 0);
    }
    // two at once: the maximum per motor; below a higher priority: ducked
    {
        Mixer m;
        Gates full = g;
        full.rumble = 100;
        CHECK(m.Trigger(kHitShield, 1, 0, full) == Mixer::kStarted);
        CHECK(m.Trigger(kHitHull, 1, 0, full) == Mixer::kStarted);
        Out o = m.Update(0.01, full); // hull prio 4 over shield 3: the shield's high motor ducked
        CHECK_NEAR(o.low, kEffects[kHitHull].low, 1e-6);
        CHECK_NEAR(o.high, std::max(kEffects[kHitHull].high, kEffects[kHitShield].high * Mixer::kDuck), 1e-6);
        CHECK(m.Playing() == 2);
    }
    // the rate limit: a trigger inside the gap merges (the highest level) and starts when the gap is over
    {
        Mixer m;
        Gates full = g;
        full.rumble = 100;
        CHECK(m.Trigger(kHitHull, 0.3f, 0, full) == Mixer::kStarted);
        CHECK(m.Trigger(kHitHull, 0.5f, 0.05, full) == Mixer::kMerged);
        CHECK(m.Trigger(kHitHull, 0.4f, 0.10, full) == Mixer::kMerged);
        CHECK_NEAR(m.Pending(kHitHull), 0.5, 1e-6);
        m.Update(0.15, full);
        CHECK(m.Pending(kHitHull) > 0); // still inside the gap
        Out o = m.Update(0.21, full);   // over: the merged hit starts with 0.5
        CHECK(m.Pending(kHitHull) == 0 && m.Counted(Mixer::kStarted) == 2 && m.Counted(Mixer::kMerged) == 2);
        CHECK_NEAR(o.low, kEffects[kHitHull].low * 0.5, 1e-6);
        // the same effect again after its gap restarts its voice (no second voice)
        CHECK(m.Trigger(kHitHull, 1, 0.5, full) == Mixer::kStarted && m.Playing() == 1);
    }
    // voices: full -> the lowest priority gives way to one as important; a less important one is refused
    {
        Mixer m;
        Gates full = g;
        full.rumble = 100;
        CHECK(m.Trigger(kHitShield, 1, 0, full) == Mixer::kStarted);  // 3
        CHECK(m.Trigger(kHitHull, 1, 0, full) == Mixer::kStarted);    // 4
        CHECK(m.Trigger(kShieldFail, 1, 0, full) == Mixer::kStarted); // 6
        CHECK(m.Trigger(kHandBack, 1, 0, full) == Mixer::kStarted);   // 6
        CHECK(m.Playing() == 4);
        CHECK(m.Trigger(kConfirm, 1, 0.01, full) == Mixer::kStarted); // 5 replaces the shield hit (3)
        CHECK(!m.Plays(kHitShield) && m.Plays(kConfirm));
        CHECK(m.Trigger(kHitShield, 1, 0.3, full) == Mixer::kFull); // 3 under 4, 5, 6, 6
    }
    // silence: triggers dropped (never replayed), running effects of a silenced category stop at once, merged ones too
    {
        Mixer m;
        Gates s = g;
        s.paused = true;
        CHECK(m.Trigger(kHitHull, 1, 0, s) == Mixer::kSilent);
        CHECK(m.Trigger(kConfirm, 1, 0, s) == Mixer::kStarted); // a box that pauses the game still asks
        Out o = m.Update(0.01, s);
        CHECK(o.high > 0);
        s.paused = false;
        m.Update(0.02, s); // the hull hit of the pause never comes back
        CHECK(!m.Plays(kHitHull) && m.Pending(kHitHull) == 0 && m.Counted(Mixer::kSilent) == 1);
        Gates f = g;
        CHECK(m.Trigger(kHandBack, 1, 1.0, f) == Mixer::kStarted);
        CHECK(m.Trigger(kHitHull, 1, 1.0, f) == Mixer::kStarted);
        CHECK(m.Trigger(kHitHull, 1, 1.05, f) == Mixer::kMerged);
        f.focus = false; // the game goes to the background
        o = m.Update(1.06, f);
        CHECK(o.low == 0 && o.high == 0 && m.Playing() == 0 && m.Pending(kHitHull) == 0);
        f.focus = true;
        o = m.Update(1.5, f);
        CHECK(o.low == 0 && o.high == 0 && m.Playing() == 0); // nothing resumes
        // a cutscene silences the hand-back and the hits; docked: the world silent, a box may ask
        Gates c = g;
        c.where = Where::Cutscene;
        CHECK(m.Trigger(kHandBack, 1, 2.0, c) == Mixer::kSilent);
        c.where = Where::Docked;
        CHECK(m.Trigger(kHitShield, 1, 2.0, c) == Mixer::kSilent && m.Trigger(kConfirm, 1, 2.0, c) == Mixer::kStarted);
        // rumble 0: nothing at all
        Gates z = g;
        z.rumble = 0;
        CHECK(m.Trigger(kShieldFail, 1, 3.0, z) == Mixer::kSilent);
        CHECK(m.Update(3.0, z).low == 0);
    }
    // fatigue: under sustained fire the motors stay busy -> the hits drop out until it eases; the important ones play
    {
        Mixer m;
        Gates full = g;
        full.rumble = 100;
        double t = 0;
        int fatigued = 0;
        for (int i = 0; i < 6 * 120; ++i, t += 1.0 / 120) // 6 s at 120 fps, a hull hit every frame
        {
            if (m.Trigger(kHitHull, 0.5f, t, full) == Mixer::kFatigued)
                ++fatigued;
            m.Update(t, full);
        }
        CHECK(fatigued > 0);
        CHECK(m.Duty() > 0.3f && m.Duty() < 0.9f); // capped near kFatigueOn instead of running at 1
        CHECK(m.Trigger(kShieldFail, 1, t, full) == Mixer::kStarted); // spared
        // a few hits a second never trip it
        Mixer calm;
        double u = 0;
        int dropped = 0;
        for (int i = 0; i < 10 * 60; ++i, u += 1.0 / 60) // 10 s, a hit every 0.5 s
        {
            if (i % 30 == 0 && calm.Trigger(kHitShield, 0.5f, u, full) != Mixer::kStarted)
                ++dropped;
            calm.Update(u, full);
        }
        CHECK(dropped == 0 && calm.Duty() < Mixer::kFatigueOn);
    }

    // ---- the renderer: a start and a stop at once, changes at most every kMinInterval, refreshes, no repeats
    {
        Renderer r;
        Out s;
        Out on;
        on.low = 0.36f, on.high = 0.216f;
        CHECK(!r.Due(Out(), 0, s)); // at rest: nothing to send
        CHECK(r.Due(on, 1.0, s) && s.low == on.low && s.high == on.high); // the start, its exact value
        CHECK(!r.Due(on, 1.01, s));                                         // unchanged
        Out other = on;
        other.low = 0.2f;
        CHECK(!r.Due(other, 1.02, s));                    // changed inside kMinInterval: later
        CHECK(r.Due(other, 1.031, s) && s.low == 0.2f);   // then
        CHECK(!r.Due(other, 1.2, s));                     // unchanged, inside kRefresh
        CHECK(r.Due(other, 1.3, s) && s.low == 0.2f);     // refreshed
        Out tiny = other;
        tiny.low += Renderer::kStep * 0.3f;               // below the step: not a change
        CHECK(!r.Due(tiny, 1.4, s));
        CHECK(r.Due(Out(), 1.41, s) && s.low == 0 && s.high == 0 && !r.On()); // the stop at once
        CHECK(!r.Due(Out(), 1.5, s));                     // stopped: no repeats
        CHECK(r.Sent() == 4);
        // the hand-back as the renderer sends it: two commands, the pulse and its stop
        Renderer h;
        Mixer m;
        int sends = 0;
        bool firstOk = false;
        CHECK(m.Trigger(kHandBack, 1, 5.0, g) == Mixer::kStarted);
        for (double t = 5.0; t < 5.5; t += 1.0 / 144)
            if (h.Due(m.Update(t, g), t, s))
            {
                if (++sends == 1)
                    firstOk = std::fabs(s.low - 0.36f) < 1e-6f && std::fabs(s.high - 0.216f) < 1e-6f;
            }
        CHECK(sends == 2 && firstOk && !h.On());
    }

    // ---- the hit detector
    {
        HitDetector d;
        Vitals v;
        v.valid = true, v.ship = 7, v.shield = 1000, v.shieldMax = 1000, v.shieldUp = true, v.hull = 800, v.hullMax = 800;
        Hits h = d.Step(v); // the baseline
        CHECK(!h.shieldFailed && h.shieldShare == 0 && h.hullShare == 0);
        v.shield = 950;
        h = d.Step(v);
        CHECK_NEAR(h.shieldShare, 0.05, 1e-6);
        CHECK(h.hullShare == 0);
        v.shield = 960; // regenerating: no hit
        CHECK(d.Step(v).shieldShare == 0);
        v.shield = 959.8f; // a fall below half a point: noise
        CHECK(d.Step(v).shieldShare == 0);
        v.shield = 0, v.shieldUp = false; // down: a failure, not a hit
        h = d.Step(v);
        CHECK(h.shieldFailed && h.shieldShare == 0);
        v.hull = 760; // the hull takes it now
        h = d.Step(v);
        CHECK(!h.shieldFailed);
        CHECK_NEAR(h.hullShare, 40.0 / 800, 1e-6);
        CHECK(!d.Step(v).shieldFailed); // still down: one failure only
        v.ship = 8, v.hull = 100;       // another ship (a load): its first frame is a baseline
        h = d.Step(v);
        CHECK(h.hullShare == 0 && !h.shieldFailed);
        Vitals none;
        CHECK(d.Step(none).hullShare == 0);
        v.hull = 50; // after an invalid frame (docked, a cutscene's gap): a baseline again
        CHECK(d.Step(v).hullShare == 0);
    }
}
