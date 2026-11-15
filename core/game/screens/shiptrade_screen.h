// FLvibed - the ship dealer page's reads and actions (the game's NN_ShipTrader window; the HUD 2 page framework, audit 70b
// F3c; moved from modules\hud\src\basepage.cpp unchanged). The game's window stands parked off the screen and does the work:
// its three ships, their refusals, its selection, its BUY button (which opens the final stage, NN_Dealer in its mode 2: the
// market page). Its 3D view of the chosen ship stays the game's, brought back to the screen's middle after the park; the pad
// turns and zooms it by writing the view's own fields, as its mouse drag and HDE's wheel do.
#pragma once
#include "shiptrade_model.h"
#include "action.h"

namespace shiptradescreen
{
// The page's reads (the runtime calls them: modules\hud\src\pages\pagerun.cpp). The window's texts change under the page with
// no event it hears, so the stamp changes each frame; Read takes the ships every 0.4 s out of the window manager's slide (at
// once on the open, soon after a pick) and chooses the first when the game has none chosen. True when the ships were read.
uint64_t Stamp();
bool Read(Model& m, u32 root);
// What the view shows of the game, read right before it draws (the old page read it as it drew): the cash, the budget, the
// player's ship, the chosen ship's refusal, card and numbers.
void ReadFrame(Model& m, u32 root);

// Asked as the page's prompts and pad decide, at that moment (the old page asked the game the same way).
bool CanBuy(const Model& m); // a ship is chosen, the game's BUY stands and nothing refuses that ship
bool Zoomable();             // the 3D view zooms (HDE's flplusplus answers its wheel): the triggers' ZOOM
// The 3D view's control (ShipTrader3DShip, exe::VtShipView), 0 none; for the `basepage` verb.
u32 ViewCtl();

// The 3D view, which is the game's: brought back to the screen's middle after the window's park (it goes where its window
// goes, by its own position call; the shell calls this right after pagerun::Frame, out of the slide); the pad's stick turns
// it and the triggers zoom it (the page's pad: the view's own fields, no handler of the game's); the wheel kept from the game
// while the mouse is not over it (it zoomed as the card scrolled: issue 41).
void ViewBack();
void Turn(float rx, float ry, float dt);
void Zoom(float lt, float rt, float dt);
bool OverShip(); // the mouse stands over the ship's hole (the wheel is the view's there)
void HoldWheel(bool on);

// The window the page claims: NN_ShipTrader's class (exe::VtShipTrader).
extern const hk::Sym* const kWindowClass;
constexpr const char* kWindowName = "NN_ShipTrader";
constexpr const char* kCloseButton = "ShipTraderCancelButton";

// The page's actions (their ids: shiptrade_model.h Action): the runtime's only way to act for it, each the window's own
// button (borrowed).
extern const screens::ActionDesc kActions[];
extern const int kActionCount;
} // namespace shiptradescreen
