// FLvibed - the pad's held A in the NeuroNet windows and the ship dealer's final stage: when a hold fires, what it does there, and the stops the side keys walk; which
// list of a dealer the right stick moves. Header only; unit test tests\dealer_rules_test.cpp; the live part is padbases.cpp UpdateWindow.
//
// The final stage is NN_Dealer in its ship mode, a dealer window to the pad, with one more control: BUY SHIP (docs\FREELANCER_RE.md "The ship dealer's final stage").
#pragma once

namespace dealerrules
{
constexpr float kHoldSec = 0.6f; // every purchase's hold: a job, a ship, a dealer's row

// One frame of the hold. True on the frame a short press ends: a tap. A hold that fired or was carried in is no tap, however short what is left of it.
inline bool Tick(float& held, bool& fired, bool down, float dt)
{
    const bool tap = !down && held > 0 && held < kHoldSec && !fired;
    held = down ? held + dt : 0;
    if (!down)
        fired = false;
    return tap;
}

// A new screen starts the count again, and a button still down counts for nothing until it is released: the hold that bought a ship at the first stage closes that
// window with A down, and must not run on into BUY SHIP.
inline void NewScreen(float& held, bool& fired, bool down)
{
    held = 0;
    fired = down;
}

inline bool Fires(bool down, bool fired, float held) { return down && !fired && held >= kHoldSec; }

// The prompt's fill: only a hold that can still fire shows one.
inline float Progress(float held, bool fired)
{
    if (fired || held <= 0)
        return 0;
    return held < kHoldSec ? held / kHoldSec : 1;
}

enum class Act
{
    None,
    Transfer, // the row's trade, as at every dealer: the game's own button says TRANSFER here
    BuyShip
};

// What a hold does at a dealer and at the final stage. BUY SHIP needs the focus on it: on a row the hold stays that row's trade, so a hold a player knows from every dealer
// can never spend the ship. The game hides the button while the player cannot pay, and then nothing buys. `onTrade`: the focus on a row or the trade panel; anywhere else
// (a filter) a row selected behind the focus is not what the player points at, and a hold spends nothing.
inline Act HoldAct(bool onBuyShip, bool onTrade, bool buyShipShown, bool transferShown)
{
    if (onBuyShip)
        return buyShipShown ? Act::BuyShip : Act::None;
    return onTrade && transferShown ? Act::Transfer : Act::None;
}

// The side keys' stops, as a ring: from either list BUY SHIP is one press away. A stop that is not there is passed over, so a plain dealer keeps its two sides.
enum class Stop
{
    Yours,
    Dealers,
    BuyShip
};

// The stop after `from` in direction `dir` (below 0 backwards); `from` when there is no other.
inline Stop NextStop(Stop from, int dir, bool yours, bool dealers, bool buyShip)
{
    const bool there[3] = {yours, dealers, buyShip};
    int i = static_cast<int>(from);
    for (int step = 0; step < 2; ++step)
    {
        i = (i + (dir < 0 ? 2 : 1)) % 3;
        if (there[i])
            return static_cast<Stop>(i);
    }
    return from;
}

// Which list of a two-list window the right stick means: the one whose row or bar is under the focus, which the D-pad and the stick's cursor both move. On anything else
// (a filter, the transfer panel, BUY SHIP) the list the focus was last on. `underFocus` 0: no list there.
inline int ListSide(int underFocus, int lastSide) { return underFocus ? underFocus : lastSide; }

// The control of `count` that stands on `side`, -1 none. Never the other side's: each list has its own bar, hidden while the list fits, so a list that fits moves nothing.
inline int OnSide(const int* sides, int count, int side)
{
    for (int i = 0; i < count; ++i)
        if (sides[i] == side)
            return i;
    return -1;
}
} // namespace dealerrules
