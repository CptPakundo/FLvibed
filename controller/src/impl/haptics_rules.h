// FLvibed - the haptics skeleton's pure pieces (IDEAS U30, WORK_QUEUE Q5 slice 3, 30 Oct 2026). Header only, no system
// headers (the module sources include it inside their namespace); unit test tests\haptics_test.cpp. The game glue (the
// event sources, the gates' inputs, the pad) is haptics.cpp.
//
// game event -> the effect table (kEffects: one row per named effect - its category, the low / high motor levels at
// full strength, an attack / hold / release envelope, repeats, a priority, a rate limit and its other cue) -> the mixer
// (up to kVoices effects at once: the maximum per motor, effects below the highest priority playing are ducked, a
// trigger inside its effect's rate limit merges into the next start, a fatigue limiter drops the low-priority effects
// while the motors have been busy, the master `rumble` percent last) -> the renderer (the two motors' values sent to the
// pad only when they change, at most every kMinInterval, a running value refreshed every kRefresh so a backend's own
// duration never ends it early).
//
// Silence (Silence()): nothing while `rumble` is 0, the controller module is off, the pad is not the game's (the one
// focus gate, focus_gate.h) or the keyboard and mouse are the device in use (a pad lying on the desk never buzzes); the
// world's effects (hits, shield failure) and the autopilot's hand-back only in flight, never in a cutscene, docked, in
// the menus or while the game is paused; a prompt (a box that waits for the player's answer) in flight or docked, also
// while paused - the game's own boxes pause the game themselves (docs\missions\M01a.md: "Popup pauses the game until
// closed"). When a gate closes, the effects of the categories it silences stop at once and their merged triggers are
// dropped: nothing silenced is replayed later.
//
// Never the only cue (XAG 110; the HUD cue rules, controller\docs\HUD_CUES.md): every row names the cue a player sees or
// hears for the same event.
#pragma once

