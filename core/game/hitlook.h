// How a hit looks, from the game's own data (user, 19 Oct 2026: the HUD shield cue shows the fitted shield's hit look,
// tinted by the incoming weapon's impact look). The same join as tools\hitfx.js (table: docs\kb\hit_effects.md):
// st_equip.ini [ShieldGenerator] shield_hit_effects (by damage) / weapon_equip.ini [Munition] munition_hit_effect ->
// DATA\FX\effects.ini [Effect] vis_effect -> a [VisEffect] under DATA\FX (alchemy .ale + effect_crc + texture
// libraries) -> the effect's appearance nodes: texture, colour / alpha / size keys over the particle's life, the
// emitters' particle lifespan and count. Facts: docs\FREELANCER_RE.md "Hit effects".
#pragma once
#include "common.h"

#include <string>
#include <vector>

namespace hitlook
{
struct Key
{
    float t, v;
};
struct ColorKey
{
    float t, r, g, b;
};

struct Layer
{
    std::string name;              // the appearance node's Node_Name ("gf_li_shield01.app")
    std::string tex;               // BasicApp_TexName ("kioncanring")
    std::vector<std::string> libs; // the [VisEffect]'s texture libraries, DATA-relative ("fx\\kioncannon.txm")
    bool additive = true;          // BasicApp_BlendInfo SRCALPHA/ONE (else SRCALPHA/INVSRCALPHA)
    std::vector<ColorKey> color;   // over the particle's life 0..1
    std::vector<Key> alpha, size;  // size in metres
    float life = 0;                // particle lifespan, s (the feeding emitters' Emitter_InitLifeSpan; 0 = node life)
    int count = 1;                 // particles at once (Emitter_InitialParticles, at least 1)
    float rate = 0;                // particles per second while the emitter runs (Emitter_Frequency)
};

struct Look
{
    std::string effect; // the [Effect] nickname
    std::string vis;    // its [VisEffect]
    std::vector<Layer> layers;
    bool ok() const { return !layers.empty(); }
};

// The fitted shield's hit look for a hit of `damage` shield points (the shield_hit_effects tier the game picks); nullptr
// when unknown (no such shield, no data). Cached; the first call reads the game's files.
const Look* Shield(u32 shieldArch, float damage);
// A munition's impact look (weapon_equip.ini [Munition] munition_hit_effect), by the munition archetype id.
const Look* Impact(u32 munitionArch);
// The ship's own hull hit look for a hit of `damage` hull points (shiparch.ini surface_hit_effects tiers: the stock
// ships all share gf_br_hull_impact01 - a white ring and flare - the tiers differ only in sound).
const Look* Hull(u32 shipArch, float damage);

// A damage stage of the ship: a non-death fuse (shiparch.ini fuse = name, t, hit points; DATA\FX\fuse*.ini) that the
// game lights when the hull drops below `hitPts` - its effects (small ships: sparks, then fire + smoke, then
// continuous damage). Ordered from the first to light (highest hitPts) to the last.
struct DamageStage
{
    float hitPts = 0;
    std::string fuse;
    std::vector<const Look*> looks;
};
const std::vector<DamageStage>& DamageStages(u32 shipArch);

// Key interpolation (linear; clamps outside the keys).
float Eval(const std::vector<Key>& k, float t, float def = 0);
void EvalColor(const std::vector<ColorKey>& k, float t, float& r, float& g, float& b);
// The layer's texture (first frame of an animated one) as top-down 0xAARRGGBB pixels; false if not found / decodable.
bool Texture(const Layer& l, int& w, int& h, std::vector<u32>& argb);
// Debug text: the look of a shield / munition (debug `hitlook`).
std::string Describe(const Look* l);
} // namespace hitlook
