// FLvibed - which device takes the input, pad or keyboard and mouse: the pure rule behind device.cpp (docs\UI_PLAN.md 6.2).
// Header only; unit test tests\device_rules_test.cpp.
//
// A held input never takes the device, only a new one: the pad takes it on a button or trigger going down this frame or
// on a stick that moved from where it rested; keyboard and mouse on a key, a mouse button going down this frame or a
// mouse move. The axis-only signals (the stick, the mouse move) wait out the grace after a switch; presses act at once.
// (Was: a held pad button or trigger, and a held mouse button, took the device every frame they were down: holding the
// afterburner trigger and the left mouse button together swapped the device each frame, each swap resetting the mouse
// and restarting the grips' haptic stream, and the game slowed to a crawl - the owner's log, 14 Nov 2026.)
//
// The previous frame's inputs are kept every frame, whichever device is active and under an input_mode lock too, so an
// input held across a switch is not a new press.
#pragma once

namespace devicerule
{
// One frame's signals, as device.cpp reads them.
struct Input
{
    unsigned pad = 0;          // the pad's held inputs as bits: buttons, and the triggers past 0.5
    bool stickMoved = false;   // a stick past its dead zone that moved from where it rested
    bool key = false;          // a physical key this frame (already new: the host's key tick advanced)
    unsigned mouse = 0;        // the physical mouse buttons held, as bits
    bool mouseMotion = false;  // the mouse motion rule
    bool graceOver = false;    // the axis grace after the last switch has run out
};

enum class Take
{
    None,
    PadButton,   // -> pad
    PadStick,    // -> pad
    Key,         // -> keyboard and mouse
    MouseButton, // -> keyboard and mouse
    Mouse,       // -> keyboard and mouse
};

inline bool TakesPad(Take t) { return t == Take::PadButton || t == Take::PadStick; }

class Rule
{
  public:
    // `padActive`: the pad is the device now. Returns what takes the device this frame (None: it stays).
    Take Step(bool padActive, const Input& in)
    {
        const bool padPress = (in.pad & ~padWas_) != 0;
        const bool mousePress = (in.mouse & ~mouseWas_) != 0;
        padWas_ = in.pad;
        mouseWas_ = in.mouse;
        if (!padActive)
        {
            if (padPress)
                return Take::PadButton;
            if (in.stickMoved && in.graceOver)
                return Take::PadStick;
            return Take::None;
        }
        if (in.key)
            return Take::Key;
        if (mousePress)
            return Take::MouseButton;
        if (in.mouseMotion && in.graceOver)
            return Take::Mouse;
        return Take::None;
    }

  private:
    unsigned padWas_ = 0, mouseWas_ = 0;
};
} // namespace devicerule
