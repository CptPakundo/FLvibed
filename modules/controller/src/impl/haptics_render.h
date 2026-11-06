// FLvibed - the haptics renderers (modules\controller\docs\HAPTICS.md 6, 7): what a pad has (Caps), which stance each trigger takes, the trigger effect's bytes, and the
// frame turned into what that pad can do. Pure and header only, as haptics_rules.h; everything leaves through a Sink, so a test or `haptics record` sees exactly what a
// pad would get. Include after haptics_rules.h, pad_backend.h and wave_synth.h.
#pragma once

namespace haptics
{
// ---------------------------------------------------------------- what the pad has
struct Caps
{
    bool motors = true;         // two rumble motors (a pad with none just ignores them)
    bool motorsAreLR = false;   // SDL's low and high are the left and right grips (a DualSense): mass and texture fold into each grip
    bool triggerMotors = false; // an Xbox pad's Impulse Triggers
    bool adaptive = false;      // Adaptive Triggers
    bool waveform = false;      // the grips' stream runs: the motors stay at zero
};

// What gamepad.cpp knows about the pad in use.
struct PadFacts
{
    bool connected = false;
    bool sdl = false;       // SDL reads it, or sends for it
    int type = padbackend::kTypeUnknown;
    bool impulseRoute = false; // XInput reads it and SDL's WGI driver sends for it
    bool waveRunning = false;
};

inline Caps CapsFor(const PadFacts& f)
{
    Caps c;
    if (!f.connected)
    {
        c.motors = false;
        return c;
    }
    if (f.sdl && f.type == padbackend::kTypePs5)
        c.motorsAreLR = c.adaptive = true, c.waveform = f.waveRunning;
    if ((f.sdl && f.type == padbackend::kTypeXboxOne) || f.impulseRoute)
        c.triggerMotors = true;
    return c;
}

// ---------------------------------------------------------------- the trigger's role
enum class Role : int
{
    None,
    Fire,
    Afterburner,
    Throttle, // ACE: the lever, with detents before afterburner and reverse
};

// ---------------------------------------------------------------- stances
enum class StanceKind : int
{
    Off,      // also "limp": no resistance at all means empty
    Feedback, // resistance from a zone on
    Weapon,   // resistance between two zones, then a break
    Vibrate,  // resistance and vibration from a zone on
    Profile,  // resistance per zone
};

struct Stance
{
    StanceKind kind = StanceKind::Off;
    int start = 0, end = 0;  // zones 0..9
    int strength = 0;        // 1..8
    int frequency = 0;       // Hz by the community's reading (Vibrate)
    int profile[10] = {};    // 0..8 per zone (Profile)
};

// One trigger effect, the 11 bytes of a DualSense's effect block. Nielk1's encoding, the 0x2x family: only it carries zones
// (docs\research\haptics-best-in-class.md 8; P4 confirms it on the Edge).
constexpr int kEffectBytes = 11;
inline void StanceBytes(const Stance& s, unsigned char out[kEffectBytes])
{
    for (int i = 0; i < kEffectBytes; ++i)
        out[i] = 0;
    const auto zones = [&out](const int* strength) { // the active zones and three bits of strength each
        unsigned active = 0, force = 0;
        for (int i = 0; i < 10; ++i)
            if (strength[i] > 0)
            {
                active |= 1u << i;
                force |= static_cast<unsigned>((strength[i] - 1) & 7) << (3 * i);
            }
        out[1] = static_cast<unsigned char>(active & 0xFF);
        out[2] = static_cast<unsigned char>(active >> 8);
        out[3] = static_cast<unsigned char>(force & 0xFF);
        out[4] = static_cast<unsigned char>((force >> 8) & 0xFF);
        out[5] = static_cast<unsigned char>((force >> 16) & 0xFF);
        out[6] = static_cast<unsigned char>((force >> 24) & 0xFF);
    };
    int all[10] = {};
    switch (s.kind)
    {
    case StanceKind::Off: out[0] = 0x05; break;
    case StanceKind::Feedback:
        for (int i = s.start; i < 10; ++i)
            all[i] = s.strength;
        out[0] = 0x21;
        zones(all);
        break;
    case StanceKind::Profile:
        out[0] = 0x21;
        zones(s.profile);
        break;
    case StanceKind::Weapon:
    {
        const unsigned both = (1u << s.start) | (1u << s.end);
        out[0] = 0x25;
        out[1] = static_cast<unsigned char>(both & 0xFF);
        out[2] = static_cast<unsigned char>(both >> 8);
        out[3] = static_cast<unsigned char>((s.strength - 1) & 7);
        break;
    }
    case StanceKind::Vibrate:
        for (int i = s.start; i < 10; ++i)
            all[i] = s.strength;
        out[0] = 0x26;
        zones(all);
        out[9] = static_cast<unsigned char>(s.frequency < 1 ? 1 : s.frequency > 255 ? 255 : s.frequency);
        break;
    }
}

// What the stances read this frame.
struct StanceInput
{
    bool open = false;               // the player flies by hand, with this pad, in front: section 7's list
    Role role[2] = {Role::None, Role::None}; // left, right
    bool held[2] = {false, false};   // a finger past the press on that trigger
    FireState fire = FireState::Ready;
    float power = -1, gauge = -1;    // 0..1, -1 unknown
    float refire = 0;                // the fastest active gun's refire_delay (s), 0 none known
    bool burner = false, hasBurner = true; // hasBurner false too where it cannot light: in cruise, on a lane
    bool excluded = false;           // the game holds the controls: only the fire trigger's wall stands
    int effects = 2;                 // `trigger_effects`: 0 off, 1 light, 2 full
    int strength = 100;              // `trigger_strength`, percent
};

constexpr int kStartZone = 2;    // resistance starts past the press point (0.12 of the pull): a trigger never blocks its action
constexpr float kSettle = 2.5f;  // seconds a held firing state takes to settle: held fire is this game's normal state, and it must not tire the finger
constexpr int kFiringStart = 4, kFiringRest = 2; // of 8

// The scale of `trigger_effects` and `trigger_strength` on a strength of 1..8. A wall keeps its meaning at Light: at least 6 of 8 before the percent.
inline int Scaled(int strength, const StanceInput& in, bool limit)
{
    float v = static_cast<float>(strength);
    if (in.effects == 1)
        v = limit ? (v > 6 ? 6.0f : v) : v * 0.5f;
    v *= Clamp01(in.strength / 100.0f);
    const int n = static_cast<int>(v + 0.5f);
    return n < 1 ? 1 : n > 8 ? 8 : n;
}

// One trigger's stance, by section 7's order: wall, limp, firing, pull weight, off. `heldFor`: seconds this trigger has been in its firing or burning state.
inline Stance ChooseStance(const StanceInput& in, int side, float heldFor)
{
    Stance s;
    if (!in.open || in.effects <= 0 || in.strength <= 0)
        return s;
    const float settle = Clamp01(heldFor / kSettle);
    const int firing = static_cast<int>(Lerp(static_cast<float>(kFiringStart), static_cast<float>(kFiringRest), settle) + 0.5f);
    switch (in.role[side])
    {
    case Role::Fire:
    {
        if (in.fire == FireState::Locked) // the owner's "you can't fire now": a limit
        {
            s.kind = StanceKind::Feedback, s.start = kStartZone, s.strength = Scaled(8, in, true);
            return s;
        }
        if (in.fire == FireState::Empty || (in.power >= 0 && in.power < 0.02f)) // empty: nothing there at all
            return s;
        const bool heavy = in.refire >= kHeavyRefire;
        if (in.held[side])
        {
            s.kind = StanceKind::Vibrate, s.start = kStartZone, s.strength = Scaled(firing, in, false);
            // a fast gun's rate lies above what a finger counts, so it plays a light buzz at about twice the shot rate; a heavy gun knocks once a shot
            const float rate = in.refire > 0 ? 1.0f / in.refire : 8.0f;
            s.frequency = static_cast<int>((heavy ? rate : rate * 2) + 0.5f);
            return s;
        }
        const float power = in.power >= 0 ? Clamp01(in.power) : 1.0f;
        if (heavy)
        {
            s.kind = StanceKind::Weapon, s.start = kStartZone, s.end = 5, s.strength = Scaled(2 + static_cast<int>(power * 3 + 0.5f), in, false);
            return s;
        }
        s.kind = StanceKind::Feedback, s.start = kStartZone, s.strength = Scaled(1 + static_cast<int>(power * 3 + 0.5f), in, false);
        return s;
    }
    case Role::Afterburner:
    {
        if (!in.hasBurner || (in.gauge >= 0 && in.gauge < 0.02f))
            return s;
        const float gauge = in.gauge >= 0 ? Clamp01(in.gauge) : 1.0f;
        if (in.burner && in.held[side])
        {
            s.kind = StanceKind::Vibrate, s.start = kStartZone, s.frequency = 30;
            s.strength = Scaled(1 + static_cast<int>(gauge * (firing - 1) + 0.5f), in, false);
            return s;
        }
        s.kind = StanceKind::Feedback, s.start = kStartZone, s.strength = Scaled(2 + static_cast<int>(gauge + 0.5f), in, false);
        return s;
    }
    case Role::Throttle:
        if (in.excluded)
            return s;
        // the lever: a slope to the step before full pull, the step itself, then past it
        s.kind = StanceKind::Profile;
        for (int i = kStartZone; i < 10; ++i)
            s.profile[i] = Scaled(i < 8 ? 1 + (i - kStartZone) / 2 : i == 8 ? 7 : 3, in, i == 8);
        return s;
    default: return s;
    }
}

// ---------------------------------------------------------------- the sink
// Where a frame goes. Each call is a command that would reach the pad.
struct Sink
{
    virtual void Motors(float low, float high, float hold) = 0;
    virtual void TriggerMotors(float left, float right, float hold) = 0;
    virtual void Stance(const unsigned char left[kEffectBytes], const unsigned char right[kEffectBytes]) = 0;
    virtual void Wave(const wavesynth::WaveFrame& f) = 0;
};

// The recording renderer: every command, in order, for the tests and debug `haptics record`.
class RecordingSink : public Sink
{
  public:
    enum Kind
    {
        kMotors,
        kTriggerMotors,
        kStance,
        kWave
    };
    struct Entry
    {
        Kind kind;
        double at;
        float a, b, hold;
        unsigned char left[kEffectBytes], right[kEffectBytes];
    };
    static constexpr int kMax = 4096;

