// controller\src\impl\wgroups_rules.h (pure): the pad's group switch goes to the next group that has guns, never to an
// empty one (RT would fire nothing: GitHub issue 4), stays put when only one group has guns, and falls back to plain
// order when the membership is unknown.
#include "test.h"

#include "../controller/src/impl/wgroups_rules.h"

void Test_wgroups()
{
    using wgroups::NextGroup;
    CHECK(NextGroup(0x7, 1) == 2 && NextGroup(0x7, 2) == 3 && NextGroup(0x7, 3) == 1); // all three in turn
    CHECK(NextGroup(0x1, 1) == 1);                                                     // only group 1: stays
    CHECK(NextGroup(0x5, 1) == 3 && NextGroup(0x5, 3) == 1);                           // an empty group 2 skipped
    CHECK(NextGroup(0x2, 1) == 2);                                                     // left an emptied group
    CHECK(NextGroup(0, 1) == 2 && NextGroup(0, 3) == 1);                               // unknown: plain order
    CHECK(NextGroup(0x7, 5) == 1 && NextGroup(0x7, -1) == 1);                          // a group past the three
    CHECK(NextGroup(0x8, 1) == 1);                                                     // only group 4: the pad's three stay
}