namespace haptics
{
enum Effect : int
{
    kHitShield,  // the shield took damage (a hit's weight = the damage's share of the shield's capacity)
    kHitHull,    // the hull took damage (its share of the hull's points)
    kShieldFail, // the shield went down
    kHandBack,   // the Advanced Autopilot gave the ship back (today's pulse, unchanged)
    kConfirm,    // a box waits for the player's answer (the game's own popups; the autopilot's confirmations when built)
    kEffectCount
};

enum Category : int
{
    kWorld,  // the world hits the player: flight only, not paused
    kAssist, // the autopilot's state: flight only, not paused
    kPrompt, // the game asks: flight or docked, also paused (its boxes pause the game)
};

enum class Where : int
{
    Flight,   // in space (also with a NeuroNet window open: the world goes on)
    Cutscene, // the game's cinematics, docking / launch cameras
    Docked,   // on a base
    Menu,     // the main menu, loading, no player ship
};

struct EffectDef
{
    const char* name;
    Category cat;
    float low, high;             // the motors' levels at full strength (0..1): low = mass, high = texture
    float attack, hold, release; // seconds
    int pulses;                  // repeats of the envelope (1 = once)
    float period;                // seconds from one pulse's start to the next (pulses > 1)
    int prio;                    // 1 low .. 7 high
    float minGap;                // seconds between two starts of this effect: a trigger inside merges into the next start
    const char* cue;             // the other cue of the same event (never the only cue)
};

// The table. The hand-back is today's pulse exactly (controller.cpp's RumblePulse(0.6, 0.18) before 30 Oct 2026: low
// 0.6, high 0.6 x 0.6, 0.18 s flat). The rest is a first authoring (controller-input-haptics.md section 4: the shield
// high and sharp, the hull low and heavy, the failure both, the prompt two light ticks), to be felt on the owner's pad.
inline constexpr EffectDef kEffects[kEffectCount] = {
    {"hit_shield", kWorld, 0.20f, 0.65f, 0.0f, 0.04f, 0.10f, 1, 0.0f, 3, 0.20f,
     "the shield cue's edge (shield_cue) + the game's shield bar and impact sound"},
    {"hit_hull", kWorld, 0.85f, 0.30f, 0.0f, 0.06f, 0.16f, 1, 0.0f, 4, 0.20f,
     "the hull cue's edge (hull_cue) + the game's hull bar, damage arcs and impact sound"},
    {"shield_fail", kWorld, 1.00f, 0.80f, 0.0f, 0.10f, 0.25f, 1, 0.0f, 6, 1.00f,
     "the shield cue's failure flash + the game's shield collapse sound and empty shield bar"},
    {"handback", kAssist, 0.60f, 0.36f, 0.0f, 0.18f, 0.0f, 1, 0.0f, 6, 0.30f,
     "the reticle cue's amber ring (reticle_cue) + the status line and the bar button going off"},
    {"confirm", kPrompt, 0.15f, 0.50f, 0.0f, 0.05f, 0.03f, 2, 0.15f, 5, 1.50f,
     "the game's box itself (it pauses the game) + its prompt bar (A / B)"},
};

// The effect by name (-1 none).
inline int EffectByName(const char* name)
{
    for (int i = 0; i < kEffectCount; ++i)
    {
        const char *a = kEffects[i].name, *b = name;
        while (*a && *a == *b)
            ++a, ++b;
        if (!*a && !*b)
            return i;
    }
    return -1;
}

inline float Clamp01(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }

// An effect's whole length in seconds.
inline float Duration(const EffectDef& d)
{
    const float one = d.attack + d.hold + d.release;
    const int n = d.pulses > 1 ? d.pulses : 1;
    const float period = n > 1 && d.period > one ? d.period : one;
    return (n - 1) * period + one;
}

// The effect's shape t seconds after its start: 0..1 while it runs (0 between pulses), -1 once it is over.
inline float Envelope(const EffectDef& d, float t)
{
    if (t < 0)
        return 0;
    if (t >= Duration(d))
        return -1;
    const float one = d.attack + d.hold + d.release;
    const int n = d.pulses > 1 ? d.pulses : 1;
    const float period = n > 1 && d.period > one ? d.period : one;
    int k = period > 0 ? static_cast<int>(t / period) : 0;
    if (k > n - 1)
        k = n - 1;
    const float u = t - k * period;
    if (u < d.attack) // (u < attack only when attack > 0; likewise release below)
        return d.attack > 0 ? u / d.attack : 1;
    if (u < d.attack + d.hold)
        return 1;
    if (u < one)
        return d.release > 0 ? 1 - (u - d.attack - d.hold) / d.release : 0;
    return 0;
}

// A hit's strength from its weight, the damage's share of the shield's capacity or the hull's points: a moderate floor
// for the lightest hit, the maximum kept for a quarter of the capacity at once (the Returnal rule: the top of the range
// for the heaviest blows). 0 = no hit.
inline float HitStrength(float share)
{
    if (!(share > 0))
        return 0;
    const float s = 0.25f + 3.0f * share;
    return s > 1 ? 1 : s;
}

// ---------------------------------------------------------------- silence
struct Gates
{
    int rumble = 60;     // the `rumble` setting, percent (0 = off)
    bool enabled = true; // the controller module is on
    bool focus = true;   // the one focus gate is open: the pad is the game's (focus_gate.h, Gamepad::ignoreReal false)
    bool pad = true;     // the pad is the device in use (device.h)
    bool paused = false; // the game is paused
    Where where = Where::Flight;
};

// Why the category is silent now ("" = it may play).
inline const char* Silence(const Gates& g, Category c)
{
    if (g.rumble <= 0)
        return "rumble 0";
    if (!g.enabled)
        return "controller off";
    if (!g.focus)
        return "focus";
    if (!g.pad)
        return "device kbm";
    if (g.where == Where::Menu)
        return "menu";
    if (g.where == Where::Cutscene)
        return "cutscene";
    if (c == kPrompt)
        return "";
    if (g.where != Where::Flight)
        return "docked";
    return g.paused ? "paused" : "";
}

struct Out
{
    float low = 0, high = 0;
};

// ---------------------------------------------------------------- the mixer
class Mixer
{
  public:
    static constexpr int kVoices = 4;           // effects at once
    static constexpr float kDuck = 0.4f;        // an effect below the highest priority playing
    static constexpr float kFatigueTau = 3.0f;  // seconds: the motors' busy share is averaged over about this long
    static constexpr float kFatigueOn = 0.5f;   // busy above this share: the low-priority triggers are dropped
    static constexpr int kFatigueSpare = 5;     // priorities from here on are never dropped for fatigue
    static constexpr float kBusy = 0.02f;       // a motor above this counts as busy

