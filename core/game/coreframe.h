// FLvibed - the logic's frame, draw passes and input events, in the one order every module runs in (docs\module_map.tsv, frame rows). Core owns the
// sequence; the Controller, HUD and Autopilot modules hold places in it through core\ui\views.h, each empty when its module is off, so the same
// sequence serves every combination of modules. The controller's entry calls it; with the controller off, core's own entry does
// (core\logic\core_page.cpp), as long as a module needs a frame at all.
#pragma once
#include "common.h"
#include "game/gamemode.h"

#include <vector>

namespace coreframe
{
// Rows that core reads for every module. They stay in the controller's registry and in `[Controller]`; with the controller off they come from the
// ini (core\logic\alone.cpp).
struct Options
{
    bool enabled = true;          // `set enabled`: the debug switch that stops the frame's modules
    int flufWindows = 2;          // fluf_windows: FLUF's windows 0 ignored, 1 player windows (keep-outs, the pad drives them), 2 all
    bool runInBackground = false; // run_in_background
    int textSize = 100;           // text_size: FLvibed's own UI text, percent (80..150)
    int hudSize = 0;              // hud_size: 0 automatic, 1 small, 2 normal, 3 large (core\game\hudsize.h)
};
inline Options opts;

inline gamemode::Mode g_mode = gamemode::Mode::Menu; // debounced: a new mode counts after 3 frames
inline u32 g_frame = 0;
// The flight HUD's windows, scanned while flying: gamemode tells the HUD from NeuroNet windows by them.
inline std::vector<uintptr_t> g_hudRoots;

void OnFrame(double dt);
// `hud_size` for this frame: asks the machine once (core\game\handheld.h, never from DllMain), writes the size globals
// (core\game\hudsize.h) and returns the text_size to draw with.
int ApplySize();
// The entry faulted: every module gives the game back what it changed (the hidden panels first), then the held commands go, with their key-ups. The host calls
// this once, at the top of the next frame: outside the scene, no logic on the stack.
void OnFault();
// Test aid (debug verb `fault`): an access violation in the logic's next frame (1) or overlay pass (2), so the fail-safe and the give-back can be tested in game.
void TestFault(int where);
// The verb: `fault frame|draw` the entry, `fault <module> frame|draw` that module (modslots::FaultPoint), `fault status` which stopped.
std::string FaultCommand(std::istream& in);
void OnRenderUnderHud();
void OnRenderOverlay();
void OnDeviceLost(); // before every device Reset: every state block the logic holds goes
// A key command the game's key handler just handled (the keyboard's maneuvers: the lock's answer, the HUD's wake cues).
void OnKeyCmd(int cmd, bool down);
// A requested maneuver while the mission has them locked: logged, and shown on HUD 2's alert card. True while locked.
bool LockAnswer(const char* what);
// A raw key as the game's handler sees it, through the key chain (core\game\input_rules.h); true = swallowed.
bool OnRawKey(int vk, bool down, bool textField);
// A host-created native button was clicked: the bar's, a HUD 2 hotspot, else false for the page.
bool OnUiButton(int id);
// The logic stops: the HUD's hit suppression goes.
void Stop();
int FpsCap(int fps); // test games: frame-rate cap (fps 0 = off, < 0 = read only); returns the cap now
} // namespace coreframe