    void SetTime(double now) { now_ = now; }
    void Motors(float low, float high, float hold) override { Add({kMotors, now_, low, high, hold, {}, {}}); }
    void TriggerMotors(float left, float right, float hold) override { Add({kTriggerMotors, now_, left, right, hold, {}, {}}); }
    void Stance(const unsigned char left[kEffectBytes], const unsigned char right[kEffectBytes]) override
    {
        Entry e = {kStance, now_, 0, 0, 0, {}, {}};
        for (int i = 0; i < kEffectBytes; ++i)
            e.left[i] = left[i], e.right[i] = right[i];
        Add(e);
    }
    void Wave(const wavesynth::WaveFrame& f) override
    {
        ++waves_, last_ = f;
        for (int side = 0; side < 2; ++side)
        {
            if (f.mass[side] > peak_)
                peak_ = f.mass[side];
            if (f.tex[side] > peak_)
                peak_ = f.tex[side];
        }
    }
    int Count() const { return n_; }
    int Count(Kind k) const
    {
        int c = k == kWave ? waves_ : 0;
        for (int i = 0; i < n_; ++i)
            c += log_[i].kind == k;
        return c;
    }
    const Entry& At(int i) const { return log_[i]; }
    // The last command of a kind, null none.
    const Entry* Last(Kind k) const
    {
        for (int i = n_ - 1; i >= 0; --i)
            if (log_[i].kind == k)
                return &log_[i];
        return nullptr;
    }
    const wavesynth::WaveFrame& LastWave() const { return last_; }
    float WavePeak() const { return peak_; }
    void Clear() { n_ = waves_ = 0, peak_ = 0; }

