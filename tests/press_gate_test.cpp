// Press ownership between the pad's consumers (core\press_gate.h): the attended run's leak (the B that closed a FLUF
// window also reached the flight dispatch) and its neighbours - the popup's A, the device switch's first press, a hold
// that began in flight before another consumer took the pad.
#include "press_gate.h"

#include "test.h"

namespace
{
enum
{
    A,
    B,
    LS,
    N
};

struct Frame
{
    bool in[N] = {};
};

// One frame: the inputs, then whichever consumers take the pad; returns what `flight` saw (and what `other` saw).
void Step(PressGate<N>& flight, PressGate<N>& other, const bool in[N], bool flightTakes, bool seen[N], bool otherSeen[N],
          bool fresh[N] = nullptr)
{
    flight.Observe(in);
    other.Observe(in);
    for (int i = 0; i < N; ++i)
        seen[i] = otherSeen[i] = false;
    if (flightTakes)
    {
        for (int i = 0; i < N; ++i)
            seen[i] = in[i];
        flight.Take(seen, fresh);
    }
    else
    {
        for (int i = 0; i < N; ++i)
            otherSeen[i] = in[i];
        other.Take(otherSeen);
    }
}
} // namespace

void Test_press_gate()
{
    bool seen[N], otherSeen[N], fresh[N];
    // 1. The attended run: B goes down while FLUF's window has the pad, the window closes (the flight dispatch takes the
    // pad again) while B is still held - flight never sees B, not on that frame and not on its release.
    {
        PressGate<N> flight, fluf;
        const bool none[N] = {}, b[N] = {false, true, false};
        Step(flight, fluf, none, false, seen, otherSeen);
        Step(flight, fluf, b, false, seen, otherSeen);
        CHECK(otherSeen[B]); // FLUF's own press
        Step(flight, fluf, b, false, seen, otherSeen);
        Step(flight, fluf, b, true, seen, otherSeen, fresh); // the window went: flight's turn, B still down
        CHECK(!seen[B]);
        CHECK(fresh[B]); // reported once as newly hidden (a dispatcher that tracked it cancels it)
        Step(flight, fluf, b, true, seen, otherSeen, fresh);
        CHECK(!seen[B]);
        CHECK(!fresh[B]); // only once
        Step(flight, fluf, none, true, seen, otherSeen); // released
        CHECK(!seen[B]);
        Step(flight, fluf, b, true, seen, otherSeen); // the next press is flight's
        CHECK(seen[B]);
    }
    // 2. The same press made on the very frame the pad comes back to flight is flight's (the first press after a device
    // switch acts at once).
    {
        PressGate<N> flight, other;
        const bool none[N] = {}, a[N] = {true, false, false};
        Step(flight, other, none, false, seen, otherSeen);
        Step(flight, other, a, true, seen, otherSeen);
        CHECK(seen[A]);
        Step(flight, other, a, true, seen, otherSeen);
        CHECK(seen[A]); // held: still flight's
    }
    // 3. The other direction: A held in flight (a hold) when a popup / FLUF window takes the pad - the window's consumer
    // must not see it as its mouse button; a fresh A there is its own.
    {
        PressGate<N> flight, popup;
        const bool none[N] = {}, a[N] = {true, false, false};
        Step(flight, popup, a, true, seen, otherSeen);
        CHECK(seen[A]);
        Step(flight, popup, a, false, seen, otherSeen); // the popup opened: popup's turn, A still held
        CHECK(!otherSeen[A]);
        Step(flight, popup, none, false, seen, otherSeen);
        Step(flight, popup, a, false, seen, otherSeen);
        CHECK(otherSeen[A]);
        Step(flight, popup, a, true, seen, otherSeen); // the A that closed the popup: not flight's context action
        CHECK(!seen[A]);
    }
    // 4. Sticks are inputs too: the left stick that moved FLUF's cursor does not throttle until re-centred.
    {
        PressGate<N> flight, fluf;
        const bool none[N] = {}, ls[N] = {false, false, true};
        Step(flight, fluf, ls, false, seen, otherSeen);
        Step(flight, fluf, ls, true, seen, otherSeen);
        CHECK(!seen[LS]);
        Step(flight, fluf, none, true, seen, otherSeen);
        Step(flight, fluf, ls, true, seen, otherSeen);
        CHECK(seen[LS]);
    }
    // 5. Several frames without any consumer (a cutscene): a press held through them is nobody's - not flight's after.
    {
        PressGate<N> flight;
        const bool b[N] = {false, true, false};
        flight.Observe(b);
        flight.Observe(b);
        bool d[N] = {false, true, false};
        flight.Take(d);
        CHECK(!d[B]);
        CHECK(flight.Hidden(B));
    }
}
