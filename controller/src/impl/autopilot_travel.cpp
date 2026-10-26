// Advanced Autopilot - travel: the route, holds and rests, arrivals. Part of autopilot.cpp until 21 Oct 2026; shared state and declarations in
// autopilot_impl.h, the module API in autopilot.h.
#include "autopilot_impl.h"

namespace autopilot
{
// ------------------------------------------------------------------ travel
// When the last jump-in happened (GetTickCount64): the first 6 s after it charge the cruise at once
static ULONGLONG g_jumpInTick = 0;
static bool JustJumpedIn() { return g_jumpInTick && GetTickCount64() - g_jumpInTick < 6000; }
// The last airlock door line read (AirlockStage), kept across trips: each trip starts its own state, and the M12 core's
// exit trip passes the plate before it reads its door again (passage_axis, set s11h: never applied)
static u32 g_lastDoorId = 0;
static float g_lastDoorP2[3] = {}, g_lastDoorAxis[3] = {};
bool LastAirlockDoor(const float near[3], float within, float p2[3], float axis[3])
{
    const float d[3] = {near[0] - g_lastDoorP2[0], near[1] - g_lastDoorP2[1], near[2] - g_lastDoorP2[2]};
    if (!g_lastDoorId || Len3(d) >= within || !space::ObjectPos(g_lastDoorId, p2))
        return false;
    memcpy(p2, g_lastDoorP2, 3 * sizeof(float));
    memcpy(axis, g_lastDoorAxis, 3 * sizeof(float));
    return true;
}
// ref_line: a reference flight through a structure - speedrun knowledge, source: the user's M12 try 3, 23 Oct 2026
// (research\experience\reference\M12_user_2026-10-23_try3_motion.tsv; "pretty good, but not perfect" - a guide). The M12
// core (St02c) from past the turbine to the exit door: the line converges on the axis through the hub's ~90 m hole, rises
// over the static plate at y ~50 and drops back onto the axis at the door. The points before the hole (rotatable) are
// turned about the exit door's axis to the azimuth where this run crossed the turbine (its timed gap may lie anywhere
// round the axis); the plate and the door do not rotate. Session 11: the patched rejoin / passage rules failed in ~24 %
// of legs (strikes at the turbine rim, the hub, the plate - dossier M12 "session 11").
namespace
{
const float kM12CoreLine[][3] = {{-176, -37, 171}, {-168, -38, 49}, {-148, -36, -72}, {-121, -29, -193},
                                 {-91, -19, -313}, {-59, -8, -429}, {-27, 2, -552},   {-7, 11, -674},
                                 {0, 22, -795},    {4, 38, -917},   {5, 51, -1040},   {2, 50, -1162},
                                 {0, 37, -1285},   {2, 23, -1408},  {0, 8, -1500},    {0, 0, -1600}};
struct RefLine
{
    u32 system;
    int rotatable; // the first n points
    const float (*pts)[3];
    int n;
};
const RefLine kRefLines[] = {
    {3215405891u, 7, kM12CoreLine, static_cast<int>(sizeof(kM12CoreLine) / sizeof(kM12CoreLine[0]))},
};
} // namespace

// The aim along this system's reference line (pure pursuit: 1.2 s of speed ahead, at least 250 m), when the switch is on,
// under the cruise lock, the exit door's axis known, the trip's destination near that door (`dest`: within 1.5 km of its
// outer point - the cell on the way and the exit itself; never another trip in the structure) and the ship within 250 m
// of the line between its ends.
bool RefLineAim(const fl::Kinematics& k, const float dest[3], float aim[3])
{
    static u32 rotSys = 0;
    static float rotCos = 1, rotSin = 0;
    static bool rotSet = false;
    if (!g_set.refLine || !g_obj.cruiseLock)
        return false;
    const u32 sys = fl::PlayerSystem();
    const RefLine* L = nullptr;
    for (const RefLine& r : kRefLines)
        if (r.system == sys)
            L = &r;
    float p2[3], ax[3];
    if (!L || !LastAirlockDoor(L->pts[L->n - 1], 800.0f, p2, ax))
        return false;
    {
        const float dd[3] = {dest[0] - p2[0], dest[1] - p2[1], dest[2] - p2[2]};
        if (Len3(dd) > 1500.0f)
            return false;
    }
    if (rotSys != sys)
        rotSys = sys, rotSet = false;
    // the axis frame: a = the door's axis, e1 / e2 across it
    float e1[3] = {ax[1], -ax[0], 0.0f};
    if (Len3(e1) < 0.1f)
        e1[0] = 0, e1[1] = ax[2], e1[2] = -ax[1];
    geom::Norm(e1);
    float e2[3];
    geom::Cross(e2, ax, e1);
    const auto across = [&](const float* p, float& u, float& v, float& s)
    {
        const float r[3] = {p[0] - p2[0], p[1] - p2[1], p[2] - p2[2]};
        s = Dot3(r, ax), u = Dot3(r, e1), v = Dot3(r, e2);
    };
    float su, sv, ss, fu, fv, fs;
    across(k.pos, su, sv, ss);
    across(L->pts[0], fu, fv, fs);
    float lu, lv, ls;
    across(L->pts[L->n - 1], lu, lv, ls);
    // between the line's ends along the axis (the axis points into the structure: s falls along the way)
    if (ss > fs + 150.0f || ss < ls)
    {
        rotSet = false; // (outside the line's band: the next pass sets its own rotation - review, session 11)
        return false;
    }
    // the rotation, once per pass: where this run is now against the line's first point, both round the axis
    float P[16][3];
    const size_t n = static_cast<size_t>(std::min(L->n, 16));
    const auto build = [&]()
    {
        for (size_t i = 0; i < n; ++i)
        {
            float u, v, s;
            across(L->pts[i], u, v, s);
            if (static_cast<int>(i) < L->rotatable)
            {
                const float ru = u * rotCos - v * rotSin, rv = u * rotSin + v * rotCos;
                u = ru, v = rv;
            }
            for (int j = 0; j < 3; ++j)
                P[i][j] = p2[j] + ax[j] * s + e1[j] * u + e2[j] * v;
        }
    };
    if (!rotSet)
    {
        // ...the rotated line tested for the ship + 15 m (the core is not symmetric between the turbine and the hub: s11s
        // rotated -81 deg and struck structure at z -228, 126 m off the axis; 28-38 deg were clear): the angle stepped back
        // toward the reference's own azimuth until clear
        // (near the axis the azimuth means nothing: no rotation within 60 m of it)
        const float a0 = sqrtf(su * su + sv * sv) < 60.0f ? 0.0f : atan2f(sv, su) - atan2f(fv, fu);
        const float m = space::PlayerRadius() + 15.0f;
        float a = 0;
        for (const float f : {1.0f, 0.75f, 0.5f, 0.25f, 0.0f})
        {
            a = a0 * f;
            rotCos = cosf(a), rotSin = sinf(a);
            build();
            bool clear = true;
            for (int i = 0; i + 1 < L->rotatable + 1 && i + 1 < static_cast<int>(n) && clear; ++i)
                clear = LineClear(P[i], P[i + 1], m);
            if (clear)
                break;
        }
        rotSet = true;
        events::Push("autopilot ref-line system=%u: rotated %.0f deg to this run's crossing (%.0f wanted)", sys,
                     a * 57.2958f, a0 * 57.2958f);
    }
    build();
    // the nearest point of the polyline, then the carrot ahead along it
    float best = 1e30f, bestT = 0;
    size_t bestI = 0;
    for (size_t i = 0; i + 1 < n; ++i)
    {
        const float d[3] = {P[i + 1][0] - P[i][0], P[i + 1][1] - P[i][1], P[i + 1][2] - P[i][2]};
        const float dl2 = std::max(1.0f, Dot3(d, d));
        const float r[3] = {k.pos[0] - P[i][0], k.pos[1] - P[i][1], k.pos[2] - P[i][2]};
        const float t = Clampf(Dot3(r, d) / dl2, 0, 1);
        const float q[3] = {P[i][0] + d[0] * t - k.pos[0], P[i][1] + d[1] * t - k.pos[1],
                            P[i][2] + d[2] * t - k.pos[2]};
        if (Len3(q) < best)
            best = Len3(q), bestI = i, bestT = t;
    }
    if (best > 250.0f)
        return false;
    float left = std::max(250.0f, 1.2f * Len3(k.vel));
    size_t i = bestI;
    float t = bestT;
    while (i + 1 < n)
    {
        const float d[3] = {P[i + 1][0] - P[i][0], P[i + 1][1] - P[i][1], P[i + 1][2] - P[i][2]};
        const float seg = Len3(d) * (1 - t);
        if (seg >= left)
        {
            const float f = t + left / std::max(1.0f, Len3(d));
            for (int j = 0; j < 3; ++j)
                aim[j] = P[i][j] + d[j] * f;
            return true;
        }
        left -= seg, ++i, t = 0;
    }
    // past the last point: on along the door's axis, outward (never back to its outer point p2 - s11r-r: p2 lies behind the
    // line's end, the run turned back at 295 m/s and struck the ring at z -1430)
    for (int j = 0; j < 3; ++j)
        aim[j] = P[n - 1][j] - ax[j] * left;
    return true;
}

// A fresh fast approach for the next step (bay / lane start found again, nothing requested yet).
static void ResetApproach()
{
    g.dock.fastBay = -1;
    g.dock.allBays.clear();
    g.dock.formationTried = false;
    g.dock.shortcutSince = -1;
    g.dock.formationTakenIn = false;
    g.dock.replySeen = HostDockReply(nullptr, nullptr, nullptr); // answers before this approach are not ours
    g.dock.queuedSince = -1;
    g.dock.queueTries = 0;
    g.dock.parkForQueue = false;
    g.dock.parkedSince = -1;
    g.dock.rushing = false;
    g.dock.portLogged = false;
    g.dock.fastKind = DockKind::None; // (ring, airlock and lane off)
    g.dock.airlockPastAt = -1;
    g.dock.airlockAlong = 1e9f;
    g.dock.passOn = false;
    g.dock.fastStill = 0;
    g.dock.fastLined = g.dock.fastRequested = g.dock.fastTried = g.dock.fastBurn = false;
    g.dock.laneCross = false;
    g.dock.laneNext = 0;
    g.dock.laneSide = -1;
    g.dock.fastBurnSwitches = 0;
    g.dock.fastBrakeCommit = g.dock.fastPlanned = false;
    g.dock.fastPredicted = 0;
    g.dock.fastDetour = 0;
    g.dock.fastDetourAt = -1;
    g.started = false;
    g.dock.laneTowardPlan = 0;
}

static std::vector<u32> ListSeqs()
{
    std::vector<u32> s;
    for (const space::Waypoint& w : space::Waypoints())
        s.push_back(w.seq);
    return s;
}

static void TravelReplan(const char* why)
{
    State::Travel& t = g.tr;
    events::Push("autopilot travel replan why=%s step=%u", why, static_cast<u32>(t.step));
    if (++t.replans > 6)
    {
        Stop(StopReason::TooManyReplans);
        return;
    }
    ResetApproach();
    t.phase = t.hasDest ? State::Travel::Plot : State::Travel::Plan;
    t.stepBegun = t.riding = t.exitSent = false;
}

// A player's run to a point that stops on it (the destination is a point): afterburner or cruise as planned from the
// ship's model, braking by full reverse at the exact braking distance. True once stopped there.
// Hold: where a waiting pilot looks (user, 18 Oct 2026: facing King during the dialogue was not logical - look
// toward where you go next). 't' the hold's own target; 'r' the next leg of the route to the game's waypoints (the
// lane ring / gate / base the player would take next: route::MakeNow, the autopilot's own plan); 0 nowhere in
// particular (the nose stays). Not the selection: it is an action's target, not where a waiting pilot looks (M02 scan
// hold, 18 Oct 2026: the scan cycled the selection every 0.4 s and the nose swung after it - "extremely erratic").
// Writes the point.
// 's' (19 Oct 2026, user + 2020 footage): for 30 s after a fight, the ship the fight was about - the friendly the hostiles
// went for (the prison ship in M01b) - if it is still there within 5 km; then the route.
// The point is levelled to our own altitude by the caller: a pilot at rest sits level with the system's plane (user,
// 19 Oct 2026: a nose pitched up at a planet "is not level with the map's primary plane").
u32 RestSubject(char& why);
char HoldLook(const float at[3], float look[3])
{
    const auto put = [&](const float p[3]) { memcpy(look, p, sizeof(float) * 3); };
    if (g.target)
        if (const space::Contact c = FindContact(g.target); c.id)
            return put(c.pos), 't';
    char why = 0;
    if (const u32 subject = RestSubject(why))
        if (const space::Contact c = FindContact(subject); c.id && c.distance < 5000 && c.health != 0)
            return put(c.pos), why;
    const route::Plan p = route::MakeNow();
    if (p.ok)
        for (const route::Step& s : p.steps)
        {
            const float to[3] = {s.pos[0] - at[0], s.pos[1] - at[1], s.pos[2] - at[2]};
            if (Len3(to) > 500) // a step right here (the hold point itself) shows no way
                return put(s.pos), 'r';
        }
    return 0;
}

// Scenic idling (user + the 2020 footage at 2 fps, M01b 19 Oct 2026): after the last Rogue the player flew to the
// prison ship, passed along its hull, then parked beside it, level, its hull broadside across the screen with the
// planets behind, for ~10 s while its crew talked (turret view panning to it and to King). The resting point: beside the
// last fight's subject (fresh: 30 s, within 5 km), off its long axis on our side, its radius + 200 m out, at its own
// altitude (level). The nose then turns onto it ('s' in HoldLook): the broadside view.
// The object the mission points at right now, if it is a scene to rest by (user, 19 Oct 2026: "most important object
// in terms of recent objectives" ranks above what the fight implies; the player's own information - the HUD's mission
// marks and the objective's targets, host API v10): the newest one within 5 km that is not hostile (a hostile mark is
// a target, not a subject).
// A place outranks a companion (user, 20 Oct 2026, M04 Mactan: idling beside Juni the ship looked at her - "I'd prefer
// it looked at Mactan"): first a station / base / solar the mission points at, then a big ship (a capital, the M01b
// prison ship the 2020 footage framed broadside: 8000+ hit points), a small craft (a wingman) last - and with no
// mission place, a friendly station or base within 5 km comes before a wingman too.
u32 MissionSubject()
{
    // waiting for the next step at its target (the objective mode's "pre-stage"): the nose on it, hostile or not - the
    // first shot goes the moment the step begins
    if (g_obj.preStageTarget && Active() && g_obj.action == Action::PreStage)
        return g_obj.preStageTarget;
    u32 ids[16];
    const std::vector<space::Contact> all = Contacts();
    const int n = MissionObjectIds(ids, 16, all);
    u32 best = 0;
    int bestRank = 9;
    const auto consider = [&](const space::Contact& c, int rank)
    {
        if (!c.id || c.distance > 5000 || c.health == 0 || rank >= bestRank)
            return;
        float att = 0;
        const bool hostile = c.ship ? c.attitude <= -0.6f : (space::AttitudeToPlayer(c.id, att) && att <= -0.6f);
        if (!hostile)
            best = c.id, bestRank = rank;
    };
    for (int i = 0; i < n; ++i) // newest first: within a rank the newest wins
    {
        const auto it = std::find_if(all.begin(), all.end(), [&](const space::Contact& x) { return x.id == ids[i]; });
        if (it != all.end())
            consider(*it, !it->ship ? 0 : it->maxHit >= 8000 ? 1 : 3);
    }
    if (bestRank > 1) // no mission place: a friendly station / base close by is still the scene
        for (const space::Contact& c : all)
        {
            float att = 0;
            if (!c.ship && c.targetable && (c.type & (space::kStation | space::kDockingRing)) && c.distance < 5000 &&
                space::AttitudeToPlayer(c.id, att) && att > 0)
                consider(c, 2);
        }
    return best;
}

// The subject to rest by: the mission's ('m'), else the last fight's ('s', fresh: 30 s). 0 none.
u32 RestSubject(char& why)
{
    if (const u32 m = MissionSubject())
        return why = 'm', m;
    if (g_subjectId && GetTickCount64() - g_subjectTick < 30000)
        return why = 's', g_subjectId;
    return why = 0, 0;
}

bool ScenicPoint(const float at[3], float out[3])
{
    // Only in a quiet area: no hostile ship within 3 km (a rest, not a maneuver in a fight)
    for (const space::Contact& h : Contacts())
        if (h.ship && h.attitude <= -0.6f && h.health != 0 && h.distance < 3000)
            return false;
    char why = 0;
    const u32 subject = RestSubject(why);
    if (!subject)
        return false;
    const space::Contact c = FindContact(subject);
    float s[3], rot[9];
    if (!c.id || c.distance > 5000 || c.health == 0 || !space::ObjectPose(c.id, s, rot))
        return false;
    float radius = 100;
    for (const space::Obstacle& o : space::Obstacles(s, 1))
        if (o.id == c.id)
            radius = o.radius;
    float f[2] = {-rot[0 * 3 + 2], -rot[2 * 3 + 2]}; // the subject's heading in the plane
    float fl = sqrtf(f[0] * f[0] + f[1] * f[1]);
    if (fl < 1e-3f)
        f[0] = 1, f[1] = 0, fl = 1;
    f[0] /= fl, f[1] /= fl;
    float side[2] = {f[1], -f[0]};
    if ((at[0] - s[0]) * side[0] + (at[2] - s[2]) * side[1] < 0) // our side of it
        side[0] = -side[0], side[1] = -side[1];
    const float d = radius + 200;
    out[0] = s[0] + side[0] * d;
    out[1] = s[1];
    out[2] = s[2] + side[1] * d;
    return true;
}

// A rotating part on the way to `pos` (avoid::WheelScan; session 7 - the M12 core's reactor wheel): scanned twice 0.25 s
// apart, then the crossing point in its plane re-aimed every frame for the predicted arrival (into aim); past its plane:
// done. True while it steers. (Used by ArriveAt's travel only: the airlock run's use and a scan along the motion, for
// "far spokes" at the M12 core exit, were dropped - those are a static plate, PassageVia; the motion scan re-found the
// turbine behind the ship.)
bool WheelTiming(const fl::Kinematics& k, const float pos[3], float aim[3])
{
    const float toPos[3] = {pos[0] - k.pos[0], pos[1] - k.pos[1], pos[2] - k.pos[2]};
    const float posDist = Len3(toPos), speed = Len3(k.vel);
    if (!g_set.wheelTiming || posDist <= 100)
        return false;
    bool on = false;
    const float dir[3] = {toPos[0] / posDist, toPos[1] / posDist, toPos[2] / posDist};
    if (g.arrive.wheel.phase == 0 && g.time - g.arrive.wheelCheckAt > 0.25f && g.time - g.arrive.wheelDoneAt > 5)
    {
        g.arrive.wheelCheckAt = g.time;
        const float look = std::min(posDist, 1500.0f);
        const float ahead[3] = {k.pos[0] + dir[0] * look, k.pos[1] + dir[1] * look, k.pos[2] + dir[2] * look};
        u32 id = 0;
        if (space::SightBlocker(k.pos, ahead, 0, &id) == 0x1)
        {
            const float h = space::RayHitDistance(k.pos, ahead);
            if (h > 150) // (closer than that there is no time to time it)
            {
                const float hit[3] = {k.pos[0] + dir[0] * h, k.pos[1] + dir[1] * h, k.pos[2] + dir[2] * h};
                if (avoid::WheelScanStep(g.arrive.wheel, hit, dir, id, g.time, nullptr))
                    events::Push("autopilot wheel part=%u ahead %.0f m: scanning", id, h);
            }
        }
    }
    else if (g.arrive.wheel.phase == 1 && g.time - g.arrive.wheel.t0 >= 0.25f)
    {
        std::string info;
        if (avoid::WheelScanStep(g.arrive.wheel, nullptr, g.arrive.wheel.dir, g.arrive.wheel.part, g.time, &info))
            events::Push("autopilot wheel timed: %s", info.c_str());
        else
            events::Push("autopilot wheel: nothing to time");
    }
    if (g.arrive.wheel.phase == -1)
        g.arrive.wheel = avoid::WheelScan(), g.arrive.wheelDoneAt = g.time;
    if (g.arrive.wheel.phase == 2)
    {
        const float pl[3] = {g.arrive.wheel.plane[0] - k.pos[0], g.arrive.wheel.plane[1] - k.pos[1],
                             g.arrive.wheel.plane[2] - k.pos[2]};
        const float s = Dot3(pl, g.arrive.wheel.dir);
        if (s < -60)
        {
            events::Push("autopilot wheel crossed (%.0f m past its plane)", -s);
            g.arrive.wheel = avoid::WheelScan();
            g.arrive.wheelDoneAt = g.time;
        }
        else
        {
            float wa[3];
            // the arrival: at the current speed, or the cruise's when cruising / charging nearly done
            const float v = std::max(80.0f, speed);
            if (s > 20 && avoid::WheelAim(g.arrive.wheel, k.pos, g.time, s / v, wa))
            {
                memcpy(aim, wa, 3 * sizeof(float));
                on = true;
            }
            g.arrive.wheelPart = g.arrive.wheel.part;
            g.arrive.wheelFrame = g.frames;
        }
    }
    return on;
}

// A trip into a script's sphere beside an airlock (s9q, M12 gen 3 -> the lair's top: the sphere's near side - and its
// centre alike - left the airlock run 260-400 m off the door's line at 80 m/s, a ~6 s loop to line up; the user's try 3
// came level onto the door's line and dived in): the door's line is the game's own (its dock bay, readable for the
// target only - the airlock is targeted once, read the next frame); a point 50 m out on it (level with the mouth), when
// inside the sphere, is the aim - the step ends there and the airlock run curves in. False while unknown or not inside.
static bool AirlockStage(float stage[3])
{
    State::Travel& t = g.tr;
    if (!t.hasDest || !g_obj.on || g_obj.flyGoalR <= 0 || t.lockId == ~0u)
        return false;
    if (!t.lockHave)
    {
        if (!t.lockId)
        {
            float best = 1500.0f;
            for (const space::Contact& c : Contacts())
            {
                const float d[3] = {c.pos[0] - t.dest[0], c.pos[1] - t.dest[1], c.pos[2] - t.dest[2]};
                if ((c.type & space::kAirlockGate) && Len3(d) < best)
                    best = Len3(d), t.lockId = c.id;
            }
            if (!t.lockId)
            {
                t.lockId = ~0u;
                return false;
            }
        }
        if (fl::TargetId() == t.lockId)
        {
            float best = 1e30f;
            for (const space::DockBay& b : space::TargetDockBays())
            {
                const float a[3] = {b.p2[0] - b.mount[0], b.p2[1] - b.mount[1], b.p2[2] - b.mount[2]};
                const float d[3] = {b.p2[0] - t.dest[0], b.p2[1] - t.dest[1], b.p2[2] - t.dest[2]};
                const float al = Len3(a);
                if (b.type != 8 || al < 0.1f || Len3(d) >= best)
                    continue;
                best = Len3(d);
                for (int i = 0; i < 3; ++i)
                    t.lockP2[i] = b.p2[i], t.lockAxis[i] = a[i] / al;
                t.lockHave = true;
            }
            if (!t.lockHave)
            {
                t.lockId = ~0u;
                return false;
            }
            g_lastDoorId = t.lockId;
            memcpy(g_lastDoorP2, t.lockP2, sizeof(g_lastDoorP2));
            memcpy(g_lastDoorAxis, t.lockAxis, sizeof(g_lastDoorAxis));
            events::Push("autopilot travel airlock %u door p2 (%.0f,%.0f,%.0f) axis (%.2f,%.2f,%.2f)", t.lockId,
                         t.lockP2[0], t.lockP2[1], t.lockP2[2], t.lockAxis[0], t.lockAxis[1], t.lockAxis[2]);
        }
        else
        {
            if (g.time - t.lockAskAt > 1.0f)
            {
                t.lockAskAt = g.time;
                space::SetTarget(t.lockId);
            }
            return false;
        }
    }
    // (50 m out: level with the mouth, try 3's height - at 350 m, s9s, the climb took ~650 m more and the 90 deg turn down
    // braked the run to 80 m/s for ~3 s; from the mouth's level the airlock run curves in at speed, f3106c8)
    for (int i = 0; i < 3; ++i)
        stage[i] = t.lockP2[i] + t.lockAxis[i] * 50.0f;
    const float s[3] = {stage[0] - g_obj.flyGoal[0], stage[1] - g_obj.flyGoal[1], stage[2] - g_obj.flyGoal[2]};
    return Len3(s) <= g_obj.flyGoalR * 0.95f;
}

// calm: no hurry (a hold's run, the scenic rest - user, 19 Oct 2026: "idling means the player is not in a hurry", the
// afterburner was not needed): no afterburner, cruise only beyond 3 km, three-fifths throttle.
bool ArriveAt(float dt, const float posIn[3], Out& out, bool calm)
{
    // (the door line's staging point replaces the goal when it lies inside the trip's sphere - AirlockStage)
    float pos[3] = {posIn[0], posIn[1], posIn[2]};
    {
        float st[3];
        if (AirlockStage(st))
            memcpy(pos, st, sizeof(pos));
    }
    fl::Kinematics k = {};
    if (!fl::PlayerKinematics(k))
        return false;
    const float toPos[3] = {pos[0] - k.pos[0], pos[1] - k.pos[1], pos[2] - k.pos[2]};
    const float posDist = Len3(toPos), speed = Len3(k.vel);
    if (posDist < 150 && speed < 30)
        return true;
    // The real line to the point (session 7, 23 Oct 2026, M12 - from generator 3 to the airlock's top: the staging went round
    // the top of the lair's sphere, a 4 km arc at 80 m/s; straight on, the lair's structure stopped the ship and "walled,
    // no progress" ended the trip 1 km short; the user's try 3 flew it on a clear line at y ~400 on the afterburner): the
    // game's ray test for the ship's width over the next ~4 s (at least 600 m: a fly-through's aim lies beyond the point,
    // maybe in structure), 4x a second. Clear: no staging, no solid's sphere avoided (ApplyAvoid). Not clear: the way
    // through the real geometry (GeoPathAim) - steered point by point, braking and the cruise plan by the way left.
    if (g.time - g.arrive.lineClearAt > 0.25f)
    {
        g.arrive.lineClearAt = g.time;
        const float look = std::min(posDist, std::max(600.0f, speed * 4.0f));
        float ahead[3];
        for (int i = 0; i < 3; ++i)
            ahead[i] = k.pos[i] + toPos[i] / std::max(1.0f, posDist) * look;
        g.arrive.lineClear = LineClear(k.pos, ahead, space::PlayerRadius());
    }
    float aimPt[3] = {pos[0], pos[1], pos[2]};
    float dist = posDist; // the way left
    g.arrive.pathOn = false;
    const bool wheelOn = WheelTiming(k, pos, aimPt);
    // A fly-through pushed off its line of approach (session 8, 23 Oct 2026, M12 core: past the wheel's gap ~170 m off the
    // axis, the line to the aim crossed the 90 m hole off centre, the planned way went round the hub at y -395 into
    // structure - 295 -> 70 m/s, 13 s to the cell; the user's try 3 went back onto the axis, the hole dead centre, at 300
    // m/s): back onto the line ahead (1.5 s of speed, at least 300 m) when the way there and on along the line are clear
    bool rejoined = false;
    if (!wheelOn && !g.arrive.lineClear && g.mode == Mode::Travel && g.tr.flyRadius > 0 && posDist > 300)
    {
        const float rel[3] = {k.pos[0] - pos[0], k.pos[1] - pos[1], k.pos[2] - pos[2]};
        // (3 s of speed: at 1.5 s the line back cut across the M12 core hub's rim, the test failed and the rejoin and the
        // planned way took turns for ~10 s; the user's try 3 regains the axis over ~800-1000 m)
        float along = Dot3(rel, g.tr.flyDir) + std::max(300.0f, speed * 3.0f);
        // (under the cruise lock: the farthest point of the line in plain sight, 1500 m ahead at most - a fixed 3 s lead put
        // the point past the M12 core's ~90 m hole and the line to it clipped the hole's rim, s9f-i2; the farthest
        // visible point lies before the hole, and once on the line the line runs through it)
        if (g_obj.cruiseLock && g.time - g.arrive.rejoinAt > 0.25f)
        {
            const float base = Dot3(rel, g.tr.flyDir);
            const float r = space::PlayerRadius();
            // (rejoin_nearest: the nearest point the turning circle reaches, the farther ones after - the user, 24 Oct
            // 2026, M12 core: "fairly wide swerves that bring you close to collisions ... keep itself closer to the center
            // through-line"; s10 base: past the wheel's 200 m ring the far point let the ship run out to 210-240 m and
            // regain the axis only by z -700. An S-curve over an offset h at turn radius R = v / w takes ~sqrt(4 R h))
            float leads[7] = {1500.0f, 1200.0f, 950.0f, 750.0f, 550.0f, 400.0f, 0.0f};
            int nLeads = 6;
            flight::Model rm;
            if (g_set.rejoinNearest && flight::ReadModel(rm) && rm.turnRate > 0)
            {
                const float h = sqrtf(std::max(0.0f, Dot3(rel, rel) - base * base));
                const float minLead = std::max(400.0f, 1.2f * sqrtf(4.0f * (speed / rm.turnRate) * h));
                nLeads = 0;
                leads[nLeads++] = minLead;
                for (const float l : {550.0f, 750.0f, 950.0f, 1200.0f, 1500.0f})
                    if (l > minLead)
                        leads[nLeads++] = l;
            }
            for (int li = 0; li < nLeads; ++li)
            {
                const float lead = leads[li];
                const float a2 = base + lead;
                if (a2 >= 0)
                    continue;
                float q[3];
                for (int i = 0; i < 3; ++i)
                    q[i] = pos[i] + g.tr.flyDir[i] * a2;
                if (LineClear(k.pos, q, r))
                {
                    along = a2;
                    break;
                }
            }
            g.arrive.rejoinLead = along;
        }
        else if (g_obj.cruiseLock && g.arrive.rejoinLead < 0)
            along = g.arrive.rejoinLead; // (the point chosen at the last test, until the next)
        if (along < 0)
        {
            float rj[3];
            for (int i = 0; i < 3; ++i)
                rj[i] = pos[i] + g.tr.flyDir[i] * along;
            if (g.time - g.arrive.rejoinAt > 0.25f)
            {
                const float r = space::PlayerRadius();
                const bool was = g.arrive.rejoinOk;
                g.arrive.rejoinAt = g.time;
                // (the line on for 600 m: the aim itself may lie past the point in structure - M12's core exit)
                float on[3];
                for (int i = 0; i < 3; ++i)
                    on[i] = rj[i] + g.tr.flyDir[i] * std::min(600.0f, -along);
                // (under the cruise lock the line is the way, untested: the planner's lattice ways at cruise circled the
                // M12 core's hub 20 s, re-planned every 2 s, and flew back through the turbine - s9d; try 3 regains the
                // axis by z -500 in one curve; the guard still steers off a wall)
                const bool ok = g_obj.cruiseLock || (LineClear(k.pos, rj, r) && LineClear(rj, on, r));
                // (dropped only after two failed tests in a row: one miss flipped to the planned way and back)
                g.arrive.rejoinMiss = ok ? 0 : g.arrive.rejoinMiss + 1;
                g.arrive.rejoinOk = ok || (was && g.arrive.rejoinMiss < 2);
                if (g.arrive.rejoinOk && !was)
                    events::Push("autopilot travel rejoin the line of approach %.0f m ahead (%.0f m off it)", -along,
                                 Len3(rel) > 0 ? sqrtf(std::max(0.0f, Dot3(rel, rel) - Dot3(rel, g.tr.flyDir) *
                                                                                           Dot3(rel, g.tr.flyDir)))
                                               : 0.0f);
            }
            if (g.arrive.rejoinOk)
                memcpy(aimPt, rj, sizeof(aimPt)), rejoined = true;
        }
    }
    if (!wheelOn && !rejoined && !g.arrive.lineClear && posDist < 8000 && posDist > 300)
    {
        float rest = 0;
        if (GeoPathAim(g.arrive.path, k, pos, 0, aimPt, rest, "arrive"))
            g.arrive.pathOn = true, dist = rest;
        // (noway_climb: no way known yet - out at a climb toward the destination's heading on a line clear for 400 m,
        // until the planner finds its way; the user, 24 Oct 2026, M12 gen 3 -> the lair door: "it can all be afterburned";
        // s10 base: from the kill point in the trench the planner answered "none" 3-4 times, 1.5 s apart, and the ship
        // crawled 4.8-6 s at 78 m/s; the user's try 3 climbed out at ~45 deg at once)
        else if (g_set.nowayClimb)
        {
            if (g.time - g.arrive.nowayAt > 0.25f)
            {
                g.arrive.nowayAt = g.time;
                const bool was = g.arrive.nowayClimbOn;
                g.arrive.nowayClimbOn = false;
                const float hz[3] = {toPos[0], 0.0f, toPos[2]};
                const float hl = Len3(hz);
                const float up = toPos[1] >= 0 ? 1.0f : -1.0f;
                if (hl > 1)
                    for (const float deg : {45.0f, 30.0f, 60.0f})
                    {
                        const float c = cosf(deg / 57.2958f), s = sinf(deg / 57.2958f);
                        float q[3];
                        for (int i = 0; i < 3; ++i)
                            q[i] = k.pos[i] + (hz[i] / hl * c + (i == 1 ? up * s : 0.0f)) * 400.0f;
                        if (LineClear(k.pos, q, space::PlayerRadius()))
                        {
                            if (!was)
                                events::Push("autopilot travel no way yet: climbing out at %.0f deg", deg);
                            g.arrive.nowayClimbOn = true;
                            memcpy(g.arrive.nowayAim, q, sizeof(g.arrive.nowayAim));
                            break;
                        }
                    }
            }
            if (g.arrive.nowayClimbOn)
                memcpy(aimPt, g.arrive.nowayAim, sizeof(aimPt));
        }
    }
    if (g.arrive.pathOn)
        g.arrive.nowayClimbOn = false;
    // ref_line: the reference flight's line replaces the rejoin / passage aims where it applies
    if (!wheelOn)
        RefLineAim(k, pos, aimPt);
    const float to[3] = {aimPt[0] - k.pos[0], aimPt[1] - k.pos[1], aimPt[2] - k.pos[2]};
    const float aimDist = Len3(to);
    flight::Model fm;
    const bool have = flight::ReadModel(fm);
    const float brake = have ? flight::BrakeDistance(fm, speed, 0) + speed * dt : 0.0f;
    if (!g.dock.fastBrakeCommit && dist <= brake)
        g.dock.fastBrakeCommit = true;
    if (g.dock.fastBrakeCommit && speed < 5 && dist > 150)
        g.dock.fastBrakeCommit = false; // stopped short: go on
    float fwd[3];
    geom::Forward(fwd, k.rot);
    const float cosNose = aimDist > 1 ? Dot3(fwd, to) / aimDist : 1.0f;
    // The turning circle (radius speed / turn rate): a point at distance d, bearing a off the nose, is reachable only if
    // d >= 2 R sin a - closer in, slow to d w / (2 sin a) (was: circled a destination at cruise for a minute, 18 Oct
    // 2026).
    const float sinA = std::sqrt(std::max(0.0f, 1 - cosNose * cosNose));
    const bool tooFast = have && fm.turnRate > 0 && sinA > 0.05f && cosNose < 0.99f &&
                         speed > dist * fm.turnRate / (2 * sinA) && dist < 3000;
    const bool braking = g.dock.fastBrakeCommit || tooFast;
    bool wantCruise = false;
    if (have && !braking)
    {
        const flight::RunPlan p = flight::PlanRun(fm, speed, dist, 0, fm.cruiseEngaged ? fm.cruiseChargeElapsed : 0,
                                                  acosf(Clampf(cosNose, -1, 1)));
        wantCruise = fm.cruiseEngaged ? p.cruiseTime < p.burnerTime + 1.0f : p.cruise;
        // no new charge before the nose is on the goal (session 6, M12 core checkpoint: facing the entrance wall, the
        // cruise was requested at once, the guard braked / released it every 0.1 s and the ship crawled backwards 70 s)
        // (...except right after a jump-in: the charge starts at once, the turn runs meanwhile - the speedrun trace, run 1 M12
        // Omicron Minor: charging 0.5 s after the exit, 300 m/s 9 s later; ours ~5-6 s later)
        if (!fm.cruiseEngaged && cosNose < 0.9f && !JustJumpedIn())
            wantCruise = false;
        // ...nor into an airlock's area (user, 23 Oct 2026, M12 gen 3 -> the lair's top: "cruises to the entrance and
        // does a 270 degree loop before entering. Why not just use afterburners from generator 3"; the runners 54 %
        // afterburner + 40 % charging there, try 3 200 m/s level, then the dive): the afterburner, a turn the door allows
        if (g.tr.airlockNear < 0 && g.tr.hasDest)
        {
            g.tr.airlockNear = 0;
            for (const space::Contact& c : Contacts())
            {
                const float d[3] = {c.pos[0] - g.tr.dest[0], c.pos[1] - g.tr.dest[1], c.pos[2] - g.tr.dest[2]};
                if ((c.type & space::kAirlockGate) && Len3(d) < 1500.0f)
                    g.tr.airlockNear = 1;
            }
        }
        if (g.tr.airlockNear == 1 && !g_obj.cruiseLock)
            wantCruise = false;
    }
    // A charge that never completes (session 8, M12 Omicron Minor, three deaths: the Nomads' cruise disruptors reset it -
    // 41 s "charging" at 80 m/s among 9 attackers, the runners cruise 85-100 % of that system): wanted 10 s without
    // reaching cruise speed -> 12 s on the afterburner (200 m/s), then the charge again
    // ...and a charge reset (a disruptor's hit: the charge's elapsed time falls back) under fire burns away at once, 8 s
    // (s9u-i1, M12 Omicron Minor: two resets, 10 s each at 80 m/s among 7 attackers, nanobots 17 -> 0 in 12 s, dead; the
    // runners never sit at 80 m/s there)
    {
        const float el = have && fm.cruiseEngaged ? fm.cruiseChargeElapsed : 0.0f;
        if (wantCruise && g.arrive.chargePrev > 0.5f && el < g.arrive.chargePrev - 0.3f && g_sit.attackers > 0 &&
            g.arrive.chargeOffUntil < g.time)
        {
            g.arrive.chargeOffUntil = g.time + 8.0f;
            g.arrive.chargeWantSince = -1;
            events::Push("autopilot travel cruise charge reset at %.1f s (attackers=%d): afterburner 8 s",
                         g.arrive.chargePrev, g_sit.attackers);
        }
        g.arrive.chargePrev = el;
    }
    if (ChargeFallback(g.arrive.chargeWantSince,
                       wantCruise && have && fm.cruiseSpeed > 0 && speed < 0.6f * fm.cruiseSpeed,
                       !wantCruise || speed >= 0.6f * fm.cruiseSpeed, g.time, 10.0f) &&
        g.arrive.chargeOffUntil < g.time)
    {
        g.arrive.chargeOffUntil = g.time + 12.0f;
        events::Push("autopilot travel cruise charge not completing (%.0f s at %.0f m/s): afterburner 12 s",
                     g.time - g.arrive.chargeWantSince, speed);
    }
    if (g.arrive.chargeOffUntil > g.time)
    {
        wantCruise = false;
        g.arrive.chargeWantSince = -1;
    }
    if (braking || wantCruise || cosNose < 0.906f || !fm.hasBurner)
        g.dock.fastBurn = false;
    else if (cosNose > 0.990f || (g.arrive.chargeOffUntil > g.time && cosNose > 0.95f))
        g.dock.fastBurn = true;
    SteerTo(out, aimPt, braking ? 0.0f : (calm ? 0.6f : 1.0f), 0.35f, wantCruise && (!calm || dist > 3000) ? 1 : 0);
    out.steerReverse = braking && speed > 1;
    out.afterburn = g.dock.fastBurn && !calm;
    // A point inside a solid's avoidance sphere (M12 22 Oct 2026: "Fly to the Waypoint" above the Nomad airlock - the
    // sphere kept the ship 1.1 km off, "arrived-close", it rested in the swarm and died): that solid is the destination's
    // own structure - not avoided on this arrival (as a dock target is not)
    u32 inside = 0;
    float ic[3] = {}, ir = 0;
    for (const space::Obstacle& ob : space::Obstacles(pos, 1))
        if (!ob.zone && ob.id && !ob.ship && !ob.asteroid)
        {
            const float od[3] = {pos[0] - ob.pos[0], pos[1] - ob.pos[1], pos[2] - ob.pos[2]};
            if (Len3(od) < ob.radius)
                inside = ob.id, ir = ob.radius, memcpy(ic, ob.pos, sizeof(ic));
        }
    // ...but its body is still in the way from the wrong side (M12 22 Oct 2026, second run: pinned against the lair's hull
    // 1 km from the waypoint above its airlock, "arrived-close", dead): from more than 25 deg off the point's own bearing
    // from the solid's centre, first round the solid (avoided) to a staging point straight out beyond the point, then in
    // along that radial line.
    // (a way through the real geometry, or a clear line, replaces the staging - above; going along the way is progress)
    g.arrive.staging = g.arrive.pathOn && aimDist + 1 < posDist;
    if (inside && !g.arrive.lineClear && !g.arrive.pathOn)
    {
        float u[3] = {pos[0] - ic[0], pos[1] - ic[1], pos[2] - ic[2]};
        const float s[3] = {k.pos[0] - ic[0], k.pos[1] - ic[1], k.pos[2] - ic[2]};
        const float ul = Len3(u), sl = Len3(s);
        // only from outside the sphere: inside a hollow solid (M12's Nomad core: ship and point both inside) the way
        // is straight across (it was sent back out to the staging point, round and round in the core's radiation)
        if (ul > 1 && sl > ir && Dot3(s, u) / (sl * ul) < 0.906f)
        {
            for (int i = 0; i < 3; ++i)
                out.steerPos[i] = ic[i] + u[i] / ul * (ir + 300.0f);
            out.steerThrottle = 1.0f;
            out.steerReverse = false;
            inside = 0;
            g.arrive.staging = true;
        }
    }
    ApplyAvoid(k, out, inside, g.arrive.lineClear || g.arrive.pathOn || (g_obj.cruiseLock && g.arrive.rejoinOk));
    // Stuck against something (M09 22 Oct 2026: throttle 100 % into an asteroid on the line to the marker, speed -16,
    // 80 s until the nag failed the mission): forward thrust wanted, no forward speed for 1.5 s -> back off and slide
    // sideways for 2 s, then fly on (the avoidance sees the rock from the new angle).
    const float vFwd = Dot3(k.vel, fwd);
    Unstick(out, g.time, vFwd, out.steerThrottle > 0.5f && !braking && dist > 300 && vFwd < 10, 1.5f, 2.0f,
            g.arrive.stuckSince, g.arrive.unstickUntil, g.arrive.unstickDir, "travel");
    return false;
}

// Travel (user, 18 Oct 2026): the game's best path to the destination (or the waypoint list as the game / mission /
// player has it), planned per system by route::Make - lanes ridden from the best ring with the best exit, gates, the
// destination - and flown step by step the player's way: fast lane entries, free flight out of a lane where flying
// on is faster, fast approaches to gates and bases. Plans again after a jump and when a ride ends early.
// A new objective's navmarker reaches the player's waypoint list a few frames after the objective itself (M01b 19 Oct
// 2026: a travel 0.35 s after "Fly to the Waypoint" found the list empty, the next one plotted its own route and the HUD
// showed "Player Waypoint 1" beside the "Mission Waypoint" - the 2020 footage shows the mission's alone). An empty list
// at the start of a travel is waited for (1.5 s of game time, a safety limit: 90 frames were 0.45 s at 200 fps) before
// giving up or planning. Some objectives never fill it (M01b's "Fly to the Waypoint" is a navmarker only): the harness
// then travels to the objective's nav position.
static bool WaitForMissionList(State::Travel& t)
{
    if (!space::Waypoints().empty())
        return false;
    // (in the objective mode the list belongs to the step: waited for only in the step's first 1.5 s - session 8, 23 Oct
    // 2026, M12: the lair's steps have no list, and every trip of the leg - pre-stage, airlock, script goal, leash - paused
    // 1.5 s before it planned, ~10 trips a leg)
    if (g_obj.on && g_obj.stepAt && GetTickCount64() - g_obj.stepAt > 1500)
        return false;
    // ...nor under the cruise lock (s9j-i3, M12 core: the cell's pickup restarted the trip at 300 m/s; 1.5 s unsteered,
    // the guard's swing put the ship into the bar beside the cell, the exit run then looped 58 s)
    if (g_obj.on && g_obj.cruiseLock)
        return false;
    // ...nor for a short trip in this system (under 6 km: no lane pays, the route is the straight line; a mission waypoint
    // that comes later at most doubles the HUD's marker - M12's lair and core steps: 1.5 s each, a5caabb's runs)
    // (a point only: a trip to an object - M05's "Dock with Baxter Research Station" - plotted before the step's own list
    // came, the game answered with none - "found no way" in 0.9 s, the ship fought, "You failed to dock", gate_s8v)
    if (g_obj.on && t.hasDest && !t.destObject && (!t.destSystem || t.destSystem == fl::PlayerSystem()))
    {
        fl::Kinematics k = {};
        if (fl::PlayerKinematics(k))
        {
            const float d[3] = {t.dest[0] - k.pos[0], t.dest[1] - k.pos[1], t.dest[2] - k.pos[2]};
            if (Len3(d) < 6000.0f)
                return false;
        }
    }
    if (t.listWaitSince < 0)
        t.listWaitSince = g.time;
    return g.time < t.listWaitSince + 1.5f;
}

// A ring flown through (State::Travel::passRing): line up on the ring's axis on the ship's side, then straight through
// its centre and on (the next ring's objective starts the next pass). Full throttle, afterburner when lined up, cruise on
// a long straight; the avoidance ignores the ring itself (its hole is the way). A race course's rings are a lane ring's
// model without docking (track_ring: SATELLITE, solar_radius 600).
// The next ring of a course after `ring` (centre c, unit axis, the side the ship is now on): another fly-through ring
// (PassRingRadius) within 45 degrees of the exit axis, the nearest along it; 0 none.
static u32 PredictNextRing(u32 ring, const float c[3], const float axis[3], int side)
{
    u32 best = 0;
    float bestScore = 1e30f;
    for (const space::Contact& o : Contacts())
    {
        if (o.id == ring || !PassRingRadius(o))
            continue;
        const float d[3] = {o.pos[0] - c[0], o.pos[1] - c[1], o.pos[2] - c[2]};
        const float dist = Len3(d);
        if (dist < 1)
            continue;
        const float cosA = Dot3(d, axis) * side / dist;
        if (cosA < 0.707f)
            continue;
        const float score = dist / cosA;
        if (score < bestScore)
            bestScore = score, best = o.id;
    }
    return best;
}

static void PassRing(float dt, State::Travel& t, Out& out)
{
    (void)dt;
    fl::Kinematics k = {};
    float c[3], rot[9];
    if (!fl::PlayerKinematics(k) || !space::ObjectPose(t.passRing, c, rot))
        return;
    float axis[3];
    geom::Forward(axis, rot);
    const float rel[3] = {k.pos[0] - c[0], k.pos[1] - c[1], k.pos[2] - c[2]};
    const float along = Dot3(rel, axis);
    const int side = along >= 0 ? 1 : -1;
    if (!t.passSide)
        t.passSide = side;
    const float lat[3] = {rel[0] - axis[0] * along, rel[1] - axis[1] * along, rel[2] - axis[2] * along};
    const float lateral = Len3(lat);
    // inside this off the axis = through the hole (the game counted a pass 49 m off a ring of physical radius 88)
    const float hole = std::max(40.0f, 0.75f * t.passRadius);
    if (!t.passed && side != t.passSide)
    {
        t.passed = true;
        events::Push("autopilot travel pass ring=%u lateral=%.0f speed=%.0f", t.passRing, lateral, Len3(k.vel));
        // Look-ahead: a course's rings face each other (Br05: ring 2's axis points at ring 4, the next gate), so the
        // next one is the ring of the same kind the exit axis points at - steer for it now, not ~0.4 s later when the
        // objective names it (the supervisor's tick; 120 m at cruise). The objective's own choice still decides.
        if (lateral <= hole)
        {
            const u32 next = PredictNextRing(t.passRing, c, axis, side);
            if (next)
            {
                events::Push("autopilot travel pass next=%u (predicted)", next);
                t.passRing = next;
                t.passed = false;
                t.passSide = 0;
                t.passNextKnown = false;
                return;
            }
        }
    }
    // Line following onto the ring's axis: aim at the axis point a lead distance (speed x 1 s) nearer than we are - far
    // out that merges onto the axis, close in it is past the plane (through the hole). At 300 m/s a fixed line-up point
    // passed ring 4 112 m off the axis (22 Oct 2026). A miss (crossed outside the hole; the objective stays) needs no
    // special case: from the far side the same rule turns back through it.
    if (t.passed && lateral > hole)
    {
        events::Push("autopilot travel pass-missed ring=%u lateral=%.0f: again", t.passRing, lateral);
        t.passed = false;
        t.passSide = side;
    }
    // v2 (22 Oct 2026): a crossing anywhere inside the hole counts, at any angle - so no line-up on the axis (v1 lined up
    // and braked at three corners, lap ~90 s, race lost): pursue a crossing point in the ring's plane, biased toward the
    // next ring (the racing line: the inside of the coming turn), and once close aim just past the plane through it.
    if (!t.passNextKnown)
    {
        t.passNext = PredictNextRing(t.passRing, c, axis, -t.passSide);
        t.passNextKnown = true;
    }
    float cross[3] = {c[0], c[1], c[2]};
    float nc[3];
    if (t.passNext && space::ObjectPos(t.passNext, nc))
    {
        float b[3] = {nc[0] - c[0], nc[1] - c[1], nc[2] - c[2]};
        const float bx = Dot3(b, axis);
        for (int i = 0; i < 3; ++i)
            b[i] -= axis[i] * bx;
        const float bl = Len3(b);
        if (bl > 1)
            for (int i = 0; i < 3; ++i)
                cross[i] += b[i] / bl * 0.5f * hole;
    }
    const float lead = std::max(150.0f, Len3(k.vel) * 0.5f);
    const float a = std::fabs(along);
    float aim[3];
    for (int i = 0; i < 3; ++i)
        aim[i] = a > lead ? cross[i] : cross[i] - axis[i] * side * 150.0f;
    // Never touch another ship on the line (M06 22 Oct 2026: the start puts Hovis 50 m beside us and we flew his line -
    // Cnd_ProjHit dexter, 3, Player counts the bumps: his warning 0280, then 0140 and "You crossed Hovis"): a ship within
    // 250 m pushes the aim sideways (perpendicular to the axis), up to 60 m, inside the hole when the ring is near.
    // Only a ship AHEAD within 150 m can be run into (v1 pushed from everything within 250 m: the start beside Hovis
    // missed ring 4 at 94 m/s, the hostile wingmen chasing us shoved the line off at ring 23 - 22 Oct 2026)
    float push[3] = {};
    const float vl = std::max(1.0f, Len3(k.vel));
    for (const space::Contact& o : Contacts())
    {
        if (!o.ship || o.distance > 150 || o.distance < 1)
            continue;
        float away[3] = {k.pos[0] - o.pos[0], k.pos[1] - o.pos[1], k.pos[2] - o.pos[2]};
        if (-Dot3(away, k.vel) / (vl * o.distance) < 0.5f) // not within 60 degrees ahead of our motion
            continue;
        const float ax = Dot3(away, axis);
        for (int i = 0; i < 3; ++i)
            away[i] -= axis[i] * ax;
        const float l = Len3(away);
        if (l < 1)
            continue;
        const float w = 40.0f * (1.0f - o.distance / 150.0f);
        for (int i = 0; i < 3; ++i)
            push[i] += away[i] / l * w;
    }
    float pl = Len3(push);
    const float maxPush = a < 400 ? std::max(0.0f, hole - 10.0f) : 40.0f;
    if (pl > maxPush && pl > 0)
        for (float& v : push)
            v *= maxPush / pl;
    for (int i = 0; i < 3; ++i)
        aim[i] += push[i];
    const float to[3] = {aim[0] - k.pos[0], aim[1] - k.pos[1], aim[2] - k.pos[2]};
    const float dist = std::max(1.0f, Len3(to));
    float fwd[3];
    geom::Forward(fwd, k.rot);
    const float cosNose = Dot3(fwd, to) / dist;
    flight::Model fm;
    const bool have = flight::ReadModel(fm);
    // A course of rings is flown in cruise (user, 22 Oct 2026: "the player genuinely has to fly, at cruise speed, the
    // best possible path between each ring"): the engine is engaged once roughly lined up and kept through the turns
    // (-1 = leave it); the afterburner would cancel the charge, so only while cruise is not wanted (no model read).
    SteerTo(out, aim, 1.0f, 0.35f, cosNose > 0.8f ? 1 : -1);
    // (thrusters on top of an engaged cruise - the 2020 footage's 359 m/s - measured in race practice 22 Oct 2026: no
    // gain, 300 m/s either way; the switch was removed by the 24 Oct checkup)
    out.afterburn = false;
    // The turning circle (as ArriveAt): a point d away, a off the nose, is reachable only if d >= 2 R sin a, R = v / turn
    // rate. Not reachable at this speed = drop the cruise and brake until it is (M06 22 Oct 2026: a sharp corner at
    // 290 m/s - the ship orbited ring 16 for 30 s, every pass 120-445 m off)
    const float speed = Len3(k.vel);
    const float sinA = std::sqrt(std::max(0.0f, 1 - cosNose * cosNose));
    const bool brake = have && fm.turnRate > 0 && cosNose < 0.98f &&
                       (cosNose < 0 || speed > dist * fm.turnRate / (2 * std::max(0.05f, sinA)));
    if (g.time - t.passTraceAt >= 1.0f) // 1 Hz trace of the race line (lap tuning)
    {
        t.passTraceAt = g.time;
        events::Push("autopilot travel pass-trace ring=%u dist=%.0f off=%.0f speed=%.0f cruise=%d charge=%.1f want=%d "
                     "brake=%d",
                     t.passRing, dist, acosf(Clampf(cosNose, -1, 1)) * 57.3f, speed, have && fm.cruiseEngaged ? 1 : 0,
                     have ? fm.cruiseChargeElapsed : -1.0f, out.cruise, brake ? 1 : 0);
    }
    if (brake != t.passBraking)
    {
        t.passBraking = brake;
        events::Push("autopilot travel pass-%s ring=%u dist=%.0f off=%.0f speed=%.0f turn=%.2f", brake ? "brake" : "go",
                     t.passRing, dist, acosf(Clampf(cosNose, -1, 1)) * 57.3f, speed, have ? fm.turnRate : 0.0f);
    }
    if (brake)
    {
        out.cruise = 0;
        out.afterburn = false;
        out.steerThrottle = 0.0f;
        out.steerReverse = speed > 60;
    }
    ApplyAvoid(k, out, t.passRing);
}

// A travel to a loot object (an objective's mark: M12's Nomad Power Cell at the core's centre, which the ship cannot
// reach - 770 m was the closest, 22 Oct 2026): tractor it as soon as it is within the beam's reach, whatever its
// value (the combat reflex skips loot below loot_min_value).
static void TractorDestination(float dt, const State::Travel& t, Out& out)
{
    g.tractorTimer -= dt;
    if (!t.destObject || g.tractorTimer > 0)
        return;
    const space::Contact dc = FindContact(t.destObject);
    if (!dc.id || !(dc.type & space::kLoot))
        return;
    ai::Self s;
    const float reach = ai::ReadSelf(s) && s.tractorRange > 0 ? s.tractorRange : 1500.0f;
    if (dc.distance < reach)
    {
        out.collectAll = true;
        g.tractorTimer = 1.5f;
        events::Push("autopilot travel tractor destination=%u dist=%.0f reach=%.0f", dc.id, dc.distance, reach);
    }
}

void UpdateTravel(float dt, Out& out)
{
    State::Travel& t = g.tr;
    // just jumped in with a far destination: the charge while the route is still being plotted (JustJumpedIn)
    if (JustJumpedIn() && t.hasDest && !t.passRing)
    {
        fl::Kinematics k = {};
        if (fl::PlayerKinematics(k))
        {
            const float d[3] = {t.dest[0] - k.pos[0], t.dest[1] - k.pos[1], t.dest[2] - k.pos[2]};
            if (Len3(d) > 3000.0f && (!t.destSystem || t.destSystem == fl::PlayerSystem()))
                out.cruise = 1;
        }
    }
    TractorDestination(dt, t, out);
    if (t.passRing)
        return PassRing(dt, t, out);
    space::Pilot p;
    if (!space::ReadPilot(p))
        return;
    if (g.dock.fastRequested && g.dock.fastLined && !p.jumping && !p.lane)
        QueuedShortcut(); // the server's answer to our request: queued = the shortcut's moment
    switch (t.phase)
    {
    case State::Travel::Plot:
        if (p.jumping)
            return;
        if (WaitForMissionList(t))
        {
            // (wait_flies: on at the destination meanwhile - the user, 24 Oct 2026, M12 gen 3 -> the lair door: "an
            // unnecessary slowdown"; s9z-i2: the airlock trip's 1.5 s wait let the ship coast 200 -> 81 m/s)
            fl::Kinematics k = {};
            if (g_set.waitFlies && t.hasDest && (!t.destSystem || t.destSystem == fl::PlayerSystem()) &&
                fl::PlayerKinematics(k))
            {
                const float to[3] = {t.dest[0] - k.pos[0], t.dest[1] - k.pos[1], t.dest[2] - k.pos[2]};
                const float d = Len3(to);
                float fwd[3];
                geom::Forward(fwd, k.rot);
                out.steerTo = true;
                memcpy(out.steerPos, t.dest, sizeof(out.steerPos));
                out.steerFullAngle = 0.35f;
                out.steerThrottle = 1.0f;
                // (the burner within 45 deg: s10-lair, the airlock trip's door lay ~35 deg below the nose - burner off,
                // 200 -> 85 m/s in 0.8 s of the wait)
                out.afterburn = d > 1 && Dot3(fwd, to) / d > 0.7f;
                ApplyAvoid(k, out, t.destObject);
            }
            return;
        }
        // No duplicate of a waypoint the player already has (user, 19 Oct 2026: a route ending on top of the mission's
        // waypoint): a destination within 1.5 km of the list's last entry follows that list as it is.
        {
            const std::vector<space::Waypoint> wl = space::Waypoints();
            if (!wl.empty() && (!t.destSystem || wl.back().system == t.destSystem || wl.back().system == p.system))
            {
                const float d[3] = {wl.back().pos[0] - t.dest[0], wl.back().pos[1] - t.dest[1],
                                    wl.back().pos[2] - t.dest[2]};
                if (Len3(d) < 1500)
                {
                    events::Push("autopilot travel follows the existing waypoints (destination is their end)");
                    t.phase = State::Travel::Plan;
                    return;
                }
            }
        }
        // The route replaces the player's waypoint list: save it, put it back when the travel ends.
        space::SaveWaypoints();
        t.plotted = true;
        t.listBefore = ListSeqs();
        t.plotFrame = g.frames;
        if (!space::PlotBestPath(t.dest, t.destObject, t.destSystem))
        {
            Stop(StopReason::NoBestPath);
            return;
        }
        t.phase = State::Travel::Await;
        return;
    case State::Travel::Await:
    {
        // The server's answer replaces the list within a few frames (frames, not time: a safety limit of 120).
        const std::vector<u32> now = ListSeqs();
        if (!now.empty() && now != t.listBefore)
            t.phase = State::Travel::Plan;
        else if (g.frames > t.plotFrame + 120)
        {
            if (now.empty())
            {
                Stop(StopReason::NoWay);
                return;
            }
            t.phase = State::Travel::Plan; // the same route as before
        }
        return;
    }
    case State::Travel::Plan:
    {
        flight::Model fm;
        flight::ReadModel(fm);
        fl::Kinematics k = {};
        const float speed = fl::PlayerKinematics(k) ? Len3(k.vel) : 0.0f;
        if (!t.hasDest && WaitForMissionList(t))
            return;
        t.plan = route::Make(fm, p.pos, speed, space::Waypoints(), p.system, t.closedRings);
        // A gate / hole as the destination is gone through (sweep navigation 20 Oct 2026: the game's list ends on its
        // position without the object, the plan stopped beside the West Point gate - "Arrive" - and never jumped).
        if (t.plan.ok && !t.noDock && t.destObject && !t.plan.steps.empty() &&
            t.plan.steps.back().kind == route::Kind::Arrive)
        {
            const space::Contact dc = FindContact(t.destObject);
            if (dc.id && (dc.type & (space::kJumpGate | space::kJumpHole)))
            {
                route::Step& last = t.plan.steps.back();
                last.kind = route::Kind::Jump;
                last.object = t.destObject;
                events::Push("autopilot travel destination %u is a jump: the last step jumps", t.destObject);
            }
            // A dock wanted at an object that does not read as dockable until the mission opens it (M13 22 Oct 2026:
            // the Dyson sphere's airlock - granted by the script's Act_DockRequest, yet the game does not fly the player
            // in; the arrival stopped beside it and the ship fought outside until the FAIL): the last step docks (the
            // game's own dock maneuver at the object).
            else if (dc.id && !dc.ship && !(dc.type & space::kPlanet))
            {
                route::Step& last = t.plan.steps.back();
                last.kind = route::Kind::Dock;
                last.object = t.destObject;
                events::Push("autopilot travel destination %u: the last step docks", t.destObject);
            }
        }
        if (t.noDock)
            route::ArriveInstead(t.plan, p.pos);
        if (!t.plan.ok)
        {
            Stop(StopReason::NoPlan);
            return;
        }
        // A base that is hostile to the player refuses the dock (nnv_dock_denied_destination_hostile; Li02_06 at
        // attitude -0.65, 18 Oct 2026) and its defenders shoot: do not fly there. -0.6 is Freelancer's hostile cutoff
        // (the HUD's red); the game's own refusal is still handled when this is wrong (the attitude is in that event).
        if (!t.plan.steps.empty() && t.plan.steps.back().kind == route::Kind::Dock)
        {
            float att = 0;
            // (the objective mode docks only where the mission says: M09 Tekagi's base reads hostile until the dock starts)
            if (!ObjectiveOn() && space::AttitudeToPlayer(t.plan.steps.back().object, att) && att <= -0.6f)
            {
                char why[96];
                sprintf_s(why, "travel: the destination is hostile (attitude %.2f)", att);
                Stop(StopReason::Hostile, why);
                return;
            }
        }
        t.system = p.system;
        t.step = 0;
        t.stepBegun = false;
        t.planStart = g.time;
        std::string line = t.plan.text;
        for (char& c : line)
            if (c == '\n')
                c = '|';
        events::Push("autopilot travel plan %s", line.c_str());
        // The best path was only asked for to plan: the player's list goes back at once, so the HUD never shows our
        // route as "Player Waypoint"s beside the mission's marker (user, 19 Oct 2026; the 2020 footage of M01b shows
        // the Mission Waypoint alone - a navmarker the mission never puts into the list).
        if (t.plotted)
        {
            events::Push("autopilot travel route kept off the HUD (list restored %d)", space::RestoreWaypoints(true));
            t.plotted = false;
        }
        t.phase = State::Travel::Fly;
        return;
    }
    case State::Travel::Fly: break;
    }
    if (t.step >= t.plan.steps.size())
    {
        Stop(StopReason::Arrived);
        return;
    }
    const route::Step& s = t.plan.steps[t.step];
    if (!t.stepBegun)
    {
        ResetApproach();
        g.target = s.object;
        g.dock.laneTowardPlan = s.toward;
        t.stepBegun = true;
        t.riding = t.exitSent = false;
        t.stepStart = g.time;
        t.jumpInsBefore = HostJumpInCount();
        t.restAt = -1;
        t.arriveBest = 1e30f, t.arriveBestAt = g.time;
        events::Push("autopilot travel step=%u kind=%d object=%u planned-eta=%.1f at=%.1f", static_cast<u32>(t.step),
                     static_cast<int>(s.kind), s.object, s.eta, g.time - t.planStart);
    }
    auto stepDone = [&]
    {
        events::Push("autopilot travel step-done=%u at=%.1f planned-eta=%.1f detours=%d blocked-frames=%u "
                     "cruise-drop-frames=%u",
                     static_cast<u32>(t.step), g.time - t.planStart, s.eta, g.avoid.detours, g.avoid.frames,
                     g.avoid.cruiseDrops);
        ++t.step;
        t.stepBegun = false;
    };
    // The approach of a lane / gate / base: the player-style run, else the game's own dock maneuver.
    auto approach = [&]
    {
        if (FastApproach(dt, out) || g.dock.fastRequested)
            return;
        space::SetTarget(s.object);
        MarkSubmit();
        ai::Maneuver(ai::OpDock, false);
        g.dock.fastRequested = true;
        g.dock.fastRequestAt = g.time;
        events::Push("autopilot travel game-dock object=%u (no fast approach)", s.object);
    };
    switch (s.kind)
    {
    case route::Kind::Arrive:
    {
        // a loot destination is taken by the beam from its reach: flown through, never braked for (the user's M12 core:
        // "tractors the Nomad Power Cell without slowing down")
        if (t.flyRadius <= 0 && t.destObject)
        {
            const space::Contact dc = FindContact(t.destObject);
            fl::Kinematics k = {};
            if (dc.id && (dc.type & space::kLoot) && fl::PlayerKinematics(k))
            {
                const float d[3] = {dc.pos[0] - k.pos[0], dc.pos[1] - k.pos[1], dc.pos[2] - k.pos[2]};
                const float dl = std::max(1.0f, Len3(d));
                for (int i = 0; i < 3; ++i)
                    t.flyDir[i] = d[i] / dl;
                ai::Self me;
                t.flyRadius = ai::ReadSelf(me) && me.tractorRange > 0 ? me.tractorRange : 1500.0f;
                events::Push("autopilot travel loot %u flown through (beam reach %.0f m)", dc.id, t.flyRadius);
            }
        }
        // fly-through (the objective changes inside the script's radius): aim past the marker, never brake for it
        float aim[3] = {s.pos[0], s.pos[1], s.pos[2]};
        if (t.flyRadius > 0)
            for (int i = 0; i < 3; ++i)
                aim[i] = s.pos[i] + t.flyDir[i] * (t.flyAhead > 0 ? t.flyAhead : std::max(1000.0f, t.flyRadius));
        if (ArriveAt(dt, aim, out))
        {
            stepDone();
            Stop(StopReason::Arrived);
            return;
        }
        // Close by but no progress (an arrival beside a planet the avoidance keeps the ship off - M03 sweep 19 Oct
        // 2026: 83 s short of the point beside CA Minor, the mission failed "fly to Planet California Minor"): within
        // 2 km with no 50 m gained in 8 s = arrived.
        fl::Kinematics k = {};
        if (fl::PlayerKinematics(k))
        {
            const float d[3] = {s.pos[0] - k.pos[0], s.pos[1] - k.pos[1], s.pos[2] - k.pos[2]};
            const float dist = Len3(d);
            if (dist < t.arriveBest - 50 || g.arrive.staging) // (going round a solid first is progress)
                t.arriveBest = std::min(dist, t.arriveBest), t.arriveBestAt = g.time;
            else if (dist < 2000 && g.time - t.arriveBestAt > 8)
            {
                // only where a solid keeps us off the point (its sphere, with the planets' lethal zones, reaches within
                // the shortfall of it); elsewhere press on (M12 22 Oct 2026: "arrived" 1371 m short of the lair's
                // waypoint while under fire - the script wants 500 m - and the ship waited there to its death)
                bool walled = false;
                for (const space::Obstacle& o : space::Obstacles(s.pos, dist + 3000))
                {
                    if (o.ship || o.asteroid)
                        continue;
                    const float e[3] = {o.pos[0] - s.pos[0], o.pos[1] - s.pos[1], o.pos[2] - s.pos[2]};
                    walled = walled || Len3(e) - o.radius < dist;
                }
                if (walled)
                {
                    events::Push("autopilot travel arrived-close dist=%.0f (no progress for 8 s)", dist);
                    stepDone();
                    Stop(StopReason::Arrived);
                    return;
                }
                events::Push("autopilot travel no progress for 8 s at dist=%.0f, no solid at the point: replanning",
                             dist);
                t.arriveBest = 1e30f, t.arriveBestAt = g.time;
                TravelReplan("no progress");
            }
            // ...and far out: 20 s without 50 m gained (M13 22 Oct 2026: a 67 s trip in the Nomad city ran 26 min with
            // no event) - re-plan; the re-plan limit ends it
            else if (dist >= 2000 && g.time - t.arriveBestAt > 20)
            {
                events::Push("autopilot travel no progress for 20 s at dist=%.0f: replanning", dist);
                t.arriveBest = 1e30f, t.arriveBestAt = g.time;
                TravelReplan("no progress (far)");
            }
        }
        return;
    }
    case route::Kind::Lane:
    {
        if (!g.dock.fastRequested)
        {
            approach();
            return;
        }
        if (!t.riding)
        {
            if (p.lane)
            {
                t.riding = true;
                float next[3];
                memcpy(t.laneOrigin, s.pos, sizeof(t.laneOrigin));
                if (space::ObjectPos(s.toward, next))
                {
                    const float d[3] = {next[0] - s.pos[0], next[1] - s.pos[1], next[2] - s.pos[2]};
                    const float l = Len3(d);
                    for (int i = 0; i < 3; ++i)
                        t.laneDir[i] = l > 0 ? d[i] / l : 0;
                }
                events::Push("autopilot travel ride ring=%u toward=%u at=%.1f", s.object, s.toward,
                             g.time - t.planStart);
            }
            else if (p.op == ai::OpFreeFlight && SubmitSettled())
            {
                // Refused outright (the dock op ends at once): closed for this trip too (was: re-planned the same
                // lane again and again, M02 after a checkpoint load, 18 Oct 2026).
                t.closedRings.push_back(s.object);
                events::Push("autopilot travel lane-closed ring=%u (refused)", s.object);
                TravelReplan("lane entry refused");
            }
            else
            {
                // A lane the mission has closed to the player (PlayerCanTradelane): the dock op runs but the ride never
                // starts and no refusal line comes (M02 Li03, 18 Oct 2026). The game starts a ride 0.4-3.4 s after
                // the ship comes to rest at the start point; resting there 3 s longer = closed for this trip.
                fl::Kinematics k = {};
                bool atStart = false;
                if (fl::PlayerKinematics(k))
                {
                    const float d[3] = {k.pos[0] - g.dock.bayP2[0], k.pos[1] - g.dock.bayP2[1],
                                        k.pos[2] - g.dock.bayP2[2]};
                    atStart = Len3(k.vel) < 5 && Len3(d) < 30;
                }
                if (!atStart)
                    t.restAt = -1;
                else if (t.restAt < 0)
                    t.restAt = g.time;
                else if (g.time - t.restAt > 3.0f)
                {
                    t.closedRings.push_back(s.object);
                    events::Push("autopilot travel lane-closed ring=%u (no ride at the start point)", s.object);
                    FreeFlightNow();
                    TravelReplan("lane closed");
                }
            }
            return;
        }
        const float rel[3] = {p.pos[0] - t.laneOrigin[0], p.pos[1] - t.laneOrigin[1], p.pos[2] - t.laneOrigin[2]};
        const float along = Dot3(rel, t.laneDir);
        // Learn what this lane really runs at once the ride has run long enough to reach its top speed (the ramp
        // takes 8 s at the game's law): a lane mod's per-lane speed then plans every later trip right.
        const auto learnRide = [&]
        {
            if (t.rideTop > 100 && g.time - t.rideBegan > 10)
            {
                const float before = route::LearnedLaneSpeed(s.object);
                route::LearnLaneSpeed(s.object, t.rideTop);
                if (std::fabs(before - t.rideTop) > 25)
                    events::Push("autopilot travel lane-speed ring=%u measured=%.0f law=%.0f", s.object, t.rideTop,
                                 route::Law().speed);
            }
            t.rideTop = 0;
            t.rideWinStart = -1;
        };
        if (p.lane)
        {
            if (t.rideWinStart < 0)
            {
                t.rideWinStart = g.time;
                t.rideWinAlong = along;
                if (t.rideBegan <= 0 || t.rideTop == 0)
                    t.rideBegan = g.time;
            }
            else if (g.time - t.rideWinStart >= 1.0f)
            {
                t.rideTop = std::max(t.rideTop, (along - t.rideWinAlong) / (g.time - t.rideWinStart));
                t.rideWinStart = g.time;
                t.rideWinAlong = along;
            }
            // Leave by free flight where the plan says (one frame of ride travel early: 2500 m/s x dt).
            if (s.exitAt >= 0 && !t.exitSent && along >= s.exitAt - route::Law().speed * dt)
            {
                learnRide();
                FreeFlightNow();
                t.exitSent = true;
                events::Push("autopilot travel lane-exit along=%.0f planned=%.0f at=%.1f", along, s.exitAt,
                             g.time - t.planStart);
            }
            return;
        }
        learnRide(); // the ride has ended (no-op when already learned at the exit)
        if (!t.exitSent && along < s.laneLength - 1500)
        {
            // The ride stops at the disrupted point (the Neural Net says "trade lane disrupted"): the ring where it
            // stopped stays down, so the re-plan must not enter there again - it enters beyond it or flies (sweep
            // navigation 20 Oct 2026: re-entered at that ring 2.5 km on, disrupted again, then flew 27 km direct).
            route::Lane lane;
            if (route::LaneBetween(s.object, s.endRing, lane))
            {
                size_t best = lane.rings.size();
                float bestD = 4000;
                for (size_t i = 0; i < lane.rings.size(); ++i)
                {
                    const float d[3] = {lane.x[i] - p.pos[0], lane.y[i] - p.pos[1], lane.z[i] - p.pos[2]};
                    if (Len3(d) < bestD)
                    {
                        bestD = Len3(d);
                        best = i;
                    }
                }
                if (best < lane.rings.size())
                {
                    t.closedRings.push_back(lane.rings[best]);
                    events::Push("autopilot travel lane-disrupted ring=%u dist=%.0f along=%.0f of %.0f",
                                 lane.rings[best], bestD, along, s.laneLength);
                }
            }
            TravelReplan("the ride ended early (disrupted?)");
            return;
        }
        if (p.op != ai::OpFreeFlight)
            return; // the end of the ride is still going on
        stepDone();
        return;
    }
    case route::Kind::Jump:
        if (p.system != t.system)
        {
            // The system id changes first; the ship keeps its old coordinates and goes through the tunnel until the game
            // places it at the arrival point ~3 s later (Li01 -> Li02, 18 Oct 2026: a best path asked for in between
            // was planned from the old coordinates - a lane 100 km away). Arrived = the game's own end of the jump: the
            // client reports JumpInComplete to the server (host API v8: a hook counts the reports; CShip::is_jumping
            // stayed false the whole jump). Older host: moved since the change and within 5 km of a gate / hole.
            const int jumpIns = HostJumpInCount();
            if (jumpIns >= 0)
            {
                if (jumpIns <= t.jumpInsBefore)
                    return;
            }
            else
            {
                if (!t.changeSeen)
                {
                    t.changeSeen = true;
                    memcpy(t.changePos, p.pos, sizeof(t.changePos));
                    return;
                }
                const float moved[3] = {p.pos[0] - t.changePos[0], p.pos[1] - t.changePos[1],
                                        p.pos[2] - t.changePos[2]};
                if (p.jumping || Len3(moved) < 2000)
                    return;
                bool atGate = false;
                for (const space::Contact& c : Contacts())
                    if ((c.type & (space::kJumpGate | space::kJumpHole)) && c.distance < 5000)
                        atGate = true;
                if (!atGate)
                    return;
            }
            t.changeSeen = false;
            // In the new system: after the jump-in the op still reads dock with no target (docs "Jumps"): free flight,
            // then the route again from here.
            events::Push("autopilot travel jumped system=%u at=%.1f planned-eta=%.1f", p.system, g.time - t.planStart,
                         s.eta);
            g_jumpInTick = GetTickCount64();
            if (g_obj.cruiseLock)
                events::Push("autopilot cruise lock off (jumped)");
            g_obj.cruiseLock = false;
            FreeFlightNow();
            ResetApproach();
            if (t.destObject && s.object == t.destObject) // the gate / hole was the destination: through it = arrived
            {
                Stop(StopReason::Arrived);
                return;
            }
            t.phase = t.hasDest ? State::Travel::Plot : State::Travel::Plan;
            t.stepBegun = false;
            return;
        }
        if (!g.dock.fastRequested)
        {
            approach();
            return;
        }
        // (not while our queue shortcut's formation runs: its refusal / hand-back reads as free flight - 20 Oct 2026, a
        // queued try right after a request was taken for "jump refused" and the travel planned the gate again)
        if (!p.jumping && p.op == ai::OpFreeFlight && SubmitSettled() && !ai::DockInProgress() &&
            g.dock.formationAt <= 0)
            TravelReplan("jump refused");
        return;
    case route::Kind::Dock:
        // An airlock flown through (FastApproach, no request): the server's system switch is the transit (airlock_in
        // places the ship inside at full throttle). Through the destination = arrived; else the route from here.
        if (!g.dock.fastRequested && p.system != t.system)
        {
            events::Push("autopilot travel airlock transit system=%u at=%.1f planned-eta=%.1f", p.system,
                         g.time - t.planStart, s.eta);
            if (g_obj.on && !g_obj.cruiseLock)
            {
                g_obj.cruiseLock = true;
                events::Push("autopilot cruise lock on (an airlock transit: kept to the next jump / dock)");
            }
            ResetApproach();
            if (t.destObject && s.object == t.destObject)
            {
                Stop(StopReason::Arrived);
                return;
            }
            t.phase = t.hasDest ? State::Travel::Plot : State::Travel::Plan;
            t.stepBegun = false;
            return;
        }
        if (!g.dock.fastRequested)
        {
            approach();
            return;
        }
        // dock_regain: a granted dock losing ground (a moving ship) - dropped, the approach again next frame
        if (DockRegain(dt, s.object))
            return;
        // Docked: the controller stops the autopilot ("docked"); a refusal leaves the ship in free flight.
        if (p.op == ai::OpFreeFlight && SubmitSettled() && !ai::DockInProgress() && !p.lane)
        {
            float att = 2;
            space::AttitudeToPlayer(s.object, att);
            events::Push("autopilot travel dock-refused object=%u attitude=%.2f", s.object, att); // data for the cutoff
            Stop(StopReason::DockRefused);
        }
        return;
    }
}

// ------------------------------------------------------------------ shoot (depot pods)
// The money layer's free cargo (22 Oct 2026, docs\LEGS.md "Free-time legs"; speedrun route: the Fort Bush H-Fuel depot,
// 4 pods x 16 H-Fuel, 300 each in net worth): a destroyable depot's cargo pods are equipment on hardpoints
// HpCargo01..; shot open they drop their cargo as loot. Each pod is shot from ~300 m with the nose exactly on it (the
// guns fire where the reticle is - the screen centre while steering), until loot appears near it or 15 s pass; the
// loot is tractored as it comes within the beam's reach.
static int LootNear(const float p[3], float r)
{
    int n = 0;
    for (const space::Contact& c : Contacts())
        if (c.type & space::kLoot)
        {
            const float d[3] = {c.pos[0] - p[0], c.pos[1] - p[1], c.pos[2] - p[2]};
            n += Len3(d) < r ? 1 : 0;
        }
    return n;
}

bool StartShootPods(u32 depot)
{
    if (!depot || !space::SetTarget(depot))
        return false;
    std::vector<float> pts;
    for (int i = 1; i <= 8; ++i)
    {
        char name[16];
        sprintf_s(name, "HpCargo%02d", i);
        float p[3];
        if (space::TargetHardpoint(name, p))
            pts.insert(pts.end(), {p[0], p[1], p[2]});
    }
    if (pts.empty() || !Start(Mode::Shoot, depot, nullptr))
        return false;
    g.sh = State::Shoot{};
    g.sh.pts = pts;
    events::Push("autopilot shoot depot=%u pods=%u", depot, static_cast<u32>(pts.size() / 3));
    return true;
}

bool StartShootObject(u32 id)
{
    if (!id || !FindContact(id).id)
        return false;
    space::SetTarget(id); // (the HUD shows what is shot; the guns fire where the nose points)
    if (!Start(Mode::Shoot, id, nullptr))
        return false;
    g.sh = State::Shoot{};
    g.sh.obj = id;
    events::Push("autopilot shoot object=%u", id);
    return true;
}

// One object to destroy, followed live (a cargo pod drifts): closed to ~300 m, the nose exactly on it, fired; nothing
// tractored here (the frame also drops any tractor-all while a marked pod to destroy is within the beam).
static void UpdateShootObject(Out& out)
{
    State::Shoot& s = g.sh;
    const space::Contact c = FindContact(s.obj);
    if (!c.id)
    {
        events::Push("autopilot shoot object=%u gone", s.obj);
        return Stop(StopReason::TargetDestroyed);
    }
    fl::Kinematics k = {};
    if (!fl::PlayerKinematics(k))
        return;
    const float to[3] = {c.pos[0] - k.pos[0], c.pos[1] - k.pos[1], c.pos[2] - k.pos[2]};
    const float dist = std::max(1.0f, Len3(to));
    float fwd[3];
    geom::Forward(fwd, k.rot);
    const float cosNose = Dot3(fwd, to) / dist;
    const float speed = Len3(k.vel);
    const bool inRange = dist < 400;
    SteerTo(out, c.pos, inRange ? 0.0f : 1.0f, 0.15f, dist > 3000 && cosNose > 0.95f ? 1 : 0);
    out.steerReverse = inRange && (speed > 15 || dist < 150);
    if (!inRange)
        ApplyAvoid(k, out, s.obj);
    out.fire = dist < 700 && cosNose > 0.9994f; // ~2 deg: a pod at 300 m
}

void UpdateShoot(float dt, Out& out)
{
    State::Shoot& s = g.sh;
    if (s.obj)
        return UpdateShootObject(out);
    // tractor what is in reach meanwhile (the hold takes what fits)
    g.tractorTimer -= dt;
    if (g.tractorTimer <= 0)
    {
        ai::Self self;
        const float reach = ai::ReadSelf(self) && self.tractorRange > 0 ? self.tractorRange : 1500.0f;
        for (const space::Contact& c : Contacts())
            if ((c.type & space::kLoot) && c.distance < reach)
            {
                out.collectAll = true;
                g.tractorTimer = 1.5f;
                break;
            }
    }
    fl::Kinematics k = {};
    if (!fl::PlayerKinematics(k))
        return;
    // The depot's owners take each hit as an attack (Li01 22 Oct 2026: four pods shot, the DSE depot read -1 and the
    // police and pirates around came for us - 1.7 x our hit points lost): its attitude is watched, and no further pod is
    // begun once it has fallen 0.3 from where it was or below 0.2 (speedrun notes: "only 2 pods unless an enemy is within
    // 2.4 km"; FT1 needs one crate: 16 H-Fuel x 300 in net worth for a gate of 3500).
    if (g.time - s.attAt >= 1.0f)
    {
        float att;
        if (space::AttitudeToPlayer(g.target, att))
        {
            if (s.att0 > 1.5f)
                s.att0 = att;
            if (fabsf(att - s.att) > 0.05f)
                events::Push("autopilot shoot depot attitude=%.2f (start %.2f)", att, s.att0);
            s.att = att;
        }
        s.attAt = g.time;
    }
    if (s.i * 3 < s.pts.size() && s.since < 0 && s.i > 0 && s.att0 < 1.5f && (s.att < s.att0 - 0.3f || s.att < 0.2f))
    {
        events::Push("autopilot shoot stops before pod %u: depot attitude %.2f (start %.2f)", static_cast<u32>(s.i),
                     s.att, s.att0);
        s.i = s.pts.size() / 3;
    }
    if (s.i * 3 >= s.pts.size())
    {
        // every pod shot: the crates drift off - fly to the nearest one outside the tractor's reach (within 3 km), rest
        // inside it while the tractor pulls; done when none is left or after 40 s
        if (s.since < 0)
            s.since = g.time;
        ai::Self self;
        const float reach = ai::ReadSelf(self) && self.tractorRange > 0 ? self.tractorRange : 1500.0f;
        const space::Contact* near = nullptr;
        const std::vector<space::Contact> all = Contacts();
        for (const space::Contact& c : all)
            if ((c.type & space::kLoot) && c.distance < 3000 && c.lootFits != 0 &&
                (!near || c.distance < near->distance))
                near = &c;
        if (!near || g.time - s.since > 40)
            return Stop(StopReason::PodsDone);
        if (near->distance > reach * 0.8f)
        {
            SteerTo(out, near->pos, 1.0f, 0.35f, 0);
        }
        else
            out.brake = true;
        return;
    }
    const float* p = &s.pts[s.i * 3];
    const float to[3] = {p[0] - k.pos[0], p[1] - k.pos[1], p[2] - k.pos[2]};
    const float dist = std::max(1.0f, Len3(to));
    float fwd[3];
    geom::Forward(fwd, k.rot);
    const float cosNose = Dot3(fwd, to) / dist;
    const float speed = Len3(k.vel);
    const bool inRange = dist < 500;
    SteerTo(out, p, inRange ? 0.0f : 1.0f, 0.15f, dist > 3000 && cosNose > 0.95f ? 1 : 0);
    out.steerReverse = inRange && (speed > 15 || dist < 200);
    if (!inRange)
        ApplyAvoid(k, out, g.target);
    if (inRange && s.since < 0)
    {
        s.since = g.time;
        s.lootBefore = static_cast<u32>(LootNear(p, 200));
    }
    out.fire = inRange && cosNose > 0.9994f; // ~2 deg: a pod at 300 m
    const bool dropped = s.since >= 0 && static_cast<u32>(LootNear(p, 200)) > s.lootBefore;
    if (dropped || (s.since >= 0 && g.time - s.since > 15))
    {
        events::Push("autopilot shoot pod=%u %s after %.1f s", static_cast<u32>(s.i), dropped ? "opened" : "given up",
                     g.time - s.since);
        ++s.i;
        s.since = -1;
    }
}

} // namespace autopilot