  private:
    void Add(const Entry& e)
    {
        if (n_ < kMax)
            log_[n_++] = e;
    }
    Entry log_[kMax];
    int n_ = 0, waves_ = 0;
    float peak_ = 0;
    double now_ = 0;
    wavesynth::WaveFrame last_ = {};
};

// ---------------------------------------------------------------- the renderer
constexpr float kMotorFloor = 0.08f;   // a motor does not spin below about this: a bed under it stays off, so a swell is what reaches the hand
constexpr float kBurnerHum = 0.12f;    // an Xbox pad: the afterburner's trigger motor, steady and low, thinning with the gauge
constexpr float kStanceGap = 0.05f;    // seconds between two stance sends
constexpr float kStanceHold = 0.25f;   // seconds a wall stands at least: rapid cycling has broken trigger springs

// The grip levels of a DualSense's two motors: both the mass and the texture of that side, at the stronger of them.
inline float GripLevel(float mass, float tex) { return mass > tex ? mass : tex; }

class Render
{
  public:
    // One frame through this pad's caps. Motors and trigger motors keep the send policy of Renderer; a stance goes on change; `Off` at every gate.
    void Send(const HapticFrame& f, const StanceInput& in, const Caps& caps, double now, float dt, Sink& sink)
    {
        // the motors
        Out want;
        if (!caps.waveform && caps.motors)
        {
            const float body = f.bed.mass > f.bed.bed ? f.bed.mass : f.bed.bed;
            const float bedMass = body >= kMotorFloor ? body : 0;
            const float bedTex = f.bed.tex >= kMotorFloor ? f.bed.tex : 0;
            if (caps.motorsAreLR)
            {
                want.low = GripLevel(Max(f.mix.mass[0], bedMass), Max(f.mix.tex[0], bedTex));
                want.high = GripLevel(Max(f.mix.mass[1], bedMass), Max(f.mix.tex[1], bedTex));
            }
            else
                want.low = Max(f.mix.low, bedMass), want.high = Max(f.mix.high, bedTex);
        }
        Out send;
        if (motors_.Due(want, now, send))
            sink.Motors(send.low, send.high, send.low > 0 || send.high > 0 ? Renderer::kHold : 0.0f);
        // the trigger motors: Impulse. The events, then the stance states that can become pulses
        if (caps.triggerMotors)
        {
            Out t;
            t.low = f.mix.trig[0], t.high = f.mix.trig[1];
            const float scale = in.effects <= 0 ? 0.0f : (in.effects == 1 ? 0.5f : 1.0f) * Clamp01(in.strength / 100.0f);
            t.low *= scale, t.high *= scale;
            for (int side = 0; in.open && side < 2; ++side)
                if (in.role[side] == Role::Afterburner && in.burner && in.held[side])
                {
                    const float hum = kBurnerHum * (in.gauge >= 0 ? 0.3f + 0.7f * Clamp01(in.gauge) : 1.0f) * scale;
                    float& cell = side == 0 ? t.low : t.high;
                    cell = Max(cell, hum);
                }
            if (trigMotors_.Due(t, now, send))
                sink.TriggerMotors(send.low, send.high, send.low > 0 || send.high > 0 ? Renderer::kHold : 0.0f);
        }
        else if (trigMotors_.On())
        {
            sink.TriggerMotors(0, 0, 0);
            trigMotors_.Forget();
        }
        // the stances
        if (caps.adaptive)
            Stances(in, now, dt, sink);
        else if (stanceOn_)
            StancesOff(sink);
        // the grips' stream
        if (caps.waveform)
        {
            wavesynth::WaveFrame w = {};
            for (int side = 0; side < 2; ++side)
            {
                w.mass[side] = Max(f.mix.mass[side], f.bed.mass);
                w.tex[side] = Max(f.mix.tex[side], f.bed.tex);
                w.bed[side] = f.bed.bed;
                w.clicks[side] = f.mix.clicks[side];
            }
            const bool events = f.mix.peak > 0;
            w.bedHz = f.bed.bedHz;
            w.massHz = events ? f.mix.massHz : f.bed.massHz;
            w.texHz = events ? f.mix.texHz : f.bed.texHz;
            w.noise = events ? f.mix.noise : f.bed.noise;
            sink.Wave(w);
            waveOn_ = true;
        }
        else if (waveOn_)
        {
            sink.Wave(wavesynth::WaveFrame{});
            waveOn_ = false;
        }
    }

