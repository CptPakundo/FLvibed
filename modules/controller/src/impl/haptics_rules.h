// FLvibed - the haptics engine's pure pieces (modules\controller\docs\HAPTICS.md). Header only, no system headers (the module sources include it
// inside their namespace); unit tests tests\haptics_test.cpp and tests\haptics_engine_test.cpp. The game glue is haptics.cpp, the pads are haptics_render.h.
//
// game state -> Signals (one snapshot a frame) -> Engine: edges into events, the effect table (kEffects: a row per named effect - its category, the mass and
// texture levels at full strength, an attack / hold / release envelope, repeats, a priority, a rate limit and its other cue), the mixer (voices, the maximum
// per cell, sided by pan; effects below the highest priority playing are ducked, a trigger inside its effect's rate limit merges into the next start, a fatigue
// limiter drops the low-priority effects while the cells have been busy) and the bed (the ship's body, continuous, low, ducked by every event) -> Frame.
//
// Silence (Silence()): nothing while `rumble` is 0, the controller module is off, the pad is not the game's (the one focus gate, focus_gate.h) or the keyboard
// and mouse are the device in use (a pad lying on the desk never buzzes); the world's effects and the autopilot's hand-back only in flight, never in a cutscene,
// docked, in the menus or while the game is paused; a set piece (a lane, a jump, a dock, a launch) in flight and through its own sequence, never in another
// cinematic; a prompt (a box that waits for the player's answer) in flight or docked, also while paused - the game's own boxes pause the game themselves
// (docs\missions\M01a.md: "Popup pauses the game until closed"). When a gate closes, the effects of the categories it silences stop at once and their merged
// triggers are dropped: nothing silenced is replayed later.
//
// Never the only cue (XAG 110; the HUD cue rules, modules\hud\docs\HUD_CUES.md): every row names the cue a player sees or hears for the same event.
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
    kShieldUp,   // the failed shield came back
    kHullBeat,   // the hull is critical: a beat on the failing bars' clock
    kCollision,  // the ship struck something (the game's contact hit)
    kExplosion,  // a blast reached the ship
    kMissileNear, // a missile homes on the player: pulses by time to impact
    kRadClick,   // a radiation zone: Geiger clicks
    kCruiseDisrupt,
    kGunFast,
    kGunHeavy,
    kGunDry,     // held fire with no power
    kFireLocked, // a press that cannot fire
    kMissileLaunch,
    kTorpedoLaunch,
    kMineDrop,
    kCmDrop,
    kEngineKill,
    kCruiseRelease,
    kTractorTick,
    kGrit, // an asteroid field
    kDockClamp,
    kLaunchRelease,
    kLaneRing,
    kLaneExit,
    kJumpFlash,
    kHoleFlash,
    kEffectCount
};

enum Category : int
{
    kWorld,    // the world and the ship: flight only, not paused
    kAssist,   // the autopilot's state: flight only, not paused
    kPrompt,   // the game asks: flight or docked, also paused (its boxes pause the game)
    kSetPiece, // a lane, a jump, a dock, a launch: flight and their own sequences
};

// The rows of the options page: what a player can turn down by itself.
enum Class : int
{
    kClassShip,
    kClassWeapons,
    kClassDanger,
    kClassSetPieces,
    kClassPrompts,
    kClassCount
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
    float low, high;             // mass and texture at full strength (0..1): on two motors exactly the low and the high motor
    float attack, hold, release; // seconds
    int pulses;                  // repeats of the envelope (1 = once)
    float period;                // seconds from one pulse's start to the next (pulses > 1)
    int prio;                    // 1 low .. 7 high
    float minGap;                // seconds between two starts of this effect: a trigger inside merges into the next start
    const char* cue;             // the other cue of the same event (never the only cue)
    int cls = kClassDanger;      // its row on the options page
    float trig = 0;              // an Xbox pad's trigger motor at full strength
    float massHz = 80, texHz = 220; // the waveform's two bands
    float noise = 0;             // the waveform: the texture band's share of noise (0 a pure tone, 1 all noise)
    bool soften = false;         // under fatigue it plays softer rather than stops, and does not count as busy: a stopped tick under a held trigger reads as a fault
    bool click = false;          // too short for a frame: the waveform draws it as a burst
    bool waveOnly = false;       // grain a motor cannot carry: off on motors, kept on trigger motors
};

