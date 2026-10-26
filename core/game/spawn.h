// NPC ships spawned natively (the game's own server calls: pub::Reputation::Alloc / SetAffiliation,
// pub::SpaceObj::Create, pub::AI::SubmitState with the [Pilot] personality), from the game's NPC templates
// (DATA\MISSIONS\npcships.ini [NPCShipArch]: ship_archetype, loadout, level, pilot, state_graph). Test aid for
// controlled combat benchmarks (the same enemies every time, a steady stream) and later for scripted scenarios.
// Facts: docs\FREELANCER_RE.md "Spawning ships". Shared by every module (core\game), compiled into each.
#pragma once
#include "common.h"

#include <string>
#include <vector>

namespace spawn
{
struct Request
{
    std::string npc;       // [NPCShipArch] nickname, e.g. "li_p_li_fighter_d1-2"
    std::string faction;   // reputation group nickname, e.g. "li_p_grp" (its attitude to the player decides hostility),
                           // or "player": a private reputation hostile to the player only (a clean arena)
    float distance = 1500; // from the player's ship (m), on a random bearing in the horizontal plane
    float spread = 300;    // random offset around that point (m)
    // Job block over the template's pilot; "" = keep it. The default is the game's solo job with scene toughness
    // HARDEST: with basic_job_no_formation (HARD) a lone spawn of levels 1-4 judged the scene too tough and never
    // engaged (18 Oct 2026; encounter NPCs come in groups). Templates' pilots often carry formation jobs (wingmen that
    // fight only under an encounter leader).
    std::string job = "basic_job_no_formation_no_flee";
    std::string pilot;    // [Pilot] instead of the template's ("" = the template's)
    bool trail = true;    // order it to trail the player, weapons free (false: no order - the state graph alone)
    bool idle = false;    // order=idle: the game's Idle directive (stops and holds still)
    bool liveBase = true; // personality base: a live NPC's (false: the game's default Personality)
    bool force = true;    // "player" faction: the player as its forced target
    u32 level = 0;        // NPC level (0 = the template's)
};

// Spawns one ship; returns its space id, 0 on failure (err says why).
u32 Spawn(const Request& r, std::string* err);
// Destroys a ship we spawned (pub::SpaceObj::Destroy, VANISH: no explosion, no loot).
bool Despawn(u32 id);
// Our spawned ships still alive.
std::vector<u32> Alive();
// spawn clearat: called every frame with the player's hull fraction (-1 unknown); despawns ours at the threshold
void CheckClearAt(float hull);

// Debug: `spawn <npc> <faction> [count] [distance] [job=<block>|keep] [pilot=<nick>] [order=trail|none|idle]
// [base=live|default] [force=0|1] [level=n]`, `spawn list`, `spawn clear`, `spawn clearat <hull 0..1>|off` (the game
// despawns ours the frame the player's hull falls to that fraction: test damage without harness poll lag).
bool Command(const std::string& cmd, const std::string& rest, std::string* reply);
} // namespace spawn