    // Everything to rest now: the logic's stop, a fault, a pad that left.
    void Stop(Sink& sink)
    {
        motors_.Forget();
        sink.Motors(0, 0, 0);
        if (trigMotors_.On())
            sink.TriggerMotors(0, 0, 0);
        trigMotors_.Forget();
        StancesOff(sink);
        if (waveOn_)
            sink.Wave(wavesynth::WaveFrame{});
        waveOn_ = false;
        held_[0] = held_[1] = 0;
    }
    // Another pad, or the same one reopened: the next frame sends what it should hold.
    void PadChanged()
    {
        motors_.Forget(), trigMotors_.Forget();
        const Stance off;
        for (int side = 0; side < 2; ++side)
            StanceBytes(off, sent_[side]), wallSent_[side] = false;
        stanceOn_ = false;
    }

    const Renderer& MotorState() const { return motors_; }
    const unsigned char* StanceSent(int side) const { return sent_[side]; }
    bool StanceOn() const { return stanceOn_; }
    int StanceSends() const { return sends_; }

  private:
    static float Max(float a, float b) { return a > b ? a : b; }

    void Stances(const StanceInput& in, double now, float dt, Sink& sink)
    {
        unsigned char want[2][kEffectBytes];
        bool same = true, wall = false, wallNow = false;
        for (int side = 0; side < 2; ++side)
        {
            held_[side] = in.open && in.held[side] ? held_[side] + dt : 0;
            StanceBytes(ChooseStance(in, side, held_[side]), want[side]);
            for (int i = 0; i < kEffectBytes; ++i)
                same = same && want[side][i] == sent_[side][i];
            wall = wall || (want[side][0] == 0x21 && in.role[side] == Role::Fire && in.fire == FireState::Locked);
            wallNow = wallNow || wallSent_[side];
        }
        const bool off = want[0][0] == 0x05 && want[1][0] == 0x05;
        if (same)
            return;
        if (in.open)
        {
            if (now - lastSend_ < kStanceGap)
                return;
            if (wall != wallNow && now - wallAt_ < kStanceHold)
                return;
        }
        for (int side = 0; side < 2; ++side)
        {
            for (int i = 0; i < kEffectBytes; ++i)
                sent_[side][i] = want[side][i];
            wallSent_[side] = want[side][0] == 0x21 && in.role[side] == Role::Fire && in.fire == FireState::Locked;
        }
        if (wall != wallNow)
            wallAt_ = now;
        lastSend_ = now;
        stanceOn_ = !off;
        ++sends_;
        sink.Stance(sent_[0], sent_[1]);
    }
    void StancesOff(Sink& sink)
    {
        if (!stanceOn_)
            return;
        const Stance off;
        for (int side = 0; side < 2; ++side)
            StanceBytes(off, sent_[side]), wallSent_[side] = false;
        stanceOn_ = false;
        ++sends_;
        sink.Stance(sent_[0], sent_[1]);
    }

    Renderer motors_, trigMotors_;
    unsigned char sent_[2][kEffectBytes] = {{0x05}, {0x05}};
    bool wallSent_[2] = {false, false};
    bool stanceOn_ = false, waveOn_ = false;
    double lastSend_ = -1e9, wallAt_ = -1e9;
    float held_[2] = {0, 0};
    int sends_ = 0;
};
} // namespace haptics
