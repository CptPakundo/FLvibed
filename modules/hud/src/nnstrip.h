// The strip of words on every NeuroNet window (modules\hud\docs\UI_HUD2_SKIN.md 6.1): LOG, INVENTORY, MAP, STATUS, INFO, with CHAT and GROUP in multiplayer, in the
// bottom lane's left half at the side margin. It replaces the box's icons. The open one is yellow and underlined and follows the game's window manager, so a
// window the game opens itself is right without a press. A click is the game's own command through core\game\nnwin.*; the pad's bumpers ask the same place.
// It stands only while the skin is on and the manager says one of the tabs is open: with HUD 2 off nothing here draws, writes or takes a click.
#pragma once
#include "common.h"

#include <istream>
#include <string>

namespace nnstrip
{
// Once a frame, from the skin's place in core's frame: a held press, the queued clicks, the click box.
void Update(float dt, bool pad);
// The under pass's start, in any mode: nothing stands until Draw says so.
void BeginPass();
// Inside the skin's draw pass: the strip, its keep-out for the prompt lane, its hotspots. False: it does not stand this frame.
bool Draw();
// The scrim the five windows share goes first, under every plate; 0 none.
float ScrimAlpha();
// Its rectangle this frame, pixels x0 y0 x1 y1 (the view the pad's screens read). False: it does not stand.
bool Rect(float out[4]);
void Shutdown();
// Debug: nnstrip [state] | go <tab> | step <dir> | close | slide <seconds> (0 the game's)
std::string Command(std::istream& in);
} // namespace nnstrip
