// Lead pursuit (pursuit.h).
#include "game/pursuit.h"

#include "game/fl.h"
#include "game/flight.h"
#include "game/flightio.h"

namespace leadchase
{
// Flies like a mouse player: the nose (and the aim cursor) on the lead cross; range keeping modelled on a human
// player (CombatTest.mp4, mission 1 vs Rogue fighters): the throttle is binary - full forward or full reverse,
// switched often - never partial. Fighters (radius ~10 m) are fought at ~40-110 m; reverse is held when close and
// through sharp turns (it tightens the turn), afterburner closes gaps beyond the close band when the target is ahead.
// weave (0..1): evasion blended into the attack - the nose circles the lead cross (a corkscrew flight path, hard to
// hit) while the reticle stays on the cross and the gimbaled guns keep firing; the circle stays inside the fire cone.
PursuitOut Pursue(const aim::Target& t, float dt, Pursuit& p, float closeBand, float fireCone, float gunRange, float weave, float weaveRadius)
{
    PursuitOut o;
    const float scale = flightio::Get().steerScale;
    const float w = static_cast<float>(fl::ScreenWidth()), h = static_cast<float>(fl::ScreenHeight());
    o.ax = w * 0.5f;
    o.ay = h * 0.5f;
    p.phase += dt * (3.0f + 3.0f * weave); // ~0.5-1 turn per second
    if (t.onScreen)
    {
        float ox = (t.screenX - w * 0.5f) / (w * 0.5f), oy = -(t.screenY - h * 0.5f) / (h * 0.5f);
        // Steer at a point circling the lead (radius up to weaveRadius of the fire cone): the guns' gimbal covers it.
        const float r = Clampf(weave, 0, 1) * fireCone * weaveRadius;
        const float steerX = ox + r * cosf(p.phase), steerY = oy + r * sinf(p.phase);
        o.sx = Clampf(steerX * 1.5f, -scale, scale);
        o.sy = Clampf(steerY * 1.5f, -scale, scale);
        o.screenFrame = true;
        o.ax = t.screenX;
        o.ay = t.screenY;
        aim::ClampToSafeArea(o.ax, o.ay);
        // The guns reach the cursor only within their gimbal range: fire near the nose.
        o.offset = sqrtf(ox * ox + oy * oy);
        o.aligned = fabsf(o.ax - t.screenX) < 2 && fabsf(o.ay - t.screenY) < 2 && o.offset < fireCone;
    }
    else
        aim::TurnTowards(t.world, scale, o.sx, o.sy);
    float closeDist = closeBand > 0 ? closeBand : std::max(45.0f, t.radius * 2.5f + 20.0f);
    if (closeBand <= 0 && gunRange > 0)
        closeDist = std::min(closeDist, gunRange * 0.5f); // big targets: still well inside the guns' reach
    const float farDist = closeDist + 60.0f;
    // Reverse early when the target is about to leave the screen (lead cross far off-center and still moving
    // outwards), not only once it is gone: the tighter turn keeps it in view.
    const float offsetRate = dt > 0 ? (o.offset - p.lastOffset) / dt : 0.0f;
    p.lastOffset = o.offset;
    const bool leaving = o.offset > 0.4f && offsetRate > 0.2f;
    const bool sharpTurn = o.offset > 0.6f || leaving;
    // Big targets (radius > 50 m: stations, generators, capital ships) are braked for by the closing speed too - 1.2 s
    // of it beyond the band (M12 22 Oct 2026: the run at a Nomad shield generator closed at ~200 m/s, reverse began
    // 60 m out, the ship rammed it and died at full hull).
    // Fighters: half a second of a fast close (> 80 m/s) beyond the band (M13 22 Oct 2026: a head-on close on a Nomad
    // fighter at ~100 m, the ship died outright at 94 % hull as the fighter blew up - a collision).
    float brakeMargin = t.radius > 50.0f    ? std::max(0.0f, t.closing) * 1.2f
                        : t.closing > 80.0f ? t.closing * 0.5f
                                            : 0.0f;
    // ...by the flight model's own braking distance where it is known (s9m, M12 gen 3: 1.2 s of 170 m/s = 204 m - full
    // reverse took the ship to 20 m/s in 0.5 s, 200 m short; then the afterburner again: three brake-and-burn cycles in
    // the last 300 m to every generator): 1.5x the stop + 40 m, never less than the old 60 m of the 22 Oct ramming
    if (t.radius > 50.0f && t.closing > 0)
    {
        flight::Model fm;
        if (flight::ReadModel(fm))
            brakeMargin =
                std::min(brakeMargin, std::max(60.0f, flight::BrakeDistance(fm, t.closing, 0) * 1.5f + 40.0f));
    }
    bool reverse =
        t.dist < closeDist || (sharpTurn && t.dist < 250.0f) || (t.closing > 40.0f && t.dist < farDist + brakeMargin);
    // Minimum hold so the switching happens in bursts (the player toggles about once a second), not every frame.
    p.reverseHold -= dt;
    if (reverse != p.reversing && p.reverseHold <= 0)
    {
        p.reversing = reverse;
        p.reverseHold = 0.5f;
    }
    o.reverse = p.reversing;
    // Aggressive afterburner: burst whenever the target is beyond the close band and roughly ahead.
    o.burn = !o.reverse && o.offset < 0.45f && (t.dist > farDist || !t.inRange);
    return o;
}
} // namespace leadchase
