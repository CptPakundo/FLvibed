// Advanced Autopilot - the objective's decision: what the current objective asks, and starting it (DecideBody).
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
// Decides what the current objective asks and starts it. Ranking (the mission's own pointers first, then the game's
// data about them): a fight the mission points at or that attacks us; the objective's target object - a friendly ship
// to join (formation, the game refuses it with capital ships: follow) or escort, a hostile to fight, a planet's docking
// ring, a station / ring / gate to fly to (dock only when the words say so); the navmarker; otherwise rest (Hold).
// (ObjectiveDecide's body since A37 S7: the decision below - the switch situation_arbiter on runs it in plan mode)
// kill_ahead's fight-step form: the destroy goals ahead of a fight step - the next step's (NextStepGoals), else the armed
// rows' (GoalsOfLive: M13's 24414 "Defend the Order Base" is set by triggers that lead nowhere; nomadcap's cap_dies is
// armed at its spawn by a sibling chain, spawnships -> nomad_timer -> freighter_dies; set ab2)
// (`live`: only for such a defend step - a Fight step's armed rows name its own target: 24425's cap_dies -> nomadcap)
std::vector<mission::Goal> FightAheadGoals(u32 textId, const mission::LiveTable& lt, bool live)
{
    std::vector<mission::Goal> out;
    for (const mission::Goal& ng : mission::NextStepGoals(textId, fl::PlayerSystem()))
        if (ng.kind == 3)
            out.push_back(ng);
    if (out.empty() && live)
        for (const mission::Goal& lg : mission::GoalsOfLive(lt, fl::PlayerSystem(), textId))
            if (lg.kind == 3)
                out.push_back(lg);
    return out;
}