// The table. The hand-back is today's pulse exactly (0.6, 0.6 x 0.6, 0.18 s flat), so the first five rows are the 100 % baseline as it shipped. The rest is a
// first authoring, modules\controller\docs\HAPTICS.md 4, to be felt on the owner's pads.
inline constexpr EffectDef kEffects[kEffectCount] = {
    {"hit_shield", kWorld, 0.20f, 0.65f, 0.0f, 0.04f, 0.10f, 1, 0.0f, 3, 0.20f,
     "the shield cue's edge (shield_cue) + the game's shield bar and impact sound", kClassDanger, 0.30f, 90, 240, 0.35f},
    {"hit_hull", kWorld, 0.85f, 0.30f, 0.0f, 0.06f, 0.16f, 1, 0.0f, 4, 0.20f,
     "the hull cue's edge (hull_cue) + the game's hull bar, damage arcs and impact sound", kClassDanger, 0.40f, 60, 160, 0.10f},
    {"shield_fail", kWorld, 1.00f, 0.80f, 0.0f, 0.10f, 0.25f, 1, 0.0f, 6, 1.00f,
     "the shield cue's failure flash + the game's shield collapse sound and empty shield bar", kClassDanger, 0.80f, 55, 280, 0.50f},
    {"handback", kAssist, 0.60f, 0.36f, 0.0f, 0.18f, 0.0f, 1, 0.0f, 6, 0.30f,
     "the reticle cue's amber ring (reticle_cue) + the status line and the bar button going off", kClassPrompts, 0.0f, 90, 200, 0},
    {"confirm", kPrompt, 0.15f, 0.50f, 0.0f, 0.05f, 0.03f, 2, 0.15f, 5, 1.50f,
     "the game's box itself (it pauses the game) + its prompt bar (A / B)", kClassPrompts, 0.20f, 120, 300, 0},
    {"shield_up", kWorld, 0.15f, 0.35f, 0.04f, 0.03f, 0.06f, 2, 0.16f, 3, 1.00f,
     "the shield bar refilling + the game's shield rebuilt sound", kClassDanger, 0.0f, 100, 260, 0},
    {"hull_beat", kWorld, 0.35f, 0.05f, 0.02f, 0.05f, 0.10f, 1, 0.0f, 5, 0.30f,
     "the hull bar's breath, on the same clock + the game's damage arcs", kClassDanger, 0.0f, 55, 160, 0},
    {"collision", kWorld, 0.90f, 0.45f, 0.0f, 0.10f, 0.14f, 1, 0.0f, 5, 0.25f,
     "the game's collision sound, the HUD's hit arrow and the hull bar", kClassDanger, 0.50f, 70, 180, 0.60f},
    {"explosion", kWorld, 0.70f, 0.20f, 0.0f, 0.05f, 0.45f, 1, 0.0f, 4, 0.30f,
     "the blast itself + the game's explosion sound", kClassDanger, 0.25f, 52, 150, 0.20f},
    {"missile_near", kWorld, 0.10f, 0.60f, 0.0f, 0.03f, 0.02f, 1, 0.0f, 7, 0.10f,
     "the MISSILE INCOMING card, the ring's chevrons and the game's alert", kClassDanger, 0.50f, 110, 280, 0},
    {"rad_click", kWorld, 0.0f, 0.40f, 0.0f, 0.012f, 0.004f, 1, 0.0f, 4, 0.03f,
     "the nav's amber radiation line + the game's alert and hull bar", kClassDanger, 0.15f, 90, 290, 0.50f, false, true},
    {"cruise_disrupt", kWorld, 1.00f, 0.70f, 0.0f, 0.08f, 0.40f, 1, 0.0f, 6, 1.00f,
     "the CRUISE DISRUPTED card + the game's cruise_disrupt_sound and the speed falling", kClassDanger, 0.80f, 60, 250, 0.50f},
    {"gun_fast", kWorld, 0.05f, 0.22f, 0.0f, 0.02f, 0.02f, 1, 0.0f, 3, 0.05f,
     "the shot on screen + the gun's own sound", kClassWeapons, 0.35f, 110, 260, 0.10f, true},
    {"gun_heavy", kWorld, 0.30f, 0.20f, 0.0f, 0.04f, 0.06f, 1, 0.0f, 3, 0.12f,
     "the shot on screen + the gun's own sound", kClassWeapons, 0.60f, 70, 180, 0.20f, true},
    {"gun_dry", kWorld, 0.0f, 0.14f, 0.0f, 0.015f, 0.015f, 1, 0.0f, 4, 0.25f,
     "the empty power bar + the gun's dry fire sound", kClassWeapons, 0.0f, 90, 300, 0.30f},
    {"fire_locked", kWorld, 0.22f, 0.0f, 0.0f, 0.03f, 0.04f, 1, 0.0f, 5, 0.25f,
     "the weapon group cue going amber with a slash + nothing firing", kClassWeapons, 0.10f, 50, 150, 0},
    {"missile_launch", kWorld, 0.45f, 0.25f, 0.0f, 0.05f, 0.18f, 1, 0.0f, 4, 0.25f,
     "the launch on screen, its sound and the ammo count", kClassWeapons, 0.50f, 65, 200, 0.60f},
    {"torpedo_launch", kWorld, 1.00f, 0.40f, 0.0f, 0.10f, 0.60f, 1, 0.0f, 5, 0.50f,
     "the launch on screen, its sound and the ammo count", kClassWeapons, 0.90f, 50, 140, 0.40f},
    {"mine_drop", kWorld, 0.30f, 0.0f, 0.01f, 0.04f, 0.08f, 1, 0.0f, 3, 0.25f,
     "the mine on screen, its sound and the ammo count", kClassWeapons, 0.20f, 60, 160, 0},
    {"cm_drop", kWorld, 0.05f, 0.30f, 0.0f, 0.02f, 0.02f, 2, 0.09f, 3, 0.25f,
     "the flare on screen, its sound and the ammo count", kClassWeapons, 0.20f, 100, 240, 0},
    {"engine_kill", kWorld, 0.30f, 0.0f, 0.0f, 0.05f, 0.12f, 1, 0.0f, 3, 0.50f,
     "the maneuver bar and the game's engine_kill_sound", kClassShip, 0.0f, 48, 150, 0},
    {"cruise_release", kWorld, 0.70f, 0.20f, 0.0f, 0.06f, 0.30f, 1, 0.0f, 4, 1.00f,
     "the speed rising, the game's cruise sound and the engine trails", kClassShip, 0.60f, 50, 160, 0.10f},
    {"tractor_tick", kWorld, 0.05f, 0.20f, 0.0f, 0.02f, 0.03f, 1, 0.0f, 2, 0.25f,
     "the beam ending, tractor_complete_snd and the item in the hold", kClassShip, 0.10f, 110, 260, 0},
    {"grit", kWorld, 0.0f, 0.12f, 0.0f, 0.01f, 0.01f, 1, 0.0f, 1, 0.20f,
     "the asteroids on screen and the zone's name in the nav", kClassShip, 0.08f, 90, 280, 0.80f, false, true, true},
    {"dock_clamp", kSetPiece, 0.45f, 0.10f, 0.0f, 0.05f, 0.07f, 2, 0.30f, 3, 0.80f,
     "the docking scene and its sound", kClassSetPieces, 0.30f, 58, 170, 0},
    {"launch_release", kSetPiece, 0.55f, 0.10f, 0.0f, 0.06f, 0.20f, 1, 0.0f, 3, 1.00f,
     "the launch scene and its sound", kClassSetPieces, 0.30f, 56, 170, 0},
    {"lane_ring", kSetPiece, 0.30f, 0.15f, 0.01f, 0.03f, 0.06f, 1, 0.0f, 3, 0.20f,
     "the ring passing, its sound and the lane's ring count", kClassSetPieces, 0.30f, 75, 200, 0},
    {"lane_exit", kSetPiece, 0.60f, 0.25f, 0.0f, 0.06f, 0.20f, 1, 0.0f, 3, 1.00f,
     "the ship dropping out of the lane and its sound", kClassSetPieces, 0.50f, 60, 170, 0.30f},
    {"jump_flash", kSetPiece, 1.00f, 0.85f, 0.0f, 0.20f, 0.60f, 1, 0.0f, 6, 2.00f,
     "the jump's flash and sound", kClassSetPieces, 1.00f, 55, 240, 0.40f},
    {"hole_flash", kSetPiece, 1.00f, 0.70f, 0.0f, 0.08f, 0.12f, 3, 0.22f, 6, 2.00f,
     "the jump's flash and sound", kClassSetPieces, 1.00f, 50, 200, 0.80f},
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
inline float Lerp(float a, float b, float t) { return a + (b - a) * t; }

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

// A point's side of the ship: -1 left .. 1 right. `rel`: the point from the ship; `rot`: its orientation, row-major, the columns the ship's right, up and back axes.
inline float PanOf(const float rel[3], const float rot[9])
{
    const float x = rel[0] * rot[0] + rel[1] * rot[3] + rel[2] * rot[6];
    const float len = sqrtf(rel[0] * rel[0] + rel[1] * rel[1] + rel[2] * rel[2]);
    if (!(len > 1e-3f))
        return 0;
    const float p = x / len;
    return p < -1 ? -1 : p > 1 ? 1 : p;
}

// How much of a voice reaches each grip: a centred one reaches both in full, a sided one fades on the far grip but never quite leaves it (the pad is one body).
inline float PanGain(float pan, int side)
{
    const float toward = side == 0 ? -pan : pan;
    return toward >= 0 ? 1.0f : 1.0f + toward * 0.85f;
}

// ---------------------------------------------------------------- silence
struct Gates
{
    int rumble = 100;    // the `rumble` setting, percent (0 = off)
    bool enabled = true; // the controller module is on
    bool focus = true;   // the one focus gate is open: the pad is the game's (focus_gate.h, Gamepad::ignoreReal false)
    bool pad = true;     // the pad is the device in use (device.h)
    bool paused = false; // the game is paused
    Where where = Where::Flight;
    bool setPiece = false;                    // a reader names the sequence running (Signals::piece): only then a cutscene is a set piece
    int cls[kClassCount] = {100, 100, 100, 100, 100}; // the class rows, percent
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
    if (c == kSetPiece)
    {
        if (g.paused)
            return "paused";
        if (g.where == Where::Flight || g.setPiece)
            return "";
        return g.where == Where::Cutscene ? "cutscene" : "docked";
    }
    if (g.where == Where::Cutscene)
        return "cutscene";
    if (c == kPrompt)
        return "";
    if (g.where != Where::Flight)
        return "docked";
    return g.paused ? "paused" : "";
}

// What the voices sum to this frame. low / high: the two motors' fold, the row's own levels for a centred effect.
struct Out
{
    float low = 0, high = 0;
    float mass[2] = {0, 0}, tex[2] = {0, 0}; // left, right
    float trig[2] = {0, 0};                  // the trigger motors
    float massHz = 80, texHz = 220, noise = 0; // the loudest voice's bands (the waveform)
    int clicks[2] = {0, 0};                  // bursts started since the last frame (the waveform)
    float peak = 0;                          // the highest cell: how hard the bed ducks
};

// ---------------------------------------------------------------- the mixer
class Mixer
{
  public:
    static constexpr int kVoices = 8;           // effects at once
    static constexpr float kDuck = 0.4f;        // an effect below the highest priority playing
    static constexpr float kFatigueTau = 3.0f;  // seconds: the cells' busy share is averaged over about this long
    static constexpr float kFatigueOn = 0.5f;   // busy above this share: the low-priority triggers are dropped
    static constexpr int kFatigueSpare = 5;     // priorities from here on are never dropped for fatigue
    static constexpr float kBusy = 0.02f;       // a cell above this counts as busy
    static constexpr float kSoftOn = 0.15f;     // the guns' own busy share above which they soften: about 3 s of held fire
    static constexpr float kSoftLevel = 0.5f;

    enum Result
    {
        kStarted,
        kMerged,   // inside its rate limit: it raises the next start's level
        kSilent,   // its category is silent now (dropped, never replayed)
        kFatigued, // dropped by the fatigue limiter
        kFull,     // every voice plays something of a higher priority
        kIgnored,  // no level or no such effect
    };

    Mixer()
    {
        for (double& t : lastStart_)
            t = -1e9;
    }

    // `pan` -1 left .. 1 right, where the thing is. `trigMask`: the trigger motors it reaches (1 left, 2 right, 3 both; 0 = by its pan).
    Result Trigger(int effect, float level, double now, const Gates& g, float pan = 0, int trigMask = 0)
    {
        if (effect < 0 || effect >= kEffectCount || !(level > 0))
            return Count(kIgnored);
        const EffectDef& d = kEffects[effect];
        level = Clamp01(level) * Clamp01(g.cls[d.cls] / 100.0f);
        if (*Silence(g, d.cat))
            return Count(kSilent);
        if (!(level > 0))
            return Count(kIgnored);
        if (d.soften && (duty_ > kFatigueOn || softDuty_ > kSoftOn))
            level *= kSoftLevel;
        else if (d.prio < kFatigueSpare && duty_ > kFatigueOn)
            return Count(kFatigued);
        if (now - lastStart_[effect] < d.minGap)
        {
            if (level > pending_[effect])
                pending_[effect] = level, pendingPan_[effect] = pan, pendingMask_[effect] = trigMask;
            return Count(kMerged);
        }
        return Count(Start(effect, level, now, pan, trigMask));
    }

    // Once per frame: the cells' mix now (after the master `rumble`), silenced categories stopped, merged triggers started when their gap is over.
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
                Count(Start(e, lv, now, pendingPan_[e], pendingMask_[e]));
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
        float loudest = 0, busyLevel = 0, softLevel = 0;
        for (const Voice& v : v_)
        {
            if (v.effect < 0)
                continue;
            const EffectDef& d = kEffects[v.effect];
            const float k = Envelope(d, static_cast<float>(now - v.start)) * v.level * (d.prio < top ? kDuck : 1.0f);
            for (int side = 0; side < 2; ++side)
            {
                const float gain = PanGain(v.pan, side);
                if (!d.waveOnly)
                {
                    Max(o.low, d.low * k);
                    Max(o.high, d.high * k);
                }
                Max(o.mass[side], d.low * k * gain);
                Max(o.tex[side], d.high * k * gain);
            }
            const int mask = v.trigMask ? v.trigMask : v.pan < -0.25f ? 1 : v.pan > 0.25f ? 2 : 3;
            for (int side = 0; side < 2; ++side)
                if (mask & (1 << side))
                    Max(o.trig[side], d.trig * k);
            const float level = (d.low > d.high ? d.low : d.high) * k;
            if (level > loudest)
                loudest = level, o.massHz = d.massHz, o.texHz = d.texHz, o.noise = d.noise;
            Max(d.soften ? softLevel : busyLevel, level);
        }
        const float a = dt / kFatigueTau;
        duty_ += ((busyLevel > kBusy ? 1.0f : 0.0f) - duty_) * (a > 1 ? 1 : a);
        softDuty_ += ((softLevel > kBusy ? 1.0f : 0.0f) - softDuty_) * (a > 1 ? 1 : a);
        const float master = Clamp01(g.rumble / 100.0f);
        o.low = Clamp01(o.low * master), o.high = Clamp01(o.high * master);
        for (int side = 0; side < 2; ++side)
        {
            o.mass[side] = Clamp01(o.mass[side] * master), o.tex[side] = Clamp01(o.tex[side] * master);
            o.trig[side] = Clamp01(o.trig[side] * master);
            o.clicks[side] = clicks_[side], clicks_[side] = 0;
            Max(o.peak, o.mass[side]);
            Max(o.peak, o.tex[side]);
        }
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
        clicks_[0] = clicks_[1] = 0;
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
    float SoftDuty() const { return softDuty_; }
    Out Last() const { return last_; }
    int Counted(Result r) const { return counts_[r]; }

  private:
    struct Voice
    {
        int effect = -1;
        float level = 0, pan = 0;
        int trigMask = 0;
        double start = 0;
    };

    static void Max(float& a, float b)
    {
        if (b > a)
            a = b;
    }

    Result Start(int effect, float level, double now, float pan, int trigMask)
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
        slot->effect = effect, slot->level = level, slot->start = now, slot->pan = pan, slot->trigMask = trigMask;
        lastStart_[effect] = now;
        pending_[effect] = 0;
        if (d.click)
            ++clicks_[pan < 0 ? 0 : 1];
        return kStarted;
    }
    Result Count(Result r)
    {
        ++counts_[r];
        return r;
    }

    Voice v_[kVoices];
    double lastStart_[kEffectCount];
    float pending_[kEffectCount] = {};
    float pendingPan_[kEffectCount] = {};
    int pendingMask_[kEffectCount] = {};
    int clicks_[2] = {0, 0};
    double lastUpdate_ = -1e9;
    float duty_ = 0, softDuty_ = 0;
    Out last_;
    int counts_[kIgnored + 1] = {};
};

