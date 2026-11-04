// FLvibed - the docked rooms' ring: which hotspots are room buttons and which room the player is in. Header only; unit
// test tests\room_ring_test.cpp; the live part is padbases.cpp RefreshRoom.
//
// GitHub issue 9 (4 Nov 2026): on a station the Trader's and the Equipment dealer's room buttons are behaviour 11 in the
// rooms that share their set (the Deck, the Trader, the Equipment room) and 1 elsewhere. The ring took 1 alone, so it
// shrank to Deck / Bar / ShipDealer there, the Trader room read as the Deck, and the bumpers could not reach Equipment.
#pragma once

namespace roomring
{
inline bool IsRoomButton(unsigned behaviour) { return behaviour == 1 || behaviour == 11; }

// The room the player is in, as an index into the ring (-1: none). `own`: the room the room's own action hotspots name
// (the dealers', the bar's); `launchHere`: a launch button is here, so the room with the ship; `area`: that room;
// `last`: the room last gone to. Those two in this order: a room gone to in another base must not win over the launch
// area of this one.
inline int Current(int own, bool launchHere, int area, int last, int count)
{
    if (own >= 0)
        return own;
    if (launchHere && area >= 0)
        return area;
    if (last >= 0)
        return last;
    if (area >= 0)
        return area;
    return count > 0 ? 0 : -1;
}
} // namespace roomring
