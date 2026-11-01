// The objective-kind coverage table (IDEAS A41, controller\docs\A37_PLAN.md 1 "A41"): every kind of task the game
// generates -> the goal type the autopilot treats it as, and what handles it today. Data only (pure; the situation
// assessor names a step's kind through it, tests\objective_kinds_test.cpp checks it against the install: every Cnd_ a
// story script uses and every vignetteparams.ini objective_text kind has a row, the uses column equals the install's
// count, an unknown name has none). The measure of "handles every task": a row whose `today` says "none" is a gap.
#pragma once
#include "common.h"

#include <cstring>

namespace objkinds
{
enum class GoalType
{
    None,       // no objective
    FlyTo,      // reach a place / an object (distance conditions, navmarkers)
    Dock,       // dock / travel to a base
    TradeLane,  // enter / ride / leave a trade lane
    Jump,       // jump / reach a system
    Attack,     // destroy (by label, nickname, count)
    Defend,     // attack until hit / health, or defend
    Formation,  // a player maneuver (formation, goto, dock maneuver)
    Escort,     // stay with / protect a friendly ship
    Loot,       // tractor loot (or jettison it)
    ReturnLoot, // bring loot back to a base
    Scan,       // scan cargo
    Wait,       // the script's own time: comms, timers, other triggers
    BaseAction, // the player's own act in a base or the front end (talk, accept, launch, a dialog)
    Constraint, // a rule that fails the mission (leashes, tethers, nags, never-kill)
    Trade,      // free roam: buy / sell
    Repair,     // free roam: repair / restock
    Unknown,
    COUNT
};

inline const char* GoalTypeName(GoalType t)
{
    static const char* const k[] = {"none",       "fly_to", "dock",  "trade_lane", "jump",        "attack",
                                    "defend",     "formation", "escort", "loot",    "return_loot", "scan",
                                    "wait",       "base_action", "constraint", "trade", "repair", "unknown"};
    const int i = static_cast<int>(t);
    return i >= 0 && i < static_cast<int>(GoalType::COUNT) ? k[i] : "?";
}

inline GoalType GoalTypeFromName(const char* name)
{
    for (int i = 0; name && i < static_cast<int>(GoalType::COUNT); ++i)
        if (strcmp(GoalTypeName(static_cast<GoalType>(i)), name) == 0)
            return static_cast<GoalType>(i);
    return GoalType::Unknown;
}

// One Cnd_ type of Content.dll (the 43 names of the live trigger table's vtable map, core\game\mission_live.cpp, + the
// one it lacks, Cnd_TetherBroke). `uses` = [Trigger] lines in the HDE 0.7.1 install's DATA\MISSIONS\M*\*.ini (0: never
// used there - kept so another mod's scripts find their row). `alt`: the type the condition has in its other form
// (Cnd_DistShip outside -> a leash).
struct CndRow
{
    const char* cnd; // lower case
    GoalType type;
    GoalType alt;
    int uses;
    const char* today; // what handles it now (controller\src\impl\autopilot_objective.cpp at 48cb87c)
};

constexpr CndRow kCnd[] = {
    {"cnd_distship", GoalType::FlyTo, GoalType::Constraint, 199, "script goal kind 1; outside -> leash (mission::Leashes)"},
    {"cnd_distvec", GoalType::FlyTo, GoalType::None, 148, "script goal kind 2"},
    {"cnd_distcircle", GoalType::FlyTo, GoalType::None, 41, "pass ring (PassRingRadius)"},
    {"cnd_locenter", GoalType::FlyTo, GoalType::None, 35, "navmarker trip"},
    {"cnd_distveclbl", GoalType::FlyTo, GoalType::None, 2, "navmarker trip"},
    {"cnd_inzone", GoalType::FlyTo, GoalType::None, 2, "navmarker trip"},
    {"cnd_cmptoplane", GoalType::FlyTo, GoalType::None, 0, "none"},
    {"cnd_baseenter", GoalType::Dock, GoalType::None, 46, "dock (verb + target)"},
    {"cnd_spaceexit", GoalType::Dock, GoalType::None, 10, "dock (verb + target)"},
    {"cnd_tlentered", GoalType::TradeLane, GoalType::None, 35, "lane ride (LaneOpen)"},
    {"cnd_tlexited", GoalType::TradeLane, GoalType::None, 33, "lane ride (LaneOpen)"},
    {"cnd_intradelane", GoalType::TradeLane, GoalType::None, 3, "lane ride (LaneOpen)"},
    {"cnd_systementer", GoalType::Jump, GoalType::None, 18, "jump (gate / hole target)"},
    {"cnd_spaceenter", GoalType::Jump, GoalType::None, 70, "jump (gate / hole target)"},
    {"cnd_jumpincomplete", GoalType::Jump, GoalType::None, 2, "jump (gate / hole target)"},
    {"cnd_systemexit", GoalType::Jump, GoalType::None, 28, "jump (gate / hole target)"},
    {"cnd_jumpgateact", GoalType::Jump, GoalType::None, 0, "none"},
    {"cnd_npcsystementer", GoalType::Wait, GoalType::None, 24, "none (rest)"},
    {"cnd_npcsystemexit", GoalType::Wait, GoalType::None, 0, "none"},
    {"cnd_destroyed", GoalType::Attack, GoalType::None, 292, "script destroy (kind 3), marks"},
    {"cnd_projhit", GoalType::Defend, GoalType::None, 66, "partial (fights)"},
    {"cnd_healthdec", GoalType::Defend, GoalType::None, 48, "partial (fights)"},
    {"cnd_projhitshiptolbl", GoalType::Defend, GoalType::None, 6, "partial (fights)"},
    {"cnd_playermaneuver", GoalType::Formation, GoalType::None, 9, "formation / follow"},
    {"cnd_lootacquired", GoalType::Loot, GoalType::None, 2, "partial (tractor marked loot)"},
    {"cnd_cargoscanned", GoalType::Scan, GoalType::None, 1, "none"},
    {"cnd_commcomplete", GoalType::Wait, GoalType::None, 607, "rest / pre-stage"},
    {"cnd_timer", GoalType::Wait, GoalType::None, 380, "rest / pre-stage"},
    {"cnd_true", GoalType::Wait, GoalType::None, 337, "rest / pre-stage"},
    {"cnd_watchtrigger", GoalType::Wait, GoalType::None, 100, "rest / pre-stage"},
    {"cnd_rtcdone", GoalType::Wait, GoalType::None, 2, "rest / pre-stage"},
    {"cnd_watchvibe", GoalType::Wait, GoalType::None, 1, "rest / pre-stage"},
    {"cnd_enclaunched", GoalType::Wait, GoalType::None, 1, "rest / pre-stage"},
    {"cnd_msnresponse", GoalType::BaseAction, GoalType::None, 27, "harness only"},
    {"cnd_charselect", GoalType::BaseAction, GoalType::None, 5, "harness only"},
    {"cnd_popupdialog", GoalType::BaseAction, GoalType::None, 4, "harness only"},
    {"cnd_launchcomplete", GoalType::BaseAction, GoalType::None, 19, "harness only"},
    {"cnd_playerlaunch", GoalType::BaseAction, GoalType::None, 18, "harness only"},
    {"cnd_hasmsn", GoalType::BaseAction, GoalType::None, 2, "harness only"},
    {"cnd_baseexit", GoalType::BaseAction, GoalType::None, 6, "harness only"},
    {"cnd_inspace", GoalType::BaseAction, GoalType::None, 15, "harness only"},
    {"cnd_locexit", GoalType::BaseAction, GoalType::None, 6, "harness only"},
    {"cnd_rumorheard", GoalType::BaseAction, GoalType::None, 0, "none"},
    {"cnd_tetherbroke", GoalType::Constraint, GoalType::None, 5, "none (leash: distship only)"},
};
constexpr int kCndCount = static_cast<int>(sizeof(kCnd) / sizeof(kCnd[0]));

// One job (random mission) objective kind: vignetteparams.ini objective_text's first field (core\game\jobs.h).
struct JobRow
{
    const char* kind; // lower case
    GoalType type;
    const char* today;
};

constexpr JobRow kJob[] = {
    {"guide_to_main_battle", GoalType::FlyTo, "verb only (navmarker trip)"},
    {"guide_to_pre_battle", GoalType::FlyTo, "verb only (navmarker trip)"},
    {"guide_to_main_from_pre", GoalType::FlyTo, "verb only (navmarker trip)"},
    {"return_to_battle", GoalType::FlyTo, "verb only (navmarker trip)"},
    {"kill_big_solar", GoalType::Attack, "verb only (fight marks)"},
    {"kill_other_solars", GoalType::Attack, "verb only (fight marks)"},
    {"pb_kill_ships", GoalType::Attack, "verb only (fight marks)"},
    {"pb_kill_big_ships", GoalType::Attack, "verb only (fight marks)"},
    {"mb_kill_ships", GoalType::Attack, "verb only (fight marks)"},
    {"mb_kill_big_ships", GoalType::Attack, "verb only (fight marks)"},
    {"destroy_loot", GoalType::Attack, "verb only (DestroyMarkedLoot)"},
    {"pb_dont_kill_ships", GoalType::Constraint, "none"},
    {"mb_dont_kill_ships", GoalType::Constraint, "none"},
    {"tractor_loot", GoalType::Loot, "partial (tractor marked loot)"},
    {"return_loot", GoalType::ReturnLoot, "none"},
    {"jettison_loot", GoalType::Loot, "none"},
};
constexpr int kJobCount = static_cast<int>(sizeof(kJob) / sizeof(kJob[0]));

// Free roam (no objective): the tasks the player sets.
struct FreeRow
{
    const char* task;
    GoalType type;
    const char* today;
};
constexpr FreeRow kFree[] = {
    {"trade", GoalType::Trade, "none (harness Invoke-FLTraderBuy)"},
    {"repair", GoalType::Repair, "none (harness Invoke-FLBaseRepair)"},
    {"explore", GoalType::FlyTo, "travel (ap start travel)"},
};

inline bool EqualsNoCase(const char* a, const char* b)
{
    const auto low = [](char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; };
    for (; *a && *b; ++a, ++b)
        if (low(*a) != low(*b))
            return false;
    return *a == *b;
}

// The row of a Cnd_ / job kind name (any case), null when the table has none (the coverage test fails on that).
inline const CndRow* CndOf(const char* name)
{
    for (const CndRow& r : kCnd)
        if (name && EqualsNoCase(r.cnd, name))
            return &r;
    return nullptr;
}
inline const JobRow* JobOf(const char* name)
{
    for (const JobRow& r : kJob)
        if (name && EqualsNoCase(r.kind, name))
            return &r;
    return nullptr;
}
} // namespace objkinds
