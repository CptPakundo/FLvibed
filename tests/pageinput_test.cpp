// modules\hud\src\pages\pageinput.h (pure): the three D-pad repeat rates and the purchase hold, replayed beside copies of
// today's code (modules\hud\src\basepage.cpp / nnpage.cpp, the lists; modules\controller\src\impl\padbases.cpp, the
// game's text bar; nnpage.cpp, INFO's text; modules\controller\src\impl\dealer_rules.h, the hold) at several frame rates:
// the same repeats on the same frames.
#include "test.h"

#include "../modules/hud/src/pages/pageinput.h"

namespace
{
// Today's lists (basepage.cpp, nnpage.cpp): repeat += dt; if (repeat > 0.3f) fire, repeat -= 0.05f.
int TodayList(float& repeat, bool held, float dt)
{
    if (!held)
        return repeat = 0, 0;
    repeat += dt;
    if (repeat > 0.3f)
        return repeat -= 0.05f, 1;
    return 0;
}
// Today's text bar (padbases.cpp): the same form, 0.07 s.
int TodayTextBar(float& textHeld, bool held, float dt)
{
    if (!held)
        return textHeld = 0, 0;
    textHeld += dt;
    if (textHeld > 0.3f)
        return textHeld -= 0.07f, 1;
    return 0;
}
// Today's INFO text (nnpage.cpp): step = held ? step + dt * 15 : 0; for (; step >= 1 + 0.3f * 15; step -= 1) a line.
int TodayInfo(float& step, bool held, float dt)
{
    step = held ? step + dt * 15 : 0;
    int n = 0;
    for (; step >= 1 + 0.3f * 15; step -= 1)
        ++n;
    return n;
}

// Holds the button for `heldFor` seconds, lets go for one, holds again; every frame both must agree.
template <typename Today>
void Replay(const pageinput::Repeat& r, Today today, float fps, int* totalOut)
{
    float old = 0;
    pageinput::RepeatState s;
    const float dt = 1.0f / fps;
    int total = 0;
    const int frames = static_cast<int>(fps * 4);
    for (int i = 0; i < frames; ++i)
    {
        const float t = i * dt;
        const bool held = t < 2.0f || t >= 2.5f;
        const int a = pageinput::Step(r, s, held, dt), b = today(old, held, dt);
        CHECK(a == b);
        total += a;
    }
    *totalOut = total;
}
} // namespace

void Test_pageinput()
{
    // The constants are today's (checked when compiling).
    static_assert(pageinput::kListRepeat.delay == 0.3f && pageinput::kListRepeat.step == 0.05f && pageinput::kListRepeat.perSecond == 0, "lists");
    static_assert(pageinput::kTextBarRepeat.delay == 0.3f && pageinput::kTextBarRepeat.step == 0.07f && pageinput::kTextBarRepeat.perSecond == 0, "text bar");
    static_assert(pageinput::kInfoTextRepeat.delay == 0.3f && pageinput::kInfoTextRepeat.perSecond == 15, "INFO");
    static_assert(pageinput::kHoldSec == 0.6f, "hold");

    // Frame by frame equal to today's code at 30, 60, 144 and 240 frames a second.
    for (float fps : {30.0f, 60.0f, 144.0f, 240.0f})
    {
        int list = 0, bar = 0, info = 0;
        Replay(pageinput::kListRepeat, TodayList, fps, &list);
        Replay(pageinput::kTextBarRepeat, TodayTextBar, fps, &bar);
        Replay(pageinput::kInfoTextRepeat, TodayInfo, fps, &info);
        CHECK(list > 0 && bar > 0 && info > 0);
    }
    // The three rates stay apart (20, 15 and about 14 a second): counted over 8 s of a long hold at 240 fps.
    {
        pageinput::RepeatState l, b, i;
        int nl = 0, nb = 0, ni = 0;
        const float dt = 1.0f / 240;
        for (int f = 0; f < 240 * 9; ++f)
        {
            const int a = pageinput::Step(pageinput::kListRepeat, l, true, dt), c = pageinput::Step(pageinput::kTextBarRepeat, b, true, dt),
                      d = pageinput::Step(pageinput::kInfoTextRepeat, i, true, dt);
            if (f >= 240)
                nl += a, nb += c, ni += d;
        }
        CHECK(nl >= 159 && nl <= 161); // 20 a second
        CHECK(ni >= 119 && ni <= 121); // 15 a second
        CHECK(nb >= 113 && nb <= 116); // 1 / 0.07 = 14.3 a second
    }
    // At 30 fps the lists' repeat is held to one a frame (today's form): 30 a second at most.
    {
        pageinput::RepeatState s;
        int n = 0;
        for (int i = 0; i < 30; ++i)
            n += pageinput::Step(pageinput::kListRepeat, s, true, 1.0f / 30);
        CHECK(n <= 30 - 9);
    }
    // INFO's rate gives several lines in one long frame; a release resets every repeat.
    {
        pageinput::RepeatState s;
        CHECK(pageinput::Step(pageinput::kInfoTextRepeat, s, true, 0.2f) == 0);
        CHECK(pageinput::Step(pageinput::kInfoTextRepeat, s, true, 0.45f) == 5); // 0.65 s x 15 = 9.75: a line at each of 9.75 .. 5.75 (the threshold 1 + 4.5)
        CHECK(pageinput::Step(pageinput::kInfoTextRepeat, s, false, 0.1f) == 0 && s.acc == 0);
    }

    // The hold: a tap never fires; held 0.6 s fires once; again only after a release.
    {
        pageinput::HoldState h;
        CHECK(!pageinput::StepHold(h, true, 0.3f));
        CHECK(pageinput::HoldProgress(h) > 0.49f && pageinput::HoldProgress(h) < 0.51f);
        CHECK(!pageinput::StepHold(h, false, 0.1f) && h.held == 0); // the tap let go: nothing spent
        int fires = 0;
        for (int i = 0; i < 120; ++i)
            fires += pageinput::StepHold(h, true, 1.0f / 60);
        CHECK(fires == 1 && h.fired && pageinput::HoldProgress(h) == 0);
        pageinput::StepHold(h, false, 1.0f / 60);
        for (int i = 0; i < 40; ++i)
            fires += pageinput::StepHold(h, true, 1.0f / 60);
        CHECK(fires == 2);
    }
}
