// The one focus gate for the physical pad (modules\controller\src\impl\focus_gate.h, U38): flight and menus ignore the real pad
// while the game is in the background unless `focus_only` is off; an input held across the focus return stays ignored
// until it is released.
#include "../modules/controller/src/impl/focus_gate.h"

#include "test.h"

void Test_focus_gate()
{
    using namespace focusgate;
    int mainWnd = 0, popup = 0, other = 0, otherOwner = 0;
    const void* main = &mainWnd;
    // the foreground: the main window, a window it owns, never another program's (or no window at all)
    CHECK(Foreground(main, main, main));
    CHECK(Foreground(&popup, main, main));
    CHECK(!Foreground(&other, &otherOwner, main));
    CHECK(!Foreground(&other, &other, main));
    CHECK(!Foreground(nullptr, nullptr, main));
    CHECK(!Foreground(main, main, nullptr)); // no main window yet: not in front (fail closed)

    // the gate: focus_only on -> ignored in the background only; off -> never
    CHECK(IgnoreReal(true, false) && !IgnoreReal(true, true));
    CHECK(!IgnoreReal(false, false) && !IgnoreReal(false, true));

    const unsigned A = 1u << 12, B = 1u << 13, LS = 1u << 18;
    Held h;
    CHECK(h.Filter(false, A) == A);          // in front: everything acts
    CHECK(h.Filter(true, A | B) == 0);       // background: nothing acts
    CHECK(h.Filter(true, B) == 0);           // (A released in the background, B still held)
    CHECK(h.Filter(false, B | LS) == LS);    // back in front: B began in the background -> hidden; LS is new -> acts
    CHECK(h.Filter(false, B | LS) == LS);    // still held: still hidden
    CHECK(h.Filter(false, LS) == LS);        // B released ...
    CHECK(h.Filter(false, B | LS) == (B | LS)); // ... and pressed again: the game's now
    CHECK(h.Filter(false, 0) == 0);

    // focus_only off: nothing goes stale, a press held in the background acts at once and on
    Held off;
    CHECK(off.Filter(IgnoreReal(false, false), A) == A);
    CHECK(off.Filter(IgnoreReal(false, true), A) == A);
}