// ---------------------------------------------------------------- the send policy
// The motors' values go to the pad only when they change (by kStep, compared, not rounded: the value sent is the mix's own), at most every kMinInterval, except a
// start from rest and a stop (at once); an unchanged running value is refreshed every kRefresh (the backends end a command after its duration: SDL's, the pad's own timer).
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
    bool shieldBack = false;              // the failed shield came back this frame
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
            else if (!up_ && v.shieldUp)
                h.shieldBack = true;
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

// ---------------------------------------------------------------- the shots
// One gun or launcher as space::PlayerWeapons reads it, cut to what a shot needs.
struct Gun
{
    unsigned id = 0;    // the hardpoint's sub object
    float refire = 0;   // refire_delay (s)
    float elapsed = 0;  // seconds since its last shot
    float damage = 0;   // hull damage per shot
    int ammo = -1;      // rounds left, -1 an energy gun
    bool homing = false, torpedo = false;
};

struct Shots
{
    float fast = 0, heavy = 0; // the level of this frame's shot of that class, 0 none
    bool missile = false, torpedo = false;
};

constexpr float kHeavyRefire = 0.3f; // seconds: a gun at least this slow gets the heavy grain and the snap

// A shot's level from its damage: a Justice-class gun near the floor, a heavy cannon near the top, on a log scale so that every gun in the game's range is told apart.
inline float ShotLevel(float damage)
{
    if (!(damage > 0))
        return 0.5f;
    return Clamp01(0.45f + 0.28f * (log10f(damage) - 1.5f));
}

