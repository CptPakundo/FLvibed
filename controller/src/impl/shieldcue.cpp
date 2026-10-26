#include "shieldcue.h"
#include "flexe.h"
#include "host.h"

#include "game/ai.h"
#include "game/hitlook.h"
#include "game/render.h"
#include "game/space.h"
#include "log.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace shieldcue
{
static Settings g_set;
Settings& Get() { return g_set; }

// A sprite on the screen edge, animated by one layer of a hit look (its colour / alpha / size keys over its life).
struct Particle
{
    const hitlook::Layer* layer = nullptr;
    float dx = 0, dy = 0; // edge direction from the screen centre (unit)
    float along = 0;      // offset along the edge, fraction of the screen height
    float angle = 0;      // sprite rotation
    float age = 0, life = 0.6f;
    float strength = 1; // alpha / size scale
    float tint[3] = {1, 1, 1};
    float tintMix = 0;    // 0 = the layer's own colour
    float sizeK = 0.42f;  // size at full scale, fraction of the screen height
    float alphaK = 0.45f; // alpha at full strength
    bool soft = false;    // fades in and out over its life (the damage edge)
};
static std::vector<Particle> g_parts;
static std::vector<Particle> g_ambient; // the hull's damage stages: a steady edge of the ship's own damage effects

struct Flash // a whole-edge glow (shield failure, burst glow), brighter toward the hit direction
{
    float r = 1, g = 1, b = 1;
    float age = 0, life = 0.3f, strength = 0;
    float dx = 0, dy = 1; // hit direction on the screen (unit)
    float rest = 1;       // brightness away from the hit side, relative (1 = even all around)
    bool hull = false;    // a hull hit: drawn with the hull pattern
};
static std::vector<Flash> g_flashes;

static struct
{
    bool known = false, up = true;
    float shield = 0, hull = 0; // points
    float burst = 0;            // damage over the last ~0.5 s, fraction of the shield's capacity
    float pending = 0, pendingHull = 0, since = 0;
    u32 attacker = 0;
    float dir[2] = {0, 1};
    u32 hitSeq = 0;            // the game's hit reports consumed (HostPlayerHit)
    int gameDirs = 0;          // cue directions taken from the game's hit point
    int stage = 0, stages = 0; // damage stages lit / the ship has
    float emit = 0;            // damage-stage particle groups due
    float testHull = -1;       // debug: hull points the damage edge uses instead of the ship's (-1 = the ship's)
    float glow = 0, pulse = 0; // the damage edge's steady glow (alpha) and its breathing clock
    float sev = 0;             // hull damage severity 0..1 (from the first fuse's hit points to zero)
    bool critical = false;     // below the game's hull-critical point
    float glowColor[3] = {0.9f, 0.45f, 0.15f};
    bool testDir = false; // debug: keep the direction set by `cue dir`
    u32 rng = 12345;
    int hits = 0, breaks = 0, hullHits = 0;
} g;

static float Rand()
{
    g.rng = g.rng * 1664525u + 1013904223u;
    return (g.rng >> 8) / 16777216.0f;
}

// The attacker nearest to us and the screen direction toward it (the ship's own axes: right / up).
static void FindAttacker()
{
    const u32 me = space::PlayerShipId();
    float best = 1e30f;
    space::Contact found;
    for (const auto& c : space::Contacts())
        if (c.ship && c.id != me && (c.targetsPlayer || c.attitude < -0.5f) && c.distance < best)
            best = c.distance, found = c;
    g.attacker = found.id;
    float pos[3], rot[9];
    if (!found.id || !space::ObjectPose(me, pos, rot))
        return;
    const float d[3] = {found.pos[0] - pos[0], found.pos[1] - pos[1], found.pos[2] - pos[2]};
    // rows of a 3x3: right = column 0, up = column 1, forward = -column 2
    const float x = d[0] * rot[0] + d[1] * rot[3] + d[2] * rot[6];
    const float y = -(d[0] * rot[1] + d[1] * rot[4] + d[2] * rot[7]);
    const float n = std::sqrt(x * x + y * y);
    if (n > 1e-3f)
        g.dir[0] = x / n, g.dir[1] = y / n;
}

// The round that just hit (space::TrackRounds, within 0.3 s): its munition, and the edge it came from (opposite its
// flight direction, in the ship's right / up axes). 0 when none: the nearest attacker stands in.
static u32 RoundSource()
{
    u32 munition = 0, owner = 0;
    float dir[3];
    double when = 0;
    if (!space::LastRoundHit(munition, owner, dir, when))
        return 0;
    static LARGE_INTEGER freq = {};
    if (!freq.QuadPart)
        QueryPerformanceFrequency(&freq);
    LARGE_INTEGER qpc;
    QueryPerformanceCounter(&qpc);
    if (static_cast<double>(qpc.QuadPart) / static_cast<double>(freq.QuadPart) - when > 0.3)
        return 0;
    float pos[3], rot[9];
    if (space::ObjectPose(space::PlayerShipId(), pos, rot))
    {
        const float x = -(dir[0] * rot[0] + dir[1] * rot[3] + dir[2] * rot[6]);
        const float y = dir[0] * rot[1] + dir[1] * rot[4] + dir[2] * rot[7]; // source above = negative screen y
        const float n = std::sqrt(x * x + y * y);
        if (n > 1e-3f)
            g.dir[0] = x / n, g.dir[1] = y / n;
    }
    g.attacker = owner;
    return munition;
}

// The game's own hit point on the player ship (host v12: the inputs of the HUD damage indicator - the impact blast of
// a round or missile, or a contact point) as a screen direction: the point relative to the ship in its right / up axes.
// The indicator reduces it to 4 arcs (none for sources ahead); the cue keeps the exact side, on screen too.
static bool GameHitDir()
{
    float hp[3];
    u32 tick = 0;
    const u32 seq = HostPlayerHit(hp, nullptr, &tick);
    if (!seq || seq == g.hitSeq)
        return false;
    g.hitSeq = seq;
    if (GetTickCount() - tick > 500) // cue groups gather hits for up to ~0.33 s
        return false;
    float pos[3], rot[9];
    if (!space::ObjectPose(space::PlayerShipId(), pos, rot))
        return false;
    const float d[3] = {hp[0] - pos[0], hp[1] - pos[1], hp[2] - pos[2]};
    const float x = d[0] * rot[0] + d[1] * rot[3] + d[2] * rot[6];
    const float y = -(d[0] * rot[1] + d[1] * rot[4] + d[2] * rot[7]);
    const float n = std::sqrt(x * x + y * y);
    if (n < 1e-3f)
        return false; // straight ahead / behind: keep the last direction
    g.dir[0] = x / n, g.dir[1] = y / n;
    ++g.gameDirs;
    return true;
}

// Where the hit came from and with what: the munition (given, or the tracked round that hit); the direction from the
// game's hit point, else the round's flight, else the nearest attacker.
static u32 Source(u32 munition)
{
    if (g.testDir)
        return munition;
    if (!munition)
        munition = RoundSource();
    const bool game = GameHitDir();
    if (!munition)
    {
        const float d[2] = {g.dir[0], g.dir[1]};
        FindAttacker(); // the weapon's tint
        if (game)
            g.dir[0] = d[0], g.dir[1] = d[1];
    }
    return munition;
}

// The incoming weapon's impact look: the munition given, else the attacker's first gun's munition (cached per attacker).
static const hitlook::Look* AttackerImpact(u32 munition)
{
    if (munition)
        return hitlook::Impact(munition);
    static u32 cachedFor = 0;
    static const hitlook::Look* cached = nullptr;
    if (g.attacker && g.attacker != cachedFor)
    {
        cachedFor = g.attacker;
        cached = nullptr;
        for (const auto& w : space::ShipWeapons(g.attacker))
            if (!w.homing && w.ammoArch)
                if ((cached = hitlook::Impact(w.ammoArch)) != nullptr)
                    break;
    }
    return g.attacker ? cached : nullptr;
}

static void ColorOf(const hitlook::Look* l, float t, float out[3])
{
    out[0] = out[1] = out[2] = 1;
    if (l && !l->layers.empty())
        hitlook::EvalColor(l->layers.front().color, t, out[0], out[1], out[2]);
}

// Brightness of an edge point (direction dx, dy from the screen centre, unit) for a hit from g.dir: 1 on the hit side,
// `rest` on the far side, smoothly in between.
static float Toward(float dx, float dy, float hx, float hy, float rest)
{
    const float f = 0.5f * (1.0f + dx * hx + dy * hy);
    return rest + (1.0f - rest) * f * f;
}

// Shield failure: rings all around, brighter toward the hit; hits: groups at the hit side.
constexpr float kBreakRest = 0.55f; // relative brightness of the failure flash away from the hit side
constexpr float kHitRest = 0.4f;    // of a hit's edge glow

static void Spawn(const hitlook::Look* look, float strength, const float tint[3], float tintMix, bool around)
{
    if (!look)
        return;
    const int groups = around ? 10 : 1 + static_cast<int>(std::min(1.0f, strength));
    for (int k = 0; k < groups; ++k)
    {
        float dx = g.dir[0], dy = g.dir[1], s = strength;
        if (around)
        {
            const float a = (k + Rand() * 0.5f) / groups * 6.2831853f;
            dx = std::cos(a), dy = std::sin(a);
            s = strength * Toward(dx, dy, g.dir[0], g.dir[1], kBreakRest);
        }
        else if (k > 0) // spread around the hit direction
        {
            const float a = std::atan2(dy, dx) + (Rand() - 0.5f) * 1.2f;
            dx = std::cos(a), dy = std::sin(a);
        }
        for (const auto& layer : look->layers)
        {
            Particle p;
            p.layer = &layer;
            p.dx = dx, p.dy = dy;
            p.along = (Rand() - 0.5f) * 0.25f;
            p.angle = Rand() * 6.2831853f;
            // the hotspot flares live 2 s in the game (on the ship); on the screen edge they would linger as a wall
            p.life = std::clamp(layer.life > 0 ? layer.life : 0.6f, 0.35f, layer.life > 1.5f ? 0.6f : 1.0f);
            p.strength = s;
            p.tint[0] = tint[0], p.tint[1] = tint[1], p.tint[2] = tint[2];
            p.tintMix = tintMix;
            g_parts.push_back(p);
        }
    }
    if (g_parts.size() > 48)
        g_parts.erase(g_parts.begin(), g_parts.begin() + (g_parts.size() - 48));
}

static void Hit(float damage, float shieldMax, const hitlook::Look* shieldLook, u32 munition)
{
    munition = Source(munition);
    const hitlook::Look* impact = AttackerImpact(munition);
    float tint[3];
    ColorOf(impact, 0.2f, tint);
    g.burst += damage / std::max(1.0f, shieldMax);
    const float strength = std::clamp(0.35f + g.burst * 6.0f, 0.35f, 1.5f);
    Spawn(shieldLook, strength, tint, impact ? 0.35f : 0.0f, false);
    // a soft edge glow all around in the shield's colour, brighter on the hit side (user, 19 Oct 2026)
    Flash f;
    ColorOf(shieldLook, 0.3f, &f.r);
    f.r += (tint[0] - f.r) * 0.35f, f.g += (tint[1] - f.g) * 0.35f, f.b += (tint[2] - f.b) * 0.35f;
    f.life = 0.4f, f.strength = 0.25f * std::min(strength, 1.2f);
    f.dx = g.dir[0], f.dy = g.dir[1], f.rest = kHitRest;
    g_flashes.push_back(f);
    ++g.hits;
}

static void Break(const hitlook::Look* shieldLook)
{
    Source(0);
    const float none[3] = {1, 1, 1};
    Spawn(shieldLook, 1.4f, none, 0, true);
    Flash f;
    ColorOf(shieldLook, 0.5f, &f.r);
    f.life = 0.45f, f.strength = 1;
    f.dx = g.dir[0], f.dy = g.dir[1], f.rest = kBreakRest;
    g_flashes.push_back(f);
    Flash white;
    white.life = 0.12f, white.strength = 0.8f;
    white.dx = g.dir[0], white.dy = g.dir[1], white.rest = 0.75f;
    g_flashes.push_back(white);
    ++g.breaks;
}

// A hull hit: the ship's own hull hit look (shiparch surface_hit_effects; the stock ships' white ring + flare), tinted
// toward the weapon like the shield's, and a patterned edge glow (the hull pattern: never a shield) that turns from
// hot white to red and grows as the hull goes (hullFrac 1 -> 0); the weapon's impact look when the ship has none.
static void HullHit(float damage, float hullMax, float hullFrac, u32 munition, u32 shipArch)
{
    munition = Source(munition);
    const hitlook::Look* impact = AttackerImpact(munition);
    const hitlook::Look* hullLook = g_set.hull ? hitlook::Hull(shipArch, damage) : nullptr;
    const float strength = std::clamp(0.4f + damage / std::max(1.0f, hullMax) * 8, 0.4f, 1.2f);
    float tint[3] = {1, 1, 1};
    if (hullLook)
    {
        ColorOf(impact, 0.2f, tint);
        Spawn(hullLook, strength, tint, impact ? 0.35f : 0.0f, false);
        const float lost = std::clamp(1.0f - hullFrac, 0.0f, 1.0f), m = std::sqrt(lost);
        const float hot[3] = {1.0f, 0.82f, 0.6f}, red[3] = {1.0f, 0.12f, 0.06f};
        Flash f;
        float* c = &f.r;
        for (int i = 0; i < 3; ++i)
            c[i] = hot[i] + (red[i] - hot[i]) * m, c[i] += (tint[i] - c[i]) * 0.2f;
        f.life = 0.4f, f.strength = (0.3f + 0.4f * lost) * std::min(strength, 1.2f);
        f.dx = g.dir[0], f.dy = g.dir[1], f.rest = kHitRest;
        f.hull = true;
        g_flashes.push_back(f);
    }
    else
        Spawn(impact, strength, tint, 0, false);
    ++g.hullHits;
}

// A damage stage's glow colour: its first coloured additive layer (the fire), else a warm orange.
static void StageColor(const hitlook::DamageStage& st, float out[3])
{
    out[0] = 0.9f, out[1] = 0.45f, out[2] = 0.15f;
    for (const auto* l : st.looks)
        for (const auto& layer : l->layers)
            if (layer.additive && !layer.color.empty() && layer.color.front().g < 0.9f)
            {
                out[0] = layer.color.front().r, out[1] = layer.color.front().g, out[2] = layer.color.front().b;
                return;
            }
}

// The game's own "hull critical" point: the relative hull below which it plays the Neural Net's "HULL BREACH
// IMMINENT" (exe::HullCriticalFrac, 0.2).
static float HullCritical()
{
    const float f = exe::HullCriticalFrac.ok() ? Mem<float>(exe::HullCriticalFrac) : 0.2f;
    return f > 0 && f < 1 ? f : 0.2f;
}

// The hull's damage edge. Stages = the fuses the game lights below their hit points (sparks / fire / smoke): a steady
// trickle of their effects along the screen edges, mostly the lower half, denser with every stage. Under it a glow and
// the hull pattern (user, 19 Oct 2026: unmistakably hull, redder / brighter as the hull goes): their severity grows
// continuously from the first fuse's hit points down to zero, the colour from the stage's fire toward red, and from
// the game's hull-critical point (its "HULL BREACH IMMINENT") they breathe slowly (0.5 Hz). No flashing.
static void DamageEdge(float dt, float hullPts, float hullMax, u32 shipArch)
{
    const auto& stages = hitlook::DamageStages(shipArch);
    g.stages = static_cast<int>(stages.size());
    g.stage = 0;
    for (const auto& st : stages)
        if (hullPts < st.hitPts)
            ++g.stage;
    const float first = stages.empty() ? hullMax * 0.25f : stages.front().hitPts;
    g.critical = hullPts < HullCritical() * hullMax;
    const bool on = g_set.hull && (g.stage > 0 || g.critical);
    g.sev = on ? std::clamp(1.0f - hullPts / std::max(1.0f, first), 0.0f, 1.0f) : 0.0f;
    g.pulse += dt;
    float target = on ? 0.15f + 0.3f * g.sev * g.sev : 0.0f;
    if (on && g.critical)
        target *= 0.75f + 0.25f * std::sin(g.pulse * 3.1415927f);
    g.glow += (target - g.glow) * std::min(1.0f, dt / 0.5f);
    if (on)
    {
        float base[3] = {0.9f, 0.45f, 0.15f};
        if (g.stage > 0)
            StageColor(stages[g.stage - 1], base);
        const float red[3] = {1.0f, 0.12f, 0.06f}, m = std::sqrt(g.sev);
        for (int i = 0; i < 3; ++i)
            g.glowColor[i] = base[i] + (red[i] - base[i]) * m;
    }
    const float depth = g.stage > 0 && g_set.hull ? static_cast<float>(g.stage) / std::max(1, g.stages) : 0.0f;
    if (depth <= 0)
    {
        g.emit = 0;
        return;
    }
    g.emit += dt * (2.0f + 5.0f * depth); // particle groups per second
    while (g.emit >= 1)
    {
        g.emit -= 1;
        const auto& st = stages[static_cast<size_t>(Rand() * g.stage) % g.stage];
        const hitlook::Look* look = st.looks[static_cast<size_t>(Rand() * st.looks.size()) % st.looks.size()];
        const float a = Rand() < 0.65f ? 0.15f + Rand() * 2.84f : Rand() * 6.2831853f; // lower half mostly
        for (const auto& layer : look->layers)
        {
            Particle p;
            p.layer = &layer;
            p.dx = std::cos(a), p.dy = std::sin(a);
            p.along = (Rand() - 0.5f) * 0.3f;
            p.angle = Rand() * 6.2831853f;
            p.life = std::clamp((layer.life > 0 ? layer.life : 0.5f) * 3.0f, 0.9f, 2.0f);
            p.strength = 1.0f;
            p.sizeK = 0.3f + 0.12f * depth;
            p.alphaK = (layer.additive ? 0.6f : 0.55f) * (0.85f + 0.45f * depth);
            p.soft = true;
            g_ambient.push_back(p);
        }
    }
    if (g_ambient.size() > 48)
        g_ambient.erase(g_ambient.begin(), g_ambient.begin() + (g_ambient.size() - 48));
}

void Update(float dt, bool flying)
{
    for (auto* list : {&g_parts, &g_ambient})
    {
        for (auto& p : *list)
            p.age += dt;
        list->erase(std::remove_if(list->begin(), list->end(), [](const Particle& p) { return p.age >= p.life; }),
                    list->end());
    }
    for (auto& f : g_flashes)
        f.age += dt;
    g_flashes.erase(std::remove_if(g_flashes.begin(), g_flashes.end(), [](const Flash& f) { return f.age >= f.life; }),
                    g_flashes.end());
    g.burst *= std::exp(-dt / 0.5f);

    ai::Self s;
    if (!flying || !g_set.on || !ai::ReadSelf(s) || s.hullMax <= 0)
    {
        g.known = false;
        g.stage = 0;
        g.glow = 0;
        return;
    }
    float cur = 0, max = 0;
    bool up = false;
    if (s.shieldMax > 0 && !space::ShieldOf(space::PlayerShipId(), cur, max, up))
        cur = s.shield * s.shieldMax, max = s.shieldMax, up = s.shield > 0;
    const float hull = s.hull * s.hullMax;
    DamageEdge(dt, g.testHull >= 0 ? g.testHull : hull, s.hullMax, s.shipArch);
    if (g.known)
    {
        const hitlook::Look* look = hitlook::Shield(s.shieldArch, g.pending);
        if (g.up && !up && s.shieldMax > 0)
            Break(look);
        else if (up && cur < g.shield - 0.5f)
            g.pending += g.shield - cur;
        if (hull < g.hull - 0.5f) // shield down, or damage that reaches the hull anyway (collisions)
            g.pendingHull += g.hull - hull;
        // at most ~3 cue groups per second (HUD principle: flashes < 3 Hz): hits in between add to the next group's
        // damage (sustained fire = a steady, stronger edge instead of a white wall of overlapping sprites, 19 Oct 2026)
        g.since += dt;
        if ((g.pending > 0 || g.pendingHull > 0) && g.since >= 0.33f)
        {
            if (g.pending > 0)
                Hit(g.pending, max, hitlook::Shield(s.shieldArch, g.pending), 0);
            if (g.pendingHull > 0)
                HullHit(g.pendingHull, s.hullMax, s.hull, 0, s.shipArch);
            g.pending = g.pendingHull = 0;
            g.since = 0;
        }
    }
    g.known = true;
    g.up = up;
    g.shield = cur;
    g.hull = hull;
}

// ---------------------------------------------------------------- drawing
static std::unordered_map<std::string, render::Image> g_images; // by texture name

static const render::Image* ImageOf(const hitlook::Layer& l, void* device)
{
    auto it = g_images.find(l.tex);
    if (it != g_images.end() && it->second.device == device)
        return it->second.texture ? &it->second : nullptr;
    render::Image& img = g_images[l.tex];
    img = render::Image();
    int w = 0, h = 0;
    std::vector<u32> px;
    if (hitlook::Texture(l, w, h, px))
        render::BuildImage(img, px, w, h);
    img.device = device; // also marks a failed load as tried on this device
    return img.texture ? &img : nullptr;
}

static u32 Argb(float a, float r, float gg, float b)
{
    auto c = [](float v) { return static_cast<u32>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f); };
    return (c(a) << 24) | (c(r) << 16) | (c(gg) << 8) | c(b);
}

