#pragma once
// A nav-map zone's mix of relations (issue 61), host-free. The game colours each political / patrol zone by the AVERAGE of its listed factions'
// feelings towards the player (exe::NavMapZoneBuild: sum(weight x feeling) / sum(weight), thresholds -0.6 / 0.6), so a zone listing police (friendly)
// and a dozen trading houses (neutral) comes out neutral white. HUD 2 sums each listed faction's weight to the relation the player has with it, by the
// game's own thresholds, and colours the zone by the dominant relation (the patrol layer's routes; the political layer's fill until HUD 2's influence
// cells stand in for it: core\game\influence.h).
namespace zonemix
{
enum Relation : int
{
    kFriendly = 0,
    kNeutral = 1,
    kHostile = 2
};
constexpr float kHostileAt = -0.6f; // feeling <= this: hostile (the build's filter 0x49CD3B, the palette 0x45A650; [0x5D2E2C] = -0.6)
constexpr float kFriendlyAt = 0.6f; // feeling >= this: friendly ([0x5D2E30] = 0.6)

inline int RelationOf(float feeling) { return feeling <= kHostileAt ? kHostile : feeling >= kFriendlyAt ? kFriendly : kNeutral; }

struct Mix
{
    float share[3] = {0, 1, 0}; // friendly, neutral, hostile; sums to 1 (a zone with no listed weight: neutral, as the game's 0 / 1)
};

// One listed faction: its weight and the player's feeling towards it (the game skips a faction it cannot read; so does the caller).
inline void Add(float acc[3], float feeling, float weight)
{
    if (weight > 0)
        acc[RelationOf(feeling)] += weight;
}

inline Mix Shares(const float acc[3])
{
    Mix m;
    const float sum = acc[0] + acc[1] + acc[2];
    if (sum > 0)
        for (int i = 0; i < 3; ++i)
            m.share[i] = acc[i] / sum;
    return m;
}

inline int Dominant(const Mix& m)
{
    int d = kNeutral;
    for (int i = 0; i < 3; ++i)
        if (m.share[i] > m.share[d])
            d = i;
    return d;
}

constexpr float kSingle = 0.999f; // a share this large: the zone is of one relation
inline bool Single(const Mix& m) { return m.share[Dominant(m)] >= kSingle; }
} // namespace zonemix
