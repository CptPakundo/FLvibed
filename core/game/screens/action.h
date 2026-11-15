// FLvibed - a page's actions and the one runner every action goes through (the HUD 2 page framework: audit 70b 2.3).
// Types and the runner's pure rules only: no game include, nothing calls the game from here. A screen's model file
// lists its actions in one table of ActionDesc; the page runtime (pagerun, F2a) hands each press to Runner::Run, which
// decides, in this order:
//  1. one action in flight per page (a deferred one or one waiting for its answer): a second press is Busy;
//  2. the model's "may I" (May: 0, or the game's own refusal, its reason / IDS);
//  3. the shadow: while the game's own window is parked (root != 0) its own refusal (WindowSays) is asked too; when the
//     two disagree (one refuses, the other does not) the press is refused and recorded (the owner's default, 70b 6.4);
//  4. the route: multiplayer ? mp : sp. The multiplayer route stays Borrowed until a table row switches it to Direct
//     WITH the mptest run that proved it (mpProof); a Direct whose replaced game handlers fail their signatures, or
//     whose mpProof is empty in multiplayer, falls back to Borrowed (a mod patched the handler: the game's own button
//     still works); Borrowed needs the parked window;
//  5. deferred: queued, and run at the frame's end (FrameEnd), where May and the shadow are asked again;
//  6. the call (Direct or Borrowed);
//  7. answer Later: pending until Done reports, or the timeout clears it.
// tests\action_runner_test.cpp holds every rule. The caller logs what the outcome says ([PAGE SHADOW] lines: ShadowLine).
#pragma once
#include <cstdint>
#include <cstdio>

namespace hk
{
struct Sym; // core\hook.h: a signed game symbol (its resolution is asked through RunEnv::SymOk)
}

namespace screens
{
enum class Route : uint8_t
{
    Off,      // the action does not exist in this mode
    Direct,   // the mod's own calls (every call the game's handler ends in, all or none)
    Borrowed, // the game's own button or handler on the parked window
};

enum class Answer : uint8_t
{
    Now,   // done when the call returns
    Later, // the server answers later: pending until Done, or the timeout
};

struct ActionDesc
{
    int id;
    const char* name;     // for the log: <page>.<name>
    const wchar_t* word;  // the prompt's word
    const char* sound;    // the button's own sound name (Direct plays it; Borrowed plays the button's)
    int (*May)(const void* model, int arg);       // 0, or the game's refusal (reason / IDS); pure, unit-tested
    bool (*Direct)(const void* model, int arg);   // every call the game's handler ends in, all or none
    bool (*Borrowed)(uint32_t root, int arg);     // the game's own button or handler on the parked window
    int (*WindowSays)(uint32_t root, int arg);    // the parked window's own refusal (null: none to compare)
    Route sp, mp;
    const char* mpProof;  // the mptest run that proved Direct in multiplayer ("" / null: not proved)
    bool deferred;        // run at the frame's end (the game's own would be)
    Answer answer;
    float timeout;        // seconds, for Later
    bool (*Done)(const void* model, int* result); // Later: true once the answer came (result: the game's)
    const hk::Sym* const* replaces;               // the game's handlers a Direct stands for, null-ended (or null)
};

// What the runner needs of its surroundings, as values and one query: pure in tests, the game's in pagerun.
struct RunEnv
{
    bool multiplayer = false;
    uint32_t root = 0;                         // the parked window, 0 when there is none
    bool (*SymOk)(const hk::Sym*) = nullptr;   // a symbol resolved (null: all are)
};

enum class Result : uint8_t
{
    Done,     // called; answered now
    Pending,  // called; waits for its answer
    Deferred, // queued for the frame's end
    Busy,     // another action is in flight
    Refused,  // May (and the window, when it is there) refused: reason in Outcome::reason
    Shadow,   // the model and the window disagreed: refused and recorded
    NoRoute,  // the action is Off in this mode, or it has no function for its route
    NoWindow, // Borrowed with no parked window
    Failed,   // the call returned false (nothing pending)
    Answered, // Later: the answer came (Outcome::reason = Done's result)
    TimedOut, // Later: no answer within the timeout; cleared
    Idle,     // nothing happened
};

struct Outcome
{
    Result result = Result::Idle;
    int reason = 0;      // the refusal (May's, or the window's) or the answer's result
    Route route = Route::Off;
    int action = -1;     // the action's id
};

// The last disagreement, for the log and the `page shadow` count (70b 2.5).
struct ShadowRecord
{
    int action = -1;
    int model = 0, window = 0;
};

// The route a press takes in this mode, before the window test (rule 4).
inline Route RouteFor(const ActionDesc& a, const RunEnv& env)
{
    Route r = env.multiplayer ? a.mp : a.sp;
    if (r == Route::Direct)
    {
        if (env.multiplayer && !(a.mpProof && a.mpProof[0]))
            r = Route::Borrowed; // multiplayer stays borrowed until an mptest run proves the Direct route
        else if (a.replaces && env.SymOk)
            for (const hk::Sym* const* s = a.replaces; *s; ++s)
                if (!env.SymOk(*s))
                {
                    r = Route::Borrowed; // a replaced handler is not the game's we read: its own button decides
                    break;
                }
    }
    return r;
}

class Runner
{
public:
    // A press of action `a` with `arg` on the page's model.
    Outcome Run(const ActionDesc& a, const void* model, int arg, const RunEnv& env)
    {
        if (phase_ != Phase::Idle)
            return Make(Result::Busy, 0, Route::Off, a.id);
        const Outcome ask = Ask(a, model, arg, env);
        if (ask.result != Result::Idle)
            return ask;
        if (a.deferred)
        {
            phase_ = Phase::Deferred, action_ = a.id, arg_ = arg, age_ = 0;
            return Make(Result::Deferred, 0, RouteFor(a, env), a.id);
        }
        return Call(a, model, arg, env);
    }

