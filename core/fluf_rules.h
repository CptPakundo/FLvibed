// FLvibed - the input rules beside FLUF's windows (26 Oct 2026; docs\MOD_COMPATIBILITY.md FLUF, docs\research\fluf.md
// "FLUF in play" E): pure functions over the HUD bus's FLUF record (core\hud_bus.h HudFluf / HudFlufExtra), used by the
// controller logic (controller\src\impl\flufaware.*) and the unit tests (tests\fluf_probe_test.cpp). Header only.
//
// The rules (the owner, 26 Oct 2026):
// - FLUF's windows are always on top. Every FLUF window that takes the player's input is driven by the pad - any window
//   whose name the probe knows or not (generalised by ImGui's own state, not by names): a window takes input when FLUF's
//   click rule gives it the mouse and ImGui hit-tests it (not ImGuiWindowFlags_NoMouseInputs). Never the developer
//   windows (the Debug Window is open all the time beside the debugger module) and never a toast (no input).
// - While FLUF's command palette is open (its search box focused or not) or FLUF's ImGui has the keyboard for text,
//   FLvibed holds all its own keyboard input ("much like a chat text box being open - it takes focus for all input").
// - B closes the focused FLUF window: FLUF's own windows (palette, options / Player Status, nav map) by the Escape key
//   FLUF handles (UiManager::ProcessEscapeKey closes the topmost escape-closeable one of them); any other window (a
//   command's window, a module's window: ImGui::Begin with a close button, which Escape does not reach) by its title
//   bar's close button through the game's cursor that FLUF reads.
#pragma once
#include "hud_bus.h"

namespace fluf
{
// FLvibed's own keys stand down (the autopilot key, the options page's keys, every FLvibed key bind).
inline bool HoldsKeys(const HudFluf& f) { return f.present && (f.textInput || f.palette); }

// Does window i take the player's input? `x` may be null (an older bus: the kind decides, as before v5).
inline bool TakesInput(const HudFluf& f, const HudFlufExtra* x, int i)
{
    if (i < 0 || i >= f.count || i >= kHudFlufMax)
        return false;
    const u8 k = f.win[i].kind;
    if (k == FlufDebug || k == FlufToast || k == FlufNone)
        return false;
    if (x && x->frame == f.frame && i < x->count)
        return x->win[i].input != 0;
    return k == FlufPalette || k == FlufWindow || k == FlufMap || k == FlufOther;
}

// The pad drives FLUF this frame (FLvibed's own dispatch steps aside): aware, not ignored, and FLUF has the text keys or
// one of its input windows is open.
inline bool OwnsPad(const HudFluf& f, const HudFlufExtra* x)
{
    if (f.present != 2 || f.policy == 0)
        return false;
    if (f.textInput)
        return true;
    for (int i = 0; i < f.count && i < kHudFlufMax; ++i)
        if (TakesInput(f, x, i))
            return true;
    return false;
}

// The window B acts on: the focused input window, else the frontmost one (ImGui lists windows back to front); -1 none.
inline int CloseTarget(const HudFluf& f, const HudFlufExtra* x)
{
    int front = -1;
    for (int i = 0; i < f.count && i < kHudFlufMax; ++i)
    {
        if (!TakesInput(f, x, i))
            continue;
        if (x && x->frame == f.frame && i < x->count && x->win[i].focused)
            return i;
        front = i;
    }
    return front;
}

// How B closes window i: by FLUF's Escape (its own windows, or no close button known), else by the close button.
inline bool CloseByEscape(const HudFluf& f, const HudFlufExtra* x, int i)
{
    if (i < 0 || i >= f.count)
        return true;
    const u8 k = f.win[i].kind;
    if (k == FlufPalette || k == FlufWindow || k == FlufMap)
        return true;
    return !(x && x->frame == f.frame && i < x->count && x->win[i].closeX >= 0 && x->win[i].closeY >= 0);
}
} // namespace fluf