    enum Result
    {
        kStarted,
        kMerged,   // inside its rate limit: it raises the next start's level
        kSilent,   // its category is silent now (dropped, never replayed)
        kFatigued, // dropped by the fatigue limiter
        kFull,     // every voice plays something of a higher priority
        kIgnored,  // no level or no such effect
    };

    Result Trigger(int effect, float level, double now, const Gates& g)
    {
        if (effect < 0 || effect >= kEffectCount || !(level > 0))
            return Count(kIgnored);
        const EffectDef& d = kEffects[effect];
        level = Clamp01(level);
        if (*Silence(g, d.cat))
            return Count(kSilent);
        if (d.prio < kFatigueSpare && duty_ > kFatigueOn)
            return Count(kFatigued);
        if (now - lastStart_[effect] < d.minGap)
        {
            if (level > pending_[effect])
                pending_[effect] = level;
            return Count(kMerged);
        }
        return Count(Start(effect, level, now));
    }

    // Once per frame: the motors' mix now (after the master `rumble`), silenced categories stopped, merged triggers
    // started when their gap is over.
    Out Update(double now, const Gates& g)
    {
        const float dt = lastUpdate_ > -1e8 && now > lastUpdate_ ? static_cast<float>(now - lastUpdate_) : 0.0f;
        lastUpdate_ = now;
        for (int e = 0; e < kEffectCount; ++e)
        {
            if (!(pending_[e] > 0))
                continue;
            if (*Silence(g, kEffects[e].cat))
                pending_[e] = 0;
            else if (now - lastStart_[e] >= kEffects[e].minGap)
            {
                const float lv = pending_[e];
                pending_[e] = 0;
                Count(Start(e, lv, now));
            }
        }
        int top = 0;
        for (Voice& v : v_)
        {
            if (v.effect < 0)
                continue;
            const EffectDef& d = kEffects[v.effect];
            if (*Silence(g, d.cat) || Envelope(d, static_cast<float>(now - v.start)) < 0)
            {
                v.effect = -1;
                continue;
            }
            if (d.prio > top)
                top = d.prio;
        }
        Out o;
        for (const Voice& v : v_)
        {
            if (v.effect < 0)
                continue;
            const EffectDef& d = kEffects[v.effect];
            const float k = Envelope(d, static_cast<float>(now - v.start)) * v.level * (d.prio < top ? kDuck : 1.0f);
            if (d.low * k > o.low)
                o.low = d.low * k;
            if (d.high * k > o.high)
                o.high = d.high * k;
        }
        const float busy = o.low > kBusy || o.high > kBusy ? 1.0f : 0.0f;
        const float a = dt / kFatigueTau;
        duty_ += (busy - duty_) * (a > 1 ? 1 : a);
        const float master = Clamp01(g.rumble / 100.0f);
        o.low = Clamp01(o.low * master), o.high = Clamp01(o.high * master);
        last_ = o;
        return o;
    }

    // Everything off now (the logic's shutdown, a reset).
    void Stop()
    {
        for (Voice& v : v_)
            v.effect = -1;
        for (int e = 0; e < kEffectCount; ++e)
            pending_[e] = 0;
        last_ = Out();
    }

    int Playing() const
    {
        int n = 0;
        for (const Voice& v : v_)
            n += v.effect >= 0;
        return n;
    }
    bool Plays(int effect) const
    {
        for (const Voice& v : v_)
            if (v.effect == effect)
                return true;
        return false;
    }
    float Pending(int effect) const { return effect >= 0 && effect < kEffectCount ? pending_[effect] : 0; }
    float Duty() const { return duty_; }
    Out Last() const { return last_; }
    int Counted(Result r) const { return counts_[r]; }

  private:
    struct Voice
    {
        int effect = -1;
        float level = 0;
        double start = 0;
    };

    Result Start(int effect, float level, double now)
    {
        const EffectDef& d = kEffects[effect];
        Voice* slot = nullptr;
        for (Voice& v : v_) // the same effect again: it restarts
            if (v.effect == effect)
                slot = &v;
        for (Voice& v : v_)
            if (!slot && v.effect < 0)
                slot = &v;
        if (!slot) // full: the lowest priority (the oldest of equals) gives way to one at least as important
        {
            Voice* low = nullptr;
            for (Voice& v : v_)
                if (!low || kEffects[v.effect].prio < kEffects[low->effect].prio ||
                    (kEffects[v.effect].prio == kEffects[low->effect].prio && v.start < low->start))
                    low = &v;
            if (kEffects[low->effect].prio > d.prio)
                return kFull;
            slot = low;
        }
        slot->effect = effect, slot->level = level, slot->start = now;
        lastStart_[effect] = now;
        pending_[effect] = 0;
        return kStarted;
    }
    Result Count(Result r)
    {
        ++counts_[r];
        return r;
    }

