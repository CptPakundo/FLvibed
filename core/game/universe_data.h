// The universe tables the autopilot steers by, from the game's data alone (no game needed: built into the unit tests
// too): the system file of a system (universe.ini), and per system its zones' encounter levels, strict zones (costly
// fields and damage zones, with a field's exclusion zones), lethal spheres (planet / sun cores) and planet dock rings.
// Read through ini::Read, so vanilla 1.1's binary (BINI) inis read like HD Edition's text ones (until
// then the hand-written text parsers in space_universe.cpp saw nothing on vanilla: SWEEP_2026-10-29 #5). space_universe.cpp
// caches the result per system and answers the game-side questions with it.
#pragma once
#include "common.h"
#include "game/ini.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace unidata
{
typedef u32 (*IdFn)(const char* nickname); // the game's CreateID (Common.dll's, or flhash::CreateId)

struct StrictZone
{
    std::string why;             // "damage", or the field's flags ("mine_danger_objects,damaging")
    std::vector<u32> exclusions; // a field's [Exclusion Zones] (gate and lane corridors through it)
};
struct LethalSphere
{
    float pos[3];
    float radius;
};
struct System
{
    std::unordered_map<u32, int> levels;        // zone id -> highest encounter level (`encounter = name, level, chance`)
    std::unordered_map<u32, StrictZone> strict; // zone id -> why it is costly
    std::vector<LethalSphere> lethal;           // SPHERE zones whose damage is >= lethalDamage per second
    std::unordered_map<u32, u32> dockRings;     // planet id -> its docking ring (the object whose dock_with = its base)
};

// The system's file from universe.ini's sections ([system] nickname + file, relative to DATA\UNIVERSE), "" if unknown.
std::string SystemFile(const std::vector<ini::Section>& universe, u32 system, IdFn id);
// One system's tables. `dataDir` = the DATA folder with a trailing backslash; `systemFile` relative to DATA\UNIVERSE.
// A zone with damage > strictDamage is strict ("damage"), or a lethal sphere when it is a SPHERE with a position and a
// radius and damage >= lethalDamage (space::ZoneRules); a field ([asteroids] / [nebula] file + zone) is strict when its
// file's [properties] flags say mine_danger_objects, gas_danger_objects, object_density_high or damaging.
System ParseSystem(const std::string& dataDir, const std::string& systemFile, IdFn id, float lethalDamage,
                   float strictDamage);
} // namespace unidata
