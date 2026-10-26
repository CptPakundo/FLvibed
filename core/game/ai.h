// The game's own AI on the player ship (shared by every module, compiled into each logic part).
// Three layers, all the game's code (docs\FREELANCER_RE.md "AI on the player ship"; research:
// controller\docs\NPC_AI_RESEARCH.md):
//  - Maneuvers: the player's maneuver dispatcher (what the maneuver bar and keys use), incl. Trail, Face, Strafe.
//  - Directives: server.dll pub::AI::SubmitDirective with the game's directive ops (goto, follow, dock, idle, ...),
//    every parameter settable.
//  - Brain: the NPC state graph (FIGHTER, ...) + a pilot personality (the game's [Pilot] definitions or a live NPC's)
//    via pub::AI::SubmitState: the ship then flies and fights like an NPC. Restored to the PLAYER graph on release.
#pragma once
#include "common.h"

#include <string>
#include <vector>

namespace ai
{
// pub::AI::OpType
enum Op : int
{
    OpNone = -1,
    OpBuzz = 1,
    OpGoto = 2,
    OpTrail = 3,
    OpFlee = 4,
    OpEvade = 5,
    OpIdle = 6,
    OpDock = 7,
    OpLaunch = 8,
    OpTradeLane = 9,
    OpFormation = 10,
    OpCruise = 12,
    OpStrafe = 13,
    OpGuide = 14,
    OpFace = 15,
    OpLoot = 16,
    OpFollow = 17,
    OpDrasticEvade = 18,
    OpFreeFlight = 19,
    OpDelay = 20,
};
const char* OpName(int op);
int OpFromName(const std::string& name); // -2 if unknown

// One attack_preference entry (type 12 ends the list).
struct JobAttack
{
    int type;
    float distance;
    int flag;
};

// pub::AI::Personality (FLSDK layout; the game's constructor fills the defaults).
struct Personality
{
    struct EvadeDodge
    {
        float evade_activate_range, evade_dodge_style_weight[4], evade_dodge_cone_angle,
            evade_dodge_cone_angle_variance_percent, evade_dodge_waggle_axis_cone_angle, evade_dodge_roll_angle,
            evade_dodge_interval_time, evade_dodge_interval_time_variance_percent, evade_dodge_distance,
            evade_dodge_time, evade_dodge_slide_throttle, evade_dodge_turn_throttle,
            evade_dodge_corkscrew_turn_throttle, evade_dodge_corkscrew_roll_throttle;
        bool evade_dodge_corkscrew_roll_flip_direction;
        float evade_dodge_direction_weight[4];
    } EvadeDodge;
    struct EvadeBreak
    {
        float evade_break_time, evade_break_interval_time, evade_break_afterburner_delay,
            evade_break_afterburner_delay_variance_percent, evade_break_direction_weight[4], evade_break_roll_throttle,
            evade_break_turn_throttle, evade_break_style_weight[3], evade_break_attempt_reverse_time,
            evade_break_reverse_distance;
    } EvadeBreak;
    struct BuzzHeadToward
    {
        float buzz_max_time_to_head_away, buzz_head_toward_style_weight[3], buzz_min_distance_to_head_toward,
            buzz_min_distance_to_head_toward_variance_percent, buzz_head_toward_engine_throttle,
            buzz_head_toward_turn_throttle, buzz_head_toward_roll_throttle;
        bool buzz_head_toward_roll_flip_direction;
        float buzz_dodge_direction_weight[4], buzz_dodge_turn_throttle, buzz_dodge_cone_angle,
            buzz_dodge_cone_angle_variance_percent, buzz_dodge_waggle_axis_cone_angle, buzz_dodge_roll_angle,
            buzz_dodge_interval_time, buzz_dodge_interval_time_variance_percent, buzz_slide_throttle,
            buzz_slide_interval_time, buzz_slide_interval_time_variance_percent;
    } BuzzHeadToward;
    struct BuzzPassBy
    {
        float buzz_pass_by_style_weight[3], buzz_distance_to_pass_by, buzz_pass_by_time;
        bool buzz_drop_bomb_on_pass_by;
        float buzz_break_direction_weight[4], buzz_break_direction_cone_angle, buzz_break_turn_throttle,
            buzz_pass_by_roll_throttle;
    } BuzzPassBy;
    struct Trail
    {
        float trail_lock_cone_angle, trail_break_time, trail_min_no_lock_time, trail_break_roll_throttle;
        bool trail_break_afterburner;
        float trail_max_turn_throttle, trail_distance;
    } Trail;
    struct Strafe
    {
        float strafe_run_away_distance, strafe_attack_throttle, strafe_turn_throttle;
    } Strafe;
    struct EngineKill
    {
        float engine_kill_search_time, engine_kill_face_time, engine_kill_use_afterburner, engine_kill_afterburner_time,
            engine_kill_max_target_distance;
    } EngineKill;
    struct Repair
    {
        float use_shield_repair_pre_delay, use_shield_repair_at_damage_percent, use_shield_repair_post_delay,
            use_hull_repair_pre_delay, use_hull_repair_at_damage_percent, use_hull_repair_post_delay;
    } Repair;
    struct Gun
    {
        int fire_style;
        float gun_fire_interval_time, gun_fire_interval_variance_percent, gun_fire_burst_interval_time,
            gun_fire_burst_interval_variance_percent, gun_fire_no_burst_interval_time, gun_fire_accuracy_cone_angle,
            gun_fire_accuracy_power, gun_fire_accuracy_power_npc, gun_range_threshold,
            gun_range_threshold_variance_percent, gun_target_point_switch_time, auto_turret_interval_time,
            auto_turret_burst_interval_time, auto_turret_no_burst_interval_time,
            auto_turret_burst_interval_variance_percent;
    } Gun;
    struct Mine
    {
        float mine_launch_interval, mine_launch_cone_angle, mine_launch_range;
    } Mine;
    struct Missile
    {
        float missile_launch_range;
        bool missile_launch_allow_out_of_range;
        float missile_launch_interval_time, missile_launch_interval_variance_percent, missile_launch_cone_angle,
            anti_cruise_missile_min_distance, anti_cruise_missile_max_distance, anti_cruise_missile_pre_fire_delay,
            anti_cruise_missile_interval_time;
    } Missile;
    struct DamageReaction
    {
        float evade_break_damage_trigger_percent, evade_dodge_more_damage_trigger_percent,
            drop_mines_damage_trigger_percent, drop_mines_damage_trigger_time, engine_kill_face_damage_trigger_percent,
            engine_kill_face_damage_trigger_time, roll_damage_trigger_percent, roll_damage_trigger_time,
            afterburner_damage_trigger_percent, afterburner_damage_trigger_time, brake_reverse_damage_trigger_percent,
            fire_missiles_damage_trigger_percent, fire_missiles_damage_trigger_time, fire_guns_damage_trigger_percent,
            fire_guns_damage_trigger_time;
        int _03C, _040;
    } DamageReaction;
    struct MissileReaction
    {
        float evade_break_missile_reaction_time, evade_slide_missile_reaction_time,
            evade_afterburn_missile_reaction_time, evade_missile_distance;
    } MissileReaction;
    struct Countermeasure
    {
        float countermeasure_active_time, countermeasure_unactive_time;
    } Countermeasure;
    struct Formation
    {
        float force_attack_formation_active_time, force_attack_formation_unactive_time,
            break_formation_damage_trigger_percent, break_formation_damage_trigger_time,
            break_apart_formation_damage_trigger_percent, break_apart_formation_damage_trigger_time,
            break_formation_missile_reaction_time, break_apart_formation_missile_reaction_time;
        bool break_apart_formation_on_buzz_head_toward;
        float break_formation_on_buzz_head_toward_time;
        bool regroup_formation_on_buzz_head_toward, break_apart_formation_on_buzz_pass_by;
        float break_formation_on_buzz_pass_by_time;
        bool regroup_formation_on_buzz_pass_by, break_apart_formation_on_evade_dodge;
        float break_formation_on_evade_dodge_time;
        bool regroup_formation_on_evade_dodge, break_apart_formation_on_evade_break;
        float break_formation_on_evade_break_time;
        bool regroup_formation_on_evade_break;
        int formation_exit_mode;
        float formation_exit_top_turn_break_away_throttle, formation_exit_roll_outrun_throttle, formation_exit_max_time;
        bool leader_makes_me_tougher;
    } Formation;
    struct Job
    {
        int _000, scene_toughness_threshold, target_toughness_preference;
        float combat_drift_distance;
        JobAttack attack_order[13];
        int attack_subtarget_order[8];
        bool wait_for_leader_target;
        float maximum_leader_target_distance;
        int field_targeting, loot_preference, loot_flee_threshold, _0E0, _0E4_120[16], flee_scene_threat_style;
        float flee_when_hull_damaged_percent;
        bool flee_when_leader_flees_style, flee_no_weapons_style, allow_player_targeting;
        float _130;
        bool force_attack_formation, force_attack_formation_used;
    } Job;
};

// Readout of the player ship's AI (false when not flying).
struct State
{
    int op = -1;         // running behaviour (behaviour manager +0xC0)
    bool locked = false; // maneuvers locked (+0xBC, Act_LockManeuvers)
    int priority = 0;    // directive priority (+0xC8)
    int graph = 0;       // state graph id (server pub::AI::get_state_graph_id)
    int graphPlayer = 0; // id of the "PLAYER" graph
    int behavior = -1;   // server pub::AI::get_behavior_id
    bool brain = false;  // FLvibed put an NPC brain on the ship (ai::Brain)
    std::string pilot;   // the brain's personality source
    u32 forced = 0;      // forced target set by ai::ForceTarget
};
bool ReadState(State& s);

// ---- Maneuvers: the player's maneuver dispatcher (thiscall 0x4D8E20, this = [0x674A58]), on the current target.
// Reports the maneuver to the server like the maneuver bar does (Cnd_PlayerManeuver sees it). False if refused.
bool Maneuver(int op, bool flag = false);

// ---- Directives: server.dll pub::AI::SubmitDirective on the player ship. Returns the server's result code (0 ok),
// or -100 when the API or the ship is missing.
struct Directive
{
    int op = OpGoto;
    u32 target = 0;      // space object (goto ship, follow, dock, trail, strafe, face, evade)
    bool hasPos = false; // goto / face a point
    float pos[3] = {};
    float range = 500;    // goto: stop distance; follow: max distance; trail: distance
    float thrust = -1;    // goto: 0..100, -1 = full
    bool cruise = true;   // goto: use cruise
    float offset[3] = {}; // follow: formation offset
    float time = 1;       // delay, drastic evade: seconds
    int mode = 0;         // drastic evade: 0 = slide along `offset` (x right, y up, -1..1), 1 = full reverse
    bool fire = false;    // BaseOp fireWeapons: the op may use weapons
    bool client = false;  // apply on the player's own behaviour manager (as the maneuver dispatcher does) instead of
                          // the server: needed for Trail / Strafe / Evade / DrasticEvade, which the server refuses
                          // for the player
};
// Result codes besides the server's: -100 API or ship missing, -104 Evade without the guard (SetEvadeGuard),
// -105 Evade threat not found (the behaviour resolved no object from `target`).
int Submit(const Directive& d);
// The same directive for another ship (an NPC we spawned), server side.
int SubmitFor(u32 ship, const Directive& d);

// Evade on the player ship needs the host's guard (the game skips its threat check for the player ship and crashes
// once the threat is gone; docs\FREELANCER_RE.md). The module sets the function that installs it; without one,
// client Evade is refused (-104).
void SetEvadeGuard(bool (*guard)(void** vtable));

// ---- Brain: the NPC state graph + a personality on the player ship.
// Personalities: the game's [Pilot] definitions (DATA\MISSIONS\pilots_population.ini, pilots_story.ini, with
// inherit), or a live NPC's (pub::AI::get_personality). Fields can be changed by their ini name before or while the
// brain runs (SetField + Brain again).
std::vector<std::string> Pilots(); // every [Pilot] nickname
bool LoadPilot(const std::string& pilot, Personality& out, std::string* err);
bool CopyNpcPersonality(u32 npc, Personality& out);
// The [Pilot]'s fields laid over an existing personality (e.g. a live NPC's): members the field table does not know
// keep the base's values. A personality from the default constructor + fields alone leaves internal members unset
// and spawned NPCs then never engage (18 Oct 2026); a live NPC's base fixes that.
bool LoadPilotOnto(const std::string& pilot, Personality& inout, std::string* err);
// One block of the pilot files (e.g. the job "basic_job_no_formation") over a personality.
bool ApplyBlock(const std::string& block, Personality& inout, std::string* err);
bool SetField(Personality& p, const std::string& name, const std::string& value, std::string* err);
std::string GetField(const Personality& p, const std::string& name);
std::vector<std::string> FieldNames();
Personality& Current(); // the personality Brain() uses (starts as the game's defaults)
// graph: "FIGHTER" (default), "LEADER", "ESCORT", "GUNBOAT", ... (DATA\AI\state_graph.db). Saves the ship's own
// personality and graph on the first call. Returns the server's result code (0 ok).
int Brain(const std::string& graph = "FIGHTER", const std::string& pilotLabel = "");
// Back to the PLAYER graph and the ship's own personality, free flight. Safe to call any time.
int Release();
bool BrainActive();
// Gives any ship (an NPC we spawned) a personality and state graph ("FIGHTER", ...) via pub::AI::SubmitState
// (SetPersonalityParams): it then flies and fights by them. Returns SubmitState's result (0 ok), < 0 not available.
int GiveBrain(u32 ship, const Personality& p, const char* graph);

// The personality the player ship's own behaviours use (the game's Evade dodge and break styles, Trail breaks):
// set on the player's behaviour manager. Apply saves the ship's own personality first (once per ship); Restore
// puts it back. False when not flying.
bool ApplyShipPersonality(const Personality& p);
void RestoreShipPersonality();

// Forced target for the brain (server pub::AI::submit_forced_target); 0 removes it.
int ForceTarget(u32 target);

// Cancel whatever runs: free flight through the maneuver dispatcher (the player's own "cancel").
void FreeFlight();

// The player ship's hull, 0..1 (CSimple::get_relative_health); -1 when not flying.
float PlayerHealth();

// What the player ship can do right now, from the game's own objects (Common.dll CShip / CEqObj / CEShield).
struct Self
{
    float hull = -1;     // 0..1
    float hullMax = 0;   // hull hit points at full health
    float shield = -1;   // 0..1 (-1: no shield or it is down for good)
    float shieldMax = 0; // shield capacity (hit points)
    u32 shieldArch = 0;  // the shield generator's archetype id (nickname hash, st_equip.ini [ShieldGenerator])
    u32 shipArch = 0;    // the ship's archetype id (nickname hash, shiparch.ini [Ship])
    float power = -1;    // gun / power capacitor, 0..1
    float gunRange = 0;  // farthest active gun's range (m)
    int nanobots = 0, nanobotMax = 0;  // repair kits aboard / the ship's limit
    int batteries = 0, batteryMax = 0; // shield batteries aboard / limit
    u32 tractorArch = 0;               // the mounted tractor's archetype id
    float tractorRange = 0;            // the mounted tractor's reach (m; its [Tractor] max_length), 0 none / unknown
    float holdSize = 0;                // cargo hold capacity (volume units, Archetype::Ship +0x100)
    float holdFree = -1;               // free room in the hold (CShip::get_cargo_hold_remaining), -1 unknown
};
bool ReadSelf(Self& s); // false when not flying
// Uses `amount` shield batteries (battery) or nanobots: the game's own IObjAffect::use_item on the player ship (what the
// HUD's buttons call - they pass the count that fills the bar). Returns the units requested (min(amount, held)), 0 if
// none aboard / not flying / the function did not verify (fails closed).
int UseConsumable(bool battery, int amount);
// Maneuvers locked by the mission (Act_LockManeuvers), or a mission directive with priority running that moves the
// ship (a parked player - Idle / free flight at a priority, Act_PobjIdle - is free to fly: not a hold).
bool MissionHolds();
// A dock is in progress: the player's behaviour manager holds a docking port (queued, approaching, docking; Common.dll
// IBehaviorManager::get_docking_port). State for "the dock goes on", instead of waiting out moments of free flight.
bool DockInProgress();
// The docking port the game assigned (the port's object id and its index = the get_dock_hardpoints index), false when
// no dock is in progress.
bool DockPort(u32& objectId, int& index);

// Called once per frame by the module (state upkeep: releases the brain when the ship is gone).
void Update();

// Debug command: ai <state|man|dir|brain|release|pilots|pilot|field|set|target|free>. False if not `ai`.
bool Command(const std::string& cmd, const std::string& rest, std::string* reply);
} // namespace ai
