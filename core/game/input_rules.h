// Who owns the input in flight, pure: the rules of docs\MODULES_PLAN.md "P7's priority table". tests\input_rules_test.cpp holds each
// against the expression the frame used. The table's quirks are kept on purpose. `sqrtf` comes with core\pre.h.
#pragma once

namespace inputrules
{
// The held actions the autopilot's flight and the pad's both reach.
enum class Channel : int
{
    Fire,
    Missile,
    Afterburner,
    Reverse,
    Rear,
    Slide
};

// While the autopilot flies, who a channel listens to.
enum class Merge : int
{
    Pad,       // the player's button works, as in the player's flight
    Autopilot, // the pad is not heard
    Either
};

constexpr Merge WhileFlying(Channel c)
{
    switch (c)
    {
    case Channel::Fire:
    case Channel::Afterburner: return Merge::Either;
    case Channel::Reverse:
    case Channel::Slide: return Merge::Autopilot;
    default: return Merge::Pad;
    }
}

constexpr bool Want(Channel c, bool autopilot, bool pad)
{
    const Merge m = WhileFlying(c);
    return m == Merge::Either ? autopilot || pad : m == Merge::Autopilot ? autopilot : pad;
}

// Who holds a command: read by debug `owners`. A release is for all of them (`ReleaseAll`).
enum Owner : int
{
    OwnerNone,
    OwnerPad,
    OwnerAutopilot,
    OwnerBot
};

constexpr Owner ClaimedBy(Channel c, bool autopilot, bool pad)
{
    const Merge m = WhileFlying(c);
    return m != Merge::Pad && autopilot ? OwnerAutopilot : m != Merge::Autopilot && pad ? OwnerPad : OwnerNone;
}

// The player's own steering takes the ship back at once: any stick past `autopilot_cancel` (0: never), except the right stick where it is
// the aim (`rightAims`). The sticks as the press gate leaves them.
struct Sticks
{
    float lx, ly, rx, ry;
    bool rightAims;
};

inline bool TakesBack(const Sticks& s, float cancel)
{
    const float right = s.rightAims ? 0.0f : sqrtf(s.rx * s.rx + s.ry * s.ry);
    const float left = sqrtf(s.lx * s.lx + s.ly * s.ly);
    const float stick = right < left ? left : right;
    return cancel > 0 && stick > cancel;
}

// The open options page owns the pad and the keys, and its ownership outlasts its close until every button is up. True: the page has
// this frame; the autopilot flies on under it unless the game is paused (FliesUnderWindow).
inline bool PageHolds(bool wantsInput, bool anyButton, bool& latch)
{
    if (wantsInput)
    {
        latch = true;
        return true;
    }
    if (latch)
    {
        if (anyButton)
            return true;
        latch = false;
    }
    return false;
}

// The autopilot keeps working if the game isn't paused: no window, menu or page stops it. `spaceUi`: a NeuroNet window has the cursor;
// `pageHolds`, `inSpace`: the options page over the flight. True: it flies, and the window or the page keeps the pad, the keys and the
// cursor, so nothing the player does there steers the ship or hands it back.
constexpr bool FliesUnderWindow(bool autopilotActive, bool spaceUi, bool pageHolds, bool inSpace, bool paused)
{
    return autopilotActive && !paused && (spaceUi || (pageHolds && inSpace));
}

// Who gets the frame past the page. Flight: the test bot or the autopilot flies, whoever touched the mouse or pad last: a keyboard start
// comes here, and the pad is read for it. Window: the autopilot flies under a window, the pad is the window's. Device: the physical mouse
// and keyboard are in use. Pad: FLUF, the menus, then the mode.
enum class Frame : int
{
    Flight,
    Window,
    Device,
    Pad
};

constexpr Frame WhoHasTheFrame(bool autoFlies, bool inSpace, bool padOwns, bool underWindow)
{
    return underWindow ? Frame::Window : autoFlies && inSpace ? Frame::Flight : !padOwns ? Frame::Device : Frame::Pad;
}

// A raw key's way, first taker wins; FLUF's text box ends it with nobody answering.
enum class KeyStep : int
{
    Fluf,
    Page,
    GoalPanel,
    Modules,
    BarButton
};
inline constexpr KeyStep kKeyChain[] = {KeyStep::Fluf, KeyStep::Page, KeyStep::GoalPanel, KeyStep::Modules, KeyStep::BarButton};
} // namespace inputrules