// The shots of this frame, from each gun's refire clock falling (a gun's `elapsed` restarts at its shot) and each launcher's ammo. The first frame of a weapon is its baseline.
class ShotDetector
{
  public:
    static constexpr int kMax = 16;

    Shots Step(const Gun* guns, int n)
    {
        Shots s;
        if (n > kMax)
            n = kMax;
        for (int i = 0; i < n; ++i)
        {
            const Gun& g = guns[i];
            const bool known = i < n_ && was_[i].id == g.id;
            if (known)
            {
                if (g.homing)
                {
                    if (g.ammo >= 0 && g.ammo < was_[i].ammo)
                        (g.torpedo ? s.torpedo : s.missile) = true;
                }
                else if (g.elapsed < was_[i].elapsed - 1e-4f)
                {
                    float& cls = g.refire >= kHeavyRefire ? s.heavy : s.fast;
                    const float level = ShotLevel(g.damage);
                    if (level > cls)
                        cls = level;
                }
            }
            was_[i] = g;
        }
        n_ = n;
        return s;
    }
    void Forget() { n_ = 0; }

  private:
    Gun was_[kMax];
    int n_ = 0;
};

// ---------------------------------------------------------------- the signals
enum class SetPiece : int
{
    None,
    Lane,
    Dock,
    Launch,
    Jump, // through a gate
    Hole, // through a jump hole
};

