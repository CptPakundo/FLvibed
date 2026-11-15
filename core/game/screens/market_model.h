// FLvibed - the dealer page's model (the game's NN_Dealer with its partner NN_Inventory, in its three modes: the commodity
// trader, the equipment dealer, the ship dealer's final stage; the HUD 2 page framework, audit 70b F3b): what the page draws,
// as a plain value. Filled by market_screen.cpp from the two windows it parks (the windows do the work: their rows, their
// selection, their transfer panel, their buttons) and the ship; drawn by modules\hud\src\pages\dealers\market_view.cpp.
// Nothing here calls the game.
#pragma once
#include "common.h"
#include "compare_model.h"
#include "screen_rules.h"
#include "game/equip_rules.h"

#include <string>
#include <vector>

namespace marketscreen
{
// NN_Dealer's kind (+0x978, docs\research\neuronet-windows-facts.md): which of its three pages it is.
enum Mode
{
    kNone = 0,
    kCommodity = 1,
    kFinal = 2, // the ship dealer's final stage: the new ship's items left, what goes to the dealer right
    kEquipment = 3,
};
// The two lists: the player's ship (NN_Inventory) on the left, the dealer's (NN_Dealer) on the right.
enum Side
{
    kYours = 0,
    kTheirs = 1,
};
// The game's verdict on a good's price (baseinfo::Deal's values).
enum Deal
{
    kDealUnknown = 0,
    kDealGood,
    kDealFair,
    kDealHigh,
    kDealLow,
};

// The page's actions (market_screen.cpp's table: each the windows' own button or handler) and its clicks that need the model
// (the page's own: market_page.cpp Hit).
enum Action
{
    kActSelect = 1, // the game's row selected (arg: side * 1000 + the game's row): its transfer panel follows
    kActTrade,      // BUY / SELL / SELL TO DEALER / TAKE BACK (BuySellTradeButton; arg: side * 1000 + the game's row in focus)
    kActLess,       // the quantity one less (QuantitySlider)
    kActMore,       // one more
    kActMax,        // as many as can go
    kActMount,      // the item mounted (NN_Inventory's own mount; arg: its EquipDesc)
    kActUnmount,    // ... taken off its hardpoint
    kActTag,        // the item the store compares with, or none again (arg: its archetype)
    kActFilter,     // one of the windows' filters (arg: 0..4)
    kActRoutes,     // BEST PRICES (TradeRouteButton)
    kActFinalBuy,   // BUY SHIP (ContinueButton)
    kActFinalBack,  // BACK TO SHIPS (BackToShipDealer)
    kActClose,      // CLOSE (Inventory_Close)
    kHitRow = 100,  // a click on a row (arg: side * 1000 + its index): the focus there
    kHitTab,        // a filter's tab (arg: its index)
    kHitTrade,      // the trade's chip
    kHitMount,      // MOUNT / UNMOUNT
    kHitTag,        // the compare tag's chip
};

// One row of a list: an item the window answers for its row, or an EMPTY hardpoint (hp >= 0; equipment, your side).
struct Row
{
    u32 arch = 0, desc = 0;
    int row = -1, count = 1, hp = -1; // the game's row (-1 none), how many, the free hardpoint's index (Model::free)
    u16 id = 0;
    bool mounted = false;
    float health = 1;
    int deal = kDealUnknown; // commodity: the game's verdict on its price
    std::wstring name, slot;
    bool noSlot = false; // the dealer's item no hardpoint of the ship takes
    // read at the view's moment (market_screen.cpp ReadFrame)
    // the game's own number on its row of five (ItemPriceNN), kept from the last time the row stood in the game's five (issue 93: the page orders
    // your rows by hardpoint, so a shown row can lie outside the game's five; the window is steered over it until it is read: market_screen.cpp Read)
    std::wstring price;
    bool priceRead = false;  // that number was read
    bool priceFresh = false; // read since the rows were read with this row as it is now (else the window is steered over it)
    std::wstring need;      // the dealer's item: what it asks that the player does not have ("NEEDS LEVEL 9"), "" nothing
    bool forMounted = false; // ammunition a mounted launcher fires
};

struct Model
{
    int mode = kNone;
    u32 dealer = 0, inv = 0; // the two windows (their handles: the runtime's icons follow them)
    std::vector<Row> rows[2];
    mutable screens::List list[2]; // each list's window on its rows: the focus is the page's, the five rows the view's
    int side = kTheirs;            // the list in focus
    bool buyStop = false;          // the final stage's middle stop: the focus on BUY SHIP
    std::vector<equiprules::Hp> free; // the ship's empty hardpoints of the filter, for the EMPTY rows
    // the card: the game's description of the item in focus, wrapped; RS and the wheel move it
    u32 cardKey = 0;
    std::wstring cardText;
    mutable int cardFirst = 0, cardLines = 0, cardRoom = 0;
    float cardStep = 0;
    mutable float wheelSide = -1; // the list the mouse stands on (its index), -1 the card, -2 neither (the view's)
    u32 rowsDue = 0;              // when the rows are read next (GetTickCount; 0 at once)
    u32 tradeDue = 0;             // an action's "read soon": the rows read at once when it comes (0 none)
    bool wasBusy = false;         // the window manager slid last frame
    int priceFirst[2] = {-1, -1}; // each game window's first row at the last ReadFrame: its prices are read only once it has stood a frame (the game refills them in its update)

