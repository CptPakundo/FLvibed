// The weapon group cue (modules\hud\docs\UI_HUD2_GROUP_CUE.md, section 0's rulings first; GitHub issue 26): a row above the ring, each mark on a small dark cell: the
// group digits, then M and T. One meaning per channel: colour selected, fill in reach, glow and the tick locked, a bar reloading, amber and a slash cannot fire.
// Pure: no game call, so tests\hud2_cue_test.cpp runs the same code; hud2_data.cpp fills it at 10 Hz, hud2_cue.cpp draws it. Fractions of H x k.
#pragma once
#include <cmath>

namespace hud2cue
{
constexpr float kCellW = 0.020f, kCellH = 0.026f, kCellGap = 0.003f, kLetterGap = 0.009f;
constexpr float kAboveRing = 0.020f;  // the row's centre beyond the ring's circle: above the ring (D5)
constexpr float kBarH = 0.0026f;      // the reload's thin bar under M and T
constexpr float kTickTravel = 0.0028f; // the tick's bounce, about 2 px at 800 lines
constexpr float kLitHold = 0.17f;     // lit and unlit each hold this long, so the row cannot cross 3 Hz
constexpr float kEase = 0.08f;
constexpr float kLockHz = 1.0f;       // the breath of the glow and the tick, together
constexpr int kMaxMarks = 5;

// The game's FireResult (docs\FREELANCER_RE.md "gunpoint"): 2 refire is ignored, a gun recharges in 0.12-0.5 s and a cell would blink with every shot.
enum Fire : int
{
    Destroyed = 0,
    Disabled = 1,
    Refire = 2,
    Ammo = 3,
    Power = 4,
    Angle = 5,
    Cruise = 6,
    Lane = 7,
    Cloak = 8,
    Ok = 9,
    Unread = -1
};

// One weapon as the test reads it.
struct Gun
{
    float reach = 0;  // CEGun::GetMunitionRange; a LOCK seeker's seeker range
    int fire = Unread;
    int ammo = -1;    // rounds, -1 unlimited
};

enum Verdict : int
{
    Hide,     // cruise, a lane: the row goes
    Unlit,    // out of reach, or off the angle
    Lit,      // in reach and able
    Amber     // nothing of this mark can fire
};

// One mark from its weapons. `onTarget`: a target stands in the ring; `dist` the distance to its surface. An unread reach shows the cell, never lit.
inline Verdict Judge(const Gun* guns, int n, bool onTarget, float dist)
{
    bool anyReach = false, able = false, allDead = n > 0;
    for (int i = 0; i < n; ++i)
    {
        const Gun& g = guns[i];
        if (g.fire == Cruise || g.fire == Lane)
            return Hide;
        const bool noRounds = g.ammo == 0;
        const bool dead = noRounds || g.fire == Destroyed || g.fire == Disabled || g.fire == Ammo || g.fire == Power;
        allDead = allDead && dead;
        if (dead)
            continue;
        if (g.fire == Ok || g.fire == Refire)
            able = true;
        anyReach = anyReach || (g.reach > 0 && dist <= g.reach);
    }
    if (allDead)
        return Amber;
    return onTarget && able && anyReach ? Lit : Unlit;
}

// The target stands in the centre: the lead point inside the ring round the aim point, or any part of the target (a battleship fills the ring before its centre does).
inline bool InCentre(float leadX, float leadY, bool leadOn, float bodyX, float bodyY, float bodyR, bool bodyOn, float aimX, float aimY, float ring)
{
    const float lx = leadX - aimX, ly = leadY - aimY, bx = bodyX - aimX, by = bodyY - aimY;
    return (leadOn && lx * lx + ly * ly <= ring * ring) || (bodyOn && sqrtf(bx * bx + by * by) <= ring + bodyR);
}

// The lit state with its hold: a change stands only after the last one has held kLitHold, and eases.
struct Hold
{
    bool lit = false;
    float since = 1e9f, shown = 0;

    float Step(bool want, float dt)
    {
        since += dt;
        if (want != lit && since >= kLitHold)
            lit = want, since = 0;
        const float to = lit ? 1.0f : 0.0f, f = dt >= kEase ? 1.0f : dt / kEase;
        shown += (to - shown) * f;
        return shown;
    }
};

// The row's cells: `digits` groups, then `letters` (M, T). Fills the cells' centres from the left; the row's half width. Centred on x = 0.
inline float Layout(int digits, int letters, float scaled, float* xs)
{
    const float w = kCellW * scaled, g = kCellGap * scaled, lg = kLetterGap * scaled;
    float x = 0;
    int n = 0;
    for (int i = 0; i < digits; ++i, ++n)
        xs[n] = x + w / 2, x += w + (i + 1 < digits ? g : 0);
    if (digits && letters)
        x += lg;
    for (int i = 0; i < letters; ++i, ++n)
        xs[n] = x + w / 2, x += w + (i + 1 < letters ? g : 0);
    for (int i = 0; i < n; ++i)
        xs[i] -= x / 2;
    return x / 2;
}

// The tick's height over its rest and the glow's strength, on one breath; both rest under skin_motion's fades and none.
inline float Breath(float seconds, int motion) { return motion != 0 ? 1.0f : 0.5f - 0.5f * cosf(2 * 3.14159265f * kLockHz * seconds); }
} // namespace hud2cue
