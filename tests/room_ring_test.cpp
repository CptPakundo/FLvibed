// The docked rooms' ring (controller\src\impl\room_ring.h, GitHub issue 9).
#include "../controller/src/impl/room_ring.h"

#include "test.h"

void Test_room_ring()
{
    using namespace roomring;
    // the room buttons: both kinds; the rooms' actions are not (read on Li01_12: news 2, jobs 3, trader 5, front desk 6,
    // ship dealer 7, equipment 8 and 9, repair 12; launch 0)
    CHECK(IsRoomButton(1) && IsRoomButton(11));
    for (unsigned b : {0u, 2u, 3u, 5u, 6u, 7u, 8u, 9u, 12u})
        CHECK(!IsRoomButton(b));
    // Deck 0, Bar 1, Trader 2, Equipment 3, ShipDealer 4
    CHECK(Current(2, false, 0, -1, 5) == 2);  // a dealer's room names itself
    CHECK(Current(2, false, 0, 4, 5) == 2);   // ...over the room last gone to
    CHECK(Current(-1, true, 0, 2, 5) == 0);   // the launch button: a stale room from another base does not win
    CHECK(Current(-1, false, 0, 3, 5) == 3);  // nothing names the room: the last one gone to
    CHECK(Current(-1, false, 0, -1, 5) == 0); // ...else the launch area
    CHECK(Current(-1, false, -1, -1, 3) == 0);
    CHECK(Current(-1, false, -1, -1, 0) == -1);
}
