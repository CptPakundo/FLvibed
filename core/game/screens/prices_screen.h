// FLvibed - the BEST PRICES page's reads and actions (the game's commodity list by base, NN_CommodityByBase; the HUD 2 page
// framework, audit 70b F3a; moved from modules\hud\src\basepage.cpp unchanged). The game's window stands parked off the
// screen; it lists the bases the player has been to, the page puts its rows in the order that answers the question.
#pragma once
#include "prices_model.h"
#include "action.h"

namespace pricesscreen
{
// The page's reads (the runtime calls them): the stamp changes each frame; Read takes the frame's words each frame (as the
// page read them while it drew) and the rows every 0.2 s out of the window's slide. True when the rows were read.
uint64_t Stamp();
bool Read(Model& m, u32 root);

// The window has no class symbol in core\flexe.h yet: the shell finds it by its name, as before.
constexpr const char* kWindowName = "NN_CommodityByBase";
constexpr const char* kCloseButton = "CommodityByBaseCloseButton";

// The page's actions (their ids: prices_model.h Action).
extern const screens::ActionDesc kActions[];
extern const int kActionCount;
} // namespace pricesscreen