enum class FireState : int
{
    Ready,
    Locked, // the game forbids it: a rule, the wall
    Empty,  // no power: limp
};

// The guns' verdict from the game's own CEGun::CanFire at a far point ahead (space::GunFireAt; FireResult: 0 destroyed, 1 disabled, 2 refire, 3 ammo, 4 power, 5 angle,
// 6 cruise, 7 lane, 8 cloak, 9 ok). One gun able to fire, or only waiting for its refire, is enough. `guns`: mounted guns, -1 unknown; `excluded`: the control exclusion in
// flight, or cruise from the charge on: the caller's word, because the codes alone cannot say it under held fire. CanFire asks the refire clock first, and that clock runs
// with no shot leaving the ship (docs\FREELANCER_RE.md "FireResult"), so 2 says nothing about a lock: `was`, the last state, stands on it. An empty or unread list says
// nothing either, so nothing stands a wall on a read that failed.
inline FireState FireStateOf(const int* codes, int n, int guns, bool excluded, float power, FireState was = FireState::Ready)
{
    if (excluded || guns == 0)
        return FireState::Locked;
    const bool dry = power >= 0 && power < 0.02f;
    int rule = 0, empty = 0, refire = 0;
    for (int i = 0; i < n; ++i)
    {
        const int c = codes[i];
        if (c == 5 || c == 9)
            return dry ? FireState::Empty : FireState::Ready;
        refire += c == 2;
        rule += c == 6 || c == 7 || c == 8;
        empty += c == 3 || c == 4;
    }
    if (rule > 0 && rule >= empty)
        return FireState::Locked;
    if (empty > 0 || dry)
        return FireState::Empty;
    return refire > 0 && was == FireState::Locked ? FireState::Locked : FireState::Ready;
}

enum class Field : int
{
    None,
    Asteroids,
    Nebula,
};

// The game's hit report (hostcalls PlayerHit), with the point already turned into what the engine needs.
struct HitReport
{
    unsigned seq = 0;  // grows with every report, 0 none yet
    unsigned kind = 0; // 1 a source point (a round far back along its flight, or a blast's centre), 2 a contact hit
    float pan = 0;     // the point's side of the ship
    float distance = -1; // the point's distance from the ship, -1 unknown
};

// One snapshot a frame. A field that was not read keeps its unknown value; every rule below takes unknown as "nothing to say".
struct Signals
{
    bool ship = false; // a player ship in space
    Vitals vitals;
    HitReport hit;
    float throttle = 0;
    bool burner = false, engineKilled = false;
    float gauge = -1, power = -1; // the thruster gauge and the gun capacitor, 0..1, -1 unknown
    bool cruiseEngaged = false;   // from the request: charging or cruising
    float chargeTime = 0, chargeElapsed = 0;
    bool disrupted = false;
    SetPiece piece = SetPiece::None;
    int ringsLeft = -1;
    float jumpLeft = -1;
    bool tractor = false;
    Field field = Field::None;
    bool radiation = false;
    float missileDistance = -1, missileClosing = 0, missilePan = 0;
    Shots shots;
    bool mine = false, countermeasure = false; // one left the ship this frame
    FireState fire = FireState::Ready;
    bool firePressed = false, fireHeld = false; // a trigger or button of the Fire role, under the player's finger
    bool launcherPressed = false;               // a launcher's button, pressed this frame
    int fireMask = 3;                           // the trigger motors a shot ticks (1 left, 2 right; 4 none: the autopilot fires under an idle finger)
    bool Charging() const { return cruiseEngaged && chargeTime > 0 && chargeElapsed > 0 && chargeElapsed < chargeTime; }
    bool Cruising() const { return cruiseEngaged && !Charging(); }
};

// The mounted engine's own numbers (`[Engine]`, DATA\EQUIPMENT\engine_equip.ini): rows without a range take these defaults, the middle of the game's own rows.
struct EngineData
{
    float attenLow = -5, attenHigh = 0;   // rumble_atten_range, dB, idle to full throttle
    float pitchLow = -25, pitchHigh = 25; // rumble_pitch_range, percent
};

// ---------------------------------------------------------------- the bed
// The continuous layer. Never above kBedCap outside the cruise charge and a set piece; sinks when nothing changes; ducked hard by any event.
struct BedOut
{
    float bed = 0, bedHz = 30; // the low bed, under 40 Hz
    float mass = 0, tex = 0, massHz = 80, texHz = 200, noise = 0;
};