static void DrawParticle(const Particle& p, void* device, float W, float H, float k);

// ---------------------------------------------------------------- the hull pattern
// Hull damage gets a pattern of its own so it never reads as a shield hit (user, 19 Oct 2026): hexagons with bold
// outlines (the user's choice, default) or armour-plate seams with rivets (option). The Pirate shields' own hit look is
// also hexagons, as sprites at the hit; the hull's is a tiled lattice along the whole edge.
// A 256 x 64 strip that tiles along the edge; row 63 lies on the screen edge, the pattern fades toward the centre.
constexpr int kPatW = 256, kPatH = 64;

static float Ramp(float d, float w) { return std::clamp(1.0f - d / w, 0.0f, 1.0f); }

static void PatternPixels(int kind, std::vector<u32>& px)
{
    px.assign(kPatW * kPatH, 0);
    const float s3 = 1.7320508f;
    for (int y = 0; y < kPatH; ++y)
        for (int x = 0; x < kPatW; ++x)
        {
            const float px_ = x + 0.5f, py = y + 0.5f, fall = std::pow(py / kPatH, kind == 2 ? 1.2f : 1.6f);
            float a = 0;
            if (kind == 2) // hexagons, pointy-top, 14 across the strip
            {
                const float sx = kPatW / 14.0f, r = sx / s3, sy = 1.5f * r, ri = sx * 0.5f;
                float edge = 1e9f;
                const int row0 = static_cast<int>(py / sy);
                for (int row = row0 - 1; row <= row0 + 1; ++row)
                {
                    const float cy = row * sy, ox = (row & 1) ? sx * 0.5f : 0.0f;
                    const int col0 = static_cast<int>(std::floor((px_ - ox) / sx + 0.5f));
                    for (int col = col0 - 1; col <= col0 + 1; ++col)
                    {
                        const float dx = px_ - (col * sx + ox), dy = py - cy;
                        const float m = std::max(
                            {std::abs(dx), std::abs(0.5f * dx + 0.866f * dy), std::abs(-0.5f * dx + 0.866f * dy)});
                        if (m <= ri)
                            edge = std::min(edge, ri - m);
                    }
                }
                // bold outlines (user, 19 Oct 2026: more pronounced): a solid 1.2 px core, soft 1.6 px shoulders, a
                // faint fill so the lines carry the pattern
                a = std::max(0.08f, edge < 1.2f ? 1.0f : Ramp(edge - 1.2f, 1.6f));
            }
            else // armour plates: rows of 16 px, plates 64 px wide, every other row offset by half a plate
            {
                const float rowH = 16, plateW = 64;
                const int row = static_cast<int>(py / rowH);
                const float fy = std::fmod(py, rowH), fx = std::fmod(px_ + ((row & 1) ? plateW * 0.5f : 0.0f), plateW);
                const float seam = Ramp(std::min(std::min(fy, rowH - fy), std::min(fx, plateW - fx)), 1.6f);
                const float rivet =
                    Ramp(std::min(std::hypot(fx - 6, fy - 5), std::hypot(fx - (plateW - 6), fy - 5)) - 1.2f, 1.0f);
                a = std::max({0.18f, seam, rivet * 0.8f});
            }
            px[y * kPatW + x] = (static_cast<u32>(std::clamp(a * fall, 0.0f, 1.0f) * 255.0f + 0.5f) << 24) | 0xFFFFFF;
        }
}

