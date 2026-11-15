// The NeuroNet pages' rules, pure (docs\NEURONET_PLAN.md): the game's five standings, the filter of the reputation list, a list's window, a number out of the game's text
// (the first two, a list's window and the number are core's now, named here), the item meshes of a list window.
// Host-free: tests\nnpage_test.cpp holds them.
#pragma once
#include "game/screens/prices_rules.h"
#include "game/screens/screen_rules.h"
#include "game/screens/status_rules.h"

#include <algorithm>
#include <string>
#include <vector>

namespace nnrules
{
// The five standings, the reputation list's filter, the share of the next level and a heading without its colon are the
// STATUS page's model's (core\game\screens\status_rules.h: audit 70b F4b); their names here as before.
using statusrules::Band;
using statusrules::BandOf;
using statusrules::Hostile;
using statusrules::Unfriendly;
using statusrules::Neutral;
using statusrules::Warm;
using statusrules::Friendly;
using statusrules::Filter;
using statusrules::Passes;
using statusrules::All;
using statusrules::Enemies;
using statusrules::Indifferent;
using statusrules::Friends;
using statusrules::kFilters;
using statusrules::Share;
using statusrules::Heading;

// A list's window, a number out of the game's text, the commodity list by base's rows and answers: the screens' models need
// them, so they are core's (core\game\screens\screen_rules.h, prices_rules.h: audit 70b F3a); their names here as before.
using screens::List;
using screens::Number;
using screens::Pick;
using screens::Picks;
using screens::Quote;
using screens::SortQuotes;

// The five meshes and the best start of the game's window over the shown rows are core's (core\game\screens\screen_rules.h,
// audit 70b F3b: the dealer's screen moves the window); their names here as before.
using screens::kMeshes;
using screens::BestWindowStart;

// Which of a list window's five item meshes shows each of the page's `n` shown rows (nnpage_kit.h RowIcons). The game's mesh k shows its row
// first + k; a shown row inside that window keeps its own mesh. A row outside it that has an item (`wants`) borrows a mesh no shown row keeps:
// first one that holds the row's model already (`holds(row, mesh)`: no load), else the lowest free. meshOf -1: no icon (an EMPTY slot, no item,
// no mesh left); borrowed: the mesh is sent the row's item;
// ... and among the rest one that `keeps(mesh)` (its own row's model is one the game sends: the lent model can stay under that name, no load a
// frame), before one whose own row the game blanks every frame.
template <class Holds, class Keeps>
inline void AssignMeshes(int first, const int* gameRow, const bool* wants, int n, Holds holds, Keeps keeps, int* meshOf, bool* borrowed)
{
    bool taken[kMeshes] = {};
    for (int i = 0; i < n; ++i)
    {
        const int k = gameRow[i] - first;
        meshOf[i] = gameRow[i] >= 0 && k >= 0 && k < kMeshes ? k : -1;
        borrowed[i] = false;
        if (meshOf[i] >= 0)
            taken[k] = true;
    }
    for (int i = 0; i < n; ++i)
    {
        if (meshOf[i] >= 0 || gameRow[i] < 0 || !wants[i])
            continue;
        for (int pass = 0; pass < 3 && meshOf[i] < 0; ++pass)
            for (int k = 0; k < kMeshes && meshOf[i] < 0; ++k)
                if (!taken[k] && (pass == 2 || (pass == 0 ? holds(i, k) : keeps(k))))
                    meshOf[i] = k;
        if (meshOf[i] >= 0)
            taken[meshOf[i]] = true, borrowed[i] = true;
    }
}
template <class Holds>
inline void AssignMeshes(int first, const int* gameRow, const bool* wants, int n, Holds holds, int* meshOf, bool* borrowed)
{
    AssignMeshes(first, gameRow, wants, n, holds, [](int) { return false; }, meshOf, borrowed);
}
} // namespace nnrules
