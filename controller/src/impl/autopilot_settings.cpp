// Advanced Autopilot - settings io: FLController.ini [Autopilot], the `ap` debug command (status, set, presets, probes).
// Part of autopilot.cpp until 21 Oct 2026; shared state and declarations in autopilot_impl.h, the module API in
// autopilot.h. Since 24 Oct 2026 every setting is declared once in kDefs (core\settings.h): the name, type, field,
// range and help drive `ap set`, the ini load / save, `ap settings` and the generated docs. One key stays hand-written
// because it sets several fields: `consumables` (needed / never); `knowledge` is an Int row since the UI build (25 Oct
// 2026) that still reads its old words all / exposed.
#include "autopilot_impl.h"
#include "settings.h"

namespace autopilot
{
// ------------------------------------------------------------------ the settings registry
using settings::Bool;
using settings::Float;
using settings::Hex;
using settings::Int;
using settings::String;
using settings::Vec3;
using settings::kNoMax;

// Order = the order in FLController.ini and in `ap settings`. The help is one line for the player (the ini file, the
// UI); the why and the measurements stay with the field in autopilot_impl.h.
static const settings::Def kDefs[] = {
    {"pilot", String, &g_set.pilot, 0, 0,
     "the game's [Pilot] whose personality the autopilot flies by (DATA\\MISSIONS\\pilots_*.ini)"},
    {"knowledge", Int, &g_set.knowledge, 0, 1,
     "what the autopilot may know: 0 only what the player was exposed to in this game (bases, routes, enemies), 1 "
     "everything the game data tells (the speedrun preset); the words exposed / all are accepted too"},
    {"engage", Int, &g_set.engage, 0, 0,
     "combat: 0 = only the chosen target, 1 = then the closest enemy until none is left"},
    {"auto_fire", Bool, &g_set.autoFire, 0, 0, "fire the guns (gimbal aim)"},
    {"missiles", Bool, &g_set.missiles, 0, 0, "launch missiles when the shot is worth it"},
    {"countermeasures", Int, &g_set.countermeasures, 0, 0,
     "0 never; 1 cruise disruptors outside a fight and missiles whose damage matters; 2 every missile homing on us"},
    {"cm_distance", Float, &g_set.cmDistance, 0, 0,
     "drop countermeasures once a missile homing on us is this close (m; the CM's range is 1000)"},
    {"ref_line", Bool, &g_set.refLine, 0, 0,
     "follow a reference flight's line through a structure where one is known (speedrun knowledge)"},
    {"stale_destroy_wait", Bool, &g_set.staleDestroyWait, 0, 0,
     "a destroy goal of a later step whose objects the mission does not mark yet is not flown to"},
    {"cm_shield_fraction", Float, &g_set.cmShieldFraction, 0, 0,
     "in a fight: counter only a missile that would cost this share of the shield"},
    {"dodge_window", Float, &g_set.dodgeWindow, 0, 0,
     "no countermeasure possible: dodge (slide + afterburner) this long before impact (s)"},
    {"evade", Bool, &g_set.evade, 0, 0, "dodge and break by the personality; 0 = pure pursuit"},
    {"fire_cone", Float, &g_set.fireCone, 0, 0,
     "fire while the lead cross is within this offset from the screen centre (half-screens)"},
    {"follow_distance", Float, &g_set.followDistance, 0, 0, "follow / escort: the game's follow max distance (m)"},
    {"escort_radius", Float, &g_set.escortRadius, 0, 0,
     "escort: fight hostiles within this distance of the escorted ship (m)"},
    {"hold_radius", Float, &g_set.holdRadius, 0, 0, "hold: the leash - farther from the hold point, fly back (m)"},
    {"flee", Float, &g_set.flee, 0, 0, "leave combat below this hull fraction; -1 = the personality's flee value"},
    {"steer", Int, &g_set.steer, 0, 0, "combat steering: 0 = lead pursuit, 1 = the game's Trail behaviour (NPC style)"},
    {"bursts", Bool, &g_set.bursts, 0, 0, "fire in the personality's bursts (NPC style) instead of whenever on target"},
    {"engage_distance", Float, &g_set.engageDistance, 0, 0, "pursuit close distance (m); 0 = by the target's size"},
    {"engage_radius", Float, &g_set.engageRadius, 0, 0,
     "pick hostiles within this distance (m; escort: of the escorted ship)"},
    {"big_ships", Bool, &g_set.bigShips, 0, 0,
     "also pick gunboats, cruisers, capital ships and transports while fighters are around"},
    {"caution", Int, &g_set.caution, 0, 0,
     "0 never leave a fight early; 1 leave when clearly outmatched below 60 % hull; 2 leave sooner"},
    {"retreat_odds", Float, &g_set.retreatOdds, 0, 0,
     "free roam: leave a fight whose measured race is lost this badly (0.05 = 20:1); 0 = off"},
    {"repair", Int, &g_set.repair, 0, 0,
     "nanobots / batteries: 0 never; 1 only when needed (the player's way); 2 at the personality's thresholds"},
    {"repair_shield_at", Float, &g_set.repairShieldAt, 0, 0,
     "repair 1: never a battery above this shield fraction (declared; the repair rule does not read it yet)"},
    {"repair_hull_at", Float, &g_set.repairHullAt, 0, 0,
     "repair 1: nanobots below this hull fraction while being hit (declared; the repair rule does not read it yet)"},
    {"battery_horizon", Float, &g_set.batteryHorizon, 0, 0,
     "repair 1: a battery when the shield left is below the loss rate x this (s)"},
    {"loot", Bool, &g_set.loot, 0, 0, "collect the loot near the fight once no enemy is left, the most valuable first"},
    {"dodge", Int, &g_set.dodge, 0, 0,
     "how to dodge: 1 slide bursts with the nose on the target, 2 the game's Evade behaviour"},
    {"loot_min_value", Float, &g_set.lootMinValue, 0, 0, "leave loot worth less than this (credits at the base price)"},
    {"evade_scale", Float, &g_set.evadeScale, 0, 0,
     "evasion: this fraction of shield + hull lost in 2 s = full evasion"},
    {"extend", Bool, &g_set.extend, 0, 0,
     "extend under sustained fire (afterburner away, jinking) and resume when the hits stop"},
    {"extend_quiet", Float, &g_set.extendQuiet, 0, 0, "extend: resume after this long without a hit (s)"},
    {"extend_max", Float, &g_set.extendMax, 0, 0, "extend: at most this long (s)"},
    {"extend_jink", Float, &g_set.extendJink, 0, 0, "extend: jink offset from straight away (degrees)"},
    {"extend_patience", Float, &g_set.extendPatience, 0, 0,
     "extend: still hit after this long = futile, fight instead (s)"},
    {"stagger", Bool, &g_set.stagger, 0, 0, "staggered gun fire: the guns started one by one over their refire cycle"},
    {"weave_radius", Float, &g_set.weaveRadius, 0, 0,
     "evasion: corkscrew radius at full evasion, as a fraction of fire_cone"},
    {"slide_time", Float, &g_set.slideTime, 0, 0, "evasion: length of each slide burst (s); 0 = weave only"},
    {"turn_evade", Bool, &g_set.turnEvade, 0, 0,
     "a turn onto a target far off the nose while attacked is itself a dodge (slides, afterburner)"},
    {"blind_spot", Bool, &g_set.blindSpot, 0, 0,
     "a big target with turrets is fought from a spot its guns cannot reach"},
    {"adaptive", Bool, &g_set.adaptive, 0, 0,
     "ship-adaptive duel: close turn / wide turn / slash by agility and the measured exchange; 0 = close turn"},
    {"duel", Int, &g_set.duel, -1, 2, "force a duel option (A/B): 0 close turn, 1 wide turn, 2 slash; -1 adaptive"},
    {"start_smart", Int, &g_set.startSmart, 0, 1,
     "the autopilot key / button: 1 = the mission objective first, else the waypoints, else the target; 0 = the "
     "target alone"},
    {"skip_dock_scene", Bool, &g_set.skipDockScene, 0, 0,
     "the autopilot's own dock skips its docking / landing cutscene"},
    {"fast_dock", Bool, &g_set.fastDock, 0, 0,
     "player-style docks: full speed to the bay's approach point, the request just before it"},
    {"dock_regain", Bool, &g_set.dockRegain, 0, 0,
     "a fast dock whose request is lost to a moving target (crawling 4 s after the grant) hands over to the game's "
     "dock"},
    {"dock_request_distance", Float, &g_set.dockRequestDistance, 0, 0,
     "fast dock: where to request, along the bay's axis from its approach point (m; negative = past it)"},
    {"dock_arrive_speed", Float, &g_set.dockArriveSpeed, 0, 0,
     "fast dock: brake to this speed before the request point (m/s); 0 = no braking"},
    {"ring_request", Float, &g_set.ringRequest, 0, 0,
     "fast dock at a planet ring, straight in: request this far from the approach point (m); 0 = ~1.4 s out"},
    {"airlock_charge_at", Float, &g_set.airlockChargeAt, 0, 0,
     "an airlock run's cruise charge timed so this share of it is done at the transit; 0 = 0.5 s before"},
    {"wait_flies", Bool, &g_set.waitFlies, 0, 0,
     "a trip waiting for the mission's waypoint list flies on meanwhile; 0 = coast"},
    {"rejoin_nearest", Bool, &g_set.rejoinNearest, 0, 0,
     "a fly-through's rejoin of its line aims at the nearest point the turning circle reaches"},
    {"airlock_burn", Bool, &g_set.airlockBurn, 0, 0,
     "an airlock run off its aim slows only as much as the turn left needs; 0 = 120 m/s within 1.2 km when 45 deg+ "
     "off"},
    {"noway_climb", Bool, &g_set.nowayClimb, 0, 0,
     "an arrival with no clear line and no way yet climbs out along a clear line until a way is found"},
    {"hop_cruise_hold", Bool, &g_set.hopCruiseHold, 0, 0,
     "a hop's started cruise charge is kept unless the afterburner wins by more than 1 s"},
    {"tractor_exit_run", Bool, &g_set.tractorExitRun, 0, 0,
     "marked loot tractored in passing under the cruise lock: the trip's own airlock is the way on at once"},
    {"tractor_exit_range", Float, &g_set.tractorExitRange, 0, 0,
     "tractor_exit_run only within this of the airlock door's outer point (m)"},
    {"blast_flank", Bool, &g_set.blastFlank, 0, 0,
     "no explosive round while another solid lies beside the ship within the round's blast reach"},
    {"pre_aggro", Float, &g_set.preAggro, 0, 0,
     "mission ships the script is about to turn hostile count as hostile within this range (m; speedrun knowledge); 0 "
     "= off"},
    {"passage_window", Bool, &g_set.passageWindow, 0, 0,
     "a passage beside a too-narrow gap looks for the opening's centre first; 0 = the 8 directions only"},
    {"hop_reset_burn", Bool, &g_set.hopResetBurn, 0, 0, "a hop's charge reset under fire burns at once for 8 s"},
    {"airlock_fly_through", Bool, &g_set.airlockFlyThrough, 0, 0,
     "airlocks are flown through like a player, no dock request; 0 = request as at a gate"},
    {"wheel_timing", Bool, &g_set.wheelTiming, 0, 0, "a rotating solar part on the way is crossed through a timed gap"},
    {"static_orbit", Bool, &g_set.staticOrbit, 0, 0,
     "a static target may be fought on an orbit round it (the last fallback)"},
    {"static_pass", Bool, &g_set.staticPass, 0, 0,
     "a static target may be fought by attack passes (the hold's fallback)"},
    {"static_hold", Bool, &g_set.staticHold, 0, 0,
     "a static target is fought by hold-and-salvo near its hull (the speedrunners' way)"},
    {"static_hold_gap", Float, &g_set.staticHoldGap, 10, kNoMax,
     "the hold point's distance from the target's real hull (m, at least 10)"},
    {"static_hold_lateral", Float, &g_set.staticHoldLateral, 0, kNoMax,
     "the hold's lateral run speed along the hull (m/s); 0 = slide by static_hold_slide"},
    {"target_dwell", Float, &g_set.targetDwell, 0, kNoMax,
     "a live combat target is kept at least this long before the ranking may switch (s)"},
    {"static_hold_slide", Float, &g_set.staticHoldSlide, 0, kNoMax,
     "the hold slid round the target at this rate (deg/s); 0 = still"},
    {"static_hold_mines", Bool, &g_set.staticHoldMines, 0, 0, "drop mines at the hold every 1.5 s"},
    {"static_hold_mine_gap", Float, &g_set.staticHoldMineGap, 0, kNoMax,
     "mines at the hold only within this hull gap (m)"},
    {"static_hop", Bool, &g_set.staticHop, 0, 0,
     "the way to a static target's hold goes through real geometry, by cruise when faster"},
    {"script_goals", Bool, &g_set.scriptGoals, 0, 0,
     "a step's flying condition read from the mission script is travelled to (objectives are triggers)"},
    {"geometry_guard", Bool, &g_set.geometryGuard, 0, 0,
     "close-quarter flight in every mode: the way ahead tested against real hulls"},
    {"motion_trace", Bool, &g_set.motionTrace, 0, 0,
     "FLMotion.tsv: 10 rows / s of the flight while the autopilot flies"},
    {"blind_shelter", Bool, &g_set.blindShelter, 0, 0,
     "among a target's blind spots prefer one the other hostiles have no clear line to"},
    {"dock_stage", Float, &g_set.dockStage, 0, 0,
     "fast dock: coming from the side or behind, line up this far out on the bay's axis (m)"},
    {"lane_request_distance", Float, &g_set.laneRequestDistance, 0, 0,
     "fast lane entry: request when the stop would end within this of the aim point (m)"},
    {"fast_thrust", Int, &g_set.fastThrust, 0, 0,
     "fast dock / lane run: 0 as planned, 1 burner, 2 cruise (verification runs)"},
    {"lane_toward", Hex, &g_set.laneToward, 0, 0,
     "fast lane entry: the ring the ride should head to (id); 0 = by the ship's side"},
    {"avoid", Bool, &g_set.avoid, 0, 0, "the autopilot's own obstacle avoidance on its runs"},
    {"disable_at", Float, &g_set.disableAt, 0, 0,
     "combat on a chosen target: stop once its hull is at or below this fraction (0 = destroy)"},
    {"follow_offset", Vec3, &g_set.followOffset, 0, 0,
     "follow / escort: offset from the leader as x, y, z (right, up, behind; m)"},
};
static const size_t kDefCount = sizeof(kDefs) / sizeof(kDefs[0]);

// ------------------------------------------------------------------ settings io
// One setting by its FLController.ini [Autopilot] name (also `ap set`). False if unknown.
static bool ApplySetting(const std::string& k, const std::string& v)
{
    // consumables (user, 18 Oct 2026: two modes, offensive and defensive alike): needed - missiles, nanobots,
    // batteries and countermeasures only when the situation calls for them (the economy counts for missiles); never
    if (k == "consumables")
    {
        if (v != "needed" && v != "never")
            return false;
        const bool use = v == "needed";
        g_set.missiles = use;
        g_set.repair = use ? 1 : 0;
        g_set.countermeasures = use ? 1 : 0;
        return true;
    }
    // knowledge: the words of the ini files before 25 Oct 2026 (all = everything, the speedrun preset; exposed = what
    // the player was exposed to - Source in autopilot_impl.h); the row itself is Int 0 / 1 since the UI build
    if (k == "knowledge" && (v == "all" || v == "exposed"))
    {
        g_set.knowledge = v == "all" ? 1 : 0;
        return true;
    }
    const settings::Def* d = settings::Find(kDefs, kDefCount, k);
    return d && settings::Apply(*d, v);
}

const settings::Def* Defs(size_t& count)
{
    count = kDefCount;
    return kDefs;
}

// The settings bus changed a row (core\settings_bus.h `applied`, the options page): the reactions `ap set` has.
void OnSettingApplied(const settings::Def* def)
{
    if (def && strcmp(def->name, "pilot") == 0)
        BuildPersonality();
}

// Every setting with its value and help (`ap settings`; the docs' settings section is generated from it).
std::string SettingsReference()
{
    return settings::Reference(kDefs, kDefCount) +
           "consumables = (needed | never)  ; sets missiles, repair and countermeasures together\n";
}

void Load(const std::string& ini)
{
    char buf[32768] = {};
    if (GetPrivateProfileSectionA("Autopilot", buf, sizeof(buf), ini.c_str()) == 0)
    {
        BuildPersonality();
        return;
    }
    g_set.overrides.clear();
    for (const char* p = buf; *p; p += strlen(p) + 1)
    {
        std::string kv = p;
        const size_t eq = kv.find('=');
        if (eq == std::string::npos)
            continue;
        std::string k = kv.substr(0, eq), v = kv.substr(eq + 1);
        k.erase(k.find_last_not_of(" \t") + 1);
        v.erase(0, v.find_first_not_of(" \t"));
        std::istringstream in(k + " " + v);
        std::string name;
        in >> name;
        if (name.rfind("p.", 0) == 0)
            g_set.overrides[name.substr(2)] = v;
        else
            ApplySetting(name, v);
    }
    BuildPersonality();
}

void Save(const std::string& ini)
{
    settings::Save("Autopilot", ini, kDefs, kDefCount);
    for (const auto& kv : g_set.overrides)
        WritePrivateProfileStringA("Autopilot", ("p." + kv.first).c_str(), kv.second.c_str(), ini.c_str());
}

// The [Autopilot] section with a comment per setting into an open file (the controller's `config save`).
void WriteCommented(FILE* f)
{
    settings::WriteCommented(f, "Autopilot", kDefs, kDefCount);
    fprintf(f, "; p.<field> = a personality field by its ini name (e.g. p.trail_distance = 200)\n");
    for (const auto& kv : g_set.overrides)
        fprintf(f, "p.%s = %s\n", kv.first.c_str(), kv.second.c_str());
    fprintf(f, "\n");
}
static std::string Status()
{
    std::ostringstream o;
    ai::State s;
    ai::ReadState(s);
    o << "autopilot mode=" << ModeName(g.mode) << " tactic=" << (g.mode == Mode::Off ? "-" : TacticName(g.tactic))
      << " objective=" << (g_obj.on ? ActionName(g_obj.action) : "off") << " target=" << g.target
      << " escorted=" << g.escorted << " kills=" << g.combat.kills << " time=" << g.time << " " << CombatStats()
      << " op=" << ai::OpName(s.op) << " hull=" << ai::PlayerHealth() << " pilot=" << g_set.pilot
      << " engage=" << g_set.engage << " auto_fire=" << g_set.autoFire << " missiles=" << g_set.missiles
      << " countermeasures=" << g_set.countermeasures << " evade=" << g_set.evade << " fire_cone=" << g_set.fireCone
      << " follow_distance=" << g_set.followDistance << " escort_radius=" << g_set.escortRadius
      << " hold_radius=" << g_set.holdRadius << " flee=" << g_set.flee << " steer=" << g_set.steer
      << " bursts=" << g_set.bursts << " engage_distance=" << g_set.engageDistance
      << " engage_radius=" << g_set.engageRadius << " big_ships=" << g_set.bigShips << " caution=" << g_set.caution
      << " repair=" << g_set.repair << " loot=" << g_set.loot << " threat=" << g_sit.threat
      << " hostiles=" << g_sit.hostiles << " attackers=" << g_sit.attackers << " evasion=" << g.combat.evasion
      << " pressure=" << g.stats.pressure << " shield=" << g.stats.shield << " lost=" << g.combat.lost
      << " evade_scale=" << g_set.evadeScale << " weave_radius=" << g_set.weaveRadius
      << " slide_time=" << g_set.slideTime << " area_level=" << g_sit.areaLevel << " rank=" << g_sit.rank
      << " overrides=" << g_set.overrides.size() << " knowledge=" << (g_set.knowledge >= 1 ? "all" : "exposed")
      << " static_hold=" << g_set.staticHold;
    return o.str();
}

std::string Command(std::istream& in, const std::string& ini)
{
    std::string sub;
    in >> sub;
    if (sub == "start") // ap start <mode> [id | x y z]
    {
        std::string m;
        in >> m;
        std::vector<std::string> args;
        std::string a;
        while (in >> a)
            args.push_back(a);
        if (m == "objective") // ap start objective: do what the current objective asks, until stopped
        {
            if (!StartObjective())
                return "objective: needs host API v11 (restart the game) | " + Status();
            return Status();
        }
        if (m == "smart") // ap start smart: the plain start as the key / the bar button / View held do it (StartSmart)
            return std::string("smart start -> ") + StartSmart() + " | " + Status();
        if (m ==
            "arm") // ap start arm: the objective mode on from the next frame in space (armed while docked: the launch
        {          // is often under fire - M13's script parks the player among Nomads; no harness round trip)
            if (!HostAtLeast(11))
                return "arm: needs host API v11 (restart the game) | " + Status();
            g_obj = ObjectiveRun();
            g_obj.on = true;
            g_obj.seq = ~0u; // decide at the first look in space
            return Status();
        }
        if (m == "pods") // ap start pods <depot id>: shoot its cargo pods open and tractor the cargo (free net worth)
        {
            if (args.empty() || !StartShootPods(static_cast<u32>(std::stoul(args[0]))))
                return "pods: no depot with cargo pods | " + Status();
            return Status();
        }
        if (m ==
            "pass") // ap start pass <ring id>: fly through it and on along the course its rings make (race practice)
        {
            const space::Contact c =
                args.empty() ? space::Contact{} : FindContact(static_cast<u32>(std::stoul(args[0])));
            const float r = PassRingRadius(c);
            if (!r)
                return "pass: not a ring to fly through | " + Status();
            StartPass(c.id, r);
            return Status();
        }
        const Mode mode = m == "auto" ? Mode::Off : ModeFromName(m);
        if (m == "auto")
        {
            StartForTarget();
            return Status();
        }
        if (mode == Mode::Off)
            return "unknown mode " + m;
        if (mode == Mode::Travel) // ap start travel [object | x y z [system [object]]] [nodock]: none = the list as is
        {
            State::Travel t;
            if (!args.empty() && args.back() == "nodock")
            {
                t.noDock = true;
                args.pop_back();
            }
            if (args.size() >= 3)
            {
                t.hasDest = true;
                for (int i = 0; i < 3; ++i)
                    t.dest[i] = static_cast<float>(atof(args[i].c_str()));
                if (args.size() >= 4)
                    t.destSystem = static_cast<u32>(strtoul(args[3].c_str(), nullptr, 0));
                if (args.size() >= 5) // the object there (a base in another system: docked with on arrival)
                    t.destObject = static_cast<u32>(strtoul(args[4].c_str(), nullptr, 0));
            }
            else if (args.size() == 1)
            {
                t.destObject = static_cast<u32>(strtoul(args[0].c_str(), nullptr, 0));
                t.hasDest = space::ObjectPos(t.destObject, t.dest);
                if (!t.hasDest)
                    return "travel: object " + args[0] + " not in this system (give x y z system) | " + Status();
            }
            t.phase = t.hasDest ? State::Travel::Plot : State::Travel::Plan;
            if (!Start(Mode::Travel, t.destObject, nullptr))
                return "refused | " + Status();
            g.tr = t;
            return Status();
        }
        float pos[3];
        const bool hasPos = args.size() >= 3;
        if (hasPos)
            for (int i = 0; i < 3; ++i)
                pos[i] = static_cast<float>(atof(args[i].c_str()));
        u32 target = !hasPos && !args.empty() ? static_cast<u32>(strtoul(args[0].c_str(), nullptr, 0)) : 0;
        if (!target && !hasPos && mode != Mode::Combat)
            target = fl::TargetId();
        return (Start(mode, target, hasPos ? pos : nullptr) ? "" : "refused | ") + Status();
    }
    if (sub == "stop")
    {
        Stop(StopReason::Command);
        return Status();
    }
    if (sub == "pilot")
    {
        std::string p;
        in >> p;
        g_set.pilot = p;
        BuildPersonality();
        return Status();
    }
    if (sub == "set") // ap set <setting|p.field> <value...>
    {
        std::string k, v;
        in >> k;
        std::getline(in >> std::ws, v);
        std::string err;
        if (k.rfind("p.", 0) == 0)
        {
            ai::Personality probe = g_p;
            if (!ai::SetField(probe, k.substr(2), v, &err))
                return err;
            g_set.overrides[k.substr(2)] = v;
            BuildPersonality();
            return k + " = " + ai::GetField(g_p, k.substr(2));
        }
        if (k == "pilot" || !ApplySetting(k, v))
            return "unknown setting " + k;
        return Status();
    }
    if (sub ==
        "rayraw") // ap rayraw x0 y0 z0 x1 y1 z1: every raw hit of the ray test (object, class, id, crc, distance)
    {
        float a[3] = {}, b[3] = {};
        in >> a[0] >> a[1] >> a[2] >> b[0] >> b[1] >> b[2];
        return space::RayHitsRaw(a, b);
    }
    if (sub == "ray") // ap ray x0 y0 z0 x1 y1 z1: the game's ray test (real hulls) - the first hit's distance, -1 none
    {
        float a[3] = {}, b[3] = {};
        in >> a[0] >> a[1] >> a[2] >> b[0] >> b[1] >> b[2];
        char line[96];
        sprintf_s(line, "ray %.1f", space::RayHitDistance(a, b));
        return line;
    }
    // ap scan x0 y0 z0 x1 y1 z1 half step: a grid of rays parallel to p0 -> p1, offset across the plane perpendicular to
    // it (-half..+half in step), as a map: '.' clear, '0'-'9' the hit at that tenth of the length (session 6, 23 Oct
    // 2026: the M12 core's small hole and its rotating "turbine" - the user: flown dead centre without slowing). Rows =
    // the plane's "up" axis from +half down, columns its "right" axis from -half; then the clear cells' centre.
    if (sub == "scan")
    {
        float a[3] = {}, b[3] = {}, half = 200, step = 20;
        in >> a[0] >> a[1] >> a[2] >> b[0] >> b[1] >> b[2] >> half >> step;
        const float d[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
        const float len = Len3(d);
        if (len < 1 || step < 1 || half / step > 40)
            return "scan: bad arguments (at most 81 x 81 rays)";
        const float f[3] = {d[0] / len, d[1] / len, d[2] / len};
        // the plane's axes: right = f x world-up (world-x when f is vertical), up = right x f
        float r[3] = {-f[2], 0, f[0]};
        if (Len3(r) < 0.1f)
            r[0] = 1, r[1] = 0, r[2] = 0;
        geom::Norm(r);
        float u[3];
        geom::Cross(u, r, f);
        std::string s;
        char line[200];
        sprintf_s(line, "scan len=%.0f half=%.0f step=%.0f right=(%.2f,%.2f,%.2f) up=(%.2f,%.2f,%.2f) t=%llu\n", len,
                  half, step, r[0], r[1], r[2], u[0], u[1], u[2], GetTickCount64());
        s += line;
        double cx = 0, cy = 0;
        int clear = 0;
        for (float y = half; y >= -half - 0.01f; y -= step)
        {
            for (float x = -half; x <= half + 0.01f; x += step)
            {
                float p0[3], p1[3];
                for (int i = 0; i < 3; ++i)
                    p0[i] = a[i] + r[i] * x + u[i] * y, p1[i] = b[i] + r[i] * x + u[i] * y;
                const float h = space::RayHitDistance(p0, p1);
                if (h < 0)
                    s += '.', cx += x, cy += y, ++clear;
                else
                    s += static_cast<char>('0' + std::min(9, static_cast<int>(h / len * 10)));
            }
            s += '\n';
        }
        sprintf_s(line, "clear %d of the rays, their centre right=%.0f up=%.0f", clear, clear ? cx / clear : 0.0,
                  clear ? cy / clear : 0.0);
        return s + line;
    }
    if (sub == "goals") // ap goals: the script's conditions that advance the current step (mission::AdvanceGoals, live)
    {
        ObjectiveStep st = {};
        HostObjectiveNow(&st);
        bool fromLast = false;
        const u32 sys = fl::PlayerSystem(), title = HostObjectiveTitle();
        const std::vector<mission::Goal> goals = mission::AdvanceGoals(st.textId, sys, &fromLast, title);
        char line[256];
        sprintf_s(line, "goals text=%u system=%u title=%u from-last=%d n=%u", st.textId, sys, title, fromLast ? 1 : 0,
                  static_cast<unsigned>(goals.size()));
        std::string s = line;
        for (const mission::Goal& goal : goals)
        {
            float p[3] = {};
            const bool at = goal.kind == 1 ? space::ObjectPos(goal.object, p) : goal.kind == 2;
            sprintf_s(line, "\n  %s kind=%d %s %s dist=%.0f count=%d objects=%u placed=%d -> %s", goal.trigger.c_str(),
                      goal.kind, goal.subject.c_str(), goal.objectName.c_str(), goal.dist, goal.count,
                      static_cast<unsigned>(goal.objects.size()), at ? 1 : 0, goal.leads.c_str());
            s += line;
        }
        return s;
    }
    if (sub == "lof") // ap lof <id>: the line-of-fire safety towards object <id> (guns' and missiles' corridors)
    {
        u32 id = 0;
        in >> id;
        const space::Contact c = FindContact(id);
        if (!c.id)
            return "no object " + std::to_string(id);
        char line[160];
        sprintf_s(line, "lof %u dist=%.0f guns=%s missiles=%s", id, c.distance,
                  LineOfFireBlocked(id, c.pos, 60.0f, std::max(0.0f, 600.0f - c.distance), 90.0f) ? "blocked" : "clear",
                  LineOfFireBlocked(id, c.pos, 200.0f, 400.0f, 400.0f) ? "blocked" : "clear");
        return line;
    }
    if (sub == "preset") // ap preset <name>: a named bundle of settings (user, 18 Oct 2026)
    {
        // standard: the default - fast docks, consumables when needed, the context-sensitive safety that is always on
        // (line of fire, blast radius). no-consumables: the same flying, nothing consumed. An "advanced" preset waits
        // for maneuvers worth the name (debate with the user). The product presets (user, 22 Oct 2026; AUTOPILOT_UX.md
        // section 2a) are bundles over the same settings - provisional until the UI: speedrun = everything known,
        // competent = the future default, knowing only what the player was exposed to.
        static const std::map<std::string, std::vector<std::pair<std::string, std::string>>> presets = {
            {"standard", {{"consumables", "needed"}, {"fast_dock", "1"}}},
            {"no-consumables", {{"consumables", "never"}, {"fast_dock", "1"}}},
            // (+ the kept speedrun switches of sessions 10-11, each speedrun knowledge naming its source - AUTOPILOT.md
            // "Speedrun flight": ring_request (the planet-ring probe), airlock_charge_at (the user: 50 % at the core
            // entry), rejoin_nearest (the user's M12 core note), ref_line (the user's M12 try 3))
            {"speedrun",
             {{"knowledge", "all"},
              {"consumables", "needed"},
              {"fast_dock", "1"},
              {"static_hold", "1"},
              {"ring_request", "10"},
              {"airlock_charge_at", "0.5"},
              {"rejoin_nearest", "1"},
              {"ref_line", "1"}}},
            {"competent",
             {{"knowledge", "exposed"}, {"consumables", "needed"}, {"fast_dock", "1"}, {"static_hold", "1"}}},
        };
        std::string name;
        in >> name;
        const auto it = presets.find(name);
        if (it == presets.end())
            return "presets: standard, no-consumables, speedrun, competent";
        for (const auto& kv : it->second)
            ApplySetting(kv.first, kv.second);
        events::Push("autopilot preset %s", name.c_str());
        return "preset " + name + " | " + Status();
    }
    if (sub == "clear") // ap clear: drop the personality overrides
    {
        g_set.overrides.clear();
        BuildPersonality();
        return Status();
    }
    if (sub == "field")
    {
        std::string f;
        in >> f;
        return f + " = " + ai::GetField(g_p, f);
    }
    if (sub == "save")
    {
        Save(ini);
        return "saved [Autopilot] to " + ini;
    }
    if (sub == "load")
    {
        Load(ini);
        return Status();
    }
    if (sub == "settings") // ap settings: every setting with its value and help (the registry)
        return SettingsReference();
    if (sub.empty() || sub == "status")
        return Status();
    return "ap: start <auto|goto|dock|follow|formation|face|escort|combat> [id | x y z] | stop | status | settings | "
           "pilot <name> | set <setting|p.field> <value> | clear | field <name> | save | load";
}
} // namespace autopilot
