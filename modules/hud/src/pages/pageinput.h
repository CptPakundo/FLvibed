// FLvibed - the pages' input rules, pure (the HUD 2 page framework: audit 70b 2.4): a page's bindings, the held D-pad's
// repeat and the purchase hold, with today's values. Three repeat rates stay apart, as they are today (the owner's
// default, 70b 6.1; unified only on his word):
//  - the lists (the dealers, LOG, INVENTORY, STATUS): 0.3 s, then a row each 0.05 s, 20 a second
//    (modules\hud\src\basepage.cpp and nnpage.cpp, their D-pad repeat; issue 68);
//  - the game's own text bar (the docked text's scroll): 0.3 s, then a step each 0.07 s, about 14 a second
//    (modules\controller\src\impl\padbases.cpp, the text bar's repeat);
//  - INFO's own text: 15 lines a second once the hold passes 0.3 s plus a line (nnpage.cpp, INFO's scroll; several lines
//    in one long frame).
// The first press is the caller's (an edge); these give the repeats only. tests\pageinput_test.cpp replays today's code
// beside them at several frame rates. No game include.
#pragma once
#include <algorithm>
#include <cstdint>

namespace pageinput
{
// A held button's repeat. Two forms, each exactly today's: `perSecond` 0 = once a frame at most, `delay` then one each
// `step` seconds; `perSecond` > 0 = a rate, any number a frame, after `delay` and one more step.
struct Repeat
{
    float delay, step, perSecond;
};
constexpr Repeat kListRepeat = {0.3f, 0.05f, 0};    // 20 rows a second
constexpr Repeat kTextBarRepeat = {0.3f, 0.07f, 0}; // the game's text bar, about 14 steps a second
constexpr Repeat kInfoTextRepeat = {0.3f, 0, 15};   // INFO's own text, 15 lines a second

struct RepeatState
{
    float acc = 0;
};

// The repeats this frame (0 when not held; the state resets on release).
inline int Step(const Repeat& r, RepeatState& s, bool held, float dt)
{
    if (!held)
    {
        s.acc = 0;
        return 0;
    }
    int fires = 0;
    if (r.perSecond > 0)
    {
        s.acc += dt * r.perSecond;
        for (; s.acc >= 1 + r.delay * r.perSecond; s.acc -= 1)
            ++fires;
        return fires;
    }
    s.acc += dt;
    if (s.acc > r.delay)
        ++fires, s.acc -= r.step;
    return fires;
}

// A scrolling text's lines this frame (the page runtime's scroll, INFO's as it was): a press a line (down +1 before up -1), a
// hold by the repeat (down before up; the state resets when neither is held).
inline int ScrollLines(const Repeat& r, RepeatState& s, bool pressDown, bool pressUp, bool holdDown, bool holdUp, float dt)
{
    const int press = pressDown ? 1 : pressUp ? -1 : 0;
    const int dir = holdDown ? 1 : holdUp ? -1 : 0;
    return press + dir * Step(r, s, dir != 0, dt);
}

// The purchase hold (a job, a ship, a dealer's row): a tap never spends; the action fires once when the button has been
// held kHoldSec, and not again until it is let go (modules\controller\src\impl\dealer_rules.h, the same 0.6 s).
constexpr float kHoldSec = 0.6f;
struct HoldState
{
    float held = 0;
    bool fired = false;
};
// True on the frame the hold completes.
inline bool StepHold(HoldState& h, bool down, float dt)
{
    if (!down)
    {
        h = HoldState{};
        return false;
    }
    h.held += dt;
    if (!h.fired && h.held >= kHoldSec)
        return h.fired = true;
    return false;
}
// The prompt's fill: 0 .. 1 while held, 0 once fired.
inline float HoldProgress(const HoldState& h) { return h.fired ? 0 : std::min(1.0f, h.held / kHoldSec); }
// The docked pages' hold (basepage.cpp's, kept as it was): it fires only while its action `may` run, so a hold that waited
// for the action fires the frame it may (a row selected late); true on that frame.
inline bool StepHoldIf(HoldState& h, bool down, float dt, bool may)
{
    h.held = down ? h.held + dt : 0;
    if (!down)
        h.fired = false;
    if (down && !h.fired && h.held >= kHoldSec && may)
        return h.fired = true;
    return false;
}

// What a binding does for the prompt rule: an action, or the page's way out (never the prompt that gives way).
enum class Role : uint8_t
{
    Action,
    Close, // leaves the page (B CLOSE, Esc)
    Back,  // returns to the page's parent (a staged page)
};

// One input of a page: a pad button (mask) or a key, the action it runs (-1: none, e.g. a close the runtime handles),
// its word (null: no prompt), whether it is a hold.
struct Binding
{
    uint32_t pad;
    int key;
    int action;
    const wchar_t* word;
    bool hold;
    Role role;
    int priority; // the prompt rule: among the actions, the lowest gives way first (ties: the later one)
};
} // namespace pageinput
