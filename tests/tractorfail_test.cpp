// Why the tractor refused an item (core\game\tractorfail.h, GitHub issue 33): only the codes the game plays its failure line for carry a reason, read from the word
// its event keeps them in; a reason without the game's text shows nothing beyond the line.
#include "game/tractorfail.h"

#include "test.h"

using namespace tractorfail;

void Test_tractorfail()
{
    // the two events, each with its word
    CHECK(ReasonOf(kEventRefused, kNanobots, 0) == kNanobots);
    CHECK(ReasonOf(kEventRefused, kBatteries, kAmmo) == kBatteries);
    CHECK(ReasonOf(kEventArmLost, kHold, kAmmo) == kAmmo);
    // no target (1), out of range (2: the game's own line), success and an unknown code: no reason
    for (const int code : {0, 1, 2, 7, -1})
    {
        CHECK(ReasonOf(kEventRefused, code, 0) == kNone);
        CHECK(ReasonOf(kEventArmLost, 0, code) == kNone);
    }
    // any other event of the tractor, whatever its data holds
    for (const u32 event : {0u, 0xAu, 0xBu, 0xCu, 0x16u, 0x17u, 0x18u})
        CHECK(ReasonOf(event, kHold, kHold) == kNone);

    CHECK(TextIds(kNanobots) == 932 && TextIds(kBatteries) == 933 && TextIds(kAmmo) == 975 && TextIds(kHold) == 1841);
    CHECK(TextIds(kNone) == 0 && TextIds(2) == 0 && TextIds(7) == 0);

    CHECK(Fresh(0) && Fresh(kFreshMs) && !Fresh(kFreshMs + 1));

    CHECK(Shout(L"Storage capacity for Nanobots is full.") == L"STORAGE CAPACITY FOR NANOBOTS IS FULL");
    CHECK(Shout(L"") == L"" && Shout(L". ") == L"");
    CHECK(Caption(L"TRACTOR FAILURE", L"Storage capacity for this ammo is full.") == L"TRACTOR FAILURE: STORAGE CAPACITY FOR THIS AMMO IS FULL");
    CHECK(Caption(L"TRACTOR FAILURE", L"") == L"TRACTOR FAILURE");
}
