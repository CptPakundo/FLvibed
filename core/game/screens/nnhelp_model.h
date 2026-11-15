// FLvibed - the INFO page's model (the game's NeuroNet window NN_Help; the HUD 2 page framework, audit 70b 2.4): what the
// window would show now, as a plain value. Filled by nnhelp_screen.cpp (Compose) from the game's own appenders; drawn by
// modules\hud\src\pages\neuronet\info_view.cpp. Nothing here calls the game or draws. The namespace keeps its first name,
// infobody (audit R3), with the pure rules' inforules (nnhelp_rules.h).
#pragma once
#include "common.h"
#include "nnhelp_rules.h"

#include <string>
#include <vector>

namespace infobody
{
enum class Case
{
    None,       // nothing to show (in flight with no ship, or the composers unresolved)
    Target,     // the target's card (not docked, the ship has a target the game finds)
    TargetGone, // a target the game no longer finds: the title, an empty body (as the window shows it)
    Space,      // no target: the system, the sector, the zone the ship is in
    Docked      // the base
};
struct Body
{
    Case kase = Case::None;
    u32 target = 0;     // the object the card is of (Target, TargetGone)
    std::wstring title; // what the window's title shows (the target's name upper-cased, else CURRENT INFORMATION)
    std::vector<inforules::Para> paras;
};
// The page's model: the text it shows, and whether it was just opened (the first read always takes the text; a later one
// keeps it when the target is cleared or gone, as the game's window does).
struct Model
{
    Body body;
    bool fresh = true;
};
// The game's window's moments (0x474B90 on the open, 0x4E4690 -> 0x4754A0 on a new target): whether to compose for `target`
// (0 none); then whether to take what was composed (a card the game no longer finds leaves an open window as it was).
inline bool Composes(const Model& m, u32 target) { return m.fresh || target != 0; }
inline bool Takes(const Model& m, Case composed) { return m.fresh || composed != Case::TargetGone; }
} // namespace infobody