static render::Image g_pattern; // the hull pattern strip on the current device
static int g_patternBuilt = -1; // the pattern kind g_pattern was built for

static const render::Image* PatternImage(void* device)
{
    if (g_set.pattern <= 0)
        return nullptr;
    if (g_patternBuilt != g_set.pattern || g_pattern.device != device)
    {
        g_pattern = render::Image();
        std::vector<u32> px;
        PatternPixels(g_set.pattern, px);
        render::BuildImage(g_pattern, px, kPatW, kPatH);
        g_pattern.device = device;
        g_patternBuilt = g_set.pattern;
    }
    return g_pattern.texture ? &g_pattern : nullptr;
}

// Logic shutdown (hot reload): the textures built by this logic's render.cpp are released with it; the next Render
// builds them again on demand.
void Shutdown()
{
    for (auto& kv : g_images)
        render::FreeImage(kv.second);
    g_images.clear();
    render::FreeImage(g_pattern);
    g_patternBuilt = -1;
}

// The pattern along the four edges, `e` pixels deep, alpha `a` toward (hdx, hdy) and `rest` away (per corner: smooth).
static void PatternBand(void* device, float W, float H, float e, float a, const float rgb[3], float hdx, float hdy,
                        float rest)
{
    const render::Image* img = PatternImage(device);
    if (!img || a < 0.004f)
        return;
    const float cx = W * 0.5f, cy = H * 0.5f, L = e * kPatW / kPatH, step = L / 8;
    auto col = [&](float x, float y)
    {
        const float dx = (x - cx) / cx, dy = (y - cy) / cy, n = std::sqrt(dx * dx + dy * dy);
        return Argb(a * (n > 1e-4f ? Toward(dx / n, dy / n, hdx, hdy, rest) : 1.0f), rgb[0], rgb[1], rgb[2]);
    };
    // edge: 0 bottom, 1 top, 2 left, 3 right; along s: x for top / bottom, y for left / right
    for (int edge = 0; edge < 4; ++edge)
    {
        const float len = edge < 2 ? W : H;
        for (float s0 = 0; s0 < len; s0 += step)
        {
            const float s1 = std::min(len, s0 + step);
            const float u0 = std::fmod(s0, L) / L, u1 = u0 + (s1 - s0) / L;
            float o0[2], o1[2], i0[2], i1[2]; // outer (on the edge) and inner points
            switch (edge)
            {
            case 0:
                o0[0] = s0, o0[1] = H, o1[0] = s1, o1[1] = H, i0[0] = s0, i0[1] = H - e, i1[0] = s1, i1[1] = H - e;
                break;
            case 1: o0[0] = s0, o0[1] = 0, o1[0] = s1, o1[1] = 0, i0[0] = s0, i0[1] = e, i1[0] = s1, i1[1] = e; break;
            case 2: o0[0] = 0, o0[1] = s0, o1[0] = 0, o1[1] = s1, i0[0] = e, i0[1] = s0, i1[0] = e, i1[1] = s1; break;
            default:
                o0[0] = W, o0[1] = s0, o1[0] = W, o1[1] = s1, i0[0] = W - e, i0[1] = s0, i1[0] = W - e, i1[1] = s1;
                break;
            }
            const float xy[8] = {i0[0], i0[1], i1[0], i1[1], o1[0], o1[1], o0[0], o0[1]};
            const float uv[8] = {u0, 0, u1, 0, u1, 1, u0, 1};
            const u32 c0 = col(o0[0], o0[1]), c1 = col(o1[0], o1[1]);
            const u32 argb[4] = {c0, c1, c1, c0};
            render::Quad(*img, xy, uv, argb, true);
        }
    }
}

