// The pad in flight: aim, the test bot, auto and fire targets, the turret look, the preset runner, FlyPlayer, UpdateSpace.
#include "controller.h"

#include "dimouse.h"
#include "game/events.h"
#include "game/aim.h"
#include "game/flightio.h"
#include "game/pursuit.h"
#include "settings.h"
#include "game/fl.h"
#include "game/ai.h"
#include "game/deckmode.h"
#include "game/flight.h"
#include "game/gameclock.h"
#include "game/geom.h"
#include "game/mission.h" // ForgetGame at a game load (host v34)
#include "game/render.h" // OnDeviceLost: the shared renderer's state block before a Reset
#include "game/room.h"
#include "game/space.h"
#include "game/spawn.h"
#include "log.h"
#include "../shm.h"
#include "device.h"
#include "gamepad.h"
#include "ui/glyphs.h"
#include "haptics.h" // the pad's vibration: events -> effect table -> mixer -> the two motors (IDEAS U30)
// phase D flight modals (the D-FLIGHT track): the goal panel is the Autopilot module's (core\ui\views.h),
#include "picker.h" // the target picker,
#include "wheel.h"  // the action wheel
#include "host.h"
#include "padhud.h"
#include "enabled_gate.h"
#include "game/coreframe.h"
#include "game/gamemode.h"
#include "game/mprules.h"
#include "ui/flufaware.h" // FLUF's windows: keep-outs, the pad on them, the keys' yield (docs\MOD_COMPATIBILITY.md FLUF)
// D-MENUS: the menu screens on the pad, the New Game prompt, speedrun mode (clock, splits, LiveSplit)
// and the HUD timer (padmenus.h / newgame.h; the clock and the timer: modules\speedrun)
#include "game/target_cycle.h"   // the targeting press's enemy rule
#include "newgame.h"
#include "padmenus.h"
#include "padbases.h" // phase D bases / dealers / NeuroNet windows (the D-BASES track),
#include "padmap.h"   // the nav map,
#include "wgroups.h"  // the weapon group editor,
#include "game/hudinfo.h"
#include "game/modslots.h" // the cut modules' slots
#include "promptbar.h" // the shared prompt bar (D-MENUS)
#include "ui/presets.h"
#include "press_gate.h" // press ownership between the pad's consumers (the attended run's B leak)
#include "ui/options_page.h"
#include "ui/ui.h"
#include "ui/uiopt.h"
#include "ui/hud_rows.h"
#include "ui/views.h"

#include <deque>
#include <map>
#include <sstream>

#include "controller_internal.h"