class Bed
{
  public:
    static constexpr float kBedCap = 0.25f;
    static constexpr float kRest = 0.02f, kFresh = 0.06f; // the engine's level after and before a steady stretch
    static constexpr float kSteady = 3.0f;                // seconds without a change until it rests
    static constexpr float kDuckMax = 0.8f;

    BedOut Update(const Signals& s, const EngineData& e, const Gates& g, float dt, float eventPeak)
    {
        BedOut o;
        const bool world = !*Silence(g, kWorld) && s.ship;
        const bool piece = !*Silence(g, kSetPiece) && s.piece != SetPiece::None;
        t_ += dt;
        // the engine: a swell on a change of throttle, then near nothing; silent in cruise and with the engine killed
        const float change = throttle_ < 0 ? 0 : fabsf(s.throttle - throttle_);
        throttle_ = s.throttle;
        swell_ = Clamp01(swell_ + change * 3.0f) * expf(-dt / 0.7f);
        steady_ = change > 0.002f || s.burner ? 0 : steady_ + dt;
        const bool engineOn = world && !s.engineKilled && !s.cruiseEngaged && s.piece == SetPiece::None;
        if (engineOn)
        {
            const float rest = Lerp(kFresh, kRest, Clamp01(steady_ / kSteady));
            const float atten = powf(10.0f, Lerp(e.attenLow, e.attenHigh, Clamp01(s.throttle)) / 20.0f);
            o.bed = (rest + swell_ * 0.18f) * atten * (0.4f + 0.6f * Clamp01(s.throttle));
            o.bedHz = 30.0f * (1.0f + Lerp(e.pitchLow, e.pitchHigh, Clamp01(s.throttle)) / 100.0f);
        }
        // the afterburner: on the left trigger alone (the owner, 7 Nov 2026: "make boost only vibrate the left trigger") - its stance's vibration on a
        // DualSense, its trigger motor on an Xbox pad (the stances, section 7). Was: mass and a noise band in the grips as well
        Ramp(burn_, world && s.burner && !s.cruiseEngaged, dt, 0.2f);
        // the tractor's pull, a nebula's slow pressure and static
        Ramp(pull_, world && s.tractor, dt, 1.0f);
        Up(o.mass, 0.04f * pull_), Up(o.tex, 0.10f * pull_);
        Ramp(nebula_, world && s.field == Field::Nebula, dt, 2.0f);
        Up(o.bed, 0.05f * nebula_ * (0.6f + 0.4f * sinf(6.2832f * 0.25f * t_)));
        Up(o.tex, 0.03f * nebula_);
        if (nebula_ > 0.01f && burn_ <= 0.01f)
            o.noise = 1.0f;
        const float bedScale = Clamp01(g.cls[kClassShip] / 100.0f);
        o.bed *= bedScale, o.mass *= bedScale, o.tex *= bedScale;
        if (o.bed > kBedCap)
            o.bed = kBedCap;
        if (o.mass > kBedCap)
            o.mass = kBedCap;
        // the cruise charge: the build that earns the release, from the inside out as its meter fills from the middle out (the owner, 7 Nov 2026: "make
        // cruise charging not rumble the whole controller, but rather 'build up' the rumble from inside outwards, just like how it shows on screen"): a
        // light, high texture first, small and close; the deep mass that fills both hands joins from a third of the charge and grows to the release.
        // Was: the mass at once from the start (the whole pad), the texture late
        if (world && s.Charging())
        {
            const float f = Clamp01(s.chargeElapsed / s.chargeTime);
            const float outer = Clamp01((f - 0.33f) / 0.67f);
            Up(o.tex, (0.04f + 0.21f * f) * bedScale);
            Up(o.mass, 0.40f * outer * outer * bedScale);
            o.texHz = Lerp(220, 160, f);
            o.massHz = Lerp(45, 90, outer);
        }
        // the set pieces: a lane's steady push; a jump builds to its flash, a hole's breaks up as it does
        const float pieceScale = Clamp01(g.cls[kClassSetPieces] / 100.0f);
        Ramp(lane_, piece && s.piece == SetPiece::Lane, dt, 0.3f);
        Up(o.mass, 0.10f * lane_ * pieceScale), Up(o.tex, 0.03f * lane_ * pieceScale);
        const bool jump = piece && (s.piece == SetPiece::Jump || s.piece == SetPiece::Hole);
        jumpFor_ = jump ? jumpFor_ + dt : 0;
        if (jump)
        {
            const float f = s.jumpLeft >= 0 ? Clamp01(1 - s.jumpLeft / 4.0f) : Clamp01(jumpFor_ / 4.0f);
            float k = 1;
            if (s.piece == SetPiece::Hole)
            {
                flutterFor_ -= dt;
                if (flutterFor_ <= 0)
                    flutterFor_ = 0.08f, flutter_ = 0.35f + 0.65f * Rand();
                k = flutter_;
            }
            Up(o.mass, (0.15f + 0.55f * f) * k * pieceScale);
            Up(o.tex, 0.35f * f * k * pieceScale);
            o.massHz = Lerp(45, 80, f);
        }
        const float duck = 1.0f - kDuckMax * Clamp01(eventPeak / 0.6f);
        const float master = Clamp01(g.rumble / 100.0f) * duck;
        o.bed *= master, o.mass *= master, o.tex *= master;
        return o;
    }
    // The engine's own small generator: the same run gives the same feel, and a test can hold it.
    float Rand()
    {
        rng_ ^= rng_ << 13, rng_ ^= rng_ >> 17, rng_ ^= rng_ << 5;
        return static_cast<float>(rng_ >> 8) / 16777216.0f;
    }

