// Which device takes the input (modules\controller\src\impl\device_rules.h): only a new input, never a held one, so a pad
// button and a mouse button held together do not swap the device every frame (the owner's log, 14 Nov 2026).
#include "../modules/controller/src/impl/device_rules.h"

#include "test.h"

void Test_device_rules()
{
    using namespace devicerule;
    const unsigned A = 1u << 12, LT = 0x10000u, LMB = 1u, RMB = 2u;
    auto run = [](Rule& r, bool& padActive, const Input& in) {
        const Take t = r.Step(padActive, in);
        if (t != Take::None)
            padActive = TakesPad(t);
        return t;
    };

    // the left trigger and the left mouse button held together for 10 frames: one switch to the pad, then none
    {
        Rule r;
        bool pad = false;
        Input in;
        in.pad = LT, in.mouse = LMB, in.graceOver = true;
        int switches = 0;
        for (int i = 0; i < 10; ++i)
            switches += run(r, pad, in) != Take::None ? 1 : 0;
        CHECK(switches == 1 && pad);
    }

    // the pad holds a button, the mouse button goes down: keyboard and mouse once; held on, nothing more
    {
        Rule r;
        bool pad = true;
        Input in;
        in.pad = A, in.graceOver = true;
        CHECK(run(r, pad, in) == Take::None && pad); // (the pad is already the device)
        in.mouse = LMB;
        CHECK(run(r, pad, in) == Take::MouseButton && !pad);
        for (int i = 0; i < 10; ++i)
            CHECK(run(r, pad, in) == Take::None && !pad);
        // a new pad press while the mouse button is still held takes the pad once
        in.pad = A | LT;
        CHECK(run(r, pad, in) == Take::PadButton && pad);
        for (int i = 0; i < 10; ++i)
            CHECK(run(r, pad, in) == Take::None && pad);
        // a second mouse button going down while the first is held takes it back once
        in.mouse = LMB | RMB;
        CHECK(run(r, pad, in) == Take::MouseButton && !pad);
        CHECK(run(r, pad, in) == Take::None && !pad);
        // released and pressed again: a new press
        in.pad = 0;
        CHECK(run(r, pad, in) == Take::None);
        in.pad = A;
        CHECK(run(r, pad, in) == Take::PadButton && pad);
    }

    // a held input seen under an input_mode lock (the rule still steps) is not new when the lock ends
    {
        Rule r;
        Input in;
        in.pad = A, in.graceOver = true;
        r.Step(false, in); // a locked frame: the result is ignored by the caller
        CHECK(r.Step(false, in) == Take::None);
    }

    // the stick and motion rules as before: they take the device only once the grace is over; a key is always new
    {
        Rule r;
        Input in;
        in.stickMoved = true;
        CHECK(r.Step(false, in) == Take::None);
        in.graceOver = true;
        CHECK(r.Step(false, in) == Take::PadStick);
        CHECK(r.Step(true, in) == Take::None); // (the stick means nothing while the pad is the device)
        Input m;
        m.mouseMotion = true;
        CHECK(r.Step(true, m) == Take::None);
        m.graceOver = true;
        CHECK(r.Step(true, m) == Take::Mouse);
        CHECK(r.Step(false, m) == Take::None);
        Input k;
        k.key = true;
        CHECK(r.Step(true, k) == Take::Key);
    }
}