// A soft gradient band `e` pixels deep along the four screen edges, in 12 segments per edge whose alpha follows the
// direction (dx, dy): 1 toward it, `rest` away from it.
static void EdgeBand(float W, float H, float e, float a, const float rgb[3], float hdx, float hdy, float rest)
{
    constexpr int kSeg = 12;
    const float cx = W * 0.5f, cy = H * 0.5f;
    const u32 z = Argb(0, rgb[0], rgb[1], rgb[2]);
    auto at = [&](float x, float y)
    {
        const float dx = (x - cx) / cx, dy = (y - cy) / cy, n = std::sqrt(dx * dx + dy * dy);
        const float w = n > 1e-4f ? Toward(dx / n, dy / n, hdx, hdy, rest) : 1.0f;
        return Argb(a * w, rgb[0], rgb[1], rgb[2]);
    };
    for (int i = 0; i < kSeg; ++i)
    {
        const float x0 = W * i / kSeg, x1 = W * (i + 1) / kSeg, y0 = H * i / kSeg, y1 = H * (i + 1) / kSeg;
        render::Gradient(x0, 0, x1 - x0, e, at(x0, 0), at(x1, 0), z, z);     // top
        render::Gradient(x0, H - e, x1 - x0, e, z, z, at(x1, H), at(x0, H)); // bottom
        render::Gradient(0, y0, e, y1 - y0, at(0, y0), z, z, at(0, y1));     // left
        render::Gradient(W - e, y0, e, y1 - y0, z, at(W, y0), at(W, y1), z); // right
    }
}
static std::string g_lastEdge; // diagnostics: the last damage-edge sprite drawn