  private:
    static void Up(float& a, float b)
    {
        if (b > a)
            a = b;
    }
    static void Ramp(float& v, bool on, float dt, float seconds)
    {
        const float step = seconds > 0 ? dt / seconds : 1;
        v = on ? (v + step > 1 ? 1 : v + step) : (v - step * 4 < 0 ? 0 : v - step * 4);
    }

    float throttle_ = -1, swell_ = 0, steady_ = 0, burn_ = 0, pull_ = 0, nebula_ = 0, lane_ = 0;
    float jumpFor_ = 0, flutterFor_ = 0, flutter_ = 1, t_ = 0;
    unsigned rng_ = 0x2545F491u;
};

// ---------------------------------------------------------------- the failing hull's beat
// The HUD's failing bars' clock (modules\hud\src\hud2_bars.h: on under 25 %, off over 30 %, 1.25 Hz at 25 % to 2.9 Hz at nothing), so the beat and the bar agree without
// either reading the other: a module reads no other module.
inline float BeatRate(float hull) { return 1.25f * powf(2.9f / 1.25f, 1.0f - Clamp01(hull / 0.25f)); }

// ---------------------------------------------------------------- the engine
// Everything between the Signals and the frame. `Fired` lists this frame's triggers, for the log and the harness.
struct HapticFrame
{
    Out mix;
    BedOut bed;
    bool Quiet() const { return mix.peak <= 0 && bed.bed <= 0 && bed.mass <= 0 && bed.tex <= 0; }
};

class Engine
{
  public:
    static constexpr float kBlastReach = 300.0f;  // a source point this near is a blast's centre (a round's report lies about 1000 m back along its flight)
    static constexpr float kReportAge = 0.25f;    // seconds a hit report can still name the side of a hit
    static constexpr float kRadiationOnly = 0.02f; // a hull loss below this share inside a radiation zone is the zone's, not a hit
    static constexpr float kPieceTail = 1.5f;     // seconds a set piece's gate outlives it
    static constexpr float kFlashAt = 0.15f;      // seconds before the jump: the 10 Hz read cannot miss it
    static constexpr int kFiredMax = 24;

    struct FiredOne
    {
        int effect;
        float level;
        Mixer::Result result;
    };

    HapticFrame Step(const Signals& s, const Gates& g, const EngineData& e, double now)
    {
        const float dt = last_ > -1e8 && now > last_ ? static_cast<float>(now - last_) : 0.0f;
        last_ = now;
        fired_ = 0;
        // a sequence's last beat comes as it ends: the gate stays open for it
        if (s.piece != SetPiece::None)
            pieceAt_ = now;
        Gates open = g;
        open.setPiece = now - pieceAt_ < kPieceTail;
        StepHits(s, open, now, dt);
        StepWeapons(s, open, now);
        StepFlight(s, open, now, dt);
        StepPieces(s, open, now);
        HapticFrame f;
        f.mix = mix_.Update(now, open);
        f.bed = bed_.Update(s, e, open, dt, f.mix.peak);
        was_ = s;
        return f;
    }

    // A trigger from outside the signals: the hand-back, a box, the debug `play`.
    Mixer::Result Fire(int effect, float level, double now, const Gates& g, float pan = 0, int mask = 0)
    {
        const Mixer::Result r = mix_.Trigger(effect, level, now, g, pan, mask);
        if (fired_ < kFiredMax)
            firedList_[fired_++] = {effect, level, r};
        return r;
    }

    void Stop()
    {
        mix_.Stop();
        shots_.Forget();
    }
    const Mixer& Mix() const { return mix_; }
    Mixer& Mix() { return mix_; }
    int FiredCount() const { return fired_; }
    const FiredOne& Fired(int i) const { return firedList_[i]; }
    ShotDetector& ShotReader() { return shots_; }
    float HullRatio() const { return hullRatio_; }

  private:
    void StepHits(const Signals& s, const Gates& g, double now, float dt)
    {
        // the report first: a hit of this frame takes its side from it
        if (s.hit.seq != reportSeq_)
        {
            const bool first = reportSeq_ == 0;
            reportSeq_ = s.hit.seq;
            if (!first && s.hit.seq)
            {
                report_ = s.hit, reportAt_ = now;
                if (s.hit.kind == 1 && s.hit.distance >= 0 && s.hit.distance < kBlastReach && s.vitals.valid)
                    Fire(kExplosion, 1.0f - 0.7f * s.hit.distance / kBlastReach, now, g, s.hit.pan);
            }
        }
        const bool fresh = now - reportAt_ < kReportAge;
        const float pan = fresh ? report_.pan : 0;
        const bool contact = fresh && report_.kind == 2;
        const Hits h = detector_.Step(s.vitals);
        if (h.shieldFailed)
            Fire(kShieldFail, 1.0f, now, g, pan);
        if (h.shieldBack)
            Fire(kShieldUp, 1.0f, now, g);
        if (h.shieldShare > 0)
            Fire(contact ? kCollision : kHitShield, HitStrength(h.shieldShare), now, g, pan);
        const bool zoneLoss = s.radiation && h.hullShare < kRadiationOnly;
        if (h.hullShare > 0 && !zoneLoss)
            Fire(contact ? kCollision : kHitHull, HitStrength(h.hullShare), now, g, pan);
        // radiation: Geiger clicks at random, the rate from the hull's real loss, so a stronger zone clicks faster
        lossRate_ += ((dt > 0 ? h.hullShare / dt : 0) - lossRate_) * Clamp01(dt / 1.5f);
        if (s.radiation)
        {
            const float rate = 3.0f + 800.0f * lossRate_;
            if (bed_.Rand() < (rate > 25 ? 25 : rate) * dt)
                Fire(kRadClick, 0.5f + 0.5f * bed_.Rand(), now, g, bed_.Rand() * 2 - 1);
        }
        // the critical hull's beat
        hullRatio_ = s.vitals.valid && s.vitals.hullMax > 0 ? s.vitals.hull / s.vitals.hullMax : -1;
        if (hullRatio_ >= 0 && hullRatio_ < 0.25f)
            critical_ = true;
        else if (hullRatio_ < 0 || hullRatio_ > 0.30f)
            critical_ = false;
        if (critical_)
        {
            beat_ += BeatRate(hullRatio_) * dt;
            if (beat_ >= 1)
                beat_ -= 1, Fire(kHullBeat, 1.0f, now, g);
        }
        else
            beat_ = 1; // the first beat comes at once
    }

