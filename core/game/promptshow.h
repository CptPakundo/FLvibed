// FLvibed - when HUD 2's context prompts show (0.2.1, the owner, 2 Nov 2026: "like the Focus preset's elements that
// disappear when not needed"). Setting hud_prompts: 0 = always shown (0.2.0's behaviour); 1 = in flight the prompt lanes
// appear when their content changes (a prompt added or removed, a label changed: the prompt set's signature) or right
// after a player action (goto, dock, formation, free flight, cruise, a target switch / clear, the wheel's actions), stay
// kHoldS, then fade out (the caller's ease: HUD 2's context fade). A prompt that waits on the player is never hidden.
// Pure: no game, no clock - the caller passes dt. Unit test: tests\promptshow_test.cpp.
#pragma once
#include <string>

namespace promptshow
{
constexpr float kHoldS = 4.0f; // how long the lanes stay after a change or an action

// FNV-1a over a prompt set: each prompt's button (or key) and label, in order. Mix one prompt at a time.
inline unsigned Mix(unsigned h, const std::wstring& s)
{
    for (wchar_t c : s)
        h = (h ^ static_cast<unsigned>(c)) * 16777619u;
    return (h ^ 0x1Fu) * 16777619u; // (a separator: "AB"+"C" != "A"+"BC")
}
inline unsigned Mix(unsigned h, int v) { return (h ^ static_cast<unsigned>(v)) * 16777619u; }
constexpr unsigned kSeed = 2166136261u;

struct State
{
    bool seen = false; // a signature was read (the first one counts as a change: the lanes show on arrival)
    unsigned sig = 0;
    float left = 0;    // seconds the lanes still show
};

// One frame: `mode` = hud_prompts, `sig` = this frame's prompt-set signature, `action` = a player action this frame,
// `waiting` = a prompt that waits on the player shows. Returns true while the lanes show (their fade target 1).
inline bool Step(State& s, int mode, unsigned sig, bool action, bool waiting, float dt)
{
    const bool changed = !s.seen || sig != s.sig;
    s.seen = true, s.sig = sig;
    if (changed || action)
        s.left = kHoldS;
    else
        s.left = s.left > dt ? s.left - dt : 0.0f;
    if (mode == 0 || waiting)
        return true;
    return s.left > 0;
}
} // namespace promptshow
