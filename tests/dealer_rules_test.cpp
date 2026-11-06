// The pad's held A and the ship dealer's final stage (modules\controller\src\impl\dealer_rules.h, GitHub issue 34).
#include "../modules/controller/src/impl/dealer_rules.h"

#include "test.h"

namespace
{
using namespace dealerrules;
constexpr float kFrame = 0.1f;

struct Run
{
    float held = 0;
    bool fired = false;
    int fires = 0, taps = 0;
    // A frame as UpdateWindow runs it: the count, then the fire.
    void Frame(bool down)
    {
        if (Tick(held, fired, down, kFrame))
            ++taps;
        if (Fires(down, fired, held))
            fired = true, ++fires;
    }
    void Frames(bool down, int n)
    {
        for (int i = 0; i < n; ++i)
            Frame(down);
    }
};
} // namespace

void Test_dealer_rules()
{
    // a hold fires once, at its time, and is no tap
    {
        Run r;
        r.Frames(true, 5);
        CHECK(r.fires == 0);
        r.Frames(true, 2);
        CHECK(r.fires == 1);
        r.Frames(true, 20);
        CHECK(r.fires == 1);
        r.Frame(false);
        CHECK(r.taps == 0);
        r.Frames(true, 7); // released: the next hold counts
        CHECK(r.fires == 2);
    }
    // cancelled early: no purchase; the release is a tap, which BUY SHIP ignores
    {
        Run r;
        r.Frames(true, 4);
        r.Frame(false);
        CHECK(r.fires == 0 && r.taps == 1 && r.held == 0);
    }
    // the hold that bought at the first stage does not run on into BUY SHIP
    {
        Run r;
        r.Frames(true, 7);
        CHECK(r.fires == 1);
        NewScreen(r.held, r.fired, true);
        r.Frames(true, 30);
        CHECK(r.fires == 1);
        CHECK(Progress(r.held, r.fired) == 0); // and its prompt does not fill
        r.Frame(false);
        CHECK(r.taps == 0);
        r.Frames(true, 7); // a fresh hold does
        CHECK(r.fires == 2);
    }
    // ...nor does a press carried in and released soon after count as a tap on the new screen
    {
        Run r;
        r.Frames(true, 2);
        NewScreen(r.held, r.fired, true);
        r.Frames(true, 2);
        r.Frame(false);
        CHECK(r.taps == 0 && r.fires == 0);
    }
    // a new screen with the button up: the first press is a plain one
    {
        Run r;
        NewScreen(r.held, r.fired, false);
        r.Frames(true, 7);
        CHECK(r.fires == 1);
    }

    CHECK(Progress(0, false) == 0);
    CHECK(Progress(kHoldSec / 2, false) > 0.49f && Progress(kHoldSec / 2, false) < 0.51f);
    CHECK(Progress(kHoldSec * 3, false) == 1);
    CHECK(Progress(kHoldSec, true) == 0);

    // the focus decides, never a selection behind it
    CHECK(HoldAct(true, false, true, true) == Act::BuyShip);   // a row still selected: the focus is on the button
    CHECK(HoldAct(true, false, true, false) == Act::BuyShip);
    CHECK(HoldAct(false, true, true, true) == Act::Transfer);  // on a row BUY SHIP is never the answer
    CHECK(HoldAct(false, true, true, false) == Act::None);     // ...not even for the frames the panel is gone after a trade
    CHECK(HoldAct(false, true, false, true) == Act::Transfer);
    // on a filter a row selected behind the focus is not traded, BUY SHIP shown or not
    CHECK(HoldAct(false, false, true, true) == Act::None);
    CHECK(HoldAct(false, false, false, true) == Act::None);
    // not enough credits: the game hides the button
    CHECK(HoldAct(true, false, false, true) == Act::None);
    CHECK(HoldAct(true, false, false, false) == Act::None);
    CHECK(HoldAct(false, true, false, false) == Act::None);

    // a plain dealer: the two sides, either way, and no stop on an empty one
    CHECK(NextStop(Stop::Dealers, 1, true, true, false) == Stop::Yours);
    CHECK(NextStop(Stop::Dealers, -1, true, true, false) == Stop::Yours);
    CHECK(NextStop(Stop::Yours, 1, true, true, false) == Stop::Dealers);
    CHECK(NextStop(Stop::Yours, -1, true, true, false) == Stop::Dealers);
    CHECK(NextStop(Stop::Yours, 1, true, false, false) == Stop::Yours);
    CHECK(NextStop(Stop::Dealers, 1, false, true, false) == Stop::Dealers);
    // the final stage, the dealer's list empty, as it opens: the player's rows and BUY SHIP
    CHECK(NextStop(Stop::BuyShip, 1, true, false, true) == Stop::Yours);
    CHECK(NextStop(Stop::BuyShip, -1, true, false, true) == Stop::Yours);
    CHECK(NextStop(Stop::Yours, 1, true, false, true) == Stop::BuyShip);
    CHECK(NextStop(Stop::Yours, -1, true, false, true) == Stop::BuyShip);
    // something sold: three stops, BUY SHIP one press from each list
    CHECK(NextStop(Stop::Yours, -1, true, true, true) == Stop::BuyShip);
    CHECK(NextStop(Stop::Dealers, 1, true, true, true) == Stop::BuyShip);
    CHECK(NextStop(Stop::Yours, 1, true, true, true) == Stop::Dealers);
    CHECK(NextStop(Stop::BuyShip, 1, true, true, true) == Stop::Yours);
    CHECK(NextStop(Stop::BuyShip, -1, true, true, true) == Stop::Dealers);
    // not enough credits: BUY SHIP is no stop
    CHECK(NextStop(Stop::Dealers, 1, true, true, false) == Stop::Yours);
    // nowhere to go
    CHECK(NextStop(Stop::BuyShip, 1, false, false, true) == Stop::BuyShip);
    CHECK(NextStop(Stop::Yours, 1, false, false, false) == Stop::Yours);

    // GitHub issue 36: the list the right stick means. The focus's list wins over the last one, the D-pad's and the cursor's alike
    CHECK(ListSide(1, 2) == 1);
    CHECK(ListSide(2, 1) == 2);
    CHECK(ListSide(2, 2) == 2);
    // ...and a filter, the transfer panel or BUY SHIP keep the list it was last on
    CHECK(ListSide(0, 1) == 1);
    CHECK(ListSide(0, 2) == 2);
    // the bar of that side, in either of the game's orders
    const int dealerFirst[2] = {1, 2}, yoursFirst[2] = {2, 1};
    CHECK(OnSide(dealerFirst, 2, 1) == 0);
    CHECK(OnSide(dealerFirst, 2, 2) == 1);
    CHECK(OnSide(yoursFirst, 2, 1) == 1);
    CHECK(OnSide(yoursFirst, 2, 2) == 0);
    // the focused list fits, so its bar is hidden: the other list's never stands in for it, the report's fault
    const int onlyDealers[1] = {1}, onlyYours[1] = {2};
    CHECK(OnSide(onlyDealers, 1, 2) == -1);
    CHECK(OnSide(onlyYours, 1, 1) == -1);
    CHECK(OnSide(onlyDealers, 1, 1) == 0);
    CHECK(OnSide(onlyYours, 0, 2) == -1);
}
