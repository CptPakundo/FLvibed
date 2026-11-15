// A nav-map zone's relation mix (core\game\zonemix.h, issue 61): shares by the game's thresholds, the dominant relation.
#include "../core/game/zonemix.h"

#include "test.h"

#include <cmath>

static bool Near(float a, float b) { return std::fabs(a - b) < 1e-4f; }

void Test_zonemix()
{
    using namespace zonemix;
    // the game's thresholds: -0.6 hostile and 0.6 friendly are inclusive (the filter 0x49CD3B)
    CHECK(RelationOf(-0.6f) == kHostile && RelationOf(-0.59f) == kNeutral && RelationOf(0.59f) == kNeutral && RelationOf(0.6f) == kFriendly);
    // HDE li01.ini Zone_Li01_001_Planet_Li01_01: police 1 + 1 (friendly at SaveC706), bounty hunters 1 and trading houses 4.97 (neutral)
    {
        float acc[3] = {};
        Add(acc, 0.9f, 1), Add(acc, 0.9f, 1), Add(acc, 0.1f, 1), Add(acc, 0.0f, 4.97f);
        const Mix m = Shares(acc);
        CHECK(Near(m.share[kFriendly], 2 / 7.97f) && Near(m.share[kNeutral], 5.97f / 7.97f) && m.share[kHostile] == 0);
        CHECK(Dominant(m) == kNeutral && !Single(m));
    }
    // no weight, or only zero weights: neutral (the game's 0 / 1); one relation only
    {
        float acc[3] = {};
        Add(acc, -1, 0);
        CHECK(Single(Shares(acc)) && Dominant(Shares(acc)) == kNeutral);
        Add(acc, -0.8f, 2);
        CHECK(Single(Shares(acc)) && Dominant(Shares(acc)) == kHostile);
    }
    // friendly over hostile: the larger share dominates; a tie keeps neutral's place only when neutral is among the largest
    {
        float acc[3] = {0.97f, 0, 0.03f};
        CHECK(Dominant(Shares(acc)) == kFriendly);
        float tie[3] = {0.4f, 0.4f, 0.2f};
        CHECK(Dominant(Shares(tie)) == kNeutral);
    }
}