void Render(void* device)
{
    if (!g_set.on || (g_parts.empty() && g_flashes.empty() && g_ambient.empty() && g.glow < 0.005f) || !device)
        return;
    render::SetDevice(device);
    int sw = 0, sh = 0;
    if (!render::ScreenSize(sw, sh) || !render::BeginPlain())
        return;
    const float W = static_cast<float>(sw), H = static_cast<float>(sh);
    const float k = std::clamp(g_set.intensity, 0.0f, 2.0f);
    // the hull's steady glow and pattern: wider, strongest at the bottom
    if (g.glow >= 0.005f)
    {
        EdgeBand(W, H, H * 0.2f, g.glow * k, g.glowColor, 0, 1, 0.45f);
        PatternBand(device, W, H, H * 0.2f, std::min(1.0f, g.glow * 2.0f * k), g.glowColor, 0, 1, 0.45f);
    }
    // hit / failure glows, brightest toward the hit; hull hits patterned
    for (const Flash& f : g_flashes)
    {
        const float t = f.age / f.life, a = f.strength * (1 - t) * (1 - t) * 0.55f * k;
        EdgeBand(W, H, H * 0.14f, a, &f.r, f.dx, f.dy, f.rest);
        if (f.hull)
            PatternBand(device, W, H, H * 0.14f, std::min(1.0f, a * 2.4f), &f.r, f.dx, f.dy, f.rest);
    }
    for (const Particle& p : g_ambient)
        DrawParticle(p, device, W, H, k);
    for (const Particle& p : g_parts)
        DrawParticle(p, device, W, H, k);
    render::End(false);
}

