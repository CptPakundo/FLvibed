// FLvibed - the one focus gate for the physical pad (U38; a game in the background ignores pad
// input, as is standard). Header only; unit test tests\focus_gate_test.cpp.
//
// Both pad readers (the flight pad, controller.cpp; the menus' and the options page's, uikit.cpp) ask the same question
// through gamepad.cpp: while the game is not the foreground window the physical pad is ignored, in flight and menus
// alike; `focus_only` 0 lets it act in the background in both. The game is in front when the foreground window is its
// main window or one the main window owns (a message box, a dialog). The virtual / test pad is never gated (gamepad.cpp
// merges it after the real read).
//
// An input held across the focus return stays ignored until it is released: the press began while the game was in the
// background (the player pressed A in another program, then clicked back into the game), so it is not the game's - the
// rule of core\press_gate.h, applied to the focus change.
#pragma once

namespace focusgate
{
// The game is in front: the foreground window is the main window, or the main window is its root owner.
inline bool Foreground(const void* fg, const void* fgRootOwner, const void* main)
{
    return main && fg && (fg == main || fgRootOwner == main);
}

// The physical pad is ignored this frame.
inline bool IgnoreReal(bool focusOnly, bool foreground) { return focusOnly && !foreground; }

// The physical pad's held inputs as bits (buttons; sticks and triggers past their dead zone) -> the bits that may act.
class Held
{
  public:
    unsigned Filter(bool ignore, unsigned down)
    {
        if (ignore)
        {
            stale_ = down; // everything held now began, or goes on, while the pad is ignored
            return 0;
        }
        stale_ &= down; // released: the next press is new
        return down & ~stale_;
    }

  private:
    unsigned stale_ = 0;
};
} // namespace focusgate
