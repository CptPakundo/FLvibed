// The weapon groups' pure rule (tests\wgroups_test.cpp): which group the pad's group switch goes to next. The game has
// six groups; the pad and the HUD work with the first three.
#pragma once

namespace wgroups
{
constexpr int kGroups = 3;

// The group after `current` (1-based) among the first kGroups that have guns (`withGuns`: bit g-1 = group g). A group
// without guns would leave RT firing nothing, so it is skipped; `current` itself comes back when no other group has
// guns. withGuns 0 = the membership is unknown: plain 1, 2, 3 order.
inline int NextGroup(unsigned withGuns, int current)
{
    if (current < 1 || current > kGroups)
        current = kGroups; // (a group the pad does not cycle: start over at the first)
    for (int step = 1; step <= kGroups; ++step)
    {
        const int g = (current - 1 + step) % kGroups + 1;
        if (withGuns == 0 || (withGuns & (1u << (g - 1))))
            return g;
    }
    return current;
}
} // namespace wgroups