static void DrawParticle(const Particle& p, void* device, float W, float H, float k)
{
    const float cx = W * 0.5f, cy = H * 0.5f;
    const hitlook::Layer& l = *p.layer;
    const render::Image* img = ImageOf(l, device);
    if (!img)
        return;
    const float t = std::clamp(p.age / p.life, 0.0f, 1.0f);
    float r, gg, b;
    hitlook::EvalColor(l.color, t, r, gg, b);
    r += (p.tint[0] - r) * p.tintMix, gg += (p.tint[1] - gg) * p.tintMix, b += (p.tint[2] - b) * p.tintMix;
    float sizeMax = 0;
    for (const auto& key : l.size)
        sizeMax = std::max(sizeMax, key.v);
    const float rel = sizeMax > 0 ? hitlook::Eval(l.size, t, sizeMax) / sizeMax : 1.0f;
    const float size = H * p.sizeK * rel * (0.7f + 0.3f * std::min(p.strength, 1.5f)) * (0.6f + 0.4f * k);
    // additive sprites add up: keep each one well below full brightness. The damage edge fades in and out softly
    // instead of following the effect's own alpha curve (made for 0.3-0.5 s particles: with the fade it stayed ~0.03)
    const float shape = p.soft ? std::sin(3.1415927f * t) : hitlook::Eval(l.alpha, t, 1.0f);
    const float alpha = std::clamp(shape * std::min(p.strength, 1.0f) * p.alphaK * k, 0.0f, 1.0f);
    // the point where the direction leaves the screen rectangle, pushed out so the sprite's core sits at the edge
    const float sx = std::abs(p.dx) > 1e-4f ? (cx / std::abs(p.dx)) : 1e9f;
    const float sy = std::abs(p.dy) > 1e-4f ? (cy / std::abs(p.dy)) : 1e9f;
    const float s = std::min(sx, sy);
    float x = cx + p.dx * s, y = cy + p.dy * s;
    x += -p.dy * p.along * H, y += p.dx * p.along * H; // along the edge
    const float push = p.soft ? -0.12f : 0.12f; // hit sprites' cores sit on the edge, the damage edge's just inside
    x += p.dx * size * push, y += p.dy * size * push;
    render::Sprite(*img, x, y, size, size, p.angle, Argb(alpha, r, gg, b), l.additive);
    if (p.soft)
    {
        std::ostringstream o;
        o << l.tex << " a=" << alpha << " size=" << size << " at=(" << x << "," << y << ")";
        g_lastEdge = o.str();
    }
}