namespace ctl
{
static float g_turretAimX = 0, g_turretAimY = 0; // turret view: the look commands' duty accumulators (stick -> share of frames)
// A group's trigger (weapon_groups 2). USER_FIRE_WEAPON_GROUPn is keymap state = keydown with no repeat, and its handler
// (0x4DCC60) fires once per keydown; only USER_FIRE_WEAPONS is polled as held (0x51D115). So a held trigger gave one shot
// a pull (GitHub issue 14). A keydown each frame matches fire-all's per-frame poll; the guns' own refire sets the rate.
static void HoldFireGroup(int cmd, bool want)
{
    const bool was = flightio::Holding(cmd);
    Hold(cmd, want);
    if (want && was)
        fl::FireCommand(cmd, true);
}
// ---------------------------------------------------------------- flight
static float Curve(float v, float expo)
{
    const float a = fabsf(v);
    return (v < 0 ? -1.0f : 1.0f) * powf(a, expo);
}

float& g_lastSteerX = flightio::LastSteerX(), &g_lastSteerY = flightio::LastSteerY();
float LastSteerX() { return g_lastSteerX; }
float LastSteerY() { return g_lastSteerY; }

// ---------------------------------------------------------------- pad aiming
// Right stick -> steering (Freelancer mouse-flight units) and aim cursor (pixels), including the
// aim assist. Shared by the real pad and the "sloppy pilot" test bot, so assists can be measured.
//
// Aim assist (hostile targets only), the three classic controller mechanisms:
//  - slowdown: steering sensitivity drops close to the target's lead cross;
//  - magnetism: the aim cursor (guns fire at it) is pulled onto the lead cross;
//  - rotational assist: while the player is actively steering towards the target, a small
//    capped steering correction closes the remaining error ("small corrections in a dogfight");
//    nothing at rest and nothing when steering away, so the player still has to keep up.
struct PadAimResult
{
    float sx = 0, sy = 0;     // steering
    float aimX = 0, aimY = 0; // aim cursor
};

static PadAimResult PadAimRaw(float rx, float ry);

// Low-pass filter on the aim cursor (frame-to-frame wobble of the lead cross at close range, and the
// jump between screen center and the cross in gimbal mode).
static bool g_aimFilterInit = false;
static float g_aimFX = 0, g_aimFY = 0;

static PadAimResult PadAim(float rx, float ry, float dt)
{
    PadAimResult r = PadAimRaw(rx, ry);
    if (!g_aimFilterInit || g_opt.aimSmooth <= 0)
    {
        g_aimFX = r.aimX;
        g_aimFY = r.aimY;
        g_aimFilterInit = true;
    }
    else
    {
        const float a = 1.0f - expf(-dt / g_opt.aimSmooth);
        g_aimFX += (r.aimX - g_aimFX) * a;
        g_aimFY += (r.aimY - g_aimFY) * a;
    }
    r.aimX = g_aimFX;
    r.aimY = g_aimFY;
    return r;
}

static PadAimResult PadAimRaw(float rx, float ry)
{
    PadAimResult r;
    const float w = static_cast<float>(fl::ScreenWidth()), h = static_cast<float>(fl::ScreenHeight());

    // Steering through Freelancer's own mouse-flight code. The response curve applies to the
    // stick's magnitude so diagonals keep their direction and strength.
    const float mag = sqrtf(rx * rx + ry * ry);
    if (mag > 0)
    {
        const float k = Curve(std::min(mag, 1.0f), g_opt.steerExpo) * g_opt.steerScale / mag;
        r.sx = rx * k;
        r.sy = ry * k;
    }
    if (g_opt.invertY)
        r.sy = -r.sy;

    if (g_opt.aimMode == 1)
    {
        // Gimbal mode: the stick only flies the ship. The aim cursor rests at the screen center and
        // snaps onto a hostile target's lead cross while it is inside the auto-aim cone; the guns'
        // own gimbal does the fine aiming. The player's job is keeping the enemy inside the cone.
        r.aimX = w * 0.5f;
        r.aimY = h * 0.5f;
        const aim::Target t = aim::Current();
        if (t.hostile && t.onScreen && aim::AngleFromCenter(t.screenX, t.screenY) <= g_opt.aimCone)
        {
            r.aimX = t.screenX;
            r.aimY = t.screenY;
        }
        aim::ClampToSafeArea(r.aimX, r.aimY);
        return r;
    }

    // Aim cursor: points where the stick steers, like mouse flight; full deflection reaches the
    // screen edge while the turn rate saturates at steer_scale. Centered at rest.
    r.aimX = w * 0.5f;
    r.aimY = h * 0.5f;
    if (g_opt.aimCursor && g_opt.steerScale > 0)
    {
        // The stick's range is a circle; stretch it to the screen rectangle so diagonals reach the
        // corners (radial stretch r / max(|x|,|y|); direction unchanged).
        float ux = r.sx / g_opt.steerScale, uy = r.sy / g_opt.steerScale;
        const float m = std::max(fabsf(ux), fabsf(uy));
        if (m > 1e-4f)
        {
            const float k = sqrtf(ux * ux + uy * uy) / m;
            ux = Clampf(ux * k, -1.0f, 1.0f);
            uy = Clampf(uy * k, -1.0f, 1.0f);
        }
        r.aimX += ux * w * 0.5f;
        r.aimY -= uy * h * 0.5f;
    }

    if (g_opt.aimAssist > 0 || g_opt.aimRotation > 0)
    {
        const aim::Target t = aim::Current();
        if (t.hostile && t.onScreen)
        {
            const float ex = t.screenX - r.aimX, ey = t.screenY - r.aimY; // cursor -> lead (pixels)
            const float d = sqrtf(ex * ex + ey * ey) / h;

            // Rotational assist: steering correction towards the lead cross, scaled by how actively
            // the player steers, capped to small corrections, none when steering away.
            if (g_opt.aimRotation > 0 && d < g_opt.aimRadius && mag > 0.1f)
            {
                // Steering offset that would move the cursor onto the lead cross.
                float cx = ex / (w * 0.5f) * g_opt.steerScale, cy = -ey / (h * 0.5f) * g_opt.steerScale;
                const float awayDot = r.sx * cx + r.sy * cy;
                const float steer = sqrtf(r.sx * r.sx + r.sy * r.sy);
                if (!(awayDot < 0 && steer > 0.2f))
                {
                    const float input = Clampf(mag / 0.35f, 0.0f, 1.0f);
                    cx *= g_opt.aimRotation * input;
                    cy *= g_opt.aimRotation * input;
                    const float len = sqrtf(cx * cx + cy * cy);
                    if (len > g_opt.aimRotationMax)
                    {
                        cx *= g_opt.aimRotationMax / len;
                        cy *= g_opt.aimRotationMax / len;
                    }
                    r.sx += cx;
                    r.sy += cy;
                }
            }
            // Slowdown only close to the cross, so a wide pull radius does not make the ship
            // sluggish whenever a hostile is on screen.
            if (g_opt.aimAssist > 0 && d < g_opt.aimSlowRadius)
            {
                const float slow = 1.0f - g_opt.aimSlowdown * (1.0f - d / g_opt.aimSlowRadius);
                r.sx *= slow;
                r.sy *= slow;
            }
            // Magnetism: the aim cursor is pulled onto the lead cross.
            if (g_opt.aimAssist > 0 && d < g_opt.aimRadius)
            {
                const float closeness = 1.0f - d / g_opt.aimRadius;
                const float pull = Clampf(closeness * 2.0f * g_opt.aimAssist, 0.0f, 1.0f);
                r.aimX += ex * pull;
                r.aimY += ey * pull;
            }
        }
    }
    aim::ClampToSafeArea(r.aimX, r.aimY);
    return r;
}

// Lead pursuit state (Pursue, below): shared by the test bot and the autopilot.
// ---------------------------------------------------------------- test bot
Bot g_bot;

// Chase limits: targets that flee (e.g. in cruise) are given up instead of chased forever.
constexpr float kBotGiveUpOutOfRange = 8.0f;  // s out of gun range
constexpr float kBotGiveUpDistance = 2500.0f; // m
constexpr float kBotIgnoreTime = 30.0f;       // s before a given-up target may be engaged again

static void UpdateBot(float dt)
{
    g_bot.time += dt;
    const float w = static_cast<float>(fl::ScreenWidth()), h = static_cast<float>(fl::ScreenHeight());
    const aim::Target t = aim::Current();
    const uintptr_t handle = fl::TargetHandle();
    for (auto it = g_bot.ignored.begin(); it != g_bot.ignored.end();)
        it = it->second <= g_bot.time ? g_bot.ignored.erase(it) : std::next(it);
    const bool ignored = handle && g_bot.ignored.count(handle);
    if (!t.valid || !t.hostile || ignored)
    {
        if (g_bot.hadTarget && !ignored)
        {
            ++g_bot.targetsLost;
            events::Push("bot target-lost (%u)", g_bot.targetsLost);
        }
        g_bot.hadTarget = false;
        g_bot.outOfRange = 0;
        Hold(fl::USER_FIRE_WEAPONS, false, inputrules::OwnerBot);
        Hold(fl::USER_AFTERBURN, false, inputrules::OwnerBot);
        Hold(fl::USER_MANEUVER_BRAKE_REVERSE, false, inputrules::OwnerBot);
        SetSteer(0, 0);
        dimouse::MoveTo(w * 0.5f, h * 0.5f);
        g_bot.retarget -= dt;
        if (g_bot.retarget <= 0)
        {
            if (t.valid && !t.hostile) // a friendly or neutral is selected: never shoot it
                Tap(fl::USER_CLEAR_TARGET);
            // Closest enemy first; if that one is being ignored, cycle through the others.
            Tap(ignored ? fl::USER_NEXT_ENEMY : fl::USER_CLOSEST_ENEMY);
            g_bot.retarget = ignored ? 0.15f : 0.5f;
        }
        return;
    }
    if (!g_bot.hadTarget)
        events::Push("bot target-acquired dist=%.0f attitude=%.2f radius=%.0f", t.dist, t.attitude, t.radius);
    g_bot.hadTarget = true;

    g_bot.outOfRange = t.inRange ? 0 : g_bot.outOfRange + dt;
    if (g_bot.outOfRange > kBotGiveUpOutOfRange || t.dist > kBotGiveUpDistance)
    {
        g_bot.ignored[handle] = g_bot.time + kBotIgnoreTime;
        ++g_bot.targetsGivenUp;
        events::Push("bot target-given-up dist=%.0f out-of-range=%.1fs", t.dist, g_bot.outOfRange);
        g_bot.hadTarget = false;
        g_bot.retarget = 0;
        return;
    }

    // Steer like a mouse player: aim cursor on the lead cross, ship turns towards the cursor (Pursue).
    const PursuitOut po = Pursue(t, dt, g_bot.pursuit);
    float sx = po.sx, sy = po.sy, ax = po.ax, ay = po.ay;
    bool aligned = po.aligned;
    if (g_bot.sloppy)
    {
        // "Sloppy pilot": steers through the pad pipeline (curve, cursor, aim assist) like a gentle
        // human - reacts 0.25 s late, under-corrects by 20 %, moves the stick at hand speed, with
        // noise - so the assists can be measured. Fires only when the aim cursor is on the cross.
        float ux = 0, uy = 0; // where the lead cross is, in cursor units (-1..1)
        if (t.onScreen)
        {
            ux = (t.screenX - w * 0.5f) / (w * 0.5f);
            uy = -(t.screenY - h * 0.5f) / (h * 0.5f);
        }
        else if (g_opt.steerScale > 0)
        {
            ux = sx / g_opt.steerScale; // full turn towards the target
            uy = sy / g_opt.steerScale;
        }
        g_bot.history.push_back({g_bot.time, ux, uy});
        while (g_bot.history.size() > 1 && g_bot.history[1].t <= g_bot.time - 0.25f)
            g_bot.history.pop_front();
        const float tx = g_bot.history.front().x * 0.8f, ty = g_bot.history.front().y * 0.8f;
        // Cursor offset -> stick deflection: |u| = curve(|stick|) = |stick|^expo.
        const float um = sqrtf(tx * tx + ty * ty);
        float stx = 0, sty = 0;
        if (um > 1e-4f)
        {
            const float sm = powf(std::min(um, 1.0f), 1.0f / g_opt.steerExpo);
            stx = tx / um * sm;
            sty = ty / um * sm;
        }
        if (g_opt.invertY)
            sty = -sty;
        const float step = 4.0f * dt; // hand speed: full stick in 0.25 s
        g_bot.stickX += Clampf(stx - g_bot.stickX, -step, step);
        g_bot.stickY += Clampf(sty - g_bot.stickY, -step, step);
        const float nx = (rand() / static_cast<float>(RAND_MAX) - 0.5f) * 0.08f;
        const float ny = (rand() / static_cast<float>(RAND_MAX) - 0.5f) * 0.08f;
        const PadAimResult a = PadAim(Clampf(g_bot.stickX + nx, -1, 1), Clampf(g_bot.stickY + ny, -1, 1), dt);
        sx = a.sx;
        sy = a.sy;
        ax = a.aimX;
        ay = a.aimY;
        aligned =
            t.onScreen && sqrtf((ax - t.screenX) * (ax - t.screenX) + (ay - t.screenY) * (ay - t.screenY)) < 0.02f * h;
    }
    SetSteer(sx, sy);
    g_lastSteerX = sx;
    g_lastSteerY = sy;
    dimouse::MoveTo(ax, ay);

    // Fire discipline: only when the cursor sits on the lead cross and the target is in range.
    const bool fire = aligned && t.inRange;
    Hold(fl::USER_FIRE_WEAPONS, fire, inputrules::OwnerBot);
    if (fire)
        g_bot.fireTime += dt;
    if (aligned)
        g_bot.onTargetTime += dt;
    // Range keeping (Pursue): binary throttle, reverse when close / turning sharply, afterburner to close gaps.
    const bool reverse = po.reverse, burn = po.burn;
    Hold(fl::USER_MANEUVER_BRAKE_REVERSE, reverse, inputrules::OwnerBot);
    Hold(fl::USER_AFTERBURN, burn, inputrules::OwnerBot);
    if (!reverse)
        fl::SetThrottle(1.0f);
    if (burn)
        g_bot.afterburnTime += dt;
    if (reverse)
        g_bot.reverseTime += dt;
    g_bot.distTime += t.dist * dt;
    g_bot.engagedTime += dt;
}

static std::string BotStats()
{
    char buf[300];
    sprintf_s(buf,
              "time=%.1f on-target=%.1f firing=%.1f afterburn=%.1f reverse=%.1f avg-dist=%.0f targets-lost=%u "
              "given-up=%u",
              g_bot.time, g_bot.onTargetTime, g_bot.fireTime, g_bot.afterburnTime, g_bot.reverseTime,
              g_bot.engagedTime > 0 ? g_bot.distTime / g_bot.engagedTime : 0.0f, g_bot.targetsLost,
              g_bot.targetsGivenUp);
    return buf;
}

void StopBot(const char* why)
{
    if (!g_bot.on)
        return;
    g_bot.on = false;
    ReleaseAll();
    events::Push("bot stopped (%s) %s", why, BotStats().c_str());
}

std::string BotCommand(std::istream& in)
{
    std::string what;
    in >> what;
    if (what == "fight" || what == "sloppy")
    {
        g_bot = Bot();
        g_bot.on = true;
        g_bot.sloppy = what == "sloppy";
        events::Push("bot started (%s)", what.c_str());
        return "bot " + what;
    }
    if (what == "off")
    {
        StopBot("command");
        return "bot off";
    }
    return std::string("bot ") + (g_bot.on ? "on " : "off ") + BotStats();
}

// ---------------------------------------------------------------- auto targeting (pad flight)
// auto_target 1: when the hostile target is destroyed (or lost), select the closest enemy;
// auto_target 2: also pick one up whenever nothing is targeted. A deliberate selection of a
// non-hostile (station, trade lane, friend) is never replaced; clearing the target with the D-pad
// pauses the automatic pick-up for 10 s.
struct AutoTarget
{
    bool hadHostile = false;
    float afterKill = -1;  // countdown to re-target after the hostile target vanished
    float acquire = 0;     // countdown to the next pick-up attempt with nothing targeted
    float manualPause = 0; // pick-up paused after the player cleared the target
};
static AutoTarget g_auto;

static void UpdateAutoTarget(float dt)
{
    if (g_opt.autoTarget <= 0)
        return;
    g_auto.manualPause -= dt;
    const aim::Target t = aim::Current();
    if (t.valid)
    {
        g_auto.hadHostile = t.hostile;
        g_auto.afterKill = -1;
        return;
    }
    if (g_auto.hadHostile)
    {
        g_auto.hadHostile = false;
        g_auto.afterKill = 0.4f; // let the explosion play
    }
    if (g_auto.afterKill > 0)
    {
        g_auto.afterKill -= dt;
        if (g_auto.afterKill <= 0)
        {
            Tap(fl::USER_CLOSEST_ENEMY);
            events::Push("auto-target after target lost");
        }
        return;
    }
    if (g_opt.autoTarget >= 2 && g_auto.manualPause <= 0)
    {
        g_auto.acquire -= dt;
        if (g_auto.acquire <= 0)
        {
            g_auto.acquire = 1.0f;
            Tap(fl::USER_CLOSEST_ENEMY);
        }
    }
}

// ---------------------------------------------------------------- select by firing
// fire_target: with nothing targeted, firing while the reticle is over a hostile or neutral ship
// selects that ship - through Freelancer's own click selection (one short left click; the cursor
// shape tells what is under the reticle: hostile/fire, neutral/fire_neutral; friendlies excluded).
static int g_selectClick = 0; // 1 = button pressed this frame, release next frame
static float g_selectCooldown = 0;

static void UpdateFireTarget(float dt, bool firing)
{
    g_selectCooldown -= dt;
    if (g_selectClick)
    {
        dimouse::SetButton(0, false);
        g_selectClick = 0;
        return;
    }
    if (!g_opt.fireTarget || !firing || g_selectCooldown > 0 || fl::TargetHandle())
        return;
    const char* c = fl::CursorName();
    if (_stricmp(c, "hostile") && _stricmp(c, "fire") && _stricmp(c, "neutral") && _stricmp(c, "fire_neutral"))
        return;
    dimouse::SetButton(0, true);
    g_selectClick = 1;
    g_selectCooldown = 0.5f;
    events::Push("fire-target select (%s under reticle)", c);
}

// ---------------------------------------------------------------- advanced autopilot
// Gimbal aim without the stick: the reticle locks onto a hostile target's lead cross inside the cone (the same rule
// as aim_mode 1), else rests at the screen centre.
static void GimbalAim(float& x, float& y) { aim::Gimbal(g_opt.aimCone, x, y); }

// The torpedo hold fires what the torpedo hardpoint carries (UI_PLAN 6: "hold torpedo or cruise disruptor"; GitHub issue
// 7). The game launches the two by different commands: USER_LAUNCH_TORPEDOS passes over a disruptor (0x4DD4E0), so a
// ship with a disruptor and no torpedo gets the disruptor's command. A torpedo wins, as on HUD 2's row.
static bool DisruptorOnly()
{
    bool disruptor = false;
    for (const space::Weapon& w : space::PlayerWeapons())
    {
        if (w.cruiseDisruptor)
            disruptor = true;
        else if (_strnicmp(w.hardpoint, "HpTorpedo", 9) == 0)
            return false;
    }
    return disruptor;
}

// weapon_groups 2's triggers: in free flight, and through views::PadFireGroups under the autopilot, whose frame never reaches FlyPlayer (GitHub issue 31)
static void HoldFireGroups()
{
    HoldFireGroup(fl::USER_FIRE_WEAPON_GROUP1, g_dispatch.Held(presets::Action::FireGroup1));
    HoldFireGroup(fl::USER_FIRE_WEAPON_GROUP2, g_dispatch.Held(presets::Action::FireGroup2));
}

static void HoldTorpedoSlot()
{
    const bool slot = g_dispatch.Held(presets::Action::Torpedo);
    const bool disruptor = slot && DisruptorOnly();
    Hold(fl::USER_LAUNCH_TORPEDOS, slot && !disruptor);
    Hold(fl::USER_LAUNCH_CRUISE_DISRUPTORS, disruptor || g_dispatch.Held(presets::Action::CruiseDisruptor));
}

// A stick's vertical value as what it points with: the camera and the cursor, which no Y inversion reaches by itself.
// The stick's steering is flipped by `invert_y` (PadAim) and by the game's own option (its steering code), so both
// count here and the two cancel as they do in flight (GitHub issue 6: turret view ignored Invert Y).
static float LookY(float y) { return g_opt.invertY != fl::GameInvertY() ? -y : y; }

// Turret view's look, the same whether the player or the autopilot flies: the reticle stays at
// the screen centre - gimbal mode's lock-on there as in flight (aim_mode 1: onto a hostile's lead cross inside the cone)
// - and the right stick turns the camera itself through the game's own look commands (USER_LOOK_ROTATE_CAMERA_*, held =
// the turret camera turns at its rate; 0x51A7D0 turns it by the cursor only while mouse flight is engaged, else by
// these - 0x51AC20 sets its yaw / pitch input). The commands are on / off: the stick's deflection is the share of frames
// they are held (an accumulator per axis), the camera's own smoothing (0x51AA47) evens it out. Out of turret view the
// look commands are let go (no-ops when not held). True = turret view (the cursor is placed). The steering is the
// caller's (the player's flight lets it go, the autopilot keeps steering).
static bool TurretLook(const PadState& p)
{
    static const int kLook[] = {fl::USER_LOOK_ROTATE_CAMERA_LEFT, fl::USER_LOOK_ROTATE_CAMERA_RIGHT,
                                fl::USER_LOOK_ROTATE_CAMERA_UP, fl::USER_LOOK_ROTATE_CAMERA_DOWN};
    if (!g_turret)
    {
        for (int c : kLook)
            Hold(c, false);
        return false;
    }
    auto pulse = [](float& acc, float v)
    {
        acc += fabsf(v);
        if (acc < 1)
            return false;
        acc -= 1;
        return true;
    };
    const bool onX = pulse(g_turretAimX, p.rx), onY = pulse(g_turretAimY, p.ry);
    if (p.rx == 0)
        g_turretAimX = 0;
    if (p.ry == 0)
        g_turretAimY = 0;
    Hold(fl::USER_LOOK_ROTATE_CAMERA_LEFT, onX && p.rx < 0);
    Hold(fl::USER_LOOK_ROTATE_CAMERA_RIGHT, onX && p.rx > 0);
    const float lookY = LookY(p.ry);
    Hold(fl::USER_LOOK_ROTATE_CAMERA_UP, onY && lookY > 0);
    Hold(fl::USER_LOOK_ROTATE_CAMERA_DOWN, onY && lookY < 0);
    float x = fl::ScreenWidth() * 0.5f, y = fl::ScreenHeight() * 0.5f;
    if (g_opt.aimMode == 1)
        GimbalAim(x, y);
    dimouse::MoveTo(x, y);
    return true;
}

// ---------------------------------------------------------------- pad flight: the preset runner
// The pad's buttons in glyphs::Id order, as the dispatcher wants them. The triggers count as buttons past the pad's
// trigger threshold (PAD_LT / PAD_RT); the sticks are axes, never buttons.
static void PadButtons(bool down[glyphs::COUNT])
{
    memset(down, 0, glyphs::COUNT);
    down[glyphs::A] = g_pad.Down(XINPUT_GAMEPAD_A);
    down[glyphs::B] = g_pad.Down(XINPUT_GAMEPAD_B);
    down[glyphs::X] = g_pad.Down(XINPUT_GAMEPAD_X);
    down[glyphs::Y] = g_pad.Down(XINPUT_GAMEPAD_Y);
    down[glyphs::LB] = g_pad.Down(XINPUT_GAMEPAD_LEFT_SHOULDER);
    down[glyphs::RB] = g_pad.Down(XINPUT_GAMEPAD_RIGHT_SHOULDER);
    down[glyphs::LT] = g_pad.Down(PAD_LT);
    down[glyphs::RT] = g_pad.Down(PAD_RT);
    down[glyphs::L3] = g_pad.Down(XINPUT_GAMEPAD_LEFT_THUMB);
    down[glyphs::R3] = g_pad.Down(XINPUT_GAMEPAD_RIGHT_THUMB);
    down[glyphs::DU] = g_pad.Down(XINPUT_GAMEPAD_DPAD_UP);
    down[glyphs::DD] = g_pad.Down(XINPUT_GAMEPAD_DPAD_DOWN);
    down[glyphs::DL] = g_pad.Down(XINPUT_GAMEPAD_DPAD_LEFT);
    down[glyphs::DR] = g_pad.Down(XINPUT_GAMEPAD_DPAD_RIGHT);
    down[glyphs::VIEW] = g_pad.Down(XINPUT_GAMEPAD_BACK);
    down[glyphs::MENU] = g_pad.Down(XINPUT_GAMEPAD_START);
}

// The press gates' inputs: the buttons, and the sticks as "down" while deflected (gamepad.cpp's dead zone gives 0).
// `pad`: g_pad (positional: flight), or g_menuPad (the menus' view: A / B as accept / back).
void PadInputs(bool in[glyphs::COUNT], const Gamepad& pad)
{
    PadButtons(in);
    in[glyphs::A] = pad.Down(XINPUT_GAMEPAD_A);
    in[glyphs::B] = pad.Down(XINPUT_GAMEPAD_B);
    const PadState& p = g_pad.Cur();
    in[glyphs::LS] = p.lx != 0 || p.ly != 0;
    in[glyphs::RS] = p.rx != 0 || p.ry != 0;
}

// The stick an axis action sits on: +1 right stick, -1 left stick, 0 unbound.
static int AxisStick(const presets::Table& t, presets::Action a)
{
    if (t.b[glyphs::RS].tap == a)
        return 1;
    if (t.b[glyphs::LS].tap == a)
        return -1;
    return 0;
}

// The phase D flight modals closed (a mode change, the device switching away, shutdown): the picker keeps the target it
// previewed (the game's own selection stands; nothing is restored behind the player's back).
void CloseModals()
{
    picker::Close(true);
    wheel::Cancel();
    views::PanelClose();
    wgroups::Close();
}

static void ToggleAutopilot(const char* why)
{
    if (views::ApActive())
        views::ApStop(why);
    else
    {
        views::ApStartSmart(); // the plain start of every device (objective, waypoints, target)
        g_apUsed = true;
    }
}

void MarkAutopilotUsed() { g_apUsed = true; }
static bool g_rsWheel = false; // the right stick is the action wheel's: while it is open, then until the stick is let go (the frame below)

// The pad as the autopilot's flight meets it (core\ui\views.h PadFlightView).
static void ViewSticks(inputrules::Sticks& s)
{
    PadState p = g_pad.Cur();
    if (g_gateFlight.Hidden(glyphs::LS)) // a stick still held from FLUF's cursor / a popup is not the player's steering
        p.lx = p.ly = 0;
    if (g_gateFlight.Hidden(glyphs::RS) || g_rsWheel) // (the action wheel's stick, until it is let go: never the autopilot's aim or take-back)
        p.rx = p.ry = 0;
    // the right stick where it is the aim: turret view, ASSISTED (UI_PLAN 6.4)
    s = {p.lx, p.ly, p.rx, p.ry, g_turret || uiopt::Get().padPreset == presets::ASSISTED};
}

static bool ViewHeld(inputrules::Channel c)
{
    using inputrules::Channel;
    switch (c)
    {
    case Channel::Fire: return g_dispatch.Held(presets::Action::Fire);
    case Channel::Missile: return g_dispatch.Held(presets::Action::Missile);
    case Channel::Afterburner: return g_dispatch.Held(presets::Action::Afterburner);
    case Channel::Reverse: return g_dispatch.Held(presets::Action::Reverse);
    case Channel::Rear: return g_dispatch.Held(presets::Action::Rear) || wheel::RearView();
    default: return false;
    }
}

static bool ViewTurretLook(float rx, float ry)
{
    PadState p = {};
    p.rx = rx, p.ry = ry;
    return TurretLook(p);
}

static void ViewRaw(float out[6], u32* buttons)
{
    const PadState& p = g_pad.Cur();
    out[0] = p.lx, out[1] = p.ly, out[2] = p.rx, out[3] = p.ry, out[4] = p.lt, out[5] = p.rt;
    *buttons = p.buttons;
}

void PublishPadFlight() { views::padflight = {ViewSticks, ViewHeld, HoldTorpedoSlot, HoldFireGroups, ViewTurretLook, MarkAutopilotUsed, ViewRaw, device::LastPadTick, device::LastKbmTick}; }
void WithdrawPadFlight() { views::padflight = {}; }

float StickDeadzone() { return g_pad.stickDeadzone; }
int PadBackend() { return g_pad.padBackend == 1 || g_pad.HandsOnFallback() ? 1 : g_pad.padBackend; }
void PadForceCandidate(int answer) { g_pad.ForceCandidate(answer); }
void PadTestRumble() { g_pad.Rumble(0.5f, 0.5f, 0.2f); }
bool FocusOnly() { return g_opt.focusOnly; }

// `context` (A): whatever applies to the target - dock / lane / jump when it is dockable (the game's dock maneuver,
// a toggle: a second press cancels a running dock), tractor ONE item when it is loot in the tractor's reach, with no
// target the nearest loot in reach (targeted, then tractored: the game's "Tractor Beam (Selected)"), else fly to it
// (goto; loot out of reach too). X tractors everything in reach (USER_COLLECT_LOOT). The HUDs name the item and its
// range ("Prompting should clarify which prompt does what (tractor specific, tractor all, etc)";
// until then A and X both tractored everything). The ASSISTED preset hands the whole thing to the autopilot.
static void ContextAction()
{
    if (uiopt::Get().padPreset == presets::ASSISTED)
    {
        if (!views::ApActive())
            ToggleAutopilot("pad");
        return;
    }
    const u32 tid = fl::TargetId();
    ai::Self self;
    const bool haveSelf = ai::ReadSelf(self);
    const float reach = haveSelf ? self.tractorRange : 0;
    bool targetLoot = false, targetInReach = false, dockable = false;
    u32 nearestLoot = 0;
    float nearestD = 0;
    for (const space::Contact& c : space::Contacts())
    {
        // (in reach = the tractor can take it: in range and the hold has room for it)
        const bool loot = (c.type & space::kLoot) != 0, inReach = loot && reach > 0 && c.distance <= reach && c.lootFits != 0;
        if (c.id == tid && tid)
        {
            dockable = c.dock || (c.type & (space::kTradelaneRing | space::kJumpGate | space::kJumpHole)) != 0;
            targetLoot = loot, targetInReach = inReach;
        }
        if (inReach && (!nearestLoot || c.distance < nearestD))
            nearestLoot = c.id, nearestD = c.distance;
    }
    if (tid && dockable)
    {
        coreframe::LockAnswer("dock");
        Tap(fl::USER_MANEUVER_DOCK);
    }
    else if (targetLoot && targetInReach)
        Tap(fl::USER_TRACTOR_BEAM);
    else if (!tid && nearestLoot)
    {
        const bool set = space::SetTarget(nearestLoot);
        Tap(set ? fl::USER_TRACTOR_BEAM : fl::USER_COLLECT_LOOT);
        events::Push("pad context: tractor the nearest loot %u (%.0f m)%s", nearestLoot, nearestD,
                     set ? "" : ": target refused, all loot instead");
    }
    else if (tid)
    {
        coreframe::LockAnswer("goto");
        Tap(fl::USER_MANEUVER_GOTO);
    }
    else
        events::Push("pad context: nothing applies (no target, no loot in reach)");
}

// `target_smart` (RS tap). Since 7 Nov 2026 a cold tap takes what the reticle is on first (RingPick, below); then, 4 Nov 2026: enemies ("right stick should default to target nearest enemy
// for a first press, and only consecutive presses should be 'next target'") - with a hostile ship within
// tcycle::kReach, a cold press takes the nearest, presses in a run the next ones (target_cycle.h), set natively
// (space::SetTarget, filter-independent: the game's NEXT / CLOSEST ENEMY walk the contact list under its filter). With
// no enemy near, "TARGET AHEAD": the object nearest the screen centre inside HUD 2's reticle ring (the cone, the owner,
// 26 Oct 2026) - ships, loot, bases, rings, anything targetable; again = the next one ahead (the current target is
// skipped). Nothing in the ring: the game's nearest / next enemy. The double tap (clear) is the preset's.
// (Was: the game's click selection when the cursor's shape named an object exactly under the reticle - in calm flight
// that almost never held and the fallback found no enemy: "target ahead just doesn't work at all", the owner, 26 Oct
// 2026; then 26 Oct - 4 Nov the ring first, enemies only when the ring was empty.)

// Reticle first (the owner, 7 Nov 2026, over 4 Nov's enemies first: with an enemy near, a tap never took what the player looked at):
// a cold tap takes the object nearest the screen centre inside the ring; the taps that follow it in a run walk the enemies as above.
static u32 RingPick(u32 cur, float& dist, float& r)
{
    const float cx = fl::ScreenWidth() * 0.5f, cy = fl::ScreenHeight() * 0.5f;
    r = views::HudRingRadius();
    u32 best = 0;
    float bestD = r * r;
    for (const space::Contact& c : space::Contacts())
    {
        if (!c.id || c.id == cur || c.health == 0 || !(c.targetable || c.ship || c.dock || (c.type & space::kLoot)))
            continue;
        float s[3];
        if (!fl::WorldToScreen(c.pos, s) || s[2] <= 0)
            continue;
        const float d = (s[0] - cx) * (s[0] - cx) + (s[1] - cy) * (s[1] - cy);
        if (d < bestD)
            bestD = d, best = c.id;
    }
    dist = sqrtf(bestD);
    return best;
}

static void SmartTarget()
{
    static tcycle::Chain chain;
    const u32 cur = fl::TargetId();
    const float now = static_cast<float>(GetTickCount()) * 0.001f;
    if (chain.Cold(now, cur))
    {
        float d = 0, r = 0;
        const u32 ahead = RingPick(cur, d, r);
        if (ahead && space::SetTarget(ahead))
        {
            chain.Took(now, ahead);
            events::Push("pad target_smart: ahead %u (%.0f px from the centre, ring %.0f)", ahead, d, r);
            return;
        }
    }
    {
        tcycle::Enemy e[64];
        int n = 0;
        for (const space::Contact& c : space::Contacts())
            if (n < 64 && aim::NearbyEnemy(c))
                e[n++] = {c.id, c.distance};
        if (n > 0)
        {
            const u32 pick = chain.Press(now, e, n, cur);
            if (!pick)
                events::Push("pad target_smart: the only enemy near is the target");
            else if (space::SetTarget(pick))
                events::Push("pad target_smart: enemy %u (%d near)", pick, n);
            else
            {
                chain.Refused(cur);
                events::Push("pad target_smart: enemy %u refused", pick);
            }
            return;
        }
    }
    // no enemy near, in a run: the next object ahead (the current one skipped)
    float d = 0, r = 0;
    if (const u32 best = RingPick(cur, d, r); best && space::SetTarget(best))
    {
        chain.Took(now, best);
        events::Push("pad target_smart: ahead %u (%.0f px from the centre, ring %.0f)", best, d, r);
        return;
    }
    const bool had = fl::TargetHandle() != 0;
    Tap(had ? fl::USER_NEXT_ENEMY : fl::USER_CLOSEST_ENEMY);
    events::Push("pad target_smart: %s", had ? "next enemy" : "closest enemy");
}

// One dispatched action in the game (the primitives: Tap / Hold with fl::USER_*, the autopilot API, the modals of
// phase D as events for now). Held-kind actions are read by the frame below through g_dispatch.Held().
static void Apply(const presets::Event& e)
{
    using presets::Action;
    using presets::Phase;
    const bool start = e.phase == Phase::Tap || e.phase == Phase::Dbl || e.phase == Phase::HoldStart;
    // hud_prompts (0.2.1): the context lanes show after a player action (core\game\promptshow.h)
    if (start)
        switch (e.action)
        {
        case Action::Context: case Action::FreeFlight: case Action::Formation: case Action::Goto:
        case Action::Autopilot: case Action::Handback: views::HudNoteAction(hudwake::Maneuver); break;
        case Action::EngineKill: case Action::Cruise: views::HudNoteAction(hudwake::Cruise); break;
        case Action::TargetSmart: case Action::TargetNearest: case Action::NextEnemy: case Action::ClearTarget:
            views::HudNoteAction(hudwake::TargetChg);
            break;
        case Action::Tractor: views::HudNoteAction(hudwake::Fire); break;
        default: break;
        }
    switch (e.action)
    {
    case Action::Context:
        if (start)
            ContextAction();
        break;
    case Action::FreeFlight: // the cancel button: free flight, and the cruise dropped (the game's free flight alone
        if (start)           // leaves the cruise engine engaged - verified 25 Oct 2026)
        {
            coreframe::LockAnswer("free flight");
            Tap(fl::USER_MANEUVER_FREE_FLIGHT);
            flight::Model fm;
            if (flight::ReadModel(fm) && fm.cruiseEngaged)
                Tap(fl::USER_CRUISE);
        }
        break;
    case Action::Tractor:
        if (start)
            Tap(fl::USER_COLLECT_LOOT);
        break;
    case Action::Formation:
        if (start)
        {
            coreframe::LockAnswer("formation");
            Tap(fl::USER_MANEUVER_FORMATION);
        }
        break;
    case Action::Goto:
        if (start)
        {
            coreframe::LockAnswer("goto");
            Tap(fl::USER_MANEUVER_GOTO);
        }
        break;
    case Action::EngineKill:
        if (start)
            Tap(fl::USER_MANEUVER_ENGINEKILL);
        break;
    case Action::Cruise:
        if (start)
            Tap(fl::USER_CRUISE);
        break;
    case Action::Scan:
        if (start)
            Tap(fl::USER_SCAN_CARGO);
        break;
    case Action::TargetSmart:
        if (start)
            SmartTarget();
        break;
    case Action::TargetNearest:
        if (start)
            Tap(fl::USER_CLOSEST_ENEMY);
        break;
    case Action::NextEnemy:
        if (start)
            Tap(fl::USER_NEXT_ENEMY);
        break;
    case Action::ClearTarget:
        if (start)
        {
            Tap(fl::USER_CLEAR_TARGET);
            g_auto.manualPause = 10.0f;
            g_auto.hadHostile = false;
        }
        break;
    case Action::Autopilot:
        if (start)
            ToggleAutopilot("pad");
        break;
    case Action::Handback:
        if (start)
        {
            if (views::ApActive())
                views::ApStop("pad");
            else
            {
                coreframe::LockAnswer("free flight");
                Tap(fl::USER_MANEUVER_FREE_FLIGHT); // nothing to hand back: the cancel button's own meaning
            }
        }
        break;
    case Action::Hub:
        if (start)
            Tap(fl::USER_NAV_MAP); // the NeuroNet hub opens map first (its pad screen is phase D)
        break;
    case Action::Pause:
        if (start)
            Tap(fl::USER_PAUSE);
        break;
    case Action::Nanobot:
        if (start)
            Tap(fl::USER_REPAIR_HEALTH);
        break;
    case Action::Battery:
        if (start)
            Tap(fl::USER_REPAIR_SHIELD);
        break;
    case Action::Cm:
        if (start)
            Tap(fl::USER_LAUNCH_COUNTERMEASURES);
        break;
    case Action::Mine:
        if (start)
            Tap(fl::USER_LAUNCH_MINES);
        break;
    case Action::WeaponGroup: // weapon_groups 1: Y flips to the next of the three HUD groups that have guns
        if (start)
        {
            // (from the game's active group, not a count of our own: a load or a key may have changed it)
            const int active = hudinfo::ActiveWeaponGroup();
            const int next = wgroups::NextGroup(wgroups::GroupsWithGuns(), active >= 1 ? active : g_weaponGroup);
            if (next == active)
            {
                events::Push("pad weapon group: no other group has guns");
                break;
            }
            g_weaponGroup = next;
            Tap(static_cast<fl::Cmd>(fl::USER_WEAPON_GROUP1 + g_weaponGroup - 1));
            events::Push("pad weapon group %d", g_weaponGroup);
        }
        break;
    case Action::Turret: // the game's turret view (USER_AUTO_TURRET, H): each hold toggles it, it stays after the release
        if (e.phase == Phase::HoldStart) // (was on at the hold, off at the release: the owner, 26 Oct 2026, wants it to stay)
        {
            // the game's camera says whether it is on (OnFrame's sync, exe::CamTurret); without the symbol: our own flag
            if (!exe::CamTurret.ok())
                g_turret = !g_turret;
            g_turretAimX = g_turretAimY = 0;
            for (int c : {fl::USER_LOOK_ROTATE_CAMERA_LEFT, fl::USER_LOOK_ROTATE_CAMERA_RIGHT,
                          fl::USER_LOOK_ROTATE_CAMERA_UP, fl::USER_LOOK_ROTATE_CAMERA_DOWN})
            {
                fl::SetHeld(c, false); // the turret camera starts (and ends) without a turn input
                flightio::DropHold(c);
                fl::FireCommand(c, false);
            }
            Tap(fl::USER_AUTO_TURRET);
            events::Push("pad turret view toggle (was %s)", g_turret ? "on" : "off");
        }
        break;
    // phase D modals (picker.*, wheel.*, goals.*): LB tap the target picker, LB held the action wheel (its release does
    // the picked action; what needs this file's state - the weapon group - comes back as an action to apply), X the goal
    // panel while the autopilot flies
    case Action::TargetPicker:
        if (start)
            picker::Open();
        break;
    case Action::ActionWheel:
        if (e.phase == Phase::HoldStart)
            wheel::Open();
        else if (e.phase == Phase::HoldEnd)
        {
            // a press lost to an unplugged pad or a lost focus is no choice: only a release picks
            if (g_pad.ReadLost())
            {
                wheel::Cancel();
                break;
            }
            const Action a = wheel::Commit();
            if (a != Action::None)
                Apply({e.btn, a, Phase::Tap});
        }
        break;
    case Action::GoalPanel:
        if (start)
            views::PanelToggle();
        break;
    case Action::Accept:
        if (start)
            events::Push("ui accept (phase D: no suggestion to accept yet)");
        break;
    default: break; // held-kind actions (fire, missile, torpedo, afterburner, reverse, rear, cruise disruptor, groups)
    }
}

// The frame's flight, around the autopilot's slot: the pad is read first, whoever flies (a pad stick hands back a keyboard start, and the
// modals and Apply run in the autopilot's flight too); the slot; then the player's flight when the slot did not fly. Who gets here:
// input_rules.h WhoHasTheFrame.
struct PadFrame
{
    PadState p;              // the sticks and triggers as the press gate leaves them
    bool aceBurn = false, aceReverse = false; // ACE's triggers at full pull
};

// The latches follow the game (presets::LatchEnded). Reverse needs none: the game keeps it through engine kill, a refused cruise and the
// hand-back (measured); drift is the game's own toggle. Whoever flies: a latched boost under the autopilot runs out the same way.
static void FollowTheGame(float dt)
{
    static presets::LatchWatch burn, rear, wheelRear;
    using A = presets::Action;
    const bool rearOn = fl::RearViewOn(), rearKnown = exe::PlayerFlightState.ok(); // (a state not read leaves the latch as it was)
    // (the key goes up at once: a press in this same frame then reaches the game as a new press)
    if (hudinfo::AfterburnerKnown() && presets::LatchEnded(burn, g_dispatch.Latched(A::Afterburner), hudinfo::AfterburnerOn(), dt))
    {
        g_dispatch.Unlatch(A::Afterburner);
        Hold(fl::USER_AFTERBURN, false);
        events::Push("pad latch: the game ended the afterburner");
    }
    if (rearKnown && presets::LatchEnded(rear, g_dispatch.Latched(A::Rear), rearOn, dt))
    {
        g_dispatch.Unlatch(A::Rear);
        Hold(fl::USER_REARVIEW_CAMERA_MODE, false);
        events::Push("pad latch: the game ended the rear view");
    }
    if (rearKnown && presets::LatchEnded(wheelRear, wheel::RearView(), rearOn, dt))
    {
        wheel::DropRearView();
        Hold(fl::USER_REARVIEW_CAMERA_MODE, false);
    }
}

static void ReadPadFrame(float dt, PadFrame& f)
{
    // the keyboard or mouse took the device while the autopilot flies: no device row runs then, and a modal nothing can close would stay
    if (device::Switched() && device::KbMouse())
    {
        picker::Close(true);
        wheel::Cancel();
        wgroups::Close();
    }
    const uiopt::UiOptions& o = uiopt::Get();
    const int preset = o.padPreset == presets::CUSTOM ? presets::FREELANCER : o.padPreset;
    g_dispatch.toggles = o.holdsAsToggles;
    FollowTheGame(dt);
    // The situation's table: the preset's flight table, or the shared flight_ap one while the autopilot flies.
    const bool apFlying = views::ApActive();
    g_table = apFlying ? presets::EffectiveAp(preset, o.weaponGroups) : presets::Effective(preset, o.weaponGroups, o.swapY);
    // Press ownership: what another consumer had when it went down (the B that closed FLUF's window, the A that closed a
    // popup, the stick that moved FLUF's cursor) stays hidden from the flight dispatch until released; a hold of the
    // dispatcher's own interrupted by another consumer is cancelled (no tap on its release).
    bool down[glyphs::COUNT], fresh[glyphs::COUNT];
    PadInputs(down);
    g_gateFlight.Take(down, fresh);
    for (int i = 0; i < glyphs::COUNT; ++i)
        if (fresh[i])
            g_dispatch.Cancel(static_cast<glyphs::Id>(i));
    PadState& p = f.p;
    p = g_pad.Cur();
    if (g_gateFlight.Hidden(glyphs::LS))
        p.lx = p.ly = 0;
    if (g_gateFlight.Hidden(glyphs::RS))
        p.rx = p.ry = 0;
    if (g_gateFlight.Hidden(glyphs::LT))
        p.lt = 0;
    if (g_gateFlight.Hidden(glyphs::RT))
        p.rt = 0;
    down[glyphs::LS] = down[glyphs::RS] = false; // the sticks are axes, not the dispatcher's buttons
    // The phase D modals (picker / wheel / goal panel) read the buttons they own this frame; those are then hidden from
    // the flight dispatch until released (a B that closed the picker is not also free flight). The sticks and the
    // triggers stay live: the left stick flies, RT fires, the wheel owns the right stick (its steering is zeroed below).
    {
        static bool swallow[glyphs::COUNT] = {};
        bool owned[glyphs::COUNT];
        for (int i = 0; i < glyphs::COUNT; ++i)
        {
            const glyphs::Id id = static_cast<glyphs::Id>(i);
            owned[i] = picker::OwnsButton(id) || wheel::OwnsButton(id) || views::PanelOwnsButton(id) || wgroups::OwnsButton(id);
        }
        // the modals are menus: A / B as accept / back (confirm_button; both are owned alike, so the swallow below
        // stays positional and right)
        bool menu[glyphs::COUNT];
        memcpy(menu, down, sizeof(menu));
        menu[glyphs::A] = down[glyphs::Menu(glyphs::A)], menu[glyphs::B] = down[glyphs::Menu(glyphs::B)];
        picker::Update(dt, menu);
        wheel::Update(dt, menu, p.rx, p.ry);
        // the wheel owns the right stick while it is open, and after it closes until the stick is let go (Shockwave, 7 Nov 2026: with the
        // autopilot flying, the wheel's pick drove the crosshair as the autopilot's aim - left to the left edge, the autopilot's own aim
        // pulling it back - and a stick still held at the release took the ship back): nothing in flight sees it then
        if (wheel::IsOpen())
            g_rsWheel = true;
        else if (p.rx == 0 && p.ry == 0)
            g_rsWheel = false;
        if (g_rsWheel)
            p.rx = p.ry = 0;
        views::PanelUpdate(dt, menu);
        wgroups::Update(dt, menu);
        for (int i = 0; i < glyphs::COUNT; ++i)
        {
            if (down[i] && owned[i])
                swallow[i] = true;
            if (!down[i])
                swallow[i] = false;
            if (swallow[i])
                down[i] = false;
        }
    }
    // ACE: the triggers are the throttle lever (RT up, afterburner at full pull; LT down, reverse at full pull): axes,
    // not buttons
    const bool aceTriggers = g_table.b[glyphs::RT].tap == presets::Action::Throttle;
    if (aceTriggers)
    {
        down[glyphs::RT] = down[glyphs::LT] = false;
        if (p.rt > 0 || p.lt > 0)
            fl::SetThrottle(Clampf(fl::GetThrottle() + (p.rt - p.lt) * g_opt.throttleRate * dt, 0.0f, 1.0f));
        f.aceBurn = p.rt > 0.95f;
        f.aceReverse = p.lt > 0.95f;
    }
    static std::vector<presets::Event> events;
    events.clear();
    g_dispatch.Update(dt, g_table, down, events);
    for (const presets::Event& e : events)
    {
        if (e.action != presets::Action::Fire && e.action != presets::Action::Missile)
            Log("pad %s %s %s", glyphs::Name(e.btn), presets::ActionName(e.action),
                e.phase == presets::Phase::Tap         ? "tap"
                : e.phase == presets::Phase::Dbl       ? "dbl"
                : e.phase == presets::Phase::HoldStart ? "hold"
                                                       : "release");
        Apply(e);
    }
}

static void FlyPlayer(float dt, const PadFrame& f)
{
    const PadState& p = f.p;
    UpdateAutoTarget(dt);
    // Steering: the stick the preset steers with (RS in FREELANCER, LS in ACE; none in ASSISTED). In turret view the
    // right stick aims the camera's reticle instead (the ship flies straight on).
    const int steerStick = AxisStick(g_table, presets::Action::Steer);
    const int aimStick = AxisStick(g_table, presets::Action::Aim);
    const bool rsOwned = wheel::IsOpen(); // the open action wheel owns the right stick (it picks the slot)
    const float srx = steerStick > 0 ? (rsOwned ? 0.0f : p.rx) : steerStick < 0 ? p.lx : 0.0f;
    const float sry = steerStick > 0 ? (rsOwned ? 0.0f : p.ry) : steerStick < 0 ? p.ly : 0.0f;
    if (TurretLook(p)) // turret view: the camera turns, the reticle stays centred, the ship flies straight on
    {
        ReleaseSteer();
        g_lastSteerX = g_lastSteerY = 0;
    }
    else if (steerStick == 0)
    {
        ReleaseSteer();
        const float ax = aimStick < 0 ? p.lx : (aimStick > 0 || g_turret) ? p.rx : 0.0f;
        const float ay = aimStick < 0 ? p.ly : (aimStick > 0 || g_turret) ? p.ry : 0.0f;
        float x = fl::ScreenWidth() * 0.5f + ax * fl::ScreenWidth() * 0.5f;
        float y = fl::ScreenHeight() * 0.5f - LookY(ay) * fl::ScreenHeight() * 0.5f;
        aim::ClampToSafeArea(x, y);
        dimouse::MoveTo(x, y);
        g_lastSteerX = g_lastSteerY = 0;
    }
    else
    {
        const PadAimResult a = PadAim(srx, sry, dt);
        SetSteer(a.sx, a.sy);
        g_lastSteerX = a.sx;
        g_lastSteerY = a.sy;
        dimouse::MoveTo(a.aimX, a.aimY);
    }

    // The throttle lever on a stick's vertical axis (FREELANCER: LS), its horizontal axis the strafe.
    const int throttleStick = AxisStick(g_table, presets::Action::Throttle);
    const int strafeStick = AxisStick(g_table, presets::Action::StrafeUd);
    float strafeX = 0, strafeY = 0;
    if (throttleStick != 0)
    {
        const float ty = throttleStick > 0 ? p.ry : p.ly;
        if (ty != 0)
            fl::SetThrottle(Clampf(fl::GetThrottle() + ty * g_opt.throttleRate * dt, 0.0f, 1.0f));
        strafeX = throttleStick > 0 ? p.rx : p.lx;
    }
    if (strafeStick != 0)
    {
        strafeX = strafeStick > 0 ? p.rx : p.lx;
        strafeY = strafeStick > 0 ? p.ry : p.ly;
    }
    Hold(fl::USER_MANEUVER_SLIDE_EVADE_LEFT, strafeX < -0.6f);
    Hold(fl::USER_MANEUVER_SLIDE_EVADE_RIGHT, strafeX > 0.6f);
    Hold(fl::USER_MANEUVER_SLIDE_EVADE_UP, strafeY > 0.6f);
    Hold(fl::USER_MANEUVER_SLIDE_EVADE_DOWN, strafeY < -0.6f);

    // Held-kind actions as commands held
    const bool fire = g_dispatch.Held(presets::Action::Fire);
    Hold(fl::USER_FIRE_WEAPONS, fire);
    UpdateFireTarget(dt, fire || g_dispatch.Held(presets::Action::FireGroup1) ||
                             g_dispatch.Held(presets::Action::FireGroup2));
    HoldFireGroups();
    Hold(fl::USER_LAUNCH_MISSILES, g_dispatch.Held(presets::Action::Missile));
    HoldTorpedoSlot();
    Hold(fl::USER_AFTERBURN, g_dispatch.Held(presets::Action::Afterburner) || f.aceBurn);
    Hold(fl::USER_MANEUVER_BRAKE_REVERSE, g_dispatch.Held(presets::Action::Reverse) || f.aceReverse);
    Hold(fl::USER_REARVIEW_CAMERA_MODE, g_dispatch.Held(presets::Action::Rear) || wheel::RearView());
}

void UpdateSpace(float dt)
{
    if (g_bot.on)
    {
        UpdateBot(dt);
        return;
    }
    PadFrame f;
    ReadPadFrame(dt, f);
    if (views::ApFly(dt))
        return;
    FlyPlayer(dt, f);
}
} // namespace ctl
