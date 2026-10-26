// Advanced Autopilot - core: modes, settings and personality, the state, start / stop. Part of autopilot.cpp until 21 Oct 2026; shared state and declarations in
// autopilot_impl.h, the module API in autopilot.h.
#include "autopilot_impl.h"

namespace autopilot
{
const char* ModeName(Mode m)
{
    switch (m)
    {
    case Mode::Off: return "off";
    case Mode::Goto: return "goto";
    case Mode::Dock: return "dock";
    case Mode::Follow: return "follow";
    case Mode::Formation: return "formation";
    case Mode::Face: return "face";
    case Mode::Escort: return "escort";
    case Mode::Combat: return "combat";
    case Mode::Travel: return "travel";
    case Mode::Hold: return "hold";
    case Mode::Shoot: return "shoot";
    }
    return "?";
}

Mode ModeFromName(const std::string& s)
{
    for (Mode m : {Mode::Off, Mode::Goto, Mode::Dock, Mode::Follow, Mode::Formation, Mode::Face, Mode::Escort,
                   Mode::Combat, Mode::Travel, Mode::Hold, Mode::Shoot})
        if (_stricmp(s.c_str(), ModeName(m)) == 0)
            return m;
    return Mode::Off;
}

const char* ActionName(Action a)
{
    switch (a)
    {
    case Action::None: return "-";
    case Action::Leash: return "leash";
    case Action::Airlock: return "airlock";
    case Action::Jump: return "jump";
    case Action::ToMarks: return "to-marks";
    case Action::ScriptKill: return "script-kill";
    case Action::Dock: return "dock";
    case Action::ScriptGoal: return "script-goal";
    case Action::PreStage: return "pre-stage";
    case Action::PreAggro: return "pre-aggro";
    case Action::Survive: return "survive";
    case Action::Escort: return "escort";
    case Action::Fight: return "fight";
    case Action::Formation: return "formation";
    case Action::Follow: return "follow";
    case Action::LaneWait: return "lane-wait";
    case Action::Lane: return "lane";
    case Action::SurviveHold: return "survive-hold";
    case Action::SurviveStay: return "survive-stay";
    case Action::MissionForm: return "mission-form";
    case Action::Pass: return "pass";
    case Action::Rest: return "rest";
    case Action::Route: return "route";
    case Action::Arrive: return "arrive";
    case Action::Via: return "via";
    case Action::Navmarker: return "navmarker";
    case Action::DockWait: return "dock-wait";
    case Action::LeadStall: return "lead-stall";
    }
    return "?";
}

const char* StopText(StopReason r)
{
    switch (r)
    {
    case StopReason::None: return "";
    case StopReason::Restart: return "restart";
    case StopReason::Command: return "command";
    case StopReason::PlayerInput: return "player input";
    case StopReason::Pad: return "pad";
    case StopReason::PlayerDied: return "player died";
    case StopReason::Docked: return "docked";
    case StopReason::Docking: return "docking";
    case StopReason::LeftSpace: return "left space";
    case StopReason::MissionControls: return "mission controls the ship";
    case StopReason::GameDocks: return "the game docks the ship";
    case StopReason::ManeuverEnded: return "maneuver ended";
    case StopReason::LeaderGone: return "leader gone";
    case StopReason::EscortedGone: return "escorted ship gone";
    case StopReason::TargetDisabled: return "target disabled";
    case StopReason::TargetDestroyed: return "target destroyed";
    case StopReason::NoEnemiesLeft: return "no enemies left";
    case StopReason::Fled: return "fled";
    case StopReason::Yield: return "yield";
    case StopReason::Arrived: return "arrived";
    case StopReason::PodsDone: return "pods done";
    case StopReason::TooManyReplans: return "travel: too many re-plans";
    case StopReason::NoBestPath: return "travel: no best path";
    case StopReason::NoWay: return "travel: the game found no way (locked gate?)";
    case StopReason::NoPlan: return "travel: no plan (no waypoints in this system)";
    case StopReason::Hostile: return "travel: the destination is hostile"; // (the stop's text adds the attitude)
    case StopReason::DockRefused: return "travel: dock refused";
    case StopReason::Other: return "?";
    }
    return "?";
}

StopReason StopReasonOf(const char* text)
{
    for (int i = static_cast<int>(StopReason::Restart); i < static_cast<int>(StopReason::Other); ++i)
        if (i != static_cast<int>(StopReason::Hostile) && !strcmp(text, StopText(static_cast<StopReason>(i))))
            return static_cast<StopReason>(i);
    return StopReason::Other;
}

// ------------------------------------------------------------------ settings (player's choices, FLController.ini)
Settings g_set;
ai::Personality g_p;
bool g_pReady = false;

void BuildPersonality()
{
    std::string err;
    g_pReady = ai::LoadPilot(g_set.pilot, g_p, &err);
    if (!g_pReady)
    {
        Log("autopilot: pilot %s: %s (defaults)", g_set.pilot.c_str(), err.c_str());
        g_p = ai::Current();
        g_pReady = true;
    }
    for (const auto& kv : g_set.overrides)
        if (!ai::SetField(g_p, kv.first, kv.second, &err))
            Log("autopilot: override %s: %s", kv.first.c_str(), err.c_str());
}

float Pf(float v, float lo, float hi, float fallback) { return v > 0 ? Clampf(v, lo, hi) : fallback; }

// ------------------------------------------------------------------ state
const char* TacticName(Tactic t)
{
    switch (t)
    {
    case Tactic::Engage: return "engage";
    case Tactic::Evade: return "evade";
    case Tactic::Break: return "break";
    case Tactic::Flee: return "flee";
    case Tactic::Seek: return "seek";
    case Tactic::Rejoin: return "rejoin";
    case Tactic::Loot: return "loot";
    case Tactic::Extend: return "extend";
    }
    return "?";
}

// The run-in's gate per target ship type (user, 21 Oct 2026: launching "without maneuvering to ensure they hit" is
// what NPCs do): the longest predicted flight a launch may have - 1.5 s to start; a miss tightens it to just under
// the flight that missed (not below 0.6 s), each hit relaxes it 0.05 s back towards 1.5 s. Types that dodge get the
// close, fast launches; types that do not keep the tempo (the fight matrix of 21 Oct 2026: 30 of 30 planned hits).
std::map<u32, float> g_missileGate;
float MissileGate(u32 arch)
{
    const auto it = g_missileGate.find(arch);
    return it == g_missileGate.end() ? 1.5f : it->second;
}

State g;

float Dot3(const float* a, const float* b) { return geom::Dot(a, b); }
float Len3(const float* a) { return geom::Len(a); }
constexpr u32 kFramesToAct = 2; // a submitted behaviour shows in the manager's op within the next frame or two

// The game has run enough frames since our last submit to show its answer (the op it runs, or refused it).
bool SubmitSettled() { return g.frames >= g.submitFrame + kFramesToAct; }
void MarkSubmit() { g.submitFrame = g.frames; }
// Every behaviour handed to the game goes through these (the submit frame is recorded).
int SubmitNow(const ai::Directive& d)
{
    MarkSubmit();
    return ai::Submit(d);
}
void FreeFlightNow()
{
    MarkSubmit();
    ai::FreeFlight();
}

bool ObjectiveOn();
// The objective mode counts as on between the modes it starts (its supervisor runs from Update).
bool Active() { return g.mode != Mode::Off || ObjectiveOn(); }
Mode Current() { return g.mode; }

std::string CombatStats()
{
    char buf[640];
    sprintf_s(
        buf,
        "engaged=%.1f on-target=%.1f firing=%.1f avg-dist=%.0f hit-time=%.1f evade-time=%.1f evade-nohit=%.1f "
        "evade-on-target=%.1f lost=%.2f dealt=%.0f dps=%.1f extends=%d extend-time=%.1f incoming-missile-hits=%d "
        "incoming-missile-misses=%d gun-shots=%d gun-duty=%.2f gun-accuracy=%.2f own-missile-hits=%d "
        "own-missile-misses=%d turns=%d turn-time=%.1f duel=%.1f they-on=%.2f we-on=%.2f both-on=%.2f turn-ratio=%.2f",
        g.stats.engaged, g.stats.onTarget, g.stats.firing,
        g.stats.engaged > 0 ? g.stats.distTime / g.stats.engaged : 0.0f, g.stats.hitTime, g.stats.evadeTime,
        g.stats.evadeNoHit, g.stats.evadeOnTarget, g.combat.lost, g.combat.dealt,
        g.stats.firing > 0 ? g.combat.dealt / g.stats.firing : 0.0f, g.stats.extends, g.stats.extendTime,
        g.missiles.hits, g.missiles.misses, g.stats.gunShots,
        g.stats.gunMaxShots > 0 ? g.stats.gunShots / g.stats.gunMaxShots : 0.0f,
        g.stats.gunShotDamage > 0 ? std::max(0.0f, g.combat.dealt - g.missiles.ownMissileDealt) / g.stats.gunShotDamage
                                  : 0.0f,
        g.missiles.ownMissileHits, g.missiles.ownMissileMisses, g.stats.turns, g.stats.turnTime, g.combat.duelT,
        g.combat.duelT > 0 ? g.combat.duelTheyOn / g.combat.duelT : 0.0f,
        g.combat.duelT > 0 ? g.combat.duelWeOn / g.combat.duelT : 0.0f,
        g.combat.duelT > 0 ? g.combat.duelBoth / g.combat.duelT : 0.0f, g.combat.duelTurnRatio);
    return buf;
}

ObjectiveRun g_obj;

bool ObjectiveOn() { return g_obj.on; }
void DuelLog();

// The mission whose intent was read (its title id) and whether it is a survival mission: outlives the objective run
// (ap start objective again after a docked scene keeps the stance for the same mission).
u32 g_survivalTitle = 0;
bool g_survivalMission = false;

// Hands back. `cancel`: also end the running behaviour (free flight); false when the game ended it itself or a
// game sequence (docking, a lane ride, a jump) must go on.
void StopImpl(StopReason why, bool cancel, const char* text)
{
    if (!text)
        text = StopText(why);
    if (g.mode != Mode::Off)
    {
        strncpy_s(g_obj.lastStop, text, _TRUNCATE); // (the objective mode reads why its sub-mode ended)
        g_obj.lastStopReason = why;
    }
    if (why == StopReason::Command || why == StopReason::PlayerInput || why == StopReason::Pad ||
        why == StopReason::PlayerDied)
        g_obj.on = false;
    if (g.mode == Mode::Off)
        return;
    events::Push("autopilot stop mode=%s why=%s kills=%u time=%.1f %s", ModeName(g.mode), text, g.combat.kills, g.time,
                 CombatStats().c_str());
    DuelLog();
    Log("autopilot: stop (%s)", text);
    // The player's own waypoints come back after our route (unless the mission has set new ones meanwhile).
    if (g.mode == Mode::Travel && g.tr.plotted)
        space::RestoreWaypoints();
    const Mode mode = g.mode;
    g = State();
    ai::RestoreShipPersonality();
    if (cancel)
    {
        ai::FreeFlight();
        // Free flight keeps a running cruise and the ship cruises straight on (a harness stop of a travel in M01a /
        // M02, 18 Oct 2026: out of the mission's area). End it - unless the player takes over (their call), a restart
        // (the next mode decides) or a flight away from a fight (fled: keep going).
        // ...nor a trip that arrived while the objective mode runs: its next step decides within a frame (session 6, M12
        // lair: each "arrived" ended the cruise and the next trip charged 5-7 s at 80 m/s again; run 3 keeps the cruise
        // across "Fly to the Core" / "Tractor" / "Exit")
        const bool keep = why == StopReason::Restart || why == StopReason::PlayerInput || why == StopReason::Pad ||
                          why == StopReason::Fled || (g_obj.on && why == StopReason::Arrived);
        flight::Model fm;
        if (!keep && flight::ReadModel(fm) && fm.cruiseEngaged)
        {
            const int cmd = fl::USER_CRUISE;
            const u32 flags = fl::CommandFlags(cmd);
            if (flags & fl::CMDF_KEYDOWN)
                fl::FireCommand(cmd, true);
            if (flags & fl::CMDF_KEYUP)
                fl::FireCommand(cmd, false);
        }
        // A fight that ended (nothing left to fight) leaves the ship at rest, not flying on at the fight's throttle
        // (user, 19 Oct 2026: after saving Beta 4 in M01b the ship flew on aimlessly).
        const bool fight = mode == Mode::Combat || mode == Mode::Escort;
        if (why == StopReason::NoEnemiesLeft || why == StopReason::TargetDisabled ||
            why == StopReason::TargetDestroyed || (fight && why == StopReason::Command))
            fl::SetThrottle(0.0f);
    }
}

static ULONGLONG g_ownDockAt = 0; // when the autopilot's own dock went through (Stop "docking")
void Stop(StopReason why, const char* text)
{
    const bool gameSequence = why == StopReason::Docked || why == StopReason::Docking || why == StopReason::LeftSpace;
    if (why == StopReason::Docking && g.mode != Mode::Off)
        g_ownDockAt = GetTickCount64();
    // (a refused dock is no dock the step still wants: M04's trade lane; was strstr(why, "refused"), which only
    // "travel: dock refused" contains)
    if (gameSequence || why == StopReason::DockRefused)
        g_obj.lastDock = 0;
    if (gameSequence || why == StopReason::PlayerDied)
        g_obj.cruiseLock = false;
    StopImpl(why, !gameSequence, text);
}

// The module API (the controller: "player input", "pad", "docked", "left space"): its text as the reason.
void Stop(const char* why) { Stop(StopReasonOf(why), why); }

bool OwnDock()
{
    return g_set.skipDockScene && (ObjectiveOn() || (g_ownDockAt && GetTickCount64() - g_ownDockAt < 5000));
}

// pre_aggro's ships (SetPreHostile; was space::SetPreHostile, applied inside space::Contacts until the core audit of 24
// Oct 2026, audit_core 1.1 b: core reports what the game says, the policy is the autopilot's).
static std::vector<u32> g_preShips;
static float g_preRange = 0;

void SetPreHostile(const std::vector<u32>& ships, float range)
{
    g_preShips = ships;
    g_preRange = range;
}

std::vector<space::Contact> Contacts()
{
    // space::Contacts() stays memoised per frame (a copy each call); the overlay goes on this copy, so a new list or range
    // takes effect at the next call with no memo invalidation
    std::vector<space::Contact> all = space::Contacts();
    if (g_preShips.empty())
        return all;
    for (space::Contact& c : all)
        if (c.attitudeAsked && c.distance <= g_preRange && c.attitude > -0.6f && c.attitude < 0.6f &&
            std::find(g_preShips.begin(), g_preShips.end(), c.id) != g_preShips.end())
            c.attitude = -0.65f, c.preHostile = true;
    return all;
}

space::Contact FindContact(u32 id)
{
    for (const space::Contact& c : Contacts())
        if (c.id == id)
            return c;
    return {};
}

// The objects the mission points at as space ids, newest marks first: marks are space ids; an objective's 'R' value is a
// reputation instance (space::ObjectiveObject: M03 Walker = 8; reps overlap the small ship ids). An older host (v10)
// gives one merged list: taken as it is.
int MissionObjectIds(u32* out, int max, const std::vector<space::Contact>& all)
{
    int n = HostMissionMarks(out, max);
    if (n < 0)
        return HostMissionObjects(out, max);
    u32 refs[16];
    const int r = HostObjectiveRefs(refs, 16);
    for (int i = 0; i < r && n < max; ++i)
        if (const u32 id = space::ObjectiveObject(refs[i], all))
            if (std::find(out, out + n, id) == out + n)
                out[n++] = id;
    // the script's destroy condition for this step (ObjectiveDecide): its objects as marks
    for (int i = 0; g_obj.on && i < g_obj.scriptMarkN && n < max; ++i)
        if (std::find(out, out + n, g_obj.scriptMarks[i]) == out + n)
            out[n++] = g_obj.scriptMarks[i];
    return n;
}

void ResetSituation();

bool Start(Mode m, u32 target, const float* pos)
{
    if (ai::MissionHolds())
    {
        events::Push("autopilot refused: the mission controls the ship");
        return false;
    }
    Stop(StopReason::Restart);
    if (!g_pReady)
        BuildPersonality();
    // The ship's own behaviours (the game's Evade, Trail breaks) fly by the chosen pilot while the autopilot runs - but
    // never launch consumables on their own timer: the autopilot decides every missile and mine (traced 18 Oct 2026:
    // rounds left 1400-3000 frames after our last tap - the game's behaviours firing on missile_launch_interval_time,
    // the "second missile" of M02's jailbreak fight).
    ai::Personality ship = g_p;
    ship.Missile.missile_launch_range = 0;
    ship.Missile.missile_launch_allow_out_of_range = false;
    ship.Missile.missile_launch_interval_time = 1e6f;
    ship.Missile.anti_cruise_missile_max_distance = 0;
    ship.Mine.mine_launch_range = 0;
    ship.Mine.mine_launch_interval = 1e6f;
    if (!ai::ApplyShipPersonality(ship))
        Log("autopilot: personality not applied to the ship");
    g = State();
    ResetSituation(); // nothing read from the last run
    g.mode = m;
    g.target = target;
    if (pos)
    {
        g.hasPos = true;
        memcpy(g.pos, pos, sizeof(g.pos));
    }
    if (m == Mode::Escort)
    {
        g.escorted = target;
        g.tactic = Tactic::Rejoin;
    }
    else if (m == Mode::Combat)
    {
        g.tactic = target ? Tactic::Engage : Tactic::Seek;
        g.combat.chosen = target != 0;
    }
    events::Push("autopilot start mode=%s target=%u pilot=%s", ModeName(m), target, g_set.pilot.c_str());
    Log("autopilot: start %s target %u", ModeName(m), target);
    return true;
}

void StartForTarget()
{
    const u32 id = fl::TargetId();
    const aim::Target t = aim::Current();
    if (!id)
    {
        Start(Mode::Combat, 0, nullptr); // no target: fight the closest enemy
        return;
    }
    if (t.valid && t.hostile)
    {
        Start(Mode::Combat, id, nullptr);
        return;
    }
    const space::Contact c = FindContact(id);
    if (c.ship)
        Start(Mode::Escort, id, nullptr);
    else if (c.dock || (c.type & (space::kTradelaneRing | space::kJumpGate | space::kJumpHole)))
        Start(Mode::Dock, id, nullptr);
    else
        Start(Mode::Goto, id, nullptr);
}

bool StartObjective()
{
    if (!HostAtLeast(11))
        return false;
    g_obj = ObjectiveRun();
    g_obj.on = true;
    g_obj.seq = ~0u; // decide at once
    ObjectiveSupervise();
    return true;
}

bool StartWaypoints()
{
    if (space::Waypoints().empty())
        return false;
    State::Travel t;
    t.phase = State::Travel::Plan; // the list as it is (the travel plans its lanes / gates across systems)
    if (!Start(Mode::Travel, 0, nullptr))
        return false;
    g.tr = t;
    return true;
}

const char* StartSmart()
{
    const char* chose = "target";
    if (g_set.startSmart)
    {
        // an objective now: the game's own step (its text id; 0 = none - free roam, between jobs)
        ObjectiveStep st = {};
        const bool objective = HostObjectiveNow(&st) != 0 && st.textId != 0;
        const size_t waypoints = space::Waypoints().size();
        if (objective && StartObjective())
            chose = "objective";
        else if (waypoints && StartWaypoints())
            chose = "waypoints";
        events::Push("autopilot smart-start objective=%d waypoints=%u -> %s", objective ? 1 : 0,
                     static_cast<unsigned>(waypoints), chose);
    }
    if (strcmp(chose, "target") == 0)
        StartForTarget();
    return Active() ? chose : "refused";
}

} // namespace autopilot
