// core\game\screens\action.h (pure): the one runner every page action goes through (audit 70b 2.3): one in flight, the
// model's "may I", the shadow against the parked window's refusal, the route (multiplayer borrowed until switched and
// proved), deferral to the frame's end, the answer later with its timeout.
#include "test.h"

#include "game/screens/action.h"

#include <cstring>

using screens::ActionDesc;
using screens::Answer;
using screens::Result;
using screens::Route;
using screens::RunEnv;

namespace
{
// A fake model and fake game: what each function was asked, what it answers.
struct Model
{
    int may = 0;
    bool answered = false;
    int answer = 0;
};
int g_window = 0;                 // the parked window's refusal
int g_direct = 0, g_borrowed = 0; // calls
bool g_callOk = true;
int g_symOk = 1;

int May(const void* m, int) { return static_cast<const Model*>(m)->may; }
int WindowSays(uint32_t, int) { return g_window; }
bool Direct(const void*, int) { return ++g_direct, g_callOk; }
bool Borrowed(uint32_t, int) { return ++g_borrowed, g_callOk; }
bool Done(const void* m, int* r)
{
    const Model* model = static_cast<const Model*>(m);
    *r = model->answer;
    return model->answered;
}
bool SymOk(const hk::Sym*) { return g_symOk != 0; }
const hk::Sym* const kReplaces[] = {reinterpret_cast<const hk::Sym*>(&g_symOk), nullptr}; // any non-null pointer

ActionDesc Act(int id, Route sp, Route mp, const char* proof = "")
{
    ActionDesc a = {};
    a.id = id, a.name = "act", a.word = L"ACT";
    a.May = May, a.Direct = Direct, a.Borrowed = Borrowed, a.WindowSays = WindowSays;
    a.sp = sp, a.mp = mp, a.mpProof = proof;
    a.answer = Answer::Now, a.timeout = 10;
    return a;
}
void Reset() { g_window = 0, g_direct = 0, g_borrowed = 0, g_callOk = true, g_symOk = 1; }
RunEnv Env(bool mp, uint32_t root)
{
    RunEnv e;
    e.multiplayer = mp, e.root = root, e.SymOk = SymOk;
    return e;
}
} // namespace

