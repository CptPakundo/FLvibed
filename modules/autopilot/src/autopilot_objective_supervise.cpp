// Advanced Autopilot - the objective's supervisor: twice a second, decide again when the objective changes, what it started has
// ended, or a fight comes to it. Also the spawn memory of the menus and the pre-aggro watch.
#include "autopilot_impl.h"
#include "game/flhash.h"
#include "game/mission_live.h"
#include "autopilot_ashride.h"
#include "lookahead.h"
#include "situation.h"
#include "situation_exec.h"
#include "autopilot_objective_impl.h"

namespace autopilot
{
// Twice a second while the objective mode is on (from Update, before the modes run): decide again when the objective
// changes, when what it started has ended, or when a fight comes to us.
// The newest spawn when the game was last in its menu (a load or a new game passes through it): the host's spawn list
// spans the whole process, and a mission's ships spawn at the same positions in every attempt - an earlier attempt's ids
// (perhaps reused by the game for other ships) are not this game's (review, session 12)
static SpawnRec s_menuSpawn = {};
static bool s_menuSpawnSet = false;
void NoteMenu()
{
    static std::vector<SpawnRec> buf(2048);
    const int n = HostSpawnedShips(buf.data(), static_cast<int>(buf.size()));
    s_menuSpawnSet = n > 0;
    if (n > 0)
        s_menuSpawn = buf[n - 1];
    // the mission-scope survival memory belongs to the game that ended (audit A1-2: M04 reloaded from before
    // its jump kept g_survivalEnded under the same title - the stance never came back)
    g_survivalTitle = 0, g_survivalMission = false, g_survivalSystem = 0, g_survivalEnded = false;
    ResetSystemEntry();
}
// (read only: the situation snapshot's "spawns since the last menu", situation_snap.cpp)
bool MenuSpawn(SpawnRec& out)
{
    out = s_menuSpawn;
    return s_menuSpawnSet;
}

// pre_aggro: once a second, the mission ships the step's script is about to turn hostile (read again when the step's
// text, the system or the spawns change) -> SetPreHostile (autopilot.cpp); neutral, near us, they read as hostile to every
// fight test (the overlay of autopilot::Contacts)
static void PreAggroUpdate()
{
    static ULONGLONG next = 0;
    static u32 lastText = 0, lastSystem = 0, lastSpawn = 0;
    static float lastRange = -1;
    static std::vector<SpawnRec> buf(2048);
    const ULONGLONG now = gameclock::NowMs64();
    if (now < next)
        return;
    next = now + 1000;
    const bool on = g_obj.on && g_set.preAggro > 0 && Knows(Source::Hidden);
    if (!on)
    {
        if (lastRange != 0)
            SetPreHostile({}, 0), lastRange = 0, lastText = 0;
        return;
    }
    ObjectiveStep st = {};
    HostObjectiveNow(&st);
    const u32 system = fl::PlayerSystem();
    const int n = HostSpawnedShips(buf.data(), static_cast<int>(buf.size()));
    const u32 newest = n > 0 ? buf[n - 1].id : 0;
    if (st.textId == lastText && system == lastSystem && lastRange == g_set.preAggro && newest == lastSpawn)
        return;
    lastText = st.textId, lastSystem = system, lastRange = g_set.preAggro, lastSpawn = newest;
    // (this game's spawns only: those after the newest one at the last menu)
    int from = 0;
    if (s_menuSpawnSet)
        for (int i = n - 1; i >= 0; --i)
            if (buf[i].id == s_menuSpawn.id && memcmp(buf[i].pos, s_menuSpawn.pos, sizeof(s_menuSpawn.pos)) == 0)
            {
                from = i + 1;
                break;
            }
    std::vector<mission::Spawn> spawns;
    for (int i = from; i < n; ++i)
    {
        mission::Spawn sp;
        sp.id = buf[i].id;
        for (int k = 0; k < 3; ++k)
            sp.pos[k] = buf[i].pos[k];
        spawns.push_back(sp);
    }
    std::vector<std::string> names;
    const std::vector<u32> ids = mission::PendingHostileShips(st.textId, system, spawns, &names);
    SetPreHostile(ids, g_set.preAggro);
    static std::vector<u32> lastIds;
    if (ids == lastIds)
        return;
    lastIds = ids;
    std::string s;
    for (size_t i = 0; i < ids.size() && i < 12; ++i)
        s += " " + names[i] + "=" + std::to_string(ids[i]);
    events::Push("autopilot pre-aggro text=%u spawns=%d ships=%d%s", st.textId, n, static_cast<int>(ids.size()),
                 s.c_str());
}

void ObjectiveSupervise()
{
    PreAggroUpdate();
    if (!g_obj.on)
        return;
    if (ai::MissionHolds())
    {
        // the mission flies the player: a formation it gave is remembered (its leader) - not one of ours
        space::Pilot mp;
        if (g.mode != Mode::Formation && space::ReadPilot(mp))
        {
            if (mp.op == 10 && !mp.leader) // a formation order, its leader not readable until it forms: wait for it
                g_obj.missionFormSince = gameclock::NowMs64();
            if (mp.leader && mp.leader != g_obj.missionLeader)
            {
                g_obj.missionLeader = mp.leader;
                events::Push("autopilot objective mission-formation leader=%u", mp.leader);
            }
            else if (!mp.leader && g_obj.missionLeader && mp.op != -1)
            {
                // the mission flies us without a leader now (M09 break_player_follow): its formation is over
                events::Push("autopilot objective mission-formation over (op=%d)", mp.op);
                g_obj.missionLeader = 0;
            }
        }
        if (g_set.situationShadow) // (A37 S3: the rules' held against this hold)
            situation::ShadowHeld("mission_holds");
        return;
    }
    // (tractor_exit_run waiting for its range: the trip to the door's airlock once within it)
    if (g_obj.tractorExitPending && g.mode == Mode::Travel)
    {
        fl::Kinematics mk = {};
        if (!g_obj.cruiseLock || !g.tr.lockHave)
            g_obj.tractorExitPending = false;
        else if (fl::PlayerKinematics(mk))
        {
            const float dd[3] = {g.tr.lockP2[0] - mk.pos[0], g.tr.lockP2[1] - mk.pos[1], g.tr.lockP2[2] - mk.pos[2]};
            if (Len3(dd) < g_set.tractorExitRange)
            {
                g_obj.tractorExitPending = false;
                ObjectiveDecide("tractor-exit");
                return;
            }
        }
    }
    if (GameDocksUs())
    {
        // the mission docks us (Act_DockRequest): a decision now would cancel it - wait until it is through
        if (!g_obj.gameDock)
            events::Push("autopilot objective the game docks the ship: waiting");
        g_obj.gameDock = true;
        if (g_set.situationShadow)
            situation::ShadowHeld("game_dock");
        return;
    }
    g_obj.gameDock = false;
    const ULONGLONG now = gameclock::NowMs64();
    if (now < g_obj.nextLook)
        return;
    g_obj.nextLook = now + 500;
    // riding a lane / jumping: the game flies; nothing to decide (a rest started now would break the ride)
    space::Pilot pilot;
    if (space::ReadPilot(pilot) && (pilot.lane || pilot.jumping))
    {
        if (g_set.situationShadow)
            situation::ShadowHeld(pilot.lane ? "lane" : "jump");
        return;
    }
    // (`ally_kill`'s recheck here, before the hold branches - probe ak3: placed after them it was never reached while the ship rested at the dock: no heartbeat after the Rest, the 30 s stall fallback dead)
    static ULONGLONG akLook = 0;
    static u32 akAskedSeq = 0;
    if (g_set.allyKill && g_obj.akVerdict != static_cast<int>(lookahead::TdVerdict::Defend) &&
        g_obj.akVerdict != static_cast<int>(lookahead::TdVerdict::Fight) && now - akLook >= 1000)
    {
        akLook = now;
        ObjectiveStep cur = {};
        if (HostObjectiveNow(&cur) == g_obj.seq)
        {
            const lookahead::AllyKillFacts af = AllyKillFactsNow(cur, Contacts());
            const lookahead::TdResult ar = lookahead::AllyKill(af);
            const bool fromToday = g_obj.akVerdict == static_cast<int>(lookahead::TdVerdict::Today);
            // (a heartbeat while it waits - probe ak2-i4: rested 3 min at the hole, the target's hull 1.000,
            // the 30 s stall fallback never seen: is this recheck reached, does the wait grow?)
            static ULONGLONG hb = 0;
            if (!fromToday && now - hb >= 10000)
            {
                hb = now;
                events::Push("autopilot objective ally-kill check verdict=%s->%s why=%s waited=%.0f drop=%.3f allies=%d",
                             lookahead::TdVerdictName(static_cast<lookahead::TdVerdict>(g_obj.akVerdict)),
                             lookahead::TdVerdictName(ar.verdict), ar.why.c_str(), af.waitedS, af.hullDrop, af.allies);
            }
            if (static_cast<int>(ar.verdict) != g_obj.akVerdict && (!fromToday || akAskedSeq != g_obj.seq))
            {
                if (fromToday)
                    akAskedSeq = g_obj.seq;
                return ObjectiveDecide("ally-kill");
            }
        }
    }
    // An armed ship leash binds every trip (M10: the script goals kept to Von Claussen's 6 km, the route's
    // navmarker trip did not - "You fled the battle" again): beyond 75 % of it on a trip, follow the ship instead
    if (g_obj.leashShip && g.mode == Mode::Travel)
    {
        const space::Contact lc = FindContact(g_obj.leashShip);
        if (!lc.id)
            g_obj.leashShip = 0;
        else if (lc.distance > 0.8f * g_obj.leashDist && !LeashTripRuns(lc))
        {
            events::Push("autopilot objective leash: %.0f m from %u (leash %.0f m) - back toward it", lc.distance,
                         lc.id, g_obj.leashDist);
            return LeashBack(lc);
        }
    }
    // the mission's formation order is forming (M09 22 Oct 2026: the hold ended before the leader was readable and our
    // travel broke the formation - Ozu stayed behind, the arch never counted): hands off until it has a leader, <= 10 s
    if (g_obj.missionFormSince && g.mode != Mode::Formation)
    {
        if (pilot.leader)
        {
            if (pilot.leader != g_obj.missionLeader)
                events::Push("autopilot objective mission-formation leader=%u (formed)", pilot.leader);
            g_obj.missionLeader = pilot.leader;
            g_obj.missionFormSince = 0;
            if (HostObjectiveNow(nullptr) == g_obj.seq)
                return ObjectiveDecide("mission formation"); // decide again now, with the leader
        }
        else if (pilot.op == 10 && now - g_obj.missionFormSince < 10000)
            return;
        else
            g_obj.missionFormSince = 0;
    }
    if (HostObjectiveNow(nullptr) != g_obj.seq)
    {
        // The same objective set again (M07 22 Oct 2026: "Fly to Planet Kyushu" re-issued every second by a repeatable
        // trigger - each one restarted the travel, the ship never left and the nag failed the mission): not a new step
        ObjectiveStep st = {};
        const u32 seq = HostObjectiveNow(&st);
        const u32 key = st.textId * 2654435761u ^ (st.refCount ? st.refs[0] : 0) ^
                        static_cast<u32>(st.hasNav ? st.nav[0] * 7 + st.nav[2] * 13 : 0);
        // ...unless the mission's marks changed with it: a step set and replaced by a placeholder in the same instant
        // (s12d M04: "Destroy the Rheinland Assault Wing" -> "Awaiting" at once, the wing marked) is a new step
        // (the host's marks alone - not our script marks nor range-dependent refs -, and only marks added count: a marked
        // ship dying is no new step; review, session 12)
        u32 mk[32];
        const int mn = std::max(0, HostMissionMarks(mk, 32));
        bool marksAdded = false;
        for (int i = 0; i < mn; ++i)
            if (std::find(g_obj.lastMarks.begin(), g_obj.lastMarks.end(), mk[i]) == g_obj.lastMarks.end())
                marksAdded = true;
        g_obj.lastMarks.assign(mk, mk + mn);
        if (key == g_obj.stepKey && g.mode != Mode::Off && !marksAdded)
        {
            g_obj.seq = seq;
            return;
        }
        g_obj.stepKey = key;
        // a mission formation belongs to the step it came with (M09: given right after the objective; M01b's earlier
        // formation with Epsilon held the ship 15 min at "Fly to the Waypoint")
        g_obj.missionLeader = 0;
        return ObjectiveDecide("objective");
    }
    // the lane the objective names has just opened (the mission's lane permission): take it now
    if (g_obj.laneRing)
    {
        u32 next = 0, prev = 0;
        if (space::LaneLinks(g_obj.laneRing, next, prev) && ((next && space::LaneOpen(g_obj.laneRing, next) == 1) ||
                                                             (prev && space::LaneOpen(g_obj.laneRing, prev) == 1)))
        {
            events::Push("autopilot objective lane=%u opened", g_obj.laneRing);
            g_obj.laneRing = 0;
            return ObjectiveDecide("lane opened");
        }
    }
    // a dock the mission keeps locked for now (M10 Holstein: "denied locked" until its own moment): ask again every 4 s
    if (g_obj.dockRetryAt && now >= g_obj.dockRetryAt)
    {
        g_obj.dockRetryAt = 0;
        return ObjectiveDecide("dock retry");
    }
    if (g.mode == Mode::Off)
    {
        // a script goal's trip the game cannot route: that goal is off for this step, decided again without it
        // ("no way": was strstr(lastStop, "no way"), which only the travel's "the game found no way (locked gate?)" -
        // 44 chars, whole in lastStop[48] - contains)
        if (g_obj.sub == Mode::Travel && g_obj.action == Action::ScriptGoal &&
            g_obj.lastStopReason == StopReason::NoWay)
        {
            g_obj.badGoal = g_obj.goalTrigger;
            g_obj.lastStop[0] = 0;
            g_obj.lastStopReason = StopReason::None;
            events::Push("autopilot objective script-goal %s: no way there - off for this step", g_obj.badGoal.c_str());
            return ObjectiveDecide("goal no way");
        }
        // (a dock trip the game cannot route is the same lock seen from the plot: M10 s11q-lf - Holstein "denied locked" at
        // the ring, the retry 4 s later found "no way" (the script unlocks it when Fischer's dx_m10_1370 comm completes),
        // the step counted as arrived and the ship rested 60 s to a STALL)
        // (at most 15 tries a step, ~75 s - Holstein unlocks ~25 s after the 5 km mark; a base out of reach for good then
        // ends as before; review, session 12)
        if (g_obj.sub == Mode::Travel && g_obj.lastDock && g_obj.subTarget == g_obj.lastDock &&
            g_obj.lastStopReason == StopReason::NoWay && ++g_obj.dockNoWayTries <= 15)
        {
            g_obj.dockRetryAt = now + 4000;
            g_obj.lastStop[0] = 0;
            g_obj.lastStopReason = StopReason::None;
            events::Push("autopilot objective dock %u: no way (locked?) - again in 4 s (resting meanwhile)",
                         g_obj.lastDock);
            return ObjectiveStart(Mode::Hold, 0, Action::DockWait, StartOrKeep(Mode::Hold, 0));
        }
        // ...and a gate, hole or ring the step docks with (no lastDock: transit, above): the same lock, and no "arrival"
        // to rest at - it re-decided the trip every ~1 s (M13 ak2-i4: "Dock with the Nomad Jump Gate" while
        // the script keeps it locked, 154 "found no way" until the leg's limit). Every 4 s for 15 tries, then every 10 s
        // until the script opens it. (A gate / hole is the "jump" exit's Action::Jump, a ring a Dock: fx1-g_i2 / i3, 31 Oct
        // 2026 - the first build keyed on Dock only, 120 tries again and two STALLs)
        if (g_obj.sub == Mode::Travel && (g_obj.action == Action::Dock || g_obj.action == Action::Jump) && g_obj.subTarget &&
            g_obj.subTarget != g_obj.lastDock && g_obj.lastStopReason == StopReason::NoWay)
        {
            const int tries = ++g_obj.dockNoWayTries;
            const ULONGLONG wait = tries <= 15 ? 4000 : 10000;
            g_obj.dockRetryAt = now + wait;
            g_obj.lastStop[0] = 0;
            g_obj.lastStopReason = StopReason::None;
            events::Push("autopilot objective dock %u: no way (locked gate?) - again in %u s (try %d, resting meanwhile)",
                         g_obj.subTarget, static_cast<u32>(wait / 1000), tries);
            return ObjectiveStart(Mode::Hold, 0, Action::DockWait, StartOrKeep(Mode::Hold, 0));
        }
        if (g_obj.sub == Mode::Travel &&
            g_obj.lastStopReason == StopReason::DockRefused) // (only "travel: dock refused")
        {
            g_obj.dockRetryAt = now + 4000;
            g_obj.lastStop[0] = 0;
            g_obj.lastStopReason = StopReason::None;
            events::Push("autopilot objective dock refused: again in 4 s (resting meanwhile)");
            return ObjectiveStart(Mode::Hold, 0, Action::DockWait, StartOrKeep(Mode::Hold, 0));
        }
        // (a loot destination still there is not "arrived": the travel goes on tractoring it - M12's power cell)
        const space::Contact sc = g_obj.sub == Mode::Travel ? FindContact(g_obj.subTarget) : space::Contact{};
        if (g_obj.inVia) // the via point reached: on to the marker
        {
            g_obj.inVia = false;
            return ObjectiveDecide("via done");
        }
        // A trip to a script condition's sphere that ended outside it is not there (M12: generator 3
        // -> the airlock's top, "arrived-close" 1 km short against the lair's structure, then the threat check fought
        // Nomads - the user: "once again engaging nomads"): at once again, from where the ship is now (its way re-planned),
        // at most 4 times a step
        if (g_obj.sub == Mode::Travel && g_obj.flyGoalR > 0 && g_obj.arriveRetries < 4)
        {
            space::Pilot fp;
            if (space::ReadPilot(fp))
            {
                const float d[3] = {g_obj.flyGoal[0] - fp.pos[0], g_obj.flyGoal[1] - fp.pos[1],
                                    g_obj.flyGoal[2] - fp.pos[2]};
                if (Len3(d) > g_obj.flyGoalR)
                {
                    ++g_obj.arriveRetries;
                    g_obj.viaNext = true;
                    events::Push("autopilot objective trip ended %.0f m outside the condition's %.0f m: again (%d)",
                                 Len3(d) - g_obj.flyGoalR, g_obj.flyGoalR, g_obj.arriveRetries);
                    return ObjectiveDecide("short of the condition");
                }
            }
        }
        // (a leash trip back toward its ship is no arrival of the step's trip: M10 gate_s8v - its end set "arrived", the
        // step's "Fly to the Waypoint" no longer outranked the chasing fleet, the ship fought it 78 s at 8-9 km from the
        // Hamburg hole and Von Claussen's nag failed the mission)
        if (g_obj.sub == Mode::Travel && !(sc.id && (sc.type & space::kLoot)) && g_obj.action != Action::Leash)
            g_obj.arrived = true, g_obj.arrivedAt = now;
        if (g_obj.sub == Mode::Formation && now - g_obj.subSince < 10000) // the game refused the formation
        {
            g_obj.sub = Mode::Follow;
            g_obj.formRefused = true;
            g_obj.formRetryAt = now + 3000;
            return ObjectiveStart(Mode::Follow, g_obj.subTarget, Action::Follow,
                                  Start(Mode::Follow, g_obj.subTarget, nullptr));
        }
        return ObjectiveDecide("ended");
    }
    // ...and asked again once close (M13 22 Oct 2026: refused at 1.5 km, the follow kept that distance, and Sinclair
    // starts guiding only with the player within 300 m - the nag failed the mission): within 800 m, every 5 s
    if (g_obj.formRefused && g_obj.sub == Mode::Follow && g.mode == Mode::Follow && now >= g_obj.formRetryAt)
    {
        g_obj.formRetryAt = now + 5000;
        const space::Contact lc = FindContact(g_obj.subTarget);
        if (lc.id && lc.distance < 800)
        {
            g_obj.sub = Mode::Formation;
            g_obj.formRefused = false;
            return ObjectiveStart(Mode::Formation, lc.id, Action::Formation, Start(Mode::Formation, lc.id, nullptr));
        }
    }
    if (g_obj.sub == Mode::Combat &&
        g.mode == Mode::Hold) // the fight is over (combat rests by itself): back to the goal
        return ObjectiveDecide("fight over");
    // Following a mission lead that has stopped, with no objective (M02 22 Oct 2026: King waited 3.25 km from the
    // Colorado gate, the script's next step fires at 3.2 km of it, our follow sat 150 m behind him for 25 min): both at
    // rest for 15 s -> up to 2 km from the jump gate / hole nearest the lead (within 6 km) - his stop is the hint.
    if (g.mode == Mode::Follow || g.mode == Mode::Formation)
    {
        const space::Contact lc = FindContact(g.target ? g.target : g_obj.subTarget);
        space::ShipMotion lm, me;
        const bool still = lc.id && space::ShipMotionOf(lc.id, lm) && space::ShipMotionOf(space::PlayerShipId(), me) &&
                           lm.speed < 5 && me.speed < 5;
        if (!still)
            g_obj.leadStillSince = 0;
        else if (!g_obj.leadStillSince)
            g_obj.leadStillSince = now;
        else if (now - g_obj.leadStillSince > 15000)
        {
            g_obj.leadStillSince = 0;
            const space::Contact* gate = nullptr;
            for (const space::Contact& c : Contacts())
            {
                if (c.ship || !(c.type & (space::kJumpGate | space::kJumpHole)))
                    continue;
                const float d[3] = {c.pos[0] - lc.pos[0], c.pos[1] - lc.pos[1], c.pos[2] - lc.pos[2]};
                if (Len3(d) < 6000 && (!gate || c.distance < gate->distance))
                    gate = &c;
            }
            if (gate && gate->distance > 2200)
            {
                float p[3];
                const float f = 2000.0f / gate->distance;
                space::Pilot me2;
                if (space::ReadPilot(me2))
                {
                    for (int i = 0; i < 3; ++i)
                        p[i] = gate->pos[i] + (me2.pos[i] - gate->pos[i]) * f;
                    events::Push("autopilot objective lead %u at rest: up to the gate %u (%.0f m)", lc.id, gate->id,
                                 gate->distance);
                    return ObjectiveStart(Mode::Travel, 0, Action::LeadStall, StartTravel(0, p, 0, true));
                }
            }
        }
    }
    // Resting "arrived" while the step's navmarker in this system is still more than 600 m off (M12 22 Oct 2026: the lair
    // waypoint, 1.7 km short; the script wants 500 m - 5.5 min of rest, then the jump hole's timer failed it): travel
    // again after 10 s, at most 4 times per step.
    if (g_obj.arrived && g.mode == Mode::Hold && g_obj.arriveRetries < 4 && now - g_obj.arrivedAt > 10000)
    {
        ObjectiveStep st = {};
        HostObjectiveNow(&st);
        space::Pilot p;
        if (st.hasNav && space::ReadPilot(p) && (!st.navSystem || st.navSystem == p.system))
        {
            const float d[3] = {st.nav[0] - p.pos[0], st.nav[1] - p.pos[1], st.nav[2] - p.pos[2]};
            if (Len3(d) > 600)
            {
                ++g_obj.arriveRetries;
                g_obj.viaNext = true;
                g_obj.arrived = false;
                g_obj.arrivedAt = now;
                events::Push("autopilot objective navmarker still %.0f m off: again (%d)", Len3(d),
                             g_obj.arriveRetries);
                return ObjectiveDecide("not there yet");
            }
        }
        g_obj.arrivedAt = now; // (looked: there - or no marker here)
    }
    // survival resting in place because no lead was there yet (M04 Mactan: Juni_mactan spawns 7 s after the launch) -
    // the moment one is, stay with it instead (checked at most once a second)
    static ULONGLONG leadLook = 0;
    if (g_obj.survival && g.mode == Mode::Escort && g.tactic == Tactic::Rejoin && g_obj.action == Action::SurviveStay &&
        now - leadLook >= 1000)
    {
        leadLook = now;
        const space::Contact lc = FindContact(g.escorted);
        if (lc.id && lc.distance < 0.6f * g_set.followDistance)
            return ObjectiveDecide("lead reached");
    }
    // Resting under fire (M04 20 Oct 2026: "survive-hold" 1.5 km beside Juni, 600 m from the hostile cruiser LNS Rampart
    // whose turrets fired on us while the cruiser itself targeted another ship - so no "attacker" for the threat check;
    // shield 0.92 -> 0.20, hull 1.00 -> 0.03 in 6 s, dead): being hit is the ground truth. Out of the fire, once a second
    // while the hits go on: from the toughest hostile within 2.5 km (capital ships first; else the nearest), to the far
    // side of the lead (she anchors the tethers), at least 1 km farther from it than now; no lead: 1 km straight away.
    static ULONGLONG fireLook = 0;
    if (g_obj.survival && g.mode == Mode::Hold && g.stats.pressure > 0 && now - fireLook >= 1000)
    {
        fireLook = now;
        fl::Kinematics k = {};
        const std::vector<space::Contact> all = Contacts();
        const space::Contact* src = nullptr;
        for (const space::Contact& c : all)
            if (c.ship && c.health != 0 && HostileContact(c) && c.distance < 2500 &&
                (!src || c.maxHit > src->maxHit * 1.5f ||
                 (c.maxHit > src->maxHit / 1.5f && c.distance < src->distance)))
                src = &c;
        // Fighters are fought, not fled (M04 Mactan: "the ship took forever to start engaging" - the
        // Rheinland wing's fighters, 5800 hull, fired from ~2 km and this rule held the ship out of the fight 35-60 s):
        // moving out is for what our guns cannot answer - a gunboat, cruiser or capital ship's turrets.
        const bool srcBig = src && ((src->type & (kGunboatBit | kCruiserBit | kCapitalBit)) || src->maxHit > 20000);
        if (src && !srcBig)
        {
            events::Push("autopilot objective under-fire from=%u hp=%.0f dist=%.0f: a fighter - engaging", src->id,
                         src->maxHit, src->distance);
            if (const u32 lead = SurvivalLead(all))
            {
                ObjectiveStart(Mode::Escort, lead, Action::Survive, StartOrKeep(Mode::Escort, lead));
                return;
            }
            ObjectiveStart(Mode::Combat, 0, Action::Survive, StartOrKeep(Mode::Combat, 0));
            return;
        }
        if (src && fl::PlayerKinematics(k))
        {
            float away[3] = {k.pos[0] - src->pos[0], k.pos[1] - src->pos[1], k.pos[2] - src->pos[2]};
            const float d = std::max(1.0f, Len3(away));
            for (float& a : away)
                a /= d;
            float to[3] = {k.pos[0] + away[0] * 1000, k.pos[1] + away[1] * 1000, k.pos[2] + away[2] * 1000};
            const space::Contact lc = FindContact(SurvivalLead(all));
            if (lc.id)
            {
                float side[3] = {lc.pos[0] - src->pos[0], lc.pos[1] - src->pos[1], lc.pos[2] - src->pos[2]};
                const float ls = std::max(1.0f, Len3(side));
                const float reach = std::max(d + 1000, ls + 500); // past her, and farther than now
                for (int i = 0; i < 3; ++i)
                    to[i] = src->pos[i] + side[i] / ls * reach;
            }
            const float out3[3] = {to[0] - src->pos[0], to[1] - src->pos[1], to[2] - src->pos[2]};
            events::Push("autopilot objective under-fire from=%u hp=%.0f dist=%.0f: out to %.0f m%s", src->id,
                         src->maxHit, d, Len3(out3), lc.id ? " (past the lead)" : "");
            ObjectiveStart(Mode::Hold, 0, g_obj.action, Start(Mode::Hold, 0, to)); // (the action kept)
            return;
        }
    }
    if (g_obj.survival && g.mode == Mode::Hold && now - leadLook >= 1000)
    {
        leadLook = now;
        if (g_obj.action == Action::Rest && SurvivalLead(Contacts()))
            return ObjectiveDecide("lead");
        // (the reverse - escorting her with nothing to fight, caught up: rest again - is below, in the escort branch)
        // resting beside the lead: she moved off (past the follow distance + 500 m) - go with her
        if (g_obj.action == Action::SurviveHold)
        {
            const u32 lead = SurvivalLead(Contacts());
            const space::Contact lc = FindContact(lead);
            if (!lc.id || lc.distance > g_set.followDistance + 500)
                return ObjectiveDecide("lead moved");
        }
    }
    // (not during a lane approach, a run to a gate or a travel objective's trip: the way out / the story's destination
    // outranks the fight - see ObjectiveDecide)
    // A fight the threat check started (not the step's own): looked at again every 3 s - the script's destroy condition
    // may have live objects by now (M13 23 Oct 2026: the sphere's final generators spawn after the first decision; the
    // ship fought the entry's fighters through the 27 s dialogue). The re-decision keeps a running fight (StartOrKeep).
    static ULONGLONG threatLook = 0;
    if (g.mode == Mode::Combat && g_obj.action == Action::Fight && !g_obj.fightObjective && !g_obj.survival &&
        Knows(Source::Hidden) && g_set.scriptGoals && now - threatLook >= 3000)
    {
        threatLook = now;
        ObjectiveStep cur = {};
        HostObjectiveNow(&cur);
        for (const mission::Goal& sg :
             mission::AdvanceGoals(cur.textId, fl::PlayerSystem(), nullptr, HostObjectiveTitle()))
            if (sg.kind == 3)
            {
                float p[3];
                for (u32 id : sg.objects)
                    if (space::ObjectPos(id, p))
                        return ObjectiveDecide(
                            "recheck"); // (only when the step has a destroy condition with an object up)
            }
    }
    // A fight step whose destroy condition has just been met (all its objects gone) is decided again once - the script
    // talks before the next objective and the pre-stage flies on (M12: generator 3 down, the ship fought a fighter 6.5 s)
    static ULONGLONG doneLook = 0;
    static std::string doneTrigger;
    if (g.mode == Mode::Combat && Knows(Source::Hidden) && g_set.scriptGoals && now - doneLook >= 500)
    {
        doneLook = now;
        ObjectiveStep cur = {};
        HostObjectiveNow(&cur);
        const std::vector<space::Contact> all = Contacts();
        const std::vector<mission::Goal> goals =
            mission::AdvanceGoals(cur.textId, fl::PlayerSystem(), nullptr, HostObjectiveTitle());
        for (const mission::Goal& sg : goals)
            if (sg.kind == 3 && sg.trigger != doneTrigger && sg.leads != "SUCCEED" && DestroyFired(sg, all))
            {
                doneTrigger = sg.trigger;
                events::Push("autopilot objective %s met (all of %s gone): deciding again", sg.trigger.c_str(),
                             sg.objectName.c_str());
                return ObjectiveDecide("condition-met");
            }
        // Where the ship goes after the kill (StaticHold holds on that side: the user's try 3 held generator 3 where the
        // climb to the airlock's top was straight - ours sat ~130 m deeper in the trench, looped there ~6 s and needed
        // 22-28 s to the core against ~16): the destroy goal's other live objects, and the next step's place
        g_obj.leaveN = 0;
        for (const mission::Goal& sg : goals)
        {
            if (sg.kind != 3 || sg.leads == "SUCCEED")
                continue;
            for (u32 id : sg.objects)
                if (g_obj.leaveN < 8 && space::ObjectPos(id, &g_obj.leave[g_obj.leaveN * 3]))
                    ++g_obj.leaveN;
            for (const mission::Goal& ng : mission::GoalsAfter(sg.leads, sg.file, fl::PlayerSystem()))
                if (g_obj.leaveN < 9 && (ng.kind == 1 || ng.kind == 2) && ng.subject == "player" &&
                    (ng.kind == 2 ? (memcpy(&g_obj.leave[g_obj.leaveN * 3], ng.pos, 3 * sizeof(float)), true)
                                  : space::ObjectPos(ng.object, &g_obj.leave[g_obj.leaveN * 3])))
                {
                    ++g_obj.leaveN;
                    break;
                }
        }
        g_obj.leaveAt = now;
    }
    // TIMER-DESTROY: the verdict looked at again every second - a ship now shooting at us at the place, a default-preset
    // fight that is no longer safe (a defence ends by itself: its combat stops when nothing shoots at us)
    static ULONGLONG tdLook = 0;
    if (g_set.timerDestroy && g_obj.tdVerdict != static_cast<int>(lookahead::TdVerdict::Today) &&
        g_obj.tdVerdict != static_cast<int>(lookahead::TdVerdict::Defend) && now - tdLook >= 1000)
    {
        tdLook = now;
        ObjectiveStep cur = {};
        if (HostObjectiveNow(&cur))
        {
            const lookahead::TdResult tr =
                lookahead::TimerDestroy(TimerDestroyFactsNow(cur, ObjectiveVerb(cur.textId), Contacts()));
            if (static_cast<int>(tr.verdict) != g_obj.tdVerdict)
                return ObjectiveDecide("timer-destroy");
        }
    }
    // ALLY-KILL: the verdict looked at again every second - the stage trip at the dock, a ship now shooting at us, the
    // fallback's clock; and while it reads Today, the step's rule coming to hold (a decision the coast guard swallowed:
    // DecideBody returns under CoastActive, uncounted) - never under a defence (it ends by itself) or the kept fallback
    // (from Today: asked once per step - an earlier exit of the decision keeps it Today, and an ask every second would
    // trip the decide-loop guard)
    // kill_ahead: the script's leave looked at again every second while the ship waits at the target
    static ULONGLONG kaLook = 0;
    if (g_set.killAhead && g_obj.killAheadWait && now - kaLook >= 1000)
    {
        kaLook = now;
        if (mission::KillAheadSafe(mission::LiveTriggers(), g_obj.killAheadWait, g_obj.killAheadNo).ok)
            return ObjectiveDecide("kill-ahead");
    }
    // ... and in a fight step: the next step's target spawning (M13's nomadcap, 36.9 s in) while the fight goes on - the
    // decision is asked only once the kill is allowed (a refusal asked every second would trip the decide-loop guard)
    static ULONGLONG kaFight = 0;
    // (Mode::Escort too: a "defend" step fights from the escort round its object - M13 24414, set ab3)
    if (g_set.killAhead && Knows(Source::Hidden) && (g.mode == Mode::Combat || g.mode == Mode::Escort) &&
        !g_obj.scriptMarkN && now - kaFight >= 1000)
    {
        kaFight = now;
        ObjectiveStep cur = {};
        // (a "defend" step with no object to escort too, as at the decision)
        const bool got = HostObjectiveNow(&cur) && cur.textId;
        const ObjVerb cv = got ? ObjectiveVerb(cur.textId) : ObjVerb::None;
        const bool defend = cv == ObjVerb::Escort && !cur.refCount;
        if (got && (cv == ObjVerb::Fight || defend))
        {
            const mission::LiveTable lt = mission::LiveTriggers();
            for (const mission::Goal& ng : FightAheadGoals(cur.textId, lt, defend))
                for (u32 id : ng.objects)
                {
                    // (never the target fought already: after 24425 the armed cap_dies names nomadcap itself - a
                    // decide every second, set ab3)
                    const space::Contact c = FindContact(id);
                    if (c.id && c.health != 0 && id != g.target && mission::KillAheadSafe(lt, id, 1).ok)
                        return ObjectiveDecide("kill-ahead");
                }
        }
    }
    // chase_kill: the candidate looked for every second while it is not fought - his contact or the script's hostility
    // can lag the step by a moment (the decision then took the navmarker trip); asked once per ship and step
    static ULONGLONG chLook = 0;
    // (spawn_charge: every 200 ms while the staged fleeing ship is expected and not chased yet - the press belongs at his
    // spawn, each second of lag costs ~300 m of the lock gap: lookahead::ChargeGap)
    const ULONGLONG chEvery = g_set.spawnCharge && !g_obj.chargeShip.empty() && !g_obj.chargeId ? 200 : 1000;
    if (g_set.chaseKill && Knows(Source::Hidden) && now - chLook >= chEvery &&
        !(g_obj.chaseShip && g.mode == Mode::Combat && g.target == g_obj.chaseShip))
    {
        chLook = now;
        ObjectiveStep cur = {};
        if (HostObjectiveNow(&cur) == g_obj.seq)
        {
            lookahead::LookFacts cf;
            lookahead::LookResult cr;
            const u32 cs = ChaseCandidate(cur, ObjectiveVerb(cur.textId), Contacts(), cf, cr);
            if (cs && cs != g_obj.chaseAsked)
            {
                g_obj.chaseAsked = cs;
                return ObjectiveDecide("chase");
            }
        }
    }
    // spawn_stage: in a fight step's combat, the moment its last kill is pending (asked once per trigger and step); while
    // it stages, its verdict every second (the trip arrived, the last ship within reach, the timeout)
    static ULONGLONG ssLook = 0;
    if (g_set.spawnStage && g_set.chaseKill && Knows(Source::Hidden) && now - ssLook >= 1000 &&
        (g_obj.stageVerdict || g.mode == Mode::Combat))
    {
        ssLook = now;
        ObjectiveStep cur = {};
        if (HostObjectiveNow(&cur) == g_obj.seq && (g_obj.stageVerdict || ObjectiveVerb(cur.textId) == ObjVerb::Fight))
        {
            const StagePlan sp = SpawnStageNow(cur, Contacts());
            const int v = StageVerdictNow(sp);
            if (g_obj.stageVerdict ? v != g_obj.stageVerdict : (v && sp.trigger != g_obj.stageAsked))
            {
                if (!g_obj.stageVerdict)
                    g_obj.stageAsked = sp.trigger;
                return ObjectiveDecide("spawn-stage");
            }
        }
    }
    // (nor the airlock run: M13 23 Oct 2026, the user: "did not move with intent into the airlock" - each 0.5 s a Nomad
    // nearby restarted the run, 15 restarts and a decide-loop pause on the way in)
    if (g.mode != Mode::Combat && g.mode != Mode::Escort && g_obj.action != Action::Lane &&
        g_obj.action != Action::Jump && g_obj.action != Action::Airlock && g_obj.action != Action::PreStage)
    {
        const std::vector<space::Contact> all = Contacts();
        // (pre_aggro interrupts a travel objective too - above, ObjectiveDecide)
        // (every 3 s at most: a dock / lane step declines it in the decision)
        static ULONGLONG preLook = 0;
        if (PreHostileNear(all) && g_obj.action != Action::Dock && now - preLook >= 3000)
        {
            preLook = now;
            return ObjectiveDecide("pre-aggro");
        }
        if (!(g_obj.travelObjective && g.mode == Mode::Travel) && ObjectiveThreat(all))
            return ObjectiveDecide("threat");
    }
}

} // namespace autopilot