    // The frame's end: a deferred action runs now, asked again (the game may have changed under it this frame).
    Outcome FrameEnd(const ActionDesc* table, int n, const void* model, const RunEnv& env)
    {
        if (phase_ != Phase::Deferred)
            return Outcome{};
        const ActionDesc* a = Find(table, n, action_);
        const int arg = arg_;
        phase_ = Phase::Idle, action_ = -1;
        if (!a)
            return Make(Result::NoRoute, 0, Route::Off, -1);
        const Outcome ask = Ask(*a, model, arg, env);
        if (ask.result != Result::Idle)
            return ask;
        return Call(*a, model, arg, env);
    }

    // Each frame: an action waiting for its answer asks Done; past its timeout it is cleared (and the caller logs it).
    Outcome Tick(const ActionDesc* table, int n, const void* model, float dt)
    {
        if (phase_ != Phase::Waiting)
            return Outcome{};
        const ActionDesc* a = Find(table, n, action_);
        int result = 0;
        if (a && a->Done && a->Done(model, &result))
        {
            const int id = action_;
            phase_ = Phase::Idle, action_ = -1;
            return Make(Result::Answered, result, Route::Off, id);
        }
        age_ += dt;
        if (!a || age_ >= a->timeout)
        {
            const int id = action_;
            phase_ = Phase::Idle, action_ = -1;
            return Make(Result::TimedOut, 0, Route::Off, id);
        }
        return Outcome{};
    }

    // A close that leaves the page, a style switch, a reload, a fault: nothing stays queued or waiting (a late answer
    // reaches the game's own window).
    void Drop() { phase_ = Phase::Idle, action_ = -1, age_ = 0; }

    bool InFlight() const { return phase_ != Phase::Idle; }
    int Action() const { return action_; } // the action in flight, -1 none
    bool Waiting() const { return phase_ == Phase::Waiting; }
    bool Queued() const { return phase_ == Phase::Deferred; }
    int Shadows() const { return shadows_; }
    const ShadowRecord& LastShadow() const { return last_; }

private:
    enum class Phase : uint8_t
    {
        Idle,
        Deferred,
        Waiting,
    };

    static Outcome Make(Result r, int reason, Route route, int action)
    {
        Outcome o;
        o.result = r, o.reason = reason, o.route = route, o.action = action;
        return o;
    }
    static const ActionDesc* Find(const ActionDesc* table, int n, int id)
    {
        for (int i = 0; i < n; ++i)
            if (table[i].id == id)
                return &table[i];
        return nullptr;
    }

    // Rules 2-4 (the "may I", the shadow, the route): Idle when the press may go on.
    Outcome Ask(const ActionDesc& a, const void* model, int arg, const RunEnv& env)
    {
        const int r = a.May ? a.May(model, arg) : 0;
        if (env.root && a.WindowSays)
        {
            const int w = a.WindowSays(env.root, arg);
            if ((r == 0) != (w == 0))
            {
                ++shadows_;
                last_.action = a.id, last_.model = r, last_.window = w;
                return Make(Result::Shadow, r ? r : w, Route::Off, a.id);
            }
        }
        if (r != 0)
            return Make(Result::Refused, r, Route::Off, a.id);
        const Route route = RouteFor(a, env);
        if (route == Route::Off || (route == Route::Direct && !a.Direct) || (route == Route::Borrowed && !a.Borrowed))
            return Make(Result::NoRoute, 0, route, a.id);
        if (route == Route::Borrowed && !env.root)
            return Make(Result::NoWindow, 0, route, a.id);
        return Outcome{};
    }

    // Rules 6-7.
    Outcome Call(const ActionDesc& a, const void* model, int arg, const RunEnv& env)
    {
        const Route route = RouteFor(a, env);
        const bool ok = route == Route::Direct ? a.Direct(model, arg) : a.Borrowed(env.root, arg);
        if (!ok)
            return Make(Result::Failed, 0, route, a.id);
        if (a.answer == Answer::Later)
        {
            phase_ = Phase::Waiting, action_ = a.id, arg_ = arg, age_ = 0;
            return Make(Result::Pending, 0, route, a.id);
        }
        return Make(Result::Done, 0, route, a.id);
    }

    Phase phase_ = Phase::Idle;
    int action_ = -1, arg_ = 0;
    float age_ = 0;
    int shadows_ = 0;
    ShadowRecord last_;
};

// The log line of a disagreement: "[PAGE SHADOW] <page>.<action> model=<r> window=<w>".
inline int ShadowLine(char* out, size_t size, const char* page, const char* action, const ShadowRecord& s)
{
    return snprintf(out, size, "[PAGE SHADOW] %s.%s model=%d window=%d", page ? page : "?", action ? action : "?", s.model, s.window);
}
} // namespace screens