    Voice v_[kVoices];
    double lastStart_[kEffectCount] = {-1e9, -1e9, -1e9, -1e9, -1e9};
    float pending_[kEffectCount] = {};
    double lastUpdate_ = -1e9;
    float duty_ = 0;
    Out last_;
    int counts_[kIgnored + 1] = {};
};
static_assert(kEffectCount == 5, "Mixer::lastStart_'s initialiser lists every effect");

// ---------------------------------------------------------------- the renderer
// The two motors' values go to the pad only when they change (by kStep, compared, not rounded: the value sent is the
// mix's own), at most every kMinInterval, except a start from rest and a stop (at once); an unchanged running value is
// refreshed every kRefresh (the backends end a command after its duration: SDL's, the pad's own timer).
class Renderer
{
  public:
    static constexpr float kStep = 1.0f / 128;
    static constexpr float kMinInterval = 0.03f;
    static constexpr float kRefresh = 0.25f;
    static constexpr float kHold = 0.5f; // the duration a command asks for (> kRefresh: the next refresh comes first)

    // True = send `send` now.
    bool Due(Out want, double now, Out& send)
    {
        const bool zero = Q(want.low) == 0 && Q(want.high) == 0;
        const bool changed = Q(want.low) != Q(last_.low) || Q(want.high) != Q(last_.high);
        bool go = false;
        if (changed)
            go = zero || !on_ || now - lastSend_ >= kMinInterval;
        else
            go = on_ && !zero && now - lastSend_ >= kRefresh;
        if (!go)
            return false;
        send = zero ? Out() : want;
        last_ = send;
        on_ = !zero;
        lastSend_ = now;
        ++sent_;
        return true;
    }
    // The pad was stopped elsewhere (the focus gate, a device change): the next value is sent as from rest.
    void Forget()
    {
        last_ = Out();
        on_ = false;
    }
    bool On() const { return on_; }
    int Sent() const { return sent_; }
    Out LastSent() const { return last_; }

  private:
    static int Q(float v) { return static_cast<int>(Clamp01(v) / kStep + 0.5f); }
    Out last_;
    bool on_ = false;
    double lastSend_ = -1e9;
    int sent_ = 0;
};

// ---------------------------------------------------------------- the hits
// The player ship's shield and hull as the game has them this frame (points; valid = a player ship in space).
struct Vitals
{
    bool valid = false;
    unsigned ship = 0;
    float shield = 0, shieldMax = 0;
    bool shieldUp = false;
    float hull = 0, hullMax = 0;
};

struct Hits
{
    float shieldShare = 0, hullShare = 0; // the damage this frame as a share of the capacity / the hull's points
    bool shieldFailed = false;            // the shield went down this frame
};

// The hits from the frame-to-frame change (shieldcue.cpp reads them the same way: a fall of more than half a point,
// the shield's up -> down). The first frame of a ship (a launch, a load, a respawn) is its baseline, never a hit.
class HitDetector
{
  public:
    Hits Step(const Vitals& v)
    {
        Hits h;
        if (!v.valid || v.ship != ship_)
            known_ = false;
        if (!v.valid)
            return h;
        if (known_)
        {
            if (up_ && !v.shieldUp && v.shieldMax > 0)
                h.shieldFailed = true;
            else if (v.shieldUp && v.shield < shield_ - 0.5f)
                h.shieldShare = (shield_ - v.shield) / (v.shieldMax > 1 ? v.shieldMax : 1);
            if (v.hull < hull_ - 0.5f)
                h.hullShare = (hull_ - v.hull) / (v.hullMax > 1 ? v.hullMax : 1);
        }
        known_ = true;
        ship_ = v.ship;
        shield_ = v.shield, hull_ = v.hull, up_ = v.shieldUp;
        return h;
    }

  private:
    bool known_ = false, up_ = false;
    unsigned ship_ = 0;
    float shield_ = 0, hull_ = 0;
};
} // namespace haptics
