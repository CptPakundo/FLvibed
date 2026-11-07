// The haptics engine by scenario (modules\controller\docs\HAPTICS.md): Signals over time through Engine and Render into the recording renderer, for every pad's Caps. Nothing
// here says how it feels: that is tests\haptics_owner_card.md. These prove the logic: what fires, what each tier keeps and drops, the gates, the bytes.
#include "test.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "../modules/controller/src/pad_backend.h"
#include "../modules/controller/src/wave_synth.h"
#include "../modules/controller/src/impl/haptics_rules.h"
#include "../modules/controller/src/impl/haptics_render.h"

using namespace haptics;

namespace
{
const double kDt = 1.0 / 60;

Caps XInputPad()
{
    PadFacts f;
    f.connected = true;
    return CapsFor(f);
}
Caps XboxPad()
{
    PadFacts f;
    f.connected = true, f.impulseRoute = true;
    return CapsFor(f);
}
Caps DualSense(bool wave = false)
{
    PadFacts f;
    f.connected = true, f.sdl = true, f.type = padbackend::kTypePs5, f.waveRunning = wave;
    return CapsFor(f);
}

// A run: the engine, the renderer and the recorder, stepped by the frame.
struct Run
{
    Engine e;
    Render r;
    static RecordingSink& Rec() // (large: not on the stack)
    {
        static RecordingSink rec;
        return rec;
    }
    Gates g;
    EngineData data;
    Caps caps;
    StanceInput in;
    double now = 100;
    HapticFrame last;
    int starts[kEffectCount] = {};

    explicit Run(Caps c) : caps(c) { Rec().Clear(); }
    void Step(const Signals& s)
    {
        now += kDt;
        last = e.Step(s, g, data, now);
        for (int i = 0; i < e.FiredCount(); ++i)
            if (e.Fired(i).result == Mixer::kStarted)
                ++starts[e.Fired(i).effect];
        Rec().SetTime(now);
        r.Send(last, in, caps, now, static_cast<float>(kDt), Rec());
    }
    void For(float seconds, const Signals& s)
    {
        for (float t = 0; t < seconds; t += static_cast<float>(kDt))
            Step(s);
    }
};

Signals Flying()
{
    Signals s;
    s.ship = true;
    s.vitals.valid = true, s.vitals.ship = 7;
    s.vitals.shield = s.vitals.shieldMax = 1000, s.vitals.shieldUp = true;
    s.vitals.hull = s.vitals.hullMax = 800;
    s.throttle = 1, s.power = 1, s.gauge = 1;
    return s;
}

StanceInput Freelancer()
{
    StanceInput in;
    in.open = true;
    in.role[0] = Role::Afterburner, in.role[1] = Role::Fire;
    in.refire = 0.167f;
    return in;
}

Stance Choose(const StanceInput& in, int side, float held = 0) { return ChooseStance(in, side, held); }
} // namespace

