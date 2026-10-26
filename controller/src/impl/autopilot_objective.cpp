// Advanced Autopilot - objective mode: what the current objective asks. Part of autopilot.cpp until 21 Oct 2026; shared state and declarations in
// autopilot_impl.h, the module API in autopilot.h.
#include "autopilot_impl.h"

namespace autopilot
{
// ------------------------------------------------------------------ objective mode
// Starts a travel (ap start travel's): to an object of this system, or a point of a system (0 = this one).
static bool StartTravel(u32 object, const float* point, u32 system, bool noDock)
{
    // A trip to where the running one already goes is that trip (session 8, 23 Oct 2026, M12 gen 3 -> airlock: the
    // pre-stage trip flew at 200 m/s toward the airlock top; "Fly to the Waypoint" 7.5 s later aimed 47 m from it, and the
    // restart braked to 80, paused 1.5 s to plot and planned a 3.2 km way back round the lattice - 19.5 s for ~1.5 km)
    if (point && !object && g.mode == Mode::Travel && g.tr.hasDest && !g.tr.destObject && g.tr.noDock == noDock &&
        (system ? system : fl::PlayerSystem()) == (g.tr.destSystem ? g.tr.destSystem : fl::PlayerSystem()))
    {
        const float d[3] = {point[0] - g.tr.dest[0], point[1] - g.tr.dest[1], point[2] - g.tr.dest[2]};
        if (Len3(d) < 150.0f)
        {
            events::Push("autopilot objective trip kept: the new destination lies %.0f m from the running one's",
                         Len3(d));
            return true;
        }
    }
    // ...and a trip to the object the running one goes to (session 8, M12 Omicron Minor hole: "Take the Jump Hole" came
    // 2 s after our request was granted and the game's dock had the ship; the restart stopped it 147 m out among 10
    // Nomads - every run restarted there, one died in 4 s)
    if (object && !point && g.mode == Mode::Travel && g.tr.destObject == object && g.tr.noDock == noDock)
    {
        events::Push("autopilot objective trip kept: already on the way to %u", object);
        return true;
    }
    State::Travel t;
    t.noDock = noDock;
    t.destObject = object;
    if (point)
    {
        t.hasDest = true;
        memcpy(t.dest, point, sizeof(t.dest));
        t.destSystem = system;
    }
    else
        t.hasDest = space::ObjectPos(object, t.dest);
    if (!t.hasDest)
        return false;
    t.phase = State::Travel::Plot;
    if (!Start(Mode::Travel, t.destObject, nullptr))
        return false;
    g.tr = t;
    return true;
}

// A ring the objective wants flown through (M06's race: "Beat Hovis to the Final Gate" and the gates' blank-titled
// objectives point at Br05_track_ring_*, passed by Cnd_DistCircle - 22 Oct 2026, the probe arrived beside ring 2 and
// waited; Hovis won both races, race 2's 30 s gate timer failed the mission): a satellite-class solar that is no lane
// ring and no dock, large enough to fly through (track ring: physical radius 88 m; a buoy is a satellite too, ~2.4 m: never through one). Its radius, else 0.
static bool HostileContact(const space::Contact& c);

float PassRingRadius(const space::Contact& c)
{
    if (!c.id || c.ship || c.dock || !(c.type & 0x10u) || (c.type & space::kTradelaneRing))
        return 0;
    u32 next = 0, prev = 0;
    if (space::LaneLinks(c.id, next, prev))
        return 0;
    for (const space::Obstacle& o : space::Obstacles(c.pos, 50))
        if (o.id == c.id)
            // track ring 88 m physical, a nav buoy ~2.4; nothing bigger than a ring can be "flown through" (M13 22 Oct
            // 2026: the Nomad City, a satellite-class solar, read as a race ring - braked at 0 m/s for 25 min)
            return o.radius >= 30 && o.radius <= 300 && !HostileContact(c) ? o.radius : 0;
    return 0;
}

bool StartPass(u32 ring, float radius)
{
    State::Travel t;
    t.destObject = ring;
    t.noDock = true;
    t.hasDest = space::ObjectPos(ring, t.dest);
    t.passRing = ring;
    t.passRadius = radius;
    t.phase = State::Travel::Fly;
    if (!t.hasDest || !Start(Mode::Travel, ring, nullptr))
        return false;
    g.tr = t;
    events::Push("autopilot objective pass ring=%u radius=%.0f", ring, radius);
    return true;
}

// What the objective's words ask beyond "go there" (the words only refine what the target's kind decides: a dock, an
// escort, a fight; English text - other languages fall back to the kind alone).
enum class ObjVerb
{
    None,
    Dock,
    Escort,
    Fight,
    Survive // "Defend Yourself!", "Survive", "Escape": staying alive is the task, kills are not
};

// Under a survival intent the objective mode takes no free fights (user, 20 Oct 2026, M04 verified from the 2020
// footage: "fast-and-cheap survival" to Benford - no missile, no CM, 0 of 14 nanobots / batteries, no hull damage,
// ~280 s; our free fights killed 25-32 ships in ~520 s, and the script answers every wing destroyed with its backup).
// Only attackers this close are fought; with a friendly mission ship near (Juni), the stance escorts her instead.

static ObjVerb ObjectiveVerb(u32 textId)
{
    std::string s = textId ? events::IdsText(textId) : std::string();
    for (char& ch : s)
        ch = static_cast<char>(tolower(static_cast<unsigned char>(ch)));
    // a word (or phrase) starting at a word boundary: "land " inside "Rheinland Agents" made M07's "Destroy the
    // Rheinland Agents" a dock order (22 Oct 2026)
    const auto has = [&](const char* w)
    {
        for (size_t at = s.find(w); at != std::string::npos; at = s.find(w, at + 1))
            if (at == 0 || !isalpha(static_cast<unsigned char>(s[at - 1])))
                return true;
        return false;
    };
    // "Enter the Dyson Sphere" (M13: its target is the sphere's airlock - arriving beside it left the ship outside)
    // "Meet Juni in the Manhattan Bar" (FT1 -> M02 chain, 22 Oct 2026: arrived beside the planet, never landed): a bar
    // is inside a base
    // (a whole word: "Head to the Barrier", M13, is no bar)
    const auto word = [&](const char* w)
    {
        const size_t n = strlen(w);
        for (size_t at = s.find(w); at != std::string::npos; at = s.find(w, at + 1))
            if ((at == 0 || !isalpha(static_cast<unsigned char>(s[at - 1]))) &&
                (at + n >= s.size() || !isalpha(static_cast<unsigned char>(s[at + n]))))
                return true;
        return false;
    };
    // ("Enter Formation with the USV Brandt", M01a: a formation, not a dock - the chain failed "never entered formation")
    if (has("dock") || has("land ") || (has("enter ") && !has("formation")) || word("bar"))
        return ObjVerb::Dock;
    // before "defend": "Defend Yourself!" asks for survival (M04 step 2: the wings keep coming), not an escort
    if (has("defend yourself") || has("survive") || has("stay alive") || has("escape"))
        return ObjVerb::Survive;
    if (has("escort") || has("protect") || has("defend") || has("guard") || has("stay with") || has("formation"))
        return ObjVerb::Escort;
    if (has("destroy") || has("kill") || has("eliminate") || has("attack") || has("disable"))
        return ObjVerb::Fight;
    return ObjVerb::None;
}

static bool HostileContact(const space::Contact& c)
{
    float att = 0;
    return c.ship ? c.attitude <= -0.6f : (space::AttitudeToPlayer(c.id, att) && att <= -0.6f);
}

// The objective asks for a destruction: a Fight verb in any of its entries' texts (a job's entry 0 is a blank step,
// "Destroy %L0v3!" comes as entry 1 - Destroy Contraband, HDE, 26 Oct 2026).
bool ObjectiveAsksDestroy()
{
    ObjectiveStep steps[8];
    const int n = HostObjectiveSteps(steps, 8);
    for (int i = 0; i < n; ++i)
        if (ObjectiveVerb(steps[i].textId) == ObjVerb::Fight)
            return true;
    ObjectiveStep st = {};
    return HostObjectiveNow(&st) && ObjectiveVerb(st.textId) == ObjVerb::Fight;
}

// Loot the mission marks while its objective asks for a destruction is the thing to destroy, never to collect (the owner,
// 26 Oct 2026, Destroy Contraband: "the job wants the cargo DESTROYED ... tries to LOOT it instead" - TractorMarkedLoot
// pulled each marked pod in, the job dropped a new one, 7 times in 12 s). The nearest such object; 0 none.
u32 DestroyMarkedLoot(const std::vector<space::Contact>& all, float* dist)
{
    u32 mk[32];
    const int mn = MissionObjectIds(mk, 32, all);
    const space::Contact* best = nullptr;
    for (const space::Contact& c : all) // (nearest first)
        if ((c.type & space::kLoot) && std::find(mk, mk + mn, c.id) != mk + mn && (!best || c.distance < best->distance))
            best = &c;
    if (!best || !ObjectiveAsksDestroy())
        return 0;
    if (dist)
        *dist = best->distance;
    return best->id;
}

// pre_aggro: the nearest live ship the script is about to turn hostile, within the switch's range (Contact::preHostile);
// 0 none
static u32 PreHostileNear(const std::vector<space::Contact>& all)
{
    for (const space::Contact& c : all) // (nearest first)
        if (c.ship && c.preHostile && c.health != 0)
            return c.id;
    return 0;
}

// Our side: a friendly ship, or a station / platform / ring whose reputation is friendly to the player.
bool FriendlyContact(const space::Contact& c)
{
    float att = 0;
    return c.ship ? c.attitude > 0 : (space::AttitudeToPlayer(c.id, att) && att > 0);
}

// A fight the objective mode must take now: a hostile the mission points at within 6 km (the harness's mission battle
// reach), a hostile ship attacking us within 3 km, or one attacking our side (a friendly ship, station, platform)
// within the battle's reach (M03 Battle of Willard, 20 Oct 2026: bombers spawned 7 km out attacking the station were
// seen and not engaged). The game's maxHit cross-check stays with the target choice.
static bool ObjectiveThreat(const std::vector<space::Contact>& all)
{
    u32 ids[32];
    const int n = MissionObjectIds(ids, 32, all);
    for (const space::Contact& c : all)
    {
        // (pre_aggro's flagged ships have their own branch in ObjectiveDecide and their own 3 s interrupt: counted here,
        // every non-travel mode re-decided at 0.5 s with one in range - review, session 12)
        if (c.health == 0 || !HostileContact(c) || c.preHostile)
            continue;
        const bool pointed = std::find(ids, ids + n, c.id) != ids + n;
        // survival: only what attacks us up close (not the battle around - its friendlies die by script in M04) - and
        // under a fight objective the ships it marks (M04 Mactan's Rheinland wing, s12d)
        if (g_obj.survival)
        {
            if (c.ship && c.targetsPlayer && c.distance < kSurvivalReach)
                return true;
            if (g_obj.fightObjective && pointed && c.ship && c.distance < 6000)
                return true;
            continue;
        }
        if ((pointed && c.distance < 6000 && (c.ship || c.maxHit < 1e30f)) ||
            (c.ship && c.targetsPlayer && c.distance < 3000))
            return true;
        if (c.ship && c.targetId && c.distance < kBattleReach)
            for (const space::Contact& f : all)
                if (f.id == c.targetId && f.id != c.id && FriendlyContact(f))
                    return true;
    }
    return false;
}

// A destroy condition met: every object it names was seen alive and is gone now (only objects seen alive count: M13's city
// generators spawn after the step begins - "none alive" then is no kill).
// (one seen in the contact list and gone from it is dead - a destroyed solar may still answer ObjectPos: M12's generator 3,
// the condition never read as met)
static std::set<u32> s_seenAlive, s_seenListed;
static bool DestroyFired(const mission::Goal& sg, const std::vector<space::Contact>& all)
{
    bool any = false, allSeen = !sg.objects.empty();
    float p[3];
    for (u32 id : sg.objects)
    {
        const auto ci = std::find_if(all.begin(), all.end(), [&](const space::Contact& c) { return c.id == id; });
        if (ci != all.end())
            s_seenListed.insert(id);
        const bool live = ci != all.end() ? ci->health != 0 : !s_seenListed.count(id) && space::ObjectPos(id, p);
        if (live)
            s_seenAlive.insert(id);
        any = any || live;
        allSeen = allSeen && s_seenAlive.count(id);
    }
    return !any && allSeen;
}

// Under the survival stance: the friendly ship the mission brought along (the objective's last friendly ship, or the
// nearest friendly mission object - Juni), within 6 km; 0 none.
static u32 SurvivalLead(const std::vector<space::Contact>& all)
{
    u32 mids[32];
    const int mn = MissionObjectIds(mids, 32, all);
    const space::Contact* lead = nullptr;
    for (const space::Contact& c : all)
        if (c.ship && c.health != 0 && FriendlyContact(c) && c.distance < 6000 &&
            (c.id == g_obj.escortShip || std::find(mids, mids + mn, c.id) != mids + mn) &&
            (!lead || c.distance < lead->distance))
            lead = &c;
    return lead ? lead->id : 0;
}

// Start a mode for the objective - or keep it when it already runs with that target (a fight goes on across the
// objective changes it causes: "Awaiting Mission Objective", "Destroy ..." restarted it twice, M03 19 Oct 2026).
static bool StartOrKeep(Mode m, u32 target)
{
    if (g.mode == m && m != Mode::Travel && (g.target == target || (m == Mode::Combat && !target)))
        return true;
    return Start(m, target, nullptr);
}

static void ObjectiveStart(Mode m, u32 target, Action action, bool ok)
{
    g_obj.sub = ok ? m : Mode::Off;
    g_obj.subTarget = target;
    g_obj.subSince = GetTickCount64();
    // (an enum since 24 Oct 2026; as a char[16], a 17-char name with strcpy_s was a fail-fast crash, 22 Oct 2026)
    g_obj.action = action;
}

// Back toward a leash's ship: our trip to where it is now (sphere avoidance), not the game's follow, which flies straight
// at cruise (session 8, M10 exit field: the follow hit a big asteroid at ~380 m/s, hull 0.98 -> dead, 1239 s). True while
// such a trip runs and still ends within 1 km of the ship.
static bool LeashTripRuns(const space::Contact& lc)
{
    if (g.mode != Mode::Travel || g_obj.action != Action::Leash || !g.tr.hasDest || g.tr.destObject)
        return false;
    const float d[3] = {lc.pos[0] - g.tr.dest[0], lc.pos[1] - g.tr.dest[1], lc.pos[2] - g.tr.dest[2]};
    return Len3(d) < 1000.0f;
}
static void LeashBack(const space::Contact& lc)
{
    ObjectiveStart(Mode::Travel, 0, Action::Leash, StartTravel(0, lc.pos, 0, true));
}

// Decides what the current objective asks and starts it. Ranking (the mission's own pointers first, then the game's
// data about them): a fight the mission points at or that attacks us; the objective's target object - a friendly ship
// to join (formation, the game refuses it with capital ships: follow) or escort, a hostile to fight, a planet's docking
// ring, a station / ring / gate to fly to (dock only when the words say so); the navmarker; otherwise rest (Hold).
void ObjectiveDecide(const char* reason)
{
    g_obj.tractorExitPending = false; // (set again below while it still applies)
    // Loop guard: more than 6 decisions in 10 s on one step (a fight that ends at once, a refused start) - pause 5 s.
    const ULONGLONG now = GetTickCount64();
    if (now - g_obj.windowStart > 10000)
        g_obj.windowStart = now, g_obj.decisions = 0;
    if (++g_obj.decisions > 6)
    {
        events::Push("autopilot objective decide-loop reason=%s (more than 6 in 10 s): pausing 5 s", reason);
        g_obj.nextLook = now + 5000;
        g_obj.windowStart = now + 5000;
        g_obj.decisions = 0;
        return;
    }
    ObjectiveStep st = {};
    const u32 seq = HostObjectiveNow(&st);
    if (seq != g_obj.seq)
        g_obj.arrived = false, g_obj.arriveRetries = 0, g_obj.stepAt = GetTickCount64(),
        g_obj.viaNext = g_obj.inVia = false, g_obj.badGoal.clear(), g_obj.deadGoals.clear(), g_obj.leashShip = 0,
        g_obj.dockNoWayTries = 0;
    // A script goal's trip that ended with the step unchanged: its condition is not armed (session 8, 23 Oct 2026, M13
    // "Destroy the Nomad Battleship": within 1 km of the Osiris, then 300 m of Sinclair, then the Osiris again, every
    // ~30 s for minutes - neither fired, the battleship was never fought)
    else if (strcmp(reason, "ended") == 0 && g_obj.action == Action::ScriptGoal && !g_obj.goalTrigger.empty() &&
             g_obj.deadGoals.insert(g_obj.goalTrigger).second)
        events::Push("autopilot objective script-goal %s reached, the step unchanged: not armed - off for this step",
                     g_obj.goalTrigger.c_str());
    g_obj.seq = seq;
    g_obj.scriptMarkN = 0; // (set again below while the step's destroy condition holds)
    // An armed ship leash outranks the decision while we are far out on it (session 8, M10: the supervisor's follow and a
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
                return; // (already on the way back)
            events::Push("autopilot objective decide reason=%s: %.0f m from the leash's ship %u - back toward it",
                         reason, lc.distance, lc.id);
            return LeashBack(lc);
        }
    }
    g_obj.preStageTarget = 0;
    g_obj.flyGoalR = 0;
    const std::vector<space::Contact> all = Contacts();
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
    // attacks - the fight first, then the pod (Destroy Contraband, 26 Oct 2026: a blank entry 0 whose navmarker sits on
    // the pod; the trip there ended "no way" and the tractor ate the pod, again and again)
    if (!ObjectiveThreat(all))
    {
        float dd = 0;
        if (const u32 dl = DestroyMarkedLoot(all, &dd))
        {
            if (g.mode == Mode::Shoot && g.target == dl)
                return ObjectiveStart(Mode::Shoot, dl, Action::Fight, true);
            events::Push("autopilot objective decide reason=%s: marked loot %u to destroy at %.0f m - shoot it", reason,
                         dl, dd);
            return ObjectiveStart(Mode::Shoot, dl, Action::Fight, StartShootObject(dl));
        }
    }
    u32 target = 0;
    for (int i = 0; i < st.refCount && !target; ++i)
        target = space::ObjectiveObject(st.refs[i], all);
    // No target and no navmarker: the mission's newest mark is what the words mean (M06 "Fly to Quintaine" 23142 is a
    // plain text objective; the script marks Quintaine - the ship rested and the nag failed the mission, 22 Oct 2026).
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
    // line (session 8, 23 Oct 2026, M12 core: the power cell spawned 843 m off, a trip "to" it turned the ship about at
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
                return ObjectiveStart(Mode::Travel, lock, Action::Airlock, StartTravel(lock, nullptr, 0, false));
            }
            // (out of range yet: the supervisor decides again once the door is within tractor_exit_range)
            g_obj.tractorExitPending = g_set.tractorExitRun && g_obj.cruiseLock && tr.lockHave;
            events::Push(
                "autopilot objective decide reason=%s text=%u: marked loot %u at %.0f m within the beam's %.0f m - "
                "the trip goes on",
                reason, st.textId, tc.id, tc.distance, reach);
            return ObjectiveStart(Mode::Travel, g_obj.subTarget, g_obj.action, true);
        }
    }
    events::Push("autopilot objective decide reason=%s text=%u verb=%d ref=%u target=%u nav=%d", reason, st.textId,
                 static_cast<int>(verb), st.refCount ? st.refs[0] : 0, target, st.hasNav ? 1 : 0);
    // A lane ride the mission asks for and allows is the way out: it outranks the fight around it (M04 "Escape from
    // Liberty", 19 Oct 2026: "Dock with the Trade Lane" TL147 under attack - the fight went on, the ship fled away
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
        return ObjectiveStart(Mode::Travel, tc.id, Action::Jump, StartTravel(tc.id, nullptr, 0, false));
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
            return ObjectiveStart(Mode::Travel, lock->id, Action::Airlock, StartTravel(lock->id, nullptr, 0, false));
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
    // during the 45 s dialogue, 23 Oct 2026)
    // (a place the step names is no reason to skip it either: "Fly to the Waypoint" at the Nomad City - its generators end
    // the mission; 23 Oct 2026)
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
                    nearest = dist, memcpy(nearPos, p, sizeof(p));
            }
            if (!alive)
                continue;
            // stale_destroy_wait: a goal only the last objective stands in for, none of whose objects the mission marks yet,
            // is a later step's (session 11, every M09 gate: at the Tohoku entry, "Awaiting", the generators 22 km off by
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
                return ObjectiveStart(Mode::Travel, 0, Action::ToMarks, StartTravel(0, q, 0, true));
            }
            return ObjectiveStart(Mode::Combat, 0, Action::ScriptKill, StartOrKeep(Mode::Combat, 0));
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
    // A Dock objective overwritten by a nag (session 8, 23 Oct 2026, M12 Omicron Minor: "Dock with Planet Toledo", then the
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
            return ObjectiveStart(Mode::Travel, g_obj.lastDock, Action::Dock,
                                  StartTravel(g_obj.lastDock, nullptr, 0, false));
        }
    }
    if (Knows(Source::Hidden) && g_set.scriptGoals && verb != ObjVerb::Escort && verb != ObjVerb::Dock &&
        (verb == ObjVerb::Fight || !tc.id || (markTarget && !HostileContact(tc))))
    {
        bool fromLast = false, leashRefused = false;
        std::vector<mission::Goal> goals =
            mission::AdvanceGoals(st.textId, fl::PlayerSystem(), &fromLast, HostObjectiveTitle());
        // A step with its own navmarker and a text no script knows (M10's exit-tunnel waypoints show a blank " ") is not
        // the last known objective: that one's goals are stale (session 8, gate_s8y - "navpoint_12" of the way in to
        // Bruchsal read as fired, the pre-stage sent the ship 24 km back toward Bundschuh base; the leash had hidden it)
        if (fromLast && st.hasNav)
            goals.clear();
        space::Pilot me;
        const mission::Goal* best = nullptr;
        float bestPos[3] = {}, bestDist = 1e30f;
        bool bestWins = false;                // leads to the mission's success
        const mission::Goal* fired = nullptr; // a condition we are inside already: it has fired (or fires now)
        // The mission's leashes (session 8, 23 Oct 2026, M10: under "Awaiting" the ship flew to a script goal 17.6 km off,
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
        for (const mission::Leash& l : mission::Leashes(fl::PlayerSystem()))
        {
            // disarmed by the script: a comm line whose end deactivates it was heard in the last 30 min (M10's
            // DX_M10_0460B_BOTZLER; the script's own state, observed - mission::Leash::offComms)
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
            // arms later and fails at once if we are out then (session 8, M10: the name resolved Von Claussen 6 km+ off, read
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
                // a fight is replaced only by a goal its own objective's chain arms (session 8, 23 Oct 2026: M05 "Destroy
                // the Rheinland Ships" left for a ring 29 km off, armed from the system entry - "You failed to destroy the
                // Rheinland ships"; M13's battleship the same, the Osiris / Sinclair goals in turn)
                const bool wins = sg.leads == "SUCCEED" || sg.leads.find("land") != std::string::npos;
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
            // A goal that arms the next one inside its own sphere is passed on the way to that one (session 8, 23 Oct
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
                if (ok)
                    memcpy(g.tr.flyDir, dir, sizeof(dir)), g.tr.flyRadius = ng.dist, g.tr.flyAhead = ng.dist * 0.7f;
                return ObjectiveStart(Mode::Travel, 0, Action::ScriptGoal, ok);
            }
            return ObjectiveStart(Mode::Travel, 0, Action::ScriptGoal, StartTravel(0, q, 0, true));
        }
        // A fired distance condition whose chain arms the next one (session 7, 23 Oct 2026, M12 Omicron Minor: "within 8 km
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
                return ObjectiveStart(Mode::Travel, 0, Action::ScriptGoal, StartTravel(0, q, 0, true));
            }
        // The step's condition has fired and the script still talks before the next objective (a placeholder stands in:
        // fromLast) - be where the next step's targets are (session 7, 23 Oct 2026, the user watching M12: the ship fought 5
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
                    if (ok)
                        memcpy(g.tr.flyDir, dir, sizeof(dir)), g.tr.flyRadius = ng.dist, g.tr.flyAhead = ng.dist * 0.7f;
                    return ObjectiveStart(Mode::Travel, 0, Action::PreStage, ok);
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
                    return ObjectiveStart(Mode::Travel, 0, Action::PreStage, StartTravel(0, q, 0, true));
                return ObjectiveStart(Mode::Hold, 0, Action::PreStage, Start(Mode::Hold, 0, q));
            }
    }
    // A travel objective (fly to / dock with a place or a navmarker - not a fight, an escort, a defence or a ship to
    // follow) outranks the fight around it: the story sends the player elsewhere (user, 20 Oct 2026: "the autopilot
    // should prioritize breaking off from a fight if a story objective is telling the player to go elsewhere"; M05
    // Sprague: "Fly to the Waypoint Coordinates" while endless wings spawn - it fought 90 s first). Once there
    // (arrived), a threat is fought as before; the mission says "Destroy ..." when a fight is required.
    // (a Dock verb at a solar is a trip even when it still reads hostile: M09 Tekagi's base turns friendly only when the
    // player starts the dock maneuver - it was fought for 4 minutes, 22 Oct 2026)
    // ...and any non-fight step with a navmarker that has gone 2 min: the trip outranks the fights from then on (M13 22 Oct
    // 2026: "Fly to the Waypoint" on the hostile Nomad City - the endless waves pulled the ship into fight after fight for
    // 12 min; the speedrun rule: fight only what the script needs)
    const bool longStep = st.hasNav && verb != ObjVerb::Fight && verb != ObjVerb::Escort && g_obj.stepAt &&
                          GetTickCount64() - g_obj.stepAt > 120000;
    // ...and a hostile place with a navmarker from the start (user, 22 Oct 2026, M13 from the city: "time wastes"; the 2020
    // playthrough flies straight to the Nomad City through the fighters, 20:42-21:15 - ours fought 8 of them for 2 min
    // first: a "Fly to" never asks for a fight, a "Destroy ..." does)
    const bool travelObjective =
        verb != ObjVerb::Fight && verb != ObjVerb::Escort && !g_obj.arrived &&
        ((tc.id && !tc.ship && (!HostileContact(tc) || verb == ObjVerb::Dock || st.hasNav || longStep)) ||
         (!tc.id && st.hasNav));
    g_obj.travelObjective = travelObjective;
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
        if (nm && nm->distance > 5000 && space::ReadPilot(me))
        {
            float p[3];
            for (int i = 0; i < 3; ++i)
                p[i] = nm->pos[i] + (me.pos[i] - nm->pos[i]) * (1500.0f / nm->distance);
            g_obj.travelObjective = true;
            events::Push("autopilot objective marked target %u is %.0f m off: there first", nm->id, nm->distance);
            return ObjectiveStart(Mode::Travel, 0, Action::ToMarks, StartTravel(0, p, 0, true));
        }
    }
    // The survival intent: a Survive objective, or the mission's own title (host API v13: "ESCAPE FROM LIBERTY" - the
    // objective "Defend Yourself!" can pass unseen, 20 Oct 2026). Kept for the whole mission (the title), across the
    // objective mode's restarts (a docked scene and relaunch restarted it at Benford and it fought the cruisers).
    const u32 title = HostObjectiveTitle();
    if (title != g_survivalTitle)
    {
        g_survivalTitle = title;
        g_survivalMission = false;
        std::string t = title ? events::IdsText(title) : std::string();
        for (char& ch : t)
            ch = static_cast<char>(tolower(static_cast<unsigned char>(ch)));
        if (t.find("escape") != std::string::npos || t.find("flee") != std::string::npos ||
            t.find("survive") != std::string::npos)
            g_survivalMission = true;
    }
    if (verb == ObjVerb::Survive)
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
                return ObjectiveStart(Mode::Combat, ph, Action::PreAggro, StartOrKeep(Mode::Combat, ph));
        }
    // Survival with something attacking us and no named target to deal with: stay with the friendly ship the mission
    // brought (its nearest marked / mission ship - Juni) and fight only what comes at us there; none: fight only the
    // close attackers (the combat's reach is kSurvivalReach under this stance).
    if (g_obj.survival && !laneEscape && !travelObjective && verb != ObjVerb::Fight && !(tc.id && HostileContact(tc)) &&
        ObjectiveThreat(all))
    {
        if (const u32 lead = SurvivalLead(all))
            return ObjectiveStart(Mode::Escort, lead, Action::Survive, StartOrKeep(Mode::Escort, lead));
        return ObjectiveStart(Mode::Combat, 0, Action::Survive, StartOrKeep(Mode::Combat, 0));
    }
    // (a "destroy" step with nothing in reach rests: the threat check starts the fight once its targets come)
    // (survival: a hostile objective target is fought only when the objective says so - M04 step 1's "Dock with the
    // Trade Lane" points at TL146, a ring of the now hostile Liberty: the fight around it killed the ship in 7 s)
    if (!laneEscape && !travelObjective &&
        (ObjectiveThreat(all) || (tc.id && HostileContact(tc) && (tc.ship || tc.maxHit < 1e30f) &&
                                  (!g_obj.survival || verb == ObjVerb::Fight))))
    {
        if (verb == ObjVerb::Escort && tc.id && tc.ship && !HostileContact(tc))
            return ObjectiveStart(Mode::Escort, tc.id, Action::Escort, StartOrKeep(Mode::Escort, tc.id));
        // A placeholder step's marked hostiles while an escort is active are the escort's threats: the escort keeps
        // fighting them carried along with its ship, never a Combat chase away from it (set s13a, 24 Oct 2026: the
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
                return ObjectiveStart(Mode::Escort, lead, Action::Fight, StartOrKeep(Mode::Escort, lead));
        }
        // Survival: the objective's own hostile ship is the chosen target ("Destroy the Battleship Unity": the ranking no
        // longer lifts marked big ships under the stance - they are scenery otherwise, 21 Oct 2026)
        if (g_obj.survival && verb == ObjVerb::Fight && tc.id && tc.ship && HostileContact(tc))
            return ObjectiveStart(Mode::Combat, tc.id, Action::Fight, StartOrKeep(Mode::Combat, tc.id));
        return ObjectiveStart(Mode::Combat, 0, Action::Fight, StartOrKeep(Mode::Combat, 0));
    }
    // "Escort the Transport Convoy to the First Buoy": the target is where to (a buoy), the escorted ship is the friendly
    // ship the mission marks (M03: more than 1200 m from transport1 fails the mission - the travel flew ahead to the
    // buoy, 19 Oct 2026). The nearest friendly marked ship; formation as for an escort target.
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
                return ObjectiveStart(Mode::Formation, best->id, Action::Formation,
                                      StartOrKeep(Mode::Formation, best->id));
            return ObjectiveStart(g_obj.sub, best->id,
                                  g_obj.sub == Mode::Formation ? Action::Formation : Action::Follow,
                                  StartOrKeep(g_obj.sub, best->id));
        }
    }
    if (tc.id && tc.ship)
    {
        if (verb == ObjVerb::Escort && g_obj.sub != Mode::Formation)
            return ObjectiveStart(Mode::Formation, tc.id, Action::Formation, StartOrKeep(Mode::Formation, tc.id));
        return ObjectiveStart(Mode::Follow, tc.id, Action::Follow, StartOrKeep(Mode::Follow, tc.id));
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
                return ObjectiveStart(Mode::Travel, tc.id, Action::LaneWait, StartTravel(tc.id, nullptr, 0, true));
            return ObjectiveStart(Mode::Hold, 0, Action::LaneWait, StartOrKeep(Mode::Hold, 0));
        }
        g_obj.laneRing = 0;
        if (!toward)
            toward = laneNext ? laneNext : lanePrev;
        if (tc.distance > 3000 && !g_obj.arrived) // (action "lane": the threat check leaves the approach alone)
            return ObjectiveStart(Mode::Travel, tc.id, Action::Lane, StartTravel(tc.id, nullptr, 0, true));
        const bool ok = Start(Mode::Dock, tc.id, nullptr);
        if (ok)
            g.dock.laneTowardPlan = toward, g.dock.fastForced = true;
        events::Push("autopilot objective lane=%u toward=%u (ride)", tc.id, toward);
        return ObjectiveStart(Mode::Dock, tc.id, Action::Lane, ok);
    }
    // Survival never rests in place (M04 20 Oct 2026: "Awaiting Mission Objective" -> hold beside the Unity battle,
    // dead in 6 s): with nothing to fly to, stay with the lead the mission brought (Juni) - she leads the way out.
    const u32 lead = g_obj.survival && !(tc.id && !g_obj.arrived) && !st.hasNav ? SurvivalLead(all) : 0;
    // ... and beside her, rest the way a player waits: in place, the nose on the scene (HoldLook -> MissionSubject: a
    // place before a wingman - user, 20 Oct 2026: at Mactan look at Mactan, not at Juni); the supervisor rejoins her
    // the moment she moves off (survive-hold)
    const auto stayWithLead = [&]()
    {
        const space::Contact lc = FindContact(lead);
        if (lc.id && lc.distance < g_set.followDistance)
            return ObjectiveStart(Mode::Hold, 0, Action::SurviveHold, StartOrKeep(Mode::Hold, 0));
        return ObjectiveStart(Mode::Escort, lead, Action::SurviveStay, StartOrKeep(Mode::Escort, lead));
    };
    // A travel the mission flies with a leader it named (its formation order): stay in that formation - the trip's
    // trigger may be the leader's arrival (M09 Ozu), and the leader is the mission's pace
    if (travelObjective && g_obj.missionLeader)
    {
        const space::Contact lc = FindContact(g_obj.missionLeader);
        if (lc.id && lc.health != 0 && FriendlyContact(lc))
            return ObjectiveStart(Mode::Formation, lc.id, Action::MissionForm, StartOrKeep(Mode::Formation, lc.id));
        g_obj.missionLeader = 0;
    }
    if (verb == ObjVerb::None || verb == ObjVerb::Survive)
        if (const float r = PassRingRadius(tc))
        {
            // the travel already flies through it (its look-ahead predicted this ring): keep it
            if (g.mode == Mode::Travel && g.tr.passRing == tc.id)
                return ObjectiveStart(Mode::Travel, tc.id, Action::Pass, true);
            return ObjectiveStart(Mode::Travel, tc.id, Action::Pass, StartPass(tc.id, r));
        }
    // A route objective (several steps: M07 "Fly to Planet Kyushu" = 5 entries, lane rings then the planet; the host
    // hands over entry 0 only): the game's waypoint list carries the whole route - when the target is one of its entries
    // but not the last, fly the list to its end (lanes and all). Was: arrived beside the first ring, looped there, and the
    // nag toward the planet failed the mission (22 Oct 2026).
    {
        // host v15: the route's entries themselves - travel to the last one (the travel plans the lanes on the way)
        ObjectiveStep steps[8];
        const int ns = HostObjectiveSteps(steps, 8);
        // the mission's own route first: step 0 a lane ring -> ride it toward the neighbour on the way to step 1 (the
        // objective advances as the ride passes its rings; a best path of our own took another lane and the nag toward
        // Kyushu failed M07, 22 Oct 2026)
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
                    return ObjectiveStart(Mode::Travel, tc.id, Action::Lane, StartTravel(tc.id, nullptr, 0, true));
                const bool ok = Start(Mode::Dock, tc.id, nullptr);
                if (ok)
                    g.dock.laneTowardPlan = toward, g.dock.fastForced = true;
                return ObjectiveStart(Mode::Dock, tc.id, Action::Lane, ok);
            }
        }
        // (the route-lane ride above goes on from beside its ring; everything below rests once its trip is done)
        if (g_obj.arrived) // this step's travel is done: rest there (scenic hold)
            return lead ? stayWithLead() : ObjectiveStart(Mode::Hold, 0, Action::Rest, StartOrKeep(Mode::Hold, 0));
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
                return ObjectiveStart(Mode::Travel, lastObj, Action::Route, ok);
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
            State::Travel t;
            t.noDock = verb != ObjVerb::Dock;
            t.phase = State::Travel::Plan;
            const bool ok = Start(Mode::Travel, 0, nullptr);
            if (ok)
                g.tr = t;
            events::Push("autopilot objective route: the mission's waypoint list (%u entries, target is entry %u)",
                         static_cast<u32>(wl.size()), static_cast<u32>(i));
            return ObjectiveStart(Mode::Travel, tc.id, Action::Route, ok);
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
        return ObjectiveStart(Mode::Travel, dest, dock ? Action::Dock : Action::Arrive,
                              StartTravel(dest, nullptr, 0, !dock));
    }
    // A navmarker on a way through - an airlock, a gate, a hole (M12 "Exit the Structure": the marker sits on the
    // core's exit dock St02c_core_exit; arriving beside it stalled 517 m off and the ship rested, 22 Oct 2026): go
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
            // the ship to the exit, crawling through the core's radiation, 22 Oct 2026)
            if (Len3(d) < 200 && (!way || c.distance < way->distance))
                way = &c;
        }
        if (way && !HostileContact(*way))
        {
            events::Push("autopilot objective navmarker on a dockable object %u: through it", way->id);
            return ObjectiveStart(Mode::Travel, way->id, Action::Dock, StartTravel(way->id, nullptr, 0, false));
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
            return ObjectiveStart(Mode::Travel, 0, Action::Via, StartTravel(0, via, p.system, true));
        }
        // Fly-through (session 6, 23 Oct 2026; the user's M12 core play-by-play: "flies ahead to the waypoint" without
        // slowing - the step ends inside the script's radius, run 1 never brakes): the script's own condition for this
        // step is the player within R of a point that holds the marker -> aim beyond the marker along the approach.
        // A marker near the sphere's edge (session 7, 23 Oct 2026, M12 "Fly to the Waypoint" above the airlock: marker y
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
        if (started && flyWhy)
        {
            memcpy(g.tr.flyDir, flyDir, sizeof(flyDir));
            g.tr.flyRadius = flyR;
            g.tr.flyAhead = flyAhead;
        }
        return ObjectiveStart(Mode::Travel, 0, Action::Navmarker, started);
    }
    if (lead)
        return stayWithLead();
    ObjectiveStart(Mode::Hold, 0, Action::Rest, StartOrKeep(Mode::Hold, 0));
}

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
    const ULONGLONG now = GetTickCount64();
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
                g_obj.missionFormSince = GetTickCount64();
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
        return;
    }
    g_obj.gameDock = false;
    const ULONGLONG now = GetTickCount64();
    if (now < g_obj.nextLook)
        return;
    g_obj.nextLook = now + 500;
    // riding a lane / jumping: the game flies; nothing to decide (a rest started now would break the ride)
    space::Pilot pilot;
    if (space::ReadPilot(pilot) && (pilot.lane || pilot.jumping))
        return;
    // An armed ship leash binds every trip (session 8, M10: the script goals kept to Von Claussen's 6 km, the route's
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
        // formation with Epsilon held the ship 15 min at "Fly to the Waypoint", 22 Oct 2026)
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
        // A trip to a script condition's sphere that ended outside it is not there (session 7, 23 Oct 2026, M12: generator 3
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
        // Fighters are fought, not fled (user, 21 Oct 2026, M04 Mactan: "the ship took forever to start engaging" - the
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
