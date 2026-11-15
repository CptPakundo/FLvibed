// HUD 2 map's influence cells (GitHub #61, the owner's design v2B): who operates where in a system, as a field of hex cells
// computed by the encounter spawn simulator (core\game\spawnsim.h) from the system's loaded data. Each cell holds the
// expected ships near a player held at its centre (spawnsim's census count), summed by the player's relation to their
// factions; the map draws a cell bigger and brighter when stronger, coloured by the strongest relation, and alternates
// two relations over neighbouring cells where the second reaches half the first (contested). Host-free: no game, no files.
// Cost control: cells whose spawn lists are the same (the zones at the ship, the smallest one, the zones within the near and
// view reach) share one simulation, run at the member cell nearest their centre.
#pragma once

#include "game/spawnsim.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

namespace influence
{
enum Relation
{
    kFriendly = 0, // the order of core\game\zonemix.h
    kNeutral = 1,
    kHostile = 2
};
// The player's relation to a faction by the game's thresholds (feeling <= -0.6 hostile, >= 0.6 friendly; zonemix::RelationOf).
int RelationOf(double standing);

struct Cell
{
    float x = 0, z = 0;           // the centre, world metres
    float strength[3] = {};       // expected ships near a player here, by relation
    int lead = -1, second = -1;   // the strongest relation; the contested one (>= Options::contested x lead), -1 none
    uint32_t zones[4] = {};       // CreateID of the non-path zones with encounters that hold the centre (the political layer's zones)
    int nZones = 0;
    int col = 0, row = 0;         // the cell's place in the hex grid (the contested pattern alternates by it)
};

// The look (the owner's v2B): strength on a log scale to kFullShips (the v3 mock's), a cell's size and opacity grow with it; a contested cell
// shows its second relation on every other cell of the grid.
constexpr float kFullShips = 12;
inline float StrengthT(float ships)
{
    const float t = std::log(1 + std::max(0.0f, ships)) / std::log(1 + kFullShips);
    return t > 1 ? 1 : t;
}
inline float CellScale(float t) { return 0.35f + 0.65f * t; } // of the full hex (touching neighbours)
inline float CellAlpha(float t) { return 0.30f + 0.70f * t; }
inline int CellRelation(const Cell& c) { return c.second >= 0 && ((c.col + c.row) & 1) ? c.second : c.lead; }
inline float CellShips(const Cell& c) { return c.lead >= 0 ? c.strength[c.lead] : 0; }

struct Options
{
    double spacing = 0;      // the hex grid's spacing in metres; 0 = the populated zones' span / 100, at least 1000
    int runs = 16;           // simulation runs per distinct list signature
    double seconds = 90;     // the player's dwell per run (counts from 30 s, spawnsim::Query)
    double contested = 0.5;  // a second relation at this share of the lead marks the cell contested
    double floor = 0.05;     // cells with fewer expected ships than this are dropped
    int maxCells = 40000;    // the grid's cap (the spacing grows to fit)
};

struct Field
{
    double spacing = 0;
    std::vector<Cell> cells;
    int signatures = 0; // distinct list signatures = simulations run
    bool cancelled = false;
};

// The field over the world's populated zones; `cancel` (optional) stops it early (Field::cancelled).
Field Compute(const spawnsim::World& w, const Options& o, const std::atomic<bool>* cancel = nullptr);
// The relations of the world's factions (RelationOf each standing), for "did a relation flip" checks.
std::vector<int> Relations(const spawnsim::World& w);
} // namespace influence
