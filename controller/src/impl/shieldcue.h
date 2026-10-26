// Shield cue: a screen-edge vignette made of the player's own shield's hit effect (user, 19 Oct 2026: context-sensitive
// to the fitted shield and its game VFX; tinted by the incoming weapon's impact look; stronger with the damage burst;
// a unique flash when the shield fails). Design: controller\docs\HUD_CUES.md; looks: core\game\hitlook.
#pragma once
#include "common.h"

#include <string>

namespace shieldcue
{
struct Settings
{
    bool on = true;
    float intensity = 1.0f; // 0..2: alpha and size scale
    bool hull = true;       // hull hits in the ship's hull hit look + the damage stages' edge effects
    int pattern = 2;        // hull damage edge pattern: 0 none, 1 armour plates, 2 hexagons (user's choice)
};
Settings& Get();

// Game state (every frame, flying or not): detects shield / hull hits and shield failure.
void Update(float dt, bool flying);
// Draws the cue (under the HUD: OnRenderUnderHud, at the start of the game's UI pass; the overlay as fallback).
void Render(void* device);
// Debug: simulate a hit (kind 0), a shield failure (1), a hull hit with the shield down (2); munition 0 = the nearest
// attacker's guns.
std::string Test(int kind, float damage, u32 munition);
// Debug: pin the hit direction (screen degrees: 0 = right, 90 = down) for tests; off = from the game again.
std::string TestDir(bool on, float degrees);
// Debug: the damage edge acts as if the hull had these points (< 0 = the ship's own again).
std::string TestHull(float hullPts);
std::string Status();
// Frees the cue's D3D textures (ctl::Shutdown, before the logic unloads: a hot reload leaked them before 24 Oct 2026).
void Shutdown();
} // namespace shieldcue
