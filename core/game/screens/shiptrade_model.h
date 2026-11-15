// FLvibed - the ship dealer page's model (the game's NN_ShipTrader window; the HUD 2 page framework, audit 70b F3c): what the
// page draws, as a plain value. Filled by shiptrade_screen.cpp from the window it parks (its three ships, their prices and
// refusals, its selection, its trade-in budget: the window does the work, its 3D view stays the game's) and the player's ship;
// drawn by modules\hud\src\pages\dealers\shiptrade_view.cpp. Nothing here calls the game.
#pragma once
#include "common.h"

#include <string>
#include <vector>

namespace shiptradescreen
{
// The page's actions (shiptrade_screen.cpp's table: each the window's own button) and its row's click (the page's own).
enum Action
{
    kActPick = 1,  // a ship chosen (ShipTraderShipNameFrame<n>; arg: its row)
    kActBuy,       // BUY THIS SHIP (ShipTraderBuyButton, while a ship it sells is chosen)
    kActClose,     // CLOSE (ShipTraderCancelButton)
    kHitRow = 100, // a click on a row (arg: its row): the card from its top, the ship chosen
};

// The game's 3D ship stands where the game draws it, in the screen's middle: the page's hole over it, fractions of H across
// the screen's middle line (the view's 0.165 + 0.115 .. 0.42). The wheel over it is the view's (its zoom), anywhere else the card's.
constexpr float kShipHoleHalfW = 0.19f, kShipHoleTop = 0.28f, kShipHoleFoot = 0.585f;

// One ship on sale: its price, why the game would refuse it (1 the level, 2 the standing, 3 the money), its level, its words.
struct Offer
{
    int price = 0, refusal = 0, level = 0;
    std::wstring name, kind;
};

// One number of the chosen ship against the player's (the AGAINST YOUR SHIP column): its word, and when the player's ship
// is known the difference and whether the new one is better.
enum Verdict
{
    kSame = 0,
    kBetter,
    kWorse,
};
struct Stat
{
    const wchar_t* label = L"";
    std::wstring mine, diff; // the chosen ship's value in words (equip_rules ValueWord); "=" or "+n" / "-n"
    int verdict = kSame;
};

struct Model
{
    std::vector<Offer> offers; // the window's ships (read every 0.4 s, out of the window's slide)
    int sel = -1;              // the game's chosen ship (-1 none)
    int focus = 0;             // the pad's side: 0 the list, 1 the ship (the stick turns it, D-pad up / down scroll its card)
    int rsWas = 0;             // the right stick's step direction last frame, and the time to its next repeat
    float rsRepeat = 0;
    u32 rowsDue = 0; // when the ships are read next (GetTickCount; 0 at once)
    // the card: the chosen ship's description, wrapped; the pad's D-pad and the wheel move it; a new ship starts at its top
    u32 cardArch = 0;
    std::wstring cardText;
    mutable int cardFirst = 0, cardLines = 0, cardRoom = 0; // where it stands; what the view laid out (the view's)
    // read right before the view draws, as the page read them while it drew (shiptrade_screen.cpp ReadFrame)
    bool devicePad = false;            // the pad is the device in use (its frame round the ship, TURN)
    long long credits = 0, budget = 0; // the player's cash; the window's budget (cash and the trade-in)
    std::wstring own;                  // the player's ship's name
    bool canBuy = false;               // BUY THIS SHIP may go
    std::wstring need;                 // the chosen ship's refusal against the player's own value (issue 56), "" none
    bool compare = false;              // the player's ship's facts were read: the column shows the differences
    static constexpr int kStats = 9;
    Stat stats[kStats];
    int nStats = 0;

    const Offer* Chosen() const { return sel >= 0 && sel < static_cast<int>(offers.size()) ? &offers[sel] : nullptr; }
};
} // namespace shiptradescreen