void Test_action_runner()
{
    Model m;
    // A plain action: May 0, single player, Direct: called once, done.
    {
        Reset();
        screens::Runner r;
        const ActionDesc a = Act(1, Route::Direct, Route::Borrowed);
        const screens::Outcome o = r.Run(a, &m, 0, Env(false, 0));
        CHECK(o.result == Result::Done && o.route == Route::Direct && g_direct == 1 && g_borrowed == 0 && !r.InFlight());
    }
    // The model refuses: nothing called, the reason given.
    {
        Reset();
        screens::Runner r;
        Model no;
        no.may = 1397;
        g_window = 1397;
        const screens::Outcome o = r.Run(Act(1, Route::Direct, Route::Borrowed), &no, 0, Env(false, 0x1000));
        CHECK(o.result == Result::Refused && o.reason == 1397 && g_direct == 0 && r.Shadows() == 0); // both refuse: no shadow
    }
    // The shadow: the model says yes and the window no (and the other way): refused and recorded, nothing called.
    {
        Reset();
        screens::Runner r;
        g_window = 1324;
        screens::Outcome o = r.Run(Act(2, Route::Direct, Route::Borrowed), &m, 0, Env(false, 0x1000));
        CHECK(o.result == Result::Shadow && g_direct == 0 && g_borrowed == 0 && r.Shadows() == 1);
        CHECK(r.LastShadow().action == 2 && r.LastShadow().model == 0 && r.LastShadow().window == 1324);
        Model no;
        no.may = 7;
        g_window = 0;
        o = r.Run(Act(3, Route::Direct, Route::Borrowed), &no, 0, Env(false, 0x1000));
        CHECK(o.result == Result::Shadow && r.Shadows() == 2 && r.LastShadow().model == 7 && r.LastShadow().window == 0);
        char line[96];
        screens::ShadowLine(line, sizeof line, "commodity", "sell", r.LastShadow());
        CHECK(strcmp(line, "[PAGE SHADOW] commodity.sell model=7 window=0") == 0);
        // with no parked window there is nothing to compare: the model alone decides
        g_window = 1324;
        o = r.Run(Act(2, Route::Direct, Route::Borrowed), &m, 0, Env(false, 0));
        CHECK(o.result == Result::Done && r.Shadows() == 2);
    }
    // The route: multiplayer stays borrowed until switched; switched without an mptest proof it still borrows.
    {
        Reset();
        screens::Runner r;
        CHECK(screens::RouteFor(Act(4, Route::Direct, Route::Borrowed), Env(true, 1)) == Route::Borrowed);
        CHECK(screens::RouteFor(Act(4, Route::Direct, Route::Direct, ""), Env(true, 1)) == Route::Borrowed);
        CHECK(screens::RouteFor(Act(4, Route::Direct, Route::Direct, nullptr), Env(true, 1)) == Route::Borrowed);
        CHECK(screens::RouteFor(Act(4, Route::Direct, Route::Direct, "mptest_2026-11-20"), Env(true, 1)) == Route::Direct);
        CHECK(screens::RouteFor(Act(4, Route::Direct, Route::Direct, ""), Env(false, 1)) == Route::Direct); // single player needs no proof
        const screens::Outcome o = r.Run(Act(4, Route::Direct, Route::Borrowed), &m, 0, Env(true, 0x1000));
        CHECK(o.result == Result::Done && o.route == Route::Borrowed && g_borrowed == 1 && g_direct == 0);
        // Borrowed with no parked window refuses; an Off route refuses
        CHECK(r.Run(Act(4, Route::Direct, Route::Borrowed), &m, 0, Env(true, 0)).result == Result::NoWindow);
        CHECK(r.Run(Act(4, Route::Direct, Route::Off), &m, 0, Env(true, 0x1000)).result == Result::NoRoute);
        ActionDesc noDirect = Act(4, Route::Direct, Route::Borrowed);
        noDirect.Direct = nullptr;
        CHECK(r.Run(noDirect, &m, 0, Env(false, 0x1000)).result == Result::NoRoute);
        CHECK(g_borrowed == 1);
    }
    // A replaced game handler failing its signature (another mod patched it) forces the game's own button.
    {
        Reset();
        screens::Runner r;
        ActionDesc a = Act(5, Route::Direct, Route::Borrowed);
        a.replaces = kReplaces;
        CHECK(screens::RouteFor(a, Env(false, 1)) == Route::Direct);
        g_symOk = 0;
        CHECK(screens::RouteFor(a, Env(false, 1)) == Route::Borrowed);
        const screens::Outcome o = r.Run(a, &m, 0, Env(false, 0x1000));
        CHECK(o.result == Result::Done && o.route == Route::Borrowed && g_direct == 0 && g_borrowed == 1);
    }
    // One action in flight: a deferred one blocks a second press until the frame's end runs it; asked again there.
    {
        Reset();
        screens::Runner r;
        ActionDesc t[2] = {Act(6, Route::Direct, Route::Borrowed), Act(7, Route::Direct, Route::Borrowed)};
        t[0].deferred = true;
        screens::Outcome o = r.Run(t[0], &m, 3, Env(false, 0));
        CHECK(o.result == Result::Deferred && g_direct == 0 && r.Queued() && r.Action() == 6);
        CHECK(r.Run(t[1], &m, 0, Env(false, 0)).result == Result::Busy && g_direct == 0);
        o = r.FrameEnd(t, 2, &m, Env(false, 0));
        CHECK(o.result == Result::Done && o.action == 6 && g_direct == 1 && !r.InFlight());
        CHECK(r.FrameEnd(t, 2, &m, Env(false, 0)).result == Result::Idle && g_direct == 1); // nothing queued any more
        // the game changed under it this frame: May refuses at the frame's end, nothing is called
        CHECK(r.Run(t[0], &m, 0, Env(false, 0)).result == Result::Deferred);
        Model now;
        now.may = 99;
        o = r.FrameEnd(t, 2, &now, Env(false, 0));
        CHECK(o.result == Result::Refused && o.reason == 99 && g_direct == 1 && !r.InFlight());
    }
    // Answer later: pending until Done, then answered; a second press meanwhile is Busy; the timeout clears and says so.
    {
        Reset();
        screens::Runner r;
        ActionDesc t[1] = {Act(8, Route::Borrowed, Route::Borrowed)};
        t[0].answer = Answer::Later, t[0].Done = Done, t[0].timeout = 10;
        Model w;
        screens::Outcome o = r.Run(t[0], &w, 0, Env(false, 0x1000));
        CHECK(o.result == Result::Pending && r.Waiting() && g_borrowed == 1);
        CHECK(r.Run(t[0], &w, 0, Env(false, 0x1000)).result == Result::Busy && g_borrowed == 1);
        CHECK(r.Tick(t, 1, &w, 5).result == Result::Idle && r.Waiting());
        w.answered = true, w.answer = 1;
        o = r.Tick(t, 1, &w, 0.016f);
        CHECK(o.result == Result::Answered && o.reason == 1 && o.action == 8 && !r.InFlight());
        w.answered = false;
        CHECK(r.Run(t[0], &w, 0, Env(false, 0x1000)).result == Result::Pending);
        CHECK(r.Tick(t, 1, &w, 9.9f).result == Result::Idle);
        o = r.Tick(t, 1, &w, 0.2f);
        CHECK(o.result == Result::TimedOut && o.action == 8 && !r.InFlight());
        // a drop (a close, a reload) leaves nothing waiting
        CHECK(r.Run(t[0], &w, 0, Env(false, 0x1000)).result == Result::Pending);
        r.Drop();
        CHECK(!r.InFlight() && r.Tick(t, 1, &w, 20).result == Result::Idle);
    }
    // A call that fails leaves nothing pending.
    {
        Reset();
        screens::Runner r;
        ActionDesc a = Act(9, Route::Direct, Route::Borrowed);
        a.answer = Answer::Later, a.Done = Done;
        g_callOk = false;
        CHECK(r.Run(a, &m, 0, Env(false, 0)).result == Result::Failed && !r.InFlight());
    }
}