void DecideBody(const char* reason)
{
    // (the coast guard flies the ship: no mode change under it - "mode switch still messed up"; with no mode running
    // nothing flies it, and waiting would wait for ever)
    if (CoastActive())
    {
        if (g.mode != Mode::Off)
            return;
        CoastAbort(false);
    }
    g_obj.tractorExitPending = false; // (set again below while it still applies)
    // Loop guard: more than 6 decisions in 10 s on one step (a fight that ends at once, a refused start) - pause 5 s.
    const ULONGLONG now = gameclock::NowMs64();
    if (now - g_obj.windowStart > 10000)
        g_obj.windowStart = now, g_obj.decisions = 0;
    if (++g_obj.decisions > 6)
    {
        events::Push("autopilot objective decide-loop reason=%s (more than 6 in 10 s): pausing 5 s", reason);
        g_obj.nextLook = now + 5000;
        g_obj.windowStart = now + 5000;
        g_obj.decisions = 0;
        // the step this call would have decided stays undecided: forget its key, or the next look takes it for "the
        // same objective set again" (ObjectiveUpdate's key check) and never decides it (gate q3s2 M07: a
        // fight ping-pong filled the guard, "Dock with the Trade Lane" arrived as the 7th decision, was swallowed, the
        // ship fought on and the nag failed the mission; earlier gates passed by 14 ms)
        g_obj.stepKey = 0;
        return;
    }
    ObjectiveStep st = {};
    const u32 seq = HostObjectiveNow(&st);
    if (seq != g_obj.seq)
    {
        g_obj.arrived = false, g_obj.arriveRetries = 0, g_obj.stepAt = gameclock::NowMs64(),
        g_obj.viaNext = g_obj.inVia = false, g_obj.badGoal.clear(), g_obj.deadGoals.clear(), g_obj.leashShip = 0,
        g_obj.dockNoWayTries = 0;
        // (chase_kill / spawn_stage: their memory is the step's; the chased ship stays while it lives - the next step
        // fights him on)
        g_obj.chaseAsked = 0, g_obj.stageAsked.clear(), g_obj.stageTrigger.clear(), g_obj.stageAt = 0,
        g_obj.stageLast = 0;
        g_obj.stageTimedOut.clear();
        if (g_obj.chaseShip && !FindContact(g_obj.chaseShip).id)
            g_obj.chaseShip = 0;
        // (spawn_charge: the fleeing ship's facts outlive the stage's step - his chase is the next one's - until he is gone)
        if (g_obj.chargeId && !FindContact(g_obj.chargeId).id)
            g_obj.chargeShip.clear(), g_obj.chargeId = 0, g_obj.chargeSaid = 0;
    }
    // A script goal's trip that ended with the step unchanged: its condition is not armed (M13
    // "Destroy the Nomad Battleship": within 1 km of the Osiris, then 300 m of Sinclair, then the Osiris again, every
    // ~30 s for minutes - neither fired, the battleship was never fought)
    else if (strcmp(reason, "ended") == 0 && g_obj.action == Action::ScriptGoal && !g_obj.goalTrigger.empty() &&
             g_obj.deadGoals.insert(g_obj.goalTrigger).second)
        events::Push("autopilot objective script-goal %s reached, the step unchanged: not armed - off for this step",
                     g_obj.goalTrigger.c_str());
    g_obj.seq = seq;
    g_obj.scriptMarkN = 0; // (set again below while the step's destroy condition holds)
    g_obj.defendOnly = false, g_obj.tdVerdict = 0; // (TIMER-DESTROY: set again below while its rule holds)
    g_obj.akVerdict = 0, g_obj.allyKillTarget = 0; // (ALLY-KILL: likewise)
    g_obj.killAheadWait = 0;                       // (kill_ahead: set again below while it waits for the script)
    g_obj.stageVerdict = 0, g_obj.stageNose = false; // (spawn_stage: set again below while it stages)
    g_obj.chargeStage = false;                       // (spawn_charge: likewise)
    // (A37 S3: the memory the rules read, as this decision begins - the shadow's, and since S7 the arbiter's)
    if (g_set.situationShadow || g_set.situationArbiter)
        situation::ShadowEntry();
    // An armed ship leash outranks the decision while we are far out on it (M10: the supervisor's follow and a
    // "threat" decision's trip took turns every second, 4.5 -> 5 km from the ship): follow it back first
    // (never when the step docks or jumps: the leash's phase is over - M10 followed a ship 4 min while "Dock with the
    // Hamburg Jump Hole" nagged, "You failed to dock with the Hamburg Jump Hole")
    if (g_obj.leashShip && ObjectiveVerb(st.textId) == ObjVerb::Dock)
        g_obj.leashShip = 0;
    // ...nor when the step points at a jump hole / gate (M10's "Dock with the Hamburg Jump Hole" shows a blank text: the
    // verb test missed it and the named-anchor run failed there too, 1227 s)
    for (int i = 0; i < st.refCount && g_obj.leashShip; ++i)
    {
        const space::Contact jc = FindContact(space::ObjectiveObject(st.refs[i], Contacts()));
        if (jc.id && (jc.type & (space::kJumpGate | space::kJumpHole)))
            g_obj.leashShip = 0;
    }
    // (or a navmarker at one: M10's Hamburg step has no refs, only the marker - 5c3b388 still failed there)
    if (g_obj.leashShip && st.hasNav)
        for (const space::Contact& c : Contacts())
            if (c.type & (space::kJumpGate | space::kJumpHole))
            {
                const float d[3] = {c.pos[0] - st.nav[0], c.pos[1] - st.nav[1], c.pos[2] - st.nav[2]};
                if (Len3(d) < 2000.0f)
                    g_obj.leashShip = 0;
            }
    if (g_obj.leashShip)
    {
        const space::Contact lc = FindContact(g_obj.leashShip);
        // (80 %: at 60 % the trips back at ~4.5 km of M10's 6 km broke the exit-tunnel waypoint chain every 20-40 s - the
        // script had already dropped the leash for a nag toward the hole, and the slow progress failed it, gate_s8w; 1.2 km
        // left is ~4 s at cruise)
        if (lc.id && lc.distance > 0.8f * g_obj.leashDist)
        {
            if (LeashTripRuns(lc))
                return ShadowAfter("leash", g_obj.sub, g_obj.subTarget, g_obj.action, true); // (already on the way back)
            events::Push("autopilot objective decide reason=%s: %.0f m from the leash's ship %u - back toward it",
                         reason, lc.distance, lc.id);
            LeashBack(lc);
            return ShadowAfter("leash", Mode::Travel, 0, Action::Leash);
        }
    }
    g_obj.preStageTarget = 0;
    g_obj.flyGoalR = 0;
    const std::vector<space::Contact> all = Contacts();
    // LOCK-TRAP (`lock_trap`, mission::LockTrapsOfLive): an armed "within R of <ship> -> lock <dock>" row whose dock the
    // script unlocks later is met first, so the lock comes before the unlock (M13: wait_for_trent, 320 m of Orillion ->
    // lock St01_to_St03_hole, beside in_hole_range -> check_cap -> unlock; met after check_cap the hole stays locked)
    if (g_set.lockTrap && Knows(Source::Hidden) && g_set.scriptGoals)
        if (LockTrapDecide(all))
            return;
    // A placeholder step (no target, no navmarker: "Awaiting Mission Objective") with live hostile ships the mission marks
    // within 6 km is their fight (s12d M04 Mactan: "Destroy the Rheinland Assault Wing" and "Awaiting" came in the same
    // instant; the wing marked, the survival stance followed Juni 110 s while others killed it - the user: "all the
    // wrong choices when Rheinland ships are attacking Mactan")
    bool markFight = false;
    if (ObjectiveVerb(st.textId) == ObjVerb::None && !st.refCount && !st.hasNav)
    {
        u32 mk[32];
        const int mn = MissionObjectIds(mk, 32, all);
        for (const space::Contact& c : all)
            if (c.ship && c.health != 0 && c.distance < 6000 && HostileContact(c) && !c.preHostile &&
                std::find(mk, mk + mn, c.id) != mk + mn)
                markFight = true;
        if (markFight)
            events::Push("autopilot objective decide reason=%s: a placeholder step with hostile marks - their fight",
                         reason);
    }
    const ObjVerb verb = markFight ? ObjVerb::Fight : ObjectiveVerb(st.textId);
    g_obj.fightObjective = verb == ObjVerb::Fight;
    if (g_obj.fightObjective) // what the fight's marks are (diagnostics: M09's generators were never chosen)
    {
        u32 mk[32];
        const int mn = MissionObjectIds(mk, 32, all);
        std::string s;
        for (int i = 0; i < mn && i < 8; ++i)
            for (const space::Contact& c : all)
                if (c.id == mk[i])
                {
                    float att = 9;
                    space::AttitudeToPlayer(c.id, att);
                    char b[96];
                    sprintf_s(b, " %u:ship=%d,tgt=%d,att=%.2f,hp=%.0f,dist=%.0f", c.id, c.ship ? 1 : 0,
                              c.targetable ? 1 : 0, att, c.maxHit, c.distance);
                    s += b;
                }
        events::Push("autopilot objective fight-marks n=%d%s", mn, s.c_str());
    }
    // Marked loot the objective says to destroy: shot (Shoot mode on the object, nothing tractored), unless something
    // attacks - the fight first, then the pod (Destroy Contraband: a blank entry 0 whose navmarker sits on
    // the pod; the trip there ended "no way" and the tractor ate the pod, again and again)
    if (!ObjectiveThreat(all))
    {
        float dd = 0;
        if (const u32 dl = DestroyMarkedLoot(all, &dd))
        {
            if (g.mode == Mode::Shoot && g.target == dl)
                return ShadowExit("marked-loot", Mode::Shoot, dl, Action::Fight, true, true);
            events::Push("autopilot objective decide reason=%s: marked loot %u to destroy at %.0f m - shoot it", reason,
                         dl, dd);
            return ShadowExit("marked-loot", Mode::Shoot, dl, Action::Fight, DecShoot(dl));
        }
    }
    // TIMER-DESTROY (`timer_destroy`; run-judged 30 Oct 2026, look0 M13 from Save3989: "Destroy the Nomads" was fought -
    // NomadCap2, 120,000 hp, from 3 km with 0 batteries after the first battleship - and the ship died in one hit 5 s
    // after "Dock with Planet Toledo"; the step ends on the script's clock ~20 s after it shows, whatever is killed, and
    // the runners dock at Toledo the moment it unlocks, R3 1.4 km flown, 0 launches, 0 hull lost): a "Destroy ..." step
    // the script advances by itself is not fought (lookahead.h TimerDestroy)
    if (g_set.timerDestroy)
    {
        const lookahead::TimerDestroyFacts tf = TimerDestroyFactsNow(st, verb, all);
        const lookahead::TdResult tr = lookahead::TimerDestroy(tf);
        g_obj.tdVerdict = static_cast<int>(tr.verdict);
        if (tr.verdict != lookahead::TdVerdict::Today)
        {
            static std::string said;
            char key[96];
            sprintf_s(key, "%u %s %s", st.textId, lookahead::TdVerdictName(tr.verdict), tr.why.c_str());
            if (said != key)
            {
                said = key;
                events::Push("autopilot objective timer-destroy %u -> %s why=%s advance=%s next=%s place=%.0f m "
                             "hull=%.2f batt=%d attacked=%d speedrun=%d",
                             st.textId, lookahead::TdVerdictName(tr.verdict), tr.why.c_str(), tf.advance.c_str(),
                             TimerDestroyNext().c_str(), tr.dist, tf.hull, tf.batteries, tf.attacked ? 1 : 0,
                             tf.speedrun ? 1 : 0);
            }
        }
        const bool notFought = tr.verdict == lookahead::TdVerdict::Stage || tr.verdict == lookahead::TdVerdict::Rest ||
                               tr.verdict == lookahead::TdVerdict::Defend || tr.verdict == lookahead::TdVerdict::Dock;
        if (notFought)
            g_obj.defendOnly = true, g_obj.fightObjective = false;
        // (td_window: the script takes the dock now - the travel docks with the next objective's object)
        if (tr.verdict == lookahead::TdVerdict::Dock)
        {
            g_obj.travelObjective = true;
            return ShadowExit("timer-destroy", Mode::Travel, tf.nextObject, Action::Dock,
                              StartTravel(tf.nextObject, nullptr, 0, false));
        }
        // (Action::PreStage: the threat check leaves the trip and the rest alone - the battle round us is no attack)
        if (tr.verdict == lookahead::TdVerdict::Stage)
        {
            g_obj.travelObjective = true;
            return ShadowExit("timer-destroy", Mode::Travel, 0, Action::PreStage, StartTravel(0, tr.point, 0, true));
        }
        if (tr.verdict == lookahead::TdVerdict::Rest)
            return ShadowExit("timer-destroy", Mode::Hold, 0, Action::PreStage, StartOrKeep(Mode::Hold, 0));
        if (tr.verdict == lookahead::TdVerdict::Defend)
            return ShadowExit("timer-destroy", Mode::Combat, 0, Action::Survive, StartOrKeep(Mode::Combat, 0));
    }
    // ALLY-KILL (`ally_kill`; docs\research\runners\M13_gate_allykill.md - M13's gate battleship: 3 deaths in ~16 legs on
    // 31 Oct, ~20 batteries spent there; R2 sits at the hole while the allies kill NomadCap3, check_cap counts any killer
    // and unlocks the hole): the step whose kill anyone may make and which unlocks the next step's dock is not fought - the
    // ship waits at that dock (a point trip: the dock itself is locked), answers only what shoots at it and docks on the
    // unlock (the next step's own decision); the fight as before once nothing friendly wears the target down
    if (g_set.allyKill)
    {
        const lookahead::AllyKillFacts af = AllyKillFactsNow(st, all);
        const lookahead::TdResult ar = lookahead::AllyKill(af);
        g_obj.akVerdict = static_cast<int>(ar.verdict);
        if (ar.verdict == lookahead::TdVerdict::Fight)
            g_obj.akFellBack = st.textId;
        if (ar.verdict != lookahead::TdVerdict::Today)
        {
            static std::string said;
            char key[96];
            sprintf_s(key, "%u %s %s", st.textId, lookahead::TdVerdictName(ar.verdict), ar.why.c_str());
            if (said != key)
            {
                said = key;
                events::Push("autopilot objective ally-kill %u -> %s why=%s target=%u hp=%.3f allies=%d dist=%.0f "
                             "waited=%.1f drop=%.3f hull=%.2f batt=%d attacked=%d",
                             st.textId, lookahead::TdVerdictName(ar.verdict), ar.why.c_str(), af.target, af.targetHull,
                             af.allies, ar.dist, af.waitedS, af.hullDrop, af.hull, af.batteries, af.attacked ? 1 : 0);
            }
        }
        if (ar.verdict == lookahead::TdVerdict::Stage || ar.verdict == lookahead::TdVerdict::Rest ||
            ar.verdict == lookahead::TdVerdict::Defend)
            g_obj.defendOnly = true, g_obj.fightObjective = false, g_obj.allyKillTarget = af.target;
        // (Action::PreStage: the threat check leaves the trip and the rest alone - the battle round us is no attack)
        if (ar.verdict == lookahead::TdVerdict::Stage)
        {
            g_obj.travelObjective = true;
            return ShadowExit("ally-kill", Mode::Travel, 0, Action::PreStage, StartTravel(0, ar.point, 0, true));
        }
        if (ar.verdict == lookahead::TdVerdict::Rest)
            return ShadowExit("ally-kill", Mode::Hold, 0, Action::PreStage, StartOrKeep(Mode::Hold, 0));
        if (ar.verdict == lookahead::TdVerdict::Defend)
            return ShadowExit("ally-kill", Mode::Combat, 0, Action::Survive, StartOrKeep(Mode::Combat, 0));
    }
    // `escort_by_place` (M13-MIMIC item 1, docs\research\runners\M13.md "## Plan"; td1full 30 Oct 2026: "Escort Sinclair to
    // the Battleship Osiris" took 136.9 s - a 16.5 s fight at the relaunch, then formation at 245 m/s - where all three
    // runners cruise the 29.7 km in 105-112 s and leave Sinclair; the step ends on the PLAYER within 1 km of the Osiris):
    // the live table's player distance goal, flown straight to (a trip: the threat check leaves it alone)
    if (g_set.escortByPlace && Knows(Source::Hidden) && verb == ObjVerb::Escort)
    {
        const mission::EscortPlace ep =
            mission::EscortByPlace(mission::LiveTriggers(), fl::PlayerSystem(), st.textId);
        space::Pilot me;
        const mission::Goal* best = nullptr;
        float bestPos[3] = {}, bestFar = 1e30f;
        if (ep.ok && space::ReadPilot(me))
            for (const mission::Goal& eg : ep.goals)
            {
                float p[3];
                if (eg.kind == 1 ? !space::ObjectPos(eg.object, p) : (memcpy(p, eg.pos, sizeof(p)), false))
                    continue;
                const float d[3] = {p[0] - me.pos[0], p[1] - me.pos[1], p[2] - me.pos[2]};
                if (Len3(d) - eg.dist < bestFar)
                    bestFar = Len3(d) - eg.dist, best = &eg, memcpy(bestPos, p, sizeof(p));
            }
        // (the place reached and the next step's dock shown, then the escort objective shown again - mm4-b_i1, 30 Oct
        // 2026: "Dock with the Battleship Osiris" 24445, 1 s later 24440 again; with no player goal left the escort fell
        // back to formation on Sinclair and stalled 69 s; 1 of 10 S415 runs): the dock shown last goes on
        if (!best && ep.why == "no player goal" && g_obj.lastDock && FindContact(g_obj.lastDock).id)
        {
            events::Push("autopilot objective escort-by-place %u: its place reached, the dock shown before (%u) goes on",
                         st.textId, g_obj.lastDock);
            return ShadowExit("dock-entry", Mode::Travel, g_obj.lastDock, Action::Dock,
                              StartTravel(g_obj.lastDock, nullptr, 0, false));
        }
        static std::string said;
        const std::string key = std::to_string(st.textId) + (best ? " " + best->trigger : " no " + ep.why);
        if (said != key)
        {
            said = key;
            events::Push("autopilot objective escort-by-place %u -> %s, %.0f m to go", st.textId,
                         best ? (best->trigger + " (" + best->leads + ")").c_str() : ("none: " + ep.why).c_str(),
                         best ? bestFar : 0.0f);
        }
        // `place_dock` (M13-MIMIC item 6, docs\research\runners\M13.md "## Plan" 6): the place is the next step's dock -
        // in_osiris_range (1 km of the Osiris) leads to dock_with_osiris, a dock with that same object, which the script
        // already lets the player take (Act_PlayerCanDock = false, osiris: the one allowed): the trip is that dock, flown
        // through the trigger at cruise into the bay (R3 docks at the 24445 banner; ours stopped 700 m out and took 12 s)
        u32 nextText = 0;
        if (g_set.placeDock && best && bestFar > 0 && best->kind == 1 &&
            mission::ObjectiveObjectOf(best->leads, best->file, &nextText) == best->object && nextText &&
            ObjectiveVerb(nextText) == ObjVerb::Dock)
        {
            events::Push("autopilot objective place-dock %s -> %s: %u docked with on the way (%u)", best->trigger.c_str(),
                         best->leads.c_str(), best->object, nextText);
            g_obj.travelObjective = true;
            g_obj.goalTrigger = best->trigger;
            return ShadowExit("place-dock", Mode::Travel, best->object, Action::Dock,
                              StartTravel(best->object, nullptr, 0, false));
        }
        if (best && bestFar > 0)
        {
            float q[3];
            const float d[3] = {me.pos[0] - bestPos[0], me.pos[1] - bestPos[1], me.pos[2] - bestPos[2]};
            const float l = std::max(1.0f, Len3(d));
            for (int i = 0; i < 3; ++i)
                q[i] = bestPos[i] + d[i] / l * best->dist * 0.7f;
            g_obj.travelObjective = true;
            g_obj.goalTrigger = best->trigger;
            return ShadowExit("escort-by-place", Mode::Travel, 0, Action::ScriptGoal, StartTravel(0, q, 0, true));
        }
    }
    u32 target = 0;
    for (int i = 0; i < st.refCount && !target; ++i)
        target = space::ObjectiveObject(st.refs[i], all);
    // No target and no navmarker: the mission's newest mark is what the words mean (M06 "Fly to Quintaine" 23142 is a
    // plain text objective; the script marks Quintaine - the ship rested and the nag failed the mission).
    // Not for a fight (its marks are the fight's business) and never a hostile mark here.
    const bool markTarget = !target && !st.hasNav && verb != ObjVerb::Fight; // the target below is only such a mark
    if (markTarget)
    {
        u32 marks[32];
        const int n = MissionObjectIds(marks, 32, all);
        for (int i = 0; i < n && !target; ++i)
            for (const space::Contact& c : all)
                if (c.id == marks[i] && c.health != 0 && !HostileContact(c))
                {
                    target = c.id;
                    break;
                }
        if (target)
            events::Push("autopilot objective mark-target=%u (no target, no navmarker)", target);
    }
    const auto it = std::find_if(all.begin(), all.end(), [&](const space::Contact& c) { return c.id == target; });
    const space::Contact tc = it == all.end() ? space::Contact{} : *it;
    // Marked loot already within the beam's reach is pulled in passing (TractorMarkedLoot): the running trip keeps its
    // line (M12 core: the power cell spawned 843 m off, a trip "to" it turned the ship about at
    // 297 m/s, the exit then lay 164 deg behind and its run looped back for 11-36 s; the user's try 3 left the core 2.5 s
    // after the tractor, never turning)
    if (markTarget && tc.id && (tc.type & space::kLoot) && g.mode == Mode::Travel && g_obj.sub == Mode::Travel)
    {
        ai::Self me;
        const float reach = ai::ReadSelf(me) && me.tractorRange > 0 ? me.tractorRange : 1500.0f;
        if (tc.distance < reach)
        {
            // (tractor_exit_run: under the cruise lock the trip's own airlock - the exit - is the way on now, the beam pulls
            // the loot in meanwhile; s10-h-i3 and 5 more fail states, the M12 core at 300 m/s: the cell's pickup completed
            // ~200 m before the static plate, the exit run took over too late for its side passage, struck the plate and
            // lost the exit door. The exit opens on the pickup, ~2.7 s later - the run to it takes ~5 s)
            const State::Travel& tr = g.tr;
            // (within tractor_exit_range of the door's outer point only: s10-j-i3, switched 970 m out, 132 m off the M12 core's
            // axis, the run headed for the rim of the ~90 m middle hole at 290 m/s, the guard's swerve turned the ship about -
            // a loop round the hub and a fail state; the cell trip re-centres on the axis through that hole first)
            float toDoor = 1e30f;
            {
                fl::Kinematics mk = {};
                if (tr.lockHave && fl::PlayerKinematics(mk))
                {
                    const float dd[3] = {tr.lockP2[0] - mk.pos[0], tr.lockP2[1] - mk.pos[1], tr.lockP2[2] - mk.pos[2]};
                    toDoor = Len3(dd);
                }
            }
            if (g_set.tractorExitRun && g_obj.cruiseLock && tr.lockId && tr.lockId != ~0u && tr.lockHave &&
                toDoor < g_set.tractorExitRange)
            {
                events::Push(
                    "autopilot objective decide reason=%s text=%u: marked loot %u at %.0f m within the beam's %.0f m - "
                    "on to the trip's airlock %u now (the beam pulls it in)",
                    reason, st.textId, tc.id, tc.distance, reach, tr.lockId);
                const u32 lock = tr.lockId;
                return ShadowExit("tractor-exit", Mode::Travel, lock, Action::Airlock, StartTravel(lock, nullptr, 0, false));
            }
            // (out of range yet: the supervisor decides again once the door is within tractor_exit_range)
            g_obj.tractorExitPending = g_set.tractorExitRun && g_obj.cruiseLock && tr.lockHave;
            events::Push(
                "autopilot objective decide reason=%s text=%u: marked loot %u at %.0f m within the beam's %.0f m - "
                "the trip goes on",
                reason, st.textId, tc.id, tc.distance, reach);
            return ShadowExit("tractor-exit", Mode::Travel, g_obj.subTarget, g_obj.action, true, true);
        }
    }
    events::Push("autopilot objective decide reason=%s text=%u verb=%d ref=%u target=%u nav=%d", reason, st.textId,
                 static_cast<int>(verb), st.refCount ? st.refs[0] : 0, target, st.hasNav ? 1 : 0);
    LookLog(st, verb, all); // (LOOK L0, look_log: logged only - nothing below reads it)
    // A lane ride the mission asks for and allows is the way out: it outranks the fight around it (M04 "Escape from
    // Liberty": "Dock with the Trade Lane" TL147 under attack - the fight went on, the ship fled away
    // from the lane and the mission failed 22755).
    u32 escNext = 0, escPrev = 0;
    const bool laneObjective = tc.id && verb == ObjVerb::Dock && space::LaneLinks(tc.id, escNext, escPrev);
    const bool laneEscape = laneObjective && ((escNext && space::LaneOpen(tc.id, escNext) == 1) ||
                                              (escPrev && space::LaneOpen(tc.id, escPrev) == 1));
    // a lane objective still closed: watched (also while fighting) until the mission opens it
    g_obj.laneRing = laneObjective && !laneEscape ? tc.id : 0;
    // ... and a jump gate / hole the objective names is the same kind of way out (M04 "Flee to the Magellan Jump Gate",
    // 19 Oct 2026: the threat check fought the pursuers for 74 s and the mission failed 22770 "failed to take the gate")
    const bool gateEscape = tc.id && (tc.type & (space::kJumpGate | space::kJumpHole)) != 0;
    if (gateEscape)
        return ShadowExit("jump", Mode::Travel, tc.id, Action::Jump, StartTravel(tc.id, nullptr, 0, false));
    // A mission's dock grant (Act_DockRequest) with no dock maneuver after it, an airlock within 3 km: the story's way
    // in - flown through (the fast approach's airlock run). (M12 22 Oct 2026: the lair entry granted while the step read
    // "Fly to the Waypoint" above it; the step had "ended" and the ship fought the Nomads around the lair instead - the
    // user: "M12 is ignoring the objective and throwing itself at Nomads". Before session 4 our own dock request went in.)
    if (SinceGameGrant() < kGameGrantWayInMs && !ai::DockInProgress())
    {
        const space::Contact* lock = nullptr;
        // (never the way we came in while another airlock is in reach: session 8, M12 core exit - the entry was nearer)
        float in[3];
        const bool haveIn = SystemEntry(in);
        auto cameIn = [&](const space::Contact& c)
        {
            const float d[3] = {c.pos[0] - in[0], c.pos[1] - in[1], c.pos[2] - in[2]};
            return haveIn && Len3(d) < 600.0f;
        };
        for (const space::Contact& c : all)
            if ((c.type & space::kAirlockGate) && c.distance < 3000 &&
                (!lock || (cameIn(*lock) && !cameIn(c)) || (cameIn(*lock) == cameIn(c) && c.distance < lock->distance)))
                lock = &c;
        if (lock)
        {
            events::Push("autopilot objective granted airlock %u at %.0f m: through it", lock->id, lock->distance);
            return ShadowExit("airlock", Mode::Travel, lock->id, Action::Airlock, StartTravel(lock->id, nullptr, 0, false));
        }
    }
    // The script's destroy condition for a step that names no target of its own (Source::Hidden; user, 22 Oct 2026, M13
    // from St03: "failed as soon as the player ship targeted a nomad ship rather than fly straight at the generators in the
    // trenches" - "Head to the Barrier" points nowhere, the ship followed Orillion and the threat check dogfought Nomad
    // wings until it died; the script advances on Cnd_Destroyed generator 4, and the runners kill the first generator
    // before "Destroy the generators" shows): its live objects become the marks and the step a fight - the ranking then
    // goes for them first and turns to attackers only when the race is lost (autopilot_target.cpp "pressed").
    // (also a step with a navmarker - M13 "Fly to the Waypoint" in the sphere marks the city; the runners rush the nearest
    // power generator - and "Awaiting Mission Objective": the user saw the ship fight the fighters at the sphere's entry
    // during the 45 s dialogue)
    // (a place the step names is no reason to skip it either: "Fly to the Waypoint" at the Nomad City - its generators end
    // the mission)
    space::Pilot mePilot;
    if (Knows(Source::Hidden) && g_set.scriptGoals && verb != ObjVerb::Fight && verb != ObjVerb::Escort &&
        verb != ObjVerb::Dock && (!tc.id || !tc.ship || (markTarget && !HostileContact(tc))) &&
        space::ReadPilot(mePilot))
    {
        bool fromLast = false;
        for (const mission::Goal& sg :
             mission::AdvanceGoals(st.textId, fl::PlayerSystem(), &fromLast, HostObjectiveTitle()))
        {
            if (sg.kind != 3)
                continue;
            // live objects: a contact with hull left, or beyond the contact list a solar the game still places (the city's
            // generators lie 20-60 km off)
            int alive = 0;
            float nearest = 1e30f, nearPos[3] = {};
            u32 nearId = 0;
            for (u32 id : sg.objects)
            {
                if (alive >= 8)
                    break;
                float p[3];
                bool live = false;
                const auto ci =
                    std::find_if(all.begin(), all.end(), [&](const space::Contact& c) { return c.id == id; });
                if (ci != all.end())
                    live = ci->health != 0, memcpy(p, ci->pos, sizeof(p));
                else
                    live = space::ObjectPos(id, p);
                if (!live)
                    continue;
                g_obj.scriptMarks[alive++] = id;
                const float d[3] = {p[0] - mePilot.pos[0], p[1] - mePilot.pos[1], p[2] - mePilot.pos[2]};
                const float dist = Len3(d);
                if (dist < nearest)
                    nearest = dist, nearId = id, memcpy(nearPos, p, sizeof(p));
            }
            if (!alive)
                continue;
            int dead = 0; // (kill_ahead: the kill it would be - the objects seen alive before and gone now)
            {
                std::vector<u32> seenAlive, seenListed;
                SeenDestroyed(seenAlive, seenListed);
                for (u32 id : sg.objects)
                    dead += std::find(seenListed.begin(), seenListed.end(), id) != seenListed.end() &&
                                    std::find(g_obj.scriptMarks, g_obj.scriptMarks + alive, id) ==
                                        g_obj.scriptMarks + alive
                                ? 1
                                : 0;
            }
            // stale_destroy_wait: a goal only the last objective stands in for, none of whose objects the mission marks yet,
            // is a later step's (every M09 gate: at the Tohoku entry, "Awaiting", the generators 22 km off by
            // the last objective - the trip toward them entered at_nebula_attack's 14.75 km sphere round Tekagi's base, the
            // patrol fight and its two nags failed the mission; the story sends the player to the Blood Dragon outpost first)
            // ...nor one replacing a trip step of its own (a target or navmarker: M09's "Fly to the Blood Dragon Outpost"
            // took the same unmarked generators, s11m)
            // (never one that ends the mission - leads to "null" / SUCCEED: M13's city generators are unmarked at the
            // start too, the rush is the speedrun's way; s11c died holding back)
            if (g_set.staleDestroyWait && (fromLast || tc.id || st.hasNav) && sg.leads != "null" &&
                sg.leads != "SUCCEED")
            {
                u32 mk[32];
                const int mn = MissionObjectIds(mk, 32, all);
                bool marked = false;
                for (int i = 0; i < alive && !marked; ++i)
                    marked = std::find(mk, mk + mn, g_obj.scriptMarks[i]) != mk + mn;
                // `kill_ahead` (M13-MIMIC item 3): a step of the script's own (not a stand-in) with no place of its own
                // and no towards nag - its later step's target is flown to now, and fought once the kill leaves the
                // script's way to the next objective intact (mission::KillAheadSafe); R1 is on barrier generator 1 from
                // +21 s and kills it before "Destroy the Generators" shows; ours chased Nomads 9.7 km off
                mission::TowardsNag tn;
                const bool ahead = g_set.killAhead && !marked && !fromLast && !st.hasNav &&
                                   (!tc.id || (markTarget && !HostileContact(tc))) &&
                                   !mission::TowardsNagOf(st.textId, tn);
                if (ahead)
                {
                    const mission::KillSafe ks = mission::KillAheadSafe(mission::LiveTriggers(), nearId, dead + 1);
                    static std::string said;
                    char key[200];
                    sprintf_s(key, "%s %u %d %s", sg.trigger.c_str(), nearId, dead + 1, ks.ok ? "fight" : ks.why.c_str());
                    if (said != key)
                    {
                        said = key;
                        events::Push("autopilot objective kill-ahead %s: %u at %.0f m (kill %d of %s) -> %s%s",
                                     sg.trigger.c_str(), nearId, nearest, dead + 1, sg.objectName.c_str(),
                                     ks.ok ? "fight" : "wait: ", ks.ok ? "" : ks.why.c_str());
                    }
                    if (ks.ok)
                    {
                        // (the nearest first: the marks are the live objects, the ranking takes the nearest)
                        g_obj.scriptMarkN = alive;
                        g_obj.fightObjective = true;
                        return ShadowExit("kill-ahead", Mode::Combat, 0, Action::ScriptKill,
                                          StartOrKeep(Mode::Combat, 0));
                    }
                    // wait at it: a trip to 600 m short (PreStage - no threat detour), then a rest there
                    g_obj.killAheadWait = nearId, g_obj.killAheadNo = dead + 1;
                    g_obj.travelObjective = true;
                    if (nearest > 900)
                    {
                        float q[3];
                        for (int i = 0; i < 3; ++i)
                            q[i] = nearPos[i] + (mePilot.pos[i] - nearPos[i]) * (600.0f / nearest);
                        return ShadowExit("kill-ahead", Mode::Travel, 0, Action::PreStage, StartTravel(0, q, 0, true));
                    }
                    return ShadowExit("kill-ahead", Mode::Hold, 0, Action::PreStage, StartOrKeep(Mode::Hold, 0));
                }
                if (!marked)
                {
                    events::Push(
                        "autopilot objective script-destroy %s: by the last objective, none marked - a later step's",
                        sg.trigger.c_str());
                    continue;
                }
            }
            g_obj.scriptMarkN = alive;
            g_obj.fightObjective = true;
            events::Push("autopilot objective script-destroy %s: %d of %s (%d alive, nearest %.0f m) -> %s%s",
                         sg.trigger.c_str(), sg.count, sg.objectName.c_str(), alive, nearest, sg.leads.c_str(),
                         fromLast ? " (last objective)" : "");
            // far: to 1.5 km short of the nearest first, as a trip (no threat interrupts it) - as for a far "Destroy ..."
            if (nearest > 5000)
            {
                float q[3];
                for (int i = 0; i < 3; ++i)
                    q[i] = nearPos[i] + (mePilot.pos[i] - nearPos[i]) * (1500.0f / nearest);
                g_obj.travelObjective = true;
                return ShadowExit("script-destroy", Mode::Travel, 0, Action::ToMarks, StartTravel(0, q, 0, true));
            }
            return ShadowExit("script-destroy", Mode::Combat, 0, Action::ScriptKill, StartOrKeep(Mode::Combat, 0));
        }
    }
    // The script's own condition for this step (Source::Hidden: knowledge scope "all"; session 5, 22 Oct 2026 - the top-3
    // speedruns, "objectives are triggers": M12's "Destroy all of the Nomad Ships" in Omicron Minor advances the moment the
    // player is within 8 km of Toledo - the runners fly straight there, 63 s against our 224; M08's "Fly to the Waypoint"
    // advances when Ozu, who follows the player, is within 9 km of the jump hole). A flying condition armed for the step
    // that leads on (a new objective or the mission's success, never a failure) - for a fight step, a navmarker trip or no
    // objective: fly there instead. Not for escorts, docks or a step that already points at a target to reach.
    // (a target that is only the newest friendly mark - "Awaiting Mission Objective" - is no target of the step: M12 23 Oct
    // 2026, Omicron Minor's "Destroy all of the Nomad Ships" shows for 0 ms, then "Awaiting"; the mark fallback chose
    // Juni and the ship escorted her 165 s instead of flying to Toledo - runners 63 s)
    // A Dock objective overwritten by a nag (M12 Omicron Minor: "Dock with Planet Toledo", then the
    // `dont_run` nag - more than 5 km from Juni - marked her; when it ended the HUD showed "Awaiting", the script still
    // ends the mission on the dock (Cnd_SpaceExit), and the ship followed Juni for minutes): while only a text the script
    // does not know follows it, the last dock target is docked with
    // (a base or planet only: a lane, gate or hole is transit - M04's "Dock with the Trade Lane" -> "Awaiting" is the ambush
    // starting; the leash test below found no leash there and the dock trip fled the battle again, gate_s8w)
    if (verb == ObjVerb::Dock && tc.id && !(tc.type & (space::kTradelaneRing | space::kJumpGate | space::kJumpHole)))
        g_obj.lastDock = tc.id;
    else if (verb == ObjVerb::Dock)
        g_obj.lastDock = 0;
    else if (g_obj.lastDock)
    {
        bool placeholder = false;
        mission::AdvanceGoals(st.textId, fl::PlayerSystem(), &placeholder, HostObjectiveTitle());
        // (never with a ship leash in the system: M04's "Dock with the Trade Lane" -> "Awaiting" is the script moving on -
        // the ambush; the dock trip flew from Juni, the ring refused, "You fled the battle" 23 s after the launch,
        // gate_s8v)
        if (!placeholder || !FindContact(g_obj.lastDock).id || !mission::Leashes(fl::PlayerSystem()).empty())
            g_obj.lastDock = 0;
        else
        {
            events::Push(
                "autopilot objective decide reason=%s: text %u is no step of the script - the last dock (%u) goes on",
                reason, st.textId, g_obj.lastDock);
            return ShadowExit("dock-entry", Mode::Travel, g_obj.lastDock, Action::Dock,
                              StartTravel(g_obj.lastDock, nullptr, 0, false));
        }
    }
    // (`prestage_next`: the last objective the script set - every step, a dock and a lane ride too: the block below skips
    // those, and M04's "Dock with the Trade Lane" was never seen there; file scope since A37 S4 - the snapshot reads it)
    u32& lastReal = s_lastReal;
    if (st.textId && st.textId != 21660 && st.textId != 1)
        lastReal = st.textId;
    if (Knows(Source::Hidden) && g_set.scriptGoals && verb != ObjVerb::Escort && verb != ObjVerb::Dock &&
        (verb == ObjVerb::Fight || !tc.id || (markTarget && !HostileContact(tc))))
    {
        bool fromLast = false, leashRefused = false;
        std::vector<mission::Goal> goals =
            mission::AdvanceGoals(st.textId, fl::PlayerSystem(), &fromLast, HostObjectiveTitle());
        // A step with its own navmarker and a text no script knows (M10's exit-tunnel waypoints show a blank " ") is not
        // the last known objective: that one's goals are stale (gate_s8y - "navpoint_12" of the way in to
        // Bruchsal read as fired, the pre-stage sent the ship 24 km back toward Bundschuh base; the leash had hidden it)
        if (fromLast && st.hasNav)
            goals.clear();
        space::Pilot me;
        const mission::Goal* best = nullptr;
        float bestPos[3] = {}, bestDist = 1e30f;
        bool bestWins = false;                // leads to the mission's success
        const mission::Goal* fired = nullptr; // a condition we are inside already: it has fired (or fires now)
        // The mission's leashes (M10: under "Awaiting" the ship flew to a script goal 17.6 km off,
        // over 6 km from Von Claussen - "Mission Failed. You fled the battle."): a trip whose near side lies beyond 85 % of
        // a leash from its anchor is none. The anchor: the object where its id resolves (a solar), else the nearest
        // friendly mission ship (mission ships have runtime ids, not their nicknames').
        struct Anchor
        {
            float p[3];
            float dist;
        };
        // (only a leash the player is inside now: one already exceeded without a failure is not armed - the scripts hold
        // phase-bound ones, M04's 1000 m at a lane ring)
        std::vector<Anchor> anchors;
        space::Pilot mePilot0;
        const bool havePilot = space::ReadPilot(mePilot0);
        // (`leash_live`: the armed ones only, from the game's live trigger table - M02's need_to_dock 3 km from King is
        // armed only at the mission's end by comm_1330_king, yet held the whole Ashcroft step: as3 / as4's stage trips
        // turned back at 2.4-2.5 km; without the table: every listed leash, as before)
        const mission::LiveTable leashTable = g_set.leashLive ? mission::LiveTriggers() : mission::LiveTable{};
        const u32 leashSys = fl::PlayerSystem();
        for (const mission::Leash& l :
             leashTable.ok ? mission::ArmedLeashes(leashTable, leashSys) : mission::Leashes(leashSys))
        {
            // disarmed by the script: a comm line whose end deactivates it was heard in the last 30 min (M10's
            // DX_M10_0460B_BOTZLER; the script's own state, observed - mission::Leash::offComms). Wall time: the host
            // stamps the comm with GetTickCount64 (core\game\gameclock.h: never compared with the game clock).
            bool off = false;
            for (const u32 c : l.offComms)
            {
                const unsigned long long at = HostCommHeardAt(c);
                off = off || (at && GetTickCount64() - at < 30ull * 60 * 1000);
            }
            if (off)
            {
                events::Push("autopilot objective leash %s %.0f m: disarmed by the script (its comm was heard)",
                             l.name.c_str(), l.dist);
                if (g_obj.leashShip)
                    g_obj.leashShip = 0;
                continue;
            }
            Anchor a = {{}, l.dist};
            // (the step's own friendly ship target first: M10's mark under "Awaiting" is Von Claussen - the lead rule's
            // escort / mark list did not hold him, session 8 rerun: fled the battle again)
            // The anchor: the leash's own ship found by its name first - kept even while we are outside it: the fail trigger
            // arms later and fails at once if we are out then (M10: the name resolved Von Claussen 6 km+ off, read
            // as "not armed"; the fallbacks - the step's friendly target, the escort lead - were other ships, 362 / 395 /
            // 444, and the ship fled the battle four times); else the step's friendly target / the lead; a solar: itself
            space::Contact oc = FindContact(l.object);
            // (a mission ship: its runtime id is not its nickname's - found by its pilot's name id, mission::Leash::nameIds)
            if (!(oc.id && oc.ship) && l.nameIds)
            {
                // (by the name id, else by the pilot's name text: Contact::nameIds did not match individual_name in M10)
                const std::string want = events::IdsText(l.nameIds);
                for (const space::Contact& c : all)
                {
                    if (!c.ship)
                        continue;
                    bool same = c.nameIds == l.nameIds;
                    if (!same && c.rep && !want.empty())
                    {
                        const std::wstring rn = space::RepName(c.rep);
                        std::string n;
                        for (wchar_t ch : rn)
                            n += ch < 128 ? static_cast<char>(ch) : '?';
                        same = n == want;
                    }
                    if (same)
                        oc = c;
                }
            }
            const bool named = oc.id && oc.ship;
            const space::Contact lc = named                                     ? oc
                                      : tc.id && tc.ship && !HostileContact(tc) ? tc
                                                                                : FindContact(SurvivalLead(all));
            const char* how = named ? "ship (named)" : "ship";
            bool found = lc.id && (memcpy(a.p, lc.pos, sizeof(a.p)), true);
            float op[3];
            if (!named && space::ObjectPos(l.object, op) && (!found || (oc.id && !oc.ship)))
                memcpy(a.p, op, sizeof(a.p)), found = true, how = "object";
            const float d[3] = {a.p[0] - mePilot0.pos[0], a.p[1] - mePilot0.pos[1], a.p[2] - mePilot0.pos[2]};
            const bool in = found && havePilot && (named || Len3(d) < l.dist);
            if (in)
                anchors.push_back(a);
            if (in && lc.id && strncmp(how, "ship", 4) == 0)
                g_obj.leashShip = lc.id, g_obj.leashDist = l.dist; // (the supervisor keeps every trip to it)
            events::Push("autopilot objective leash %s %.0f m: %s%s", l.name.c_str(), l.dist,
                         !found ? "no anchor" : how, found ? (in ? " (kept)" : " (outside: not armed)") : "");
        }
        const auto leashed = [&](const float* p, float r)
        {
            for (const Anchor& a : anchors)
            {
                const float d[3] = {p[0] - a.p[0], p[1] - a.p[1], p[2] - a.p[2]};
                if (Len3(d) - 0.7f * r > 0.85f * a.dist)
                    return true;
            }
            return false;
        };
        if (space::ReadPilot(me))
            for (const mission::Goal& sg : goals)
            {
                // A fresh objective is no proof of the intended path (M04's "wrong_way": Juni near a cruiser -> "defend
                // yourselves"): a script goal replaces only a fight step, a step with no objective, or a trip whose
                // condition a ship that follows the player drives (M08: Ozu within 9 km of the hole); the mission's
                // success first, then the nearest.
                if (sg.kind == 3)
                {
                    // (destroy conditions: above) - all its objects gone = fired: the script talks before the next step
                    // (M12: generator 3 down -> King's 6.5 s line -> "Fly to the Waypoint"; the ship fought a fighter)
                    if (DestroyFired(sg, all) && !fired && sg.leads != "SUCCEED")
                        fired = &sg;
                    continue;
                }
                if (!(verb == ObjVerb::Fight || fromLast || sg.subject != "player"))
                    continue;
                // a fight is replaced only by a goal its own objective's chain arms (M05 "Destroy
                // the Rheinland Ships" left for a ring 29 km off, armed from the system entry - "You failed to destroy the
                // Rheinland ships"; M13's battleship the same, the Osiris / Sinclair goals in turn)
                const bool wins = sg.leads == "SUCCEED" || situation::LeadsToLanding(sg.leads);
                if (verb == ObjVerb::Fight && !fromLast && sg.byEntry)
                    continue;
                // ...and only by a goal that ends the mission (gate rerun, M02 "Destroy ..." -> within 2 km of the jump
                // point 53 km off, armed by the step's own chain: "You ran from the battle" - the battle nags
                // (Act_NagDistLeaving King 5 km) fail it; the M12 case this was built for runs under "Awaiting")
                if (verb == ObjVerb::Fight && !fromLast && !wins)
                    continue;
                if (bestWins && !wins)
                    continue;
                float p[3];
                if (sg.kind == 1 ? !space::ObjectPos(sg.object, p) : (memcpy(p, sg.pos, sizeof(p)), false))
                    continue;
                if (leashed(p, sg.dist))
                {
                    events::Push("autopilot objective script-goal %s: beyond a leash of the mission - no trip",
                                 sg.trigger.c_str());
                    leashRefused = true;
                    continue;
                }
                const float d[3] = {p[0] - me.pos[0], p[1] - me.pos[1], p[2] - me.pos[2]};
                const float far = geom::Len(d);
                // inside already: the trigger fires on its own - or has fired (M12 23 Oct 2026: 678 m from the core point
                // of "within 700 m" under "Awaiting", the 0.9 margin chose it over the power cell to tractor, the trip
                // found "no way" and the ship rested in the core)
                if (far <= sg.dist)
                {
                    if (!fired && sg.leads != "SUCCEED")
                        fired = &sg;
                    continue;
                }
                if (g_obj.badGoal == sg.trigger || g_obj.deadGoals.count(sg.trigger))
                    continue; // its trip found no way this step / it was reached and did not fire
                if ((wins && !bestWins) || far - sg.dist < bestDist)
                    bestDist = far - sg.dist, best = &sg, bestWins = wins, memcpy(bestPos, p, sizeof(p));
            }
        // a navmarker trip whose marker lies inside the goal's reach is the same trip
        if (best && st.hasNav && verb != ObjVerb::Fight)
        {
            const float d[3] = {st.nav[0] - bestPos[0], st.nav[1] - bestPos[1], st.nav[2] - bestPos[2]};
            if (geom::Len(d) < best->dist)
                best = nullptr;
        }
        if (best)
        {
            // to 70 % of the condition's distance on our side of the object or point
            float q[3];
            const float d[3] = {me.pos[0] - bestPos[0], me.pos[1] - bestPos[1], me.pos[2] - bestPos[2]};
            const float l = std::max(1.0f, geom::Len(d));
            for (int i = 0; i < 3; ++i)
                q[i] = bestPos[i] + d[i] / l * best->dist * 0.7f;
            events::Push("autopilot objective script-goal %s within %.0f m of %s (%s) -> %s, %.0f m to go%s",
                         best->trigger.c_str(), best->dist, best->kind == 1 ? best->objectName.c_str() : "a point",
                         best->subject.c_str(), best->leads.c_str(), bestDist, fromLast ? " (last objective)" : "");
            g_obj.travelObjective = true;
            g_obj.goalTrigger = best->trigger;
            // A goal that arms the next one inside its own sphere is passed on the way to that one (23 Oct
            // 2026, M12 Omicron Minor: the trip to 0.7 x 8 km of Toledo arrived, braked to 52 m/s, lost the cruise and
            // charged again for the 3 km ring trigger it armed - ~20 s; the runners cruise straight to the ring): aim at
            // the armed goal's near side, flown through - the first trigger fires in passing, the second ends the step.
            for (const mission::Goal& ng : mission::GoalsArmedBy(best->trigger, best->file, fl::PlayerSystem()))
            {
                float gp[3];
                if (ng.kind == 3 || ng.subject != "player" ||
                    !(ng.kind == 2 ? (memcpy(gp, ng.pos, sizeof(gp)), true) : space::ObjectPos(ng.object, gp)))
                    continue;
                if (leashed(gp, ng.dist))
                    continue;
                const float in[3] = {gp[0] - bestPos[0], gp[1] - bestPos[1], gp[2] - bestPos[2]};
                const float tg[3] = {gp[0] - me.pos[0], gp[1] - me.pos[1], gp[2] - me.pos[2]};
                const float tl = Len3(tg);
                if (Len3(in) > best->dist || tl <= ng.dist)
                    continue; // only a goal inside the first one's sphere (the line to it crosses that sphere)
                float q2[3], dir[3];
                for (int i = 0; i < 3; ++i)
                    q2[i] = gp[i] - tg[i] / tl * ng.dist * 0.7f, dir[i] = tg[i] / tl;
                events::Push("autopilot objective script-goal %s arms %s inside it: through to within %.0f m of %s, "
                             "%.0f m to go",
                             best->trigger.c_str(), ng.trigger.c_str(), ng.dist,
                             ng.kind == 1 ? ng.objectName.c_str() : "a point", tl - ng.dist);
                memcpy(g_obj.flyGoal, gp, sizeof(gp)), g_obj.flyGoalR = ng.dist;
                const bool ok = StartTravel(0, q2, 0, true);
                DecFly(ok, dir, ng.dist, ng.dist * 0.7f);
                return ShadowExit("script-goal", Mode::Travel, 0, Action::ScriptGoal, ok);
            }
            return ShadowExit("script-goal", Mode::Travel, 0, Action::ScriptGoal, StartTravel(0, q, 0, true));
        }
        // A fired distance condition whose chain arms the next one (M12 Omicron Minor: "within 8 km
        // of Toledo" fired -> "within 3 km of its ring" -> "Dock with Planet Toledo"; with nothing armed in view the ship
        // followed Juni at 5.6 km): the armed one is the trip
        if (fired && fired->kind != 3 && verb != ObjVerb::Fight)
            for (const mission::Goal& ng : mission::GoalsArmedBy(fired->trigger, fired->file, fl::PlayerSystem()))
            {
                float gp[3];
                if (ng.kind == 3 || ng.subject != "player" || g_obj.deadGoals.count(ng.trigger) ||
                    !(ng.kind == 2 ? (memcpy(gp, ng.pos, sizeof(gp)), true) : space::ObjectPos(ng.object, gp)))
                    continue;
                if (leashed(gp, ng.dist))
                    continue;
                const float tg[3] = {gp[0] - me.pos[0], gp[1] - me.pos[1], gp[2] - me.pos[2]};
                const float tl = Len3(tg);
                if (tl <= ng.dist)
                    continue;
                float q[3];
                for (int i = 0; i < 3; ++i)
                    q[i] = gp[i] - tg[i] / tl * ng.dist * 0.7f;
                events::Push("autopilot objective script-goal %s armed by %s: within %.0f m of %s -> %s, %.0f m to go",
                             ng.trigger.c_str(), fired->trigger.c_str(), ng.dist,
                             ng.kind == 1 ? ng.objectName.c_str() : "a point", ng.leads.c_str(), tl - ng.dist);
                g_obj.travelObjective = true;
                g_obj.goalTrigger = ng.trigger;
                memcpy(g_obj.flyGoal, gp, sizeof(gp)), g_obj.flyGoalR = ng.dist;
                return ShadowExit("script-goal", Mode::Travel, 0, Action::ScriptGoal, StartTravel(0, q, 0, true));
            }
        // The step's condition has fired and the script still talks before the next objective (a placeholder stands in:
        // fromLast) - be where the next step's targets are (the user watching M12: the ship fought 5
        // interceptors through the lair's ~31 s dialogue and reached generator 1 12 s after "Destroy all the Shield
        // Generators"; the user's try 3 and run 1 wait beside generator 1 and fire the second the banner shows - the
        // interceptors are never fought). The nearest live object of the next step's destroy condition: there by a trip,
        // then a hold 300 m off its centre on our side; the threat check leaves both alone ("pre-stage").
        // (also a fight step whose destroy condition is met while the script talks: M12's generator 3 -> King's 6.5 s line
        // -> "Fly to the Waypoint" within 500 m of the airlock's top - the ship fought a fighter meanwhile; the next step's
        // distance condition is flown to, its near side, through)
        if (fired && ((fromLast && verb == ObjVerb::None) || (fired->kind == 3 && verb == ObjVerb::Fight)))
            for (const mission::Goal& ng : mission::GoalsAfter(fired->leads, fired->file, fl::PlayerSystem()))
            {
                float gp[3];
                if ((ng.kind == 1 || ng.kind == 2) && ng.subject == "player" &&
                    (ng.kind == 2 ? (memcpy(gp, ng.pos, sizeof(gp)), true) : space::ObjectPos(ng.object, gp)))
                {
                    const float tg[3] = {gp[0] - me.pos[0], gp[1] - me.pos[1], gp[2] - me.pos[2]};
                    const float tl = Len3(tg);
                    if (tl <= ng.dist)
                        continue; // inside it already: the step fires the moment it is set
                    float q[3], dir[3];
                    for (int i = 0; i < 3; ++i)
                        q[i] = gp[i] - tg[i] / tl * ng.dist * 0.7f, dir[i] = tg[i] / tl;
                    events::Push("autopilot objective pre-stage %s -> %s: next step's %s, within %.0f m, %.0f m to go",
                                 fired->trigger.c_str(), fired->leads.c_str(), ng.trigger.c_str(), ng.dist,
                                 tl - ng.dist);
                    g_obj.travelObjective = true;
                    memcpy(g_obj.flyGoal, gp, sizeof(gp)), g_obj.flyGoalR = ng.dist;
                    const bool ok = StartTravel(0, q, 0, true);
                    DecFly(ok, dir, ng.dist, ng.dist * 0.7f);
                    return ShadowExit("pre-stage", Mode::Travel, 0, Action::PreStage, ok);
                }
                if (ng.kind != 3)
                    continue;
                float nearPos[3] = {}, nearest = 1e30f;
                u32 nearId = 0;
                for (u32 id : ng.objects)
                {
                    float p[3];
                    if (!space::ObjectPos(id, p))
                        continue;
                    const float d[3] = {p[0] - me.pos[0], p[1] - me.pos[1], p[2] - me.pos[2]};
                    if (Len3(d) < nearest)
                        nearest = Len3(d), nearId = id, memcpy(nearPos, p, sizeof(p));
                }
                if (!nearId)
                    continue;
                float q[3];
                const float stand = std::min(300.0f, nearest);
                for (int i = 0; i < 3; ++i)
                    q[i] = nearPos[i] + (me.pos[i] - nearPos[i]) * (stand / std::max(1.0f, nearest));
                events::Push("autopilot objective pre-stage %s -> %s: next step's %s, nearest %u at %.0f m",
                             fired->trigger.c_str(), fired->leads.c_str(), ng.trigger.c_str(), nearId, nearest);
                g_obj.travelObjective = true;
                g_obj.preStageTarget = nearId;
                if (nearest > 700)
                    return ShadowExit("pre-stage", Mode::Travel, 0, Action::PreStage, StartTravel(0, q, 0, true));
                return ShadowExit("pre-stage", Mode::Hold, 0, Action::PreStage, DecHoldAt(q));
            }
        // `prestage_next` (M04: "immediately move at top speed to Benford when reaching the
        // badlands, no waiting. This is speedrun mode, after all"; the runners cruise the 5.9 km from the last lane to
        // Benford in 22-24 s, 79-90 % cruise): a placeholder step after an objective whose end was no distance condition
        // (a lane ride: nothing "fired") - the script still talks before the next objective (M04: "Dock with the Trade
        // Lane" -> Juni's DX_M04_0410C -> "Fly to the Waypoint", 20 s of following her at 78 m/s): the next step's own
        // distance goal, read through the dialogue chain (mission::NextStepGoals), is flown to now - its near side.
        {
            std::string next;
            if (g_set.prestageNext && !fired && verb == ObjVerb::None && (fromLast || st.textId == 21660) && lastReal &&
                !st.hasNav && space::ReadPilot(me))
                for (const mission::Goal& ng : mission::NextStepGoals(lastReal, fl::PlayerSystem(), &next))
                {
                    float gp[3];
                    if (!((ng.kind == 1 || ng.kind == 2) && ng.subject == "player" &&
                          (ng.kind == 2 ? (memcpy(gp, ng.pos, sizeof(gp)), true) : space::ObjectPos(ng.object, gp))))
                        continue;
                    if (leashed(gp, ng.dist))
                        continue; // (M04: Juni's 6 km leash - the trip may not break it)
                    const float tg[3] = {gp[0] - me.pos[0], gp[1] - me.pos[1], gp[2] - me.pos[2]};
                    const float tl = Len3(tg);
                    if (tl <= ng.dist)
                        continue;
                    float q[3], dir[3];
                    for (int i = 0; i < 3; ++i)
                        q[i] = gp[i] - tg[i] / tl * ng.dist * 0.7f, dir[i] = tg[i] / tl;
                    events::Push("autopilot objective pre-stage-next %u -> %s: its %s, within %.0f m, %.0f m to go", lastReal,
                                 next.c_str(), ng.trigger.c_str(), ng.dist, tl - ng.dist);
                    g_obj.travelObjective = true;
                    memcpy(g_obj.flyGoal, gp, sizeof(gp)), g_obj.flyGoalR = ng.dist;
                    const bool ok = StartTravel(0, q, 0, true);
                    DecFly(ok, dir, ng.dist, ng.dist * 0.7f);
                    return ShadowExit("pre-stage-next", Mode::Travel, 0, Action::PreStage, ok);
                }
            // ...else the objective the script declares next, where it has a point here (M04: the chain from "Dock with
            // the Trade Lane" runs through lane-exit conditions and comms - none of them a goal the reader follows; the
            // [NNObjective] order has "Fly to the Waypoint" depot_wp1 right after it)
            float np[3];
            u32 nextText = 0;
            if (g_set.prestageNext && !fired && verb == ObjVerb::None && (fromLast || st.textId == 21660) && lastReal &&
                !st.hasNav && space::ReadPilot(me) &&
                mission::NextObjectivePlace(lastReal, fl::PlayerSystem(), np, &nextText) && !leashed(np, 0))
            {
                const float tg[3] = {np[0] - me.pos[0], np[1] - me.pos[1], np[2] - me.pos[2]};
                if (Len3(tg) > 1000.0f)
                {
                    events::Push("autopilot objective pre-stage-next %u -> %u (declared next): its point, %.0f m to go",
                                 lastReal, nextText, Len3(tg));
                    g_obj.travelObjective = true;
                    return ShadowExit("pre-stage-next", Mode::Travel, 0, Action::PreStage, StartTravel(0, np, 0, true));
                }
            }
        }
    }
    // `chase_kill` (ASH-START, docs\research\runners\M02.md "## Plan" 1; Source::Hidden + Meta): a later step's kill ship
    // armed now, alive, hostile and targetable (LOOK's kill verdict) is fought now in a step that asks no fight of its own -
    // M02 "Follow Ashcroft" (22262): the navmarker trip below never targeted him (LOOK_PLAN 7: tgt=0 until +103 s, the pod
    // segment 115 s against the runners' 18-29 s, who catch him during his ~8 s cruise charge). The combat chases a target
    // faster than our engine in cruise and the coast guard fires on him by engine-kill coasts (ChaseStep)
    if (!laneEscape)
    {
        lookahead::LookFacts cf;
        lookahead::LookResult cr;
        if (const u32 cs = ChaseCandidate(st, verb, all, cf, cr))
        {
            g_obj.chaseShip = cs, g_obj.chaseAsked = cs;
            // (spawn_charge: the ship the stage expected - his contact; ChargeFleeing reads it from now on)
            if (g_set.spawnCharge && !g_obj.chargeShip.empty() && cr.ship == g_obj.chargeShip)
                g_obj.chargeId = cs;
            if (g_obj.chaseSaid != cs)
            {
                g_obj.chaseSaid = cs;
                float dSpawn = -1;
                for (const lookahead::KillGoal& ckg : cf.kills)
                    for (const lookahead::KillShip& cks : ckg.ships)
                        if (cks.id == cs && cks.spawnKnown && cf.havePilot)
                        {
                            const float cd[3] = {cks.spawn[0] - cf.me[0], cks.spawn[1] - cf.me[1],
                                                 cks.spawn[2] - cf.me[2]};
                            dSpawn = Len3(cd);
                        }
                fl::Kinematics ck = {};
                flight::Model cm;
                const bool cruise = flight::ReadModel(cm) && cm.cruiseEngaged;
                events::Push("autopilot objective chase %s=%u d_spawn=%.0f dist=%.0f speed=%.0f cruise=%d trigger=%s",
                             cr.ship.c_str(), cs, dSpawn, cr.dist, fl::PlayerKinematics(ck) ? Len3(ck.vel) : -1.0f,
                             cruise ? 1 : 0, cr.trigger.c_str());
            }
            g_obj.travelObjective = false;
            return ShadowExit("chase", Mode::Combat, cs, Action::ScriptKill, StartOrKeep(Mode::Combat, cs));
        }
    }
    // A travel objective (fly to / dock with a place or a navmarker - not a fight, an escort, a defence or a ship to
    // follow) outranks the fight around it: the story sends the player elsewhere ("the autopilot
    // should prioritize breaking off from a fight if a story objective is telling the player to go elsewhere"; M05
    // Sprague: "Fly to the Waypoint Coordinates" while endless wings spawn - it fought 90 s first). Once there
    // (arrived), a threat is fought as before; the mission says "Destroy ..." when a fight is required.
    // (a Dock verb at a solar is a trip even when it still reads hostile: M09 Tekagi's base turns friendly only when the
    // player starts the dock maneuver - it was fought for 4 minutes)
    // ...and any non-fight step with a navmarker that has gone 2 min: the trip outranks the fights from then on (M13 22 Oct
    // 2026: "Fly to the Waypoint" on the hostile Nomad City - the endless waves pulled the ship into fight after fight for
    // 12 min; the speedrun rule: fight only what the script needs)
    const bool longStep = st.hasNav && verb != ObjVerb::Fight && verb != ObjVerb::Escort && g_obj.stepAt &&
                          gameclock::NowMs64() - g_obj.stepAt > 120000;
    // ...and a hostile place with a navmarker from the start (M13 from the city: "time wastes"; the 2020
    // playthrough flies straight to the Nomad City through the fighters, 20:42-21:15 - ours fought 8 of them for 2 min
    // first: a "Fly to" never asks for a fight, a "Destroy ..." does)
    const bool travelObjective =
        verb != ObjVerb::Fight && verb != ObjVerb::Escort && !g_obj.arrived &&
        ((tc.id && !tc.ship && (!HostileContact(tc) || verb == ObjVerb::Dock || st.hasNav || longStep)) ||
         (!tc.id && st.hasNav));
    g_obj.travelObjective = travelObjective;
    // `spawn_stage` (ASH-START, with chase_kill; Source::Hidden): a fight step's last kill pending, whose firing spawns a
    // later step's fleeing kill ship (M02 "Destroy the Enemy Rogues": the 4th escort's death spawns Ashcroft, who cruises
    // off to his run point 25 km away) - wait beside that spawn inside the fight's leaving nag, the last ship fought there
    // when it comes within reach; 25 s there without the kill: the fight as before (fail closed). The runners are cruising
    // 0.8-2 km from his spawn when the 4th escort dies; we sat 4.6 km off at rest (M02.md M02.ashcroft / M02.pod)
    if (g_set.spawnStage && g_set.chaseKill && Knows(Source::Hidden) && verb == ObjVerb::Fight && !laneEscape &&
        !travelObjective && !g_obj.survival)
    {
        const StagePlan sp = SpawnStageNow(st, all);
        float toGo = 0;
        int v = StageVerdictNow(sp, &toGo);
        // (spawn_charge, design 3: the fleeing ship's facts for the chase - armed whether or not the point was refused)
        if (g_set.spawnCharge && sp.fled)
        {
            if (g_obj.chargeShip != sp.ship)
                g_obj.chargeId = 0, g_obj.chargeSaid = 0;
            g_obj.chargeShip = sp.ship;
            memcpy(g_obj.chargeSpawn, sp.spawn, sizeof(g_obj.chargeSpawn));
            memcpy(g_obj.chargeRun, sp.run, sizeof(g_obj.chargeRun));
        }
        static std::string said;
        const std::string key = std::to_string(st.textId) + " " + sp.trigger + " " + std::to_string(v) + " " +
                                (sp.ok ? std::string() : sp.chargeRefused ? std::string("charge-refused") : sp.why);
        if (v == 4)
        {
            g_obj.stageTimedOut.insert(sp.trigger);
            events::Push("autopilot objective spawn-stage %s timeout -> fight (%.0f s at the point)", sp.trigger.c_str(),
                         lookahead::kStageTimeoutS);
            g_obj.stageTrigger.clear(), g_obj.stageAt = 0, g_obj.stageLast = 0;
            v = 0;
        }
        else if (said != key)
        {
            said = key;
            if (sp.ok)
                events::Push("autopilot objective spawn-stage %s %d/%d alive -> %s spawn d=%.0f P=(%.0f,%.0f) anchor=%s "
                             "%.0f/%.0f m %s to_go=%.0f -> %s (%s)",
                             sp.trigger.c_str(), sp.alive, sp.need, sp.ship.c_str(), sp.st.dSpawn, sp.st.p[0],
                             sp.st.p[2], sp.anchorName.empty() ? "-" : sp.anchorName.c_str(), sp.st.dAnchor, sp.leash,
                             sp.st.why.c_str(), toGo,
                             v == 1 ? "travel" : v == 2 ? "fight" : v == 3 ? "hold" : "today", sp.why.c_str());
            else if (!sp.trigger.empty())
                events::Push("autopilot objective spawn-stage %s: none (%s)", sp.trigger.c_str(), sp.why.c_str());
            // spawn_charge's line: the point and the leashes it keeps to (`bound` the ones that pulled it), the gap the
            // charge is expected to lock at (ChargeGap from the point's distance, our press 0 / 0.5 / 1 s after his) -
            // or why the charge's point is not used
            if (g_set.spawnCharge && sp.charge && sp.ok)
            {
                const lookahead::GapPrediction g0 = lookahead::ChargeGap(sp.st.dSpawn, 0.0f),
                                               g5 = lookahead::ChargeGap(sp.st.dSpawn, 0.5f),
                                               g1 = lookahead::ChargeGap(sp.st.dSpawn, 1.0f);
                events::Push("autopilot spawn-charge stage P=(%.0f,%.0f) d_spawn=%.0f leashes=%d clamp=%s bound=%s "
                             "anchor_d=%.0f expect_lock=%.0f/%.0f/%.0f min=%.0f -> %s ship=%s",
                             sp.st.p[0], sp.st.p[2], sp.st.dSpawn, sp.chargeLeashes, sp.st.why.c_str(),
                             sp.chargeBound.empty() ? "-" : sp.chargeBound.c_str(), sp.st.dAnchor, g0.lockGap,
                             g5.lockGap, g1.lockGap, g0.minGap,
                             v == 1 ? "travel" : v == 2 ? "fight" : v == 3 ? "hold" : "today", sp.ship.c_str());
            }
            else if (g_set.spawnCharge && sp.chargeRefused)
                events::Push("autopilot spawn-charge none (%s) leashes=%d bound=%s -> fight ship=%s",
                             sp.chargeWhy.c_str(), sp.chargeLeashes, sp.chargeBound.empty() ? "-" : sp.chargeBound.c_str(),
                             sp.ship.c_str());
            else if (g_set.spawnCharge && sp.ok && !sp.chargeWhy.empty())
                events::Push("autopilot spawn-charge fallback (%s) -> the spawn_stage point", sp.chargeWhy.c_str());
        }
        if (v)
        {
            g_obj.chargeStage = sp.charge;
            g_obj.stageLastShip = sp.last;
            if (g_obj.stageTrigger != sp.trigger)
                g_obj.stageTrigger = sp.trigger, g_obj.stageAt = 0;
            memcpy(g_obj.stagePos, sp.st.p, sizeof(g_obj.stagePos));
            memcpy(g_obj.stageDir, sp.st.u, sizeof(g_obj.stageDir));
            g_obj.stageVerdict = g_obj.stageLast = v;
            g_obj.stageAsked = sp.trigger;
            if (v != 1 && !g_obj.stageAt)
                g_obj.stageAt = gameclock::NowMs64();
            // (Action::PreStage: the supervisor's threat check leaves the trip and the hold alone - the last ship is left
            // to the fight's own allies until it comes within reach of the point)
            if (v == 1)
            {
                g_obj.travelObjective = true;
                return ShadowExit("spawn-stage", Mode::Travel, 0, Action::PreStage, StartTravel(0, sp.st.p, 0, true));
            }
            if (v == 2)
                return ShadowExit("spawn-stage", Mode::Combat, sp.last, Action::ScriptKill,
                                  StartOrKeep(Mode::Combat, sp.last));
            g_obj.stageNose = true;
            return ShadowExit("spawn-stage", Mode::Hold, 0, Action::PreStage, DecHoldAt(sp.st.p));
        }
        g_obj.stageTrigger.clear(), g_obj.stageLast = 0;
    }
    // A "Destroy ..." step whose marked targets are all far (M13 22 Oct 2026: the city's generators 39-62 km away, the
    // fighter waves dragged the ship off for 18 min - a fight engages within ~3 km only): to 1.5 km short of the nearest
    // first, as a trip (no threat interrupts it)
    if (verb == ObjVerb::Fight)
    {
        u32 mk[32];
        const int mn = MissionObjectIds(mk, 32, all);
        const space::Contact* nm = nullptr;
        for (const space::Contact& c : all)
            if (c.health != 0 && std::find(mk, mk + mn, c.id) != mk + mn && !FriendlyContact(c) &&
                (!nm || c.distance < nm->distance))
                nm = &c;
        space::Pilot me;
        // (`big_coast`: a huge static mark is flown at by the fight itself - its approach cruises in and drops 300 m outside
        // gun reach of the hull for the coast; the trip below stopped 1500 m short at rest - "it drops
        // cruise, STOPS, and then goes off doing ... whatever")
        bool hugeStatic = false;
        if (nm && g_set.bigCoast && !nm->ship)
            for (const space::Obstacle& o : space::Obstacles(nm->pos, 50))
                if (o.id == nm->id && o.radius >= 500.0f)
                    hugeStatic = true;
        if (hugeStatic)
            return ShadowExit("mark-fight", Mode::Combat, nm->id, Action::Fight, StartOrKeep(Mode::Combat, nm->id));
        if (nm && nm->distance > 5000 && space::ReadPilot(me))
        {
            float p[3];
            for (int i = 0; i < 3; ++i)
                p[i] = nm->pos[i] + (me.pos[i] - nm->pos[i]) * (1500.0f / nm->distance);
            g_obj.travelObjective = true;
            events::Push("autopilot objective marked target %u is %.0f m off: there first", nm->id, nm->distance);
            return ShadowExit("mark-fight", Mode::Travel, 0, Action::ToMarks, StartTravel(0, p, 0, true));
        }
    }
    // The survival intent: a Survive objective, or the mission's own title (host API v13: "ESCAPE FROM LIBERTY" - the
    // objective "Defend Yourself!" can pass unseen). Kept for the whole mission (the title), across the
    // objective mode's restarts (a docked scene and relaunch restarted it at Benford and it fought the cruisers).
    const u32 title = HostObjectiveTitle();
    if (title != g_survivalTitle)
    {
        g_survivalTitle = title;
        g_survivalMission = false;
        // By the title's string id first - the same in every language of the game (research A12: English
        // words alone missed it in a translated install): 22602 "ESCAPE FROM LIBERTY" (M04), the story's one escape
        // title (resources.tsv). The words stay as the fallback for a mod's own titles.
        static const u32 kEscapeTitles[] = {22602};
        for (const u32 id : kEscapeTitles)
            g_survivalMission = g_survivalMission || title == id;
        std::string t = title && !g_survivalMission ? events::IdsText(title) : std::string();
        for (char& ch : t)
            ch = static_cast<char>(tolower(static_cast<unsigned char>(ch)));
        if (t.find("escape") != std::string::npos || t.find("flee") != std::string::npos ||
            t.find("survive") != std::string::npos)
            g_survivalMission = true;
        g_survivalEnded = false;
        space::Pilot sp;
        g_survivalSystem = space::ReadPilot(sp) ? sp.system : 0;
    }
    // `survival_until_jump` (M04: "the entire mission up to and after Benford's destruction does
    // not benefit from the player doggedly pursuing time on target. After the player jumps ... that's when speedrunning
    // kills has meaning"): an escape is out of its system at the first jump - the title's stance ends there (the hunters,
    // the wing and Mactan are fought by the normal rules: M04.mactan, +104 s over par, was the stance's 1200 m reach)
    if (g_set.survivalUntilJump && g_survivalMission && !g_survivalEnded && g_survivalSystem)
    {
        space::Pilot sp;
        if (space::ReadPilot(sp) && sp.system && sp.system != g_survivalSystem)
        {
            g_survivalEnded = true;
            g_survivalMission = false;
            g_obj.survival = false;
            events::Push("autopilot objective survival ends: out of the mission's first system (%u -> %u)",
                         g_survivalSystem, sp.system);
        }
    }
    if (verb == ObjVerb::Survive && !g_survivalEnded)
        g_survivalMission = true;
    if (g_survivalMission && !g_obj.survival)
    {
        g_obj.survival = true;
        events::Push("autopilot objective survival text=%u title=%u (no free fights: attackers within %.0f m only)",
                     st.textId, title, kSurvivalReach);
    }
    // pre_aggro: a ship the script is about to turn hostile, near us, is fought now, whatever the step (s12c: the bounty
    // hunters flagged 31 s before "Destroy the Bounty Hunters" while "Fly to Freeport 4" travelled on - a travel objective
    // takes no threat, and the survival stance's target choice only close attackers); never on a dock / lane step
    // (nor on an escort step; with an armed ship leash only a ship within 75 % of it from the leash's anchor - the chase
    // must not break the leash; review, session 12)
    if (verb != ObjVerb::Dock && verb != ObjVerb::Escort && !laneEscape)
        if (const u32 ph = PreHostileNear(all))
        {
            bool inLeash = true;
            if (g_obj.leashShip)
            {
                const space::Contact lc = FindContact(g_obj.leashShip), pc = FindContact(ph);
                if (lc.id && pc.id)
                {
                    const float d[3] = {pc.pos[0] - lc.pos[0], pc.pos[1] - lc.pos[1], pc.pos[2] - lc.pos[2]};
                    inLeash = Len3(d) < 0.75f * g_obj.leashDist;
                }
            }
            if (inLeash)
                return ShadowExit("pre-aggro", Mode::Combat, ph, Action::PreAggro, StartOrKeep(Mode::Combat, ph));
        }
    // Survival with something attacking us and no named target to deal with: stay with the friendly ship the mission
    // brought (its nearest marked / mission ship - Juni) and fight only what comes at us there; none: fight only the
    // close attackers (the combat's reach is kSurvivalReach under this stance).
    // (`survival_calm`, the owner 29 Oct 2026: "better to save the effort and balance survivability and no taking hull
    // damage, while remaining close to the upcoming objective" - the runners' M04 Unity section: 0-2 kills, hull lost
    // 0.0-0.04, run 1 parked at the Unity's hull: under the stance a "destroy" step and its marks are not pursued either)
    const bool calm = g_obj.survival && g_set.survivalCalm;
    // `survival_stage` (speedrun knowledge; the owner, 29 Oct 2026, M04: "wasted some time before starting the first
    // trade lane, it was busy fighting rather than sticking close to the trade lane. Remember - it's a speedrun, lookahead
    // knowledge is key"): under the calm stance a step with no place of its own ("Defend Yourself!", "Destroy the
    // Battleship Unity": 0, 0, 0 markers) waits where the script's next place is - M04: Trade Lane Ring 147, declared
    // two objectives on - fighting only what shoots at us there; the lane step then starts at the ring (lane3 was +25 s
    // over par from a ring 2.4 km off)
    if (calm && g_set.survivalStage && Knows(Source::Hidden) && !laneEscape && !travelObjective && !st.hasNav)
    {
        const u32 from = st.textId && st.textId != 21660 && st.textId != 1 ? st.textId : lastReal;
        float sp[3];
        u32 nt = 0;
        space::Pilot mp;
        if (from && mission::NextObjectivePlace(from, fl::PlayerSystem(), sp, &nt, 3) && space::ReadPilot(mp))
        {
            const float tg[3] = {sp[0] - mp.pos[0], sp[1] - mp.pos[1], sp[2] - mp.pos[2]};
            const float tl = Len3(tg);
            if (ObjectiveThreat(all) && tl < 1500.0f)
                return ShadowExit("survival-stage", Mode::Combat, 0, Action::Survive, StartOrKeep(Mode::Combat, 0));
            float q[3];
            for (int i = 0; i < 3; ++i)
                q[i] = sp[i] - (tl > 1 ? tg[i] / tl : 0.0f) * 300.0f; // (300 m short: beside the ring, not in its mouth)
            static u32 said = 0;
            if (said != nt)
            {
                said = nt;
                events::Push("autopilot objective survival stage %u -> next place %u, %.0f m off", from, nt, tl);
            }
            if (tl > 700.0f)
                return ShadowExit("survival-stage", Mode::Travel, 0, Action::Survive, StartTravel(0, q, 0, true));
            return ShadowExit("survival-stage", Mode::Hold, 0, Action::Survive, StartOrKeep(Mode::Hold, 0));
        }
    }
    if (g_obj.survival && !laneEscape && !travelObjective && (verb != ObjVerb::Fight || calm) &&
        (calm || !(tc.id && HostileContact(tc))) && ObjectiveThreat(all))
    {
        if (const u32 lead = SurvivalLead(all))
            return ShadowExit("survival", Mode::Escort, lead, Action::Survive, StartOrKeep(Mode::Escort, lead));
        return ShadowExit("survival", Mode::Combat, 0, Action::Survive, StartOrKeep(Mode::Combat, 0));
    }
    // `kill_ahead` in a fight step (M13-MIMIC item 4, docs\research\runners\M13.md "## Plan" 4: be at the battleship from
    // its spawn - nomadcap spawns 36.9 s after the launch, "Destroy the Nomad Battleship" shows 12.4 s later; R1 is at its
    // hull 10 s before the banner, R2 fires from 1,358 m 12 s before it, td1 was 4,923 m out fighting interceptors): the
    // next step's destroy target, alive and safe to kill now (mission::KillAheadSafe), becomes this fight's mark
    // (a "defend" step with no object to escort that we fight in counts as a fight step: M13's 24414 "Defend the Order
    // Base" reads as Escort with ref 0 - the form never fired from Save3989, set ab1 31 Oct 2026: 13.7 s on interceptors
    // after nomadcap's spawn; S415 starts past it - the step runs Mode::Escort round the base, target 29, set ab3)
    const bool defendFight = verb == ObjVerb::Escort && !st.refCount;
    if (g_set.killAhead && Knows(Source::Hidden) && g_set.scriptGoals && (verb == ObjVerb::Fight || defendFight) &&
        !laneEscape && !travelObjective && !g_obj.survival && st.textId && st.textId != 21660)
    {
        space::Pilot me;
        const bool placed = space::ReadPilot(me);
        const mission::LiveTable kaLt = mission::LiveTriggers();
        for (const mission::Goal& ng : FightAheadGoals(st.textId, kaLt, defendFight))
        {
            if (ng.kind != 3 || !placed)
                continue;
            int alive = 0;
            u32 nearId = 0;
            float nearest = 1e30f;
            for (u32 id : ng.objects)
            {
                const auto ci = std::find_if(all.begin(), all.end(), [&](const space::Contact& c) { return c.id == id; });
                if (ci == all.end() || ci->health == 0 || alive >= 8)
                    continue;
                g_obj.scriptMarks[alive++] = id;
                if (ci->distance < nearest)
                    nearest = ci->distance, nearId = id;
            }
            if (!alive)
                continue;
            const mission::KillSafe ks = mission::KillAheadSafe(kaLt, nearId, 1);
            static std::string said;
            char key[200];
            sprintf_s(key, "%u %s %u %s", st.textId, ng.trigger.c_str(), nearId, ks.ok ? "fight" : ks.why.c_str());
            if (said != key)
            {
                said = key;
                events::Push("autopilot objective kill-ahead %s (next step's, in fight %u): %u at %.0f m -> %s%s",
                             ng.trigger.c_str(), st.textId, nearId, nearest, ks.ok ? "fight" : "today's fight: ",
                             ks.ok ? "" : ks.why.c_str());
            }
            if (!ks.ok)
                continue;
            g_obj.scriptMarkN = alive;
            g_obj.fightObjective = true;
            return ShadowExit("kill-ahead", Mode::Combat, 0, Action::ScriptKill, StartOrKeep(Mode::Combat, 0));
        }
    }
    // (a "destroy" step with nothing in reach rests: the threat check starts the fight once its targets come)
    // (survival: a hostile objective target is fought only when the objective says so - M04 step 1's "Dock with the
    // Trade Lane" points at TL146, a ring of the now hostile Liberty: the fight around it killed the ship in 7 s)
    if (!laneEscape && !travelObjective &&
        (ObjectiveThreat(all) || (tc.id && HostileContact(tc) && (tc.ship || tc.maxHit < 1e30f) &&
                                  (!g_obj.survival || (verb == ObjVerb::Fight && !calm)))))
    {
        if (verb == ObjVerb::Escort && tc.id && tc.ship && !HostileContact(tc))
            return ShadowExit("threat-fight", Mode::Escort, tc.id, Action::Escort, StartOrKeep(Mode::Escort, tc.id));
        // A placeholder step's marked hostiles while an escort is active are the escort's threats: the escort keeps
        // fighting them carried along with its ship, never a Combat chase away from it (set s13a: the
        // chase after the Rheinland wing at Mactan killed in 33-38 s against the escort's 43-44, but ended off Juni with
        // a hold / follow interlude and a longer trip - M04.mactan +16.8 s; the user: keep the escort). The escort's own
        // ship first (g.escorted, alive), else the survival stance's lead; none -> the fight as below.
        if (markFight)
        {
            u32 lead = 0;
            if (g.mode == Mode::Escort && g.escorted)
                for (const space::Contact& c : all)
                    if (c.id == g.escorted && c.ship && c.health != 0)
                        lead = c.id;
            if (!lead && g_obj.survival)
                lead = SurvivalLead(all);
            if (lead)
                return ShadowExit("threat-fight", Mode::Escort, lead, Action::Fight, StartOrKeep(Mode::Escort, lead));
        }
        // Survival: the objective's own hostile ship is the chosen target ("Destroy the Battleship Unity": the ranking no
        // longer lifts marked big ships under the stance - they are scenery otherwise)
        if (g_obj.survival && verb == ObjVerb::Fight && tc.id && tc.ship && HostileContact(tc))
            return ShadowExit("threat-fight", Mode::Combat, tc.id, Action::Fight, StartOrKeep(Mode::Combat, tc.id));
        return ShadowExit("threat-fight", Mode::Combat, 0, Action::Fight, StartOrKeep(Mode::Combat, 0));
    }
    // "Escort the Transport Convoy to the First Buoy": the target is where to (a buoy), the escorted ship is the friendly
    // ship the mission marks (M03: more than 1200 m from transport1 fails the mission - the travel flew ahead to the
    // buoy). The nearest friendly marked ship; formation as for an escort target.
    if (tc.id && tc.ship && !HostileContact(tc))
        g_obj.escortShip = tc.id; // "Fly to the Transport Convoy": the ship the escort is about
    if (verb == ObjVerb::Escort && !(tc.id && tc.ship))
    {
        // the ship the mission pointed at earlier (M03: transport1 - the convoy moves only with the player in its
        // formation; the nearest marked friendly was another ship, the convoy stalled at buoy 2), else the nearest
        // friendly ship the mission marks
        u32 marks[32];
        const int n = MissionObjectIds(marks, 32, all);
        const space::Contact* best = nullptr;
        for (const space::Contact& c : all)
            if (c.ship && c.id == g_obj.escortShip && c.health != 0)
                best = &c;
        // the ship the objective names first ("Escort Sinclair to the Battleship Osiris", M13 22 Oct 2026: the nearest
        // marked friendly was Juni - who follows the player: neither moved, Sinclair went alone and the nag failed the
        // mission), by the game's own pilot / ship name; else the nearest
        std::string text = events::IdsText(st.textId);
        for (char& ch : text)
            ch = static_cast<char>(tolower(static_cast<unsigned char>(ch)));
        // any word of 4+ letters of the name, as a word of the text (M13: the ship is "Sinclair and Quintaine")
        auto named = [&](const space::Contact& c)
        {
            const std::wstring w = space::SpeakerName(c.id) + L" ";
            std::string word;
            for (wchar_t ch : w)
            {
                if (ch < 128 && isalpha(static_cast<unsigned char>(ch)))
                {
                    word += static_cast<char>(tolower(ch));
                    continue;
                }
                if (word.size() >= 4)
                    for (size_t at = text.find(word); at != std::string::npos; at = text.find(word, at + 1))
                    {
                        const size_t end = at + word.size();
                        if ((at == 0 || !isalpha(static_cast<unsigned char>(text[at - 1]))) &&
                            (end >= text.size() || !isalpha(static_cast<unsigned char>(text[end]))))
                            return true;
                    }
                word.clear();
            }
            return false;
        };
        if (!best)
            for (const space::Contact& c : all)
                if (c.ship && !HostileContact(c) && std::find(marks, marks + n, c.id) != marks + n)
                {
                    const bool cn = named(c), bn = best && named(*best);
                    if (!best || (cn && !bn) || (cn == bn && c.distance < best->distance))
                        best = &c;
                }
        if (best && named(*best))
            events::Push("autopilot objective escort the named ship %u", best->id);
        if (best)
        {
            if (g_obj.sub != Mode::Formation && g_obj.sub != Mode::Follow)
                return ShadowExit("escort", Mode::Formation, best->id, Action::Formation,
                                  StartOrKeep(Mode::Formation, best->id));
            // (StartOrKeep never writes g_obj.sub - Start's restart stop writes only lastStop / lastStopReason - so the
            // arguments' evaluation order cannot change the mode or action read here)
            return ShadowExit("escort", g_obj.sub, best->id, g_obj.sub == Mode::Formation ? Action::Formation : Action::Follow,
                              StartOrKeep(g_obj.sub, best->id));
        }
    }
    if (tc.id && tc.ship)
    {
        if (verb == ObjVerb::Escort && g_obj.sub != Mode::Formation)
            return ShadowExit("ship-target", Mode::Formation, tc.id, Action::Formation, StartOrKeep(Mode::Formation, tc.id));
        return ShadowExit("ship-target", Mode::Follow, tc.id, Action::Follow, StartOrKeep(Mode::Follow, tc.id));
    }
    // "Dock with the Trade Lane" (a dock verb at a lane ring): ride it toward the neighbour the mission lets the player
    // take (space::LaneOpen - M03 step 5 opens only 42 -> 43; was: the travel arrived beside the ring and rested, 19 Oct
    // 2026). Far away: arrive near it first (the travel's routing); close: the fast lane entry toward that ring.
    u32 laneNext = 0, lanePrev = 0;
    if (tc.id && verb == ObjVerb::Dock && space::LaneLinks(tc.id, laneNext, lanePrev))
    {
        u32 toward = 0;
        bool known = false;
        for (u32 cand : {laneNext, lanePrev})
        {
            const int o = cand ? space::LaneOpen(tc.id, cand) : -1;
            known |= o >= 0;
            if (o == 1 && !toward)
                toward = cand;
        }
        if (!toward && known)
        {
            // closed now, but the objective names it: the mission opens it later (M04 West Point: when Juni reaches
            // TL134, then 100 s to enter) - wait beside the ring, re-decided when it opens (ObjectiveSupervise)
            g_obj.laneRing = tc.id;
            events::Push("autopilot objective lane=%u closed both ways (the mission's lane permission): waiting at it",
                         tc.id);
            if (tc.distance > 1500 && !g_obj.arrived)
                return ShadowExit("lane", Mode::Travel, tc.id, Action::LaneWait, StartTravel(tc.id, nullptr, 0, true));
            return ShadowExit("lane", Mode::Hold, 0, Action::LaneWait, StartOrKeep(Mode::Hold, 0));
        }
        g_obj.laneRing = 0;
        if (!toward)
            toward = laneNext ? laneNext : lanePrev;
        if (tc.distance > 3000 && !g_obj.arrived) // (action "lane": the threat check leaves the approach alone)
            return ShadowExit("lane", Mode::Travel, tc.id, Action::Lane, StartTravel(tc.id, nullptr, 0, true));
        const bool ok = DecDock(tc.id, toward);
        events::Push("autopilot objective lane=%u toward=%u (ride)", tc.id, toward);
        return ShadowExit("lane", Mode::Dock, tc.id, Action::Lane, ok);
    }
    // Survival never rests in place (M04 20 Oct 2026: "Awaiting Mission Objective" -> hold beside the Unity battle,
    // dead in 6 s): with nothing to fly to, stay with the lead the mission brought (Juni) - she leads the way out.
    const u32 lead = g_obj.survival && !(tc.id && !g_obj.arrived) && !st.hasNav ? SurvivalLead(all) : 0;
    // ... and beside her, rest the way a player waits: in place, the nose on the scene (HoldLook -> MissionSubject: a
    // place before a wingman - at Mactan look at Mactan, not at Juni); the supervisor rejoins her
    // the moment she moves off (survive-hold)
    // (`rule`: the shadow's branch name - the arrived rest or the final rest)
    const auto stayWithLead = [&](const char* rule)
    {
        const space::Contact lc = FindContact(lead);
        if (lc.id && lc.distance < g_set.followDistance)
            return ShadowExit(rule, Mode::Hold, 0, Action::SurviveHold, StartOrKeep(Mode::Hold, 0));
        return ShadowExit(rule, Mode::Escort, lead, Action::SurviveStay, StartOrKeep(Mode::Escort, lead));
    };
    // A travel the mission flies with a leader it named (its formation order): stay in that formation - the trip's
    // trigger may be the leader's arrival (M09 Ozu), and the leader is the mission's pace
    if (travelObjective && g_obj.missionLeader)
    {
        const space::Contact lc = FindContact(g_obj.missionLeader);
        if (lc.id && lc.health != 0 && FriendlyContact(lc))
            return ShadowExit("mission-form", Mode::Formation, lc.id, Action::MissionForm,
                              StartOrKeep(Mode::Formation, lc.id));
        g_obj.missionLeader = 0;
    }
    if (verb == ObjVerb::None || verb == ObjVerb::Survive)
        if (const float r = PassRingRadius(tc))
        {
            // the travel already flies through it (its look-ahead predicted this ring): keep it
            if (g.mode == Mode::Travel && g.tr.passRing == tc.id)
                return ShadowExit("pass", Mode::Travel, tc.id, Action::Pass, true, true);
            return ShadowExit("pass", Mode::Travel, tc.id, Action::Pass, DecPass(tc.id, r));
        }
    // A route objective (several steps: M07 "Fly to Planet Kyushu" = 5 entries, lane rings then the planet; the host
    // hands over entry 0 only): the game's waypoint list carries the whole route - when the target is one of its entries
    // but not the last, fly the list to its end (lanes and all). Was: arrived beside the first ring, looped there, and the
    // nag toward the planet failed the mission.
    {
        // host v15: the route's entries themselves - travel to the last one (the travel plans the lanes on the way)
        ObjectiveStep steps[8];
        const int ns = HostObjectiveSteps(steps, 8);
        // the mission's own route first: step 0 a lane ring -> ride it toward the neighbour on the way to step 1 (the
        // objective advances as the ride passes its rings; a best path of our own took another lane and the nag toward
        // Kyushu failed M07)
        u32 n0 = 0, p0 = 0;
        if (ns >= 2 && steps[ns - 1].textId == st.textId && tc.id && space::LaneLinks(tc.id, n0, p0))
        {
            u32 s1 = 0;
            for (int i = 0; i < steps[1].refCount && !s1; ++i)
                s1 = space::ObjectiveObject(steps[1].refs[i], all);
            float s1pos[3] = {};
            const bool haveS1 = s1 ? space::ObjectPos(s1, s1pos) : steps[1].hasNav;
            if (!s1 && steps[1].hasNav)
                memcpy(s1pos, steps[1].nav, sizeof(s1pos));
            u32 toward = 0;
            float best = 1e30f;
            for (u32 cand : {n0, p0})
            {
                float cp[3];
                if (!cand || !haveS1 || !space::ObjectPos(cand, cp))
                    continue;
                const float d[3] = {cp[0] - s1pos[0], cp[1] - s1pos[1], cp[2] - s1pos[2]};
                if (Len3(d) < best)
                    best = Len3(d), toward = cand;
            }
            if (toward)
            {
                events::Push("autopilot objective route-lane ring=%u toward=%u (step 1 %u)", tc.id, toward, s1);
                if (tc.distance > 3000 && !g_obj.arrived)
                    return ShadowExit("route-lane", Mode::Travel, tc.id, Action::Lane,
                                      StartTravel(tc.id, nullptr, 0, true));
                const bool ok = DecDock(tc.id, toward);
                return ShadowExit("route-lane", Mode::Dock, tc.id, Action::Lane, ok);
            }
        }
        // (the route-lane ride above goes on from beside its ring; everything below rests once its trip is done)
        if (g_obj.arrived) // this step's travel is done: rest there (scenic hold)
            return lead ? stayWithLead("arrived")
                        : ShadowExit("arrived", Mode::Hold, 0, Action::Rest, StartOrKeep(Mode::Hold, 0));
        if (ns >= 2 && steps[ns - 1].textId == st.textId)
        {
            const ObjectiveStep& last = steps[ns - 1];
            u32 lastObj = 0;
            for (int i = 0; i < last.refCount && !lastObj; ++i)
                lastObj = space::ObjectiveObject(last.refs[i], all);
            const bool dock = verb == ObjVerb::Dock;
            bool ok = false;
            if (lastObj)
            {
                const u32 ring = space::PlanetDockRing(lastObj);
                ok = StartTravel(ring ? ring : lastObj, nullptr, 0, !dock);
            }
            else if (last.hasNav)
                ok = StartTravel(0, last.nav, last.navSystem, true);
            if (lastObj || last.hasNav)
            {
                events::Push("autopilot objective route: %d steps, to the last (object %u)", ns, lastObj);
                return ShadowExit("route", Mode::Travel, lastObj, Action::Route, ok);
            }
        }
    }
    if (tc.id)
    {
        const std::vector<space::Waypoint> wl = space::Waypoints();
        for (size_t i = 0; i + 1 < wl.size(); ++i)
        {
            const float d[3] = {wl[i].pos[0] - tc.pos[0], wl[i].pos[1] - tc.pos[1], wl[i].pos[2] - tc.pos[2]};
            if (wl[i].object != tc.id && Len3(d) > 1500)
                continue;
            const bool ok = DecWaypoints(verb != ObjVerb::Dock);
            events::Push("autopilot objective route: the mission's waypoint list (%u entries, target is entry %u)",
                         static_cast<u32>(wl.size()), static_cast<u32>(i));
            return ShadowExit("waypoints", Mode::Travel, tc.id, Action::Route, ok);
        }
    }
    if (tc.id)
    {
        const u32 ring = space::PlanetDockRing(tc.id); // a planet: its docking ring (when it has one)
        const u32 dest = ring ? ring : tc.id;
        // an airlock as the objective's own target is a way through, whatever the verb (M13 "Enter the Dyson Sphere":
        // the travel arrived beside St03_dyson_airlock_1 and the ship fought the Nomads outside until it died)
        // (a Nomad airlock reads hostile: the mission opened it, PlayerCanDock - as a Dock verb at a hostile base, abb5b5a)
        const bool dock = verb == ObjVerb::Dock || (!tc.ship && (tc.type & 0x400u));
        return ShadowExit("arrive", Mode::Travel, dest, dock ? Action::Dock : Action::Arrive,
                          StartTravel(dest, nullptr, 0, !dock));
    }
    // A navmarker on a way through - an airlock, a gate, a hole (M12 "Exit the Structure": the marker sits on the
    // core's exit dock St02c_core_exit; arriving beside it stalled 517 m off and the ship rested): go
    // through that object
    if (st.hasNav)
    {
        const space::Contact* way = nullptr;
        for (const space::Contact& c : all)
        {
            if (c.ship || !(c.type & (0x400u | space::kJumpGate | space::kJumpHole))) // airlock (0x400), gate, hole
                continue;
            const float d[3] = {c.pos[0] - st.nav[0], c.pos[1] - st.nav[1], c.pos[2] - st.nav[2]};
            // on it, not near it (M12 "Fly to the Core": its marker 616 m from the core's exit dock - the old 800 m sent
            // the ship to the exit, crawling through the core's radiation)
            if (Len3(d) < 200 && (!way || c.distance < way->distance))
                way = &c;
        }
        if (way && !HostileContact(*way))
        {
            events::Push("autopilot objective navmarker on a dockable object %u: through it", way->id);
            return ShadowExit("nav-way", Mode::Travel, way->id, Action::Dock, StartTravel(way->id, nullptr, 0, false));
        }
    }
    if (st.hasNav)
    {
        // A retry after ending short of the marker goes by a via point first: 500 m off the straight line at its middle,
        // up / down / left / right in turn (M12 22 Oct 2026: pinned against the Nomad core's inner structure, every retry
        // on the same blocked line). Then the marker.
        space::Pilot p;
        if (g_obj.viaNext && g_obj.arriveRetries > 0 && space::ReadPilot(p) &&
            (!st.navSystem || st.navSystem == p.system))
        {
            g_obj.viaNext = false;
            float d[3] = {st.nav[0] - p.pos[0], st.nav[1] - p.pos[1], st.nav[2] - p.pos[2]};
            const float dl = std::max(1.0f, Len3(d));
            for (float& x : d)
                x /= dl;
            // a perpendicular pair: world up projected off the line, and the line x up
            float up[3] = {-d[1] * d[0], 1 - d[1] * d[1], -d[1] * d[2]};
            const float ul = Len3(up);
            if (ul < 0.1f)
                up[0] = 1, up[1] = 0, up[2] = 0;
            else
                geom::Norm(up);
            float side[3];
            geom::Cross(side, d, up);
            const int k = (g_obj.arriveRetries - 1) % 4;
            const float* dir = k < 2 ? up : side;
            const float sgn = (k % 2) ? -1.0f : 1.0f;
            float via[3];
            for (int i = 0; i < 3; ++i)
                via[i] = (p.pos[i] + st.nav[i]) * 0.5f + dir[i] * sgn * 500.0f;
            g_obj.inVia = true;
            events::Push("autopilot objective via point %d (%.0f,%.0f,%.0f) before the marker", k, via[0], via[1],
                         via[2]);
            return ShadowExit("via", Mode::Travel, 0, Action::Via, StartTravel(0, via, p.system, true));
        }
        // Fly-through (the user's M12 core play-by-play: "flies ahead to the waypoint" without
        // slowing - the step ends inside the script's radius, run 1 never brakes): the script's own condition for this
        // step is the player within R of a point that holds the marker -> aim beyond the marker along the approach.
        // A marker near the sphere's edge (M12 "Fly to the Waypoint" above the airlock: marker y
        // 924, condition "within 500 m of y 474" - the ship climbed to the marker and braked there, 45 s; the user's try 3
        // went straight at the airlock, inside the sphere, ~5 s): the trip goes to the sphere's near side (0.7 R from its
        // centre toward us), flown through.
        space::Pilot me;
        float dest[3] = {st.nav[0], st.nav[1], st.nav[2]};
        float flyDir[3] = {}, flyR = 0;
        float flyAhead = 0;
        const char* flyWhy = nullptr;
        if (Knows(Source::Hidden) && g_set.scriptGoals && space::ReadPilot(me) &&
            (!st.navSystem || st.navSystem == fl::PlayerSystem()))
        {
            for (const mission::Goal& sg :
                 mission::AdvanceGoals(st.textId, fl::PlayerSystem(), nullptr, HostObjectiveTitle()))
            {
                float gp[3];
                if (sg.subject != "player" || (sg.kind == 1 ? !space::ObjectPos(sg.object, gp) : sg.kind != 2))
                    continue;
                if (sg.kind == 2)
                    memcpy(gp, sg.pos, sizeof(gp));
                const float off[3] = {st.nav[0] - gp[0], st.nav[1] - gp[1], st.nav[2] - gp[2]};
                if (Len3(off) > sg.dist)
                    continue; // the marker must lie inside the condition's sphere
                const float tg[3] = {gp[0] - me.pos[0], gp[1] - me.pos[1], gp[2] - me.pos[2]};
                if (Len3(tg) <= sg.dist)
                    break; // inside already
                if (Len3(off) > sg.dist * 0.5f)
                {
                    // near the edge: the sphere's near side instead of the marker
                    const float tl = Len3(tg);
                    for (int i = 0; i < 3; ++i)
                        dest[i] = gp[i] - tg[i] / tl * sg.dist * 0.7f;
                    flyWhy = "the condition's near side";
                    flyAhead = sg.dist * 0.7f;
                }
                else
                    flyWhy = "the marker";
                float d[3] = {dest[0] - me.pos[0], dest[1] - me.pos[1], dest[2] - me.pos[2]};
                const float dl = std::max(1.0f, Len3(d));
                for (int i = 0; i < 3; ++i)
                    flyDir[i] = d[i] / dl;
                flyR = sg.dist;
                memcpy(g_obj.flyGoal, gp, sizeof(gp)), g_obj.flyGoalR = sg.dist;
                events::Push(
                    "autopilot objective fly-through %s: within %.0f m ends the step - aim %s (%.0f,%.0f,%.0f)",
                    sg.trigger.c_str(), sg.dist, flyWhy, dest[0], dest[1], dest[2]);
                break;
            }
        }
        const bool started = StartTravel(0, dest, st.navSystem, true);
        if (flyWhy)
            DecFly(started, flyDir, flyR, flyAhead);
        return ShadowExit("navmarker", Mode::Travel, 0, Action::Navmarker, started);
    }
    if (lead)
        return stayWithLead("rest");
    ShadowExit("rest", Mode::Hold, 0, Action::Rest, StartOrKeep(Mode::Hold, 0));
}

} // namespace autopilot