    // read each frame at the view's moment, as the page read them while it drew (market_screen.cpp ReadFrame)
    std::wstring filterWord[5]; // the windows' filters in the game's words
    int filterNow = 0;
    bool cargoRead = false;
    int used = 0, cap = 0;
    int guns = 0;
    float hull = 0, shield = 0, energy = 0; // the mounted guns' damage a second (equipment)
    u32 tagged = 0;                         // the item the store compares with (gamedata::TaggedCompare)
    // the focus's column
    bool selected = false, canTrade = false, slider = false;
    std::wstring priceText, totalText; // the transfer panel's TransferPriceText, QuantityTotalText
    int qty = 1, most = 1;
    std::wstring errorHelp;        // the game's refusal (ErrorHelp)
    std::wstring need, yours;      // the need against the player's own value (issue 56)
    bool canTag = false, tagOn = false;
    bool transferMesh = false;     // the game's TransferItemMesh stands
    bool compare = false;          // the compare block stands (equipment) rather than the good's facts
    screens::CompareBlock block;
    float volume = -1, basePrice = -1;
    std::vector<std::wstring> fits; // an EMPTY hardpoint: what in the hold takes it
    // the final stage's deal
    long long shipNet = 0; // the game's SHIP PRICE (already less what went to the dealer's side)
    int tradeIn = 0;
    bool canBuyFinal = false, back = false, routes = false; // BUY SHIP may go; BACK TO SHIPS stands; BEST PRICES stands
    std::wstring why;                                        // the game's refusal of the ship (ShipBuyProblem)

    bool Final() const { return mode == kFinal; }
    int Filters() const { return Final() ? 5 : 4; } // (the final stage has the inventory's five, the equipment dealer's window four)
    bool OnRow() const { return list[side].count > 0 && list[side].sel < static_cast<int>(rows[side].size()); }
    const Row* Focus() const { return OnRow() ? &rows[side][list[side].sel] : nullptr; }
    // the focus's arg for kActSelect / kActTrade (-1 none)
    int FocusArg() const
    {
        const Row* r = Focus();
        return r && r->row >= 0 ? side * 1000 + r->row : -1;
    }
    // a hold item that goes on a hardpoint (MOUNT), or a mounted one (UNMOUNT): your side of the equipment dealer
    bool Mountable() const
    {
        const Row* r = Focus();
        return mode == kEquipment && side == kYours && r && r->hp < 0 && !r->slot.empty();
    }
};
} // namespace marketscreen