std::string Test(int kind, float damage, u32 munition)
{
    ai::Self s;
    if (!ai::ReadSelf(s))
        return "not flying";
    const hitlook::Look* look = hitlook::Shield(s.shieldArch, damage);
    if (kind == 1)
        Break(look);
    else if (kind == 2)
        HullHit(damage, s.hullMax, g.testHull >= 0 ? g.testHull / std::max(1.0f, s.hullMax) : s.hull, munition,
                s.shipArch);
    else
        Hit(damage, s.shieldMax, look, munition);
    return Status();
}

std::string TestHull(float hullPts)
{
    g.testHull = hullPts;
    return Status();
}

std::string TestDir(bool on, float degrees)
{
    g.testDir = on;
    if (on)
        g.dir[0] = std::cos(degrees * 0.017453293f), g.dir[1] = std::sin(degrees * 0.017453293f);
    return Status();
}

std::string Status()
{
    std::ostringstream o;
    o << "shieldcue on=" << g_set.on << " intensity=" << g_set.intensity << " particles=" << g_parts.size()
      << " flashes=" << g_flashes.size() << " burst=" << g.burst << " hits=" << g.hits << " breaks=" << g.breaks
      << " hull_hits=" << g.hullHits << " attacker=" << g.attacker << " dir=(" << g.dir[0] << "," << g.dir[1] << ")"
      << " shield=" << g.shield << " up=" << g.up << " game_dirs=" << g.gameDirs
      << " game_hits=" << HostPlayerHit(nullptr, nullptr, nullptr) << " hull=" << g.hull << " damage_stage=" << g.stage
      << "/" << g.stages << " edge=" << g_ambient.size() << " glow=" << g.glow << " sev=" << g.sev
      << (g.critical ? " CRITICAL" : "") << " pattern=" << g_set.pattern << (g.testHull >= 0 ? " (test hull)" : "")
      << (g_lastEdge.empty() ? "" : " last_edge: " + g_lastEdge);
    float hp[3] = {};
    u32 sub = 0, tick = 0;
    if (HostPlayerHit(hp, &sub, &tick))
        o << " last_hit=(" << hp[0] << "," << hp[1] << "," << hp[2] << ") kind=" << sub
          << " age_ms=" << (GetTickCount() - tick);
    return o.str();
}
} // namespace shieldcue
