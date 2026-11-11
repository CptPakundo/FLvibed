// The strip of words on every NeuroNet window (modules\hud\docs\UI_HUD2_SKIN.md 6.1): LOG, INVENTORY, MAP, STATUS, INFO, with CHAT and GROUP in multiplayer, in the
// bottom lane's left half at the side margin. It replaces the box's icons. The open one is yellow and underlined and follows the game's window manager, so a
// window the game opens itself is right without a press. A click is the game's own command through core\game\nnwin.*; the pad's bumpers ask the same place.
// It stands only while the skin is on and the manager says one of the tabs is open: with HUD 2 off nothing here draws, writes or takes a click.
#pragma once
#include "common.h"
#include "navmap_layout.h"

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
// The room's words, in that pass when the strip does not stand: the room bar's rooms, the row of the room's actions and the NeuroNet's tabs on plates that cover the game's
// icons. The game's buttons stay where they are and nothing is written to them; a word's click is the button's (room::Goto) or the tab's command. Off (the plates' old look
// stands) in a scene, under a window that hides the bar and when the words do not fit.
void DrawRoom();
// The room's words this frame: the bar's left edge and the lowest foot, pixels; 0 they do not stand (the strip's budget, the pages' top).
float RoomLeft();
float RoomFoot();
// A NeuroNet window of the row stands and nothing else has the screen: the strip is wanted, and a page may stand.
bool Wanted();
// The scrim the five windows share goes first, under every plate; 0 none.
float ScrimAlpha();
// Its rectangle this frame, pixels x0 y0 x1 y1 (the view the pad's screens read). False: it does not stand.
bool Rect(float out[4]);
// The hover help's view of the words (GitHub issue 71), as the last pass drew them: true while (x, y) is on one of their plates - the room bar, the actions'
// row, the NeuroNet box, the strip and its CLOSE. Then `ctl` is the game control the word under it stands for (the one its click acts for: the room's button,
// the box's button of the tab's command; 0 between words or for CLOSE), `word` its text and `r` its click box, pixels x0 y0 x1 y1.
bool WordAt(float x, float y, u32& ctl, std::wstring& word, float r[4]);
// What it drew the last pass it stood, for what draws before it in the pass (the map's labels: issue 72): its rectangle with the View badge docked on the
// pad, then the mouse's CLOSE in the foot's lane. How many; 0: it did not stand then.
int LastAreas(navlayout::Rect out[2]);
void Shutdown();
// Debug: nnstrip [state] | go <tab> | step <dir> | close | slide <seconds> (0 the game's) | rooms | hit <x> <y> | room <i> | act <i> | tab <i>
std::string Command(std::istream& in);
} // namespace nnstrip
