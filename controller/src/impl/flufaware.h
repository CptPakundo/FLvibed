// FLUF awareness in the controller logic (26 Oct 2026; docs\MOD_COMPATIBILITY.md FLUF, docs\research\fluf.md "FLUF in
// play" E): FLUF's windows as the HUD bus reads them (bus v4, core\fluf.h) - keep-outs for HUD 2, Extended HUD 2's strip
// and card, the prompt lane; the input rules (FLvibed's raw keys yield while FLUF's text box has the keys; the pad drives
// FLUF's input windows with the cursor, or the palette with its own keys, and says so in the prompt bar); the
// `fluf_windows` setting pushed to the bus. No FLUF (or an older host / bus): every query answers "nothing", so FLvibed
// behaves exactly as without this file.
#pragma once
#include "common.h"
#include "hud_bus.h"

#include <string>

namespace flufaware
{
// Every frame (OnFrame), first thing after the options: reads the bus once (FLUF's snapshot moves once per its frame),
// pushes `policy` (`fluf_windows`), logs a status change once.
void Update(float dt, int policy);
const HudFluf& State();
bool Present();   // FLUF is loaded (any build)
bool TextInput(); // FLUF's ImGui has the keyboard for text
// FLvibed holds all its own keyboard input: FLUF's text box has the keys, or its command palette is open, focused or
// not (the owner, 26 Oct 2026: "much like a chat text box being open"; core\fluf_rules.h HoldsKeys)
bool HoldsKeys();
// A FLUF window that takes input is open (the palette, the options / Player Status window, the nav map, any command's or
// module's window that ImGui hit-tests; not the Debug Window or a toast - core\fluf_rules.h): the pad drives it this
// frame instead of FLvibed's own dispatch.
bool OwnsPad();
// The window B closes (the focused input window, else the frontmost): by FLUF's Escape (`byEscape`), else by its close
// button at (fx, fy) in fractions of the back buffer. False = none. `name` = its ImGui name (bus v5).
bool CloseTarget(bool& byEscape, float& fx, float& fy, std::string& name);
// A window of this ImGui name is open (bus v5; false without it).
bool WindowOpen(const std::string& name);
bool PaletteOpen();
// Does a FLUF keep-out meet this rectangle (pixels of a W x H back buffer)?
bool Meets(float x0, float y0, float x1, float y1, float W, float H);
// The alpha factor of one of FLvibed's parts: eases to 0.15 while a FLUF window covers any of it (FLUF draws over FLvibed
// and takes the mouse there), back to 1 when free (about 0.15 s each way, eased once per frame per slot).
enum Slot
{
    SlotNav,
    SlotTarget,
    SlotShip,
    SlotLoadout,
    SlotRing,
    SlotContext,
    SlotAlert,
    SlotStrip, // Extended HUD 2's docked overlay: the strip, the character card and the decision words, each its own
    SlotCard,
    SlotWords,
    SlotCount,
};
float Dim(Slot slot, float x0, float y0, float x1, float y1, float W, float H);
// The keep-outs in pixels (the prompt lane's extras).
void KeepOuts(float W, float H, void (*add)(float x0, float y0, float x1, float y1, void* ctx), void* ctx);
std::string Describe(); // debug `fluf`
} // namespace flufaware