void Test_haptics_engine()
{
    // ---- Caps: what each pad has
    {
        PadFacts none;
        CHECK(!CapsFor(none).motors);
        const Caps x = XInputPad();
        CHECK(x.motors && !x.motorsAreLR && !x.triggerMotors && !x.adaptive && !x.waveform);
        CHECK(XboxPad().triggerMotors && !XboxPad().adaptive);
        const Caps d = DualSense();
        CHECK(d.motorsAreLR && d.adaptive && !d.waveform && !d.triggerMotors);
        CHECK(DualSense(true).waveform);
        PadFacts ps4;
        ps4.connected = true, ps4.sdl = true, ps4.type = padbackend::kTypePs4;
        CHECK(!CapsFor(ps4).motorsAreLR && !CapsFor(ps4).adaptive); // only a DualSense
        PadFacts sw;
        sw.connected = true, sw.sdl = true, sw.type = padbackend::kTypeSwitchPro, sw.waveRunning = true;
        CHECK(!CapsFor(sw).waveform && !CapsFor(sw).triggerMotors);
    }

    // ---- the side of a point: the ship's right axis is the matrix's first column
    {
        const float rot[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
        const float right[3] = {100, 0, 0}, left[3] = {-5, 0, 0}, ahead[3] = {0, 0, -100}, none[3] = {0, 0, 0};
        CHECK_NEAR(PanOf(right, rot), 1, 1e-5);
        CHECK_NEAR(PanOf(left, rot), -1, 1e-5);
        CHECK_NEAR(PanOf(ahead, rot), 0, 1e-5);
        CHECK(PanOf(none, rot) == 0);
        const float turned[9] = {0, 0, 1, 0, 1, 0, -1, 0, 0}; // yawed: the ship's right is world -z
        const float worldZ[3] = {0, 0, -100};
        CHECK_NEAR(PanOf(worldZ, turned), 1, 1e-5);
    }

    // ---- can the guns fire: the game's verdict, one able gun enough, an unread list says nothing
    {
        const int ok[] = {9, 6}, refire[] = {2}, angle[] = {5, 5}, cruise[] = {6, 6}, lane[] = {7}, cloak[] = {8}, power[] = {4, 4}, ammo[] = {3};
        CHECK(FireStateOf(ok, 2, 2, false, 1) == FireState::Ready);
        CHECK(FireStateOf(refire, 1, 1, false, 1) == FireState::Ready);
        CHECK(FireStateOf(refire, 1, 1, false, 1, FireState::Locked) == FireState::Locked); // CanFire asks the refire clock first: 2 lifts no lock
        CHECK(FireStateOf(ok, 2, 2, false, 1, FireState::Locked) == FireState::Ready);      // a 9 does
        CHECK(FireStateOf(angle, 2, 2, false, 1) == FireState::Ready);
        CHECK(FireStateOf(cruise, 2, 2, false, 1) == FireState::Locked);
        CHECK(FireStateOf(lane, 1, 1, false, 1) == FireState::Locked);
        CHECK(FireStateOf(cloak, 1, 1, false, 1) == FireState::Locked);
        CHECK(FireStateOf(power, 2, 2, false, 0.5f) == FireState::Empty);
        CHECK(FireStateOf(ammo, 1, 1, false, 1) == FireState::Empty);
        CHECK(FireStateOf(ok, 2, 2, true, 1) == FireState::Locked);            // the controls excluded
        CHECK(FireStateOf(nullptr, 0, 0, false, 1) == FireState::Locked);      // no gun mounted
        CHECK(FireStateOf(nullptr, 0, 2, false, 1) == FireState::Ready);       // guns, and a read that failed: no wall on it
        CHECK(FireStateOf(nullptr, 0, 2, false, 0) == FireState::Empty);       // the capacitor still says empty
        CHECK(FireStateOf(angle, 2, 2, false, 0) == FireState::Empty);         // power first over a gun waiting
    }

    // ---- the two-motor renderer: today's rumble unchanged on a plain pad, the 100 % baseline
    {
        Run r(XInputPad());
        Signals s = Flying();
        s.throttle = 0;
        r.For(4, s); // the engine at rest: under the motors' floor, silent
        CHECK(Run::Rec().Count(RecordingSink::kMotors) == 0);
        r.e.Fire(kHandBack, 1, r.now, r.g);
        r.Step(s);
        const RecordingSink::Entry* m = Run::Rec().Last(RecordingSink::kMotors);
        CHECK(m != nullptr);
        if (m)
        {
            CHECK_NEAR(m->a, 0.6, 1e-5);
            CHECK_NEAR(m->b, 0.36, 1e-5);
        }
        r.For(0.3f, s);
        m = Run::Rec().Last(RecordingSink::kMotors);
        CHECK(m && m->a == 0 && m->b == 0); // and its stop
        CHECK(Run::Rec().Count(RecordingSink::kStance) == 0 && Run::Rec().Count(RecordingSink::kTriggerMotors) == 0);
    }

    // ---- hits: the side from the report, the kinds
    {
        Run r(DualSense());
        Signals s = Flying();
        s.throttle = 0;
        r.For(1, s);
        s.hit.seq = 5, s.hit.kind = 1, s.hit.pan = 1, s.hit.distance = 1000; // the first report is the baseline: the sequence before this session
        r.Step(s);
        s.hit.seq = 6;
        s.vitals.hull -= 100;
        r.Step(s);
        CHECK(r.starts[kHitHull] == 1);
        CHECK(r.last.mix.mass[1] > r.last.mix.mass[0] * 3);
        const RecordingSink::Entry* m = Run::Rec().Last(RecordingSink::kMotors);
        CHECK(m && m->b > m->a); // SDL's high is the right grip: a heavy hit from the right no longer leans left
        // a contact hit is a collision, not a gun's hit
        r.For(1, s);
        s.hit.seq = 7, s.hit.kind = 2, s.hit.pan = -1, s.hit.distance = 10;
        s.vitals.hull -= 40;
        r.Step(s);
        CHECK(r.starts[kCollision] == 1 && r.starts[kHitHull] == 1);
        CHECK(r.last.mix.mass[0] > r.last.mix.mass[1]);
        // a blast's centre within reach
        r.For(1, s);
        s.hit.seq = 8, s.hit.kind = 1, s.hit.pan = 0.5f, s.hit.distance = 120;
        r.Step(s);
        CHECK(r.starts[kExplosion] == 1);
        // a report 1000 m back is a round, no blast
        r.For(1, s);
        s.hit.seq = 9, s.hit.distance = 1000;
        r.Step(s);
        CHECK(r.starts[kExplosion] == 1);
        // the shield: failure, then back
        s.vitals.shieldUp = false, s.vitals.shield = 0;
        r.Step(s);
        CHECK(r.starts[kShieldFail] == 1);
        r.For(2, s);
        s.vitals.shieldUp = true, s.vitals.shield = 300;
        r.Step(s);
        CHECK(r.starts[kShieldUp] == 1);
    }

    // ---- the critical hull beats on the HUD's clock
    {
        CHECK_NEAR(BeatRate(0.25f), 1.25, 1e-4);
        CHECK_NEAR(BeatRate(0), 2.9, 1e-4);
        Run r(XInputPad());
        Signals s = Flying();
        s.throttle = 0;
        s.vitals.hull = 800 * 0.2f;
        r.For(4, s);
        const int beats = r.starts[kHullBeat];
        CHECK(beats >= 5 && beats <= 8); // about 1.5 Hz at 20 %, with the fatigue limiter sparing it
        s.vitals.hull = 800 * 0.28f; // inside the hysteresis: it goes on
        r.For(2, s);
        CHECK(r.starts[kHullBeat] > beats);
        const int more = r.starts[kHullBeat];
        s.vitals.hull = 800 * 0.5f; // repaired
        r.For(2, s);
        CHECK(r.starts[kHullBeat] == more);
    }

    // ---- shots: the clock and the ammo
    {
        ShotDetector d;
        Gun guns[3];
        guns[0].id = 1, guns[0].refire = 0.17f, guns[0].elapsed = 1, guns[0].damage = 60;
        guns[1].id = 2, guns[1].refire = 1.0f, guns[1].elapsed = 1, guns[1].damage = 900;
        guns[2].id = 3, guns[2].homing = true, guns[2].ammo = 5, guns[2].refire = 1;
        d.Step(guns, 3); // the baseline: nothing
        guns[0].elapsed = 0.01f;
        Shots s = d.Step(guns, 3);
        CHECK(s.fast > 0 && s.heavy == 0 && !s.missile);
        guns[0].elapsed = 0.03f, guns[1].elapsed = 0;
        s = d.Step(guns, 3);
        CHECK(s.fast == 0 && s.heavy > 0);
        guns[2].ammo = 4;
        s = d.Step(guns, 3);
        CHECK(s.missile && !s.torpedo);
        guns[2].torpedo = true, guns[2].ammo = 3;
        s = d.Step(guns, 3);
        CHECK(s.torpedo && !s.missile);
        // per-gun feel from data: a heavier shot is a stronger one
        CHECK(ShotLevel(900) > ShotLevel(60) && ShotLevel(60) > 0 && ShotLevel(100000) <= 1);
    }

    // ---- cruise: the charge rises, the release lands, then silence
    {
        Run r(XInputPad());
        Signals s = Flying();
        s.cruiseEngaged = true, s.chargeTime = 5;
        float rise = 0;
        bool grew = true, insideFirst = true; // (from the inside out, 7 Nov 2026: a light texture first, the mass from a third of the charge)
        for (float t = 0.1f; t < 5; t += static_cast<float>(kDt))
        {
            s.chargeElapsed = t;
            r.Step(s);
            grew = grew && r.last.bed.mass >= rise - 1e-4f;
            rise = r.last.bed.mass;
            if (t < 5.0f / 3.0f)
                insideFirst = insideFirst && r.last.bed.mass < 1e-4f && r.last.bed.tex > 0;
        }
        CHECK(grew && rise > 0.35f && insideFirst);
        s.chargeElapsed = 0; // charged: the game's clock is over
        r.Step(s);
        CHECK(r.starts[kCruiseRelease] == 1);
        r.For(1, s);
        CHECK(r.last.bed.bed == 0 && r.last.bed.mass == 0 && r.last.mix.peak == 0); // cruising: near silence
        // an aborted charge lands no thump
        Run a(XInputPad());
        Signals c = Flying();
        c.cruiseEngaged = true, c.chargeTime = 5, c.chargeElapsed = 2;
        a.Step(c);
        c.cruiseEngaged = false;
        a.Step(c);
        CHECK(a.starts[kCruiseRelease] == 0);
        // disrupted
        c.disrupted = true;
        a.Step(c);
        CHECK(a.starts[kCruiseDisrupt] == 1);
        a.Step(c);
        CHECK(a.starts[kCruiseDisrupt] == 1);
    }

    // ---- the bed: swells on a change, sinks at steady, ducks under an event, never above its cap
    {
        Run r(DualSense(true));
        Signals s = Flying();
        s.throttle = 0.2f;
        r.For(1, s);
        s.throttle = 1;
        r.Step(s);
        const float swell = r.last.bed.bed;
        r.For(5, s);
        const float rest = r.last.bed.bed;
        CHECK(swell > rest * 3 && swell <= Bed::kBedCap);
        CHECK(rest > 0 && rest <= Bed::kFresh);
        CHECK(r.last.bed.bedHz < 40 && r.last.bed.bedHz > 15);
        r.e.Fire(kHitHull, 1, r.now, r.g);
        r.Step(s);
        CHECK(r.last.bed.bed < rest * 0.5f); // ducked hard
        // per engine: another ship's range gives another bed
        EngineData heavy;
        heavy.pitchLow = -40, heavy.pitchHigh = -10;
        r.data = heavy;
        r.For(0.1f, s);
        CHECK(r.last.bed.bedHz < 28);
        // the engine killed, the bed cut
        Signals k = s;
        k.engineKilled = true;
        r.Step(k);
        CHECK(r.starts[kEngineKill] == 1 && r.last.bed.bed == 0);
    }
    // motors: a bed under the floor stays off, a swell reaches the hand
    {
        Run r(XInputPad());
        Signals s = Flying();
        s.throttle = 1;
        r.For(5, s);
        CHECK(Run::Rec().Count(RecordingSink::kMotors) == 0);
        s.throttle = 0.2f;
        r.Step(s);
        s.throttle = 1;
        r.Step(s);
        r.Step(s);
        CHECK(Run::Rec().Count(RecordingSink::kMotors) > 0);
    }

    // ---- missiles: pulses by time to impact, the side, the alternation
    {
        Run r(XboxPad());
        Signals s = Flying();
        s.throttle = 0;
        s.missileDistance = 2400, s.missileClosing = 300, s.missilePan = 0;
        r.For(2, s);
        const int far = r.starts[kMissileNear];
        s.missileDistance = 200;
        r.For(2, s);
        const int near = r.starts[kMissileNear] - far;
        CHECK(far >= 2 && near > far * 2);
        // dead ahead on an Xbox pad the two triggers take turns
        int left = 0, right = 0;
        for (int i = 0; i < 120; ++i)
        {
            r.Step(s);
            left += r.last.mix.trig[0] > 0 && r.last.mix.trig[1] == 0;
            right += r.last.mix.trig[1] > 0 && r.last.mix.trig[0] == 0;
        }
        CHECK(left > 0 && right > 0);
        s.missileDistance = -1;
        r.For(1, s);
        const int done = r.starts[kMissileNear];
        r.For(1, s);
        CHECK(r.starts[kMissileNear] == done);
    }

    // ---- radiation: random clicks by the hull's loss; the zone's own loss is no hit
    {
        Run r(DualSense(true));
        Signals s = Flying();
        s.throttle = 0, s.radiation = true;
        for (int i = 0; i < 300; ++i)
        {
            s.vitals.hull -= 0.6f;
            r.Step(s);
        }
        CHECK(r.starts[kRadClick] > 5 && r.starts[kHitHull] == 0);
    }

    // ---- set pieces: a lane by its rings, a jump to its flash, a hole's own, through the cutscene gate; a true cinematic silent
    {
        Run r(XboxPad());
        Signals s = Flying();
        s.throttle = 0;
        s.piece = SetPiece::Lane, s.ringsLeft = 3;
        r.For(1, s);
        CHECK(r.last.bed.mass > 0.05f);
        s.ringsLeft = 2;
        r.For(0.5f, s);
        s.ringsLeft = 1;
        r.For(0.5f, s);
        CHECK(r.starts[kLaneRing] == 2);
        s.piece = SetPiece::None, s.ringsLeft = -1;
        r.Step(s);
        CHECK(r.starts[kLaneExit] == 1);

        Run j(XInputPad());
        Signals t = Flying();
        t.piece = SetPiece::Jump, t.jumpLeft = 4;
        j.g.where = Where::Cutscene;
        j.For(0.1f, t);
        const float start = j.last.bed.mass;
        t.jumpLeft = 1;
        j.For(0.1f, t);
        CHECK(j.last.bed.mass > start * 2 && j.last.bed.mass > 0.4f); // it builds as the jump nears
        t.piece = SetPiece::None, t.jumpLeft = -1;
        j.Step(t);
        CHECK(j.starts[kJumpFlash] == 1); // as it ends: the gate's tail holds it open
        j.For(3, t);
        CHECK(j.last.bed.mass == 0 && j.last.mix.peak == 0);
        // the jump's reader counts down: the flash comes at the jump, once, before any load can shut the gates
        Run f(XInputPad());
        Signals w = Flying();
        w.piece = SetPiece::Jump, w.jumpLeft = 0.1f;
        f.g.where = Where::Cutscene;
        f.For(0.1f, w);
        CHECK(f.starts[kJumpFlash] == 1);
        w.piece = SetPiece::None, w.jumpLeft = -1;
        f.Step(w);
        CHECK(f.starts[kJumpFlash] == 1);

        Run h(XInputPad());
        Signals u = Flying();
        u.piece = SetPiece::Hole;
        h.g.where = Where::Cutscene;
        float lo = 1, hi = 0;
        for (int i = 0; i < 240; ++i)
        {
            h.Step(u);
            if (i > 180)
                lo = std::min(lo, h.last.bed.mass), hi = std::max(hi, h.last.bed.mass);
        }
        CHECK(hi > lo * 1.4f); // rough and uneven
        u.piece = SetPiece::None;
        h.Step(u);
        CHECK(h.starts[kHoleFlash] == 1 && h.starts[kJumpFlash] == 0);

        Run c(XInputPad());
        Signals v = Flying();
        c.g.where = Where::Cutscene;
        v.vitals.hull -= 50;
        c.For(2, v);
        c.e.Fire(kLaneRing, 1, c.now, c.g);
        c.Step(v);
        CHECK(c.last.mix.peak == 0 && Run::Rec().Count(RecordingSink::kMotors) == 0);
    }

    // ---- the gates hold the whole engine
    {
        Run r(XInputPad());
        Signals s = Flying();
        s.throttle = 0;
        r.g.focus = false;
        s.vitals.hull -= 50;
        r.For(1, s);
        s.vitals.hull -= 50;
        r.Step(s);
        CHECK(Run::Rec().Count() == 0);
        r.g.focus = true;
        r.For(1, s);
        CHECK(Run::Rec().Count() == 0); // nothing silenced is replayed
        r.g.rumble = 0;
        s.throttle = 1;
        r.For(1, s);
        CHECK(r.last.bed.bed == 0);
    }

    // ---- stances: the order, the roles, the settle
    {
        StanceInput in = Freelancer();
        CHECK(Choose(in, 1).kind == StanceKind::Feedback); // pull weight
        CHECK(Choose(in, 0).kind == StanceKind::Feedback);
        in.power = 1;
        const int full = Choose(in, 1).strength;
        in.power = 0.2f;
        CHECK(Choose(in, 1).strength < full); // thins as the capacitor drains
        in.power = 0;
        CHECK(Choose(in, 1).kind == StanceKind::Off); // limp
        in.fire = FireState::Empty, in.power = 0.5f;
        CHECK(Choose(in, 1).kind == StanceKind::Off);
        in.fire = FireState::Locked;
        const Stance wall = Choose(in, 1);
        CHECK(wall.kind == StanceKind::Feedback && wall.strength == 8 && wall.start == kStartZone); // the wall over everything
        in.held[1] = true;
        CHECK(Choose(in, 1).strength == 8);
        in.effects = 1;
        CHECK(Choose(in, 1).strength == 6); // Light keeps the wall's meaning
        in.effects = 2, in.fire = FireState::Ready, in.power = 1;
        const Stance firing = Choose(in, 1, 0);
        CHECK(firing.kind == StanceKind::Vibrate && firing.strength == kFiringStart);
        CHECK(firing.frequency == 12); // 0.167 s refire, twice the shot rate
        CHECK(Choose(in, 1, kSettle + 1).strength == kFiringRest); // held fire settles
        in.refire = 1.0f;
        CHECK(Choose(in, 1, 0).frequency == 1);
        in.held[1] = false;
        CHECK(Choose(in, 1).kind == StanceKind::Weapon); // a heavy gun's break
        // the afterburner
        in = Freelancer();
        in.held[0] = true, in.burner = true;
        const Stance burn = Choose(in, 0);
        CHECK(burn.kind == StanceKind::Vibrate && burn.start == kStartZone);
        in.gauge = 0.3f;
        CHECK(Choose(in, 0).strength <= burn.strength);
        in.gauge = 0;
        CHECK(Choose(in, 0).kind == StanceKind::Off);
        in.gauge = 0.5f, in.hasBurner = false;
        CHECK(Choose(in, 0).kind == StanceKind::Off);
        // ACE's lever: a step before full pull
        in = Freelancer();
        in.role[1] = Role::Throttle;
        const Stance lever = Choose(in, 1);
        CHECK(lever.kind == StanceKind::Profile && lever.profile[8] > lever.profile[7] && lever.profile[9] < lever.profile[8]);
        CHECK(lever.profile[0] == 0 && lever.profile[1] == 0); // past the press point
        // closed, or turned off: nothing at all
        in = Freelancer();
        in.open = false;
        CHECK(Choose(in, 1).kind == StanceKind::Off);
        in = Freelancer();
        in.effects = 0;
        CHECK(Choose(in, 1).kind == StanceKind::Off);
        in = Freelancer();
        in.role[1] = Role::None;
        CHECK(Choose(in, 1).kind == StanceKind::Off);
        // Light and the strength row
        in = Freelancer();
        in.held[1] = true;
        in.effects = 1;
        CHECK(Choose(in, 1).strength == 2);
        in.effects = 2, in.strength = 50;
        CHECK(Choose(in, 1).strength == 2);
        for (const Stance& st : {Choose(in, 1), wall, firing, lever})
            CHECK(st.strength >= 0 && st.strength <= 8);
    }

    // ---- the bytes: Nielk1's encoding
    {
        unsigned char b[kEffectBytes];
        Stance s;
        StanceBytes(s, b);
        CHECK(b[0] == 0x05 && b[1] == 0 && b[10] == 0);
        s.kind = StanceKind::Feedback, s.start = 2, s.strength = 8;
        StanceBytes(s, b);
        CHECK(b[0] == 0x21 && b[1] == 0xFC && b[2] == 0x03); // zones 2..9
        CHECK(b[3] == 0xC0 && b[4] == 0xFF && b[5] == 0xFF && b[6] == 0x3F); // 7 in each active zone's three bits
        s.kind = StanceKind::Weapon, s.start = 2, s.end = 5, s.strength = 4;
        StanceBytes(s, b);
        CHECK(b[0] == 0x25 && b[1] == 0x24 && b[2] == 0 && b[3] == 3);
        s.kind = StanceKind::Vibrate, s.start = 2, s.strength = 1, s.frequency = 30;
        StanceBytes(s, b);
        CHECK(b[0] == 0x26 && b[1] == 0xFC && b[2] == 0x03 && b[3] == 0 && b[9] == 30);
        s.frequency = 500;
        StanceBytes(s, b);
        CHECK(b[9] == 255);
        Stance p;
        p.kind = StanceKind::Profile;
        p.profile[9] = 8, p.profile[0] = 1;
        StanceBytes(p, b);
        CHECK(b[0] == 0x21 && b[1] == 0x01 && b[2] == 0x02);
        CHECK(((b[6] << 24 | b[5] << 16 | b[4] << 8 | b[3]) >> 27 & 7) == 7);
    }

    // ---- the stances' sends: on change, the wall's hold, Off at every gate
    {
        Run r(DualSense());
        r.in = Freelancer();
        Signals s = Flying();
        s.throttle = 0;
        r.For(1, s);
        CHECK(Run::Rec().Count(RecordingSink::kStance) == 1); // one send, then nothing
        // a gate: Off at once
        r.in.open = false;
        r.Step(s);
        CHECK(Run::Rec().Count(RecordingSink::kStance) == 2);
        const RecordingSink::Entry* e = Run::Rec().Last(RecordingSink::kStance);
        CHECK(e && e->left[0] == 0x05 && e->right[0] == 0x05);
        CHECK(!r.r.StanceOn());
        r.For(1, s);
        CHECK(Run::Rec().Count(RecordingSink::kStance) == 2);
        // back, then a wall flickering every frame: it may change at most every kStanceHold
        r.in.open = true;
        r.For(0.5f, s);
        const int before = Run::Rec().Count(RecordingSink::kStance);
        for (int i = 0; i < 120; ++i)
        {
            r.in.fire = i % 2 ? FireState::Locked : FireState::Ready;
            r.Step(s);
        }
        const int flips = Run::Rec().Count(RecordingSink::kStance) - before;
        CHECK(flips <= static_cast<int>(2.0f / kStanceHold) + 1 && flips >= 2);
        // the settle costs a handful of sends, not one a frame
        r.in.fire = FireState::Ready;
        r.For(0.5f, s);
        const int held0 = Run::Rec().Count(RecordingSink::kStance);
        r.in.held[1] = true;
        r.For(4, s);
        CHECK(Run::Rec().Count(RecordingSink::kStance) - held0 <= 5);
        // the logic's stop leaves nothing
        r.r.Stop(Run::Rec());
        e = Run::Rec().Last(RecordingSink::kStance);
        CHECK(e && e->left[0] == 0x05 && e->right[0] == 0x05);
        const RecordingSink::Entry* m = Run::Rec().Last(RecordingSink::kMotors);
        CHECK(m && m->a == 0 && m->b == 0);
        // an Xbox pad never gets a stance; a plain pad no trigger motor
        Run x(XboxPad());
        x.in = Freelancer();
        x.For(1, s);
        CHECK(Run::Rec().Count(RecordingSink::kStance) == 0);
    }

    // ---- Impulse: guns tick the fire trigger, hits their side, the afterburner hums, the autopilot's guns stay off an idle finger
    {
        Run r(XboxPad());
        r.in = Freelancer();
        Signals s = Flying();
        s.throttle = 0;
        s.fireMask = 2;
        s.shots.fast = 0.6f;
        r.Step(s);
        s.shots = Shots();
        CHECK(r.last.mix.trig[1] > 0 && r.last.mix.trig[0] == 0);
        const RecordingSink::Entry* t = Run::Rec().Last(RecordingSink::kTriggerMotors);
        CHECK(t && t->b > 0 && t->a == 0);
        r.For(0.5f, s);
        t = Run::Rec().Last(RecordingSink::kTriggerMotors);
        CHECK(t && t->b == 0);
        r.in.held[0] = true, r.in.burner = true;
        r.Step(s);
        t = Run::Rec().Last(RecordingSink::kTriggerMotors);
        CHECK(t && t->a > 0);
        const float hum = t->a;
        r.in.gauge = 0.2f;
        r.For(0.3f, s);
        t = Run::Rec().Last(RecordingSink::kTriggerMotors);
        CHECK(t && t->a < hum);
        // the owner's rows turn them off
        Run o(XboxPad());
        o.in = Freelancer();
        o.in.effects = 0;
        Signals u = Flying();
        u.fireMask = 2, u.shots.heavy = 1;
        o.Step(u);
        CHECK(Run::Rec().Count(RecordingSink::kTriggerMotors) == 0);
        // under the autopilot
        Run a(XboxPad());
        a.in = Freelancer();
        a.in.open = false;
        Signals v = Flying();
        v.fireMask = 4, v.shots.fast = 0.6f;
        a.Step(v);
        CHECK(a.last.mix.trig[0] == 0 && a.last.mix.trig[1] == 0 && a.last.mix.peak > 0); // the grips feel it, the triggers do not
    }

    // ---- a locked gun's refire clock runs with no shot leaving the ship: no grain (seen in game: 25 clock resets in 3 s of cruise, no bolt in the frames)
    {
        Run r(XInputPad());
        Signals s = Flying();
        s.throttle = 0, s.fire = FireState::Locked, s.shots.fast = 0.6f, s.shots.heavy = 0.6f;
        r.Step(s);
        CHECK(r.starts[kGunFast] == 0 && r.starts[kGunHeavy] == 0);
        s.fire = FireState::Ready;
        r.Step(s);
        CHECK(r.starts[kGunFast] == 1 && r.starts[kGunHeavy] == 1);
    }

    // ---- fire: dull tick on a locked press, a dry tick on an empty hold, a launcher in cruise
    {
        Run r(DualSense());
        r.in = Freelancer();
        Signals s = Flying();
        s.fire = FireState::Locked, s.firePressed = true;
        r.Step(s);
        CHECK(r.starts[kFireLocked] == 1);
        s.firePressed = false, s.fire = FireState::Empty, s.fireHeld = true;
        r.For(1, s);
        CHECK(r.starts[kGunDry] >= 3 && r.starts[kGunDry] <= 5);
        Signals c = Flying();
        c.cruiseEngaged = true, c.chargeTime = 5, c.launcherPressed = true;
        Run l(XInputPad());
        l.Step(c);
        CHECK(l.starts[kFireLocked] == 1);
    }

    // ---- the waveform: the motors stay at zero, the frame carries the bands; a stream that stops gives the motors back
    {
        Run r(DualSense(true));
        Signals s = Flying();
        s.throttle = 0;
        r.Step(s);
        r.e.Fire(kHitHull, 1, r.now, r.g, -1);
        r.Step(s);
        CHECK(Run::Rec().Count(RecordingSink::kMotors) == 0 && Run::Rec().Count(RecordingSink::kWave) > 0);
        const wavesynth::WaveFrame& w = Run::Rec().LastWave();
        CHECK(w.mass[0] > w.mass[1] && w.massHz == kEffects[kHitHull].massHz);
        r.caps = DualSense(false); // the stream failed
        r.e.Fire(kHitShield, 1, r.now, r.g);
        r.Step(s);
        CHECK(Run::Rec().Count(RecordingSink::kMotors) > 0);
        CHECK(Run::Rec().LastWave().mass[0] == 0); // and the stream was told to fall silent
    }

    // ---- a recording of a whole fight on each pad never asks for more than the pad has
    {
        for (const Caps& c : {XInputPad(), XboxPad(), DualSense(), DualSense(true)})
        {
            Run r(c);
            r.in = Freelancer();
            Signals s = Flying();
            s.fireMask = 2;
            double hull = 800;
            for (int i = 0; i < 60 * 20; ++i)
            {
                r.in.held[1] = i % 240 < 180;
                s.fireHeld = r.in.held[1];
                s.shots.fast = r.in.held[1] && i % 10 == 0 ? 0.6f : 0;
                if (i % 40 == 0)
                    s.vitals.shield = s.vitals.shield > 100 ? s.vitals.shield - 90 : s.vitals.shield, s.hit.seq++, s.hit.kind = 1, s.hit.pan = i % 80 ? 1.0f : -1.0f, s.hit.distance = 1000;
                if (i % 200 == 0 && i)
                    hull -= 30, s.vitals.hull = static_cast<float>(hull);
                s.power = 0.5f + 0.5f * sinf(i * 0.01f);
                r.Step(s);
                CHECK(r.last.mix.low >= 0 && r.last.mix.low <= 1 && r.last.mix.high <= 1 && r.last.bed.mass <= 1);
            }
            CHECK((Run::Rec().Count(RecordingSink::kStance) > 0) == c.adaptive);
            CHECK((Run::Rec().Count(RecordingSink::kTriggerMotors) > 0) == c.triggerMotors);
            CHECK((Run::Rec().Count(RecordingSink::kWave) > 0) == c.waveform);
            CHECK((Run::Rec().Count(RecordingSink::kMotors) > 0) == !c.waveform);
            CHECK(Run::Rec().Count() < 2500); // sent on change: 20 s of fight, not a command a frame per output
            for (int i = 0; i < Run::Rec().Count(); ++i)
            {
                const RecordingSink::Entry& e = Run::Rec().At(i);
                if (e.kind == RecordingSink::kMotors)
                    CHECK(e.a >= 0 && e.a <= 1 && e.b >= 0 && e.b <= 1);
            }
        }
    }
}
