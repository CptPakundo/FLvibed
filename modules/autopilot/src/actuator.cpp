// The autopilot's actuator: applies what autopilot.cpp decides. Each frame it flies, in the frame's slot (core\ui\views.h Fly): the steering, the
// cursor, the throttle, the held commands and the taps, all through core\game\flightio.h, merged with the pad by core\game\input_rules.h. The
// pad is a view here: without the Controller module nothing merges and no stick hands back.
#include "autopilot.h"

#include "dimouse.h"
#include "game/aim.h"
#include "game/fl.h"
#include "game/modslots.h"
#include "game/flight.h"
#include "game/flightio.h"
#include "game/gameclock.h"
#include "game/geom.h"
#include "game/hostcalls.h"
#include "game/pursuit.h"
#include "game/events.h"
#include "game/space.h"
#include "ui/views.h"

namespace autopilot
{
using flightio::Claim;
using flightio::Hold;
using flightio::ReleaseAll;
using flightio::ReleaseSteer;
using flightio::SetSteer;
using flightio::Tap;
using inputrules::Channel;
using leadchase::Pursue;
using leadchase::Pursuit;
using leadchase::PursuitOut;

// The cursor is the guns' aim and the mouse flight's centre; under a window it is the player's (the guns then take a world point, below).
static void AimCursor(float x, float y)
{
    if (!flightio::UnderWindow())
        dimouse::MoveTo(x, y);
}

bool Fly(float dt)
{
    modslots::FaultPoint(modslots::kAutopilot, 1);
    // When it stops, everything it held goes back first (was: pad flight re-holds only its own commands, so a slide
    // up / down or a missile hold of the last autopilot frame stayed held).
    static bool flew = false, wasFiring = false;
    const auto handBack = [&]
    {
        if (flew)
        {
            ReleaseAll(); // (the hand-back pulse: haptics.cpp, on the autopilot's end in flight)
            if (hostcalls::g.SetGunAim)
                hostcalls::g.SetGunAim(false, nullptr); // the guns back to the player's cursor
        }
        flew = wasFiring = false;
        return false;
    };
    if (!Active())
    {
        TracePlayer(dt); // (the player's own flight in the same trace: the reference runs)
        return handBack();
    }
    inputrules::Sticks p;
    views::PadSticks(p); // as the press gate leaves them; none without the pad
    // The player's own steering takes the ship back at once (never re-issued by the autopilot): any stick past
    // autopilot_cancel - except the right stick in turret view or the ASSISTED preset, where it is the aim (UI_PLAN 6.4).
    const bool rightAims = p.rightAims;
    if (inputrules::TakesBack(p, flightio::Get().autopilotCancel))
    {
        Stop("player input");
        return handBack();
    }
    Out o;
    Update(dt, o);
    if (!o.active)
        return handBack();
    flew = true;
    static Pursuit pursuit;
    static float fireHold = 0; // keeps the guns going through a one-frame loss of alignment
    bool fire = o.fire, reverse = false;
    // The coast (`big_coast`; "Killing the engine when in cruise engine only works when NO
    // additional engine-related input is sent ... no stopping, no throttle action. Just engine kill"): the steering
    // alone - no throttle, reverse, afterburner, slide or cruise toggle while it lasts (all released below)
    if (o.coast)
    {
        pursuit = Pursuit();
        float sx = 0, sy = 0;
        aim::SteerTowards(o.steerPos, flightio::Get().steerScale, 0.35f, sx, sy);
        float cam[9];
        if (o.steerLevel)
            aim::LevelSteer(sx, sy, nullptr, o.steerLevel == 2 && fl::FlightScreenAxes(cam) ? cam : nullptr);
        SetSteer(sx, sy);
        AimCursor(fl::ScreenWidth() * 0.5f, fl::ScreenHeight() * 0.5f);
        o.afterburn = false, o.slide = 0, o.dodgeSlide = 0, o.cruise = -1;
    }
    else if (o.pursue)
    {
        // Lead pursuit (the bot's flight): the nose and the reticle on the lead cross; the guns fire on it. In the ship's
        // own frame (aim::Current(true)): the same flight whatever camera the player looks through.
        const aim::Target t = aim::Current(true);
        // the head-on pass's predictive turn: the nose at the point ahead of the target (the guns keep the lead
        // through the autopilot's gunAim / SetGunAim)
        aim::Target ts = t;
        float sp[3];
        if (o.steerAhead && fl::WorldToFlightScreen(o.steerAheadPos, sp))
        {
            ts.screenX = sp[0], ts.screenY = sp[1];
            ts.onScreen = sp[0] >= 0 && sp[1] >= 0 && sp[0] < fl::ScreenWidth() && sp[1] < fl::ScreenHeight();
            memcpy(ts.world, o.steerAheadPos, sizeof(ts.world));
        }
        PursuitOut po = Pursue(ts, dt, pursuit, o.pursueBand, o.fireCone, o.gunRange, o.weave, o.weaveRadius);
        float axes[9];
        const bool haveAxes = fl::FlightScreenAxes(axes);
        // (Pursue steers along the flight screen's axes - the camera's, or the ship's off screen; restated into the frame the
        // game turns in: `steer_level` 1 the level frame, 2 the camera's - measured in fights: the turn followed the screen)
        if (o.steerLevel)
            aim::LevelSteer(po.sx, po.sy, po.screenFrame && haveAxes ? axes : nullptr,
                            o.steerLevel == 2 && haveAxes ? axes : nullptr);
        // (debug `steer_check`: where the lead sits round the nose in the ship's frame vs where the nose turns, every
        // 0.25 s while it is 5+ deg off - the steering's frame measured in a fight)
        static DWORD checkAt = 0;
        fl::Kinematics kc = {};
        if (o.steerCheck && haveAxes && gameclock::NowMs() >= checkAt && fl::PlayerKinematics(kc))
        {
            checkAt = gameclock::NowMs() + 250;
            const float w[3] = {ts.world[0] - kc.pos[0], ts.world[1] - kc.pos[1], ts.world[2] - kc.pos[2]};
            float d[3];
            for (int i = 0; i < 3; ++i)
                d[i] = kc.rot[0 * 3 + i] * w[0] + kc.rot[1 * 3 + i] * w[1] + kc.rot[2 * 3 + i] * w[2];
            const float len = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
            const float off = len > 1 ? acosf(Clampf(-d[2] / len, -1, 1)) * 57.3f : 0;
            float spin[3] = {};
            if (off > 5 && space::PlayerSpin(spin))
            {
                // the camera's up in the ship's frame: its roll against the ship's
                const float cu[3] = {axes[0 * 3 + 1], axes[1 * 3 + 1], axes[2 * 3 + 1]};
                const float cx = kc.rot[0] * cu[0] + kc.rot[3] * cu[1] + kc.rot[6] * cu[2];
                const float cy = kc.rot[1] * cu[0] + kc.rot[4] * cu[1] + kc.rot[7] * cu[2];
                events::Push("autopilot steer-check off=%.0f phi=%.0f turn=%.0f w=%.2f bank=%.0f camroll=%.0f screen=%d level=%d", off,
                        atan2f(d[0], d[1]) * 57.3f, atan2f(-spin[1], spin[0]) * 57.3f,
                        sqrtf(spin[0] * spin[0] + spin[1] * spin[1]), geom::RollDeg(kc.rot), atan2f(cx, cy) * 57.3f,
                        po.screenFrame ? 1 : 0, o.steerLevel);
            }
        }
        SetSteer(po.sx, po.sy);
        flightio::LastSteerX() = po.sx;
        flightio::LastSteerY() = po.sy;
        AimCursor(po.ax, po.ay);
        fireHold = po.aligned && t.inRange ? 0.15f : fireHold - dt;
        fire = fire && fireHold > 0;
        reverse = po.reverse;
        o.afterburn = o.afterburn || po.burn;
        if (o.turnManeuver) // the autopilot's turn maneuver decides the throttle (autopilot.cpp, turn_evade)
        {
            reverse = o.turnManeuver == 2;
            o.afterburn = !reverse;
        }
        if (!reverse)
            fl::SetThrottle(1.0f);
        fl::Kinematics k = {};
        if (o.station && fl::PlayerKinematics(k))
        {
            // Station keeping: a velocity toward the point that shrinks with the distance left (40 % of it per second,
            // at most 60 m/s along the nose, 40 sideways), reached by partial throttle / reverse along the nose and the
            // slide keys across it (the larger sideways error); no afterburner.
            const float err[3] = {o.stationPos[0] - k.pos[0], o.stationPos[1] - k.pos[1], o.stationPos[2] - k.pos[2]};
            auto axis = [&](int c, float sign, const float* v)
            { return sign * (v[0] * k.rot[0 * 3 + c] + v[1] * k.rot[1 * 3 + c] + v[2] * k.rot[2 * 3 + c]); };
            const float along = axis(2, -1, err), vAlong = axis(2, -1, k.vel);
            const float want = Clampf(along * 0.4f, -60.0f, 60.0f);
            reverse = want - vAlong < -5.0f;
            fl::SetThrottle(reverse ? 0.0f : Clampf((want - vAlong) / 40.0f, 0.0f, 1.0f));
            o.afterburn = false;
            const float right = axis(0, 1, err), vRight = axis(0, 1, k.vel);
            const float up = axis(1, 1, err), vUp = axis(1, 1, k.vel);
            const float dr = Clampf(right * 0.4f, -40.0f, 40.0f) - vRight, du = Clampf(up * 0.4f, -40.0f, 40.0f) - vUp;
            if (!o.dodgeSlide && !o.slide) // (a slide already set - the autopilot's wall slide - wins)
                o.slide = std::max(fabsf(dr), fabsf(du)) < 8.0f ? 0
                          : fabsf(dr) >= fabsf(du)              ? (dr > 0 ? 2 : 1)
                                                                : (du > 0 ? 3 : 4);
        }
    }
    else if (o.steerTo)
    {
        // Between targets: keep flying, nose towards the next one (never coast in a fight).
        pursuit = Pursuit();
        fireHold = 0;
        float sx = 0, sy = 0;
        if (o.steerFullAngle > 0)
            aim::SteerTowards(o.steerPos, flightio::Get().steerScale, o.steerFullAngle, sx, sy); // navigation: calm
        else
            aim::TurnTowards(o.steerPos, flightio::Get().steerScale, sx, sy);
        float cam[9];
        if (o.steerLevel) // (the ship's frame restated into the game's turn frame)
            aim::LevelSteer(sx, sy, nullptr, o.steerLevel == 2 && fl::FlightScreenAxes(cam) ? cam : nullptr);
        SetSteer(sx, sy);
        AimCursor(fl::ScreenWidth() * 0.5f, fl::ScreenHeight() * 0.5f);
        fl::SetThrottle(o.steerThrottle);
        reverse = o.steerReverse;
    }
    else if (o.brake)
    {
        // Hold position (e.g. inside the tractor's reach): throttle 0, reverse while still moving.
        pursuit = Pursuit();
        fireHold = 0;
        SetSteer(0, 0);
        AimCursor(fl::ScreenWidth() * 0.5f, fl::ScreenHeight() * 0.5f);
        fl::SetThrottle(0.0f);
        fl::Kinematics k = {};
        if (fl::PlayerKinematics(k))
            reverse = sqrtf(k.vel[0] * k.vel[0] + k.vel[1] * k.vel[1] + k.vel[2] * k.vel[2]) > 5.0f;
    }
    else
    {
        pursuit = Pursuit();
        fireHold = 0;
        ReleaseSteer(); // the game's behaviour steers
        float ax, ay;
        aim::Gimbal(flightio::Get().aimCone, ax, ay);
        AimCursor(ax, ay);
    }
    // Turret view while the autopilot flies: the right stick turns the camera and the reticle stays centred, as in the
    // player's own flight (was: the cursor followed the stick - the turret block sat after this function's early
    // return, so the camera never turned and a look command held at the start stayed held). ASSISTED: the right stick
    // is the player's aim (the reticle follows the stick from the screen centre; the autopilot's gimbal aim yields
    // while the stick is deflected).
    const bool turret = views::PadTurretLook(p.rx, p.ry);
    // The guns in turret view (the player may look around while the autopilot keeps full
    // control): the game aims every gun along the cursor's ray through the current camera, which is now the player's -
    // so the autopilot hands the guns its own world point instead (host v28): the target's lead cross (aim::Current: the
    // game's lead when in range), else straight ahead of the nose. Out of turret view the game's own aim, by the cursor.
    if (hostcalls::g.SetGunAim)
    {
        if (turret || flightio::UnderWindow())
        {
            const aim::Target t = aim::Current(true);
            fl::Kinematics k = {};
            float ahead[3];
            const float* world = nullptr;
            if (!turret && o.gunAim && fire) // (`fire_covering`, as out of a window)
                world = o.gunAimPos;
            else if (t.valid && t.inRange)
                world = t.world;
            else if (fl::PlayerKinematics(k))
            {
                for (int i = 0; i < 3; ++i)
                    ahead[i] = k.pos[i] - k.rot[i * 3 + 2] * 1000.0f; // -z = forward (the rot's third column)
                world = ahead;
            }
            hostcalls::g.SetGunAim(world != nullptr, world);
        }
        else if (o.gunAim && fire) // the autopilot fires off the nose at a point some gun can reach (`fire_covering`)
            hostcalls::g.SetGunAim(true, o.gunAimPos);
        else
            hostcalls::g.SetGunAim(false, nullptr);
    }
    if (!turret && rightAims && (p.rx != 0 || p.ry != 0))
    {
        float ax = fl::ScreenWidth() * 0.5f + p.rx * fl::ScreenWidth() * 0.5f;
        float ay = fl::ScreenHeight() * 0.5f - aim::LookY(p.ry) * fl::ScreenHeight() * 0.5f;
        aim::ClampToSafeArea(ax, ay);
        AimCursor(ax, ay);
    }
    // Staggered fire (the player's trick, user 17 Oct 2026), for any loadout: a burst starts with the guns one by one,
    // evenly over the refire cycle (N active non-homing guns, the slowest refire r: gun k alone at k*r/N, by its
    // FIRE_WEAPON slot = its place in the ship's weapon list), then fire-all - the guns stay out of phase for the
    // whole burst, a steady stream of shots instead of volleys.
    static float staggerTime = 0, staggerStep = 0;
    static int staggerNext = 0, staggerCount = 0;
    static int staggerSlots[10];
    bool fireAll = fire;
    if (o.stagger && fire)
    {
        if (!wasFiring)
        {
            staggerTime = 0;
            staggerNext = 0;
            staggerCount = 0;
            float r = 0;
            const std::vector<space::Weapon> ws = space::PlayerWeapons();
            // By the game's fire slot (the HUD weapon list), never the list index: the index tapped the Javelin's slot
            // at every burst start (M02 jailbreak stray, traced 18 Oct 2026). A gun without a known slot is not staggered.
            for (size_t i = 0; i < ws.size() && staggerCount < 10; ++i)
                if (ws[i].active && !ws[i].homing && ws[i].refire > 0 && ws[i].fireSlot >= 1 && ws[i].fireSlot <= 10)
                {
                    staggerSlots[staggerCount++] = ws[i].fireSlot - 1;
                    r = std::max(r, ws[i].refire);
                }
            staggerStep = staggerCount > 1 ? r / staggerCount : 0.0f;
        }
        staggerTime += dt;
        if (staggerStep > 0 && staggerNext < staggerCount - 1)
        {
            while (staggerNext < staggerCount - 1 && staggerTime >= staggerNext * staggerStep)
                Tap(static_cast<fl::Cmd>(fl::USER_FIRE_WEAPON1 + staggerSlots[staggerNext++]));
            fireAll = false;
        }
        else
            fireAll = staggerTime >= (staggerCount - 1) * staggerStep;
    }
    wasFiring = fire;
    // The autopilot's "fire all" holds each gun's own fire slot, never the game's fire-all: that fires every weapon of
    // the active group, missile launchers included (measured 18 Oct 2026: 3 s of USER_FIRE_WEAPONS launched a Javelin -
    // the strays of M02's jailbreak fight). Only when a gun's slot is unknown does it fall back to fire-all.
    static unsigned heldSlots = 0;
    unsigned wantSlots = 0;
    bool slotsKnown = false;
    if (fireAll && !o.fireAllCmd)
    {
        slotsKnown = true;
        for (const space::Weapon& w : space::PlayerWeapons())
            if (!w.homing && w.ammo < 0 && w.refire > 0)
            {
                if (w.fireSlot < 1 || w.fireSlot > 10)
                    slotsKnown = false;
                else
                    wantSlots |= 1u << (w.fireSlot - 1);
            }
        if (!slotsKnown)
            wantSlots = 0;
    }
    for (int s = 0; s < 10; ++s)
        if ((heldSlots | wantSlots) & (1u << s))
            Hold(static_cast<fl::Cmd>(fl::USER_FIRE_WEAPON1 + s), (wantSlots & (1u << s)) != 0, inputrules::OwnerAutopilot);
    heldSlots = wantSlots;
    // Manual weapons stay available: the player's fire and missile buttons (the flight_ap bindings) as always.
    Claim(Channel::Fire, fl::USER_FIRE_WEAPONS, fireAll && !slotsKnown, views::PadHeld(Channel::Fire));
    Claim(Channel::Missile, fl::USER_LAUNCH_MISSILES, false, views::PadHeld(Channel::Missile));
    views::PadTorpedoSlot();
    views::PadFireGroups();
    if (o.missile)
        Tap(fl::USER_LAUNCH_MISSILES);
    if (o.torpedo)
        Tap(fl::USER_LAUNCH_TORPEDOS);
    if (o.mine)
        Tap(fl::USER_LAUNCH_MINES);
    if (o.engineKill)
        Tap(fl::USER_MANEUVER_ENGINEKILL);
    if (o.cm)
        Tap(fl::USER_LAUNCH_COUNTERMEASURES);
    if (o.closestEnemy)
        Tap(fl::USER_CLOSEST_ENEMY);
    if (o.repairHull)
        Tap(fl::USER_REPAIR_HEALTH); // the game's own repair command (the HUD's nanobot button)
    if (o.repairShield)
        Tap(fl::USER_REPAIR_SHIELD); // shield battery
    // Cruise by the engine's real state (CEEngine::IsCruiseEngaged: true from the request, charging or cruising): the
    // toggle only when it differs from what the autopilot wants, and not again before the game has run a few frames
    // since the last toggle (it shows in the engine's state by then).
    static u32 cruiseTapFrame = 0;
    if (o.cruise >= 0 && !o.coast && flightio::Frame() >= cruiseTapFrame + 3)
    {
        flight::Model fm;
        if (flight::ReadModel(fm) && fm.cruiseEngaged != (o.cruise == 1))
        {
            Tap(fl::USER_CRUISE);
            cruiseTapFrame = flightio::Frame();
        }
    }
    if (o.collectAll)
        Tap(fl::USER_COLLECT_LOOT); // tractor beam, all items in reach
    if (o.dodgeSlide)
    {
        o.slide = o.dodgeSlide; // a missile dodge wins over the evasion's slide bursts
        o.afterburn = true;
    }
    Claim(Channel::Afterburner, fl::USER_AFTERBURN, o.afterburn, views::PadHeld(Channel::Afterburner));
    Claim(Channel::Reverse, fl::USER_MANEUVER_BRAKE_REVERSE, reverse, views::PadHeld(Channel::Reverse));
    Claim(Channel::Rear, fl::USER_REARVIEW_CAMERA_MODE, false, views::PadHeld(Channel::Rear));
    Claim(Channel::Slide, fl::USER_MANEUVER_SLIDE_EVADE_LEFT, o.slide == 1, false);
    Claim(Channel::Slide, fl::USER_MANEUVER_SLIDE_EVADE_RIGHT, o.slide == 2, false);
    Claim(Channel::Slide, fl::USER_MANEUVER_SLIDE_EVADE_UP, o.slide == 3, false);
    Claim(Channel::Slide, fl::USER_MANEUVER_SLIDE_EVADE_DOWN, o.slide == 4, false);
    return true;
}
} // namespace autopilot
