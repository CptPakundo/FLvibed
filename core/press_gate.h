// FLvibed - press ownership between the pad's consumers (26 Oct 2026, the owner's attended run on vanilla 1.1 + FLUF:
// the B that closed FLUF's "Move System" window and the B that closed FLUF's palette were each ALSO free flight - `pad B
// free_flight tap` - because the flight dispatch, idle while FLUF had the pad, saw the still-held B as a new press on the
// frame the window went and acted on its release). Header only; unit test tests\press_gate_test.cpp.
//
// The rule: a press belongs to the consumer that had the pad when it went down. Each consumer of the pad's inputs (the
// flight dispatch, the pad on FLUF's windows, the menu / window cursor) keeps one gate. An input already down on a frame
// this consumer did not take the pad hides from it until it is released - whoever took it then (a FLUF window, the
// palette, the game's popup, a pad menu, the options page, a cutscene, the other consumer) owns that press to its end. A
// press made on a frame this consumer takes is its own, so the press that makes the pad the device acts at once.
// Inputs are buttons and sticks alike (a stick is "down" while deflected past its dead zone).
#pragma once

template <int N> class PressGate
{
  public:
    // Once per frame, before any consumer runs: every input's down state this frame.
    void Observe(const bool down[N])
    {
        const bool tookPrev = took_;
        took_ = false;
        for (int i = 0; i < N; ++i)
        {
            if (!down[i])
                hidden_[i] = false; // released: the next press is new
            else if (prev_[i] && !tookPrev)
                hidden_[i] = true; // held from a frame another consumer had the pad: theirs to the release
            prev_[i] = down[i];
        }
    }
    // This consumer takes the pad this frame: the presses it does not own read as up (down[] edited in place). `fresh`,
    // when given, gets the inputs hidden since this consumer's last take (a consumer that tracked them down cancels them:
    // a hold of its own begun before the other consumer took over must not end in a tap).
    void Take(bool down[N], bool fresh[N] = nullptr)
    {
        took_ = true;
        for (int i = 0; i < N; ++i)
        {
            if (fresh)
                fresh[i] = hidden_[i] && !reported_[i];
            reported_[i] = hidden_[i];
            if (hidden_[i])
                down[i] = false;
        }
    }
    bool Hidden(int i) const { return i >= 0 && i < N && hidden_[i]; }

  private:
    bool prev_[N] = {};
    bool hidden_[N] = {};
    bool reported_[N] = {};
    bool took_ = false;
};
