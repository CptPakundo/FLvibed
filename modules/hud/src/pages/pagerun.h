// FLvibed - the page runtime (the HUD 2 page framework: audit 70b 2.2, F2a): the game's half of a page. For a page of the
// list (pagelist.h) it claims the game's window by its class in the window registry (core\ui\winplates.h g_wins), parks it
// off the screen by the window's own setter and gives it back where the game had it, holds the page's model (made on the
// open, read again when its stamp changes) and its state, draws its view, walks its pad bindings (its scroll by its repeat,
// its actions through screens::Runner, core\game\screens\action.h), and gives everything back on Shutdown (a style switch to
// the module going, a fault, a hot reload). One page at a time; dialogs come with the pages that need them.
// Its first page is INFO (pages\neuronet\info_page.*). The NeuroNet's shell (nnpage.cpp) still decides which of its pages
// stands, draws the ground, title and left column round INFO's view and applies the world pause; it calls these entries.
// The docked windows' pages (pages\dealers\, audit 70b F3) have their shell in basepage.cpp, which calls them the same way.
// The test surface: the verb `page` (Command).
#pragma once
#include "page.h"
#include "page_view.h"
#include "ui/views.h"

#include <istream>
#include <string>

namespace page
{
// A page's own input (PageCore::input), where it does not fit the bindings (the docked pages, audit 70b F3a). Each hook
// gets the page's model; a game action goes through Act (the runner), never a call of the game's.
struct PageInput
{
    int (*Prompts)(const void* model, views::PagePrompt* out, int max);           // its prompts as drawn
    unsigned (*Pad)(void* model, unsigned pressed, unsigned held, float dt); // its pad; the buttons it takes (besides padTakes)
    void (*Wheel)(void* model, int notches);                                 // the mouse wheel's notches this frame (null: none)
    void (*Hit)(void* model, int action, int arg, bool pad); // a click on a hit box whose action is not in the table (a row)
    // What the page read of the game while it drew, read again right before its view, once a frame, in pagerun::Draw (not in
    // the frame's update: the game's text is placed between the two). Null: none (audit 70b F4a).
    void (*Prepare)(void* model, u32 root);
    // The page's own frame, once a frame in pagerun::Frame after the read, in place of Wheel: its window, the wheel's notches,
    // the frame's time and the pad's form (INVENTORY's reads keep their clocks in frame time; audit 70b F4 unit b2). Null: none.
    void (*Tick)(void* model, u32 root, int notches, float dt, bool pad) = nullptr;
};
} // namespace page

namespace pagerun
{
// Two shells decide which page stands until the pages leave their files (audit 70b F3 / F4): the NeuroNet's (nnpage.cpp) and
// the docked windows' (basepage.cpp). Each one's Keep releases only a page it opened.
constexpr int kShellNeuroNet = 0, kShellBase = 1;
// The window page `id` claims: the first window of its class the registry holds that is shown; 0 none (or no such page).
u32 Claim(const char* id);
// Before the frame's page is known to stand: a parked window whose page no longer stands over it (another page, none, or
// another window) goes back where the game had it, if the game still shows it. `id` null: none of this shell's pages.
void Keep(const char* id, u32 root, int shell = kShellNeuroNet);
// Page `id` stands over `root` this frame (`opened`: newly, or over another window): its model read on the open and when its
// stamp changes, its window parked (once the window manager's slide is over), the mouse wheel's notches scroll it (or go to
// its Wheel hook). `pad`: the pad's form this frame (its Hit hook is told). `partner`: the window an Away page works beside
// its own (the dealer's NN_Inventory), parked by the same rule after it and never given back either; 0 none.
void Frame(const char* id, u32 root, bool opened, int wheel, float dt, bool pad = false, u32 partner = 0);
// The docked pages' shell's Draw begins: the game lists the last view recorded are dropped (a list this Draw does not draw
// sends nothing under the next UI pass), and the ground's holes are the shell's: the docked pages share one ground, so a page
// opened after another starts with that one's holes, whichever file draws either (basepage.cpp keeps them).
void DrawStart(const page::Holes& holes);
// The standing page's view in `in` (hover and last frame's holes filled here, its Prepare hook first); what it used. Its hit
// boxes go to the skin's click box after it, in its order; its game lists get their meshes and holes, then its icon sets move
// the game's icons; its holes are kept for GroundUnder.
page::ViewOut Draw(const page::ViewIn& in);
// Under the game's UI pass: the page's ground in the holes its view left (the game's icons stand on it, not on the room).
void GroundUnder();
// Between the game's row fill and its UI pass: the last view's game lists send their rows' models (nnpage_kit.h ListIcons),
// each only while its window is still the one the model holds.
void IconsUnder();
// The last view's game lists dropped (nothing sends; their last sends forgotten), the ground's holes kept: the NeuroNet's shell
// under a frame its INVENTORY did not send (its window gone, another page, none).
void DropLists();
// List `k`'s rows the last under-pass gave a mesh (their game rows, at most `max`); how many (the verb `nnpage` shownRows).
int ListShown(int k, int* gameRows, int max);
// The A hold the docked pages share (a job, a ship, a dealer's row): one that fired on one page is not taken up by the next
// until A is let go (the ship bought opens the final stage on BUY SHIP: its hold needs a fresh press).
pageinput::HoldState& PurchaseHold();
// The standing page's prompts (its scroll first while the text is longer than the page, then its actions that may run; or
// its own Prompts hook); the way out (B CLOSE) is the pad's own.
int Prompts(views::PagePrompt* out, int max);
// The standing page's pad: presses run its actions, the D-pad and right stick scroll (or its own Pad hook); the buttons it
// takes from the game.
unsigned Pad(unsigned pressed, unsigned held, float dt);
// One action of the standing page with `arg`, through the runner (its input hooks' way to the game). False: not run.
bool Act(int action, int arg);
// Everything back: the parked window where the game had it, the models freed, nothing in flight.
void Shutdown();

// What the rest of HUD 2 asks of a standing page (the skin, the game's text, the strip, the HUD's roots), whichever file
// draws it: the pages not on the runtime yet answer through their entries (pagelist.h LegacyPage).
bool Standing();          // some page stands
bool Covers(int fam);     // a standing page is the game's window of that family (winplates): no plate, no focus on it
bool ControlsOffScreen(); // a standing page keeps its window's controls off the screen: nothing of the game's text to place
void DrawUnder();         // under the game's UI pass (the page's ground under the game's 3D icons)
void DrawOver();          // over the game's UI pass
void DrawAfter();         // after it (a 3D view over the page)

// The test surface (and nnpage's `nnpage info`): whether a window is parked; a page's state and model (null: never opened).
bool Parked();
const page::PageState* State(const char* id);
const void* Model(const char* id);
// page [state] | model | game | press <a|b|x|y|up|down|left|right>
std::string Command(std::istream& in);
} // namespace pagerun
