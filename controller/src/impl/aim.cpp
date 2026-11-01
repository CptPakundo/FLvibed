#include "aim.h"

#include "../fl.h"
#include "game/ai.h"
#include "game/gameclock.h"
#include "game/space.h"
#include "ui.h"

namespace aim
{
Target Current(bool flightFrame)
{
    Target t;
    fl::Kinematics me, tgt;
    if (!fl::PlayerKinematics(me) || !fl::TargetKinematics(tgt))
        return t;
    t.valid = true;
    fl::TargetAttitude(t.attitude);
    t.hostile = t.attitude <= kHostileAttitude;
    fl::TargetRadius(t.radius);
    const float d[3] = {tgt.pos[0] - me.pos[0], tgt.pos[1] - me.pos[1], tgt.pos[2] - me.pos[2]};
    t.dist = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    if (t.dist > 1e-3f)
    {
        const float rv[3] = {tgt.vel[0] - me.vel[0], tgt.vel[1] - me.vel[1], tgt.vel[2] - me.vel[2]};
        t.closing = -(rv[0] * d[0] + rv[1] * d[1] + rv[2] * d[2]) / t.dist;
    }
    memcpy(t.world, tgt.pos, sizeof(t.world));
    float lead[3];
    if (fl::TargetLeadPos(lead) == 1)
    {
        t.inRange = true;
        memcpy(t.world, lead, sizeof(t.world));
    }
    else
    {
        // Beyond the HUD's lead cross (it shows only in weapons range) a player eyeballs where it will appear and
        // steers there (user, 19 Oct 2026: not head-on at the target, then a swing onto the cross at range): our own
        // intercept from 2x the guns' range in - the point the target reaches when a shot at our guns' muzzle speed
        // would (relative velocity: |d + v t| = s t). Guns read once a second (the game's range, the active guns'
        // mean muzzle speed).
        static float speed = 0, range = 0;
        static DWORD next = 0;
        if (gameclock::NowMs() >= next)
        {
            next = gameclock::NowMs() + 1000;
            float sum = 0;
            int n = 0;
            for (const space::Weapon& wpn : space::PlayerWeapons())
                if (wpn.active && !wpn.homing && wpn.muzzleVelocity > 0)
                    sum += wpn.muzzleVelocity, ++n;
            speed = n ? sum / n : 0;
            ai::Self self;
            range = ai::ReadSelf(self) && self.gunRange > 0 ? self.gunRange : 0;
        }
        if (speed > 0 && range > 0 && t.dist <= 2 * range)
        {
            const float v[3] = {tgt.vel[0] - me.vel[0], tgt.vel[1] - me.vel[1], tgt.vel[2] - me.vel[2]};
            const float a = v[0] * v[0] + v[1] * v[1] + v[2] * v[2] - speed * speed;
            const float b = 2 * (d[0] * v[0] + d[1] * v[1] + d[2] * v[2]);
            const float c = d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
            const float disc = b * b - 4 * a * c;
            if (a < 0 && disc >= 0) // the shot is faster than the target closes away: one positive root
            {
                const float tt = (-b - sqrtf(disc)) / (2 * a);
                if (tt > 0 && tt < 10)
                    for (int i = 0; i < 3; ++i)
                        t.world[i] = tgt.pos[i] + v[i] * tt;
            }
        }
    }
    // A huge static target (the Dyson city's generators: radius 915 m, the hull ~430 m from the centre): the autopilot
    // aims at its hull in front of us, not at its centre - the centre lay beyond gun reach 260 m off the hull, the game's
    // CanPointAt refused it ("guns 1 of 5 point") and every hold had to hug the hull (the owner, 29 Oct 2026: "the ship
    // does not need to hug the huge generator solars so tightly")
    if (flightFrame && t.radius >= 500.0f)
    {
        const float sp2 = tgt.vel[0] * tgt.vel[0] + tgt.vel[1] * tgt.vel[1] + tgt.vel[2] * tgt.vel[2];
        const float hit = sp2 < 4.0f ? space::RayHitDistance(me.pos, tgt.pos) : -1.0f;
        if (hit > 0 && t.dist > 1)
        {
            for (int i = 0; i < 3; ++i)
                t.world[i] = me.pos[i] + d[i] / t.dist * hit;
            // ...and "in range" by that hull point: the game's lead cross measures to the centre, so the ships never
            // opened fire on the city generators with the hull well inside gun reach (the owner, 29 Oct 2026: "the ships
            // never opened fire, not once")
            static float gunRange = 0;
            static DWORD next = 0;
            if (gameclock::NowMs() >= next)
            {
                next = gameclock::NowMs() + 1000;
                ai::Self self;
                gunRange = ai::ReadSelf(self) && self.gunRange > 0 ? self.gunRange : 0;
            }
            if (gunRange > 0)
                t.inRange = hit <= gunRange;
        }
    }
    float s[3];
    const float w = static_cast<float>(fl::ScreenWidth()), h = static_cast<float>(fl::ScreenHeight());
    if ((flightFrame ? fl::WorldToFlightScreen(t.world, s) : fl::WorldToScreen(t.world, s)) && s[0] >= 0 && s[0] < w &&
        s[1] >= 0 && s[1] < h)
    {
        t.onScreen = true;
        t.screenX = s[0];
        t.screenY = s[1];
    }
    return t;
}

bool TurnTowards(const float world[3], float scale, float& sx, float& sy)
{
    fl::Kinematics me;
    if (!fl::PlayerKinematics(me))
        return false;
    const float w[3] = {world[0] - me.pos[0], world[1] - me.pos[1], world[2] - me.pos[2]};
    float d[3]; // R^T * w: x right, y up, -z forward
    for (int i = 0; i < 3; ++i)
        d[i] = me.rot[0 * 3 + i] * w[0] + me.rot[1 * 3 + i] * w[1] + me.rot[2 * 3 + i] * w[2];
    const float yaw = atan2f(d[0], -d[2]), pitch = atan2f(d[1], -d[2]);
    const float len = sqrtf(yaw * yaw + pitch * pitch);
    if (len < 1e-4f)
    {
        sx = sy = 0;
        return true;
    }
    sx = yaw / len * scale;
    sy = pitch / len * scale;
    return true;
}

bool SteerTowards(const float world[3], float scale, float fullAngle, float& sx, float& sy)
{
    fl::Kinematics me;
    if (!fl::PlayerKinematics(me))
        return false;
    const float w[3] = {world[0] - me.pos[0], world[1] - me.pos[1], world[2] - me.pos[2]};
    float d[3]; // R^T * w: x right, y up, -z forward
    for (int i = 0; i < 3; ++i)
        d[i] = me.rot[0 * 3 + i] * w[0] + me.rot[1 * 3 + i] * w[1] + me.rot[2 * 3 + i] * w[2];
    const float yaw = atan2f(d[0], -d[2]), pitch = atan2f(d[1], -d[2]);
    const float len = sqrtf(yaw * yaw + pitch * pitch);
    if (len < 1e-4f)
    {
        sx = sy = 0;
        return true;
    }
    // Rate proportional to the angle up to fullAngle, full beyond (direction kept).
    const float rate = scale * (fullAngle > 0 ? std::min(1.0f, len / fullAngle) : 1.0f);
    sx = yaw / len * rate;
    sy = pitch / len * rate;
    return true;
}

void LevelSteer(float& sx, float& sy, const float* frame, const float* into)
{
    fl::Kinematics me;
    if (!fl::PlayerKinematics(me))
        return;
    const float* f = frame ? frame : me.rot;
    const float right[3] = {f[0 * 3 + 0], f[1 * 3 + 0], f[2 * 3 + 0]};
    const float up[3] = {f[0 * 3 + 1], f[1 * 3 + 1], f[2 * 3 + 1]};
    const float fwd[3] = {-me.rot[0 * 3 + 2], -me.rot[1 * 3 + 2], -me.rot[2 * 3 + 2]}; // (-z)
    // the turn frame's right: the level one = fwd x (0, 1, 0), or `into`'s right (the camera's) square to the nose; its up =
    // right x fwd
    float lr[3] = {-fwd[2], 0.0f, fwd[0]};
    if (into)
    {
        const float ir[3] = {into[0 * 3 + 0], into[1 * 3 + 0], into[2 * 3 + 0]};
        const float d = ir[0] * fwd[0] + ir[1] * fwd[1] + ir[2] * fwd[2];
        for (int i = 0; i < 3; ++i)
            lr[i] = ir[i] - d * fwd[i];
    }
    const float n = sqrtf(lr[0] * lr[0] + lr[1] * lr[1] + lr[2] * lr[2]);
    if (n < 0.2f)
        return;
    lr[0] /= n, lr[1] /= n, lr[2] /= n;
    const float lu[3] = {lr[1] * fwd[2] - lr[2] * fwd[1], lr[2] * fwd[0] - lr[0] * fwd[2],
                         lr[0] * fwd[1] - lr[1] * fwd[0]};
    // the wanted turn as a world direction, then its components on the level axes (a rotation by the bank)
    float c[3];
    for (int i = 0; i < 3; ++i)
        c[i] = sx * right[i] + sy * up[i];
    sx = c[0] * lr[0] + c[1] * lr[1] + c[2] * lr[2];
    sy = c[0] * lu[0] + c[1] * lu[1] + c[2] * lu[2];
}

static bool g_hudTransparent = false;
void SetHudTransparent(bool on) { g_hudTransparent = on; }

void ClampToSafeArea(float& x, float& y)
{
    if (g_hudTransparent)
    {
        x = Clampf(x, 1.0f, static_cast<float>(fl::ScreenWidth() - 2));
        y = Clampf(y, 1.0f, static_cast<float>(fl::ScreenHeight() - 2));
        return;
    }
    // Ask Freelancer's own UI hit test (works for any HUD layout, e.g. HudShift): if the point would
    // hover a control, pull it back towards the screen center until it does not.
    const float w = static_cast<float>(fl::ScreenWidth()), h = static_cast<float>(fl::ScreenHeight());
    x = Clampf(x, w * 0.02f, w * 0.98f);
    y = Clampf(y, h * 0.02f, h * 0.98f);
    if (!ui::HitTest(x, y))
        return;
    const float cx = w * 0.5f, cy = h * 0.5f;
    if (ui::HitTest(cx, cy))
        return;           // something covers the center: nothing sensible to do
    float lo = 0, hi = 1; // fraction of the way from the center to (x, y); lo is known to be free
    for (int i = 0; i < 8; ++i)
    {
        const float mid = (lo + hi) * 0.5f;
        if (ui::HitTest(cx + (x - cx) * mid, cy + (y - cy) * mid))
            hi = mid;
        else
            lo = mid;
    }
    const float t = lo * 0.97f; // small margin off the control's edge
    x = cx + (x - cx) * t;
    y = cy + (y - cy) * t;
}
} // namespace aim

namespace aim
{
float AngleFromCenter(float x, float y)
{
    // the current 3D camera's pixel centre and scale (pixels per unit of tan): fl.h CameraView
    fl::CameraView c;
    if (!fl::ReadCamera(c))
        return 180.0f;
    const float sx = fabsf(c.sx), sy = fabsf(c.sy);
    if (sx < 1 || sy < 1)
        return 180.0f;
    const float tx = (x - c.cx) / sx, ty = (y - c.cy) / sy;
    return atanf(sqrtf(tx * tx + ty * ty)) * 57.29578f;
}
} // namespace aim
