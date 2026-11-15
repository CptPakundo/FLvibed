// The door between core and HUD 2 (docs\ARCHITECTURE.md "The two interfaces"): the one file of core that calls into the HUD module's views
// (views::hud, views::skin in core\ui\views.h). Core's frame (core\game\coreframe.cpp), the options page and the window plates reach HUD 2 only
// through it. It decides which interface is selected, once a frame: HUD 2 runs only between its Enter (the first frame it is selected) and its
// Leave (the first classic frame after), which gives back everything it changed in the game; while the classic HUD is selected no HUD 2 code runs,
// its views stay empty and every entry answers as with the HUD module off. It counts the calls into the HUD module made while the classic HUD is
// selected (`huddoor`, a debug verb), which must stay 0. The other modules still reach HUD 2 through views.h themselves, counted by
// tools\module_lint.py: while the classic HUD is selected those views are empty, so they get the classic answers.
#pragma once
#include "common.h"
#include "ui/views.h"

namespace huddoor
{
// What the HUD module hands the door at its Attach (modules\hud\src\hud2_entry.cpp); empty at its Detach and at a fault's (its own Fault gives back).
struct Hud2Entry
{
    void (*Enter)(); // the first frame HUD 2 is selected: it publishes its views, the loading screen's draw and the idle cursor's feed
    // Each frame from the first classic one until it answers true: its views withdrawn, everything it changed in the game given back. More than one frame
    // only while a base page's window still closes (the game destroys its controls at the close, so the window's text goes back first).
    bool (*Leave)();
    // A frame of a load (the host's load-frame hook, through the door while HUD 2 is selected): true, HUD 2's loading screen; false, the game's own,
    // with what HUD 2's screen changed (its word) put back.
    void (*LoadDraw)(bool hud2);
};
void Register(const Hud2Entry& entry);

// The HUD module is loaded and running (not stopped by a fault): HUD 2 can be selected. Read from the module's slot, never from its views, which stay
// empty while the classic HUD is selected (core\logic\alone.cpp decides from it whether core's frame runs with the controller off).
bool Present();
// The classic HUD is selected: hud_style 0, or no HUD module, as the door decided at this frame's start. The same on both devices: the two are not
// mixed (decision 77).
bool Classic();
// hud_style as the player set it, one of its two states (0 CLASSIC, 2 HUD 2: core\ui\hud_rows.h StyleOf), for the UI options' copy (core\ui\uiopt.h).
int Style();

// First in core's frame: the decision (Enter, Leave) and the counter's frame step.
void Frame();

// The extras that run under both interfaces (the shield and reticle cues, game_text_size, the idle cursor's hide, the hover help's rule: the owner keeps
// them on the classic HUD). Their calls are not counted as HUD 2's.
void Cues(float dt, bool flying, bool autopilot);
void GameText(bool menu);
void Cursor(float dt, bool padOwns, bool flyingFree, bool (*onlyHudOpen)());

// HUD 2's places in core's frame and passes; on the classic HUD the classic pass where one is due.
void Flight(float dt, const views::HudContext& c);
void Skin(float dt, bool padOwns, bool enabled, bool docked);
// The UI pass's entry. `flying`: in space (a NeuroNet window too); `space`: free flight (the hover help's rule); `padOwns`: the pad is the device
// (on the classic HUD the rule is the pad's only). True: a modal menu holds the flight HUD under its scrim (HUD 2's skin only).
bool UnderBegin(void* device, bool flying, bool space, bool padOwns);
// The under pass's end: HUD 2's glass, or the classic pass (core\ui\winplates.h ClassicPass); then the extras' shield cue over it.
void UnderEnd(void* device, bool shieldCue, bool enabled, bool padOwns);
void Overlay(void* device, bool menuOver, bool noUnderPass);
void Strip(void* device);
void Top(void* device);
void Hotspot(int slot);       // a click on one of HUD 2's keyboard-and-mouse hotspots
bool LockNotice();            // the maneuver lock's alert card; false: HUD 2 did not show it
void NoteAction(unsigned wake); // hudwake::Wake

// HUD 2's state, for core's own screens.
bool PageOn();         // a NeuroNet page stands (the wheel and the log's keep-outs are the page's)
bool SkinOn();         // Extended HUD 2 dresses the game's windows (the options page's look)
bool NNStrip();        // the strip of words on the NeuroNet windows stands (its bumpers)
float StripRight();    // the docked strip's right edge, pixels; 0 none
float StripBottom();   // its bottom edge, fractions of H

// Debug verb `huddoor [reset | loadstyle <0..2>]`: the selected interface, the calls into the HUD module while the classic HUD was selected, the load
// frames drawn by HUD 2 and by the game. TEST ONLY loadstyle: hud_style is written at the next load's 3rd frame (no command reaches a load).
std::string Command(std::istream& in);
} // namespace huddoor