    void StepWeapons(const Signals& s, const Gates& g, double now)
    {
        // (a held fire under a lock runs the refire clocks with no shot leaving the ship: seen in cruise and on a lane, no bolt in the frames. No grain for a shot that never was)
        if (s.shots.fast > 0 && s.fire != FireState::Locked)
            Fire(kGunFast, s.shots.fast, now, g, 0, s.fireMask);
        if (s.shots.heavy > 0 && s.fire != FireState::Locked)
            Fire(kGunHeavy, s.shots.heavy, now, g, 0, s.fireMask);
        if (s.shots.missile)
            Fire(kMissileLaunch, 1.0f, now, g);
        if (s.shots.torpedo)
            Fire(kTorpedoLaunch, 1.0f, now, g);
        if (s.mine)
            Fire(kMineDrop, 1.0f, now, g);
        if (s.countermeasure)
            Fire(kCmDrop, 1.0f, now, g);
        if (s.fire == FireState::Locked && s.firePressed)
            Fire(kFireLocked, 1.0f, now, g, 0, s.fireMask);
        else if (s.fire == FireState::Empty && s.fireHeld)
            Fire(kGunDry, 1.0f, now, g, 0, s.fireMask);
        if (s.launcherPressed && s.Cruising()) // nothing launches in cruise (docs\FREELANCER_RE.md "Weapons")
            Fire(kFireLocked, 1.0f, now, g);
    }

    void StepFlight(const Signals& s, const Gates& g, double now, float dt)
    {
        if (s.engineKilled && !was_.engineKilled && was_.ship)
            Fire(kEngineKill, 1.0f, now, g);
        if (was_.Charging() && s.cruiseEngaged && !s.Charging())
            Fire(kCruiseRelease, 1.0f, now, g);
        if (s.disrupted && !was_.disrupted)
            Fire(kCruiseDisrupt, 1.0f, now, g);
        if (was_.tractor && !s.tractor)
            Fire(kTractorTick, 1.0f, now, g);
        // a missile: a pulse on its bearing's side, faster as it closes; one dead ahead alternates, so that an Xbox pad's triggers can say it
        if (s.missileDistance >= 0)
        {
            const float closing = s.missileClosing > 50 ? s.missileClosing : 50;
            const float impact = s.missileDistance / closing;
            float interval = impact * 0.12f;
            interval = interval < 0.12f ? 0.12f : interval > 0.7f ? 0.7f : interval;
            missileIn_ -= dt;
            if (missileIn_ <= 0)
            {
                missileIn_ = interval, alternate_ = 3 - alternate_;
                const float near = 1.0f - Clamp01(s.missileDistance / 2500.0f);
                Fire(kMissileNear, 0.5f + 0.5f * near, now, g, s.missilePan, s.missilePan > -0.25f && s.missilePan < 0.25f ? alternate_ : 0);
            }
        }
        else
            missileIn_ = 0;
        // an asteroid field: sparse grit
        if (s.field == Field::Asteroids && s.ship && !s.cruiseEngaged && bed_.Rand() < 0.7f * dt)
            Fire(kGrit, 0.4f + 0.6f * bed_.Rand(), now, g, bed_.Rand() * 2 - 1);
    }

    void StepPieces(const Signals& s, const Gates& g, double now)
    {
        // the flash comes at the jump itself, while the game still flies the sequence: what follows may be a load, with every gate shut
        const bool jump = s.piece == SetPiece::Jump || s.piece == SetPiece::Hole;
        if (jump && s.jumpLeft >= 0 && s.jumpLeft <= kFlashAt && !flashed_)
            flashed_ = true, Fire(s.piece == SetPiece::Hole ? kHoleFlash : kJumpFlash, 1.0f, now, g);
        if (s.piece == was_.piece)
        {
            if (s.piece == SetPiece::Lane && s.ringsLeft >= 0 && was_.ringsLeft > s.ringsLeft)
                Fire(kLaneRing, 1.0f, now, g);
            return;
        }
        switch (s.piece)
        {
        case SetPiece::Dock: Fire(kDockClamp, 1.0f, now, g); break;
        case SetPiece::Launch: Fire(kLaunchRelease, 1.0f, now, g); break;
        default: break;
        }
        if (was_.piece == SetPiece::Lane)
            Fire(kLaneExit, 1.0f, now, g);
        else if ((was_.piece == SetPiece::Jump || was_.piece == SetPiece::Hole) && !flashed_)
            Fire(was_.piece == SetPiece::Hole ? kHoleFlash : kJumpFlash, 1.0f, now, g);
        flashed_ = false;
    }

    Mixer mix_;
    Bed bed_;
    HitDetector detector_;
    ShotDetector shots_;
    Signals was_;
    HitReport report_;
    unsigned reportSeq_ = 0;
    double reportAt_ = -1e9, last_ = -1e9, pieceAt_ = -1e9;
    float lossRate_ = 0, beat_ = 1, missileIn_ = 0, hullRatio_ = -1;
    bool critical_ = false, flashed_ = false;
    int alternate_ = 1;
    FiredOne firedList_[kFiredMax];
    int fired_ = 0;
};
} // namespace haptics
